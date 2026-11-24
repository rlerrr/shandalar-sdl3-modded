"""Verify Oracle rules, actor mirrors, and real alternate cast receipts."""

import copy
import unittest

import alternate_zone_cast_additional_riders_recipes as recipe

BETRAYAL = 'Mnemonic Betrayal'
DAXOS = 'Daxos of Meletis'
PILLAGER = 'Dream Pillager'
SUNBIRD = "Sunbird's Invocation"


class AlternateCastTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        pins = recipe.profile()
        cls.sources = pins['sources']
        records = pins['sources'] | pins['witnesses']
        cls.oracle = {name: record['oracle'] for name, record in records.items()}
        cls.rows = {name: record['registry'] for name, record in records.items()}

    def case(self, name, variant):
        record = self.oracle[name]
        params = recipe.parameters(record)
        declaration = dict(family=recipe.FAMILY, version=2, oracle_text=record['oracle_text'],
                           params=params, variants=[variant])
        shape = recipe.spec(declaration, record)
        return recipe.expand(name, self.rows[name]['csvid'], declaration, shape, [variant], self.rows, self.oracle)[0]

    def test_complete_oracle_rules(self):
        for name in self.sources:
            record = copy.deepcopy(self.oracle[name])
            record['oracle_text'] = record['oracle_text'].split('\n')[0] + ' '
            with self.assertRaises(ValueError):
                recipe.parameters(record)

    def test_witness_handler_drift(self):
        rows = copy.deepcopy(self.rows)
        rows[recipe.DRAW]['handler'] = 'card_dummy'
        with self.assertRaises(ValueError):
            recipe.validate_witnesses(self.oracle, rows)

    def test_mnemonic_real_exile_use_and_return(self):
        for actor in (0, 1):
            steps = self.case(BETRAYAL, f'alternate_pay_{actor}')['steps']
            opponent = 1 - actor
            advance = next(i for i, s in enumerate(steps) if s['op'] == 'advance_turn')
            play = [s for s in steps[:advance] if s['op'] == 'play_exiled' and s['value']]
            self.assertEqual(play, [dict(op='play_exiled', player=actor, owner=opponent,
                                         index=self.rows[recipe.DRAW]['csvid'], value=1)])
            self.assertTrue(any(s.get('op') == 'assert_zone_multiplicity' and s.get('zone') == 'graveyard'
                                and s.get('player') == opponent and s.get('index') == self.rows['Forest']['csvid']
                                and s.get('value') == 1 for s in steps[advance:]))

    def test_daxos_block_threshold_and_actual_cast(self):
        for actor in (0, 1):
            for mode, legal in (('big_block', 0), ('legal_block', 1)):
                steps = self.case(DAXOS, f'{mode}_{actor}')['steps']
                cast = next(i for i, s in enumerate(steps) if s['op'] == 'cast')
                blocks = [i for i, s in enumerate(steps) if s['op'] == 'block' and s['value'] == legal]
                self.assertEqual(len(blocks), 1)
                self.assertEqual(sum(s['op'] == 'advance_turn' for s in steps[cast:blocks[0]]), 2)
            high = self.case(DAXOS, f'higher_{actor}')['steps']
            self.assertIn(dict(op='assert_life', player=actor, value=24), high)

    def test_pillager_two_paid_casts_and_land_denial(self):
        for actor in (0, 1):
            plays = [s for s in self.case(PILLAGER, f'two_spells_{actor}')['steps']
                     if s['op'] == 'play_exiled' and s['value']]
            self.assertEqual([(s['player'], s['owner'], s['index']) for s in plays],
                             [(actor, actor, self.rows[n]['csvid']) for n in (recipe.DRAW, 'Mind Stone')])
            land_steps = self.case(PILLAGER, f'land_{actor}')['steps']
            self.assertIn(dict(op='play_exiled', player=actor, owner=actor,
                               index=self.rows['Forest']['csvid'], value=0), land_steps)

    def test_pillager_exile_notice_only_on_actual_nonempty_hit(self):
        for actor in (0, 1):
            for mode, expects_notice in (('exact_pay', True), ('short_library', True),
                    ('flying_ground', True), ('replay', True), ('reach_block', False), ('empty', False)):
                with self.subTest(actor=actor, mode=mode):
                    steps = self.case(PILLAGER, f'{mode}_{actor}')['steps']
                    damage = [i for i, step in enumerate(steps) if step['op'] == 'combat_damage']
                    self.assertEqual(len(damage), 2 if mode == 'replay' else 1)
                    notices = [i for i, step in enumerate(steps) if step['op'] == 'choose_zone']
                    self.assertEqual(notices, [i - 1 for i in damage] if expects_notice else [])
                    for notice in notices:
                        self.assertEqual(steps[notice], dict(op='choose_zone', player=actor, value=-1))

    def test_sunbird_reveal_then_selector_then_free_equal_cost(self):
        for actor in (0, 1):
            steps = self.case(SUNBIRD, f'equal_{actor}')['steps']
            at = [s['op'] for s in steps]
            announce = at.index('announce_spell')
            resolve = next(i for i, s in enumerate(steps) if i > announce and s['op'] == 'resolve')
            self.assertEqual(steps[announce-3:announce], [dict(op='choose_zone', player=actor, value=-1),
                dict(op='observe_search_filters', value=1),
                dict(op='choose_zone_card', player=actor, value=self.rows['Grizzly Bears']['csvid'])])
            self.assertIn(dict(op='assert_zone_multiplicity', player=actor, zone='battlefield',
                               index=self.rows['Grizzly Bears']['csvid'], value=1), steps[announce:resolve])
            self.assertIn(dict(op='assert_stack', value=1), steps[announce:resolve])
            self.assertTrue(all(s['value'] == 0 for s in steps[announce:resolve] if s['op'] == 'assert_mana'))

    def test_sunbird_decline_keeps_only_prefix_order_guarantees(self):
        for actor in (0, 1):
            steps = self.case(SUNBIRD, f'decline_{actor}')['steps']
            self.assertEqual(sum(s['op'] == 'choose_zone' and s['value'] == -1 for s in steps), 2)
            ordered = [s for s in steps if s['op'] == 'assert_zone_card' and s['player'] == actor]
            actual = {(s['index'], s['value']) for s in ordered}
            self.assertEqual(actual, {(i, self.rows[name]['csvid']) for i, name in enumerate(recipe.DECK)})


if __name__ == '__main__':
    unittest.main()
