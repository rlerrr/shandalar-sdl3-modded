"""Check typed draw phases, pending choices and observed printed draw riders."""
import json
import unittest

from generate_cases import registry
import draw_step_discard_and_hand_size_recipes as model


class DrawStepTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        data = model.PROFILE.parent
        cls.oracle = json.loads((data / 'oracle.json').read_bytes())['cards']
        cls.rows = {row['name']: row for row in registry() if row['kind'] != 'internal'}
        cls.homes = {}
        for name, pin in model.profiles()['sources'].items():
            home, = (data / 'cases/cards').glob(f"*/{pin['registry']['csvid']:05d}-*.json")
            cls.homes[name] = json.loads(home.read_bytes())['recipes'][0]

    def steps(self, name, variant):
        declaration = self.homes[name]
        shape = model.spec(declaration, self.oracle[name])
        return model.build(name, self.rows[name]['csvid'], declaration, shape, variant, self.rows, self.oracle)['steps']

    def test_complete_pinned_primaries_and_explicit_draw_timing(self):
        model.validate_witnesses(self.oracle, self.rows)
        for name, declaration in self.homes.items():
            record = self.oracle[name]
            shape = model.spec(declaration, record)
            self.assertEqual(declaration['variants'], model.variants(shape))
            for key, value in (('oracle_text', model.OWN_DRAW), ('mana_cost', '{1}')):
                with self.assertRaises(ValueError):
                    model.spec(declaration, dict(record, **{key: value}))
            for variant in declaration['variants']:
                steps = self.steps(name, variant)
                phase = {}
                saved = None
                for item in steps:
                    op = item['op']
                    if op == 'capture':
                        saved = phase.copy()
                    elif op == 'restore':
                        phase = saved.copy()
                    elif op == 'untap_step':
                        phase[item['player']] = 'untap'
                    elif op == 'upkeep_step':
                        self.assertEqual(phase[item['player']], 'untap')
                        phase[item['player']] = 'upkeep'
                    elif op == 'draw_step':
                        self.assertEqual(phase[item['player']], 'upkeep')
                        phase[item['player']] = 'draw'

    def test_hand_zone_discard_and_ordinary_draw(self):
        for name, variant in (('Anvil of Bogardan', 'scope_1'), ('Grafted Skullcap', 'scope_0'),
                              ('Heightened Awareness', 'scope_0')):
            steps = self.steps(name, variant)
            self.assertTrue(any(step['op'] == 'choose_zone_card' for step in steps))
            self.assertIn(dict(op='cast', ref='ordinary', value=1), steps)
            self.assertTrue(all(not (step['op'] == 'choose_target' and step['ref'].startswith(('initial_', 'discard_')))
                                for step in steps))

    def test_box_uses_real_menu_and_retains_normal_draw_before_exchange(self):
        name = "Teferi's Puzzle Box"
        steps = self.steps(name, 'empty_hand_1')
        index = next(i for i, item in enumerate(steps) if item['op'] == 'draw_step')
        self.assertEqual(steps[index-1:index+1], [dict(op='choose_dialog', player=0, value=0),
                                                 dict(op='draw_step', player=0)])
        suffix = steps[index+1:]
        reach, mountain = [self.rows[name]['csvid'] for name in (model.REACH, 'Mountain')]
        self.assertIn(dict(op='assert_zone_multiplicity', player=0, zone='hand', index=reach, value=1), suffix)
        self.assertIn(dict(op='assert_zone_card', player=0, zone='library', index=4, value=mountain), suffix)

    def test_oil_discard_loss_scope_and_overbeing_hybrid_replay(self):
        for seat in (0, 1):
            for mode, loss in (('discard_loss', 1), ('discard_foe', 0)):
                steps = self.steps('Midnight Oil', f'{mode}_{seat}')
                self.assertIn(dict(op='activate', ref='looter', value=1), steps)
                self.assertEqual([item['value'] for item in steps if item['op'] == 'assert_life' and item['player'] == seat][-1], 20-loss)
            steps = self.steps('Overbeing of Myth', f'mixed_replay_{seat}')
            cast = steps.index(dict(op='cast', ref='source', value=1))
            self.assertEqual(sum(item['value'] for item in steps[cast-7:cast] if item['op'] == 'set_mana'), 5)
            power = {item['value'] for item in steps if item.get('property') == 'power'}
            self.assertTrue({2, 4} <= power)
            capture = steps.index(dict(op='capture'))
            self.assertIn(dict(op='draw_step', player=seat), steps[capture:])
            self.assertTrue(any(item['op'] == 'assert_recorded_state' for item in steps[capture:]))


if __name__ == '__main__':
    unittest.main()

