"""Whole fixed upkeep costs exercised with actual upkeep turns and live lands."""

from collections import Counter
from functools import lru_cache
import json
from pathlib import Path
import re

from death_counter_trigger_recipes import exact


FAMILY = 'upkeep_pay_or_sacrifice_creature'
PROFILE = Path(__file__).parent / 'data/upkeep_pay_or_sacrifice_creature_wave8_profiles.json'
SHADOW = 'Shadow (This creature can block or be blocked by only creatures with shadow.)'
LANDS = ('Wastes', 'Swamp', 'Island', 'Forest', 'Mountain', 'Plains')
COMMON = ('cast_payment', 'cast_short', 'cast_wrong_color', 'pay', 'decline', 'short', 'wrong_color',
          'opponent_upkeep', 'opponent_funds', 'repeat_pay', 'repeat_decline', 'repeat_short', 'in_hand',
          'cast_after_upkeep', 'destroy_before', 'control_pay', 'control_short', 'untap_replay', 'paid_replay',
          'fresh_attack', 'pay_attack', 'attack_ground', 'sacrifice_indestructible')
KEYWORD_MODES = {'Flying': ('attack_reach', 'attack_flyer', 'defend_flyer'),
                 'Shadow': ('attack_shadow', 'defend_shadow', 'defend_ground')}
COUNTED_RETURN = 'At the beginning of your upkeep, sacrifice this creature unless you pay {1} for each other creature you control.'
COUNTED_MODES = ('zero_only', 'zero_foe', 'zero_noncreature', 'count_two', 'count_two_short', 'count_removed',
                 'generic_colored')
MORPH_MODES = ('hidden', 'hidden_choice', 'hidden_short', 'hidden_upkeep', 'hidden_combat', 'reveal_pay',
               'reveal_short', 'reveal_wrong_color', 'reveal_opposing', 'reveal_after_upkeep',
               'reveal_then_short', 'reveal_replay')


@lru_cache(maxsize=1)
def pins():
    return json.loads(PROFILE.read_text(encoding='utf8'))


def parameters(record):
    from morph_creature_recipes import MORPH
    from recipes import cost_from_oracle

    if (not record or record.get('layout') != 'normal' or 'card_faces' in record or 'selected_face' in record or
            not record.get('type_line', '').startswith(('Creature — ', 'Legendary Creature — ')) or
            any(not re.fullmatch(r'[1-9][0-9]*', record.get(k, '')) for k in ('power', 'toughness'))):
        raise ValueError('normal whole numeric creature required')
    lines = record['oracle_text'].splitlines()
    morph = MORPH.fullmatch(lines[-1]) if lines else None
    if morph:
        lines = lines[:-1]
    counted = lines == ['Flying',COUNTED_RETURN]
    source = '(?:this creature|' + re.escape(record['name']) + ')'
    match = re.fullmatch(r'At the beginning of your upkeep, sacrifice '+source+r' unless you pay ((?:\{(?:[1-9]|[WUBRG])\})+)\.', lines[-1] if lines else '')
    keys = {(): [], ('Flying',): ['Flying'], (SHADOW,): ['Shadow']}.get(tuple(lines[:-1]))
    expected_keys = set(keys or ()) | ({'Morph'} if morph else set())
    if (not match and not counted) or keys is None or set(record['keywords']) != expected_keys:
        raise ValueError('a complete fixed self-sacrifice clause and reviewed intrinsic keyword required')
    fee = [1,0,0,0,0,0] if counted else cost_from_oracle(dict(record,mana_cost=match[1]))
    if not counted and (not any(fee[1:]) or sum(fee)>4):
        raise ValueError('bounded colored payment required')
    cast = cost_from_oracle(record)
    if not any(cast[1:]):
        raise ValueError('colored cast required for the separate wrong-color fixture')
    model = dict(upkeep_mana=fee,cast_mana=cast,keywords=keys,
                 power=int(record['power']),toughness=int(record['toughness']))
    if counted:
        model['fee_per_other_creature'] = True
    if morph:
        price = cost_from_oracle(dict(record,mana_cost=morph[1]))
        if counted or keys or cast[0]<3 or not any(price[1:]):
            raise ValueError('unreviewed coupled Morph clause or fee')
        model['morph'] = price
    return model


def variants(model):
    modes = COMMON + tuple(m for key in model['keywords'] for m in KEYWORD_MODES[key])
    if model.get('fee_per_other_creature'):
        modes = tuple(m for m in modes if m not in ('decline','repeat_decline','wrong_color')) + COUNTED_MODES
    if 'morph' in model:
        modes = tuple(m for m in modes if m not in ('cast_short','cast_wrong_color')) + MORPH_MODES
    return [f'{mode}_{owner}' for owner in (0, 1) for mode in modes] + ['cancel_0']


def spec(declaration, record):
    model = parameters(record)
    selected = declaration.get('variants')
    if (declaration.get('family') != FAMILY or type(declaration.get('version')) is not int or
            declaration['version'] != 2 or declaration.get('oracle_text') != record['oracle_text'] or
            not exact(declaration.get('params'), model) or type(selected) is not list or not selected or
            any(type(v) is not str for v in selected) or len(set(selected)) != len(selected) or set(selected)-set(variants(model))):
        raise ValueError('exact whole Oracle upkeep declaration and explicit variants required')
    return dict(model, family=FAMILY)


def validate_witnesses(oracle, registry):
    for group in ('sources','witnesses'):
        for name, pin in pins()[group].items():
            row, record = registry.get(name, {}), oracle.get(name, {})
            if (not exact(pin, {'oracle':record, 'registry':row}) or record.get('layout') != 'normal' or
                    row.get('kind') != 'card' or row.get('handler') in (None, 'NULL', '0', 'card_dummy')):
                raise ValueError(f'implemented normal upkeep identity changed: {name}')
    hidden = pins().get('hidden_observer')
    if hidden is not None and not exact(registry.get('Face-Down Creature'),hidden):
        raise ValueError('actual face-down identity changed')


def short_fee(fee):
    result = list(fee)
    result[next(i for i in range(1,6) if result[i])] -= 1
    return result


def wrong_fee(fee, replacement):
    result = short_fee(fee)
    result[replacement] += 1
    return result


class Scene:
    def __init__(self, name, model, mode, owner, registry, oracle):
        self.name, self.model, self.mode, self.owner = name, model, mode, owner
        self.other, self.active = 1-owner, owner
        self.rows, self.oracle = registry, oracle
        self.setup, self.steps, self.entities, self.lands = [], [], {}, {}
        self.life, self.mana = [20,20], [[0]*6 for _ in (0,1)]
        self.snapshot = None
        self.face_down = False

    def emit(self, op, **fields):
        from recipes import step

        self.steps.append(step(op, **fields))

    def add(self, name, ref, owner, zone='battlefield'):
        from recipes import card

        self.setup.append(card(name, ref, owner, zone))
        record = self.oracle[name]
        self.entities[ref] = dict(name=name, owner=owner, controller=owner, zone=zone, tapped=False,
                                  damage=0, power=int(record.get('power') or 0), toughness=int(record.get('toughness') or 0),
                                  keywords=record['keywords'])

    def add_lands(self, player, amounts):
        for color, amount in enumerate(amounts):
            for copy in range(amount):
                ref = f'land_{player}_{color}_{copy}'
                self.add(LANDS[color], ref, player)
                self.lands[ref] = color

    def observe(self, ref, property_name, value):
        self.emit('assert_card', ref=ref, property=property_name, value=int(value))

    def visible_csvid(self, ref):
        from morph_creature_recipes import FACE_DOWN

        return FACE_DOWN if ref=='source' and self.face_down else self.rows[self.entities[ref]['name']]['csvid']

    def morph_traits(self):
        if 'morph' not in self.model:
            return
        from recipes import COLORS

        source = self.entities['source']
        who, csvid = source['controller'], self.visible_csvid('source')
        values = {'color':0 if self.face_down else sum(1 << COLORS[c] for c in self.oracle[self.name]['colors']),
                  'token':0,'artifact':0,'subtype_Beast':int(not self.face_down)}
        self.observe('source','csvid',csvid)
        for prop,value in values.items():
            self.emit('assert_matching_permanents',player=who,index=csvid,property=prop,value=value)

    def assert_mana(self):
        for player, amounts in enumerate(self.mana):
            for color, value in enumerate(amounts):
                self.emit('assert_mana', player=player, color=color, value=value)

    def receipts(self):
        self.emit('assert_stack', value=0)
        self.emit('assert_choices_consumed')
        self.assert_mana()
        for ref, entity in self.entities.items():
            alive = entity['zone'] == 'battlefield'
            self.observe(ref,'in_play',alive)
            if alive:
                self.observe(ref,'tapped',entity['tapped'])
                if 'Creature' in self.oracle[entity['name']]['type_line']:
                    for key in ('power','toughness','damage'):
                        self.observe(ref,key,entity[key])
        if self.entities['source']['zone']=='battlefield':
            self.morph_traits()
        for player in (0,1):
            self.emit('assert_life', player=player, value=self.life[player])
            for zone in ('battlefield','hand','graveyard','exile','library'):
                counts = Counter((self.visible_csvid(ref) if zone=='battlefield' else self.rows[e['name']]['csvid'])
                                 for ref,e in self.entities.items() if e['zone']==zone and
                                 e['controller' if zone=='battlefield' else 'owner']==player)
                self.emit('assert_zone', player=player, zone=zone, value=sum(counts.values()))
                for csvid, count in counts.items():
                    self.emit('assert_zone_multiplicity', player=player, zone=zone,
                              index=csvid, value=count)

    def resolve(self):
        self.emit('resolve')
        self.emit('refresh')

    def fund(self, player, funding):
        assert not any(self.mana[player])
        self.mana[player] = list(funding)
        for color, value in enumerate(funding):
            if value:
                self.emit('set_mana', player=player, color=color, value=value)
        self.emit('refresh')

    def cast(self, ref, destination, *, target=None, target_zone=None, denied=None, down=False, hidden_choice=False):
        from recipes import cost_from_oracle

        entity = self.entities[ref]
        player = entity['owner']
        cost = cost_from_oracle(self.oracle[entity['name']])
        if down:
            funding = cost if hidden_choice else [2 if denied else 3,0,0,0,0,0]
            cost = [3,0,0,0,0,0]
        else:
            funding = short_fee(cost) if denied=='short' else wrong_fee(cost,0) if denied=='wrong_color' else cost
        self.fund(player,funding)
        if target:
            self.emit('choose_target', player=player, ref=target)
        if ref=='source' and 'morph' in self.model and not denied and (not down or hidden_choice):
            self.emit('choose_dialog',player=player,value=int(not down))
        self.emit('cast', ref=ref, value=int(denied is None))
        self.emit('assert_choices_consumed')
        self.emit('assert_stack', value=int(denied is None))
        if denied is None:
            self.observe(ref,'in_hand',False)
            if down:
                self.face_down = True
                entity.update(power=2,toughness=2,keywords=[])
                self.observe(ref,'csvid',self.visible_csvid(ref))
                self.observe(ref,'power',2)
                self.observe(ref,'toughness',2)
            self.mana[player] = [have-fee for have,fee in zip(funding,cost)]
            self.assert_mana()
            self.resolve()
            entity['zone'] = destination
            if target_zone:
                self.entities[target]['zone'] = target_zone
        self.receipts()

    def reveal(self, denied=None):
        source = self.entities['source']
        player = source['controller']
        cost = self.model['morph']
        funding = short_fee(cost) if denied=='short' else wrong_fee(cost,4) if denied=='wrong_color' else cost
        self.fund(player,funding)
        self.emit('can_activate',ref='source',value=int(denied is None))
        self.emit('activate',ref='source',value=int(denied is None))
        if denied is None:
            self.face_down = False
            source.update(power=self.model['power'],toughness=self.model['toughness'])
            self.mana[player] = [0]*6
            self.emit('refresh')
            self.morph_traits()
            self.observe('source','power',source['power'])
            self.observe('source','toughness',source['toughness'])
            self.assert_mana()
            # Drain legacy activation bookkeeping after verifying the actual reveal.
            self.resolve()
        self.receipts()

    def payment_lands(self):
        fee = self.model['upkeep_mana']
        if self.model.get('fee_per_other_creature'):
            count = sum(ref!='source' and entity['zone']=='battlefield' and entity['controller']==self.active and
                        'Creature' in self.oracle[entity['name']]['type_line'] for ref,entity in self.entities.items())
            fee = [fee[0]*count,0,0,0,0,0]
        free = [ref for ref in self.lands if self.entities[ref]['controller']==self.active and
                self.entities[ref]['zone']=='battlefield' and not self.entities[ref]['tapped']]
        colored = []
        for color, amount in enumerate(fee[1:],1):
            matching = [ref for ref in free if self.lands[ref] == color]
            if len(matching)<amount:
                return None
            colored.extend(matching[:amount])
        generic = [ref for ref in free if ref not in colored]
        generic.sort(key=lambda ref: self.lands[ref] != 0)
        if len(generic)<fee[0]:
            return None
        return generic[:fee[0]] + colored

    def visit(self, choice='pay', *, replay=False):
        assert not any(map(any,self.mana))
        self.emit('advance_turn')
        self.active = 1-self.active
        self.emit('untap_step', player=self.active)
        for entity in self.entities.values():
            entity['damage'] = 0
            if entity['zone']=='battlefield' and entity['controller']==self.active:
                entity['tapped'] = False
        self.receipts()
        if replay:
            self.snapshot = len(self.steps)
        source = self.entities['source']
        if source['zone']=='battlefield' and source['controller']==self.active and not self.face_down:
            refs = self.payment_lands()
            if refs is not None:
                if not self.model.get('fee_per_other_creature'):
                    self.emit('choose_dialog',player=self.active,value=int(choice=='decline'))
                if choice=='cancel':
                    assert self.active==0 and refs
                    self.emit('choose_cancel_mana', player=0)
                elif choice=='pay':
                    for ref in refs:
                        if self.active==0:
                            self.emit('choose_mana_source', ref=ref)
                        self.entities[ref]['tapped'] = True
            if refs is None or choice != 'pay':
                source['zone'] = 'graveyard'
        self.emit('upkeep_step', player=self.active)
        self.receipts()
        self.emit('turn', player=self.active, value=0)

    def next_own_upkeep(self, choice='pay', *, replay=False):
        self.visit()
        self.visit(choice, replay=replay)

    def transfer(self):
        self.emit('require_can_activate', ref='trader', value=1)
        self.emit('choose_player', player=self.owner, value=self.other)
        self.emit('choose_target', player=self.owner, ref='source')
        self.emit('activate', ref='trader', value=1)
        self.emit('assert_stack', value=1)
        self.resolve()
        self.entities['trader']['tapped'] = True
        self.entities['source']['controller'] = self.other
        self.emit('bind_permanent', ref='source', player=self.other, value=self.rows[self.name]['csvid'])
        self.emit('require_card', ref='source', property='in_play', value=1)
        self.receipts()

    def combat(self, attacker='source', blocker=None, *, allowed=True):
        entity = self.entities[attacker]
        self.emit('begin_attackers')
        self.emit('attack', ref=attacker, value=int(allowed))
        self.observe(attacker,'attacking',allowed)
        self.emit('finish_attackers')
        if not allowed:
            self.receipts()
            return
        self.emit('begin_blockers')
        blocked = False
        entity['tapped'] = True
        if blocker:
            defense = self.entities[blocker]
            attack_keys, defense_keys = entity['keywords'], defense['keywords']
            blocked = ('Shadow' in attack_keys)==('Shadow' in defense_keys) and (
                'Flying' not in attack_keys or 'Flying' in defense_keys or 'Reach' in defense_keys)
            self.emit('block', ref=blocker, other=attacker, value=int(blocked))
            self.observe(blocker,'blocking_count',blocked)
        self.emit('advance_combat')
        self.emit('combat_damage')
        self.resolve()
        self.emit('end_combat')
        if blocked:
            for recipient, dealer in ((entity,defense),(defense,entity)):
                recipient['damage'] += dealer['power']
                if recipient['damage']>=recipient['toughness'] or 'Deathtouch' in dealer['keywords']:
                    recipient['zone'] = 'graveyard'
        else:
            self.life[1-self.active] -= entity['power']
        self.receipts()

    def setup_counted(self):
        mode = self.mode
        if mode in ('cast_payment','cast_short','cast_wrong_color','fresh_attack','opponent_upkeep'):
            return
        if mode.startswith('zero_'):
            if mode=='zero_foe':
                self.add('Goblin Tinkerer','opposing_ally',self.other)
            if mode=='zero_noncreature':
                self.add('Darksteel Relic','artifact_witness',self.owner)
            self.add_lands(self.owner,[1,0,0,0,0,0])
            return
        if mode!='sacrifice_indestructible':
            total = 2 if mode in ('count_two','count_two_short','count_removed') else 1
            for index in range(total):
                self.add('Goblin Tinkerer',f'ally_{index}',self.owner)
        lands = 0 if mode in ('short','opponent_funds','sacrifice_indestructible') else 2 if mode=='count_two' else 1
        mana_source = [0,0,0,0,1,0] if mode=='generic_colored' else [lands,0,0,0,0,0]
        self.add_lands(self.owner,mana_source)
        if mode in ('control_pay','control_short'):
            self.add('Goblin Tinkerer','recipient_ally',self.other)
        if mode in ('control_pay','opponent_funds'):
            self.add_lands(self.other,[1,0,0,0,0,0])

    def setup_cards(self):
        mode = self.mode
        fee = self.model['upkeep_mana']
        self.add(self.name,'source',self.owner,'hand')
        if self.model.get('fee_per_other_creature'):
            self.setup_counted()
        elif mode not in ('cast_payment','cast_short','cast_wrong_color','fresh_attack','opponent_upkeep',
                          'sacrifice_indestructible','hidden_short','hidden_choice','reveal_short','reveal_wrong_color'):
            if mode in ('short','reveal_then_short'):
                funding = short_fee(fee)
            elif mode=='wrong_color':
                funding = wrong_fee(fee,4)
            elif mode=='opponent_funds':
                funding = [0]*6
            else:
                funding = fee
            self.add_lands(self.owner,funding)
        if mode in ('opponent_funds','control_pay') and not self.model.get('fee_per_other_creature'):
            self.add_lands(self.other,fee)
        if mode in ('control_pay','control_short'):
            self.add('Bazaar Trader','trader',self.owner)
        if mode=='repeat_short':
            self.add('Stone Rain','removal',self.owner,'hand')
        if mode in ('destroy_before','sacrifice_indestructible','count_removed'):
            self.add('Murder','removal',self.other,'hand')
        if mode=='sacrifice_indestructible':
            self.add('Avacyn, Angel of Hope','protector',self.owner)
        witness = {'attack_ground':'Goblin Tinkerer','defend_ground':'Goblin Tinkerer',
                   'attack_reach':'Deadly Recluse','attack_flyer':'Spire Golem','defend_flyer':'Spire Golem',
                   'attack_shadow':'Dauthi Marauder','defend_shadow':'Dauthi Marauder'}.get(mode)
        if witness:
            self.add(witness,'witness',self.other)

    def play_morph(self):
        mode = self.mode
        self.cast('source','battlefield',down=True,hidden_choice=mode=='hidden_choice',
                  denied='short' if mode=='hidden_short' else None)
        if mode in ('hidden','hidden_choice','hidden_short'):
            return
        if mode in ('hidden_upkeep','hidden_combat','reveal_after_upkeep'):
            self.next_own_upkeep()
            if mode=='hidden_combat':
                self.combat()
            if mode!='reveal_after_upkeep':
                return
        if mode=='reveal_opposing':
            self.visit()
        if mode=='reveal_replay':
            self.snapshot = len(self.steps)
        denied = {'reveal_short':'short','reveal_wrong_color':'wrong_color'}.get(mode)
        self.reveal(denied)
        if denied is None:
            if mode=='reveal_opposing':
                self.visit()
            else:
                self.next_own_upkeep()
            if mode=='reveal_pay':
                self.combat()
        self.receipts()

    def play(self):
        mode = self.mode
        self.setup_cards()
        self.emit('turn', player=self.active, value=0)
        self.emit('present_ai_dialogs', value=1)
        for player in (0,1):
            for color in range(6):
                self.emit('set_mana', player=player, color=color, value=0)
        self.emit('refresh')
        if mode in MORPH_MODES:
            self.play_morph()
            return
        if mode in ('in_hand','cast_after_upkeep'):
            self.next_own_upkeep()
            if mode=='in_hand':
                return
        denied = {'cast_short':'short','cast_wrong_color':'wrong_color'}.get(mode)
        self.cast('source','battlefield',denied=denied)
        if mode in ('cast_payment','cast_short','cast_wrong_color'):
            return
        if mode=='fresh_attack':
            self.combat(allowed=False)
        elif mode=='opponent_upkeep':
            self.visit()
        elif mode in ('control_pay','control_short'):
            self.transfer()
            self.visit()
            self.visit()
        elif mode in ('destroy_before','sacrifice_indestructible'):
            self.cast('removal','graveyard',target='source',target_zone='graveyard' if mode=='destroy_before' else None)
            self.next_own_upkeep()
        elif mode=='count_removed':
            self.cast('removal','graveyard',target='ally_1',target_zone='graveyard')
            self.next_own_upkeep()
        elif mode.startswith('defend_'):
            self.visit()
            self.combat(attacker='witness',blocker='source')
        else:
            self.next_own_upkeep(mode if mode in ('decline','cancel') else 'pay',replay=mode=='untap_replay')
            if mode in ('pay_attack','attack_ground','attack_reach','attack_flyer','attack_shadow'):
                self.combat(blocker='witness' if 'witness' in self.entities else None)
            if mode in ('repeat_pay','repeat_decline','repeat_short','paid_replay'):
                if mode=='repeat_short':
                    loss = next(ref for ref,color in self.lands.items() if color or self.model.get('fee_per_other_creature'))
                    self.cast('removal','graveyard',target=loss,target_zone='graveyard')
                if mode=='paid_replay':
                    self.snapshot = len(self.steps)
                self.next_own_upkeep('decline' if mode=='repeat_decline' else 'pay')
        self.receipts()


def expand(name, csvid, declaration, model, selected, registry, oracle):
    from recipes import step

    validate_witnesses(oracle,registry)
    if pins()['sources'].get(name,{}).get('registry',{}).get('csvid')!=csvid:
        raise ValueError('unreserved upkeep source')
    cases = []
    for variant in selected:
        mode, owner = variant.rsplit('_',1)
        scene = Scene(name,model,mode,int(owner),registry,oracle)
        scene.play()
        steps = scene.steps
        if scene.snapshot is not None:
            cut = scene.snapshot
            tail = steps[cut:]
            steps = (steps[:cut]+[step('capture')]+tail+[step('record_state'),step('ai_roundtrip'),step('restore')]
                     +tail+[step('assert_recorded_state')])
        meta = dict(family=FAMILY,version=2,variant=variant,params=declaration['params'],
                    oracle_text=declaration['oracle_text'],untested_components=[
                        'Priority between upkeep trigger placement and resolution',
                        'Multiple simultaneous instances and additional/skipped upkeep steps',
                        'Cancellation after a partially attempted mana payment'])
        if model.get('fee_per_other_creature'):
            meta['untested_components'].append('Refusing the zero-mana upkeep and voluntary refusal on AI auto-payment paths')
        if 'morph' in model:
            meta['untested_components'].append('Direct Mutant subtype receipt and Morph with another object pending')
        cases.append(dict(name=f'UpkeepPayOrSacrificeCreatureRecipe_{csvid}_{variant}',card=name,
                          fixture='empty_main',seed=123,setup=scene.setup,steps=steps,_recipe=meta))
    return cases
