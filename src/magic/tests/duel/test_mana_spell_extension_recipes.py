"""Wave6 guards use the public dispatcher and production operation vocabulary."""
import copy,json,unittest
from pathlib import Path
from generate_cases import registry,OPS,PROPERTIES,TOKEN_PROPERTIES,validate_untap_step
from recipes import expand,explicit_spec
import mana_spell_extension_recipes as m

class ManaWave6Test(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.o=json.loads((Path(__file__).parent/'data/oracle.json').read_text(encoding='utf8'))['cards']
        cls.rows={x['name']:x for x in registry()};cls.pins=m.pins()
    def declaration(self,name,selected=None):
        p=self.pins['sources'][name]['params']
        d=dict(family=p['family'],version=2,params=copy.deepcopy(p),oracle_text=self.o[name]['oracle_text'],variants=['effect_0'])
        d['variants']=selected or m.variants(explicit_spec(d,self.o[name]));return d
    def cases(self,name,selected=None):
        return expand(name,self.rows[name]['csvid'],self.declaration(name,selected),self.o[name],self.rows,oracle_context=self.o)
    def test_public_every_source_every_variant(self):
        for name in self.pins['sources']:
            with self.subTest(name=name):
                cs=self.cases(name);self.assertTrue(cs)
                self.assertEqual(len(cs),len({x['name'] for x in cs}))
                for c in cs:
                    self.assertEqual(c['_recipe']['version'],2)
                    for s in c['steps']:
                        self.assertIn(s['op'],OPS)
                        if s['op'] in ('assert_card','require_card'):self.assertIn(s['property'],PROPERTIES)
                        if s['op']=='assert_matching_permanents':self.assertIn(s['property'],PROPERTIES|TOKEN_PROPERTIES)
                        if s['op']=='untap_step':validate_untap_step(s)
    def test_full_source_pin(self):
        for name in self.pins['sources']:
            d=self.declaration(name)
            for key,value in [('oracle_text','changed'),('mana_cost','{19}'),('layout','transform'),('type_line','Token Creature')]:
                with self.assertRaises(ValueError):explicit_spec(d,dict(self.o[name],**{key:value}))
    def test_params_must_exactly_match_reviewed_profile(self):
        d=self.declaration('Battle Hymn')
        for change in [dict(wave=7),dict(wave=True),dict(extra=1),dict(modes=[])]:
            with self.assertRaises(ValueError):explicit_spec(dict(d,params=dict(d['params'],**change)),self.o['Battle Hymn'])
    def test_top_level_version2_only(self):
        d=self.declaration('Channel the Suns')
        for v in (1,3,True,2.0):
            with self.assertRaises(ValueError):explicit_spec(dict(d,version=v),self.o['Channel the Suns'])
    def test_nested_boolean_cost_aliases_rejected_publicly(self):
        for name in self.pins['sources']:
            d=self.declaration(name)
            for mode in range(len(d['params']['modes'])):
                for field in ('input','pool'):
                    for color,value in enumerate(d['params']['modes'][mode][field]):
                        if value not in (0,1):continue
                        changed=copy.deepcopy(d);changed['params']['modes'][mode][field][color]=bool(value)
                        with self.assertRaises(ValueError,msg=(name,mode,field,color)):
                            explicit_spec(changed,self.o[name])
    def test_independent_witnesses(self):
        m.validate_witnesses(self.o,self.rows)
        for name in ('Twiddle','Negate','Murder','Grizzly Bears','Swamp'):
            changed=copy.deepcopy(self.o);changed[name]['oracle_text']='changed'
            with self.assertRaises(ValueError):m.validate_witnesses(changed,self.rows)
    def test_source_spell_pending_and_payment(self):
        for owner in (0,1):
            c=self.cases('Channel the Suns',[f'effect_{owner}'])[0];ss=c['steps'];n=next(i for i,s in enumerate(ss) if s['op']=='cast' and s['ref']=='source')
            self.assertEqual(ss[n+1],dict(op='assert_stack',value=1))
            receipt=ss[n+2:n+8];self.assertEqual([s['value'] for s in receipt],[0]*6)
            self.assertTrue(all(s['player']==owner for s in receipt))
            self.assertTrue(any(s==dict(op='assert_mana',player=owner,color=5,value=1) for s in ss[n+8:]))
    def test_no_hypothetical_mana_ability_resolution(self):
        for name,profile in self.pins['sources'].items():
            if profile['params']['model']!='permanent':continue
            for c in self.cases(name):
                ss=c['steps']
                for i,s in enumerate(ss):
                    if s['op']=='activate' and s['value']==1:
                        self.assertEqual(ss[i+1]['op'],'assert_choices_consumed');self.assertEqual(ss[i+2],dict(op='assert_stack',value=0))
    def test_ai_sacrifice_mode_is_honest_gap(self):
        d=self.declaration('Crystal Vein');self.assertNotIn('effect_m1_1',d['variants']);self.assertIn('effect_m1_0',d['variants'])
        self.assertIn('effect_m0_1',d['variants']);self.assertTrue(d['params']['gaps'])
    def test_zero_input_receipt_not_duplicated_as_separate_payment_case(self):
        for name in ('Crystal Vein','Basal Thrull','Tainted Field','Hickory Woodlot'):
            self.assertFalse(any(v.startswith('payment_m') for v in self.declaration(name)['variants']))
    def test_nontapping_sacrifice_works_from_sick_and_tapped_boards(self):
        for name in ('Blood Vassal','Composite Golem','Coal Golem','Satyr Hedonist'):
            for v in ('sick','tapped'):
                for owner in (0,1):
                    ss=self.cases(name,[f'{v}_{owner}'])[0]['steps']
                    self.assertIn(dict(op='activate',ref='source',value=1),ss)
                    self.assertIn(dict(op='assert_card',ref='source',property='in_play',value=0),ss)
    def test_paid_activation_denials_are_real_and_leave_source(self):
        for name in ('Coal Golem','Satyr Hedonist'):
            for owner in (0,1):
                ss=self.cases(name,[f'insufficient_input_m0_{owner}'])[0]['steps']
                self.assertIn(dict(op='require_can_activate',ref='source',value=0),ss)
                self.assertIn(dict(op='activate',ref='source',value=0),ss)
                self.assertIn(dict(op='assert_card',ref='source',property='in_play',value=1),ss)
    def test_priest_cast_denial_has_no_usable_owned_mana_sources(self):
        for owner in (0,1):
            c=self.cases('Priest of Titania',[f'insufficient_cast_mana_{owner}'])[0]
            own=[x for x in c['setup'] if x['player']==owner and x['zone']=='battlefield' and x['name']=='Elvish Mystic']
            self.assertEqual(len(own),2);self.assertTrue(all(x['state']==['tapped'] for x in own))
            foreign=[x for x in c['setup'] if x['player']==1-owner and x['name']=='Elvish Mystic']
            self.assertEqual(len(foreign),1);self.assertFalse(foreign[0].get('state'))
    def test_ramos_ai_sacrifice_uses_actual_tap_not_an_absent_menu(self):
        ss=self.cases('Eye of Ramos',['effect_m1_1'])[0]['steps']
        self.assertIn(dict(op='cast',ref='bind_sacrifice',value=1),ss)
        self.assertIn(dict(op='choose_dialog',player=1,value=0),ss) # Twiddle tap choice
        self.assertFalse(any(s['op']=='choose_dialog' and s.get('player')==1 and s['value']==1 for s in ss))
        self.assertIn(dict(op='assert_zone_multiplicity',player=1,zone='graveyard',index=self.rows['Eye of Ramos']['csvid'],value=1),ss)
    def test_life_cost_and_damage_have_different_prevention_receipts(self):
        for owner in (0,1):
            for name,life in [('Blightsoil Druid',19),('Elves of Deep Shadow',20)]:
                ss=self.cases(name,[f'prevention_{owner}'])[0]['steps']
                self.assertIn(dict(op='cast',ref='prevent',value=1),ss)
                self.assertEqual([s['value'] for s in ss if s['op']=='assert_life' and s['player']==owner][-1],life)
    def test_ramos_tap_keeps_its_distinct_sacrifice_ability_available(self):
        for name in ('Eye of Ramos','Heart of Ramos','Horn of Ramos','Skull of Ramos','Tooth of Ramos'):
            for owner in (0,1):
                ss=self.cases(name,[f'tap_then_sacrifice_m0_{owner}'])[0]['steps']
                self.assertEqual(sum(s['op']=='activate' and s['value']==1 for s in ss),2)
                self.assertFalse(any(s==dict(op='require_can_activate',ref='source',value=0) for s in ss))
                self.assertIn(dict(op='assert_zone_multiplicity',player=owner,zone='graveyard',index=self.rows[name]['csvid'],value=1),ss)
    def test_threshold_source_does_not_count_itself_while_resolving(self):
        for owner in (0,1):
            for variant,n in [('grave_six',3),('grave_seven',5),('opponent_grave_seven',3)]:
                ss=self.cases('Cabal Ritual',[f'{variant}_{owner}'])[0]['steps']
                self.assertIn(dict(op='assert_mana',player=owner,color=1,value=n),ss)
    def test_initial_grave_refs_are_not_active_card_references(self):
        for c in self.cases('Cabal Ritual'):
            grave={x['ref'] for x in c['setup'] if x['zone']=='graveyard'}
            self.assertTrue(grave)
            self.assertFalse(any(s['op'] in ('assert_card','require_card') and s.get('ref') in grave for s in c['steps']))
            self.assertTrue(any(s['op']=='assert_zone_multiplicity' and s['zone']=='graveyard' for s in c['steps']))
    def test_counted_permanent_loss_happens_before_mana_activation(self):
        for owner in (0,1):
            ss=self.cases("Gaea's Cradle",[f'count_removed_{owner}'])[0]['steps']
            kill=next(i for i,s in enumerate(ss) if s['op']=='cast' and s['ref']=='remove_count')
            activation=next(i for i,s in enumerate(ss) if s['op']=='activate')
            self.assertLess(kill,activation)
            self.assertIn(dict(op='assert_mana',player=owner,color=3,value=1),ss)
    def test_power_derived_mana_uses_real_pump(self):
        for owner in (0,1):
            ss=self.cases('Viridian Joiner',[f'pumped_{owner}'])[0]['steps']
            self.assertIn(dict(op='cast',ref='pump',value=1),ss)
            self.assertIn(dict(op='assert_mana',player=owner,color=3,value=4),ss)
            self.assertIn(dict(op='assert_card',ref='source',property='power',value=4),ss)
    def test_conditional_thresholds_use_actual_controlled_world(self):
        for owner in (0,1):
            c=self.cases('Circle of Elders',[f'absent_condition_{owner}'])[0]
            power=sum(int(self.o[x['name']]['power']) for x in c['setup'] if x['player']==owner and x['zone']=='battlefield' and 'Creature' in self.o[x['name']]['type_line'])
            self.assertEqual(power,7)
            self.assertIn(dict(op='require_can_activate',ref='source',value=0),c['steps'])
            c=self.cases('Temple of the False God',[f'absent_condition_{owner}'])[0]
            self.assertEqual(sum(x['player']==owner and x['zone']=='battlefield' and 'Land' in self.o[x['name']]['type_line'] for x in c['setup']),4)
    def test_threshold_ferocious_is_individual_not_total_power(self):
        for owner in (0,1):
            c=self.cases('Whisperer of the Wilds',[f'effect_m0_{owner}'])[0]
            powers=[int(self.o[x['name']]['power']) for x in c['setup'] if x['player']==owner and x['zone']=='battlefield' and 'Creature' in self.o[x['name']]['type_line']]
            self.assertEqual(sum(powers),4);self.assertLess(max(powers),4)
    def test_rite_second_cast_spends_first_output_and_excludes_resolving_copy(self):
        for owner in (0,1):
            ss=self.cases('Rite of Flame',[f'two_copies_{owner}'])[0]['steps']
            n=next(i for i,s in enumerate(ss) if s['op']=='cast' and s['ref']=='second_rite')
            self.assertIn(dict(op='assert_mana',player=owner,color=4,value=5),ss[:n])
            self.assertIn(dict(op='assert_mana',player=owner,color=4,value=4),ss[n:])
            self.assertIn(dict(op='assert_mana',player=owner,color=4,value=10),ss[n:])
    def test_spoils_union_and_life_are_mandatory(self):
        for owner in (0,1):
            ss=self.cases('Spoils of Evil',[f'effect_{owner}'])[0]['steps']
            self.assertIn(dict(op='assert_mana',player=owner,color=0,value=3),ss)
            self.assertIn(dict(op='assert_life',player=owner,value=23),ss)
    def test_zhur_asserts_settled_trigger_damage_without_fabricated_pending(self):
        for owner in (0,1):
            for v,life in [('effect_m0',19),('prevention',20),('incidental_tap',20)]:
                ss=self.cases('Zhur-Taa Druid',[f'{v}_{owner}'])[0]['steps']
                self.assertEqual([s['value'] for s in ss if s['op']=='assert_life' and s['player']==1-owner][-1],life)
            self.assertTrue(self.declaration('Zhur-Taa Druid')['params']['gaps'])
    def test_depletion_two_real_activations_and_death(self):
        for owner in (0,1):
            c=self.cases('Hickory Woodlot',[f'exhaustion_{owner}'])[0];ss=c['steps']
            self.assertEqual(sum(s['op']=='activate' and s['value']==1 for s in ss),2)
            self.assertIn(dict(op='assert_card',ref='source',property='in_play',value=0),ss)
            self.assertIn(dict(op='assert_zone_multiplicity',player=owner,zone='graveyard',index=self.rows['Hickory Woodlot']['csvid'],value=1),ss)
    def test_conditional_opponent_swamp_not_own(self):
        for owner in (0,1):
            c=self.cases('Tainted Field',[f'opposing_condition_{owner}'])[0]
            self.assertEqual(next(x['player'] for x in c['setup'] if x['ref']=='condition'),1-owner)
            self.assertFalse(any(s['op']=='choose_color' for s in c['steps']))
    def test_resolution_time_creature_loss_real_response(self):
        for owner in (0,1):
            ss=self.cases('Battle Hymn',[f'response_loss_{owner}'])[0]['steps']
            self.assertIn(dict(op='cast_response',ref='response',value=1),ss)
            self.assertIn(dict(op='assert_mana',player=owner,color=4,value=1),ss)
            self.assertEqual(sum(s['op']=='resolve_response' for s in ss),1)
    def test_counter_response_uses_its_actual_sole_parent_context(self):
        for name in ('Channel the Suns','Battle Hymn'):
            for owner in (0,1):
                ss=self.cases(name,[f'countered_{owner}'])[0]['steps']
                self.assertIn(dict(op='cast_response',ref='counter',value=1),ss)
                self.assertFalse(any(s['op']=='choose_target' for s in ss))
                self.assertTrue(any(s['op']=='assert_zone_multiplicity' and s['player']==owner and s['zone']=='graveyard' and s['index']==self.rows[name]['csvid'] and s['value']==1 for s in ss))
    def test_replay_occurrences_and_owners_are_independent(self):
        for owner in (0,1):
            c=self.cases('Channel the Suns',[f'pending_snapshot_{owner}'])[0];ss=c['steps']
            self.assertEqual(len(ss),len({id(s) for s in ss}))
            receipts=[s for s in ss if s==dict(op='assert_mana',player=owner,color=5,value=1)]
            self.assertGreaterEqual(len(receipts),4)

if __name__=='__main__':unittest.main()
