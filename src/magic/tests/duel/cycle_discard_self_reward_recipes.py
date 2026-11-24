"""Whole-Oracle temporary self rewards for cycling or discarding."""

import json
import re
from collections import Counter
from pathlib import Path

from cast_untap_trigger_recipes import exact, pin

FAMILY = 'cycle_discard_self_reward'
DATA = Path(__file__).parent / 'data/cycle_discard_self_reward_wave8_profiles.json'
MENACE = 'Menace (This creature can\'t be blocked except by two or more creatures.)'
INDESTRUCTIBLE = 'gains indestructible until end of turn. (Damage and effects that say "destroy" don\'t destroy it. If its toughness is 0 or less, it still dies.)'
TRIGGER = re.compile(r'Whenever you cycle or discard (a|another) card, this creature (.*)')
PUMP = re.compile(r'gets ([+-]\d+)/([+-]\d+) until end of turn( and can\'t be blocked this turn)?\.')
CYCLE = re.compile(r'Cycling (\{[0-9BUGRW]\}) \(\1, Discard this card: Draw a card\.\)')
LIBRARY = ('Grizzly Bears', 'Mountain', 'Island', 'Forest')
WITNESSES = set(LIBRARY) | {'Expunge', 'Tireless Tribe', 'Ornithopter', 'Reach Through Mists', 'Grasp of Darkness', 'Plains'}


def pins():
    return json.loads(DATA.read_text(encoding='utf8'))


def classify(record):
    from recipes import cost_from_oracle

    types = record.get('type_line', '').partition(' — ')[0].split()
    if (record.get('layout') != 'normal' or record.get('card_faces') or record.get('selected_face')
            or 'Creature' not in types or 'Token' in types):
        raise ValueError('normal whole creature required')
    lines = record.get('oracle_text', '').splitlines()
    matches = [(line, TRIGGER.fullmatch(line)) for line in lines]
    triggers = [(line, match) for line, match in matches if match]
    if len(triggers) != 1 or len(set(lines)) != len(lines):
        raise ValueError('one complete own cycling/discard trigger required')
    trigger, match = triggers[0]
    effect = match[2]
    pump = PUMP.fullmatch(effect)
    if not pump and effect != INDESTRUCTIBLE:
        raise ValueError('unreviewed self reward')
    menace_lines = set(lines) & {'Menace', MENACE}
    if len(menace_lines) > 1:
        raise ValueError('one Menace clause required')
    menace = bool(menace_lines)
    cycling_lines = [(line, CYCLE.fullmatch(line)) for line in lines if line.startswith('Cycling')]
    if len(cycling_lines) > 1 or (cycling_lines and not cycling_lines[0][1]):
        raise ValueError('unreviewed cycling cost or reminder')
    cycling = cycling_lines[0][1][1] if cycling_lines else None
    expected = {trigger}
    keywords = set()
    if menace:
        expected.update(menace_lines)
        keywords.add('Menace')
    if cycling:
        expected.add(cycling_lines[0][0])
        keywords.add('Cycling')
    if set(lines) != expected or set(record.get('keywords', [])) != keywords:
        raise ValueError('unmodeled whole Oracle clause')
    if any(not re.fullmatch(r'\d+', record.get(key, '')) for key in ('power', 'toughness')) or int(record['toughness']) <= 0:
        raise ValueError('positive fixed creature toughness required')
    return dict(mana=cost_from_oracle(record), power=int(record['power']), toughness=int(record['toughness']),
                delta_power=int(pump[1]) if pump else 0, delta_toughness=int(pump[2]) if pump else 0,
                unblockable=bool(pump and pump[3]), indestructible=effect == INDESTRUCTIBLE,
                menace=menace, other_only=match[1] == 'another',
                cycling_mana=cost_from_oracle({'mana_cost': cycling}) if cycling else None)


def variants(params):
    modes = ['own_cycle', 'own_discard', 'opponent_cycle', 'opponent_discard', 'cycle_cancel',
             'cycle_unaffordable', 'repeated_cycle', 'cycle_then_discard', 'draw_only', 'expiry',
             'source_cast', 'source_cast_denied', 'offturn_cycle', 'pending_replay']
    if params['menace']:
        modes += ['menace_single', 'menace_pair']
    if params['unblockable']:
        modes += ['unblockable', 'no_reward_block', 'expired_block']
    if params['indestructible']:
        modes += ['survives_destroy', 'base_destroy', 'expired_destroy', 'zero_toughness']
    if params['cycling_mana']:
        modes += ['self_cycle', 'self_cycle_denied', 'second_copy_cycles']
    return [f'{mode}_{who}' for who in (0, 1) for mode in modes]


def spec(declaration, record):
    if (declaration.get('family') != FAMILY or type(declaration.get('version')) is not int
            or declaration['version'] != 2 or declaration.get('oracle_text') != record.get('oracle_text')
            or not exact(pins()['sources'].get(record['name']), pin(record))):
        raise ValueError('whole source Oracle pin and exact recipe version required')
    params = classify(record)
    if not exact(declaration.get('params'), params):
        raise ValueError('parameters must derive from complete Oracle clauses')
    selected = declaration.get('variants')
    if (not isinstance(selected, list) or not selected or any(not isinstance(v, str) for v in selected)
            or len(set(selected)) != len(selected) or set(selected) - set(variants(params))):
        raise ValueError('distinct calibrated variants required')
    return dict(params, family=FAMILY)


def validate_witnesses(oracle, rows):
    data = pins()
    if set(data['witnesses']) != WITNESSES:
        raise ValueError('exact witness set required')
    records = dict(data['witnesses'], **data['sources'])
    if set(records) != set(data['handlers']):
        raise ValueError('exact handler set required')
    for name, expected in records.items():
        row = rows[name]
        if not exact(pin(oracle[name]), expected) or row['handler'] != data['handlers'][name]:
            raise ValueError(f'Oracle or handler changed for {name}')
        if row['handler'] in ('0', 'NULL') or (name in data['sources'] and (row['kind'] != 'card' or row['handler'] == 'card_dummy')):
            raise ValueError(f'implemented primary and legal fixture witness required for {name}')


class RewardCase:
    def __init__(self, name, csvid, declaration, params, variant, rows, oracle):
        from recipes import step

        self.name, self.csvid, self.declaration, self.params = name, csvid, declaration, params
        self.variant, self.rows, self.oracle = variant, rows, oracle
        self.mode, who = variant.rsplit('_', 1)
        self.owner = int(who)
        self.actor = 1 - self.owner if self.mode.startswith('opponent_') else self.owner
        active = 1 - self.owner if self.mode == 'offturn_cycle' else self.actor
        self.setup, self.steps, self.live = [], [], {}
        self.zones = {zone: [[], []] for zone in ('hand', 'battlefield', 'graveyard', 'library', 'exile')}
        self.mana = [[0] * 6, [0] * 6]
        self.life = [20, 20]
        self.bonuses = 0
        self.outlet_bonus = 0
        self.pending_cut = None
        for player in (0, 1):
            for index, card in enumerate(LIBRARY):
                self.seed(card, f'lib_{player}_{index}', player, 'library')
            self.seed('Plains', f'grave_{player}', player, 'graveyard')
            self.seed('Forest', f'exile_{player}', player, 'exile')
        self.seed('Grizzly Bears', 'ally', self.owner)
        self.source_in_hand = self.mode in ('source_cast', 'source_cast_denied', 'self_cycle', 'self_cycle_denied')
        self.seed(name, 'source', self.owner, 'hand' if self.source_in_hand else 'battlefield')
        discard_modes = ('own_discard', 'opponent_discard', 'cycle_then_discard')
        self.discard_needed = self.mode in discard_modes
        if self.discard_needed:
            self.seed('Tireless Tribe', 'outlet', self.actor)
            if self.mode != 'cycle_then_discard':
                self.seed('Grizzly Bears', 'discarded', self.actor, 'hand')
        cycle_modes = ('own_cycle', 'opponent_cycle', 'cycle_cancel', 'cycle_unaffordable', 'repeated_cycle',
                       'cycle_then_discard', 'expiry', 'offturn_cycle', 'pending_replay', 'unblockable', 'expired_block',
                       'survives_destroy', 'expired_destroy', 'zero_toughness', 'menace_single', 'menace_pair')
        self.cycles = 2 if self.mode == 'repeated_cycle' else 1 if self.mode in cycle_modes else 0
        if self.cycles:
            self.seed('Expunge', 'cycler', self.actor, 'hand')
            if self.cycles == 2:
                self.seed('Expunge', 'cycler_second', self.actor, 'hand')
            self.mana[self.actor][0] = 0 if self.mode == 'cycle_unaffordable' else 2 * self.cycles
        if self.mode in ('self_cycle', 'second_copy_cycles'):
            self.mana[self.owner] = list(params['cycling_mana'])
        if self.mode == 'second_copy_cycles':
            self.seed(name, 'source_copy', self.owner, 'hand')
        if self.mode == 'source_cast':
            self.mana[self.owner] = list(params['mana'])
        if self.mode == 'source_cast_denied':
            self.mana[self.owner] = list(params['mana'])
            color = next(color for color, amount in enumerate(self.mana[self.owner]) if amount)
            self.mana[self.owner][color] -= 1
        if self.mode == 'draw_only':
            self.seed('Reach Through Mists', 'draw_spell', self.owner, 'hand')
            self.mana[self.owner][2] = 1
        if self.mode == 'zero_toughness':
            self.seed('Grasp of Darkness', 'reduction', 1 - self.owner, 'hand')
            self.mana[1 - self.owner][1] = 2
        combat_modes = ('unblockable', 'no_reward_block', 'expired_block', 'menace_single', 'menace_pair')
        if self.mode in combat_modes:
            self.seed('Ornithopter', 'blocker', 1 - self.owner)
            if self.mode == 'menace_pair':
                self.seed('Ornithopter', 'second_blocker', 1 - self.owner)
        self.names = sorted({entry['name'] for entry in self.setup})
        self.before = [step('turn', player=active, value=0)]
        self.before += [step('set_mana', player=player, color=color, value=amount)
                        for player in (0, 1) for color, amount in enumerate(self.mana[player])]
        self.before.append(step('refresh'))

    def seed(self, name, ref, player, zone='battlefield'):
        from recipes import card

        entry = card(name, ref, player, zone)
        if zone == 'battlefield':
            entry['state'] = ['untapped']
            self.live[ref] = True
        self.setup.append(entry)
        self.zones[zone][player].append(name)

    def emit(self, op, **kwargs):
        from recipes import step

        self.steps.append(step(op, **kwargs))

    def property(self, ref, key, value):
        self.emit('assert_card', ref=ref, property=key, value=value)

    def move(self, name, player, source, destination):
        self.zones[source][player].remove(name)
        self.zones[destination][player].append(name)

    def draw(self, player):
        self.move(self.zones['library'][player][0], player, 'library', 'hand')

    def spend(self, owner, cost):
        self.mana[owner] = [available - amount for available, amount in zip(self.mana[owner], cost)]

    def receipt(self, stack=0, check_reward=True):
        self.emit('assert_stack', value=stack)
        self.emit('assert_choices_consumed')
        for player in (0, 1):
            self.emit('assert_life', player=player, value=self.life[player])
            for color, amount in enumerate(self.mana[player]):
                self.emit('assert_mana', player=player, color=color, value=amount)
            for zone, by_player in self.zones.items():
                contents = by_player[player]
                self.emit('assert_zone', player=player, zone=zone, value=len(contents))
                counts = Counter(contents)
                for name in self.names:
                    self.emit('assert_zone_multiplicity', player=player, zone=zone, index=self.rows[name]['csvid'], value=counts[name])
                if zone == 'library':
                    for index, name in enumerate(contents):
                        self.emit('assert_zone_card', player=player, zone=zone, index=index, value=self.rows[name]['csvid'])
        self.property('ally', 'power', 2)
        self.property('ally', 'toughness', 2)
        if check_reward and self.live.get('source'):
            self.property('source', 'power', self.params['power'] + self.bonuses * self.params['delta_power'])
            self.property('source', 'toughness', self.params['toughness'] + self.bonuses * self.params['delta_toughness'])
        if 'outlet' in self.live and check_reward:
            self.property('outlet', 'power', 1)
            self.property('outlet', 'toughness', 1 + self.outlet_bonus)

    def cycle(self, ref, name, player, cost, cancel=False, denied=False):
        if denied:
            self.emit('can_activate_hand', ref=ref, value=0)
            self.emit('activate_hand', ref=ref, value=0)
            return
        self.emit('require_can_activate_hand', ref=ref, value=1)
        self.emit('choose_dialog', player=player, value=0 if cancel else 1)
        self.emit('activate_hand', ref=ref, value=int(not cancel))
        self.emit('assert_unused_choices', value=0)
        if cancel:
            return
        self.spend(player, cost)
        self.move(name, player, 'hand', 'graveyard')
        self.receipt(stack=1, check_reward=False)
        if self.mode == 'pending_replay':
            self.pending_cut = len(self.steps)
        self.emit('resolve')
        self.emit('refresh')
        self.draw(player)
        self.bonuses += int(player == self.owner and self.live.get('source', False))
        self.receipt()

    def discard(self):
        self.emit('require_can_activate', ref='outlet', value=1)
        if self.actor == 0:
            self.emit('choose_zone_card', player=self.actor, value=self.rows['Grizzly Bears']['csvid'])
        self.emit('activate', ref='outlet', value=1)
        self.emit('assert_unused_choices', value=0)
        self.move('Grizzly Bears', self.actor, 'hand', 'graveyard')
        self.receipt(stack=1, check_reward=False)
        self.emit('resolve')
        self.emit('refresh')
        self.outlet_bonus += 4
        self.bonuses += int(self.actor == self.owner)
        self.receipt()

    def cast(self, ref, name, player, cost, destination, target=None):
        if target:
            self.emit('choose_target', player=player, ref=target)
        self.emit('cast', ref=ref, value=1)
        self.emit('assert_unused_choices', value=0)
        self.emit('require_card', ref=ref, property='in_hand', value=0)
        self.emit('assert_stack', value=1)
        self.emit('resolve')
        self.emit('refresh')
        self.spend(player, cost)
        self.move(name, player, 'hand', destination)

    def clean(self):
        self.emit('cleanup')
        self.emit('refresh')
        self.bonuses = self.outlet_bonus = 0

    def next_own_main(self):
        for player in (1 - self.owner, self.owner):
            self.emit('advance_turn')
            self.emit('untap_step', player=player)
        self.emit('turn', player=self.owner, value=0)
        self.emit('refresh')
        self.bonuses = self.outlet_bonus = 0

    def source_dies(self):
        self.move(self.name, self.owner, 'battlefield', 'graveyard')
        self.live['source'] = False

    def combat(self):
        self.emit('begin_attackers')
        self.emit('attack', ref='source', value=1)
        self.emit('finish_attackers')
        self.emit('begin_blockers')
        can_block = self.mode not in ('unblockable', 'menace_single')
        self.emit('block', ref='blocker', other='source', value=int(can_block))
        if self.mode == 'menace_pair':
            self.emit('block', ref='second_blocker', other='source', value=1)
        self.emit('advance_combat')
        self.property('source', 'blocked', int(can_block))
        if self.mode == 'menace_pair':
            for ref in ('blocker', 'second_blocker'):
                self.property(ref, 'blocking_count', 1)
            for ref in ('source', 'blocker', 'second_blocker'):
                self.emit('remove_from_combat', ref=ref)
            self.clean()
            return
        self.emit('combat_damage')
        if not can_block:
            self.life[1 - self.owner] -= self.params['power'] + self.bonuses * self.params['delta_power']
        self.emit('end_combat')

    def case(self):
        from recipes import step

        for index in range(self.cycles):
            self.cycle('cycler' if index == 0 else 'cycler_second', 'Expunge', self.actor, [2, 0, 0, 0, 0, 0],
                       cancel=self.mode == 'cycle_cancel', denied=self.mode == 'cycle_unaffordable')
        if self.discard_needed:
            self.discard()
        if self.mode in ('self_cycle', 'self_cycle_denied', 'second_copy_cycles'):
            ref = 'source_copy' if self.mode == 'second_copy_cycles' else 'source'
            self.cycle(ref, self.name, self.owner, self.params['cycling_mana'], denied=self.mode == 'self_cycle_denied')
        if self.mode == 'source_cast':
            self.cast('source', self.name, self.owner, self.params['mana'], 'battlefield')
            self.live['source'] = True
        elif self.mode == 'source_cast_denied':
            self.emit('cast', ref='source', value=0)
        elif self.mode == 'draw_only':
            self.cast('draw_spell', 'Reach Through Mists', self.owner, [0, 0, 1, 0, 0, 0], 'graveyard')
            self.draw(self.owner)
        if self.mode == 'expired_block':
            self.next_own_main()
        elif self.mode in ('expiry', 'expired_destroy'):
            self.clean()
        if self.mode in ('menace_single', 'menace_pair', 'unblockable', 'no_reward_block', 'expired_block'):
            self.combat()
        if self.mode in ('survives_destroy', 'base_destroy', 'expired_destroy'):
            self.emit('destroy', ref='source')
            self.emit('refresh')
            if self.mode != 'survives_destroy':
                self.source_dies()
        elif self.mode == 'zero_toughness':
            self.cast('reduction', 'Grasp of Darkness', 1 - self.owner, [0, 2, 0, 0, 0, 0], 'graveyard', target='source')
            self.source_dies()
        self.receipt()
        cut = self.pending_cut if self.pending_cut is not None else 0
        replay = self.steps[cut:]
        sequence = self.before + self.steps[:cut] + [step('capture')] + replay
        sequence += [step('record_state'), step('restore')] + replay + [step('assert_recorded_state')]
        return dict(name=f'CycleDiscardSelfRewardRecipe_{self.csvid}_{self.variant}', card=self.name, fixture='empty_main',
                    seed=123, setup=self.setup, steps=sequence,
                    _recipe=dict(family=FAMILY, version=2, variant=self.variant, params=self.declaration['params'],
                                 oracle_text=self.declaration['oracle_text'], untested_components=[
                                     'No separate priority window inside the hand-activation callback is asserted.',
                                     'Menace pair checks both declared blockers then removes them before damage to avoid unrelated division decisions.',
                                 ]))


def expand(name, csvid, declaration, params, selected, rows, oracle):
    validate_witnesses(oracle, rows)
    if not selected or len(set(selected)) != len(selected) or set(selected) - set(variants(params)):
        raise ValueError('unsupported cycling/discard reward variants')
    return [RewardCase(name, csvid, declaration, params, variant, rows, oracle).case() for variant in selected]
