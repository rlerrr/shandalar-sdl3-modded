import copy
import json
from pathlib import Path
import re
import unittest

from generate_cases import registry
from recipes import expand as public_expand
import death_recurrence_recipes as family


class DeathRecurrenceTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.oracle = json.loads((Path(__file__).parent/'data/oracle.json').read_text(encoding='utf8'))['cards']
        cls.rows = {r['name']: r for r in registry()}
        cls.pins = family.pins()

    def declaration(self, name, variants=None):
        record = self.oracle[name]
        params = dict(schema=8, **family.oracle_model(record))
        pairs = re.findall(r'\{([WUBRG])/([WUBRG])\}', record['mana_cost'])
        if pairs:
            params['payment_colors'] = [pair[0] for pair in pairs]
        return dict(family=family.FAMILY, version=2, params=params, oracle_text=record['oracle_text'],
                    variants=variants or ['death_0'])

    def case(self, name='Safehold Elite', variant='death_0'):
        declaration = self.declaration(name, [variant])
        return public_expand(name, self.rows[name]['csvid'], declaration, self.oracle[name], self.rows, self.oracle)[0]

    def has(self, steps, **fields):
        return any(all(s.get(k) == v for k, v in fields.items()) for s in steps)

    def count(self, steps, name, player, zone, number):
        return self.has(steps, op='assert_zone_multiplicity', player=player, zone=zone,
                        index=self.rows[name]['csvid'], value=number)

    def test_whole_oracle_and_typed_parameters_fail_closed(self):
        for name in self.pins['sources']:
            d = self.declaration(name)
            family.spec(d, self.oracle[name])
            for key, value in (('mechanism', 'undying' if d['params']['mechanism'] == 'persist' else 'persist'),
                               ('entry_gain', True), ('schema', 7)):
                bad = copy.deepcopy(d); bad['params'][key] = value
                with self.assertRaises(ValueError):
                    family.spec(bad, self.oracle[name])
            bad = copy.deepcopy(self.oracle[name]); bad['oracle_text'] += '\n{T}: Draw a card.'
            with self.assertRaises(ValueError):
                family.oracle_model(bad)
            bad = copy.deepcopy(self.oracle[name]); bad['oracle_text'] = bad['oracle_text'].replace('under its owner', 'under your')
            with self.assertRaises(ValueError):
                family.oracle_model(bad)
        d = self.declaration('Young Wolf'); d['variants'] = ['invented_0']
        with self.assertRaises(ValueError):
            family.spec(d, self.oracle['Young Wolf'])

    def test_complete_witness_and_implemented_source_pins(self):
        family.validate_witnesses(self.oracle, self.rows)
        for name, key, value in [('Fume Spitter', 'oracle_text', 'Draw a card.'),
                                 ('Murder', 'mana_cost', '{B}'), ('Kitchen Finks', 'toughness', '3')]:
            oracle = copy.deepcopy(self.oracle); oracle[name][key] = value
            with self.assertRaises(ValueError):
                family.validate_witnesses(oracle, self.rows)
        rows = copy.deepcopy(self.rows); rows['Young Wolf']['handler'] = 'card_dummy'
        with self.assertRaises(ValueError):
            family.validate_witnesses(self.oracle, rows)

    def test_first_return_has_exact_sign_owner_and_modified_stats(self):
        for name in self.pins['sources']:
            sign = -1 if 'Persist' in self.oracle[name]['keywords'] else 1
            for player in (0, 1):
                s = self.case(name, f'death_{player}')['steps']
                cast = next(i for i, a in enumerate(s) if a['op'] == 'cast' and a['ref'] == 'murder_1')
                bind = next(i for i, a in enumerate(s) if a['op'] == 'bind_permanent')
                self.assertLess(cast, bind)
                self.assertEqual(s[bind], dict(op='bind_permanent', ref='returned_1', player=player, value=self.rows[name]['csvid']))
                for prop, value in [('power', int(self.oracle[name]['power'])+sign),
                                    ('toughness', int(self.oracle[name]['toughness'])+sign),
                                    ('minus1_counters', int(sign == -1)), ('plus1_counters', int(sign == 1)),
                                    ('damage', 0), ('tapped', 0), ('sick', int('Haste' not in self.oracle[name]['keywords']))]:
                    self.assertTrue(self.has(s[bind:], op='assert_card', ref='returned_1', property=prop, value=value))
                self.assertTrue(self.count(s, name, player, 'graveyard', 0))

    def test_return_counter_prevents_a_second_death_return(self):
        for name in self.pins['sources']:
            for player in (0, 1):
                s = self.case(name, f'second_death_{player}')['steps']
                self.assertEqual(sum(a['op'] == 'bind_permanent' for a in s), 1)
                self.assertTrue(self.has(s, op='choose_target', ref='returned_1'))
                self.assertTrue(self.has(s, op='cast', ref='murder_2', value=1))
                self.assertTrue(self.count(s, name, player, 'graveyard', 1))
                self.assertTrue(self.count(s, name, player, 'battlefield', 0))

    def test_blocking_counters_are_earned_by_real_spell_or_sacrifice(self):
        for name in self.pins['sources']:
            persist = 'Persist' in self.oracle[name]['keywords']
            for player in (0, 1):
                case = self.case(name, f'preexisting_counter_{player}'); s = case['steps']
                witness = next(c for c in case['setup'] if c['ref'] == 'counter_control')
                self.assertEqual(witness['name'], 'Fume Spitter' if persist else 'Burst of Strength')
                self.assertTrue(self.has(s, op='cast', ref='counter_control', value=1))
                if persist:
                    self.assertTrue(self.has(s, op='activate', ref='counter_control', value=1))
                    self.assertTrue(self.count(s, 'Fume Spitter', player, 'graveyard', 1))
                self.assertTrue(self.has(s, op='assert_card', ref='source', property='minus1_counters' if persist else 'plus1_counters', value=1))
                self.assertFalse(any(a['op'] == 'bind_permanent' for a in s))
                self.assertTrue(self.count(s, name, player, 'graveyard', 1))

    def test_opposite_counter_does_not_block_and_old_counter_is_lost(self):
        for name in self.pins['sources']:
            persist = 'Persist' in self.oracle[name]['keywords']
            for player in (0, 1):
                s = self.case(name, f'opposite_counter_{player}')['steps']
                self.assertTrue(self.has(s, op='bind_permanent', ref='returned_1'))
                self.assertTrue(self.has(s, op='assert_card', ref='returned_1', property='minus1_counters' if persist else 'plus1_counters', value=1))
                self.assertTrue(self.has(s, op='assert_card', ref='returned_1', property='plus1_counters' if persist else 'minus1_counters', value=0))
                # One-toughness undying sources die to Fume itself; sturdier
                # sources survive that counter, then die to real Murder.
                self.assertEqual(self.has(s, op='cast', ref='murder_1'), persist or int(self.oracle[name]['toughness']) > 1)

    def test_foreign_control_returns_to_owner_and_finks_gain_follows_return_controller(self):
        for name in self.pins['sources']:
            for controller in (0, 1):
                c = self.case(name, f'foreign_owner_{controller}'); s = c['steps']
                source = next(e for e in c['setup'] if e['ref'] == 'source')
                self.assertIn('opponent_owned', source['state'])
                self.assertFalse(self.has(s, op='cast', ref='source'))
                self.assertTrue(self.has(s, op='bind_permanent', ref='returned_1', player=1-controller))
                self.assertTrue(self.count(s, name, 1-controller, 'battlefield', 1))
                if name == 'Kitchen Finks':
                    self.assertTrue(self.has(s, op='assert_life', player=controller, value=22))
                    self.assertTrue(self.has(s, op='assert_life', player=1-controller, value=22))

    def test_new_object_resets_real_growth_tap_and_damage(self):
        for name in self.pins['sources']:
            for player in (0, 1):
                s = self.case(name, f'new_object_{player}')['steps']
                bind = next(i for i, a in enumerate(s) if a['op'] == 'bind_permanent')
                self.assertTrue(self.has(s[:bind], op='cast', ref='growth', value=1))
                self.assertTrue(self.has(s[:bind], op='cast', ref='tap_source', value=1))
                self.assertTrue(self.has(s[:bind], op='cast', ref='injury', value=1))
                for prop, value in [('power', int(self.oracle[name]['power'])+3), ('damage', 2), ('tapped', 1)]:
                    self.assertTrue(self.has(s[:bind], op='assert_card', ref='source', property=prop, value=value))
                for prop, value in [('damage', 0), ('tapped', 0), ('sick', int('Haste' not in self.oracle[name]['keywords']))]:
                    self.assertTrue(self.has(s[bind:], op='assert_card', ref='returned_1', property=prop, value=value))
                self.assertFalse(any(a['op'] == 'assert_card' and a.get('ref') == 'source' for a in s[bind:]))

    def test_resolution_dialog_follows_twiddle_cast_target(self):
        s = self.case('Kitchen Finks', 'new_object_0')['steps']
        cast = next(i for i, a in enumerate(s) if a['op'] == 'cast' and a['ref'] == 'tap_source')
        self.assertEqual(s[cast-1], dict(op='choose_target', player=1, ref='source'))
        self.assertEqual(s[cast+2], dict(op='choose_dialog', player=1, value=0))
        self.assertEqual(s[cast+3], dict(op='resolve'))

    def test_token_bounce_and_exile_do_not_return(self):
        for name in self.pins['sources']:
            for mode in ('token_death', 'bounce_only', 'exile_only'):
                for player in (0, 1):
                    c = self.case(name, f'{mode}_{player}'); s = c['steps']
                    self.assertFalse(any(a['op'] == 'bind_permanent' for a in s))
                    self.assertTrue(self.count(s, name, player, 'graveyard', 0))
                    self.assertTrue(self.count(s, name, player, 'battlefield', 0))
                    if mode == 'token_death':
                        self.assertIn('token', next(e for e in c['setup'] if e['ref'] == 'source')['state'])
                        self.assertFalse(self.has(s, op='cast', ref='source'))
                    else:
                        self.assertTrue(self.count(s, name, player, 'hand' if mode == 'bounce_only' else 'exile', 1))

    def test_finks_entire_etb_and_persist_receipt(self):
        for player in (0, 1):
            for mode, expected in [('death', 24), ('second_death', 24), ('preexisting_counter', 22),
                                   ('cleanup_once', 24), ('cast_only', 22), ('token_death', 22), ('countered', 20)]:
                s = self.case('Kitchen Finks', f'{mode}_{player}')['steps']
                self.assertTrue(self.has(s, op='assert_life', player=player, value=expected))
            s = self.case('Kitchen Finks', f'death_{player}')['steps']
            self.assertLess(next(i for i, a in enumerate(s) if a == dict(op='assert_life', player=player, value=22)),
                            next(i for i, a in enumerate(s) if a['op'] == 'cast' and a['ref'] == 'murder_1'))

    def test_cleanup_preserves_return_counter_without_another_return(self):
        for name in self.pins['sources']:
            s = self.case(name, 'cleanup_once_0')['steps']
            cleanup = next(i for i, a in enumerate(s) if a['op'] == 'cleanup')
            self.assertEqual(sum(a['op'] == 'bind_permanent' for a in s), 1)
            self.assertTrue(self.has(s[cleanup:], op='assert_card', ref='returned_1', property='sick', value=int('Haste' not in self.oracle[name]['keywords'])))
            self.assertTrue(self.has(s[cleanup:], op='assert_card', ref='returned_1', property='minus1_counters' if 'Persist' in self.oracle[name]['keywords'] else 'plus1_counters', value=1))

    def test_real_hybrid_alternative_mixed_and_explicit_ambiguous_choice(self):
        for name in ('Safehold Elite', 'Gravelgill Axeshark', 'Kitchen Finks'):
            for player in (0, 1):
                c = self.case(name, f'hybrid_payment_other_{player}')
                self.assertNotEqual(c['_recipe']['params']['payment_colors'], self.declaration(name)['params']['payment_colors'])
                self.assertTrue(self.has(c['steps'], op='cast', ref='source', value=1))
            s = self.case(name, 'hybrid_choice_0')['steps']
            choice = next(i for i, a in enumerate(s) if a['op'] == 'choose_payment')
            self.assertEqual(s[choice+1], dict(op='cast', ref='source', value=1))
            s = self.case(name, 'wrong_color_0')['steps']
            pairs = re.findall(r'\{([WUBRG])/([WUBRG])\}', self.oracle[name]['mana_cost'])
            from recipes import COLORS
            for color in {c for pair in pairs for c in pair}:
                self.assertTrue(self.has(s, op='set_mana', player=0, color=COLORS[color], value=0))
            self.assertTrue(self.has(s, op='cast', ref='source', value=0))
        c = self.case('Kitchen Finks', 'hybrid_payment_mixed_1')
        self.assertEqual(c['_recipe']['params']['payment_colors'], ['G', 'W'])

    def test_countered_announcement_pays_both_real_spells_and_no_return(self):
        for name in self.pins['sources']:
            for player in (0, 1):
                s = self.case(name, f'countered_{player}')['steps']
                i = next(i for i, a in enumerate(s) if a['op'] == 'announce_spell')
                self.assertEqual(s[i], dict(op='announce_spell', ref='source'))
                self.assertEqual(s[i+1], dict(op='cast_response', ref='counter', value=1))
                self.assertTrue(self.count(s, name, player, 'graveyard', 1))
                self.assertTrue(self.count(s, 'Counterspell', 1-player, 'graveyard', 1))
                self.assertFalse(any(a['op'] == 'bind_permanent' for a in s))

    def test_snapshots_use_actual_source_or_death_pending_boundary(self):
        for mode in ('ready_snapshot', 'pending_snapshot', 'death_snapshot'):
            s = self.case('Kitchen Finks', f'{mode}_0')['steps']
            capture = next(i for i, a in enumerate(s) if a['op'] == 'capture')
            self.assertIn(dict(op='restore'), s)
            self.assertEqual(s[-1], dict(op='assert_recorded_state'))
            if mode == 'ready_snapshot':
                self.assertEqual(s[capture+1], dict(op='cast', ref='source', value=1))
            else:
                self.assertEqual(s[capture+1], dict(op='resolve'))
            if mode == 'death_snapshot':
                self.assertTrue(self.has(s[:capture], op='cast', ref='murder_1', value=1))
                self.assertFalse(self.has(s[:capture], op='bind_permanent'))

    def test_all_selected_declarations_expand_through_public_dispatch(self):
        for name in self.pins['sources']:
            d = self.declaration(name)
            d['variants'] = family.variants(family.spec(d, self.oracle[name]))
            cases = public_expand(name, self.rows[name]['csvid'], d, self.oracle[name], self.rows, self.oracle)
            self.assertEqual(len(cases), len(set(d['variants'])))
            self.assertTrue(all(c['_recipe']['oracle_text'] == self.oracle[name]['oracle_text'] for c in cases))

    def test_keyword_composition_accounts_for_complete_printed_text(self):
        for name in ('Scuzzback Marauders', 'Strangleroot Geist', 'Vengeful Vampire', 'Vorapede', 'Hound of Griselbrand'):
            model = family.oracle_model(self.oracle[name])
            self.assertEqual(set(model['keywords']), set(self.oracle[name]['keywords'])-{'Persist', 'Undying'})
            declaration = self.declaration(name)
            del declaration['params']['keywords']
            with self.assertRaises(ValueError):
                family.spec(declaration, self.oracle[name])
            oracle = copy.deepcopy(self.oracle[name]); oracle['oracle_text'] += '\n{T}: Draw a card.'
            with self.assertRaises(ValueError):
                family.oracle_model(oracle)
            oracle = copy.deepcopy(self.oracle[name]); oracle['keywords'] = ['Persist'] if 'Persist' in oracle['keywords'] else ['Undying']
            with self.assertRaises(ValueError):
                family.oracle_model(oracle)
        self.assertNotIn('keywords', family.oracle_model(self.oracle['Kitchen Finks']))

    def test_keyword_cycle_casts_then_uses_distinct_legal_combats_around_real_death(self):
        for name in ('Scuzzback Marauders', 'Strangleroot Geist', 'Vengeful Vampire', 'Vorapede', 'Hound of Griselbrand'):
            for owner in (0, 1):
                s = self.case(name, f'keyword_cycle_{owner}')['steps']
                cast = next(i for i, a in enumerate(s) if a['op'] == 'cast' and a['ref'] == 'source')
                attacks = [i for i, a in enumerate(s) if a['op'] == 'attack']
                murder = next(i for i, a in enumerate(s) if a['op'] == 'cast' and a['ref'] == 'murder_1')
                bind = next(i for i, a in enumerate(s) if a['op'] == 'bind_permanent')
                self.assertEqual(len(attacks), 2)
                self.assertLess(cast, attacks[0]); self.assertLess(attacks[0], murder)
                self.assertLess(murder, bind); self.assertLess(bind, attacks[1])
                self.assertEqual(s[attacks[0]], dict(op='attack', ref='source', value=1))
                self.assertEqual(s[attacks[1]], dict(op='attack', ref='returned_1', value=1))
                self.assertEqual(sum(a['op'] == 'advance_turn' for a in s[attacks[0]:murder]), 2)
                self.assertTrue(self.has(s[attacks[0]:murder], op='untap_step', player=owner))
                self.assertEqual(sum(a['op'] == 'combat_damage' for a in s), 2)
                self.assertEqual(sum(a['op'] == 'end_combat' for a in s), 2)

    def test_haste_permits_fresh_original_and_fresh_returned_attacks(self):
        for owner in (0, 1):
            s = self.case('Strangleroot Geist', f'keyword_cycle_{owner}')['steps']
            cast = next(i for i, a in enumerate(s) if a['op'] == 'cast' and a['ref'] == 'source')
            bind = next(i for i, a in enumerate(s) if a['op'] == 'bind_permanent')
            attacks = [i for i, a in enumerate(s) if a['op'] == 'attack']
            self.assertFalse(any(a['op'] == 'advance_turn' for a in s[cast:attacks[0]]))
            self.assertFalse(any(a['op'] == 'advance_turn' for a in s[bind:attacks[1]]))
            self.assertTrue(self.has(s[bind:], op='assert_card', ref='returned_1', property='sick', value=0))
            self.assertTrue(self.has(s, op='assert_life', player=1-owner, value=18))
            self.assertTrue(self.has(s, op='assert_life', player=1-owner, value=15))

    def test_flying_rejects_ground_and_accepts_real_reach_before_and_after_return(self):
        for owner in (0, 1):
            s = self.case('Vengeful Vampire', f'keyword_cycle_{owner}')['steps']
            for stage, target in (('before', 'source'), ('after', 'returned_1')):
                self.assertTrue(self.has(s, op='block', ref=f'{stage}_blocker', other=target, value=0))
            self.assertTrue(self.has(s, op='assert_life', player=1-owner, value=13))
            self.assertTrue(self.count(s, 'Grizzly Bears', 1-owner, 'battlefield', 2))
            case = self.case('Vengeful Vampire', f'keyword_reach_cycle_{owner}'); s = case['steps']
            self.assertEqual([c['name'] for c in case['setup'] if c['ref'].endswith('_blocker')], ['Giant Spider', 'Giant Spider'])
            for stage, target in (('before', 'source'), ('after', 'returned_1')):
                self.assertTrue(self.has(s, op='block', ref=f'{stage}_blocker', other=target, value=1))
            self.assertTrue(self.has(s, op='cast', ref='reach_growth', value=1))
            bind = next(i for i, a in enumerate(s) if a['op'] == 'bind_permanent')
            self.assertFalse(self.has(s[bind:], op='cast', ref='reach_growth'))
            self.assertTrue(self.has(s[bind:], op='assert_card', ref='returned_1', property='damage', value=2))
            self.assertTrue(self.count(s, 'Giant Spider', 1-owner, 'graveyard', 2))

    def test_trample_and_vigilance_apply_to_original_and_counter_modified_return(self):
        for name, final in (('Scuzzback Marauders', 15), ('Vorapede', 13)):
            for owner in (0, 1):
                case = self.case(name, f'keyword_cycle_{owner}'); s = case['steps']
                self.assertEqual([c['name'] for c in case['setup'] if c['ref'].endswith('_blocker')], ['Ornithopter', 'Ornithopter'])
                for stage, target in (('before', 'source'), ('after', 'returned_1')):
                    self.assertTrue(self.has(s, op='block', ref=f'{stage}_blocker', other=target, value=1))
                self.assertTrue(self.has(s, op='assert_life', player=1-owner, value=17))
                self.assertTrue(self.has(s, op='assert_life', player=1-owner, value=final))
                attacks = [i for i, a in enumerate(s) if a['op'] == 'attack']
                for index in attacks:
                    self.assertEqual(s[index+2], dict(op='assert_card', ref=s[index]['ref'], property='tapped', value=int(name != 'Vorapede')))
                self.assertTrue(self.count(s, 'Ornithopter', 1-owner, 'graveyard', 2))

    def test_double_strike_has_two_hits_and_first_strike_survival_after_return(self):
        for owner in (0, 1):
            s = self.case('Hound of Griselbrand', f'keyword_cycle_{owner}')['steps']
            self.assertTrue(self.has(s, op='assert_life', player=1-owner, value=16))
            self.assertTrue(self.has(s, op='assert_life', player=1-owner, value=10))
            s = self.case('Hound of Griselbrand', f'keyword_first_strike_cycle_{owner}')['steps']
            for stage, target in (('before', 'source'), ('after', 'returned_1')):
                self.assertTrue(self.has(s, op='block', ref=f'{stage}_blocker', other=target, value=1))
                self.assertTrue(self.has(s, op='assert_card', ref=target, property='damage', value=0))
            self.assertTrue(self.count(s, 'Grizzly Bears', 1-owner, 'graveyard', 3))
            self.assertTrue(self.has(s, op='assert_life', player=1-owner, value=20))

    def test_four_toughness_recurrence_uses_four_damage_removal(self):
        case = self.case('Vorapede', 'lethal_damage_0')
        self.assertEqual(next(c['name'] for c in case['setup'] if c['ref'] == 'damage_removal'), 'Lightning Blast')
        self.assertTrue(self.has(case['steps'], op='cast', ref='damage_removal', value=1))
        self.assertTrue(self.has(case['steps'], op='bind_permanent', ref='returned_1'))

    def test_real_untap_is_followed_by_same_player_main_phase_selection(self):
        for name in ('Scuzzback Marauders', 'Strangleroot Geist', 'Vengeful Vampire', 'Vorapede', 'Hound of Griselbrand'):
            d = self.declaration(name)
            model = family.spec(d, self.oracle[name])
            for variant in family.variants(model):
                if not variant.startswith('keyword_'):
                    continue
                owner = int(variant[-1])
                steps = self.case(name, variant)['steps']
                for index, action in enumerate(steps):
                    if action['op'] == 'untap_step':
                        self.assertEqual(steps[index-2:index+2], [dict(op='advance_turn'), dict(op='advance_turn'),
                                         dict(op='untap_step', player=owner), dict(op='turn', player=owner, value=0)])

    def test_haste_raw_bits_do_not_replace_real_fresh_attack_proof(self):
        name = 'Strangleroot Geist'
        d = self.declaration(name)
        for variant in family.variants(family.spec(d, self.oracle[name])):
            steps = self.case(name, variant)['steps']
            for action in steps:
                if action['op'] == 'assert_card' and action.get('property') == 'sick':
                    self.assertEqual(action['value'], 0)
        for owner in (0, 1):
            # A creature without haste still needs actual turn gates before
            # the returned object's attack. Haste's earlier test forbids them.
            steps = self.case('Hound of Griselbrand', f'keyword_cycle_{owner}')['steps']
            bind = next(i for i, action in enumerate(steps) if action['op'] == 'bind_permanent')
            attack = next(i for i in range(bind, len(steps)) if steps[i]['op'] == 'attack')
            self.assertEqual(sum(action['op'] == 'advance_turn' for action in steps[bind:attack]), 2)


if __name__ == '__main__':
    unittest.main()
