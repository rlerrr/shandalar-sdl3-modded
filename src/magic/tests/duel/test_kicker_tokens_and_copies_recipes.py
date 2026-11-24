"""Guards for exact kicker prices, copy ordering, and replay boundaries."""

from copy import deepcopy
import unittest

from kicker_tokens_and_copies_recipes import FAMILY, KickerProductBoard, classify, profiles, spec, variants


class KickerTokenProfilesTest(unittest.TestCase):
    def setUp(self):
        pins = profiles()
        objects = pins['sources'] | pins['witnesses']
        self.oracle = {name: entry['oracle'] for name, entry in objects.items()}
        self.rows = {name: entry['registry'] for name, entry in objects.items()}

    def make(self, name, mode, owner=0):
        model = classify(self.oracle[name])
        board = KickerProductBoard(name, model, mode, owner, self.rows, self.oracle)
        board.run()
        return board

    def test_exact_complete_records_reject_modified_payment_and_duplicate_variants(self):
        for name, pin in profiles()['sources'].items():
            record = pin['oracle']
            model = classify(record)
            valid = dict(family=FAMILY, version=2, oracle_text=record['oracle_text'],
                         params=model, variants=variants(model))
            self.assertEqual(spec(valid, record)['family'], FAMILY)
            for mutation in ('payment', 'duplicate'):
                changed = deepcopy(valid)
                if mutation == 'payment':
                    changed['params']['fee'][0] += 1
                else:
                    changed['variants'].append(changed['variants'][0])
                with self.assertRaises(ValueError, msg=f'{name}: {mutation}'):
                    spec(changed, record)

    def test_paid_rite_uses_target_before_menu_and_paid_state_before_replay(self):
        board = self.make('Rite of Replication', 'replay', 1)
        choice = [s for s in board.steps[:board.pending] if s['op'].startswith('choose_')]
        self.assertEqual([s['op'] for s in choice], ['choose_target', 'choose_dialog'])
        self.assertEqual((choice[0]['ref'], choice[0]['player'], choice[1]['value']), ('copy_target', 1, 0))
        self.assertEqual(board.steps[board.pending]['op'], 'resolve')
        self.assertIn({'op': 'assert_stack', 'value': 1}, board.steps[:board.pending])

    def test_declined_and_short_receipts_precede_clearing_for_combat(self):
        for mode, amount in [('declined', 3), ('short', 2)]:
            board = self.make('Verix Bladewing', mode, 1)
            steps = board.steps
            turn = next(i for i, s in enumerate(steps) if s['op'] == 'advance_turn')
            resolve = next(i for i, s in enumerate(steps) if s['op'] == 'resolve')
            self.assertIn(dict(op='assert_mana', player=1, color=0, value=amount), steps[resolve:turn])
            self.assertIn(dict(op='set_mana', player=1, color=0, value=0), steps[resolve:turn])
            menu = [s['value'] for s in steps[:resolve] if s['op'] == 'choose_dialog']
            self.assertEqual(menu, [1] if mode == 'declined' else [])
