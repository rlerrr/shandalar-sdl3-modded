import json
import unittest
from pathlib import Path

from generate_cases import registry
from phase_untap_creature_recipes import FAMILY, classify, expand, profiles, spec, validate_witnesses, variants


class PhaseUntapCreatureRecipesTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.oracle = json.loads((Path(__file__).parent / 'data/oracle.json').read_text(encoding='utf8'))['cards']
        cls.rows = {row['name']: row for row in registry()}

    def declaration(self, name, selected):
        record = self.oracle[name]
        return dict(family=FAMILY, version=2, params=classify(record), oracle_text=record['oracle_text'], variants=selected)

    def case(self, name, mode, owner=0):
        selected = [f'{mode}_{owner}']
        declaration = self.declaration(name, selected)
        case, = expand(name, self.rows[name]['csvid'], declaration, spec(declaration, self.oracle[name]),
                       selected, self.rows, self.oracle)
        self.assertTrue(all(not obj.get('state') for obj in case['setup']))
        return case

    def values(self, entries, op, **fields):
        return [step['value'] for step in entries if step['op'] == op and all(step.get(key) == value for key, value in fields.items())]

    def untaps(self, case, player):
        actions = case['steps']
        points = [i for i, step in enumerate(actions) if step['op'] == 'untap_step' and step['player'] == player]
        segments = []
        for untap in points:
            upkeep = next(i for i in range(untap + 1, len(actions)) if actions[i]['op'] == 'upkeep_step')
            main = next(i for i in range(upkeep + 1, len(actions)) if actions[i]['op'] == 'turn')
            segments.append((actions[untap + 1:upkeep], actions[upkeep + 1:main]))
        return segments

    def test_pinned_whole_source_and_companions_fail_closed(self):
        validate_witnesses(self.oracle, self.rows)
        names = profiles()['sources']
        self.assertEqual({self.rows[name]['csvid'] for name in names},
                         {2677, 2629, 3055, 2592, 2630, 2540, 2578, 3065, 2456, 3048})
        for name in names:
            params = classify(self.oracle[name])
            decl = self.declaration(name, variants(params))
            self.assertEqual(spec(decl, self.oracle[name])['mana'], params['mana'])
            with self.assertRaises(ValueError):
                wrong_mana = [float(params['mana'][0]), *params['mana'][1:]]
                spec(dict(decl, params=dict(params, mana=wrong_mana)), self.oracle[name])
        for name in ("Ertai's Familiar", 'Shimmering Efreet', 'Taniwha', "Teferi's Isle", 'Warping Wurm'):
            with self.assertRaises(ValueError):
                classify(self.oracle[name])

    def test_own_step_toggles_original_card_and_other_step_never_toggles_it(self):
        for owner in (0, 1):
            case = self.case('Sandbar Crocodile', 'cycle', owner)
            for player, expected in ((owner, [0, 1]), (1-owner, [1, 0])):
                actual = [self.values(before, 'assert_card', ref='source', property='in_play')[0]
                          for before, _ in self.untaps(case, player)]
                self.assertEqual(actual, expected)
            for before, _ in self.untaps(case, owner):
                self.assertEqual(self.values(before, 'assert_card', ref='source', property='csvid'), [2592])
                self.assertEqual(self.values(before, 'assert_zone_multiplicity', player=owner,
                                             index=2592, zone='battlefield'), [1])
                for zone in ('hand', 'graveyard', 'exile'):
                    self.assertEqual(self.values(before, 'assert_zone_multiplicity', player=owner, index=2592, zone=zone), [0])

    def test_real_tap_stays_phased_then_phases_in_before_untapping(self):
        for owner in (0, 1):
            case = self.case('Tolarian Drake', 'untap_order', owner)
            self.assertEqual(self.values(case['steps'], 'activate', ref='icy'), [1])
            after = [pre for pre, _ in self.untaps(case, owner)]
            self.assertEqual([self.values(pre, 'assert_card', ref='source', property='tapped') for pre in after], [[1], [0]])
            self.assertEqual([self.values(pre, 'assert_card', ref='icy', property='tapped') for pre in after], [[0], [0]])

    def test_real_counters_and_both_attachments_survive_hidden_state(self):
        case = self.case('Tolarian Drake', 'retained_attachments', 0)
        for card in ('aura', 'equipment', 'burst'):
            self.assertEqual(self.values(case['steps'], 'cast', ref=card), [1])
        self.assertEqual(self.values(case['steps'], 'activate', ref='equipment'), [1])
        before = [pre for pre, _ in self.untaps(case, 0)]
        for ref in ('aura', 'equipment'):
            self.assertEqual([self.values(pre, 'assert_card', ref=ref, property='in_play') for pre in before], [[0], [1]])
        self.assertEqual(self.values(before[1], 'assert_card', ref='source', property='plus1_counters'), [1])
        self.assertEqual(self.values(before[1], 'assert_card', ref='source', property='power'), [6])
        self.assertEqual(self.values(before[1], 'assert_card', ref='source', property='toughness'), [7])

    def test_foreign_aura_returns_with_host_on_hosts_untap(self):
        for owner in (0, 1):
            case = self.case("Teferi's Drake", 'foreign_aura', owner)
            aura, = [obj for obj in case['setup'] if obj['ref'] == 'aura']
            self.assertEqual(aura['player'], 1-owner)
            self.assertEqual(self.values(self.untaps(case, 1-owner)[-1][0], 'assert_card', ref='aura', property='in_play'), [0])
            phased_out, returned = [pre for pre, _ in self.untaps(case, owner)]
            self.assertEqual(self.values(phased_out, 'assert_card', ref='aura', property='in_play'), [0])
            self.assertEqual(self.values(returned, 'assert_card', ref='aura', property='in_play'), [1])
            self.assertEqual(self.values(returned, 'assert_card', ref='source', property='power'), [4])

    def test_real_stasis_blocks_both_phasing_directions_until_destroyed(self):
        for mode, expected in (('skip_out', [1, 0]), ('skip_in', [0, 0, 1])):
            case = self.case('Sandbar Crocodile', mode, 0)
            self.assertEqual(self.values(case['steps'], 'cast', ref='stasis'), [1])
            self.assertEqual(self.values(case['steps'], 'cast', ref='naturalize'), [1])
            self.assertEqual([self.values(pre, 'assert_card', ref='source', property='in_play')[0]
                              for pre, _ in self.untaps(case, 0)], expected)
            skipped = self.untaps(case, 0)[-2][0]
            self.assertEqual(self.values(skipped, 'assert_card', ref='source', property='tapped'), [1])

    def test_actual_target_and_block_legality_follow_visibility(self):
        for owner in (0, 1):
            case = self.case('Breezekeeper', 'target_visibility', owner)
            self.assertEqual(self.values(case['steps'], 'can_target_creature', ref='murder', other='source'), [1, 0, 1])
            case = self.case('Breezekeeper', 'block_while_out', owner)
            self.assertEqual(self.values(case['steps'], 'block', ref='source', other='opposing'), [0])
            self.assertEqual(self.values(case['steps'], 'assert_life', player=owner)[-1], 19)

    def test_no_etb_or_death_but_real_witnesses_observe_paid_casts_and_death(self):
        case = self.case('Sandbar Crocodile', 'no_zone_change_triggers', 0)
        self.assertEqual(self.values(case['steps'], 'cast', ref='warden'), [1])
        self.assertEqual(self.values(case['steps'], 'cast', ref='staff'), [1])
        self.assertEqual(self.values(case['steps'], 'cast', ref='murder'), [1])
        for before, after in self.untaps(case, 0):
            self.assertEqual(self.values(before+after, 'assert_life', player=0), [21, 21])
        self.assertEqual(self.values(case['steps'], 'assert_life', player=0)[-1], 19)

    def test_flying_and_islandwalk_check_real_defender_and_island_controller(self):
        for owner in (0, 1):
            for name, mode, blocked, remaining in (('Breezekeeper', 'flying_ground', 0, 16),
                                                    ('Breezekeeper', 'flying_reach', 1, 20),
                                                    ('Merfolk Raiders', 'islandwalk_enabled', 0, 18),
                                                    ('Merfolk Raiders', 'islandwalk_own_island', 1, 20)):
                case = self.case(name, mode, owner)
                self.assertEqual(self.values(case['steps'], 'attack', ref='source'), [1])
                self.assertEqual(self.values(case['steps'], 'block', ref='opposing', other='source'), [blocked])
                self.assertEqual(self.values(case['steps'], 'assert_life', player=1-owner)[-1], remaining)

    def test_imp_discard_and_draw_are_held_until_upkeep_and_exact(self):
        for owner in (0, 1):
            case = self.case("Teferi's Imp", 'imp_timing', owner)
            intervals = self.untaps(case, owner)
            self.assertEqual(len(intervals), 4)
            for i, (before, after) in enumerate(intervals):
                if i % 2 == 0:
                    discarded = 163 if i == 0 else 91
                    self.assertEqual(self.values(before, 'assert_zone_multiplicity',
                                                 player=owner, index=discarded, zone='graveyard'), [0])
                    self.assertEqual(self.values(after, 'assert_zone_multiplicity',
                                                 player=owner, index=discarded, zone='graveyard'), [1])
                else:
                    self.assertEqual(self.values(before, 'assert_zone', player=owner, zone='library'), [3-i//2])
                    self.assertEqual(self.values(after, 'assert_zone', player=owner, zone='library'), [2-i//2])
        choice = self.case("Teferi's Imp", 'imp_choice', 0)
        self.assertEqual(self.values(choice['steps'], 'discard_policy'), [1])
        self.assertEqual(self.values(choice['steps'], 'assert_zone_multiplicity', player=0, index=188, zone='hand')[-1], 1)
        empty = self.case("Teferi's Imp", 'imp_empty_hand', 0)
        self.assertFalse(any(obj['ref'] == 'discard' for obj in empty['setup']))

    def test_payment_denials_and_snapshot_have_real_boundaries(self):
        for mode in ('wrong_color', 'insufficient_mana'):
            case = self.case('Merfolk Raiders', mode)
            self.assertEqual(self.values(case['steps'], 'cast', ref='source'), [0])
            self.assertEqual(self.values(case['steps'], 'assert_card', ref='source', property='in_hand'), [1])
        case = self.case('Merfolk Raiders', 'countered')
        self.assertEqual(self.values(case['steps'], 'cast_response', ref='counter'), [1])
        self.assertEqual(self.values(case['steps'], 'assert_zone_multiplicity', player=0, index=2540, zone='graveyard')[-1], 1)
        case = self.case('Merfolk Raiders', 'replay_phase_in')
        actions = case['steps']
        names = [step['op'] for step in actions]
        capture, record, restore = map(names.index, ('capture', 'record_state', 'restore'))
        self.assertEqual(names[capture+1], 'untap_step')
        self.assertEqual(actions[capture+1:record], actions[restore+1:-1])

    def test_ripple_phases_three_legal_permanent_types_after_paid_spell(self):
        for owner in (0, 1):
            for mode, witness in (('opponent_creature', 'Grizzly Bears'), ('opponent_artifact', 'Icy Manipulator'),
                                  ('opponent_land', 'Island')):
                case = self.case('Reality Ripple', mode, owner)
                target, = [entry for entry in case['setup'] if entry['ref'] == 'target']
                self.assertEqual((target['name'], target['player']), (witness, 1-owner))
                self.assertEqual(self.values(case['steps'], 'cast', ref='source'), [1])
                self.assertIn(0, self.values(case['steps'], 'assert_card', ref='target', property='in_play'))
                self.assertEqual(self.values(case['steps'], 'assert_card', ref='target', property='in_play')[-1], 1)
            denied = self.case('Reality Ripple', 'only_enchantment', owner)
            self.assertEqual(self.values(denied['steps'], 'cast', ref='source'), [0])
            self.assertEqual(sum(self.values(denied['steps'], 'assert_mana', player=owner)), 2)
            countered = self.case('Reality Ripple', 'lost_target', owner)
            self.assertEqual(self.values(countered['steps'], 'cast_response', ref='answer'), [1])
            self.assertEqual(self.values(countered['steps'], 'assert_zone_multiplicity', player=1-owner,
                                         zone='graveyard', index=106)[-1], 1)

    def test_ripple_attachment_and_untap_retain_same_recipient(self):
        for owner in (0, 1):
            case = self.case('Reality Ripple', 'carry_aura', owner)
            steps = case['steps']
            self.assertEqual(self.values(steps, 'activate', ref='icy'), [1])
            self.assertIn(0, self.values(steps, 'assert_card', ref='aura', property='in_play'))
            self.assertEqual(self.values(steps, 'assert_card', ref='aura', property='in_play')[-1], 1)
            self.assertEqual(self.values(steps, 'assert_card', ref='target', property='power')[-1], 3)
            self.assertEqual(self.values(steps, 'assert_card', ref='target', property='tapped')[-1], 0)

    def test_illusionist_pays_two_blue_taps_and_rejects_fresh_or_noncreature(self):
        for owner in (0, 1):
            fresh = self.case('Vodalian Illusionist', 'fresh', owner)
            self.assertEqual(self.values(fresh['steps'], 'activate', ref='source'), [0])
            self.assertEqual(sum(self.values(fresh['steps'], 'assert_mana', player=owner)), 2)
            probe = self.case('Vodalian Illusionist', 'noncreature_filter', owner)
            self.assertEqual(self.values(probe['steps'], 'probe_activation_target', ref='source', other='artifact'), [0])
            actual = self.case('Vodalian Illusionist', 'self_target', owner)
            self.assertEqual(self.values(actual['steps'], 'activate', ref='source'), [1])
            self.assertEqual(self.values(actual['steps'], 'assert_card', ref='source', property='tapped')[-1], 0)
            self.assertIn(0, self.values(actual['steps'], 'assert_card', ref='source', property='in_play')[1:-1])

    def test_dream_fighter_uses_real_block_boundaries_and_independent_untaps(self):
        for owner in (0, 1):
            for mode, blocker, attacker in (('blocks','source','opposing'), ('blocked','opposing','source')):
                case = self.case('Dream Fighter', mode, owner)
                self.assertEqual(self.values(case['steps'], 'block', ref=blocker, other=attacker), [1])
                names = [s['op'] for s in case['steps']]
                advance, damage = names.index('advance_combat'), names.index('combat_damage')
                between = case['steps'][advance+1:damage]
                for ref in ('source','opposing'):
                    self.assertEqual(self.values(between, 'assert_card', ref=ref, property='in_play'), [0])
                self.assertEqual(self.values(case['steps'], 'assert_life', player=owner)[-1], 20)
                self.assertEqual(self.values(case['steps'], 'assert_life', player=1-owner)[-1], 20)
                self.assertEqual(self.values(case['steps'], 'assert_card', ref='source', property='in_play')[-1], 1)

    def test_veil_effect_uses_actual_damage_and_survives_losing_enchantment(self):
        for owner in (0, 1):
            name = "Teferi's Veil"
            for mode, expected in (('own_attack', [1, 0]), ('veil_removed_pending', [1, 0]), ('opponent_attack', [1, 1])):
                case = self.case(name, mode, owner)
                ops = [s['op'] for s in case['steps']]
                damage, end = ops.index('combat_damage'), ops.index('end_combat')
                self.assertEqual(self.values(case['steps'][damage:end], 'assert_card', ref='first', property='in_play'), expected[:1])
                untap = ops.index('advance_turn', end)
                self.assertEqual(self.values(case['steps'][end:untap], 'assert_card', ref='first', property='in_play'), expected[1:])
            removed = self.case(name, 'veil_removed_pending', owner)
            self.assertEqual(self.values(removed['steps'], 'cast', ref='answer'), [1])
            dead = self.case(name, 'attacker_dies', owner)
            self.assertEqual(self.values(dead['steps'], 'assert_zone_multiplicity', player=owner, zone='graveyard', index=106)[-1], 1)
