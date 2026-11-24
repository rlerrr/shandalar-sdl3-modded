"""Regression guards for pinned Madness source inputs and cost/zone coverage."""

from copy import deepcopy
import unittest

import madness_variable_and_zones_recipes as model


class MadnessVariableAndZonesTests(unittest.TestCase):
    def setUp(self):
        self.pins = model.profiles()
        self.sources = self.pins['sources']
        self.oracle = {name: pin['oracle'] for group in self.pins.values() for name, pin in group.items()}
        self.registry = {name: pin['registry'] for group in self.pins.values() for name, pin in group.items()}

    def declaration(self, record, csvid):
        shape = model.classify(record, csvid)
        return dict(family=model.FAMILY, version=2, params=shape,
                    oracle_text=record['oracle_text'], variants=model.variants(shape))

    def test_exact_source_records_and_variants(self):
        for name, pin in self.sources.items():
            record, csvid = pin['oracle'], pin['registry']['csvid']
            declaration = self.declaration(record, csvid)
            self.assertEqual(model.spec(declaration, record)['csvid'], csvid)
            changed = dict(record, oracle_text=record['oracle_text'].splitlines()[0])
            with self.assertRaises(ValueError):
                model.spec(declaration, changed)
            wrong_cost = deepcopy(declaration)
            wrong_cost['params']['madness_fixed'][0] += 1
            with self.assertRaises(ValueError):
                model.spec(wrong_cost, record)
            wrong_variants = dict(declaration, variants=['paid_ninety_0'])
            with self.assertRaises(ValueError):
                model.spec(wrong_variants, record)

    def test_impostor_zombie_rejected(self):
        self.registry[model.ZOMBIE] = dict(self.registry[model.ZOMBIE], csvid=106)
        with self.assertRaises(ValueError):
            model.validate_witnesses(self.oracle, self.registry)

    def test_alternative_fee_remains_x_plus_fixed(self):
        for name, pin in self.sources.items():
            csvid = pin['registry']['csvid']
            if csvid not in (model.TOKENS, model.CONTROL):
                continue
            shape = model.classify(pin['oracle'], csvid)
            mode, x = next((mode, x) for mode, x in model.VARIANTS[csvid] if x)
            game = model.Spell(shape, pin['oracle'], 0, mode, x, self.registry, self.oracle)
            game.build()
            activation, = [i for i, step in enumerate(game.steps) if step['op'] == 'activate']
            funding = next(step for step in reversed(game.steps[:activation])
                           if step['op'] == 'set_mana' and step['color'] == 0)
            receipt = next(step for step in game.steps[activation:] if step['op'] == 'assert_mana')
            self.assertEqual(funding['value'] - receipt['value'], x)
            self.assertEqual(receipt['value'], shape['normal'][0])

    def test_zero_damage_fall_back_retains_unchanged_receipts(self):
        name = "Avacyn's Judgment"
        record = self.sources[name]['oracle']
        game = model.Spell(model.classify(record, model.DAMAGE), record, 0, 'paid_zero', 0,
                           self.registry, self.oracle)
        game.build()
        self.assertEqual(game.life, [20, 20])
        self.assertEqual(game.cards['victim']['zone'], 'battlefield')
        self.assertIn(2, [step['value'] for step in game.steps if step['op'] == 'assert_unused_choices'])
        self.assertTrue(all(step['value'] == 0 for step in game.steps if step.get('property') == 'damage'))

    def test_prompt_sensitive_counter_controls_preserve_state(self):
        for name, pin in self.sources.items():
            csvid = pin['registry']['csvid']
            record = pin['oracle']
            for actor in (0, 1):
                game = model.Spell(model.classify(record, csvid), record, actor,
                                   'normal_countered', None, self.registry, self.oracle)
                original_library = deepcopy(game.library)
                game.build()
                ops = [step['op'] for step in game.steps]
                self.assertLess(ops.index('announce_spell'), ops.index('cast_response'))
                self.assertLess(ops.index('cast_response'), ops.index('resolve_response'))
                self.assertNotIn('choose_zone', ops)
                self.assertEqual(game.library, original_library)
                self.assertEqual(game.life, [20, 20])
                self.assertEqual(game.cards['source']['zone'], 'graveyard')
                self.assertEqual(game.cards['counter']['zone'], 'graveyard')
