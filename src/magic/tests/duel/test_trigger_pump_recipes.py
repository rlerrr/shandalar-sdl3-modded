import copy, json, unittest
from pathlib import Path
from generate_cases import registry
from recipes import expand, explicit_spec
from trigger_pump_recipes import variants, validate_witnesses

class TriggerPumpRecipesTest(unittest.TestCase):
 @classmethod
 def setUpClass(cls):
  cls.oracle=json.loads((Path(__file__).parent/'data/oracle.json').read_text(encoding='utf8'))['cards'];cls.rows={r['name']:r for r in registry()}
 def declaration(self, name='Bloodfire Expert', selected=None, **params):
  p=dict(trigger='prowess',power=1,toughness=1,keywords=[])
  if name=='Plated Geopede':p.update(trigger='landfall',power=2,toughness=2,keywords=['First strike'])
  p.update(params)
  return dict(family='trigger_pump_creature',version=2,params=p,oracle_text=self.oracle[name]['oracle_text'],variants=selected or ['effect_0'])
 def cases(self,name='Bloodfire Expert',selected=None,**params):return expand(name,self.rows[name]['csvid'],self.declaration(name,selected,**params),self.oracle[name],self.rows,oracle_context=self.oracle)
 def has(self,seq,**kw):return any(all(a.get(k)==v for k,v in kw.items()) for a in seq)
 def test_explicit_parameters(self):
  for p in ({},dict(self.declaration()['params'],extra=True)):
   with self.assertRaises(ValueError):explicit_spec(dict(self.declaration(),params=p),self.oracle['Bloodfire Expert'])
 def test_exact_prowess_bonus(self):
  for p in (dict(power=2),dict(toughness=2),dict(trigger='landfall')):
   with self.assertRaises(ValueError):self.cases(**p)
 def test_exact_landfall_bonus(self):
  with self.assertRaises(ValueError):self.cases('Plated Geopede',power=1)
 def test_integer_bounds(self):
  for p in (dict(power=True),dict(power=1.0),dict(power=0),dict(toughness=3),dict(trigger='cast')):
   with self.assertRaises(ValueError):self.cases(**p)
 def test_whole_text_pin(self):
  with self.assertRaises(ValueError):explicit_spec(dict(self.declaration(),oracle_text='changed'),self.oracle['Bloodfire Expert'])
 def test_extra_rules_rejected(self):
  r=dict(self.oracle['Bloodfire Expert']);r['oracle_text']+='\nWhen this creature dies, draw a card.'
  with self.assertRaises(ValueError):explicit_spec(dict(self.declaration(),oracle_text=r['oracle_text']),r)
 def test_duplicate_trigger_rejected(self):
  r=dict(self.oracle['Bloodfire Expert']);r['oracle_text']+='\n'+r['oracle_text']
  with self.assertRaises(ValueError):explicit_spec(dict(self.declaration(),oracle_text=r['oracle_text']),r)
 def test_conditional_landfall_rejected(self):
  r=dict(self.oracle['Plated Geopede']);r['oracle_text']=r['oracle_text'].replace('enters,','enters, if you control another Insect,')
  with self.assertRaises(ValueError):explicit_spec(dict(self.declaration('Plated Geopede'),oracle_text=r['oracle_text']),r)
 def test_keywords_pin(self):
  with self.assertRaises(ValueError):self.cases(keywords=['Flying'])
 def test_variants_explicit(self):
  d=self.declaration();del d['variants']
  with self.assertRaises(ValueError):explicit_spec(d,self.oracle['Bloodfire Expert'])
 def test_invalid_variant(self):
  with self.assertRaises(ValueError):self.cases(selected=['made_up_0'])
 def test_single_face_numeric_creature(self):
  for fields in (dict(layout='transform'),dict(card_faces=[{}]),dict(power='*'),dict(toughness='0')):
   with self.assertRaises(ValueError):explicit_spec(self.declaration(),dict(self.oracle['Bloodfire Expert'],**fields))
 def test_witness_pins(self):
  for name in ('Darksteel Relic','Rampant Growth',"Nature's Lore",'Giant Growth','Ornithopter'):
   o=copy.deepcopy(self.oracle);o[name]['oracle_text']='changed'
   with self.assertRaises(ValueError):validate_witnesses(o,self.rows)
 def test_both_controllers(self):
  p=explicit_spec(self.declaration(),self.oracle['Bloodfire Expert']);self.assertIn('effect_0',variants(p));self.assertIn('effect_1',variants(p))
 def test_real_spells(self):
  for name in ('Bloodfire Expert','Plated Geopede'):
   c=self.cases(name)[0];self.assertTrue(self.has(c['steps'],op='cast',ref='spell_0',value=1));self.assertFalse(self.has(c['steps'],op='dispatch_event'))
 def test_landfall_search_is_scripted(self):
  s=self.cases('Plated Geopede')[0]['steps'];self.assertTrue(self.has(s,op='choose_zone_card',value=self.rows['Forest']['csvid']));self.assertTrue(self.has(s,ref='source',property='power',value=3))
 def test_repeated_triggers_stack(self):
  s=self.cases(selected=['three_triggers_1'])[0]['steps'];self.assertEqual(sum(a['op']=='cast' for a in s),3);self.assertTrue(self.has(s,ref='source',property='power',value=6))
 def test_opponent_and_creature_do_not_trigger(self):
  for v in ('opponent_trigger_1','creature_spell_0'):
   s=self.cases(selected=[v])[0]['steps'];self.assertFalse(self.has(s,ref='source',property='power',value=4))
 def test_nonland_and_failed_search_do_not_trigger(self):
  for v in ('nonland_spell_0','failed_search_1'):
   s=self.cases('Plated Geopede',[v])[0]['steps'];self.assertFalse(self.has(s,ref='source',property='power',value=3))
 def test_late_arrival_not_retroactive(self):
  s=self.cases(selected=['source_arrives_late_0'])[0]['steps'];casts=[a['ref'] for a in s if a['op']=='cast'];self.assertEqual(casts,['spell_0','source']);self.assertFalse(self.has(s,ref='source',property='power',value=4))
 def test_two_sources_receive_bonus(self):
  s=self.cases(selected=['two_sources_0'])[0]['steps'];self.assertTrue(self.has(s,ref='copy',property='power',value=4));self.assertFalse(self.has(s,ref='idle',property='power',value=4))
 def test_combat_damage_uses_bonus(self):
  s=self.cases(selected=['unblocked_1'])[0]['steps'];self.assertTrue(self.has(s,op='combat_damage'));self.assertTrue(self.has(s,op='assert_life',player=0,value=16))
 def test_cleanup(self):
  s=self.cases(selected=['cleanup_0'])[0]['steps'];j=next(i for i,a in enumerate(s) if a['op']=='cleanup');self.assertTrue(self.has(s[:j],ref='source',property='power',value=4));self.assertTrue(self.has(s[j:],ref='source',property='power',value=3))
 def test_pending_snapshot_replays_resolution(self):
  ops=[a['op'] for a in self.cases('Plated Geopede',['pending_snapshot_0'])[0]['steps']];self.assertLess(ops.index('cast'),ops.index('capture'));self.assertLess(ops.index('capture'),ops.index('choose_zone_card'));self.assertEqual(ops.count('cast'),1);self.assertEqual(ops.count('resolve'),2);self.assertIn('ai_roundtrip',ops)
 def test_structured_trigger_keyword_required(self):
  with self.assertRaises(ValueError):explicit_spec(self.declaration(),dict(self.oracle['Bloodfire Expert'],keywords=[]))
 def test_repeated_search_names_cards_after_shuffles(self):
  s=self.cases('Plated Geopede',['three_triggers_0'])[0]['steps'];self.assertEqual([a['value'] for a in s if a['op']=='choose_zone_card'],[self.rows[n]['csvid'] for n in ('Forest','Plains','Forest')]);self.assertFalse(self.has(s,op='choose_zone',value=0))
 def test_owned_zones_and_bare_variant(self):
  c=self.cases()[0]
  for player in (0,1):
   for zone in ('hand','graveyard','exile','library'):self.assertTrue(any(x['player']==player and x['zone']==zone for x in c['setup']))
  c=self.cases(selected=['bare_zones_0'])[0];self.assertFalse(any(x['zone'] in ('graveyard','exile') for x in c['setup']))
 def test_numeric_cpp_values(self):
  for name in ('Bloodfire Expert','Plated Geopede'):
   p=explicit_spec(self.declaration(name),self.oracle[name])
   for c in self.cases(name,variants(p)):
    for action in c['steps']:
     if 'value' in action:self.assertIs(type(action['value']),int)

if __name__=='__main__':unittest.main()
