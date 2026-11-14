import copy
import json
from pathlib import Path
import unittest

from generate_cases import registry
from recipes import COLORS, expand as public_expand
import death_self_destination_recipes as family


class DeathSelfDestinationTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.oracle = json.loads((Path(__file__).parent/'data/oracle.json').read_text(encoding='utf8'))['cards']
        cls.rows = {r['name']: r for r in registry()}
        cls.pins = family.pins()

    def declaration(self, name, variants):
        return dict(family=family.FAMILY, version=2, params=dict(schema=10, **family.oracle_model(self.oracle[name])),
                    oracle_text=self.oracle[name]['oracle_text'], variants=variants)

    def case(self, name, mode='dies', player=0):
        d = self.declaration(name, [f'{mode}_{player}'])
        return public_expand(name, self.rows[name]['csvid'], d, self.oracle[name], self.rows, self.oracle)[0]

    def has(self, steps, **fields):
        return any(all(s.get(k) == v for k, v in fields.items()) for s in steps)

    def count(self, steps, name, player, zone, value):
        return self.has(steps, op='assert_zone_multiplicity', player=player, zone=zone,
                        index=self.rows[name]['csvid'], value=value)

    def after_cast(self, case, ref):
        s = case['steps']
        return s[next(i for i, x in enumerate(s) if x['op'] == 'cast' and x['ref'] == ref)+1:]

    def test_whole_oracle_and_typed_parameters_reject_unmodeled_rules(self):
        for name in self.pins['sources']:
            d = self.declaration(name, ['dies_0']); family.spec(d, self.oracle[name])
            for key, value in [('schema', True), ('destination', 'hand' if d['params']['destination'] != 'hand' else 'top'), ('keywords', [])]:
                if key == 'keywords' and not d['params']['keywords']:
                    continue
                bad = copy.deepcopy(d); bad['params'][key] = value
                with self.assertRaises(ValueError):
                    family.spec(bad, self.oracle[name])
            for suffix in ('\n{T}: Draw a card.', '\nWhen this creature enters, gain 1 life.'):
                with self.assertRaises(ValueError):
                    family.oracle_model(dict(self.oracle[name], oracle_text=self.oracle[name]['oracle_text']+suffix))
        for name in ('Arashin Sovereign', 'Bogardan Phoenix', 'Ashcloud Phoenix'):
            with self.assertRaises(ValueError):
                family.oracle_model(self.oracle[name])

    def test_full_source_and_witness_implementation_pins(self):
        family.validate_witnesses(self.oracle, self.rows)
        for name, key, value in [('Withstand Death', 'oracle_text', 'Draw a card.'), ('Dragonscale Boon', 'mana_cost', '{G}')]:
            o = copy.deepcopy(self.oracle); o[name][key] = value
            with self.assertRaises(ValueError):
                family.validate_witnesses(o, self.rows)
        rows = copy.deepcopy(self.rows); rows['Reach Through Mists']['handler'] = 'NULL'
        with self.assertRaises(ValueError):
            family.validate_witnesses(self.oracle, rows)

    def test_real_death_destination_and_living_mob_receipt(self):
        for name in self.pins['sources']:
            destination = family.oracle_model(self.oracle[name])['destination']
            for player in (0, 1):
                c = self.case(name, player=player); s = c['steps']
                self.assertEqual(next(x['name'] for x in c['setup'] if x['ref'] == 'death_spell'), 'Murder')
                self.assertTrue(self.has(s, op='assert_card', ref='observer', property='plus1_counters', value=1))
                self.assertTrue(self.count(s, name, player, 'graveyard', 0))
                self.assertTrue(self.count(s, name, player, 'hand' if destination == 'hand' else 'library' if destination in ('top', 'shuffle') else 'exile', 1))
                self.assertFalse(any(x['op'] in ('destroy', 'choose_trigger_source', 'choose_dialog') for x in s))
                tail = self.after_cast(c, 'death_spell'); resolve = next(i for i, x in enumerate(tail) if x['op'] == 'resolve')
                self.assertFalse(any(x['op'] in ('assert_card', 'require_card', 'bind_permanent') and x.get('ref') == 'source' for x in tail[resolve+1:]))

    def test_mummy_exile_still_requires_actual_death_in_both_fixture_orders(self):
        for player in (0, 1):
            for mode in ('dies', 'observer_first'):
                c = self.case('Cyclopean Mummy', mode, player); s = c['steps']
                refs = [x['ref'] for x in c['setup']]
                self.assertEqual(refs.index('observer') < refs.index('source'), mode == 'observer_first')
                self.assertTrue(self.count(s, 'Cyclopean Mummy', player, 'exile', 1))
                self.assertTrue(self.has(s, op='assert_card', ref='observer', property='plus1_counters', value=1))
                self.assertTrue(self.has(s, op='assert_unused_choices', value=0))

    def test_owner_destinations_are_independent_of_death_time_controller(self):
        for name in self.pins['sources']:
            d = family.oracle_model(self.oracle[name])['destination']
            zone = 'library' if d in ('top', 'shuffle') else d
            for player in (0, 1):
                c = self.case(name, 'foreign_owner', player); s = c['steps']
                self.assertIn('opponent_owned', next(x['state'] for x in c['setup'] if x['ref'] == 'source'))
                self.assertTrue(self.count(s, name, 1-player, zone, 1))
                self.assertTrue(self.count(s, name, player, zone, 0))
                self.assertTrue(self.has(s, op='assert_card', ref='observer', property='plus1_counters', value=1))

    def test_top_is_ordered_and_real_draw_returns_source_to_hand(self):
        for player in (0, 1):
            s = self.case('Undying Beast', player=player)['steps']
            self.assertTrue(self.has(s, op='assert_zone_card', player=player, zone='library', index=0, value=2836))
            self.assertTrue(self.has(s, op='assert_shuffle_count', player=player, value=0))
            c = self.case('Undying Beast', 'draw_returned', player); s = c['steps']
            self.assertTrue(self.has(s, op='cast', ref='draw_returned', value=1))
            self.assertTrue(self.count(s, 'Undying Beast', player, 'hand', 1))
            self.assertTrue(self.count(s, 'Undying Beast', player, 'library', 0))
            self.assertTrue(self.count(s, 'Reach Through Mists', player, 'graveyard', 1))

    def test_shuffle_uses_exact_multiset_and_owner_shuffle_receipt(self):
        for player in (0, 1):
            c = self.case('Alabaster Dragon', 'foreign_owner', player); s = c['steps']; owner = 1-player
            self.assertTrue(self.has(s, op='assert_shuffle_count', player=owner, value=1))
            self.assertTrue(self.has(s, op='assert_shuffle_count', player=player, value=0))
            self.assertTrue(self.count(s, 'Alabaster Dragon', owner, 'library', 1))
            self.assertFalse(any(x['op'] == 'assert_zone_card' and x.get('zone') == 'library' and x.get('player') == owner for x in s))

    def test_token_ceases_but_specific_object_shuffle_still_shuffles(self):
        for name in self.pins['sources']:
            for player in (0, 1):
                c = self.case(name, 'token_death', player); s = c['steps']
                self.assertIn('token', next(x['state'] for x in c['setup'] if x['ref'] == 'source'))
                for zone in ('hand', 'library', 'graveyard', 'exile', 'battlefield'):
                    self.assertTrue(self.count(s, name, player, zone, 0))
                self.assertTrue(self.has(s, op='assert_card', ref='observer', property='plus1_counters', value=1))
                self.assertTrue(self.has(s, op='assert_shuffle_count', player=player, value=int(name == 'Alabaster Dragon')))

    def test_countered_creature_and_direct_bounce_exile_never_die(self):
        for name in self.pins['sources']:
            for mode, zone in [('countered', 'graveyard'), ('bounce', 'hand'), ('exile', 'exile')]:
                c = self.case(name, mode); s = c['steps']
                self.assertTrue(self.count(s, name, 0, zone, 1))
                self.assertTrue(self.has(s, op='assert_card', ref='observer', property='plus1_counters', value=0))
                self.assertFalse(self.has(s, op='assert_card', ref='observer', property='plus1_counters', value=1))
                self.assertFalse(self.has(s, op='assert_shuffle_count', player=0, value=1))
            s = self.case(name, 'countered')['steps']; at = next(i for i, x in enumerate(s) if x['op'] == 'announce_spell')
            self.assertEqual(s[at+1], dict(op='cast_response', ref='counter', value=1))

    def test_recast_pays_again_and_fresh_object_loses_real_pump_counters_damage(self):
        for name in ('Endless Cockroaches', 'Shivan Phoenix', 'Weatherseed Treefolk', 'Undying Beast'):
            c = self.case(name, 'new_object'); s = c['steps']
            self.assertTrue(self.has(s, op='cast', ref='body_growth', value=1))
            self.assertTrue(self.has(s, op='cast', ref='body_counters', value=1))
            self.assertTrue(self.has(s, op='cast', ref='body_damage', value=1))
            self.assertTrue(self.has(s, op='assert_card', ref='source', property='plus1_counters', value=2))
            self.assertTrue(self.has(s, op='assert_card', ref='source', property='damage', value=2))
            bind = next(i for i, x in enumerate(s) if x['op'] == 'bind_hand'); tail = s[bind:]
            self.assertEqual(s[bind]['ref'], 'returned_1')
            self.assertTrue(self.has(tail, op='cast', ref='returned_1', value=1))
            for prop, value in [('plus1_counters', 0), ('damage', 0), ('sick', 1), ('power', int(self.oracle[name]['power']))]:
                self.assertTrue(self.has(tail, op='assert_card', ref='returned_1', property=prop, value=value))

    def test_foreign_recast_requires_real_owner_turn_and_owner_hand_binding(self):
        for name in ('Endless Cockroaches', 'Undying Beast'):
            for player in (0, 1):
                s = self.case(name, 'recast_foreign_owner', player)['steps']; bind = next(i for i, x in enumerate(s) if x['op'] == 'bind_hand')
                self.assertEqual(s[bind]['player'], 1-player)
                at = next(i for i, x in enumerate(s) if x['op'] == 'advance_turn')
                self.assertEqual(s[at:at+3], [dict(op='advance_turn'), dict(op='untap_step', player=1-player), dict(op='turn', player=1-player, value=0)])
                self.assertLess(at, bind)

    def test_fresh_recast_attack_is_denied_and_repeat_earns_second_death(self):
        for name in ('Endless Cockroaches', 'Shivan Phoenix', 'Weatherseed Treefolk', 'Undying Beast'):
            for player in (0, 1):
                s = self.case(name, 'fresh_attack', player)['steps']
                self.assertTrue(self.has(s, op='attack', ref='returned_1', value=0))
                self.assertFalse(any(x['op'] == 'advance_turn' for x in s))
                s = self.case(name, 'repeat', player)['steps']
                self.assertTrue(self.has(s, op='cast', ref='second_death', value=1))
                self.assertTrue(self.has(s, op='assert_card', ref='observer', property='plus1_counters', value=2))

    def test_real_flying_and_trample_before_death_after_paid_recast(self):
        for name in ('Shivan Phoenix', 'Weatherseed Treefolk', 'Alabaster Dragon'):
            for player in (0, 1):
                modes = ['keyword_before']+(['keyword_after'] if name != 'Alabaster Dragon' else [])
                for mode in modes:
                    c = self.case(name, mode, player); s = c['steps']; ref = 'returned_1' if mode.endswith('after') else 'source'
                    self.assertTrue(self.has(s, op='block', ref='blocker', other=ref, value=int(name == 'Weatherseed Treefolk')))
                    self.assertTrue(self.has(s, op='combat_damage'))
                    self.assertEqual(sum(x['op'] == 'advance_turn' for x in s), 2)
                    if mode.endswith('after'):
                        self.assertTrue(self.has(s, op='cast', ref='returned_1', value=1))
                    self.assertTrue(self.has(s, op='assert_life', player=1-player, value=16 if name != 'Shivan Phoenix' else 17))
        for mode in ('reach_before', 'reach_after'):
            c = self.case('Shivan Phoenix', mode); s = c['steps']
            self.assertTrue(self.has(s, op='cast', ref='combat_growth', value=1))
            self.assertTrue(self.count(s, 'Giant Spider', 1, 'graveyard', 1))

    def test_simultaneous_protects_only_observer_and_counts_controlled_deaths(self):
        for name in self.pins['sources']:
            for player in (0, 1):
                c = self.case(name, 'mixed_simultaneous', player); s = c['steps']
                self.assertEqual(next(x['name'] for x in c['setup'] if x['ref'] == 'protect_observer'), 'Withstand Death')
                self.assertTrue(self.has(s, op='choose_target', player=player, ref='observer'))
                self.assertEqual(next(x['player'] for x in c['setup'] if x['ref'] == 'sweep'), player)
                self.assertTrue(self.count(s, 'Eager Cadet', player, 'graveyard', 1))
                self.assertTrue(self.count(s, 'Eager Cadet', 1-player, 'graveyard', 1))
                self.assertTrue(self.has(s, op='assert_card', ref='observer', property='plus1_counters', value=2))

    def test_empty_and_unrelated_boundaries(self):
        for name in self.pins['sources']:
            s = self.case(name, 'empty_library')['steps']
            dest = family.oracle_model(self.oracle[name])['destination']
            self.assertTrue(self.has(s, op='assert_zone', player=0, zone='library', value=int(dest in ('top', 'shuffle'))))
            for mode, count in [('other_dies', 1), ('opponent_dies', 0)]:
                s = self.case(name, mode)['steps']
                self.assertTrue(self.has(s, op='assert_card', ref='source', property='in_play', value=1))
                self.assertTrue(self.has(s, op='assert_card', ref='observer', property='plus1_counters', value=count))

    def test_snapshot_boundaries_follow_actual_paid_cast_removal(self):
        for name in self.pins['sources']:
            for mode, op in [('ready_snapshot', 'cast'), ('pending_snapshot', 'resolve'), ('death_snapshot', 'resolve')]:
                s = self.case(name, mode)['steps']; at = next(i for i, x in enumerate(s) if x['op'] == 'capture')
                self.assertEqual(s[at+1]['op'], op)
                self.assertEqual(s[-1], dict(op='assert_recorded_state'))
                self.assertFalse(any(x['op'].startswith('choose_trigger') for x in s))

    def test_all_reserved_full_oracle_variants_expand(self):
        totals = {}
        for name in self.pins['sources']:
            d = self.declaration(name, ['dies_0']); d['variants'] = family.variants(d['params'])
            cases = public_expand(name, self.rows[name]['csvid'], d, self.oracle[name], self.rows, self.oracle)
            self.assertEqual(len(cases), len(d['variants']))
            self.assertTrue(all(c['_recipe']['oracle_text'] == self.oracle[name]['oracle_text'] for c in cases))
            totals[self.rows[name]['csvid']] = len(cases)
        self.assertEqual(sum(v for csvid, v in totals.items() if csvid in (2822, 4055, 4086, 2836, 2927, 606)), 370)
        if 13012 in totals:
            self.assertEqual({csvid: totals[csvid] for csvid in (13012, 12697, 19139)}, {13012: 62, 12697: 72, 19139: 70})

    def test_mortus_complete_cost_and_real_paid_recast(self):
        self.assertEqual(family.oracle_model(self.oracle['Mortus Strider']), family.oracle_model(self.oracle['Endless Cockroaches']))
        for player in (0, 1):
            s = self.case('Mortus Strider', 'recast', player)['steps']
            bind = next(i for i, x in enumerate(s) if x['op'] == 'bind_hand')
            cast = next(i for i, x in enumerate(s) if x['op'] == 'cast' and x['ref'] == 'returned_1')
            for color, amount in [(0, 1), (COLORS['U'], 1), (COLORS['B'], 1)]:
                self.assertTrue(self.has(s[bind:cast], op='set_mana', player=player, color=color, value=amount))
                self.assertTrue(self.has(s[cast:], op='assert_mana', player=player, color=color, value=0))
            self.assertTrue(self.has(s[cast:], op='assert_card', ref='returned_1', property='sick', value=1))
            denied = self.case('Mortus Strider', 'wrong_color', player)['steps']
            self.assertTrue(self.has(denied, op='cast', ref='source', value=0))

    def test_dark_revenant_top_draw_recast_and_real_flying(self):
        self.assertEqual(family.oracle_model(self.oracle['Dark Revenant']), dict(destination='top', keywords=['Flying']))
        for player in (0, 1):
            c = self.case('Dark Revenant', 'dies', player); s = c['steps']
            self.assertTrue(self.has(s, op='assert_zone_card', player=player, zone='library', index=0, value=12697))
            self.assertTrue(self.has(s, op='assert_shuffle_count', player=player, value=0))
            c = self.case('Dark Revenant', 'keyword_after', player); s = c['steps']
            refs = [x['ref'] for x in s if x['op'] == 'cast']
            self.assertLess(refs.index('draw_returned'), refs.index('returned_1'))
            self.assertTrue(self.has(s, op='block', ref='blocker', other='returned_1', value=0))
            self.assertTrue(self.has(s, op='combat_damage'))
            self.assertTrue(self.has(s, op='assert_life', player=1-player, value=18))
            self.assertTrue(self.has(s, op='assert_card', ref='observer', property='plus1_counters', value=2))

    def test_immortal_phoenix_safe_reach_uses_printed_five_three_without_pump(self):
        self.assertEqual(family.oracle_model(self.oracle['Immortal Phoenix']), family.oracle_model(self.oracle['Shivan Phoenix']))
        for player in (0, 1):
            c = self.case('Immortal Phoenix', 'reach_after', player); s = c['steps']
            self.assertNotIn('combat_growth', [x['ref'] for x in c['setup']])
            self.assertTrue(self.has(s, op='block', ref='blocker', other='returned_1', value=1))
            self.assertTrue(self.has(s, op='assert_card', ref='returned_1', property='power', value=5))
            self.assertTrue(self.has(s, op='assert_card', ref='returned_1', property='toughness', value=3))
            self.assertTrue(self.has(s, op='assert_card', ref='returned_1', property='damage', value=2))
            self.assertTrue(self.count(s, 'Giant Spider', 1-player, 'graveyard', 1))
            self.assertTrue(self.count(s, 'Immortal Phoenix', player, 'hand', 1))
            self.assertEqual(sum(x['op'] == 'advance_turn' for x in s), 2)

    def test_follow3_owner_provenance_preserves_true_owner_and_death_observer(self):
        for name, zone in [('Mortus Strider', 'hand'), ('Dark Revenant', 'library'), ('Immortal Phoenix', 'hand')]:
            for player in (0, 1):
                c = self.case(name, 'foreign_owner', player); s = c['steps']
                self.assertIn('opponent_owned', next(x['state'] for x in c['setup'] if x['ref'] == 'source'))
                self.assertTrue(self.count(s, name, 1-player, zone, 1))
                self.assertTrue(self.count(s, name, 1-player, 'graveyard', 0))
                self.assertTrue(self.has(s, op='assert_card', ref='observer', property='plus1_counters', value=1))


if __name__ == '__main__':
    unittest.main()
