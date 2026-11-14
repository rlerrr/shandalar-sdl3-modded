import copy,json,unittest
from pathlib import Path
from generate_cases import registry
import cast_temporary_stat_trigger_recipes as f

class CastTemporaryStatTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.o=json.loads((Path(__file__).parent/'data/oracle.json').read_text(encoding='utf8'))['cards'];cls.rows={r['name']:r for r in registry()}
    def declaration(self,n,v):return dict(family=f.FAMILY,version=2,oracle_text=self.o[n]['oracle_text'],params=f.parameters(f.classify(self.o[n])),variants=[v])
    def case(self,n='Dragon-Style Twins',v='effect_0'):
        d=self.declaration(n,v);return f.expand(n,self.rows[n]['csvid'],d,f.spec(d,self.o[n]),[v],self.rows,self.o)[0]
    def final(self,c,op,**kw):return next(s['value'] for s in reversed(c['steps']) if s['op']==op and all(s.get(k)==v for k,v in kw.items()))
    def stat(self,c,k,ref='source'):return self.final(c,'assert_card',ref=ref,property=k)
    def multiplicity(self,c,n,w,z):return self.final(c,'assert_zone_multiplicity',player=w,zone=z,index=self.rows[n]['csvid'])
    def library_delta(self,c,w):return sum(s['zone']=='library' and s['player']==w for s in c['setup'])-self.final(c,'assert_zone',player=w,zone='library')
    def test_whole_text_grammar_admits_pinned_compositions_without_name_dispatch(self):
        f.validate_witnesses(self.o,self.rows)
        self.assertEqual(len(f.pins()['sources']),7)
        for n in f.pins()['sources']:
            r=self.o[n];self.assertEqual(f.parameters(f.classify(r)),f.parameters(f.classify(dict(r,name='Independent creature'))))
            for extra in ('\nFlying','\nWhen this creature enters, draw a card.','\n{1}: Draw a card.'):
                with self.assertRaises(ValueError):f.classify(dict(r,oracle_text=r['oracle_text']+extra))
            for key,value in [('layout','transform'),('toughness','*'),('keywords',[])]:
                if value==r.get(key):continue
                with self.assertRaises(ValueError):f.classify(dict(r,**{key:value}))
    def test_source_and_witness_Oracle_handler_identity_guards(self):
        for n in ('Jeskai Sage','Ingenious Skaab','Darksteel Relic','Visara the Dreadful','Jace Beleren'):
            altered=copy.deepcopy(self.o);altered[n]['oracle_text']+='\nDraw a card.'
            with self.assertRaises(ValueError):f.validate_witnesses(altered,self.rows)
        rows=copy.deepcopy(self.rows);rows['Jeskai Sage']['handler']='card_dummy'
        with self.assertRaises(ValueError):f.validate_witnesses(self.o,rows)
        rows=copy.deepcopy(self.rows);rows['Whirlwind Adept']['kind']='token'
        with self.assertRaises(ValueError):f.validate_witnesses(self.o,rows)
    def test_parameters_are_typed_and_preserve_every_secondary_clause(self):
        for n in f.pins()['sources']:
            d=self.declaration(n,'effect_0')
            for key,value in [('scope','any'),('death_draw',True),('lifelink',1),('keywords',[]),('invented',1)]:
                changed=copy.deepcopy(d);changed['params'][key]=value
                if f.exact(d,changed):continue
                with self.assertRaises(ValueError):f.spec(changed,self.o[n])
        self.assertEqual(f.classify(self.o['Jeskai Sage'])['death_draw'],1)
        self.assertEqual(f.classify(self.o['Ingenious Skaab'])['activation'],dict(mana=[0,0,1,0,0,0],power=1,toughness=-1))
    def test_all_noncreature_spell_types_trigger_with_real_casts_and_announcements(self):
        for owner in (0,1):
            for mode in ('effect','instant_spell','sorcery_spell','enchantment_spell','planeswalker_spell'):
                c=self.case(v=f'{mode}_{owner}');self.assertEqual(self.stat(c,'power'),4);self.assertEqual(self.stat(c,'toughness'),4)
                self.assertEqual(sum(x['op']=='cast' and x.get('ref')=='witness_0' for x in c['steps']),1)
                self.assertEqual(sum(x['op']=='announce_spell' and x.get('ref')=='witness_0' for x in c['steps']),1)
            c=self.case(v=f'planeswalker_spell_{owner}');self.assertEqual(self.final(c,'assert_counter',ref='witness_0',counter_kind='loyalty'),3)
    def test_creature_artifact_creature_and_opposing_spell_do_not_trigger(self):
        for owner in (0,1):
            for mode in ('creature_spell','artifact_creature_spell','opponent_cast'):
                c=self.case(v=f'{mode}_{owner}');self.assertEqual(self.stat(c,'power'),3);self.assertEqual(self.stat(c,'toughness'),3)
            self.assertEqual(self.stat(self.case(v=f'artifact_creature_spell_{owner}'),'power','witness_0'),0)
    def test_announcement_proves_pump_before_primary_effect_and_pending_hand_observer(self):
        c=self.case();s=c['steps'];a=next(i for i,x in enumerate(s) if x['op']=='announce_spell');r=next(i for i,x in enumerate(s) if x['op']=='resolve')
        self.assertTrue(any(x==dict(op='assert_card',ref='source',property='power',value=4) for x in s[a+1:r]))
        self.assertIn(dict(op='require_card',ref='witness_0',property='in_hand',value=0),s[:a])
        self.assertEqual(next(x['value'] for x in s[a+1:r] if x['op']=='assert_zone_multiplicity' and x['player']==0 and x['zone']=='hand' and x['index']==self.rows['Darksteel Relic']['csvid']),1)
        self.assertEqual(self.multiplicity(c,'Darksteel Relic',0,'hand'),0)
        self.assertEqual(self.multiplicity(c,'Darksteel Relic',0,'battlefield'),2)
    def test_self_and_team_scopes_are_independent_with_stacking(self):
        for owner in (0,1):
            own=self.case(v=f'effect_{owner}');team=self.case('Strongarm Monk',f'effect_{owner}')
            self.assertEqual(self.stat(own,'power','idle'),2);self.assertEqual(self.stat(team,'power','idle'),3)
            self.assertEqual(self.stat(team,'power','enemy'),2)
            team=self.case('Strongarm Monk',f'two_sources_{owner}')
            self.assertEqual(self.stat(team,'power'),5);self.assertEqual(self.stat(team,'power','second'),5);self.assertEqual(self.stat(team,'power','idle'),4)
            self.assertEqual(self.stat(self.case(v=f'three_spells_{owner}'),'power'),6)
    def test_real_late_entry_and_removed_source_controls(self):
        for owner in (0,1):
            c=self.case('Strongarm Monk',f'late_ally_{owner}');self.assertEqual(self.stat(c,'power','idle'),3);self.assertEqual(self.stat(c,'power','late'),2)
            c=self.case('Strongarm Monk',f'source_arrives_late_{owner}');self.assertEqual(self.stat(c,'power'),3);self.assertEqual(self.stat(c,'power','idle'),2)
            c=self.case('Strongarm Monk',f'source_destroyed_before_{owner}');self.assertEqual(self.stat(c,'power','idle'),2);self.assertEqual(self.multiplicity(c,'Strongarm Monk',owner,'graveyard'),1)
    def test_source_costs_real_payment_denials_and_fresh_attack(self):
        for n in f.pins()['sources']:
            for owner in (0,1):
                self.assertEqual(self.final(self.case(n,f'source_payment_{owner}'),'assert_mana_total',player=owner),0)
                for mode in ('source_insufficient_mana','source_wrong_color'):
                    c=self.case(n,f'{mode}_{owner}');self.assertIn(dict(op='cast',ref='source',value=0),c['steps']);self.assertEqual(self.multiplicity(c,n,owner,'hand'),1)
                c=self.case(n,f'fresh_attack_{owner}');self.assertIn(dict(op='attack',ref='source',value=0),c['steps'])
    def test_witness_cost_denials_have_no_planned_target_or_ready_producer(self):
        for owner in (0,1):
            for mode in ('witness_insufficient_mana','witness_wrong_color'):
                c=self.case(v=f'{mode}_{owner}');self.assertIn(dict(op='cast',ref='witness_0',value=0),c['steps']);self.assertEqual(self.stat(c,'power'),3)
                self.assertFalse(any(x['op'].startswith('choose_') for x in c['steps']))
                self.assertFalse(any(x['name']=='Sol Ring' and x['zone']=='battlefield' for x in c['setup']))
            self.assertEqual(self.final(self.case(v=f'payment_{owner}'),'assert_mana_total',player=owner),0)
    def test_mana_activation_is_not_a_cast(self):
        for owner in (0,1):
            c=self.case(v=f'activation_not_cast_{owner}');self.assertEqual(self.stat(c,'power'),3)
            self.assertEqual(self.final(c,'assert_mana',player=owner,color=0),32);self.assertFalse(any(x['op']=='announce_spell' for x in c['steps']))
    def test_countered_announced_noncreature_retains_trigger(self):
        for owner in (0,1):
            c=self.case(v=f'countered_witness_{owner}');self.assertEqual(self.stat(c,'power'),4)
            self.assertIn(dict(op='cast_response',ref='response',value=1),c['steps']);self.assertIn(dict(op='resolve_response'),c['steps'])
            self.assertEqual(self.multiplicity(c,'Darksteel Relic',owner,'graveyard'),1)
    def test_Double_strike_uses_actual_two_strike_damage_and_blocker_death(self):
        for owner in (0,1):
            self.assertEqual(self.final(self.case(v=f'base_double_strike_{owner}'),'assert_life',player=1-owner),14)
            c=self.case(v=f'unblocked_{owner}');self.assertEqual(self.final(c,'assert_life',player=1-owner),12)
            c=self.case(v=f'boosted_double_strike_{owner}');self.assertEqual(self.stat(c,'damage'),0);self.assertEqual(self.multiplicity(c,'Grizzly Bears',1-owner,'graveyard'),2)
            self.assertTrue(any(x['op']=='combat_damage' for x in c['steps']))
    def test_Hexproof_spell_and_real_activation_predicates_cover_both_controllers(self):
        for owner in (0,1):
            c=self.case('Whirlwind Adept',f'targetability_{owner}')
            for side,value in [('own',1),('enemy',0)]:
                self.assertIn(dict(op='can_target_creature',ref=side+'_probe',other='source',value=value),c['steps'])
                self.assertIn(dict(op='probe_activation_target',ref=side+'_ability',other='source',index=-1,value=value),c['steps'])
            self.assertFalse(any(x['op'].startswith('choose_') for x in c['steps']))
            c=self.case('Whirlwind Adept',f'own_bounce_{owner}');self.assertEqual(self.multiplicity(c,'Whirlwind Adept',owner,'hand'),1)
    def test_lifelink_damage_and_actual_cleanup_expiry(self):
        for owner in (0,1):
            c=self.case('Seeker of the Way',f'two_spell_lifelink_{owner}');self.assertEqual(self.final(c,'assert_life',player=owner),24)
            c=self.case('Seeker of the Way',f'lifelink_cleanup_{owner}');self.assertEqual(self.final(c,'assert_life',player=owner),23);self.assertEqual(self.final(c,'assert_life',player=1-owner),15)
            self.assertEqual(self.stat(c,'power'),2);self.assertEqual(sum(x['op']=='advance_turn' for x in c['steps']),2)
            self.assertIn(dict(op='untap_step',player=owner),c['steps'])
    def test_Sage_exact_self_death_draw_order_and_other_death_negatives(self):
        for owner in (0,1):
            for mode in ('self_death','boosted_death','lethal_combat_death'):
                c=self.case('Jeskai Sage',f'{mode}_{owner}');self.assertEqual(self.library_delta(c,owner),1)
                self.assertEqual(self.multiplicity(c,'Jeskai Sage',owner,'graveyard'),1);self.assertEqual(self.multiplicity(c,'Forest',owner,'hand'),1)
            c=self.case('Jeskai Sage',f'two_source_deaths_{owner}');self.assertEqual(self.library_delta(c,owner),2)
            for mode in ('other_death','opponent_death','bounce_no_death','countered_source'):
                self.assertEqual(self.library_delta(self.case('Jeskai Sage',f'{mode}_{owner}'),owner),0)
    def test_Sage_nonlethal_cleanup_preserves_life_and_no_spurious_draw(self):
        for owner in (0,1):
            c=self.case('Jeskai Sage',f'nonlethal_cleanup_{owner}');self.assertEqual(self.library_delta(c,owner),0)
            self.assertEqual((self.stat(c,'power'),self.stat(c,'toughness'),self.stat(c,'damage')),(1,1,0))
    def test_dead_or_bounced_sources_use_zone_receipts_without_invalid_instance_observations(self):
        for n,v in [('Jeskai Sage','boosted_death_0'),('Whirlwind Adept','own_bounce_1'),('Ingenious Skaab','ability_lethal_0')]:
            c=self.case(n,v);s=c['steps'];death=next((i for i,x in enumerate(s) if x['op']=='destroy' and x.get('ref')=='source'),None)
            if death is not None:self.assertFalse(any(x['op']=='assert_card' and x.get('ref')=='source' for x in s[death+1:]))
            self.assertTrue(any(x['op']=='assert_zone_multiplicity' and x['index']==self.rows[n]['csvid'] for x in s))
    def test_Skaab_repeat_and_real_U_payment_do_not_trigger_prowess(self):
        for owner in (0,1):
            c=self.case('Ingenious Skaab',f'ability_repeat_{owner}');self.assertEqual((self.stat(c,'power'),self.stat(c,'toughness')),(4,1))
            self.assertEqual(self.final(c,'assert_mana',player=owner,color=2),28);self.assertEqual(sum(x['op']=='activate' for x in c['steps']),2)
            self.assertFalse(any(x['op']=='cast' or x['op']=='announce_spell' for x in c['steps']))
            self.assertEqual(self.final(self.case('Ingenious Skaab',f'ability_payment_{owner}'),'assert_mana_total',player=owner),0)
    def test_Skaab_lethal_threshold_changes_only_after_actual_prowess_cast(self):
        for owner in (0,1):
            for mode,count,blue in [('ability_lethal',3,27),('combined_lethal',4,26)]:
                c=self.case('Ingenious Skaab',f'{mode}_{owner}');self.assertEqual(sum(x['op']=='activate' for x in c['steps']),count)
                self.assertEqual(self.multiplicity(c,'Ingenious Skaab',owner,'graveyard'),1);self.assertEqual(self.final(c,'assert_mana',player=owner,color=2),blue)
                self.assertEqual(sum(x['op']=='announce_spell' for x in c['steps']),int(mode=='combined_lethal'))
    def test_Skaab_cleanup_denials_tapped_sick_and_source_removal(self):
        for owner in (0,1):
            for mode in ('ability_cleanup','combined_cleanup'):
                c=self.case('Ingenious Skaab',f'{mode}_{owner}');self.assertEqual((self.stat(c,'power'),self.stat(c,'toughness')),(2,3))
            for mode in ('ability_insufficient_mana','ability_wrong_color'):
                c=self.case('Ingenious Skaab',f'{mode}_{owner}');self.assertIn(dict(op='activate',ref='source',value=0),c['steps'])
            for mode in ('ability_tapped','ability_sick'):
                c=self.case('Ingenious Skaab',f'{mode}_{owner}');self.assertIn(dict(op='activate',ref='source',value=1),c['steps'])
            c=self.case('Ingenious Skaab',f'ability_source_removed_pending_{owner}');self.assertEqual(self.multiplicity(c,'Ingenious Skaab',owner,'graveyard'),1)
            s=c['steps'];i=next(i for i,x in enumerate(s) if x['op']=='destroy');self.assertTrue(any(x==dict(op='assert_stack',value=1) for x in s[i+1:]));self.assertEqual(self.final(c,'assert_stack'),0)
    def test_replay_boundaries_record_actual_effects_without_synthetic_history(self):
        for n,v in [('Dragon-Style Twins','ready_snapshot_0'),('Strongarm Monk','pending_snapshot_1'),('Jeskai Sage','death_snapshot_0'),('Ingenious Skaab','combined_pending_snapshot_1')]:
            c=self.case(n,v);s=c['steps'];self.assertIn(dict(op='capture'),s);self.assertIn(dict(op='assert_recorded_state'),s)
            self.assertFalse(any(x['op'] in ('set_storm','set_history','add_counter','set_power','set_toughness') for x in s))
    def soulblade_home(self):return json.loads((Path(__file__).parent/'data/cases/cards/152/15222-soulblade-djinn.json').read_text(encoding='utf8'))
    def test_Soulblade_data_composes_six_Flying_cases_without_invalid_ground_block_templates(self):
        home=self.soulblade_home();vs=home['recipes'][0]['variants'];cases=home['cases']
        self.assertEqual(len(vs),60);self.assertNotIn('blocked_0',vs);self.assertNotIn('blocked_1',vs)
        self.assertEqual(len(cases),6);self.assertEqual({c['coverage_variant'] for c in cases},{f'flying_{kind}_{owner}' for kind in ('ground','flying','reach') for owner in (0,1)})
        for c in cases:
            self.assertEqual(c['oracle_text'],self.o['Soulblade Djinn']['oracle_text'])
            self.assertEqual(self.stat(c,'power'),5);self.assertEqual(self.stat(c,'toughness'),4)
            self.assertEqual(self.stat(c,'power','idle'),3);self.assertEqual(self.stat(c,'power','enemy'),2)
    def test_Soulblade_explicit_cases_pay_full_source_and_witness_costs_before_actual_combat(self):
        for c in self.soulblade_home()['cases']:
            s=c['steps'];owner=next(x['player'] for x in c['setup'] if x['ref']=='source')
            self.assertEqual(next(x['zone'] for x in c['setup'] if x['ref']=='source'),'hand')
            self.assertIn(dict(op='cast',ref='source',value=1),s);self.assertIn(dict(op='cast',ref='flying_spell',value=1),s)
            self.assertEqual(sum(x['op']=='announce_spell' for x in s),2)
            zero=next(i for i,x in enumerate(s) if x['op']=='set_mana' and x['value']==0)
            self.assertTrue(any(x==dict(op='assert_mana',player=owner,color=0,value=27) for x in s[:zero]))
            self.assertTrue(any(x==dict(op='assert_mana',player=owner,color=2,value=28) for x in s[:zero]))
            self.assertEqual(sum(x['op']=='advance_turn' for x in s),2);self.assertIn(dict(op='untap_step',player=owner),s)
            self.assertEqual(self.final(c,'assert_mana_total',player=owner),0);self.assertEqual(self.library_delta(c,owner),2)
    def test_Soulblade_Flying_blocker_receipts_use_independent_Reach_and_Flying_pins(self):
        self.assertEqual((self.o['Giant Spider']['power'],self.o['Giant Spider']['toughness'],self.o['Giant Spider']['keywords']),('2','4',['Reach']))
        self.assertEqual((self.o['Suntail Hawk']['power'],self.o['Suntail Hawk']['toughness'],self.o['Suntail Hawk']['keywords']),('1','1',['Flying']))
        self.assertEqual(f.pins()['witnesses']['Giant Spider'],f.pin(self.o['Giant Spider']))
        for c in self.soulblade_home()['cases']:
            variant=c['coverage_variant'];owner=int(variant[-1]);allowed='_ground_' not in variant
            self.assertIn(dict(op='block',ref='flying_blocker',other='source',value=int(allowed)),c['steps'])
            self.assertEqual(self.final(c,'assert_life',player=1-owner),20 if allowed else 15)
            self.assertEqual(self.stat(c,'damage'),2 if '_reach_' in variant else 1 if '_flying_' in variant else 0)
            self.assertEqual(self.stat(c,'blocked'),0);self.assertEqual(self.stat(c,'attacking'),0)
    def test_accepted_helper_is_not_mutated_and_full_pilot_cardinality_is_bounded(self):
        total=0
        for n in f.pins()['sources']:
            p=f.classify(self.o[n]);vs=f.variants(p);self.assertEqual(len(vs),len(set(vs)));total+=len(vs)
            for v in vs:
                c=self.case(n,v);self.assertEqual(c['_recipe']['oracle_text'],self.o[n]['oracle_text'])
                self.assertLess(len(c['steps']),6000)
        self.assertEqual(total,498)

if __name__=='__main__':unittest.main()
