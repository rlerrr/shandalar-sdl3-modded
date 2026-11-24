"""Focused guards on the production Fight and Morph boundaries."""

import unittest

import fight_triggered_sources_recipes as model


class TriggeredFightGuard(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        pins = model.profiles()
        entries = pins['sources'] | pins['witnesses']
        cls.oracle = {name: pin['oracle'] for name, pin in entries.items()}
        cls.rows = {name: pin['registry'] for name, pin in entries.items()}
        hidden = pins['native_face_down_observer']
        cls.rows[hidden['name']] = hidden
        cls.sources = {row['csvid']: row for row in cls.rows.values()}

    def duel(self, csvid, mode, actor):
        game = model.Duel(self.sources[csvid], actor, mode, self.rows, self.oracle)
        game.build()
        return game

    def test_exact_declared_card_text_and_hidden_observer(self):
        model.validate_witnesses(self.oracle, self.rows)
        for csvid, row in self.sources.items():
            if csvid not in model.CARDS:
                continue
            record = self.oracle[row['name']]
            shape = model.classify(record, csvid)
            declaration = dict(version=2, family=model.FAMILY, params=shape,
                               oracle_text=record['oracle_text'], variants=model.variants(shape))
            self.assertEqual(model.spec(declaration, record)['csvid'], csvid)
            with self.assertRaises(ValueError):
                model.spec(declaration, {**record, 'oracle_text': record['oracle_text'] + '\nDraw a card.'})
            with self.assertRaises(ValueError):
                model.spec({**declaration, 'version': True}, record)

    def test_actual_temur_yes_no_menu_precedes_target_for_both_actors(self):
        for actor in (0, 1):
            yes = self.duel(14644, 'manifest_fight', actor)
            no = self.duel(14644, 'manifest_pass', actor)
            yes_ops = [s for s in yes.steps if s['op'] in ('choose_dialog', 'choose_target', 'activate')]
            no_ops = [s for s in no.steps if s['op'] in ('choose_dialog', 'choose_target', 'activate')]
            self.assertEqual([s['op'] for s in yes_ops], ['choose_dialog', 'choose_target', 'activate'])
            self.assertEqual((yes_ops[0]['player'], yes_ops[0]['value'], yes_ops[1]['ref']), (actor, 0, 'target'))
            self.assertEqual([s['op'] for s in no_ops], ['choose_dialog', 'activate'])
            self.assertEqual((no_ops[0]['player'], no_ops[0]['value']), (actor, 1))
            self.assertEqual(no.cards['target']['zone'], 'battlefield')
            self.assertTrue(no.cards['manifested']['hidden'])

    def test_face_up_timing_witness_distinguishes_two_power_from_four(self):
        for actor in (0, 1):
            game = self.duel(14644, 'faceup_uses_revealed_pt', actor)
            self.assertEqual(game.cards['target']['name'], 'Hill Giant')
            self.assertEqual(game.cards['target']['zone'], 'graveyard')
            self.assertEqual(game.cards['turner']['damage'], 3)
            self.assertEqual([s['value'] for s in game.steps if s['op'] == 'assert_card'
                              and s['ref'] == 'turner' and s['property'] == 'power'], [2, 4])

    def test_fight_deaths_and_opposing_flyer_controls(self):
        for actor in (0, 1):
            game = self.duel(12966, 'mutual_death', actor)
            self.assertEqual((game.cards['source']['zone'], game.cards['victim']['zone']), ('graveyard', 'graveyard'))
            siege = self.duel(14555, 'enemy_flier', actor)
            self.assertEqual(siege.cards['flier']['owner'], 1 - actor)
            self.assertEqual(siege.cards['flier']['damage'], 0)
            self.assertFalse(any(s['op'] == 'choose_target' for s in siege.steps))

    def test_regent_delayed_counter_outlasts_source_and_waits_for_end(self):
        for actor in (0, 1):
            game = self.duel(14916, 'persists_after_source', actor)
            self.assertEqual(game.cards['source']['zone'], 'graveyard')
            self.assertEqual(game.cards['ally']['counters'], 2)
            advances = [i for i, s in enumerate(game.steps) if s['op'] == 'advance_turn']
            last = advances[-1]
            values = [(i, s['value']) for i, s in enumerate(game.steps) if s['op'] == 'assert_counter' and s['ref'] == 'ally']
            self.assertTrue(all(value == 0 for i, value in values if i < last))
            self.assertEqual([value for i, value in values if i > last], [2])
