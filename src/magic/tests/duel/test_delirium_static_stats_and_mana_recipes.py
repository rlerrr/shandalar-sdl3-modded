"""Focused guards for paid mana use and real graveyard transitions."""

from copy import deepcopy
import unittest

from delirium_static_stats_and_mana_recipes import FAMILY, StaticDuel, classify, profiles, spec, variants


class StaticAndManaTests(unittest.TestCase):
    def setUp(self):
        pins = profiles()['sources'] | profiles()['witnesses']
        self.rows = {n: pin['registry'] for n, pin in pins.items()}
        self.oracle = {n: pin['oracle'] for n, pin in pins.items()}

    def board(self, name, actor, mode):
        game = StaticDuel(name, self.rows[name]['csvid'], actor, mode, self.rows, self.oracle)
        game.build()
        return game

    def test_crypt_exiles_its_sacrifice_only_if_its_own_seat_was_targeted(self):
        for actor, expected in ((0, 5), (1, 4)):
            game = self.board('Moldgraf Scavenger', actor, 'lose_threshold')
            receipt, = (s for s in game.steps if s['op'] == 'assert_zone' and s['zone'] == 'exile')
            self.assertEqual((receipt['player'], receipt['value']), (actor, expected))
            self.assertFalse(game.active)

    def test_distinct_earned_mana_pays_both_real_spells_after_actual_untap(self):
        game = self.board('Deathcap Cultivator', 1, 'mana_repeat')
        choices = [s for s in game.steps if s['op'] == 'choose_color']
        self.assertEqual([(s['player'], s['value']) for s in choices], [(1, 1), (1, 3)])
        for spend in ('spend', 'again'):
            finish, = (i for i, s in enumerate(game.steps) if s['op'] == 'cast' and s.get('ref') == spend)
            start = max(i for i, s in enumerate(game.steps[:finish]) if s['op'] == 'choose_color')
            self.assertNotIn('set_mana', (s['op'] for s in game.steps[start:finish]))

    def test_exact_pinned_delirium_amount_and_variants(self):
        record = self.oracle['Gnarlwood Dryad']
        model = classify(record, 16113)
        entry = dict(family=FAMILY, version=2, oracle_text=record['oracle_text'],
                     params=model, variants=variants(model))
        self.assertEqual(spec(entry, record)['delirium_delta'], [2, 2])
        mutation = deepcopy(entry)
        mutation['params']['delirium_delta'][0] += 1
        with self.assertRaises(ValueError):
            spec(mutation, record)
