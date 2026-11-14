"""Focused whole-clause and real-lifecycle guards; no engine execution."""
import copy
import hashlib
import json
from pathlib import Path
import unittest

from generate_cases import registry
from recipes import expand
import counter_condition_recurrence_recipes as family


class CounterConditionRecurrenceTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.base = Path(__file__).parent
        cls.oracle = json.loads((cls.base/'data/oracle.json').read_text(encoding='utf8'))['cards']
        cls.rows = {r['name']: r for r in registry()}
        cls.data = family.pins()
        cls.declarations, cls.cases = {}, {}
        for name, source in cls.data['sources'].items():
            home = next((cls.base/'data/cases/cards'/f"{source['csvid']//100:03d}").glob(f"{source['csvid']:05d}-*.json"))
            declaration = json.loads(home.read_text(encoding='utf8'))['recipes'][0]
            cls.declarations[name] = declaration
            cls.cases[name] = {c['_recipe']['variant']: c for c in expand(name, source['csvid'], declaration, cls.oracle[name], cls.rows, cls.oracle)}

    def case(self, name, variant):
        return self.cases[name][variant]

    def steps(self, name, variant):
        return self.case(name, variant)['steps']

    def has(self, steps, **fields):
        return any(all(s.get(k) == v for k, v in fields.items()) for s in steps)

    def test_exact_six_whole_models_and_all_explicit_variants(self):
        self.assertEqual({v['csvid'] for v in self.data['sources'].values()}, {12364, 9857, 12260, 12270, 9891, 12400})
        self.assertEqual(sum(map(len, self.cases.values())), 389)
        for name, declaration in self.declarations.items():
            model = family.spec(declaration, self.oracle[name])
            self.assertEqual(set(declaration['variants']), set(family.variants(model)))
            self.assertEqual(len(self.cases[name]), len(declaration['variants']))
            self.assertEqual(model['mechanism'], 'persist' if 'Persist' in self.oracle[name]['keywords'] else 'undying')

    def test_identity_version_variant_and_whole_clause_mutations_rejected(self):
        for name, declaration in self.declarations.items():
            for key, value in [('family', 'death_recurrence'), ('version', True), ('version', 2.0), ('variants', []),
                               ('variants', ['death_0', 'death_0']), ('variants', ['invented_0']), ('variants', [True])]:
                bad = copy.deepcopy(declaration); bad[key] = value
                with self.assertRaises(ValueError):
                    family.spec(bad, self.oracle[name])
            bad = copy.deepcopy(declaration); bad['params']['mechanism'] = 'undying' if bad['params']['mechanism'] == 'persist' else 'persist'
            with self.assertRaises(ValueError):
                family.spec(bad, self.oracle[name])
            for text in [self.oracle[name]['oracle_text']+'\n{T}: Draw a card.',
                         self.oracle[name]['oracle_text'].replace("under its owner's control", 'under your control')]:
                record = dict(self.oracle[name], oracle_text=text)
                with self.assertRaises(ValueError):
                    family.oracle_model(record)
        record = copy.deepcopy(self.oracle['Sightless Ghoul'])
        record['oracle_text'] += "\nThis creature can't block."
        with self.assertRaises(ValueError):
            family.oracle_model(record)

    def test_independent_full_witness_and_implementation_pins(self):
        family.validate_witnesses(self.oracle, self.rows)
        for name in ('Elvish Mystic', 'Serra Angel', 'Darksteel Myr', 'Act of Treason', 'Fume Spitter'):
            rows = copy.deepcopy(self.rows); rows[name]['handler'] = 'card_dummy'
            with self.assertRaises(ValueError):
                family.validate_witnesses(self.oracle, rows)
            oracle = copy.deepcopy(self.oracle); oracle[name]['oracle_text'] += '\nDraw a card.'
            with self.assertRaises(ValueError):
                family.validate_witnesses(oracle, self.rows)

    def test_natural_death_return_counter_and_second_death(self):
        for name, source in self.data['sources'].items():
            mechanism = family.oracle_model(self.oracle[name])['mechanism']
            kind, sign = ('minus1', -1) if mechanism == 'persist' else ('plus1', 1)
            for player in (0, 1):
                s = self.steps(name, f'second_death_{player}')
                binds = [i for i, a in enumerate(s) if a['op'] == 'bind_permanent']
                self.assertEqual(len(binds), 1)
                b = binds[0]
                self.assertEqual(s[b], dict(op='bind_permanent', ref='returned_1', player=player, value=source['csvid']))
                self.assertTrue(self.has(s[:b], op='cast', ref='murder_1', value=1))
                self.assertTrue(self.has(s[b:], op='assert_counter', ref='returned_1', counter_kind=kind, value=1))
                self.assertTrue(self.has(s[b:], op='assert_card', ref='returned_1', property='power', value=int(self.oracle[name]['power'])+sign))
                self.assertTrue(self.has(s[b:], op='cast', ref='murder_2', value=1))
                self.assertTrue(self.has(s[b:], op='assert_card', ref='returned_1', property='in_play', value=0))

    def test_real_counter_cancellation_reenables_return_without_seeded_counters(self):
        for name in self.cases:
            for player in (0, 1):
                c = self.case(name, f'counter_reset_{player}'); s = c['steps']
                self.assertEqual([a['ref'] for a in s if a['op'] == 'bind_permanent'], ['returned_1', 'returned_2'])
                control = next(c for c in c['setup'] if c['ref'] == 'counter_control')
                self.assertEqual(control['name'], 'Burst of Strength' if 'Persist' in self.oracle[name]['keywords'] else 'Fume Spitter')
                self.assertTrue(self.has(s, op='cast', ref='counter_control', value=1))
                if control['name'] == 'Fume Spitter':
                    self.assertTrue(self.has(s, op='activate', ref='counter_control', value=1))
                for entity in c['setup']:
                    self.assertFalse(set(entity)-{'name', 'ref', 'player', 'zone', 'state'})
                    self.assertFalse(any('counter' in str(state) for state in entity.get('state', [])))

    def test_real_theft_returns_to_owner_and_no_second_return(self):
        for name, source in self.data['sources'].items():
            for owner in (0, 1):
                s = self.steps(name, f'stolen_second_death_{owner}')
                i = next(i for i, a in enumerate(s) if a['op'] == 'cast' and a['ref'] == 'theft')
                binds = [a for a in s[i:] if a['op'] == 'bind_permanent']
                self.assertEqual(binds, [dict(op='bind_permanent', ref='stolen_source', player=1-owner, value=source['csvid']),
                                         dict(op='bind_permanent', ref='returned_1', player=owner, value=source['csvid'])])
                self.assertTrue(self.has(s[i:], op='choose_target', player=1-owner, ref='stolen_source'))
                self.assertTrue(self.has(s[i:], op='assert_zone_multiplicity', player=owner, zone='graveyard', index=source['csvid'], value=1))
                self.assertTrue(self.has(s[i:], op='assert_zone_multiplicity', player=1-owner, zone='graveyard', index=source['csvid'], value=0))

    def test_foreign_owner_fixture_starts_without_counter_and_returns_to_owner(self):
        for name, source in self.data['sources'].items():
            for controller in (0, 1):
                c = self.case(name, f'foreign_return_second_{controller}')
                self.assertIn('opponent_owned', next(c for c in c['setup'] if c['ref'] == 'source')['state'])
                self.assertTrue(self.has(c['steps'], op='bind_permanent', ref='returned_1', player=1-controller, value=source['csvid']))
                self.assertEqual(sum(a['op'] == 'bind_permanent' for a in c['steps']), 1)

    def test_pump_cost_and_live_stats_are_pending_then_resolved(self):
        for name, amount, base in [('Evernight Shade', 1, 2), ('Restless Apparition', 3, 1)]:
            for owner in (0, 1):
                s = self.steps(name, f'pump_repeat_{owner}')
                activations = [i for i, a in enumerate(s) if a['op'] == 'activate' and a['ref'] == 'returned_1']
                self.assertEqual(len(activations), 2)
                for n, i in enumerate(activations):
                    self.assertTrue(self.has(s[i:], op='assert_stack', value=1))
                    resolve = next(j for j in range(i+1, len(s)) if s[j]['op'] == 'resolve')
                    self.assertTrue(self.has(s[i:resolve], op='assert_card', ref='returned_1', property='power', value=base+n*amount))
                    self.assertTrue(self.has(s[resolve:], op='assert_card', ref='returned_1', property='power', value=base+(n+1)*amount))
                    self.assertTrue(all(self.has(s[i:resolve], op='assert_mana', player=owner, color=c, value=0) for c in range(6)))

    def test_pump_before_death_resets_and_cleanup_keeps_natural_counter(self):
        for name in ('Evernight Shade', 'Restless Apparition'):
            for owner in (0, 1):
                s = self.steps(name, f'pump_before_death_{owner}')
                pump = next(i for i, a in enumerate(s) if a['op'] == 'activate')
                murder = next(i for i, a in enumerate(s) if a['op'] == 'cast' and a['ref'] == 'murder_1')
                self.assertLess(pump, murder)
                s = self.steps(name, f'pump_cleanup_{owner}')
                i = next(i for i, a in enumerate(s) if a['op'] == 'cleanup')
                sign = -1 if 'Persist' in self.oracle[name]['keywords'] else 1
                self.assertTrue(self.has(s[i:], op='assert_card', ref='returned_1', property='power', value=int(self.oracle[name]['power'])+sign))
                self.assertTrue(self.has(s[i:], op='assert_counter', ref='returned_1', counter_kind='minus1' if sign == -1 else 'plus1', value=1))

    def test_zero_and_wrong_pump_costs_do_not_change_stats_or_stack(self):
        for name in ('Evernight Shade', 'Restless Apparition'):
            for owner in (0, 1):
                for variant in ('pump_zero_pool', 'pump_wrong_color', 'pump_insufficient'):
                    s = self.steps(name, f'{variant}_{owner}')
                    self.assertTrue(self.has(s, op='require_can_activate', ref='returned_1', value=0))
                    self.assertTrue(self.has(s, op='activate', ref='returned_1', value=0))
                    self.assertFalse(any(a['op'].startswith('choose_') for a in s if a.get('ref') == 'returned_1'))

    def test_hybrid_pump_uses_exact_real_color_vector(self):
        for owner in (0, 1):
            for variant, black, white in [('pump_after_return', 0, 3), ('pump_other_color', 3, 0), ('pump_mixed', 1, 2)]:
                s = self.steps('Restless Apparition', f'{variant}_{owner}')
                a = next(i for i, a in enumerate(s) if a['op'] == 'activate')
                fund = {a['color']: a['value'] for a in s[:a] if a['op'] == 'set_mana' and a['player'] == owner}
                self.assertEqual((fund[1], fund[5]), (black, white))

    def test_blocking_prohibitions_use_actual_declarations_both_cycles(self):
        for owner in (0, 1):
            for name in ('Sightless Ghoul', 'Stormbound Geist'):
                for variant, flying in [('block_ground_cycle', False), ('block_flying_cycle', True)]:
                    s = self.steps(name, f'{variant}_{owner}')
                    blocks = [a for a in s if a['op'] == 'block']
                    self.assertEqual([a['ref'] for a in blocks], ['source', 'returned_1'])
                    self.assertEqual([a['value'] for a in blocks], [int(flying and name == 'Stormbound Geist')]*2)
            s = self.steps('Stormbound Geist', f'attack_flying_cycle_{owner}')
            self.assertEqual([a['value'] for a in s if a['op'] == 'block'], [0, 1, 0, 1])

    def test_howlgeist_live_power_boundary_and_real_counter_or_growth(self):
        for owner in (0, 1):
            for variant, expected in [('block_boundary_cycle', [1, 0]), ('block_lower_cycle', [0, 0]),
                                      ('block_equal_cycle', [1, 1]), ('block_higher_cycle', [1, 1])]:
                c = self.case('Howlgeist', f'{variant}_{owner}'); s = c['steps']
                self.assertEqual([a['value'] for a in s if a['op'] == 'block'], expected)
                if variant == 'block_equal_cycle':
                    self.assertTrue(self.has(s, op='cast', ref='after_blocker_counter', value=1))
                elif variant == 'block_higher_cycle':
                    self.assertEqual(sum(a['op'] == 'cast' and a['ref'].endswith('_blocker_growth') for a in s), 2)
                self.assertTrue(all(c['name'] != 'Air Elemental' for c in c['setup']))

    def test_thunderblust_haste_and_conditional_trample_use_real_combat(self):
        for owner in (0, 1):
            s = self.steps('Thunderblust', f'trample_cycle_{owner}')
            attacks = [i for i, a in enumerate(s) if a['op'] == 'attack']
            self.assertEqual(len(attacks), 2)
            first_cast = next(i for i, a in enumerate(s) if a['op'] == 'cast')
            bind = next(i for i, a in enumerate(s) if a['op'] == 'bind_permanent')
            self.assertFalse(any(a['op'] == 'advance_turn' for a in s[first_cast:attacks[0]]))
            self.assertFalse(any(a['op'] == 'advance_turn' for a in s[bind:attacks[1]]))
            self.assertEqual(sum(a['op'] == 'combat_damage' for a in s), 2)
            self.assertTrue(self.has(s, op='assert_life', player=1-owner, value=20))
            self.assertTrue(self.has(s, op='assert_life', player=1-owner, value=15))
            self.assertTrue(self.has(s, op='assert_card', ref='before_blocker', property='damage', value=7))
            self.assertTrue(self.has(s, op='assert_card', ref='after_blocker', property='damage', value=1))

    def test_snapshots_capture_actual_pending_actions_and_replay_full_tail(self):
        for name in self.cases:
            modes = ['ready_snapshot', 'pending_snapshot', 'death_snapshot', 'resolved_snapshot']
            if 'pump' in family.oracle_model(self.oracle[name]):modes += ['pump_snapshot']
            for mode in modes:
                s = self.steps(name, mode+'_0')
                capture = next(i for i, a in enumerate(s) if a['op'] == 'capture')
                self.assertIn(dict(op='restore'), s)
                self.assertEqual(s[-1], dict(op='assert_recorded_state'))
                if mode not in ('resolved_snapshot', 'ready_snapshot'):
                    self.assertEqual(s[capture+1], dict(op='resolve'))

    def test_countered_cast_and_non_death_zone_changes_never_return(self):
        for name in self.cases:
            for owner in (0, 1):
                for variant in ('countered', 'bounce_only', 'exile_only', 'token_death'):
                    s = self.steps(name, f'{variant}_{owner}')
                    self.assertFalse(any(a['op'] == 'bind_permanent' for a in s))
                s = self.steps(name, f'countered_{owner}')
                i = next(i for i, a in enumerate(s) if a['op'] == 'announce_spell')
                self.assertEqual(s[i+1], dict(op='cast_response', ref='counter', value=1))
                self.assertFalse(any(a['op'] in ('choose_trigger', 'choose_optional_target') for a in s))

    def test_every_combat_end_completes_actual_damage_after_declarations(self):
        for cases in self.cases.values():
            for case in cases.values():
                phase = 'none'
                for action in case['steps']:
                    op = action['op']
                    if op == 'begin_blockers':phase = 'declare'
                    elif op == 'advance_combat':
                        self.assertEqual(phase, 'declare', case['name'])
                        phase = 'response'
                    elif op == 'combat_damage':
                        self.assertEqual(phase, 'response', case['name'])
                        phase = 'damage'
                    elif op == 'end_combat':
                        self.assertEqual(phase, 'damage', case['name'])
                        phase = 'ended'

    def test_legality_controls_pay_real_fog_and_preserve_controlled_death(self):
        count = 0
        for name, cases in self.cases.items():
            for variant, case in cases.items():
                fog = [c for c in case['setup'] if c['name'] == 'Fog']
                if not fog:
                    continue
                count += 1
                owner = int(variant[-1])
                self.assertEqual(len(fog), 2)
                self.assertTrue(all(c['player'] == owner and c['zone'] == 'hand' for c in fog))
                steps = case['steps']
                for stage in ('before', 'after'):
                    cast = next(i for i, a in enumerate(steps) if a['op'] == 'cast' and a['ref'] == stage+'_combat_fog')
                    advance = max(i for i, a in enumerate(steps[:cast]) if a['op'] == 'advance_combat')
                    damage = next(i for i in range(cast+1, len(steps)) if steps[i]['op'] == 'combat_damage')
                    self.assertLess(advance, cast)
                    self.assertTrue(self.has(steps[cast:damage], op='resolve'))
                self.assertTrue(self.has(steps, op='assert_zone_multiplicity', player=owner, zone='graveyard', index=self.rows['Fog']['csvid'], value=2))
                self.assertEqual(sum(a['op'] == 'bind_permanent' for a in steps), 1)
        self.assertEqual(count, 22)
        for owner in (0, 1):
            case = self.case('Thunderblust', f'trample_cycle_{owner}')
            self.assertFalse(any(c['name'] == 'Fog' for c in case['setup']))

    def test_new_defender_stays_sick_during_opponents_turn_but_can_block(self):
        for name in ('Sightless Ghoul', 'Stormbound Geist'):
            for owner in (0, 1):
                for mode in ('block_ground_cycle', 'block_flying_cycle'):
                    s = self.steps(name, f'{mode}_{owner}')
                    for ref in ('source', 'returned_1'):
                        block = next(i for i, a in enumerate(s) if a['op'] == 'block' and a['ref'] == ref)
                        end = next(i for i in range(block+1, len(s)) if s[i]['op'] == 'end_combat')
                        next_turn = next((i for i in range(end+1, len(s)) if s[i]['op'] == 'advance_turn'), len(s))
                        self.assertTrue(self.has(s[end:next_turn], op='assert_card', ref=ref, property='sick', value=1))
                        self.assertFalse(self.has(s[end:next_turn], op='assert_card', ref=ref, property='sick', value=0))
                    self.assertTrue(self.has(s, op='block', ref='returned_1', other='after_attacker',
                                             value=int(name == 'Stormbound Geist' and mode == 'block_flying_cycle')))

    def test_accepted_helpers_are_byte_immutable(self):
        root = self.base.parents[3]
        before = json.loads((root/'out/wave8/next949/recurrence/immutable-helper-before.json').read_text(encoding='utf8'))
        for path, expected in before.items():
            self.assertEqual(hashlib.sha256((root/path).read_bytes()).hexdigest(), expected)


if __name__ == '__main__':
    unittest.main()
