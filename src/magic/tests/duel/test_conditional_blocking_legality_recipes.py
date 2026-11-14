"""Oracle predicates, real actions, exact ledgers and replay boundary guards."""
import copy
import hashlib
import json
import unittest
from pathlib import Path
from unittest.mock import patch

from generate_cases import OPS, PROPERTIES, registry
import conditional_blocking_legality_recipes as model


class ConditionalBlockingLegalityTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.oracle = json.loads((Path(__file__).parent / 'data/oracle.json').read_text(encoding='utf8'))['cards']
        cls.rows = {r['name']: r for r in registry()}
        cls.cases = {}
        for name, source in model.profiles()['sources'].items():
            d = dict(family=model.FAMILY, version=2, oracle_text=source['oracle']['oracle_text'], params=source['params'], variants=model.variants(source['params']))
            p = model.spec(d, cls.oracle[name])
            cls.cases[name] = {c['_recipe']['variant']: c for c in model.expand(name, source['csvid'], d, p, d['variants'], cls.rows, cls.oracle)}

    def case(self, name, variant): return self.cases[name][variant]
    def steps(self, name, variant): return self.case(name, variant)['steps']
    def has(self, name, variant, op, **kw): self.assertIn(dict(op=op, **kw), self.steps(name, variant))
    def block(self, name, variant): return next(s['value'] for s in self.steps(name, variant) if s['op'] == 'block')
    def final(self, name, variant, op, **kw):
        return next(s['value'] for s in reversed(self.steps(name, variant)) if s['op'] == op and all(s.get(k) == v for k, v in kw.items()))

    def test_exact_eight_primaries_whole_pinned_oracle(self):
        self.assertEqual({s['csvid'] for s in model.profiles()['sources'].values() if s['csvid'] in model.PILOT_IDS}, {8476, 622, 4566, 16761, 12507, 6742, 13711, 4804})
        self.assertEqual(sum(len(cases) for name, cases in self.cases.items() if self.rows[name]['csvid'] in model.PILOT_IDS), 384)
        self.assertEqual(hashlib.sha256((Path(__file__).parent / 'data/oracle.json').read_bytes()).hexdigest(), 'c11dcfedde2ed4ce99ff355f5cf30c187c2b3745c87a03319e715bbfec17cf50')
        for name, source in model.profiles()['sources'].items():
            self.assertEqual(source['oracle'], self.oracle[name]); self.assertEqual(source['params'], model.parameters(self.oracle[name]))

    def test_whole_text_layout_keyword_and_stats_fail_closed(self):
        for name in self.cases:
            for change in (dict(layout='transform'), dict(card_faces=[{}]), dict(power='*'), dict(keywords=['Flying']),
                           dict(oracle_text=self.oracle[name]['oracle_text'] + '\nDraw a card.'),
                           dict(oracle_text=self.oracle[name]['oracle_text'] + '\n(Draw a card.)')):
                with self.assertRaises(ValueError): model.parameters(dict(self.oracle[name], **change))

    def test_strict_declaration_version_types_parameters_and_variants(self):
        name = 'Outland Boar'; source = model.profiles()['sources'][name]
        d = dict(family=model.FAMILY, version=2, params=source['params'], oracle_text=source['oracle']['oracle_text'], variants=['allowed_block_0'])
        for version in (True, '2', 2.0, 1):
            with self.assertRaises(ValueError): model.spec(dict(d, version=version), self.oracle[name])
        for vs in ([], ['allowed_block_0']*2, ['unknown_0']):
            with self.assertRaises(ValueError): model.spec(dict(d, variants=vs), self.oracle[name])
        changed = copy.deepcopy(d); changed['params']['limit'] = 2.0
        with self.assertRaises(ValueError): model.spec(changed, self.oracle[name])
        with self.assertRaises(ValueError): model.spec(d, dict(self.oracle[name], mana_cost='{G}'))

    def test_source_and_effect_witness_pins_fail_closed(self):
        model.validate_witnesses(self.oracle, self.rows)
        for name in ('Amrou Seekers', 'Glasses of Urza', 'Giant Growth', 'Kragma Butcher'):
            oracle = copy.deepcopy(self.oracle); oracle[name]['oracle_text'] += ' '
            with self.assertRaises(ValueError): model.validate_witnesses(oracle, self.rows)
        for field, value in (('handler', 'card_dummy'), ('kind', 'token'), ('csvid', -1)):
            rows = copy.deepcopy(self.rows); rows['Neurok Spy'][field] = value
            with self.assertRaises(ValueError): model.validate_witnesses(self.oracle, rows)
        pins = model.profiles(); pins['sources']['Amrou Seekers']['compiled_source']['sha256'] = '0'*64
        with patch.object(model, 'profiles', return_value=pins):
            with self.assertRaises(ValueError): model.validate_witnesses(self.oracle, self.rows)

    def test_dummy_witnesses_are_static_only_and_never_cast_or_activated(self):
        native = set(model.profiles()['native_characteristics_only_witnesses'])
        self.assertEqual(native, {'Grizzly Bears', 'Ornithopter', 'Savannah Lions', 'Wall of Denial', 'Giant Spider', 'Centaur Courser'})
        for cases in self.cases.values():
            for case in cases.values():
                refs = {c['ref'] for c in case['setup'] if c['name'] in native}
                self.assertFalse(any(s['op'] in ('cast', 'activate', 'can_activate') and s.get('ref') in refs for s in case['steps']))

    def test_all_cases_use_supported_real_operations_and_no_injection(self):
        forbidden = {'destroy', 'set_life', 'choose_priority', 'reset_death_history', 'remove_from_combat', 'set_counters'}
        for cases in self.cases.values():
            for case in cases.values():
                self.assertLessEqual({s['op'] for s in case['steps']}, OPS)
                self.assertFalse(any(s['op'] in forbidden for s in case['steps']))
                self.assertTrue(all(s['property'] in PROPERTIES for s in case['steps'] if s['op'] == 'assert_card'))
                self.assertIn(dict(op='assert_unused_choices', value=0), case['steps'])
                self.assertIn(dict(op='assert_forced_target_choices', value=0), case['steps'])

    def test_color_artifact_or_receipts_have_independent_positives(self):
        name = 'Amrou Seekers'
        for v, witness in [('allowed_block_0', 'Ornithopter'), ('exception_second_0', 'Savannah Lions'), ('exception_both_0', 'Court Homunculus')]:
            self.assertEqual(next(c['name'] for c in self.case(name, v)['setup'] if c['ref'] == 'opponent'), witness)
            self.assertEqual(self.block(name, v), 1)
        self.assertEqual(self.block(name, 'rejected_block_0'), 0); self.assertEqual(self.block(name, 'neither_0'), 0)
        self.has(name, 'rejected_block_0', 'assert_card', ref='source', property='blocked', value=0)
        self.assertEqual(self.final(name, 'rejected_block_0', 'assert_life', player=1), 18)

    def test_elven_riders_wall_flying_or_does_not_admit_reach_only(self):
        for v in ('allowed_block_0', 'exception_second_0', 'exception_both_0'): self.assertEqual(self.block('Elven Riders', v), 1)
        self.assertEqual(next(c['name'] for c in self.case('Elven Riders', 'rejected_block_0')['setup'] if c['ref'] == 'opponent'), 'Giant Spider')
        for v in ('rejected_block_0', 'neither_0'): self.assertEqual(self.block('Elven Riders', v), 0)

    def test_wall_prohibition_is_not_flying_prohibition(self):
        for v, expected in [('allowed_block_0', 1), ('rejected_block_0', 0), ('flying_nonwall_0', 1), ('wall_flying_0', 0)]:
            self.assertEqual(self.block('Rampart Crawler', v), expected)
        self.assertEqual(self.block('Rampart Crawler', 'defend_0'), 1)

    def test_fixed_power_boundary_and_real_blocker_pump(self):
        name = 'Outland Boar'
        for v, expected in [('allowed_block_0', 1), ('rejected_block_0', 0), ('power_zero_0', 0), ('power_one_0', 0), ('blocker_pump_0', 1), ('source_pump_boundary_0', 1)]:
            self.assertEqual(self.block(name, v), expected)
        self.has(name, 'blocker_pump_0', 'choose_target', player=1, ref='opponent')
        self.has(name, 'blocker_pump_0', 'cast', ref='helper', value=1)
        self.has(name, 'blocker_pump_0', 'assert_mana', player=1, color=3, value=9)

    def test_relative_power_uses_equality_and_current_source_power(self):
        name = 'Wandering Wolf'
        for v, expected in [('allowed_block_0', 1), ('rejected_block_0', 0), ('power_one_0', 0), ('blocker_pump_0', 1), ('source_pump_boundary_0', 0)]:
            self.assertEqual(self.block(name, v), expected)
        self.has(name, 'source_pump_boundary_0', 'choose_target', player=0, ref='source')
        self.has(name, 'source_pump_boundary_0', 'assert_card', ref='source', property='power', value=5)

    def test_defending_artifact_gate_and_paid_removal(self):
        name = 'Neurok Spy'
        for v, expected in [('allowed_block_0', 1), ('rejected_block_0', 0), ('artifact_own_0', 1), ('artifact_both_0', 0), ('artifact_remove_0', 1), ('artifact_own_cast_0', 1)]:
            self.assertEqual(self.block(name, v), expected)
        self.has(name, 'artifact_remove_0', 'choose_target', player=0, ref='gate_artifact')
        self.has(name, 'artifact_remove_0', 'cast', ref='helper', value=1)
        self.assertEqual(self.final(name, 'artifact_remove_0', 'assert_zone_multiplicity', player=1, zone='graveyard', index=self.rows['Glasses of Urza']['csvid']), 1)
        self.assertEqual(self.final(name, 'artifact_remove_0', 'assert_mana_total', player=0), 58)

    def test_another_minotaur_requires_own_support_and_real_bounce(self):
        name = 'Felhide Brawler'
        for v, expected in [('allowed_block_0', 1), ('rejected_block_0', 0), ('support_other_controller_0', 0), ('support_removed_0', 0), ('support_second_copy_0', 1), ('support_tapped_0', 1)]:
            self.assertEqual(self.block(name, v), expected)
        self.has(name, 'support_removed_0', 'choose_target', player=0, ref='support')
        self.assertEqual(self.final(name, 'support_removed_0', 'assert_zone_multiplicity', player=0, zone='hand', index=self.rows['Kragma Butcher']['csvid']), 1)
        self.assertEqual(self.final(name, 'support_removed_0', 'assert_mana_total', player=0), 59)

    def test_brawlers_exact_role_and_untapped_land_clauses(self):
        name = 'Branded Brawlers'
        self.has(name, 'rejected_block_0', 'attack', ref='source', value=0)
        self.has(name, 'two_lands_0', 'attack', ref='source', value=0)
        for v in ('attack_no_land_0', 'attack_own_untapped_0'): self.has(name, v, 'attack', ref='source', value=1)
        for v, expected in [('defend_0', 1), ('defend_untapped_0', 0), ('defend_no_land_0', 1), ('defend_enemy_untapped_0', 1)]:
            self.assertEqual(self.block(name, v), expected)

    def test_brawlers_land_tap_pays_real_cost_produces_then_spends_mana(self):
        for variant, who, target in [('land_tap_0', 1, 'opponent'), ('defend_land_tap_0', 0, 'source')]:
            name = 'Branded Brawlers'; steps = self.steps(name, variant)
            self.has(name, variant, 'activate', ref='gate_land', value=1)
            self.has(name, variant, 'assert_mana_total', player=who, value=61)
            self.has(name, variant, 'choose_target', player=who, ref=target)
            self.has(name, variant, 'assert_mana_total', player=who, value=60)
            self.assertEqual(self.block(name, variant), 1)
            activation = next(i for i, s in enumerate(steps) if s['op'] == 'activate')
            self.assertIn(dict(op='refresh'), steps[:activation])
            self.assertFalse(any(s['op'] == 'set_mana' for s in steps[activation:]))

    def test_real_cast_payment_failure_freshness_and_pending_characteristic(self):
        for name in self.cases:
            self.has(name, 'payment_0', 'cast', ref='source', value=1)
            self.has(name, 'payment_0', 'assert_card', ref='source', property='in_hand', value=0)
            self.has(name, 'payment_0', 'assert_mana_total', player=0, value=0)
            self.has(name, 'payment_0', 'assert_card', ref='source', property='sick', value=1)
            for v in ('wrong_color_0', 'insufficient_mana_0'): self.has(name, v, 'cast', ref='source', value=0)
            self.has(name, 'fresh_attack_0', 'attack', ref='source', value=0)

    def test_real_single_block_damage_zone_and_owner_receipts(self):
        name = 'Wandering Wolf'; v = 'allowed_block_0'
        for player, card in [(0, name), (1, 'Grizzly Bears')]:
            self.assertEqual(self.final(name, v, 'assert_zone_multiplicity', player=player, zone='graveyard', index=self.rows[card]['csvid']), 1)
        self.assertEqual(self.final(name, v, 'assert_life', player=1), 20)
        for player in (0, 1):
            self.assertEqual(self.final(name, 'source_bounce_' + str(player), 'assert_zone_multiplicity', player=player, zone='hand', index=self.rows[name]['csvid']), 1)

    def test_declared_damage_and_ready_replay_boundaries_are_actual(self):
        for name in self.cases:
            for variant in ('ready_snapshot_0', 'declared_snapshot_0', 'damage_snapshot_0'):
                steps = self.steps(name, variant); at = next(i for i, s in enumerate(steps) if s['op'] == 'capture')
                self.assertEqual(sum(s['op'] == 'assert_recorded_state' for s in steps), 1)
                self.assertIn(dict(op='ai_roundtrip'), steps)
                if variant.startswith('declared'): self.assertIn(dict(op='advance_combat'), steps[:at]); self.assertNotIn(dict(op='combat_damage'), steps[:at])
                if variant.startswith('damage'): self.assertIn(dict(op='combat_damage'), steps[:at])

    def test_mirror_preserves_all_actions_and_swaps_only_players(self):
        for name, cases in self.cases.items():
            a = copy.deepcopy(cases['declared_snapshot_0']); model._mirror(a)
            b = copy.deepcopy(cases['declared_snapshot_1'])
            a['name'] = b['name']; a['_recipe']['variant'] = b['_recipe']['variant']
            self.assertEqual(a, b)

    def test_two_combats_have_actual_turn_untap_and_main_handoff(self):
        for name in self.cases:
            steps = self.steps(name, 'two_combats_0')
            advances = [i for i, s in enumerate(steps) if s['op'] == 'advance_turn']
            self.assertEqual(len(advances), 2)
            self.assertEqual(steps[advances[0]+1], dict(op='untap_step', player=1))
            self.assertEqual(steps[advances[1]+1], dict(op='untap_step', player=0))
            self.assertEqual(steps[advances[1]+2], dict(op='turn', player=0, value=0))
            self.assertEqual(sum(s['op'] == 'begin_attackers' for s in steps), 2)

    def test_cleanup_expires_actual_paid_pump_and_keeps_cost_receipts(self):
        for name, source in model.profiles()['sources'].items():
            self.has(name, 'cleanup_0', 'cast', ref='helper', value=1)
            self.has(name, 'cleanup_0', 'cleanup')
            self.assertEqual(self.final(name, 'cleanup_0', 'assert_card', ref='source', property='power'), int(source['oracle']['power']))

    def test_selected_shared_dispatch_uses_private_family(self):
        from recipes import expand
        source = model.profiles()['sources']['Neurok Spy']; d = dict(family=model.FAMILY, version=2, params=source['params'], oracle_text=source['oracle']['oracle_text'], variants=['artifact_remove_1'])
        cases = expand('Neurok Spy', source['csvid'], d, source['oracle'], self.rows, oracle_context=self.oracle)
        self.assertEqual(len(cases), 1); self.assertEqual(cases[0]['_recipe']['family'], model.FAMILY)

    def test_exact_seventeen_followers_and_complete_printed_parameters(self):
        expected = {8852, 10410, 3605, 12919, 12040, 13073, 8213, 2120, 16665, 18211, 18517, 19615, 3973, 11354, 10610, 15857, 4907}
        self.assertEqual({s['csvid'] for s in model.profiles()['sources'].values()} - model.PILOT_IDS, expected)
        self.assertEqual(sum(map(len, self.cases.values())), 1180)
        for name, source in model.profiles()['sources'].items():
            self.assertEqual(source['oracle'], self.oracle[name])
            self.assertEqual(source['params'], model.parameters(self.oracle[name]))

    def test_black_exception_rejects_colorless_artifact_unlike_fear(self):
        for name in ('Dread Warlock', 'Prowling Nightstalker'):
            self.assertEqual(self.block(name, 'allowed_block_0'), 1)
            self.assertEqual(self.block(name, 'rejected_block_0'), 0)
            self.assertEqual(next(c['name'] for c in self.case(name, 'allowed_block_0')['setup'] if c['ref'] == 'opponent'), 'Dreg Mangler')
            self.assertEqual(next(c['name'] for c in self.case(name, 'rejected_block_0')['setup'] if c['ref'] == 'opponent'), 'Ornithopter')

    def test_red_exception_has_independent_red_and_red_artifact_receipts(self):
        name = 'Skirk Shaman'
        for variant, witness in [('exception_second_0', 'Orcish Artillery'), ('exception_both_0', 'Etherium-Horn Sorcerer')]:
            self.assertEqual(next(c['name'] for c in self.case(name, variant)['setup'] if c['ref'] == 'opponent'), witness)
            self.assertEqual(self.block(name, variant), 1)
        self.assertEqual(self.block(name, 'rejected_block_0'), 0)

    def test_rogue_exception_and_real_hybrid_vectors(self):
        name = 'Deathcult Rogue'
        self.assertEqual(self.block(name, 'allowed_block_0'), 1); self.assertEqual(self.block(name, 'rejected_block_0'), 0)
        for variant in ('payment_0', 'hybrid_other_0', 'hybrid_mixed_0', 'payment_1', 'hybrid_other_1', 'hybrid_mixed_1'):
            who = int(variant[-1])
            self.has(name, variant, 'cast', ref='source', value=1)
            self.has(name, variant, 'assert_mana_total', player=who, value=0)
        source = model.profiles()['sources'][name]
        self.assertEqual(source['params']['hybrid_pairs'], [['U', 'B'], ['U', 'B']])
        self.has(name, 'cast_0', 'choose_payment', player=0, value=1 + (2 << 8))
        self.assertFalse(any(s['op'] == 'choose_payment' for s in self.steps(name, 'cast_1')))
        self.has(name, 'cast_1', 'set_mana', player=1, color=1, value=0)
        self.assertFalse(any(s['op'] == 'choose_payment' for s in self.steps(name, 'hybrid_mixed_0')))

    def test_flying_reach_alternatives_and_real_paid_both_intersection(self):
        for name in ('Orchard Spirit', 'Spire Tracer'):
            for variant in ('allowed_block_0', 'exception_second_0', 'exception_both_0'): self.assertEqual(self.block(name, variant), 1)
            self.assertEqual(self.block(name, 'rejected_block_0'), 0)
            self.has(name, 'exception_both_0', 'choose_target', player=1, ref='opponent')
            self.has(name, 'exception_both_0', 'cast', ref='reach_grant', value=1)
            self.has(name, 'exception_both_0', 'assert_card', ref='opponent', property='tapped', value=0)
            self.has(name, 'exception_both_0', 'assert_card', ref='opponent', property='power', value=2)
            self.has(name, 'exception_both_0', 'assert_card', ref='opponent', property='toughness', value=6)
            self.assertEqual(self.final(name, 'exception_both_0', 'assert_mana_total', player=1), 57)

    def test_flying_denial_does_not_forbid_ground_blockers_or_source_defense(self):
        for name in ('Gnat Alley Creeper', 'Stone Spirit'):
            for variant, expected in [('allowed_block_0', 1), ('rejected_block_0', 0), ('flying_nonwall_0', 0), ('wall_flying_0', 0), ('defend_0', 1)]:
                self.assertEqual(self.block(name, variant), expected)

    def test_audacious_legally_blocks_artifact_and_must_take_nonzero_damage(self):
        name = 'Audacious Infiltrator'
        for owner in (0, 1):
            variant = 'defend_artifact_' + str(owner)
            case = self.case(name, variant)
            self.assertEqual(next(c['name'] for c in case['setup'] if c['ref'] == 'opponent'), 'Iron Myr')
            self.has(name, variant, 'turn', player=1-owner, value=0)
            self.has(name, variant, 'attack', ref='opponent', value=1)
            self.has(name, variant, 'block', ref='source', other='opponent', value=1)
            self.assertLess(next(i for i, s in enumerate(case['steps']) if s['op'] == 'advance_combat'), next(i for i, s in enumerate(case['steps']) if s['op'] == 'combat_damage'))
            for who, card in [(owner, name), (1-owner, 'Iron Myr')]:
                self.assertEqual(self.final(name, variant, 'assert_zone_multiplicity', player=who, zone='graveyard', index=self.rows[card]['csvid']), 1)
            self.assertFalse(any(s['op'] in ('activate', 'destroy', 'set_life') for s in case['steps']))
        self.assertEqual(self.block(name, 'rejected_block_0'), 0)

    def test_vampire_requirement_does_not_accept_another_zombie_copy(self):
        name = 'Mindless Null'
        for variant, expected in [('allowed_block_0', 1), ('rejected_block_0', 0), ('support_other_controller_0', 0), ('support_second_copy_0', 0), ('support_removed_0', 0), ('support_tapped_0', 1)]:
            self.assertEqual(self.block(name, variant), expected)
        self.assertEqual(next(c['name'] for c in self.case(name, 'allowed_block_0')['setup'] if c['ref'] == 'support'), 'Vampire Nighthawk')

    def test_wolf_or_werewolf_supporters_are_independent_real_types(self):
        name = 'Howlpack Wolf'
        for variant, witness in [('allowed_block_0', 'Young Wolf'), ('support_alternative_0', 'Greater Werewolf')]:
            self.assertEqual(next(c['name'] for c in self.case(name, variant)['setup'] if c['ref'] == 'support'), witness)
            self.assertEqual(self.block(name, variant), 1)
            self.has(name, variant, 'turn', player=1, value=0)
        self.assertEqual(self.block(name, 'rejected_block_0'), 0)
        self.assertEqual(self.block(name, 'support_other_controller_0'), 0)
        self.assertEqual(self.block(name, 'support_second_copy_0'), 1)

    def test_new_witnesses_are_whole_pinned_and_implemented(self):
        profile = model.profiles()
        for name in ('Spidery Grasp', 'Iron Myr', 'Dreg Mangler', 'Invisible Stalker', 'Young Wolf', 'Greater Werewolf', 'Vampire Nighthawk', 'Etherium-Horn Sorcerer'):
            self.assertEqual(profile['witnesses'][name], self.oracle[name])
            self.assertNotIn('dummy', self.rows[name]['handler'])
            changed = copy.deepcopy(self.oracle); changed[name]['oracle_text'] += ' '
            with self.assertRaises(ValueError): model.validate_witnesses(changed, self.rows)


if __name__ == '__main__': unittest.main()
