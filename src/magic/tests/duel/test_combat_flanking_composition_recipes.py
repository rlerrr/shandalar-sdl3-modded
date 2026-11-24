"""Regression guards for full Oracle and distinct Flanking declarations."""

import json
import unittest
from pathlib import Path

import combat_flanking_composition_recipes as model
from generate_cases import registry

DATA = Path(__file__).with_name('data')


class CombatFlankingCompositionTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.oracle = json.loads((DATA / 'oracle.json').read_text(encoding='utf8'))['cards']
        cls.rows = {row['name']: row for row in registry()}
        cls.sources = model.profiles()['sources']

    def case(self, name, variant):
        pin = self.sources[name]
        declaration = dict(family=model.FAMILY, version=2, params=pin['params'], oracle_text=pin['oracle']['oracle_text'], variants=[variant])
        params = model.spec(declaration, self.oracle[name])
        return model.expand(name, pin['registry']['csvid'], declaration, params, [variant], self.rows, self.oracle)[0]['steps']

    @staticmethod
    def declared(steps):
        start = next(index for index, step in enumerate(steps) if step['op'] == 'advance_combat')
        stop = next((index for index, step in enumerate(steps) if step['op'] == 'combat_damage'), len(steps))
        return steps[start:stop]

    def test_full_pinned_cards_and_unimplemented_exclusion(self):
        model.validate_witnesses(self.oracle, self.rows)
        self.assertEqual({pin['registry']['csvid'] for pin in self.sources.values()}, {2702, 2515, 8665, 8497, 2995, 2610})
        for name, pin in self.sources.items():
            with self.subTest(name=name):
                self.assertEqual(pin['oracle'], self.oracle[name])
                with self.assertRaises(ValueError):
                    model.parameters(dict(self.oracle[name], oracle_text=pin['oracle']['oracle_text'] + '\nVigilance'))

    def test_dead_blocker_leaves_attack_blocked_and_no_normal_damage_to_player(self):
        steps = self.case('Fallen Askari', 'lethal_declaration_0')
        declaration = self.declared(steps)
        self.assertIn(dict(op='assert_card', ref='blocker', property='in_play', value=0), declaration)
        self.assertIn(dict(op='assert_card', ref='source', property='blocked', value=1), declaration)
        self.assertTrue(all(step['value'] == 20 for step in steps if step['op'] == 'assert_life'))
        defending = self.case('Fallen Askari', 'defending_0')
        self.assertIn(dict(op='block', ref='source', other='attacker', value=0), defending)

    def test_friendly_and_enemy_cavalry_and_sliver_scopes(self):
        expected = {'normal_block': 1, 'friendly_native': 0, 'enemy_native': 1, 'ordinary_ally': 2}
        for variant, toughness in expected.items():
            with self.subTest(cavalry=variant):
                steps = self.declared(self.case('Cavalry Master', variant + '_0'))
                receipt = dict(op='assert_card', ref='blocker', property='in_play' if not toughness else 'toughness', value=toughness)
                self.assertIn(receipt, steps)
        for variant, toughness in {'friendly_sliver': 1, 'enemy_sliver': 1, 'non_sliver': 2, 'sliver_blocker': 1}.items():
            with self.subTest(sliver=variant):
                steps = self.declared(self.case('Sidewinder Sliver', variant + '_1'))
                self.assertIn(dict(op='assert_card', ref='blocker', property='toughness', value=toughness), steps)

    def test_native_and_granted_flanking_blockers_avoid_penalty(self):
        for name, variant in (("Jolrael's Centaur", 'flanking_blocker_0'), ("Jabari's Banner", 'grant_defender_1')):
            steps = self.declared(self.case(name, variant))
            self.assertIn(dict(op='assert_card', ref='blocker', property='toughness', value=2), steps)
        shroud = self.case("Jolrael's Centaur", 'shroud_targets_1')
        for spell in ('friendly_spell', 'enemy_spell'):
            self.assertIn(dict(op='can_target_creature', ref=spell, other='source', value=0), shroud)
            self.assertIn(dict(op='can_target_creature', ref=spell, other='blocker', value=1), shroud)

    def test_banner_repeats_real_cost_after_twiddle_and_replays_pending_grant(self):
        steps = self.case("Jabari's Banner", 'grant_repeat_0')
        payments = [index for index, step in enumerate(steps) if step == dict(op='activate', ref='source', value=1)]
        untap = steps.index(dict(op='cast', ref='untap_banner', value=1))
        self.assertEqual(len(payments), 2)
        self.assertLess(payments[0], untap)
        self.assertLess(untap, payments[1])
        self.assertIn(dict(op='assert_mana_total', player=0, value=57), steps[payments[1]:])
        self.assertIn(dict(op='assert_card', ref='blocker', property='toughness', value=1), self.declared(steps))
        replay = self.case("Jabari's Banner", 'grant_pending_snapshot_1')
        cut = replay.index(dict(op='capture'))
        self.assertEqual(replay[cut + 1], dict(op='resolve'))
        self.assertFalse(any(step['op'] == 'activate' for step in replay[cut:]))
        self.assertIn(dict(op='restore'), replay[cut:])

    def test_sidar_taps_before_blocking_and_real_legend_choice_after_cast(self):
        steps = self.case('Sidar Jabari', 'tap_only_blocker_1')
        selected = steps.index(dict(op='choose_target', player=1, ref='blocker'))
        self.assertEqual(steps[selected + 1], dict(op='finish_attackers'))
        self.assertEqual(steps[selected + 2], dict(op='assert_card', ref='blocker', property='tapped', value=1))
        self.assertIn(dict(op='block', ref='blocker', other='source', value=0), steps)
        legend = self.case('Sidar Jabari', 'legend_rule_0')
        cast = legend.index(dict(op='cast', ref='source', value=1))
        choose = legend.index(dict(op='choose_target', player=0, ref='other_legend'))
        self.assertLess(cast, choose)
        self.assertEqual(legend[choose + 1], dict(op='resolve'))


if __name__ == '__main__':
    unittest.main()
