"""Check verified Suspend menus, upkeep order, entry, and cast responses."""
import json
import unittest

from generate_cases import registry
import suspend_entry_and_sacrifice_recipes as model


class SuspendEntryAndSacrificeTests(unittest.TestCase):
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

    def test_pins_and_real_suspend_upkeeps(self):
        model.validate_witnesses(self.oracle, self.rows)
        for name, declaration in self.homes.items():
            record = self.oracle[name]
            shape = model.spec(declaration, record)
            self.assertEqual(declaration['variants'], model.variants(shape))
            with self.assertRaises(ValueError):
                model.spec(declaration, dict(record, oracle_text='Suspend'))
            for actor in (0, 1):
                duel = self.duel(name, f'countdown_{actor}')
                self.assertEqual([s['player'] for s in duel['steps'] if s['op'] == 'upkeep_step'], [1-actor, actor] * shape['duration'])
                self.assertFalse(any('Gargadon Suspended' in card['name'] for card in duel['setup']))

    def test_nantuko_two_affordable_options(self):
        for actor in (0, 1):
            for mode in ('parked', 'countdown'):
                steps = self.duel('Nantuko Shaman', f'{mode}_{actor}')['steps']
                cast = steps.index(dict(op='cast', ref='source', value=0))
                self.assertEqual(steps[cast-1], dict(op='choose_dialog', player=actor, value=1))
            normal = self.duel('Nantuko Shaman', f'normal_{actor}')['steps']
            self.assertFalse(any(step['op'] == 'choose_dialog' for step in normal))
            untapped = self.duel('Nantuko Shaman', f'normal_untapped_{actor}')['steps']
            cast = untapped.index(dict(op='cast', ref='source', value=1))
            self.assertEqual(untapped[cast-1], dict(op='choose_dialog', player=actor, value=0))
            self.assertIn(dict(op='assert_card', ref='land', property='tapped', value=0), untapped[cast+1:])
        for name in self.homes:
            wrong = self.duel(name, 'wrong_suspend_color_0')['steps']
            self.assertIn(dict(op='cast', ref='source', value=0), wrong)
            self.assertFalse(any(s['op'] in ('upkeep_step', 'choose_dialog') for s in wrong))

    def test_paid_response_targets_and_life(self):
        for name in self.homes:
            steps = self.duel(name, 'countered_normal_1')['steps']
            self.assertEqual([s['op'] for s in steps if s['op'] in ('announce_spell', 'cast_response', 'resolve_response')],
                             ['announce_spell', 'cast_response', 'resolve_response'])
        growth = self.duel('Phthisis', 'response_growth_0')['steps']
        self.assertEqual([s['player'] for s in growth if s['op'] == 'choose_target'], [0, 1])
        self.assertIn(dict(op='assert_life', player=1, value=8), growth)
        bounce = self.duel('Phthisis', 'target_bounced_0')['steps']
        self.assertIn(dict(op='assert_life', player=1, value=20), bounce)

    def test_release_replay_runs_final_upkeep_and_effect_twice(self):
        for name in self.homes:
            steps = self.duel(name, 'replay_release_1')['steps']
            self.assertEqual(sum(s == dict(op='cast', ref='source', value=0) for s in steps), 1)
            ops = [s['op'] for s in steps]
            capture, record, restore, ai, compare = map(ops.index, ('capture', 'record_state', 'restore', 'ai_roundtrip', 'assert_recorded_state'))
            self.assertLess(record, restore)
            self.assertLess(restore, ai)
            self.assertIn(dict(op='upkeep_step', player=1), steps[capture+1:record])
            self.assertEqual(steps[capture+1:record], steps[ai+1:compare])


if __name__ == '__main__':
    unittest.main()
