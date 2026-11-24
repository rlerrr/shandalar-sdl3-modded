import json
import unittest
from pathlib import Path

from entry_discard_payment_recipes import FAMILY, classify, expand, profiles, spec, validate_witnesses, variants
from generate_cases import registry


class EntryDiscardPaymentTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.oracle = json.loads((Path(__file__).parent / 'data/oracle.json').read_text(encoding='utf8'))['cards']
        cls.rows = {card['name']: card for card in registry()}

    def case(self, name, mode, actor=0):
        record = self.oracle[name]
        selected = [f'{mode}_{actor}']
        declaration = dict(family=FAMILY, version=2, params=classify(record), oracle_text=record['oracle_text'],
                           variants=selected)
        case, = expand(name, self.rows[name]['csvid'], declaration, spec(declaration, record), selected,
                       self.rows, self.oracle)
        self.assertFalse(any('state' in card for card in case['setup']))
        self.assertFalse(any(s['op'] in ('activate', 'destroy', 'move', 'discard', 'draw') for s in case['steps']))
        return case['steps']

    def actions(self, steps, op, **fields):
        return [s for s in steps if s['op'] == op and all(s.get(key) == value for key, value in fields.items())]

    def values(self, steps, op, **fields):
        return [s['value'] for s in self.actions(steps, op, **fields)]

    def copies(self, steps, card, player, zone):
        return self.values(steps, 'assert_zone_multiplicity', index=self.rows[card]['csvid'], player=player, zone=zone)

    def test_exact_whole_printed_clauses_and_hybrid_vectors(self):
        validate_witnesses(self.oracle, self.rows)
        self.assertEqual({self.rows[name]['csvid'] for name in profiles()['sources']}, {1686, 5615, 2892, 8169})
        for name in profiles()['sources']:
            record = self.oracle[name]
            params = classify(record)
            declaration = dict(family=FAMILY, version=2, params=params, oracle_text=record['oracle_text'],
                               variants=variants(params))
            self.assertEqual(spec(declaration, record), dict(params, family=FAMILY))
            with self.assertRaises(ValueError):
                spec(dict(declaration, params=dict(params, random=int(params['random']))), record)
            with self.assertRaises(ValueError):
                classify(dict(record, oracle_text=record['oracle_text'] + '\nTrample'))
        avatar = classify(self.oracle['Avatar of Discord'])
        self.assertEqual({(vec[1], vec[4]) for vec in avatar['hybrid']}, {(3, 0), (0, 3), (1, 2), (2, 1)})

    def test_paid_declined_and_no_hand_random_use_real_dialog(self):
        for player in (0, 1):
            for name in ('Balduvian Horde', 'Pillaging Horde', 'Minotaur Explorer'):
                for mode, answer, escaped in (('funded', [0], True), ('decline', [1], False), ('empty', [], False)):
                    steps = self.case(name, mode, player)
                    self.assertEqual(self.values(steps, 'choose_dialog', player=player), answer)
                    self.assertEqual(self.copies(steps, name, player, 'battlefield')[-1], int(escaped))
                    self.assertEqual(self.copies(steps, name, player, 'graveyard')[-1], int(not escaped))
                    if mode != 'empty':
                        self.assertEqual(self.copies(steps, 'Swamp', player, 'graveyard')[-1], int(escaped))
                    self.assertEqual(self.copies(steps, 'Plains', 1-player, 'hand')[-1], 1)

    def test_random_three_exact_eligible_population_without_seed_prediction(self):
        for player in (0, 1):
            steps = self.case('Balduvian Horde', 'mixed_random', player)
            self.assertEqual(self.values(steps, 'assert_zone', player=player, zone='hand')[-1], 2)
            self.assertEqual(self.values(steps, 'assert_zone', player=player, zone='graveyard')[-1], 1)
            for card in ('Forest', 'Swamp', 'Lightning Bolt'):
                pick = self.rows[card]['csvid']
                self.assertEqual(self.values(steps, 'assert_hand_graveyard_total', player=player, index=pick), [1])
                self.assertEqual(self.copies(steps, card, player, 'hand'), [1])
                self.assertEqual(self.copies(steps, card, player, 'graveyard'), [0])

    def test_avatar_requires_two_whole_resources_or_sacrifices(self):
        for player in (0, 1):
            for mode, hand, grave, kept in (('funded', 0, 2, 1), ('extra', 1, 2, 1), ('short_hand', 1, 1, 0)):
                steps = self.case('Avatar of Discord', mode, player)
                self.assertEqual(self.values(steps, 'assert_zone', player=player, zone='hand')[-1], hand)
                self.assertEqual(self.values(steps, 'assert_zone', player=player, zone='graveyard')[-1], grave)
                self.assertEqual(self.copies(steps, 'Avatar of Discord', player, 'battlefield')[-1], kept)

    def test_avatar_two_actual_choices_and_atomic_second_decline(self):
        for mode, policy in (('choose_low', 1), ('choose_high', 2)):
            steps = self.case('Avatar of Discord', mode)
            self.assertEqual(self.values(steps, 'discard_policy'), [policy])
            self.assertEqual(self.copies(steps, 'Lightning Bolt', 0, 'graveyard')[-1], 1)
            self.assertEqual(self.copies(steps, 'Forest', 0, 'graveyard')[-1], int(policy == 1))
            self.assertEqual(self.copies(steps, 'Swamp', 0, 'graveyard')[-1], int(policy == 2))
        for mode, first in (('cancel_first', []), ('cancel_second', [self.rows['Swamp']['csvid']])):
            steps = self.case('Avatar of Discord', mode)
            self.assertEqual(self.values(steps, 'discard_policy'), [0])
            self.assertEqual(self.values(steps, 'choose_zone_card'), first)
            self.assertEqual(len(self.actions(steps, 'choose_optional_zone')), 1)
            for card in ('Forest', 'Swamp', 'Lightning Bolt'):
                self.assertEqual(self.copies(steps, card, 0, 'hand')[-1], 1)
                self.assertEqual(self.copies(steps, card, 0, 'graveyard')[-1], 0)
            self.assertEqual(self.copies(steps, 'Avatar of Discord', 0, 'graveyard')[-1], 1)

    def test_hybrid_exact_cast_payment_and_legal_flying_combat(self):
        menu = self.case('Avatar of Discord', 'payment_menu')
        choice, = self.actions(menu, 'choose_payment')
        cast, = self.actions(menu, 'cast', ref='source')
        self.assertLess(menu.index(choice), menu.index(cast))
        self.assertEqual(choice['value'], (1 << 4) + (2 << 16))
        for color, remainder in ((1, 2), (4, 1)):
            self.assertEqual(self.values(menu, 'assert_mana', player=0, color=color)[-1], remainder)
        for player in (0, 1):
            for mode, blocked in (('flying_ground', 0), ('flying_reach', 1)):
                steps = self.case('Avatar of Discord', mode, player)
                self.assertEqual(self.values(steps, 'block', ref='blocker'), [blocked])
                self.assertEqual(self.values(steps, 'assert_life', player=1-player)[-1], 20 if blocked else 15)

    def test_invalid_cost_leaves_source_hand_and_trigger_unfired(self):
        for player in (0, 1):
            for name in profiles()['sources']:
                for mode in ('wrong_mana', 'mana_short'):
                    steps = self.case(name, mode, player)
                    self.assertEqual(self.values(steps, 'cast', ref='source'), [0])
                    self.assertFalse(self.actions(steps, 'choose_dialog'))
                    self.assertEqual(self.copies(steps, name, player, 'hand')[-1], 1)

    def test_paid_pending_and_countered_leave_eligible_discard_untouched(self):
        for player in (0, 1):
            for name in profiles()['sources']:
                steps = self.case(name, 'countered', player)
                self.assertEqual(self.values(steps, 'assert_card', ref='pitch_0', property='in_hand'), [1])
                self.assertEqual(self.values(steps, 'cast_response', ref='counter'), [1])
                self.assertFalse(self.actions(steps, 'choose_dialog'))
                self.assertEqual(self.copies(steps, 'Swamp', player, 'hand')[-1], classify(self.oracle[name])['amount'])
                self.assertEqual(self.copies(steps, name, player, 'graveyard')[-1], 1)

    def test_paid_real_foreign_entry_and_original_owner_destination(self):
        for player in (0, 1):
            for name in profiles()['sources']:
                for mode in ('borrowed_paid', 'borrowed_empty'):
                    steps = self.case(name, mode, player)
                    self.assertEqual(self.values(steps, 'cast', ref='carrier'), [1])
                    self.assertEqual(self.values(steps, 'choose_zone_card', player=player)[0], self.rows[name]['csvid'])
                    self.assertEqual(self.copies(steps, 'Plains', 1-player, 'hand')[-1], 1)
                    self.assertEqual(self.copies(steps, name, 1-player, 'graveyard')[-1], int(mode == 'borrowed_empty'))
                    self.assertEqual(self.copies(steps, name, player, 'battlefield')[-1], int(mode == 'borrowed_paid'))

    def test_borrowed_random_dialog_probe_preserves_controller_and_owner_rules(self):
        for name in ('Balduvian Horde', 'Minotaur Explorer', 'Pillaging Horde'):
            for player in (0, 1):
                for mode, unused in (('borrowed_paid', 0), ('borrowed_empty', 1)):
                    steps = self.case(name, mode, player)
                    self.assertEqual(self.values(steps, 'choose_dialog', player=-1), [0])
                    self.assertEqual(self.values(steps, 'assert_unused_choices'), [0, unused])
                    self.assertEqual(self.values(steps, 'assert_zone', player=player, zone='graveyard')[-1], 2 - unused)
                    if not unused:
                        self.assertEqual(self.copies(steps, 'Swamp', player, 'graveyard')[-1], 1)
                    self.assertEqual(self.copies(steps, 'Plains', 1-player, 'hand')[-1], 1)
                    self.assertEqual(self.copies(steps, name, 1-player, 'graveyard')[-1], unused)

    def test_true_sacrifice_ignores_indestructibility_and_replay_matches(self):
        steps = self.case('Minotaur Explorer', 'indestructible')
        self.assertEqual(self.values(steps, 'cast', ref='control_kill'), [1])
        self.assertEqual(self.copies(steps, 'Avacyn, Angel of Hope', 0, 'battlefield')[-1], 1)
        self.assertEqual(self.copies(steps, 'Minotaur Explorer', 0, 'graveyard')[-1], 1)
        snap = self.case('Pillaging Horde', 'snapshot', 1)
        ops = [s['op'] for s in snap]
        self.assertEqual(snap[ops.index('capture')+1:ops.index('record_state')],
                         snap[ops.index('restore')+1:ops.index('assert_recorded_state')])
