"""Protect actual choice and payment paths in the Flash alternate-cost cohort."""
import json
import unittest

from generate_cases import registry
from recipes import COLORS
import flash_remaining_alternate_costs_and_modes_recipes as model


class FlashRemainingAlternateCostsTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        data = model.PROFILE.parent
        cls.oracle = json.loads((data / 'oracle.json').read_bytes())['cards']
        cls.rows = {row['name']: row for row in registry() if row['kind'] != 'internal'}
        cls.homes = {}
        for name, pin in model.profiles()['sources'].items():
            home, = (data / 'cases/cards').glob(f"*/{pin['registry']['csvid']:05d}-*.json")
            cls.homes[name] = json.loads(home.read_bytes())['recipes'][0]

    def case(self, name, mode):
        source = self.oracle[name]
        declaration = self.homes[name]
        spec = model.spec(declaration, source)
        return model.expand(name, spec['csvid'], declaration, spec, [mode], self.rows, self.oracle)[0]

    def test_complete_normal_source_and_witness_pins(self):
        model.validate_witnesses(self.oracle, self.rows)
        for name, declaration in self.homes.items():
            shape = model.spec(declaration, self.oracle[name])
            self.assertEqual(declaration['variants'], model.variants(shape))
            with self.assertRaises(ValueError):
                model.spec(declaration, dict(self.oracle[name], oracle_text='Flash'))
        rows = dict(self.rows)
        rows[model.PINGER] = dict(rows[model.PINGER], handler='card_dummy')
        with self.assertRaises(ValueError):
            model.validate_witnesses(self.oracle, rows)

    def test_offalsnout_observes_both_real_choices_without_weakening_exile(self):
        for seat in (0, 1):
            for mode in ('evoke_foe', 'evoke_own', 'normal_death'):
                steps = self.case('Offalsnout', f'{mode}_{seat}')['steps']
                idx = steps.index(dict(op='assert_unused_choices', value=0))
                choices = [step for step in steps[:idx] if step['op'] in ('choose_player', 'choose_zone_card')]
                self.assertEqual([action['op'] for action in choices], ['choose_player', 'choose_zone_card'])
                victim = choices[1]['value']
                recipient = seat if mode == 'evoke_own' else 1-seat
                self.assertEqual(choices[0]['value'], recipient)
                for zone, amount in (('graveyard', 0), ('exile', 1)):
                    self.assertIn(dict(op='assert_zone_multiplicity', player=recipient, zone=zone,
                                       index=victim, value=amount), steps[idx:])

    def test_cycling_paid_against_real_opponent_activation_and_own_control(self):
        for seat in (0, 1):
            for mode, choice_values, life in (('cycle_enemy', [1, 0], 20), ('cycle_own', [1], 19)):
                steps = self.case('Nimble Obstructionist', f'{mode}_{seat}')['steps']
                activate = steps.index(dict(op='activate', ref='pinger', value=1))
                cycle = steps.index(dict(op='activate_hand', ref='source', value=1))
                self.assertLess(activate, cycle)
                if mode == 'cycle_enemy':
                    self.assertEqual(steps[cycle+1], dict(op='assert_unused_choices', value=0))
                self.assertEqual([action['value'] for action in steps[activate:cycle]
                                  if action['op'] == 'choose_dialog'], choice_values)
                self.assertIn(dict(op='assert_mana', player=seat, color=COLORS['U'], value=0), steps[cycle:])
                self.assertIn(dict(op='assert_life', player=1-seat, value=life), steps[cycle:])

    def test_samut_real_haste_double_strike_vigilance_and_foreign_untap(self):
        for seat in (0, 1):
            steps = self.case('Samut, Voice of Dissent', f'haste_team_{seat}')['steps']
            advance = steps.index(dict(op='advance_turn'))
            self.assertEqual(steps[advance-7:advance-1], [
                dict(op='set_mana', player=seat, color=color, value=0) for color in range(6)])
            attack = steps.index(dict(op='begin_attackers'))
            for ref, tapped in (('source', 0), ('ally', 1)):
                self.assertIn(dict(op='cast', ref=ref, value=1), steps[:attack])
                self.assertIn(dict(op='attack', ref=ref, value=1), steps[attack:])
                self.assertIn(dict(op='assert_card', ref=ref, property='tapped', value=tapped), steps[attack:])
            self.assertIn(dict(op='assert_life', player=1-seat, value=12), steps[attack:])
            steps = self.case('Samut, Voice of Dissent', f'foreign_untap_{seat}')['steps']
            self.assertIn(dict(op='choose_target', ref='enemy', player=seat), steps)
            self.assertIn(dict(op='probe_activation_target', ref='source', other='source', index=-1, value=0), steps)
            self.assertIn(dict(op='assert_card', ref='enemy', property='tapped', value=0), steps)


if __name__ == '__main__':
    unittest.main()
