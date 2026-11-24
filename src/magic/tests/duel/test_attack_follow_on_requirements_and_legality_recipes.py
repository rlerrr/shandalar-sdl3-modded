"""Guard the actual follow-on input and scope differences."""

from copy import deepcopy
import unittest

import attack_follow_on_requirements_and_legality_recipes as model


class AttackRequirementGuards(unittest.TestCase):
    def fixtures(self):
        records = {}
        for group in model.profiles().values():
            records.update(group)
        return {n: p['oracle'] for n,p in records.items()}, {n: p['registry'] for n,p in records.items()}

    def case(self, name, mode, seat):
        oracle, rows = self.fixtures()
        board = model.FollowOn(oracle[name],rows[name]['csvid'],mode,seat,rows,oracle)
        board.play()
        return board

    def test_exact_whole_record_guards(self):
        oracle, rows = self.fixtures()
        model.validate_witnesses(oracle, rows)
        record = oracle['Spectral Force']
        params = model.classify(record, model.FORCE)
        declaration = dict(family=model.FAMILY,version=2,oracle_text=record['oracle_text'],
                           params=params,variants=model.variants(params))
        self.assertEqual(model.spec(declaration,record)['csvid'],model.FORCE)
        bad = deepcopy(record)
        bad['oracle_text'] = bad['oracle_text'].replace('black permanents', 'black nontoken permanents')
        with self.assertRaises(ValueError):
            model.spec(declaration,bad)
        with self.assertRaises(ValueError):
            model.spec(declaration | {'variants':['empty_0','empty_0']},record)

    def test_amplify_uses_actual_reveal_and_exact_counters(self):
        for seat in (0,1):
            board = self.case('Feral Throwback','reveal_two',seat)
            self.assertEqual(board.cards['beast_0']['zone'],'hand')
            self.assertEqual(board.cards['beast_1']['zone'],'hand')
            self.assertEqual([(s['player'],s['value']) for s in board.steps if s['op']=='choose_zone'],[(0,-1)])
            self.assertEqual([s['value'] for s in board.steps if s['op']=='assert_counter'],[4])

    def test_actual_token_takes_opposite_untap_rules(self):
        oracle,rows = self.fixtures()
        for name,branch,expected in (('Spectral Force',model.FollowOn,0),('Spectral Bears',model.Board,1)):
            board = branch(oracle[name],rows[name]['csvid'],'black_token',0,rows,oracle)
            board.play()
            self.assertTrue(any(s['op']=='destroy' and s.get('ref')=='dissenter' for s in board.steps))
            skip = next(i for i,s in enumerate(board.steps) if s['op']=='untap_step')
            self.assertEqual(board.steps[skip+1]['value'],expected)

    def test_first_strike_observes_live_counterdamage(self):
        low = self.case('Lowland Tracker','first_strike',0)
        self.assertEqual(low.cards['source']['zone'],'battlefield')
        self.assertEqual(low.cards['guard']['zone'],'graveyard')
        unbanded = self.case('Icatian Skirmishers','unbanded',1)
        self.assertTrue(any(s.get('ref')=='ally' and s.get('property')=='damage' and s['value']==1 for s in unbanded.steps))


if __name__ == '__main__':
    unittest.main()
