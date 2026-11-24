"""Check linked entry interactions and exact action witnesses."""

import unittest

import entry_interaction_destruction_recipes as model


class DestructionInteractionTests(unittest.TestCase):
    def setUp(self):
        pins = model.profiles()
        cards = pins['sources'] | pins['witnesses']
        self.oracle = {name: item['oracle'] for name, item in cards.items()}
        self.rows = {name: item['registry'] for name, item in cards.items()}
        self.names = {row['csvid']: name for name, row in self.rows.items()}

    def case(self, csvid, variant):
        name = self.names[csvid]
        record = self.oracle[name]
        shape = model.classify(record, csvid)
        declaration = dict(family=model.FAMILY, version=2, params=shape,
                           oracle_text=record['oracle_text'], variants=[variant])
        self.assertEqual(model.spec(declaration, record), dict(shape, family=model.FAMILY))
        case, = model.expand(name, csvid, declaration, shape, [variant], self.rows, self.oracle)
        return case

    def test_complete_oracle_and_witness_guard(self):
        name = self.names[model.DEMON]
        record = self.oracle[name]
        with self.assertRaises(ValueError):
            model.classify(dict(record, oracle_text=record['oracle_text'].splitlines()[0]), model.DEMON)
        with self.assertRaises(ValueError):
            model.validate_witnesses(self.oracle, dict(self.rows, **{model.EFREET: dict(self.rows[model.EFREET], csvid=0)}))

    def test_doom_requires_paid_tap_and_sacrifice_with_two_real_choosers(self):
        steps = self.case(model.DOOM, 'marked_1')['steps']
        self.assertIn(dict(op='choose_target', ref='own', player=1), steps)
        self.assertIn(dict(op='choose_target', ref='foe', player=0), steps)
        pay = steps.index(dict(op='activate', ref='source', value=1))
        self.assertIn(dict(op='assert_mana', player=1, color=0, value=0), steps[pay:])
        self.assertIn(dict(op='assert_card', ref='source', property='in_play', value=0), steps[pay:])
        self.assertIn(dict(op='assert_card', ref='unmarked', property='in_play', value=1), steps[pay:])

    def test_land_limit_uses_ordinary_land_actions(self):
        steps = self.case(model.LIMITED, 'removed_1')['steps']
        denied = steps.index(dict(op='play_land', ref='visitor', value=0))
        allowed = steps.index(dict(op='play_land', ref='visitor', value=1))
        self.assertIn(dict(op='cast', ref='removal', value=1), steps[denied:allowed])
        trimmed = self.case(model.LIMITED, 'over_five_1')['steps']
        self.assertIn(dict(op='choose_target', ref='land_0_5', player=0), trimmed)

    def test_real_double_counters_and_opponent_grave_payment(self):
        spike = self.case(model.SPIKE, 'multiple_0')['steps']
        self.assertIn(dict(op='assert_card', ref='source', property='plus1_counters', value=4), spike)
        self.assertIn(dict(op='assert_card', ref='own', property='plus1_counters', value=2), spike)
        demon = self.case(model.DEMON, 'foe_grave_0')['steps']
        self.assertIn(dict(op='assert_energy', player=0, value=4), demon)
        paid = demon.index(dict(op='activate', ref='source', value=1))
        self.assertIn(dict(op='choose_player', player=0, value=1), demon[:paid])
        self.assertIn(dict(op='assert_energy', player=0, value=0), demon[paid:])
        self.assertIn(dict(op='assert_card', ref='returned', property='tapped', value=1), demon[paid:])

    def test_real_single_point_target_and_multicolor_blocking_are_preserved(self):
        steps = self.case(model.PROWLER, 'decline_0')['steps']
        self.assertIn(dict(op='choose_optional_target', ref='victim', player=0), steps)
        steps = self.case(model.MARE, 'colored_block_1')['steps']
        self.assertIn(dict(op='block', ref='multicolor', other='source', value=0), steps)
        self.assertIn(dict(op='block', ref='legal_blocker', other='source', value=1), steps)
