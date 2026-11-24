import json
import unittest
from pathlib import Path

from generate_cases import registry
from morph_turn_up_resources_recipes import (
    FACE_DOWN, FAMILY, classify, expand, profiles, spec, validate_witnesses, variants,
)
from recipes import COLORS


class MorphTurnUpResourcesRecipesTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.oracle = json.loads((Path(__file__).parent / 'data/oracle.json').read_bytes())['cards']
        cls.registry = {row['name']: row for row in registry()}

    def case(self, name, scenario, owner):
        record = self.oracle[name]
        variant = f'{scenario}_{owner}'
        declaration = dict(family=FAMILY, version=2, oracle_text=record['oracle_text'],
                           params=classify(record), variants=[variant])
        result, = expand(name, self.registry[name]['csvid'], declaration, spec(declaration, record),
                         [variant], self.registry, self.oracle)
        return result

    def actions(self, case, op, **expected):
        return [step for step in case['steps'] if step['op'] == op
                and all(step.get(key) == value for key, value in expected.items())]

    def values(self, case, op, **expected):
        return [step['value'] for step in self.actions(case, op, **expected)]

    def final_life(self, case, seat):
        return self.values(case, 'assert_life', player=seat)[-1]

    def test_complete_resource_grammar_and_five_implemented_sources(self):
        validate_witnesses(self.oracle, self.registry)
        pins = profiles()['sources']
        self.assertEqual({pin['registry']['csvid'] for pin in pins.values()}, {14221, 8491, 14348, 8969, 9044})
        self.assertEqual(sum(len(variants(classify(self.oracle[name]))) for name in pins), 162)
        for name in pins:
            record = self.oracle[name]
            params = classify(record)
            declaration = dict(family=FAMILY, version=2, oracle_text=record['oracle_text'],
                               params=dict(params, power=float(params['power'])), variants=['printed_cast_0'])
            with self.assertRaises(ValueError):
                spec(declaration, record)
            with self.assertRaises(ValueError):
                classify(dict(record, oracle_text=record['oracle_text'] + '\nDraw a card.'))

    def test_source_clearance_and_exact_face_down_payment(self):
        for owner in (0, 1):
            for name in ('Rattleclaw Mystic', 'Ashcloud Phoenix'):
                shown = self.case(name, 'printed_cast', owner)
                self.assertEqual(self.values(shown, 'choose_dialog', player=owner),
                                 [] if name == 'Rattleclaw Mystic' else [1])
                hidden = self.case(name, 'hidden_choice', owner)
                self.assertEqual(self.values(hidden, 'choose_dialog', player=owner), [0])
                self.assertEqual(self.values(hidden, 'assert_card', ref='source', property='csvid')[-1], FACE_DOWN)
                opening = hidden['steps'][1:13]
                self.assertEqual({(s['player'], s['color']) for s in opening}, {(p, c) for p in (0, 1) for c in range(6)})
                self.assertTrue(all(s['op'] == 'set_mana' and s['value'] == 0 for s in opening))
                self.assertEqual(self.values(self.case(name, 'short_cast', owner), 'cast', ref='source'), [0])

    def test_paid_reveal_receipts_precede_legacy_drain_and_replay(self):
        for owner in (0, 1):
            case = self.case('Maelstrom Djinn', 'reveal_replay', owner)
            self.assertEqual(self.values(case, 'activate', ref='source'), [1, 1])
            self.assertEqual(len(self.actions(case, 'assert_recorded_state')), 1)
            activation = case['steps'].index(self.actions(case, 'activate', ref='source')[0])
            drain = next(i for i, s in enumerate(case['steps'][activation+1:], activation+1) if s['op'] == 'resolve')
            interim = {'steps': case['steps'][activation+1:drain]}
            self.assertEqual(self.values(interim, 'assert_card', ref='source', property='csvid'), [8969])
            self.assertEqual(self.values(interim, 'assert_mana', player=owner), [0] * 6)
            denied = self.case('Maelstrom Djinn', 'reveal_wrong_color', owner)
            self.assertEqual(self.values(denied, 'activate', ref='source'), [0])
            self.assertEqual(self.values(denied, 'assert_counter', ref='source', counter_kind='time')[-1], 0)

    def test_phoenix_dies_returns_only_face_up_and_damages_both_on_reveal(self):
        for owner in (0, 1):
            returned = self.case('Ashcloud Phoenix', 'dies_returns', owner)
            self.assertEqual(self.values(returned, 'assert_zone_multiplicity', player=owner, zone='battlefield',
                                         index=FACE_DOWN)[-1], 1)
            dead = self.case('Ashcloud Phoenix', 'hidden_death', owner)
            self.assertEqual(self.values(dead, 'assert_zone_multiplicity', player=owner, zone='graveyard',
                                         index=14221)[-1], 1)
            replay = self.case('Ashcloud Phoenix', 'returns_reveals', owner)
            self.assertTrue(self.actions(replay, 'bind_permanent_copy', ref='source', value=FACE_DOWN))
            self.assertEqual([self.final_life(replay, p) for p in (0, 1)], [18, 18])
            shield = self.case('Ashcloud Phoenix', 'prevent_damage', owner)
            self.assertEqual(self.final_life(shield, owner), 18)
            self.assertEqual(self.final_life(shield, 1-owner), 20)

    def test_brine_affects_exactly_the_next_opposing_untap(self):
        for owner in (0, 1):
            for scenario in ('skip_once', 'normal_untap', 'skip_after_removal', 'skip_offturn'):
                case = self.case('Brine Elemental', scenario, owner)
                self.assertEqual([s['ref'] for s in self.actions(case, 'choose_target')][:2], ['own_land', 'opposing_land'])
                self.assertEqual([s['player'] for s in self.actions(case, 'untap_step')][-3:],
                                 [1-owner, owner, 1-owner])
                self.assertEqual(self.values(case, 'assert_card', ref='opposing_land', property='tapped')[-3:],
                                 [0, 0, 0] if scenario == 'normal_untap' else [1, 1, 0])
                self.assertEqual(self.values(case, 'assert_card', ref='own_land', property='tapped')[-3:],
                                 [0, 0, 0] if scenario == 'skip_offturn' else [1, 0, 0])

    def test_reveal_mana_spends_exactly_once_and_optional_taps_use_printed_colors(self):
        for owner in (0, 1):
            case = self.case('Rattleclaw Mystic', 'reveal_spend', owner)
            before = case['steps'].index(self.actions(case, 'activate', ref='source')[0])
            spend = case['steps'].index(self.actions(case, 'cast', ref='spender')[0])
            span = {'steps': case['steps'][before+1:spend]}
            self.assertFalse(self.actions(span, 'set_mana'))
            for color in 'GUR':
                self.assertEqual(self.values(span, 'assert_mana', player=owner, color=COLORS[color])[-1], 1)
                self.assertEqual(self.values(case, 'assert_mana', player=owner, color=COLORS[color])[-1], 0)
                tap = self.case('Rattleclaw Mystic', 'mana_' + color.lower(), owner)
                self.assertEqual(self.values(tap, 'choose_color', player=owner), [COLORS[color]])
                self.assertEqual(self.values(tap, 'assert_mana', player=owner, color=COLORS[color])[-1], 1)
            no_hidden = self.case('Rattleclaw Mystic', 'no_hidden_mana', owner)
            self.assertEqual(len(self.actions(no_hidden, 'advance_turn')), 2)
            self.assertEqual(self.values(no_hidden, 'activate', ref='source'), [0])

    def test_actual_upkeep_removes_time_and_sacrifices_only_after_turn_up(self):
        for owner in (0, 1):
            case = self.case('Maelstrom Djinn', 'vanishing_cycle', owner)
            self.assertEqual([s['player'] for s in self.actions(case, 'upkeep_step')], [1-owner, owner, 1-owner, owner])
            self.assertEqual(self.values(case, 'assert_counter', ref='source', counter_kind='time'), [0, 2, 2, 1, 1])
            self.assertEqual(self.values(case, 'assert_zone_multiplicity', player=owner, zone='graveyard', index=8969)[-1], 1)
            normal = self.case('Maelstrom Djinn', 'normal_no_vanishing', owner)
            self.assertEqual(self.values(normal, 'assert_counter', ref='source', counter_kind='time'), [0] * 5)
            self.assertEqual(self.values(normal, 'assert_zone_multiplicity', player=owner, zone='graveyard', index=8969)[-1], 0)
            snap = self.case('Maelstrom Djinn', 'snap_vanishing', owner)
            self.assertTrue(self.actions(snap, 'cast', ref='snap', value=1))
            self.assertEqual(self.values(snap, 'assert_zone_multiplicity', player=owner, zone='graveyard', index=8969)[-1], 1)

    def test_scry_reads_exact_human_library_and_independent_power(self):
        for scenario, expected, library_top in (('paid_reveal', 4, 'Mountain'), ('scry_reorder', 4, 'Island'),
                                                ('scry_decline', 2, 'Plains')):
            case = self.case('Unblinking Bleb', scenario, 0)
            self.assertEqual(self.values(case, 'assert_card', ref='adept', property='power')[-1], expected)
            first = self.values(case, 'assert_zone_card', player=0, zone='library', index=0)[-1]
            self.assertEqual(first, self.registry[library_top]['csvid'])
            self.assertEqual(self.values(case, 'assert_unused_choices')[-1], 2 if scenario == 'scry_decline' else 0)
        ai = self.case('Unblinking Bleb', 'paid_reveal', 1)
        self.assertTrue(self.actions(ai, 'assert_scry_result', player=1, value=2))
        self.assertEqual(self.values(ai, 'assert_card', ref='adept', property='power')[-1], 4)

    def test_scry_watcher_requires_face_up_and_accepts_either_others_controller(self):
        for scenario, actor in (('friendly_other_turns_up', 0), ('opposing_other_turns_up', 1)):
            case = self.case('Unblinking Bleb', scenario, 0)
            self.assertEqual(self.values(case, 'choose_zone', player=0), [0, 0])
            self.assertTrue(self.actions(case, 'activate', ref='other', value=1))
            self.assertTrue(any(s['player'] == actor and s['color'] == COLORS['U'] and s['value'] == 1
                                for s in self.actions(case, 'set_mana')))
            self.assertEqual(self.values(case, 'assert_card', ref='adept', property='power')[-1], 4)
            if actor == 1:
                self.assertEqual(self.values(case, 'assert_card', ref='opposing_adept', property='power')[-1], 2)
        for owner in (0, 1):
            hidden = self.case('Unblinking Bleb', 'hidden_other_turns_up', owner)
            self.assertFalse(self.actions(hidden, 'choose_zone'))
            self.assertEqual(self.values(hidden, 'assert_card', ref='adept', property='power')[-1], 2)

    def test_scry_input_is_seat_specific_and_keeps_oracle_outcomes(self):
        for scenario, repetitions in (('paid_reveal', 1), ('reveal_offturn', 1),
                                      ('reveal_replay', 2), ('scry_one', 1), ('scry_empty', 0)):
            ai = self.case('Unblinking Bleb', scenario, 1)
            self.assertEqual(self.values(ai, 'choose_zone', player=1), [-1] * repetitions)
            self.assertEqual(self.values(ai, 'assert_scry_result', player=1), [2] * repetitions)
        case = self.case('Unblinking Bleb', 'opposing_other_turns_up', 0)
        for seat, expected in ((0, 4), (1, 1)):
            self.assertEqual(sum(c['zone'] == 'library' and c['player'] == seat for c in case['setup']), expected)
        self.assertEqual(sum(c['name'] == 'Forest' and c['zone'] == 'hand' and c['player'] == 1
                             for c in case['setup']), 4)
        self.assertEqual(self.values(case, 'choose_zone', player=0), [0, 0])
        self.assertFalse(self.actions(case, 'choose_zone', player=1))
        self.assertEqual(self.values(case, 'assert_zone_card', player=0, zone='library', index=0)[-1],
                         self.registry['Mountain']['csvid'])
        self.assertEqual(self.values(case, 'assert_card', ref='adept', property='power')[-1], 4)
        self.assertEqual(self.values(case, 'assert_card', ref='opposing_adept', property='power')[-1], 2)
