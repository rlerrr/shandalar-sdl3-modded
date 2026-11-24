"""Check genuinely ordered crown draws and each independently modeled rider."""

import json
from pathlib import Path
import unittest

from generate_cases import registry
import monarch_entry_riders_recipes as recipe


DUEL = Path(__file__).resolve().parent
ORACLE = DUEL / 'data/oracle.json'


class MonarchEntryRidersRecipes(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.oracle = json.loads(ORACLE.read_text(encoding='utf8'))['cards']
        cls.rows = {row['name']: row for row in registry()}
        cls.homes = {}
        for name in recipe.profile()['sources']:
            csvid = cls.rows[name]['csvid']
            path, = (DUEL / f'data/cases/cards/{csvid // 100:03d}').glob(f'{csvid:05d}-*.json')
            cls.homes[name] = json.loads(path.read_text(encoding='utf8'))

    def case(self, name, mode, who):
        home = self.homes[name]
        clause, = home['recipes']
        params = recipe.spec(clause, self.oracle[name])
        result, = recipe.expand(name, home['csvid'], clause, params, [f'{mode}_{who}'], self.rows, self.oracle)
        return result

    def copies(self, name, who, zone, n):
        return dict(op='assert_zone_multiplicity', player=who, zone=zone, index=self.rows[name]['csvid'], value=n)

    def test_exact_oracle_and_catalog(self):
        recipe.validate_witnesses(self.oracle, self.rows)
        self.assertEqual({h['csvid'] for h in self.homes.values()}, {16330, 16334, 16424, 16472, 16473, 16483, 16519, 16547})
        self.assertEqual(sum(len(h['recipes'][0]['variants']) for h in self.homes.values()), 164)
        for name, home in self.homes.items():
            clause, = home['recipes']
            self.assertEqual(clause['variants'], recipe.variants(recipe.spec(clause, self.oracle[name])))
            with self.assertRaises(ValueError):
                recipe.classify(dict(self.oracle[name], oracle_text=clause['oracle_text'] + '\nDraw a card.'))

    def test_paid_entry_draw_and_true_combat_only_crown_change(self):
        for who in (0, 1):
            steps = self.case('Palace Sentinels', 'transfer', who)['steps']
            hit = steps.index(dict(op='combat_damage'))
            end = steps.index(dict(op='cleanup'))
            self.assertIn(self.copies('Island', who, 'hand', 1), steps[:hit])
            self.assertNotIn(self.copies('Swamp', 1-who, 'hand', 1), steps[:end])
            self.assertIn(self.copies('Swamp', 1-who, 'hand', 1), steps[end:])
            self.assertIn(dict(op='assert_zone_card', player=1-who, zone='library', index=0, value=self.rows['Forest']['csvid']), steps[end:])
            for mode in ('noncombat', 'fogged', 'source_gone'):
                negative = self.case('Palace Sentinels', mode, who)['steps']
                self.assertNotIn(self.copies('Swamp', 1-who, 'hand', 1), negative)
                self.assertIn(self.copies('Island', who, 'hand', 1), negative)

    def test_thorn_deathtouch_kills_the_larger_attacker_without_player_damage(self):
        for who in (0, 1):
            steps = self.case('Thorn of the Black Rose', 'death_touch', who)['steps']
            hit = steps.index(dict(op='combat_damage'))
            self.assertIn(dict(op='block', ref='source', other='stealer', value=1), steps[:hit])
            self.assertIn(self.copies('Hill Giant', 1-who, 'graveyard', 1), steps[hit:])
            self.assertIn(self.copies('Thorn of the Black Rose', who, 'graveyard', 1), steps[hit:])
            self.assertIn(dict(op='assert_life', player=who, value=20), steps[hit:])

    def test_hireling_requires_defending_monarch_and_can_take_crown_back(self):
        for who in (0, 1):
            locked = self.case('Crown-Hunter Hireling', 'locked', who)['steps']
            self.assertIn(dict(op='attack', ref='source', value=0), locked)
            steps = self.case('Crown-Hunter Hireling', 'steal_back', who)['steps']
            swing = steps.index(dict(op='attack', ref='source', value=1))
            self.assertIn(self.copies('Swamp', 1-who, 'hand', 1), steps[:swing])
            self.assertIn(self.copies('Mountain', who, 'hand', 1), steps[swing:])

    def test_lich_targets_any_player_and_keys_sacrifice_to_new_acquisition(self):
        name = 'Custodi Lich'
        for who in (0, 1):
            taken = self.case(name, 'entry', who)['steps']
            self.assertIn(dict(op='choose_player', player=who, value=1-who), taken)
            self.assertIn(self.copies('Grizzly Bears' if who else 'Hill Giant', 1-who, 'graveyard', 1), taken)
            self.assertIn(dict(op='choose_player', player=who, value=who), self.case(name, 'self_target', who)['steps'])
            unchanged = self.case(name, 'already', who)['steps']
            self.assertIn(dict(op='assert_unused_choices', value=1 + int(who == 1)), unchanged)
            regained = self.case(name, 'regain_combat', who)['steps']
            swing = regained.index(dict(op='attack', ref='source', value=1))
            self.assertIn(dict(op='choose_player', player=who, value=1-who), regained[swing:])
            self.assertIn(self.copies('Raging Goblin', 1-who, 'graveyard', 1), regained[swing:])

    def test_keeper_needs_real_upkeep_and_protects_team_after_resolution(self):
        for who in (0, 1):
            for mode, attacker, legal in [('upkeep_self', 'source', 0), ('upkeep_ally', 'ally', 0),
                                          ('removed', 'ally', 0), ('no_upkeep', 'ally', 1), ('lost', 'ally', 1)]:
                steps = self.case('Keeper of Keys', mode, who)['steps']
                battle = steps.index(dict(op='attack', ref=attacker, value=1))
                self.assertEqual(dict(op='upkeep_step', player=who) in steps[:battle], mode != 'no_upkeep')
                self.assertIn(dict(op='block', ref='blocker', other=attacker, value=legal), steps[battle:])
                if mode == 'removed':
                    self.assertIn(self.copies('Keeper of Keys', who, 'graveyard', 1), steps[:battle])

    def test_jailer_returns_the_exact_card_to_its_owner_after_crown_loss(self):
        for who in (0, 1):
            for mode, rightful in [('cross_owned', who), ('source_gone', 1-who)]:
                steps = self.case('Palace Jailer', mode, who)['steps']
                hit = steps.index(dict(op='combat_damage'))
                self.assertIn(dict(op='choose_target', player=who, ref='victim'), steps[:hit])
                self.assertIn(self.copies('Hill Giant', rightful, 'exile', 1), steps[:hit])
                self.assertIn(self.copies('Hill Giant', rightful, 'battlefield', 1), steps[hit:])
                if mode == 'source_gone':
                    murder = steps.index(dict(op='cast', ref='remove', value=1))
                    self.assertIn(self.copies('Hill Giant', rightful, 'exile', 1), steps[murder:hit])

    def test_protector_redirect_does_not_give_combat_damage_to_monarch(self):
        for who in (0, 1):
            steps = self.case('Protector of the Crown', 'transfer', who)['steps']
            damage = steps.index(dict(op='combat_damage'))
            self.assertIn(dict(op='assert_card', ref='source', property='damage', value=2), steps[damage:])
            self.assertIn(dict(op='assert_life', player=who, value=20), steps[damage:])
            self.assertNotIn(self.copies('Swamp', 1-who, 'hand', 1), steps)
            lethal = self.case('Protector of the Crown', 'redirect_lethal', who)['steps']
            self.assertIn(self.copies('Protector of the Crown', who, 'graveyard', 1), lethal)

    def test_despot_creates_a_real_red_flying_dragon_only_as_monarch(self):
        for who in (0, 1):
            steps = self.case('Skyline Despot', 'dragon_flying', who)['steps']
            upkeep = steps.index(dict(op='upkeep_step', player=who))
            for prop, value in [('token', 1), ('color', 16), ('subtype_Dragon', 1), ('power', 5), ('toughness', 5)]:
                self.assertIn(dict(op='assert_matching_permanents', player=who, index=18719, property=prop, value=value), steps[upkeep:])
            self.assertIn(dict(op='block', ref='blocker', other='dragon', value=0), steps)
            for mode in ('no_upkeep', 'lost'):
                denied = self.case('Skyline Despot', mode, who)['steps']
                self.assertNotIn(self.copies('Dragon', who, 'battlefield', 1), denied)

    def test_cleanup_clears_marks_after_the_separate_damage_receipt(self):
        for who in (0, 1):
            for name, mode, ref in [
                ('Keeper of Keys', 'lost', 'blocker'), ('Keeper of Keys', 'no_upkeep', 'blocker'),
                ('Protector of the Crown', 'noncombat', 'source'), ('Protector of the Crown', 'transfer', 'source'),
            ]:
                steps = self.case(name, mode, who)['steps']
                cleanup = steps.index(dict(op='cleanup'))
                self.assertIn(dict(op='assert_card', ref=ref, property='damage', value=2), steps[:cleanup])
                self.assertIn(dict(op='assert_card', ref=ref, property='damage', value=0), steps[cleanup:])
                self.assertNotIn(dict(op='assert_card', ref=ref, property='damage', value=2), steps[cleanup:])

    def test_every_ending_replay_runs_identical_paid_and_end_step_actions(self):
        for name in self.homes:
            for who in (0, 1):
                case = self.case(name, 'replay', who)
                self.assertFalse(any(obj['name'] == 'The Monarch' for obj in case['setup']))
                steps = case['steps']
                cut, record, restore = [steps.index(dict(op=op)) for op in ('capture', 'record_state', 'restore')]
                self.assertEqual(steps[cut+1:record], steps[restore+1:-1])
                self.assertIn(dict(op='cleanup'), steps[cut+1:record])


if __name__ == '__main__':
    unittest.main()
