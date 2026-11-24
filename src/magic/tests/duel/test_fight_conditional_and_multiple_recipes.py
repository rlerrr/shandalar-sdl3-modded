"""Guard the independent Fight fixture receipts and exact source records."""

from copy import deepcopy
import unittest

import fight_conditional_and_multiple_recipes as model


class ConditionalAndMultipleFightTests(unittest.TestCase):
    def game(self, csvid, seat, mode):
        pins = [p for group in ('sources', 'witnesses') for p in model.pins()[group].values()]
        rows = {pin['registry']['name']: pin['registry'] for pin in pins}
        oracle = {pin['registry']['name']: pin['oracle'] for pin in pins}
        source, = [r for r in rows.values() if r['csvid'] == csvid]
        game = model.Game(source, seat, mode, rows, oracle)
        game.play()
        return game

    def test_whole_oracle_source_is_required(self):
        for pin in model.pins()['sources'].values():
            oracle, csvid = pin['oracle'], pin['registry']['csvid']
            self.assertEqual(model.classify(oracle, csvid)['csvid'], csvid)
            changed = deepcopy(oracle)
            changed['oracle_text'] += ' '
            with self.assertRaises(ValueError):
                model.classify(changed, csvid)

    def test_real_tribute_payee_and_cast_path(self):
        for seat in (0, 1):
            game = self.game(model.RAVAGER, seat, 'tribute_paid')
            self.assertIn(dict(op='cast', ref='source', value=1), game.steps)
            self.assertNotIn(dict(op='require_can_cast', ref='source', value=1), game.steps)
            self.assertIn(dict(op='choose_dialog', player=1 - seat, value=0), game.steps)
            self.assertIn(dict(op='assert_card', ref='source', property='plus1_counters', value=6), game.steps)

    def test_declining_guild_selection_mills_every_revealed_card(self):
        for seat in (0, 1):
            game = self.game(model.FEUD, seat, 'opponent_declines')
            self.assertEqual(game.zones[1 - seat]['graveyard'], ['Wall of Stone', 'Forest', 'Giant Growth'])
            self.assertIn(dict(op='assert_forced_zone_choices', value=0), game.steps)

    def test_predation_keeps_printed_color_subtypes_and_nontargeted_shroud_effect(self):
        for seat in (0, 1):
            game = self.game(model.PREDATION, seat, 'shroud')
            self.assertFalse(any(step['op'] == 'choose_target' for step in game.steps))
            self.assertIn(dict(op='assert_card', ref='shroud', property='damage', value=4), game.steps)
            for prop, value in (('color', 0x08), ('subtype_Phyrexian', 1), ('subtype_Beast', 1)):
                self.assertIn(dict(op='assert_matching_permanents', player=seat, property=prop,
                                   value=value, index=18695), game.steps)

    def test_kiora_has_two_real_tapped_target_intents(self):
        for seat in (0, 1):
            game = self.game(model.KIORA, seat, 'untap')
            choices = [step for step in game.steps if step['op'] == 'choose_target']
            self.assertEqual([(s['player'], s['ref']) for s in choices],
                             [(seat, 'tap_creature'), (seat, 'tap_land')])
            self.assertIn(dict(op='assert_unused_choices', value=0), game.steps)
            self.assertEqual([s['value'] for s in game.steps if s.get('counter_kind') == 'loyalty'], [4, 5])

    def test_kiora_ultimate_pays_eight_earned_loyalty(self):
        for seat in (0, 1):
            game = self.game(model.KIORA, seat, 'ultimate')
            modes = [s['value'] for s in game.steps if s['op'] == 'choose_dialog']
            self.assertEqual(modes, [0, 0, 0, 0, 2])
            self.assertEqual(sum(s['op'] == 'untap_step' for s in game.steps), 8)
            self.assertEqual(game.zones[seat]['battlefield'].count('Octopus'), 3)
            self.assertEqual(game.zones[seat]['graveyard'], ['Kiora, Master of the Depths'])
