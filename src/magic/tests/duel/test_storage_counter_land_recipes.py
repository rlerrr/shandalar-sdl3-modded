"""Guard the exact Oracle pins and the real phase/choice boundaries."""
import copy
import json
import unittest
from pathlib import Path

import storage_counter_land_recipes as model
from generate_cases import registry


class StorageCounterLandRecipesTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        data = Path(__file__).parent / 'data'
        cls.oracle = json.loads((data/'oracle.json').read_text(encoding='utf8'))['cards']
        cls.rows = {row['name']:row for row in registry()}

    def declaration(self, name):
        oracle = self.oracle[name]
        mechanism = model.mechanism(oracle)
        return dict(family=model.FAMILY,version=2,oracle_text=oracle['oracle_text'],params=dict(schema=1,mechanism=mechanism))

    def cases(self, name, *variants):
        declaration = self.declaration(name)
        params = model.spec(declaration,self.oracle[name])
        return model.expand(name,self.rows[name]['csvid'],declaration,params,variants,self.rows,self.oracle)

    def case(self, variant, name='Bottomless Vault'):
        return self.cases(name,variant)[0]['steps']

    def test_whole_source_pins_colors_and_complete_variants(self):
        identities = {'Bottomless Vault':(1753,'B'), 'Dwarven Hold':(1765,'R'), 'Hollow Trees':(1791,'G'),
                      'Icatian Store':(1804,'W'), 'Sand Silos':(1819,'U')}
        self.assertEqual(set(model.pins()['sources']),set(identities))
        model.validate_witnesses(self.oracle,self.rows)
        count = 0
        for name,(csvid,color) in identities.items():
            declaration = self.declaration(name)
            params = model.spec(declaration,self.oracle[name])
            self.assertEqual((params['csvid'],params['color']),(csvid,color))
            cases = self.cases(name,*model.variants(params))
            count += len(cases)
            for case in cases:
                self.assertEqual(case['setup'][0],dict(name=name,ref='source',zone='hand',player=int(case['name'][-1])))
                self.assertEqual(case['_recipe']['covered_clauses'],self.oracle[name]['oracle_text'].splitlines())
                self.assertFalse(any(step['op']=='cast' and step.get('ref')=='source' for step in case['steps']))
        self.assertEqual(count,180)

    def test_rejects_oracle_and_parameter_changes(self):
        name = 'Bottomless Vault'
        oracle = self.oracle[name]
        declaration = self.declaration(name)
        with self.assertRaises(ValueError):
            model.mechanism(dict(oracle,type_line='Basic Land — Swamp'))
        with self.assertRaises(ValueError):
            model.mechanism(dict(oracle,oracle_text=oracle['oracle_text']+'\n{T}: Add {C}.'))
        changed = copy.deepcopy(declaration)
        changed['params']['mechanism']['color'] = 'U'
        with self.assertRaises(ValueError):
            model.spec(changed,oracle)
        with self.assertRaises(ValueError):
            model.spec(declaration,dict(oracle,oracle_id='replaced'))

    def test_land_quota_and_offturn_controls_use_real_land_plays(self):
        for owner in (0,1):
            quota = self.case(f'land_quota_{owner}')
            plays = [(i,s) for i,s in enumerate(quota) if s['op']=='play_land']
            self.assertEqual([s['value'] for _,s in plays],[1,0,1])
            self.assertFalse(any(s['op']=='advance_turn' for s in quota[plays[0][0]:plays[1][0]]))
            self.assertEqual(sum(s['op']=='advance_turn' for s in quota[plays[1][0]:plays[2][0]]),2)
            offturn = self.case(f'offturn_play_{owner}')
            self.assertEqual(offturn[0],dict(op='turn',player=1-owner,value=0))
            self.assertEqual([s['value'] for s in offturn if s['op']=='play_land'],[0,1])

    def test_bank_counter_lifecycle_and_both_human_untap_choices(self):
        for owner in (0,1):
            steps = self.case(f'untap_banked_{owner}')
            self.assertEqual([s['player'] for s in steps if s['op']=='upkeep_step'],[1-owner,owner]*4)
            counters = [s['value'] for s in steps if s['op']=='assert_counter']
            self.assertEqual(counters,[0,0,0,0,1,1,1,1,2,2,2,2,3,3,3,3,3])
            priorities = [s for s in steps if s['op']=='choose_priority']
            if owner==0:
                self.assertEqual(priorities,[dict(op='choose_priority',player=0,value=-2)]*3+[dict(op='choose_priority',player=0,ref='source')])
            else:
                self.assertEqual(priorities,[])
            self.assertEqual([s['value'] for s in steps if s['op']=='assert_card' and s.get('property')=='tapped'][-2:],[0,0])
        steps = self.case('explicit_early_0')
        self.assertEqual([s['value'] for s in steps if s['op']=='assert_counter'],[0]*5)

    def test_selected_number_is_immediately_used_and_colored_payment_is_real(self):
        for owner in (0,1):
            for mode, amounts in (('cash_partial',[1]),('cash_all',[3]),('cash_zero',[0]),('cash_cancel',[-1])):
                steps = self.case(f'{mode}_{owner}')
                choices = [(i,s) for i,s in enumerate(steps) if s['op']=='choose_number']
                self.assertEqual([s['value'] for _,s in choices],amounts)
                for i,step in choices:
                    self.assertEqual((step['player'],steps[i+1]),(owner,dict(op='activate',ref='source',value=int(step['value']>=0))))
                if mode=='cash_cancel':
                    final = steps[choices[0][0]+2:]
                    self.assertIn(dict(op='assert_mana_total',player=owner,value=0),final)
                    self.assertEqual(final[-2:], [dict(op='assert_card',ref='source',property='tapped',value=0),
                                                  dict(op='assert_counter',ref='source',counter_kind='storage',value=3)])
            case = self.cases('Sand Silos',f'spend_mana_{owner}')[0]
            cast = case['steps'].index(dict(op='cast',ref='spender',value=1))
            self.assertIn(dict(op='assert_mana',player=owner,color=2,value=1), case['steps'][:cast])
            self.assertIn(dict(op='assert_mana',player=owner,color=2,value=0), case['steps'][cast:])
            self.assertFalse(any(s['op']=='set_mana' for s in case['steps'] if s.get('value',0)>0))
            self.assertEqual(next(item['name'] for item in case['setup'] if item['ref']=='spender'),'Mist-Cloaked Herald')

    def test_restore_repeats_the_actual_boundary(self):
        for owner in (0,1):
            for mode,boundary in (('replay_upkeep','upkeep_step'),('replay_untap','untap_step'),('replay_cash','activate')):
                steps = self.case(f'{mode}_{owner}')
                ops = [s['op'] for s in steps]
                before = steps[ops.index('capture')+1:ops.index('record_state')]
                after = steps[ops.index('ai_roundtrip')+1:ops.index('assert_recorded_state')]
                self.assertEqual(before,after)
                self.assertIn(boundary,[s['op'] for s in before])
                self.assertTrue(any(s['op']=='assert_counter' for s in before))
