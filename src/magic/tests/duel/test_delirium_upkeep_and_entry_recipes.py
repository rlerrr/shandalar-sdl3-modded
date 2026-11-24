"""Guard the Oracle records, paid Delirium crossing and real upkeep replay."""
import copy
import unittest

import delirium_upkeep_and_entry_recipes as model


class DeliriumUpkeepTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        pins = model.profile()
        combined = pins['sources'] | pins['witnesses']
        cls.sources = pins['sources']
        cls.oracle = {name: row['oracle'] for name, row in combined.items()}
        cls.rows = {name: row['registry'] for name, row in combined.items()}

    def case(self, name, variant):
        printed = self.oracle[name]
        declaration = dict(family=model.FAMILY, version=2, oracle_text=printed['oracle_text'],
                           variants=[variant], params=model.parameters(printed))
        shape = model.spec(declaration, printed)
        case, = model.expand(name, self.rows[name]['csvid'], declaration, shape, [variant], self.rows, self.oracle)
        return case

    def test_printed_rules_and_witnesses(self):
        model.validate_witnesses(self.oracle, self.rows)
        for name in self.sources:
            changed = copy.deepcopy(self.oracle[name])
            changed['oracle_text'] = changed['oracle_text'].replace('four or more', 'three or more')
            with self.subTest(name=name), self.assertRaises(ValueError):
                model.parameters(changed)

    def test_crossing_uses_actual_fourth_type_and_real_upkeeps(self):
        steps = self.case('Obsessive Skinner', 'crossing_1')['steps']
        use = next(i for i, action in enumerate(steps) if action.get('ref') == 'disenchant')
        self.assertEqual(steps[use - 1], dict(op='choose_target', ref='amulet', player=1))
        self.assertEqual([action['player'] for action in steps[:use] if action['op'] == 'upkeep_step'], [0, 1])
        self.assertEqual([action['player'] for action in steps[use:] if action['op'] == 'upkeep_step'], [0, 1])
        self.assertIn(dict(op='assert_zone_multiplicity', player=1, zone='graveyard',
                           index=self.rows["Traveler's Amulet"]['csvid'], value=1), steps[use:])
        self.assertEqual([action['value'] for action in steps[:use] if action['op'] == 'assert_counter'
                          and action['ref'] == 'upkeep_target'], [0] * 5)
        self.assertIn(dict(op='assert_counter', ref='upkeep_target', counter_kind='plus1', value=1), steps[use:])

    def test_mindwrack_mills_to_threshold_from_same_grave(self):
        lower = self.case('Mindwrack Demon', 'three_types_0')
        upper = self.case('Mindwrack Demon', 'four_types_0')
        self.assertEqual([card for card in lower['setup'] if card['zone'] == 'graveyard'],
                         [card for card in upper['setup'] if card['zone'] == 'graveyard'])
        [low], [high] = ([card for card in case['setup'] if card['ref'] == 'd_0_3'] for case in (lower, upper))
        self.assertEqual((low['name'], high['name']), ('Mountain', "Traveler's Amulet"))
        for case, life in ((lower, 16), (upper, 20)):
            values = [a['value'] for a in case['steps'] if a['op'] == 'assert_life' and a['player'] == 0]
            self.assertEqual(values[-1], life)

    def test_wrong_color_and_snapshot_replay_guards(self):
        wrong = self.case('Topplegeist', 'wrong_color_0')['steps']
        self.assertEqual([a for a in wrong if a['op'] == 'cast'], [dict(op='cast', ref='source', value=0)])
        self.assertFalse(any(a['op'] in ('upkeep_step', 'choose_target') for a in wrong))
        replay = self.case('Tooth Collector', 'replay_0')['steps']
        checkpoints = {a['op']: i for i, a in enumerate(replay) if a['op'] in
                       ('capture', 'record_state', 'ai_roundtrip', 'assert_recorded_state')}
        start, record, ai, finish = (checkpoints[op] for op in
                                      ('capture', 'record_state', 'ai_roundtrip', 'assert_recorded_state'))
        self.assertEqual(replay[start + 1:record], replay[ai + 1:finish])


if __name__ == '__main__':
    unittest.main()
