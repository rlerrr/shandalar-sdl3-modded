"""Legendary Vehicles with damage, alternate Crew costs, and historic reveals."""

from functools import lru_cache
import json
from pathlib import Path
import re

from death_counter_trigger_recipes import exact
from vehicle_crew_combat_recipes import CREW, CrewScene, record_hash


FAMILY = 'vehicle_specialized_riders'
PROFILE = Path(__file__).parent/'data/vehicle_specialized_riders_wave8_profiles.json'
SKY_DAMAGE = 'Whenever Skysovereign enters or attacks, it deals 3 damage to target creature or planeswalker an opponent controls.'
HEART_COST = "You may remove a loyalty counter from a planeswalker you control rather than pay Heart of Kiran's crew cost."
WEATHERLIGHT_REVEAL = ('Whenever Weatherlight deals combat damage to a player, look at the top five cards of your library. '
    'You may reveal a historic card from among them and put it into your hand. Put the rest on the bottom of your '
    'library in a random order. (Artifacts, legendaries, and Sagas are historic.)')
COMMON = ('cast','cast_short','ordered_exact','underpowered','partial_cancel','expires','pending_replay')
SKY_ENTRY = ('entry_creature','entry_lethal','entry_walker','entry_walker_lethal','entry_no_enemy','entry_replay')
SKY_ATTACK = ('attack_creature','attack_lethal','attack_walker','attack_walker_lethal','attack_no_enemy','attack_other',
              'attack_ground','attack_air','attack_replay')
HEART_LOYALTY = ('loyalty','both_loyalty','last_loyalty','other_walker','foreign_walker','loyalty_air','loyalty_expiry',
                 'loyalty_pending_replay')
ARTIFACT = 'Darksteel Relic'
LEGENDARY = 'Isamaru, Hound of Konda'
SAGA = 'The Eldest Reborn'
WEATHER_HITS = {'artifact':ARTIFACT,'legendary':LEGENDARY,'saga':SAGA,'fifth':ARTIFACT,'ground':ARTIFACT,'replay':ARTIFACT}
WEATHER = (*WEATHER_HITS,'decline','empty','blocked','walker')
GAPS = ['Multiplayer','Legend rule with a duplicate of this Vehicle','Extra voluntary crew selections after the native selection threshold']


@lru_cache(maxsize=1)
def pins():
    return json.loads(PROFILE.read_bytes())


def parameters(record):
    from recipes import cost_from_oracle

    if (not record or record.get('layout')!='normal' or record.get('card_faces') or record.get('selected_face')
            or record.get('type_line')!='Legendary Artifact — Vehicle'):
        raise ValueError('complete normal legendary Vehicle required')
    flight, *clauses = record['oracle_text'].splitlines()
    if len(clauses)!=2:
        raise ValueError('one complete Crew clause and one specialized rider required')
    before,after = clauses
    if flight=='Flying' and before==SKY_DAMAGE:
        effect,crew,keys = 'sky',after,['Flying']
    elif flight=='Flying' and before==WEATHERLIGHT_REVEAL:
        effect,crew,keys = 'weatherlight',after,['Flying']
    elif flight=='Flying, vigilance' and after==HEART_COST:
        effect,crew,keys = 'heart',before,['Flying','Vigilance']
    else:
        raise ValueError('exact complete specialized Vehicle clause required')
    match = CREW.fullmatch(crew)
    if (not match or match[2] and match[2]!=match[1] or set(record['keywords'])!={'Crew',*keys}
            or not all(re.fullmatch(r'[1-9][0-9]*',record.get(k,'')) for k in ('power','toughness'))):
        raise ValueError('printed Crew, keywords and positive fixed stats required')
    return dict(effect=effect,cost=cost_from_oracle(record),threshold=int(match[1]),power=int(record['power']),
                toughness=int(record['toughness']),keywords=keys)


def variants(model):
    effect = model['effect']
    specific = {'sky':(*SKY_ENTRY,*SKY_ATTACK),'heart':(*HEART_LOYALTY,'both_crew','crew_ground'),
                'weatherlight':WEATHER}[effect]
    options = [f'{mode}_{seat}' for mode in (*COMMON,*specific) for seat in (0,1)]
    if effect=='weatherlight':
        # The empty eligible first-five plus a sixth historic card is exercised through the human menu.
        options.append('sixth_locked_0')
    return options


def spec(declaration,record):
    model = parameters(record)
    selected = declaration.get('variants')
    if (declaration.get('family')!=FAMILY or type(declaration.get('version')) is not int or declaration['version']!=2
            or declaration.get('oracle_text')!=record['oracle_text'] or not exact(declaration.get('params'),model)
            or pins()['sources'].get(record['name'],{}).get('oracle_record_sha256')!=record_hash(record)
            or type(selected) is not list or not selected or any(type(item) is not str for item in selected)
            or len(set(selected))!=len(selected) or set(selected)-set(variants(model))):
        raise ValueError('pinned whole specialized Vehicle and exact typed variants required')
    return dict(model,family=FAMILY)


def validate_witnesses(oracle,rows):
    profile = pins()
    if profile['version']!=1:
        raise ValueError('unreviewed specialized Vehicle profiles')
    for group in ('sources','witnesses'):
        for name,pin in profile[group].items():
            row,record = rows.get(name,{}),oracle.get(name,{})
            if (pin['oracle_record_sha256']!=record_hash(record) or not exact(pin['registry'],row)
                    or row.get('kind')!='card' or row.get('handler') in (None,'NULL','0','card_dummy')
                    or record.get('layout')!=('saga' if group=='witnesses' and name==SAGA else 'normal')):
                raise ValueError(f'changed or unsupported specialized Vehicle record: {name}')
    types = {name:oracle[name]['type_line'] for name in (ARTIFACT,LEGENDARY,SAGA)}
    if not(types[ARTIFACT]=='Artifact' and types[LEGENDARY].startswith('Legendary Creature')
           and types[SAGA]=='Enchantment — Saga'):
        raise ValueError('independent artifact, legendary, and Saga historic witnesses required')


class SpecializedScene(CrewScene):
    def setup_base(self,*,in_hand=False,pilots=True):
        self.add(self.name,'source',self.owner,'hand' if in_hand else 'battlefield')
        self.add('Unsummon','type_probe',self.foe,'hand')
        if pilots:
            self.setup_cost(self.model['threshold'])

    def ordinary(self):
        mode = self.mode
        if mode in ('cast','cast_short'):
            self.cast_permanent('source',mode=='cast')
            return True
        if mode=='underpowered':
            self.start_crew((),available=False)
            return True
        if mode=='partial_cancel':
            self.start_crew(self.pilots[:1],cancel=True)
            return True
        return False

    def crew(self):
        self.start_crew(self.pilots)
        self.finish_crew()
        if self.mode=='expires':
            self.animated = False
            self.cleanup()

    def prepare(self):
        self.refresh_zero()
        if self.mode=='attack_replay':
            self.replay = len(self.steps)


class SkyScene(SpecializedScene):
    def __init__(self,*args):
        super().__init__(*args)
        self.walker = None

    def setup_cards(self):
        mode = self.mode
        # Fixture insertion uses real entry. Seed the source before any enemy targets in attack-only cases.
        in_hand = mode in ('cast','cast_short',*SKY_ENTRY)
        self.setup_base(in_hand=in_hand,pilots=not in_hand)
        if mode in SKY_ENTRY or mode in SKY_ATTACK:
            self.add('Catapult Squad','friendly',self.owner)
        victim = None
        if mode.endswith('walker'):
            victim = 'Chandra Nalaar'
        elif mode.endswith('walker_lethal'):
            victim = 'Jace Beleren'
        elif mode.endswith('lethal'):
            victim = 'Mogg Fanatic'
        elif mode in (*SKY_ENTRY,*SKY_ATTACK) and not mode.endswith('no_enemy'):
            victim = 'Shivan Dragon'
        if victim:
            self.add(victim,'victim',self.foe)
            if 'Planeswalker' in self.oracle[victim]['type_line']:
                self.walker = int(self.oracle[victim]['loyalty'])
        if mode in ('attack_ground','attack_air'):
            self.add('Welkin Tern' if mode=='attack_air' else 'Mogg Fanatic','blocker',self.foe)

    def receipts(self):
        super().receipts()
        if self.walker is not None and self.entities['victim']['zone']=='battlefield':
            self.emit('assert_counter',ref='victim',counter_kind='loyalty',value=self.walker)

    def damage(self):
        if self.walker is not None:
            self.walker -= 3
            if self.walker<=0:
                self.leave('victim','graveyard')
        elif self.stats('victim')[1]<=3:
            self.leave('victim','graveyard')
        else:
            self.entities['victim']['damage'] += 3

    def entry(self):
        fee = self.model['cost']
        if self.mode=='entry_replay':
            self.replay = len(self.steps)
        self.fund(self.owner,fee)
        self.emit('cast',ref='source',value=1)
        self.emit('assert_stack',value=1)
        self.pay(self.owner,fee)
        self.pools()
        if 'victim' in self.entities:
            self.emit('choose_target',ref='victim',player=self.owner)
        self.emit('resolve')
        self.emit('assert_unused_choices',value=0)
        self.emit('refresh')
        self.entities['source']['zone'] = 'battlefield'
        if 'victim' in self.entities:
            self.damage()
        self.receipts()

    def attack_with_rider(self):
        mode = self.mode
        other = mode=='attack_other'
        attacker = 'friendly' if other else 'source'
        self.emit('begin_attackers')
        self.emit('attack',ref=attacker,value=1)
        self.entities[attacker]['tapped'] = True
        expected_damage = not other and 'victim' in self.entities
        optional_probe = other and self.owner==1
        if expected_damage or optional_probe:
            self.emit('choose_target',ref='victim',player=self.owner)
        self.emit('finish_attackers')
        self.emit('assert_unused_choices',value=int(optional_probe))
        self.resolve()
        if expected_damage:
            self.damage()
        self.receipts()
        blocked = mode=='attack_air'
        self.finish_combat(attacker,'blocker' if mode in ('attack_air','attack_ground') else None,blocked=blocked)

    def play(self):
        self.setup_cards()
        self.prepare()
        if self.ordinary():
            return
        if self.mode in SKY_ENTRY:
            self.entry()
            return
        self.crew()
        if self.mode in SKY_ATTACK:
            self.attack_with_rider()


class HeartScene(SpecializedScene):
    def __init__(self,*args):
        super().__init__(*args)
        self.loyalty = {}

    def add_walker(self,name,ref,player):
        self.add(name,ref,player)
        self.loyalty[ref] = int(self.oracle[name]['loyalty'])

    def setup_cards(self):
        mode = self.mode
        in_hand = mode in ('cast','cast_short')
        self.setup_base(in_hand=in_hand,pilots=not in_hand and (mode not in HEART_LOYALTY or mode=='both_loyalty'))
        if mode in HEART_LOYALTY or mode=='both_crew':
            self.add_walker('Jace Beleren','walker',self.foe if mode=='foreign_walker' else self.owner)
        if mode=='other_walker':
            self.add_walker('Chandra Nalaar','other_walker',self.owner)
        if mode=='last_loyalty':
            self.add('Shock','damage_spell',self.foe,'hand')
        if mode in ('loyalty_air','crew_ground'):
            self.add('Welkin Tern' if mode=='loyalty_air' else 'Mogg Fanatic','blocker',self.foe)

    def loyalty_receipts(self):
        for ref,n in self.loyalty.items():
            if self.entities[ref]['zone']=='battlefield':
                self.emit('assert_counter',ref=ref,counter_kind='loyalty',value=n)
            else:
                self.emit('assert_zone_multiplicity',player=self.owner,zone='graveyard',index=self.observed_id(ref),value=1)

    def receipts(self):
        super().receipts()
        self.loyalty_receipts()

    def start_crew(self,plan,*,available=True,cancel=False):
        from recipes import step

        at = len(self.steps)
        result = super().start_crew(plan,available=available,cancel=cancel)
        if available and (self.owner==1 or self.mode=='both_crew'):
            self.steps.insert(at+1,step('choose_dialog',player=self.owner,value=1))
        return result

    def use_loyalty(self):
        mode = self.mode
        if mode=='last_loyalty':
            self.cast_spell('damage_spell','walker')
            self.resolve()
            self.leave('damage_spell','graveyard')
            self.loyalty['walker'] -= 2
            self.receipts()
        allowed = mode!='foreign_walker'
        self.emit('can_activate',ref='source',value=int(allowed))
        if not allowed:
            self.emit('activate',ref='source',value=0)
            self.receipts()
            return
        ref = 'other_walker' if mode=='other_walker' else 'walker'
        if self.owner==1 or mode=='both_loyalty':
            self.emit('choose_dialog',player=self.owner,value=0)
        self.emit('choose_target',ref=ref,player=self.owner)
        self.emit('activate',ref='source',value=1)
        self.emit('assert_choices_consumed')
        self.emit('assert_stack',value=1)
        self.emit('refresh')
        self.loyalty[ref] -= 1
        if self.loyalty[ref]==0:
            self.leave(ref,'graveyard')
        self.loyalty_receipts()
        self.source_traits()
        self.pools()
        for pilot in self.pilots:
            self.prop(pilot,'tapped',0)
        if mode=='loyalty_pending_replay':
            self.replay = len(self.steps)
        self.resolve()
        self.animated = True
        self.receipts()

    def play(self):
        self.setup_cards()
        self.prepare()
        if self.ordinary():
            return
        if self.mode in HEART_LOYALTY:
            self.use_loyalty()
        else:
            self.crew()
        if self.mode in ('loyalty_air','crew_ground'):
            self.declare_attacker('source')
            self.finish_combat('source','blocker',blocked=self.mode=='loyalty_air')
        if self.mode=='loyalty_expiry':
            self.animated = False
            self.cleanup()


class WeatherlightScene(SpecializedScene):
    def __init__(self,*args):
        super().__init__(*args)
        self.deck = []
        self.walker = None

    def setup_cards(self):
        mode = self.mode
        in_hand = mode in ('cast','cast_short')
        self.setup_base(in_hand=in_hand,pilots=not in_hand)
        names = ()
        if mode in WEATHER_HITS:
            names = (WEATHER_HITS[mode],'Forest')
            if mode=='fifth':
                names = ('Island','Mogg Fanatic','Forest','Unsummon',ARTIFACT,'Plains','Mountain')
        elif mode=='decline':
            names = (ARTIFACT,)
        elif mode in ('blocked','walker'):
            names = (ARTIFACT,'Forest')
        elif mode=='sixth_locked':
            names = ('Island','Mogg Fanatic','Forest','Unsummon','Plains',ARTIFACT,'Mountain')
        for i,name in enumerate(names):
            ref = f'library_{i}'
            self.add(name,ref,self.owner,'library')
            self.deck.append(ref)
        if mode in (*WEATHER,'sixth_locked'):
            self.add('Swamp','enemy_library',self.foe,'library')
        if mode in ('blocked','ground'):
            self.add('Welkin Tern' if mode=='blocked' else 'Mogg Fanatic','blocker',self.foe)
        if mode=='walker':
            self.add('Jace Beleren','walker',self.foe)
            self.walker = int(self.oracle['Jace Beleren']['loyalty'])

    def receipts(self):
        super().receipts()
        if self.walker is not None and self.entities['walker']['zone']=='battlefield':
            self.emit('assert_counter',ref='walker',counter_kind='loyalty',value=self.walker)

    def historic_input(self):
        mode = self.mode
        if mode=='decline':
            self.emit('choose_optional_zone',player=self.owner,value=self.rows[ARTIFACT]['csvid'])
            if self.owner==1:
                # The AI retry consults the same optional selection policy again.
                self.emit('choose_optional_zone',player=self.owner,value=self.rows[ARTIFACT]['csvid'])
            return 0,self.deck
        if mode=='sixth_locked':
            self.emit('choose_zone',player=self.owner,value=-1)
        else:
            pick = next(ref for ref in self.deck[:5] if self.entities[ref]['name']==WEATHER_HITS[mode])
            self.emit('choose_zone_card',player=self.owner,value=self.observed_id(pick))
            if self.owner==0:
                self.emit('choose_dialog',player=0,value=0)  # Real public reveal notice.
            self.entities[pick]['zone'] = 'hand'
        leftovers = [ref for ref in self.deck[:5] if self.entities[ref]['zone']=='library']
        # Drive the real legacy order menu if offered. Oracle requires a random order with no ordering input.
        for ref in leftovers[:-1]:
            self.emit('choose_zone_card',player=self.owner,value=self.observed_id(ref))
        return max(0,len(leftovers)-1),self.deck[5:]

    def terminal_deck(self,prefix):
        for i,ref in enumerate(prefix):
            self.emit('assert_zone_card',player=self.owner,zone='library',index=i,value=self.observed_id(ref))
        if self.mode in (*WEATHER,'sixth_locked'):
            self.emit('assert_zone_card',player=self.foe,zone='library',index=0,value=self.rows['Swamp']['csvid'])

    def look_damage(self):
        mode = self.mode
        self.emit('begin_attackers')
        if mode=='walker':
            self.emit('choose_dialog',player=self.owner,value=1)
            self.emit('attack_with_tax',ref='source',value=1)
        else:
            self.emit('attack',ref='source',value=1)
        self.entities['source']['tapped'] = True
        self.emit('finish_attackers')
        self.emit('begin_blockers')
        blocked = mode=='blocked'
        if mode in ('blocked','ground'):
            self.emit('block',ref='blocker',other='source',value=int(blocked))
        self.emit('advance_combat')
        self.prop('source','blocked',blocked)
        if mode=='replay':
            self.replay = len(self.steps)
        prefix = self.deck
        unused = 0
        hit_player = not blocked and mode!='walker'
        if hit_player and self.deck:
            unused,prefix = self.historic_input()
        self.emit('combat_damage')
        self.emit('resolve')
        if hit_player and self.deck:
            self.emit('assert_unused_choices',value=unused)
            self.emit('assert_forced_zone_choices',value=0)
        self.emit('refresh')
        self.emit('end_combat')
        if mode=='walker':
            self.leave('walker','graveyard')
        elif blocked:
            self.entities['source']['damage'] = self.stats('blocker')[0]
            self.leave('blocker','graveyard')
        else:
            self.life[self.foe] -= self.model['power']
        if mode in ('artifact','legendary','saga','ground','replay','decline'):
            self.emit('assert_offered_count',value=len(self.deck))
            self.emit('assert_offered_selection_required',value=0)
            chosen = ARTIFACT if mode=='decline' else WEATHER_HITS[mode]
            self.emit('assert_offered_legal_card',index=self.rows[chosen]['csvid'],value=1)
            if len(self.deck)==2:
                self.emit('assert_offered_legal_card',index=self.rows['Forest']['csvid'],value=0)
        self.receipts()
        self.terminal_deck(prefix)

    def play(self):
        self.setup_cards()
        self.prepare()
        self.emit('observe_search_filters',value=1)
        if self.ordinary():
            return
        self.crew()
        if self.mode in (*WEATHER,'sixth_locked'):
            self.look_damage()


SCENES = {'sky':SkyScene,'heart':HeartScene,'weatherlight':WeatherlightScene}
EFFECT_GAPS = {
    'sky': ['Changing an already chosen attack or entry target before the trigger resolves'],
    'heart': ['Protected planeswalker as a non-targeted cost choice'],
    'weatherlight': ['Statistical randomness of a multi-card bottom permutation; prefix and exact membership are checked'],
}


def expand(name,csvid,declaration,model,selected,rows,oracle):
    from recipes import step

    validate_witnesses(oracle,rows)
    if csvid!=pins()['sources'][name]['registry']['csvid'] or set(selected)-set(variants(model)):
        raise ValueError('unreserved specialized Vehicle or variant')
    result = []
    for variant in selected:
        mode,seat = variant.rsplit('_',1)
        scene = SCENES[model['effect']](name,model,mode,int(seat),rows,oracle)
        scene.play()
        steps = scene.steps
        if scene.replay is not None:
            cut = scene.replay
            suffix = steps[cut:]
            steps = steps[:cut]+[step('capture')]+suffix+[step('record_state'),step('ai_roundtrip'),step('restore')]+suffix+[step('assert_recorded_state')]
        metadata = dict(family=FAMILY,version=2,variant=variant,params=declaration['params'],oracle_text=declaration['oracle_text'],
                        covered_clauses=declaration['oracle_text'].splitlines(),untested_components=[*GAPS,*EFFECT_GAPS[model['effect']]])
        result.append(dict(name=f'VehicleSpecializedRidersRecipe_{csvid}_{variant}',card=name,fixture='empty_main',seed=1970,
                           setup=scene.setup,steps=steps,_recipe=metadata))
    return result
