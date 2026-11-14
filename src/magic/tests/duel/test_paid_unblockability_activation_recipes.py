"""Whole printed activation costs, honest target probes and real combat guards."""
import copy
import hashlib
import json
import unittest
from pathlib import Path
from unittest.mock import patch

from generate_cases import OPS, PROPERTIES, registry
import paid_unblockability_activation_recipes as model


class PaidUnblockabilityActivationTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.oracle = json.loads((Path(__file__).parent / 'data/oracle.json').read_text(encoding='utf8'))['cards']
        cls.rows = {r['name']: r for r in registry()}; cls.cases = {}
        for name, pin in model.profiles()['sources'].items():
            d = dict(family=model.FAMILY, version=2, params=pin['params'], oracle_text=pin['oracle']['oracle_text'], variants=model.variants(pin['params']))
            p = model.spec(d, cls.oracle[name])
            cls.cases[name] = {c['_recipe']['variant']: c for c in model.expand(name, pin['csvid'], d, p, d['variants'], cls.rows, cls.oracle)}

    def case(self, name, variant): return self.cases[name][variant]
    def steps(self, name, variant): return self.case(name, variant)['steps']
    def has(self, name, variant, op, **kw): self.assertIn(dict(op=op, **kw), self.steps(name, variant))
    def final(self, name, variant, op, **kw):
        return next(s['value'] for s in reversed(self.steps(name, variant)) if s['op'] == op and all(s.get(k) == v for k, v in kw.items()))
    def block(self, name, variant): return next(s['value'] for s in self.steps(name, variant) if s['op'] == 'block')

    def test_exact_twelve_full_oracle_sources_and_population(self):
        self.assertEqual({p['csvid'] for p in model.profiles()['sources'].values()}, {13367, 14070, 4268, 10938, 15524, 16391, 15300, 19452, 6095, 2458, 16991, 18923})
        self.assertEqual(sum(map(len, self.cases.values())), 736)
        self.assertEqual(hashlib.sha256((Path(__file__).parent / 'data/oracle.json').read_bytes()).hexdigest(), 'c11dcfedde2ed4ce99ff355f5cf30c187c2b3745c87a03319e715bbfec17cf50')
        for name, pin in model.profiles()['sources'].items():
            self.assertEqual(pin['oracle'], self.oracle[name]); self.assertEqual(pin['params'], model.parameters(self.oracle[name]))

    def test_complete_cost_scope_and_timing_from_oracle(self):
        expected = {'Agent of Horizons': ('{2}{U}', False, 'self', 'any'), 'Amphin Pathmage': ('{2}{U}', False, 'any', 'any'),
            'Zhuge Jin, Wu Strategist': ('', True, 'any', 'own_before_attackers'), 'Goblin Tunneler': ('', True, 'any', 'any'),
            'Kaseto, Orochi Archmage': ('{G}{U}', False, 'any', 'any'), 'Ghirapur Guide': ('{2}{G}', False, 'own', 'any')}
        for name, values in expected.items():
            p = model.parameters(self.oracle[name]); self.assertEqual(tuple(p[k] for k in ('activation_cost', 'tap', 'scope', 'timing')), values)
        self.assertEqual(model.parameters(self.oracle['Goblin Tunneler'])['target_power_max'], 2)
        self.assertEqual(model.parameters(self.oracle['Ghirapur Guide'])['blocker_power_max'], 2)
        self.assertEqual(model.parameters(self.oracle['Kaseto, Orochi Archmage'])['snake_bonus'], [2, 2])

    def test_unknown_clauses_layout_keywords_costs_fail_closed(self):
        for name in self.cases:
            for change in (dict(layout='transform'), dict(card_faces=[{}]), dict(power='*'), dict(keywords=['Haste']),
                    dict(oracle_text=self.oracle[name]['oracle_text'] + '\nDraw a card.'), dict(oracle_text=self.oracle[name]['oracle_text'] + ' (Draw a card.)')):
                with self.assertRaises(ValueError): model.parameters(dict(self.oracle[name], **change))
        for text in ('{U/P}: This creature can\'t be blocked this turn.', '{U/B}: This creature can\'t be blocked this turn.', 'This creature can\'t be blocked this turn.'):
            with self.assertRaises(ValueError): model.parameters(dict(self.oracle['Agent of Horizons'], oracle_text=text))

    def test_strict_declaration_version_types_parameters_and_variants(self):
        name = 'Goblin Tunneler'; pin = model.profiles()['sources'][name]
        d = dict(family=model.FAMILY, version=2, params=pin['params'], oracle_text=pin['oracle']['oracle_text'], variants=['activated_0'])
        for version in (True, 2.0, '2', 1):
            with self.assertRaises(ValueError): model.spec(dict(d, version=version), self.oracle[name])
        for variants in ([], ['activated_0']*2, ['unknown_0']):
            with self.assertRaises(ValueError): model.spec(dict(d, variants=variants), self.oracle[name])
        for field, value in [('tap', 1), ('target_power_max', 2.0), ('snake_bonus', [False, 0])]:
            changed = copy.deepcopy(d); changed['params'][field] = value
            with self.assertRaises(ValueError): model.spec(changed, self.oracle[name])
        with self.assertRaises(ValueError): model.spec(d, dict(self.oracle[name], mana_cost='{R}'))

    def test_independent_source_witness_and_compiled_implementation_pins(self):
        model.validate_witnesses(self.oracle, self.rows)
        for name in ('Agent of Horizons', 'Sakura-Tribe Elder', 'Diplomatic Immunity', 'Twiddle', 'Giant Growth', 'Unsummon'):
            changed = copy.deepcopy(self.oracle); changed[name]['oracle_text'] += ' '
            with self.assertRaises(ValueError): model.validate_witnesses(changed, self.rows)
        for field, value in [('handler', 'card_dummy'), ('kind', 'token'), ('csvid', -1)]:
            changed = copy.deepcopy(self.rows); changed['Agent of Horizons'][field] = value
            with self.assertRaises(ValueError): model.validate_witnesses(self.oracle, changed)
        pins = model.profiles(); pins['sources']['Agent of Horizons']['compiled_source']['sha256'] = '0'*64
        with patch.object(model, 'profiles', return_value=pins):
            with self.assertRaises(ValueError): model.validate_witnesses(self.oracle, self.rows)

    def test_native_witnesses_are_honestly_static_only(self):
        names = set(model.profiles()['native_characteristics_only_witnesses'])
        self.assertEqual(names, {'Grizzly Bears', 'Wall of Denial', 'Ornithopter', 'Centaur Courser'})
        for cases in self.cases.values():
            for case in cases.values():
                refs = {c['ref'] for c in case['setup'] if c['name'] in names}
                self.assertFalse(any(s['op'] in ('cast', 'activate', 'can_activate') and s.get('ref') in refs for s in case['steps']))

    def test_supported_operations_no_fake_effect_damage_or_priority(self):
        forbidden = {'destroy', 'remove_from_combat', 'set_life', 'choose_priority', 'cast_response', 'resolve_response', 'reset_death_history'}
        for cases in self.cases.values():
            for case in cases.values():
                self.assertLessEqual({s['op'] for s in case['steps']}, OPS)
                self.assertFalse(any(s['op'] in forbidden for s in case['steps']))
                self.assertTrue(all(s['property'] in PROPERTIES for s in case['steps'] if s['op'] == 'assert_card'))
                self.assertIn(dict(op='assert_unused_choices', value=0), case['steps'])
                self.assertIn(dict(op='assert_forced_target_choices', value=0), case['steps'])

    def test_real_ability_resolution_precedes_rejected_block_and_damage(self):
        for name in self.cases:
            steps = self.steps(name, 'activated_0')
            activation = next(i for i, s in enumerate(steps) if s['op'] == 'activate')
            resolve = next(i for i, s in enumerate(steps) if s['op'] == 'resolve')
            declared = next(i for i, s in enumerate(steps) if s['op'] == 'advance_combat')
            damage = next(i for i, s in enumerate(steps) if s['op'] == 'combat_damage')
            self.assertLess(activation, resolve); self.assertLess(resolve, declared); self.assertLess(declared, damage)
            self.assertEqual(self.block(name, 'activated_0'), 0)
            self.assertEqual(self.block(name, 'unactivated_0'), 1)
            expected_life = 20 - (int(self.oracle[name]['power']) if self.oracle[name]['oracle_text'].endswith("This creature can't be blocked this turn.") else 2)
            self.assertEqual(self.final(name, 'activated_0', 'assert_life', player=1), expected_life)
            self.assertEqual(self.final(name, 'unactivated_0', 'assert_life', player=1), 20)

    def test_cast_cost_and_activation_cost_are_independent_exact_receipts(self):
        for name in self.cases:
            self.has(name, 'cast_payment_0', 'cast', ref='source', value=1)
            self.has(name, 'cast_payment_0', 'assert_card', ref='source', property='in_hand', value=0)
            self.has(name, 'cast_payment_0', 'assert_card', ref='source', property='sick', value=1)
            self.assertEqual(self.final(name, 'cast_payment_0', 'assert_mana_total', player=0), 0)
            self.has(name, 'activation_payment_0', 'activate', ref='source', value=1)
            self.assertEqual(self.final(name, 'activation_payment_0', 'assert_mana_total', player=0), 0)
            for mode in ('cast_insufficient', 'cast_wrong_color'): self.has(name, mode+'_0', 'cast', ref='source', value=0)
        self.assertEqual(self.final('Agent of Horizons', 'cast_0', 'assert_mana', player=0, color=3), 9)
        self.assertEqual(self.final('Agent of Horizons', 'activated_0', 'assert_mana', player=0, color=2), 9)

    def test_insufficient_and_wrong_activation_mana_do_not_queue_target_intents(self):
        for name, pin in model.profiles()['sources'].items():
            if not pin['params']['activation_cost']: continue
            for mode in ('activation_insufficient', 'activation_wrong_color'):
                self.has(name, mode+'_0', 'activate', ref='source', value=0)
                self.assertFalse(any(s['op'] == 'choose_target' for s in self.steps(name, mode+'_0')))

    def test_actual_tap_cost_and_tapped_source_contrast(self):
        for name, pin in model.profiles()['sources'].items():
            p = pin['params']
            self.has(name, 'activated_0', 'assert_card', ref='source', property='tapped', value=int(p['tap']))
            self.has(name, 'tapped_source_0', 'activate', ref='source', value=int(not p['tap']))
            if p['tap']:
                self.has(name, 'repeat_denied_0', 'activate', ref='source', value=0)
                self.assertEqual(sum(s['op'] == 'activate' and s['value'] == 1 for s in self.steps(name, 'repeat_denied_0')), 1)

    def test_real_fresh_cast_sickness_affects_tap_cost_not_untargeted_cost(self):
        for name, pin in model.profiles()['sources'].items():
            self.has(name, 'fresh_source_0', 'cast', ref='source', value=1)
            self.has(name, 'fresh_source_0', 'activate', ref='source', value=int(not pin['params']['tap']))
            self.has(name, 'fresh_attack_0', 'attack', ref='source', value=0)

    def test_repeat_tap_activation_pays_real_untap_spell_before_second_cost(self):
        for name in ('Goblin Tunneler', 'Zhuge Jin, Wu Strategist'):
            steps = self.steps(name, 'repeat_activation_0')
            casts = [i for i, s in enumerate(steps) if s['op'] == 'cast' and s['ref'] == 'untap']
            activations = [i for i, s in enumerate(steps) if s['op'] == 'activate' and s['value'] == 1]
            self.assertEqual(len(casts), 1); self.assertEqual(len(activations), 2)
            self.assertLess(activations[0], casts[0]); self.assertLess(casts[0], activations[1])
            self.has(name, 'repeat_activation_0', 'choose_dialog', player=0, value=1)
            self.assertEqual(self.final(name, 'repeat_activation_0', 'assert_mana', player=0, color=2), 9)

    def test_post_attackers_real_window_and_own_before_attackers_gate(self):
        for name in self.cases:
            steps = self.steps(name, 'after_attackers_0')
            finish = next(i for i, s in enumerate(steps) if s['op'] == 'finish_attackers')
            query = next(i for i, s in enumerate(steps) if s['op'] == 'can_activate')
            begin = next(i for i, s in enumerate(steps) if s['op'] == 'begin_blockers')
            self.assertLess(finish, query); self.assertLess(query, begin)
            is_zhuge = name == 'Zhuge Jin, Wu Strategist'
            self.has(name, 'after_attackers_0', 'can_activate', ref='source', value=int(not is_zhuge))
            self.assertEqual(self.block(name, 'after_attackers_0'), int(is_zhuge))
        name = 'Zhuge Jin, Wu Strategist'
        self.has(name, 'opposing_turn_0', 'turn', player=1, value=0)
        self.has(name, 'opposing_turn_0', 'can_activate', ref='source', value=0)
        steps = self.steps(name, 'late_main2_0')
        self.assertLess(next(i for i, s in enumerate(steps) if s['op'] == 'end_combat'), next(i for i, s in enumerate(steps) if s['op'] == 'can_activate'))

    def test_readonly_probes_never_choose_forbidden_target(self):
        for name, cases in self.cases.items():
            for mode in ('probe_foreign', 'probe_noncreature', 'probe_large', 'pump_before_probe'):
                if mode+'_0' not in cases: continue
                steps = self.steps(name, mode+'_0')
                self.assertTrue(any(s['op'] == 'probe_activation_target' and s['index'] == -1 for s in steps))
                self.assertFalse(any(s['op'] == 'activate' for s in steps))
                if mode != 'pump_before_probe': self.assertFalse(any(s['op'] == 'choose_target' for s in steps))
        self.has('Ghirapur Guide', 'probe_foreign_0', 'probe_activation_target', ref='source', other='foreign', index=-1, value=0)
        self.assertNotIn('foreign_target_0', self.cases['Ghirapur Guide'])

    def test_target_power_zero_one_two_three_and_after_grant_growth(self):
        name = 'Goblin Tunneler'
        for v in ('zero_target_0', 'small_target_0', 'activated_0'): self.has(name, v, 'activate', ref='source', value=1)
        self.has(name, 'probe_large_0', 'probe_activation_target', ref='source', other='ally', index=-1, value=0)
        self.has(name, 'pump_before_probe_0', 'cast', ref='growth', value=1)
        self.has(name, 'pump_before_probe_0', 'probe_activation_target', ref='source', other='ally', index=-1, value=0)
        self.assertEqual(self.block(name, 'pumped_after_grant_0'), 0)
        self.assertEqual(self.final(name, 'pumped_after_grant_0', 'assert_life', player=1), 15)

    def test_ghirapur_is_blocker_power_predicate_not_blanket_unblockable(self):
        name = 'Ghirapur Guide'
        for v, expected in [('activated_0', 0), ('blocker_one_0', 0), ('blocker_two_0', 0), ('blocker_three_0', 1), ('blocker_pump_0', 1)]:
            self.assertEqual(self.block(name, v), expected)
        self.has(name, 'blocker_pump_0', 'choose_target', player=1, ref='blocker')
        self.assertEqual(self.final(name, 'blocker_pump_0', 'assert_mana', player=1, color=3), 9)

    def test_shroud_uses_real_paid_aura_and_does_not_stop_untargeted_source(self):
        for name, pin in model.profiles()['sources'].items():
            self.has(name, 'shrouded_source_0', 'cast', ref='cloak', value=1)
            self.has(name, 'shrouded_source_0', 'activate', ref='source', value=1)
            if pin['params']['scope'] != 'self':
                self.has(name, 'probe_shroud_0', 'cast', ref='cloak', value=1)
                self.has(name, 'probe_shroud_0', 'probe_activation_target', ref='source', other='ally', index=-1, value=0)

    def test_any_target_off_turn_activation_and_foreign_ownership(self):
        for name in ('Amphin Pathmage', 'Goblin Tunneler', 'Kaseto, Orochi Archmage'):
            self.has(name, 'foreign_target_0', 'turn', player=1, value=0)
            self.has(name, 'foreign_target_0', 'choose_target', player=0, ref='ally')
            self.has(name, 'foreign_target_0', 'attack', ref='ally', value=1)
            self.assertEqual(self.final(name, 'foreign_target_0', 'assert_life', player=0), 18)
        name = 'Zhuge Jin, Wu Strategist'
        self.has(name, 'foreign_target_0', 'turn', player=0, value=0)
        self.has(name, 'foreign_target_0', 'choose_target', player=0, ref='ally')
        self.assertFalse(any(s['op'] == 'begin_attackers' for s in self.steps(name, 'foreign_target_0')))
        self.assertTrue(any('foreign-target' in gap for gap in self.case(name, 'foreign_target_0')['_recipe']['untested_components']))

    def test_snake_bonus_and_non_snake_no_bonus(self):
        name = 'Kaseto, Orochi Archmage'
        self.assertEqual(self.final(name, 'activated_0', 'assert_card', ref='ally', property='power'), 2)
        self.assertEqual(self.final(name, 'snake_target_0', 'assert_card', ref='ally', property='power'), 3)
        self.assertEqual(self.final(name, 'self_target_0', 'assert_card', ref='source', property='power'), 4)
        self.assertEqual(self.final(name, 'foreign_snake_0', 'assert_life', player=0), 17)
        self.assertEqual(self.final(name, 'snake_repeat_0', 'assert_card', ref='ally', property='power'), 5)
        self.assertEqual(self.final(name, 'snake_repeat_0', 'assert_mana_total', player=0), 56)

    def test_expiry_real_own_turn_untap_and_snake_stat_reset(self):
        for name in self.cases:
            steps = self.steps(name, 'expiry_combat_0')
            advances = [i for i, s in enumerate(steps) if s['op'] == 'advance_turn']
            self.assertEqual(len(advances), 2)
            self.assertEqual(steps[advances[0]+1], dict(op='untap_step', player=1))
            self.assertEqual(steps[advances[1]+1], dict(op='untap_step', player=0))
            self.assertEqual(steps[advances[1]+2], dict(op='turn', player=0, value=0))
            self.assertEqual([s['value'] for s in steps if s['op'] == 'block'], [0, 1])
        self.assertEqual(self.final('Kaseto, Orochi Archmage', 'snake_expiry_0', 'assert_card', ref='ally', property='power'), 1)
        self.assertEqual(self.final('Kaseto, Orochi Archmage', 'snake_expiry_0', 'assert_card', ref='ally', property='toughness'), 1)

    def test_resolved_effect_survives_source_bounce_and_target_destination_is_original_owner(self):
        for name, pin in model.profiles()['sources'].items():
            if pin['params']['scope'] == 'self': continue
            self.has(name, 'source_bounce_0', 'choose_target', player=0, ref='source')
            self.assertEqual(self.block(name, 'source_bounce_0'), 0)
            self.assertEqual(self.final(name, 'source_bounce_0', 'assert_zone_multiplicity', player=0, zone='hand', index=self.rows[name]['csvid']), 1)
            self.assertEqual(self.final(name, 'target_bounce_0', 'assert_zone_multiplicity', player=0, zone='hand', index=self.rows['Grizzly Bears']['csvid']), 1)

    def test_target_specific_effect_does_not_apply_to_other_attacker(self):
        for name in self.cases:
            self.has(name, 'other_attacker_0', 'activate', ref='source', value=1)
            self.has(name, 'other_attacker_0', 'attack', ref='idle', value=1)
            self.assertEqual(self.block(name, 'other_attacker_0'), 1)
            self.assertEqual(self.final(name, 'other_attacker_0', 'assert_life', player=1), 20)

    def test_snapshot_boundaries_are_real_pending_resolved_and_declared_states(self):
        for name in self.cases:
            for mode in ('ready_snapshot', 'pending_snapshot', 'resolved_snapshot', 'declared_snapshot'):
                steps = self.steps(name, mode+'_0'); cut = next(i for i, s in enumerate(steps) if s['op'] == 'capture')
                self.assertEqual(sum(s['op'] == 'assert_recorded_state' for s in steps), 1)
                if mode == 'pending_snapshot':
                    self.assertIn(dict(op='activate', ref='source', value=1), steps[:cut])
                    self.assertNotIn(dict(op='resolve'), steps[:cut])
                    self.assertIn(dict(op='assert_stack', value=1), steps[:cut])
                if mode == 'resolved_snapshot': self.assertIn(dict(op='resolve'), steps[:cut]); self.assertNotIn(dict(op='begin_attackers'), steps[:cut])
                if mode == 'declared_snapshot': self.assertIn(dict(op='advance_combat'), steps[:cut]); self.assertNotIn(dict(op='combat_damage'), steps[:cut])

    def test_both_controllers_mirror_exact_actions_and_ownership(self):
        for name, cases in self.cases.items():
            for mode in ('activated', 'pending_snapshot', 'expiry_combat'):
                a = copy.deepcopy(cases[mode+'_0']); model._mirror(a)
                b = cases[mode+'_1']; a['name'] = b['name']; a['_recipe']['variant'] = b['_recipe']['variant']
                self.assertEqual(a, b)

    def test_new_dispatcher_selects_private_family(self):
        from recipes import expand
        name = 'Agent of Horizons'; pin = model.profiles()['sources'][name]
        d = dict(family=model.FAMILY, version=2, params=pin['params'], oracle_text=pin['oracle']['oracle_text'], variants=['activated_1'])
        cases = expand(name, pin['csvid'], d, pin['oracle'], self.rows, oracle_context=self.oracle)
        self.assertEqual(len(cases), 1); self.assertEqual(cases[0]['_recipe']['family'], model.FAMILY)

    def test_followers_independently_pin_printed_cost_stats_and_activation(self):
        expected = {'Coralhelm Guide': ('{1}{U}', '2', '1', '{4}{U}'), 'Passwall Adept': ('{1}{U}', '1', '3', '{2}{U}'),
            'Crafty Pathmage': ('{2}{U}', '1', '1', ''), 'Dwarven Nomad': ('{2}{R}', '1', '1', ''),
            'Pathmaker Initiate': ('{1}{R}', '2', '1', ''), 'Frilled Sea Serpent': ('{4}{U}{U}', '4', '6', '{5}{U}{U}')}
        for name, values in expected.items():
            pin = model.profiles()['sources'][name]; record = self.oracle[name]
            self.assertEqual(tuple(record[k] for k in ('mana_cost', 'power', 'toughness')) + (pin['params']['activation_cost'],), values)
            self.assertEqual(record, pin['oracle']); self.assertEqual(model.parameters(record), pin['params'])
            self.has(name, 'cast_payment_0', 'cast', ref='source', value=1)
            self.has(name, 'activation_payment_1', 'activate', ref='source', value=1)
            self.assertEqual(self.final(name, 'cast_payment_0', 'assert_mana_total', player=0), 0)
            self.assertEqual(self.final(name, 'activation_payment_1', 'assert_mana_total', player=1), 0)

    def test_exact_handler_aliases_do_not_replace_source_costs_or_stats(self):
        pins = model.profiles()['sources']
        for name in ('Crafty Pathmage', 'Dwarven Nomad', 'Pathmaker Initiate'):
            self.assertEqual(pins[name]['handler'], pins['Goblin Tunneler']['handler'])
            self.assertEqual(pins[name]['params'], pins['Goblin Tunneler']['params'])
        self.assertEqual(pins['Passwall Adept']['handler'], pins['Amphin Pathmage']['handler'])
        self.assertEqual(pins['Passwall Adept']['params'], pins['Amphin Pathmage']['params'])
        self.assertEqual(self.final('Pathmaker Initiate', 'self_target_0', 'assert_card', ref='source', property='power'), 2)
        self.has('Pathmaker Initiate', 'probe_self_0', 'probe_activation_target', ref='source', other='source', index=-1, value=1)
        self.has('Pathmaker Initiate', 'self_target_0', 'cast', ref='untap', value=1)

    def test_seven_mana_self_activation_repeat_has_exact_generic_blue_receipts(self):
        name = 'Frilled Sea Serpent'; variant = 'repeat_activation_1'
        self.assertEqual(sum(s['op'] == 'activate' and s['value'] == 1 for s in self.steps(name, variant)), 2)
        self.assertEqual(self.final(name, variant, 'assert_mana', player=1, color=0), 0)
        self.assertEqual(self.final(name, variant, 'assert_mana', player=1, color=2), 6)
        self.assertEqual(self.final(name, variant, 'assert_mana_total', player=1), 46)
        self.assertEqual(self.final(name, variant, 'assert_card', ref='source', property='power'), 4)
        self.assertEqual(self.final(name, variant, 'assert_card', ref='source', property='toughness'), 6)
        self.assertEqual(self.final(name, variant, 'assert_life', player=0), 16)

    def test_targeted_followers_keep_foreign_target_and_power_boundary_controls(self):
        for name in ('Coralhelm Guide', 'Passwall Adept', 'Crafty Pathmage', 'Dwarven Nomad', 'Pathmaker Initiate'):
            self.has(name, 'foreign_target_1', 'turn', player=0, value=0)
            self.has(name, 'foreign_target_1', 'choose_target', player=1, ref='ally')
            self.assertEqual(self.final(name, 'foreign_target_1', 'assert_life', player=1), 18)
        for name in ('Crafty Pathmage', 'Dwarven Nomad', 'Pathmaker Initiate'):
            self.has(name, 'probe_large_1', 'probe_activation_target', ref='source', other='ally', index=-1, value=0)
            self.assertEqual(self.block(name, 'pumped_after_grant_0'), 0)
            self.has(name, 'repeat_activation_1', 'cast', ref='untap', value=1)


if __name__ == '__main__': unittest.main()
