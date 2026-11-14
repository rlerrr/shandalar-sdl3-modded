import copy,json,unittest
from pathlib import Path
from generate_cases import registry
import cascade_spell_effect_recipes as f

class CascadeTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.o=json.loads((Path(__file__).parent/'data/oracle.json').read_text(encoding='utf8'))['cards'];cls.rows={r['name']:r for r in registry()}
    def case(self,n='Enlisted Wurm',v='accept_0'):
        r=self.o[n];p=f.classify(r);d=dict(family=f.FAMILY,version=2,oracle_text=r['oracle_text'],params=f.parameters(p),variants=[v])
        return f.expand(n,self.rows[n]['csvid'],d,f.spec(d,r),[v],self.rows,self.o)[0]
    def final(self,c,op,**kw):return next(x['value'] for x in reversed(c['steps']) if x['op']==op and all(x.get(k)==v for k,v in kw.items()))
    def test_complete_source_and_witness_pins_and_nine_implemented_primaries(self):
        f.validate_witnesses(self.o,self.rows);self.assertEqual(len(f.pins()['sources']),9)
        self.assertEqual({f.classify(self.o[n])['effect'] for n in f.pins()['sources']},{'vanilla','haste','exalted','damage','life','cannot_block','bounce','team_power'})
    def test_whole_Cascade_reminder_and_unmodeled_clauses_fail_closed(self):
        r=self.o['Enlisted Wurm']
        for q in (dict(r,oracle_text='Cascade'),dict(r,oracle_text=r['oracle_text']+'\nDraw a card.'),dict(r,layout='transform'),dict(r,keywords=[]),dict(r,cmc=2),dict(r,mana_cost='{X}{G}'),dict(r,power='*')):
            with self.assertRaises(ValueError):f.classify(q)
    def test_whole_primary_clauses_cannot_be_replaced_by_similar_effects(self):
        changes={'Bituminous Blast':('4 damage','3 damage'),'Demonic Dread':("can't block this turn","can't attack this turn"),'Deny Reality':("owner's hand","controller's hand"),'Captured Sunlight':('4 life','3 life'),'Violent Outburst':('+1/+0','+1/+1'),'Bloodbraid Elf':(f.HASTE,'Haste'),'Ardent Plea':(f.EXALTED,'Exalted')}
        for n,(a,b) in changes.items():
            with self.assertRaises(ValueError):f.classify(dict(self.o[n],oracle_text=self.o[n]['oracle_text'].replace(a,b)))
    def test_damage_grammar_is_name_neutral_and_uses_actual_self_name(self):
        r=self.o['Bituminous Blast'];q=dict(r,name='Another Blast',oracle_text=r['oracle_text'].replace(r['name'],'Another Blast'))
        self.assertEqual(f.classify(r),f.classify(q))
        with self.assertRaises(ValueError):f.classify(dict(q,oracle_text=r['oracle_text']))
    def test_typed_parameters_exact_variants_and_source_identity(self):
        n='Enlisted Wurm';r=self.o[n];p=f.classify(r);d=dict(family=f.FAMILY,version=2,oracle_text=r['oracle_text'],params=f.parameters(p),variants=['accept_0'])
        for key,value in [('mv',True),('amount',False),('target','creature'),('extra',0)]:
            q=copy.deepcopy(d);q['params'][key]=value
            with self.assertRaises(ValueError):f.spec(q,r)
        for vs in ([],['accept_0','accept_0'],['scan_0']):
            with self.assertRaises(ValueError):f.spec(dict(d,variants=vs),r)
    def test_witness_cost_body_full_Oracle_and_handler_fail_closed(self):
        for n in ('Grizzly Bears','Darksteel Relic','Lightning Bolt'):
            o=copy.deepcopy(self.o);o[n]['oracle_text']+='\nDraw a card.'
            with self.assertRaises(ValueError):f.validate_witnesses(o,self.rows)
        rows=copy.deepcopy(self.rows);rows['Enlisted Wurm']['handler']='card_dummy'
        with self.assertRaises(ValueError):f.validate_witnesses(self.o,rows)
        self.assertEqual(f.pins()['witnesses']['Grizzly Bears']['mana_cost'],'{1}{G}')
    def test_source_payment_leaves_zero_and_free_positive_MV_is_real_library_card(self):
        for n in f.pins()['sources']:
            for who in (0,1):
                c=self.case(n,'accept_'+str(who));s=c['steps'];cost=f.classify(self.o[n])['mana']
                self.assertEqual([x['value'] for x in s[:14] if x['op']=='set_mana' and x['player']==who],cost)
                self.assertEqual(self.final(c,'assert_mana_total',player=who),0)
                self.assertEqual(sum(x['name']=='Grizzly Bears' and x['player']==who and x['zone']=='battlefield' for x in c['setup']),1)
                self.assertEqual(next(x['name'] for x in c['setup'] if x['ref']=='cascade_library_0'),'Grizzly Bears')
                self.assertFalse(any(x['op'] in ('put_into_play','set_counter','add_counter','set_storm','set_history','free_cast') for x in s))
    def test_real_target_before_dialog_before_cast_and_no_unoffered_free_target(self):
        for n in f.pins()['sources']:
            c=self.case(n);s=c['steps'];i=next(i for i,x in enumerate(s) if x['op']=='cast')
            self.assertEqual(s[i-1],dict(op='choose_dialog',player=0,value=0))
            targets=[x for x in s[:i] if x['op']=='choose_target']
            self.assertEqual(targets,[dict(op='choose_target',player=0,ref='enemy')] if f.classify(self.o[n])['target'] else [])
    def test_nested_child_receipt_after_real_cast_before_original_announcement(self):
        c=self.case();s=c['steps'];cast=next(i for i,x in enumerate(s) if x['op']=='cast');bind=next(i for i,x in enumerate(s) if x['op']=='bind_permanent_copy');announce=next(i for i,x in enumerate(s) if x['op']=='announce_spell');resolve=next(i for i,x in enumerate(s) if x['op']=='resolve')
        self.assertLess(cast,bind);self.assertLess(bind,announce);self.assertLess(announce,resolve)
        self.assertIn(dict(op='require_card',ref='source',property='in_hand',value=0),s[cast:bind])
        self.assertIn(dict(op='assert_stack',value=1),s[bind:announce])
        self.assertIn(dict(op='assert_card',ref='free',property='in_play',value=1),s[bind:announce])
        self.assertFalse(any(x['op']=='resolve_response' for x in s))
    def test_pending_original_keeps_only_physical_slot_padding_not_logical_hand(self):
        c=self.case();s=c['steps'];a=next(i for i,x in enumerate(s) if x['op']=='cast');b=next(i for i,x in enumerate(s) if x['op']=='resolve')
        self.assertIn(dict(op='assert_zone',player=0,zone='hand',value=5),s[a:b])
        self.assertIn(dict(op='assert_zone_multiplicity',player=0,zone='hand',index=self.rows['Enlisted Wurm']['csvid'],value=1),s[a:b])
        self.assertEqual(self.final(c,'assert_zone_multiplicity',player=0,zone='hand',index=self.rows['Enlisted Wurm']['csvid']),0)
    def test_free_card_exits_library_without_changing_baseline_exile_or_other_library(self):
        c=self.case();self.assertEqual(self.final(c,'assert_zone',player=0,zone='library'),21);self.assertEqual(self.final(c,'assert_zone',player=1,zone='library'),49)
        for who in (0,1):self.assertEqual(self.final(c,'assert_zone',player=who,zone='exile'),2)
        self.assertEqual(self.final(c,'assert_zone_multiplicity',player=0,zone='battlefield',index=self.rows['Grizzly Bears']['csvid']),2)
    def test_real_primary_damage_life_and_bounce_ledger(self):
        c=self.case('Bituminous Blast');self.assertEqual(self.final(c,'assert_zone_multiplicity',player=1,zone='battlefield',index=self.rows['Grizzly Bears']['csvid']),0);self.assertEqual(self.final(c,'assert_zone_multiplicity',player=1,zone='graveyard',index=self.rows['Grizzly Bears']['csvid']),2)
        c=self.case('Captured Sunlight','accept_1');self.assertEqual(self.final(c,'assert_life',player=1),24)
        c=self.case('Deny Reality');self.assertEqual(self.final(c,'assert_zone',player=1,zone='hand'),5);self.assertEqual(self.final(c,'assert_zone_multiplicity',player=1,zone='hand',index=self.rows['Grizzly Bears']['csvid']),2)
    def test_haste_actual_attack_does_not_grant_haste_to_free_creature(self):
        c=self.case('Bloodbraid Elf');s=c['steps'];self.assertIn(dict(op='attack',ref='free',value=0),s);self.assertIn(dict(op='attack',ref='source',value=1),s);self.assertEqual(self.final(c,'assert_life',player=1),17)
    def test_exalted_actual_solo_attack_and_cleanup_receipts(self):
        c=self.case('Ardent Plea');s=c['steps'];self.assertIn(dict(op='assert_card',ref='idle',property='power',value=3),s);self.assertIn(dict(op='assert_card',ref='idle',property='toughness',value=3),s);self.assertEqual(self.final(c,'assert_card',ref='idle',property='power'),2);self.assertEqual(self.final(c,'assert_life',player=1),17)
    def test_team_pump_includes_nested_creature_excludes_enemy_and_expires(self):
        c=self.case('Violent Outburst');s=c['steps']
        for ref in ('idle','free'):self.assertIn(dict(op='assert_card',ref=ref,property='power',value=3),s);self.assertEqual(self.final(c,'assert_card',ref=ref,property='power'),2)
        self.assertFalse(any(x.get('ref')=='enemy' and x.get('property')=='power' and x['value']!=2 for x in s))
    def test_cannot_block_real_denial_and_real_EOT_turn_untap_legal_block(self):
        c=self.case('Demonic Dread');s=c['steps'];first=next(i for i,x in enumerate(s) if x['op']=='block');last=next(i for i,x in enumerate(s[first+1:],first+1) if x['op']=='block')
        self.assertEqual(s[first]['value'],0);self.assertEqual(s[last]['value'],1);self.assertTrue(any(x['op']=='cleanup' for x in s[first:last]));self.assertEqual(sum(x['op']=='advance_turn' for x in s[first:last]),2);self.assertEqual(sum(x['op']=='untap_step' for x in s[first:last]),2)
        self.assertEqual(self.final(c,'assert_zone_multiplicity',player=0,zone='graveyard',index=self.rows['Grizzly Bears']['csvid']),2);self.assertEqual(self.final(c,'assert_life',player=1),18)
    def test_ready_replay_both_controllers_full_Oracle_and_gaps_not_silent_claims(self):
        count=0
        for n in f.pins()['sources']:
            for who in (0,1):
                c=self.case(n,'accept_'+str(who));count+=1;s=c['steps'];a=next(i for i,x in enumerate(s) if x['op']=='capture');b=next(i for i,x in enumerate(s) if x['op']=='record_state');r=next(i for i,x in enumerate(s) if x['op']=='restore')
                self.assertEqual(s[a+1:b],s[r+1:-1]);self.assertEqual(c['_recipe']['oracle_text'],self.o[n]['oracle_text']);self.assertEqual(c['_recipe']['family'],f.FAMILY);self.assertLess(len(s),10000)
                self.assertTrue(any('random-bottom' in x for x in c['_recipe']['untested_components']))
        self.assertEqual(count,18)
    def test_strict_threshold_has_independent_equal_higher_and_exact_lower_witnesses(self):
        for n in f.pins()['sources']:
            mv=self.o[n]['cmc'];free=f.BOUNDARY[int(mv)];self.assertEqual(self.o[free]['cmc'],mv-1);self.assertGreater(self.o['Enormous Baloth']['cmc'],mv)
            c=self.case(n,'scan_equal_0');lib=[x['name'] for x in c['setup'] if x['player']==0 and x['zone']=='library'];self.assertEqual(lib[:2],[n,'Grizzly Bears'])
            c=self.case(n,'lower_boundary_1');self.assertIn(dict(op='bind_permanent_copy',ref='free',player=1,value=self.rows[free]['csvid'],index=int(free=='Grizzly Bears')),c['steps'])
    def test_zero_MV_card_is_eligible_and_no_mana_is_required(self):
        c=self.case(v='zero_cost_0');self.assertEqual(self.o['Memnite']['cmc'],0);self.assertIn(dict(op='assert_card',ref='free',property='power',value=1),c['steps']);self.assertEqual(self.final(c,'assert_mana_total',player=0),0)
    def test_decline_offered_to_both_actors_and_returns_qualifier_to_bottom(self):
        for who in (0,1):
            c=self.case(v=f'decline_{who}');self.assertIn(dict(op='choose_dialog',player=who,value=1),c['steps']);self.assertFalse(any(x['op']=='bind_permanent_copy' for x in c['steps']));self.assertEqual(self.final(c,'assert_zone',player=who,zone='library'),22)
            self.assertEqual(self.final(c,'assert_zone_card',player=who,zone='library',index=21),self.rows['Grizzly Bears']['csvid']);self.assertEqual(self.final(c,'assert_zone_multiplicity',player=who,zone='battlefield',index=self.rows['Grizzly Bears']['csvid']),1)
    def test_random_bottom_checks_complete_contents_and_unchanged_ordered_suffix(self):
        for mode,size in [('scan_land',23),('scan_equal',22),('scan_higher',22),('scan_mixed',25),('decline_mixed',26)]:
            c=self.case(v=mode+'_0');self.assertEqual(self.final(c,'assert_zone',player=0,zone='library'),size);self.assertEqual(self.final(c,'assert_zone',player=0,zone='exile'),2)
            order=[x for x in c['steps'] if x['op']=='assert_zone_card' and x['player']==0 and x['zone']=='library'];self.assertTrue(order);self.assertEqual({x['index'] for x in order},set(range(22 if size==22 else 21)))
            self.assertTrue(any(x['op']=='assert_zone_multiplicity' and x['player']==0 and x['zone']=='library' and x['index']==self.rows['Enormous Baloth']['csvid'] for x in c['steps']) if 'mixed' in mode or mode=='scan_higher' else True)
    def test_empty_and_no_eligible_have_no_dialog_and_preserve_correct_return_receipts(self):
        for mode,size in [('empty_library',0),('no_eligible',5)]:
            c=self.case(v=mode+'_0');self.assertFalse(any(x['op'] in ('choose_dialog','bind_permanent_copy') for x in c['steps']));self.assertEqual(self.final(c,'assert_zone',player=0,zone='library'),size);self.assertEqual(self.final(c,'assert_zone',player=0,zone='exile'),2);self.assertTrue(all(x['value']==0 for x in c['steps'] if x['op']=='assert_unused_choices'))
            if size:self.assertEqual(self.final(c,'assert_zone_multiplicity',player=0,zone='library',index=self.rows['Enormous Baloth']['csvid']),5)
    def test_last_card_qualifier_uses_ordinary_retired_intent_and_independent_child_gate(self):
        c=self.case(v='last_eligible_0');s=c['steps'];i=next(i for i,x in enumerate(s) if x['op']=='cast');receipt=next(i for i,x in enumerate(s[i+1:],i+1) if x['op']=='assert_unused_choices');bind=next(i for i,x in enumerate(s) if x['op']=='bind_permanent_copy')
        self.assertEqual(s[i-1],dict(op='choose_dialog',player=0,value=0));self.assertEqual(s[receipt]['value'],0);self.assertLess(receipt,bind);self.assertEqual(self.final(c,'assert_zone',player=0,zone='library'),4);self.assertEqual(self.final(c,'assert_zone',player=0,zone='exile'),2)
        self.assertEqual([x['name'] for x in c['setup'] if x['zone']=='library' and x['player']==0][-1],'Grizzly Bears')
        self.assertEqual(self.final(self.case(v='one_card_0'),'assert_zone',player=0,zone='library'),0)
    def test_countered_original_keeps_real_Cascade_child_and_cancels_primary(self):
        for n in f.pins()['sources']:
            c=self.case(n,'countered_parent_0');s=c['steps'];bind=next(i for i,x in enumerate(s) if x['op']=='bind_permanent_copy');response=next(i for i,x in enumerate(s) if x['op']=='cast_response')
            self.assertLess(bind,response);self.assertTrue(any(x['op']=='resolve_response' for x in s));self.assertFalse(any(x['op']=='resolve' for x in s));self.assertEqual(self.final(c,'assert_zone_multiplicity',player=0,zone='graveyard',index=self.rows[n]['csvid']),1);self.assertEqual(self.final(c,'assert_life',player=0),20);self.assertEqual(self.final(c,'assert_life',player=1),20);self.assertEqual(self.final(c,'assert_zone_multiplicity',player=0,zone='battlefield',index=self.rows['Grizzly Bears']['csvid']),2)
            self.assertEqual([x for x in s[:response] if x['op']=='choose_target'],[dict(op='choose_target',player=0,ref='enemy')] if f.classify(self.o[n])['target'] else [])
    def test_real_source_denials_do_not_queue_nested_input_or_move_library(self):
        for n in f.pins()['sources']:
            for mode in ('source_insufficient_mana','source_wrong_color'):
                c=self.case(n,mode+'_0');self.assertIn(dict(op='cast',ref='source',value=0),c['steps']);self.assertFalse(any(x['op'] in ('choose_dialog','choose_target','announce_spell','resolve') for x in c['steps']));self.assertEqual(self.final(c,'assert_zone',player=0,zone='library'),22);self.assertEqual(self.final(c,'assert_zone_multiplicity',player=0,zone='hand',index=self.rows[n]['csvid']),1)
    def test_original_target_loss_does_not_undo_free_cast(self):
        for n in ('Bituminous Blast','Demonic Dread','Deny Reality'):
            c=self.case(n,'target_removed_0');s=c['steps'];destroy=next(i for i,x in enumerate(s) if x['op']=='destroy');resolve=next(i for i,x in enumerate(s) if x['op']=='resolve');self.assertLess(destroy,resolve);self.assertEqual(self.final(c,'assert_zone_multiplicity',player=0,zone='battlefield',index=self.rows['Grizzly Bears']['csvid']),2);self.assertEqual(self.final(c,'assert_zone_multiplicity',player=1,zone='graveyard',index=self.rows['Grizzly Bears']['csvid']),2)
    def test_no_legal_original_target_prevents_cast_Cascade_and_payment(self):
        for n in ('Bituminous Blast','Demonic Dread','Deny Reality'):
            c=self.case(n,'no_legal_target_1');self.assertFalse(any(x['op'] in ('choose_target','choose_dialog','resolve') for x in c['steps']));self.assertIn(dict(op='cast',ref='source',value=0),c['steps']);self.assertEqual(self.final(c,'assert_zone',player=1,zone='battlefield'),0);self.assertEqual(self.final(c,'assert_zone',player=1,zone='library'),22)
    def test_any_controller_target_scope_including_real_own_block_denial(self):
        for n in ('Bituminous Blast','Demonic Dread','Deny Reality'):
            c=self.case(n,'own_target_0');self.assertIn(dict(op='choose_target',player=0,ref='idle'),c['steps'])
        c=self.case('Demonic Dread','own_target_0');self.assertIn(dict(op='block',ref='idle',other='enemy',value=0),c['steps']);self.assertEqual(self.final(c,'assert_life',player=0),18);self.assertEqual(self.final(c,'assert_life',player=1),20)
    def test_Deny_Reality_all_permanent_categories_return_to_real_owner_hand(self):
        for mode,n in [('artifact_target','Darksteel Relic'),('land_target','Forest'),('enchantment_target','Fervor'),('planeswalker_target','Jace Beleren')]:
            c=self.case('Deny Reality',mode+'_0');w=0 if mode=='artifact_target' else 1;self.assertEqual(self.final(c,'assert_zone_multiplicity',player=w,zone='hand',index=self.rows[n]['csvid']),1);self.assertEqual(self.final(c,'assert_zone_multiplicity',player=w,zone='battlefield',index=self.rows[n]['csvid']),0)
    def test_Exalted_real_two_attacker_opponent_and_removed_source_negatives(self):
        for mode,own,enemy in [('two_attackers',20,16),('opponent_attack',18,20),('source_removed',20,18)]:
            c=self.case('Ardent Plea',mode+'_0');self.assertEqual(self.final(c,'assert_life',player=0),own);self.assertEqual(self.final(c,'assert_life',player=1),enemy);self.assertFalse(any(x.get('ref')=='idle' and x.get('property')=='power' and x['value']==3 for x in c['steps']))
    def test_late_own_creature_avoids_already_resolved_team_pump(self):
        c=self.case('Violent Outburst','late_creature_0');s=c['steps'];self.assertIn(dict(op='assert_card',ref='late',property='power',value=2),s);self.assertFalse(any(x.get('ref')=='late' and x.get('property')=='power' and x['value']==3 for x in s));self.assertIn(dict(op='assert_card',ref='free',property='power',value=3),s);self.assertIn(dict(op='cast',ref='late',value=1),s)
    def test_exact_broad_population_is_unique_with_accepted16_and_new274(self):
        count=0
        for n in f.pins()['sources']:
            vs=f.variants(f.classify(self.o[n]));self.assertEqual(len(vs),len(set(vs)));self.assertTrue({'accept_0','accept_1'}<=set(vs));count+=len(vs)
        self.assertEqual(sum(len(f.variants(f.classify(self.o[n]))) for n in f.pins()['sources'] if n!='Shardless Agent'),290)
        self.assertEqual(count,322)
    def test_exact_Shardless_follow_on_is_whole_Cascade_artifact_creature_real_cost(self):
        n='Shardless Agent';r=self.o[n];p=f.classify(r)
        self.assertEqual(r['oracle_text'],f.CASCADE);self.assertEqual(r['type_line'],'Artifact Creature — Human Rogue');self.assertEqual((r['power'],r['toughness']),('2','2'));self.assertEqual(r['mana_cost'],'{1}{G}{U}')
        self.assertEqual(self.rows[n]['handler'],self.rows['Enlisted Wurm']['handler']);self.assertEqual(self.rows[n]['kind'],'card');self.assertEqual(p['effect'],'vanilla');self.assertEqual(p['mana'],[1,0,1,1,0,0]);self.assertEqual(len(f.variants(p)),32)
        for who in (0,1):
            c=self.case(n,'accept_'+str(who));self.assertEqual(self.final(c,'assert_card',ref='source',property='power'),2);self.assertEqual(self.final(c,'assert_card',ref='source',property='toughness'),2);self.assertEqual(self.final(c,'assert_mana_total',player=who),0);self.assertEqual(c['_recipe']['oracle_text'],r['oracle_text'])

if __name__=='__main__':unittest.main()
