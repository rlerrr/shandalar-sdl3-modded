import copy,json,unittest
from pathlib import Path
from generate_cases import registry,validate_upkeep_step
from cumulative_upkeep_payment_recipes import FAMILY,classify,spec,variants,expand,validate_witnesses

NAMES=['Firestorm Hellkite','Yavimaya Ants','Illusionary Wall','Gallowbraid','Revered Unicorn','Illusions of Grandeur','Phobian Phantasm','Flow of Maggots']
class CumulativeUpkeepPaymentRecipesTest(unittest.TestCase):
 @classmethod
 def setUpClass(cls):cls.o=json.loads((Path(__file__).parent/'data/oracle.json').read_text(encoding='utf8'))['cards'];cls.rows={r['name']:r for r in registry()}
 def declaration(self,n):return dict(family=FAMILY,version=2,oracle_text=self.o[n]['oracle_text'],params=classify(self.o[n]),variants=['age_one_0'])
 def case(self,n,v):
  d=self.declaration(n);p=spec(d,self.o[n]);return expand(n,self.rows[n]['csvid'],d,p,[v],self.rows,self.o)[0]
 def observations(self,c,op,**kw):return [s['value'] for s in c['steps'] if s['op']==op and all(s.get(k)==v for k,v in kw.items())]
 def test_complete_Oracle_costs_native_passives_and_callbacks(self):
  for n in NAMES:
   d=self.declaration(n);p=spec(d,self.o[n]);self.assertIn(p['cost_kind'],('mana','life'))
   with self.assertRaises(ValueError):classify(dict(self.o[n],oracle_text=self.o[n]['oracle_text']+'\nDraw a card.'))
  self.assertEqual(classify(self.o['Yavimaya Ants'])['mana_per_age'],[0,0,0,2,0,0]);self.assertEqual(classify(self.o['Firestorm Hellkite'])['mana_per_age'],[0,0,1,0,1,0]);self.assertEqual(classify(self.o['Gallowbraid'])['life_per_age'],1)
  self.assertEqual(classify(self.o['Illusionary Wall'])['keywords'],['Defender','First strike','Flying']);self.assertEqual((self.o['Illusionary Wall']['power'],self.o['Illusionary Wall']['toughness']),('7','4'))
  self.assertEqual(classify(self.o['Revered Unicorn'])['leave_age_gain'],1);self.assertEqual(classify(self.o['Illusions of Grandeur'])['entry_gain'],20);self.assertEqual(classify(self.o['Illusions of Grandeur'])['leave_loss'],20)
 def test_exact_JSON_types_and_full_independent_source_pin(self):
  n='Firestorm Hellkite';d=self.declaration(n);r=self.o[n]
  for k,v in [('version',2.0),('version',True),('family','upkeep_echo_payment'),('oracle_text','Cumulative upkeep {U}')]:
   with self.assertRaises(ValueError):spec(dict(d,**{k:v}),r)
  for k,v in [('mana_per_age',[0,0,1.0,0,1,0]),('life_per_age',False),('entry_gain',0.0),('blocker_requirement',[]),('keywords',()),('extra',1)]:
   bad=copy.deepcopy(d);bad['params'][k]=v
   with self.assertRaises(ValueError):spec(bad,r)
  with self.assertRaises(ValueError):spec(d,dict(r,colors=['B']))
 def test_whole_Fear_and_Wall_blocker_clauses_are_parameters_not_names(self):
  p=classify(self.o['Phobian Phantasm']);self.assertEqual(p['keywords'],['Fear','Flying']);self.assertEqual(p['blocker_requirement'],'artifact_or_black');self.assertEqual(classify(self.o['Flow of Maggots'])['blocker_requirement'],'wall_only')
  self.assertEqual(classify(dict(self.o['Flow of Maggots'],name='Another Insect'))['blocker_requirement'],'wall_only')
  bad=dict(self.o['Phobian Phantasm'],oracle_text=self.o['Phobian Phantasm']['oracle_text'].replace(" (This creature can't be blocked except by artifact creatures and/or black creatures.)",''))
  with self.assertRaises(ValueError):classify(bad)
 def test_no_omitted_activations_hybrid_snow_or_extra_phase_clauses(self):
  for n in ("Arnjlot's Ascent",'Arctic Nishoba','Blizzard','Balduvian Fallen','Elephant Grass','Cover of Winter',"Hibernation's End",'Phyrexian Etchings','Sustaining Spirit','Glacial Chasm','Thought Lash'):
   with self.assertRaises(ValueError):classify(self.o[n])
  n='Illusions of Grandeur';bad=dict(self.o[n],oracle_text=self.o[n]['oracle_text'].replace('\nWhen this enchantment leaves the battlefield, you lose 20 life.',''))
  with self.assertRaises(ValueError):classify(bad)
 def test_full_witness_honesty_and_live_primary_handlers(self):
  validate_witnesses(self.o,self.rows);self.assertEqual(self.rows['Ornithopter']['handler'],'card_dummy')
  bad=copy.deepcopy(self.o);bad['Bog Imp']['oracle_text']=''
  with self.assertRaises(ValueError):validate_witnesses(bad,self.rows)
  bad=copy.deepcopy(self.rows);bad['Gallowbraid']['handler']='card_dummy'
  with self.assertRaises(ValueError):validate_witnesses(self.o,bad)
 def test_exact_released_followers_preserve_full_cost_and_native_clauses(self):
  for n,kind,keys,total in [('Uktabi Efreet','mana',[],62),('Illusionary Forces','mana',['Flying'],70),('Morinfen','life',['Flying'],65)]:
   p=spec(self.declaration(n),self.o[n]);self.assertEqual(p['cost_kind'],kind);self.assertEqual(p['keywords'],keys);self.assertEqual(len(variants(p)),total)
  self.assertEqual(spec(self.declaration('Morinfen'),self.o['Morinfen'])['legendary'],True)
  self.assertEqual(classify(self.o['Uktabi Efreet'])['mana_per_age'],[0,0,0,1,0,0]);self.assertEqual(classify(self.o['Illusionary Forces'])['mana_per_age'],[0,0,1,0,0,0]);self.assertEqual(classify(self.o['Morinfen'])['life_per_age'],1)
 def test_follower_composition_uses_real_costs_control_native_combat_and_cancel(self):
  c=self.case('Uktabi Efreet','age_three_0');self.assertEqual(len([s for s in c['steps'] if s['op']=='choose_mana_source']),6);self.assertEqual(self.observations(c,'assert_counter',ref='source',counter_kind='age')[-1],3)
  c=self.case('Illusionary Forces','defend_flying_0');self.assertEqual(self.observations(c,'block'),[1]);self.assertEqual(self.observations(c,'assert_zone_multiplicity',player=0,zone='graveyard',index=self.rows['Illusionary Forces']['csvid'])[-1],1)
  c=self.case('Morinfen','control_return_1');self.assertEqual(self.observations(c,'assert_counter',ref='source',counter_kind='age')[-1],4);self.assertEqual(self.observations(c,'assert_life',player=0)[-1],17);self.assertEqual(self.observations(c,'assert_life',player=1)[-1],13)
  for n in ('Uktabi Efreet','Illusionary Forces','Morinfen'):
   c=self.case(n,'cast_cancel_0');self.assertEqual(self.observations(c,'require_can_cast',ref='source'),[]);i=next(i for i,s in enumerate(c['steps']) if s['op']=='choose_cancel_mana');self.assertEqual(c['steps'][i+1],dict(op='cast',ref='source',value=0));self.assertEqual(c['steps'][i+2],dict(op='assert_unused_choices',value=0))
 def test_age_scales_each_paid_upkeep_and_real_human_mana_sources(self):
  c=self.case('Firestorm Hellkite','age_three_0');s=c['steps'];self.assertEqual(len([x for x in s if x['op']=='choose_mana_source']),12)
  self.assertEqual(max(self.observations(c,'assert_counter',ref='source',counter_kind='age')),3)
  self.assertEqual(self.observations(c,'assert_life',player=0)[-1],20)
  c=self.case('Gallowbraid','age_three_1');self.assertEqual(self.observations(c,'assert_life',player=1)[-1],14);self.assertEqual(max(self.observations(c,'assert_counter',ref='source',counter_kind='age')),3)
  self.assertFalse(any(s['op']=='choose_mana_source' for s in c['steps']))
 def test_cancel_uses_real_Oracle_affordability_not_raw_creature_callback(self):
  from recipes import cost_from_oracle
  for n in NAMES:
   c=self.case(n,'cast_cancel_0');s=c['steps'];fee=cost_from_oracle(self.o[n]);lands=[x for x in c['setup'] if x['ref'].startswith('cast_land_')]
   self.assertEqual([sum(x['name']==nm for x in lands) for nm in ('Wastes','Swamp','Island','Forest','Mountain','Plains')],fee)
   self.assertEqual(len([x for x in s if x['op']=='require_can_activate' and x['value']==1]),sum(fee));self.assertTrue(all(x['value']==0 for x in s if x['op'] in ('set_mana','assert_mana')))
   # The API's raw EVENT_CAN_CAST result is not full ordinary-creature
   # eligibility. The real cast path checks mana and handles cancellation.
   self.assertEqual(self.observations(c,'require_can_cast',ref='source'),[] if 'Creature' in self.o[n]['type_line'] else [1])
   i=next(i for i,x in enumerate(s) if x['op']=='choose_cancel_mana');self.assertEqual(s[i],dict(op='choose_cancel_mana',player=0));self.assertEqual(s[i+1],dict(op='cast',ref='source',value=0));self.assertEqual(s[i+2],dict(op='assert_unused_choices',value=0))
 def test_unaffordable_color_and_life_costs_are_not_synthetic_declines(self):
  for owner in (0,1):
   c=self.case('Yavimaya Ants','unaffordable_one_'+str(owner));self.assertEqual(self.observations(c,'choose_dialog',player=owner),[1] if owner else [])
   c=self.case('Firestorm Hellkite','wrong_color_age3_'+str(owner));self.assertEqual(self.observations(c,'assert_zone_multiplicity',player=owner,zone='graveyard',index=self.rows['Firestorm Hellkite']['csvid'])[-1],1)
   c=self.case('Gallowbraid','life_short_'+str(owner));self.assertEqual(len([s for s in c['steps'] if s['op']=='cast' and s['ref'].startswith('bolt_')]),5);self.assertEqual(self.observations(c,'assert_life',player=owner)[-1],2)
   self.assertEqual(self.observations(c,'assert_zone_multiplicity',player=owner,zone='graveyard',index=self.rows['Gallowbraid']['csvid'])[-1],1)
  c=self.case('Gallowbraid','armor_life_0');self.assertEqual(self.observations(c,'assert_life',player=0)[-1],17)
 def test_leave_age_uses_actual_sacrifice_and_owner_zone_receipts(self):
  for v,life in [('decline_one_0',21),('decline_after_one_0',22),('pay_cancel_0',21),('destroy_age2_0',22),('combat_leave_0',22)]:
   c=self.case('Revered Unicorn',v);self.assertEqual(self.observations(c,'assert_life',player=0)[-1],life);self.assertEqual(self.observations(c,'assert_zone_multiplicity',player=0,zone='graveyard',index=self.rows['Revered Unicorn']['csvid'])[-1],1)
  c=self.case('Revered Unicorn','controller_decline_0');self.assertEqual(self.observations(c,'assert_life',player=1)[-1],23);self.assertEqual(self.observations(c,'assert_zone_multiplicity',player=0,zone='graveyard',index=self.rows['Revered Unicorn']['csvid'])[-1],1)
 def test_Grandeur_bookends_and_countered_cast_never_enters(self):
  c=self.case('Illusions of Grandeur','recast_0');life=self.observations(c,'assert_life',player=0);self.assertEqual(life[-1],40);self.assertIn(20,life);self.assertIn(40,life)
  ages=self.observations(c,'assert_counter',ref='source',counter_kind='age');self.assertEqual(ages[-1],1);self.assertIn(2,ages)
  c=self.case('Illusions of Grandeur','cast_countered_1');self.assertEqual(self.observations(c,'assert_life',player=1)[-1],20);self.assertEqual(self.observations(c,'assert_counter',ref='source',counter_kind='age'),[])
  ops=[s['op'] for s in c['steps']];self.assertIn('cast_response',ops);self.assertIn('resolve_response',ops);self.assertNotIn('resolve',ops);self.assertEqual(ops.count('announce_spell'),2)
 def test_control_changes_preserve_age_new_object_restarts(self):
  for owner in (0,1):
   c=self.case('Yavimaya Ants','control_return_'+str(owner));self.assertEqual(self.observations(c,'assert_counter',ref='source',counter_kind='age')[-1],4)
   self.assertEqual([s['player'] for s in c['steps'] if s['op']=='bind_permanent_copy' and s['ref']=='source'],[1-owner,owner])
   c=self.case('Yavimaya Ants','recast_'+str(owner));ages=self.observations(c,'assert_counter',ref='source',counter_kind='age');self.assertEqual(ages[-1],1);self.assertTrue(any(a==2 and 0 in ages[i:] for i,a in enumerate(ages)))
 def test_native_evasion_and_defending_first_strike_are_actual_combat(self):
  choices=[('Phobian Phantasm','fear_artifact_flyer_0',1),('Phobian Phantasm','fear_black_flyer_1',1),('Phobian Phantasm','fear_nonmatching_flyer_0',0),('Phobian Phantasm','fear_ground_artifact_1',0),('Flow of Maggots','wall_changeling_0',1),('Flow of Maggots','wall_nonmatching_1',0),('Firestorm Hellkite','ground_block_1',0),('Firestorm Hellkite','reach_block_0',1)]
  for n,v,legal in choices:
   c=self.case(n,v);self.assertEqual(self.observations(c,'block'),[legal]);self.assertIn('combat_damage',[s['op'] for s in c['steps']])
  c=self.case('Illusionary Wall','defend_flying_1');self.assertEqual(self.observations(c,'block'),[1]);self.assertEqual(self.observations(c,'assert_card',ref='source',property='damage')[-1],0);self.assertEqual(self.observations(c,'assert_zone_multiplicity',player=0,zone='graveyard',index=self.rows['Air Elemental']['csvid'])[-1],1)
  self.assertEqual(self.observations(self.case('Illusionary Wall','fresh_attack_0'),'attack'),[0]);self.assertEqual(self.observations(self.case('Yavimaya Ants','fresh_attack_1'),'attack'),[1])
 def test_every_new_case_uses_real_phase_entry_and_replayable_snapshots(self):
  count=0
  for n in NAMES:
   d=self.declaration(n);p=spec(d,self.o[n]);cases=expand(n,self.rows[n]['csvid'],d,p,variants(p),self.rows,self.o);count+=len(cases)
   for c in cases:
    current=int(c['name'].rsplit('_',1)[1]);ready=None;saved=None;advanced=False
    for s in c['steps']:
     op=s['op'];self.assertNotIn(op,('set_counter','add_counter','remove_counter','set_life','set_phase','set_turn','set_state'))
     if op=='advance_turn':current=1-current;advanced=True;ready=None
     elif op=='untap_step':self.assertTrue(advanced);self.assertEqual(s['player'],current);ready=current;advanced=False
     elif op=='upkeep_step':validate_upkeep_step(s);self.assertEqual(ready,current);self.assertEqual(s['player'],current);ready=None
     elif op=='turn':self.assertEqual(s['player'],current);self.assertEqual(s['value'],0);ready=None
     elif op=='capture':saved=(current,ready,advanced)
     elif op=='restore':self.assertIsNotNone(saved);current,ready,advanced=saved
    if 'snapshot' in c['_recipe']['variant']:
     self.assertEqual(len([s for s in c['steps'] if s['op']=='assert_recorded_state']),1);self.assertIn('ai_roundtrip',[s['op'] for s in c['steps']])
  self.assertEqual(count,515)

if __name__=='__main__':unittest.main()
