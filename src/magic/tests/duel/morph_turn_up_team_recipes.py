"""Paid Morph and continuous, combat, or other-permanent turn-up effects."""

from functools import lru_cache
import json
from pathlib import Path

from death_counter_trigger_recipes import exact
from morph_creature_recipes import FACE_DOWN, MORPH
from morph_turn_up_target_recipes import HIDDEN_FEE, Scene


FAMILY = 'morph_turn_up_team'
PROFILE = Path(__file__).parent / 'data/morph_turn_up_team_wave8_profiles.json'
VARIABLE = 'Morph {X}{B}{B} (You may cast this card face down as a 2/2 creature for {3}. Turn it face up any time for its morph cost.)'
CLAUSES = {
    'When this creature is turned face up, creatures you control get +0/+2 until end of turn.': ('team', 0, 2),
    'When this creature is turned face up, creatures you control get +2/+2 until end of turn.': ('team', 2, 2),
    'When this creature is turned face up, creatures of the creature type of your choice get +2/+2 and gain trample until end of turn.': ('tribe', 2, 2),
    'When this creature is turned face up, all creatures get -X/-X until end of turn.': ('mass_x', 0, 0),
    'When this creature is turned face up, prevent all combat damage non-Soldier creatures would deal this turn.': ('prevention', 0, 0),
    'Whenever this creature or another creature you control is turned face up, untap that creature.': ('untap', 0, 0),
}
BASE = ('hidden', 'faceup', 'hidden_choice', 'hidden_short', 'flip_short', 'flip_wrong', 'offturn', 'replay')
ADDITIONAL = {
    'team': ('team', 'late_entry', 'blocked', 'blocked_hidden'),
    'tribe': ('goblins', 'angels', 'trample', 'trample_hidden', 'trample_expired'),
    'mass_x': ('x_zero', 'x_one_reserved', 'x_two', 'x_three'),
    'prevention': ('prevent_attack', 'prevent_block', 'hidden_combat', 'spell_damage', 'expiry'),
    'untap': ('self_attack', 'ally_flip', 'opposing_flip', 'watcher_hidden'),
}


@lru_cache(maxsize=1)
def pins():
    return json.loads(PROFILE.read_text(encoding='utf8'))


def parameters(record):
    from recipes import cost_from_oracle

    if (not record or record.get('layout') != 'normal' or record.get('selected_face') or
            'card_faces' in record or not record.get('type_line', '').startswith('Creature — ') or
            record.get('keywords') != ['Morph']):
        raise ValueError('whole normal Morph creature required')
    lines = record['oracle_text'].splitlines()
    if len(lines) != 2 or lines[1] not in CLAUSES:
        raise ValueError('complete supported team or combat turn-up clause required')
    effect, power_delta, toughness_delta = CLAUSES[lines[1]]
    variable = lines[0] == VARIABLE
    fixed = MORPH.fullmatch(lines[0])
    if variable != (effect == 'mass_x') or not (fixed or variable):
        raise ValueError('complete reviewed Morph mana cost required')
    return dict(effect=effect, power_delta=power_delta, toughness_delta=toughness_delta,
                cast_mana=cost_from_oracle(record), flip_mana=cost_from_oracle(
                    dict(record, mana_cost='{B}{B}' if variable else fixed[1])),
                variable=variable, power=int(record['power']), toughness=int(record['toughness']))


def variants(model):
    return [f'{mode}_{actor}' for actor in (0, 1) for mode in BASE + ADDITIONAL[model['effect']]]


def spec(declaration, record):
    model = parameters(record)
    pin = pins()['sources'].get(record['name'], {})
    selected = declaration.get('variants')
    if (not exact(pin.get('oracle'), record) or declaration.get('family') != FAMILY or
            type(declaration.get('version')) is not int or declaration['version'] != 2 or
            declaration.get('oracle_text') != record['oracle_text'] or
            not exact(declaration.get('params'), model) or type(selected) is not list or not selected or
            any(type(v) is not str for v in selected) or len(set(selected)) != len(selected) or
            set(selected) - set(variants(model))):
        raise ValueError('exact indexed Morph team declaration and explicit distinct variants required')
    return dict(model, family=FAMILY)


def validate_witnesses(oracle, rows):
    data = pins()
    if not exact(data['hidden_observer'], rows.get('Face-Down Creature')) or data['hidden_observer']['csvid'] != FACE_DOWN:
        raise ValueError('live face-down characteristics observer changed')
    for group in ('sources', 'witnesses'):
        for name, pin in data[group].items():
            row = rows.get(name, {})
            if (not exact(pin, dict(oracle=oracle.get(name), registry=row)) or row.get('kind') != 'card' or
                    row.get('handler') in (None, 'NULL', '0', 'card_dummy') or oracle[name].get('layout') != 'normal'):
                raise ValueError(f'implemented original team or witness changed: {name}')


class TeamScene(Scene):
    """Track expected recipients and damage independently of the native callbacks."""

    def __init__(self, name, model, mode, owner, rows, oracle):
        super().__init__(name, model, mode, owner, rows, oracle)
        self.secondary_hidden = False
        self.prevention = False

    def observed_id(self, ref):
        return FACE_DOWN if ref == 'secondary' and self.secondary_hidden else super().observed_id(ref)

    def stats(self, ref):
        if ref == 'secondary' and self.secondary_hidden:
            return tuple(2 + bonus for bonus in self.entities[ref]['bonus'])
        return super().stats(ref)

    def creature_type(self, ref, subtype):
        if (ref == 'source' and self.hidden) or (ref == 'secondary' and self.secondary_hidden):
            return False
        return subtype in self.oracle[self.entities[ref]['name']]['type_line'].partition(' — ')[2].split()

    def active_creatures(self):
        return [ref for ref,e in self.entities.items() if e['zone'] == 'battlefield'
                and 'Creature' in self.oracle[e['name']]['type_line'].partition(' — ')[0].split()]

    def cleanup(self):
        self.prevention = False
        super().cleanup()

    def setup_cards(self):
        self.add(self.name, 'source', self.owner, 'hand')
        mode = self.mode
        if self.effect == 'untap':
            self.add('Icy Manipulator', 'icy', self.owner)
            if mode in ('ally_flip', 'opposing_flip', 'watcher_hidden'):
                actor = self.foe if mode == 'opposing_flip' else self.owner
                self.add('Canyon Lurkers', 'secondary', actor, 'hand')
            return
        self.add('Goblin Tinkerer', 'ally', self.owner)
        if self.effect == 'prevention':
            soldier_owner = self.foe if mode == 'prevent_block' else self.owner
            self.add('Catapult Squad', 'soldier', soldier_owner)
            if mode == 'spell_damage':
                self.add('Shock', 'shock', self.owner, 'hand')
            return
        if self.effect == 'tribe':
            self.add('Mogg Fanatic', 'goblin_model', self.owner)
            self.add('Serra Angel', 'angel_model', self.owner)
            self.add('Goblin Tinkerer', 'foe', self.foe)
        else:
            self.add('Serra Angel', 'foe', self.foe)
        self.add('Icy Manipulator', 'noncreature', self.owner)
        if self.effect == 'mass_x':
            self.add('Catapult Squad', 'small', self.owner)
        if mode in ('blocked', 'blocked_hidden', 'trample', 'trample_hidden', 'trample_expired'):
            self.add('Catapult Squad', 'blocker', self.foe)
        if mode == 'late_entry':
            self.add('Mogg Fanatic', 'late', self.owner, 'hand')

    def chosen_type(self):
        return 'Angel' if self.mode == 'angels' else 'Goblin'

    def choose_tribe(self):
        # The native chooser has an existing creature-to-subtype route. Each
        # witness offered here has exactly one printed creature type.
        ref = 'angel_model' if self.chosen_type() == 'Angel' else 'goblin_model'
        self.emit('choose_dialog', player=self.owner, value=2)
        self.emit('choose_target', player=self.owner, ref=ref)
        self.emit('choose_dialog', player=self.owner, value=0)

    def turn_up_effect(self, ref, x):
        if self.effect == 'untap':
            source = self.entities['source']
            if not self.hidden and self.entities[ref]['controller'] == source['controller']:
                self.entities[ref]['tapped'] = False
            return
        if self.effect == 'prevention':
            self.prevention = True
            return
        for recipient in self.active_creatures():
            e = self.entities[recipient]
            if self.effect == 'team' and e['controller'] != self.owner:
                continue
            if self.effect == 'tribe' and not self.creature_type(recipient, self.chosen_type()):
                continue
            e['bonus'] = (-x, -x) if self.effect == 'mass_x' else (self.model['power_delta'], self.model['toughness_delta'])

    def reap(self, damage=False):
        for ref in self.active_creatures():
            toughness = self.stats(ref)[1]
            if toughness <= 0 or damage and self.entities[ref]['damage'] >= toughness:
                self.leave(ref, 'graveyard')

    def flip_paid(self, ref='source', x=0, extra=0):
        actor = self.entities[ref]['controller']
        fee = list(self.model['flip_mana'] if ref == 'source' else self.secondary_fee())
        fee[0] += x
        budget = fee.copy()
        budget[0] += extra
        denied = self.mode in ('flip_short', 'flip_wrong')
        if denied:
            color = next(i for i,n in enumerate(fee) if i and n)
            budget[color] -= 1
            if self.mode == 'flip_wrong':
                budget[2] += 1
        self.fund(actor, budget)
        if self.mode == 'replay':
            self.replay = len(self.steps)
        self.emit('can_activate', ref=ref, value=int(not denied))
        if self.model['variable'] and not denied:
            self.emit('choose_x', player=actor, value=x)
        if self.effect == 'tribe' and not denied:
            self.choose_tribe()
        self.emit('activate', ref=ref, value=int(not denied))
        self.emit('assert_choices_consumed')
        if denied:
            self.receipts()
            return
        self.pay(actor, fee)
        if ref == 'source':
            self.hidden = False
        else:
            self.secondary_hidden = False
        self.turn_up_effect(ref, x)
        self.reap()
        self.emit('refresh')
        self.pools()
        if self.entities[ref]['zone'] == 'battlefield':
            self.prop(ref, 'csvid', self.observed_id(ref))
        # The native EVENT_TURNED_FACE_UP callback runs during the action.
        # Drain the native proxy after measuring real identity and price.
        self.resolve()
        self.receipts()

    def secondary_fee(self):
        from recipes import cost_from_oracle

        card = self.oracle[self.entities['secondary']['name']]
        clause = MORPH.fullmatch(card['oracle_text'])
        if not clause:
            raise ValueError('second source must be a plain fixed-cost Morph')
        return cost_from_oracle(dict(card, mana_cost=clause[1]))

    def cast_secondary(self):
        actor = self.entities['secondary']['owner']
        assert actor == self.active
        self.fund(actor, HIDDEN_FEE)
        self.emit('cast', ref='secondary', value=1)
        self.emit('assert_stack', value=1)
        self.pay(actor, HIDDEN_FEE)
        self.resolve()
        self.secondary_hidden = True
        self.entities['secondary']['zone'] = 'battlefield'
        self.prop('secondary', 'csvid', FACE_DOWN)
        self.receipts()

    def tap_with_icy(self, ref):
        self.fund(self.owner, (1, 0, 0, 0, 0, 0))
        self.emit('choose_target', player=self.owner, ref=ref)
        self.emit('activate', ref='icy', value=1)
        self.emit('assert_stack', value=1)
        self.pay(self.owner, (1, 0, 0, 0, 0, 0))
        self.resolve()
        self.entities['icy']['tapped'] = self.entities[ref]['tapped'] = True
        self.receipts()

    def combat_start(self, attackers, blocks):
        self.emit('begin_attackers')
        for attacker in attackers:
            self.emit('attack', ref=attacker, value=1)
            self.entities[attacker]['tapped'] = True
        self.emit('finish_attackers')
        self.emit('begin_blockers')
        for attacker,blocker in blocks.items():
            self.emit('block', ref=blocker, other=attacker, value=1)
        self.emit('advance_combat')

    def damage_power(self, ref):
        if self.prevention and not self.creature_type(ref, 'Soldier'):
            return 0
        return max(0, self.stats(ref)[0])

    def combat_finish(self, attackers, blocks, trample=False):
        self.emit('combat_damage')
        self.resolve()
        self.emit('end_combat')
        for attacker in attackers:
            harm = self.damage_power(attacker)
            if attacker not in blocks:
                self.life[1-self.active] -= harm
                continue
            blocker = blocks[attacker]
            spill = max(0, harm - self.stats(blocker)[1]) if trample else 0
            self.entities[blocker]['damage'] += harm - spill
            self.entities[attacker]['damage'] += self.damage_power(blocker)
            self.life[1-self.active] -= spill
        self.reap(damage=True)
        self.receipts()

    def play_stats(self):
        mode = self.mode
        if mode in ('blocked', 'blocked_hidden', 'trample', 'trample_hidden', 'trample_expired'):
            blocks = {'ally':'blocker'}
            if mode == 'trample_expired':
                self.flip_paid()
                self.cleanup()
                self.advance()
                self.advance()
            self.combat_start(('ally',), blocks)
            if mode in ('blocked', 'trample'):
                self.flip_paid()
            self.combat_finish(('ally',), blocks, trample=mode=='trample')
        else:
            x,extra = {'x_zero':(0,0), 'x_one_reserved':(1,2), 'x_two':(2,0), 'x_three':(3,0)}.get(mode, (2,0))
            self.flip_paid(x=x if self.model['variable'] else 0, extra=extra)
            if mode == 'late_entry':
                self.cast_spell('late', None)
                self.resolve()
                self.entities['late']['zone'] = 'battlefield'
                self.receipts()
        if mode not in ('flip_short', 'flip_wrong'):
            self.cleanup()

    def play_prevention(self):
        mode = self.mode
        if mode in ('prevent_attack', 'prevent_block', 'hidden_combat', 'replay'):
            if mode == 'prevent_block':
                self.advance()
                attackers,blocks = ('soldier',), {'soldier':'ally'}
            else:
                attackers,blocks = ('ally', 'soldier'), {}
            self.combat_start(attackers, blocks)
            if mode != 'hidden_combat':
                self.flip_paid()
            self.combat_finish(attackers, blocks)
        else:
            self.flip_paid()
            if mode == 'spell_damage':
                from recipes import cost_from_oracle

                fee = cost_from_oracle(self.oracle['Shock'])
                self.fund(self.owner, fee)
                self.emit('choose_player', player=self.owner, value=self.foe)
                self.emit('cast', ref='shock', value=1)
                self.pay(self.owner, fee)
                self.resolve()
                self.leave('shock', 'graveyard')
                self.life[self.foe] -= 2
                self.receipts()
            if mode == 'expiry':
                self.cleanup()
                self.advance()
                self.advance()
                self.combat_start(('ally',), {})
                self.combat_finish(('ally',), {})

    def play_untap(self):
        mode = self.mode
        if mode in ('ally_flip', 'opposing_flip', 'watcher_hidden'):
            if mode == 'opposing_flip':
                self.advance()
            self.cast_secondary()
            self.tap_with_icy('secondary')
            self.flip_paid('secondary')
        elif mode == 'self_attack':
            self.advance()
            self.advance()
            self.combat_start(('source',), {})
            self.flip_paid()
            self.prop('source', 'attacking', 1)
            self.combat_finish(('source',), {})
        elif mode in ('replay', 'offturn'):
            self.tap_with_icy('source')
            self.flip_paid()
        else:
            self.flip_paid()

    def play(self):
        self.setup_cards()
        self.emit('turn', player=self.owner, value=0)
        self.emit('present_ai_dialogs', value=1)
        for player in (0, 1):
            for color in range(6):
                self.emit('set_mana', player=player, color=color, value=0)
        self.emit('refresh')
        if self.mode in ('ally_flip', 'opposing_flip'):
            original_mode = self.mode
            self.mode = 'faceup'
            self.cast_source()
            self.mode = original_mode
        elif not self.cast_source() or self.mode in ('hidden', 'faceup', 'hidden_choice'):
            return
        if self.mode == 'offturn':
            self.advance()
        if self.effect == 'untap':
            self.play_untap()
        elif self.effect == 'prevention':
            self.play_prevention()
        else:
            self.play_stats()

    def cast_spell(self, ref, target):
        if target is not None:
            return super().cast_spell(ref, target)
        from recipes import cost_from_oracle

        entity = self.entities[ref]
        actor = entity['owner']
        fee = cost_from_oracle(self.oracle[entity['name']])
        self.fund(actor, fee)
        self.emit('cast', ref=ref, value=1)
        self.emit('assert_stack', value=1)
        self.pay(actor, fee)
        entity['zone'] = 'stack'
        self.pools()


def expand(name, csvid, declaration, model, selected, rows, oracle):
    from recipes import step

    validate_witnesses(oracle, rows)
    if csvid != pins()['sources'][name]['registry']['csvid']:
        raise ValueError('unreserved Morph team source')
    cases = []
    for variant in selected:
        mode,owner = variant.rsplit('_', 1)
        scene = TeamScene(name, model, mode, int(owner), rows, oracle)
        scene.play()
        steps = scene.steps
        if scene.replay is not None:
            at = scene.replay
            suffix = steps[at:]
            steps = steps[:at] + [step('capture')] + suffix + [
                step('record_state'), step('ai_roundtrip'), step('restore')] + suffix + [step('assert_recorded_state')]
        gaps = ['Independent response priority during legacy synchronous turn-up effects', 'Multiplayer']
        metadata = dict(family=FAMILY, version=2, variant=variant, params=declaration['params'],
                        oracle_text=declaration['oracle_text'], covered_clauses=declaration['oracle_text'].splitlines(),
                        untested_components=gaps)
        cases.append(dict(name=f'MorphTurnUpTeamRecipe_{csvid}_{variant}', card=name, fixture='empty_main',
                          seed=123, setup=scene.setup, steps=steps, _recipe=metadata))
    return cases
