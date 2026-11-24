"""Each-end-step triggers driven by actual life and death history."""

import copy
import hashlib
import json
from pathlib import Path

from combat_value_trigger_recipes import _mirror
from own_end_step_state_trigger_recipes import EndStepBoard

FAMILY = 'each_end_step_history_trigger'
PROFILE_PATH = Path(__file__).with_name('data') / 'each_end_step_history_trigger_wave8_profiles.json'
PREFIX = 'At the beginning of each end step, '
CLAUSES = {
    PREFIX + 'if you gained 4 or more life this turn, create a 4/4 white Angel creature token with flying.': ('angel', False),
    'Flying\n' + PREFIX + 'if you gained life this turn, create a 1/1 black Bat creature token with flying.': ('bat', True),
    'Morbid — ' + PREFIX + 'if a creature died this turn, you may draw a card.': ('draw', False),
    'Flying\n' + PREFIX + 'put a +1/+1 counter on Asmira for each creature put into your graveyard from the battlefield this turn.': ('counters', True),
    'Flying\nMorbid — ' + PREFIX + 'if a creature died this turn, destroy target non-Demon creature.': ('destroy', True),
    PREFIX + 'each opponent loses life equal to the life they lost this turn. (Damage causes loss of life.)': ('life_loss', False),
}
TOKENS = {'angel': ('Angel', 18687, 4, 4, 32), 'bat': ('Bat', 18693, 1, 1, 2)}
MODES = {
    'angel': ['gain_one', 'gain_three', 'gain_five', 'split_gain', 'opponent_gained', 'gain_then_lose', 'token_flight'],
    'bat': ['gain_one', 'gain_five', 'split_gain', 'opponent_gained', 'gain_then_lose', 'token_flight'],
    'draw': ['enemy_death', 'two_deaths', 'exile_only', 'bounce_only', 'artifact_death'],
    'counters': ['enemy_death', 'two_deaths', 'mixed_deaths', 'own_grave_other_controller', 'other_grave_own_controller',
                 'exile_only', 'bounce_only', 'artifact_death'],
    'destroy': ['enemy_death', 'target_own', 'demon_only', 'indestructible', 'exile_only', 'bounce_only', 'artifact_death'],
    'life_loss': ['damage_two', 'pure_loss', 'own_loss', 'loss_then_gain', 'mixed_loss', 'two_reflections'],
}


def profiles():
    return json.loads(PROFILE_PATH.read_text(encoding='utf8'))


def parameters(record):
    clause = CLAUSES.get(record.get('oracle_text'))
    if not clause or record.get('layout') != 'normal' or record.get('selected_face') or record.get('card_faces'):
        raise ValueError('one complete reviewed each-end-step history clause required')
    role, flying = clause
    keywords = ['Flying'] if flying else []
    if role in ('draw', 'destroy'):
        keywords.append('Morbid')
    if record.get('keywords') != keywords or (('Creature' in record.get('type_line', '')) != flying):
        raise ValueError('pinned each-end-step source characteristics required')
    if flying and any(not record.get(key, '').isascii() or not record[key].isdecimal() for key in ('power', 'toughness')):
        raise ValueError('flying sources require fixed stats')
    return dict(role=role, printed_mana=record['mana_cost'], flying=flying)


def variants(params):
    role = params['role']
    modes = ['cast_payment', 'cast_unaffordable', 'cast_wrong_color', 'cast_countered', 'no_history', 'own_history',
             'opponent_history', 'before_end', 'removed_before_end', 'end_snapshot']
    if role not in ('draw', 'destroy'):
        # The bounded bridge omits production init_turn(), which resets the
        # host Morbid history. Those two sources retain their same-turn tests.
        modes.append('next_turn_reset')
    modes += MODES[role]
    if params['flying']:
        modes += ['flying_ground', 'flying_reach']
    result = [f'{mode}_{owner}' for owner in (0, 1) for mode in modes]
    if role == 'draw':
        result += ['decline_draw_0', 'decline_draw_on_opponent_0']
    return result


def validate_variants(params, selected):
    if (not isinstance(selected, list) or not selected or any(type(v) is not str for v in selected)
            or len(set(selected)) != len(selected) or not set(selected) <= set(variants(params))):
        raise ValueError('distinct supported each-end-step history variants required')


def spec(declaration, record):
    params = parameters(record)
    pin = profiles()['sources'].get(record['name'])
    if (declaration.get('family') != FAMILY or type(declaration.get('version')) is not int or declaration['version'] != 2
            or not pin or pin['oracle'] != record or pin['params'] != params or declaration.get('oracle_text') != record['oracle_text']
            or json.dumps(declaration.get('params'), sort_keys=True) != json.dumps(params, sort_keys=True)):
        raise ValueError('complete pinned Oracle and typed each-end-step parameters required')
    validate_variants(params, declaration.get('variants'))
    return dict(params, family=FAMILY)


def validate_witnesses(oracle, rows):
    data = profiles()
    root = Path(__file__).resolve().parents[4]
    catalog = json.loads((PROFILE_PATH.parent / 'unimplemented.json').read_text(encoding='utf8'))
    unimplemented = {row['csvid'] for row in catalog['cases'].values()}
    static_only = set(data['native_characteristics_only_witnesses'])
    for name, pin in data['witnesses'].items():
        row = rows.get(name, {})
        if oracle.get(name) != pin['oracle'] or row != pin['registry']:
            raise ValueError('each-end-step witness changed: ' + name)
        if name not in static_only and (row['kind'] != 'card' or row['csvid'] in unimplemented or 'dummy' in row['handler']):
            raise ValueError('each-end-step action witness must be implemented: ' + name)
    for role, (name, csvid, power, toughness, color) in TOKENS.items():
        if (data['token_outputs'][role] != [name, csvid, power, toughness, color]
                or rows[name]['csvid'] != csvid or oracle[name]['layout'] != 'token'):
            raise ValueError('each-end-step token identity changed: ' + name)
    for name, pin in data['sources'].items():
        row = rows.get(name, {})
        source = pin['compiled_source']
        if (oracle.get(name) != pin['oracle'] or row != pin['registry'] or row.get('kind') != 'card' or row.get('csvid') in unimplemented
                or 'dummy' in row['handler'] or hashlib.sha256((root / source['path']).read_bytes()).hexdigest() != source['sha256']):
            raise ValueError('implemented each-end-step source changed: ' + name)


class HistoryBoard(EndStepBoard):
    """Reuse the bounded phase/payment machinery with an independent history ledger."""

    def __init__(self, name, csvid, declaration, params, variant, rows, oracle):
        self.history_actions = []
        self.gained, self.lost, self.dead = [0, 0], [0, 0], [0, 0]
        self.created = []
        mode, owner = variant.rsplit('_', 1)
        # Only the reused constructor's initial turn flag depends on this alias.
        remote = mode in ('opponent_history', 'decline_draw_on_opponent')
        start = 'opponent_end_step_' + owner if remote else variant
        self.history_mode = mode
        super().__init__(name, csvid, declaration, params, start, rows, oracle)
        self.mode = mode
        if params['role'] in ('draw', 'counters', 'destroy'):
            self.emit('reset_death_history')

    def feature_board(self, role):
        mode = self.history_mode
        if role == 'draw':
            # The real AI accepts optional draws with a comfortably stocked
            # library. Avoid asserting strategy over a nearly empty library.
            for player in (0, 1):
                extra = list(self.zones[player]['library'])
                current = len(extra)
                for index, name in enumerate(extra, current):
                    self.setup.append(self.card(name, f'extended_library_{player}_{index}', player, 'library'))
                self.zones[player]['library'].extend(extra)
        if mode in ('flying_ground', 'flying_reach', 'token_flight'):
            blocker = 'Giant Spider' if mode == 'flying_reach' else 'Grizzly Bears'
            self.add(blocker, 'blocker', 1)
        if mode in ('flying_ground', 'flying_reach'):
            return
        if mode == 'no_history':
            if role == 'destroy':
                self.add('Centaur Courser', 'target', 1)
            return
        if mode == 'removed_before_end':
            self.add('Unsummon' if self.p['flying'] else 'Disenchant', 'remove_history_source', 0, 'hand')
        if role in TOKENS:
            self.gain_setup(mode)
        elif role == 'life_loss':
            self.loss_setup(mode)
        else:
            self.death_setup(mode, role)

    def witness_spell(self, name, kind, target, amount=0):
        ref = f'history_spell_{len(self.history_actions)}'
        # The caster remains the source owner. Every off-turn history witness
        # is an instant. The two sorcery routes are confined to their main phase.
        self.add(name, ref, 0, 'hand')
        self.history_actions.append((ref, kind, target, amount))

    def gain_setup(self, mode):
        if mode in ('gain_one', 'gain_three', 'split_gain'):
            amount = {'gain_one': 1, 'gain_three': 3, 'split_gain': 2}[mode]
            names = {1: 'Elvish Mystic', 2: 'Grizzly Bears', 3: 'Centaur Courser'}
            for index in range(2 if mode == 'split_gain' else 1):
                target = f'gain_creature_{index}'
                self.add(names[amount], target)
                self.witness_spell('Swords to Plowshares', 'exile_gain', target, amount)
        elif mode == 'gain_five':
            self.witness_spell("Whitesun's Passage", 'gain', None, 5)
        else:
            self.witness_spell('Life Burst', 'gain', 1 if mode == 'opponent_gained' else 0, 4)
        if mode == 'gain_then_lose':
            self.witness_spell('Lightning Bolt', 'loss', 0, 3)

    def loss_setup(self, mode):
        if mode == 'pure_loss':
            self.witness_spell('Bump in the Night', 'loss', 1, 3)
        elif mode == 'damage_two':
            self.witness_spell('Shock', 'loss', 1, 2)
        elif mode == 'mixed_loss':
            self.witness_spell('Shock', 'loss', 1, 2)
            self.witness_spell('Bump in the Night', 'loss', 1, 3)
        else:
            self.witness_spell('Lightning Bolt', 'loss', 0 if mode == 'own_loss' else 1, 3)
        if mode == 'loss_then_gain':
            self.witness_spell('Life Burst', 'gain', 1, 4)
        if mode == 'two_reflections':
            self.add(self.name, 'second_source')

    def death_setup(self, mode, role):
        if mode == 'artifact_death':
            self.add('Glasses of Urza', 'history_victim')
            self.witness_spell('Disenchant', 'noncreature_death', 'history_victim')
        else:
            controller = int(mode in ('enemy_death', 'own_grave_other_controller'))
            stolen = mode in ('own_grave_other_controller', 'other_grave_own_controller')
            self.add('Grizzly Bears', 'history_victim', controller, state=['opponent_owned'] if stolen else None)
            if stolen:
                self.entities['history_victim']['owner'] = 1 - controller
            spell, action = {'exile_only': ('Swords to Plowshares', 'exile_gain'),
                             'bounce_only': ('Unsummon', 'bounce')}.get(mode, ('Doom Blade', 'death'))
            self.witness_spell(spell, action, 'history_victim', 2 if action == 'exile_gain' else 0)
            if mode in ('two_deaths', 'mixed_deaths'):
                self.add('Grizzly Bears', 'second_victim', int(mode == 'mixed_deaths'))
                self.witness_spell('Doom Blade', 'death', 'second_victim')
        if role == 'destroy':
            self.add('Lord of the Pit', 'demon', 1)
            if mode != 'demon_only':
                target = 'Darksteel Myr' if mode == 'indestructible' else 'Centaur Courser'
                self.add(target, 'target', int(mode != 'target_own'))
            if mode == 'next_turn_reset':
                self.add('Grizzly Bears', 'later_target', 1)

    def record_action(self, ref, kind, target, amount):
        # Bump in the Night automatically targets the sole opponent in a duel.
        if type(target) is int and self.entities[ref]['name'] != 'Bump in the Night':
            self.emit('choose_player', player=0, value=target)
        self.paid(ref, target if isinstance(target, str) else None)
        if kind == 'gain':
            player = 0 if target is None else target
            self.life[player] += amount
            self.gained[player] += amount
        elif kind == 'loss':
            self.life[target] -= amount
            self.lost[target] += amount
        elif kind == 'exile_gain':
            player = self.entities[target]['player']
            self.move(target, 'exile')
            self.life[player] += amount
            self.gained[player] += amount
        elif kind == 'bounce':
            self.move(target, 'hand')
        else:
            if kind == 'death':
                self.dead[self.entities[target]['owner']] += 1
            self.move(target, 'graveyard')

    def mint_token(self, role):
        name, _, power, toughness, _ = TOKENS[role]
        ref = f'made_token_{len(self.created)}'
        self.created.append(ref)
        # The generic ledger checks characteristics. The receipt below also
        # asserts actual token identity and color through the engine observer.
        self.entities[ref] = dict(name=name, player=0, owner=0, zone='battlefield', creature=True,
                                  power=power, toughness=toughness, counters=0, boost=0, damage=0, tapped=False, token=False)

    def receipts(self):
        if self.created:
            role = self.p['role']
            name, csvid, _, _, color = TOKENS[role]
            subtype = 'subtype_' + name
            for index, ref in enumerate(self.created):
                if ref not in self.bound:
                    self.emit('bind_permanent_copy', player=0, value=csvid, index=index, ref=ref)
                    self.bound.add(ref)
            for prop, value in [('token', 1), ('color', color), (subtype, 1)]:
                self.emit('assert_matching_permanents', player=0, index=csvid, property=prop, value=value)
        super().receipts()

    def expect_end_step(self):
        if self.entities['source']['zone'] != 'battlefield':
            return
        role = self.p['role']
        if role in TOKENS:
            if self.gained[0] >= (4 if role == 'angel' else 1):
                self.mint_token(role)
        elif role == 'counters':
            self.entities['source']['counters'] += self.dead[0]
        elif role == 'draw' and sum(self.dead) and not self.mode.startswith('decline_draw'):
            if self.owner == 0:
                self.emit('choose_trigger_source', player=0, ref='source')
            self.draw(0, 1)
        elif role == 'destroy' and sum(self.dead) and self.entities.get('target', {}).get('zone') == 'battlefield':
            self.emit('choose_target', player=0, ref='target')
            if self.mode != 'indestructible':
                self.dead[self.entities['target']['owner']] += 1
                self.move('target', 'graveyard')
        elif role == 'life_loss':
            amount = self.lost[1]
            self.life[1] -= amount
            if self.mode == 'two_reflections':
                self.life[1] -= 2 * amount

    def real_next_turn(self):
        self.advance_and_untap()
        self.gained, self.lost, self.dead = [0, 0], [0, 0], [0, 0]

    def flying_combat(self, ref, reach=False):
        self.emit('begin_attackers')
        self.emit('attack', ref=ref, value=1)
        self.entities[ref]['tapped'] = True
        self.emit('finish_attackers')
        self.emit('begin_blockers')
        self.emit('block', ref='blocker', other=ref, value=int(reach))
        self.emit('advance_combat')
        self.emit('assert_card', ref=ref, property='blocked', value=int(reach))
        self.receipts()
        if reach:
            out, incoming = self.power(ref), self.power('blocker')
            for target, damage in [(ref, incoming), ('blocker', out)]:
                self.entities[target]['damage'] += damage
                if self.entities[target]['damage'] >= self.entities[target]['toughness']:
                    self.move(target, 'graveyard')
        else:
            self.life[1] -= self.power(ref)
        self.emit('combat_damage')
        self.emit('resolve')
        self.emit('end_combat')

    def run(self):
        mode = self.mode
        if mode in ('cast_wrong_color', 'cast_unaffordable'):
            self.emit('cast', ref='source', value=0)
            self.emit('assert_card', ref='source', property='in_hand', value=1)
        elif mode == 'cast_countered':
            self.cast_countered()
        elif mode == 'cast_payment':
            self.paid('source')
            if self.p['flying']:
                self.emit('assert_card', ref='source', property='sick', value=1)
        elif mode in ('flying_ground', 'flying_reach'):
            self.flying_combat('source', mode == 'flying_reach')
        else:
            for action in self.history_actions:
                self.record_action(*action)
            if mode == 'removed_before_end':
                self.paid('remove_history_source', 'source')
                self.move('source', 'hand' if self.p['flying'] else 'graveyard')
            self.receipts()
            if mode == 'token_flight':
                self.real_next_turn()
                self.real_next_turn()
                if self.p['role'] == 'angel':
                    # Untap leaves the bounded phase at untap. Continue to
                    # main phase on the already established same active turn.
                    self.emit('turn', player=self.player, value=0)
                self.flying_combat(self.created[0])
            elif mode == 'next_turn_reset':
                self.real_next_turn()
                self.finish_step()
            elif mode != 'before_end':
                cut = len(self.steps)
                self.finish_step()
                if mode == 'end_snapshot':
                    self.steps = replay_from(self.step, self.steps, cut)
        self.receipts()
        case = dict(name=f'EachEndStepHistoryTriggerRecipe_{self.csvid}_{mode}_{self.owner}', card=self.name,
                    fixture='empty_main', seed=123, setup=self.setup, steps=self.steps,
                    _recipe=dict(family=FAMILY, version=2, variant=f'{mode}_{self.owner}',
                                 oracle_text=self.d['oracle_text'], params=copy.deepcopy(self.d['params']), untested_components=[
                                     'The existing end-turn operation exposes the completed end trigger. Separate priority during that trigger is outside these fixtures.',
                                     'Optional draw decline is a human control. AI acceptance uses a stocked library. Generated token Flying is exercised by actual later combat.',
                                 ]))
        if self.owner:
            _mirror(case)
        return case


def replay_from(step, steps, cut):
    replay = steps[cut:]
    return (steps[:cut] + [step('capture')] + replay + [step('record_state'), step('ai_roundtrip'), step('restore')]
            + copy.deepcopy(replay) + [step('assert_recorded_state')])


def expand(name, csvid, declaration, params, selected, rows, oracle):
    validate_witnesses(oracle, rows)
    validate_variants(params, selected)
    return [HistoryBoard(name, csvid, declaration, params, variant, rows, oracle).run() for variant in selected]
