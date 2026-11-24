"""Verify whole-Oracle riders and the real input and observation boundaries."""

import json
from pathlib import Path
import unittest

from generate_cases import registry
import reanimation_additional_riders_recipes as recipe


DUEL = Path(__file__).resolve().parent
ORACLE = DUEL / 'data/oracle.json'


class ReanimationAdditionalRidersRecipes(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.oracle = json.loads(ORACLE.read_text(encoding='utf8'))['cards']
        cls.rows = {row['name']: row for row in registry()}
        cls.homes = {}
        for name in recipe.profile()['sources']:
            csvid = cls.rows[name]['csvid']
            path, = (DUEL / f'data/cases/cards/{csvid // 100:03d}').glob(f'{csvid:05d}-*.json')
            cls.homes[name] = json.loads(path.read_text(encoding='utf8'))

    def actions(self, name, mode, who):
        home = self.homes[name]
        declaration, = home['recipes']
        params = recipe.spec(declaration, self.oracle[name])
        case, = recipe.expand(name, home['csvid'], declaration, params, [f'{mode}_{who}'], self.rows, self.oracle)
        return case['steps']

    def copies(self, name, who, zone, count):
        return dict(op='assert_zone_multiplicity', player=who, zone=zone, index=self.rows[name]['csvid'], value=count)

    def test_whole_oracle_and_catalog(self):
        recipe.validate_witnesses(self.oracle, self.rows)
        self.assertEqual({home['csvid'] for home in self.homes.values()}, {4013, 4098, 8190, 9637, 12491, 14009})
        self.assertEqual(sum(len(h['recipes'][0]['variants']) for h in self.homes.values()), 148)
        for name, home in self.homes.items():
            declaration, = home['recipes']
            self.assertEqual(declaration['variants'], recipe.variants(recipe.spec(declaration, self.oracle[name])))
            with self.assertRaises(ValueError):
                recipe.classify(dict(self.oracle[name], oracle_text=declaration['oracle_text'] + '\nDraw a card.'))

    def test_guide_echo_does_not_float_or_repeat_and_protection_is_observable(self):
        name = 'Karmic Guide'
        for who in (0, 1):
            steps = self.actions(name, 'echo_pay', who)
            upkeep = steps.index(dict(op='upkeep_step', player=who))
            untap = steps.index(dict(op='untap_step', player=who))
            self.assertFalse(any(s['op'] == 'set_mana' for s in steps[untap:upkeep]))
            self.assertEqual(sum(s['op'] == 'choose_mana_source' for s in steps[untap:upkeep]), 5 if who == 0 else 0)
            self.assertEqual(steps.count(dict(op='choose_dialog', player=who, value=0)), 1)
            self.assertIn(dict(op='assert_card', ref='land_4', property='tapped', value=1), steps[upkeep:])
            for mode in ('echo_decline', 'echo_short', 'echo_wrong_color'):
                denied = self.actions(name, mode, who)
                self.assertIn(self.copies(name, who, 'graveyard', 1), denied)
                self.assertIn(self.copies('Grizzly Bears', who, 'battlefield', 1), denied)
            protect = self.actions(name, 'target_protection', who)
            self.assertIn(dict(op='can_target_creature', ref='black_spell', other='source', value=0), protect)
            self.assertIn(dict(op='can_target_creature', ref='bounce', other='source', value=1), protect)
            self.assertIn(dict(op='block', ref='blocker', other='source', value=0), self.actions(name, 'protection_black', who))

    def test_snatcher_discard_and_death_are_separate_legal_actions(self):
        name = 'Body Snatcher'
        for who in (0, 1):
            steps = self.actions(name, 'effect', who)
            death = steps.index(dict(op='destroy', ref='source'))
            self.assertIn(self.copies('Memnite', who, 'graveyard', 1), steps[:death])
            self.assertIn(self.copies(name, who, 'battlefield', 1), steps[:death])
            self.assertIn(self.copies(name, who, 'exile', 1), steps[death:])
            self.assertIn(self.copies('Memnite', who, 'battlefield', 1), steps[death:])
            for mode in ('empty', 'wrong_type', 'entry_decline'):
                denied = self.actions(name, mode, who)
                self.assertFalse(any(s['op'] == 'destroy' for s in denied))
                self.assertIn(self.copies(name, who, 'exile', 1), denied)
            self.assertIn(self.copies(name, who, 'graveyard', 1), self.actions(name, 'death_shroud', who))

    def test_crypt_spends_red_and_uses_both_legal_apnap_choices(self):
        name = 'Crypt Champion'
        for who in (0, 1):
            for mode, red in [('effect', 0), ('boundary', 1)]:
                steps = self.actions(name, mode, who)
                cast = steps.index(dict(op='cast', ref='source', value=1))
                pool = [s['value'] for s in steps[:cast] if s['op'] == 'set_mana' and s['player'] == who][-6:]
                self.assertEqual(pool, [3-red,1,0,0,red,0])
                choices = [s['player'] for s in steps if s['op'] == 'choose_zone_card']
                self.assertEqual(choices, [who,1-who])
                self.assertIn(self.copies(name, who, 'battlefield' if red else 'graveyard', 1), steps[cast:])
            damage = self.actions(name, 'double_strike', who)
            hit = damage.index(dict(op='combat_damage'))
            self.assertIn(dict(op='assert_life', player=1-who, value=16), damage[hit:])

    def test_clique_expires_at_controller_end_after_genuine_persist(self):
        name = 'Puppeteer Clique'
        for who in (0, 1):
            steps = self.actions(name, 'persist', who)
            deaths = [i for i, step in enumerate(steps) if step == dict(op='destroy', ref='source')]
            self.assertEqual(len(deaths), 2)
            self.assertIn(self.copies('Grizzly Bears', 1-who, 'exile', 1), steps[:deaths[0]])
            self.assertIn(dict(op='assert_card', ref='source', property='minus1_counters', value=1), steps[deaths[0]:deaths[1]])
            cleanup = steps.index(dict(op='cleanup'))
            attack = steps.index(dict(op='attack', ref='borrowed', value=1))
            self.assertLess(deaths[1], attack)
            self.assertLess(attack, cleanup)
            self.assertIn(self.copies('Hill Giant', who, 'battlefield', 1), steps[deaths[1]:cleanup])
            self.assertIn(self.copies('Hill Giant', 1-who, 'exile', 1), steps[cleanup:])
            returned = self.actions(name, 'borrowed_bounced', who)
            self.assertIn(self.copies('Grizzly Bears', 1-who, 'hand', 1), returned)

    def test_pit_dweller_uses_true_entry_origin_ownership_and_counter(self):
        name = 'Treacherous Pit-Dweller'
        for who in (0, 1):
            steps = self.actions(name, 'twice', who)
            deaths = [i for i, step in enumerate(steps) if step == dict(op='destroy', ref='source')]
            self.assertEqual(len(deaths), 2)
            self.assertIn(self.copies(name, who, 'battlefield', 1), steps[:deaths[0]])
            self.assertIn(self.copies(name, 1-who, 'battlefield', 1), steps[deaths[0]:deaths[1]])
            self.assertIn(dict(op='assert_card', ref='source', property='plus1_counters', value=1), steps[deaths[0]:deaths[1]])
            self.assertIn(self.copies(name, who, 'graveyard', 1), steps[deaths[1]:])
            revived = self.actions(name, 'from_grave', who)
            self.assertIn(dict(op='cast', ref='return_spell', value=1), revived)
            self.assertIn(self.copies(name, 1-who, 'battlefield', 1), revived)
            self.assertIn(dict(op='assert_card', ref='source', property='plus1_counters', value=0), revived)
            self.assertIn(self.copies(name, who, 'battlefield', 1), self.actions(name, 'hexproof', who))

    def test_magister_source_first_ballots_and_both_outcomes(self):
        name = 'Magister of Worth'
        for who in (0, 1):
            for mode, ballots in [('effect', [0,0]), ('condemn', [1,1]), ('tie_owner_grace', [0,1]), ('tie_enemy_grace', [1,0])]:
                steps = self.actions(name, mode, who)
                actual = [(s['player'],s['value']) for s in steps if s['op']=='choose_dialog']
                self.assertEqual(actual, list(zip((who,1-who), ballots)))
                if sum(ballots):
                    self.assertIn(self.copies('Darksteel Myr', 1-who, 'battlefield', 1), steps)
                    self.assertIn(self.copies('Spellbook', who, 'battlefield', 1), steps)
                else:
                    for seat in (who, 1-who):
                        self.assertIn(self.copies('Hill Giant', seat, 'battlefield', 1), steps)
                        self.assertIn(self.copies('Reach Through Mists', seat, 'graveyard', 1), steps)
                self.assertIn(self.copies(name, who, 'battlefield', 1), steps)

    def test_mirrored_replays_restore_identical_real_steps(self):
        for name in self.homes:
            for who in (0, 1):
                steps = self.actions(name, 'replay', who)
                cut = steps.index(dict(op='capture'))
                end = steps.index(dict(op='record_state'))
                restore = steps.index(dict(op='restore'))
                self.assertEqual(steps[cut+1:end], steps[restore+1:-1])
                self.assertIn(dict(op='cast', ref='source', value=1), steps[cut+1:end])


if __name__ == '__main__':
    unittest.main()
