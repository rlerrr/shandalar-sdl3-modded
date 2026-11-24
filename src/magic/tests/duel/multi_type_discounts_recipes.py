"""Oracle type discounts verified by actual payments and cast-trigger effects."""

from functools import lru_cache
import json
from pathlib import Path
import re

from death_counter_trigger_recipes import exact
from morph_turn_up_target_recipes import Scene


FAMILY = 'multi_type_discounts'
PROFILE = Path(__file__).parent/'data/multi_type_discounts_wave8_profiles.json'
DISCOUNT = re.compile(r'(Creature|Enchantment|Instant and sorcery|Instant and enchantment) spells you cast cost \{([1-9][0-9]*)\} less to cast\.')
COLOR_NAMES = {'White':'W','Blue':'U','Black':'B','Red':'R','Green':'G'}
COLOR_DISCOUNT = re.compile(r'(White|Blue|Black|Red|Green) spells and (white|blue|black|red|green) spells you cast cost \{1\} less to cast\.')
REGENERATION = re.compile(r'(\{1\}\{B\}): Regenerate this creature\.')
COLOR_PROBE = {'W':"Angel's Mercy",'U':'Divination','B':'Greed','R':'Ghitu War Cry','G':'Centaur Glade'}
OVERLAP_PROBE = {'RU':'Mercurial Chemister','BW':'Teysa, Envoy of Ghosts','GU':'Fleetfeather Cockatrice',
                 'RW':'Firemane Angel','BG':'Golgari Rotwurm'}
PASSIVE_CLAUSES = {'Flying':'Flying','Defender (This creature can\x27t attack.)':'Defender','First strike':'First strike'}
FAMILIAR_MODES = ('first_color','second_color','both_colors','both_payment','nonmatching_color','colorless',
                  'opposing_caster','source_cast','source_short','source_grave','source_bounce','combined',
                  'countered','paid_replay','wrong_color','one_short')
REGEN_MODES = ('regenerate_paid','regenerate_short','regenerate_wrong_color','regenerate_forbidden','regenerate_proactive')
KEYWORD_MODES = {'Flying':('ground_denied','flying_block'), 'Defender':('defender_attack','defender_block'),
                 'First strike':('first_strike_attack','first_strike_block')}
RIDER = {'Creatures you control get -1/-1.':'shrink',
         'Whenever you cast an enchantment spell, you gain 1 life.':'life',
         'Whenever you cast an instant or sorcery spell, scry 1.':'scry'}
RIDER_TYPES = {'shrink':['Creature'],'life':['Enchantment'],'scry':['Instant','Sorcery']}
TYPE_PROBE = {'Instant':"Angel's Mercy",'Sorcery':'Divination','Creature':'Unyielding Krumar',
              'Enchantment':'Centaur Glade','Artifact':'Mind Stone'}
FLOOR_PROBE = {'Instant':'Holy Day','Creature':'Goblin Tinkerer','Enchantment':'Font of Vigor'}
PAIR = {'life':'Mana Matrix','scry':'Goblin Electromancer','shrink':'Planar Gate'}
STOCK = ('Mogg Fanatic','Elvish Mystic','Forest','Island','Mountain','Swamp')
MODES = ('payment','one_short','wrong_color','opposing_caster','source_cast','source_short','source_grave',
         'source_bounce','combined','countered','paid_replay','colored_floor','artifact_creature','enchantment_creature')
GAPS = ('Separate response window within a synchronous legacy cast-trigger announcement','Multiplayer')


@lru_cache(maxsize=1)
def pins():
    return json.loads(PROFILE.read_bytes())


def card_types(record):
    return set(record['type_line'].partition(' — ')[0].split())


def parameters(record):
    from recipes import cost_from_oracle

    if (not record or record.get('layout')!='normal' or record.get('card_faces') or record.get('selected_face')
            or card_types(record) not in ({'Creature'},{'Artifact'},{'Enchantment'})):
        raise ValueError('plain normal permanent with complete cost-reduction clauses required')
    lines = record['oracle_text'].splitlines()
    color = next((m for line in lines if (m := COLOR_DISCOUNT.fullmatch(line))),None)
    if color:
        return familiar_parameters(record,lines,color)
    match = DISCOUNT.fullmatch(lines[0]) if 1<=len(lines)<=2 else None
    if not match:
        raise ValueError('complete generic discount over supported spell types required')
    types = [name.title() for name in match[1].split(' and ')]
    amount = int(match[2])
    rider = RIDER.get(lines[1]) if len(lines)==2 else 'none'
    keyword = ['Scry'] if rider=='scry' else []
    if (amount>2 or rider is None or rider in RIDER_TYPES and types!=RIDER_TYPES[rider]
            or record.get('keywords')!=keyword):
        raise ValueError('unreviewed whole discount/rider combination')
    return dict(effect='static_discount',cost=cost_from_oracle(record),types=types,amount=amount,rider=rider)


def familiar_parameters(record,lines,color):
    from recipes import cost_from_oracle

    clause = color[0]
    keyword = ''
    regen = None
    if len(lines)==2 and lines[0] in PASSIVE_CLAUSES and lines[1]==clause:
        keyword = PASSIVE_CLAUSES[lines[0]]
    elif len(lines)==2 and lines[0]==clause and (m := REGENERATION.fullmatch(lines[1])):
        keyword = 'Regenerate'
        regen = cost_from_oracle(dict(record,mana_cost=m[1]))
    elif lines!=[clause]:
        raise ValueError('complete Familiar ability and keyword wording required')
    colors = [COLOR_NAMES[key.title()] for key in color.groups()]
    if (card_types(record)!={'Creature'} or len(set(colors))!=2 or record.get('keywords')!=([keyword] if keyword else [])
            or ''.join(sorted(colors)) not in OVERLAP_PROBE):
        raise ValueError('reviewed distinct printed Familiar colors and keyword required')
    return dict(effect='color_discount',cost=cost_from_oracle(record),colors=colors,amount=1,rider='none',
                keyword=keyword,regeneration_cost=regen)


def variants(params):
    if params['effect']=='color_discount':
        extra = REGEN_MODES if params['regeneration_cost'] else KEYWORD_MODES.get(params['keyword'],())
        return [f'{mode}_{actor}' for mode in FAMILIAR_MODES+extra for actor in (0,1)]
    modes = list(MODES)
    modes += [f'{"eligible" if t in params["types"] else "excluded"}_{t.lower()}' for t in TYPE_PROBE]
    if 'Instant' in params['types']:
        modes += ['offturn_instant']
    if 'Creature' in params['types']:
        modes += ['zero_total']
    if params['rider']=='shrink':
        modes += ['state_based_death']
    if params['rider']=='scry':
        modes += ['scry_bottom','scry_empty']
    return [f'{mode}_{actor}' for mode in modes for actor in (0,1)]


def spec(declaration,record):
    model = parameters(record)
    pin = pins()['sources'].get(record['name'],{})
    selected = declaration.get('variants')
    if (not exact(pin.get('oracle'),record) or declaration.get('family')!=FAMILY
            or type(declaration.get('version')) is not int or declaration['version']!=2
            or declaration.get('oracle_text')!=record['oracle_text'] or not exact(declaration.get('params'),model)
            or type(selected) is not list or not selected or any(type(v) is not str for v in selected)
            or len(set(selected))!=len(selected) or set(selected)-set(variants(model))):
        raise ValueError('pinned whole discount, exact typed fee, and distinct variants required')
    return dict(model,family=FAMILY)


def validate_witnesses(oracle,rows):
    profile = pins()
    if profile['version']!=1:
        raise ValueError('unreviewed type-discount profile')
    for group in ('sources','witnesses'):
        for name,pin in profile[group].items():
            row,record = rows.get(name,{}),oracle.get(name,{})
            if (not exact(pin,dict(oracle=record,registry=row)) or record.get('layout')!='normal'
                    or row.get('kind')!='card' or row.get('handler') in (None,'NULL','0','card_dummy')):
                raise ValueError(f'implemented whole source or witness changed: {name}')
    for kind,name in TYPE_PROBE.items():
        if card_types(oracle[name])!={kind}:
            raise ValueError(f'single spell type witness required: {name}')


def eligible(record,reducer):
    if reducer['effect']=='color_discount':
        return bool(set(record['colors']).intersection(reducer['colors']))
    return bool(card_types(record).intersection(reducer['types']))


def discounted_cost(record,reducers):
    from recipes import cost_from_oracle

    costs = cost_from_oracle(record)
    costs[0] = max(0,costs[0]-sum(p['amount'] for p in reducers if eligible(record,p)))
    return costs


class DiscountScene(Scene):
    def __init__(self,name,model,mode,owner,rows,oracle):
        super().__init__(name,model,mode,owner,rows,oracle)
        self.actor = self.foe if mode=='opposing_caster' else owner
        self.active = self.foe if mode in ('opposing_caster','offturn_instant') else owner
        self.library = [[],[]]

    def probe_name(self):
        if self.mode.startswith(('eligible_','excluded_')):
            return TYPE_PROBE[self.mode.split('_',1)[1].title()]
        if self.mode in ('offturn_instant','scry_bottom','scry_empty'):
            return TYPE_PROBE['Instant']
        if self.mode=='colored_floor':
            return FLOOR_PROBE[self.model['types'][0]]
        probes = {'artifact_creature':'Golem Artisan','enchantment_creature':'Lucent Liminid',
                  'zero_total':'Runed Servitor','state_based_death':'Mogg Fanatic'}
        return probes.get(self.mode,TYPE_PROBE[self.model['types'][0]])

    def setup_cards(self):
        mode = self.mode
        source_zone = 'hand' if mode in ('source_cast','source_short') else 'graveyard' if mode=='source_grave' else 'battlefield'
        self.add(self.name,'source',self.owner,source_zone)
        if mode=='combined':
            self.add(PAIR.get(self.model['rider'],self.name),'copy',self.owner)
        self.add('Goblin Tinkerer','own',self.owner)
        self.add('Goblin Tinkerer','enemy',self.foe)
        if mode!='source_short':
            self.add(self.probe_name(),'probe',self.actor,'hand')
        if mode=='source_bounce':
            self.add('Boomerang','bounce',self.foe,'hand')
        if mode=='countered':
            self.add('Counterspell','counter',self.foe,'hand')
        for player in (0,1):
            stock = () if mode=='scry_empty' and player==self.owner else STOCK
            for i,name in enumerate(stock):
                ref = f'library_{player}_{i}'
                self.add(name,ref,player,'library')
                self.library[player].append(ref)

    def reducers(self,controller):
        return [parameters(self.oracle[e['name']]) for ref,e in self.entities.items() if ref in ('source','copy')
                and e['zone']=='battlefield' and e['controller']==controller]

    def stats(self,ref):
        power,toughness = super().stats(ref)
        shrink = sum(p['rider']=='shrink' for p in self.reducers(self.entities[ref]['controller']))
        return power-shrink,toughness-shrink

    def source_traits(self,full=False):
        self.prop('source','csvid',self.csvid)
        if full:
            self.emit('assert_matching_permanents',player=self.owner,index=self.csvid,property='artifact',
                      value=int('Artifact' in card_types(self.oracle[self.name])))

    def refresh_zero(self):
        self.emit('turn',player=self.active,value=0)
        for player in (0,1):
            for color in range(6):
                self.emit('set_mana',player=player,color=color,value=0)
        self.emit('present_ai_dialogs',value=1)
        self.emit('refresh')

    def ordered_library(self,player):
        for index,ref in enumerate(self.library[player]):
            self.emit('assert_zone_card',player=player,zone='library',index=index,
                      value=self.rows[self.entities[ref]['name']]['csvid'])

    def gates(self):
        self.receipts()
        for player in (0,1):
            self.ordered_library(player)

    def cast_paid(self,ref,budget=None,allowed=True,response=False):
        e = self.entities[ref]
        actor = e['owner']
        price = discounted_cost(self.oracle[e['name']],self.reducers(actor))
        if budget is not None or not any(self.mana[actor]):
            self.fund(actor,price if budget is None else budget)
        self.emit('cast_response' if response else 'cast',ref=ref,value=int(allowed))
        self.emit('assert_choices_consumed')
        self.emit('assert_stack',value=1+int(response) if allowed else 0)
        self.prop(ref,'in_hand',int(not allowed))
        if allowed:
            self.pay(actor,price)
            e['zone'] = 'stack'
        self.pools()

    def announce_probe(self):
        record = self.oracle[self.probe_name()]
        matching = [p['rider'] for p in self.reducers(self.actor) if eligible(record,p)]
        library = self.library[self.actor]
        scry = matching.count('scry')
        if scry>1:
            raise ValueError('a separate multi-trigger order fixture is required')
        if scry and library:
            self.emit('record_library',player=self.actor)
            if self.mode in ('scry_bottom','countered'):
                top = library.pop(0)
                library.append(top)
                self.emit('choose_zone_card',player=self.actor,value=self.rows[self.entities[top]['name']]['csvid'])
            else:
                self.emit('choose_zone',player=self.actor,value=-1)
        self.emit('announce_spell',ref='probe')
        self.emit('assert_choices_consumed')
        self.emit('assert_stack',value=1)
        if scry and library:
            self.emit('assert_scry_result',player=self.actor,value=1)
        self.life[self.actor] += matching.count('life')
        self.emit('assert_life',player=self.actor,value=self.life[self.actor])
        self.ordered_library(self.actor)

    def finish_probe(self):
        entity = self.entities['probe']
        record = self.oracle[entity['name']]
        self.resolve()
        if card_types(record).intersection(('Instant','Sorcery')):
            self.leave('probe','graveyard')
            life = re.fullmatch(r'You gain ([1-9][0-9]*) life\.',record['oracle_text'])
            if life:
                self.life[self.actor] += int(life[1])
            if record['oracle_text']=='Draw two cards.':
                for _ in range(2):
                    self.leave(self.library[self.actor].pop(0),'hand')
        else:
            entity['zone'] = 'battlefield'
            if 'Creature' in card_types(record) and self.stats('probe')[1]<=0:
                # Observe the actual state-based death, never a test kill.
                self.leave('probe','graveyard')
        self.gates()

    def begin_probe(self):
        from recipes import cost_from_oracle

        printed = cost_from_oracle(self.oracle[self.probe_name()])
        reduced = discounted_cost(self.oracle[self.probe_name()],self.reducers(self.actor))
        allowed = self.mode not in ('one_short','wrong_color')
        budget = printed if self.mode in ('payment','source_grave','source_bounce','opposing_caster','both_payment') else reduced.copy()
        if self.mode=='one_short':
            budget[next(i for i,n in enumerate(budget) if n)] -= 1
        elif self.mode=='wrong_color':
            color = next(i for i,n in enumerate(budget) if i and n)
            budget[0],budget[color] = budget[0]+budget[color],0
        if self.mode=='paid_replay':
            self.replay = len(self.steps)
        self.cast_paid('probe',budget=budget,allowed=allowed)
        if not allowed:
            self.gates()
            return
        self.announce_probe()
        if self.mode=='countered':
            # Counterspell uses the real announced parent automatically.
            self.cast_paid('counter',response=True)
            self.resolve(parent=True)
            self.leave('counter','graveyard')
            self.leave('probe','graveyard')
            self.gates()
            return
        self.finish_probe()

    def play(self):
        self.setup_cards()
        self.refresh_zero()
        self.gates()
        if self.mode in ('source_cast','source_short'):
            budget = self.model['cost'].copy()
            allowed = self.mode=='source_cast'
            if not allowed:
                budget[next(i for i,n in enumerate(budget) if n)] -= 1
            self.cast_paid('source',budget=budget,allowed=allowed)
            if not allowed:
                self.gates()
                return
            self.resolve()
            self.entities['source']['zone'] = 'battlefield'
            self.gates()
        if self.mode=='source_bounce':
            self.cast_spell('bounce','source')
            self.resolve()
            self.leave('bounce','graveyard')
            self.leave('source','hand')
            self.gates()
        self.begin_probe()


class FamiliarScene(DiscountScene):
    def probe_name(self):
        first, second = self.model['colors']
        if self.mode=='colorless':
            return TYPE_PROBE['Artifact']
        if self.mode in ('both_colors','both_payment'):
            return OVERLAP_PROBE[''.join(sorted((first,second)))]
        if self.mode=='nonmatching_color':
            return next(name for symbol,name in COLOR_PROBE.items() if symbol not in self.model['colors'])
        return COLOR_PROBE[second if self.mode=='second_color' else first]

    def setup_cards(self):
        mode = self.mode
        if mode in FAMILIAR_MODES:
            super().setup_cards()
            return
        self.add(self.name,'source',self.owner)
        if mode in REGEN_MODES:
            self.active = self.foe
            self.add('Terminate' if mode=='regenerate_forbidden' else 'Shock','lethal',self.foe,'hand')
            return
        if mode in ('defender_block','first_strike_block'):
            self.active = self.foe
        enemy = ('Mogg Fanatic' if mode.startswith('first_strike') else 'Welkin Tern' if mode=='flying_block' else 'Goblin Tinkerer')
        self.add(enemy,'enemy',self.foe)

    def regenerate(self):
        fee = self.model['regeneration_cost']
        budget = fee.copy()
        if self.mode=='regenerate_short':
            budget[0] -= 1
        elif self.mode=='regenerate_wrong_color':
            budget[0] += budget[1]
            budget[1] = 0
        self.fund(self.owner,budget)
        paid = self.mode in ('regenerate_paid','regenerate_proactive')
        proactive = self.mode=='regenerate_proactive'
        if proactive:
            self.emit('can_activate',ref='source',value=1)
            self.emit('activate',ref='source',value=1)
            self.emit('assert_stack',value=1)
            self.pay(self.owner,fee)
            self.pools()
            self.resolve()
        self.cast_spell('lethal','source')
        if paid and not proactive:
            # The production destruction window offers this actual activation.
            self.emit('choose_regeneration',ref='source')
            self.pay(self.owner,fee)
        self.resolve()
        self.leave('lethal','graveyard')
        if paid:
            self.entities['source']['tapped'] = True
        else:
            self.leave('source','graveyard')
        self.gates()

    def keyword_combat(self):
        defensive = self.mode in ('defender_block','first_strike_block')
        attacker, blocker = ('enemy','source') if defensive else ('source','enemy')
        allowed = self.mode!='defender_attack'
        self.emit('begin_attackers')
        self.emit('attack',ref=attacker,value=int(allowed))
        self.emit('finish_attackers')
        if not allowed:
            self.gates()
            return
        self.entities[attacker]['tapped'] = True
        self.emit('begin_blockers')
        blocked = self.mode!='ground_denied'
        self.emit('block',ref=blocker,other=attacker,value=int(blocked))
        self.emit('advance_combat')
        self.prop(attacker,'blocked',blocked)
        self.emit('combat_damage')
        self.resolve()
        self.emit('end_combat')
        if self.mode.startswith('first_strike'):
            self.leave('enemy','graveyard')
        elif not blocked:
            self.life[1-self.active] -= self.stats(attacker)[0]
        else:
            ap,at = self.stats(attacker)
            bp,bt = self.stats(blocker)
            for ref,damage,toughness in ((attacker,bp,at),(blocker,ap,bt)):
                if damage>=toughness:
                    self.leave(ref,'graveyard')
                else:
                    self.entities[ref]['damage'] = damage
        self.gates()

    def play(self):
        if self.mode in FAMILIAR_MODES:
            super().play()
            return
        self.setup_cards()
        self.refresh_zero()
        self.gates()
        if self.mode in REGEN_MODES:
            self.regenerate()
        else:
            self.keyword_combat()


def expand(name,csvid,declaration,model,selected,rows,oracle):
    from recipes import step

    validate_witnesses(oracle,rows)
    if csvid!=pins()['sources'][name]['registry']['csvid']:
        raise ValueError('unreserved cost reducer')
    cases = []
    for variant in selected:
        mode,actor = variant.rsplit('_',1)
        world = FamiliarScene if model['effect']=='color_discount' else DiscountScene
        scene = world(name,model,mode,int(actor),rows,oracle)
        scene.play()
        steps = scene.steps
        if scene.replay is not None:
            at = scene.replay
            suffix = steps[at:]
            steps = steps[:at]+[step('capture')]+suffix+[step('record_state'),step('ai_roundtrip'),step('restore')]+suffix+[step('assert_recorded_state')]
        metadata = dict(family=FAMILY,version=2,variant=variant,params=declaration['params'],oracle_text=declaration['oracle_text'],
                        covered_clauses=declaration['oracle_text'].splitlines(),untested_components=list(GAPS if model['rider'] in ('life','scry') else GAPS[1:]))
        cases.append(dict(name=f'MultiTypeDiscountsRecipe_{csvid}_{variant}',card=name,fixture='empty_main',seed=1772,
                          setup=scene.setup,steps=steps,_recipe=metadata))
    return cases
