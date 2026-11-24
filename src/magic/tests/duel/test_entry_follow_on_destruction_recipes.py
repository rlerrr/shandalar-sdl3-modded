"""Guard whole Oracle records and independent destructive entry branches."""

from copy import deepcopy
import unittest

import entry_follow_on_destruction_recipes as model


class EntryFollowOnDestructionTests(unittest.TestCase):
    def setUp(self):
        pins = model.profiles()
        records = pins['sources'] | pins['witnesses'] | pins['tokens']
        self.oracle = {name: item['oracle'] for name, item in records.items() if item['oracle'] is not None}
        self.rows = {name: item['registry'] for name, item in records.items()}

    def selection(self, name, variant):
        row = self.rows[name]
        record = self.oracle[name]
        shape = model.classify(record, row['csvid'])
        declaration = dict(family=model.FAMILY, version=2, params=shape,
                           oracle_text=record['oracle_text'], variants=[variant])
        return row, record, shape, declaration

    def case(self, name, variant):
        row, _, shape, declaration = self.selection(name, variant)
        case, = model.expand(name, row['csvid'], declaration, shape, declaration['variants'], self.rows, self.oracle)
        return case

    def test_exact_whole_source_witness_and_token_guards(self):
        _, record, shape, declaration = self.selection('Spine of Ish Sah', 'owner_return_0')
        self.assertEqual(model.spec(declaration, record), dict(shape, family=model.FAMILY))
        with self.assertRaises(ValueError):
            model.spec(declaration, dict(record, oracle_text=record['oracle_text'].splitlines()[0]))
        with self.assertRaises(ValueError):
            model.spec(dict(declaration, version=2.0), record)
        bad = deepcopy(self.rows)
        bad['Zombie']['csvid'] = 18822
        with self.assertRaises(ValueError):
            model.validate_witnesses(self.oracle, bad)

    def test_cast_gate_controls_use_real_reanimation(self):
        for name in ('Deathbringer Regent', 'Dread Cacodemon', 'Reiver Demon'):
            case = self.case(name, 'reanimated_0')
            self.assertIn(dict(ref='source', player=0, name=name, zone='graveyard'), case['setup'])
            self.assertIn(dict(op='cast', ref='revival', value=1), case['steps'])
            self.assertIn(dict(op='choose_zone_card', player=0, value=self.rows[name]['csvid']), case['steps'])
            self.assertNotIn(dict(op='cast', ref='source', value=1), case['steps'])

    def test_real_zombie_and_elephant_receipts_distinguish_controller_and_eligibility(self):
        token = self.case('Overseer of the Damned', 'opponent_token_1')['steps']
        self.assertIn(dict(op='cast', ref='raise', value=1), token)
        self.assertIn(dict(op='assert_zone_multiplicity', player=1, zone='battlefield', index=18860, value=0), token)
        foe_death = self.case('Overseer of the Damned', 'foe_creature_1')['steps']
        self.assertIn(dict(op='assert_matching_permanents', player=1, index=18860, property='tapped', value=1), foe_death)
        elephant = self.case('Terastodon', 'one_each_0')['steps']
        for seat in (0, 1):
            self.assertIn(dict(op='assert_zone_multiplicity', player=seat, zone='battlefield', index=18729, value=1), elephant)
        self.assertIn(dict(op='choose_done_target', player=0), elephant)

    def test_grave_threshold_and_opponent_payment_are_observed_at_real_boundaries(self):
        for mode, count in (('five_upkeep_0', 5), ('six_upkeep_0', 6)):
            case = self.case('Shadowborn Demon', mode)
            self.assertEqual(sum(card['ref'].startswith('grave_') and card['name'] == model.HOST for card in case['setup']), count)
            upkeep = case['steps'].index(dict(op='upkeep_step', player=0))
            action = dict(op='assert_card', ref='sacrifice', property='in_play', value=0)
            self.assertEqual(action in case['steps'][upkeep:], count < 6)
        paid = self.case('Rishadan Brigand', 'paid_0')['steps']
        gate = paid.index(dict(op='cast', ref='source', value=1))
        self.assertIn(dict(op='assert_mana', player=1, color=0, value=3), paid[gate:])
        self.assertIn(dict(op='assert_mana', player=1, color=0, value=0), paid[gate:])
