"""Guard exact rules, real upkeep costs, and the retained whole-card riders."""

import unittest

import upkeep_payment_and_sacrifice_recipes as model


class UpkeepPaymentAndSacrificeTests(unittest.TestCase):
    def setUp(self):
        pins = model.profiles()
        cards = pins['sources'] | pins['witnesses']
        self.oracle = {name: pin['oracle'] for name, pin in cards.items()}
        self.rows = {name: pin['registry'] for name, pin in cards.items()}
        self.names = {row['csvid']: name for name, row in self.rows.items()}

    def board(self, csvid, variant):
        return model.trial(csvid, self.names[csvid], variant, self.rows, self.oracle)

    def test_rules_pin_the_entire_card(self):
        model.validate_witnesses(self.oracle, self.rows)
        source = self.oracle[self.names[model.ARCADES]]
        with self.assertRaises(ValueError):
            model.classify(dict(source, oracle_text=source['oracle_text'].splitlines()[0]), model.ARCADES)

    def test_conversion_uses_real_land_mana_and_two_separate_enemy_lands(self):
        trial = self.board(model.CONVERSION, 'paid_0')
        self.assertEqual([step[1] for step in trial.steps if step[0] == 'choose_mana_source'], ['fee_0', 'fee_1'])
        self.assertEqual([step[1] for step in trial.steps if step[0] == 'activate'], ['enemy_first', 'enemy_second'])
        self.assertTrue(all(trial.objects[f'fee_{i}']['name'] == model.PLAINS for i in range(2)))
        self.assertTrue(all(trial.objects[f'enemy_{ref}']['name'] == model.MOUNTAIN for ref in ('first', 'second')))
        self.assertIn(('assert_card', 'fee_1', '', 'tapped', 0, 1, 0, 0, 0), trial.steps)
        self.assertIn(('assert_zone_multiplicity', '', '', '', 1, 2, 1, self.rows[model.PLAINS]['csvid'], 0), trial.steps)

    def test_barrow_and_vultures_keep_the_top_noncreature_and_exile_the_top_creature(self):
        for csvid in (model.GHOUL, model.VULTURES):
            with self.subTest(csvid=csvid):
                trial = self.board(csvid, 'top_creature_1')
                self.assertEqual(trial.objects['fee']['zone'], 'exile')
                self.assertEqual(trial.objects['lower_creature']['zone'], 'graveyard')
                self.assertEqual(trial.objects['newer_noncreature']['zone'], 'graveyard')
                self.assertEqual(trial.objects['foe_grave']['zone'], 'graveyard')

    def test_breeding_pit_checks_prior_and_later_tokens(self):
        want = {0: 'short_1', 1: 'paid_1'}
        csvid = self.rows[model.THRULL]['csvid']
        for increment, variant in want.items():
            steps = self.board(model.PIT, variant).steps
            counts = [step[5] for step in steps if step[0] == 'assert_zone_multiplicity' and
                      step[4] == 1 and step[6] == 1 and step[7] == csvid]
            self.assertEqual(counts, [1, 1 + increment])

    def test_distinct_first_strike_rampage_and_trample_aftermaths(self):
        horror = self.board(model.HORROR, 'first_strike_1')
        self.assertIn(('assert_card', 'source', '', 'damage', 0, 0, 0, 0, 0), horror.steps)
        dragon = self.board(model.CHROMIUM, 'rampage_0')
        self.assertEqual([step[1] for step in dragon.steps if step[0] == 'block'], ['air_one', 'air_two'])
        self.assertIn(('assert_card', 'source', '', 'power', 0, 9, 0, 0, 0), dragon.steps)
        larva = self.board(model.LARVA, 'trample_0')
        self.assertEqual((larva.life[1], larva.objects['blocker']['zone']), (15, 'graveyard'))
