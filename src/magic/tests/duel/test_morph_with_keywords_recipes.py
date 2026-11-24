import json
import unittest
from pathlib import Path

from generate_cases import registry
from morph_with_keywords_recipes import FACE_DOWN, FAMILY, classify, expand, profiles, spec, validate_witnesses, variants
from recipes import COLORS


class MorphWithKeywordsRecipesTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.oracle = json.loads((Path(__file__).parent / 'data/oracle.json').read_bytes())['cards']
        cls.registry = {row['name']: row for row in registry()}

    def dec(self, name, variant):
        record = self.oracle[name]
        return dict(family=FAMILY, version=2, oracle_text=record['oracle_text'], params=classify(record), variants=[variant])

    def case(self, name, scenario, owner):
        variant = f'{scenario}_{owner}'
        d = self.dec(name, variant)
        result, = expand(name, self.registry[name]['csvid'], d, spec(d, self.oracle[name]), [variant],
                         self.registry, self.oracle)
        return result

    def actions(self, case, op, **expected):
        return [step for step in case['steps'] if step['op'] == op
                and all(step.get(key) == value for key, value in expected.items())]

    def values(self, case, op, **expected):
        return [step['value'] for step in self.actions(case, op, **expected)]

    def property(self, case, ref, prop):
        return self.values(case, 'assert_card', ref=ref, property=prop)[-1]

    def test_complete_printed_grammar_and_exact_bindings(self):
        validate_witnesses(self.oracle, self.registry)
        pins = profiles()['sources']
        pilot = {14210, 14212, 6137, 8738, 14359, 14384}
        self.assertEqual({pin['registry']['csvid'] for pin in pins.values()}, pilot | {14379, 6102, 6278, 14360, 14331, 14361, 14335, 6187})
        self.assertEqual(sum(len(variants(classify(self.oracle[name]))) for name, pin in pins.items()
                             if pin['registry']['csvid'] in pilot), 247)
        self.assertEqual(classify(self.oracle["Sidisi's Pet"])['mode'], 'lifelink')
        for name in pins:
            declaration = self.dec(name, 'printed_cast_0')
            record = self.oracle[name]
            with self.assertRaises(ValueError):
                spec(dict(declaration, version=2.0), record)
            with self.assertRaises(ValueError):
                spec(dict(declaration, params=dict(declaration['params'], power=float(record['power']))), record)
            with self.assertRaises(ValueError):
                classify(dict(record, oracle_text='Flying\n' + record['oracle_text']))

    def test_real_hidden_cast_and_explicit_two_choice_payment(self):
        for owner in (0, 1):
            for scenario, choice, identity in (('printed_cast', [1], 14210), ('hidden_cast', [], FACE_DOWN),
                                               ('hidden_choice', [0], FACE_DOWN)):
                case = self.case('Abzan Guide', scenario, owner)
                self.assertEqual(self.values(case, 'choose_dialog', player=owner), choice)
                self.assertEqual(self.property(case, 'source', 'csvid'), identity)
                self.assertEqual(self.property(case, 'source', 'power'), 2 if identity == FACE_DOWN else 4)
                self.assertEqual(self.values(case, 'assert_mana', player=owner, color=COLORS['G'])[-1],
                                 int(scenario == 'hidden_choice'))
                opening = case['steps'][1:13]
                self.assertEqual(len(opening), 12)
                self.assertTrue(all(step['op'] == 'set_mana' and step['value'] == 0 for step in opening))
                self.assertEqual({(step['player'], step['color']) for step in opening},
                                 {(p, c) for p in (0, 1) for c in range(6)})
            short = self.case('Abzan Guide', 'cast_short', owner)
            self.assertEqual(self.values(short, 'cast', ref='source'), [0])

    def test_reveal_is_paid_and_changes_identity_before_legacy_drain(self):
        for owner in (0, 1):
            for scenario, allowed in (('paid_reveal', True), ('reveal_offturn', True), ('pending_damage', True),
                                      ('reveal_short', False), ('wrong_color', False)):
                case = self.case('Snowhorn Rider', scenario, owner)
                self.assertEqual(self.values(case, 'activate', ref='source'), [int(allowed)])
                self.assertEqual(self.property(case, 'source', 'csvid'), 14384 if allowed else FACE_DOWN)
                if allowed:
                    steps = case['steps']
                    at = steps.index(self.actions(case, 'activate', ref='source')[0])
                    drain = next(i for i, step in enumerate(steps[at+1:], at+1)
                                 if step['op'] in ('resolve', 'resolve_response'))
                    interim = {'steps': steps[at+1:drain]}
                    self.assertEqual(self.values(interim, 'assert_card', ref='source', property='csvid'), [14384])
                    self.assertEqual(self.values(interim, 'assert_mana', player=owner), [0] * 6)
                if scenario == 'pending_damage':
                    self.assertEqual(self.property(case, 'source', 'damage'), 2)
                    self.assertEqual(len(self.actions(case, 'resolve_response')), 1)

    def test_face_down_and_first_strike_differ_in_actual_block_combat(self):
        for owner in (0, 1):
            for mode, victim, survives in (('first_strike_attack', 'blocker', True),
                                          ('first_strike_defense', 'opposing', True),
                                          ('first_strike_hidden', 'source', False)):
                case = self.case('Ainok Tracker', mode, owner)
                self.assertEqual(self.values(case, 'block'), [1])
                dead = 14212 if victim == 'source' else self.registry['Hill Giant']['csvid']
                player = owner if victim == 'source' else 1 - owner
                self.assertEqual(self.values(case, 'assert_zone_multiplicity', player=player, zone='graveyard',
                                             index=dead)[-1], 1)
                if survives:
                    self.assertEqual(self.property(case, 'source', 'damage'), 0)
            fresh = self.case('Ainok Tracker', 'fresh_reveal', owner)
            self.assertEqual(self.values(fresh, 'attack', ref='source'), [0])

    def test_lifelink_and_damage_trigger_observe_player_creature_and_fog_damage(self):
        for owner in (0, 1):
            for name in ('Abzan Guide', 'Exalted Angel'):
                self.assertNotEqual(classify(self.oracle[name])['keywords'], [])
                for scenario, life in (('revealed_hit', 24), ('blocked_life', 24), ('hidden_hit', 20),
                                       ('noncombat_life', 21), ('prevented_life', 20)):
                    case = self.case(name, scenario, owner)
                    self.assertEqual(self.values(case, 'assert_life', player=owner)[-1], life)
                    if scenario == 'noncombat_life':
                        self.assertTrue(self.actions(case, 'activate', ref='grant', value=1))
                        self.assertFalse(self.actions(case, 'combat_damage'))

    def test_flying_and_trample_have_actual_positive_and_negative_receipts(self):
        for owner in (0, 1):
            for scenario, valid in (('flying_ground', 0), ('flying_reach', 1), ('flying_defends', 1)):
                case = self.case('Sage-Eye Harrier', scenario, owner)
                self.assertEqual(self.values(case, 'block'), [valid])
                self.assertEqual(self.values(case, 'assert_life', player=1 - owner)[-1], 20 if valid else 19)
            for scenario, life in (('trample_hit', 16), ('trample_prevented', 20), ('hidden_block', 20)):
                case = self.case('Snowhorn Rider', scenario, owner)
                self.assertEqual(self.values(case, 'block'), [1])
                self.assertEqual(self.values(case, 'assert_life', player=1 - owner)[-1], life)

    def test_akroma_real_white_blue_protection_blocks_targets_and_damage(self):
        name = 'Akroma, Angel of Fury'
        for owner in (0, 1):
            for color, spill in (('white', 2), ('blue', 4)):
                case = self.case(name, f'{color}_block_damage', owner)
                self.assertEqual(self.values(case, 'block', ref='blocker', other='source'), [1])
                self.assertEqual(self.property(case, 'source', 'damage'), 0)
                self.assertEqual(self.values(case, 'assert_life', player=1 - owner)[-1], 20 - spill)
                denied = self.case(name, f'protection_{color}_block', owner)
                self.assertEqual(self.values(denied, 'block'), [0])
                targeting = self.case(name, f'{color}_target', owner)
                self.assertEqual(self.values(targeting, 'can_cast', ref='targeting'), [0])
            black = self.case(name, 'black_target', owner)
            self.assertEqual(self.values(black, 'cast', ref='targeting'), [1])
            self.assertEqual(self.values(black, 'assert_zone_multiplicity', player=owner, zone='graveyard',
                                         index=8738)[-1], 1)
            aura = self.case(name, 'white_aura_detaches', owner)
            self.assertTrue(self.actions(aura, 'choose_target', player=1-owner, ref='source'))
            self.assertEqual(self.values(aura, 'assert_zone_multiplicity', player=1-owner, zone='graveyard',
                                         index=self.registry['Pacifism']['csvid'])[-1], 1)

    def test_akroma_nontargeted_counter_contrast_and_paid_red_pump(self):
        name = 'Akroma, Angel of Fury'
        counter = self.case(name, 'uncounterable_reveal', 1)
        self.assertEqual(self.oracle['Scornful Egotist']['cmc'], self.oracle[name]['cmc'])
        self.assertTrue(self.actions(counter, 'choose_trigger_source', player=0, ref='counterbalance'))
        self.assertEqual(self.property(counter, 'source', 'csvid'), 8738)
        for owner in (0, 1):
            hidden = self.case(name, 'hidden_countered', owner)
            self.assertTrue(self.actions(hidden, 'cast_response', ref='counter', value=1))
            self.assertEqual(self.values(hidden, 'assert_zone_multiplicity', player=owner, zone='graveyard',
                                         index=8738)[-1], 1)
            repeat = self.case(name, 'red_pump_repeat', owner)
            self.assertEqual(self.values(repeat, 'activate', ref='source'), [1, 1, 1])
            self.assertEqual(self.property(repeat, 'source', 'power'), 8)
            self.assertEqual(self.values(repeat, 'assert_life', player=1 - owner)[-1], 12)
            expired = self.case(name, 'red_pump_expires', owner)
            self.assertEqual(self.property(expired, 'source', 'power'), 6)
            for mode, allowed in (('red_pump_short', [1, 0]), ('red_pump_hidden', [0])):
                self.assertEqual(self.values(self.case(name, mode, owner), 'activate', ref='source'), allowed)

    def test_paid_reveal_replays_with_real_activation_twice(self):
        for owner in (0, 1):
            case = self.case('Snowhorn Rider', 'reveal_replay', owner)
            self.assertEqual(self.values(case, 'activate', ref='source'), [1, 1])
            self.assertEqual(len(self.actions(case, 'assert_recorded_state')), 1)

    def test_akroma_trample_uses_a_legal_flying_blocker(self):
        for owner in (0, 1):
            case = self.case('Akroma, Angel of Fury', 'trample_hit', owner)
            blocker, = [card for card in case['setup'] if card['ref'] == 'blocker']
            self.assertEqual(blocker['name'], 'Scryb Sprites')
            self.assertEqual(self.oracle[blocker['name']]['colors'], ['G'])
            self.assertEqual(self.oracle[blocker['name']]['keywords'], ['Flying'])
            self.assertEqual(self.values(case, 'block', ref='blocker', other='source'), [1])
            self.assertEqual(self.property(case, 'source', 'damage'), 1)
            self.assertEqual(self.values(case, 'assert_life', player=1-owner)[-1], 15)
            self.assertEqual(self.values(case, 'assert_zone_multiplicity', player=1-owner, zone='graveyard',
                                         index=217)[-1], 1)

    def test_sidisis_pet_has_actual_black_payment_and_lifelink_receipts(self):
        name = "Sidisi's Pet"
        self.assertEqual(classify(self.oracle[name])['morph'], [1, 1, 0, 0, 0, 0])
        for owner in (0, 1):
            for mode, life in (('blocked_life', 21), ('noncombat_life', 21), ('hidden_hit', 20), ('prevented_life', 20)):
                case = self.case(name, mode, owner)
                self.assertEqual(self.values(case, 'assert_life', player=owner)[-1], life)
                self.assertEqual(self.property(case, 'source', 'power'), 2 if mode == 'hidden_hit' else 1)
                self.assertEqual(self.property(case, 'source', 'toughness'), 2 if mode == 'hidden_hit' else 4)
            paid = self.case(name, 'paid_reveal', owner)
            exact = [step['value'] for step in self.actions(paid, 'set_mana', player=owner)][-6:]
            self.assertEqual(exact, [1, 1, 0, 0, 0, 0])

    def test_revealed_defender_and_late_defender_use_real_attack_declarations(self):
        for owner in (0, 1):
            for name, hit in (('Crude Rampart', 4), ('Monastery Flock', 0)):
                hidden = self.case(name, 'hidden_hit', owner)
                self.assertEqual(self.values(hidden, 'attack', ref='source'), [1])
                revealed = self.case(name, 'revealed_hit', owner)
                self.assertEqual(self.values(revealed, 'attack', ref='source'), [0])
                late = self.case(name, 'defender_late_reveal', owner)
                self.assertEqual(self.values(late, 'attack', ref='source'), [1])
                steps = late['steps']
                self.assertLess(steps.index(self.actions(late, 'advance_combat')[0]),
                                steps.index(self.actions(late, 'activate', ref='source')[0]))
                self.assertEqual(self.values(late, 'assert_life', player=1-owner)[-1], 20-hit)

    def test_flock_exact_printed_cost_options_and_generic_floating_payment(self):
        name = 'Monastery Flock'
        for owner in (0, 1):
            for scenario, mode in (('printed_cast', 1), ('hidden_choice', 0)):
                case = self.case(name, scenario, owner)
                actions = case['steps']
                menu, = self.actions(case, 'choose_dialog', player=owner, value=mode)
                cast, = self.actions(case, 'cast', ref='source', value=1)
                pay = self.actions(case, 'choose_mana', player=owner, color=COLORS['U'])
                self.assertEqual(len(pay), int(owner == 0 and mode == 0))
                if pay:
                    self.assertLess(actions.index(menu), actions.index(pay[0]))
                    self.assertLess(actions.index(pay[0]), actions.index(cast))
                self.assertEqual(self.values(case, 'assert_mana', player=owner, color=COLORS['U'])[-1], 0)
        self.assertEqual(classify(self.oracle[name])['morph'], [0, 0, 1, 0, 0, 0])

    def test_reach_and_flying_are_lost_when_face_down(self):
        for owner in (0, 1):
            for name in ('Spitting Gourna', 'Sagu Archer', 'Monastery Flock'):
                for scenario, allowed in (('block_flyer', 1), ('hidden_flyer', 0)):
                    case = self.case(name, scenario, owner)
                    self.assertEqual(self.values(case, 'block', ref='source', other='opposing'), [allowed])
                    self.assertEqual(self.property(case, 'source', 'damage'), 2*allowed)
                    self.assertEqual(self.values(case, 'assert_life', player=owner)[-1], 18+2*allowed)

    def test_mauler_has_typed_hexproof_and_trample_after_paid_reveal(self):
        name = 'Sagu Mauler'
        for owner in (0, 1):
            pending = self.case(name, 'pending_damage', owner)
            self.assertTrue(self.actions(pending, 'cast', ref='shock', value=1))
            self.assertEqual(self.property(pending, 'source', 'damage'), 0)
            denied = self.case(name, 'hexproof_foe_denied', owner)
            self.assertTrue(self.actions(denied, 'can_cast', ref='targeting', value=0))
            self.assertTrue(self.actions(denied, 'cast', ref='targeting', value=0))
            own = self.case(name, 'hexproof_own', owner)
            self.assertEqual(self.property(own, 'source', 'damage'), 2)
            early = self.case(name, 'hidden_hexproof_target', owner)
            self.assertTrue(self.actions(early, 'choose_target', ref='source', player=1-owner))
            self.assertEqual(self.values(early, 'assert_zone_multiplicity', zone='graveyard', player=owner,
                                         index=14361)[-1], 1)
            trample = self.case(name, 'trample_hit', owner)
            self.assertEqual(self.values(trample, 'assert_life', player=1-owner)[-1], 15)

    def test_mystic_keeps_an_existing_block_but_denies_a_new_one(self):
        name = 'Mystic of the Hidden Way'
        for owner in (0, 1):
            for scenario, blocked, harm in (('blocked_face_up', 0, 3), ('flip_after_block', 1, 0)):
                case = self.case(name, scenario, owner)
                self.assertEqual(self.values(case, 'block', ref='blocker', other='source'), [blocked])
                self.assertEqual(self.values(case, 'assert_life', player=1-owner)[-1], 20-harm)
            lethal = self.case(name, 'pending_damage', owner)
            self.assertEqual(self.values(lethal, 'assert_zone_multiplicity', zone='graveyard', player=owner,
                                         index=14335)[-1], 1)

    def test_crusher_counts_real_assignments_in_hidden_and_revealed_combat(self):
        name = 'Ironfist Crusher'
        for owner in (0, 1):
            for mode, blocks, life in (('hidden_multiple', [1,0,0], 18), ('flip_after_blocks', [1,0,0], 18),
                                       ('multiple_three', [1]*3, 20), ('multiple_nine', [1]*9, 20)):
                case = self.case(name, mode, owner)
                self.assertEqual(self.values(case, 'block', ref='source'), blocks)
                self.assertEqual(self.values(case, 'assert_life', player=owner)[-1], life)
                self.assertEqual(max(self.values(case, 'assert_card', ref='source',
                                                 property='blocking_count')), sum(blocks))
                if mode == 'multiple_three':
                    self.assertEqual(self.property(case, 'source', 'damage'), 3)
                if mode == 'multiple_nine':
                    self.assertEqual(self.values(case, 'assert_zone_multiplicity', zone='graveyard', player=owner,
                                                 index=6187)[-1], 1)
                if mode == 'flip_after_blocks':
                    steps = case['steps']
                    self.assertLess(steps.index(self.actions(case, 'advance_combat')[0]),
                                    steps.index(self.actions(case, 'activate', ref='source')[0]))

if __name__ == '__main__':
    unittest.main()
