"""Check whole Oracle and genuine reanimation action boundaries."""

import json
from pathlib import Path
import unittest

from generate_cases import registry
import graveyard_reanimation_permissions_recipes as model


DUEL = Path(__file__).resolve().parent
ORACLE = DUEL / 'data/oracle.json'


class GraveyardReanimationPermissionsRecipes(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.oracle = json.loads(ORACLE.read_text(encoding='utf8'))['cards']
        cls.rows = {row['name']: row for row in registry()}
        cls.homes = {}
        for name in model.profile()['sources']:
            csvid = cls.rows[name]['csvid']
            path, = (DUEL / f'data/cases/cards/{csvid // 100:03d}').glob(f'{csvid:05d}-*.json')
            cls.homes[name] = json.loads(path.read_text(encoding='utf8'))

    def case(self, name, mode, who):
        home = self.homes[name]
        declaration, = home['recipes']
        params = model.spec(declaration, self.oracle[name])
        cases = model.expand(name, home['csvid'], declaration, params, [f'{mode}_{who}'], self.rows, self.oracle)
        return cases[0]['steps']

    def copies(self, name, who, zone, amount):
        return dict(op='assert_zone_multiplicity', player=who, zone=zone, index=self.rows[name]['csvid'], value=amount)

    def test_entire_oracle_and_catalog(self):
        model.validate_witnesses(self.oracle, self.rows)
        self.assertEqual({home['csvid'] for home in self.homes.values()}, {8042, 9317, 15225, 18273})
        self.assertEqual(sum(len(home['recipes'][0]['variants']) for home in self.homes.values()), 120)
        for name, home in self.homes.items():
            declaration, = home['recipes']
            self.assertEqual(declaration['variants'], model.variants(model.spec(declaration, self.oracle[name])))
            with self.assertRaises(ValueError):
                model.classify(dict(self.oracle[name], oracle_text=self.oracle[name]['oracle_text'] + '\nDraw a card.'))

    def test_knell_four_hybrid_splits_and_denial(self):
        name = "Debtors' Knell"
        for who in (0, 1):
            for branch, black in [('effect', 3), ('mixed_two_black', 2), ('mixed_one_black', 1), ('white', 0)]:
                actions = self.case(name, branch, who)
                cast = actions.index(dict(op='cast', ref='source', value=1))
                pool = [s['value'] for s in actions[:cast] if s['op'] == 'set_mana' and s['player'] == who][-6:]
                self.assertEqual(pool, [4, black, 0, 0, 0, 3 - black])
                self.assertIn(self.copies('Hill Giant', who, 'battlefield', 1), actions[cast:])
            for branch in ('underpaid', 'wrong_color'):
                self.assertIn(dict(op='cast', ref='source', value=0), self.case(name, branch, who))

    def test_knell_separates_origin_controller_and_real_owner(self):
        name = "Debtors' Knell"
        for who in (0, 1):
            actions = self.case(name, 'other_and_own', who)
            who_choice = actions.index(dict(op='choose_player', player=who, value=1-who))
            card_choice = actions.index(dict(op='choose_zone_card', player=who, value=self.rows['Hill Giant']['csvid']))
            upkeep = actions.index(dict(op='upkeep_step', player=who))
            self.assertLess(who_choice, card_choice)
            self.assertLess(card_choice, upkeep)
            self.assertIn(self.copies('Grizzly Bears', who, 'graveyard', 1), actions[upkeep:])
            actions = self.case(name, 'owner_death', who)
            murder = actions.index(dict(op='cast', ref='reply', value=1))
            self.assertIn(self.copies('Hill Giant', who, 'battlefield', 1), actions[:murder])
            self.assertIn(self.copies('Hill Giant', 1-who, 'graveyard', 1), actions[murder:])
            self.assertIn(self.copies('Hill Giant', who, 'graveyard', 0), actions[murder:])

    def test_wort_kindred_and_fear(self):
        name = 'Wort, Boggart Auntie'
        for who in (0, 1):
            actions = self.case(name, 'kindred', who)
            upkeep = actions.index(dict(op='upkeep_step', player=who))
            self.assertIn(self.copies('Tarfire', who, 'hand', 1), actions[upkeep:])
            self.assertIn(self.copies('Grizzly Bears', who, 'graveyard', 1), actions[upkeep:])
            for branch, legal in [('fear_ground', 0), ('fear_black', 1), ('fear_artifact', 1)]:
                self.assertIn(dict(op='block', ref='blocker', other='source', value=legal), self.case(name, branch, who))
            decline = self.case(name, 'decline', who)
            self.assertTrue(any(s['op'] == 'choose_optional_zone' for s in decline))

    def test_knell_repeat_observes_both_creatures_before_actual_owner_bounce(self):
        for who in (0, 1):
            actions = self.case("Debtors' Knell", 'repeat', who)
            upkeeps = [i for i, s in enumerate(actions) if s == dict(op='upkeep_step', player=who)]
            self.assertEqual(len(upkeeps), 2)
            bounce = actions.index(dict(op='cast', ref='reply', value=1))
            self.assertIn(dict(op='choose_target', player=1-who, ref='raised'), actions[upkeeps[-1]:bounce])
            for name in ('Hill Giant', 'Grizzly Bears'):
                self.assertIn(self.copies(name, who, 'battlefield', 1), actions[upkeeps[-1]:bounce])
            self.assertIn(self.copies('Hill Giant', who, 'battlefield', 0), actions[bounce:])
            self.assertIn(self.copies('Hill Giant', 1-who, 'hand', 1), actions[bounce:])
            self.assertIn(self.copies('Grizzly Bears', who, 'battlefield', 1), actions[bounce:])

    def test_starfield_animates_exact_other_nonauras_at_five(self):
        name = 'Starfield of Nyx'
        for who in (0, 1):
            for mode in ('four', 'five', 'six', 'threshold_rise', 'threshold_fall'):
                checks = [s for s in self.case(name, mode, who) if s['op'] == 'can_target_creature']
                own = [s['value'] for s in checks if s['other'] == 'enchantment_0']
                self.assertEqual(own, {'four': [0], 'five': [1], 'six': [1], 'threshold_rise': [0, 1],
                                       'threshold_fall': [1, 0]}[mode])
                self.assertTrue(all(s['value'] == 0 for s in checks
                                    if s['other'] in ('aura', 'source', 'enemy_enchantment')))
            actions = self.case(name, 'threshold_rise', who)
            upkeep = actions.index(dict(op='upkeep_step', player=who))
            for ref, mv in [('enchantment_0', 3), ('enchantment_1', 4), ('animated_return', 4)]:
                for prop in ('power', 'toughness'):
                    self.assertIn(dict(op='assert_card', ref=ref, property=prop, value=mv), actions[upkeep:])

    def test_phoenix_creates_real_named_token_and_repeats(self):
        name = 'Rekindling Phoenix'
        for who in (0, 1):
            actions = self.case(name, 'repeat', who)
            deaths = [i for i, s in enumerate(actions) if s['op'] == 'destroy' and s['ref'] == 'source']
            upkeeps = [i for i, s in enumerate(actions) if s == dict(op='upkeep_step', player=who)]
            self.assertEqual(len(deaths), 2)
            self.assertLess(deaths[0], upkeeps[0])
            self.assertLess(upkeeps[0], deaths[1])
            self.assertLess(deaths[1], upkeeps[1])
            for start, end in zip(deaths, upkeeps):
                self.assertIn(self.copies(name, who, 'graveyard', 1), actions[start:end])
                self.assertIn(dict(op='assert_matching_permanents', player=who, index=18726,
                                   property='subtype_Elemental', value=1), actions[start:end])
            self.assertEqual(actions.count(dict(op='attack', ref='source', value=1)), 2)
            self.assertIn(dict(op='assert_life', player=1-who, value=12), actions[upkeeps[-1]:])

    def test_phoenix_checks_name_grave_ownership_flying_and_haste_end(self):
        name = 'Rekindling Phoenix'
        for who in (0, 1):
            wrong = self.case(name, 'wrong_owner', who)
            upkeep = wrong.index(dict(op='upkeep_step', player=who))
            self.assertIn(self.copies(name, 1-who, 'graveyard', 1), wrong[upkeep:])
            self.assertIn(self.copies('Elemental', who, 'battlefield', 1), wrong[upkeep:])
            missing = self.case(name, 'missing_name', who)
            self.assertIn(dict(op='activate', ref='crypt', value=1), missing)
            self.assertIn(self.copies(name, who, 'exile', 1), missing)
            for mode, allowed in [('flying_ground', 0), ('flying_reach', 1)]:
                self.assertIn(dict(op='block', ref='blocker', other='source', value=allowed), self.case(name, mode, who))
            expiry = self.case(name, 'haste_expires', who)
            change = expiry.index(dict(op='cast', ref='theft', value=1))
            self.assertIn(dict(op='bind_permanent_copy', player=1-who, ref='source', value=18273, index=0), expiry[change:])
            self.assertIn(dict(op='attack', ref='source', value=0), expiry[change:])

    def test_every_replay_repeats_same_real_upkeep(self):
        for name in self.homes:
            for who in (0, 1):
                actions = self.case(name, 'replay', who)
                cut = actions.index(dict(op='capture'))
                record = actions.index(dict(op='record_state'))
                restore = actions.index(dict(op='restore'))
                self.assertEqual(actions[cut+1:record], actions[restore+1:-1])
                self.assertIn(dict(op='upkeep_step', player=who), actions[cut+1:record])

    def test_human_optional_trigger_selection_precedes_legal_grave_menu(self):
        for name, positive in [('Wort, Boggart Auntie', 'kindred'), ('Starfield of Nyx', 'threshold_rise')]:
            for who in (0, 1):
                for mode, menu in [(positive, 'choose_zone_card'), ('decline', 'choose_optional_zone')]:
                    actions = self.case(name, mode, who)
                    intent = dict(op='choose_trigger_source', player=who, ref='source')
                    self.assertEqual(actions.count(intent), int(who == 0))
                    if who == 0:
                        self.assertEqual(actions[actions.index(intent) + 1]['op'], menu)
            wrong = self.case(name, 'wrong_type', 0)
            self.assertFalse(any(s['op'] == 'choose_trigger_source' for s in wrong))


if __name__ == '__main__':
    unittest.main()
