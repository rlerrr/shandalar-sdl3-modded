import json,unittest
from pathlib import Path
from generate_cases import registry
from recipes import explicit_spec,expand,cost_from_oracle
from hybrid_creature_recipes import normalize,EXTRA,MIRRORS

class HybridCreatureRecipesTest(unittest.TestCase):
 @classmethod
 def setUpClass(cls):
  cls.oracle=json.loads((Path(__file__).parent/'data/oracle.json').read_text(encoding='utf8'))['cards'];cls.rows={r['name']:r for r in registry()}
 def declaration(self,name='Nip Gwyllion',selected=None):
  return dict(family='creature_keywords',version=2,params=dict(keywords=['Lifelink'],payment_colors=['W']),oracle_text=self.oracle[name]['oracle_text'],variants=selected or ['cast'])
 def cases(self,d=None):
  return expand('Nip Gwyllion',self.rows['Nip Gwyllion']['csvid'],d or self.declaration(),self.oracle['Nip Gwyllion'],self.rows,oracle_context=self.oracle)
 def test_explicit_color_choice_required(self):
  d=self.declaration();del d['params']['payment_colors']
  with self.assertRaises(ValueError):self.cases(d)
 def test_illegal_color_count_and_type(self):
  for p in ([],['R'],['W','B'],'W',[True]):
   d=self.declaration();d['params']['payment_colors']=p
   with self.assertRaises(ValueError):self.cases(d)
 def test_ordinary_parser_stays_strict(self):
  with self.assertRaises(ValueError):cost_from_oracle(self.oracle['Nip Gwyllion'])
 def test_whole_text_and_keywords_stay_pinned(self):
  for field,value in [('oracle_text','changed'),('params',dict(keywords=[],payment_colors=['W']))]:
   d=self.declaration();d[field]=value
   with self.assertRaises(ValueError):self.cases(d)
 def test_extra_fields_and_variants_fail_closed(self):
  for change in (dict(extra=True),dict(variants=['unknown']),dict(variants=[]),dict(variants=['cast','cast'])):
   with self.assertRaises(ValueError):self.cases(dict(self.declaration(),**change))
 def test_no_oracle_or_declaration_mutation(self):
  d=self.declaration();before=json.dumps((d,self.oracle),sort_keys=True);self.cases(d);self.assertEqual(before,json.dumps((d,self.oracle),sort_keys=True))
 def test_menu_and_exact_payments(self):
  cases=self.cases(self.declaration(selected=['cast','payment',*EXTRA]));by={c['_recipe']['variant']:c for c in cases}
  self.assertTrue(any(s['op']=='choose_payment' for s in by['cast']['steps']));self.assertFalse(any(s['op']=='choose_payment' for s in by['payment']['steps']))
  self.assertEqual(by['hybrid_payment_other']['_recipe']['payment_colors'],['B'])
 def test_controller_mirror_and_snapshot_replay(self):
  c=self.cases(self.declaration(selected=['snapshot_1']))[0];self.assertEqual(c['setup'][0]['player'],1);self.assertEqual(c['steps'][0],dict(op='turn',player=1,value=0));self.assertTrue(all(s['player']==1 for s in c['steps'] if s['op']=='assert_mana_total'))
 def test_populated_zones(self):
  c=self.cases()[0];self.assertEqual(c['fixture'],'stocked_zones');self.assertTrue(any(s['op']=='assert_zone_card' and s['zone']=='library' for s in c['steps']))
 def test_normalized_cost_preserves_characteristics(self):
  _,r,mana,_=normalize(self.declaration(),self.oracle['Nip Gwyllion']);self.assertEqual(mana,[0,0,0,0,0,1]);self.assertEqual(r['colors'],self.oracle['Nip Gwyllion']['colors']);self.assertEqual(r['oracle_text'],self.oracle['Nip Gwyllion']['oracle_text'])
 def test_etb_hybrid_choices_cover_countered_and_second_copy(self):
  name='Wistful Selkie';d=dict(family='etb_value_creature',version=2,params=dict(keywords=[],draw=1,gain_life=0,lose_life=0,payment_colors=['G']*3),oracle_text=self.oracle[name]['oracle_text'],variants=['countered','two_copies','insufficient_mana'])
  cases=expand(name,self.rows[name]['csvid'],d,self.oracle[name],self.rows,oracle_context=self.oracle)
  by={c['_recipe']['variant']:c for c in cases}
  for variant,expected in [('countered',1),('two_copies',2),('insufficient_mana',0)]:
   payments=[s for s in by[variant]['steps'] if s['op']=='choose_payment']
   self.assertEqual(len(payments),expected)
   self.assertTrue(all(s==dict(op='choose_payment',player=0,value=3<<(4*3)) for s in payments))
  countered=by['countered']['steps'];self.assertIn(dict(op='cast',ref='source',value=0),countered)
  self.assertLess(next(i for i,s in enumerate(countered) if s['op']=='choose_payment'),next(i for i,s in enumerate(countered) if s['op']=='choose_priority'))

if __name__=='__main__':unittest.main()
