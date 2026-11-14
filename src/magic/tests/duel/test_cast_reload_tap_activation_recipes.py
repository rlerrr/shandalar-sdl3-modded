import copy,json,unittest
from pathlib import Path
from generate_cases import registry
import cast_reload_tap_activation_recipes as f

class ReloadTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.o=json.loads((Path(__file__).parent/'data/oracle.json').read_text(encoding='utf8'))['cards'];cls.rows={r['name']:r for r in registry()}
    def case(self,n='Gelectrode',v='reload_0'):
        r=self.o[n];p=f.classify(r);d=dict(family=f.FAMILY,version=2,oracle_text=r['oracle_text'],params=f.parameters(p),variants=[v]);return f.expand(n,self.rows[n]['csvid'],d,f.spec(d,r),[v],self.rows,self.o)[0]
    def final(self,c,op,**kw):return next(x['value'] for x in reversed(c['steps']) if x['op']==op and all(x.get(k)==v for k,v in kw.items()))
    def stat(self,c,prop,ref='source'):return self.final(c,'assert_card',ref=ref,property=prop)
    def mult(self,c,n,w,z):return self.final(c,'assert_zone_multiplicity',player=w,zone=z,index=self.rows[n]['csvid'])
    def test_whole_Oracle_and_name_neutral_grammar(self):
        self.assertEqual(set(f.pins()['sources'])-{'Thermo-Alchemist'},{'Gelectrode','Leonin Battlemage','Merrow Bonegnawer','Merrow Levitator','Lobber Crew','Nettle Drone','Zephyr Scribe','Inspired Sprite'});f.validate_witnesses(self.o,self.rows)
        for n in f.pins()['sources']:
            r=self.o[n];self.assertEqual(f.parameters(f.classify(r)),f.parameters(f.classify(dict(r,name='Independent creature'))))
            for altered in (dict(r,layout='transform'),dict(r,oracle_text=r['oracle_text']+'\nWhen this creature enters, draw a card.'),dict(r,oracle_text=r['oracle_text'].replace('{T}:','{Q}:'))):
                with self.assertRaises(ValueError):f.classify(altered)
    def test_typed_derived_parameters_fail_closed(self):
        n='Gelectrode';r=self.o[n];p=f.classify(r);d=dict(family=f.FAMILY,version=2,oracle_text=r['oracle_text'],params=f.parameters(p),variants=['gain_0'])
        for key,value in [('optional',1),('fee',[False]*6),('effect','draw100'),('predicate','enemy'),('extra',True)]:
            q=copy.deepcopy(d);q['params'][key]=value
            with self.assertRaises(ValueError):f.spec(q,r)
    def test_profile_handlers_and_full_witness_fields_fail_closed(self):
        for n in ('Inspired Sprite','Watchwolf','Giant Spider','Prodigal Sorcerer'):
            o=copy.deepcopy(self.o);o[n]['oracle_text']+='\nDraw a card.'
            with self.assertRaises(ValueError):f.validate_witnesses(o,self.rows)
        rows=copy.deepcopy(self.rows);rows['Gelectrode']['handler']='card_dummy'
        with self.assertRaises(ValueError):f.validate_witnesses(self.o,rows)
        self.assertEqual(f.pins()['witnesses']['Giant Spider']['power'],'2');self.assertEqual(f.pins()['witnesses']['Giant Spider']['toughness'],'4');self.assertEqual(f.pins()['witnesses']['Giant Spider']['keywords'],['Reach'])
    def test_cast_predicates_independent_positive_negative_witnesses(self):
        for n in f.pins()['sources']:
            p=f.classify(self.o[n])
            for who in (0,1):
                for mode in ('gain','alternate_spell'):self.assertEqual(self.stat(self.case(n,f'{mode}_{who}'),'tapped'),0)
                self.assertEqual(self.stat(self.case(n,f'opponent_cast_{who}'),'tapped'),1)
                if p['predicate']!='any':self.assertEqual(self.stat(self.case(n,f'nonmatching_spell_{who}'),'tapped'),1)
        self.assertFalse(f.matches(self.o['Ornithopter'],f.classify(self.o['Lobber Crew'])));self.assertTrue(f.matches(self.o['Watchwolf'],f.classify(self.o['Lobber Crew'])))
        self.assertTrue(f.matches(self.o['Darksteel Relic'],f.classify(self.o['Nettle Drone'])));self.assertFalse(f.matches(self.o['Raging Goblin'],f.classify(self.o['Nettle Drone'])))
    def test_actual_tap_cast_announcement_untap_tap_loop(self):
        for n in f.pins()['sources']:
            for who in (0,1):
                c=self.case(n,f'reload_{who}');s=c['steps'];act=[i for i,x in enumerate(s) if x['op']=='activate'];cast=next(i for i,x in enumerate(s) if x['op']=='cast');announce=next(i for i,x in enumerate(s) if x['op']=='announce_spell')
                self.assertEqual(len(act),2);self.assertLess(act[0],cast);self.assertLess(cast,announce);self.assertLess(announce,act[1]);self.assertIn(dict(op='require_can_activate',ref='source',value=0),s[act[0]+1:cast]);self.assertEqual(self.stat(c,'tapped'),1)
    def test_optional_accept_decline_and_actual_source_ordering(self):
        for n in f.pins()['sources']:
            p=f.classify(self.o[n]);s=self.case(n,'two_sources_0')['steps'];intents=[x['ref'] for x in s if x['op']=='choose_trigger_source']
            self.assertEqual(intents,['source','second'] if p['optional'] else ['source'] if f.pins()['mandatory_multi_source_choice'][n] else [])
            if p['optional']:
                c=self.case(n,'decline_0');self.assertEqual(self.stat(c,'tapped'),1);self.assertFalse(any(x['op']=='choose_trigger_source' for x in c['steps']));self.assertNotIn('decline_1',f.variants(p))
    def test_no_self_trigger_before_entry_and_late_source_no_retro_trigger(self):
        for n in f.pins()['sources']:
            c=self.case(n,'source_cast_0');self.assertEqual(self.stat(c,'tapped'),0);self.assertFalse(any(x['op']=='choose_trigger_source' for x in c['steps']))
            c=self.case(n,'source_arrives_late_1');self.assertEqual(self.stat(c,'tapped'),0);self.assertEqual(sum(x['op']=='announce_spell' for x in c['steps']),2)
    def test_source_costs_use_all_actual_colored_components(self):
        for n in f.pins()['sources']:
            p=f.classify(self.o[n])
            for who in (0,1):
                c=self.case(n,f'source_payment_{who}');self.assertEqual(self.final(c,'assert_mana_total',player=who),0)
                for mode in ('source_insufficient_mana','source_wrong_color'):
                    c=self.case(n,f'{mode}_{who}');self.assertIn(dict(op='cast',ref='source',value=0),c['steps']);self.assertEqual(self.mult(c,n,who,'hand'),1)
    def test_sick_tapped_fresh_abilities_cannot_pay_T(self):
        for n in f.pins()['sources']:
            for mode in ('tapped_activation','sick_activation','fresh_activation'):
                c=self.case(n,f'{mode}_0');self.assertIn(dict(op='require_can_activate',ref='source',value=0),c['steps']);self.assertIn(dict(op='activate',ref='source',value=0),c['steps']);self.assertFalse(any(x['op']=='choose_target' for x in c['steps']))
    def test_untap_comes_from_real_turn_progress_and_actual_step(self):
        c=self.case('Gelectrode','real_untap_cycle_1');s=c['steps'];self.assertEqual(sum(x['op']=='advance_turn' for x in s),2);i=next(i for i,x in enumerate(s) if x['op']=='untap_step');self.assertEqual(s[i]['player'],1);self.assertTrue(any(x['op']=='activate' for x in s[i:]));self.assertEqual(self.final(c,'assert_life',player=0),18)
    def test_pending_cost_receipts_precede_effect_and_activation_survives_source_death(self):
        for n in f.pins()['sources']:
            c=self.case(n,'source_removed_pending_0');s=c['steps'];kill=next(i for i,x in enumerate(s) if x['op']=='destroy' and x['ref']=='source');self.assertTrue(any(x['op']=='assert_stack' and x['value']==1 for x in s[kill:]));self.assertEqual(self.mult(c,n,0,'graveyard'),1);self.assertFalse(any(x['op']=='assert_card' and x.get('ref')=='source' for x in s[kill:]))
    def test_damage_any_all_target_categories_and_planeswalker_loyalty(self):
        for who in (0,1):
            self.assertEqual(self.final(self.case(v=f'own_player_{who}'),'assert_life',player=who),19)
            self.assertEqual(self.stat(self.case(v=f'enemy_creature_{who}'),'damage','enemy'),1)
            c=self.case(v=f'planeswalker_{who}');self.assertEqual(self.final(c,'assert_counter',ref='walker',counter_kind='loyalty'),2)
            c=self.case(v=f'self_target_{who}');self.assertEqual(self.mult(c,'Gelectrode',who,'graveyard'),1)
    def test_damage_each_opponent_has_no_target_and_exact_repeat_receipts(self):
        for n in ('Lobber Crew','Nettle Drone'):
            c=self.case(n,'loop_two_1');self.assertEqual(self.final(c,'assert_life',player=0),17);self.assertEqual(self.final(c,'assert_life',player=1),20);self.assertFalse(any(x['op'] in ('choose_player','choose_target') for x in c['steps']))
    def test_graveyard_target_player_chooses_and_zones_not_live_instances(self):
        for who in (0,1):
            for mode,recipient in [('own_graveyard',who),('enemy_graveyard',1-who)]:
                c=self.case('Merrow Bonegnawer',f'{mode}_{who}');self.assertIn(dict(op='choose_zone_card',player=recipient,value=106),c['steps']);self.assertEqual(self.mult(c,'Grizzly Bears',recipient,'graveyard'),0);self.assertEqual(self.mult(c,'Grizzly Bears',recipient,'exile'),1)
                self.assertFalse(any(x['op'] in ('assert_card','require_card') and x.get('ref','').startswith('custom_graveyard') for x in c['steps']))
    def test_empty_and_duplicate_graveyards_do_not_invent_choices(self):
        c=self.case('Merrow Bonegnawer','empty_graveyard_0');self.assertFalse(any(x['op']=='choose_zone_card' for x in c['steps']));self.assertEqual(self.stat(c,'tapped'),1)
        c=self.case('Merrow Bonegnawer','duplicate_graveyard_1');self.assertEqual(self.mult(c,'Grizzly Bears',0,'graveyard'),1);self.assertEqual(self.mult(c,'Grizzly Bears',0,'exile'),1)
    def test_loot_identical_hand_and_library_prove_exact_AI_choice_independence(self):
        for n in ('Zephyr Scribe','Inspired Sprite'):
            for who in (0,1):
                c=self.case(n,f'reload_{who}');hand={x['name'] for x in c['setup'] if x['zone']=='hand' and x['player']==who};lib={x['name'] for x in c['setup'] if x['zone']=='library' and x['player']==who};self.assertEqual(hand,lib);self.assertEqual(len(hand),1)
                self.assertTrue(any(x['op']=='bind_hand_copy' for x in c['steps']));self.assertEqual(sum(x['op']=='choose_dialog' for x in c['steps']),2 if who==1 else 0)
                self.assertEqual(self.final(c,'assert_discard_choices'),2 if who==0 else 0)
    def test_loot_empty_hand_and_single_library_card_still_draw_then_discard(self):
        for n in ('Zephyr Scribe','Inspired Sprite'):
            for who in (0,1):
                c=self.case(n,f'empty_hand_{who}');self.assertEqual(self.final(c,'assert_zone',player=who,zone='hand'),0);self.assertEqual(self.final(c,'assert_zone',player=who,zone='library'),11)
                c=self.case(n,f'one_card_library_{who}');self.assertEqual(self.final(c,'assert_zone',player=who,zone='library'),0)
    def test_U_loot_fee_exact_missing_mana_wrong_color_and_refresh(self):
        for who in (0,1):
            c=self.case('Zephyr Scribe',f'payment_{who}');self.assertEqual(self.final(c,'assert_mana_total',player=who),0)
            for mode in ('insufficient_mana','wrong_color'):
                s=self.case('Zephyr Scribe',f'{mode}_{who}')['steps'];i=next(i for i,x in enumerate(s) if x['op']=='require_can_activate');self.assertEqual(s[i-1],dict(op='refresh'));self.assertEqual(s[i]['value'],0)
    def test_target_PT_flying_ownership_and_expiry_have_actual_combat_receipts(self):
        self.assertEqual(self.stat(self.case('Leonin Battlemage','own_creature_0'),'power','idle'),3);self.assertEqual(self.stat(self.case('Leonin Battlemage','enemy_creature_0'),'toughness','enemy'),3)
        self.assertEqual(self.stat(self.case('Leonin Battlemage','cleanup_1'),'power','idle'),2)
        c=self.case('Merrow Levitator','target_combat_0');self.assertIn(dict(op='block',ref='combat_other',other='idle',value=0),c['steps']);self.assertEqual(self.final(c,'assert_life',player=1),18)
        c=self.case('Merrow Levitator','cleanup_0');self.assertIn(dict(op='cleanup'),c['steps']);self.assertIn(dict(op='block',ref='combat_other',other='idle',value=1),c['steps']);self.assertEqual(self.mult(c,'Grizzly Bears',0,'graveyard'),2)
    def test_Defender_attack_rejection_real_ground_and_flying_blocks(self):
        for who in (0,1):
            c=self.case('Lobber Crew',f'source_attack_{who}');self.assertIn(dict(op='attack',ref='source',value=0),c['steps'])
            c=self.case('Lobber Crew',f'block_ground_{who}');self.assertIn(dict(op='block',ref='source',other='combat_other',value=1),c['steps']);self.assertEqual(self.stat(c,'damage'),2)
            c=self.case('Lobber Crew',f'block_flying_{who}');self.assertIn(dict(op='block',ref='source',other='combat_other',value=0),c['steps'])
    def test_Devoid_source_is_colorless_despite_actual_red_cast_cost(self):
        p=f.classify(self.o['Nettle Drone']);self.assertEqual(p['mana'],[2,0,0,0,1,0]);self.assertEqual(p['keywords'],['Devoid']);self.assertEqual(self.final(self.case('Nettle Drone','source_cast_0'),'assert_matching_permanents',player=0,index=15383,property='color'),0)
    def test_Flash_response_and_Flying_Reach_combat_are_whole_printed_controls(self):
        for who in (0,1):
            c=self.case('Inspired Sprite',f'source_flash_{who}');s=c['steps'];self.assertIn(dict(op='cast_response',ref='source',value=1),s);i=s.index(dict(op='resolve_response'));self.assertIn(dict(op='assert_stack',value=2),s[:i]);self.assertFalse(any(x['op']=='resolve' or x['op']=='assert_stack' and x['value']!=0 for x in s[i+1:]));self.assertEqual(self.final(c,'assert_stack'),0)
            self.assertIn(dict(op='turn',player=1-who,value=0),s);self.assertEqual(self.final(c,'assert_mana_total',player=who),176);self.assertEqual(self.final(c,'assert_mana_total',player=1-who),178)
            for mode,can in [('flying_ground_block',0),('flying_flyer_block',1),('flying_reach_block',1)]:self.assertIn(dict(op='block',ref='combat_other',other='source',value=can),self.case('Inspired Sprite',f'{mode}_{who}')['steps'])
    def test_countered_qualifying_spell_keeps_untap_and_paid_actual_cast(self):
        for n in f.pins()['sources']:
            c=self.case(n,'countered_witness_0');self.assertEqual(self.stat(c,'tapped'),0);self.assertIn(dict(op='cast_response',ref='response',value=1),c['steps']);self.assertIn(dict(op='resolve_response'),c['steps'])
    def test_snapshots_replay_actual_pending_cast_and_activation_boundaries(self):
        for n in f.pins()['sources']:
            for mode in ('ready_snapshot','pending_snapshot','activation_pending_snapshot','resolved_snapshot'):
                c=self.case(n,f'{mode}_1');self.assertIn(dict(op='capture'),c['steps']);self.assertIn(dict(op='assert_recorded_state'),c['steps'])
    def test_Thermo_exact_followon_whole_Defender_T_damage_mandatory_IS(self):
        p=f.classify(self.o['Thermo-Alchemist']);self.assertEqual(p['mana'],[1,0,0,0,1,0]);self.assertEqual(p['keywords'],['Defender']);self.assertEqual(p['predicate'],'instant_sorcery');self.assertFalse(p['optional']);self.assertEqual(p['effect'],'damage_opponents');self.assertEqual(p['fee'],[0]*6)
        self.assertEqual(self.o['Thermo-Alchemist']['power'],'0');self.assertEqual(self.o['Thermo-Alchemist']['toughness'],'3');self.assertEqual(len(f.variants(p)),62)
        for who in (0,1):
            c=self.case('Thermo-Alchemist',f'reload_{who}');self.assertEqual(self.final(c,'assert_life',player=1-who),18);self.assertEqual(sum(x['op']=='activate' for x in c['steps']),2);self.assertFalse(any(x['op'] in ('choose_target','choose_player') for x in c['steps']))
            c=self.case('Thermo-Alchemist',f'alternate_spell_{who}');self.assertTrue(any(x['name']=='Divination' and x['zone']=='hand' for x in c['setup']));self.assertEqual(self.stat(c,'tapped'),0)
            c=self.case('Thermo-Alchemist',f'source_attack_{who}');self.assertIn(dict(op='attack',ref='source',value=0),c['steps'])
            self.assertEqual(self.stat(self.case('Thermo-Alchemist',f'nonmatching_spell_{who}'),'tapped'),1)
        self.assertNotIn('decline_0',f.variants(p))
    def test_entire_pilot_unique_full_Oracle_bounded_and_no_synthetic_rules_events(self):
        total=0
        for n in f.pins()['sources']:
            p=f.classify(self.o[n]);vs=f.variants(p);self.assertEqual(len(vs),len(set(vs)));total+=len(vs)
            for v in vs:
                c=self.case(n,v);self.assertEqual(c['_recipe']['oracle_text'],self.o[n]['oracle_text']);self.assertLess(len(c['steps']),18000)
                self.assertFalse(any(x['op'] in ('set_counter','add_counter','set_storm','set_history','untap','set_power') for x in c['steps']))
        self.assertEqual(total,629);self.assertEqual(sum(len(f.variants(f.classify(self.o[n]))) for n in f.pins()['sources'] if n!='Thermo-Alchemist'),567)

if __name__=='__main__':unittest.main()
