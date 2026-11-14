import copy,json,unittest
from pathlib import Path
from generate_cases import registry
import storm_effect_recipes as f

class StormEffectTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.o=json.loads((Path(__file__).parent/'data/oracle.json').read_text(encoding='utf-8'))['cards'];cls.rows={r['name']:r for r in registry()}
    def declaration(self,n,v='effect_0'):
        return dict(family=f.FAMILY,version=2,oracle_text=self.o[n]['oracle_text'],params=f.parameters(f.classify(self.o[n])),variants=[v])
    def case(self,n='Brain Freeze',v='effect_0'):
        d=self.declaration(n,v);return f.expand(n,self.rows[n]['csvid'],d,f.spec(d,self.o[n]),[v],self.rows,self.o)[0]
    def final(self,c,op,**kw):return next(s['value'] for s in reversed(c['steps']) if s['op']==op and all(s.get(k)==v for k,v in kw.items()))
    def multiplicity(self,c,n,w,z):return self.final(c,'assert_zone_multiplicity',player=w,zone=z,index=self.rows[n]['csvid'])
    def library_delta(self,c,w):return sum(x['zone']=='library' and x['player']==w for x in c['setup'])-self.final(c,'assert_zone',player=w,zone='library')
    def test_full_source_identity_costs_all_clauses_and_typed_effects(self):
        f.validate_witnesses(self.o,self.rows)
        for n in f.pins()['sources']:
            d=self.declaration(n)
            for field,value in [('amount',True),('scope','any'),('copy_count',10)]:
                changed=copy.deepcopy(d);changed['params'][field]=value
                if f.exact(changed,d):continue
                with self.assertRaises(ValueError):f.spec(changed,self.o[n])
            for field,value in [('mana_cost','{0}'),('type_line','Enchantment'),('layout','transform'),('oracle_text',self.o[n]['oracle_text']+'\nDraw a card.')]:
                changed=dict(self.o[n]);changed[field]=value
                with self.assertRaises(ValueError):f.spec(d,changed)
            changed=dict(self.o[n]);changed['oracle_text']+='\nBuyback {2}';changed['keywords']=changed['keywords']+['Buyback']
            with self.assertRaises(ValueError):f.classify(changed)
        rows=copy.deepcopy(self.rows);rows['Dragonstorm']['handler']='different'
        with self.assertRaises(ValueError):f.validate_witnesses(self.o,rows)
        rows=copy.deepcopy(self.rows);rows['Dragonstorm']['kind']='internal'
        with self.assertRaises(ValueError):f.validate_witnesses(self.o,rows)
    def test_all_primary_effects_derive_from_wording_without_source_name_dispatch(self):
        for n in f.pins()['sources']:
            a=self.o[n];b=dict(a,name='An independent source name')
            self.assertEqual(f.parameters(f.classify(a)),f.parameters(f.classify(b)))
        for n in ('Haze of Rage',"Mind's Desire"):
            with self.assertRaises(ValueError):f.classify(self.o[n])
        self.assertEqual(f.classify(self.o['Flusterstorm'])['scope'],'instant_or_sorcery')
        self.assertEqual(f.classify(self.o['Hindering Touch'])['amount'],2)
    def test_zero_one_two_three_real_predecessors_give_n_plus_one_mills(self):
        for owner in (0,1):
            for mode,n in [('no_prior',0),('effect',1),('prefix_two',2),('prefix_three',3)]:
                c=self.case(v=f'{mode}_{owner}');self.assertEqual(c['_recipe']['prior_real_spells'],n)
                self.assertEqual(self.library_delta(c,1-owner),3*(n+1))
                self.assertEqual(sum(s['op']=='announce_spell' for s in c['steps']),n+1)
                self.assertEqual(self.final(c,'assert_life',player=owner),21+n)
                self.assertEqual(self.final(c,'assert_mana_total',player=owner),178-n)
    def test_opposing_and_split_history_are_real_legal_instants_not_turn_setters(self):
        for owner in (0,1):
            c=self.case(v=f'opponent_history_{owner}');prefix=[x for x in c['setup'] if x['ref'].startswith('prefix_')]
            self.assertTrue(all(x['name']=='Reach Through Mists' and x['player']==1-owner for x in prefix))
            self.assertEqual(sum(s['op']=='turn' for s in c['steps']),1)
            self.assertEqual(c['_recipe']['prior_real_spells'],2);self.assertEqual(self.library_delta(c,1-owner),11)
            self.assertEqual(self.final(c,'assert_life',player=owner),21)
            c=self.case(v=f'split_history_{owner}');self.assertEqual(c['_recipe']['prior_real_spells'],2)
            self.assertEqual(self.final(c,'assert_life',player=owner),22)
    def test_countered_predecessor_and_counterspell_both_count(self):
        for owner in (0,1):
            c=self.case(v=f'countered_predecessor_{owner}');self.assertEqual(c['_recipe']['prior_real_spells'],2)
            self.assertEqual(self.library_delta(c,1-owner),9);self.assertEqual(self.library_delta(c,owner),0)
            s=c['steps'];self.assertLess(next(i for i,x in enumerate(s) if x['op']=='announce_spell'),next(i for i,x in enumerate(s) if x['op']=='cast_response'))
            self.assertIn(dict(op='resolve_response'),s)
    def test_real_turn_reset_drops_old_history_but_preserves_watcher_life(self):
        c=self.case(v='previous_turn_0');self.assertEqual(sum(s['op']=='advance_turn' for s in c['steps']),2)
        self.assertEqual(c['_recipe']['prior_real_spells'],0);self.assertEqual(self.library_delta(c,1),3)
        self.assertEqual(self.final(c,'assert_life',player=0),22)
    def test_activation_does_not_count_as_cast_or_charge_for_copies(self):
        c=self.case(v='activation_not_cast_0');self.assertIn(dict(op='activate',ref='mana_control',value=1),c['steps'])
        self.assertEqual(c['_recipe']['prior_real_spells'],0);self.assertEqual(self.library_delta(c,1),3)
        self.assertEqual(self.final(c,'assert_life',player=0),21)
        self.assertEqual(self.final(c,'assert_mana_total',player=0),180)
    def test_exact_source_payment_plus_real_prefix_costs_not_copy_costs(self):
        for n in f.pins()['sources']:
            for owner in (0,1):
                c=self.case(n,f'payment_{owner}');self.assertEqual(self.final(c,'assert_mana_total',player=owner),0)
                for mode in ('insufficient_mana','wrong_color'):
                    c=self.case(n,f'{mode}_{owner}')
                    self.assertIn(dict(op='cast_response' if f.classify(self.o[n])['kind']=='tax' else 'cast',ref='source',value=0),c['steps'])
                    source_cast=next(i for i,s in enumerate(c['steps']) if s['op'] in ('cast','cast_response') and s.get('ref')=='source')
                    self.assertFalse(any(s['op'].startswith('choose_') for s in c['steps'][source_cast-1:]))
    def test_source_and_ephemeral_copies_have_correct_card_destinations(self):
        for n in f.pins()['sources']:
            c=self.case(n,'prefix_three_0');before=sum(x['name']==n and x['zone']=='graveyard' and x['player']==0 for x in c['setup'])
            self.assertEqual(self.multiplicity(c,n,0,'graveyard'),before+1)
            self.assertEqual(self.multiplicity(c,n,0,'hand'),0)
    def test_mill_self_split_short_and_empty_libraries_are_bounded(self):
        c=self.case(v='self_target_0');self.assertEqual(self.library_delta(c,0),6);self.assertEqual(self.library_delta(c,1),0)
        c=self.case(v='split_targets_0');self.assertEqual(self.library_delta(c,0),3);self.assertEqual(self.library_delta(c,1),3)
        for mode,n in [('short_library',2),('empty_target_library',0)]:
            c=self.case(v=mode+'_0');self.assertEqual(self.library_delta(c,1),n);self.assertEqual(self.final(c,'assert_zone',player=1,zone='library'),0)
    def test_tutors_have_independent_destinations_and_shuffles(self):
        for owner in (0,1):
            c=self.case('Dragonstorm',f'prefix_two_{owner}');self.assertEqual(self.final(c,'assert_shuffle_count',player=owner),3)
            self.assertEqual(self.final(c,'assert_zone',player=owner,zone='battlefield'),6)
            self.assertEqual(sum(s['op']=='bind_permanent_copy' for s in c['steps']),3)
            self.assertEqual(sum(s['op']=='choose_zone_card' for s in c['steps']),3)
            c=self.case('Sprouting Vines',f'prefix_two_{owner}');self.assertEqual(self.final(c,'assert_shuffle_count',player=owner),3)
            before=sum(x['name']=='Forest' and x['zone']=='hand' and x['player']==owner for x in c['setup'])
            self.assertEqual(self.multiplicity(c,'Forest',owner,'hand'),before+3)
    def test_tutor_eligibility_distinguishes_type_subtype_basic_and_changeling(self):
        dragon=f.classify(self.o['Dragonstorm']);basic=f.classify(self.o['Sprouting Vines'])
        self.assertTrue(f.eligible(self.o['Volcanic Dragon'],dragon));self.assertFalse(f.eligible(self.o['Crib Swap'],dragon))
        self.assertTrue(f.eligible(self.o['Forest'],basic));self.assertFalse(f.eligible(self.o['Sol Ring'],basic))
        for n in ('Dragonstorm','Sprouting Vines'):
            c=self.case(n);self.assertIn(dict(op='observe_search_filters',value=1),c['steps'])
            self.assertTrue(any(s['op']=='assert_offered_legal_card' and s['value']==0 for s in c['steps']))
    def test_failed_or_declined_searches_keep_mandatory_resolution_callbacks(self):
        for n in ('Dragonstorm','Sprouting Vines'):
            for mode in ('no_match','empty_library','fail_to_find'):
                c=self.case(n,mode+'_0');self.assertEqual(sum(s['op']=='choose_optional_zone' for s in c['steps']),2)
                self.assertEqual(self.final(c,'assert_life',player=0),22)
                self.assertIn(dict(op='assert_unused_choices',value=0),c['steps'])
    def test_reaping_copy_grave_targets_are_selected_at_copy_cast_and_stay_distinct(self):
        c=self.case('Reaping the Graves','prefix_two_0');choices=[s['value'] for s in c['steps'] if s['op']=='choose_zone_card']
        self.assertEqual(choices,[self.rows[n]['csvid'] for n in f.GRAVE_TARGETS[:3]])
        for n in f.GRAVE_TARGETS[:3]:
            self.assertEqual(self.multiplicity(c,n,0,'graveyard'),0)
            before=sum(x['name']==n and x['zone']=='hand' and x['player']==0 for x in c['setup'])
            self.assertEqual(self.multiplicity(c,n,0,'hand'),before+1)
        for mode in ('empty_graveyard','wrong_graveyard','wrong_type'):
            c=self.case('Reaping the Graves',mode+'_0');self.assertFalse(any(s['op']=='choose_zone_card' for s in c['steps']))
    def test_distinct_land_copy_targets_and_owner_permissions(self):
        for owner in (0,1):
            c=self.case('Volcanic Awakening',f'prefix_two_{owner}');self.assertEqual([s['ref'] for s in c['steps'] if s['op']=='choose_target'],['target_0','target_1','target_2'])
            before=sum(x['name']=='Forest' and x['zone']=='graveyard' and x['player']==1-owner for x in c['setup'])
            self.assertEqual(self.multiplicity(c,'Forest',1-owner,'graveyard'),before+3)
            c=self.case('Volcanic Awakening',f'own_targets_{owner}');self.assertEqual(self.multiplicity(c,'Forest',owner,'battlefield'),3)
    def test_countering_original_retains_prior_copied_effects(self):
        for owner in (0,1):
            c=self.case(v=f'countered_original_{owner}');self.assertEqual(self.library_delta(c,1-owner),3)
            c=self.case('Reaping the Graves',f'countered_original_{owner}');self.assertEqual(self.multiplicity(c,'Grizzly Bears',owner,'graveyard'),1);self.assertEqual(self.multiplicity(c,'Wind Drake',owner,'graveyard'),0)
            c=self.case('Crow Storm',f'countered_original_{owner}');self.assertEqual(self.multiplicity(c,'Storm Crow',owner,'battlefield'),1)
    def test_ground_rift_actual_block_rejection_and_real_turn_expiry(self):
        for owner in (0,1):
            c=self.case('Ground Rift',f'effect_{owner}');self.assertEqual(sum(s['op']=='block' and s['value']==0 for s in c['steps']),2)
            self.assertEqual(self.final(c,'assert_life',player=1-owner),18)
            c=self.case('Ground Rift',f'expiry_{owner}');self.assertEqual(sum(s['op']=='advance_turn' for s in c['steps']),2)
            self.assertIn(dict(op='untap_step',player=owner),c['steps']);self.assertTrue(any(s['op']=='block' and s['value']==1 for s in c['steps']))
            c=self.case('Ground Rift',f'countered_original_{owner}');self.assertIn(dict(op='block',ref='target_0',other='attack_probe',value=1),c['steps'])
            self.assertEqual(self.final(c,'assert_life',player=1-owner),20)
    def test_tax_parent_is_a_real_previous_cast_and_copies_are_separate_taxes(self):
        for owner in (0,1):
            paid=self.case('Flusterstorm',f'tax_paid_{owner}');partial=self.case('Flusterstorm',f'tax_partial_{owner}');other=1-owner
            self.assertEqual(paid['_recipe']['prior_real_spells'],2)
            self.assertEqual(self.final(paid,'assert_mana_total',player=other),0);self.assertEqual(self.library_delta(paid,other),1)
            self.assertEqual(self.final(partial,'assert_mana_total',player=other),0);self.assertEqual(self.library_delta(partial,other),0)
            if other==0:
                self.assertEqual(sum(s['op']=='choose_dialog' for s in paid['steps']),3)
                self.assertEqual(sum(s['op']=='choose_dialog' for s in partial['steps']),1)
            first_parent=next(i for i,s in enumerate(paid['steps']) if s['op']=='announce_spell' and s['ref']=='parent')
            storm_cast=next(i for i,s in enumerate(paid['steps']) if s['op']=='cast_response' and s['ref']=='source')
            self.assertLess(first_parent,storm_cast)
    def test_tax_forbidden_parent_and_no_parent_do_not_inject_choice_inputs(self):
        for owner in (0,1):
            for mode in ('forbidden_parent','no_parent'):
                c=self.case('Flusterstorm',f'{mode}_{owner}');self.assertFalse(any(s['op']=='choose_dialog' for s in c['steps']))
                self.assertEqual(self.final(c,'assert_card',ref='source',property='in_hand'),1)
    def test_token_witness_is_explicit_token_identity_and_created_stats_from_source(self):
        self.assertEqual(self.o['Storm Crow']['layout'],'token');self.assertEqual(f.pins()['witnesses']['Storm Crow'],f.pin(self.o['Storm Crow']))
        c=self.case('Crow Storm','prefix_three_0');self.assertEqual(self.multiplicity(c,'Storm Crow',0,'battlefield'),4)
        self.assertEqual(self.final(c,'assert_matching_permanents',player=0,index=self.rows['Storm Crow']['csvid'],property='token'),1)
        self.assertEqual(self.final(c,'assert_matching_permanents',player=0,index=self.rows['Storm Crow']['csvid'],property='subtype_Bird'),1)
        self.assertEqual(self.final(c,'assert_life',player=0),24)
    def test_token_flying_sickness_and_zero_mana_value_use_real_actions(self):
        for owner in (0,1):
            c=self.case('Crow Storm',f'token_flying_block_{owner}');self.assertIn(dict(op='block',ref='token_0',other='flying_attacker',value=1),c['steps'])
            self.assertEqual(self.final(c,'assert_card',ref='token_0',property='damage'),1)
            c=self.case('Crow Storm',f'token_fresh_attack_{owner}');self.assertIn(dict(op='attack',ref='token_0',value=0),c['steps'])
            c=self.case('Crow Storm',f'token_mana_value_zero_{owner}');self.assertIn(dict(op='choose_x',player=owner,value=0),c['steps'])
            self.assertEqual(self.multiplicity(c,'Storm Crow',owner,'battlefield'),0);self.assertEqual(self.final(c,'assert_life',player=owner),23)
    def test_preannouncement_boundary_does_not_expect_early_storm_effects(self):
        c=self.case('Crow Storm');s=c['steps'];i=next(i for i,x in enumerate(s) if x['op']=='cast' and x.get('ref')=='source');j=next(i for i,x in enumerate(s) if x['op']=='announce_spell' and x.get('ref')=='source')
        boundary=[x for x in s[i:j] if x['op']=='assert_zone' and x['zone']=='battlefield' and x['player']==0]
        self.assertEqual(boundary[-1]['value'],2)
    def test_pending_original_legacy_hand_slot_does_not_change_in_hand_or_final_zones(self):
        for owner in (0,1):
            c=self.case(v=f'prefix_two_{owner}');s=c['steps'];announced=next(i for i,x in enumerate(s) if x['op']=='announce_spell' and x.get('ref')=='source')
            resolve=next(i for i in range(announced+1,len(s)) if s[i]['op']=='resolve');pending=s[announced+1:resolve]
            initial=sum(x['zone']=='hand' and x['player']==owner for x in c['setup'])
            self.assertEqual(next(x['value'] for x in pending if x['op']=='assert_zone' and x['zone']=='hand' and x['player']==owner),initial-2)
            self.assertEqual(next(x['value'] for x in pending if x['op']=='assert_zone_multiplicity' and x['zone']=='hand' and x['player']==owner and x['index']==self.rows['Brain Freeze']['csvid']),1)
            self.assertIn(dict(op='require_card',ref='source',property='in_hand',value=0),s)
            self.assertEqual(self.multiplicity(c,'Brain Freeze',owner,'hand'),0)
            self.assertEqual(self.multiplicity(c,'Brain Freeze',owner,'graveyard'),1)
    def test_pending_tax_parent_slot_exists_only_while_that_spell_remains_pending(self):
        for owner in (0,1):
            for mode,parent_slot in [('tax_paid',1),('tax_partial',0)]:
                c=self.case('Flusterstorm',f'{mode}_{owner}');s=c['steps'];a=next(i for i,x in enumerate(s) if x['op']=='announce_spell' and x.get('ref')=='source')
                r=next(i for i in range(a+1,len(s)) if s[i]['op'] in ('resolve','resolve_response'));mid=s[a+1:r]
                for who,name,expected in [(owner,'Flusterstorm',1),(1-owner,'Reach Through Mists',parent_slot)]:
                    self.assertEqual(next(x['value'] for x in mid if x['op']=='assert_zone_multiplicity' and x['zone']=='hand' and x['player']==who and x['index']==self.rows[name]['csvid']),expected)
                self.assertIn(dict(op='require_card',ref='parent',property='in_hand',value=0),s)
                self.assertEqual(self.multiplicity(c,'Flusterstorm',owner,'hand'),0)
                self.assertEqual(self.multiplicity(c,'Reach Through Mists',1-owner,'hand'),0)
    def test_paid_tax_resolution_restores_parent_context_only_when_Oracle_parent_survives(self):
        for owner in (0,1):
            for mode in ('tax_paid','tax_exact','tax_partial','tax_zero_budget'):
                c=self.case('Flusterstorm',f'{mode}_{owner}');s=c['steps']
                a=next(i for i,x in enumerate(s) if x['op']=='announce_spell' and x.get('ref')=='source')
                resolution=next(x['op'] for x in s[a+1:] if x['op'] in ('resolve','resolve_response'))
                self.assertEqual(resolution,'resolve_response' if mode in ('tax_paid','tax_exact') else 'resolve')
                if mode in ('tax_paid','tax_exact'):
                    self.assertEqual(next(x['value'] for x in s[a+1:] if x['op']=='assert_stack'),2)
                else:self.assertEqual(next(x['value'] for x in s[a+1:] if x['op']=='assert_stack'),1)
    def test_paid_tax_dialogues_cover_both_scripted_controllers_without_rewriting_tax_costs(self):
        for owner in (0,1):
            for mode in ('tax_paid','tax_exact'):
                c=self.case('Flusterstorm',f'{mode}_{owner}');s=c['steps']
                self.assertEqual([x for x in s if x['op']=='choose_dialog'],[dict(op='choose_dialog',player=1-owner,value=0)]*3)
                a=next(i for i,x in enumerate(s) if x['op']=='announce_spell' and x.get('ref')=='source')
                self.assertEqual(sum(x['op']=='choose_dialog' for x in s[:a]),2)
                self.assertEqual(sum(x['op']=='choose_dialog' for x in s[a+1:]),1)
                self.assertEqual(next(x['value'] for x in s[a+1:] if x['op']=='assert_mana_total' and x['player']==1-owner),1)
                self.assertEqual(self.final(c,'assert_mana_total',player=1-owner),0)
                self.assertEqual(c['_recipe']['params']['amount'],1)
                self.assertEqual(self.library_delta(c,1-owner),1)
    def test_hindering_pin_is_tax2_cast3U_any_spell_without_a_false_tax4(self):
        p=f.classify(self.o['Hindering Touch'])
        self.assertEqual(p['amount'],2);self.assertEqual(p['scope'],'any_spell')
        self.assertEqual(p['mana'],[3,0,1,0,0,0]);self.assertEqual(self.o['Hindering Touch']['cmc'],4)
        self.assertEqual(len(f.variants(p)),42)
        self.assertFalse(any('forbidden_parent' in v for v in f.variants(p)))
    def test_hindering_pilot_has_real_tax2_payments_for_both_payers_and_partial_counter(self):
        for owner in (0,1):
            for mode in ('tax_paid','tax_partial'):
                c=self.case('Hindering Touch',f'{mode}_{owner}');s=c['steps'];paid=mode=='tax_paid'
                self.assertEqual([x for x in s if x['op']=='choose_dialog'],[dict(op='choose_dialog',player=1-owner,value=0)]*(3 if paid else 1))
                self.assertEqual(c['_recipe']['prior_real_spells'],2)
                self.assertEqual(sum(x['op']=='cast' for x in s),2)
                self.assertEqual(sum(x['op']=='cast_response' for x in s),1)
                a=next(i for i,x in enumerate(s) if x['op']=='announce_spell' and x.get('ref')=='source')
                self.assertEqual(next(x['value'] for x in s[a+1:] if x['op']=='assert_mana_total' and x['player']==1-owner),2 if paid else 0)
                self.assertEqual(self.final(c,'assert_mana_total',player=owner),175)
                self.assertEqual(self.final(c,'assert_mana_total',player=1-owner),0)
                self.assertEqual(self.library_delta(c,1-owner),1 if paid else 0)
                self.assertEqual(next(x['op'] for x in s[a+1:] if x['op'] in ('resolve','resolve_response')),'resolve_response' if paid else 'resolve')
    def test_scripted_tax_payer_optin_is_source_pinned_typed_and_not_global(self):
        from unittest.mock import patch
        self.assertEqual(f.pins()['tax_dialog_players'],{'Hindering Touch':[0,1]})
        for name,players in [('Hindering Touch',[True]),('Hindering Touch',[2]),('Hindering Touch',[0,0]),('Brain Freeze',[0]),('Unknown',[0])]:
            data=copy.deepcopy(f.pins());data['tax_dialog_players']={name:players}
            with patch.object(f,'pins',return_value=data):
                with self.assertRaises(ValueError):f.validate_witnesses(self.o,self.rows)
        for owner in (0,1):
            c=self.case('Flusterstorm',f'tax_partial_{owner}')
            self.assertEqual(sum(x['op']=='choose_dialog' for x in c['steps']),int(owner==1))
    def test_expiry_combat_selects_first_main_after_actual_cleanup_turn_and_untap(self):
        for owner in (0,1):
            s=self.case('Ground Rift',f'expiry_{owner}')['steps']
            i=next(i for i,x in enumerate(s) if x['op']=='untap_step')
            self.assertEqual(s[i-2:i],[dict(op='advance_turn'),dict(op='advance_turn')])
            self.assertEqual(s[i],dict(op='untap_step',player=owner))
            self.assertEqual(s[i+1],dict(op='turn',player=owner,value=0))
            self.assertEqual(s[i+2]['op'],'begin_attackers')
            self.assertEqual(sum(x['op']=='advance_turn' for x in s),2)
    def test_snapshots_and_full_supported_cohort_do_not_invent_history_or_windows(self):
        for n in f.pins()['sources']:
            p=f.classify(self.o[n]);d=self.declaration(n);d['variants']=f.variants(p);cases=f.expand(n,self.rows[n]['csvid'],d,f.spec(d,self.o[n]),d['variants'],self.rows,self.o)
            self.assertEqual(len(cases),len({c['name'] for c in cases}))
            for v in ('ready_snapshot_0','pending_snapshot_1','resolved_snapshot_0'):
                c=self.case(n,v);self.assertIn(dict(op='capture'),c['steps']);self.assertIn(dict(op='assert_recorded_state'),c['steps'])
                self.assertFalse(any(s['op'] in ('set_storm','set_history','resolve_one_copy','copy_spell') for s in c['steps']))
        self.assertTrue({'Brain Freeze','Dragonstorm','Flusterstorm','Ground Rift','Reaping the Graves','Sprouting Vines','Volcanic Awakening','Crow Storm'}<=set(f.pins()['sources']))

if __name__=='__main__':unittest.main()
