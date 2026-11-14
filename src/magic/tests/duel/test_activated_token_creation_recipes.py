"""Oracle-independent assertions for activated-token costs and live receipts."""
import copy
import json
from pathlib import Path
import unittest

from generate_cases import registry
import activated_token_creation_recipes as model


class ActivatedTokenCreationRecipesTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.oracle = json.loads((Path(__file__).parent / 'data/oracle.json').read_text(encoding='utf8'))['cards']
        cls.rows = {r['name']: r for r in registry()}

    def declaration(self, name, variant='create_0'):
        source = model.pins()['sources'][name]
        return dict(family=model.FAMILY, version=2, oracle_text=self.oracle[name]['oracle_text'],
                    params=dict(ability=source['ability'], interface=copy.deepcopy(source['interface']),
                                inactive_clauses=[line for i, line in enumerate(self.oracle[name]['oracle_text'].splitlines()) if i != source['ability']]),
                    variants=[variant])

    def case(self, name, variant='create_0'):
        declaration = self.declaration(name, variant)
        p = model.spec(declaration, self.oracle[name])
        return model.expand(name, self.rows[name]['csvid'], declaration, p, [variant], self.rows, self.oracle)[0]

    def last(self, case, op, **fields):
        return [s for s in case['steps'] if s['op'] == op and all(s.get(k) == v for k, v in fields.items())][-1]['value']

    def test_eight_creators_independently_determine_actual_token_characteristics(self):
        expected = {
            'Selesnya Guildmage': (1, 1, 1, 8, 0, 'Saproling'),
            'Icatian Crier': (2, 1, 1, 32, 0, 'Citizen'),
            'Zombie Infestation': (1, 2, 2, 2, 0, 'Zombie'),
            'Squirrel Wrangler': (2, 1, 1, 8, 0, 'Squirrel'),
            'Garruk, Apex Predator': (1, 3, 3, 2, 0, 'Beast'),
            'Xenagos, the Reveler': (1, 2, 2, 24, 0, 'Satyr'),
            'Dovin, Grand Arbiter': (1, 1, 1, 0, 1, 'Thopter'),
            'Nissa, Voice of Zendikar': (1, 0, 1, 8, 0, 'Plant'),
        }
        for name, (count, power, toughness, color, artifact, subtype) in expected.items():
            for owner in (0, 1):
                with self.subTest(name=name, owner=owner):
                    case = self.case(name, f'create_{owner}'); cid = self.rows[subtype]['csvid']
                    self.assertEqual(self.last(case, 'assert_zone_multiplicity', player=owner, zone='battlefield', index=cid), count)
                    for prop, value in [('token', 1), ('power', power), ('toughness', toughness), ('color', color), ('artifact', artifact), ('subtype_' + subtype, 1)]:
                        self.assertEqual(self.last(case, 'assert_matching_permanents', player=owner, index=cid, property=prop), value)

    def test_generic_token_oracle_is_identity_only_and_deliberately_disagrees(self):
        self.assertEqual(self.oracle['Beast']['power'], '4')
        self.assertEqual(self.oracle['Satyr']['power'], '1')
        self.assertEqual(self.oracle['Thopter']['colors'], ['U'])
        self.assertEqual(self.oracle['Citizen']['colors'], ['G', 'W'])
        model.validate_witnesses(self.oracle, self.rows)

    def test_loyalty_is_acquired_by_real_cast_then_paid_before_tokens_exist(self):
        for name, initial, after in [('Garruk, Apex Predator', 5, 6), ('Xenagos, the Reveler', 3, 3),
                                     ('Dovin, Grand Arbiter', 3, 2), ('Nissa, Voice of Zendikar', 3, 4)]:
            for owner in (0, 1):
                c = self.case(name, f'pending_snapshot_{owner}'); steps = c['steps']
                cast = next(i for i, s in enumerate(steps) if s['op'] == 'cast' and s['ref'] == 'source')
                activated = next(i for i, s in enumerate(steps) if s['op'] == 'activate')
                captured = next(i for i, s in enumerate(steps) if s['op'] == 'capture')
                self.assertLess(cast, activated)
                self.assertIn(dict(op='assert_counter', ref='source', counter_kind='loyalty', value=initial), steps[cast:activated])
                self.assertIn(dict(op='assert_counter', ref='source', counter_kind='loyalty', value=after), steps[activated:captured])
                self.assertTrue(any(s['op'] == 'assert_zone_multiplicity' and s['zone'] == 'battlefield' and s['player'] == owner and s['value'] == 0 for s in steps[activated:captured]))
                self.assertFalse(any(s['op'].startswith('set_') and 'counter' in str(s) for s in steps))

    def test_dovin_life_rider_is_resolution_only_and_restored(self):
        c = self.case('Dovin, Grand Arbiter', 'pending_snapshot_1'); s = c['steps']; cut = next(i for i, x in enumerate(s) if x['op'] == 'capture')
        self.assertIn(dict(op='assert_life', player=1, value=20), s[:cut])
        self.assertEqual(self.last(c, 'assert_life', player=1), 21)
        self.assertEqual(sum(x['op'] == 'assert_recorded_state' for x in s), 1)

    def test_ordinary_exact_activation_payments_leave_zero(self):
        for name in ('Selesnya Guildmage', 'Icatian Crier', 'Squirrel Wrangler', 'Zombie Infestation'):
            for owner in (0, 1):
                c = self.case(name, f'payment_{owner}')
                self.assertEqual(self.last(c, 'assert_mana_total', player=owner), 0)
                self.assertEqual([self.last(c, 'assert_mana', player=owner, color=color) for color in range(6)], [0] * 6)

    def test_hybrid_source_cast_is_real_legal_GG_and_separate_from_activation(self):
        c = self.case('Selesnya Guildmage', 'cast_0'); steps = c['steps']; cast = next(i for i, s in enumerate(steps) if s['op'] == 'cast')
        self.assertEqual([s['value'] for s in steps[:cast] if s['op'] == 'set_mana'], [0, 0, 0, 2, 0, 0])
        self.assertIn(dict(op='assert_mana_total', player=0, value=0), steps[cast:])
        self.assertEqual(self.last(c, 'assert_mana_total', player=0), 76)

    def test_discard_costs_move_exactly_one_or_two_cards_before_resolution(self):
        for name, discarded in [('Icatian Crier', 1), ('Zombie Infestation', 2)]:
            for owner in (0, 1):
                c = self.case(name, f'pending_snapshot_{owner}'); s = c['steps']; cut = next(i for i, x in enumerate(s) if x['op'] == 'capture')
                self.assertIn(dict(op='assert_zone', player=owner, zone='hand', value=4 - discarded), s[:cut])
                self.assertIn(dict(op='assert_zone_multiplicity', player=owner, zone='graveyard', index=self.rows['Plains']['csvid'], value=discarded), s[:cut])

    def test_discard_shortage_does_not_pay_partial_cost_or_create(self):
        for name in ('Icatian Crier', 'Zombie Infestation'):
            for mode in (('empty_hand', 'bare_zones') if name == 'Icatian Crier' else ('empty_hand', 'short_hand', 'bare_zones')):
                c = self.case(name, mode + '_0')
                self.assertIn(dict(op='require_can_activate', ref='source', value=0), c['steps'])
                self.assertIn(dict(op='activate', ref='source', value=0), c['steps'])
                self.assertEqual(self.last(c, 'assert_zone_multiplicity', player=0, zone='graveyard', index=self.rows['Plains']['csvid']), 0)

    def test_land_cost_is_own_tapped_land_and_enemy_is_untouched(self):
        for owner in (0, 1):
            c = self.case('Squirrel Wrangler', f'pending_snapshot_{owner}'); steps = c['steps']; cut = next(i for i, s in enumerate(steps) if s['op'] == 'capture')
            for ref in ('victim', 'enemy_land'):
                self.assertEqual(next(x for x in c['setup'] if x['ref'] == ref)['state'], ['tapped'])
            self.assertIn(dict(op='assert_zone_multiplicity', player=owner, zone='battlefield', index=self.rows['Forest']['csvid'], value=0), steps[:cut])
            self.assertIn(dict(op='require_card', ref='enemy_land', property='in_play', value=1), steps[:cut])
            targets = [s['ref'] for s in steps[:cut] if s['op'] == 'choose_target']
            self.assertEqual(targets, ['victim'] if owner == 0 else [])

    def test_enemy_land_cannot_pay_and_source_creature_is_not_a_land(self):
        c = self.case('Squirrel Wrangler', 'no_own_land_1')
        self.assertFalse(any(x['ref'] == 'victim' for x in c['setup']))
        self.assertIn(dict(op='require_can_activate', ref='source', value=0), c['steps'])
        self.assertEqual(self.last(c, 'require_card', ref='enemy_land', property='in_play'), 1)

    def test_fresh_tap_cost_is_denied_but_nontap_fresh_source_works(self):
        denied = self.case('Icatian Crier', 'cast_1')
        self.assertIn(dict(op='cast', ref='source', value=1), denied['steps'])
        self.assertIn(dict(op='activate', ref='source', value=0), denied['steps'])
        self.assertIn(dict(op='activate', ref='source', value=1), self.case('Selesnya Guildmage', 'cast_1')['steps'])

    def test_tapped_tap_source_denied_but_nontap_source_not_blocked(self):
        self.assertIn(dict(op='activate', ref='source', value=0), self.case('Icatian Crier', 'tapped_0')['steps'])
        self.assertIn(dict(op='activate', ref='source', value=1), self.case('Squirrel Wrangler', 'tapped_0')['steps'])

    def test_loyalty_own_main_only_ordinary_activations_offturn_allowed(self):
        self.assertIn(dict(op='activate', ref='source', value=0), self.case('Xenagos, the Reveler', 'offturn_1')['steps'])
        self.assertIn(dict(op='activate', ref='source', value=1), self.case('Selesnya Guildmage', 'offturn_1')['steps'])

    def test_once_per_turn_loyalty_and_spent_tap_or_land_cost_are_not_reused(self):
        for name in ('Xenagos, the Reveler', 'Icatian Crier', 'Squirrel Wrangler'):
            c = self.case(name, 'repeat_0')
            self.assertEqual([s['value'] for s in c['steps'] if s['op'] == 'activate'], [1, 0])
        c = self.case('Zombie Infestation', 'repeat_0')
        self.assertEqual([s['value'] for s in c['steps'] if s['op'] == 'activate'], [1, 1])
        self.assertEqual(self.last(c, 'assert_zone_multiplicity', player=0, zone='battlefield', index=self.rows['Zombie']['csvid']), 2)
        self.assertEqual(self.last(c, 'assert_zone', player=0, zone='hand'), 0)

    def test_destroying_source_while_pending_preserves_token_effect(self):
        for name in model.pins()['sources']:
            c = self.case(name, 'source_destroyed_pending_1')
            self.assertIn(dict(op='destroy', ref='source'), c['steps'])
            self.assertEqual(self.last(c, 'assert_zone_multiplicity', player=1, zone='battlefield', index=self.rows[name]['csvid']), 0)
            destroyed = next(i for i, s in enumerate(c['steps']) if s['op'] == 'destroy')
            self.assertFalse(any(s.get('ref') == 'source' and s['op'] == 'require_card' for s in c['steps'][destroyed:]))
            self.assertTrue(any(s['op'] == 'assert_matching_permanents' and s['property'] == 'token' and s['value'] == 1 for s in c['steps']))

    def test_actual_snapshots_replay_activation_or_pending_resolution(self):
        for mode, activations in [('ready_snapshot', 2), ('pending_snapshot', 1), ('resolved_snapshot', 1)]:
            c = self.case('Nissa, Voice of Zendikar', mode + '_0'); s = c['steps']
            self.assertEqual(sum(x['op'] == 'activate' for x in s), activations)
            for op in ('capture', 'restore', 'ai_roundtrip', 'assert_recorded_state'):
                self.assertEqual(sum(x['op'] == op for x in s), 1)

    def test_haste_is_actual_fresh_attack_permission_not_fabricated_sick_bit(self):
        c = self.case('Xenagos, the Reveler', 'fresh_attack_1')
        self.assertIn(dict(op='attack', ref='token', value=1), c['steps'])
        self.assertEqual(self.last(c, 'assert_life', player=0), 18)
        self.assertIn(dict(op='attack', ref='token', value=0), self.case('Dovin, Grand Arbiter', 'fresh_attack_0')['steps'])
        self.assertFalse(any('sick' == s.get('property') and s.get('value') == 0 for s in c['steps']))

    def test_satyr_can_block_despite_generic_proxy_cannot_block_clause(self):
        c = self.case('Xenagos, the Reveler', 'block_ground_0')
        self.assertIn(dict(op='block', ref='token', other='witness', value=1), c['steps'])
        self.assertEqual(self.last(c, 'assert_zone_multiplicity', player=1, zone='battlefield', index=self.rows['Grizzly Bears']['csvid']), 1)

    def test_garruk_deathtouch_kills_nonlethal_four_toughness_blocker_and_survives(self):
        c = self.case('Garruk, Apex Predator', 'deathtouch_1')
        self.assertEqual(next(x for x in c['setup'] if x['ref'] == 'witness')['name'], 'Giant Spider')
        self.assertEqual(self.last(c, 'assert_zone_multiplicity', player=0, zone='battlefield', index=self.rows['Giant Spider']['csvid']), 0)
        self.assertEqual(self.last(c, 'require_card', ref='expected_token_0', property='damage'), 2)
        self.assertIn(dict(op='attack', ref='token', value=1), self.case('Garruk, Apex Predator', 'ready_attack_1')['steps'])

    def test_thopter_flying_blocks_flying_and_evades_ground_but_reach_blocks(self):
        for mode, blocker, blocks in [('block_flying', 'token', 1), ('evasion', 'witness', 0), ('reach_blocker', 'witness', 1)]:
            c = self.case('Dovin, Grand Arbiter', mode + '_0')
            self.assertIn(dict(op='block', ref=blocker, other='witness' if blocker == 'token' else 'token', value=blocks), c['steps'])

    def test_zero_power_plant_blocks_and_dies_without_damaging_witness(self):
        c = self.case('Nissa, Voice of Zendikar', 'block_ground_0')
        self.assertEqual(self.last(c, 'require_card', ref='witness', property='damage'), 0)
        self.assertEqual(self.last(c, 'assert_zone_multiplicity', player=0, zone='battlefield', index=self.rows['Plant']['csvid']), 0)
        self.assertEqual(self.last(c, 'assert_life', player=0), 20)

    def test_removed_tokens_leave_no_hand_grave_or_exile_card(self):
        for mode in ('destroy_token', 'bounce_token', 'exile_token'):
            c = self.case('Dovin, Grand Arbiter', mode + '_1'); cid = self.rows['Thopter']['csvid']
            for zone in ('hand', 'graveyard', 'exile', 'battlefield'):
                self.assertEqual(self.last(c, 'assert_zone_multiplicity', player=1, zone=zone, index=cid), 0)
            self.assertEqual(self.last(c, 'assert_life', player=1), 22 if mode == 'exile_token' else 21)

    def test_no_cost_tap_or_token_state_setters_and_all_fixture_tap_states_explicit(self):
        for name in model.pins()['sources']:
            c = self.case(name)
            self.assertTrue(all('state' in x and bool(set(x['state']) & {'tapped', 'untapped'}) for x in c['setup'] if x['zone'] == 'battlefield'))
            self.assertFalse(any(s['op'] in ('set_counter', 'set_power', 'set_toughness', 'set_tapped', 'add_token') for s in c['steps']))

    def test_family_version_variant_and_params_fail_closed(self):
        name = 'Dovin, Grand Arbiter'
        for field, bad in [('family', 'token_creation'), ('version', True), ('version', 1), ('variants', []),
                           ('variants', ['create_0', 'create_0']), ('variants', ['bogus_0']), ('variants', [1]), ('variants', 'create_0')]:
            d = self.declaration(name); d[field] = bad
            with self.subTest(field=field, bad=bad), self.assertRaises(ValueError):
                model.spec(d, self.oracle[name])
        d = self.declaration(name); d['params']['token'] = {'power': 4}
        with self.assertRaises(ValueError):
            model.spec(d, self.oracle[name])

    def test_whole_oracle_gaps_and_ui_cannot_be_silently_changed(self):
        name = 'Squirrel Wrangler'
        for change in ('text', 'gap', 'ui', 'ability'):
            d = self.declaration(name)
            if change == 'text': d['oracle_text'] += '\nDraw a card.'
            if change == 'gap': d['params']['inactive_clauses'] = []
            if change == 'ui': d['params']['interface']['dialogs']['1'] = 1
            if change == 'ability': d['params']['ability'] = True
            with self.subTest(change=change), self.assertRaises(ValueError):
                model.spec(d, self.oracle[name])

    def test_parser_rejects_partial_delayed_and_unobservable_effects(self):
        for text in ['Create a 1/1 green Plant creature token. Draw a card.',
                     'Create a 1/1 green Plant creature token at the beginning of the next end step.',
                     'Create a 1/1 green Cleric enchantment creature token.',
                     'Create a 1/1 red Goblin creature token with menace.']:
            with self.subTest(text=text), self.assertRaises(ValueError):
                model.creation(text)

    def test_independently_pinned_witness_changes_are_rejected(self):
        changed = copy.deepcopy(self.oracle); changed['Giant Spider']['toughness'] = '3'
        with self.assertRaises(ValueError):
            model.validate_witnesses(changed, self.rows)

    def test_consumed_hand_slots_are_not_retained_as_live_entity_handles(self):
        c = self.case('Icatian Crier', 'create_0')
        self.assertFalse(any(s['op'] == 'require_card' and s.get('ref', '').startswith('hand_') for s in c['steps']))

    def test_initial_creators_are_distinct_implemented_normal_primaries(self):
        ids = []
        for name, source in model.pins()['sources'].items():
            row = self.rows[name]; ids.append(source['csvid'])
            self.assertEqual(row['csvid'], source['csvid'])
            self.assertEqual(row['kind'], 'card')
            self.assertTrue(row.get('handler'))
            self.assertNotIn('dummy', row['handler'].lower())
            self.assertEqual(source['oracle']['layout'], 'normal')
            self.assertFalse(source['oracle']['type_line'].startswith('Token '))
        self.assertEqual(len(ids), len(set(ids)))

    def test_preloaded_combat_witness_is_present_in_pending_and_final_zone_ledgers(self):
        for owner in (0, 1):
            c = self.case('Garruk, Apex Predator', f'deathtouch_{owner}'); steps = c['steps']
            activated = next(i for i, s in enumerate(steps) if s['op'] == 'activate')
            resolved = next(i for i in range(activated + 1, len(steps)) if steps[i]['op'] == 'resolve')
            pending = steps[activated:resolved]
            self.assertIn(dict(op='assert_zone', player=1-owner, zone='battlefield', value=2), pending)
            self.assertIn(dict(op='require_card', ref='witness', property='in_play', value=1), pending)
            self.assertIn(dict(op='require_card', ref='witness', property='toughness', value=4), pending)
            self.assertEqual(self.last(c, 'assert_zone', player=1-owner, zone='battlefield'), 1)
            self.assertEqual(self.last(c, 'assert_zone', player=1-owner, zone='graveyard'), 2)

    def test_later_removal_spell_is_already_in_hand_during_pending_cost_receipts(self):
        for owner in (0, 1):
            for mode, spell_name in [('bounce_token', 'Unsummon'), ('exile_token', 'Swords to Plowshares')]:
                c = self.case('Icatian Crier', f'{mode}_{owner}'); steps = c['steps']
                activated = next(i for i, s in enumerate(steps) if s['op'] == 'activate')
                resolved = next(i for i in range(activated + 1, len(steps)) if steps[i]['op'] == 'resolve')
                pending = steps[activated:resolved]
                self.assertIn(dict(op='assert_zone', player=owner, zone='hand', value=4), pending)
                self.assertIn(dict(op='assert_zone_multiplicity', player=owner, zone='hand', index=self.rows[spell_name]['csvid'], value=1), pending)
                self.assertIn(dict(op='assert_zone_multiplicity', player=owner, zone='graveyard', index=self.rows['Plains']['csvid'], value=1), pending)
                self.assertEqual(self.last(c, 'assert_zone', player=owner, zone='hand'), 3)
                self.assertEqual(self.last(c, 'assert_zone_multiplicity', player=owner, zone='graveyard', index=self.rows[spell_name]['csvid']), 1)

    def test_AI_multi_discard_then_opponents_real_instant_preserves_both_costs(self):
        for mode, spell_name, spell_ref in [('bounce_token', 'Unsummon', 'bounce'),
                                           ('exile_token', 'Swords to Plowshares', 'exile')]:
            c = self.case('Zombie Infestation', mode + '_1'); steps = c['steps']
            spell = next(x for x in c['setup'] if x['ref'] == spell_ref)
            self.assertEqual((spell['player'], spell['zone']), (0, 'hand'))
            self.assertTrue(all(x['name'] == 'Plains' for x in c['setup'] if x['player'] == 1 and x['zone'] == 'hand'))
            self.assertIn(dict(op='choose_target', player=0, ref='token'), steps)
            self.assertIn(dict(op='cast', ref=spell_ref, value=1), steps)
            self.assertFalse(any(x['op'] == 'turn' and x['player'] == 0 for x in steps))
            self.assertEqual(self.last(c, 'assert_zone', player=1, zone='hand'), 2)
            self.assertEqual(self.last(c, 'assert_zone_multiplicity', player=1, zone='graveyard', index=self.rows['Plains']['csvid']), 2)
            self.assertEqual(self.last(c, 'assert_zone_multiplicity', player=0, zone='graveyard', index=self.rows[spell_name]['csvid']), 1)
            self.assertEqual(self.last(c, 'assert_mana_total', player=0), 79)
            self.assertEqual(self.last(c, 'assert_mana_total', player=1), 80)
            self.assertEqual(self.last(c, 'assert_life', player=1), 22 if mode == 'exile_token' else 20)

    def test_followon_three_soldiers_have_creator_count_and_one_loyalty_payment(self):
        for owner in (0, 1):
            c = self.case("Elspeth, Sun's Champion", f'create_{owner}')
            self.assertEqual(self.last(c, 'assert_zone_multiplicity', player=owner, zone='battlefield', index=self.rows['Soldier']['csvid']), 3)
            self.assertEqual(self.last(c, 'assert_counter', ref='source', counter_kind='loyalty'), 5)
            self.assertEqual(sum(s['op'] == 'activate' for s in c['steps']), 1)

    def test_followon_two_subtype_tokens_observe_both_printed_subtypes(self):
        for name, token_name, subtypes in [('Gideon, Ally of Zendikar', 'Knight Ally', ['Knight', 'Ally']),
                                           ('Thraben Standard Bearer', 'Human Soldier', ['Human', 'Soldier'])]:
            c = self.case(name, 'create_1'); cid = self.rows[token_name]['csvid']
            for subtype in subtypes:
                self.assertEqual(self.last(c, 'assert_matching_permanents', player=1, index=cid, property='subtype_' + subtype), 1)
            self.assertEqual(self.last(c, 'assert_matching_permanents', player=1, index=cid, property='color'), 32)

    def test_followon_negative_loyalty_and_flying_dragon_are_printed_not_generic(self):
        for owner in (0, 1):
            c = self.case('Sarkhan Unbroken', f'create_{owner}'); cid = self.rows['Dragon']['csvid']
            self.assertEqual(self.last(c, 'assert_counter', ref='source', counter_kind='loyalty'), 2)
            self.assertEqual(self.last(c, 'assert_matching_permanents', player=owner, index=cid, property='color'), 16)
            self.assertEqual(self.last(c, 'assert_matching_permanents', player=owner, index=cid, property='power'), 4)
            self.assertEqual(self.last(c, 'assert_matching_permanents', player=owner, index=cid, property='toughness'), 4)
            self.assertIn(dict(op='block', ref='witness', other='token', value=0), self.case('Sarkhan Unbroken', f'evasion_{owner}')['steps'])

    def test_retrofitter_tapped_and_repeat_budgets_are_exact_printed_two(self):
        for owner in (0, 1):
            for mode in ('tapped', 'repeat'):
                c = self.case('Retrofitter Foundry', f'{mode}_{owner}')
                budget = [s['value'] for s in c['steps'] if s['op'] == 'set_mana' and s['player'] == owner]
                self.assertEqual(budget, [2, 0, 0, 0, 0, 0])
                self.assertIn(dict(op='require_can_activate', ref='source', value=0), c['steps'])
                self.assertEqual([s['value'] for s in c['steps'] if s['op'] == 'activate'], [0] if mode == 'tapped' else [1, 0])
                self.assertEqual(self.last(c, 'assert_mana_total', player=owner), 2 if mode == 'tapped' else 0)
                self.assertEqual(self.last(c, 'assert_zone_multiplicity', player=owner, zone='battlefield', index=self.rows['Servo']['csvid']), 0 if mode == 'tapped' else 1)

    def test_retrofitter_auto_menus_are_inputs_not_oracle_mode_assertions(self):
        human = self.case('Retrofitter Foundry', 'create_0')
        self.assertIn(dict(op='choose_dialog', player=0, value=1), human['steps'])
        for variant in ('payment_0', 'repeat_0'):
            c = self.case('Retrofitter Foundry', variant)
            self.assertFalse(any(s['op'] in ('choose_dialog', 'choose_activation_mode', 'require_activation_mode') for s in c['steps']))
            self.assertIn(dict(op='activate', ref='source', value=1), c['steps'])
        # AUTOCHOOSE_IF_1 is explicitly ignored for the AI. With actual AI
        # presentation enabled, even the sole legal Servo option is a dialog.
        for variant in ('create_1', 'payment_1', 'cast_1', 'repeat_1'):
            c = self.case('Retrofitter Foundry', variant)
            self.assertEqual([s for s in c['steps'] if s['op'] == 'choose_dialog'], [dict(op='choose_dialog', player=1, value=1)])
            self.assertFalse(any(s['op'] in ('choose_activation_mode', 'require_activation_mode') for s in c['steps']))

    def test_cryptbreaker_manual_AI_discard_keeps_removal_in_opponents_hand(self):
        for mode, nm, ref in [('bounce_token', 'Unsummon', 'bounce'), ('exile_token', 'Swords to Plowshares', 'exile')]:
            c = self.case('Cryptbreaker', mode + '_1')
            self.assertEqual(next(x for x in c['setup'] if x['ref'] == ref)['player'], 0)
            self.assertIn(dict(op='choose_dialog', player=1, value=0), c['steps'])
            self.assertIn(dict(op='choose_target', player=0, ref='token'), c['steps'])
            self.assertEqual(self.last(c, 'assert_zone_multiplicity', player=1, zone='graveyard', index=self.rows['Plains']['csvid']), 1)
            self.assertEqual(self.last(c, 'assert_zone_multiplicity', player=0, zone='graveyard', index=self.rows[nm]['csvid']), 1)
            self.assertEqual(self.last(c, 'assert_mana_total', player=1), 78)
            self.assertEqual(self.last(c, 'assert_mana_total', player=0), 79)

    def test_cryptbreaker_draw_sibling_has_no_three_untapped_zombie_fixture(self):
        c = self.case('Cryptbreaker', 'repeat_1')
        self.assertFalse(any(x['name'] == 'Zombie' for x in c['setup']))
        self.assertEqual([s['value'] for s in c['steps'] if s['op'] == 'activate'], [1, 0])
        self.assertEqual(self.last(c, 'assert_zone_multiplicity', player=1, zone='battlefield', index=self.rows['Zombie']['csvid']), 1)
        self.assertEqual(self.last(c, 'require_card', ref='source', property='tapped'), 1)

    def test_new_interface_annotations_cannot_be_forged_in_declarations(self):
        for name, key, value in [('Retrofitter Foundry', 'exact_activation_budget_variants', ['create_0']),
                                  ('Retrofitter Foundry', 'silent_variants', ['create_0']),
                                  ('Cryptbreaker', 'manual_ai_discard', False)]:
            d = self.declaration(name); d['params']['interface'][key] = value
            with self.subTest(name=name, key=key), self.assertRaises(ValueError):
                model.spec(d, self.oracle[name])


if __name__ == '__main__':
    unittest.main()
