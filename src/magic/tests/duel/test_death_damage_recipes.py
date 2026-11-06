import copy,json,unittest
from pathlib import Path
from generate_cases import registry
from death_damage_recipes import spec,variants,validate_witnesses,expand

class DeathDamageRecipesTest(unittest.TestCase):
 @classmethod
 def setUpClass(cls):
  cls.oracle=json.loads((Path(__file__).parent/'data/oracle.json').read_text(encoding='utf8'))['cards'];cls.rows={r['name']:r for r in registry()}
 def declaration(self,name='Perilous Myr'):
  return dict(family='death_damage_creature',version=2,oracle_text=self.oracle[name]['oracle_text'],params=dict(amount=1 if name=='Goblin Arsonist' else 2,scope='any',optional=name=='Goblin Arsonist',inactive=[]),variants=['destroy_0'])
 def cases(self,d=None,name='Perilous Myr'):
  d=d or self.declaration(name);p=spec(d,self.oracle[name]);return expand(name,self.rows[name]['csvid'],d,p,d['variants'],self.rows,self.oracle)
 def test_reject_amount_scope_optional_and_whole_text_drift(self):
  for field,value in [('amount',True),('amount',3),('scope','self'),('optional',True),('inactive',['Flying'])]:
   d=self.declaration();d['params'][field]=value
   with self.assertRaises(ValueError):spec(d,self.oracle['Perilous Myr'])
  for name in ('Magma Phoenix','Plague Spitter','Murderous Redcap'):
   d=self.declaration();d['oracle_text']=self.oracle[name]['oracle_text']
   with self.assertRaises(ValueError):spec(d,self.oracle[name])
 def test_whole_ordinary_creature_required(self):
  for patch in (dict(layout='transform'),dict(keywords=['Flying']),dict(toughness='0'),dict(power='*')):
   r=dict(self.oracle['Perilous Myr'],**patch)
   with self.assertRaises(ValueError):spec(self.declaration(),r)
 def test_cast_only_and_exile_have_no_death_damage(self):
  d=self.declaration();d['variants']=['cast_only_0','exile_1']
  for c in self.cases(d):
   self.assertFalse(any(s['op']=='destroy' for s in c['steps']))
   self.assertIn(dict(op='assert_card',ref='victim',property='damage',value=0),c['steps'])
 def test_target_survivor_and_lethal_receipts(self):
  d=self.declaration();d['variants']=['destroy_0','lethal_target_1','indestructible_target_0']
  a,b,c=self.cases(d)
  self.assertIn(dict(op='assert_card',ref='victim',property='damage',value=2),a['steps'])
  self.assertIn(dict(op='assert_card',ref='victim',property='in_play',value=0),b['steps'])
  self.assertIn(dict(op='assert_card',ref='victim',property='in_play',value=1),c['steps'])
  self.assertIn(dict(op='assert_card',ref='unselected',property='damage',value=0),a['steps'])
 def test_optional_decline_does_not_offer_target(self):
  d=self.declaration('Goblin Arsonist');d['variants']=['destroy_0','decline_0']
  a,b=self.cases(d,'Goblin Arsonist')
  self.assertIn(dict(op='choose_target',player=0,ref='victim'),a['steps'])
  self.assertIn(dict(op='choose_cancel_target',player=0),b['steps'])
  self.assertFalse(any(s['op']=='choose_target' for s in b['steps']))
  self.assertIn(dict(op='assert_card',ref='victim',property='damage',value=0),b['steps'])
 def test_bounce_recast_and_blink_death_are_separate_actions(self):
  d=self.declaration();d['variants']=['bounce_recast_1','blink_destroy_0','blink_only_0']
  a,b,c=self.cases(d)
  self.assertEqual(sum(s['op']=='cast' and s.get('ref')=='source' for s in a['steps']),2)
  self.assertIn(dict(op='bind_hand',ref='source',player=1,value=self.rows['Perilous Myr']['csvid']),a['steps'])
  self.assertIn(dict(op='bind_permanent',ref='source',player=0,value=self.rows['Perilous Myr']['csvid']),b['steps'])
  self.assertNotIn(dict(op='destroy',ref='source'),c['steps'])
 def test_death_snapshot_captures_pending_real_removal_and_input(self):
  d=self.declaration();d['variants']=['death_snapshot_0']
  s=self.cases(d)[0]['steps'];cut=s.index(dict(op='capture'))
  self.assertEqual(s[cut+1],dict(op='resolve'))
  self.assertIn(dict(op='choose_target',player=0,ref='victim'),s[:cut])
  for op in ('record_state','ai_roundtrip','restore','assert_recorded_state'):self.assertIn(dict(op=op),s)
 def test_combat_uses_real_declarations_and_damage(self):
  d=self.declaration();d['variants']=['combat_death_1'];s=self.cases(d)[0]['steps']
  self.assertLess(s.index(dict(op='advance_combat')),s.index(dict(op='combat_damage')))
  self.assertIn(dict(op='block',ref='source',other='attacker',value=1),s)
 def test_witness_pins_are_independent(self):
  validate_witnesses(self.oracle,self.rows);o=copy.deepcopy(self.oracle);o['Murder']['oracle_text']='Exile target creature.'
  with self.assertRaises(ValueError):validate_witnesses(o,self.rows)
 def test_hybrid_payment_other_color_is_explicit(self):
  d=self.declaration();r=self.oracle['Footlight Fiend'];d.update(oracle_text=r['oracle_text']);d['params'].update(amount=1,payment_colors=['B']);d['variants']=['alternate_payment_0','wrong_color_0']
  p=spec(d,r);self.assertIn('alternate_payment_1',variants(p));cs=expand('Footlight Fiend',19626,d,p,d['variants'],self.rows,self.oracle)
  self.assertIn(dict(op='set_mana',player=0,color=4,value=1),cs[0]['steps'])
  self.assertIn(dict(op='set_mana',player=0,color=2,value=1),cs[1]['steps'])
 def test_dispatch_rejects_unknown_duplicate_and_empty_variants(self):
  from recipes import expand as dispatched
  for chosen in ([],['not_reviewed_0'],['destroy_0','destroy_0']):
   d=self.declaration();d['variants']=chosen
   with self.assertRaises(ValueError):dispatched('Perilous Myr',11333,d,self.oracle['Perilous Myr'],self.rows,oracle_context=self.oracle)
 def test_complete_variant_dispatch_has_no_injected_engine_events(self):
  from recipes import expand as dispatched
  d=self.declaration();d['variants']=variants(spec(d,self.oracle['Perilous Myr']))
  cs=dispatched('Perilous Myr',11333,d,self.oracle['Perilous Myr'],self.rows,oracle_context=self.oracle)
  self.assertEqual(len(cs),len(d['variants']))
  for c in cs:
   self.assertFalse(any(s['op'] in ('event','call_handler') for s in c['steps']))
   self.assertIn('planeswalker targets',c['_recipe']['untested_components'])
 def test_lethal_targets_and_second_copy_use_real_removal_resolution(self):
  d=self.declaration();d['variants']=['lethal_target_0','two_copies_1']
  for c,count in zip(self.cases(d),(1,2)):
   refs={s['ref'] for s in c['setup'] if s['name']=='Murder'}
   self.assertEqual(len(refs),count)
   self.assertEqual(sum(s['op']=='cast' and s.get('ref') in refs for s in c['steps']),count)
 def test_cleanup_observes_positive_damage_before_expiration(self):
  d=self.declaration();d['variants']=['cleanup_once_0','cleanup_once_1']
  for c in self.cases(d):
   steps=c['steps'];cut=steps.index(dict(op='cleanup'))
   self.assertIn(dict(op='assert_card',ref='victim',property='damage',value=2),steps[:cut])
   self.assertIn(dict(op='assert_card',ref='victim',property='damage',value=0),steps[cut+1:])

if __name__=='__main__':unittest.main()
