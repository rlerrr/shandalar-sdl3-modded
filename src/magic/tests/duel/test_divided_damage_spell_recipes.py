"""Exact Oracle, point allocations, and resolution revalidation guards."""

import json
import unittest

from generate_cases import registry
import divided_damage_spell_recipes as model


class DividedDamageSpellTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.oracle = json.loads((model.PROFILE_PATH.parent / 'oracle.json').read_text(encoding='utf8'))['cards']
        cls.rows = {row['name']: row for row in registry()}

    def case(self, name, mode):
        record = self.oracle[name]
        declaration = dict(family=model.FAMILY, version=2, oracle_text=record['oracle_text'],
                           params=model.parameters(record), variants=[mode])
        return model.expand(name, self.rows[name]['csvid'], declaration, model.spec(declaration, record),
                            [mode], self.rows, self.oracle)[0]

    @staticmethod
    def values(steps, op, **fields):
        return [step['value'] for step in steps if step['op'] == op and all(step.get(key) == value for key, value in fields.items())]

    @staticmethod
    def target_choices(steps):
        return [(step['op'], step.get('ref', step.get('value'))) for step in steps if step['op'] in ('choose_player', 'choose_target')]

    def test_complete_clauses_costs_and_source_cards(self):
        model.validate_witnesses(self.oracle, self.rows)
        expected = {1746: (3, '{2}{R}', False, False), 12570: (3, '{2}{R}', False, False),
                    10930: (2, '{R}', False, False), 15054: (2, '{1}{R}', False, True),
                    16314: (2, '{1}{R}', False, True), 752: (4, '{4}{R}', True, False),
                    13395: (5, '{6}{R}{R}', True, True)}
        sources = {name: pin for name, pin in model.profiles()['sources'].items() if 'target_scope' not in pin['params']}
        self.assertEqual({pin['registry']['csvid'] for pin in sources.values()}, set(expected))
        for name, pin in sources.items():
            record = self.oracle[name]
            total, cost, any_number, instant = expected[pin['registry']['csvid']]
            self.assertEqual(model.parameters(record), dict(damage=total, target_cap=total, printed_mana=cost,
                                                            any_number=any_number, instant=instant))
            for text in (record['oracle_text'] + '\nDraw a card.', record['oracle_text'].replace(' targets.', ' target artifact creatures.')):
                with self.assertRaises(ValueError):
                    model.parameters(dict(record, oracle_text=text))

    def test_real_cast_choices_cover_single_and_printed_maximum(self):
        targets = [('choose_player', 0), ('choose_player', 1), ('choose_target', 'enemy'),
                   ('choose_target', 'walker'), ('choose_target', 'own')]
        for name, total in [('Forked Bolt', 2), ('Arc Lightning', 3), ('Pyrotechnics', 4), ('Boulderfall', 5)]:
            maximum = self.case(name, 'max_distinct_1')['steps']
            self.assertEqual(self.target_choices(maximum), targets[:total])
            self.assertEqual(self.values(maximum, 'assert_life', player=0), [20, 19])
            self.assertEqual(self.values(maximum, 'assert_life', player=1), [20, 19])
            single = self.case(name, 'all_to_self_1')['steps']
            self.assertEqual(self.target_choices(single), [('choose_player', 1)] * total)
            self.assertEqual(self.values(single, 'assert_life', player=1), [20, 20 - total])

    def test_mixed_and_indestructible_each_keep_correct_damage_ledger(self):
        for who in (0, 1):
            steps = self.case('Boulderfall', f'mixed_three_types_{who}')['steps']
            self.assertEqual(self.target_choices(steps), [('choose_target', 'enemy'), ('choose_target', 'walker')] + [('choose_player', 1 - who)] * 3)
            self.assertEqual(self.values(steps, 'assert_counter', ref='walker', counter_kind='loyalty'), [3, 2])
            self.assertEqual(self.values(steps, 'assert_card', ref='enemy', property='damage'), [0, 1])
            self.assertEqual(self.values(steps, 'assert_life', player=1 - who), [20, 17])
            indestructible = self.case('Boulderfall', f'indestructible_{who}')['steps']
            self.assertEqual(self.values(indestructible, 'assert_card', ref='enemy', property='damage'), [0, 5])
            self.assertEqual(self.values(indestructible, 'assert_card', ref='enemy', property='in_play'), [1])
        dead = self.case('Forked Bolt', 'walker_lethal_0')['steps']
        self.assertEqual(self.values(dead, 'assert_counter', ref='walker', counter_kind='loyalty'), [2])
        self.assertEqual(self.values(dead, 'assert_card', ref='walker', property='in_play'), [0])

    def test_real_responses_revalidate_each_share_without_reassigning(self):
        for mode, effect in [('late_bounce_all', 20), ('late_bounce_first', 19), ('late_bounce_last', 19), ('late_hexproof', 19)]:
            steps = self.case('Pyrotechnics', mode + '_1')['steps']
            announce = steps.index(dict(op='announce_spell', ref='source'))
            reply = steps.index(dict(op='cast_response', ref='response', value=1))
            self.assertEqual(len(self.target_choices(steps[:announce])), 4)
            self.assertEqual(self.target_choices(steps[announce:reply]), [('choose_target', 'enemy')])
            self.assertEqual(self.values(steps, 'assert_life', player=0), [20, effect])
            self.assertIn(dict(op='resolve_response'), steps)
        hexproof = self.case('Pyrotechnics', 'late_hexproof_0')['steps']
        self.assertEqual(self.values(hexproof, 'assert_card', ref='enemy', property='damage'), [0, 0])
        own = self.case('Pyrotechnics', 'own_hexproof_0')['steps']
        self.assertEqual(self.values(own, 'assert_card', ref='own', property='damage'), [0, 4])

    def test_repeated_native_point_choices_are_one_packet_under_armor(self):
        case = self.case('Flames of the Firebrand', 'single_packet_armor_0')
        self.assertIn(dict(name="Urza's Armor", ref='armor', player=1, zone='battlefield'), case['setup'])
        self.assertEqual(self.target_choices(case['steps']), [('choose_player', 1)] * 3)
        self.assertEqual(self.values(case['steps'], 'assert_life', player=1), [20, 18])

    def test_exact_paid_boundary_cancel_and_counter(self):
        for mode, generic, red in [('cancel_first', 6, 2), ('unfunded', 0, 0), ('wrong_color', 8, 0)]:
            steps = self.case('Boulderfall', mode + '_1')['steps']
            self.assertEqual(self.values(steps, 'cast', ref='source'), [0])
            self.assertEqual(self.values(steps, 'assert_mana', player=1, color=0), [generic, generic])
            self.assertEqual(self.values(steps, 'assert_mana', player=1, color=4), [red, red])
            self.assertEqual(sum(step['op'] == 'choose_cancel_target' for step in steps), int(mode == 'cancel_first'))
        countered = self.case('Boulderfall', 'countered_1')['steps']
        self.assertEqual(self.values(countered, 'assert_mana', player=1, color=4), [0, 0])
        self.assertIn(dict(op='cast_response', ref='response', value=1), countered)
        self.assertEqual(self.values(countered, 'assert_life', player=0), [20, 20])

    def test_pending_snapshot_replays_damage_without_targets_or_payment(self):
        steps = self.case("Chandra's Pyrohelix", 'pending_replay_1')['steps']
        cut = steps.index(dict(op='capture'))
        self.assertEqual(len(self.target_choices(steps[:cut])), 2)
        self.assertEqual(self.target_choices(steps[cut:]), [])
        self.assertEqual(self.values(steps, 'cast', ref='source'), [1])
        self.assertEqual(steps[cut:].count(dict(op='resolve')), 2)
        self.assertIn(dict(op='assert_recorded_state'), steps)
        offturn = self.case('Twin Bolt', 'offturn_instant_1')['steps']
        self.assertIn(dict(op='turn', player=0, value=0), offturn)
        with self.assertRaises(ValueError):
            self.case('Forked Bolt', 'offturn_instant_1')


class RestrictedDividedDamagePreviewTest(unittest.TestCase):
    """Focused guards for the privately staged target-scope variants."""

    case = DividedDamageSpellTest.case
    values = staticmethod(DividedDamageSpellTest.values)
    target_choices = staticmethod(DividedDamageSpellTest.target_choices)

    @classmethod
    def setUpClass(cls):
        cls.oracle = json.loads((model.PROFILE_PATH.parent / 'oracle.json').read_text(encoding='utf8'))['cards']
        cls.rows = {row['name']: row for row in registry()}

    def test_exact_target_scopes_costs_and_maxima(self):
        for name, scope, total, price in [('Fire at Will', 'combat', 3, '{R/W}{R/W}{R/W}'),
                                          ('Aerial Volley', 'flying', 3, '{G}'), ("Roil's Retribution", 'combat', 5, '{3}{W}{W}'),
                                          ('Spreading Flames', 'creature', 6, '{6}{R}'), ('Deft Dismissal', 'combat', 3, '{3}{W}')]:
            vals = model.parameters(self.oracle[name])
            self.assertEqual((vals['target_scope'], vals['damage'], vals['target_cap'], vals['printed_mana']), (scope, total, total, price))
            steps = self.case(name, 'max_distinct_0')['steps']
            self.assertEqual(self.target_choices(steps), [('choose_target', f'target_{i}') for i in range(total)])
            self.assertEqual(self.values(steps, 'cast', ref='source'), [1])

    def test_actual_combat_eligibility_and_late_flash_removal(self):
        for owner in (0, 1):
            for mode, op in [('attacker_only', 'attack'), ('blocker_only', 'block')]:
                steps = self.case('Deft Dismissal', f'{mode}_{owner}')['steps']
                cast = steps.index(dict(op='cast', ref='source', value=1))
                self.assertEqual(self.values(steps[:cast], op, ref='target_0'), [1])
                self.assertGreater(cast, steps.index(dict(op='advance_combat')))
            steps = self.case("Roil's Retribution", f'late_leave_combat_{owner}')['steps']
            reply = steps.index(dict(op='cast_response', ref='reply', value=1))
            self.assertIn(dict(op='choose_target', player=1 - owner, ref='target_0'), steps[reply:])
            self.assertEqual(self.values(steps, 'assert_card', ref='target_0', property='attacking'), [1, 0])
            self.assertEqual(self.values(steps, 'assert_card', ref='target_0', property='damage'), [0, 0])
            self.assertEqual(self.values(steps, 'assert_card', ref='target_1', property='damage'), [0, 1])
            boosted = self.case("Roil's Retribution", f'late_hexproof_{owner}')['steps']
            self.assertEqual(self.values(boosted, 'assert_life', player=owner), [20, 19])

    def test_flying_comes_and_goes_through_real_aura_actions(self):
        granted = self.case('Aerial Volley', 'flight_grant_0')['steps']
        self.assertEqual(self.values(granted, 'can_cast', ref='source'), [0, 1])
        self.assertIn(dict(op='cast', ref='flight', value=1), granted)
        self.assertEqual(self.values(granted, 'assert_card', ref='target_0', property='damage'), [0, 3])
        lost = self.case('Aerial Volley', 'late_flying_loss_1')['steps']
        self.assertIn(dict(op='choose_target', player=0, ref='flight'), lost)
        self.assertIn(dict(op='cast_response', ref='reply', value=1), lost)
        self.assertEqual(self.values(lost, 'assert_card', ref='target_0', property='damage'), [0, 0])
        self.assertEqual(self.values(lost, 'assert_card', ref='target_1', property='damage'), [0, 1])

    def test_hybrid_four_exact_unique_payments_and_ordinary_alias(self):
        for mode, red in [('all_to_creature', 3), ('hybrid_white', 0), ('hybrid_red_mixed', 2), ('hybrid_white_mixed', 1)]:
            steps = self.case('Fire at Will', mode + '_1')['steps']
            self.assertIn(dict(op='set_mana', player=1, color=4, value=red), steps)
            self.assertIn(dict(op='set_mana', player=1, color=5, value=3 - red), steps)
            self.assertEqual(self.values(steps, 'assert_mana_total', player=1)[-1], 0)
            self.assertFalse(any(step['op'] == 'choose_dialog' for step in steps))
        steps = self.case('Deft Dismissal', 'all_to_creature_0')['steps']
        self.assertIn(dict(op='set_mana', player=0, color=0, value=3), steps)
        self.assertIn(dict(op='set_mana', player=0, color=5, value=1), steps)

    def test_ineligible_recipients_use_actual_can_cast_no_forced_input(self):
        for name, mode in [('Spreading Flames', 'wrong_types_only'), ('Aerial Volley', 'ground_no_fly'),
                           ('Fire at Will', 'combat_idle_negative'), ('Deft Dismissal', 'none_eligible')]:
            steps = self.case(name, mode + '_1')['steps']
            self.assertEqual(self.values(steps, 'can_cast', ref='source'), [0])
            self.assertEqual(self.values(steps, 'cast', ref='source'), [0])
            self.assertEqual(self.target_choices(steps), [])


if __name__ == '__main__':
    unittest.main()
