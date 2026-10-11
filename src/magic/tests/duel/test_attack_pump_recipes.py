import copy,json,unittest
from pathlib import Path
from generate_cases import registry
from recipes import explicit_spec,expand
from attack_pump_recipes import variants,validate_witnesses

class AttackPumpRecipesTest(unittest.TestCase):
 @classmethod
 def setUpClass(cls):cls.oracle=json.loads((Path(__file__).parent/'data/oracle.json').read_text(encoding='utf8'))['cards'];cls.rows={r['name']:r for r in registry()}
 def declaration(self,name='Benalish Veteran',selected=None,**p):
  params=dict(power=1,toughness=1,keywords=[]);params.update(p);return dict(family='attack_pump_creature',version=2,params=params,oracle_text=self.oracle[name]['oracle_text'],variants=selected or ['attack_0'])
 def cases(self,name='Benalish Veteran',selected=None,**p):return expand(name,self.rows[name]['csvid'],self.declaration(name,selected,**p),self.oracle[name],self.rows,oracle_context=self.oracle)
 def has(self,s,**kw):return any(all(a.get(k)==v for k,v in kw.items()) for a in s)
 def test_explicit_parameters(self):
  for p in ({},dict(self.declaration()['params'],extra=1)):
   with self.assertRaises(ValueError):explicit_spec(dict(self.declaration(),params=p),self.oracle['Benalish Veteran'])
 def test_exact_bonus(self):
  for p in (dict(power=2),dict(toughness=0),dict(power=0,toughness=0)):
   with self.assertRaises(ValueError):self.cases(**p)
 def test_types_and_bounds(self):
  for p in (dict(power=True),dict(power=6),dict(toughness=-1),dict(toughness=4),dict(power=1.0)):
   with self.assertRaises(ValueError):self.cases(**p)
 def test_full_text_pin(self):
  with self.assertRaises(ValueError):explicit_spec(dict(self.declaration(),oracle_text='changed'),self.oracle['Benalish Veteran'])
 def test_keywords_pin(self):
  with self.assertRaises(ValueError):self.cases(keywords=['Flying'])
 def test_variants_explicit(self):
  d=self.declaration();del d['variants']
  with self.assertRaises(ValueError):explicit_spec(d,self.oracle['Benalish Veteran'])
 def test_invalid_variant(self):
  with self.assertRaises(ValueError):self.cases(selected=['made_up_0'])
 def test_non_normal_and_nonnumeric(self):
  for p in (dict(layout='transform'),dict(layout='token'),dict(card_faces=[{}]),dict(selected_face='front'),dict(power='*'),dict(toughness='0')):
   with self.assertRaises(ValueError):explicit_spec(self.declaration(),dict(self.oracle['Benalish Veteran'],**p))
 def test_additional_trigger_rejected(self):
  r=dict(self.oracle['Benalish Veteran']);r['oracle_text']+='\nWhen this creature dies, draw a card.'
  with self.assertRaises(ValueError):explicit_spec(dict(self.declaration(),oracle_text=r['oracle_text']),r)
 def test_conditional_trigger_rejected(self):
  r=dict(self.oracle['Benalish Veteran']);r['oracle_text']=r['oracle_text'].replace('attacks,','attacks alone,')
  with self.assertRaises(ValueError):explicit_spec(dict(self.declaration(),oracle_text=r['oracle_text']),r)
 def test_duplicate_trigger_rejected(self):
  r=dict(self.oracle['Benalish Veteran']);r['oracle_text']+='\n'+r['oracle_text']
  with self.assertRaises(ValueError):explicit_spec(dict(self.declaration(),oracle_text=r['oracle_text']),r)
 def test_witness_refresh(self):
  for n in ('Hill Giant','Grizzly Bears','Ornithopter','Unsummon'):
   o=copy.deepcopy(self.oracle);o[n]['oracle_text']='changed'
   with self.assertRaises(ValueError):validate_witnesses(o,self.rows)
 def test_both_controllers(self):
  p=explicit_spec(self.declaration(),self.oracle['Benalish Veteran']);self.assertIn('attack_0',variants(p));self.assertIn('attack_1',variants(p))
 def test_group_declaration_timing(self):
  s=self.cases()[0]['steps'];j=next(i for i,a in enumerate(s) if a['op']=='finish_attackers');self.assertTrue(self.has(s[:j],op='assert_card',ref='source',property='power',value=2));self.assertFalse(self.has(s[:j],ref='source',property='power',value=3));self.assertTrue(self.has(s[j:],ref='source',property='power',value=3));self.assertFalse(self.has(s,op='dispatch_event'))
 def test_pair_buffs_only_source(self):
  s=self.cases(selected=['attack_pair_1'])[0]['steps'];j=next(i for i,a in enumerate(s) if a['op']=='finish_attackers');self.assertTrue(self.has(s[j:],ref='source',property='power',value=3));self.assertTrue(self.has(s[j:],ref='idle',property='power',value=3));self.assertFalse(self.has(s[j:],ref='idle',property='power',value=4))
 def test_other_attack_does_not_buff(self):
  s=self.cases(selected=['other_attack_0'])[0]['steps'];self.assertFalse(self.has(s,ref='source',property='power',value=3))
 def test_blocking_does_not_trigger(self):
  s=self.cases(selected=['defend_1'])[0]['steps'];self.assertTrue(self.has(s,op='block',ref='source',other='enemy',value=1));self.assertFalse(self.has(s,ref='source',property='power',value=3))
 def test_tapped_and_sick_attack_rejected(self):
  for v in ('tapped_0','sick_1'):
   s=self.cases(selected=[v])[0]['steps'];self.assertTrue(self.has(s,op='attack',ref='source',value=0));self.assertFalse(self.has(s,ref='source',property='power',value=3))
 def test_fresh_cast_uses_real_attack_gate(self):
  s=self.cases(selected=['fresh_attack_0'])[0]['steps'];self.assertTrue(self.has(s,op='cast',ref='source',value=1));self.assertTrue(self.has(s,op='attack',ref='source',value=0))
 def test_exact_payment(self):
  s=self.cases(selected=['payment_1'])[0]['steps'];self.assertTrue(self.has(s,op='set_mana',player=1,color=0,value=2));self.assertTrue(self.has(s,op='assert_mana',player=1,color=5,value=0))
 def test_insufficient_mana(self):
  s=self.cases(selected=['insufficient_mana_0'])[0]['steps'];self.assertTrue(self.has(s,op='cast',value=0));self.assertFalse(self.has(s,op='resolve'))
 def test_populated_owned_zones(self):
  c=self.cases()[0]
  for player in (0,1):
   for z in ('hand','graveyard','exile','library'):self.assertTrue(any(a['zone']==z and a['player']==player for a in c['setup']))
 def test_bare_zones_explicit(self):
  c=self.cases(selected=['bare_zones_1'])[0];self.assertFalse(any(a['zone']!='battlefield' for a in c['setup']))
 def test_blocked_damage_and_trample(self):
  s=self.cases('Hungry Spriggan',['blocked_0'],power=3,toughness=3,keywords=['Trample'])[0]['steps'];self.assertTrue(self.has(s,op='assert_life',player=1,value=18));self.assertTrue(self.has(s,op='assert_zone_contains',player=1,zone='graveyard',value=self.rows['Ornithopter']['csvid']))
 def test_unblocked_damage_uses_bonus(self):
  s=self.cases(selected=['unblocked_1'])[0]['steps'];self.assertTrue(self.has(s,op='assert_life',player=0,value=17));self.assertTrue(self.has(s,op='end_combat'))
 def test_cleanup_restores_printed_stats(self):
  s=self.cases(selected=['cleanup_0'])[0]['steps'];j=next(i for i,a in enumerate(s) if a['op']=='cleanup');self.assertTrue(self.has(s[:j],ref='source',property='power',value=3));self.assertTrue(self.has(s[j:],ref='source',property='power',value=2))
 def test_removed_from_combat_keeps_bonus(self):
  s=self.cases(selected=['remove_from_combat_0'])[0]['steps'];j=next(i for i,a in enumerate(s) if a['op']=='remove_from_combat');self.assertTrue(self.has(s[j:],ref='source',property='power',value=3));self.assertTrue(self.has(s,op='assert_life',player=1,value=20))
 def test_source_bounce_returns_owned_card(self):
  s=self.cases(selected=['source_bounced_after_1'])[0]['steps'];self.assertTrue(self.has(s,op='bind_hand',player=1,value=self.rows['Benalish Veteran']['csvid']));self.assertTrue(self.has(s,ref='source',property='in_hand',value=1));self.assertTrue(self.has(s,op='assert_life',player=0,value=20))
 def test_declaration_snapshot_replays_trigger(self):
  s=self.cases(selected=['declaration_snapshot_0'])[0]['steps'];ops=[a['op'] for a in s];self.assertLess(ops.index('attack'),ops.index('capture'));self.assertLess(ops.index('capture'),ops.index('finish_attackers'));self.assertEqual(ops.count('attack'),1);self.assertEqual(ops.count('finish_attackers'),2);self.assertIn('ai_roundtrip',ops)
 def test_two_sources_declared_together(self):
  s=self.cases(selected=['two_sources_0'])[0]['steps'];j=next(i for i,a in enumerate(s) if a['op']=='finish_attackers');self.assertTrue(self.has(s[:j],op='attack',ref='copy',value=1));self.assertTrue(self.has(s[j:],ref='copy',property='power',value=3))
 def test_cpp_observation_values_are_numeric(self):
  p=explicit_spec(self.declaration(),self.oracle['Benalish Veteran'])
  for case in self.cases(selected=variants(p)):
   for action in case['steps']:
    if 'value' in action:self.assertIs(type(action['value']),int,(case['name'],action))
 def test_legendary_sources_excluded(self):
  p=explicit_spec(self.declaration(),dict(self.oracle['Benalish Veteran'],type_line='Legendary Creature — Human Soldier'));self.assertNotIn('two_sources_0',variants(p))

if __name__=='__main__':unittest.main()
