"""Focused checks for natural age changes and independent land control."""

from copy import deepcopy
import unittest

from cumulative_upkeep_alternative_and_snow_recipes import (
    CoverOfWinter, FAMILY, HeraldOfLeshrac, classify, profiles, spec, variants,
)


class CumulativeAlternativeTests(unittest.TestCase):
    def setUp(self):
        pins = profiles()['sources'] | profiles()['witnesses']
        self.rows = {n: pin['registry'] for n, pin in pins.items()}
        self.oracle = {n: pin['oracle'] for n, pin in pins.items()}

    def new_board(self, world, name, mode):
        game = world(self.rows[name]['csvid'], self.oracle[name], 0, mode, self.rows, self.oracle)
        game.build()
        return game

    def test_cover_records_only_real_snow_payments_before_combat(self):
        world = self.new_board(CoverOfWinter, 'Cover of Winter', 'activated_prevent')
        self.assertEqual([s['ref'] for s in world.steps if s['op'] == 'choose_target'], ['land', 'later'])
        self.assertEqual([s['value'] for s in world.steps if s['op'] == 'assert_counter'][-1], 2)
        self.assertEqual(world.life, [20, 20])
        self.assertTrue(world.cards['land']['tapped'] and world.cards['later']['tapped'])

    def test_herald_returns_only_land_its_controller_holds_for_other_owner(self):
        world = self.new_board(HeraldOfLeshrac, 'Herald of Leshrac', 'separate_theft')
        self.assertEqual(world.age, 0)
        self.assertEqual(world.cards['owned']['player'], 1)
        self.assertEqual(world.cards['foreign_0']['player'], 1)
        source_power = [s['value'] for s in world.steps if s.get('ref') == 'source' and s.get('property') == 'power']
        self.assertEqual(source_power, [2, 3])

    def test_snow_cost_is_pinned_independently_of_printed_casting_budget(self):
        record = self.oracle['Glacial Plating']
        model = classify(record, 8374)
        entry = dict(family=FAMILY, version=2, params=model, oracle_text=record['oracle_text'],
                     variants=variants(model))
        self.assertEqual(spec(entry, record)['fee'], '{S}')
        changed = deepcopy(entry)
        changed['params']['fee'] = '{W}'
        with self.assertRaises(ValueError):
            spec(changed, record)
