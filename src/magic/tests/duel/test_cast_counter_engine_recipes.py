import copy,json,unittest
from pathlib import Path
from generate_cases import registry
import cast_counter_engine_recipes as f

class CounterEngineTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.o=json.loads((Path(__file__).parent/'data/oracle.json').read_text(encoding='utf8'))['cards'];cls.rows={r['name']:r for r in registry()}
    def declaration(self,n,v):return dict(family=f.FAMILY,version=2,oracle_text=self.o[n]['oracle_text'],params=f.parameters(f.classify(self.o[n])),variants=[v])
    def case(self,n='Baku Altar',v='value_0'):
        d=self.declaration(n,v);return f.expand(n,self.rows[n]['csvid'],d,f.spec(d,self.o[n]),[v],self.rows,self.o)[0]
    def final(self,c,op,**kw):return next(x['value'] for x in reversed(c['steps']) if x['op']==op and all(x.get(k)==v for k,v in kw.items()))
    def counter(self,c,kind,ref='source'):return self.final(c,'assert_counter',ref=ref,counter_kind=kind)
    def stat(self,c,key,ref='source'):return self.final(c,'assert_card',ref=ref,property=key)
    def mult(self,c,n,w,z):return self.final(c,'assert_zone_multiplicity',player=w,zone=z,index=self.rows[n]['csvid'])
    def draw_delta(self,c,w):return sum(x['zone']=='library' and x['player']==w for x in c['setup'])-self.final(c,'assert_zone',player=w,zone='library')
    def test_whole_Oracle_counter_cost_and_effect_grammar_not_source_name_dispatch(self):
        f.validate_witnesses(self.o,self.rows);self.assertEqual(set(f.pins()['sources']),{'Baku Altar','Blademane Baku','Petalmane Baku','Waxmane Baku','Scroll of the Masters',"Firemind's Research",'Runaway Steam-Kin','Golem Foundry'})
        for n in f.pins()['sources']:
            r=self.o[n];self.assertEqual(f.parameters(f.classify(r)),f.parameters(f.classify(dict(r,name='Independent engine'))))
            with self.assertRaises(ValueError):f.classify(dict(r,oracle_text=r['oracle_text']+'\nWhen this permanent enters, draw a card.'))
            with self.assertRaises(ValueError):f.classify(dict(r,layout='transform'))
        p=f.classify(self.o['Firemind\'s Research']);self.assertEqual([a['remove'] for a in p['abilities']],[2,5]);self.assertEqual([a['mana'] for a in p['abilities']],[[1,0,1,0,0,0],[1,0,0,0,1,0]])
    def test_pinned_handlers_witnesses_and_token_identity_fail_closed(self):
        for n in ('Petalmane Baku','Kami of False Hope','Reach Through Mists'):
            altered=copy.deepcopy(self.o);altered[n]['oracle_text']+='\nDraw a card.'
            with self.assertRaises(ValueError):f.validate_witnesses(altered,self.rows)
        changed=copy.deepcopy(self.rows);changed['Waxmane Baku']['handler']='card_dummy'
        with self.assertRaises(ValueError):f.validate_witnesses(self.o,changed)
        self.assertEqual(f.pins()['token_identity']['Spirit'],f.pin(self.o['Spirit']))
        self.assertEqual(self.o['Spirit']['layout'],'token')
    def test_typed_derived_parameters_reject_boolean_counter_amounts_and_unknown_clauses(self):
        for n in f.pins()['sources']:
            d=self.declaration(n,'gain_0')
            for key,value in [('optional',1),('cap',True),('counter','anything'),('unreviewed',1)]:
                change=copy.deepcopy(d);change['params'][key]=value
                if f.exact(change,d):continue
                with self.assertRaises(ValueError):f.spec(change,self.o[n])
        for raw in ['{1}, Remove all ki counters from this creature: Draw a card.','{1}, Remove X ki counters from this creature: Draw a card.']:
            with self.assertRaises(ValueError):f.activation(raw)
    def test_Spirit_and_Arcane_are_independent_real_cast_positives(self):
        for who in (0,1):
            c=self.case(v=f'gain_{who}');self.assertEqual(self.counter(c,'ki'),1)
            self.assertTrue(any(x['name']=='Kami of False Hope' and x['zone']=='hand' for x in c['setup']))
            c=self.case(v=f'alternate_spell_{who}');self.assertEqual(self.counter(c,'ki'),1);self.assertEqual(self.draw_delta(c,who),1)
            self.assertTrue(any(x['op']=='announce_spell' for x in c['steps']))
    def test_opponent_and_nonmatching_casts_do_not_generate_counters(self):
        for n in f.pins()['sources']:
            kind=f.classify(self.o[n])['counter']
            for who in (0,1):
                for mode in ('opponent_cast','nonmatching_spell'):
                    self.assertEqual(self.counter(self.case(n,f'{mode}_{who}'),kind),0)
    def test_optional_human_intents_accept_and_decline_without_arbitrary_AI_choices(self):
        for n in ('Baku Altar','Blademane Baku','Petalmane Baku','Waxmane Baku'):
            c=self.case(n,'gain_0');self.assertIn(dict(op='choose_trigger_source',player=0,ref='source'),c['steps'])
            c=self.case(n,'decline_0');self.assertEqual(self.counter(c,'ki'),0);self.assertFalse(any(x['op']=='choose_trigger_source' for x in c['steps']))
            self.assertNotIn('decline_1',f.variants(f.classify(self.o[n])))
    def test_two_sources_and_cast_repeats_build_each_counter_from_actual_announcements(self):
        c=self.case(v='two_sources_0');self.assertEqual(self.counter(c,'ki'),1);self.assertEqual(self.counter(c,'ki','second'),1)
        for mode,n in [('two_casts',2),('three_casts',3)]:
            c=self.case(v=f'{mode}_1');self.assertEqual(self.counter(c,'ki'),n)
            self.assertEqual(sum(x['op']=='announce_spell' for x in c['steps']),n)
    def test_source_casting_late_entry_removed_source_and_countered_witness(self):
        for n in f.pins()['sources']:
            kind=f.classify(self.o[n])['counter']
            for who in (0,1):
                self.assertEqual(self.counter(self.case(n,f'source_cast_{who}'),kind),0)
                self.assertEqual(self.counter(self.case(n,f'source_arrives_late_{who}'),kind),0)
                c=self.case(n,f'source_removed_before_{who}');self.assertEqual(self.mult(c,n,who,'graveyard'),1)
                c=self.case(n,f'countered_witness_{who}');self.assertEqual(self.counter(c,kind),1);self.assertIn(dict(op='resolve_response'),c['steps'])
    def test_source_costs_and_real_missing_mana_controls(self):
        for n in f.pins()['sources']:
            for who in (0,1):
                c=self.case(n,f'source_payment_{who}');self.assertEqual(self.final(c,'assert_mana_total',player=who),0)
                c=self.case(n,f'source_insufficient_mana_{who}');self.assertIn(dict(op='cast',ref='source',value=0),c['steps'])
                if not any(f.classify(self.o[n])['mana'][1:]):self.assertNotIn(f'source_wrong_color_{who}',f.variants(f.classify(self.o[n])))
    def test_counter_and_tap_costs_precede_token_resolution(self):
        c=self.case();s=c['steps'];a=next(i for i,x in enumerate(s) if x['op']=='activate');r=next(i for i in range(a+1,len(s)) if s[i]['op']=='resolve')
        self.assertIn(dict(op='assert_counter',ref='source',counter_kind='ki',value=0),s[a+1:r])
        self.assertIn(dict(op='assert_card',ref='source',property='tapped',value=1),s[a+1:r])
        self.assertFalse(any(x['op']=='bind_permanent_copy' for x in s[:r]))
        self.assertEqual(self.mult(c,'Spirit',0,'battlefield'),1);self.assertEqual(self.counter(c,'ki'),0)
    def test_token_definition_comes_from_source_not_unrelated_token_Oracle_color_and_Flying(self):
        a=f.classify(self.o['Baku Altar'])['abilities'][0];self.assertEqual(a['token'],dict(name='Spirit',power=1,toughness=1,color=0,artifact=False,subtype='Spirit',keywords=[]))
        c=self.case();self.assertEqual(self.stat(c,'power','token_0'),1)
        for prop,value in [('token',1),('color',0),('artifact',0),('subtype_Spirit',1)]:self.assertEqual(self.final(c,'assert_matching_permanents',property=prop),value)
        c=self.case(v='token_flying_negative_0');self.assertIn(dict(op='block',ref='token_0',other='token_opponent',value=0),c['steps']);self.assertEqual(self.final(c,'assert_life',player=0),19)
    def test_variable_zero_X_is_legal_without_fabricated_minimum(self):
        for n in ('Blademane Baku','Petalmane Baku','Waxmane Baku'):
            for who in (0,1):
                c=self.case(n,f'zero_x_{who}');self.assertIn(dict(op='require_can_activate',ref='source',value=1),c['steps']);self.assertIn(dict(op='activate',ref='source',value=1),c['steps']);self.assertEqual(self.counter(c,'ki'),0)
                self.assertEqual(sum(x['op']=='cast' for x in c['steps']),0)
            c=self.case(n,'zero_from_two_0');self.assertEqual(self.counter(c,'ki'),2)
        self.assertNotIn('partial_x_1',f.variants(f.classify(self.o['Blademane Baku'])))
    def test_Blademane_X1_and_X2_pump_exactly_twice_removed_and_cleanup_keeps_ki(self):
        c=self.case('Blademane Baku','partial_x_0');self.assertEqual(self.counter(c,'ki'),2);self.assertEqual((self.stat(c,'power'),self.stat(c,'toughness')),(3,1));self.assertIn(dict(op='choose_number',player=0,value=1),c['steps'])
        c=self.case('Blademane Baku','value_1');self.assertEqual((self.stat(c,'power'),self.stat(c,'toughness')),(5,1));self.assertEqual(self.counter(c,'ki'),0)
        c=self.case('Blademane Baku','cleanup_0');self.assertEqual(self.stat(c,'power'),1);self.assertEqual(self.counter(c,'ki'),0)
    def test_Petalmane_pays_generic_removes_chosen_X_and_produces_one_color_immediately(self):
        for who in (0,1):
            for mode,color in [('value',2),('mana_white',5),('mana_black',1),('mana_red',4),('mana_green',3)]:
                c=self.case('Petalmane Baku',f'{mode}_{who}');self.assertEqual(self.counter(c,'ki'),0)
                before=30-(2 if color==5 else 0);self.assertEqual(self.final(c,'assert_mana',player=who,color=color),before+2)
                a=next(i for i,x in enumerate(c['steps']) if x['op']=='activate');self.assertFalse(any(x['op']=='resolve' for x in c['steps'][a+1:]));self.assertEqual(self.final(c,'assert_stack'),0)
    def test_Waxmane_X_is_actual_target_count_with_legal_cancel_not_a_number_menu(self):
        for who in (0,1):
            c=self.case('Waxmane Baku',f'value_{who}');self.assertEqual(self.stat(c,'tapped','enemy'),1);self.assertEqual(self.stat(c,'tapped','idle'),1);self.assertEqual(self.counter(c,'ki'),0)
            self.assertFalse(any(x['op']=='choose_number' for x in c['steps']))
            c=self.case('Waxmane Baku',f'partial_x_{who}');self.assertEqual(self.counter(c,'ki'),2);self.assertEqual(self.stat(c,'tapped','enemy'),1);self.assertEqual(self.stat(c,'tapped','idle'),0)
            self.assertIn(dict(op='choose_cancel_target',player=who),c['steps'])
    def test_Scroll_keeps_lore_and_uses_correct_resolution_counter_count(self):
        for who in (0,1):
            c=self.case('Scroll of the Masters',f'value_{who}');self.assertEqual(self.counter(c,'lore'),2);self.assertEqual((self.stat(c,'power','idle'),self.stat(c,'toughness','idle')),(4,4))
            c=self.case('Scroll of the Masters',f'lore_changes_pending_{who}');self.assertEqual(self.counter(c,'lore'),3);self.assertEqual(self.stat(c,'power','idle'),5);self.assertEqual(self.draw_delta(c,who),1)
            s=c['steps'];a=next(i for i,x in enumerate(s) if x['op']=='activate');b=next(i for i in range(a+1,len(s)) if s[i]['op']=='cast');r=next(i for i in range(b+1,len(s)) if s[i]['op']=='resolve')
            self.assertIn(dict(op='assert_stack',value=2),s[b:r]);self.assertIn(dict(op='assert_card',ref='idle',property='power',value=2),s[b:r])
    def test_Scroll_target_scope_and_cleanup_no_lore_removal(self):
        c=self.case('Scroll of the Masters','opponent_target_probe_0');self.assertIn(dict(op='probe_activation_target',ref='source',other='enemy',index=-1,value=0),c['steps'])
        c=self.case('Scroll of the Masters','cleanup_1');self.assertEqual(self.counter(c,'lore'),2);self.assertEqual(self.stat(c,'power','idle'),2)
    def test_Firemind_two_source_bound_modes_pay_two_and_five_real_charge_costs(self):
        for who in (0,1):
            c=self.case("Firemind's Research",f'both_modes_{who}');self.assertEqual(self.counter(c,'charge'),0);self.assertEqual(self.draw_delta(c,who),8);self.assertEqual(self.final(c,'assert_life',player=1-who),15)
            self.assertEqual([x['mode_label'] for x in c['steps'] if x['op']=='choose_activation_mode'],['Draw a card','Deal 5 damage'])
            self.assertEqual(sum(x['op']=='require_activation_mode' for x in c['steps']),2)
    def test_Firemind_player_creature_and_planeswalker_damage_categories(self):
        for who in (0,1):
            self.assertEqual(self.final(self.case("Firemind's Research",f'damage_own_player_{who}'),'assert_life',player=who),15)
            self.assertEqual(self.mult(self.case("Firemind's Research",f'damage_creature_{who}'),'Grizzly Bears',1-who,'graveyard'),2)
            self.assertEqual(self.mult(self.case("Firemind's Research",f'damage_planeswalker_{who}'),'Jace Beleren',1-who,'graveyard'),1)
    def test_SteamKin_cap_permanent_PT_mana_counter_payment_and_regrowth(self):
        for who in (0,1):
            c=self.case('Runaway Steam-Kin',f'cap_fourth_{who}');self.assertEqual(self.counter(c,'plus1'),3);self.assertEqual((self.stat(c,'power'),self.stat(c,'toughness')),(4,4))
            c=self.case('Runaway Steam-Kin',f'payment_{who}');self.assertEqual(self.counter(c,'plus1'),0);self.assertEqual(self.final(c,'assert_mana',player=who,color=4),3);self.assertEqual(self.stat(c,'power'),1)
            c=self.case('Runaway Steam-Kin',f'spend_regrow_{who}');self.assertEqual(self.counter(c,'plus1'),1);self.assertEqual(self.stat(c,'power'),2);self.assertEqual(self.final(c,'assert_mana',player=who,color=4),29)
            self.assertEqual(self.counter(self.case('Runaway Steam-Kin',f'permanent_cleanup_{who}'),'plus1'),3)
    def test_printed_T_denials_and_absence_of_T_when_tapped_or_sick(self):
        for n in f.pins()['sources']:
            p=f.classify(self.o[n]);a=p['abilities'][0]
            for who in (0,1):
                c=self.case(n,f'tapped_{who}');self.assertIn(dict(op='activate',ref='source',value=int(not a['tap'])),c['steps'])
                c=self.case(n,f'sick_{who}');self.assertIn(dict(op='activate',ref='source',value=1),c['steps'])
                if type(a['remove']) is int and a['remove']>0:
                    c=self.case(n,f'counter_short_{who}');self.assertIn(dict(op='activate',ref='source',value=0),c['steps']);self.assertFalse(any(x['op'].startswith('choose_') and x['op']!='choose_trigger_source' for x in c['steps']))
    def test_pending_source_removal_uses_ability_and_zone_receipts_not_dead_instance_properties(self):
        for n in ('Baku Altar','Blademane Baku','Scroll of the Masters',"Firemind's Research"):
            c=self.case(n,'source_removed_pending_0');s=c['steps'];i=next(i for i,x in enumerate(s) if x['op']=='destroy' and x.get('ref')=='source')
            self.assertIn(dict(op='assert_stack',value=1),s[i+1:]);self.assertFalse(any(x['op']=='assert_counter' and x.get('ref')=='source' for x in s[i+1:]));self.assertEqual(self.mult(c,n,0,'graveyard'),1)
    def test_multiple_human_mandatory_sources_and_changed_pool_cache_use_actual_protocol(self):
        c=self.case('Runaway Steam-Kin','two_sources_0');s=c['steps'];announce=next(i for i,x in enumerate(s) if x['op']=='announce_spell')
        self.assertEqual([x for x in s[:announce] if x['op']=='choose_trigger_source'],[dict(op='choose_trigger_source',player=0,ref='source')])
        self.assertEqual(self.counter(c,'plus1'),1);self.assertEqual(self.counter(c,'plus1','second'),1)
        self.assertFalse(any(x['op']=='choose_trigger_source' for x in self.case('Runaway Steam-Kin','two_sources_1')['steps']))
        for who in (0,1):
            s=self.case('Petalmane Baku',f'insufficient_mana_{who}')['steps'];i=next(i for i,x in enumerate(s) if x['op']=='require_can_activate')
            self.assertEqual(s[i],dict(op='require_can_activate',ref='source',value=0));self.assertEqual(s[i-1],dict(op='refresh'))
            self.assertEqual(s[i-2]['op'],'set_mana');self.assertEqual(s[i-2]['value'],30 if who==0 else 0)
            self.assertFalse(any(x['op']=='choose_color' for x in s))
    def test_Golem_whole_artifact_optional_counter_cost_and_source_defined_token(self):
        p=f.classify(self.o['Golem Foundry']);a=p['abilities'][0]
        self.assertEqual(p['predicate'],'artifact');self.assertTrue(p['optional']);self.assertEqual(p['mana'],[3,0,0,0,0,0])
        self.assertEqual(a['remove'],3);self.assertEqual(a['mana'],[0]*6);self.assertFalse(a['tap'])
        self.assertEqual(a['token'],dict(name='Golem',power=3,toughness=3,color=0,artifact=True,subtype='Golem',keywords=[]))
        self.assertEqual(self.o['Golem']['power'],'4');self.assertEqual(f.pins()['token_identity']['Golem'],f.pin(self.o['Golem']))
        for who in (0,1):
            c=self.case('Golem Foundry',f'value_{who}');self.assertEqual(self.counter(c,'charge'),0)
            self.assertEqual(sum(x['op']=='announce_spell' for x in c['steps']),3)
            self.assertEqual(self.stat(c,'power','token_0'),3);self.assertEqual(self.stat(c,'toughness','token_0'),3)
            self.assertIn(dict(op='assert_matching_permanents',player=who,index=18752,property='artifact',value=1),c['steps'])
            c=self.case('Golem Foundry',f'counter_short_{who}');self.assertEqual(self.counter(c,'charge'),2);self.assertNotIn(dict(op='bind_permanent_copy',player=who,value=18752,index=0,ref='token_0'),c['steps'])
            self.assertEqual(self.counter(self.case('Golem Foundry',f'nonmatching_spell_{who}'),'charge'),0);self.assertEqual(self.counter(self.case('Golem Foundry',f'opponent_cast_{who}'),'charge'),0)
            self.assertEqual(self.mult(self.case('Golem Foundry',f'token_ground_combat_{who}'),'Golem',who,'battlefield'),1)
        self.assertEqual(self.counter(self.case('Golem Foundry','decline_0'),'charge'),0)
        self.assertEqual(len(f.variants(p)),53)
    def test_replays_and_full_pilot_use_only_real_counter_generation(self):
        total=0
        for n in f.pins()['sources']:
            p=f.classify(self.o[n]);vs=f.variants(p);total+=len(vs);self.assertEqual(len(vs),len(set(vs)))
            for v in vs:
                c=self.case(n,v);self.assertEqual(c['_recipe']['oracle_text'],self.o[n]['oracle_text']);self.assertLess(len(c['steps']),12000)
                self.assertFalse(any(x['op'] in ('add_counter','set_counter','set_storm','set_history','set_power') for x in c['steps']))
            for mode in ('ready_snapshot','pending_snapshot','resolved_snapshot'):
                c=self.case(n,f'{mode}_1');self.assertIn(dict(op='capture'),c['steps']);self.assertIn(dict(op='assert_recorded_state'),c['steps'])
        self.assertEqual(total,449)

if __name__=='__main__':unittest.main()
