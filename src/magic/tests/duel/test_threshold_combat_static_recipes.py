"""Guards for exact Threshold composition and actual phase inputs."""

import unittest

import threshold_combat_static_recipes as model


class ThresholdCombatGuard(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        pins = model.profiles()
        entries = pins['sources'] | pins['witnesses']
        cls.oracle = {name: entry['oracle'] for name, entry in entries.items()}
        cls.rows = {name: entry['registry'] for name, entry in entries.items()}
        cls.names = {entry['csvid']: name for name, entry in cls.rows.items()}

    def model_case(self, csvid, actor, mode):
        record = self.oracle[self.names[csvid]]
        game = model.Duel(csvid, record, actor, mode, self.rows, self.oracle)
        game.build()
        return game

    def test_complete_oracle_and_no_duplicate_variants(self):
        for csvid in model.CARDS:
            record = self.oracle[self.names[csvid]]
            shape = model.classify(record, csvid)
            declaration = dict(family=model.FAMILY, version=2, params=shape, oracle_text=record['oracle_text'],
                               variants=['six_0', 'seven_1'])
            self.assertEqual(model.spec(declaration, record)['mana'], shape['mana'])
            with self.assertRaises(ValueError):
                model.classify({**record, 'oracle_text': record['oracle_text'] + '\nDraw a card.'}, csvid)
            with self.assertRaises(ValueError):
                model.spec({**declaration, 'variants': ['seven_1', 'seven_1']}, record)

    def test_live_threshold_loss_permits_real_block_again(self):
        game = self.model_case(5570, 1, 'lose_threshold')
        cast = next(i for i, s in enumerate(game.steps) if s['op'] == 'activate' and s.get('ref') == 'crypt')
        block = next(i for i, s in enumerate(game.steps) if s['op'] == 'block' and s.get('ref') == 'source')
        self.assertEqual(game.steps[block]['value'], 1)
        self.assertTrue(any(s['op'] == 'resolve' for s in game.steps[cast:block]))
        self.assertEqual(sum(e['name'] == 'Mountain' and e['zone'] == 'exile' for e in game.cards.values()), 7)

    def test_wayward_live_crossing_precedes_actual_upkeeps(self):
        for actor in (0, 1):
            game = self.model_case(5751, actor, 'upkeep_crossed')
            cast = next(i for i, s in enumerate(game.steps) if s['op'] == 'cast' and s.get('ref') == 'disenchant')
            phases = [(i, s['player']) for i, s in enumerate(game.steps) if s['op'] == 'upkeep_step']
            self.assertEqual([seat for _, seat in phases], [1 - actor, actor])
            self.assertTrue(any(s['op'] == 'resolve' for s in game.steps[cast:phases[0][0]]))
            self.assertEqual(game.cards['source']['zone'], 'graveyard')
