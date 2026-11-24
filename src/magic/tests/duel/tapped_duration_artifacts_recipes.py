"""Targeted artifact modifiers whose duration follows the tapped source."""

from functools import lru_cache
import json
from pathlib import Path
import re

from death_counter_trigger_recipes import exact
from morph_turn_up_team_recipes import TeamScene


FAMILY = 'tapped_duration_artifacts'
PROFILE = Path(__file__).parent / 'data/tapped_duration_artifacts_wave8_profiles.json'
OPTION = 'You may choose not to untap this artifact during your untap step.'
TARGETED_DURATION = re.compile(
    r'\{([1-9][0-9]*)\}, \{T\}: Target creature gets \+([0-9]+)/\+([0-9]+) for as long as this artifact remains tapped\.'
)
MODES = ('cast', 'cast_short', 'activate', 'fee_short', 'initial_tapped', 'no_creature', 'cancel',
         'enemy_target', 'offturn', 'keep_tapped', 'untap_after_target_leaves', 'opponent_untap',
         'untap_external', 'untap_pending', 'retap', 'source_leaves', 'source_leaves_pending',
         'target_leaves_pending', 'target_bounce', 'reactivate', 'combat', 'combat_no_boost',
         'replay', 'pending_replay')
HUMAN_CHOICES = ('untap_choice_0', 'untap_choice_replay_0')


@lru_cache(maxsize=1)
def pins():
    return json.loads(PROFILE.read_text(encoding='utf8'))


def parameters(record):
    from recipes import cost_from_oracle

    if (not record or record.get('layout') != 'normal' or record.get('selected_face') or 'card_faces' in record or
            record.get('type_line') != 'Artifact' or record.get('keywords')):
        raise ValueError('complete normal untap-choice artifact required')
    lines = record['oracle_text'].splitlines()
    match = TARGETED_DURATION.fullmatch(lines[1]) if len(lines)==2 and lines[0]==OPTION else None
    if not match or not any(map(int, match.groups()[1:])):
        raise ValueError('complete whole targeted duration modifier required')
    fee,power,toughness = map(int,match.groups())
    return dict(effect='tapped_duration',cast_mana=cost_from_oracle(record),activation_generic=fee,
                power_delta=power,toughness_delta=toughness)


def variants(_model):
    return [f'{mode}_{actor}' for actor in (0,1) for mode in MODES] + list(HUMAN_CHOICES)


def spec(declaration, record):
    model = parameters(record)
    pin = pins()['sources'].get(record['name'], {})
    selected = declaration.get('variants')
    if (not exact(pin.get('oracle'), record) or declaration.get('family') != FAMILY or
            type(declaration.get('version')) is not int or declaration['version'] != 2 or
            declaration.get('oracle_text') != record['oracle_text'] or not exact(declaration.get('params'), model) or
            type(selected) is not list or not selected or any(type(v) is not str for v in selected) or
            len(set(selected)) != len(selected) or set(selected) - set(variants(model))):
        raise ValueError('independently pinned artifact and typed distinct duration variants required')
    return dict(model,family=FAMILY)


def validate_witnesses(oracle, rows):
    for group in ('sources','witnesses'):
        for name,pin in pins()[group].items():
            row = rows.get(name,{})
            if (not exact(pin,dict(oracle=oracle.get(name),registry=row)) or row.get('kind')!='card' or
                    row.get('handler') in (None,'NULL','0','card_dummy') or oracle[name].get('layout')!='normal'):
                raise ValueError(f'implemented original artifact or witness changed: {name}')


class DurationScene(TeamScene):
    def __init__(self,name,model,mode,owner,rows,oracle):
        super().__init__(name,model,mode,owner,rows,oracle)
        self.recipient = None

    def stats(self,ref):
        power,toughness = super().stats(ref)
        if ref==self.recipient and self.entities['source']['tapped']:
            return power+self.model['power_delta'],toughness+self.model['toughness_delta']
        return power,toughness

    def leave(self,ref,zone):
        super().leave(ref,zone)
        if ref in ('source',self.recipient):
            self.recipient = None

    def setup_cards(self):
        mode = self.mode
        self.add(self.name,'source',self.owner,'hand')
        if mode=='no_creature':
            self.add('Forest','noncreature',self.foe,tapped=True)
            return
        self.add('Goblin Tinkerer','ally',self.owner)
        self.add('Serra Angel','enemy',self.foe)
        if mode in ('keep_tapped','untap_after_target_leaves','opponent_untap','untap_choice','untap_choice_replay'):
            self.add('Mogg Fanatic','ready_witness',self.owner,tapped=True)
        if mode=='opponent_untap':
            self.add('Mogg Fanatic','opponent_witness',self.foe,tapped=True)
        if mode in ('combat','combat_no_boost'):
            name = 'Dragon Whelp' if self.model['toughness_delta']==0 else 'Catapult Squad'
            self.add(name,'blocker',self.foe)
        if mode in ('initial_tapped','untap_external','untap_pending','retap','reactivate'):
            self.add('Twiddle','twiddle',self.foe,'hand')
        if mode=='retap':
            self.add('Twiddle','retap',self.foe,'hand')
        if mode in ('source_leaves','source_leaves_pending'):
            self.add('Disenchant','removal',self.foe,'hand')
        if mode in ('target_leaves_pending','target_bounce','untap_after_target_leaves'):
            self.add('Unsummon','removal',self.foe,'hand')

    def source_traits(self,full=False):
        self.prop('source','csvid',self.csvid)
        if full:
            self.emit('assert_matching_permanents',player=self.owner,index=self.csvid,property='artifact',value=1)
            self.emit('assert_matching_permanents',player=self.owner,index=self.csvid,property='color',value=0)

    def cast_artifact(self):
        fee = self.model['cast_mana']
        budget = list(fee)
        allowed = self.mode!='cast_short'
        if not allowed:
            budget[0] -= 1
        self.fund(self.owner,budget)
        self.emit('cast',ref='source',value=int(allowed))
        self.emit('assert_choices_consumed')
        self.emit('assert_stack',value=int(allowed))
        if not allowed:
            self.receipts()
            return False
        self.pay(self.owner,fee)
        self.entities['source']['zone'] = 'stack'
        self.source_traits()
        self.pools()
        self.resolve()
        self.entities['source']['zone'] = 'battlefield'
        self.receipts()
        return True

    def begin_activation(self, target='ally', *, available=True, cancel=False, short=False):
        fee = self.model['activation_generic']
        self.fund(self.owner,(fee-int(short),0,0,0,0,0))
        if self.mode=='replay':
            self.replay = len(self.steps)
        self.emit('can_activate',ref='source',value=int(available))
        if available:
            if cancel:
                self.emit('choose_cancel_target',player=self.owner)
            else:
                self.emit('choose_target',player=self.owner,ref=target)
        accepted = available and not cancel
        self.emit('activate',ref='source',value=int(accepted))
        self.emit('assert_choices_consumed')
        self.emit('assert_stack',value=int(accepted))
        if not accepted:
            self.receipts()
            return False
        self.pay(self.owner,(fee,0,0,0,0,0))
        self.entities['source']['tapped'] = True
        self.prop('source','tapped',1)
        for prop,value in zip(('power','toughness'),self.stats(target)):
            self.prop(target,prop,value)
        self.pools()
        return True

    def end_activation(self,target):
        self.resolve()
        if (self.entities['source']['zone']=='battlefield' and self.entities['source']['tapped'] and
                self.entities[target]['zone']=='battlefield'):
            self.recipient = target
        self.receipts()

    def start_response(self,ref,target):
        from recipes import cost_from_oracle

        entity = self.entities[ref]
        actor = entity['owner']
        fee = cost_from_oracle(self.oracle[entity['name']])
        self.fund(actor,fee)
        self.emit('choose_target',player=actor,ref=target)
        self.emit('cast',ref=ref,value=1)
        self.emit('assert_choices_consumed')
        self.emit('assert_stack',value=2)
        self.pay(actor,fee)
        entity['zone'] = 'stack'
        self.pools()

    def use_twiddle(self,ref,untap,pending=False):
        if pending:
            self.start_response(ref,'source')
        else:
            self.cast_spell(ref,'source')
        self.emit('choose_dialog',player=self.foe,value=int(untap))
        self.resolve()
        self.entities['source']['tapped'] = not untap
        if untap:
            self.recipient = None
        self.leave(ref,'graveyard')
        self.receipts()

    def remove(self,ref,zone,pending=False):
        if pending:
            self.start_response('removal',ref)
        else:
            self.cast_spell('removal',ref)
        self.resolve()
        self.leave('removal','graveyard')
        self.leave(ref,zone)
        self.receipts()

    def untap(self,actor,keep_source=False):
        assert self.active==actor and not any(map(any,self.mana))
        source = self.entities['source']
        optional = actor==0 and actor==self.owner and source['tapped']
        if optional:
            if keep_source:
                self.emit('choose_priority',player=actor,value=-2)
            else:
                self.emit('choose_priority',ref='source')
        self.emit('untap_step',player=actor)
        self.emit('assert_choices_consumed')
        for ref,e in self.entities.items():
            if e['zone']=='battlefield' and e['controller']==actor and not (ref=='source' and keep_source):
                e['tapped'] = False
        if not source['tapped']:
            self.recipient = None
        self.receipts()

    def activate_then(self,target):
        if not self.begin_activation(target):
            raise ValueError('positive duration fixture missing its activation')
        mode = self.mode
        if mode=='pending_replay':
            self.replay = len(self.steps)
        if mode=='untap_pending':
            self.use_twiddle('twiddle',untap=True,pending=True)
        elif mode=='source_leaves_pending':
            self.remove('source','graveyard',pending=True)
        elif mode=='target_leaves_pending':
            self.remove(target,'hand',pending=True)
        else:
            self.end_activation(target)

    def after_resolution(self):
        mode = self.mode
        if mode in ('untap_external','retap','reactivate'):
            self.use_twiddle('twiddle',untap=True)
            if mode=='retap':
                self.use_twiddle('retap',untap=False)
            elif mode=='reactivate':
                self.begin_activation('enemy')
                self.end_activation('enemy')
        elif mode=='source_leaves':
            self.remove('source','graveyard')
        elif mode in ('target_bounce','untap_after_target_leaves'):
            self.remove('ally','hand')
            if mode=='target_bounce':
                self.emit('bind_hand_copy',ref='ally',player=self.owner,index=0,value=self.rows['Goblin Tinkerer']['csvid'])
                self.cast_spell('ally',None)
                self.resolve()
                self.entities['ally']['zone'] = 'battlefield'
                self.receipts()
        if mode in ('keep_tapped','untap_after_target_leaves','opponent_untap','untap_choice','untap_choice_replay'):
            self.cleanup()
            self.advance()
            self.untap(self.foe)
            if mode!='opponent_untap':
                self.advance()
                if mode=='untap_choice_replay':
                    self.replay = len(self.steps)
                self.untap(self.owner,keep_source=mode=='keep_tapped')

    def play(self):
        self.setup_cards()
        self.emit('turn',player=self.owner,value=0)
        self.emit('present_ai_dialogs',value=1)
        for p in (0,1):
            for color in range(6):
                self.emit('set_mana',player=p,color=color,value=0)
        self.emit('refresh')
        if not self.cast_artifact() or self.mode=='cast':
            return
        mode = self.mode
        if mode in ('fee_short','initial_tapped','no_creature','cancel'):
            if mode=='initial_tapped':
                self.use_twiddle('twiddle',untap=False)
            self.begin_activation(available=mode=='cancel',cancel=mode=='cancel',short=mode=='fee_short')
            return
        if mode=='offturn':
            self.advance()
        if mode in ('combat','combat_no_boost'):
            attackers,blocks = ('ally',),{'ally':'blocker'}
            self.combat_start(attackers,blocks)
            if mode=='combat':
                self.activate_then('ally')
            self.combat_finish(attackers,blocks)
            return
        self.activate_then('enemy' if mode=='enemy_target' else 'ally')
        self.after_resolution()


def expand(name,csvid,declaration,model,selected,rows,oracle):
    from recipes import step

    validate_witnesses(oracle,rows)
    if csvid!=pins()['sources'][name]['registry']['csvid']:
        raise ValueError('unreserved tapped-duration artifact source')
    cases = []
    for variant in selected:
        mode,actor = variant.rsplit('_',1)
        scene = DurationScene(name,model,mode,int(actor),rows,oracle)
        scene.play()
        steps = scene.steps
        if scene.replay is not None:
            at = scene.replay
            suffix = steps[at:]
            steps = steps[:at] + [step('capture')] + suffix + [
                step('record_state'),step('ai_roundtrip'),step('restore')] + suffix + [step('assert_recorded_state')]
        metadata = dict(family=FAMILY,version=2,variant=variant,params=declaration['params'],
                        oracle_text=declaration['oracle_text'],covered_clauses=declaration['oracle_text'].splitlines(),
                        untested_components=['Interactive seat1 untap choice, governed by native AI strategy in this driver',
                                             'Multiplayer'])
        cases.append(dict(name=f'TappedDurationArtifactRecipe_{csvid}_{variant}',card=name,fixture='empty_main',
                          seed=123,setup=scene.setup,steps=steps,_recipe=metadata))
    return cases
