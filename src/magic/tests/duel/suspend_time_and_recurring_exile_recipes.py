"""Actual paid Suspend, alternate timing, and recurring exile."""

from collections import Counter
from functools import lru_cache
import json
from pathlib import Path
import re

from counter_condition_recurrence_recipes import exact
from recipes import cost_from_oracle

FAMILY = 'suspend_time_and_recurring_exile'
PROFILE = Path(__file__).parent / 'data/suspend_time_and_recurring_exile_wave8_profiles.json'
ZONE = {'hand':0,'battlefield':1,'library':2,'graveyard':3,'exile':4}
ZONE_NAMES = ('hand','battlefield','library','graveyard','exile')
EXPANSION = ('replay','countered','exile_loss')
CREATURE_EXPANSION = ('unfunded','normal_entry')
STATIC_WITNESSES = {'Grizzly Bears','Runeclaw Bear','Ornithopter','Suntail Hawk'}
WITNESSES = {
    'Counterspell', 'Darksteel Relic', 'Forest',
    'Grizzly Bears', 'Island', 'Mountain',
    'Ornithopter', 'Plains', 'Pull from Eternity',
    'Runeclaw Bear', 'Shock', 'Spellbook',
    'Suntail Hawk', 'Swamp', 'Tome Scour',
}

ZERO = [0] * 6
RECURRENT = {8903, 8924, 9000}
DURATIONS = {8903: 3, 8517: 9, 8924: 3, 8983: 7, 8626: 2, 9000: 3}
SPELL_MODES = ('countdown', 'own_spell', 'opponent_spell')
MODES = {
    **{csvid: ('recurring', 'normal_recur', 'unfunded') for csvid in RECURRENT},
    8517: SPELL_MODES, 8626: SPELL_MODES,
    8983: ('countdown', 'own_grave', 'opponent_death', 'opponent_mill'),
}
PRINTED_FEE = re.compile(r'Suspend (\d+)—((?:\{\d+\}|\{[BUGRW]\})+)(?:$| )')


class SuspendTimingCase:
    def __init__(self, csvid, record, actor, mode, rows, oracle):
        self.csvid, self.record, self.actor, self.foe, self.mode = csvid, record, actor, 1-actor, mode
        self.rows, self.oracle = rows, oracle
        self.only = False
        self.setup, self.steps, self.cards = [], [], {}
        self.life = [20,20]
        self.deck = {actor:['Forest','Plains','Island','Mountain','Swamp','Forest','Island'],
                     self.foe:['Island','Swamp','Forest','Plains','Mountain','Plains','Forest']}
        lines = [line for line in record['oracle_text'].splitlines() if line.startswith('Suspend ')]
        line, = lines
        duration, fee = PRINTED_FEE.match(line).groups()
        self.duration = int(duration)
        assert self.duration == DURATIONS[csvid]
        self.payment = cost_from_oracle({'mana_cost':fee})
        self.target = None
        self.add('source', actor, record['name'], 'hand')
        for seat, names in self.deck.items():
            for index, name in enumerate(names):
                self.add(f'deck_{seat}_{index}', seat, name, 'library')
        if csvid==8903:
            self.add('attacker', self.foe, 'Grizzly Bears', 'battlefield')
        if csvid==8924:
            self.add('weak', self.foe, 'Suntail Hawk', 'battlefield')
            self.add('strong', self.foe, 'Grizzly Bears', 'battlefield')
            self.add('ally', actor, 'Runeclaw Bear', 'battlefield')
            self.add('noncreature', self.foe, 'Spellbook', 'battlefield')
        if csvid==9000:
            self.add('first', self.foe, 'Spellbook', 'battlefield')
            self.add('repeat', actor, 'Grizzly Bears', 'battlefield')
        if csvid in (8517,8626,8983):
            self.add('blocker', self.foe, 'Grizzly Bears', 'battlefield')
        if csvid==8983:
            self.add('artifact_blocker', self.foe, 'Ornithopter', 'battlefield')
        if mode in ('own_spell','opponent_spell','opponent_declines'):
            caster = actor if mode=='own_spell' else self.foe
            self.add('trigger_spell', caster, 'Darksteel Relic', 'hand')
        if mode in ('own_grave','opponent_mill'):
            self.add('trigger_spell', actor, 'Tome Scour', 'hand')
        if mode=='opponent_death':
            self.add('trigger_spell', actor, 'Shock', 'hand')
            self.add('victim', self.foe, 'Suntail Hawk', 'battlefield')
        if mode=='countered':
            self.add('counter', self.foe, 'Counterspell', 'hand')
        if mode=='exile_loss':
            self.add('pull', self.foe, 'Pull from Eternity', 'hand')
            self.add('unselected_exile', actor, 'Darksteel Relic', 'exile')

    def add(self, ref, player, name, zone):
        from recipes import card

        self.setup.append(card(name,ref,player,zone))
        self.cards[ref]=dict(name=name,player=player,zone=zone)

    def emit(self, op, ref='', other='', prop='', **fields):
        from recipes import step

        if ref:
            fields['ref']=ref
        if other:
            fields['other']=other
        if prop:
            fields['property']=prop
        if 'zone' in fields:
            fields['zone']=ZONE_NAMES[fields['zone']]
        self.steps.append(step(op,**fields))

    def observe(self, ref, prop, value):
        self.emit('assert_card', ref, prop=prop, value=value)

    def mana(self, op, player, amounts):
        for color, amount in enumerate(amounts):
            self.emit(op, player=player, color=color, value=amount)

    def receipts(self, *, ordered_library=True):
        self.emit('assert_stack')
        names = sorted({card['name'] for card in self.cards.values()})
        for seat in (0, 1):
            self.emit('assert_life', player=seat, value=self.life[seat])
            for zone in ZONE:
                contents = self.deck[seat] if zone == 'library' else [
                    card['name'] for card in self.cards.values() if card['player'] == seat and card['zone'] == zone
                ]
                self.emit('assert_zone', player=seat, zone=ZONE[zone], value=len(contents))
                counts = Counter(contents)
                for name in names:
                    self.emit('assert_zone_multiplicity', player=seat, zone=ZONE[zone], index=self.rows[name]['csvid'],
                              value=counts[name])
                if zone == 'library' and (ordered_library or seat == self.foe):
                    for index, name in enumerate(contents):
                        self.emit('assert_zone_card', player=seat, zone=ZONE[zone], index=index,
                                  value=self.rows[name]['csvid'])

    def suspend_from_hand(self):
        self.emit('turn', player=self.actor)
        self.mana('set_mana', self.foe, [0] * 6)
        parked = self.mode != 'unfunded'
        self.mana('set_mana', self.actor, self.payment if parked else [0] * 6)
        self.emit('refresh')
        if parked and not self.only and self.actor == 1:
            # The native AI sees the "Suspend" row even with only one affordable option.
            self.emit('choose_dialog', player=self.actor, value=1)
        self.emit('cast', 'source', value=0)
        self.emit('assert_choices_consumed')
        self.mana('assert_mana', self.actor, [0] * 6)
        self.mana('assert_mana', self.foe, [0] * 6)
        if parked:
            self.cards['source']['zone'] = 'exile'
        self.receipts()

    def upkeep(self, player, *, last=False):
        self.emit('advance_turn')
        self.emit('untap_step', player=player)
        if last:
            # The legal acceptance route casts through the actual Suspend legacy.
            self.plan_last()
        self.emit('upkeep_step', player=player)
        self.emit('resolve')
        self.emit('refresh')
        self.emit('assert_unused_choices', value=0)
        self.emit('assert_choices_consumed')
        for seat in (0, 1):
            self.mana('assert_mana', seat, [0] * 6)
        if not last:
            self.receipts()

    def plan_last(self):
        if self.target:
            self.emit('choose_target', self.target, player=self.actor)

    def settle(self):
        self.emit('resolve')
        self.emit('refresh')
        self.emit('assert_stack')
        self.emit('assert_choices_consumed')

    def cast_actual(self, ref, *, target=None, player_target=None, normal_suspend=False, announce_choice=None, exile_selection=None):
        card = self.cards[ref]
        player = card['player']
        record = self.oracle[card['name']]
        fee = ZERO if record['mana_cost']=='{0}' else cost_from_oracle(record)
        self.mana('set_mana', player, fee)
        self.emit('refresh')
        if normal_suspend:
            self.emit('choose_dialog', player=player, value=0)
        if target:
            self.emit('choose_target', target, player=player)
        if player_target is not None:
            self.emit('choose_player', player=player, value=player_target)
        self.emit('cast', ref, value=1)
        self.emit('assert_choices_consumed')
        self.emit('assert_stack', value=1)
        self.mana('assert_mana', player, ZERO)
        if announce_choice is not None:
            self.emit('choose_dialog', player=self.foe, value=announce_choice)
        self.emit('announce_spell', ref)
        if exile_selection is not None:
            self.emit('choose_zone_card', player=player, value=exile_selection)
        self.settle()
        card['zone'] = ('graveyard' if record['type_line'].startswith(('Instant','Sorcery')) else 'battlefield')

    def attack_permission(self, permitted):
        self.emit('turn', player=self.foe)
        self.emit('begin_attackers')
        self.emit('attack', 'attacker', value=int(permitted))
        self.observe('attacker','attacking',int(permitted))
        self.emit('finish_attackers')
        self.emit('begin_blockers')
        self.emit('advance_combat')
        self.emit('combat_damage')
        self.emit('resolve')
        self.emit('end_combat')
        self.emit('assert_choices_consumed')
        if permitted:
            self.life[self.actor]-=int(self.oracle[self.cards['attacker']['name']]['power'])
        self.receipts()

    def mark_recurrence(self, target=None):
        self.cards['source']['zone']='exile'
        if target:
            self.cards[target]['zone']='hand'
        if self.csvid==8924:
            self.cards['weak']['zone']='graveyard'
            self.observe('strong','power',1)
            self.observe('strong','toughness',1)
            self.observe('ally','power',2)
            self.observe('ally','toughness',2)
            self.observe('noncreature','in_play',1)
        self.receipts()

    def mark_expiry(self):
        if self.csvid==8924:
            self.observe('strong','power',2)
            self.observe('strong','toughness',2)
            self.observe('ally','power',2)
            self.observe('ally','toughness',2)

    def run_recurrent(self):
        self.target='first' if self.csvid==9000 else None
        if self.mode=='normal_recur':
            self.emit('turn',player=self.actor)
            self.mana('set_mana',self.foe,ZERO)
            self.cast_actual('source',target=self.target,normal_suspend=True)
        else:
            for turn in range(1,4):
                self.upkeep(self.foe)
                self.upkeep(self.actor,last=turn==3)
        self.mark_recurrence(self.target)

        # A newly exiled card stays out for two complete owner upkeeps and
        # casts again on the third. No seeded time counters or direct exile play.
        self.target='repeat' if self.csvid==9000 else None
        for turn in range(1,4):
            self.upkeep(self.foe)
            if turn==1:
                self.mark_expiry()
            if self.csvid==8903 and turn in (1,2):
                self.attack_permission(turn==2)
            self.upkeep(self.actor,last=turn==3)
        self.mark_recurrence(self.target)
        if self.csvid==8903:
            self.upkeep(self.foe)
            self.attack_permission(False)

    def grave_move(self):
        if self.mode=='opponent_death':
            self.cast_actual('trigger_spell',target='victim')
            self.cards['victim']['zone']='graveyard'
            return int(self.actor==1)
        player=self.actor if self.mode=='own_grave' else self.foe
        self.cast_actual('trigger_spell',player_target=player)
        del self.deck[player][:5]
        for index in range(5):
            self.cards[f'deck_{player}_{index}']['zone']='graveyard'
        # The manual human route leaves the optional prompt unchosen. The
        # controller-one default takes each legal removal; zone timing proves it.
        return 5 if player==self.foe and self.actor==1 else 0

    def suspended_creature(self):
        self.emit('bind_permanent_copy','played',player=self.actor,value=self.csvid,index=0)
        self.emit('turn',player=self.actor)
        power,toughness=int(self.record['power']),int(self.record['toughness'])
        self.observe('played','power',power)
        self.observe('played','toughness',toughness)
        pumped=self.csvid==8626
        if pumped:
            self.mana('set_mana',self.actor,cost_from_oracle({'mana_cost':'{R}'}))
            self.emit('refresh')
            self.emit('activate','played',value=1)
            self.emit('assert_stack',value=1)
            self.mana('assert_mana',self.actor,ZERO)
            self.settle()
            self.observe('played','power',power+1)
            self.observe('played','toughness',toughness)
            power+=1
        self.emit('begin_attackers')
        self.emit('attack','played',value=1)
        self.observe('played','attacking',1)
        self.emit('finish_attackers')
        self.emit('begin_blockers')
        self.emit('block','blocker','played',value=0)
        fear=self.csvid==8983
        if fear:
            self.emit('block','artifact_blocker','played',value=1)
        self.emit('advance_combat')
        self.observe('played','blocked',int(fear))
        self.emit('combat_damage')
        self.emit('resolve')
        self.emit('end_combat')
        self.emit('assert_choices_consumed')
        if fear:
            self.cards['artifact_blocker']['zone']='graveyard'
        else:
            self.life[self.foe]-=power
        self.receipts()
        if pumped:
            self.emit('cleanup')
            self.emit('refresh')
            self.observe('played','power',power-1)
            self.observe('played','toughness',toughness)

    def run_creature(self):
        visits=self.duration
        already_foe=False
        if self.mode=='own_spell':
            self.cast_actual('trigger_spell')
            self.receipts()
        if self.mode in ('opponent_spell','opponent_declines'):
            self.upkeep(self.foe)
            self.emit('turn',player=self.foe)
            already_foe=True
            # Dragon grants the caster the choice, rather than its owner.
            menu=int(self.mode=='opponent_declines') if self.csvid==8626 and self.foe==0 else None
            self.cast_actual('trigger_spell',announce_choice=menu)
            self.receipts()
            if self.csvid==8517:
                visits-=1
            elif self.mode!='opponent_declines':
                visits+=1
        if self.mode in ('own_grave','opponent_death','opponent_mill'):
            visits-=self.grave_move()
            self.receipts()
        for turn in range(1,visits+1):
            if turn!=1 or not already_foe:
                self.upkeep(self.foe)
            self.upkeep(self.actor,last=turn==visits)
        self.cards['source']['zone']='battlefield'
        self.receipts()
        # Timing variants keep exact expected arrival without depending on
        # named on-field bindings if the accelerated legacy is wrong.
        if self.mode in ('countdown','replay'):
            self.suspended_creature()

    def start_normal(self):
        self.emit('turn',player=self.actor)
        self.mana('set_mana',self.foe,ZERO)
        self.mana('set_mana',self.actor,cost_from_oracle(self.record))
        self.emit('refresh')
        # Both printed alternatives are affordable; choose a normal cast.
        self.emit('choose_dialog',player=self.actor,value=0)
        if self.csvid==9000:
            self.emit('choose_target','first',player=self.actor)
        self.emit('cast','source',value=1)
        self.emit('assert_stack',value=1)
        self.emit('assert_choices_consumed')
        self.mana('assert_mana',self.actor,ZERO)
        self.emit('announce_spell','source')

    def normal_entry(self):
        self.start_normal()
        self.settle()
        self.cards['source']['zone']='battlefield'
        self.observe('source','in_play',1)
        self.observe('source','sick',1)
        self.observe('source','power',int(self.record['power']))
        self.observe('source','toughness',int(self.record['toughness']))
        self.emit('begin_attackers')
        self.emit('attack','source',value=0)
        self.observe('source','attacking',0)
        self.emit('finish_attackers')
        self.emit('begin_blockers')
        self.emit('advance_combat')
        self.emit('combat_damage')
        self.emit('end_combat')
        self.receipts()

    def normal_countered(self):
        self.start_normal()
        self.mana('set_mana',self.foe,cost_from_oracle(self.oracle['Counterspell']))
        self.emit('refresh')
        self.emit('cast_response','counter',value=1)
        self.emit('assert_stack',value=2)
        self.emit('assert_choices_consumed')
        self.mana('assert_mana',self.foe,ZERO)
        self.emit('resolve_response')
        self.emit('refresh')
        self.emit('assert_stack')
        self.emit('assert_choices_consumed')
        self.cards['source']['zone']='graveyard'
        self.cards['counter']['zone']='graveyard'
        if self.csvid==8924:
            self.mark_expiry()
        self.receipts()

    def lose_exiled_card(self):
        self.upkeep(self.foe)
        self.emit('turn',player=self.foe)
        # A human opponent declines Pardic Dragon's optional time counter.
        decline=1 if self.csvid==8626 and self.foe==0 else None
        self.cast_actual('pull',player_target=self.actor,announce_choice=decline,exile_selection=self.csvid)
        self.cards['source']['zone']='graveyard'
        self.receipts()
        # Allow one extra owner upkeep for Dragon's automatic opponent-one choice.
        for visit in range(self.duration+1):
            if visit:
                self.upkeep(self.foe)
            self.upkeep(self.actor)

    def build(self):
        if self.mode=='countered':
            self.normal_countered()
            return
        if self.mode=='normal_entry':
            self.normal_entry()
            return
        if self.mode!='normal_recur':
            self.suspend_from_hand()
        if self.mode=='unfunded':
            return
        if self.mode=='exile_loss':
            self.lose_exiled_card()
        elif self.csvid in RECURRENT:
            self.run_recurrent()
        else:
            self.run_creature()


@lru_cache(maxsize=1)
def profile():
    return json.loads(PROFILE.read_text(encoding='utf8'))


def shape(record):
    pin=profile()['sources'].get(record['name'],{})
    csvid=pin.get('registry',{}).get('csvid')
    if (record.get('layout')!='normal' or record.get('selected_face') or record.get('card_faces')
            or 'Suspend' not in record.get('keywords',()) or not exact(pin.get('oracle'),record)):
        raise ValueError('whole pinned Oracle Suspend record required')
    clause,=[line for line in record['oracle_text'].splitlines() if line.startswith('Suspend ')]
    match=PRINTED_FEE.match(clause)
    if not match or int(match[1])!=DURATIONS[csvid]:
        raise ValueError('unreviewed printed Suspend duration')
    return dict(source=csvid,duration=int(match[1]),suspend_fee=cost_from_oracle({'mana_cost':match[2]}))


def variants(model):
    csvid=model['source']
    extra=() if csvid in RECURRENT else CREATURE_EXPANSION
    result=[f'{mode}_{seat}' for mode in (*MODES[csvid],*EXPANSION,*extra) for seat in (0,1)]
    if csvid==8626:
        result.append('opponent_declines_1')
    return result


def spec(declaration,record):
    expected=shape(record)
    selected=declaration.get('variants')
    if (declaration.get('family')!=FAMILY or type(declaration.get('version')) is not int
            or declaration['version']!=2 or declaration.get('oracle_text')!=record['oracle_text']
            or not exact(declaration.get('params'),expected)):
        raise ValueError('whole pinned Oracle and typed Suspend model required')
    if (not isinstance(selected,list) or not selected or any(type(v) is not str for v in selected)
            or len(selected)!=len(set(selected)) or set(selected)-set(variants(expected))):
        raise ValueError('distinct calibrated timing variants required')
    return dict(family=FAMILY,**expected)


def validate_witnesses(oracle,registry):
    pins=profile()
    if (pins['witnesses'].keys()!=WITNESSES
            or {pin['registry']['csvid'] for pin in pins['sources'].values()}!=DURATIONS.keys()):
        raise ValueError('private Suspend source or witness scope changed')
    for name,pin in (pins['sources']|pins['witnesses']).items():
        row=registry.get(name,{})
        if (not exact(oracle.get(name),pin['oracle']) or not exact(row,pin['registry'])
                or row.get('kind')!='card' or row.get('handler') in (None,'0','NULL')
                or row.get('handler')=='card_dummy' and name not in STATIC_WITNESSES):
            raise ValueError('exact implementation or pinned Oracle changed for '+name)


def expand(name,csvid,declaration,model,selected,registry,oracle):
    from recipes import step

    validate_witnesses(oracle,registry)
    if model['source']!=csvid or registry[name]['csvid']!=csvid or not selected or set(selected)-set(variants(model)):
        raise ValueError('only exact registered Suspend timing variants may expand')
    gaps={
        8983: ['The named choice cannot address the invisible TYPE_EFFECT legacy for human optional acceptance. Legal human decline and automatic AI acceptance are verified through grave movement and exact timing.'],
        8903: ['The raw attack restriction covers the player as sole attack destination. Attacks directed at planeswalkers are uncovered.'],
    }.get(csvid,[])
    cases=[]
    for variant in selected:
        mode,seat=variant.rsplit('_',1)
        board=SuspendTimingCase(csvid,oracle[name],int(seat),mode,registry,oracle)
        board.build()
        steps=board.steps
        if mode=='replay':
            steps=[step('capture'),*steps,step('record_state'),step('restore'),*steps,step('assert_recorded_state')]
        cases.append(dict(name=f'SuspendTimeAndRecurringExileRecipe_{csvid}_{variant}',card=name,
                          fixture='empty_main',seed=2439,setup=board.setup,steps=steps,
                          _recipe=dict(family=FAMILY,version=2,variant=variant,
                                       params=declaration['params'],oracle_text=declaration['oracle_text'],
                                       untested_components=gaps)))
    return cases
