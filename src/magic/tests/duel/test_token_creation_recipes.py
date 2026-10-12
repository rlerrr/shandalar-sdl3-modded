import copy,json,tempfile,unittest
from pathlib import Path
from unittest.mock import patch
from generate_cases import registry,build
from recipes import expand
from token_creation_recipes import spec,variants

class TokenCreationRecipesTest(unittest.TestCase):
 @classmethod
 def setUpClass(cls):
  cls.o=json.loads((Path(__file__).parent/'data/oracle.json').read_text(encoding='utf8'))['cards'];cls.rows={r['name']:r for r in registry()}
 def declaration(self,name='Dragon Fodder',selected=None):
  profiles={
   'Dragon Fodder':([],('Goblin',2,1,1,'R',[],False)),
   'Attended Knight':(['First strike'],('Soldier',1,1,1,'W',[],False)),
   'Hornet Queen':(['Deathtouch','Flying'],('Insect',4,1,1,'G',['Flying','Deathtouch'],False)),
   'Armada Wurm':(['Trample'],('Wurm',1,5,5,'G',['Trample'],False)),
   'Flurry of Horns':([],('Minotaur',2,2,3,'R',['Haste'],False)),
   'Knight Watch':([],('Knight',2,2,2,'W',['Vigilance'],False)),
   'Midnight Haunting':([],('Spirit',2,1,1,'W',['Flying'],False)),
   'Servo Exhibition':([],('Servo',2,1,1,'',[],True))}
  keys,t=profiles[name];nm,count,power,toughness,colors,keywords,artifact=t
  token=dict(name=nm,count=count,power=power,toughness=toughness,colors=list(colors),subtypes=nm.split(),artifact=artifact,keywords=keywords)
  return dict(family='token_creation',version=2,params=dict(keywords=keys,token=token),oracle_text=self.o[name]['oracle_text'],variants=selected or ['create_0'])
 def cases(self,name='Dragon Fodder',selected=None,d=None,o=None,rows=None):
  return expand(name,self.rows[name]['csvid'],d or self.declaration(name,selected),self.o[name],rows or self.rows,oracle_context=o or self.o)
 def test_whole_pin_and_exact_parameter_schema(self):
  d=self.declaration()
  for x in (dict(d,oracle_text='changed'),dict(d,params={}),dict(d,params=dict(d['params'],tap=True))):
   with self.assertRaises(ValueError):self.cases(d=x)
 def test_wrong_characteristics_cannot_match_the_creation_clause(self):
  for changes in ({'count':3},{'power':2},{'colors':['G']},{'keywords':['Haste']},{'artifact':True},{'name':'Soldier','subtypes':['Soldier']}):
   d=self.declaration();d['params']['token'].update(changes)
   with self.assertRaises(ValueError):self.cases(d=d)
 def test_bounded_typed_and_unique_token_fields(self):
  for changes in ({'count':True},{'count':5},{'power':0},{'toughness':9},{'colors':['R','R']},{'subtypes':[]},{'subtypes':['Goblin','Goblin']},{'keywords':['Flying','Flying']},{'keywords':['Infect']},{'artifact':1}):
   d=self.declaration();d['params']['token'].update(changes)
   with self.assertRaises(ValueError):self.cases(d=d)
 def test_additional_source_clauses_reject_partial_expectations(self):
  for suffix in ('\n{T}: Draw a card.','\nWhen this creature dies, draw a card.',' Exile those tokens at the beginning of the next end step.'):
   r=dict(self.o['Attended Knight'],oracle_text=self.o['Attended Knight']['oracle_text']+suffix);d=self.declaration('Attended Knight');d['oracle_text']=r['oracle_text']
   with self.assertRaises(ValueError):spec(d,r)
 def test_normal_single_face_card_required(self):
  for change in (dict(layout='transform'),dict(card_faces=[{}]),dict(selected_face='front'),dict(type_line='Token Creature — Goblin')):
   with self.assertRaises(ValueError):spec(self.declaration(),dict(self.o['Dragon Fodder'],**change))
 def test_only_selected_unique_reviewed_variants(self):
  for vs in ([],['bad_0'],['create_0','create_0']):
   d=self.declaration();d['variants']=vs
   with self.assertRaises(ValueError):self.cases(d=d)
 def test_cast_stack_and_real_resolution_for_both_players(self):
  for c in self.cases(selected=['create_0','create_1']):
   s=c['steps'];self.assertIn(dict(op='assert_stack',value=1),s);self.assertIn(dict(op='resolve'),s);self.assertFalse(any(x['op']=='choose_target' for x in s))
 def test_token_catalog_variants_do_not_supply_expected_characteristics(self):
  o=copy.deepcopy(self.o);o['Goblin'].update(power='99',toughness='99',colors=['U'],keywords=['Flying'],oracle_text='Flying')
  self.assertEqual(self.cases(),self.cases(o=o))
 def test_registry_handler_hints_are_identity_only(self):
  rows=copy.deepcopy(self.rows)
  for r in rows.values():r['handler']='card_dummy'
  self.assertEqual(self.cases(),self.cases(rows=rows))
 def test_fixture_loading_uses_real_entry_without_spell_payment(self):
  s=self.cases('Attended Knight',['preloaded_source_1'])[0]['steps'];self.assertFalse(any(x['op']=='cast' for x in s));self.assertIn(dict(op='assert_zone_multiplicity',player=1,zone='battlefield',index=self.rows['Soldier']['csvid'],value=1),s);self.assertIn(dict(op='assert_mana',player=1,color=5,value=10),s)
 def test_blink_and_recast_create_more_tokens_source_loss_retains_them(self):
  cs=self.cases('Attended Knight',['blink_0','bounce_recast_1','source_destroyed_0','source_bounced_1'])
  for c,n in zip(cs,(2,2,1,1)):
   owner=int(c['_recipe']['variant'][-1]);self.assertIn(dict(op='assert_zone_multiplicity',player=owner,zone='battlefield',index=self.rows['Soldier']['csvid'],value=n),c['steps'])
 def test_tokens_cease_to_exist_after_leaving_the_battlefield(self):
  for c in self.cases(selected=['destroy_token_0','bounce_token_1','exile_token_0']):
   owner=int(c['_recipe']['variant'][-1]);s=c['steps'];self.assertIn(dict(op='assert_zone_multiplicity',player=owner,zone='battlefield',index=self.rows['Goblin']['csvid'],value=1),s)
   for zone in ('hand','graveyard','exile'):self.assertIn(dict(op='assert_zone_multiplicity',player=owner,zone=zone,index=self.rows['Goblin']['csvid'],value=0),s)
 def test_actual_fresh_attack_obeys_haste_without_raw_sickness_assertions(self):
  no=self.cases(selected=['fresh_attack_0'])[0]['steps'];yes=self.cases('Flurry of Horns',['fresh_attack_1'])[0]['steps']
  self.assertIn(dict(op='attack',ref='token',value=0),no);self.assertIn(dict(op='attack',ref='token',value=1),yes);self.assertFalse(any(s.get('property')=='sick' for s in yes))
 def test_flying_evasion_and_reach_use_normal_blocker_declarations(self):
  evasion,reach=self.cases('Midnight Haunting',['evasion_0','reach_blocker_1'])
  self.assertIn(dict(op='block',ref='witness',other='token',value=0),evasion['steps']);self.assertIn(dict(op='block',ref='witness',other='token',value=1),reach['steps'])
 def test_deathtouch_receipts_are_separate_from_power_damage(self):
  s=self.cases('Hornet Queen',['block_ground_0'])[0]['steps'];self.assertIn(dict(op='assert_zone_multiplicity',player=0,zone='battlefield',index=self.rows['Insect']['csvid'],value=3),s);self.assertIn(dict(op='assert_zone_multiplicity',player=1,zone='graveyard',index=self.rows['Grizzly Bears']['csvid'],value=2),s)
 def test_trample_excess_and_vigilance_tapping(self):
  trample=self.cases('Armada Wurm',['trample_0'])[0]['steps'];self.assertIn(dict(op='assert_life',player=1,value=17),trample)
  vig=self.cases('Knight Watch',['ready_attack_0'])[0]['steps'];self.assertIn(dict(op='assert_card',ref='expected_token_0',property='tapped',value=0),vig)
 def test_token_color_artifact_subtypes_and_all_copies_are_inspected(self):
  s=self.cases('Servo Exhibition')[0]['steps'];tid=self.rows['Servo']['csvid']
  for prop,val in (('token',1),('color',0),('artifact',1),('subtype_Servo',1)):
   self.assertIn(dict(op='assert_matching_permanents',player=0,index=tid,property=prop,value=val),s)
  self.assertIn(dict(op='bind_permanent_copy',player=0,value=tid,index=1,ref='expected_token_1'),s)
 def test_payments_and_stocked_zone_absence_are_exact(self):
  paid,wrong=self.cases(selected=['payment_0','wrong_color_1']);self.assertIn(dict(op='assert_mana',player=0,color=4,value=0),paid['steps']);self.assertIn(dict(op='cast',ref='source',value=0),wrong['steps']);self.assertTrue(any(s['op']=='assert_zone_card' for s in paid['steps']))
 def test_snapshots_replay_creation_and_characteristic_checks_after_ai(self):
  for c in self.cases(selected=['ready_snapshot_0','pending_snapshot_1','resolved_snapshot_0']):
   s=c['steps'];cut=s.index(dict(op='restore'));self.assertIn(dict(op='ai_roundtrip'),s);self.assertIn(dict(op='assert_recorded_state'),s);self.assertTrue(any(x['op']=='assert_matching_permanents' for x in s[cut:]));self.assertFalse(any(x['op']=='event' for x in s))
 def test_custom_decisions_and_changed_witnesses_rejected(self):
  o=copy.deepcopy(self.o);o['Grizzly Bears']['power']='3'
  with self.assertRaises(ValueError):self.cases(o=o)
  with self.assertRaises(ValueError):self.cases(d=dict(self.declaration(),support_permanents=['Forest']))
 def test_copy_binding_and_matching_observation_schema(self):
  with tempfile.TemporaryDirectory(dir=Path.cwd()) as temporary:
   root=Path(temporary);cases=root/'cases';cases.mkdir();(root/'oracle.json').write_text(json.dumps(dict(version=1,cards={'Wrath of God':self.o['Wrath of God'],'Soldier':self.o['Soldier']},source={})));(root/'oracle_aliases.json').write_text(json.dumps(dict(version=1,aliases={})));(root/'fixtures.json').write_text(json.dumps(dict(version=1,fixtures={'empty_main':dict(cards=[])})))
   base=dict(op='bind_permanent_copy',player=0,value=self.rows['Soldier']['csvid'],index=0,ref='token')
   bad=[dict(base,index=-1),dict(base,index=16),dict(base,index=True),dict(base,ref=''),dict(base,player=2),dict(base,value=-1)]
   bad += [dict(op='assert_matching_permanents',player=0,index=-1,property='token',value=1),dict(op='assert_matching_permanents',player=0,index=self.rows['Soldier']['csvid'],property='subtype_Unknown',value=1)]
   for action in bad:
    (cases/'case.json').write_text(json.dumps(dict(version=1,card='Wrath of God',csvid=self.rows['Wrath of God']['csvid'],cases=[dict(name='Boundary',fixture='empty_main',steps=[action])])));
    with patch('generate_cases.registry',return_value=[self.rows['Wrath of God'],self.rows['Soldier']]):
     with self.assertRaises(ValueError):build(root,root/'generated')
   (cases/'case.json').write_text(json.dumps(dict(version=1,card='Wrath of God',csvid=self.rows['Wrath of God']['csvid'],cases=[dict(name='Boundary',fixture='empty_main',steps=[base,dict(op='assert_card',ref='token',property='power',value=1),base])])));
   with patch('generate_cases.registry',return_value=[self.rows['Wrath of God'],self.rows['Soldier']]):build(root,root/'generated')

if __name__=='__main__':unittest.main()
