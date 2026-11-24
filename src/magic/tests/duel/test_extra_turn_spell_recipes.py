"""Focused guards for whole extra-turn scheduling, actual spell inputs and loss."""

import json
from pathlib import Path
import unittest

from generate_cases import registry
from recipes import expand
import extra_turn_spell_recipes as family


class ExtraTurnSpellTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        data = Path(__file__).parent/'data'
        cls.oracle = json.loads((data/'oracle.json').read_bytes())['cards']
        cls.rows = {row['name']:row for row in registry()}
        cls.declarations,cls.cases = {},{}
        for name,pin in family.pins()['sources'].items():
            csvid = pin['registry']['csvid']
            path, = (data/'cases/cards'/f'{csvid//100:03d}').glob(f'{csvid:05d}-*.json')
            declaration = json.loads(path.read_bytes())['recipes'][0]
            cls.declarations[name] = declaration
            cls.cases[name] = {case['_recipe']['variant']:case for case in expand(
                name,csvid,declaration,cls.oracle[name],cls.rows,cls.oracle)}
        follow = ('Time Warp','Time Stretch','Beacon of Tomorrows')
        cls.target_declarations = {name:cls.declarations.pop(name) for name in follow}
        cls.target_cases = {name:cls.cases.pop(name) for name in follow}

    def steps(self, name, mode, owner):
        return self.cases[name][f'{mode}_{owner}']['steps']

    def seats(self, steps):
        return [s['value'] for s in steps if s['op']=='advance_scheduled_turn']

    def last_life(self, steps, player):
        return [s['value'] for s in steps if s['op']=='assert_life' and s['player']==player][-1]

    def test_whole_oracle_printed_cost_and_source_eligibility(self):
        self.assertEqual(sum(map(len,self.cases.values())),220)
        expected_fees = ([3,0,2,0,0,0],[3,0,2,0,0,0],[1,0,1,0,0,0],[0,0,0,0,2,0],
                         [0,0,0,0,2,0],[0,0,0,0,2,0])
        self.assertEqual(tuple(p['params']['cast_cost'] for p in self.declarations.values()),expected_fees)
        for name,decl in self.declarations.items():
            record = self.oracle[name]
            with self.assertRaises(ValueError):
                family.parameters(dict(record,oracle_text=record['oracle_text']+' Draw a card.'))
            with self.assertRaises(ValueError):
                family.spec(dict(decl,params=dict(decl['params'],instant=int(decl['params']['instant']))),record)
        wrong = dict(self.rows,**{'Platinum Angel':dict(self.rows['Platinum Angel'],handler='card_dummy')})
        with self.assertRaises(ValueError):
            family.validate_witnesses(self.oracle,wrong)

    def test_one_initial_seat_and_no_expected_player_setter_afterward(self):
        for card in self.cases.values():
            for case in card.values():
                steps = case['steps']
                self.assertEqual([s for s in steps if s['op']=='turn'],steps[:1])
                self.assertNotIn(dict(op='advance_turn'),steps)
                for op in [s for s in steps if s['op']=='advance_scheduled_turn']:
                    self.assertEqual(set(op),{'op','value'})
                    self.assertIn(op['value'],(-1,0,1))

    def test_ordinary_extra_turn_resumes_normal_alternation_and_pays(self):
        for name,decl in self.declarations.items():
            for owner in (0,1):
                steps = self.steps(name,'single',owner)
                cast = steps.index(dict(op='cast',ref='source',value=1))
                fee = [0]*6
                for step in steps[:cast]:
                    if step['op']=='set_mana' and step['player']==owner:
                        fee[step['color']] = step['value']
                self.assertEqual(fee,decl['params']['cast_cost'])
                if not decl['params']['lose_at_linked_end']:
                    self.assertEqual(self.seats(steps),[owner,1-owner,owner])
                    self.assertEqual(self.last_life(steps,owner),20)

    def test_linked_loss_only_after_its_real_extra_turn_ends(self):
        for name,decl in self.declarations.items():
            if not decl['params']['lose_at_linked_end']:
                continue
            for owner in (0,1):
                expected = [('single',[owner,-1]),('older_plain',[owner,-1]),
                            ('newer_plain',[owner,owner,-1]),('stacked',[owner,-1])]
                for mode,seats in expected:
                    steps = self.steps(name,mode,owner)
                    self.assertEqual(self.seats(steps),seats)
                    terminal = steps.index(dict(op='advance_scheduled_turn',value=-1))
                    self.assertEqual(self.last_life(steps[:terminal],owner),20)
                    self.assertEqual(self.last_life(steps[terminal:],owner),family.LOSS_SENTINEL)
                    self.assertEqual(self.last_life(steps,1-owner),20)

    def test_two_independent_grants_and_opposite_player_response_order(self):
        for owner in (0,1):
            for name in ('Time Walk','Capture of Jingzhou','Temporal Manipulation'):
                steps = self.steps(name,'stacked',owner)
                self.assertEqual(self.seats(steps),[owner,owner,1-owner,owner])
            for name in self.cases:
                steps = self.steps(name,'opposing_schedule',owner)
                self.assertIn(dict(op='announce_spell',ref='source'),steps)
                self.assertIn(dict(op='cast_response',ref='opposition',value=1),steps)
                self.assertIn(dict(op='assert_stack',value=2),steps)
                self.assertEqual(self.seats(steps),[owner,1-owner,1-owner,owner])

    def test_real_counter_and_canceled_cast_leave_no_extra_or_pending_loss(self):
        for name in self.cases:
            for owner in (0,1):
                for mode,seats in [('countered',[1-owner,owner,1-owner]),('counter_extra_only',[owner,1-owner,owner])]:
                    steps = self.steps(name,mode,owner)
                    self.assertNotIn(dict(op='choose_target',player=1-owner,ref='source'),steps)
                    self.assertIn(dict(op='cast_response',ref='counter',value=1),steps)
                    self.assertIn(dict(op='assert_stack',value=2),steps)
                    resolved = steps.index(dict(op='resolve_response'))
                    copies = 2 if name=='Time Walk' and mode=='counter_extra_only' else 1
                    self.assertIn(dict(op='assert_zone_multiplicity',player=owner,zone='graveyard',
                                       index=self.rows[name]['csvid'],value=copies),steps[resolved:])
                    self.assertEqual(self.seats(steps),seats)
                    self.assertEqual(self.last_life(steps,owner),20)
                self.assertEqual(self.seats(self.steps(name,'unfunded',owner)),[1-owner,owner])
            canceled = self.steps(name,'cancel',0)
            self.assertIn(dict(op='choose_cancel_mana',player=0),canceled)
            self.assertIn(dict(op='cast',ref='source',value=0),canceled)
            self.assertEqual(self.seats(canceled),[1,0])

    def test_real_skip_meditate_and_replacement_scope(self):
        for name in self.cases:
            for owner in (0,1):
                for mode,seats in [('suppress_foe',[1-owner,owner,1-owner]),('suppress_nexus',[1-owner,owner,1-owner]),
                                   ('skip_meditate',[1-owner,owner,1-owner]),('normal_skip',[owner,1-owner,owner])]:
                    steps = self.steps(name,mode,owner)
                    self.assertEqual(self.seats(steps),seats)
                    self.assertEqual(self.last_life(steps,owner),20)
                    if mode in ('skip_meditate','normal_skip'):
                        self.assertIn(dict(op='cast',ref='skip',value=1),steps)
                        payer = owner if mode=='skip_meditate' else 1-owner
                        self.assertIn(dict(op='assert_zone_multiplicity',player=payer,zone='hand',index=91,value=4),steps)
                self.assertEqual(self.seats(self.steps(name,'suppress_own',owner))[0],owner)

    def test_offturn_instant_and_actual_selected_seat_untap(self):
        for owner in (0,1):
            for mode in ('offturn','offturn_response'):
                steps = self.steps('Final Fortune',mode,owner)
                self.assertEqual(steps[0],dict(op='turn',player=1-owner,value=0))
                self.assertEqual(self.seats(steps),[owner,-1])
                if mode=='offturn_response':
                    self.assertIn(dict(op='announce_spell',ref='parent'),steps)
                    self.assertIn(dict(op='cast_response',ref='source',value=1),steps)
            for name in self.cases:
                steps = self.steps(name,'untap_probe',owner)
                index = steps.index(dict(op='untap_step',player=owner))
                self.assertEqual(steps[index-1],dict(op='advance_scheduled_turn',value=owner))
                self.assertIn(dict(op='assert_card',ref=f'marker_{owner}',property='tapped',value=0),steps[index+1:])
                self.assertIn(dict(op='assert_card',ref=f'marker_{1-owner}',property='tapped',value=1),steps[index+1:])

    def test_loss_prevention_is_one_linked_end_then_no_redelivery(self):
        for name,decl in self.declarations.items():
            if not decl['params']['lose_at_linked_end']:
                continue
            for owner in (0,1):
                steps = self.steps(name,'protected_end',owner)
                removal = steps.index(dict(op='cast',ref='removal',value=1))
                self.assertEqual(self.seats(steps[:removal]),[owner,1-owner])
                self.assertIn(dict(op='choose_target',player=1-owner,ref='protection'),steps[:removal])
                self.assertEqual(self.seats(steps[removal:]),[owner,1-owner,owner])
                self.assertEqual(self.last_life(steps,owner),20)

    def test_snapshot_repeats_real_scheduling_without_resetting_seats(self):
        for name in self.cases:
            for owner in (0,1):
                for mode in ('pending_replay','scheduled_replay'):
                    steps = self.steps(name,mode,owner)
                    at = steps.index(dict(op='capture'))
                    stored = steps.index(dict(op='record_state'))
                    restore = steps.index(dict(op='restore'))
                    self.assertEqual(steps[at+1:stored],steps[restore+1:-1])
                    self.assertTrue(self.seats(steps[at+1:stored]))
                    self.assertIn(dict(op='ai_roundtrip'),steps[stored:restore])
                    self.assertEqual(steps[-1],dict(op='assert_recorded_state'))

    def test_targeted_count_and_real_player_selection_are_independent(self):
        for name, cases in self.target_cases.items():
            price = self.target_declarations[name]['params']['cast_cost']
            n = 2 if name=='Time Stretch' else 1
            self.assertEqual(self.target_declarations[name]['params']['target_turns'],n)
            for owner in (0,1):
                foe = 1-owner
                for mode, target in [('target_self',owner),('target_other',foe)]:
                    steps = cases[f'{mode}_{owner}']['steps']
                    cast = steps.index(dict(op='cast',ref='source',value=1))
                    self.assertEqual(steps[cast-1],dict(op='choose_player',player=owner,value=target))
                    mana = [0]*6
                    for s in steps[:cast]:
                        if s['op']=='set_mana' and s['player']==owner:
                            mana[s['color']] = s['value']
                    self.assertEqual(mana,price)
                    self.assertEqual(self.seats(steps),[target]*n+[foe,owner,foe])
                stack = cases[f'target_stacked_{owner}']['steps']
                self.assertEqual([s['value'] for s in stack if s['op']=='choose_player'],[owner,foe])
                self.assertEqual(self.seats(stack),[foe]*n+[owner]*n+[foe,owner,foe])
                probe = cases[f'target_untap_{owner}']['steps']
                self.assertIn(dict(op='untap_step',player=foe),probe)
                self.assertIn(dict(op='assert_card',ref=f'marker_{owner}',property='tapped',value=1),probe[-5:])
                self.assertIn(dict(op='assert_card',ref=f'marker_{foe}',property='tapped',value=0),probe[-5:])

    def test_target_invalidation_and_counter_are_different_real_responses(self):
        for name,cases in self.target_cases.items():
            for owner in (0,1):
                foe = 1-owner
                for mode, ref in [('target_invalidated','shield'),('target_countered','counter')]:
                    steps = cases[f'{mode}_{owner}']['steps']
                    self.assertIn(dict(op='choose_player',player=owner,value=foe),steps)
                    self.assertIn(dict(op='announce_spell',ref='source'),steps)
                    self.assertIn(dict(op='cast_response',ref=ref,value=1),steps)
                    self.assertIn(dict(op='resolve_response'),steps)
                    self.assertIn(dict(op='assert_zone_multiplicity',player=owner,zone='graveyard',
                                       index=self.rows[name]['csvid'],value=1),steps)
                    self.assertFalse(any(a==b for a,b in zip([owner]+self.seats(steps),self.seats(steps))))

    def test_beacon_shuffles_only_owner_library_after_valid_resolution(self):
        cases = self.target_cases['Beacon of Tomorrows']
        for owner in (0,1):
            foe = 1-owner
            for mode,copies in [('target_other',1),('target_suppressed',1),('target_stacked',2),('empty_library',1),
                                ('target_invalidated',0),('target_countered',0),('wrong_color',0)]:
                steps = cases[f'{mode}_{owner}']['steps']
                shuffles = [s['value'] for s in steps if s['op']=='assert_shuffle_count' and s['player']==owner]
                self.assertEqual(shuffles[0],0)
                self.assertEqual(shuffles[-1],copies)
                self.assertTrue(all(s['value']==0 for s in steps if s['op']=='assert_shuffle_count' and s['player']==foe))
                self.assertIn(dict(op='assert_zone_card',player=foe,zone='library',index=0,value=91),steps)
                self.assertIn(dict(op='assert_zone_card',player=foe,zone='library',index=1,value=126),steps)
                if copies:
                    self.assertIn(dict(op='assert_zone_multiplicity',player=owner,zone='library',index=7015,value=copies),steps)
            pending = cases[f'target_replay_{owner}']['steps']
            capture = pending.index(dict(op='capture'))
            self.assertIn(dict(op='choose_player',player=owner,value=foe),pending[:capture])
            recorded = pending.index(dict(op='record_state'))
            restored = pending.index(dict(op='restore'))
            self.assertEqual(pending[capture+1:recorded],pending[restored+1:-1])
            self.assertEqual(pending[-1],dict(op='assert_recorded_state'))


if __name__=='__main__':
    unittest.main()
