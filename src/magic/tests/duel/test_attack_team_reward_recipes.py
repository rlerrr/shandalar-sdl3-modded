import copy,json,unittest
from pathlib import Path
from attack_team_reward_recipes import classify,spec,variants,expand,validate_witnesses
from generate_cases import registry

NAMES=['Chieftain en-Dal','Fangren Pathcutter','Riot Ringleader','Smogsteed Rider',"Victory's Herald",'Soltari Champion','Nagao, Bound by Honor',"Telim'Tor"]

class AttackTeamRewardRecipesTest(unittest.TestCase):
 @classmethod
 def setUpClass(cls):
  cls.o=json.loads((Path(__file__).parent/'data/oracle.json').read_text(encoding='utf8'))['cards'];cls.rows={r['name']:r for r in registry()}
 def declaration(self,name):return dict(family='attack_team_reward',version=2,oracle_text=self.o[name]['oracle_text'],params=classify(self.o[name]),variants=['effect_0'])
 def case(self,name,value='effect',owner=0):
  d=self.declaration(name);return expand(name,self.rows[name]['csvid'],d,spec(d,self.o[name]),[f'{value}_{owner}'],self.rows,self.o)[0]
 def card(self,ref,key,value):return dict(op='assert_card',ref=ref,property=key,value=value)
 def after(self,c,op='finish_attackers'):
  s=c['steps'];return s[next(i for i,x in enumerate(s) if x['op']==op):]
 def test_whole_oracle_typed_scope_and_full_passive_grammar(self):
  for name in NAMES:
   self.assertEqual(len(classify(self.o[name])['grants'])>0 or classify(self.o[name])['power']>0,True)
   with self.subTest(name=name),self.assertRaises(ValueError):classify(dict(self.o[name],oracle_text=self.o[name]['oracle_text']+'\nDraw a card.'))
  self.assertEqual(classify(dict(self.o['Chieftain en-Dal'],name='Another Chief')),classify(self.o['Chieftain en-Dal']))
  self.assertEqual(classify(self.o['Nagao, Bound by Honor'])['bushido'],1)
  for name in ('Patron of the Akki','Cultivator of Blades','Legion Loyalist'):
   with self.assertRaises(ValueError):classify(self.o[name])
 def test_exact_parameter_types_and_identity_fail_closed(self):
  name='Riot Ringleader';d=self.declaration(name)
  for key,value in [('version',2.0),('version',True),('family','attack_value_trigger'),('oracle_text','Whenever this attacks.')]:
   with self.assertRaises(ValueError):spec(dict(d,**{key:value}),self.o[name])
  for key,value in [('power',1.0),('bushido',False),('other',0),('subtype',[]),('grants',()),('keywords',()),('extra',1)]:
   bad=copy.deepcopy(d);bad['params'][key]=value
   with self.assertRaises(ValueError):spec(bad,self.o[name])
  for change in [dict(layout='transform'),dict(card_faces=[{}]),dict(selected_face='front'),dict(type_line='Token Creature — Human')]:
   with self.assertRaises(ValueError):classify(dict(self.o[name],**change))
 def test_witnesses_pin_full_records_colors_subtypes_and_handler_bindings(self):
  validate_witnesses(self.o,self.rows)
  for name,change in [('Walking Corpse',dict(colors=['G'])),('Glory Seeker',dict(type_line='Creature — Elf Soldier')),('Memnite',dict(type_line='Creature — Construct')),('Devoted Retainer',dict(oracle_text='Bushido 2'))]:
   with self.assertRaises(ValueError):validate_witnesses(dict(self.o,**{name:dict(self.o[name],**change)}),self.rows)
  rows=copy.deepcopy(self.rows);rows['Walking Corpse']['handler']='NULL'
  with self.assertRaises(ValueError):validate_witnesses(self.o,rows)
 def test_all462_expand_without_lethal_fixtures_or_seeded_effects(self):
  total=0
  for name in NAMES:
   d=self.declaration(name);p=spec(d,self.o[name]);cases=expand(name,self.rows[name]['csvid'],d,p,variants(p),self.rows,self.o);total+=len(cases)
   for c in cases:
    self.assertFalse(any(x.get('state') for x in c['setup']));self.assertTrue(all(x['value']>0 for x in c['steps'] if x['op']=='assert_life'))
    self.assertFalse(any(x['op'] in ('set_counter','set_power','set_abilities') for x in c['steps']))
  self.assertEqual(total,462)
 def test_controlled_subtype_and_other_scopes_include_bench_and_exclude_enemy(self):
  for owner in (0,1):
   for name,expected in [('Riot Ringleader',{'source':3,'crew':3,'bench_match':3,'ally':2,'bench':2,'other_match':2}),('Soltari Champion',{'source':2,'crew':3,'bench_match':3,'ally':3,'bench':3,'other_match':2}),('Nagao, Bound by Honor',{'source':4,'crew':2,'bench_match':2,'ally':2,'bench':2,'other_match':1}),("Telim'Tor",{'source':3,'crew':3,'bench_match':2,'ally':2,'bench':2,'other_match':2})]:
    s=self.after(self.case(name,owner=owner))
    for ref,power in expected.items():self.assertIn(self.card(ref,'power',power),s)
 def test_no_source_declaration_produces_no_reward_and_no_fake_priority(self):
  for name in NAMES:
   for owner in (0,1):
    c=self.case(name,'fresh_attack',owner);self.assertIn(dict(op='attack',ref='source',value=0),c['steps'])
    c=self.case(name,'only_others',owner);self.assertFalse(any(x['op']=='attack' and x['ref']=='source' for x in c['steps']));self.assertIn(self.card('source','power',int(self.o[name]['power'])),self.after(c))
    s=self.case(name,owner=owner)['steps'];last=max(i for i,x in enumerate(s) if x['op']=='attack');finish=s.index(dict(op='finish_attackers'))
    self.assertFalse(any(x['op'] in ('cast','activate','destroy','turn','choose_priority','resolve') for x in s[last+1:finish]))
 def test_first_strike_changes_real_ally_combat_and_requires_source(self):
  for owner in (0,1):
   s=self.after(self.case('Chieftain en-Dal','granted_combat',owner),'combat_damage');self.assertIn(self.card('ally','in_play',1),s);self.assertIn(self.card('ally','damage',0),s)
   s=self.after(self.case('Chieftain en-Dal','without_source_combat',owner),'combat_damage');self.assertFalse(any(x['op']=='assert_card' and x['ref']=='ally' for x in s));self.assertIn(dict(op='assert_zone_multiplicity',player=owner,zone='graveyard',index=self.rows['Grizzly Bears']['csvid'],value=1),s)
 def test_trample_assigns_lethal_and_excess_for_both_source_and_ally(self):
  for owner in (0,1):
   s=self.case('Fangren Pathcutter','trample_spill',owner)['steps'];self.assertIn(dict(op='block',ref='enemy',other='source',value=1),s);self.assertIn(dict(op='block',ref='small',other='ally',value=1),s)
   s=self.after(dict(steps=s),'combat_damage');self.assertIn(dict(op='assert_life',player=1-owner,value=17),s);self.assertIn(self.card('source','damage',2),s);self.assertIn(self.card('ally','damage',1),s)
 def test_fear_has_black_and_artifact_permissions_and_excludes_source(self):
  for owner in (0,1):
   for value,b,a,allowed in [('fear_ground_denied','enemy','ally',0),('fear_black','black','ally',1),('fear_artifact','artifact','ally',1),('fear_source_excluded','enemy','source',1)]:
    self.assertIn(dict(op='block',ref=b,other=a,value=allowed),self.case('Smogsteed Rider',value,owner)['steps'])
 def test_flying_lifelink_prevention_and_nonattacking_damage_are_real(self):
  for owner in (0,1):
   for value,b,legal in [('flying_ground_denied','enemy',0),('flying_reach','reach',1),('flying_blocker','flying',1)]:self.assertIn(dict(op='block',ref=b,other='ally',value=legal),self.case("Victory's Herald",value,owner)['steps'])
   s=self.after(self.case("Victory's Herald",'prevented_combat',owner),'combat_damage')
   for pl in (0,1):self.assertIn(dict(op='assert_life',player=pl,value=20),s)
   s=self.case("Victory's Herald",'nonattacker_lifelink',owner)['steps'];act=s.index(dict(op='activate',ref='tim',value=1));combat=s.index(dict(op='begin_blockers'));self.assertIn(dict(op='assert_life',player=owner,value=20),s[act:combat]);self.assertIn(dict(op='assert_life',player=1-owner,value=19),s[act:combat]);self.assertIn(self.card('tim','tapped',1),s[act:combat])
 def test_shadow_is_checked_in_both_blocking_directions(self):
  for owner in (0,1):
   for value,b,a,allowed in [('shadow_ground_denied','enemy','source',0),('shadow_block','enemy_shadow','source',1),('source_blocks','source','enemy',0),('shadow_defend','source','enemy_shadow',1)]:self.assertIn(dict(op='block',ref=b,other=a,value=allowed),self.case('Soltari Champion',value,owner)['steps'])
 def test_bushido_source_and_all_support_are_cast_before_combat(self):
  for owner in (0,1):
   c=self.case('Nagao, Bound by Honor','bushido_attacker',owner);s=c['steps'];finish=s.index(dict(op='finish_attackers'));advance=s.index(dict(op='advance_combat'));damage=s.index(dict(op='combat_damage'))
   for ref in ('source','crew','bench_match','other_match'):self.assertIn(dict(op='cast',ref=ref,value=1),s[:finish]);self.assertEqual(next(x['zone'] for x in c['setup'] if x['ref']==ref),'hand')
   self.assertIn(self.card('source','power',4),s[finish:advance]);self.assertIn(self.card('source','power',5),s[advance:damage]);self.assertIn(self.card('source','toughness',5),s[advance:damage])
   s=self.case('Nagao, Bound by Honor','bushido_defender',owner)['steps'];advance=s.index(dict(op='advance_combat'));damage=s.index(dict(op='combat_damage'));self.assertIn(self.card('source','power',4),s[advance:damage]);self.assertNotIn(self.card('source','power',5),s)
 def test_flanking_penalty_immunity_and_defending_are_real_declaration_receipts(self):
  for owner in (0,1):
   s=self.case("Telim'Tor",'flanking_penalty',owner)['steps'];advance=s.index(dict(op='advance_combat'));damage=s.index(dict(op='combat_damage'));self.assertIn(self.card('giant','power',3),s[:advance]);self.assertIn(self.card('giant','power',2),s[advance:damage]);self.assertIn(self.card('giant','toughness',2),s[advance:damage])
   s=self.case("Telim'Tor",'flanking_immune',owner)['steps'];advance=s.index(dict(op='advance_combat'));damage=s.index(dict(op='combat_damage'));self.assertIn(self.card('other_match','power',2),s[advance:damage])
   s=self.case("Telim'Tor",'flanking_defender',owner)['steps'];advance=s.index(dict(op='advance_combat'));damage=s.index(dict(op='combat_damage'));self.assertIn(self.card('enemy','power',2),s[advance:damage]);self.assertIn(self.card('source','power',2),s[advance:damage])
 def test_later_cast_does_not_receive_completed_trigger_and_source_death_preserves_grant(self):
  for owner in (0,1):
   for name,base in [('Riot Ringleader',2),('Nagao, Bound by Honor',1),('Soltari Champion',2)]:
    s=self.case(name,'late_entry',owner)['steps'];end=s.index(dict(op='end_combat'));cast=s.index(dict(op='cast',ref='late',value=1));self.assertLess(end,cast);self.assertIn(self.card('late','power',base),s[cast:])
   s=self.case('Chieftain en-Dal','source_destroyed_after',owner)['steps'];cut=s.index(dict(op='destroy',ref='source'));self.assertIn(dict(op='block',ref='enemy',other='ally',value=1),s[cut:]);self.assertIn(self.card('ally','damage',0),self.after(dict(steps=s),'combat_damage'));self.assertFalse(any(x['op']=='assert_card' and x['ref']=='source' for x in s[cut+1:]))
 def test_expiry_uses_real_turns_and_untapped_flying_denial_control(self):
  for owner in (0,1):
   s=self.case('Chieftain en-Dal','expiry',owner)['steps'];cleanup=s.index(dict(op='cleanup'));self.assertEqual(sum(x['op']=='advance_turn' for x in s[cleanup:]),2);self.assertIn(dict(op='block',ref='enemy',other='ally',value=1),s[cleanup:]);self.assertIn(dict(op='assert_zone_multiplicity',player=owner,zone='graveyard',index=self.rows['Grizzly Bears']['csvid'],value=1),s[cleanup:])
   s=self.case("Victory's Herald",'expiry_flying',owner)['steps'];cleanup=s.index(dict(op='cleanup'));self.assertIn(dict(op='cast',ref='twiddle',value=1),s[cleanup:]);self.assertIn(dict(op='cast',ref='source_untap',value=1),s[cleanup:]);self.assertIn(dict(op='choose_dialog',player=owner,value=1),s[cleanup:]);self.assertIn(self.card('ally','tapped',0),s[cleanup:]);self.assertIn(self.card('source','tapped',0),s[cleanup:]);self.assertIn(dict(op='block',ref='ally',other='flying',value=0),s[cleanup:]);self.assertIn(dict(op='block',ref='source',other='flying',value=1),s[cleanup:]);self.assertTrue(dict(op='assert_life',player=owner,value=28) in s[cleanup:],'20+8 lifelink from firstcombat; source keeps nativeFlying but noLifelink after expiry, blocking Flying attacker without additionalgain')
 def test_real_cast_costs_human_cancel_and_replay_boundaries(self):
  for name in NAMES:
   c=self.case(name,'cast_cancel');s=c['steps'];lands=[x['ref'] for x in c['setup'] if x['ref'].startswith('payment_land_')];self.assertEqual(len(lands),sum(spec(self.declaration(name),self.o[name])['mana']));cancel=s.index(dict(op='choose_cancel_mana',player=0));self.assertEqual(s[cancel+1],dict(op='cast',ref='source',value=0));self.assertEqual(s[cancel+2],dict(op='assert_unused_choices',value=0))
   for ref in lands:self.assertIn(self.card(ref,'tapped',0),s[cancel:])
  for value in ('ready_snapshot','declaration_snapshot','resolved_snapshot'):
   s=self.case('Riot Ringleader',value)['steps'];self.assertEqual(sum(x['op']=='capture' for x in s),1);self.assertIn(dict(op='ai_roundtrip'),s);self.assertIn(dict(op='assert_recorded_state'),s)

if __name__=='__main__':unittest.main()
