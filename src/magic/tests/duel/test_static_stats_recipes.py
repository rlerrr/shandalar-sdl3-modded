"""Guard complete continuous clauses, overlapping predicates and death receipts."""
import copy,json,unittest
from pathlib import Path
from generate_cases import registry
from recipes import explicit_spec,expand
from static_stats_recipes import CORE,matches,variants


class StaticStatsRecipesTest(unittest.TestCase):
 @classmethod
 def setUpClass(cls):
  cls.o=json.loads((Path(__file__).parent/'data/oracle.json').read_text(encoding='utf8'))['cards'];cls.rows={r['name']:r for r in registry()}

 def declaration(self,name='Glorious Anthem',selected=None):
  def clause(scope='own',other=False,state='all',predicate='all',value='',power=1,toughness=1):return dict(scope=scope,other=other,state=state,predicate=predicate,value=value,power=power,toughness=toughness)
  configs={
   'Glorious Anthem':[clause()],
   "Night of Souls' Betrayal":[clause(scope='both',power=-1,toughness=-1)],
   'Elesh Norn, Grand Cenobite':[clause(other=True,power=2,toughness=2),clause(scope='opponents',power=-2,toughness=-2)],
   'Aven Brigadier':[clause(scope='both',other=True,predicate='subtypes',value=['Bird']),clause(scope='both',other=True,predicate='subtypes',value=['Soldier'])],
   'Boartusk Liege':[clause(other=True,predicate='color',value='red'),clause(other=True,predicate='color',value='green')],
   "Builder's Blessing":[clause(state='untapped',power=0,toughness=2)],
   'Favorable Winds':[clause(predicate='flying')],
   'Flowstone Surge':[clause(power=1,toughness=-1)],
   'Tide Drifter':[clause(other=True,predicate='colorless',power=0,toughness=1)],
   'Day of Destiny':[clause(predicate='legendary',power=2,toughness=2)],
   'Serra Aviary':[clause(scope='both',predicate='flying')],
  }
  p=dict(clauses=configs[name],keywords=sorted(self.o[name]['keywords']),activated_abilities=[])
  if name=='Boartusk Liege':p['payment_colors']=['R']*3
  return dict(family='static_stats',version=2,params=p,oracle_text=self.o[name]['oracle_text'],variants=selected or ['static_0'])

 def cases(self,name='Glorious Anthem',selected=None,d=None,oracle=None):
  return expand(name,self.rows[name]['csvid'],d or self.declaration(name,selected),self.o[name],self.rows,oracle_context=oracle or self.o)

 def test_whole_effect_and_signed_bounds_fail_closed(self):
  for key,value in [('scope','both'),('other',True),('power',-1),('state','tapped'),('predicate','artifact')]:
   d=self.declaration();d['params']['clauses'][0][key]=value
   with self.assertRaises(ValueError):self.cases(d=d)
  for patch in (dict(power=True),dict(toughness=-4),dict(power=0,toughness=0),dict(value=['Human'])):
   d=self.declaration();d['params']['clauses'][0].update(patch)
   with self.assertRaises(ValueError):self.cases(d=d)
  for patch in (dict(oracle_text='Creatures you control get +2/+2.'),dict(variants=[]),dict(variants=['unknown_0'])):
   with self.assertRaises(ValueError):self.cases(d=dict(self.declaration(),**patch))

 def test_changeling_receives_each_distinct_tribal_bonus(self):
  s=self.cases('Aven Brigadier')[0]['steps'];ref=f'army_0_{CORE.index("Avian Changeling")}'
  self.assertIn(dict(op='assert_card',ref=ref,property='power',value=4),s)
  self.assertIn(dict(op='assert_card',ref=ref,property='toughness',value=4),s)
  self.assertIn(dict(op='assert_card',ref='source',property='power',value=3),s)

 def test_overlapping_colors_add_twice_without_matching_color_identity(self):
  s=self.cases('Boartusk Liege')[0]['steps'];ref=f'army_0_{CORE.index("Woolly Thoctar")}'
  self.assertIn(dict(op='assert_card',ref=ref,property='power',value=7),s)
  self.assertIn(dict(op='assert_card',ref=ref,property='toughness',value=6),s)
  q=self.declaration('Boartusk Liege')['params']['clauses'][0]
  self.assertFalse(matches(q,self.o['Mothdust Changeling'],0,0,'candidate','source'))

 def test_global_negative_stats_kill_creatures_but_not_noncreatures(self):
  s=self.cases("Night of Souls' Betrayal")[0]['steps']
  self.assertIn(dict(op='assert_zone_multiplicity',player=0,zone='battlefield',index=self.rows['Savannah Lions']['csvid'],value=0),s)
  self.assertIn(dict(op='assert_zone_multiplicity',player=0,zone='graveyard',index=self.rows['Savannah Lions']['csvid'],value=1),s)
  self.assertIn(dict(op='assert_card',ref='artifact_0',property='in_play',value=1),s)

 def test_token_copy_dies_without_leaving_a_card_in_the_graveyard(self):
  s=self.cases('Elesh Norn, Grand Cenobite')[0]['steps']
  self.assertIn(dict(op='assert_zone_multiplicity',player=1,zone='battlefield',index=self.rows['Grizzly Bears']['csvid'],value=0),s)
  # One pre-existing grave card and the real creature; the token vanishes.
  self.assertIn(dict(op='assert_zone_multiplicity',player=1,zone='graveyard',index=self.rows['Grizzly Bears']['csvid'],value=2),s)

 def test_real_damage_precedes_source_bounce_and_state_based_death(self):
  c=self.cases(selected=['damage_then_bounce_0'])[0];s=c['steps']
  self.assertTrue(any(x['name'] in ('Shock','Lightning Bolt','Flame Slash') for x in c['setup']))
  first=next(i for i,x in enumerate(s) if x['op']=='cast' and x['ref']=='damage');second=next(i for i,x in enumerate(s) if x['op']=='cast' and x['ref']=='bounce')
  self.assertLess(first,second);self.assertTrue(any(x['op']=='assert_card' and x['property']=='damage' and x['value']>0 for x in s[first:second]))

 def test_tapping_predicate_changes_after_actual_twiddle(self):
  c=self.cases("Builder's Blessing",['change_tapping_1'])[0]
  self.assertTrue(any(x['name']=='Twiddle' for x in c['setup']))
  ref='army_1_0';s=c['steps'];self.assertIn(dict(op='assert_card',ref=ref,property='toughness',value=3),s);self.assertIn(dict(op='assert_card',ref=ref,property='toughness',value=1),s)

 def test_late_arrival_chooses_an_affected_creature_when_available(self):
  c=self.cases('Favorable Winds',['late_arrival_0'])[0]
  n=next(x['name'] for x in c['setup'] if x['ref']=='late')
  self.assertIn('Flying',self.o[n]['keywords'])
  self.assertIn(dict(op='cast',ref='late',value=1),c['steps'])
  for name in ('Aven Brigadier',):
   c=self.cases(name,['late_arrival_0'])[0];n=next(x['name'] for x in c['setup'] if x['ref']=='late')
   self.assertNotIn('Bestow',self.o[n]['keywords'])
  c=self.cases('Day of Destiny',['late_arrival_0'])[0];late=next(x for x in c['setup'] if x['ref']=='late')
  self.assertIn('Legendary',self.o[late['name']]['type_line'])
  self.assertFalse(any(x['ref']!='late' and x['player']==late['player'] and x['zone']=='battlefield' and x['name']==late['name'] for x in c['setup']))

 def test_combat_only_predicates_end_when_creature_leaves_combat(self):
  s=self.cases(selected=['attackers_0'])[0]['steps']
  remove=next(i for i,x in enumerate(s) if x['op']=='remove_from_combat');cleanup=next(i for i,x in enumerate(s) if x['op']=='cleanup')
  self.assertLess(remove,cleanup)

 def test_hybrid_payments_are_explicit_and_other_allocations_use_exact_pools(self):
  s=self.cases('Boartusk Liege',['cast_0'])[0]['steps'];self.assertTrue(any(x['op']=='choose_payment' for x in s))
  c=self.cases('Boartusk Liege',['hybrid_other_1'])[0];pools={x['color']:x['value'] for x in c['steps'] if x['op']=='set_mana' and x['player']==1}
  self.assertEqual(pools,{0:1,1:0,2:0,3:3,4:0,5:0})
  self.assertIn(dict(op='assert_mana',player=1,color=3,value=0),c['steps'])

 def test_pending_snapshot_captures_before_continuous_effect_begins(self):
  s=self.cases(selected=['pending_snapshot_0'])[0]['steps'];capture=next(i for i,x in enumerate(s) if x['op']=='capture');cast=next(i for i,x in enumerate(s) if x['op']=='cast')
  self.assertLess(cast,capture);self.assertEqual(s[capture+1],dict(op='resolve'))
  for op in ('record_state','ai_roundtrip','restore','assert_recorded_state'):self.assertTrue(any(x['op']==op for x in s))

 def test_world_and_legendary_sources_obey_copy_fixture_limits(self):
  for name in ("Night of Souls' Betrayal",'Elesh Norn, Grand Cenobite'):
   p=explicit_spec(self.declaration(name),self.o[name]);self.assertNotIn('two_sources_0',variants(p));self.assertIn('opposing_source_0',variants(p))
  p=explicit_spec(self.declaration('Serra Aviary'),self.o['Serra Aviary'])
  self.assertNotIn('two_sources_0',variants(p));self.assertNotIn('opposing_source_0',variants(p))

 def test_handler_flags_do_not_determine_oracle_expectations(self):
  d=self.declaration();saved=copy.deepcopy(d);expected=self.cases(d=d);self.assertEqual(d,saved)
  rows=copy.deepcopy(self.rows);rows['Glorious Anthem']['handler']='unrelated';rows['Glorious Anthem']['mana_cost']='{19}'
  self.assertEqual(expected,expand('Glorious Anthem',self.rows['Glorious Anthem']['csvid'],d,self.o['Glorious Anthem'],rows,oracle_context=self.o))
  o=copy.deepcopy(self.o);o['Avian Changeling']['keywords']=[]
  with self.assertRaises(ValueError):self.cases(oracle=o)


if __name__=='__main__':unittest.main()
