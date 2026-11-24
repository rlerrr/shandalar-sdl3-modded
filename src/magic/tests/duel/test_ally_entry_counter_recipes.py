import json
import unittest
from pathlib import Path

from ally_entry_counter_recipes import FAMILY, classify, expand, is_ally, profiles, spec, validate_witnesses, variants
from generate_cases import registry


class AllyEntryCounterRecipesTest(unittest.TestCase):
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
        checked = spec(declaration, self.oracle[name])
        case, = expand(name, self.rows[name]['csvid'], declaration, checked, selected, self.rows, self.oracle)
        self.assertTrue(all(not card.get('state') for card in case['setup']))
        return case

    def actions(self, case, op, **fields):
        return [s for s in case['steps'] if s['op'] == op and all(s.get(k) == v for k, v in fields.items())]

    def counters(self, case, ref='source'):
        return [s['value'] for s in self.actions(case, 'assert_card', ref=ref, property='plus1_counters')]

    def test_source_bindings_validate_whole_oracle_and_typed_declarations(self):
        validate_witnesses(self.oracle, self.rows)
        names = profiles()['sources']
        self.assertEqual({self.rows[n]['csvid'] for n in names},
                         {10772, 10602, 10618, 10628, 10694, 10698, 10768, 10579, 10581, 10697, 10845})
        for name in names:
            p = classify(self.oracle[name])
            d = self.declaration(name, variants(p))
            self.assertEqual(spec(d, self.oracle[name])['mana'], p['mana'])
            self.assertNotIn('self_decline_1', variants(p))
            with self.assertRaises(ValueError):
                spec(dict(d, params=dict(p, power=float(p['power']))), self.oracle[name])
        for name in ('Ondu Cleric', 'Halimar Excavator', 'Murasa Pyromancer', 'Akoum Battlesinger'):
            with self.assertRaises(ValueError):
                classify(self.oracle[name])

    def test_separate_human_self_take_and_deliberate_decline(self):
        take = self.case('Hada Freeblade', 'self_take')
        decline = self.case('Hada Freeblade', 'self_decline')
        ai = self.case('Hada Freeblade', 'self_take', 1)
        cast, = self.actions(take, 'cast', ref='source', value=1)
        intent, = self.actions(take, 'choose_trigger', ref='source', player=0)
        resolve, = self.actions(take, 'resolve')
        steps = take['steps']
        self.assertLess(steps.index(cast), steps.index(intent))
        self.assertLess(steps.index(intent), steps.index(resolve))
        self.assertEqual(self.counters(take), [1])
        self.assertEqual(self.counters(decline), [0])
        self.assertEqual(self.counters(ai), [1])
        self.assertFalse(self.actions(decline, 'choose_trigger'))
        self.assertFalse(self.actions(ai, 'choose_trigger'))

    def test_real_ally_characteristics_and_controller_contrast(self):
        for witness in ('Cliffside Lookout', 'Avian Changeling'):
            self.assertTrue(is_ally(self.oracle[witness]))
        for witness in ('Grizzly Bears', 'Sol Ring'):
            self.assertFalse(is_ally(self.oracle[witness]))
        for owner in (0, 1):
            for mode, final in (('friendly_ally', 2), ('changeling', 2), ('opposing_ally', 1),
                                ('nonally_creature', 1), ('noncreature', 1)):
                case = self.case('Oran-Rief Survivalist', mode, owner)
                self.assertTrue(self.actions(case, 'cast', ref='first', value=1))
                self.assertEqual(self.counters(case)[-1], final)
                entrant, = [c for c in case['setup'] if c['ref'] == 'first']
                self.assertEqual(entrant['player'], 1-owner if mode == 'opposing_ally' else owner)

    def test_other_living_source_intent_and_friendly_decline_are_distinct(self):
        take = self.case('Oran-Rief Survivalist', 'friendly_ally')
        decline = self.case('Oran-Rief Survivalist', 'friendly_decline')
        delayed = self.case('Oran-Rief Survivalist', 'take_after_decline')
        self.assertTrue(self.actions(take, 'choose_trigger_source', ref='source', player=0))
        self.assertFalse(self.actions(decline, 'choose_trigger_source'))
        self.assertEqual(self.counters(take), [1, 2])
        self.assertEqual(self.counters(decline), [1, 1])
        self.assertEqual(self.counters(delayed), [0, 1])
        self.assertEqual(self.counters(self.case('Oran-Rief Survivalist', 'sequence')), [1, 2, 2, 3])

    def test_independent_same_card_sources_accept_separately(self):
        human = self.case('Hada Freeblade', 'two_sources')
        ai = self.case('Hada Freeblade', 'two_sources', 1)
        self.assertEqual([step['ref'] for step in self.actions(human, 'choose_trigger_source')], ['source', 'copy'])
        self.assertEqual([step['ref'] for step in self.actions(human, 'choose_trigger')], ['source'])
        self.assertEqual(self.counters(human), [1, 2, 2])
        self.assertEqual(self.counters(human, 'copy'), [0, 1])
        self.assertEqual(self.counters(ai), [1, 2, 3])
        self.assertEqual(self.counters(ai, 'copy'), [1, 2])

    def test_piper_activation_has_a_real_entry_and_ordered_decisions(self):
        for owner in (0, 1):
            c = self.case('Umara Raptor', 'piper_entry', owner)
            self.assertTrue(self.actions(c, 'activate', ref='piper', value=1))
            choice, = self.actions(c, 'choose_zone_card', player=owner, value=15294)
            if owner == 0:
                trigger, = self.actions(c, 'choose_trigger_source', player=owner, ref='source')
                self.assertLess(c['steps'].index(choice), c['steps'].index(trigger))
            self.assertFalse(self.actions(c, 'cast', ref='first'))
            self.assertEqual(self.counters(c), [1, 2])

    def test_countered_entry_and_bounce_new_object(self):
        for owner in (0, 1):
            denied = self.case('Nimana Sell-Sword', 'countered_entry', owner)
            self.assertTrue(self.actions(denied, 'cast_response', ref='counter', value=1))
            self.assertTrue(self.actions(denied, 'assert_zone_multiplicity', player=owner, zone='graveyard', index=15294, value=1))
            self.assertEqual(self.counters(denied), [1, 1])
            recast = self.case('Nimana Sell-Sword', 'bounce_reset', owner)
            self.assertTrue(self.actions(recast, 'bind_hand_copy', ref='source', player=owner, value=10618))
            self.assertEqual(len(self.actions(recast, 'cast', ref='source', value=1)), 2)
            self.assertEqual(self.counters(recast), [1, 2, 1, 2])

    def test_printed_payment_controls_preserve_source_in_hand(self):
        for label in ('wrong_color', 'insufficient_mana'):
            c = self.case('Graypelt Hunter', label, 1)
            self.assertTrue(self.actions(c, 'cast', ref='source', value=0))
            self.assertFalse(self.actions(c, 'resolve'))
            self.assertTrue(self.actions(c, 'assert_card', ref='source', property='in_hand', value=1))
            self.assertEqual(self.actions(c, 'assert_mana', player=1, color=0)[-1]['value'], 4 if label == 'wrong_color' else 0)

    def test_printed_combat_uses_real_phase_transitions_and_damage(self):
        for owner in (0, 1):
            haste = self.case('Tuktuk Grunts', 'fresh_haste', owner)
            self.assertTrue(self.actions(haste, 'attack', ref='source', value=1))
            self.assertFalse(self.actions(haste, 'advance_turn'))
            self.assertTrue(self.actions(haste, 'assert_life', player=1-owner, value=17))
            defender = self.case('Makindi Shieldmate', 'defender_attack', owner)
            self.assertTrue(self.actions(defender, 'attack', ref='source', value=0))
            self.assertEqual(len(self.actions(defender, 'untap_step')), 2)
            names = [s['op'] for s in defender['steps']]
            self.assertLess(names.index('combat_damage'), names.index('end_combat'))
            blocking = self.case('Makindi Shieldmate', 'defender_block', owner)
            self.assertTrue(self.actions(blocking, 'block', ref='source', other='opposing', value=1))
            flying = self.case('Umara Raptor', 'flying_ground', owner)
            self.assertTrue(self.actions(flying, 'block', ref='opposing', other='source', value=0))
            self.assertTrue(self.actions(flying, 'assert_life', player=1-owner, value=18))
            reach = self.case('Umara Raptor', 'flying_reach', owner)
            self.assertTrue(self.actions(reach, 'block', ref='opposing', other='source', value=1))
            self.assertTrue(self.actions(reach, 'assert_zone_multiplicity', player=owner, zone='graveyard', index=10698, value=1))
            trample = self.case('Graypelt Hunter', 'trample_combat', owner)
            self.assertTrue(self.actions(trample, 'block', ref='opposing', other='source', value=1))
            self.assertTrue(self.actions(trample, 'assert_life', player=1-owner, value=18))
        decline = self.case('Tuktuk Grunts', 'fresh_haste_decline')
        self.assertEqual(self.counters(decline), [0, 0])
        self.assertTrue(self.actions(decline, 'assert_life', player=1, value=18))

    def test_snapshot_replays_pending_entry_and_real_human_choice(self):
        c = self.case('Hada Freeblade', 'pending_replay')
        steps = c['steps']
        names = [s['op'] for s in steps]
        capture, record, restore = map(names.index, ('capture', 'record_state', 'restore'))
        self.assertEqual(steps[capture+1:record], steps[restore+1:-1])
        self.assertEqual(names[capture+1:capture+3], ['choose_trigger_source', 'resolve'])
        self.assertEqual(self.counters(c), [1, 2, 2])

    def test_kazandu_first_strike_and_vigilance_share_actual_combat(self):
        for owner in (0, 1):
            case = self.case('Kazandu Blademaster', 'first_strike_vigilance', owner)
            self.assertEqual(self.counters(case)[-1], 1)
            self.assertTrue(self.actions(case, 'block', ref='opposing', other='source', value=1))
            self.assertTrue(self.actions(case, 'assert_zone_multiplicity',
                                         player=1-owner, zone='graveyard', index=106, value=1))
            for prop in ('tapped', 'damage'):
                self.assertEqual(self.actions(case, 'assert_card', ref='source', property=prop)[-1]['value'], 0)
            self.assertTrue(self.actions(case, 'assert_zone_multiplicity',
                                         player=owner, zone='battlefield', index=10579, value=1))

    def test_warlord_counters_each_actual_friendly_ally_and_changeling(self):
        for owner in (0, 1):
            case = self.case('Kazuul Warlord', 'team_scope', owner)
            self.assertEqual(self.counters(case), [1, 2])
            self.assertEqual(self.counters(case, 'ally'), [0, 1, 2])
            self.assertEqual(self.counters(case, 'first'), [1])
            self.assertEqual(self.counters(case, 'enemy'), [0, 0, 0])
            self.assertEqual(self.counters(case, 'nonally'), [0, 0, 0])

    def test_turntimber_creates_distinct_real_expected_wolves_without_recursion(self):
        for owner in (0, 1):
            case = self.case('Turntimber Ranger', 'wolf_sequence', owner)
            wolves = self.actions(case, 'assert_zone_multiplicity', player=owner, zone='battlefield', index=18856)
            self.assertEqual([step['value'] for step in wolves], [0, 1, 2, 2, 3])
            for prop, value in (('token', 1), ('power', 2), ('toughness', 2), ('color', 8),
                                ('subtype_Wolf', 1), ('subtype_Ally', 0)):
                self.assertTrue(self.actions(case, 'assert_matching_permanents', player=owner, index=18856, property=prop, value=value))
            self.assertEqual(self.counters(case), [1, 2, 2, 3])
        decline = self.case('Turntimber Ranger', 'self_decline')
        self.assertFalse(self.actions(decline, 'choose_trigger', ref='source'))
        self.assertEqual(self.counters(decline), [0])
        self.assertTrue(self.actions(decline, 'assert_zone_multiplicity', player=0, zone='battlefield', index=18856, value=0))

    def test_talus_four_actual_dialog_choices_have_independent_combat_receipts(self):
        for mode, index, counter, life in (('talus_lifelink', 0, 0, 22), ('talus_counter', 1, 1, 20),
                                            ('talus_both', 2, 1, 22), ('talus_neither', 3, 0, 20)):
            c = self.case('Talus Paladin', mode)
            self.assertEqual([s['value'] for s in self.actions(c, 'choose_dialog', player=0)], [index])
            self.assertEqual(self.counters(c)[-1], counter)
            self.assertEqual(self.actions(c, 'assert_life', player=0)[-1]['value'], life)
            self.assertEqual(self.actions(c, 'assert_life', player=1)[-1]['value'], 16)
            self.assertEqual([s['ref'] for s in self.actions(c, 'attack')], ['ally', 'nonally'])
            self.assertFalse(self.actions(c, 'choose_trigger'))
        separated = self.case('Talus Paladin', 'talus_two_choices')
        self.assertEqual([s['value'] for s in self.actions(separated, 'choose_dialog', player=0)], [0, 1])
        self.assertEqual(self.counters(separated)[-1], 1)
        self.assertEqual(self.actions(separated, 'assert_life', player=0)[-1]['value'], 20)
        for owner in (0, 1):
            later = self.case('Talus Paladin', 'talus_later', owner)
            expiry = self.case('Talus Paladin', 'talus_expiry', owner)
            self.assertEqual(self.counters(later)[-1], 2)
            self.assertEqual(self.actions(later, 'assert_life', player=owner)[-1]['value'], 26)
            self.assertEqual(self.actions(expiry, 'assert_life', player=owner)[-1]['value'], 20)
