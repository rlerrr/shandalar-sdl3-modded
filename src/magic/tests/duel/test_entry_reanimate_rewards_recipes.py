"""Check whole Oracle, true entry and choice, and real qualifier histories."""

import json
from pathlib import Path
import unittest

from generate_cases import registry
import entry_reanimate_rewards_recipes as recipe


DUEL = Path(__file__).resolve().parent
ORACLE = DUEL / 'data/oracle.json'


class EntryReanimateRewardsRecipes(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.oracle = json.loads(ORACLE.read_text(encoding='utf8'))['cards']
        cls.rows = {row['name']: row for row in registry()}
        cls.homes = {}
        for name in recipe.profile()['sources']:
            csvid = cls.rows[name]['csvid']
            home, = (DUEL / f'data/cases/cards/{csvid // 100:03d}').glob(f'{csvid:05d}-*.json')
            cls.homes[name] = json.loads(home.read_text(encoding='utf8'))

    def actions(self, name, mode, who):
        home = self.homes[name]
        declaration, = home['recipes']
        params = recipe.spec(declaration, self.oracle[name])
        specimen, = recipe.expand(name, home['csvid'], declaration, params, [f'{mode}_{who}'], self.rows, self.oracle)
        return specimen['steps']

    def copies(self, name, who, zone, count):
        return dict(op='assert_zone_multiplicity', player=who, zone=zone, index=self.rows[name]['csvid'], value=count)

    def choose(self, name, who, *, decline=False):
        return dict(op='choose_optional_zone' if decline else 'choose_zone_card', player=who, value=self.rows[name]['csvid'])

    def test_whole_oracle_and_catalog(self):
        recipe.validate_witnesses(self.oracle, self.rows)
        self.assertEqual({h['csvid'] for h in self.homes.values()}, {4459, 5059, 13048, 14409, 16778, 17285})
        self.assertEqual(sum(len(h['recipes'][0]['variants']) for h in self.homes.values()), 160)
        for name, home in self.homes.items():
            declaration, = home['recipes']
            self.assertEqual(declaration['variants'], recipe.variants(recipe.spec(declaration, self.oracle[name])))
            with self.assertRaises(ValueError):
                recipe.classify(dict(self.oracle[name], oracle_text=declaration['oracle_text'] + '\nDraw a card.'))

    def test_delver_rider_follows_selected_card_mana_value_after_entry(self):
        for who in (0, 1):
            for mode, witness, life in [('effect', 'Grizzly Bears', 18), ('alternate', 'Hill Giant', 16), ('zero', 'Memnite', 20)]:
                steps = self.actions('Phyrexian Delver', mode, who)
                select = steps.index(self.choose(witness, who))
                self.assertEqual(steps[select+1], dict(op='resolve'))
                self.assertIn(dict(op='assert_life', player=who, value=20), steps[:select])
                self.assertIn(dict(op='assert_life', player=who, value=life), steps[select+2:])
                self.assertIn(self.copies(witness, who, 'battlefield', 1), steps[select+2:])

    def test_raid_requires_actual_own_current_declaration_even_fogged(self):
        for who in (0, 1):
            for mode in ('effect', 'fogged', 'zero'):
                steps = self.actions('Timely Hordemate', mode, who)
                attack = steps.index(dict(op='attack', ref='attacker', value=1))
                target = self.choose('Memnite' if mode == 'zero' else 'Grizzly Bears', who)
                self.assertIn(target, steps[attack:])
                if mode == 'fogged':
                    self.assertIn(dict(op='cast', ref='fog', value=1), steps[:attack])
                    self.assertNotIn(dict(op='assert_life', player=1-who, value=18), steps)
                self.assertIn(dict(op='assert_offered_legal_card', index=self.rows['Hill Giant']['csvid'], value=0), steps)
            for mode in ('damage_only', 'no_attack', 'prior_turn', 'opponent_attack', 'mana_boundary'):
                steps = self.actions('Timely Hordemate', mode, who)
                self.assertFalse(any(s['op'] == 'choose_zone_card' for s in steps))

    def test_bloodline_subtype_union_decline_and_lifelink(self):
        for who in (0, 1):
            for mode, name in [('effect', 'Sengir Vampire'), ('alternate', 'Prodigal Sorcerer'), ('changeling', 'Woodland Changeling')]:
                steps = self.actions('Bloodline Necromancer', mode, who)
                self.assertIn(self.choose(name, who), steps)
                self.assertIn(self.copies(name, who, 'battlefield', 1), steps)
                self.assertIn(dict(op='assert_offered_legal_card', index=self.rows['Nameless Inversion']['csvid'], value=0), steps)
            decline = self.actions('Bloodline Necromancer', 'decline', who)
            self.assertIn(self.choose('Sengir Vampire', who, decline=True), decline)
            for mode in ('lifelink', 'life_trade'):
                actions = self.actions('Bloodline Necromancer', mode, who)
                swing = actions.index(dict(op='combat_damage'))
                self.assertIn(dict(op='assert_life', player=who, value=23), actions[swing:])
                self.assertIn(dict(op='assert_life', player=1-who, value=20 if mode == 'life_trade' else 17), actions[swing:])

    def test_horror_opponent_chooses_and_can_take_untargeted_under_shroud(self):
        for who in (0, 1):
            for mode in ('effect', 'shroud'):
                steps = self.actions('Enslaved Horror', mode, who)
                self.assertIn(self.choose('Grizzly Bears', 1-who), steps)
                self.assertIn(self.copies('Grizzly Bears', 1-who, 'battlefield', 1), steps)
                self.assertIn(self.copies('Grizzly Bears', who, 'battlefield', 0), steps)
            decline = self.actions('Enslaved Horror', 'decline', who)
            self.assertIn(self.choose('Grizzly Bears', 1-who, decline=True), decline)

    def test_primordial_grave_owner_control_and_actual_bounce(self):
        for who in (0, 1):
            steps = self.actions('Sepulchral Primordial', 'effect', who)
            select = steps.index(self.choose('Grizzly Bears', who))
            bounce = steps.index(dict(op='cast', ref='reply', value=1))
            self.assertIn(self.copies('Grizzly Bears', who, 'battlefield', 1), steps[select:bounce])
            self.assertIn(self.copies('Grizzly Bears', 1-who, 'graveyard', 0), steps[select:bounce])
            self.assertIn(self.copies('Grizzly Bears', 1-who, 'hand', 1), steps[bounce:])
            for mode, allowed in [('intimidate_ground', 0), ('intimidate_black', 1), ('intimidate_artifact', 1)]:
                rules = self.actions('Sepulchral Primordial', mode, who)
                self.assertIn(dict(op='block', ref='blocker', other='source', value=allowed), rules)

    def test_revolt_uses_controlled_current_departures_or_actual_blink(self):
        for who in (0, 1):
            for mode, witness in [('effect', 'Grizzly Bears'), ('artifact', 'Darksteel Relic'), ('land', 'Forest'),
                                  ('stolen_departure', 'Grizzly Bears'), ('blink', 'Grizzly Bears')]:
                steps = self.actions('Renegade Rallier', mode, who)
                prior = 'blink' if mode == 'blink' else 'bounce'
                cast = steps.index(dict(op='cast', ref=prior, value=1))
                choice = steps.index(self.choose(witness, who))
                self.assertLess(cast, choice)
                self.assertIn(self.copies(witness, who, 'battlefield', 1), steps[choice:])
                if mode == 'stolen_departure':
                    self.assertIn(self.copies('Grizzly Bears', 1-who, 'hand', 1), steps[cast:choice])
            for mode in ('no_departure', 'opponent_departs', 'prior_turn', 'mana_boundary'):
                actions = self.actions('Renegade Rallier', mode, who)
                self.assertFalse(any(s['op'] == 'choose_zone_card' for s in actions))

    def test_all_replays_restore_exact_pending_decisions_and_actions(self):
        for name in self.homes:
            for who in (0, 1):
                steps = self.actions(name, 'replay', who)
                cut = steps.index(dict(op='capture'))
                record = steps.index(dict(op='record_state'))
                replay = steps.index(dict(op='restore'))
                self.assertEqual(steps[cut+1:record], steps[replay+1:-1])
                self.assertIn(dict(op='cast', ref='source', value=1), steps[cut+1:record])


if __name__ == '__main__':
    unittest.main()
