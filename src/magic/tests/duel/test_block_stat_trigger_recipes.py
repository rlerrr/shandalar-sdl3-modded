"""Block declaration, independent count arithmetic, real payment and replay guards."""
import copy
import json
import unittest
from unittest.mock import patch
from pathlib import Path

from generate_cases import OPS, registry
import block_stat_trigger_recipes as model


class BlockStatTriggerTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.oracle = json.loads((Path(__file__).parent/'data/oracle.json').read_text(encoding='utf8'))['cards']
        cls.rows = {r['name']: r for r in registry()}

    def declaration(self, name, variants=None):
        params = model.parameters(self.oracle[name])
        default = 'defend_one_0' if params.get('role') == 'defending' else 'attack_one_0' if params.get('mechanism') == 'fixed_predicate' else 'two_blockers_0'
        return dict(family=model.FAMILY, version=2, oracle_text=self.oracle[name]['oracle_text'],
                    params=params, variants=variants or [default])

    def case(self, name, variant):
        d = self.declaration(name, [variant]); p = model.spec(d, self.oracle[name])
        return model.expand(name, self.rows[name]['csvid'], d, p, [variant], self.rows, self.oracle)[0]

    def has(self, case, op, **kwargs):
        self.assertIn(dict(op=op, **kwargs), case['steps'])

    def section(self, case, begin='advance_combat', end='combat_damage'):
        steps = case['steps']; start = next(i for i, s in enumerate(steps) if s['op'] == begin)
        stop = next(i for i, s in enumerate(steps[start+1:], start+1) if s['op'] == end)
        return steps[start+1:stop]

    def test_complete_twelve_sources_and_exact_signed_formulas(self):
        expected = {'Balduvian War-Makers': ('beyond_first', 1, 1, 'self'), 'Craw Giant': ('beyond_first', 2, 2, 'self'),
                    "Teeka's Dragon": ('beyond_first', 4, 4, 'self'), 'Elvish Berserker': ('per_blocker', 1, 1, 'self'),
                    'Jungle Wurm': ('beyond_first', -1, -1, 'self'), 'Johtull Wurm': ('beyond_first', -2, -1, 'self'),
                    'Spined Sliver': ('per_blocker', 1, 1, 'all_slivers'),
                    'Sparring Golem': ('per_blocker', 1, 1, 'self'), 'Frost Giant': ('beyond_first', 2, 2, 'self'),
                    'Hunding Gjornersen': ('beyond_first', 1, 1, 'self'), 'Marhault Elsdragon': ('beyond_first', 1, 1, 'self'),
                    'Wolverine Pack': ('beyond_first', 2, 2, 'self')}
        self.assertEqual(set(expected), {name for name, pin in model.profiles()['sources'].items() if pin['params'].get('mechanism') != 'fixed_predicate'})
        for name, values in expected.items():
            p = model.parameters(self.oracle[name]); self.assertEqual(tuple(p[k] for k in ('basis', 'power', 'toughness', 'scope')), values)

    def test_strict_version_parameter_types_full_record_and_variants(self):
        d = self.declaration('Craw Giant')
        for value in (True, 2.0, '2', 1):
            with self.assertRaises(ValueError): model.spec(dict(d, version=value), self.oracle['Craw Giant'])
        bad = copy.deepcopy(d); bad['params']['power'] = 2.0
        with self.assertRaises(ValueError): model.spec(bad, self.oracle['Craw Giant'])
        for vs in ([], ['two_blockers_0']*2, ['not_a_control_0']):
            with self.assertRaises(ValueError): model.spec(dict(d, variants=vs), self.oracle['Craw Giant'])
        r = copy.deepcopy(self.oracle['Craw Giant']); r['mana_cost'] = '{G}'
        with self.assertRaises(ValueError): model.spec(d, r)

    def test_unknown_full_clauses_reminders_layout_and_keywords_fail_closed(self):
        for extra in ('\nDraw a card.', '\n{G}: Regenerate this creature.', '\n(Draw a card.)'):
            r = copy.deepcopy(self.oracle['Elvish Berserker']); r['oracle_text'] += extra
            with self.assertRaises(ValueError): model.parameters(r)
        for change in (dict(layout='transform'), dict(power='*'), dict(keywords=['Trample']), dict(card_faces=[{}])):
            with self.assertRaises(ValueError): model.parameters(dict(self.oracle['Elvish Berserker'], **change))

    def test_source_and_effect_witnesses_are_independently_pinned(self):
        model.validate_witnesses(self.oracle, self.rows)
        for name in ('Gaseous Form', 'Hermetic Study', 'Homing Sliver', 'Changeling Sentinel', 'Giant Growth'):
            changed = copy.deepcopy(self.oracle); changed[name]['oracle_text'] += ' '
            with self.assertRaises(ValueError): model.validate_witnesses(changed, self.rows)
        rows = copy.deepcopy(self.rows); rows['Spined Sliver']['handler'] = 'card_dummy'
        with self.assertRaises(ValueError): model.validate_witnesses(self.oracle, rows)
        rows = copy.deepcopy(self.rows); rows['Gaseous Form']['handler'] = 'card_dummy'
        with self.assertRaises(ValueError): model.validate_witnesses(self.oracle, rows)
        rows['Gaseous Form']['handler'] = 'manalinkeh_card_giant_growth'
        with self.assertRaises(ValueError): model.validate_witnesses(self.oracle, rows)
        rows = copy.deepcopy(self.rows); rows['Spined Sliver']['kind'] = 'token'
        with self.assertRaises(ValueError): model.validate_witnesses(self.oracle, rows)
        pins = model.profiles(); pins['sources']['Spined Sliver']['compiled_source']['sha256'] = '0'*64
        with patch.object(model, 'profiles', return_value=pins):
            with self.assertRaises(ValueError): model.validate_witnesses(self.oracle, self.rows)

    def test_selected_shared_dispatch_composes_the_private_family(self):
        from recipes import expand
        name = 'Craw Giant'; declaration = self.declaration(name)
        cases = expand(name, self.rows[name]['csvid'], declaration, self.oracle[name], self.rows, oracle_context=self.oracle)
        self.assertEqual(len(cases), 1)
        self.assertEqual(cases[0]['_recipe']['family'], model.FAMILY)

    def test_all_variants_use_real_supported_operations_and_properties(self):
        count = 0
        properties = {'csvid', 'power', 'toughness', 'tapped', 'sick', 'attacking', 'in_play', 'in_hand', 'damage', 'blocking_count', 'blocked', 'plus1_counters', 'minus1_counters'}
        for name in model.profiles()['sources']:
            d = self.declaration(name); p = model.spec(d, self.oracle[name]); vs = model.variants(p)
            cases = model.expand(name, self.rows[name]['csvid'], d, p, vs, self.rows, self.oracle); count += len(cases)
            self.assertEqual(len({c['name'] for c in cases}), len(vs))
            for c in cases:
                self.assertLessEqual({s['op'] for s in c['steps']}, OPS)
                self.assertFalse(any(s['op'] in ('destroy', 'choose_priority', 'set_life', 'remove_from_combat', 'reset_death_history') for s in c['steps']))
                self.assertTrue(all(s['property'] in properties for s in c['steps'] if s['op'] in ('assert_card', 'require_card')))
                self.assertEqual(c['_recipe']['oracle_text'], self.oracle[name]['oracle_text'])
        self.assertEqual(count, 1492)

    def test_count_zero_one_two_three_before_real_damage(self):
        for name, expected in [('Balduvian War-Makers', [3, 3, 4, 5]), ('Craw Giant', [6, 6, 8, 10]),
                               ("Teeka's Dragon", [5, 5, 9, 13]), ('Elvish Berserker', [1, 2, 3, 4]),
                               ('Jungle Wurm', [5, 5, 4, 3]), ('Johtull Wurm', [6, 6, 4, 2])]:
            for mode, power in zip(('zero_blockers', 'one_blocker', 'two_blockers', 'three_blockers'), expected):
                c = self.case(name, mode+'_0'); middle = self.section(c)
                self.assertIn(dict(op='assert_card', ref='source', property='power', value=power), middle)
                self.assertIn(dict(op='assert_card', ref='source', property='blocked', value=int(mode != 'zero_blockers')), c['steps'])
                self.has(c, 'cast', ref='prevention', value=1)
                self.assertEqual(sum(s['op'] == 'block' and s['value'] for s in c['steps']), ('zero_blockers', 'one_blocker', 'two_blockers', 'three_blockers').index(mode))

    def test_asymmetric_penalty_preserves_negative_power_and_no_damage_healing(self):
        c = self.case('Johtull Wurm', 'negative_power_1'); middle = self.section(c)
        self.assertIn(dict(op='assert_card', ref='source', property='power', value=-2), middle)
        self.assertIn(dict(op='assert_card', ref='source', property='toughness', value=2), middle)
        self.assertFalse(any(s['op'] == 'cast' for s in c['steps']))
        for pl in (0, 1): self.has(c, 'assert_life', player=pl, value=20)

    def test_nonpositive_toughness_dies_with_attached_real_aura_before_damage(self):
        c = self.case('Jungle Wurm', 'zero_toughness_0'); middle = self.section(c)
        for ref in ('source', 'prevention'):
            self.assertIn(dict(op='assert_card', ref=ref, property='in_play', value=0), middle)
        self.assertEqual(sum(s['op'] == 'block' for s in c['steps']), 6)
        for name in ('Jungle Wurm', 'Gaseous Form'):
            self.assertIn(dict(op='assert_zone_multiplicity', player=0, zone='graveyard', index=self.rows[name]['csvid'], value=1), middle)

    def test_actual_paid_prevention_and_independent_giant_growth(self):
        c = self.case('Craw Giant', 'pump_0')
        self.has(c, 'choose_target', player=0, ref='source'); self.has(c, 'cast', ref='prevention', value=1)
        self.has(c, 'cast', ref='helper', value=1)
        self.assertIn(dict(op='assert_card', ref='source', property='power', value=11), self.section(c))
        self.has(c, 'assert_mana', player=0, color=0, value=8); self.has(c, 'assert_mana', player=0, color=2, value=9)
        self.has(c, 'assert_mana', player=0, color=3, value=9)

    def test_declared_bonus_persists_through_end_combat_and_expires_at_cleanup(self):
        c = self.case('Craw Giant', 'cleanup_0'); steps = c['steps']
        end = next(i for i, s in enumerate(steps) if s['op'] == 'end_combat')
        clean = next(i for i, s in enumerate(steps) if s['op'] == 'cleanup')
        self.assertIn(dict(op='assert_card', ref='source', property='power', value=8), steps[end:clean])
        self.assertIn(dict(op='assert_card', ref='source', property='power', value=6), steps[clean:])

    def test_both_controllers_real_fresh_haste_attack_and_nonhaste_rejection(self):
        for owner in (0, 1):
            for name, legal in [('Balduvian War-Makers', 1), ('Craw Giant', 0)]:
                c = self.case(name, 'fresh_attack_'+str(owner))
                self.has(c, 'cast', ref='source', value=1); self.has(c, 'attack', ref='source', value=legal)
                self.has(c, 'assert_life', player=1-owner, value=17 if legal else 20)

    def test_printed_trample_has_no_prevention_and_exact_excess_damage(self):
        for name, life in [('Craw Giant', 16), ("Teeka's Dragon", 15)]:
            c = self.case(name, 'trample_1')
            self.assertFalse(any(x['name'] == 'Gaseous Form' for x in c['setup']))
            self.has(c, 'assert_life', player=0, value=life)
            for ref in ('blocker_0', 'blocker_1'): self.has(c, 'assert_card', ref=ref, property='in_play', value=0)

    def test_flying_rejects_real_ground_block_and_other_sources_allow_it(self):
        for name, legal in [("Teeka's Dragon", 0), ('Craw Giant', 1)]:
            c = self.case(name, 'ground_block_0'); self.has(c, 'block', ref='blocker_0', other='source', value=legal)
            self.assertEqual(next(x['name'] for x in c['setup'] if x['ref'] == 'blocker_0'), 'Grizzly Bears')

    def test_defending_and_unrelated_attackers_do_not_pump_source(self):
        for mode in ('defend', 'other_attack'):
            c = self.case('Craw Giant', mode+'_0')
            self.assertIn(dict(op='assert_card', ref='source', property='power', value=6), self.section(c))
        c = self.case('Craw Giant', 'unrelated_blockers_0')
        self.assertIn(dict(op='assert_card', ref='source', property='power', value=8), self.section(c))
        self.assertEqual(sum(s['op'] == 'block' for s in c['steps']), 4)
        self.assertEqual(next(x['name'] for x in c['setup'] if x['ref'] == 'idle'), 'Ornithopter')
        for ref in ('unrelated_0', 'unrelated_1'): self.has(c, 'block', ref=ref, other='idle', value=1)

    def test_global_sliver_grant_works_for_either_player_and_changeling(self):
        for owner in (0, 1):
            for mode, actor in [('own_sliver', owner), ('enemy_sliver', 1-owner), ('changeling', owner)]:
                c = self.case('Spined Sliver', mode+'_'+str(owner)); self.has(c, 'turn', player=actor, value=0)
                self.has(c, 'attack', ref='ally', value=1)
                expected = 5 if mode == 'changeling' else 4
                self.assertIn(dict(op='assert_card', ref='ally', property='power', value=expected), self.section(c))
                self.assertIn(dict(op='assert_card', ref='source', property='power', value=2), self.section(c))
                if mode == 'changeling': self.has(c, 'assert_card', ref='ally', property='tapped', value=0)

    def test_non_sliver_contrary_and_two_source_stacking(self):
        c = self.case('Spined Sliver', 'non_sliver_0')
        self.assertIn(dict(op='assert_card', ref='idle', property='power', value=2), self.section(c))
        c = self.case('Spined Sliver', 'two_sources_1')
        self.assertIn(dict(op='assert_card', ref='source', property='power', value=6), self.section(c))
        self.assertIn(dict(op='assert_card', ref='copy', property='power', value=2), self.section(c))

    def test_real_noncombat_tap_damage_does_not_announce_block_trigger(self):
        c = self.case('Elvish Berserker', 'noncombat_player_1')
        self.has(c, 'cast', ref='grant', value=1); self.has(c, 'activate', ref='grant', value=1)
        self.has(c, 'assert_card', ref='source', property='tapped', value=1)
        self.has(c, 'assert_card', ref='grant', property='tapped', value=0)
        self.has(c, 'assert_card', ref='source', property='power', value=1); self.has(c, 'assert_life', player=0, value=19)
        self.assertFalse(any(s['op'] == 'advance_combat' for s in c['steps']))

    def test_repeated_combats_use_real_both_player_turn_and_untap_lifecycle(self):
        c = self.case('Elvish Berserker', 'two_combats_0'); steps = c['steps']
        advances = [i for i, s in enumerate(steps) if s['op'] == 'advance_turn']
        self.assertEqual(len(advances), 2)
        self.assertEqual(steps[advances[0]+1], dict(op='untap_step', player=1))
        self.assertEqual(steps[advances[1]+1:advances[1]+3], [dict(op='untap_step', player=0), dict(op='turn', player=0, value=0)])
        self.assertEqual(sum(s['op'] == 'begin_attackers' for s in steps), 2)
        self.assertFalse(any(s.get('property') == 'power' and s.get('value') == 5 for s in steps))

    def test_snapshots_replay_the_actual_remaining_stage(self):
        for mode in ('ready_snapshot', 'declared_snapshot', 'damage_snapshot'):
            c = self.case('Spined Sliver', mode+'_1'); steps = c['steps']
            for op in ('capture', 'record_state', 'ai_roundtrip', 'restore', 'assert_recorded_state'): self.has(c, op)
            a = next(i for i, s in enumerate(steps) if s['op'] == 'capture')
            b = next(i for i, s in enumerate(steps) if s['op'] == 'record_state')
            d = next(i for i, s in enumerate(steps) if s['op'] == 'restore')
            self.assertEqual(steps[a+1:b], steps[d+1:-1])

    def test_printed_payment_receipts_and_no_fake_wrong_color_for_generic_card(self):
        c = self.case("Teeka's Dragon", 'payment_0')
        self.has(c, 'assert_mana_total', player=0, value=0)
        p = model.spec(self.declaration("Teeka's Dragon"), self.oracle["Teeka's Dragon"])
        self.assertNotIn('wrong_color_0', model.variants(p))
        c = self.case('Balduvian War-Makers', 'payment_1')
        self.has(c, 'assert_mana', player=1, color=0, value=0); self.has(c, 'assert_mana', player=1, color=4, value=0)

    def test_real_human_damage_assignment_is_queued_even_before_prevention(self):
        c = self.case("Teeka's Dragon", 'three_blockers_0')
        decisions = [s for s in self.section(c) if s['op'] == 'choose_target']
        self.assertEqual(decisions, [dict(op='choose_target', player=0, ref='blocker_0')]*2
                         +[dict(op='choose_target', player=0, ref='blocker_1')]*2
                         +[dict(op='choose_target', player=0, ref='blocker_2')]*9)
        self.has(c, 'cast', ref='prevention', value=1)

    def test_human_trample_inputs_do_not_override_real_ai_assignment(self):
        for owner in (0, 1):
            c = self.case('Craw Giant', 'trample_'+str(owner))
            decisions = [s for s in self.section(c) if s['op'] == 'choose_target']
            self.assertEqual(len(decisions), 8 if owner == 0 else 0)
            self.has(c, 'assert_life', player=1-owner, value=16)

    def test_global_enemy_sliver_uses_actual_human_chooser_after_mirror(self):
        for owner in (0, 1):
            c = self.case('Spined Sliver', 'enemy_sliver_'+str(owner))
            decisions = [s for s in self.section(c) if s['op'] == 'choose_target']
            self.assertEqual(len(decisions), 4 if owner == 1 else 0)
            self.assertTrue(all(s['player'] == 0 for s in decisions))

    def test_follow5_independent_printed_cost_stats_and_callback_aliases(self):
        expected = {'Sparring Golem': ([3, 0, 0, 0, 0, 0], 2, 2, 'manalinkeh_card_elvish_berserker'),
                    'Frost Giant': ([3, 0, 0, 0, 3, 0], 4, 4, 'manalinkeh_card_craw_giant'),
                    'Hunding Gjornersen': ([3, 0, 2, 0, 0, 1], 5, 4, 'manalinkeh_card_hunding_gjornersen'),
                    'Marhault Elsdragon': ([3, 0, 0, 1, 2, 0], 4, 6, 'manalinkeh_card_hunding_gjornersen'),
                    'Wolverine Pack': ([2, 0, 0, 2, 0, 0], 2, 4, 'manalinkeh_card_craw_giant')}
        for name, (mana, power, toughness, handler) in expected.items():
            p = model.spec(self.declaration(name), self.oracle[name])
            self.assertEqual((p['mana'], p['base_power'], p['base_toughness']), (mana, power, toughness))
            self.assertEqual(self.rows[name]['handler'], handler)
            self.assertEqual(p['keywords'], [] if name == 'Sparring Golem' else ['Rampage'])
            self.assertNotIn('trample_0', model.variants(p))
            c = self.case(name, 'payment_1')
            for color in range(6): self.has(c, 'assert_mana', player=1, color=color, value=0)
            self.has(c, 'assert_card', ref='source', property='power', value=power)
            c = self.case(name, 'fresh_attack_0'); self.has(c, 'attack', ref='source', value=0)


    def fixed_tail(self, case):
        steps = case['steps']; start = next(i for i, s in enumerate(steps) if s['op'] == 'advance_combat')
        stop = next((i for i, s in enumerate(steps[start+1:], start+1) if s['op'] == 'combat_damage'), len(steps))
        return steps[start+1:stop]

    def test_fixed_seventeen_complete_printed_clause_parameters(self):
        expected = {'Chub Toad': ('both', 'once', 'any', 2, 2, []),
                    'Goblin Elite Infantry': ('both', 'once', 'any', -1, -1, []),
                    'Dwarven Berserker': ('attacking', 'once', 'any', 3, 0, ['Trample']),
                    'Root Spider': ('defending', 'once', 'any', 1, 0, ['First strike']),
                    'Giant Badger': ('defending', 'once', 'any', 2, 2, []),
                    "Ezuri's Archers": ('defending', 'per_opponent', 'flying', 3, 0, []),
                    'Dwarven Soldier': ('both', 'once', 'orc', 0, 2, []),
                    'Serra Inquisitors': ('both', 'once', 'black', 2, 0, []),
                    'Razorclaw Bear': ('attacking', 'once', 'any', 2, 2, []),
                    'Slashing Tiger': ('attacking', 'once', 'any', 2, 2, []),
                    'Snorting Gahr': ('attacking', 'once', 'any', 2, 2, []),
                    'Deeproot Warrior': ('attacking', 'once', 'any', 1, 1, []),
                    'Royal Trooper': ('defending', 'once', 'any', 2, 2, []),
                    'Folk of An-Havva': ('defending', 'once', 'any', 2, 0, []),
                    'Netcaster Spider': ('defending', 'per_opponent', 'flying', 2, 0, []),
                    'Woolly Spider': ('defending', 'per_opponent', 'flying', 0, 2, []),
                    'Rashka the Slayer': ('defending', 'once', 'black', 1, 2, [])}
        self.assertEqual(set(expected), {n for n, pin in model.profiles()['sources'].items() if pin['params'].get('mechanism') == 'fixed_predicate'})
        for name, values in expected.items():
            p = model.parameters(self.oracle[name])
            self.assertEqual(tuple(p[k] for k in ('role', 'multiplicity', 'predicate', 'power', 'toughness', 'granted_keywords')), values)
            self.assertEqual(p['keywords'], ['Reach'] if name in ("Ezuri's Archers", 'Netcaster Spider', 'Woolly Spider', 'Rashka the Slayer') else [])

    def test_fixed_unreviewed_clauses_and_inconsistent_parameters_fail_closed(self):
        for name in ('Root Spider', "Ezuri's Archers", 'Serra Inquisitors'):
            for extra in ('\nDraw a card.', '\n{G}: Regenerate this creature.', '\nFlying'):
                with self.assertRaises(ValueError): model.parameters(dict(self.oracle[name], oracle_text=self.oracle[name]['oracle_text']+extra))
            d = self.declaration(name); d['params']['power'] = 1.0
            with self.assertRaises(ValueError): model.spec(d, self.oracle[name])

    def test_valor_and_predicate_witnesses_are_real_independent_pins(self):
        for name in ('Valor Made Real', 'Orcish Artillery', 'Orcish Lumberjack', 'Giant Scorpion', 'Dreg Mangler', 'Darksteel Myr'):
            changed = copy.deepcopy(self.oracle); changed[name]['mana_cost'] = '{0}'
            with self.assertRaises(ValueError): model.validate_witnesses(changed, self.rows)
        self.assertEqual(self.oracle['Valor Made Real']['oracle_text'], 'Target creature can block any number of creatures this turn.')
        self.assertEqual(self.oracle['Valor Made Real']['mana_cost'], '{W}')

    def test_fixed_once_positive_and_negative_do_not_multiply_by_opponents(self):
        for name, power, toughness in [('Chub Toad', 3, 3), ('Goblin Elite Infantry', 1, 1), ('Giant Badger', 4, 4)]:
            for mode in ('attack_two', 'defend_two', 'defend_three'):
                c = self.case(name, mode+'_0'); tail = self.fixed_tail(c)
                expected_power, expected_toughness = (2, 2) if name == 'Giant Badger' and mode.startswith('attack') else (power, toughness)
                self.assertIn(dict(op='assert_card', ref='source', property='power', value=expected_power), tail)
                self.assertIn(dict(op='assert_card', ref='source', property='toughness', value=expected_toughness), tail)
                self.assertFalse(any(s['op'] == 'combat_damage' for s in c['steps']))

    def test_actual_valor_in_response_window_pays_defender_white(self):
        for owner in (0, 1):
            c = self.case('Giant Badger', 'defend_two_'+str(owner)); steps = c['steps']
            a = next(i for i, s in enumerate(steps) if s['op'] == 'finish_attackers')
            b = next(i for i, s in enumerate(steps) if s['op'] == 'begin_blockers')
            self.assertIn(dict(op='choose_target', player=owner, ref='source'), steps[a:b])
            self.assertIn(dict(op='cast', ref='permission', value=1), steps[a:b])
            self.assertIn(dict(op='resolve'), steps[a:b])
            self.has(c, 'assert_mana', player=owner, color=5, value=9)
            self.has(c, 'assert_mana', player=1-owner, color=0, value=8)
            self.has(c, 'assert_mana', player=1-owner, color=2, value=9)
            for ref in ('opponent_0', 'opponent_1'): self.has(c, 'block', ref='source', other=ref, value=1)
            self.has(c, 'assert_card', ref='source', property='blocking_count', value=2)

    def test_no_permission_and_real_expiry_reject_second_block(self):
        c = self.case('Chub Toad', 'no_permission_0')
        self.assertFalse(any(s['op'] == 'cast' and s.get('ref') == 'permission' for s in c['steps']))
        self.has(c, 'block', ref='source', other='opponent_0', value=1)
        self.has(c, 'block', ref='source', other='opponent_1', value=0)
        self.has(c, 'assert_card', ref='source', property='blocking_count', value=1)
        c = self.case('Giant Badger', 'permission_expiry_0')
        self.assertEqual(sum(s['op'] == 'cast' and s.get('ref') == 'permission' for s in c['steps']), 1)
        self.assertEqual(sum(s['op'] == 'advance_turn' for s in c['steps']), 2)
        self.has(c, 'block', ref='source', other='opponent_1', value=0)
        self.has(c, 'assert_card', ref='source', property='blocking_count', value=1)

    def test_flying_predicate_counts_each_actual_blocked_flyer_only(self):
        for mode, expected in [('defend_two', 7), ('defend_three', 10), ('defend_mixed', 4),
                               ('defend_mixed_reverse', 4), ('defend_nonqualifying', 1), ('defend_unrelated_qualifier', 1), ('attack_two', 1)]:
            c = self.case("Ezuri's Archers", mode+'_1')
            self.assertIn(dict(op='assert_card', ref='source', property='power', value=expected), self.fixed_tail(c))
        c = self.case("Ezuri's Archers", 'defend_one_0')
        self.assertEqual(next(x['name'] for x in c['setup'] if x['ref'] == 'opponent_0'), 'Ornithopter')
        self.has(c, 'block', ref='source', other='opponent_0', value=1)

    def test_orc_boolean_predicate_mixed_orders_changeling_and_both_roles(self):
        for mode in ('attack_two', 'defend_two', 'attack_mixed_reverse', 'defend_mixed_reverse', 'attack_changeling', 'defend_changeling'):
            c = self.case('Dwarven Soldier', mode+'_0')
            self.assertIn(dict(op='assert_card', ref='source', property='toughness', value=3), self.fixed_tail(c))
        for mode in ('attack_nonqualifying', 'defend_nonqualifying', 'defend_unrelated_qualifier'):
            c = self.case('Dwarven Soldier', mode+'_0')
            self.assertIn(dict(op='assert_card', ref='source', property='toughness', value=1), self.fixed_tail(c))
        c = self.case('Dwarven Soldier', 'defend_two_1')
        self.assertEqual([x['name'] for x in c['setup'] if x['ref'] in ('opponent_0', 'opponent_1')], ['Orcish Artillery', 'Orcish Lumberjack'])

    def test_black_boolean_predicate_multicolor_and_colorless_contraries(self):
        for mode, expected in [('attack_two', 5), ('defend_two', 5), ('attack_multicolor', 5), ('defend_multicolor', 5),
                               ('attack_mixed_reverse', 5), ('defend_mixed_reverse', 5), ('attack_colorless', 3), ('defend_nonqualifying', 3)]:
            c = self.case('Serra Inquisitors', mode+'_0')
            self.assertIn(dict(op='assert_card', ref='source', property='power', value=expected), self.fixed_tail(c))
        c = self.case('Serra Inquisitors', 'defend_two_0')
        self.assertEqual([x['name'] for x in c['setup'] if x['ref'] in ('opponent_0', 'opponent_1')], ['Giant Scorpion', 'Dreg Mangler'])

    def test_gained_first_strike_real_damage_and_opposite_role_contrary(self):
        c = self.case('Root Spider', 'effect_damage_0')
        self.assertFalse(any(x['name'] == 'Gaseous Form' for x in c['setup']))
        self.has(c, 'assert_card', ref='source', property='in_play', value=1)
        self.has(c, 'assert_card', ref='source', property='damage', value=0)
        self.has(c, 'assert_card', ref='opponent_0', property='in_play', value=0)
        c = self.case('Root Spider', 'role_damage_contrary_1')
        self.assertEqual(next(x['name'] for x in c['setup'] if x['ref'] == 'opponent_0'), 'Grizzly Bears')
        self.has(c, 'assert_card', ref='source', property='in_play', value=0)
        self.has(c, 'assert_card', ref='opponent_0', property='in_play', value=0)

    def test_temporary_first_strike_expires_before_real_next_turn_contrary(self):
        c = self.case('Root Spider', 'first_strike_expiry_0'); steps = c['steps']
        self.assertEqual(sum(s['op'] == 'begin_attackers' for s in steps), 2)
        index = next(i for i, s in enumerate(steps) if s['op'] == 'advance_turn')
        self.assertEqual(steps[index+1:index+3], [dict(op='untap_step', player=0), dict(op='turn', player=0, value=0)])
        self.has(c, 'block', ref='second_victim', other='source', value=1)
        self.assertEqual(next(x['name'] for x in c['setup'] if x['ref'] == 'second_victim'), 'Grizzly Bears')
        self.has(c, 'assert_card', ref='source', property='in_play', value=0)
        self.has(c, 'assert_card', ref='second_victim', property='in_play', value=0)

    def test_gained_trample_real_single_blocker_damage_and_defending_contrary(self):
        for owner in (0, 1):
            c = self.case('Dwarven Berserker', 'effect_damage_'+str(owner))
            self.has(c, 'assert_life', player=1-owner, value=18)
            self.has(c, 'assert_card', ref='opponent_0', property='in_play', value=0)
            self.has(c, 'assert_card', ref='source', property='in_play', value=1)
            c = self.case('Dwarven Berserker', 'role_damage_contrary_'+str(owner))
            self.has(c, 'assert_card', ref='source', property='in_play', value=0)
            self.has(c, 'assert_card', ref='opponent_0', property='damage', value=1)
            for player in (0, 1): self.has(c, 'assert_life', player=player, value=20)

    def test_fixed_cleanup_and_real_repeated_actor_turn_do_not_stack(self):
        c = self.case('Chub Toad', 'cleanup_0'); steps = c['steps']; index = next(i for i, s in enumerate(steps) if s['op'] == 'cleanup')
        self.assertIn(dict(op='assert_card', ref='source', property='power', value=3), steps[:index])
        self.assertIn(dict(op='assert_card', ref='source', property='power', value=1), steps[index:])
        c = self.case('Root Spider', 'repeat_combat_1'); steps = c['steps']; indices = [i for i, s in enumerate(steps) if s['op'] == 'advance_turn']
        self.assertEqual(len(indices), 2)
        self.assertEqual(steps[indices[0]+1], dict(op='untap_step', player=1))
        self.assertEqual(steps[indices[1]+1:indices[1]+3], [dict(op='untap_step', player=0), dict(op='turn', player=0, value=0)])
        self.assertFalse(any(s.get('ref') == 'source' and s.get('property') == 'power' and s.get('value') == 4 for s in steps))

    def test_multi_snapshot_replays_real_declarations_and_labels_damage_gap(self):
        c = self.case('Giant Badger', 'multi_snapshot_0'); steps = c['steps']
        for op in ('capture', 'record_state', 'restore', 'ai_roundtrip', 'assert_recorded_state'): self.has(c, op)
        self.assertFalse(any(s['op'] == 'combat_damage' for s in steps))
        self.has(c, 'assert_card', ref='source', property='blocking_count', value=2)
        self.assertIn('Multiple-opponent', c['_recipe']['untested_components'][0])
        a = next(i for i, s in enumerate(steps) if s['op'] == 'capture'); b = next(i for i, s in enumerate(steps) if s['op'] == 'record_state'); d = next(i for i, s in enumerate(steps) if s['op'] == 'restore')
        self.assertEqual(steps[a+1:b], steps[d+1:-1])


    def test_exact_follow9_independent_printed_costs_stats_and_keywords(self):
        expected = {'Razorclaw Bear': ([2, 0, 0, 2, 0, 0], 3, 3), 'Slashing Tiger': ([2, 0, 0, 2, 0, 0], 3, 3),
                    'Snorting Gahr': ([2, 0, 0, 2, 0, 0], 3, 3), 'Deeproot Warrior': ([1, 0, 0, 1, 0, 0], 2, 2),
                    'Royal Trooper': ([2, 0, 0, 0, 0, 1], 2, 2), 'Folk of An-Havva': ([0, 0, 0, 1, 0, 0], 1, 1),
                    'Netcaster Spider': ([2, 0, 0, 1, 0, 0], 2, 3), 'Woolly Spider': ([1, 0, 0, 2, 0, 0], 2, 3),
                    'Rashka the Slayer': ([3, 0, 0, 0, 0, 2], 3, 3)}
        for name, (mana, power, toughness) in expected.items():
            p = model.spec(self.declaration(name), self.oracle[name])
            self.assertEqual((p['mana'], p['base_power'], p['base_toughness']), (mana, power, toughness))
            self.assertNotIn('Infect', p['keywords'])
            self.assertEqual(p['granted_keywords'], [])
            c = self.case(name, 'payment_1')
            for color in range(6): self.has(c, 'assert_mana', player=1, color=color, value=0)

    def test_rashka_named_wording_preserves_blocks_only_black_once_and_reach(self):
        p = model.parameters(self.oracle['Rashka the Slayer'])
        self.assertEqual((p['role'], p['predicate'], p['multiplicity']), ('defending', 'black', 'once'))
        c = self.case('Rashka the Slayer', 'attack_multicolor_0')
        self.assertIn(dict(op='assert_card', ref='source', property='power', value=3), self.fixed_tail(c))
        c = self.case('Rashka the Slayer', 'defend_mixed_reverse_0')
        self.assertIn(dict(op='assert_card', ref='source', property='power', value=4), self.fixed_tail(c))
        self.assertIn(dict(op='assert_card', ref='source', property='toughness', value=5), self.fixed_tail(c))
        for owner in (0, 1):
            c = self.case('Rashka the Slayer', 'reach_flyer_'+str(owner))
            self.assertEqual(next(x['name'] for x in c['setup'] if x['ref'] == 'opponent_0'), 'Ornithopter')
            self.has(c, 'block', ref='source', other='opponent_0', value=1)
            self.assertIn(dict(op='assert_card', ref='source', property='power', value=3), self.fixed_tail(c))
            self.assertIn(dict(op='assert_card', ref='source', property='toughness', value=3), self.fixed_tail(c))
        for name, pin in model.profiles()['sources'].items():
            if name == 'Rashka the Slayer': continue
            p = model.spec(self.declaration(name), self.oracle[name])
            self.assertNotIn('reach_flyer_0', model.variants(p))

    def test_ornithopter_callback_is_dummy_and_static_flying_only_is_used(self):
        pins = model.profiles()
        self.assertEqual(self.rows['Ornithopter']['handler'], 'card_dummy')
        self.assertEqual(pins['witness_handlers']['Ornithopter'], 'card_dummy')
        self.assertIn('Ornithopter', pins['native_characteristics_only_witnesses'])
        self.assertEqual((self.oracle['Ornithopter']['power'], self.oracle['Ornithopter']['toughness'], self.oracle['Ornithopter']['keywords']), ('0', '2', ['Flying']))
        root = Path(__file__).resolve().parents[4]
        row = next(line for line in (root/'src/magic/src/carddata.c').read_text(encoding='utf8').splitlines() if ': Ornithopter */' in line)
        self.assertIn('card_dummy, 0x00000020u', row)
        self.assertIn('KEYWORD_FLYING = 1 << 5', (root/'src/defs.h').read_text(encoding='utf8'))
        c = self.case('Rashka the Slayer', 'reach_flyer_0')
        self.assertFalse(any(s['op'] in ('cast', 'activate') and s.get('ref') == 'opponent_0' for s in c['steps']))


if __name__ == '__main__': unittest.main()
