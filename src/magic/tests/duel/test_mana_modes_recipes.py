import copy,json,unittest
from pathlib import Path
from generate_cases import registry
from recipes import expand
from mana_modes_recipes import inputs,pool,spec,variants,SYMBOLS

MENU_COLORS={'Cascade Bluffs':'UR','Fetid Heath':'BW','Fire-Lit Thicket':'GR','Flooded Grove':'UG','Graven Cairns':'BR','Mystic Gate':'UW','Rugged Prairie':'WR','Sunken Ruins':'UB','Twilight Mire':'BG','Wooded Bastion':'GW'}

def declaration(o,name,vs=None):
 modes=[]
 for j,line in enumerate(o[name]['oracle_text'].splitlines()):
  cost,wording=line.split(': Add ');wording=wording.removesuffix('.')
  form='any_color' if wording=='one mana of any color' else 'choice' if ' or ' in wording else 'fixed'
  outputs=['{B}','{U}','{G}','{R}','{W}'] if form=='any_color' else wording.replace(', or ',' or ').replace(', ',' or ').split(' or ')
  paid=inputs(cost);choices=[]
  for out in outputs:
   players=[]
   for owner in (0,1):
    scripts=[]
    for payment in paid:
     if name in MENU_COLORS:
      colors=MENU_COLORS[name]
      if j==0:script=[dict(kind='dialog',value=0)] if owner==0 else []
      elif owner==1:script=[dict(kind='color',value=c) for c,n in enumerate(pool(out)) for _ in range(n)]
      else:
       consumed=next(SYMBOLS[c] for c,n in enumerate(payment) if n);mode_index=0 if out==('{'+colors[0]+'}')*2 else 2 if out==('{'+colors[1]+'}')*2 else 1
       script=[dict(kind='dialog',value=1+3*colors.index(consumed)+mode_index)]
     elif name=='Crystal Quarry':script=[dict(kind='dialog',value=j)]+([dict(kind='color',value=0)] if j==0 else [])
     else:script=[dict(kind='color',value=next(c for c,n in enumerate(pool(out)) if n))]
     scripts.append(script)
    players.append(scripts)
   choices.append(players)
  fallback=[[],[]] if name!='Crystal Quarry' or j!=0 else [[dict(kind='color',value=0)],[dict(kind='dialog',value=0),dict(kind='color',value=0)]]
  modes.append(dict(cost=cost,production=form,outputs=outputs,choices=choices,fallback_choices=fallback))
 return dict(family='mana_modes',version=2,params=dict(modes=modes),oracle_text=o[name]['oracle_text'],variants=vs or ['entry_0'])

class ManaModesRecipesTest(unittest.TestCase):
 @classmethod
 def setUpClass(cls):cls.o=json.loads((Path(__file__).parent/'data/oracle.json').read_text(encoding='utf8'))['cards'];cls.rows={r['name']:r for r in registry()}
 def cases(self,name='Mystic Gate',vs=None,d=None,rows=None,o=None):return expand(name,self.rows[name]['csvid'],d or declaration(self.o,name,vs),self.o[name],rows or self.rows,oracle_context=o or self.o)
 def test_hybrid_input_has_both_legal_payments(self):self.assertEqual(inputs('{W/U}, {T}'),[[0,0,0,0,0,1],[0,0,1,0,0,0]])
 def test_generic_input_is_not_hybrid(self):self.assertEqual(inputs('{5}, {T}'),[[5,0,0,0,0,0]])
 def test_invalid_inputs_fail_closed(self):
  for c in ('{0}, {T}','{W/W}, {T}','{2/W}, {T}','{W/P}, {T}','{X}, {T}',False):
   with self.assertRaises(ValueError):inputs(c)
 def test_five_distinct_output_symbols(self):self.assertEqual(pool('{W}{U}{B}{R}{G}'),[0,1,1,1,1,1])
 def test_invalid_outputs_fail_closed(self):
  for o in ('{X}','{C}'*6,'{W/U}',{},''):
   with self.assertRaises(ValueError):pool(o)
 def test_params_pin_and_mode_shapes_fail_closed(self):
  d=declaration(self.o,'Mystic Gate')
  for patch in (dict(params={}),dict(oracle_text='changed'),dict(params=dict(modes=[])),dict(params=dict(modes=[dict(d['params']['modes'][0],cost='{0}, {T}'),d['params']['modes'][1]]))):
   with self.assertRaises(ValueError):self.cases(d=dict(d,**patch))
 def test_extra_clauses_and_faces_rejected(self):
  d=declaration(self.o,'Mystic Gate');r=dict(self.o['Mystic Gate'],oracle_text=d['oracle_text']+'\nWhen this land enters, draw a card.');d['oracle_text']=r['oracle_text']
  with self.assertRaises(ValueError):spec(d,r)
  with self.assertRaises(ValueError):spec(declaration(self.o,'Mystic Gate'),dict(self.o['Mystic Gate'],layout='transform'))
 def test_cost_and_output_mismatch_rejected(self):
  for field,value in (('cost','{1}, {T}'),('outputs',['{W}'])):
   d=declaration(self.o,'Mystic Gate');d['params']['modes'][1][field]=value
   with self.assertRaises(ValueError):self.cases(d=d)
 def test_decision_scripts_are_bounded_and_typed(self):
  for script in ([dict(kind='dialog',value=True)],[dict(kind='color',value=6)],[dict(kind='unknown',value=0)],[dict(kind='dialog',value=17)]):
   d=declaration(self.o,'Mystic Gate');d['params']['modes'][0]['choices']=[[[script],[[]]]]
   with self.assertRaises(ValueError):self.cases(d=d)
 def test_unique_variants_cover_every_output_input_and_controller(self):
  p=spec(declaration(self.o,'Mystic Gate'),self.o['Mystic Gate']);v=variants(p);self.assertEqual(len(v),len(set(v)))
  for owner in (0,1):
   for out in range(3):
    for payment in range(2):self.assertIn(f'payment_1_{out}_{payment}_{owner}',v)
 def test_invalid_variant_lists_rejected(self):
  for vs in ([],['bad_0'],['entry_0','entry_0']):
   d=declaration(self.o,'Mystic Gate');d['variants']=vs
   with self.assertRaises(ValueError):self.cases(d=d)
 def test_hybrid_payment_consumes_each_alternative_exactly(self):
  for c in self.cases(vs=['payment_1_1_0_0','payment_1_1_1_0']):
   s=c['steps'];self.assertIn(dict(op='assert_mana',player=0,color=2,value=1),s);self.assertIn(dict(op='assert_mana',player=0,color=5,value=1),s)
 def test_mana_ability_resolves_immediately(self):
  s=self.cases(vs=['payment_1_0_0_0'])[0]['steps'];self.assertIn(dict(op='assert_stack',value=0),s);self.assertNotIn(dict(op='resolve'),s);self.assertIn(dict(op='assert_card',ref='source',property='tapped',value=1),s)
 def test_tapping_prevents_repeat(self):
  s=self.cases(vs=['repeat_denied_1_0_0_0'])[0]['steps'];self.assertIn(dict(op='require_can_activate',ref='source',value=0),s);self.assertIn(dict(op='activate',ref='source',value=0),s)
 def test_untap_repeat_casts_real_twiddle(self):
  s=self.cases(vs=['untap_repeat_1_0_0_0'])[0]['steps'];self.assertIn(dict(op='cast',ref='untap',value=1),s);self.assertEqual(sum(x['op']=='activate' and x['value']==1 for x in s),2)
 def test_zero_pool_fallback_cannot_bootstrap_second_activation(self):
  s=self.cases(vs=['zero_pool_fallback_0'])[0]['steps'];self.assertIn(dict(op='assert_mana',player=0,color=0,value=1),s);self.assertIn(dict(op='require_can_activate',ref='source',value=0),s)
 def test_cast_cost_and_fresh_noncreature_activation(self):
  for c in self.cases('Prismatic Lens',['cast_payment_0','fresh_activation_0']):self.assertIn(dict(op='cast',ref='source',value=1),c['steps'])
 def test_real_colored_and_generic_spending(self):
  for c in self.cases(vs=['spend_colored_1_1_0_0','spend_generic_1_1_0_0']):self.assertTrue(any(x['op']=='cast' and x['ref'].startswith('spender') for x in c['steps']));self.assertTrue(all(dict(op='assert_mana',player=0,color=k,value=0) in c['steps'] for k in range(6)))
 def test_snapshots_replay_around_ai(self):
  for c in self.cases(vs=['ready_snapshot_1_0_0_0','mana_snapshot_1_0_0_0']):
   for op in ('capture','record_state','ai_roundtrip','restore','assert_recorded_state'):self.assertIn(dict(op=op),c['steps'])
 def test_populated_and_bare_zones_are_separate(self):
  a,b=self.cases(vs=['entry_0','bare_zones_0']);self.assertEqual(a['fixture'],'stocked_zones');self.assertEqual(b['fixture'],'empty_main')
 def test_all_final_zones_and_ownership_checked(self):
  s=self.cases()[0]['steps'];self.assertEqual({x['zone'] for x in s if x['op']=='assert_zone'},{'battlefield','hand','graveyard','library','exile'});self.assertEqual({x['player'] for x in s if x['op']=='assert_zone'},{0,1});self.assertTrue(any(x['op']=='assert_zone_card' for x in s))
 def test_handler_independence_and_witness_pin_mutation(self):
  rows=copy.deepcopy(self.rows)
  for row in rows.values():row['handler']='card_dummy';row['mana_cost']='{99}'
  self.assertEqual(self.cases(),self.cases(rows=rows));o=copy.deepcopy(self.o);o['Twiddle']['oracle_text']='Untap target creature.'
  with self.assertRaises(ValueError):self.cases(o=o)
 def test_human_menu_binds_both_input_and_output(self):
  a,b=self.cases(vs=['payment_1_0_0_0','payment_1_0_1_0']);self.assertIn(dict(op='choose_dialog',player=0,value=6),a['steps']);self.assertIn(dict(op='choose_dialog',player=0,value=3),b['steps'])
 def test_ai_output_script_selects_both_mana_symbols(self):
  s=self.cases(vs=['payment_1_1_0_1'])[0]['steps'];self.assertIn(dict(op='choose_color',player=1,value=2),s);self.assertIn(dict(op='choose_color',player=1,value=5),s)
 def test_crystal_quarry_free_mode_uses_colorless_subchoice(self):
  for c in self.cases('Crystal Quarry',['payment_0_0_0_0','payment_0_0_0_1','payment_1_0_0_0']):self.assertIn(dict(op='assert_mana',player=int(c['_recipe']['variant'][-1]),color=0,value=1 if c['_recipe']['variant'].startswith('payment_0') else 0),c['steps'])
 def test_missing_controller_or_payment_script_rejected(self):
  for bad in ([],[[[]]],[[[]],[[],[]]]):
   d=declaration(self.o,'Mystic Gate');d['params']['modes'][1]['choices'][0]=bad
   with self.assertRaises(ValueError):self.cases(d=d)
 def test_colored_reserves_can_fund_generic_filter_mode(self):
  s=self.cases('Crystal Quarry',['production_0_0_0_0'])[0]['steps'];self.assertIn(dict(op='choose_dialog',player=0,value=0),s);self.assertIn(dict(op='choose_color',player=0,value=0),s)

if __name__=='__main__':unittest.main()
