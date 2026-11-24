"""Check exact Mill source identity and rules under the staged coverage model."""
from copy import deepcopy
import unittest

from mill_combat_and_draw_recipes import (
    ALCHEMIST, BELL, DRAGON, FAMILY, MIND, SIDISI, SZADEK, TUTEL, WORLD, Board, classify,
    profiles, spec, validate_witnesses, variants,
)


def inputs():
    pinned = profiles()
    oracle = {name: value['oracle'] for group in pinned.values() for name, value in group.items()}
    rows = {name: value['registry'] for group in pinned.values() for name, value in group.items()}
    return oracle, rows


def scene(csvid, owner, mode):
    oracle, rows = inputs()
    name, = [name for name in profiles()['sources'] if rows[name]['csvid'] == csvid]
    board = Board(rows[name], owner, mode, rows, oracle)
    board.run()
    return board


class MillCombatDrawGuards(unittest.TestCase):
    def test_whole_oracle_and_typed_declaration(self):
        oracle, _ = inputs()
        record = oracle['World Shaper']
        shape = classify(record, WORLD)
        declaration = dict(family=FAMILY, version=2, params=shape,
                           oracle_text=record['oracle_text'], variants=variants(shape))
        self.assertEqual(spec(declaration, record)['csvid'], WORLD)
        for invalid in (dict(declaration, version=2.0), dict(declaration, variants=['decline_1']),
                        dict(declaration, params=dict(shape, csvid=float(WORLD)))):
            with self.assertRaises(ValueError):
                spec(invalid, record)
        with self.assertRaises(ValueError):
            spec(declaration, dict(record, oracle_text=record['oracle_text'].replace('may ', '')))

    def test_witnesses_and_token_registry_are_pinned(self):
        oracle, rows = inputs()
        validate_witnesses(oracle, rows)
        altered = deepcopy(rows)
        altered['Zombie']['iid'] += 1
        with self.assertRaises(ValueError):
            validate_witnesses(oracle, altered)

    def test_bump_uses_actual_automatic_opponent_but_still_mills(self):
        for actor in (0, 1):
            for mode, left in (('loss', 5), ('short_library', 0)):
                game = scene(MIND, actor, mode)
                self.assertFalse(any(step['op'] == 'choose_player' for step in game.steps))
                self.assertEqual(game.life[1-actor], 17)
                self.assertEqual(len(game.libraries[1-actor]), left)
                self.assertEqual(len(game.libraries[actor]), 8)

    def test_original_damage_draw_and_token_contrasts(self):
        for actor in (0, 1):
            self.assertEqual(len(scene(BELL, actor, 'combat').libraries[1-actor]), 4)
            self.assertEqual(len(scene(TUTEL, actor, 'turn_chain').libraries[1-actor]), 2)
            self.assertEqual(scene(DRAGON, actor, 'paid').zombies, 1)
            self.assertEqual(scene(DRAGON, actor, 'unfunded').zombies, 0)

    def test_sidisi_counts_one_zombie_per_own_batch(self):
        for actor in (0, 1):
            self.assertEqual(scene(SIDISI, actor, 'entry_mixed').zombies, 1)
            self.assertEqual(scene(SIDISI, actor, 'entry_lands').zombies, 0)
            self.assertEqual(scene(SIDISI, actor, 'attack_trigger').zombies, 1)
            self.assertEqual(scene(SIDISI, actor, 'external_mill').zombies, 1)
            self.assertEqual(scene(SIDISI, actor, 'foe_mill').zombies, 0)

    def test_szadek_grows_and_alchemist_only_replaces_own_zombie(self):
        for actor in (0, 1):
            repeated = scene(SZADEK, actor, 'repeat')
            self.assertEqual(repeated.stats('source'), (20, 20))
            self.assertEqual(repeated.life, [20, 20])
            self.assertEqual(len(repeated.libraries[1-actor]), 9)
            self.assertEqual(scene(SZADEK, actor, 'noncombat').life[1-actor], 19)
            allied = scene(ALCHEMIST, actor, 'own_zombie')
            self.assertEqual(allied.life, [20, 20])
            self.assertEqual(allied.zombies, 2)
            other = scene(ALCHEMIST, actor, 'enemy_zombie')
            self.assertEqual(other.life[actor], 18)
            self.assertEqual(other.zombies, 0)

    def test_world_shaper_exact_tapped_ordered_returns(self):
        for actor in (0, 1):
            result = scene(WORLD, actor, 'after_attack')
            returns = [entry['name'] for entry in result.cards.values() if entry.get('returned_tapped')]
            self.assertEqual(returns, ['Plains', 'Forest', 'Island'])
            exiled = scene(WORLD, actor, 'exiled')
            self.assertEqual(exiled.cards['source']['zone'], 'exile')
            self.assertEqual(exiled.cards['grave_control']['zone'], 'graveyard')
        decline = scene(WORLD, 0, 'decline')
        self.assertEqual(decline.cards['grave_control']['zone'], 'battlefield')
        self.assertFalse(any(step['op'] == 'choose_trigger_source' for step in decline.steps))
