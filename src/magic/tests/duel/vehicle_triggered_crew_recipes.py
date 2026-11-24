"""Paid Crew and distinct triggered Vehicle abilities at real phase boundaries."""

from functools import lru_cache
import json
from pathlib import Path

from death_counter_trigger_recipes import exact
from vehicle_crew_combat_recipes import CREW, PILOT, CrewScene, record_hash


FAMILY = 'vehicle_triggered_crew'
PROFILE = Path(__file__).parent/'data/vehicle_triggered_crew_wave8_profiles.json'
EFFECTS = {
    'Whenever this Vehicle attacks, it deals 1 damage to any target.': ('damage', ()),
    'When this Vehicle enters, draw a card.': ('draw', ()),
    'Trample, haste\nWhen this Vehicle enters, it becomes an artifact creature until end of turn.':
        ('animate', ('Trample','Haste')),
    'Whenever this Vehicle attacks, it gets +1/+1 and gains trample until end of turn.': ('pump', ()),
    'Flying\nWhenever this Vehicle attacks or blocks, you may draw a card. If you do, discard a card.':
        ('loot', ('Flying',)),
    'Whenever this Vehicle attacks, untap another target artifact or creature you control.': ('untap', ()),
    'Trample\nWhenever this Vehicle attacks, put a +1/+1 counter on it.': ('counter', ('Trample',)),
}
COMMON = ('cast','cast_short','ordered_exact','underpowered','partial_cancel','attack_uncrewed','expires','pending_replay')
MODES = {
    'damage': ('player','controller','creature','lethal','walker','other_attacker','trigger_replay'),
    'draw': ('entry_replay','entry_repeat','no_draw_crew','attack_combat'),
    'animate': ('entry_combat','entry_expires','fresh_vehicle','entry_replay','attack_combat'),
    'pump': ('unblocked','trample','other_attacker','trigger_replay','cleanup_recrew'),
    'loot': ('attack_accept','attack_empty_hand','block_accept','attack_ground','other_attacker','trigger_replay'),
    'untap': ('pilot','artifact_target','ready_target','no_target','enemy_target','other_attacker','trigger_replay'),
    'counter': ('unblocked','trample','other_attacker','trigger_replay','cleanup_recrew','repeat_attack'),
}
ENTRY = ('entry_replay','entry_repeat','entry_combat','entry_expires')
CAST = ('cast','cast_short',*ENTRY,'fresh_vehicle')
GAPS = ['Multiplayer','Additional voluntary crew selections after reaching the native menu threshold']
EFFECT_GAPS = {
    'damage': ['Battle targets','Priority after selecting the triggered ability target'],
    'loot': ['AI optional-decline policy','Empty-library loss and draw replacements'],
    'untap': ['Priority after selecting the triggered ability target'],
}


@lru_cache(maxsize=1)
def pins():
    return json.loads(PROFILE.read_bytes())


def parameters(record):
    from recipes import cost_from_oracle

    if (not record or record.get('layout')!='normal' or record.get('card_faces') or record.get('selected_face')
            or record.get('type_line')!='Artifact — Vehicle'):
        raise ValueError('normal Vehicle required')
    abilities, _, crew_text = record['oracle_text'].rpartition('\n')
    match = CREW.fullmatch(crew_text)
    effect,keywords = EFFECTS.get(abilities,(None,()))
    power,toughness = (int(record[k]) for k in ('power','toughness'))
    if (not effect or not match or match[2] and match[1]!=match[2] or min(power,toughness)<1
            or set(record['keywords'])!={'Crew',*keywords}):
        raise ValueError('complete supported triggered Vehicle rules required')
    return dict(effect=effect,cost=cost_from_oracle(record),threshold=int(match[1]),power=power,
                toughness=toughness,keywords=list(keywords))


def variants(model):
    options = [f'{mode}_{seat}' for mode in (*COMMON,*MODES[model['effect']]) for seat in (0,1)]
    if model['effect']=='loot':
        options += ['attack_decline_0','block_decline_0']
    return options


def spec(declaration,record):
    model = parameters(record)
    selected = declaration.get('variants')
    if (declaration.get('family')!=FAMILY or type(declaration.get('version')) is not int or declaration['version']!=2
            or declaration.get('oracle_text')!=record['oracle_text'] or not exact(declaration.get('params'),model)
            or pins()['sources'].get(record['name'],{}).get('oracle_record_sha256')!=record_hash(record)
            or type(selected) is not list or not selected or any(type(item) is not str for item in selected)
            or len(set(selected))!=len(selected) or set(selected)-set(variants(model))):
        raise ValueError('pinned triggered Vehicle and exact typed variants required')
    return dict(model,family=FAMILY)


def validate_witnesses(oracle,rows):
    profile = pins()
    if profile['version']!=1:
        raise ValueError('unreviewed triggered Vehicle profile')
    for group in ('sources','witnesses'):
        for name,pin in profile[group].items():
            row,record = rows.get(name,{}),oracle.get(name,{})
            if (pin['oracle_record_sha256']!=record_hash(record) or not exact(pin['registry'],row)
                    or row.get('kind')!='card' or row.get('handler') in (None,'NULL','0','card_dummy')
                    or record.get('layout')!='normal'):
                raise ValueError(f'changed or unsupported triggered Vehicle record: {name}')


class TriggeredCrewScene(CrewScene):
    def __init__(self,name,model,mode,owner,rows,oracle):
        super().__init__(name,model,mode,owner,rows,oracle)
        self.counters = 0
        self.loyalty = None
        self.deck = []
        if mode.startswith('block_'):
            self.active = self.foe

    def setup_cards(self):
        mode,effect = self.mode,self.effect
        preset_barge = effect=='draw' and mode not in CAST
        if preset_barge:
            self.seed_draws()
        self.add(self.name,'source',self.owner,'hand' if mode in CAST or effect=='animate' else 'battlefield')
        if preset_barge:
            # Battlefield setup uses real entry, so its already-seeded library is live.
            self.draw_one()
        self.add('Unsummon','type_probe',self.foe,'hand')
        if mode not in ('cast','cast_short','attack_uncrewed',*ENTRY):
            if mode in ('no_target','enemy_target'):
                self.add_pilot(PILOT[self.model['threshold']])
            else:
                self.setup_cost(self.model['threshold'])
        if effect=='draw' and not preset_barge or effect=='loot':
            self.seed_draws()
        if mode in ('creature','lethal','walker'):
            name = {'creature':'Goblin Tinkerer','lethal':'Mogg Fanatic','walker':'Jace Beleren'}[mode]
            self.add(name,'victim',self.foe)
            if mode=='walker':
                self.loyalty = int(self.oracle[name]['loyalty'])
        if mode in ('trample','entry_combat','attack_ground'):
            self.add('Goblin Tinkerer','blocker',self.foe)
        if mode=='other_attacker' or mode.startswith('block_'):
            self.add('Welkin Tern' if mode.startswith('block_') else 'Mogg Fanatic',
                     'attacker',self.active)
        if mode in ('artifact_target','ready_target'):
            self.add('Darksteel Relic','recipient',self.owner,tapped=mode=='artifact_target')
        if mode in ('no_target','enemy_target','entry_repeat'):
            self.add('Boomerang','bounce',self.foe,'hand')
        if mode=='enemy_target':
            self.add('Darksteel Relic','recipient',self.foe,tapped=True)

    def seed_draws(self):
        names = ('Island','Forest')
        if self.effect=='loot' and self.owner==1 and self.mode in (
                'attack_accept','attack_empty_hand','attack_ground','trigger_replay'):
            # The real AI policy accepts on an attack with more than eleven cards.
            names += ('Forest',)*10
        for name in names:
            ref = f'library_{len(self.deck)}'
            self.deck.append(ref)
            self.add(name,ref,self.owner,'library')
        if (self.effect=='loot' and self.owner==0 and self.mode not in ('attack_empty_hand','cast','cast_short')):
            self.add('Plains','discardable',self.owner,'hand')

    def draw_one(self):
        ref = self.deck.pop(0)
        self.entities[ref]['zone'] = 'hand'
        return ref

    def stats(self,ref):
        power,toughness = super().stats(ref)
        count = self.counters if ref=='source' else 0
        return power+count,toughness+count

    def receipts(self):
        super().receipts()
        if self.effect=='counter' and self.entities['source']['zone']=='battlefield':
            self.emit('assert_counter',ref='source',counter_kind='plus1',value=self.counters)
        if self.loyalty is not None:
            self.emit('assert_counter',ref='victim',counter_kind='loyalty',value=self.loyalty)

    def cast_vehicle(self):
        fee = self.model['cost']
        allowed = self.mode!='cast_short'
        budget = fee.copy()
        if not allowed:
            budget[0] -= 1
        self.fund(self.owner,budget)
        self.emit('cast',ref='source',value=int(allowed))
        self.emit('assert_choices_consumed')
        self.emit('assert_stack',value=int(allowed))
        self.prop('source','in_hand',not allowed)
        if allowed:
            self.pay(self.owner,fee)
            self.pools()
            self.resolve()
            self.entities['source']['zone'] = 'battlefield'
            if self.effect=='draw':
                self.draw_one()
            self.animated = self.effect=='animate'
        self.receipts()

    def bounce(self,ref):
        self.cast_spell('bounce',ref)
        self.resolve()
        self.leave('bounce','graveyard')
        self.leave(ref,'hand')
        self.receipts()

    def loot_input(self,accept,block):
        if self.owner==0:
            if block:
                self.emit('choose_dialog',player=0,value=0 if accept else 1)
            elif accept:
                self.emit('choose_trigger_source',player=0,ref='source')
        if not accept:
            return
        discard = 'discardable' if 'discardable' in self.entities else self.deck[0]
        if self.owner==0:
            name = self.entities[discard]['name']
            self.emit('choose_zone_card',player=0,value=self.rows[name]['csvid'])
        drawn = self.draw_one()
        if self.owner==1 and discard!=drawn:
            raise ValueError('AI loot requires its sole real hand card')
        self.leave(discard,'graveyard')

    def configure_attack_trigger(self):
        mode,effect = self.mode,self.effect
        if effect=='damage':
            if mode in ('creature','lethal','walker'):
                self.emit('choose_target',player=self.owner,ref='victim')
            else:
                self.emit('choose_player',player=self.owner,value=self.owner if mode=='controller' else self.foe)
        elif effect=='loot':
            self.loot_input(mode!='attack_decline',block=False)
        elif effect=='untap' and mode!='no_target':
            ref = 'recipient' if mode in ('artifact_target','ready_target','enemy_target') else self.pilots[0]
            self.emit('choose_target',player=self.owner,ref=ref)

    def attack_effect(self):
        mode,effect = self.mode,self.effect
        if effect=='damage':
            if mode=='creature':
                self.entities['victim']['damage'] += 1
            elif mode=='lethal':
                self.leave('victim','graveyard')
            elif mode=='walker':
                self.loyalty -= 1
            else:
                self.life[self.owner if mode=='controller' else self.foe] -= 1
        elif effect=='pump':
            self.entities['source']['bonus'] = (1,1)
        elif effect=='counter':
            self.counters += 1
        elif effect=='untap' and mode not in ('no_target','enemy_target'):
            ref = 'recipient' if mode in ('artifact_target','ready_target') else self.pilots[0]
            self.entities[ref]['tapped'] = False

    def attack_sequence(self,ref):
        source = ref=='source'
        self.emit('begin_attackers')
        self.emit('attack',ref=ref,value=1)
        self.entities[ref]['tapped'] = True
        self.receipts()
        if self.mode=='trigger_replay':
            self.replay = len(self.steps)
        unexpected_probe = (not source and self.mode=='other_attacker' and self.owner==1
                            and self.effect in ('damage','untap'))
        if source:
            self.configure_attack_trigger()
        elif unexpected_probe:
            # Leave the fallback unused when another creature's attack correctly does not trigger this Vehicle.
            if self.effect=='damage':
                self.emit('choose_player',player=self.owner,value=self.foe)
            else:
                self.emit('choose_target',player=self.owner,ref=self.pilots[0])
        self.emit('finish_attackers')
        if unexpected_probe or source and self.mode=='enemy_target':
            # Drain the conditional choice and require that no wrong trigger or target was offered.
            self.emit('assert_unused_choices',value=1)
        self.resolve()
        if source:
            self.attack_effect()
        self.receipts()
        blocked = self.mode in ('trample','entry_combat')
        if self.effect=='pump' and blocked:
            # The common combat ledger models trample via the resolved ability.
            self.model = dict(self.model,keywords=[*self.model['keywords'],'Trample'])
        self.finish_combat(ref,'blocker' if blocked or self.mode=='attack_ground' else None,blocked=blocked)

    def block_sequence(self):
        self.declare_attacker('attacker')
        self.emit('begin_blockers')
        self.emit('block',ref='source',other='attacker',value=1)
        self.prop('source','blocking_count',1)
        self.loot_input(self.mode!='block_decline',block=True)
        self.emit('advance_combat')
        self.resolve()
        self.receipts()
        self.emit('combat_damage')
        self.resolve()
        self.emit('end_combat')
        self.entities['source']['damage'] = self.stats('attacker')[0]
        self.leave('attacker','graveyard')
        self.receipts()

    def next_own_main(self):
        for active in (self.foe,self.owner):
            self.emit('advance_turn')
            self.emit('untap_step',player=active)
            self.emit('turn',player=active,value=0)
            for state in self.entities.values():
                if state['zone']=='battlefield' and state['controller']==active:
                    state['tapped'] = False
            self.receipts()

    def cleanup_and_recrew(self):
        self.animated = False
        self.cleanup()
        self.next_own_main()
        self.start_crew(self.pilots)
        self.finish_crew()

    def play(self):
        self.setup_cards()
        self.refresh_zero()
        mode = self.mode
        if mode in CAST:
            if mode=='entry_replay':
                self.replay = len(self.steps)
            self.cast_vehicle()
            if mode=='entry_repeat':
                self.bounce('source')
                self.emit('bind_hand',ref='source',player=self.owner,value=self.csvid)
                self.cast_vehicle()
                return
            if mode=='entry_combat':
                self.attack_sequence('source')
            if mode in ('entry_expires','entry_combat'):
                self.animated = False
                self.cleanup()
            if mode!='fresh_vehicle':
                return
            self.animated = False
            self.cleanup_and_recrew()
            self.attack_sequence('source')
            return
        if self.effect=='animate':
            # Establish a finished real entry before testing an ordinary uncrewed Vehicle.
            self.cast_vehicle()
            self.animated = False
            self.cleanup()
            self.next_own_main()
        if mode=='attack_uncrewed':
            self.declare_attacker('source',allowed=False)
            self.receipts()
            return
        if mode=='underpowered':
            self.start_crew((),available=False)
            return
        if mode=='partial_cancel':
            self.start_crew(self.pilots[:1],cancel=True)
            return
        self.start_crew(self.pilots)
        self.finish_crew()
        if mode in ('ordered_exact','pending_replay','no_draw_crew'):
            return
        if mode=='expires':
            self.animated = False
            self.cleanup()
            return
        if mode in ('no_target','enemy_target'):
            self.bounce(self.pilots[0])
        if mode.startswith('block_'):
            self.block_sequence()
            return
        self.attack_sequence('attacker' if mode=='other_attacker' else 'source')
        if mode in ('cleanup_recrew','repeat_attack'):
            self.cleanup_and_recrew()
            if mode=='repeat_attack':
                self.attack_sequence('source')


def expand(name,csvid,declaration,model,selected,rows,oracle):
    from recipes import step

    validate_witnesses(oracle,rows)
    if csvid!=pins()['sources'][name]['registry']['csvid'] or set(selected)-set(variants(model)):
        raise ValueError('unreserved triggered Vehicle or variant')
    result = []
    for variant in selected:
        mode,seat = variant.rsplit('_',1)
        scene = TriggeredCrewScene(name,model,mode,int(seat),rows,oracle)
        scene.play()
        steps = scene.steps
        if scene.replay is not None:
            at = scene.replay
            suffix = steps[at:]
            steps = steps[:at]+[step('capture')]+suffix+[step('record_state'),step('ai_roundtrip'),step('restore')]+suffix+[step('assert_recorded_state')]
        metadata = dict(family=FAMILY,version=2,variant=variant,params=declaration['params'],oracle_text=declaration['oracle_text'],
                        covered_clauses=declaration['oracle_text'].splitlines(),
                        untested_components=[*GAPS,*EFFECT_GAPS.get(model['effect'],[])])
        result.append(dict(name=f'VehicleTriggeredCrewRecipe_{csvid}_{variant}',card=name,fixture='empty_main',seed=1900,
                           setup=scene.setup,steps=steps,_recipe=metadata))
    return result
