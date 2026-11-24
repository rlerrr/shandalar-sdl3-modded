"""Check Flash combat, activation costs, and exact damage-derived rewards."""
from copy import deepcopy
import unittest

from flash_remaining_combat_and_activated_recipes import (
    AMBUSHER, FAMILY, HIXUS, INTERCEPTOR, RANGER, SAILBACK, THIEF, TRAP, Board, classify, profiles,
    spec, validate_witnesses, variants,
)


def inputs():
    pins = profiles()
    oracle = {name: pin['oracle'] for entries in pins.values() for name, pin in entries.items()}
    rows = {name: pin['registry'] for entries in pins.values() for name, pin in entries.items()}
    return oracle, rows


def scene(csvid, owner, mode):
    oracle, rows = inputs()
    name, = [name for name in profiles()['sources'] if rows[name]['csvid'] == csvid]
    board = Board(rows[name], owner, mode, rows, oracle)
    board.run()
    return board


class FlashCombatActivationGuards(unittest.TestCase):
    def test_whole_oracle_and_typed_variants(self):
        oracle, _ = inputs()
        record = oracle['Shielded Aether Thief']
        shape = classify(record, THIEF)
        declaration = dict(family=FAMILY, version=2, params=shape, oracle_text=record['oracle_text'],
                           variants=variants(shape))
        self.assertEqual(spec(declaration, record)['csvid'], THIEF)
        for invalid in (dict(declaration, version=2.0), dict(declaration, variants=['reset_0']),
                        dict(declaration, params=dict(shape, csvid=float(THIEF)))):
            with self.assertRaises(ValueError):
                spec(invalid, record)
        with self.assertRaises(ValueError):
            spec(declaration, dict(record, oracle_text=record['oracle_text'].replace('{E}{E}{E}', '{E}{E}')))

    def test_witnesses_registry_and_resource_origin(self):
        oracle, rows = inputs()
        validate_witnesses(oracle, rows)
        modified = deepcopy(rows)
        modified['Krosan Tusker']['iid'] += 1
        with self.assertRaises(ValueError):
            validate_witnesses(oracle, modified)
        for owner in (0, 1):
            game = scene(AMBUSHER, owner, 'combat_lethal')
            deck = [row for row in game.setup if row['zone'] == 'library']
            self.assertEqual([row['player'] for row in deck], [owner] * 4)

    def test_original_additional_block_and_paid_sacrifice(self):
        for owner in (0, 1):
            defense = scene(INTERCEPTOR, owner, 'double')
            blocks = [step['value'] for step in defense.steps if step['op'] == 'block']
            self.assertEqual(blocks, [1, 1, 0])
            self.assertEqual(defense.life[owner], 19)
            trap = scene(TRAP, owner, 'filtered')
            self.assertEqual(trap.cards['source']['zone'], 'graveyard')
            self.assertEqual(trap.cards['first']['zone'], 'graveyard')
            self.assertEqual(trap.cards['second']['damage'], 2)
            self.assertEqual(trap.cards['third']['damage'], 0)

    def test_ranger_real_turn_and_hixus_linked_removal(self):
        for owner in (0, 1):
            reset = scene(RANGER, owner, 'reset')
            actions = [step['op'] for step in reset.steps]
            start = actions.index('advance_turn')
            self.assertEqual(actions[start:start+4], ['advance_turn', 'untap_step', 'upkeep_step', 'turn'])
            self.assertEqual([reset.cards[ref]['zone'] for ref in ('first_forest', 'reserve_forest')], ['hand'] * 2)
            hixus = scene(HIXUS, owner, 'release')
            self.assertEqual(hixus.cards['source']['zone'], 'graveyard')
            self.assertEqual([hixus.cards[ref]['zone'] for ref in ('first', 'second')], ['battlefield'] * 2)

    def test_ambusher_draws_from_damage_even_if_lethal(self):
        for owner in (0, 1):
            game = scene(AMBUSHER, owner, 'combat_lethal')
            self.assertEqual(game.cards['source']['zone'], 'graveyard')
            self.assertEqual([game.cards[f'library_{i}']['zone'] for i in range(4)], ['hand'] * 3 + ['library'])
            surviving = scene(AMBUSHER, owner, 'spell_survives')
            self.assertEqual(surviving.cards['source']['damage'], 2)
            self.assertEqual(surviving.stats('source'), (7, 4))
            self.assertEqual(scene(AMBUSHER, owner, 'prevented').cards['library_0']['zone'], 'library')

    def test_thief_gains_energy_across_real_blocks_then_spends_three(self):
        for owner in (0, 1):
            energy = scene(THIEF, owner, 'charge_and_draw')
            self.assertEqual(sum(step['op'] == 'block' for step in energy.steps), 3)
            self.assertEqual(sum(step['op'] == 'advance_turn' for step in energy.steps), 4)
            spending = next(i for i, step in enumerate(energy.steps) if step['op'] == 'activate')
            self.assertTrue(any(step['op'] == 'assert_energy' and step['value'] == 3 for step in energy.steps[:spending]))
            self.assertEqual(energy.energy, 0)
            self.assertEqual(energy.cards['library_0']['zone'], 'hand')
            self.assertEqual(scene(THIEF, owner, 'dies_blocking').energy, 1)
            self.assertEqual(scene(THIEF, owner, 'does_not_block').energy, 0)

    def test_sailback_real_counters_survive_cleanup_but_marked_damage_does_not(self):
        for owner in (0, 1):
            twice = scene(SAILBACK, owner, 'noncombat_twice')
            self.assertEqual(twice.stats('source'), (6, 6))
            self.assertEqual(twice.cards['source']['damage'], 4)
            later = scene(SAILBACK, owner, 'expiry')
            self.assertEqual(later.stats('source'), (5, 5))
            self.assertEqual(later.cards['source']['damage'], 0)
            self.assertEqual(scene(SAILBACK, owner, 'lethal').cards['source']['zone'], 'graveyard')
            self.assertEqual(scene(SAILBACK, owner, 'prevented').stats('source'), (4, 4))
