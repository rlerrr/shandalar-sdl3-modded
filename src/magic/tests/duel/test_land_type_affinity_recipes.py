"""Guard printed land subtypes, exact payments, and meaningful combat."""

import json
import unittest

import land_type_affinity_recipes as model
from generate_cases import registry


class LandTypeAffinityTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.oracle = json.loads((model.PROFILE_PATH.parent / 'oracle.json').read_text(encoding='utf8'))['cards']
        cls.rows = {row['name']: row for row in registry()}

    def case(self, name, mode):
        record = self.oracle[name]
        declaration = dict(family=model.FAMILY, version=2, oracle_text=record['oracle_text'],
                           params=model.parameters(record), variants=[mode])
        return model.expand(name, self.rows[name]['csvid'], declaration, model.spec(declaration, record),
                            declaration['variants'], self.rows, self.oracle)[0]

    @staticmethod
    def values(steps, op, **fields):
        return [s['value'] for s in steps if s['op'] == op and all(s.get(k) == v for k, v in fields.items())]

    def test_whole_oracle_cost_subtype_and_keywords(self):
        expected = {'Dross Golem': (5, 'Swamp', 'Fear'), 'Oxidda Golem': (6, 'Mountain', 'Haste'),
                    'Razor Golem': (6, 'Plains', 'Vigilance'), 'Spire Golem': (6, 'Island', 'Flying'),
                    'Tangle Golem': (7, 'Forest', ''), 'Gate Colossus': (8, 'Gate', '')}
        self.assertEqual(set(model.profile()['sources']), expected.keys())
        model.validate_witnesses(self.oracle, self.rows)
        for name, value in expected.items():
            record = self.oracle[name]
            params = model.parameters(record)
            self.assertEqual((params['mana'], params['land_type'], params['keyword']), value)
            with self.assertRaises(ValueError):
                model.parameters(dict(record, oracle_text=record['oracle_text'] + '\nDraw a card.'))

    def test_own_matching_lands_only_and_full_payment_contrasts(self):
        name = 'Dross Golem'
        for owner in (0, 1):
            for mode, price, success in [('full_price', 5, 1), ('short_full', 4, 0), ('own_basics', 3, 1),
                    ('short_discount', 2, 0), ('typed_nonbasic', 4, 1), ('foe_only', 5, 1), ('unrelated', 5, 1),
                    ('mixed', 3, 1), ('hand_grave_library_only', 5, 1)]:
                steps = self.case(name, f'{mode}_{owner}')['steps']
                self.assertEqual(self.values(steps, 'set_mana', player=owner, color=0)[-1], price)
                self.assertEqual(self.values(steps, 'cast', ref='source'), [success])
                self.assertEqual(self.values(steps, 'assert_mana', player=owner, color=0)[-1], 0 if success else price)
                first_cast = next(i for i, step in enumerate(steps) if step['op'] == 'cast')
                self.assertEqual({(s['player'], s['color']) for s in steps[:first_cast] if s['op'] == 'set_mana'},
                                 {(player, color) for player in (0, 1) for color in range(6)})

    def test_real_tapped_nonbasic_enters_and_real_destroy_removes_discount(self):
        for owner in (0, 1):
            case = self.case('Razor Golem', f'land_play_{owner}')
            self.assertIn(dict(name='Mistveil Plains', ref='entering', player=owner, zone='hand'), case['setup'])
            steps = case['steps']
            self.assertLess(steps.index(dict(op='play_land', ref='entering', value=1)), steps.index(dict(op='cast', ref='source', value=1)))
            self.assertEqual(self.values(steps, 'assert_card', ref='entering', property='tapped'), [1, 1])
            self.assertEqual(self.values(steps, 'set_mana', player=owner, color=0)[-1], 5)
            removed = self.case('Razor Golem', f'remove_land_{owner}')['steps']
            self.assertIn(dict(op='choose_target', player=1-owner, ref='own_0'), removed)
            self.assertIn(dict(op='cast', ref='removal', value=1), removed)
            self.assertEqual(self.values(removed, 'assert_card', ref='own_0', property='in_play'), [0, 0])
            self.assertEqual(self.values(removed, 'set_mana', player=owner, color=0)[-1], 6)

    def test_effective_subtype_and_independent_second_artifact(self):
        steps = self.case('Tangle Golem', 'type_grant_0')['steps']
        self.assertLess(steps.index(dict(op='cast', ref='omen', value=1)), steps.index(dict(op='cast', ref='source', value=1)))
        self.assertEqual(self.values(steps, 'set_mana', player=0, color=0)[-1], 5)
        repeated = self.case('Spire Golem', 'repeat_spell_1')['steps']
        self.assertEqual(self.values(repeated, 'set_mana', player=1, color=0)[-1], 10)
        self.assertEqual(self.values(repeated, 'assert_mana', player=1, color=0)[:2], [5, 0])
        floor = self.case('Tangle Golem', 'floor_surplus_0')['steps']
        self.assertEqual(self.values(floor, 'assert_mana', player=0, color=0), [2, 2])

    def test_fear_and_flying_use_actual_legal_and_illegal_blocks(self):
        for owner in (0, 1):
            for name, mode, legal in [('Dross Golem', 'fear_ground', 0), ('Dross Golem', 'fear_black', 1),
                                      ('Dross Golem', 'fear_artifact', 1), ('Spire Golem', 'flying_ground', 0),
                                      ('Spire Golem', 'flying_reach', 1)]:
                steps = self.case(name, f'{mode}_{owner}')['steps']
                self.assertEqual(self.values(steps, 'block', ref='blocker', other='source'), [legal])
                self.assertEqual(self.values(steps, 'assert_life', player=1-owner), [20 if legal else 20-model.parameters(self.oracle[name])['power']])

    def test_haste_and_vigilance_use_cast_and_actual_turns(self):
        for owner in (0, 1):
            haste = self.case('Oxidda Golem', f'fresh_attack_{owner}')['steps']
            self.assertEqual(self.values(haste, 'attack', ref='source'), [1])
            self.assertEqual(self.values(haste, 'assert_life', player=1-owner), [17])
            self.assertFalse(any(s['op'] == 'advance_turn' for s in haste))
            self.assertEqual(self.values(self.case('Tangle Golem', f'fresh_attack_{owner}')['steps'], 'attack', ref='source'), [0])
            vigilant = self.case('Razor Golem', f'vigilance_{owner}')['steps']
            self.assertEqual(self.values(vigilant, 'assert_card', ref='source', property='tapped'), [0, 0])
            self.assertEqual(self.values(vigilant, 'block', ref='source', other='other_attacker'), [1])

    def test_snapshot_preserves_paid_pending_source_and_replays_only_resolution(self):
        steps = self.case('Spire Golem', 'pending_replay_0')['steps']
        ops = [step['op'] for step in steps]
        self.assertLess(ops.index('cast'), ops.index('capture'))
        self.assertLess(ops.index('capture'), ops.index('resolve'))
        self.assertEqual(ops.count('cast'), 1)
        self.assertEqual(ops.count('resolve'), 2)
        self.assertEqual(ops[-1], 'assert_recorded_state')


    def test_gate_discount_and_exact_threshold(self):
        for owner in (0, 1):
            short = self.case('Gate Colossus', f'short_discount_{owner}')['steps']
            self.assertEqual(self.values(short, 'set_mana', player=owner, color=0)[-1], 5)
            self.assertEqual(self.values(short, 'cast', ref='source'), [0])
            entry = self.case('Gate Colossus', f'gate_play_{owner}')['steps']
            self.assertLess(entry.index(dict(op='play_land', ref='entry_land', value=1)), entry.index(dict(op='cast', ref='source', value=1)))
            self.assertEqual(self.values(entry, 'set_mana', player=owner, color=0)[-1], 7)
            for power, legal in [(2, 0), (3, 1)]:
                block = self.case('Gate Colossus', f'power_{"two" if power == 2 else "three"}_{owner}')['steps']
                self.assertEqual(self.values(block, 'block', ref='blocker', other='source'), [legal])
                self.assertEqual(self.values(block, 'assert_life', player=1-owner), [20 if legal else 12])

    def test_gate_graveyard_owns_entry_and_offers_human_decline(self):
        for owner in (0, 1):
            steps = self.case('Gate Colossus', f'grave_accept_{owner}')['steps']
            initial = steps.index(dict(op='cast', ref='source', value=1))
            shatter = steps.index(dict(op='cast', ref='shatter', value=1))
            entry = steps.index(dict(op='play_land', ref='entry_land', value=1))
            self.assertLess(initial, shatter)
            self.assertLess(shatter, entry)
            self.assertEqual(self.values(steps, 'choose_dialog', player=owner), [0] if owner == 0 else [])
            self.assertEqual(self.values(steps, 'assert_zone_card', player=owner, zone='library', index=0), [19631, 19631])
            for mode in ('grave_wrong_land', 'grave_foe_gate'):
                wrong = self.case('Gate Colossus', f'{mode}_{owner}')['steps']
                self.assertEqual(self.values(wrong, 'choose_dialog', player=owner), [])
                self.assertEqual(self.values(wrong, 'assert_zone_multiplicity', player=owner, zone='graveyard', index=19631), [1, 1, 1])
        decline = self.case('Gate Colossus', 'grave_decline_0')['steps']
        self.assertEqual(self.values(decline, 'choose_dialog', player=0), [1])
        self.assertEqual(self.values(decline, 'assert_zone_multiplicity', player=0, zone='graveyard', index=19631), [1, 1, 1])
        again = self.case('Gate Colossus', 'grave_recast_1')['steps']
        self.assertEqual(self.values(again, 'cast', ref='source'), [1, 1])
        self.assertEqual(self.values(again, 'bind_hand', player=1, ref='source'), [19631])
        self.assertEqual(self.values(again, 'set_mana', player=1, color=0)[-1], 7)


if __name__ == '__main__':
    unittest.main()
