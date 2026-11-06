"""Fail-closed continuous grant profiles and actual functional recipient checks."""
import copy,json,unittest
from pathlib import Path
from generate_cases import registry
import tribal_static_extension_recipes as t


class TribalStaticExtensionTests(unittest.TestCase):
 @classmethod
 def setUpClass(cls):
  cls.o=json.loads((Path(__file__).parent/'data/oracle.json').read_text(encoding='utf8'))['cards'];cls.rows={r['name']:r for r in registry()}

 def declaration(self,n='Galerider Sliver'):
  return dict(family=t.FAMILY,version=2,oracle_text=self.o[n]['oracle_text'],params=copy.deepcopy(t.REVIEWS['sources'][n]['params']),variants=['effect_0'])

 def cases(self,n,selected):
  d=self.declaration(n);p=t.spec(d,self.o[n]);return t.expand(n,self.rows[n]['csvid'],d,p,selected,self.rows,self.o)

 def test_whole_source_characteristics_and_parameters_are_pinned(self):
  d=self.declaration();r=self.o['Galerider Sliver']
  for changed in (dict(oracle_text='changed'),dict(power='2'),dict(colors=['B']),dict(type_line='Creature — Bird'),dict(mana_cost='{W}'),dict(keywords=['Flying']),dict(layout='transform')):
   with self.assertRaises(ValueError):t.spec(d,dict(r,**changed))
  changed=copy.deepcopy(d);changed['params']['clauses'][0]['scope']='both'
  with self.assertRaises(ValueError):t.spec(changed,r)
  changed_type=copy.deepcopy(d);changed_type['params']['clauses'][0]['power']=False
  with self.assertRaises(ValueError):t.spec(changed_type,r)
  for changed in (dict(params={}),dict(version=1),dict(version=2.0)):
   with self.assertRaises(ValueError):t.spec(dict(d,**changed),r)

 def test_variants_fail_closed(self):
  for selected in ([],['effect_0','effect_0'],['unknown_0']):
   with self.assertRaises(ValueError):self.cases('Galerider Sliver',selected)

 def test_witness_rules_costs_and_types_are_pinned(self):
  t.validate_witnesses(self.o,self.rows)
  for n,k,v in [('Sliver Construct','power','9'),('Mothdust Changeling','oracle_text',''),('Giant Growth','mana_cost','{U}'),('Grizzly Bears','type_line','Creature — Sliver')]:
   o=copy.deepcopy(self.o);o[n][k]=v
   with self.assertRaises(ValueError):t.validate_witnesses(o,self.rows)

 def test_flying_matching_nonmatching_and_opponent_controls(self):
  for v,legal in [('effect_0',0),('nonmatching_1',1),('opponent_recipient_0',1),('changeling_1',0),('source_bounce_0',1)]:
   s=self.cases('Galerider Sliver',[v])[0]['steps'];self.assertIn(dict(op='block',ref='blocker',other='nonmatch_1' if v=='nonmatching_1' else 'match_1' if v=='opponent_recipient_0' else 'changeling_1' if v=='changeling_1' else 'match_0',value=legal),s)

 def test_first_strike_checks_zero_reciprocal_damage(self):
  s=self.cases('Striking Sliver',['effect_0'])[0]['steps']
  self.assertIn(dict(op='combat_damage'),s);self.assertIn(dict(op='assert_card',ref='match_0',property='damage',value=0),s)
  self.assertIn(dict(op='assert_zone_multiplicity',player=1,zone='graveyard',index=self.rows['Savannah Lions']['csvid'],value=1),s)

 def test_lifelink_double_strike_vigilance_and_haste_have_real_combat(self):
  for n in ('Syphon Sliver','Bonescythe Sliver','Sentinel Sliver','Blur Sliver'):
   s=self.cases(n,['effect_1'])[0]['steps'];self.assertIn(dict(op='combat_damage'),s)
  self.assertIn(dict(op='assert_life',player=1,value=22),self.cases('Syphon Sliver',['effect_1'])[0]['steps'])
  self.assertIn(dict(op='assert_life',player=0,value=16),self.cases('Bonescythe Sliver',['effect_1'])[0]['steps'])
  self.assertIn(dict(op='assert_card',ref='match_1',property='tapped',value=0),self.cases('Sentinel Sliver',['effect_1'])[0]['steps'])
  s=self.cases('Blur Sliver',['effect_1'])[0]['steps'];self.assertIn(dict(op='bind_hand_copy',player=1,value=self.rows['Sliver Construct']['csvid'],index=0,ref='match_1'),s)

 def test_fresh_nonmatching_witness_rebinds_copy_in_populated_hand(self):
  for owner in (0,1):
   s=self.cases('Blur Sliver',['nonmatching_'+str(owner)])[0]['steps']
   self.assertIn(dict(op='bind_hand_copy',player=owner,value=self.rows['Grizzly Bears']['csvid'],index=1,ref='nonmatch_'+str(owner)),s)
   self.assertIn(dict(op='attack',ref='nonmatch_'+str(owner),value=0),s)

 def test_source_payment_checked_before_independent_witness_pool(self):
  s=self.cases('Galerider Sliver',['payment_1'])[0]['steps'];first=next(i for i,a in enumerate(s) if a['op']=='assert_mana' and a['player']==1)
  self.assertEqual([a['value'] for a in s[first:first+6]],[0]*6)
  self.assertTrue(any(a['op']=='set_mana' and a['player']==1 and a['value']==20 for a in s[first+6:]))

 def test_snapshot_replays_functional_combat(self):
  for c in self.cases('Galerider Sliver',['ready_snapshot_0','pending_snapshot_1','resolved_snapshot_0']):
   ops=[a['op'] for a in c['steps']];self.assertIn('ai_roundtrip',ops);self.assertEqual(ops[-1],'assert_recorded_state')

 def test_source_loss_proves_active_grant_before_restoring_and_removing(self):
  for mode in ('source_bounce_0','source_destroyed_1','remove_one_source_0'):
   c=self.cases('Galerider Sliver',[mode])[0];s=c['steps'];restore=next(i for i,a in enumerate(s) if a['op']=='restore')
   self.assertIn(dict(op='combat_damage'),s[:restore]);self.assertTrue(any(a['op']=='block' and a['value']==0 for a in s[:restore]))
   if not mode.startswith('remove_one'):self.assertTrue(any(a['op']=='block' and a['value']==1 for a in s[restore:]))

 def test_every_enrolled_profile_expands_all_reviewed_variants(self):
  for n in t.REVIEWS['sources']:
   d=self.declaration(n);p=t.spec(d,self.o[n]);cases=t.expand(n,self.rows[n]['csvid'],d,p,t.variants(p),self.rows,self.o)
   self.assertEqual(len(cases),len(t.variants(p)));self.assertEqual(len({c['name'] for c in cases}),len(cases))

 def test_stat_and_keyword_clauses_stack_for_relevant_controllers(self):
  for n,v,ref,expected in [('Field Marshal','effect_0','match_1',3),('Stromkirk Captain','effect_0','match_1',3),('Sunrise Sovereign','two_sources_1','match_1',7)]:
   s=self.cases(n,[v])[0]['steps'];self.assertIn(dict(op='assert_card',ref=ref,property='power',value=expected),s)

 def test_hexproof_and_shroud_have_distinct_owner_permissions(self):
  for n,allowed in [('Drogskol Captain',1),('Lord of the Unreal',1),('Scion of Oona',0)]:
   s=self.cases(n,['owner_targeting_1'])[0]['steps']
   self.assertIn(dict(op='can_target_creature',ref='probe',other='match_1',value=allowed),s)
   self.assertEqual(any(a['op']=='cast' and a['ref']=='probe' for a in s),bool(allowed))

 def test_shroud_source_pair_uses_nontargeted_source_removal(self):
  s=self.cases('Scion of Oona',['remove_one_source_0'])[0]['steps']
  after=s[next(i for i,a in enumerate(s) if a['op']=='restore')+1:]
  self.assertIn(dict(op='destroy',ref='source'),after)
  self.assertFalse(any(a['op']=='choose_target' and a['ref']=='source' for a in after))

 def test_indestructible_nonmatching_probe_uses_different_live_control(self):
  for n in ('Knight Exemplar','Sliver Hivelord'):
   s=self.cases(n,['nonmatching_1'])[0]['steps']
   self.assertIn(dict(op='choose_target',player=0,ref='nonmatch_1'),s)
   self.assertIn(dict(op='choose_target',player=0,ref='match_1'),s)
   self.assertIn(dict(op='assert_zone_multiplicity',player=1,zone='graveyard',index=self.rows['Grizzly Bears']['csvid'],value=2),s)

 def test_profile_recipients_are_normal_numeric_creatures(self):
  for review in t.REVIEWS['sources'].values():
   for key in ('matching','nonmatching'):
    r=self.o[review['params'][key]]
    self.assertEqual(r['layout'],'normal');self.assertIn('Creature',r['type_line'])
    self.assertTrue(r['power'].isdigit() and r['toughness'].isdigit())


if __name__=='__main__':unittest.main()
