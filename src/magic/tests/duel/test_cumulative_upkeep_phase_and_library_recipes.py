"""Checks for exact phase/library Oracle, selector and draw provenance."""

from copy import deepcopy
import unittest

import cumulative_upkeep_phase_and_library_recipes as model


class PhaseAndLibraryTests(unittest.TestCase):
    def setUp(self):
        self.pins = model.profiles()
        self.sources = self.pins['sources']
        self.oracle = {n: p['oracle'] for group in self.pins.values() for n, p in group.items()}
        self.rows = {n: p['registry'] for group in self.pins.values() for n, p in group.items()}

    def test_whole_records_and_versioned_payment(self):
        for pin in self.sources.values():
            record, csvid = pin['oracle'], pin['registry']['csvid']
            shape = model.classify(record, csvid)
            declaration = dict(family=model.FAMILY, version=2, params=shape,
                               oracle_text=record['oracle_text'], variants=model.variants(shape))
            self.assertEqual(model.spec(declaration, record)['csvid'], csvid)
            with self.assertRaises(ValueError):
                model.spec(declaration, dict(record, oracle_text=record['oracle_text'].splitlines()[0]))
            altered = deepcopy(declaration)
            altered['params']['upkeep'][0] += 1
            with self.assertRaises(ValueError):
                model.spec(altered, record)

    def test_exact_witnesses_reject_wrong_token_and_mana_dork(self):
        model.validate_witnesses(self.oracle, self.rows)
        for changed in (model.TOMBSPAWN, model.MV1):
            rows = dict(self.rows)
            rows[changed] = rows['Zombie' if changed == model.TOMBSPAWN else model.MV2]
            with self.assertRaises(ValueError):
                model.validate_witnesses(self.oracle, rows)

    def test_real_draw_follows_same_seat_actual_upkeep(self):
        for source, mode in (('Wave of Terror', 'draw_values'), ('Malignant Growth', 'draw_ages')):
            for actor in (0, 1):
                game = model.ExtendedCase(self.rows[source], actor, mode, self.oracle, self.rows)
                game.build()
                preceding = None
                draw_players = []
                for step in game.steps:
                    if step['op'] == 'upkeep_step':
                        preceding = step['player']
                    if step['op'] == 'draw_step':
                        self.assertEqual(step, dict(op='draw_step', player=preceding))
                        draw_players.append(step['player'])
                expected = [1 - actor, actor, 1 - actor, actor]
                if source == 'Malignant Growth':
                    expected.append(1 - actor)
                    self.assertEqual(game.life[1 - actor], 17)
                else:
                    self.assertEqual(game.cards['skeleton']['zone'], 'graveyard')
                self.assertEqual(draw_players, expected)

    def test_tombspawn_ends_with_source_and_respects_actor_graves(self):
        for actor in (0, 1):
            game = model.ExtendedCase(self.rows['Tombstone Stairwell'], actor, 'destroy_stairwell',
                                      self.oracle, self.rows)
            game.build()
            self.assertEqual(game.token_counts[actor], 1)
            self.assertEqual(game.token_counts[1 - actor], 2)
            self.assertEqual(game.tokens, [0, 0])
            self.assertEqual(game.cards['source']['zone'], 'graveyard')
            self.assertEqual(game.cards['disenchant']['zone'], 'graveyard')

    def test_remora_payment_and_owner_decline_are_distinct_inputs(self):
        actor = 0
        row = self.rows['Mystic Remora']
        games = [model.ExtendedCase(row, actor, mode, self.oracle, self.rows)
                 for mode in ('exact_payment', 'decline_draw')]
        for game, chooser in zip(games, (1 - actor, actor)):
            game.build()
            spell, = [i for i, op in enumerate(game.steps) if op['op'] == 'announce_spell']
            self.assertEqual(game.steps[spell - 1]['player'], chooser)
            self.assertEqual(game.steps[spell - 1]['op'], 'choose_dialog')
            self.assertEqual(game.steps[spell - 1]['value'], 1)
