"""Printed Haste and a permanent's real end-step return to its owner."""
from collections import Counter
from functools import lru_cache
import json
from pathlib import Path
import re

from death_counter_trigger_recipes import exact


FAMILY = 'haste_end_step_return'
PROFILE = Path(__file__).parent / 'data/haste_end_step_return_wave8_profiles.json'
HASTE = 'Haste (This creature can attack and {T} as soon as it comes under your control.)'
RETURN = "At the beginning of the end step, return this creature to its owner's hand."
RETURN_REMINDER = " (Return it only if it's on the battlefield.)"
DEVOID = 'Devoid (This card has no color.)'
CONDITIONAL_RETURN = ("At the beginning of each end step, if you control no other colorless creatures, "
                      "return this creature to its owner's hand.")
COMMON = ('fresh_attack','fresh_comparison','no_attack','opposing_end','ground_block','reach_blocks',
          'spell_death','early_bounce','single_recast','donated','pending_replay','end_replay','twice_end',
          'unfunded','wrong_color','countered')
COLOR_WITNESSES = {
    'own_colorless': ('Walking Atlas',False),
    'opposing_colorless': ('Walking Atlas',False),
    'foe_colorless': ('Walking Atlas',True),
    'colored_only': ('Goblin Tinkerer',False),
    'noncreature_only': ('Darksteel Relic',False),
    'witness_lost': ('Walking Atlas',False),
    'donate_retains': ('Walking Atlas',True),
    'donate_returns': ('Walking Atlas',False),
    'spell_colorless': ("Kozilek's Sentinel",False),
}
COLOR_BITS = {'B':2,'U':4,'G':8,'R':16,'W':32}


@lru_cache(maxsize=1)
def pins():
    return json.loads(PROFILE.read_text(encoding='utf8'))


def parameters(record):
    lines = record.get('oracle_text','').splitlines()
    if lines == [DEVOID,'Haste',CONDITIONAL_RETURN]:
        if (record.get('layout')!='normal' or record.get('card_faces') or record.get('selected_face') or
                not record.get('type_line','').startswith('Creature — ') or record.get('colors')!=[] or
                set(record.get('keywords',[]))!={'Devoid','Haste'} or
                not re.fullmatch(r'\{[1-9]\}\{B\}\{R\}',record.get('mana_cost','')) or
                any(not re.fullmatch('[1-9]',record.get(k,'')) for k in ('power','toughness'))):
            raise ValueError('complete normal Devoid, Haste and conditional end-step return required')
        return dict(schema=2,flying=False,printed_mana=record['mana_cost'],power=int(record['power']),
                    toughness=int(record['toughness']),devoid=True,other_colorless_gate=True)
    if (record.get('layout')!='normal' or record.get('card_faces') or record.get('selected_face') or
            not record.get('type_line','').startswith('Creature — ') or len(lines)!=2 or
            lines[0] not in ('Haste',HASTE,'Flying, haste') or lines[1] not in (RETURN,RETURN+RETURN_REMINDER) or
            not re.fullmatch(r'(?:\{[1-9]\})?\{R\}(?:\{R\})?',record.get('mana_cost','')) or
            any(not re.fullmatch('[1-9]',record.get(k,'')) for k in ('power','toughness'))):
        raise ValueError('complete normal red Haste and unconditional end-step return required')
    flying = lines[0]=='Flying, haste'
    if set(record.get('keywords',[]))!=({'Haste','Flying'} if flying else {'Haste'}):
        raise ValueError('printed intrinsic keywords must match the complete clause')
    return dict(schema=1,flying=flying,printed_mana=record['mana_cost'],
                power=int(record['power']),toughness=int(record['toughness']))


def variants(model):
    modes = COMMON+('flying_blocker','blocking_flyer') if model['flying'] else COMMON
    if model.get('other_colorless_gate'):
        modes += tuple(COLOR_WITNESSES)
    return [f'{mode}_{owner}' for owner in (0,1) for mode in modes]


def spec(declaration, record):
    from recipes import cost_from_oracle

    model = parameters(record)
    if (declaration.get('family')!=FAMILY or type(declaration.get('version')) is not int or
            declaration['version']!=2 or declaration.get('oracle_text')!=record['oracle_text'] or
            not exact(declaration.get('params'),model)):
        raise ValueError('typed whole Haste return declaration required')
    selected = declaration.get('variants')
    if (not isinstance(selected,list) or not selected or any(not isinstance(v,str) for v in selected) or
            len(set(selected))!=len(selected) or set(selected)-set(variants(model))):
        raise ValueError('explicit distinct supported Haste return variants required')
    return dict(model,family=FAMILY,mana=cost_from_oracle(record))


def validate_witnesses(oracle, registry):
    data = pins()
    for group in ('sources','witnesses'):
        for name, pin in data[group].items():
            record, row = oracle.get(name,{}), registry.get(name,{})
            if (not exact(record,pin['oracle']) or not exact(row,pin['registry']) or record.get('layout')!='normal' or
                    row.get('kind')!='card' or row.get('handler') in (None,'NULL','0','card_dummy')):
                raise ValueError(f'implemented normal Haste return {group} identity changed: {name}')


class Scene:
    def __init__(self, name, owner, mode, model, registry, oracle):
        self.name, self.owner, self.enemy, self.mode = name, owner, 1-owner, mode
        self.model, self.registry, self.oracle = model, registry, oracle
        self.csvid = registry[name]['csvid']
        self.steps, self.setup, self.entities = [], [], {}
        self.life = [20,20]
        self.mana = [[0]*6 for _ in (0,1)]
        self.active, self.snapshot = owner, None

    def emit(self, op, **fields):
        from recipes import step

        self.steps.append(step(op,**fields))

    def add(self, ref, name, who, zone='hand'):
        from recipes import card

        self.setup.append(card(name,ref,who,zone))
        record = self.oracle[name]
        self.entities[ref] = dict(name=name,owner=who,actor=who,zone=zone,tapped=False,damage=0,
                                  power=int(record.get('power') or 0),toughness=int(record.get('toughness') or 0),
                                  keywords=record['keywords'])

    def prop(self, ref, prop, value, required=False):
        self.emit('require_card' if required else 'assert_card',ref=ref,property=prop,value=int(value))

    def permanent(self, ref):
        entity = self.entities[ref]
        alive = entity['zone']=='battlefield'
        self.prop(ref,'in_play',alive)
        if alive:
            card = self.oracle[entity['name']]
            props = ('power','toughness','damage','tapped') if 'Creature' in card['type_line'] else ('tapped',)
            for prop in props:
                self.prop(ref,prop,entity[prop])
            if self.model.get('devoid') and ref in ('source','color_witness'):
                self.emit('assert_matching_permanents',player=entity['actor'],
                          index=self.registry[entity['name']]['csvid'],property='color',
                          value=sum(COLOR_BITS[color] for color in card['colors']))

    def pools(self):
        for who, values in enumerate(self.mana):
            for color, amount in enumerate(values):
                self.emit('assert_mana',player=who,color=color,value=amount)

    def fund(self, who, values):
        if any(self.mana[who]):
            raise ValueError('old mana must be spent before adding the next fixture payment')
        self.mana[who] = list(values)
        for color, amount in enumerate(values):
            if amount:
                self.emit('set_mana',player=who,color=color,value=amount)
        self.emit('refresh')

    def cast(self, ref, target=None, *, response=False, funding=None, success=True):
        from recipes import cost_from_oracle

        entity = self.entities[ref]
        who = entity['actor']
        printed = cost_from_oracle(self.oracle[entity['name']])
        self.fund(who,printed if funding is None else funding)
        # Plain creature casting does not use its card's EVENT_CAN_CAST
        # predicate. The actual cast is the payment/permission receipt.
        if target is not None and success and not response:
            self.emit('require_can_cast',ref=ref,value=1)
        if target:
            self.emit('choose_target',player=who,ref=target)
        self.emit('cast_response' if response else 'cast',ref=ref,value=int(success))
        self.emit('assert_choices_consumed')
        self.emit('assert_stack',value=2 if response else int(success))
        if success:
            self.prop(ref,'in_hand',0,required=True)
            self.mana[who] = [a-b for a,b in zip(self.mana[who],printed)]
        self.pools()

    def resolve(self, response=False):
        self.emit('resolve_response' if response else 'resolve')
        self.emit('refresh')
        self.emit('assert_choices_consumed')
        self.emit('assert_stack',value=0)

    def enter(self, ref):
        self.resolve()
        self.entities[ref]['zone'] = 'battlefield'
        self.prop(ref,'in_play',1,required=True)
        self.permanent(ref)

    def die_or_bounce(self, ref, zone):
        self.cast('removal',ref)
        self.resolve()
        self.entities['removal']['zone'] = 'graveyard'
        self.entities[ref]['zone'] = zone
        self.permanent(ref)

    def transfer(self):
        self.emit('require_can_activate',ref='trader',value=1)
        self.emit('choose_player',player=self.owner,value=self.enemy)
        self.emit('choose_target',player=self.owner,ref='source')
        self.emit('activate',ref='trader',value=1)
        self.emit('assert_stack',value=1)
        self.resolve()
        self.entities['trader']['tapped'] = True
        self.entities['source']['actor'] = self.enemy
        self.emit('bind_permanent',ref='source',player=self.enemy,value=self.csvid)
        self.prop('source','in_play',1,required=True)
        self.receipts()

    def hit(self, recipient, sender):
        target, dealer = self.entities[recipient], self.entities[sender]
        target['damage'] += dealer['power']
        if target['damage']>=target['toughness'] or (dealer['power']>0 and 'Deathtouch' in dealer['keywords']):
            target['zone'] = 'graveyard'

    def combat(self, *, attacker='source', blocker=None):
        a = self.entities[attacker]
        self.emit('begin_attackers')
        if self.mode=='fresh_comparison':
            self.emit('attack',ref='normal',value=0)
            self.prop('normal','attacking',0)
        self.emit('attack',ref=attacker,value=1)
        self.prop(attacker,'attacking',1)
        a['tapped'] = True
        self.emit('finish_attackers')
        self.emit('begin_blockers')
        legal = False
        if blocker:
            keywords = set(self.entities[blocker]['keywords'])
            legal = 'Flying' not in a['keywords'] or bool(keywords & {'Flying','Reach'})
            self.emit('block',ref=blocker,other=attacker,value=int(legal))
            self.prop(blocker,'blocking_count',int(legal))
        self.emit('advance_combat')
        self.emit('combat_damage')
        self.resolve()
        self.emit('end_combat')
        if legal:
            self.hit(attacker,blocker)
            self.hit(blocker,attacker)
        else:
            self.life[1-a['actor']] -= a['power']
        self.receipts()

    def end(self, advance=False):
        if any(map(any,self.mana)):
            raise ValueError('end-step fixtures use spent mana pools')
        self.emit('advance_turn' if advance else 'cleanup')
        self.emit('refresh')
        source = self.entities['source']
        stays = self.model.get('other_colorless_gate') and any(
            ref!='source' and entity['zone']=='battlefield' and entity['actor']==source['actor'] and
            'Creature' in self.oracle[entity['name']]['type_line'] and self.oracle[entity['name']]['colors']==[]
            for ref,entity in self.entities.items())
        if source['zone']=='battlefield' and not stays:
            source.update(zone='hand',actor=source['owner'])
        if self.mode=='spell_colorless':
            witness = self.entities['color_witness']
            witness['power'] = int(self.oracle[witness['name']]['power'])
        for entity in self.entities.values():
            entity['damage'] = 0
        if advance:
            self.active = 1-self.active
            self.receipts()

    def new_main(self):
        self.emit('untap_step',player=self.active)
        self.emit('upkeep_step',player=self.active)
        self.emit('turn',player=self.active,value=0)
        for entity in self.entities.values():
            if entity['zone']=='battlefield' and entity['actor']==self.active:
                entity['tapped'] = False

    def setup_cards(self):
        m = self.mode
        source_zone = 'battlefield' if m in ('opposing_end','opposing_colorless','blocking_flyer') else 'hand'
        self.add('source',self.name,self.owner,source_zone)
        if m in ('fresh_comparison','spell_colorless'):
            self.add('normal','Goblin Tinkerer',self.owner)
        if m=='ground_block':
            self.add('blocker','Goblin Tinkerer',self.enemy,'battlefield')
        if m=='reach_blocks':
            self.add('blocker','Deadly Recluse',self.enemy,'battlefield')
        if m in ('flying_blocker','blocking_flyer'):
            self.add('flyer','Spire Golem',self.enemy,'battlefield')
        if m in ('donated','donate_retains','donate_returns'):
            self.add('trader','Bazaar Trader',self.owner,'battlefield')
        if m in ('spell_death','early_bounce'):
            self.add('removal','Murder' if m=='spell_death' else 'Unsummon',self.enemy)
        if m=='countered':
            self.add('counter','Essence Scatter',self.enemy)
        if m in COLOR_WITNESSES:
            name, opposing = COLOR_WITNESSES[m]
            self.add('color_witness',name,self.enemy if opposing else self.owner,'battlefield')
        if m=='witness_lost':
            self.add('removal','Murder',self.enemy)

    def play(self):
        m = self.mode
        self.setup_cards()
        self.active = self.enemy if m in ('opposing_end','opposing_colorless','blocking_flyer') else self.owner
        self.emit('turn',player=self.active,value=0)
        self.emit('present_ai_dialogs',value=1)
        for who in (0,1):
            for color in range(6):
                self.emit('set_mana',player=who,color=color,value=0)
        self.emit('refresh')
        if m in ('wrong_color','unfunded'):
            amounts = [sum(self.model['mana']) if m=='wrong_color' else 0,0,0,0,0,0]
            self.cast('source',funding=amounts,success=False)
            if m=='unfunded':
                self.end()
            return
        if m=='spell_colorless':
            self.cast('normal')
            self.emit('announce_spell',ref='normal')
            self.enter('normal')
            self.permanent('color_witness')
        if self.entities['source']['zone']=='hand':
            self.cast('source')
            if m=='pending_replay':
                self.snapshot = len(self.steps)
            if m=='countered':
                self.emit('announce_spell',ref='source')
                self.cast('counter',response=True)
                self.resolve(response=True)
                self.entities['counter']['zone'] = self.entities['source']['zone'] = 'graveyard'
                self.end()
                return
            if m=='spell_colorless':
                self.emit('announce_spell',ref='source')
            self.enter('source')
        elif self.model.get('devoid'):
            self.permanent('source')
        if m=='spell_colorless':
            self.entities['color_witness']['power'] += 1
            self.permanent('color_witness')
        if m in COLOR_WITNESSES and m!='spell_colorless':
            self.permanent('color_witness')
        if m=='fresh_comparison':
            self.cast('normal')
            self.enter('normal')
        if m in ('fresh_attack','fresh_comparison','pending_replay','end_replay','single_recast'):
            self.combat()
        if m in ('ground_block','reach_blocks'):
            self.combat(blocker='blocker')
        if m=='flying_blocker':
            self.combat(blocker='flyer')
        if m=='blocking_flyer':
            self.combat(attacker='flyer',blocker='source')
        if m in ('donated','donate_retains','donate_returns'):
            self.transfer()
        if m in ('spell_death','early_bounce'):
            self.die_or_bounce('source','graveyard' if m=='spell_death' else 'hand')
        if m=='witness_lost':
            self.die_or_bounce('color_witness','graveyard')
        if m=='end_replay':
            self.snapshot = len(self.steps)
        if m in ('single_recast','twice_end'):
            self.end(advance=True)
            self.new_main()
            self.end(advance=True)
            self.new_main()
            if m=='twice_end':
                return
            self.emit('bind_hand',ref='source',player=self.owner,value=self.csvid)
            self.prop('source','in_hand',1,required=True)
            self.entities['source']['tapped'] = False
            self.cast('source')
            self.enter('source')
            self.combat()
        self.end()

    def receipts(self):
        self.emit('assert_stack',value=0)
        self.emit('assert_choices_consumed')
        self.pools()
        for ref in self.entities:
            self.permanent(ref)
        for who, life in enumerate(self.life):
            self.emit('assert_life',player=who,value=life)
            for zone in ('hand','battlefield','graveyard','exile','library'):
                counts = Counter(x['name'] for x in self.entities.values() if x['zone']==zone and
                                 (x['actor'] if zone=='battlefield' else x['owner'])==who)
                self.emit('assert_zone',player=who,zone=zone,value=sum(counts.values()))
                for name, value in counts.items():
                    self.emit('assert_zone_multiplicity',player=who,zone=zone,index=self.registry[name]['csvid'],value=value)


def expand(name, csvid, declaration, model, selected, registry, oracle):
    from recipes import step

    if pins()['sources'].get(name,{}).get('registry',{}).get('csvid')!=csvid:
        raise ValueError('unreserved Haste return source')
    cases = []
    for variant in selected:
        mode, owner = variant.rsplit('_',1)
        scene = Scene(name,int(owner),mode,model,registry,oracle)
        scene.play()
        scene.receipts()
        steps = scene.steps
        if scene.snapshot is not None:
            cut = scene.snapshot
            tail = steps[cut:]
            steps = (steps[:cut]+[step('capture')]+tail+[step('record_state'),step('ai_roundtrip'),step('restore')]
                     +tail+[step('assert_recorded_state')])
        metadata = dict(family=FAMILY,version=2,variant=variant,params=declaration['params'],
                        oracle_text=declaration['oracle_text'],untested_components=[
                            'Independent priority within the end step before cleanup',
                            'Multiple simultaneous return triggers and multiplayer'])
        if model.get('devoid'):
            metadata['untested_components'].append('Color queries for source cards in hand, library, graveyard and exile')
        cases.append(dict(name=f'HasteEndStepReturnRecipe_{csvid}_{variant}',card=name,fixture='empty_main',seed=123,
                          setup=scene.setup,steps=steps,_recipe=metadata))
    return cases
