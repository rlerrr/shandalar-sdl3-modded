import json
import unittest
from pathlib import Path

from generate_cases import registry
from megamorph_target_effects_recipes import FACE_DOWN, FAMILY, classify, expand, profiles, spec, validate_witnesses, variants


class MegamorphTargetEffectsTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.oracle = json.loads((Path(__file__).parent / 'data/oracle.json').read_bytes())['cards']
        cls.registry = {row['name']: row for row in registry()}

    def case(self, name, scenario, owner):
        record = self.oracle[name]
        variant = f'{scenario}_{owner}'
        declaration = dict(family=FAMILY, version=2, oracle_text=record['oracle_text'],
                           params=classify(record), variants=[variant])
        case, = expand(name, self.registry[name]['csvid'], declaration, spec(declaration, record), [variant],
                       self.registry, self.oracle)
        return case

    def actions(self, case, op, **properties):
        return [action for action in case['steps'] if action['op'] == op
                and all(action.get(key) == value for key, value in properties.items())]

    def values(self, case, op, **properties):
        return [action['value'] for action in self.actions(case, op, **properties)]

    def last(self, case, op, **properties):
        return self.values(case, op, **properties)[-1]

    def test_pinned_complete_grammar(self):
        validate_witnesses(self.oracle, self.registry)
        names = profiles()['sources']
        self.assertEqual({pin['registry']['csvid'] for pin in names.values()}, {14666, 14937, 15027, 14849, 14927, 14948})
        self.assertEqual(sum(len(variants(classify(self.oracle[name]))) for name in names), 144)
        for name in names:
            record = self.oracle[name]
            params = classify(record)
            declaration = dict(family=FAMILY, version=2, params=dict(params, toughness=float(params['toughness'])),
                               oracle_text=record['oracle_text'], variants=['normal_cast_0'])
            with self.assertRaises(ValueError):
                spec(declaration, record)
            with self.assertRaises(ValueError):
                classify(dict(record, oracle_text=record['oracle_text'] + '\nDraw a card.'))

    def test_actual_cast_restricted_and_initial_pools_clear(self):
        name = 'Kolaghan Stormsinger'
        for owner in (0, 1):
            hidden = self.case(name, 'hidden_choice', owner)
            self.assertEqual(self.last(hidden, 'assert_card', ref='source', property='csvid'), FACE_DOWN)
            self.assertEqual(self.last(hidden, 'assert_card', ref='source', property='plus1_counters'), 0)
            self.assertEqual(self.values(hidden, 'choose_dialog', player=owner), [0])
            opening = hidden['steps'][1:13]
            self.assertEqual({(x['player'], x['color']) for x in opening}, {(p, c) for p in (0, 1) for c in range(6)})
            self.assertTrue(all(x['op'] == 'set_mana' and x['value'] == 0 for x in opening))
            self.assertEqual(self.values(self.case(name, 'short_cast', owner), 'cast', ref='source'), [0])
            for denied in ('reveal_short', 'reveal_wrong_color'):
                case = self.case(name, denied, owner)
                self.assertEqual(self.values(case, 'activate', ref='source'), [0])
                self.assertFalse(self.actions(case, 'choose_target'))
                self.assertEqual(self.last(case, 'assert_card', ref='source', property='csvid'), FACE_DOWN)

    def test_paid_turn_up_counter_stays_through_both_turns_and_replay(self):
        for owner in (0, 1):
            case = self.case('Ainok Survivalist', 'persist_replay', owner)
            self.assertEqual(self.values(case, 'activate', ref='source'), [1, 1])
            self.assertEqual(self.values(case, 'assert_counter', ref='source', counter_kind='plus1'), [1, 1])
            self.assertEqual(self.values(case, 'assert_card', ref='source', property='plus1_counters')[-3:], [1, 1, 1])
            self.assertEqual([s['player'] for s in self.actions(case, 'untap_step')], [1-owner, owner] * 2)
            self.assertTrue(self.actions(case, 'assert_recorded_state'))
            fountain = self.registry['Fountain of Youth']['csvid']
            self.assertEqual(self.last(case, 'assert_zone_multiplicity', player=1-owner, zone='graveyard', index=fountain), 1)

    def test_destruction_probes_filter_controller_type_and_power(self):
        for owner in (0, 1):
            for name, valid, invalid in (
                ('Ainok Survivalist', 'enemy_artifact', ('friendly_artifact', 'friendly_enchantment', 'enemy_creature')),
                ('Hidden Dragonslayer', 'enemy_threshold', ('friendly_threshold', 'enemy_wrong_power')),
                ('Silumgar Assassin', 'enemy_threshold', ('friendly_threshold', 'enemy_wrong_power')),
            ):
                case = self.case(name, 'paid_reveal', owner)
                self.assertTrue(self.actions(case, 'choose_target', player=owner, ref=valid))
                self.assertEqual(self.values(case, 'probe_activation_target', other=valid), [1])
                for ref in invalid:
                    self.assertEqual(self.values(case, 'probe_activation_target', other=ref), [0])
                no_eligible = self.case(name, 'no_eligible', owner)
                self.assertFalse(self.actions(no_eligible, 'choose_target'))
                self.assertEqual(self.values(no_eligible, 'assert_counter', ref='source', counter_kind='plus1'), [1])
            case = self.case('Ainok Survivalist', 'indestructible', owner)
            self.assertTrue(self.actions(case, 'choose_target', ref='enemy_artifact', player=owner))
            csvid = self.registry['Darksteel Citadel']['csvid']
            self.assertEqual(self.last(case, 'assert_zone_multiplicity', player=1-owner, zone='battlefield', index=csvid), 1)

    def test_lifelink_and_block_power_are_combat_receipts(self):
        for owner in (0, 1):
            for variant, life in (('combat_lifelink', 23), ('hidden_no_lifelink', 20)):
                case = self.case('Hidden Dragonslayer', variant, owner)
                self.assertTrue(self.actions(case, 'combat_damage'))
                self.assertEqual(self.last(case, 'assert_life', player=owner), life)
            for variant, legal in (('block_over', 0), ('block_equal', 1), ('block_dynamic', 1), ('hidden_block', 1)):
                case = self.case('Silumgar Assassin', variant, owner)
                self.assertEqual(self.values(case, 'block', other='source'), [legal])
                if variant == 'block_dynamic':
                    self.assertEqual(self.values(case, 'cast', ref='growth'), [1])

    def test_damage_reaches_correct_recipients_and_can_be_prevented(self):
        for owner in (0, 1):
            name = 'Atarka Efreet'
            for scenario, recipient, life in (('paid_reveal', 1-owner, 19), ('own_player', owner, 19),
                                               ('prevented_player', 1-owner, 20)):
                case = self.case(name, scenario, owner)
                self.assertEqual(self.values(case, 'choose_player', player=owner), [recipient])
                self.assertEqual(self.last(case, 'assert_life', player=recipient), life)
            for scenario, damage in (('damage_creature', 1), ('prevented_creature', 0)):
                case = self.case(name, scenario, owner)
                self.assertEqual(self.last(case, 'assert_card', ref='victim', property='damage'), damage)
                self.assertEqual(self.values(case, 'probe_activation_target', other='enemy_scout'), [0])
            walker = self.case(name, 'damage_planeswalker', owner)
            self.assertEqual(self.values(walker, 'cast', ref='walker'), [1])
            self.assertEqual(self.last(walker, 'assert_counter', ref='walker', counter_kind='loyalty'), 2)

    def test_another_friendly_creature_counter_is_independent_and_persistent(self):
        for owner in (0, 1):
            name = 'Guardian Shield-Bearer'
            paid = self.case(name, 'paid_reveal', owner)
            self.assertEqual(self.values(paid, 'probe_activation_target', other='source'), [0])
            self.assertEqual(self.values(paid, 'probe_activation_target', other='foe_creature'), [0])
            self.assertEqual(self.values(paid, 'probe_activation_target', other='friendly_creature'), [1])
            later = self.case(name, 'recipient_persists', owner)
            self.assertEqual(self.values(later, 'assert_card', ref='friendly_creature', property='plus1_counters')[-3:], [1, 1, 1])
            self.assertEqual(self.last(later, 'assert_card', ref='foe_creature', property='plus1_counters'), 0)

    def test_haste_changes_real_fresh_combat_and_loses_an_offturn_tap_right(self):
        for owner in (0, 1):
            name = 'Kolaghan Stormsinger'
            for scenario in ('grant_own_haste', 'grant_opposing_haste'):
                case = self.case(name, scenario, owner)
                self.assertEqual(self.values(case, 'cast', ref='recipient'), [1])
                self.assertEqual(self.values(case, 'can_activate', ref='recipient'), [0, 1])
                self.assertEqual(self.values(case, 'attack', ref='recipient'), [1])
            expired = self.case(name, 'grant_expires', owner)
            self.assertEqual(self.values(expired, 'can_activate', ref='recipient'), [0, 1, 0])
            self.assertEqual([s['player'] for s in self.actions(expired, 'untap_step')], [1-owner, owner])
            self.assertEqual(self.values(self.case(name, 'printed_haste', owner), 'attack', ref='source'), [1])
            self.assertEqual(self.values(self.case(name, 'hidden_no_haste', owner), 'attack', ref='source'), [0])
