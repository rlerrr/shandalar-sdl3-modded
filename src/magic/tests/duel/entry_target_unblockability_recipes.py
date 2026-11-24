"""Temporary targeted unblockability from a genuine creature-entry trigger."""

from functools import lru_cache
import json
from pathlib import Path
import re

from activated_unblockability_recipes import EvasionScene, TARGET
from death_counter_trigger_recipes import exact


FAMILY = 'entry_target_unblockability'
PROFILE = Path(__file__).parent/'data/entry_target_unblockability_wave8_profiles.json'
INTRINSIC = "This creature can't be blocked."
ENTRY = 'When this creature enters, '
COMMON = ('effect','no_cast','short','wrong_color','countered','self_only','enemy_target',
          'source_leaves','expiry','bounce_recast','pending_replay')
GAPS = ('Separate target placement and response window for the legacy synchronous entry callback',
        'Direct offered-target enumeration for entry triggers','Multiplayer')


@lru_cache(maxsize=1)
def pins():
    return json.loads(PROFILE.read_bytes())


def parameters(record):
    from recipes import cost_from_oracle

    if (not record or record.get('layout')!='normal' or record.get('card_faces') or record.get('selected_face')
            or not record['type_line'].startswith('Creature — ') or record.get('keywords')
            or any(not re.fullmatch(r'[1-9][0-9]*',record.get(stat,'')) for stat in ('power','toughness'))):
        raise ValueError('normal fixed-stat creature with complete entry clause required')
    lines = record['oracle_text'].splitlines()
    innate = len(lines)==2 and lines[0]==INTRINSIC
    if len(lines)==1 or innate:
        clause = lines[-1]
        target_clause = clause.removeprefix(ENTRY) if clause.startswith(ENTRY) else ''
        target = TARGET.fullmatch('Target '+target_clause[7:]) if target_clause.startswith('target ') else None
    else:
        target = None
    if not target or target[3] or target[4]:
        raise ValueError('whole targeted unblockability entry effect required')
    limit = int(target[2]) if target[2] else None
    return dict(effect='entry_target',cast_mana=cost_from_oracle(record),intrinsic=innate,power_limit=limit,
                power=int(record['power']),toughness=int(record['toughness']))


def variants(model):
    modes = list(COMMON)
    if model['intrinsic']:
        modes += ['intrinsic','intrinsic_replay']
    if model['power_limit'] is not None:
        modes += ['power_boundary','above_boundary_alternative','live_power_alternative']
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
        raise ValueError('pinned entry source, exact typed parameters, and distinct variants required')
    return dict(model,family=FAMILY)


def validate_witnesses(oracle,rows):
    profile = pins()
    if profile['version']!=1:
        raise ValueError('unreviewed entry profile')
    for group in ('sources','witnesses'):
        for name,pin in profile[group].items():
            row,record = rows.get(name,{}),oracle.get(name,{})
            if (not exact(pin,dict(oracle=record,registry=row)) or record.get('layout')!='normal'
                    or row.get('kind')!='card' or row.get('handler') in (None,'NULL','0','card_dummy')):
                raise ValueError(f'implemented entry source or witness changed: {name}')


class EntryScene(EvasionScene):
    def __init__(self,name,model,mode,owner,rows,oracle):
        super().__init__(name,model,mode,owner,rows,oracle)
        # The caster remains active even when the entry trigger targets a defender.
        self.active = self.owner

    def setup_cards(self):
        mode = self.mode
        self.add(self.name,'source',self.owner,'hand')
        if mode!='self_only':
            ally = {'power_boundary':'Catapult Squad','above_boundary_alternative':'Unyielding Krumar'}.get(mode,'Goblin Tinkerer')
            self.add(ally,'ally',self.owner)
        only_self = mode in ('self_only','above_boundary_alternative','live_power_alternative')
        self.add('Gladecover Scout' if only_self else 'Goblin Tinkerer','blocker',self.foe)
        if mode in ('source_leaves','bounce_recast'):
            self.add('Unsummon','bounce',self.foe,'hand')
        if mode=='live_power_alternative':
            self.add('Giant Growth','growth',self.owner,'hand')
        if mode=='countered':
            self.add('Counterspell','counter',self.foe,'hand')

    def shown_target(self):
        if self.mode in ('self_only','above_boundary_alternative','live_power_alternative'):
            return 'source'
        if self.mode=='enemy_target':
            return 'blocker'
        return 'ally'

    def precast_funding(self):
        fee = self.model['cast_mana']
        budget = fee.copy()
        if self.mode=='short':
            budget[0] -= 1
        elif self.mode=='wrong_color':
            color = next(i for i,n in enumerate(budget) if i and n)
            budget[1],budget[color] = budget[color],0
        self.fund(self.owner,budget)

    def enter(self):
        self.precast_funding()
        allowed = self.mode not in ('short','wrong_color')
        self.emit('cast',ref='source',value=int(allowed))
        self.emit('assert_choices_consumed')
        self.emit('assert_stack',value=int(allowed))
        if not allowed:
            self.receipts()
            return False
        self.pay(self.owner,self.model['cast_mana'])
        self.prop('source','in_hand',0)
        self.prop('source','in_play',0)
        self.entities['source']['zone'] = 'stack'
        self.pools()
        if self.mode=='countered':
            self.counter_pending_spell()
            return False
        if self.mode=='pending_replay':
            self.replay = len(self.steps)
        target = self.shown_target()
        # Both native handlers offer the mandatory target inside ETB_ABILITY.
        self.emit('choose_target',player=self.owner,ref=target)
        self.resolve()
        self.entities['source']['zone'] = 'battlefield'
        self.recipient = target
        self.prop('source','sick',1)
        self.receipts()
        return True

    def counter_pending_spell(self):
        from recipes import cost_from_oracle

        fee = cost_from_oracle(self.oracle['Counterspell'])
        self.emit('announce_spell',ref='source')
        self.fund(self.foe,fee)
        # The announced parent is Counterspell's automatic native target.
        self.emit('cast_response',ref='counter',value=1)
        self.emit('assert_stack',value=2)
        self.emit('assert_choices_consumed')
        self.pay(self.foe,fee)
        self.resolve(parent=True)
        self.leave('source','graveyard')
        self.leave('counter','graveyard')
        self.receipts()

    def bounce_source(self):
        self.cast_spell('bounce','source')
        self.resolve()
        self.leave('bounce','graveyard')
        self.leave('source','hand')
        self.emit('bind_hand',ref='source',player=self.owner,value=self.csvid)
        self.receipts()

    def next_own_turn(self):
        self.advance()
        self.advance()
        self.emit('untap_step',player=self.owner)
        for e in self.entities.values():
            if e['zone']=='battlefield' and e['controller']==self.owner:
                e['tapped'] = False
        self.recipient = None
        self.emit('turn',player=self.owner,value=0)
        self.receipts()

    def combat(self, attacker='ally'):
        if attacker=='ally':
            return super().combat()
        fresh_self = self.mode=='self_only'
        self.emit('begin_attackers')
        self.emit('attack',ref='source',value=int(not fresh_self))
        self.emit('finish_attackers')
        if not fresh_self:
            self.entities['source']['tapped'] = True
            self.emit('begin_blockers')
            self.emit('block',ref='blocker',other='source',value=0)
            self.emit('advance_combat')
            self.prop('source','blocked',0)
            self.emit('combat_damage')
            self.resolve()
            self.emit('end_combat')
            self.life[self.foe] -= self.stats('source')[0]
        self.receipts()

    def play(self):
        self.setup_cards()
        self.turn_setup()
        mode = self.mode
        if mode=='no_cast':
            self.combat()
            return
        if mode=='live_power_alternative':
            self.cast_spell('growth','ally')
            self.resolve()
            self.leave('growth','graveyard')
            self.entities['ally']['bonus'] = (3,3)
            self.receipts()
        if not self.enter():
            self.combat()
            return
        if mode=='self_only':
            self.combat('source')
            return
        if mode in ('intrinsic','intrinsic_replay'):
            self.next_own_turn()
            if mode=='intrinsic_replay':
                self.replay = len(self.steps)
            self.combat('source')
            return
        if mode=='source_leaves':
            self.bounce_source()
        self.combat()
        if mode=='bounce_recast':
            self.bounce_source()
        if mode in ('expiry','bounce_recast'):
            self.next_own_turn()
            if mode=='bounce_recast':
                self.enter()
            self.combat()


def expand(name,csvid,declaration,model,selected,rows,oracle):
    from recipes import step

    validate_witnesses(oracle,rows)
    if csvid!=pins()['sources'][name]['registry']['csvid']:
        raise ValueError('unreserved entry evasion source')
    result = []
    for variant in selected:
        mode,actor = variant.rsplit('_',1)
        scene = EntryScene(name,model,mode,int(actor),rows,oracle)
        scene.play()
        steps = scene.steps
        if scene.replay is not None:
            at = scene.replay
            suffix = steps[at:]
            steps = steps[:at]+[step('capture')]+suffix+[step('record_state'),step('ai_roundtrip'),step('restore')]+suffix+[step('assert_recorded_state')]
        metadata = dict(family=FAMILY,version=2,variant=variant,params=declaration['params'],oracle_text=declaration['oracle_text'],
                        covered_clauses=declaration['oracle_text'].splitlines(),untested_components=list(GAPS))
        result.append(dict(name=f'EntryTargetUnblockabilityRecipe_{csvid}_{variant}',card=name,fixture='empty_main',seed=17321,
                           setup=scene.setup,steps=steps,_recipe=metadata))
    return result
