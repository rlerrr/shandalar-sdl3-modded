import copy,json,unittest
from pathlib import Path
from generate_cases import registry
from recipes import explicit_spec,expand
from mass_stats_recipes import variants,validate_witnesses,ARMY

class MassStatsRecipesTest(unittest.TestCase):
 @classmethod
 def setUpClass(cls):
  cls.oracle=json.loads((Path(__file__).parent/'data/oracle.json').read_text(encoding='utf8'))['cards'];cls.rows={r['name']:r for r in registry()}
 def declaration(self,name='Glorious Charge',selected=None,**changes):
  p=dict(scope='own',state='all',predicate='all',value='',power=1,toughness=1,draw=0,untap=False);p.update(changes)
  return dict(family='mass_stats',version=2,params=p,oracle_text=self.oracle[name]['oracle_text'],variants=selected or ['effect_0'])
 def cases(self,name='Glorious Charge',selected=None,**changes):
  return expand(name,self.rows[name]['csvid'],self.declaration(name,selected,**changes),self.oracle[name],self.rows,oracle_context=self.oracle)
 def has(self,steps,**values):return any(all(s.get(k)==v for k,v in values.items()) for s in steps)
 def test_explicit_keys(self):
  for p in ({},dict(self.declaration()['params'],extra=1)):
   with self.assertRaises(ValueError):explicit_spec(dict(self.declaration(),params=p),self.oracle['Glorious Charge'])
 def test_exact_signed_stats(self):
  for p in (dict(power=2),dict(toughness=-1),dict(power=0,toughness=0)):
   with self.assertRaises(ValueError):self.cases(**p)
 def test_controller_pin(self):
  for scope in ('both','opponents','target'):
   with self.assertRaises(ValueError):self.cases(scope=scope)
 def test_combat_state_pin(self):
  for state in ('attacking','blocking','tapped'):
   with self.assertRaises(ValueError):self.cases(state=state)
 def test_predicate_pin(self):
  for p in (dict(predicate='color',value='white'),dict(predicate='not_color',value='blue'),dict(predicate='not_subtype',value='Goblin'),dict(value='white')):
   with self.assertRaises(ValueError):self.cases(**p)
 def test_scalar_types_and_bounds(self):
  for p in (dict(power=True),dict(power=8),dict(toughness=-5),dict(draw=True),dict(draw=2),dict(untap=1)):
   with self.assertRaises(ValueError):self.cases(**p)
 def test_additional_effect_pin(self):
  for p in (dict(draw=1),dict(untap=True)):
   with self.assertRaises(ValueError):self.cases(**p)
 def test_full_oracle_pin(self):
  with self.assertRaises(ValueError):explicit_spec(dict(self.declaration(),oracle_text='changed'),self.oracle['Glorious Charge'])
 def test_non_normal_or_keyword_record(self):
  for p in (dict(layout='transform'),dict(card_faces=[{}]),dict(selected_face='front'),dict(type_line='Enchantment'),dict(keywords=['Storm'])):
   with self.assertRaises(ValueError):explicit_spec(self.declaration(),dict(self.oracle['Glorious Charge'],**p))
 def test_missing_explicit_variants(self):
  d=self.declaration();del d['variants']
  with self.assertRaises(ValueError):explicit_spec(d,self.oracle['Glorious Charge'])
 def test_invalid_variant(self):
  with self.assertRaises(ValueError):self.cases(selected=['made_up_0'])
 def test_witness_refresh(self):
  for n in ('Scaled Wurm','Counterspell','Darksteel Relic','Maze of Ith','Ashcoat Bear'):
   o=copy.deepcopy(self.oracle);o[n]['oracle_text']='changed'
   with self.assertRaises(ValueError):validate_witnesses(o,self.rows)
 def test_negative_zero_oracle_spelling(self):
  for n in ('Hell Swarm','Marsh Gas'):
   p=explicit_spec(self.declaration(n,scope='both',power=-1 if n=='Hell Swarm' else -2,toughness=0),self.oracle[n]);self.assertEqual(p['toughness'],0)
 def test_real_cast_resolve(self):
  ops=[s['op'] for s in self.cases()[0]['steps']];self.assertLess(ops.index('cast'),ops.index('resolve'));self.assertNotIn('dispatch_event',ops)
 def test_populated_zones_and_mixed_armies(self):
  c=self.cases()[0]
  for player in (0,1):
   for z in ('hand','library','graveyard','exile'):self.assertTrue(any(s['zone']==z and s['player']==player for s in c['setup']))
   self.assertEqual(sum(s['zone']=='battlefield' and s['player']==player for s in c['setup']),len(ARMY)+2)
 def test_noncreature_witnesses_cannot_supply_mana(self):
  c=self.cases()[0];ns={s['name'] for s in c['setup'] if s['ref'].startswith(('artifact_','land_'))};self.assertEqual(ns,{'Darksteel Relic','Maze of Ith'})
 def test_controller_distinction(self):
  s=self.cases()[0]['steps'];self.assertTrue(self.has(s,op='assert_card',ref='army_0_0',property='power',value=3));self.assertTrue(self.has(s,op='assert_card',ref='army_1_0',property='power',value=2))
 def test_color_distinction(self):
  s=self.cases("Guardians' Pledge",power=2,toughness=2,predicate='color',value='white')[0]['steps'];self.assertTrue(self.has(s,ref='army_0_0',property='power',value=4));self.assertFalse(self.has(s,ref='army_0_3',property='power',value=4))
 def test_nonwhite_deaths(self):
  s=self.cases('Holy Light',scope='both',predicate='not_color',value='white',power=-1,toughness=-1)[0]['steps'];self.assertTrue(self.has(s,ref='army_0_1',property='in_play',value=0));self.assertFalse(self.has(s,ref='army_0_0',property='in_play',value=0))
 def test_nonelf_predicate(self):
  s=self.cases('Eyeblight Massacre',scope='both',predicate='not_subtype',value='Elf',power=-2,toughness=-2)[0]['steps'];self.assertFalse(self.has(s,ref='army_0_5',property='power',value=0));self.assertTrue(self.has(s,ref='army_0_3',property='in_play',value=0))
 def test_exact_payment(self):
  s=self.cases(selected=['payment_1'])[0]['steps'];self.assertTrue(self.has(s,op='set_mana',player=1,color=0,value=1));self.assertTrue(self.has(s,op='assert_mana',player=1,color=4,value=0))
 def test_insufficient_mana_no_resolution(self):
  s=self.cases(selected=['insufficient_mana_0'])[0]['steps'];self.assertTrue(self.has(s,op='cast',value=0));self.assertFalse(self.has(s,op='resolve'))
 def test_empty_board_is_deliberate(self):
  c=self.cases(selected=['empty_battlefield_0'])[0];self.assertFalse(any(s['zone']=='battlefield' for s in c['setup']));self.assertTrue(self.has(c['steps'],op='cast',value=1))
 def test_tapped_and_sick_are_eligible(self):
  for v in ('tapped_0','sick_0'):
   s=self.cases(selected=[v])[0]['steps'];self.assertTrue(self.has(s,ref='army_0_0',property='power',value=3))
 def test_pending_hand_inspection_skipped(self):
  s=self.cases()[0]['steps'];i=next(i for i,a in enumerate(s) if a['op']=='cast');j=next(i for i,a in enumerate(s) if a['op']=='resolve');self.assertFalse(any(a['op']=='assert_zone' and a['zone']=='hand' for a in s[i:j]))
 def test_stacked_spells_share_stack(self):
  s=self.cases(selected=['stacked_spells_1'])[0]['steps'];j=next(i for i,a in enumerate(s) if a['op']=='resolve');self.assertEqual(sum(a['op']=='cast' for a in s[:j]),2);self.assertTrue(self.has(s[:j],op='assert_stack',value=2));self.assertTrue(self.has(s[j:],ref='army_1_0',property='power',value=4))
 def test_two_copies_resolve_sequentially(self):
  ops=[s['op'] for s in self.cases(selected=['two_copies_0'])[0]['steps']];self.assertEqual([op for op in ops if op in ('cast','resolve')],['cast','resolve','cast','resolve'])
 def test_late_entry_frozen_cohort(self):
  s=self.cases(selected=['late_entry_0'])[0]['steps'];self.assertTrue(self.has(s,op='cast',ref='late',value=1));self.assertTrue(self.has(s,ref='late',property='power',value=2));self.assertFalse(self.has(s,ref='late',property='power',value=3))
 def test_cleanup_restores_base_stats(self):
  s=self.cases(selected=['cleanup_0'])[0]['steps'];i=next(i for i,a in enumerate(s) if a['op']=='cleanup');self.assertTrue(self.has(s[:i],ref='army_0_0',property='power',value=3));self.assertTrue(self.has(s[i:],ref='army_0_0',property='power',value=2))
 def test_counter_through_priority(self):
  s=self.cases(selected=['countered_1'])[0]['steps'];self.assertTrue(self.has(s,op='choose_priority',ref='response'));self.assertTrue(self.has(s,op='cast',ref='source',value=0));self.assertFalse(self.has(s,ref='army_1_0',property='power',value=3))
 def test_real_combat_declarations(self):
  s=self.cases(selected=['combat_blocked_1'])[0]['steps'];ops=[a['op'] for a in s];self.assertTrue(self.has(s,op='block',other='army_1_7'));self.assertLess(ops.index('begin_attackers'),ops.index('cast'));self.assertIn('combat_damage',ops);self.assertIn('end_combat',ops)
 def test_sorcery_before_combat(self):
  s=self.cases('Languish',['combat_blocked_0'],scope='both',power=-4,toughness=-4)[0]['steps'];ops=[a['op'] for a in s];self.assertLess(ops.index('resolve'),ops.index('begin_attackers'))
 def test_attacking_and_blocking_are_distinct(self):
  for n,state,power,toughness,boostref in (('Trumpet Blast','attacking',2,0,'army_0_7'),('Piety','blocking',0,3,'army_0_7')):
   s=self.cases(n,state=state,scope='both',power=power,toughness=toughness)[0]['steps'];self.assertTrue(self.has(s,ref=boostref,property='power' if power else 'toughness',value=9 if power else 9))
 def test_draw_and_untap_components(self):
  s=self.cases('Blinding Spray',scope='opponents',power=-4,toughness=0,draw=1)[0]['steps'];self.assertTrue(self.has(s,op='assert_zone',player=0,zone='library',value=6))
  s=self.cases('War Flare',['tapped_0'],power=2,toughness=1,untap=True)[0]['steps'];self.assertTrue(self.has(s,ref='army_0_0',property='tapped',value=0))
 def test_snapshot_replay_preserves_pending_stack(self):
  s=self.cases(selected=['pending_snapshot_0'])[0]['steps'];ops=[a['op'] for a in s];self.assertLess(ops.index('cast'),ops.index('capture'));self.assertLess(ops.index('capture'),ops.index('resolve'));self.assertEqual(ops.count('cast'),1);self.assertEqual(ops.count('resolve'),2);self.assertIn('ai_roundtrip',ops)

if __name__=='__main__':unittest.main()
