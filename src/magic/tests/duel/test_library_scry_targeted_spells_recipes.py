"""Focused coverage of exact prompts, target loss, and ordered Scry receipts."""

from copy import deepcopy
import unittest

import library_scry_targeted_spells_recipes as model


class TargetedScryTests(unittest.TestCase):
    def scene(self, cid, seat, mode):
        pins = model.pins()['sources'] | model.pins()['witnesses']
        rows = {name: pin['registry'] for name, pin in pins.items()}
        oracle = {name: pin['oracle'] for name, pin in pins.items()}
        row, = (row for row in rows.values() if row['csvid'] == cid)
        board = model.Scene(row, seat, mode, rows, oracle)
        board.play()
        return board

    def test_whole_oracle_guard(self):
        self.assertEqual(set(model.MODES), {x['registry']['csvid'] for x in model.pins()['sources'].values()})
        for name, pin in model.pins()['sources'].items():
            record, cid = pin['oracle'], pin['registry']['csvid']
            shape = model.classify(record, cid)
            self.assertEqual(shape['scry'], 2 if cid == model.FILL else 1)
            changed = deepcopy(record)
            changed['oracle_text'] = record['oracle_text'][:record['oracle_text'].lower().index('scry')]
            with self.assertRaisesRegex(ValueError, 'complete normal Scry rules'):
                model.classify(changed, cid)
            self.assertEqual(record['name'], name)

    def test_anchor_keeps_correct_owner_and_orders_effects(self):
        for seat in (0, 1):
            for mode in ('own_keep', 'own_bottom', 'borrowed'):
                scene = self.scene(model.ANCHOR, seat, mode)
                deck = scene.zones[seat]['library']
                foreign = scene.zones[1 - seat]['library']
                if mode == 'own_keep':
                    self.assertEqual(deck[0], 'Hill Giant')
                if mode == 'own_bottom':
                    self.assertEqual(deck[-1], 'Hill Giant')
                    self.assertEqual(deck[0], model.DECK[0])
                if mode == 'borrowed':
                    self.assertEqual(foreign, ['Hill Giant', *model.FOE_DECK])
                    self.assertEqual(deck, [*model.DECK[1:], model.DECK[0]])

    def test_actual_loss_preserves_scry_order(self):
        for cid, mode in ((model.CRUEL, 'target_lost'), (model.SEA, 'all_lost'), (model.SELECT, 'untap_response'),
                          (model.HARSH, 'shroud_response'), (model.SPITE, 'target_lost')):
            for seat in (0, 1):
                scene = self.scene(cid, seat, mode)
                with self.subTest(cid=cid, seat=seat):
                    self.assertIn(dict(op='cast_response', ref='response', value=1), scene.steps)
                    self.assertIn(dict(op='resolve_response'), scene.steps)
                    self.assertIn(dict(op='assert_unused_choices', value=1), scene.steps)
                    self.assertEqual(scene.zones[seat]['library'], list(model.DECK))
        untapped = self.scene(model.SELECT, 0, 'untapped_only')
        self.assertIn(dict(op='can_cast', ref='source', value=0), untapped.steps)
        self.assertFalse(any(step['op'] == 'choose_zone' for step in untapped.steps))

    def test_fill_targets_players_and_orders_two_bottomed_cards(self):
        for seat in (0, 1):
            two = self.scene(model.FILL, seat, 'two_both_bottom')
            one = self.scene(model.FILL, seat, 'self_one_bottom')
            self.assertIn(dict(op='choose_player', player=seat, value=1-seat), two.steps)
            self.assertIn(dict(op='choose_player', player=seat, value=seat), one.steps)
            self.assertEqual(two.zones[1-seat]['graveyard'], ['Hill Giant', 'Giant Growth'])
            self.assertEqual(two.zones[seat]['library'], ['Forest', 'Plains', 'Lightning Bolt', 'Giant Growth'])
            self.assertEqual(one.zones[seat]['library'], ['Giant Growth', 'Forest', 'Plains', 'Lightning Bolt'])

    def test_spite_counts_only_own_instant_and_sorcery_cards(self):
        for mode, count in (('mixed_count', 2), ('zero_spells', 0)):
            scene = self.scene(model.SPITE, 0, mode)
            self.assertIn(dict(op='assert_card', ref='recipient', property='damage', value=count), scene.steps)
            self.assertIn('Giant Growth', scene.zones[1]['graveyard'])
            self.assertIn('Grizzly Bears', scene.zones[0]['graveyard'])
            self.assertIn('Forest', scene.zones[0]['graveyard'])
        lethal = self.scene(model.SPITE, 1, 'lethal_count')
        self.assertIn('Hill Giant', lethal.zones[0]['graveyard'])

    def test_search_and_tapped_land_are_distinct_from_scry(self):
        for seat in (0, 1):
            both = self.scene(model.PEREGRINATION, seat, 'both_basics')
            none = self.scene(model.PEREGRINATION, seat, 'no_basic')
            self.assertIn(dict(op='assert_card', ref='ramp_land', property='tapped', value=1), both.steps)
            self.assertIn(dict(op='assert_shuffle_count', player=seat, value=1), both.steps)
            self.assertEqual(both.zones[seat]['library'], ['Lightning Bolt'])
            self.assertEqual(both.zones[seat]['hand'], ['Island'])
            self.assertEqual(none.zones[seat]['library'], ['Yavimaya Coast'])
            self.assertEqual(none.zones[seat]['battlefield'], [])
