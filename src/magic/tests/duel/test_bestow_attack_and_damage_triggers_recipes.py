"""Focused guards for real Bestow mode, damage timing and qualifying attackers."""

from copy import deepcopy
import unittest

import bestow_attack_and_damage_triggers_recipes as model


class BestowAttackAndDamageTriggersTests(unittest.TestCase):
    def setUp(self):
        self.pins = model.pins()
        self.oracle = {n: p['oracle'] for group in ('sources', 'witnesses')
                       for n, p in self.pins[group].items()}
        self.rows = {n: p['registry'] for group in ('sources', 'witnesses')
                     for n, p in self.pins[group].items()}
        self.rows.update({p['registry']['name']: p['registry'] for p in self.pins['engine_aura_forms'].values()})

    def game(self, name, actor, mode):
        game = model.Game(self.rows[name], actor, mode, self.rows, self.oracle)
        game.play()
        return game

    def test_intact_oracle_price_and_dynamic_identity(self):
        model.validate_witnesses(self.oracle, self.rows)
        name = 'Kestia, the Cultivator'
        oracle = self.oracle[name]
        shape = model.classify(oracle, model.KESTIA)
        declaration = dict(family=model.FAMILY, version=2, params=shape, oracle_text=oracle['oracle_text'],
                           variants=model.variants(shape))
        self.assertEqual(model.spec(declaration, oracle)['bestowed'], [3, 0, 1, 1, 0, 1])
        bad = deepcopy(declaration)
        bad['params']['bestowed'][0] -= 1
        with self.assertRaises(ValueError):
            model.spec(bad, oracle)
        with self.assertRaises(ValueError):
            model.spec(declaration, dict(oracle, oracle_text=oracle['oracle_text'].replace('you control', '')))
        game = self.game(name, 0, 'bestowed_attack')
        self.assertEqual(game.id, game.aura_id)
        self.assertTrue(any(s['op'] == 'can_target_creature' and s['value'] == 0 for s in game.steps))

    def test_spiteful_life_is_independent_of_combat(self):
        for actor in (0, 1):
            foreign = self.game('Spiteful Returned', actor, 'foreign_attack')
            self.assertEqual(foreign.life[actor], 15)
            self.assertEqual(foreign.life[1 - actor], 20)
            blocked = self.game('Spiteful Returned', actor, 'bestowed_attack')
            self.assertEqual(blocked.life[1 - actor], 18)
            unrelated = self.game('Spiteful Returned', actor, 'unrelated_attacker')
            self.assertEqual(unrelated.life[1 - actor], 18)
            self.assertEqual([s['ref'] for s in unrelated.steps if s['op'] == 'attack'], ['host'])

    def test_thassa_real_noncombat_damage_never_draws(self):
        for actor in (0, 1):
            source = "Thassa's Emissary"
            game = self.game(source, actor, 'noncombat_damage')
            self.assertEqual(game.life[1 - actor], 17)
            self.assertEqual(game.library[actor], list(model.LIBRARY))
            self.assertFalse(any(s['op'] == 'combat_damage' for s in game.steps))
            self.assertEqual(game.objects['damage_spell']['zone'], 'graveyard')
            blocked = self.game(source, actor, 'blocked')
            self.assertEqual(blocked.life, [20, 20])
            self.assertEqual(blocked.library[actor], list(model.LIBRARY))

    def test_kestia_counts_the_union_and_ignores_the_opponent(self):
        for actor in (0, 1):
            source = 'Kestia, the Cultivator'
            team = self.game(source, actor, 'team')
            self.assertEqual(team.library[actor], list(model.LIBRARY[2:]))
            self.assertEqual([s['ref'] for s in team.steps
                              if s['op'] == 'cast' and s.get('ref', '').startswith('aura_')],
                             ['aura_one', 'aura_two'])
            opposing = self.game(source, actor, 'opponent_enchantment')
            self.assertEqual(opposing.library[actor], list(model.LIBRARY))
            self.assertEqual(opposing.attacking_player, 1 - actor)
