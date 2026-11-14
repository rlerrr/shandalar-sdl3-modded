import copy
import json
from pathlib import Path
import unittest

from generate_cases import registry
from recipes import COLORS, expand as public_expand
import death_counter_extended_recipes as family


class DeathCounterExtendedTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.oracle = json.loads((Path(__file__).parent/'data/oracle.json').read_text(encoding='utf8'))['cards']
        cls.rows = {r['name']: r for r in registry()}
        cls.pins = family.pins()

    def declaration(self, name, selected):
        return dict(family=family.FAMILY, version=2, params=dict(schema=9, **family.oracle_model(self.oracle[name])),
                    oracle_text=self.oracle[name]['oracle_text'], variants=selected)

    def case(self, name, mode, player=0):
        d = self.declaration(name, [f'{mode}_{player}'])
        return public_expand(name, self.rows[name]['csvid'], d, self.oracle[name], self.rows, self.oracle)[0]

    def has(self, rows, **fields):
        return any(all(r.get(k) == v for k, v in fields.items()) for r in rows)

    def after_cast(self, case, ref):
        steps = case['steps']
        return steps[next(i for i, s in enumerate(steps) if s['op'] == 'cast' and s['ref'] == ref)+1:]

    def count(self, rows, name, player, zone, value):
        return self.has(rows, op='assert_zone_multiplicity', index=self.rows[name]['csvid'], player=player, zone=zone, value=value)

    def test_complete_oracle_typed_models_fail_closed(self):
        for name in self.pins['sources']:
            d = self.declaration(name, ['own_death_0'])
            family.spec(d, self.oracle[name])
            for key, value in [('schema', 8), ('optional', 1), ('counters', True)]:
                bad = copy.deepcopy(d); bad['params'][key] = value
                with self.assertRaises(ValueError):
                    family.spec(bad, self.oracle[name])
            for text in (self.oracle[name]['oracle_text']+'\n{T}: Draw a card.', self.oracle[name]['oracle_text'].replace('+1/+1', '+2/+2')):
                record = dict(self.oracle[name], oracle_text=text)
                with self.assertRaises(ValueError):
                    family.oracle_model(record)
        self.assertEqual(family.oracle_model(self.oracle['Sengir Vampire'])['predicate'], 'source_damaged_this_turn')
        self.assertEqual(family.oracle_model(self.oracle['Village Cannibals'])['keywords'], [])

    def test_full_witness_and_implementation_pins(self):
        family.validate_witnesses(self.oracle, self.rows)
        for name, key, value in [('Wall of Stone', 'toughness', '4'), ('Grounded', 'oracle_text', 'Draw a card.'),
                                 ('Make a Stand', 'oracle_text', 'Creatures you control get +1/+1 until end of turn.')]:
            o = copy.deepcopy(self.oracle); o[name][key] = value
            with self.assertRaises(ValueError):
                family.validate_witnesses(o, self.rows)
        rows = copy.deepcopy(self.rows); rows['Prey Upon']['handler'] = 'card_dummy'
        with self.assertRaises(ValueError):
            family.validate_witnesses(self.oracle, rows)

    def test_real_damage_then_death_not_synthetic_history(self):
        for player in (0, 1):
            c = self.case('Sengir Vampire', 'opponent_death', player); s = c['steps']
            self.assertEqual(next(x['name'] for x in c['setup'] if x['ref'] == 'victim'), 'Wall of Stone')
            fight = next(x['ref'] for x in c['setup'] if x['name'] == 'Prey Upon')
            at = next(i for i, x in enumerate(s) if x['op'] == 'cast' and x['ref'] == fight)
            self.assertEqual(s[at-2:at], [dict(op='choose_target', player=player, ref='source'), dict(op='choose_target', player=player, ref='victim')])
            removal = next(i for i, x in enumerate(s) if x['op'] == 'cast' and x['ref'] == 'removal')
            self.assertTrue(self.has(s[at:removal], op='assert_card', ref='victim', property='damage', value=4))
            self.assertTrue(self.has(s[at:removal], op='assert_card', ref='source', property='plus1_counters', value=0))
            self.assertTrue(self.has(s[removal:], op='assert_card', ref='source', property='plus1_counters', value=1))
            self.assertFalse(any(x['op'] in ('destroy', 'advance_turn') for x in s))

    def test_same_controller_damage_is_legal_real_another_target(self):
        for player in (0, 1):
            c = self.case('Sengir Vampire', 'own_death', player)
            self.assertIn('Fall of the Hammer', [x['name'] for x in c['setup']])
            self.assertTrue(self.count(c['steps'], 'Wall of Stone', player, 'graveyard', 1))
            self.assertTrue(self.has(c['steps'], op='assert_card', ref='source', property='plus1_counters', value=1))

    def test_prevention_and_undamaged_death_are_negative(self):
        for mode in ('prevented_death', 'undamaged_death'):
            c = self.case('Sengir Vampire', mode); s = c['steps']
            self.assertTrue(self.has(s, op='assert_card', ref='source', property='plus1_counters', value=0))
            self.assertFalse(self.has(s, op='assert_card', ref='source', property='plus1_counters', value=1))
            if mode == 'prevented_death':
                self.assertTrue(self.has(s, op='cast', ref='prevention', value=1))
                self.assertTrue(self.has(s, op='assert_card', ref='victim', property='damage', value=0))
            else:
                self.assertTrue(self.count(s, 'Wall of Stone', 1, 'battlefield', 1))
                self.assertTrue(self.count(s, 'Wall of Stone', 1, 'graveyard', 1))

    def test_real_turn_expiry_and_new_objects_do_not_inherit_damage(self):
        for player in (0, 1):
            c = self.case('Sengir Vampire', 'expired_history', player); s = c['steps']
            at = next(i for i, x in enumerate(s) if x['op'] == 'advance_turn')
            self.assertEqual(s[at:at+3], [dict(op='advance_turn'), dict(op='untap_step', player=1-player), dict(op='turn', player=1-player, value=0)])
            self.assertTrue(self.has(s[at:], op='assert_card', ref='source', property='plus1_counters', value=0))
            for mode, ref in [('source_new_object', 'source_new'), ('victim_new_object', 'victim_new')]:
                c = self.case('Sengir Vampire', mode, player); s = c['steps']
                bind = next(i for i, x in enumerate(s) if x['op'] == 'bind_permanent')
                self.assertEqual(s[bind]['ref'], ref)
                self.assertTrue(self.has(s[bind:], op='assert_card', ref='source_new' if mode == 'source_new_object' else 'source', property='plus1_counters', value=0))

    def test_flying_predicate_tracks_actual_modifier_at_death(self):
        for player in (0, 1):
            for mode, modifier, expected in [('flying_gained', 'Jump', 1), ('flying_lost', 'Grounded', 0)]:
                c = self.case('Soulcatcher', mode, player); s = c['steps']
                self.assertEqual(next(x['name'] for x in c['setup'] if x['ref'] == 'flying_modifier'), modifier)
                tail = self.after_cast(c, 'removal')
                self.assertTrue(self.has(tail, op='assert_card', ref='source', property='plus1_counters', value=expected))
                if mode == 'flying_lost':
                    self.assertTrue(self.count(tail, 'Grounded', player, 'graveyard', 1))
                    self.assertTrue(self.count(tail, 'Grounded', player, 'battlefield', 0))

    def test_human_another_creature_and_changeling_predicates(self):
        for player in (0, 1):
            for mode, n, count in [('own_death', 'Eager Cadet', 1), ('changeling_death', 'Woodland Changeling', 1), ('nonmatching', 'Grizzly Bears', 0)]:
                c = self.case('Village Cannibals', mode, player)
                self.assertEqual(next(x['name'] for x in c['setup'] if x['ref'] == 'victim'), n)
                self.assertTrue(self.has(self.after_cast(c, 'removal'), op='assert_card', ref='source', property='plus1_counters', value=count))

    def test_first_strike_combat_models_blocker_death_counter(self):
        for player in (0, 1):
            for mode, before, after in [('keyword_before', 0, 1), ('keyword_after', 1, 2)]:
                c = self.case('Rockslide Elemental', mode, player); s = c['steps']
                damage = next(i for i, x in enumerate(s) if x['op'] == 'combat_damage')
                self.assertTrue(self.has(s[:damage], op='block', ref='blocker', other='source', value=1))
                self.assertTrue(self.has(s[damage:], op='assert_card', ref='source', property='damage', value=0))
                self.assertTrue(self.has(s[damage:], op='assert_card', ref='source', property='plus1_counters', value=after))
                if player == 0:
                    self.assertTrue(self.has(s[:damage], op='choose_trigger_source', ref='source', player=player))

    def test_flying_ground_and_reach_actual_combat_before_after_counter(self):
        for name in ('Soulcatcher', 'Sengir Vampire'):
            for player in (0, 1):
                for stage in ('before', 'after'):
                    c = self.case(name, 'keyword_'+stage, player); s = c['steps']
                    self.assertTrue(self.has(s, op='block', ref='blocker', other='source', value=0))
                    self.assertTrue(self.has(s, op='combat_damage'))
                    c = self.case(name, 'reach_'+stage, player); s = c['steps']
                    self.assertTrue(self.has(s, op='block', ref='blocker', other='source', value=1))
                    self.assertTrue(self.count(s, 'Giant Spider', 1-player, 'graveyard', 1))
                    expected = int(stage == 'after')+int(name == 'Sengir Vampire')
                    damage = next(i for i, x in enumerate(s) if x['op'] == 'combat_damage')
                    self.assertTrue(self.has(s[damage:], op='assert_card', ref='source', property='plus1_counters', value=expected))
                    self.assertTrue(self.has(s[damage:], op='assert_card', ref='source', property='damage', value=2))

    def test_shroud_uses_targetability_observers_and_no_illegal_positive(self):
        for player in (0, 1):
            c = self.case('Algae Gharial', 'shroud_probe', player); s = c['steps']
            for p in (0, 1):
                self.assertTrue(self.has(s, op='can_target_creature', ref=f'probe_{p}', other='source', value=0))
            self.assertTrue(self.has(s, op='assert_card', ref='source', property='plus1_counters', value=1))
            model = family.spec(self.declaration('Algae Gharial', ['own_death_0']), self.oracle['Algae Gharial'])
            self.assertNotIn('source_blinked_0', family.variants(model))
            for v in family.variants(model):
                case = self.case('Algae Gharial', *v.rsplit('_', 1)[:1], player=int(v.rsplit('_', 1)[1]))
                self.assertFalse(self.has(case['steps'], op='choose_target', ref='source'))

    def test_untargeted_shroud_lifecycle_and_no_stale_dead_source_properties(self):
        for name in self.pins['sources']:
            for player in (0, 1):
                c = self.case(name, 'source_simultaneous', player); s = c['steps']
                tail = self.after_cast(c, 'sweep'); resolve = next(i for i, x in enumerate(tail) if x['op'] == 'resolve')
                self.assertFalse(any(x['op'] in ('assert_card', 'require_card', 'choose_trigger_source', 'bind_permanent') and x.get('ref') == 'source' for x in tail[resolve+1:]))
                self.assertTrue(self.count(s, name, player, 'graveyard', 1))
        for mode, spell, zone in [('source_removed', 'Wrath of God', 'graveyard'), ('source_bounced', 'Evacuation', 'hand'), ('source_exiled', 'Final Judgment', 'exile')]:
            c = self.case('Algae Gharial', mode)
            self.assertEqual(next(x['name'] for x in c['setup'] if x['ref'] == 'source_removal'), spell)
            self.assertTrue(self.count(c['steps'], 'Algae Gharial', 0, zone, 1))

    def test_shroud_counter_cancel_real_x_payment_and_shuffle_multiset(self):
        for player in (0, 1):
            c = self.case('Algae Gharial', 'counter_cancel', player); s = c['steps']
            at = next(i for i, x in enumerate(s) if x['op'] == 'cast' and x['ref'] == 'counter_cancel')
            self.assertTrue(self.has(s[:at], op='choose_x', player=player, value=1))
            self.assertTrue(self.has(s[:at], op='set_mana', player=player, color=0, value=1))
            self.assertTrue(self.has(s[:at], op='set_mana', player=player, color=COLORS['B'], value=2))
            self.assertTrue(self.has(s[at:], op='assert_card', ref='source', property='plus1_counters', value=0))
            self.assertTrue(self.has(s[at:], op='assert_shuffle_count', player=player, value=1))
            self.assertTrue(self.count(s, "Black Sun's Zenith", player, 'library', 1))
            self.assertFalse(any(x['op'] == 'assert_zone_card' and x.get('zone') == 'library' and x.get('player') == player for x in s[at:]))

    def test_partial_acceptance_uses_one_actual_offer_and_real_protection(self):
        for name in ('Algae Gharial', 'Rockslide Elemental'):
            c = self.case(name, 'partial_accept'); s = c['steps']; tail = self.after_cast(c, 'sweep')
            self.assertEqual(sum(x['op'] == 'choose_trigger_source' for x in s), 1)
            self.assertTrue(self.has(tail, op='assert_card', ref='source', property='plus1_counters', value=1))
            self.assertFalse(self.has(tail, op='assert_card', ref='source', property='plus1_counters', value=2))
            self.assertTrue(self.has(tail, op='assert_unused_choices', value=0))
            if name == 'Algae Gharial':
                self.assertEqual(next(x['name'] for x in c['setup'] if x['ref'] == 'protection'), 'Make a Stand')
                self.assertTrue(self.has(tail, op='assert_card', ref='source', property='power', value=3))
                self.assertTrue(self.has(tail, op='assert_card', ref='source', property='toughness', value=2))

    def test_snapshot_boundaries_and_counterspell_response_are_real(self):
        for name in self.pins['sources']:
            s = self.case(name, 'countered')['steps']
            at = next(i for i, x in enumerate(s) if x['op'] == 'announce_spell')
            self.assertEqual(s[at:at+3], [dict(op='announce_spell', ref='source'), dict(op='cast_response', ref='counter', value=1), dict(op='assert_stack', value=2)])
            for mode, following in [('ready_snapshot', 'cast'), ('pending_snapshot', 'resolve')]:
                s = self.case(name, mode)['steps']; at = next(i for i, x in enumerate(s) if x['op'] == 'capture')
                self.assertEqual(s[at+1]['op'], following)
                self.assertEqual(s[-1], dict(op='assert_recorded_state'))
            s = self.case(name, 'death_snapshot')['steps']; at = next(i for i, x in enumerate(s) if x['op'] == 'capture')
            optional = family.oracle_model(self.oracle[name])['optional']
            self.assertEqual(s[at+1]['op'], 'choose_trigger_source' if optional else 'resolve')

    def test_public_dispatch_all_selected_variants_and_closed_scope(self):
        totals = {}
        for name in self.pins['sources']:
            d = self.declaration(name, ['own_death_0']); model = family.spec(d, self.oracle[name]); d['variants'] = family.variants(model)
            csvid = self.rows[name]['csvid']
            homes = list((Path(__file__).parent/'data/cases/cards').glob(f'*/{csvid:05d}-*.json'))
            self.assertEqual(len(homes), 1)
            declared = json.loads(homes[0].read_text(encoding='utf8'))['recipes'][0]
            excluded = set(self.pins.get('enrollment_limits', {}).get(name, {}).get('excluded_variants', []))
            self.assertEqual(set(declared['variants']), set(d['variants'])-excluded)
            d['variants'] = declared['variants']
            cases = public_expand(name, self.rows[name]['csvid'], d, self.oracle[name], self.rows, self.oracle)
            self.assertEqual(len(cases), len(set(d['variants'])))
            self.assertTrue(all(c['_recipe']['oracle_text'] == self.oracle[name]['oracle_text'] for c in cases))
            self.assertFalse(any(s['op'] == 'destroy' for c in cases for s in c['steps']))
            totals[csvid] = len(cases)
        self.assertEqual(sum(v for csvid, v in totals.items() if csvid in (1372, 5703, 12118, 220, 1669)), 316)
        if 1373 in totals:
            self.assertEqual(totals[1373], 64)
            self.assertEqual(totals[2240], 74)

    def test_new_compositions_match_complete_calibrated_predicates(self):
        if 'Scavenger Drake' not in self.pins['sources']:
            self.skipTest('composition sources not enrolled')
        drake = family.oracle_model(self.oracle['Scavenger Drake'])
        self.assertEqual(drake, dict(predicate='another_creature', optional=True, counters=1, keywords=['Flying']))
        self.assertEqual(family.oracle_model(self.oracle['Sengir Bats']), family.oracle_model(self.oracle['Sengir Vampire']))
        self.assertEqual((self.oracle['Sengir Bats']['power'], self.oracle['Sengir Bats']['toughness']), ('1', '2'))

    def test_scavenger_reach_blocker_death_adds_optional_counter(self):
        if 'Scavenger Drake' not in self.pins['sources']:
            self.skipTest('composition source not enrolled')
        for player in (0, 1):
            c = self.case('Scavenger Drake', 'reach_after', player); s = c['steps']
            damage = next(i for i, x in enumerate(s) if x['op'] == 'combat_damage')
            self.assertTrue(self.has(s[:damage], op='block', ref='blocker', other='source', value=1))
            self.assertTrue(self.has(s[damage:], op='assert_card', ref='source', property='plus1_counters', value=2))
            self.assertTrue(self.has(s[damage:], op='assert_card', ref='source', property='power', value=6))
            self.assertTrue(self.has(s[damage:], op='assert_card', ref='source', property='damage', value=2))
            self.assertTrue(self.count(s, 'Giant Spider', 1-player, 'graveyard', 1))
            self.assertEqual(sum(x['op'] == 'choose_trigger_source' for x in s), 2 if player == 0 else 0)
            self.assertTrue(self.has(s[damage:], op='assert_unused_choices', value=0))

    def test_bats_nonlethal_one_damage_and_safe_real_flying_combat(self):
        if 'Sengir Bats' not in self.pins['sources']:
            self.skipTest('composition source not enrolled')
        for player in (0, 1):
            c = self.case('Sengir Bats', 'opponent_death', player); s = c['steps']
            removal = next(i for i, x in enumerate(s) if x['op'] == 'cast' and x['ref'] == 'removal')
            self.assertTrue(self.has(s[:removal], op='assert_card', ref='victim', property='damage', value=1))
            self.assertFalse(self.has(s[:removal], op='assert_card', ref='victim', property='damage', value=4))
            self.assertTrue(self.has(s[:removal], op='assert_card', ref='source', property='plus1_counters', value=0))
            self.assertTrue(self.has(s[removal:], op='assert_card', ref='source', property='plus1_counters', value=1))
            self.assertFalse(any(x['op'] == 'advance_turn' for x in s))
            for stage, power in [('before', 1), ('after', 2)]:
                c = self.case('Sengir Bats', 'keyword_'+stage, player); s = c['steps']
                self.assertTrue(self.has(s, op='block', ref='blocker', other='source', value=0))
                self.assertTrue(self.has(s, op='combat_damage'))
                self.assertTrue(self.has(s, op='assert_life', player=1-player, value=20-power))
            c = self.case('Sengir Bats', 'reach_before', player); s = c['steps']
            self.assertTrue(self.has(s, op='cast', ref='reach_growth', value=1))
            self.assertTrue(self.has(s, op='block', ref='blocker', other='source', value=1))
            self.assertTrue(self.count(s, 'Giant Spider', 1-player, 'graveyard', 1))
            self.assertTrue(self.has(s, op='assert_card', ref='source', property='plus1_counters', value=1))
            c = self.case('Sengir Bats', 'reach_after', player); s = c['steps']
            damage = next(i for i, x in enumerate(s) if x['op'] == 'combat_damage')
            self.assertTrue(self.has(s[:damage], op='cast', ref='reach_growth', value=1))
            self.assertTrue(self.has(s[:damage], op='block', ref='blocker', other='source', value=1))
            self.assertTrue(self.has(s[damage:], op='assert_card', ref='source', property='plus1_counters', value=2))
            self.assertTrue(self.has(s[damage:], op='assert_card', ref='source', property='power', value=6))
            self.assertTrue(self.has(s[damage:], op='assert_card', ref='source', property='toughness', value=7))
            self.assertTrue(self.has(s[damage:], op='assert_card', ref='source', property='damage', value=2))
        self.assertNotIn('Sengir Bats', self.pins.get('enrollment_limits', {}))


if __name__ == '__main__':
    unittest.main()
