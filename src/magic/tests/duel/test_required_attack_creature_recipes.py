import copy
import json
import unittest
from pathlib import Path

from generate_cases import registry
import required_attack_creature_recipes as model


class RequiredAttackCreatureTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.oracle = json.loads((Path(__file__).parent / 'data/oracle.json').read_text(encoding='utf8'))['cards']
        cls.rows = {row['name']: row for row in registry()}

    def case(self, variant, name='Bloodrock Cyclops'):
        record = self.oracle[name]
        declaration = dict(family=model.FAMILY, version=2, params=model.classify(record),
                           oracle_text=record['oracle_text'], variants=[variant])
        return model.expand(name, self.rows[name]['csvid'], declaration, model.spec(declaration, record),
                            [variant], self.rows, self.oracle)[0]

    @staticmethod
    def value(steps, op, **fields):
        return next(step['value'] for step in reversed(steps) if step['op'] == op
                    and all(step.get(k) == value for k, value in fields.items()))

    def count(self, steps, player, zone, name):
        return self.value(steps, 'assert_zone_multiplicity', player=player, zone=zone, index=self.rows[name]['csvid'])

    @staticmethod
    def one_declaration(steps):
        begin = next(i for i, s in enumerate(steps) if s['op'] == 'begin_required_attackers')
        end = next(i for i, s in enumerate(steps) if s['op'] == 'finish_attackers')
        return steps[begin:end+1]

    def test_normal_implemented_sources_and_real_restriction_witness(self):
        model.validate_witnesses(self.oracle, self.rows)
        self.assertEqual({self.rows[name]['csvid'] for name in model.pins()['sources']}, {2944, 5167, 6534, 6868, 7436, 8311, 9693, 10387, 11253, 12604, 13209, 13737, 14421, 16707, 19720})
        rows = copy.deepcopy(self.rows)
        rows['Pacifism']['handler'] = 'card_dummy'
        with self.assertRaises(ValueError):
            model.validate_witnesses(self.oracle, rows)

    def test_exact_whole_clause_and_cost_relation(self):
        r = self.oracle['Tattermunge Maniac']
        self.assertEqual(model.classify(r)['costs'], {'red': [0, 0, 0, 0, 1, 0], 'green': [0, 0, 0, 1, 0, 0]})
        for text in (model.CLAUSE + '\nHaste', 'This creature attacks each turn if able.', 'This creature may attack.'):
            with self.assertRaises(ValueError):
                model.classify(dict(r, oracle_text=text))

    def test_omission_rejected_until_manual_selection_on_each_seat(self):
        for name in model.pins()['sources']:
            for who in (0, 1):
                lines = self.one_declaration(self.case(f'omitted_{who}', name)['steps'])
                self.assertEqual([s for s in lines if s['op'] in ('attack', 'can_finish_attackers')],
                    [dict(op='can_finish_attackers', value=0), dict(op='attack', ref='source', value=1),
                     dict(op='can_finish_attackers', value=1)])

    def test_eligible_ordinary_ally_is_optional_and_cannot_replace_source(self):
        for who in (0, 1):
            alternative = self.case(f'competing_selected_{who}')['steps']
            d = self.one_declaration(alternative)
            self.assertLess(d.index(dict(op='attack', ref='ally', value=1)), d.index(dict(op='can_finish_attackers', value=0)))
            self.assertEqual(self.value(alternative, 'assert_life', player=1-who), 14)
            optional = self.case(f'optional_ally_{who}')['steps']
            self.assertFalse(any(s['op'] == 'attack' and s.get('ref') == 'ally' for s in optional))
            self.assertEqual(self.value(optional, 'assert_life', player=1-who), 17)

    def test_eligible_omissions_considered_independently_for_two_requirements(self):
        for who in (0, 1):
            d = self.one_declaration(self.case(f'two_required_{who}')['steps'])
            checks = [s['value'] for s in d if s['op'] == 'can_finish_attackers']
            self.assertEqual(checks, [0, 0, 1])
            self.assertEqual([s['ref'] for s in d if s['op'] == 'attack'], ['source', 'second'])

    def test_tapped_sick_fresh_and_actual_pacifism_allow_done(self):
        for who in (0, 1):
            for variant in ('tapped', 'sick', 'paid_fresh', 'pacified'):
                steps = self.case(f'{variant}_{who}')['steps']
                d = self.one_declaration(steps)
                self.assertIn(dict(op='attack', ref='source', value=0), d)
                self.assertEqual([s['value'] for s in d if s['op'] == 'can_finish_attackers'], [1])
                self.assertEqual(self.value(steps, 'assert_life', player=1-who), 20)
                if variant == 'pacified':
                    self.assertIn(dict(op='choose_target', ref='source', player=who), steps)
                    self.assertEqual(self.count(steps, who, 'battlefield', 'Pacifism'), 1)

    def test_opposing_required_creature_is_inactive_and_does_not_prevent_done(self):
        for who in (0, 1):
            steps = self.case(f'opponent_turn_{who}')['steps']
            self.assertEqual(steps[0], dict(op='turn', player=1-who, value=0))
            d = self.one_declaration(steps)
            self.assertEqual([s['value'] for s in d if s['op'] == 'can_finish_attackers'], [1, 1])
            self.assertFalse(any(s.get('ref') == 'source' for s in d if s['op'] == 'attack'))
            self.assertEqual(self.value(steps, 'assert_life', player=who), 17)

    def test_real_cost_both_hybrid_alternatives_and_next_turn(self):
        for who in (0, 1):
            for mode, color in [('paid_next_turn', 4), ('green_payment', 3)]:
                steps = self.case(f'{mode}_{who}', 'Tattermunge Maniac')['steps']
                cast = next(i for i, s in enumerate(steps) if s['op'] == 'cast')
                opening = next(i for i, s in enumerate(steps) if s['op'] == 'begin_required_attackers')
                self.assertEqual([self.value(steps[:cast], 'set_mana', player=who, color=c) for c in range(6)],
                                 [int(c == color) for c in range(6)])
                self.assertEqual(self.value(steps[cast:opening], 'assert_mana', player=who, color=color), 0)
                self.assertEqual([s['op'] for s in steps[cast:opening]].count('advance_turn'), 2)
                self.assertIn(dict(op='untap_step', player=who), steps[cast:opening])

    def test_blocked_combat_has_real_damage_and_no_player_damage(self):
        steps = self.case('blocked_0', 'Crazed Goblin')['steps']
        self.assertIn(dict(op='block', ref='blocker', other='source', value=1), steps)
        self.assertEqual(self.value(steps, 'assert_card', ref='source', property='blocked'), 1)
        self.assertEqual(self.count(steps, 0, 'graveyard', 'Crazed Goblin'), 1)
        self.assertEqual(self.value(steps, 'assert_life', player=1), 20)

    def test_gate_witness_is_real_opposing_tapped_and_rules_expectation_same(self):
        card = self.case('other_registration_0', 'Rubblebelt Recluse')
        witness = next(row for row in card['setup'] if row['ref'] == 'other_required')
        self.assertEqual(witness, dict(name='Crazed Goblin', player=1, ref='other_required', zone='battlefield', state=['tapped']))
        self.assertEqual(self.one_declaration(card['steps']), self.one_declaration(self.case('omitted_0', card['card'])['steps']))

    def test_replay_preserves_proposed_declaration_and_later_combat_requires_again(self):
        steps = self.case('proposed_replay_0')['steps']
        a, b, c = [next(i for i, s in enumerate(steps) if s['op'] == op) for op in ('capture', 'record_state', 'restore')]
        self.assertEqual(steps[a-1], dict(op='can_finish_attackers', value=0))
        self.assertEqual(steps[a+1:b], steps[c+1:-1])
        repeat = self.case('later_combat_0')['steps']
        actions = repeat[:next(i for i, s in enumerate(repeat) if s['op'] == 'record_state')]
        self.assertEqual(sum(s['op'] == 'begin_required_attackers' for s in actions), 2)
        self.assertEqual(sum(s == dict(op='can_finish_attackers', value=0) for s in actions), 2)
        self.assertEqual(self.value(repeat, 'assert_life', player=1), 14)

    def test_first_combat_initializes_via_real_end_turn_at_zero_mana(self):
        for who in (0, 1):
            for mode in ('paid_fresh', 'tapped', 'sick', 'pacified', 'omitted', 'opponent_turn'):
                steps = self.case(f'{mode}_{who}')['steps']
                active = 1 - who if mode == 'opponent_turn' else who
                self.assertEqual(steps[:1], [dict(op='turn', player=active, value=0)])
                self.assertEqual(steps[1:13], [dict(op='set_mana', player=p, color=color, value=0)
                                 for p in (0, 1) for color in range(6)])
                self.assertEqual(steps[13:15], [dict(op='cleanup'), dict(op='turn', player=active, value=0)])
                self.assertLess(steps.index(dict(op='cleanup')), steps.index(dict(op='capture')))
            for mode in ('paid_next_turn', 'unfunded', 'wrong_color'):
                self.assertFalse(any(s['op'] == 'cleanup' for s in self.case(f'{mode}_{who}')['steps']))


    def test_evergreen_actual_fresh_attack_eligibility_and_combat_receipts(self):
        for seat in (0, 1):
            for name in ('Flameborn Hellion', 'Reckless Brute', 'Valley Dasher', 'Ashen Monstrosity', 'Impetuous Sunchaser'):
                steps = self.case(f'paid_fresh_{seat}', name)['steps']
                d = self.one_declaration(steps)
                self.assertIn(dict(op='cast', ref='source', value=1), steps)
                self.assertFalse(any(s['op'] in ('untap_step', 'advance_turn') for s in steps))
                self.assertLess(d.index(dict(op='can_finish_attackers', value=0)), d.index(dict(op='attack', ref='source', value=1)))
            for name in ('Urborg Drake', 'Utvara Scalper', 'Impetuous Sunchaser'):
                for mode, legal in [('blocked', 0), ('air_block', 1), ('reach_block', 1)]:
                    steps = self.case(f'{mode}_{seat}', name)['steps']
                    self.assertIn(dict(op='block', ref='blocker', other='source', value=legal), steps)
                    self.assertEqual(self.value(steps, 'assert_card', ref='source', property='blocked'), legal)
            lethal = self.case(f'blocked_{seat}', 'Riot Piker')['steps']
            nonlethal = self.case(f'first_nonlethal_{seat}', 'Riot Piker')['steps']
            self.assertEqual(self.value(lethal, 'assert_card', ref='source', property='damage'), 0)
            self.assertEqual(self.count(lethal, 1-seat, 'graveyard', 'Grizzly Bears'), 1)
            self.assertEqual(self.count(lethal, seat, 'graveyard', 'Riot Piker'), 0)
            self.assertEqual(self.count(nonlethal, seat, 'graveyard', 'Riot Piker'), 1)
            self.assertEqual(self.value(nonlethal, 'assert_card', ref='blocker', property='damage'), 2)



if __name__ == '__main__':
    unittest.main()
