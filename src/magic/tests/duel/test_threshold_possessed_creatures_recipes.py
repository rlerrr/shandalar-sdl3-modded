"""Focused guards for the Possessed Threshold cases."""
import json
import unittest

from generate_cases import registry
import threshold_possessed_creatures_recipes as model


class PossessedThresholdTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        data = model.PROFILE.parent
        cls.oracle = json.loads((data / 'oracle.json').read_bytes())['cards']
        cls.rows = {row['name']: row for row in registry()}
        cls.homes = {}
        for name, pin in model.profiles()['sources'].items():
            home, = (data / 'cases/cards').glob(f"*/{pin['registry']['csvid']:05d}-*.json")
            cls.homes[name] = json.loads(home.read_bytes())['recipes'][0]

    def duel(self, name, variant):
        declaration = self.homes[name]
        shape = model.spec(declaration, self.oracle[name])
        return model.build(name, self.rows[name]['csvid'], declaration, shape, variant, self.rows, self.oracle)

    def test_full_pins_and_duplicate_counts(self):
        model.validate_witnesses(self.oracle, self.rows)
        for name, declaration in self.homes.items():
            record = self.oracle[name]
            self.assertEqual(declaration['variants'], model.variants(model.spec(declaration, record)))
            with self.assertRaises(ValueError):
                model.spec(declaration, dict(record, oracle_text='Threshold'))
            for mode, expected in (('six', [0, 6]), ('seven', [0, 7]), ('opponent_only', [7, 6])):
                setup = self.duel(name, mode + '_1')['setup']
                grave = [card for card in setup if card['zone'] == 'graveyard']
                self.assertEqual({card['name'] for card in grave}, {'Mountain'})
                self.assertEqual([sum(card['player'] == player for card in grave) for player in (0, 1)], expected)

    def test_actual_threshold_gain_and_loss(self):
        for name in self.homes:
            base_color = model.PRIMARIES[self.rows[name]['csvid']][3]
            for mode, states in (('gain_threshold', [base_color, base_color, 2]),
                                 ('lose_threshold', [2, 2, base_color])):
                steps = self.duel(name, mode + '_1')['steps']
                self.assertEqual([s['value'] for s in steps if s.get('ref') == 'source' and s.get('property') == 'color'], states)
            gained = self.duel(name, 'gain_threshold_1')['steps']
            self.assertLess(gained.index(dict(op='cast', ref='disenchant', value=1)),
                            gained.index(dict(op='activate', ref='source', value=1)))
            lost = self.duel(name, 'lose_threshold_1')['steps']
            self.assertLess(lost.index(dict(op='activate', ref='crypt', value=1)),
                            lost.index(dict(op='activate', ref='source', value=0)))

    def test_pending_activation_and_real_counter(self):
        for name in self.homes:
            steps = self.duel(name, 'pending_loss_0')['steps']
            ability = steps.index(dict(op='activate', ref='source', value=1))
            crypt = steps.index(dict(op='activate', ref='crypt', value=1))
            self.assertLess(ability, crypt)
            self.assertIn(dict(op='assert_stack', value=1), steps[ability:crypt])
            resolve = next(index for index in range(crypt+1, len(steps)) if steps[index]['op'] == 'resolve')
            self.assertIn(dict(op='assert_stack', value=2), steps[crypt:resolve])
            self.assertIn(dict(op='can_activate', ref='source', value=0), steps[resolve:])
            countered = self.duel(name, 'countered_1')['steps']
            self.assertEqual([s['op'] for s in countered if s['op'] in ('announce_spell', 'cast_response', 'resolve_response')],
                             ['announce_spell', 'cast_response', 'resolve_response'])

    def test_exact_generic_shortage_and_activation_replay(self):
        for name in self.homes:
            short = self.duel(name, 'short_generic_1')['steps']
            self.assertIn(dict(op='activate', ref='source', value=0), short)
            self.assertFalse(any(s['op'] == 'choose_target' for s in short))
            self.assertIn(dict(op='assert_mana', player=1, color=0, value=1), short)
            self.assertIn(dict(op='assert_mana', player=1, color=1, value=1), short)
            steps = self.duel(name, 'replay_0')['steps']
            self.assertEqual(sum(s == dict(op='cast', ref='source', value=1) for s in steps), 1)
            ops = [s['op'] for s in steps]
            capture, record, restore, ai, compare = map(ops.index, ('capture', 'record_state', 'restore', 'ai_roundtrip', 'assert_recorded_state'))
            self.assertLess(record, restore)
            self.assertLess(restore, ai)
            self.assertEqual(steps[capture+1:record], steps[ai+1:compare])


if __name__ == '__main__':
    unittest.main()
