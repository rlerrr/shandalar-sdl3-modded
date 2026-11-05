"""Guard Oracle-derived activation predicates, resource costs and real fixtures."""
import copy
import json
import unittest
from pathlib import Path
from generate_cases import registry
from recipes import expand, explicit_spec
from removal_activation_recipes import eligible, variants, TARGETS


class RemovalActivationRecipesTest(unittest.TestCase):
 @classmethod
 def setUpClass(cls):
  cls.oracle=json.loads((Path(__file__).parent/'data/oracle.json').read_text(encoding='utf8'))['cards']
  cls.rows={r['name']:r for r in registry()}

 def declaration(self,name='Royal Assassin',selected=None):
  scope,state,effect,self_cost,mana,tap,discard,keywords,mana_ability={
   'Royal Assassin':('creature','tapped','destroy','none','',True,0,[],False),
   'Visara the Dreadful':('creature','any','destroy','none','',True,0,['Flying'],False),
   'Wasteland':('nonbasic_land','any','destroy','sacrifice','',True,0,[],True),
   'Brittle Effigy':('creature','any','exile','exile','{4}',True,0,[],False),
   'Seismic Mage':('land','any','destroy','none','{2}{R}',True,1,[],False),
   'Ramses Overdark':('enchanted_creature','any','destroy','none','',True,0,[],False),
   'Savaen Elves':('land_aura','any','destroy','none','{G}{G}',True,0,[],False),
   'Pit Trap':('nonflying_creature','attacking','destroy','sacrifice','{2}',True,0,[],False),
  }[name]
  p=dict(effect=dict(effect=effect,scope=scope,state=state,controller='any',limit=None,no_regenerate=name in ('Visara the Dreadful','Pit Trap'),draw=0,gain_life=0,target_gain_life=0,target_lose_life=0,target_damage=0,add_mana=''),cost=dict(mana=mana,tap=tap,self=self_cost,discard=discard,pay_life=0),keywords=keywords,mana_ability=mana_ability,enters_tapped=False)
  d=dict(family='removal_activation',version=2,params=p,oracle_text=self.oracle[name]['oracle_text'],variants=selected or ['effect_0'])
  if mana_ability:d['activation_dialogs']={'0':1,'1':1}
  return d

 def cases(self,name='Royal Assassin',selected=None,d=None,oracle=None):
  return expand(name,self.rows[name]['csvid'],d or self.declaration(name,selected),self.oracle[name],self.rows,oracle_context=oracle or self.oracle)

 def test_complete_printed_cost_effect_and_source_pin_fail_closed(self):
  for key,value in [('cost',dict(mana='{1}',tap=True,self='none',discard=0,pay_life=0)),('keywords',['Haste']),('mana_ability',True),('enters_tapped',True)]:
   d=self.declaration();d['params'][key]=value
   with self.assertRaises(ValueError):self.cases(d=d)
  for key,value in [('scope','land'),('state','any'),('no_regenerate',True),('draw',1)]:
   d=self.declaration();d['params']['effect'][key]=value
   with self.assertRaises(ValueError):self.cases(d=d)
  for patch in (dict(oracle_text='Destroy target tapped creature.'),dict(variants=[]),dict(variants=['unknown_0'])):
   with self.assertRaises(ValueError):self.cases(d=dict(self.declaration(),**patch))

 def test_target_probe_distinguishes_candidate_from_legal_source(self):
  s=self.cases('Visara the Dreadful',[f'card_{TARGETS.index("Forest")}_0'])[0]['steps']
  self.assertIn(dict(op='probe_activation_target',ref='source',other='recipient',index=-1,value=0),s)
  self.assertFalse(any(x['op']=='activate' and x['ref']=='source' for x in s))
  s=self.cases('Visara the Dreadful',['empty_battlefield_0'])[0]['steps']
  self.assertIn(dict(op='choose_target',player=0,ref='source'),s)

 def test_sacrifice_and_exile_are_paid_before_resolution(self):
  for name,zone in [('Wasteland','graveyard'),('Brittle Effigy','exile')]:
   s=self.cases(name,['payment_1'])[0]['steps'];a=next(i for i,x in enumerate(s) if x['op']=='activate');r=next(i for i,x in enumerate(s) if i>a and x['op']=='resolve')
   self.assertIn(dict(op='assert_zone',player=1,zone=zone,value=5 if zone=='graveyard' else 3),s[a:r])
   self.assertIn(dict(op='assert_mana',player=1,color=0,value=0),s[a:r])

 def test_land_mode_probe_and_real_activation_both_select_removal(self):
  s=self.cases('Wasteland',['effect_0'])[0]['steps']
  self.assertIn(dict(op='probe_activation_target',ref='source',other='recipient',index=1,value=1),s)
  self.assertIn(dict(op='choose_dialog',player=0,value=1),s)
  self.assertNotIn('insufficient_mana_0',variants(explicit_spec(self.declaration('Wasteland'),self.oracle['Wasteland'])))
  s=self.cases('Wasteland',['payment_0'])[0]['steps']
  self.assertFalse(any(x['op']=='probe_activation_target' for x in s))
  self.assertIn(dict(op='activate',ref='source',value=1),s)

 def test_exact_fixture_receipts_do_not_repeat_absent_witness_palette(self):
  c=self.cases()[0];names={x['name'] for x in c['setup']}
  observed={x['index'] for x in c['steps'] if x['op']=='assert_zone_multiplicity'}
  self.assertEqual(observed,{self.rows[n]['csvid'] for n in names})

 def test_land_creature_combat_witness_is_ready_to_attack(self):
  c=self.cases('Pit Trap',[f'card_{TARGETS.index("Dryad Arbor")}_0'])[0]
  self.assertIn('untapped',next(x for x in c['setup'] if x['ref']=='recipient')['state'])
  self.assertIn(dict(op='attack',ref='recipient',value=1),c['steps'])

 def test_sick_and_tapped_sources_are_denied_without_consuming_choices(self):
  for variant in ('sick_0','tapped_source_0','fresh_activation_0'):
   s=self.cases(selected=[variant])[0]['steps']
   self.assertIn(dict(op='require_can_activate',ref='source',value=0),s)
   self.assertIn(dict(op='activate',ref='source',value=0),s)
   self.assertFalse(any(x['op']=='choose_target' for x in s))

 def test_enchanted_creature_requires_a_real_attached_aura(self):
  c=self.cases('Ramses Overdark')[0]
  self.assertTrue(any(x['name']=='Pacifism' and x['zone']=='hand' for x in c['setup']))
  self.assertTrue(any(x['op']=='cast' and x['ref']=='recipient_aura' for x in c['steps']))
  s=self.cases('Ramses Overdark',['unenchanted_0'])[0]['steps']
  self.assertIn(dict(op='probe_activation_target',ref='source',other='recipient',index=-1,value=0),s)

 def test_aura_predicates_use_attached_permanent_controller(self):
  p=self.declaration()['params'];p['effect'].update(scope='own_creature_aura',state='any')
  aura=self.oracle['Robe of Mirrors'];bearer=self.oracle['Grizzly Bears']
  self.assertTrue(eligible(aura,p,0,1,parent=bearer,parent_owner=0))
  self.assertFalse(eligible(aura,p,0,0,parent=bearer,parent_owner=1))
  self.assertFalse(eligible(aura,p,0,0,parent=self.oracle['Forest'],parent_owner=0))
  self.assertFalse(eligible(aura,p,0,0))

 def test_regeneration_keeps_target_and_pays_regeneration_mana(self):
  s=self.cases(selected=['regeneration_0'])[0]['steps']
  self.assertIn(dict(op='choose_regeneration',ref='recipient'),s)
  self.assertIn(dict(op='assert_card',ref='recipient',property='in_play',value=1),s)
  self.assertIn(dict(op='assert_mana',player=1,color=1,value=19),s)

 def test_snapshot_replays_real_pending_activation_and_all_receipts(self):
  s=self.cases('Seismic Mage',['pending_snapshot_1'])[0]['steps']
  for op in ('capture','restore','ai_roundtrip','record_state','assert_recorded_state'):self.assertTrue(any(x['op']==op for x in s))
  self.assertEqual(sum(x['op']=='activate' for x in s),1)
  self.assertTrue(any(x['op']=='assert_hand_graveyard_total' for x in s))

 def test_real_untap_helper_allows_second_activation(self):
  c=self.cases(selected=['untap_repeat_0'])[0]
  self.assertTrue(any(x['name']=="Kiora's Follower" for x in c['setup']))
  self.assertEqual(sum(x['op']=='activate' for x in c['steps']),3)
  self.assertIn(dict(op='choose_target',player=0,ref='second'),c['steps'])

 def test_handler_metadata_does_not_drive_expectations_and_witnesses_are_pinned(self):
  d=self.declaration();before=copy.deepcopy(d);original=self.cases(d=d);self.assertEqual(d,before)
  altered=copy.deepcopy(self.rows);altered['Royal Assassin']['handler']='unrelated';altered['Royal Assassin']['mana_cost']='{19}'
  self.assertEqual(original,expand('Royal Assassin',self.rows['Royal Assassin']['csvid'],d,self.oracle['Royal Assassin'],altered,oracle_context=self.oracle))
  o=copy.deepcopy(self.oracle);o['River Boa']['keywords']=[]
  with self.assertRaises(ValueError):self.cases(oracle=o)


if __name__=='__main__':unittest.main()
