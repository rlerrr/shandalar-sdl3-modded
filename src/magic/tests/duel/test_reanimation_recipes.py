import copy,json,sys,unittest
from pathlib import Path

HERE=Path(__file__).resolve().parent
ROOT=next(p for p in HERE.parents if (p/'src/magic/tests/duel/data/oracle.json').exists())
sys.path.insert(0,str(ROOT/'src/magic/tests/duel'))
sys.path.insert(0,str(HERE))
import reanimation_recipes as f
from generate_cases import registry

class ReanimationRecipesTest(unittest.TestCase):
 @classmethod
 def setUpClass(cls):
  cls.o=json.loads((ROOT/'src/magic/tests/duel/data/oracle.json').read_text(encoding='utf8'))['cards'];cls.rows={r['name']:r for r in registry()}
  homes=HERE/'cards' if (HERE/'cards').is_dir() else HERE/'data/cases/cards'
  cls.docs={d['card']:d for d in (json.loads(p.read_text(encoding='utf8')) for p in homes.rglob('*.json')) if any(r.get('family')=='reanimation' for r in d.get('recipes',[]))}
 def declaration(self,n='Hymn of Rebirth',v='effect_0'):
  d=copy.deepcopy(self.docs[n]['recipes'][0]);d['variants']=[v];return d
 def cases(self,n='Hymn of Rebirth',v='effect_0',d=None):
  d=d or self.declaration(n,v);p=f.spec(d,self.o[n]);return f.expand(n,self.rows[n]['csvid'],d,p,d['variants'],self.rows,self.o)
 def test_full_pin_and_normal_source_required(self):
  d=self.declaration();d['oracle_text']='different'
  with self.assertRaises(ValueError):self.cases(d=d)
  for k,val in [('layout','split'),('card_faces',[{}]),('selected_face',{}),('type_line','Creature')]:
   r=dict(self.o['Hymn of Rebirth'],**{k:val})
   if k=='selected_face':r[k]={'name':'face'}
   with self.assertRaises(ValueError):f.spec(self.declaration(),r)
 def test_unknown_missing_or_wrong_parameters_fail_closed(self):
  for key,val in [('scope','card'),('graveyard','all'),('control','owner'),('tapped',1),('history',1),('scope',[]),('limit',True),('extras',['unknown']),('self_destination','exile'),('legacy_x_payment_prompt',1),('legacy_x_payment_prompt',True)]:
   d=self.declaration();d['params'][key]=val
   with self.assertRaises(ValueError):self.cases(d=d)
  d=self.declaration();d['params']['extra']=True
  with self.assertRaises(ValueError):self.cases(d=d)
  d=self.declaration();del d['params']['tapped']
  with self.assertRaises(ValueError):self.cases(d=d)
 def test_empty_duplicate_or_unknown_variants_rejected(self):
  for values in ([],['bad'],['effect_0','effect_0'],[{}]):
   d=self.declaration();d['variants']=values
   with self.assertRaises(ValueError):self.cases(d=d)
 def test_changed_extra_clause_not_accepted_as_gap(self):
  d=self.declaration('Rise from the Grave');d['params']['extras']=[]
  with self.assertRaises(ValueError):self.cases('Rise from the Grave',d=d)
  r=copy.deepcopy(self.o['Rise from the Grave']);r['oracle_text']+=' Draw a card.';d['oracle_text']=r['oracle_text']
  with self.assertRaises(ValueError):f.spec(d,r)
 def test_predicate_pin_distinguishes_type_union_and_intersection(self):
  p=self.declaration('Beacon of Unrest')['params']
  for n in ('Sol Ring','Grizzly Bears','Ornithopter'):self.assertTrue(f.eligible(self.o[n],p))
  self.assertFalse(f.eligible(self.o['Seal of Cleansing'],p))
  p=self.declaration('Zombie Apocalypse')['params'];self.assertTrue(f.eligible(self.o['Scathe Zombies'],p));self.assertFalse(f.eligible(self.o['Eager Cadet'],p))
 def test_basic_land_excludes_nonbasic_artifact_land(self):
  p=self.declaration('Planar Birth')['params'];self.assertTrue(f.eligible(self.o['Forest'],p));self.assertFalse(f.eligible(self.o['Darksteel Citadel'],p))
 def test_x_predicate_has_zero_and_boundary_values(self):
  p=self.declaration('Stir the Grave')['params'];self.assertTrue(f.eligible(self.o['Ornithopter'],p,0));self.assertFalse(f.eligible(self.o['Grizzly Bears'],p,1));self.assertTrue(f.eligible(self.o['Grizzly Bears'],p,2))
  for x in (0,1,2,3,5,8):
   v=f'effect_x{x}_1' if x!=3 else 'effect_1';steps=self.cases('Stir the Grave',v)[0]['steps'];self.assertIn(dict(op='choose_x',player=1,value=x),steps)
 def test_both_controllers_cast_and_exact_payment(self):
  for pl in (0,1):
   c=self.cases(v=f'payment_{pl}')[0];self.assertEqual(next(x['player'] for x in c['setup'] if x['ref']=='source'),pl)
   self.assertTrue(any(s['op']=='cast' and s['ref']=='source' and s['value']==1 for s in c['steps']))
   self.assertTrue(any(s['op']=='assert_mana_total' and s['player']==pl and s['value']==0 for s in c['steps']))
 def test_noop_sensitive_success_and_empty_rejection(self):
  c=self.cases()[0];self.assertTrue(any(s['op']=='bind_permanent_copy' for s in c['steps']))
  c=self.cases(v='empty_1')[0];self.assertIn(dict(op='cast',ref='source',value=0),c['steps']);self.assertFalse(any(s['op']=='choose_zone_card' for s in c['steps']))
 def test_mass_empty_casts_and_moves_source(self):
  c=self.cases('Replenish','empty_1')[0];self.assertIn(dict(op='cast',ref='source',value=1),c['steps']);self.assertFalse(any(s['op']=='choose_zone_card' for s in c['steps']))
 def test_wrong_owner_has_legal_targets_in_other_graveyard(self):
  c=self.cases('Ashen Powder','wrong_owner_1')[0];self.assertTrue(any(x['zone']=='graveyard' and x['player']==1 for x in c['setup']));self.assertFalse(any(x['zone']=='graveyard' and x['player']==0 for x in c['setup']))
 def test_opponent_reanimation_returns_to_owner_on_death(self):
  c=self.cases('Ashen Powder','death_owner_1')[0];steps=c['steps'];self.assertTrue(any(s['op']=='bind_permanent' and s['player']==1 for s in steps))
  self.assertTrue(any(s['op']=='assert_zone_multiplicity' and s['zone']=='graveyard' and s['player']==0 and s['index']==self.rows['Grizzly Bears']['csvid'] and s['value']==1 for s in steps))
 def test_tapped_entry_is_oracle_derived(self):
  for name in ('Planar Birth','Zombie Apocalypse','Splendid Reclamation'):
   c=self.cases(name)[0];self.assertTrue(any(s['op']=='assert_card' and s['property']=='tapped' and s['value']==1 for s in c['steps']))
 def test_actual_provenance_precedes_cast(self):
  c=self.cases('Grim Return')[0];steps=c['steps'];self.assertTrue(any(s['op']=='destroy' for s in steps[:next(i for i,s in enumerate(steps) if s['op']=='cast')]))
  c=self.cases('Grim Return','old_graveyard_0')[0];self.assertIn(dict(op='cast',ref='source',value=0),c['steps'])
 def test_history_initialization_is_opt_in_and_precedes_authored_deaths(self):
  for name in ("Faith's Reward",'Grim Return','Thrilling Encore'):
   for pl in (0,1):
    positive=self.cases(name,f'effect_{pl}')[0]['steps']
    self.assertEqual(positive[0],dict(op='reset_death_history'))
    self.assertGreater(next(i for i,s in enumerate(positive) if s['op']=='destroy'),0)
    self.assertEqual(sum(s['op']=='reset_death_history' for s in positive),1)
    probes=[(i,s['value']) for i,s in enumerate(positive) if s['op']=='can_cast']
    self.assertEqual([value for _,value in probes],[0,1] if name=='Grim Return' else [1,1])
    self.assertLess(probes[0][0],next(i for i,s in enumerate(positive) if s['op']=='destroy'))
    self.assertGreater(probes[1][0],max(i for i,s in enumerate(positive) if s['op']=='destroy'))
    old=self.cases(name,f'old_graveyard_{pl}')[0]['steps']
    self.assertEqual(old[0],dict(op='reset_death_history'))
    self.assertFalse(any(s['op']=='destroy' for s in old))
  self.assertFalse(any(s['op']=='reset_death_history' for s in self.cases()[0]['steps']))
 def test_historical_card_selection_uses_its_single_offered_array(self):
  for pl in (0,1):
   c=self.cases('Grim Return',f'effect_{pl}')[0]
   self.assertFalse(any(s['op']=='choose_player' for s in c['steps']))
   self.assertTrue(all(s['player']==pl for s in c['setup'] if s['ref'].startswith('history_')))
 def test_threshold_counts_graveyard_before_resolving_source_arrives(self):
  for pl in (0,1):
   six=self.cases('Stitch Together',f'threshold_six_{pl}')[0]
   seven=self.cases('Stitch Together',f'threshold_seven_{pl}')[0]
   self.assertFalse(any(s['op']=='bind_permanent_copy' for s in six['steps']))
   self.assertTrue(any(s['op']=='bind_permanent_copy' for s in seven['steps']))
   bear=self.rows['Grizzly Bears']['csvid']
   self.assertIn(dict(op='assert_zone_multiplicity',player=pl,zone='hand',index=bear,value=1),six['steps'])
   self.assertIn(dict(op='assert_zone_multiplicity',player=pl,zone='hand',index=bear,value=0),seven['steps'])
 def test_ordered_x_payment_precedes_grave_selection(self):
  c=self.cases('Stir the Grave','effect_x2_0')[0]
  self.assertEqual([s['value'] for s in c['steps'] if s['op']=='choose_x'],[2])
  self.assertLess(max(i for i,s in enumerate(c['steps']) if s['op']=='choose_x'),next(i for i,s in enumerate(c['steps']) if s['op']=='choose_zone_card'))
  self.assertIn(dict(op='assert_mana_total',player=0,value=57),c['steps'])
  d=self.declaration('Stir the Grave','effect_x2_0');d['params']['legacy_x_payment_prompt']=True
  plain=self.cases('Stir the Grave',d=d)[0]['steps']
  self.assertEqual([s['value'] for s in plain if s['op']=='choose_x'],[2,2])
  self.assertIn(dict(op='assert_mana_total',player=0,value=57),plain)
  exact=self.cases('Stir the Grave','payment_x2_0')[0]['steps']
  self.assertEqual([s['value'] for s in exact if s['op']=='choose_x'],[2])
  self.assertIn(dict(op='choose_zone_card',player=0,value=self.rows['Grizzly Bears']['csvid']),exact)
  self.assertIn(dict(op='assert_mana_total',player=0,value=0),exact)
 def test_variable_negative_type_and_value_boards_are_independent(self):
  c=self.cases('Stir the Grave','wrong_type_0')[0]
  self.assertTrue(all('Creature' not in self.o[s['name']]['type_line'] for s in c['setup'] if s['zone']=='graveyard'))
  c=self.cases('Stir the Grave','wrong_value_0')[0]
  graves=[s['name'] for s in c['setup'] if s['zone']=='graveyard']
  self.assertTrue(graves and all('Creature' in self.o[n]['type_line'] and self.o[n]['cmc']>3 for n in graves))
  self.assertIn(dict(op='choose_x',player=0,value=3),c['steps'])
  self.assertTrue(any(s['op']=='choose_optional_zone' and s['value'] in {self.rows[n]['csvid'] for n in graves} for s in c['steps']))
 def test_irrelevant_populated_zones_and_zero_multiplicities(self):
  c=self.cases()[0]
  for pl in (0,1):
   for zone in ('hand','graveyard','library','exile'):self.assertTrue(any(s['player']==pl and s['zone']==zone for s in c['setup']))
  self.assertTrue(any(s['op']=='assert_zone_multiplicity' and s['value']==0 for s in c['steps']))
 def test_lost_target_is_real_response(self):
  for pl in (0,1):
   c=self.cases(v=f'lost_target_{pl}')[0];self.assertTrue(any(s['name']=='Reclaim' and s['zone']=='hand' for s in c['setup']));self.assertTrue(any(s['op']==('choose_priority' if pl==0 else 'cast_response') and s['ref']=='response' for s in c['steps']));self.assertFalse(any(s['op']=='bind_permanent' for s in c['steps']))
 def test_bare_single_graveyard_skips_unoffered_player_selection(self):
  for name in ('Hymn of Rebirth','Beacon of Unrest','Rise from the Grave','Fated Return','Endless Obedience'):
   for pl in (0,1):
    c=self.cases(name,f'bare_zones_{pl}')[0]
    self.assertFalse(any(s['op']=='choose_player' for s in c['steps']))
    self.assertTrue(any(s['op']=='choose_zone_card' for s in c['steps']))
 def test_repeat_has_mana_reserve_and_explicit_normal_convoke_payment(self):
  c=self.cases('Rise of the Dark Realms','repeat_0')[0]['steps']
  self.assertEqual(sum(s['op']=='set_mana' and s['value']==30 for s in c),6)
  c=self.cases('Endless Obedience','repeat_1')[0]['steps']
  self.assertIn(dict(op='choose_dialog',player=1,value=1),c)
 def test_countered_never_returns_permanent(self):
  c=self.cases(v='countered_0')[0];self.assertIn(dict(op='cast',ref='source',value=0),c['steps']);self.assertFalse(any(s['op']=='bind_permanent' for s in c['steps']))
 def test_repeat_uses_second_spell_and_two_legal_cards(self):
  c=self.cases(v='repeat_0')[0];self.assertEqual(sum(s['op']=='cast' for s in c['steps']),2);self.assertEqual(len({s['value'] for s in c['steps'] if s['op']=='choose_zone_card'}),2)
 def test_ready_pending_and_resolved_snapshot_replay(self):
  for v in ('ready_snapshot_1','pending_snapshot_0','resolved_snapshot_1'):
   c=self.cases(v=v)[0];self.assertIn(dict(op='ai_roundtrip'),c['steps']);self.assertEqual(c['steps'][-1]['op'],'assert_recorded_state')
 def test_extra_component_gaps_remain_explicit(self):
  for name,fragment in [('Endless Obedience','Convoke'),('Rise from the Grave','black Zombie'),('Fated Return','scry 2'),('Profound Journey','next-upkeep'),('Rally the Ancestors','next upkeep'),('Open the Vaults','Aura')]:
   self.assertTrue(any(fragment in g for g in self.cases(name)[0]['_recipe']['untested_components']))
 def test_witness_mutation_rejected_and_inputs_not_mutated(self):
  f.validate_witnesses(self.o,self.rows)
  for field,val in [('type_line','Land'),('cmc',9),('power','9'),('oracle_text','changed'),('colors',[])]:
   changed=copy.deepcopy(self.o);changed['Grizzly Bears'][field]=val
   with self.assertRaises(ValueError):f.validate_witnesses(changed,self.rows)
  d=self.declaration();before=copy.deepcopy((d,self.o));self.cases(d=d);self.assertEqual(before,(d,self.o))

if __name__=='__main__':unittest.main()
