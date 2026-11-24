"""Focused guards for Delirium grade and real threshold transitions."""

import unittest

import delirium_combat_keywords_recipes as model


class DeliriumCombatGuard(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        pins = model.profiles()
        entries = pins['sources'] | pins['witnesses']
        cls.oracle = {name: pin['oracle'] for name, pin in entries.items()}
        cls.rows = {name: pin['registry'] for name, pin in entries.items()}
        cls.names = {row['csvid']: name for name, row in cls.rows.items()}

    def test_exact_whole_oracle_and_variant_declaration(self):
        for csvid in model.SHAPES:
            record = self.oracle[self.names[csvid]]
            shape = model.classify(record, csvid)
            declaration = dict(family=model.FAMILY, version=2, params=shape, oracle_text=record['oracle_text'],
                               variants=['three_duplicates_0', 'mixed_type_1'])
            self.assertEqual(model.spec(declaration, record)['mana'], shape['mana'])
            with self.assertRaises(ValueError):
                model.classify({**record, 'oracle_text': record['oracle_text'] + '\nDraw a card.'}, csvid)
            with self.assertRaises(ValueError):
                model.spec({**declaration, 'variants': ['mixed_type_1', 'mixed_type_1']}, record)

    def test_equal_size_graveyards_and_mixed_type_witness(self):
        three = model.types_for(model.THREE, self.oracle)
        four = model.types_for(model.FOUR, self.oracle)
        mixed = model.types_for(model.MIXED, self.oracle)
        self.assertEqual((len(model.THREE), len(model.FOUR), len(model.MIXED)), (4, 4, 3))
        self.assertEqual(four - three, {'Artifact'})
        self.assertEqual(four, mixed)

    def test_live_transition_is_resolved_before_real_combat(self):
        csvid = 15864
        name = self.names[csvid]
        for mode, ref, payoff, tapped in (('replay', 'disenchant', 'cast', 0), ('lose_fourth', 'crypt', 'activate', 1)):
            duel = model.Duel(name, csvid, 0, mode, self.rows, self.oracle)
            duel.build()
            steps = duel.steps
            selected = next(i for i, step in enumerate(steps) if step['op'] == payoff and step.get('ref') == ref)
            damage = next(i for i, step in enumerate(steps) if step['op'] == 'combat_damage')
            self.assertTrue(any(step['op'] == 'resolve' for step in steps[selected:damage]))
            self.assertTrue(any(step['op'] == 'assert_card' and step['property'] == 'tapped'
                                and step['value'] == tapped for step in steps[selected:damage]))
