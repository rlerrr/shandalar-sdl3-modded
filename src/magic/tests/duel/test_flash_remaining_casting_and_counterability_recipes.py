"""Guard real counter contests and the two added Flash compositions."""

import unittest

import flash_remaining_casting_and_counterability_recipes as model


class FlashCastingGuard(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        profiles = model.profiles()
        pins = profiles['sources'] | profiles['witnesses']
        cls.oracle = {name: pin['oracle'] for name, pin in pins.items()}
        cls.rows = {name: pin['registry'] for name, pin in pins.items()}

    def game(self, csvid, mode, seat):
        name = model.PEARL_NAME if csvid == model.PEARL else model.CHROMIUM_NAME
        result = model.CreatureResponse(csvid, self.oracle[name], seat, mode, self.rows, self.oracle)
        result.build()
        return result

    def test_full_record_and_casting_cost_are_pinned(self):
        record = self.oracle[model.CHROMIUM_NAME]
        shape = model.classify(record, model.CHROMIUM)
        decl = dict(family=model.FAMILY, version=2, params=shape, oracle_text=record['oracle_text'], variants=['shift_0'])
        self.assertEqual(model.spec(decl, record)['mana'], [4, 1, 1, 0, 0, 1])
        with self.assertRaises(ValueError):
            model.spec(decl, dict(record, oracle_text=record['oracle_text'].replace('hexproof', 'shroud')))
        with self.assertRaises(ValueError):
            model.spec(dict(decl, version=2.0), record)
        with self.assertRaises(ValueError):
            model.spec(dict(decl, variants=['unreviewed_1']), record)

    def test_both_creatures_are_cast_off_turn_and_face_real_counterspell(self):
        for csvid in (model.PEARL, model.CHROMIUM):
            for seat in (0, 1):
                game = self.game(csvid, 'counter', seat)
                self.assertEqual(game.steps[0], dict(op='turn', player=1 - seat))
                steps = game.steps
                announce = steps.index(dict(op='announce_spell', ref='source'))
                counter = steps.index(dict(op='cast_response', ref='counter', value=1))
                self.assertLess(announce, counter)
                self.assertIn(dict(op='set_mana', player=1 - seat, color=2, value=2), steps[announce:counter])
                self.assertIn(dict(op='assert_stack', value=2), steps[counter:])
                self.assertEqual(game.cards['source']['zone'], 'battlefield')
                self.assertEqual(game.cards['counter']['zone'], 'graveyard')

    def test_pearl_pays_three_controlled_lands_and_returns_to_owner(self):
        for seat in (0, 1):
            game = self.game(model.PEARL, 'bounce_three', seat)
            choices = [step['ref'] for step in game.steps if step['op'] == 'choose_target']
            self.assertEqual(choices, ['land_a', 'land_b', 'land_c'])
            self.assertEqual(game.cards['land_c']['zone'], 'hand')
            self.assertEqual(game.cards['land_c']['player'], 1 - seat)
            self.assertEqual(game.cards['retained_land']['zone'], 'battlefield')
            self.assertIn(dict(op='bind_hand', ref='returned_source', player=seat, value=model.PEARL), game.steps)
            short = self.game(model.PEARL, 'short_lands', seat)
            self.assertIn(dict(op='activate', ref='source', value=0), short.steps)
            self.assertEqual(short.cards['source']['zone'], 'battlefield')

    def test_pearl_prowess_follows_own_noncreature_with_cleanup(self):
        for seat in (0, 1):
            game = self.game(model.PEARL, 'prowess', seat)
            power = [step['value'] for step in game.steps if step.get('ref') == 'source'
                     and step['op'] == 'assert_card' and step.get('property') == 'power']
            self.assertEqual(power, [6, 6, 7, 6, 6])
            actual = [step['ref'] for step in game.steps if step['op'] == 'cast' and step['ref'] != 'intro']
            self.assertEqual(actual, ['foe_spell', 'boost', 'friendly_creature'])

    def test_chromium_payment_type_targeting_and_expiry(self):
        for seat in (0, 1):
            game = self.game(model.CHROMIUM, 'shift', seat)
            self.assertEqual(game.cards['discard']['zone'], 'graveyard')
            self.assertEqual(game.cards['reserve']['zone'], 'hand')
            humans = [step['value'] for step in game.steps if step.get('property') == 'subtype_Human']
            self.assertEqual(humans, [0, 0, 1, 0])
            foes = [step['value'] for step in game.steps if step['op'] == 'probe_activation_target'
                    and step['ref'] == 'foe_pinger']
            self.assertEqual(foes, [1, 0, 1])
            self.assertEqual(game.life[1 - seat], 19)

    def test_chromium_flying_contrasts_and_empty_hand(self):
        for seat in (0, 1):
            flight = self.game(model.CHROMIUM, 'flight', seat)
            self.assertIn(dict(op='block', ref='air', other='source', value=1), flight.steps)
            self.assertEqual(flight.cards['air']['zone'], 'graveyard')
            defense = self.game(model.CHROMIUM, 'shift_defends', seat)
            self.assertIn(dict(op='block', ref='source', other='air_attacker', value=0), defense.steps)
            self.assertEqual(defense.life[seat], 18)
            empty = self.game(model.CHROMIUM, 'empty_hand', seat)
            self.assertIn(dict(op='can_activate', ref='source', value=0), empty.steps)
