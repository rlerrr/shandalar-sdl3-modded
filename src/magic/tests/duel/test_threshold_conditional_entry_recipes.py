"""Focused semantic guards for Threshold entry/upkeep and real cast-trigger controls."""
import copy
import unittest

import threshold_conditional_entry_recipes as model


class ThresholdEntryTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        pins = model.profile()
        records = pins['sources'] | pins['witnesses']
        cls.sources = pins['sources']
        cls.oracle = {name: row['oracle'] for name, row in records.items()}
        cls.rows = {name: row['registry'] for name, row in records.items()}

    def case(self, name, variant):
        printed = self.oracle[name]
        declaration = dict(family=model.FAMILY, version=2, variants=[variant],
                           oracle_text=printed['oracle_text'], params=model.parameters(printed))
        shape = model.spec(declaration, printed)
        case, = model.expand(name, self.rows[name]['csvid'], declaration, shape, [variant], self.rows, self.oracle)
        return case

    def test_exact_oracle_and_registry(self):
        model.validate_witnesses(self.oracle, self.rows)
        for name in self.sources:
            record = copy.deepcopy(self.oracle[name])
            record['oracle_text'] = record['oracle_text'].replace('seven or more', 'six or more')
            with self.subTest(name=name), self.assertRaises(ValueError):
                model.parameters(record)

    def test_chieftain_checks_unevolved_raptor_and_legal_combat(self):
        for seat in (0, 1):
            case = self.case('Centaur Chieftain', f'seven_{seat}')
            refs = [card['ref'] for card in case['setup']]
            self.assertLess(refs.index('guard'), refs.index('blocker'))
            steps = case['steps']
            fresh = steps.index(dict(op='attack', ref='source', value=1))
            self.assertIn(dict(op='assert_card', ref='blocker', property='toughness', value=1), steps[:fresh])
            self.assertIn(dict(op='assert_counter', ref='blocker', counter_kind='plus1', value=0), steps[:fresh])
            self.assertIn(dict(op='assert_life', player=1-seat, value=17), steps[fresh:])

    def test_paid_crossing_precedes_source_and_adds_exact_seventh_card(self):
        for name in self.sources:
            actions = self.case(name, 'crossing_0')['steps']
            prepay = actions.index(dict(op='cast', ref='disenchant', value=1))
            source = actions.index(dict(op='cast', ref='source', value=1))
            self.assertIn(dict(op='assert_zone', player=0, zone='graveyard', value=6), actions[:prepay])
            self.assertIn(dict(op='assert_zone', player=0, zone='graveyard', value=7), actions[prepay:source])
            self.assertNotIn(dict(op='assert_zone_multiplicity', player=0, zone='graveyard',
                                  index=self.rows["Traveler's Amulet"]['csvid'], value=1), actions[prepay:source])

    def test_grounds_replay_and_wrong_color(self):
        actions = self.case('Hunting Grounds', 'replay_0')['steps']
        capture = actions.index(dict(op='capture'))
        record = actions.index(dict(op='record_state'))
        again = actions.index(dict(op='ai_roundtrip'))
        self.assertEqual(actions[capture + 1:record], actions[again + 1:-1])
        pending = actions.index(dict(op='announce_spell', ref='foe_spell'))
        self.assertEqual(actions[pending - 1], dict(op='choose_zone_card', player=0, value=self.rows['Bottle Gnomes']['csvid']))
        wrong = self.case('Hunting Grounds', 'wrong_color_0')['steps']
        self.assertEqual([a for a in wrong if a['op'] == 'cast'], [dict(op='cast', ref='source', value=0)])


if __name__ == '__main__':
    unittest.main()
