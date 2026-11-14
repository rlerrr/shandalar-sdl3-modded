"""Rules, legality, cost and receipt guards for combat-player value mechanisms."""
import copy
import json
import unittest
from pathlib import Path

from generate_cases import registry
import combat_value_trigger_recipes as model


class CombatValueTriggerTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.oracle=json.loads((Path(__file__).parent/'data/oracle.json').read_text(encoding='utf8'))['cards']
        cls.registry={r['name']:r for r in registry()}

    def declaration(self,name):
        return dict(family=model.FAMILY,version=2,oracle_text=self.oracle[name]['oracle_text'],
                    params=model.parameters(self.oracle[name]),variants=['unblocked_0'])

    def cases(self,name,selected):
        d=self.declaration(name);d['variants']=selected;p=model.spec(d,self.oracle[name])
        return model.expand(name,self.registry[name]['csvid'],d,p,selected,self.registry,self.oracle)

    def has(self,case,op,**fields):
        self.assertIn(dict(op=op,**fields),case['steps'])

    def test_primary_effects_are_derived_from_the_complete_printed_clause(self):
        expected={'Scroll Thief':dict(draw=1,discard=0,mill=False,self_counters=0),
                  'Dimir Cutpurse':dict(draw=1,discard=1,mill=False,self_counters=0),
                  'Blazing Specter':dict(draw=0,discard=1,mill=False,self_counters=0),
                  'Crosstown Courier':dict(draw=0,discard=0,mill=True,self_counters=0),
                  'Slith Ascendant':dict(draw=0,discard=0,mill=False,self_counters=1),
                  'Falkenrath Marauders':dict(draw=0,discard=0,mill=False,self_counters=2)}
        for name,effect in expected.items():self.assertEqual(model.parameters(self.oracle[name])['effects'],effect)
        for name in ('Havengul Vampire','Arcbound Slith','Markov Blademaster'):
            with self.assertRaises(ValueError):model.parameters(self.oracle[name])

    def test_fail_closed_params_types_whole_text_variants_and_witnesses(self):
        d=self.declaration('Slith Ascendant')
        for changed in (dict(oracle_text=d['oracle_text']+' '),dict(family='creature_keywords'),dict(version=2.0),dict(version=True),dict(variants=[]),
                        dict(variants=['unblocked_0','unblocked_0']),dict(variants=['invented_0'])):
            with self.assertRaises(ValueError):model.spec(dict(d,**changed),self.oracle['Slith Ascendant'])
        changed=copy.deepcopy(d);changed['params']['effects']['self_counters']=True
        with self.assertRaises(ValueError):model.spec(changed,self.oracle['Slith Ascendant'])
        for name,field,value in (('Wall of Denial','toughness','7'),('Hydrosurge','oracle_text','Target creature gets -4/-0 until end of turn.'),('Giant Growth','mana_cost','{1}')):
            changed=copy.deepcopy(self.oracle);changed[name][field]=value
            with self.assertRaisesRegex(ValueError,'combat value witness'):model.validate_witnesses(changed,self.registry)

    def test_fixed_draw_and_actual_damage_mill_have_independent_quantities(self):
        draw=self.cases('Scroll Thief',['pump_0'])[0]
        self.has(draw,'assert_life',player=1,value=16)
        self.has(draw,'assert_zone',player=0,zone='library',value=11)
        mill=self.cases('Crosstown Courier',['pump_1'])[0]
        self.has(mill,'assert_life',player=0,value=15)
        self.has(mill,'assert_zone',player=0,zone='library',value=7)
        self.has(mill,'assert_zone',player=0,zone='graveyard',value=6)

    def test_counter_growth_is_after_damage_persistent_and_adds_per_hit(self):
        c=self.cases('Falkenrath Marauders',['pump_0'])[0]
        self.has(c,'assert_life',player=1,value=15)
        self.has(c,'assert_card',ref='source',property='power',value=7)
        self.has(c,'assert_card',ref='source',property='power',value=4)
        self.has(c,'assert_card',ref='source',property='plus1_counters',value=2)
        c=self.cases('Falkenrath Marauders',['two_hits_1'])[0]
        self.has(c,'assert_life',player=0,value=14)
        self.has(c,'assert_card',ref='source',property='plus1_counters',value=4)
        self.has(c,'assert_card',ref='source',property='power',value=6)
        self.assertEqual(sum(s['op']=='advance_turn' for s in c['steps']),2)
        self.assertEqual(sum(s['op']=='untap_step' for s in c['steps']),2)
        self.assertEqual(sum(s['op']=='combat_damage' for s in c['steps']),2)

    def test_blocked_zero_damage_and_other_attacker_do_not_create_source_value(self):
        for name in ('Scroll Thief','Slith Ascendant','Crosstown Courier'):
            for variant in ('blocked_0','zero_power_0'):
                c=self.cases(name,[variant])[0]
                self.has(c,'assert_life',player=1,value=20)
                self.has(c,'assert_zone',player=0,zone='library',value=12)
                self.has(c,'assert_zone',player=1,zone='library',value=12)
                self.has(c,'assert_card',ref='source',property='plus1_counters',value=0)
        c=self.cases('Scroll Thief',['other_attack_1'])[0]
        self.has(c,'assert_life',player=0,value=17)
        self.has(c,'assert_zone',player=1,zone='library',value=12)
        c=self.cases('Scroll Thief',['attack_pair_0'])[0]
        self.has(c,'assert_life',player=1,value=16)
        self.has(c,'assert_zone',player=0,zone='library',value=11)

    def test_discard_empty_hand_does_not_suppress_the_separate_draw_sentence(self):
        c=self.cases('Dimir Cutpurse',['empty_hand_0'])[0]
        self.has(c,'assert_zone',player=1,zone='hand',value=0)
        self.has(c,'assert_zone',player=0,zone='library',value=11)
        self.has(c,'assert_zone_multiplicity',player=0,zone='hand',index=self.registry['Forest']['csvid'],value=4)
        for variant,count in (('empty_library_1',1),('short_library_1',2)):
            c=self.cases('Crosstown Courier',[variant])[0]
            self.has(c,'assert_zone',player=0,zone='library',value=0)
            self.has(c,'assert_zone',player=0,zone='graveyard',value=count)
            self.has(c,'assert_life',player=0,value=18)

    def test_real_casting_payment_and_fresh_haste_observe_legality(self):
        for owner in (0,1):
            c=self.cases('Scroll Thief',[f'payment_{owner}'])[0]
            cast=c['steps'].index(dict(op='cast',ref='source',value=1));resolve=c['steps'].index(dict(op='resolve'))
            self.assertLess(cast,resolve)
            self.assertIn(dict(op='assert_mana_total',player=owner,value=0),c['steps'][cast:resolve])
            self.assertIn(dict(op='assert_card',ref='source',property='in_hand',value=0),c['steps'][cast:resolve])
            for name,legal in (('Scroll Thief',0),('Blazing Specter',1)):
                c=self.cases(name,[f'fresh_attack_{owner}'])[0]
                self.has(c,'attack',ref='source',value=legal)
                self.assertLess(c['steps'].index(dict(op='cast',ref='source',value=1)),c['steps'].index(dict(op='begin_attackers')))
                self.assertTrue(all(s['player']==owner for s in c['steps'] if s['op']=='turn'))

    def test_zone_change_resets_counters_and_combat_snapshots_replay_real_effects(self):
        c=self.cases('Slith Ascendant',['bounce_after_1'])[0]
        self.has(c,'bind_hand',player=1,value=self.registry['Slith Ascendant']['csvid'],ref='source')
        self.has(c,'assert_card',ref='source',property='plus1_counters',value=1)
        self.has(c,'assert_card',ref='source',property='plus1_counters',value=0)
        for variant,attacks in (('ready_snapshot_0',2),('combat_snapshot_1',1)):
            c=self.cases('Falkenrath Marauders',[variant])[0]
            self.assertEqual(sum(s['op']=='attack' and s.get('ref')=='source' for s in c['steps']),attacks)
            self.assertEqual(sum(s['op']=='combat_damage' for s in c['steps']),2)
            self.has(c,'assert_recorded_state')
            self.assertEqual(len(c['steps']),len({id(s) for s in c['steps']}))

    def test_two_hits_enter_main_only_after_two_real_turns_and_untaps(self):
        for owner in (0,1):
            c=self.cases('Falkenrath Marauders',[f'two_hits_{owner}'])[0]
            turns=[i for i,s in enumerate(c['steps']) if s['op']=='advance_turn']
            self.assertEqual(len(turns),2)
            self.assertEqual(c['steps'][turns[0]+1],dict(op='untap_step',player=1-owner))
            self.assertEqual(c['steps'][turns[1]+1],dict(op='untap_step',player=owner))
            self.assertEqual(c['steps'][turns[1]+2],dict(op='turn',player=owner,value=0))
            self.assertEqual(c['steps'][turns[1]+3],dict(op='begin_attackers'))
            self.assertEqual([s for s in c['steps'] if s['op']=='turn'],[dict(op='turn',player=owner,value=0)]*2)
            self.assertTrue(all(any(s==dict(op='set_mana',player=player,color=color,value=0) for s in c['steps'][:turns[0]]) for player in (0,1) for color in range(6)))

    def test_all_pilot_variants_expand_without_mutating_their_inputs(self):
        for name in ('Scroll Thief','Crosstown Courier','Dimir Cutpurse','Blazing Specter','Slith Ascendant','Falkenrath Marauders'):
            d=self.declaration(name);p=model.spec(d,self.oracle[name]);d['variants']=model.variants(p)
            before=copy.deepcopy((d,p,self.oracle));cases=model.expand(name,self.registry[name]['csvid'],d,p,d['variants'],self.registry,self.oracle)
            self.assertEqual(before,(d,p,self.oracle));self.assertEqual(len(cases),len(d['variants']))
            self.assertTrue(all(c['_recipe']['untested_components'] for c in cases))

    def test_adjacent_optional_draw_and_exchange_order(self):
        accepted,declined=self.cases('Shadowmage Infiltrator',['unblocked_0','declined_0'])
        self.has(accepted,'choose_trigger',player=0,ref='source')
        self.has(accepted,'assert_zone',player=0,zone='library',value=11)
        self.has(declined,'assert_zone',player=0,zone='library',value=12)
        self.has(declined,'assert_life',player=1,value=19)
        loot=self.cases('Jeskai Elder',['empty_source_hand_0'])[0]
        self.has(loot,'assert_zone',player=0,zone='hand',value=0)
        self.has(loot,'assert_zone',player=0,zone='library',value=11)
        self.has(loot,'assert_zone_multiplicity',player=0,zone='graveyard',index=self.registry['Forest']['csvid'],value=1)
        rummage=self.cases('Academy Raider',['empty_source_hand_0'])[0]
        self.has(rummage,'assert_zone',player=0,zone='library',value=12)
        self.assertFalse(any(s['op']=='choose_dialog' for s in rummage['steps']))
        declined=self.cases('Academy Raider',['declined_1'])[0]
        self.has(declined,'choose_dialog',player=1,value=1)

    def test_adjacent_actual_damage_quantities_and_prowess_helper_costs(self):
        c=self.cases('Glint-Eye Nephilim',['pump_0'])[0]
        self.has(c,'assert_life',player=1,value=15)
        self.has(c,'assert_zone',player=0,zone='library',value=7)
        c=self.cases('Diviner Spirit',['unblocked_1'])[0]
        for player in (0,1):self.has(c,'assert_zone',player=player,zone='library',value=10)
        c=self.cases('Jeskai Elder',['pump_0'])[0]
        self.has(c,'assert_life',player=1,value=15)
        self.has(c,'assert_card',ref='source',property='power',value=5)
        self.has(c,'assert_card',ref='source',property='power',value=1)

    def test_printed_creator_defines_zombie_stats_not_the_star_token_record(self):
        self.assertEqual(self.oracle['Zombie']['power'],'*')
        c=self.cases("Liliana's Reaver",['unblocked_0'])[0]
        for prop,value in (('power',2),('toughness',2),('tapped',1)):
            self.has(c,'assert_card',ref='made_0',property=prop,value=value)
        for prop,value in (('token',1),('color',2),('subtype_Zombie',1)):
            self.has(c,'assert_matching_permanents',player=0,index=18860,property=prop,value=value)
        self.has(c,'bind_permanent_copy',player=0,value=18860,index=0,ref='made_0')
        self.has(c,'assert_zone',player=1,zone='hand',value=2)
        c=self.cases("Liliana's Reaver",['blocked_0'])[0]
        self.has(c,'assert_card',ref='wall',property='in_play',value=0)
        changed=copy.deepcopy(self.registry);changed['Zombie']['csvid']+=1
        with self.assertRaisesRegex(ValueError,'token registry bridge'):model.validate_witnesses(self.oracle,changed)

    def test_real_grant_pays_creature_tap_cost_and_distinguishes_damage_scope(self):
        for owner in (0,1):
            c=self.cases('Scroll Thief',[f'noncombat_player_{owner}'])[0]
            self.has(c,'cast',ref='grant',value=1);self.has(c,'activate',ref='grant',value=1)
            self.has(c,'assert_card',ref='source',property='tapped',value=1)
            self.has(c,'assert_card',ref='grant',property='tapped',value=0)
            self.has(c,'assert_zone',player=owner,zone='library',value=12)
            self.has(c,'assert_life',player=1-owner,value=19)
            self.assertFalse(any(s['op'] in ('combat_damage','set_life') for s in c['steps']))
            c=self.cases('Abyssal Specter',[f'noncombat_self_{owner}'])[0]
            self.has(c,'assert_life',player=owner,value=19)
            self.has(c,'assert_zone',player=owner,zone='hand',value=2)
            c=self.cases('Scroll Thief',[f'noncombat_fresh_{owner}'])[0]
            self.has(c,'activate',ref='grant',value=0)
            self.has(c,'assert_card',ref='source',property='sick',value=1)
            c=self.cases('Falkenrath Marauders',[f'noncombat_fresh_{owner}'])[0]
            self.has(c,'activate',ref='grant',value=1)

    def test_adjacent_variants_and_boundary_snapshots_preserve_inputs(self):
        names=['Shadowmage Infiltrator','Diviner Spirit','Glint-Eye Nephilim','Academy Raider',"Liliana's Reaver",'Niv-Mizzet, Dracogenius','Jeskai Elder','Jhessian Thief']
        for name in names:
            d=self.declaration(name);p=model.spec(d,self.oracle[name]);d['variants']=model.variants(p)
            before=copy.deepcopy((d,p));cases=model.expand(name,self.registry[name]['csvid'],d,p,d['variants'],self.registry,self.oracle)
            self.assertEqual(before,(d,p));self.assertEqual(len(cases),len(d['variants']))
        c=self.cases('Abyssal Specter',['noncombat_snapshot_1'])[0]
        self.assertEqual(sum(s['op']=='cast' and s.get('ref')=='grant' for s in c['steps']),1)
        self.assertEqual(sum(s['op']=='activate' and s.get('ref')=='grant' for s in c['steps']),2)
        self.has(c,'assert_recorded_state')

    def test_team_draw_and_counters_belong_to_actual_damager_or_controller(self):
        c=self.cases('Necropolis Regent',['pump_0'])[0]
        self.has(c,'assert_life',player=1,value=15)
        self.has(c,'assert_card',ref='ally',property='plus1_counters',value=5)
        self.has(c,'assert_card',ref='source',property='plus1_counters',value=0)
        c=self.cases('Rakish Heir',['pump_1'])[0]
        self.has(c,'assert_life',player=0,value=14)
        self.has(c,'assert_card',ref='ally',property='plus1_counters',value=1)
        self.has(c,'assert_card',ref='ally',property='power',value=4)
        c=self.cases('Synapse Sliver',['enemy_hit_0'])[0]
        self.has(c,'choose_dialog',player=1,value=0)
        self.has(c,'assert_zone',player=1,zone='hand',value=4)
        self.has(c,'assert_zone',player=0,zone='hand',value=3)
        self.has(c,'assert_life',player=0,value=18)
        c=self.cases('Rakish Heir',['enemy_hit_0'])[0]
        self.has(c,'assert_card',ref='enemy_ally',property='plus1_counters',value=0)

    def test_team_scope_controls_real_damage_and_independent_per_attacker_values(self):
        c=self.cases('Rakish Heir',['wrong_type_hit_0'])[0]
        self.has(c,'assert_life',player=1,value=17)
        self.has(c,'assert_card',ref='idle',property='plus1_counters',value=0)
        c=self.cases('Necropolis Regent',['source_pair_0'])[0]
        self.has(c,'assert_life',player=1,value=12)
        self.has(c,'assert_card',ref='source',property='plus1_counters',value=6)
        self.has(c,'assert_card',ref='ally',property='plus1_counters',value=2)
        c=self.cases('Necropolis Regent',['two_hits_1'])[0]
        self.has(c,'assert_life',player=0,value=14)
        self.has(c,'assert_card',ref='ally',property='plus1_counters',value=6)
        self.assertEqual(sum(s['op']=='advance_turn' for s in c['steps']),2)
        for name in ('Necropolis Regent','Rakish Heir','Synapse Sliver','Seshiro the Anointed'):
            for mode in ('blocked','zero_power','noncombat_ally','noncombat_source'):
                c=self.cases(name,[mode+'_0'])[0]
                self.has(c,'assert_card',ref='source',property='plus1_counters',value=0)
                self.has(c,'assert_card',ref='ally',property='plus1_counters',value=0)
                self.has(c,'assert_zone',player=0,zone='library',value=12)
                if mode.startswith('noncombat_'):
                    ref='ally' if mode=='noncombat_ally' else 'source'
                    self.has(c,'assert_card',ref=ref,property='tapped',value=1)
                    self.has(c,'assert_card',ref='grant',property='tapped',value=0)
                    self.has(c,'assert_mana_total',player=0,value=58)

    def test_other_snake_bonus_and_real_bounce_have_correct_scope(self):
        c=self.cases('Seshiro the Anointed',['source_bounce_0'])[0]
        self.has(c,'assert_life',player=1,value=17)
        self.has(c,'assert_card',ref='source',property='power',value=3)
        self.has(c,'assert_card',ref='ally',property='power',value=3)
        self.has(c,'assert_card',ref='ally',property='power',value=1)
        self.has(c,'assert_card',ref='enemy_ally',property='power',value=1)
        self.has(c,'assert_card',ref='idle',property='power',value=3)
        self.has(c,'cast',ref='helper',value=1)
        self.has(c,'assert_card',ref='source',property='in_play',value=0)
        c=self.cases('Seshiro the Anointed',['two_attackers_1'])[0]
        self.has(c,'choose_dialog',player=1,value=0)
        self.has(c,'assert_life',player=0,value=14)
        self.has(c,'assert_zone',player=1,zone='library',value=10)

    def test_team_full_oracle_and_optional_dialogs_fail_closed(self):
        for name in ('Necropolis Regent','Rakish Heir','Synapse Sliver','Seshiro the Anointed'):
            d=self.declaration(name);p=model.spec(d,self.oracle[name]);d['variants']=model.variants(p)
            before=copy.deepcopy((d,p));cases=model.expand(name,self.registry[name]['csvid'],d,p,d['variants'],self.registry,self.oracle)
            self.assertEqual(before,(d,p));self.assertEqual(len(cases),len(d['variants']))
            for c in cases:
                self.assertEqual(len(c['setup']),len({x['ref'] for x in c['setup']}))
            changed=copy.deepcopy(d);changed['params']['team']['controller']='any' if p['team']['controller']=='own' else 'own'
            with self.assertRaises(ValueError):model.spec(changed,self.oracle[name])
        for name in ('Synapse Sliver','Seshiro the Anointed'):
            for owner in (0,1):
                c=self.cases(name,[f'declined_{owner}'])[0]
                self.has(c,'present_ai_dialogs',value=1)
                self.has(c,'choose_dialog',player=owner,value=2)
                self.has(c,'assert_zone',player=owner,zone='library',value=12)
                self.assertFalse(any(s['op']=='choose_trigger' for s in c['steps']))

    def test_team_expansion_uses_printed_haste_devoid_costs_and_actual_input_stage(self):
        c=self.cases('Shaman of the Great Hunt',['fresh_attack_1'])[0]
        self.has(c,'cast',ref='source',value=1);self.has(c,'attack',ref='source',value=1)
        self.has(c,'assert_life',player=0,value=16)
        self.has(c,'assert_card',ref='source',property='plus1_counters',value=1)
        c=self.cases('Shaman of the Great Hunt',['pump_0'])[0]
        self.has(c,'assert_life',player=1,value=15)
        self.has(c,'assert_card',ref='ally',property='plus1_counters',value=1)
        c=self.cases('Deepfathom Skulker',['payment_0'])[0]
        self.has(c,'assert_mana_total',player=0,value=0)
        self.has(c,'set_mana',player=0,color=2,value=1)
        self.has(c,'assert_matching_permanents',player=0,index=15583,property='color',value=0)
        for owner in (0,1):
            c=self.cases('Deepfathom Skulker',[f'two_attackers_{owner}'])[0]
            self.has(c,'assert_zone',player=owner,zone='library',value=10)
            self.assertEqual(sum(s['op']=='choose_trigger' for s in c['steps']),2 if owner==0 else 0)
            self.assertFalse(any(s['op']=='choose_dialog' for s in c['steps']))
        for name in ('Shaman of the Great Hunt','Deepfathom Skulker'):
            d=self.declaration(name);p=model.spec(d,self.oracle[name]);d['variants']=model.variants(p)
            self.assertTrue(p['inactive_abilities']);before=copy.deepcopy((d,p))
            cases=model.expand(name,self.registry[name]['csvid'],d,p,d['variants'],self.registry,self.oracle)
            self.assertEqual(before,(d,p));self.assertEqual(len(cases),len(d['variants']))
        self.assertNotIn('declined_1',model.variants(p))
        changed=copy.deepcopy(self.oracle);changed['Deepfathom Skulker']['colors']=['U']
        with self.assertRaises(ValueError):model.parameters(changed['Deepfathom Skulker'])

    def test_renown_printed_quantity_is_once_per_permanent_not_every_hit(self):
        c=self.cases('Citadel Castellan',['two_hits_0'])[0]
        self.has(c,'assert_life',player=1,value=18);self.has(c,'assert_life',player=1,value=14)
        self.has(c,'assert_card',ref='source',property='plus1_counters',value=2)
        self.assertNotIn(dict(op='assert_card',ref='source',property='plus1_counters',value=4),c['steps'])
        self.assertEqual(sum(s['op']=='advance_turn' for s in c['steps']),2)
        c=self.cases('Citadel Castellan',['pump_1'])[0]
        self.has(c,'assert_life',player=0,value=15)
        self.has(c,'assert_card',ref='source',property='plus1_counters',value=2)
        self.has(c,'assert_card',ref='source',property='power',value=7)
        self.has(c,'assert_card',ref='source',property='power',value=4)
        for name in ('Knight of the Pilgrim\'s Road','Citadel Castellan'):
            bad=copy.deepcopy(self.oracle[name]);bad['oracle_text']=bad['oracle_text'].replace('Renown 1','Renown 2') if 'Renown 1' in bad['oracle_text'] else bad['oracle_text'].replace('two +1/+1 counters','a +1/+1 counter')
            with self.assertRaises(ValueError):model.parameters(bad)

    def test_actual_counter_removal_preserves_renown_and_new_permanent_resets_it(self):
        for owner in (0,1):
            c=self.cases('Citadel Castellan',[f'hexmage_remove_{owner}'])[0]
            activate=next(i for i,s in enumerate(c['steps']) if s.get('ref')=='remover' and s['op']=='activate')
            self.assertEqual(sum(s['op']=='combat_damage' for s in c['steps'][:activate]),1)
            self.has(c,'choose_target',player=1-owner,ref='source')
            self.has(c,'assert_card',ref='remover',property='in_play',value=0)
            self.has(c,'assert_card',ref='source',property='plus1_counters',value=0)
            self.has(c,'assert_life',player=1-owner,value=16)
            self.assertEqual(sum(s['op']=='combat_damage' for s in c['steps'][activate:]),1)
            c=self.cases('Citadel Castellan',[f'bounce_recast_{owner}'])[0]
            self.has(c,'cast',ref='helper',value=1);self.has(c,'bind_hand',player=owner,value=15100,ref='source')
            self.has(c,'cast',ref='source',value=1)
            self.has(c,'assert_card',ref='source',property='plus1_counters',value=0)
            self.has(c,'assert_card',ref='source',property='plus1_counters',value=2)
            self.has(c,'assert_life',player=1-owner,value=16)
            self.assertEqual(sum(s['op']=='advance_turn' for s in c['steps']),2)
        c=self.cases('Citadel Castellan',['counter_before_hit_0'])[0]
        self.has(c,'reset_death_history');self.has(c,'cast',ref='helper',value=1)
        self.has(c,'assert_life',player=1,value=17)
        self.has(c,'assert_card',ref='source',property='plus1_counters',value=3)

    def test_renown_keywords_follow_actual_damage_and_granted_damage_never_renowns(self):
        c=self.cases('Akroan Sergeant',['keyword_block_0'])[0]
        self.has(c,'assert_card',ref='source',property='in_play',value=1)
        self.has(c,'assert_card',ref='source',property='damage',value=0)
        self.has(c,'assert_card',ref='small_blocker',property='in_play',value=0)
        self.has(c,'assert_card',ref='source',property='plus1_counters',value=0)
        c=self.cases('Rhox Maulers',['keyword_block_1'])[0]
        self.has(c,'assert_life',player=0,value=18)
        self.has(c,'assert_card',ref='source',property='plus1_counters',value=2)
        self.has(c,'assert_card',ref='source',property='damage',value=2)
        c=self.cases('War Oracle',['keyword_block_0'])[0]
        self.has(c,'assert_life',player=0,value=23);self.has(c,'assert_life',player=1,value=20)
        self.has(c,'assert_card',ref='source',property='plus1_counters',value=0)
        c=self.cases('Pharika\'s Disciple',['blocked_0'])[0]
        self.has(c,'assert_card',ref='wall',property='in_play',value=0)
        c=self.cases('Firefiend Elemental',['fresh_attack_1'])[0]
        self.has(c,'attack',ref='source',value=1);self.has(c,'assert_life',player=0,value=17)
        for mode,life0,life1 in (('noncombat_player',21,19),('noncombat_self',20,20),('noncombat_creature',21,20)):
            c=self.cases('War Oracle',[mode+'_0'])[0]
            self.has(c,'assert_life',player=0,value=life0);self.has(c,'assert_life',player=1,value=life1)
            self.has(c,'assert_card',ref='source',property='plus1_counters',value=0)

    def test_all_renown_pilot_variants_preserve_full_oracle_and_replay_removed_state(self):
        for name in ('Knight of the Pilgrim\'s Road','Citadel Castellan','Firefiend Elemental','Rhox Maulers','Pharika\'s Disciple','War Oracle','Akroan Sergeant'):
            d=self.declaration(name);p=model.spec(d,self.oracle[name]);d['variants']=model.variants(p);before=copy.deepcopy((d,p))
            cases=model.expand(name,self.registry[name]['csvid'],d,p,d['variants'],self.registry,self.oracle)
            self.assertEqual(before,(d,p));self.assertEqual(len(cases),len(d['variants']))
            self.assertEqual(d['oracle_text'],self.oracle[name]['oracle_text'])
            for c in cases:self.assertEqual(len(c['setup']),len({x['ref'] for x in c['setup']}))
        c=self.cases('Citadel Castellan',['removed_snapshot_1'])[0]
        self.assertEqual(sum(s['op']=='activate' and s.get('ref')=='remover' for s in c['steps']),1)
        self.assertEqual(sum(s['op']=='combat_damage' for s in c['steps']),3)
        self.has(c,'assert_recorded_state')

    def test_fixed_library_transfer_destinations_quantities_and_order_are_independent_of_damage(self):
        c=self.cases('Culling Drone',['pump_0'])[0]
        self.has(c,'assert_life',player=1,value=15)
        self.has(c,'assert_zone',player=1,zone='library',value=11)
        self.has(c,'assert_zone',player=1,zone='exile',value=2)
        self.has(c,'assert_zone',player=1,zone='graveyard',value=1)
        self.has(c,'assert_zone_multiplicity',player=1,zone='exile',index=self.registry['Forest']['csvid'],value=1)
        c=self.cases('Shriekgeist',['pump_1'])[0]
        self.has(c,'assert_life',player=0,value=16)
        self.has(c,'assert_zone',player=0,zone='library',value=10)
        self.has(c,'assert_zone',player=0,zone='graveyard',value=3)
        self.has(c,'assert_zone',player=0,zone='exile',value=1)
        self.has(c,'assert_zone_card',player=0,zone='library',index=0,value=self.registry['Grizzly Bears']['csvid'])
        c=self.cases('Culling Drone',['duplicate_hits_0'])[0]
        self.has(c,'assert_zone_multiplicity',player=1,zone='exile',index=self.registry['Forest']['csvid'],value=2)
        c=self.cases('Culling Drone',['last_card_two_hits_1'])[0]
        self.has(c,'assert_zone',player=0,zone='library',value=0)
        self.has(c,'assert_zone',player=0,zone='exile',value=2)
        self.assertEqual(sum(s['op']=='combat_damage' for s in c['steps']),2)
        for name,variant,count in (('Culling Drone','empty_library_0',1),('Shriekgeist','exact_library_0',3),('Shriekgeist','short_library_0',2)):
            c=self.cases(name,[variant])[0]
            self.has(c,'assert_zone',player=1,zone='library',value=0)
            self.has(c,'assert_zone',player=1,zone='exile' if name=='Culling Drone' else 'graveyard',value=count)

    def test_library_unblockable_deathtouch_devoid_and_actual_noncombat_source(self):
        c=self.cases('Benthic Infiltrator',['blocked_0'])[0]
        self.has(c,'block',ref='wall',other='source',value=0)
        self.has(c,'assert_life',player=1,value=19)
        self.has(c,'assert_zone',player=1,zone='exile',value=2)
        c=self.cases('Fathom Feeder',['blocked_1'])[0]
        self.has(c,'block',ref='wall',other='source',value=1)
        self.has(c,'assert_card',ref='wall',property='in_play',value=0)
        self.has(c,'assert_zone',player=0,zone='exile',value=1)
        c=self.cases('Culling Drone',['payment_0'])[0]
        self.has(c,'set_mana',player=0,color=1,value=1)
        self.has(c,'assert_mana_total',player=0,value=0)
        self.has(c,'assert_matching_permanents',player=0,index=15304,property='color',value=0)
        for name,moved in (('Reef Pirates',True),('Culling Drone',False)):
            c=self.cases(name,['noncombat_player_0'])[0]
            self.has(c,'cast',ref='grant',value=1);self.has(c,'activate',ref='grant',value=1)
            self.has(c,'assert_card',ref='source',property='tapped',value=1)
            self.has(c,'assert_zone',player=1,zone='library',value=11 if moved else 12)

    def test_dominator_actual_entry_condition_and_recast_are_not_seeded_life_effects(self):
        for owner in (0,1):
            for mode,value in (('entry_source_only',20),('entry_own_colorless',18),('entry_two_colorless',18),('entry_opponent_colorless',20),('entry_noncreature_colorless',20),('entry_own_colored',20),('entry_recast',16)):
                c=self.cases('Dominator Drone',[f'{mode}_{owner}'])[0]
                self.has(c,'cast',ref='source',value=1)
                self.has(c,'assert_life',player=1-owner,value=value)
                self.has(c,'assert_zone',player=1-owner,zone='library',value=12)
                if mode=='entry_recast':
                    self.assertEqual(sum(s['op']=='cast' and s.get('ref')=='source' for s in c['steps']),2)
                    self.has(c,'bind_hand',player=owner,value=15310,ref='source')
                    self.has(c,'assert_mana_total',player=owner,value=53)
            c=self.cases('Dominator Drone',[f'unblocked_{owner}'])[0]
            self.has(c,'assert_life',player=1-owner,value=17)

    def test_dominator_second_fixture_source_really_enters_before_combat(self):
        for owner in (0,1):
            c=self.cases('Dominator Drone',[f'two_sources_{owner}'])[0]
            source=next(i for i,e in enumerate(c['setup']) if e['ref']=='source')
            copy=next(i for i,e in enumerate(c['setup']) if e['ref']=='copy')
            self.assertLess(source,copy)
            self.assertEqual(c['setup'][source]['zone'],'battlefield')
            self.assertEqual(c['setup'][copy]['zone'],'battlefield')
            combat=next(i for i,s in enumerate(c['steps']) if s['op']=='begin_attackers')
            self.assertIn(dict(op='assert_life',player=1-owner,value=18),c['steps'][:combat])
            self.has(c,'assert_life',player=1-owner,value=12)
            self.has(c,'assert_zone',player=1-owner,zone='library',value=10)
            self.has(c,'assert_zone',player=1-owner,zone='exile',value=3)
            self.assertFalse(any(s['op'] in ('cast','set_life') for s in c['steps']))

    def test_metalcraft_counts_own_artifacts_and_real_player_target_is_queued_for_resolution(self):
        for mode,library in (('artifacts_below',12),('artifacts_at',8),('artifacts_above',8),('artifacts_opponent',12),('artifacts_noncreature',8),('artifacts_none',12)):
            c=self.cases('Screeching Silcaw',[mode+'_0'])[0]
            self.has(c,'assert_zone',player=1,zone='library',value=library)
            self.has(c,'assert_life',player=1,value=19)
        for owner in (0,1):
            c=self.cases('Mindscour Dragon',[f'unblocked_{owner}'])[0]
            self.has(c,'choose_player',player=owner,value=1-owner)
            choice=next(i for i,s in enumerate(c['steps']) if s['op']=='choose_player')
            self.assertEqual(c['steps'][choice-1]['op'],'advance_combat')
            self.assertEqual(c['steps'][choice+1]['op'],'combat_damage')
        c=self.cases('Mindscour Dragon',['self_target_0'])[0]
        self.has(c,'choose_player',player=0,value=0)
        self.has(c,'assert_zone',player=0,zone='library',value=8)
        self.has(c,'assert_zone',player=1,zone='library',value=12)
        d=self.declaration('Mindscour Dragon');p=model.spec(d,self.oracle['Mindscour Dragon'])
        self.assertNotIn('self_target_1',model.variants(p))

    def test_library_complete_oracle_metadata_and_all_pilot_variants_fail_closed(self):
        names=['Culling Drone','Benthic Infiltrator','Dominator Drone','Fathom Feeder','Reef Pirates','Shriekgeist','Mindscour Dragon','Screeching Silcaw']
        for name in names:
            d=self.declaration(name);p=model.spec(d,self.oracle[name]);d['variants']=model.variants(p);before=copy.deepcopy((d,p))
            cases=model.expand(name,self.registry[name]['csvid'],d,p,d['variants'],self.registry,self.oracle)
            self.assertEqual(before,(d,p));self.assertEqual(len(cases),len(d['variants']))
            for c in cases:
                self.assertEqual(len(c['setup']),len({x['ref'] for x in c['setup']}))
                nonlive={x['ref'] for x in c['setup'] if x['zone'] in ('library','exile','graveyard')}
                self.assertFalse(any(s['op']=='assert_card' and s.get('ref') in nonlive for s in c['steps']))
            changed=copy.deepcopy(d);changed['params']['movement']['amount']=True
            with self.assertRaises(ValueError):model.spec(changed,self.oracle[name])
        for name,field,value in (('Culling Drone','colors',['B']),('Screeching Silcaw','keywords',['Flying','Mill']),('Benthic Infiltrator','oracle_text',self.oracle['Benthic Infiltrator']['oracle_text']+'\nWard {1}')):
            changed=copy.deepcopy(self.oracle[name]);changed[field]=value
            with self.assertRaises(ValueError):model.parameters(changed)

    def test_library_exact_followons_use_calibrated_ingest_grammar(self):
        for name,csvid in (('Mist Intruder',15376),('Sludge Crawler',15445)):
            d=self.declaration(name);p=model.spec(d,self.oracle[name]);d['variants']=model.variants(p)
            self.assertEqual(p['schema'],12)
            self.assertEqual(p['movement'],dict(zone='exile',amount=1,target='damaged'))
            before=copy.deepcopy((d,p));cases=model.expand(name,csvid,d,p,d['variants'],self.registry,self.oracle)
            self.assertEqual(before,(d,p));self.assertEqual(len(cases),54)
            for owner in (0,1):
                c=self.cases(name,[f'pump_{owner}'])[0]
                self.has(c,'assert_life',player=1-owner,value=16)
                self.has(c,'assert_zone',player=1-owner,zone='library',value=11)
                self.has(c,'assert_zone',player=1-owner,zone='exile',value=2)
                self.has(c,'assert_matching_permanents',player=owner,index=csvid,property='color',value=0)
                c=self.cases(name,[f'payment_{owner}'])[0]
                self.has(c,'assert_mana_total',player=owner,value=0)
                c=self.cases(name,[f'noncombat_player_{owner}'])[0]
                self.has(c,'assert_zone',player=1-owner,zone='library',value=12)
            if name=='Sludge Crawler':self.assertEqual(p['inactive_abilities'],['{2}: This creature gets +1/+1 until end of turn.'])

    def test_constrained_full_oracle_and_new_mechanisms_remain_closed(self):
        names=['Cold-Eyed Selkie','Neurok Commando','Wei Night Raiders','Stronghold Rats','Stromkirk Noble','Guul Draz Specter','Rakdos Ringleader','Wandering Champion']
        for name in names:
            d=self.declaration(name);p=model.spec(d,self.oracle[name]);d['variants']=model.variants(p);before=copy.deepcopy((d,p))
            self.assertEqual(p['schema'],13)
            cases=model.expand(name,self.registry[name]['csvid'],d,p,d['variants'],self.registry,self.oracle)
            self.assertEqual(before,(d,p));self.assertEqual(len(cases),len(d['variants']))
            for c in cases:self.assertEqual(len(c['setup']),len({x['ref'] for x in c['setup']}))
            changed=copy.deepcopy(d);changed['params']['effects']['discard']=True
            with self.assertRaises(ValueError):model.spec(changed,self.oracle[name])
        for name in ('Bloodcrazed Neonate','Jagged Poppet','Salvage Drone','Vile Aggregate','Raven Guild Master','Scalpelexis'):
            with self.assertRaises(ValueError):model.parameters(self.oracle[name])

    def test_constrained_hybrid_payments_preserve_real_oracle_and_both_alternatives(self):
        for owner in (0,1):
            for mode,green,blue in (('payment',2,0),('payment_other',0,2),('payment_mixed',1,1)):
                c=self.cases('Cold-Eyed Selkie',[f'{mode}_{owner}'])[0]
                self.has(c,'set_mana',player=owner,color=0,value=1)
                self.has(c,'set_mana',player=owner,color=3,value=green)
                self.has(c,'set_mana',player=owner,color=2,value=blue)
                self.has(c,'assert_mana_total',player=owner,value=0)
                self.assertEqual(c['_recipe']['oracle_text'],self.oracle['Cold-Eyed Selkie']['oracle_text'])
            c=self.cases('Cold-Eyed Selkie',[f'wrong_color_{owner}'])[0]
            self.has(c,'set_mana',player=owner,color=2,value=0);self.has(c,'set_mana',player=owner,color=3,value=0)
            self.has(c,'cast',ref='source',value=0)
        c=self.cases('Cold-Eyed Selkie',['cast_0'])[0]
        self.has(c,'choose_payment',player=0,value=8193)
        c=self.cases('Cold-Eyed Selkie',['pump_1'])[0]
        self.has(c,'assert_life',player=0,value=16);self.has(c,'assert_zone',player=1,zone='library',value=8)

    def test_constrained_shroud_rejects_targets_without_invalid_cast_choices(self):
        d=self.declaration('Neurok Commando');p=model.spec(d,self.oracle['Neurok Commando'])
        for forbidden in ('pump_0','zero_power_1','noncombat_player_0','noncombat_fresh_1'):self.assertNotIn(forbidden,model.variants(p))
        for owner in (0,1):
            c=self.cases('Neurok Commando',[f'shroud_target_{owner}'])[0]
            for ref in ('probe_own','probe_enemy','probe_aura'):self.has(c,'can_target_creature',ref=ref,other='source',value=0)
            self.has(c,'can_target_creature',ref='probe_own',other='idle',value=1)
            self.assertFalse(any(s['op'] in ('cast','choose_target') for s in c['steps']))

    def test_constrained_real_horse_shadow_human_and_landwalk_blockers(self):
        for name in ('Cold-Eyed Selkie','Wei Night Raiders','Stronghold Rats','Stromkirk Noble'):
            c=self.cases(name,['forbidden_block_0'])[0]
            self.assertTrue(any(s['op']=='block' and s['value']==0 for s in c['steps']))
            c=self.cases(name,['blocked_1'])[0]
            self.assertTrue(any(s['op']=='block' and s['value']==1 for s in c['steps']))
        c=self.cases('Stronghold Rats',['blocked_1'])[0]
        self.has(c,'assert_card',ref='source',property='in_play',value=0)
        self.has(c,'assert_card',ref='chosen_blocker',property='in_play',value=0)
        c=self.cases('Stronghold Rats',['unblocked_0'])[0]
        for player in (0,1):self.has(c,'assert_zone',player=player,zone='hand',value=2)
        c=self.cases('Stromkirk Noble',['forbidden_block_1'])[0]
        self.has(c,'assert_card',ref='source',property='plus1_counters',value=1)
        for mode in ('land_own_only','land_opponent_wrong'):
            c=self.cases('Cold-Eyed Selkie',[mode+'_0'])[0]
            self.has(c,'block',ref='wall',other='source',value=1)

    def test_constrained_empty_hand_bonus_uses_pre_hit_power_and_real_refill(self):
        for owner in (0,1):
            c=self.cases('Guul Draz Specter',[f'last_card_two_hits_{owner}'])[0]
            self.has(c,'assert_life',player=1-owner,value=18);self.has(c,'assert_life',player=1-owner,value=13)
            self.has(c,'assert_card',ref='source',property='power',value=5)
            c=self.cases('Guul Draz Specter',[f'refill_hand_{owner}'])[0]
            self.has(c,'cast',ref='helper',value=1);self.has(c,'assert_life',player=1-owner,value=16)
            self.has(c,'assert_card',ref='source',property='power',value=5);self.has(c,'assert_card',ref='source',property='power',value=2)
            for player in (0,1):self.has(c,'assert_zone',player=player,zone='library',value=10)
            self.assertEqual(sum(s['op']=='advance_turn' for s in c['steps']),2)

    def test_constrained_guul_ai_refill_keeps_exact_identities_for_either_legal_discard(self):
        c=self.cases('Guul Draz Specter',['refill_hand_0'])[0]
        own=[x['name'] for x in c['setup'] if x['zone']=='library' and x['player']==0]
        enemy=[x['name'] for x in c['setup'] if x['zone']=='library' and x['player']==1]
        self.assertEqual(own[:2],['Forest','Plains'])
        self.assertEqual(enemy[:2],['Forest','Forest'])
        self.assertEqual(enemy[2:],own[2:])
        for count in (2,1):self.has(c,'assert_zone_multiplicity',player=1,zone='hand',index=self.registry['Forest']['csvid'],value=count)
        self.has(c,'assert_zone_multiplicity',player=1,zone='hand',index=self.registry['Plains']['csvid'],value=0)
        self.has(c,'assert_zone_multiplicity',player=1,zone='graveyard',index=self.registry['Forest']['csvid'],value=2)
        self.has(c,'assert_life',player=1,value=16)
        self.has(c,'assert_card',ref='source',property='power',value=5)
        self.has(c,'assert_card',ref='source',property='power',value=2)
        c=self.cases('Guul Draz Specter',['refill_hand_1'])[0]
        enemy=[x['name'] for x in c['setup'] if x['zone']=='library' and x['player']==0]
        self.assertEqual(enemy[:2],['Forest','Plains'])

    def test_constrained_random_discard_keeps_mixed_identity_conservation_and_zero_choices(self):
        for owner in (0,1):
            c=self.cases('Rakdos Ringleader',[f'unblocked_{owner}'])[0]
            hand=[x['name'] for x in c['setup'] if x['zone']=='hand' and x['player']==1-owner]
            self.assertEqual(hand,['Forest','Plains','Mountain'])
            self.has(c,'assert_zone',player=1-owner,zone='hand',value=2)
            self.has(c,'assert_zone',player=1-owner,zone='graveyard',value=2)
            self.has(c,'assert_discard_choices',value=0)
            for n in ('Forest','Plains','Mountain'):self.has(c,'assert_hand_graveyard_total',player=1-owner,index=self.registry[n]['csvid'],value=1)
            self.assertFalse(any(s['op']=='assert_zone_multiplicity' and s.get('player')==1-owner and s.get('zone') in ('hand','graveyard') for s in c['steps']))
        c=self.cases('Rakdos Ringleader',['first_strike_block_0'])[0]
        self.has(c,'assert_card',ref='source',property='in_play',value=1);self.has(c,'assert_card',ref='source',property='damage',value=0)

    def test_constrained_color_qualification_and_rummage_order_use_real_zone_receipts(self):
        for mode,draw in (('qualifier_none',False),('qualifier_enemy_blue',False),('qualifier_hand_blue',False),('qualifier_mana_blue',False),('qualifier_island',False),('qualifier_green',False),('qualifier_own_blue',True),('qualifier_own_red',True),('qualifier_noncreature_blue',True)):
            c=self.cases('Wandering Champion',[mode+'_0'])[0]
            self.has(c,'assert_zone',player=0,zone='library',value=11 if draw else 12)
            self.assertEqual(any(s['op']=='choose_trigger' for s in c['steps']),draw)
        c=self.cases('Wandering Champion',['rummage_order_1'])[0]
        self.has(c,'assert_zone_multiplicity',player=1,zone='hand',index=self.registry['Forest']['csvid'],value=1)
        self.has(c,'assert_zone_multiplicity',player=1,zone='hand',index=self.registry['Plains']['csvid'],value=0)
        self.has(c,'assert_zone_multiplicity',player=1,zone='graveyard',index=self.registry['Plains']['csvid'],value=1)
        c=self.cases('Wandering Champion',['empty_source_hand_0'])[0]
        self.has(c,'assert_zone',player=0,zone='library',value=12)
        self.assertFalse(any(s['op']=='choose_trigger' for s in c['steps']))

    def test_constrained_followon_color_blocks_use_implemented_nonartifact_witnesses(self):
        for owner in (0,1):
            c=self.cases('Order of Yawgmoth',[f'color_legal_block_{owner}'])[0]
            self.assertEqual(next(x['name'] for x in c['setup'] if x['ref']=='chosen_blocker'),'Abyssal Specter')
            self.has(c,'block',ref='chosen_blocker',other='source',value=1)
            self.has(c,'assert_matching_permanents',player=1-owner,index=971,property='artifact',value=0)
            self.has(c,'assert_matching_permanents',player=1-owner,index=971,property='color',value=2)
            self.has(c,'assert_card',ref='source',property='in_play',value=0)
            self.has(c,'assert_card',ref='chosen_blocker',property='in_play',value=1)
            self.has(c,'assert_card',ref='chosen_blocker',property='damage',value=2)
            self.has(c,'assert_life',player=1-owner,value=20)
            self.has(c,'assert_zone',player=1-owner,zone='hand',value=3)
            c=self.cases('Heirs of Stromkirk',[f'color_legal_block_{owner}'])[0]
            self.assertEqual(next(x['name'] for x in c['setup'] if x['ref']=='chosen_blocker'),'Slith Firewalker')
            self.has(c,'block',ref='chosen_blocker',other='source',value=1)
            self.has(c,'assert_matching_permanents',player=1-owner,index=1476,property='artifact',value=0)
            self.has(c,'assert_matching_permanents',player=1-owner,index=1476,property='color',value=16)
            self.has(c,'assert_card',ref='source',property='in_play',value=1)
            self.has(c,'assert_card',ref='source',property='damage',value=1)
            self.has(c,'assert_card',ref='source',property='plus1_counters',value=0)
            self.has(c,'assert_card',ref='chosen_blocker',property='in_play',value=0)
            self.has(c,'assert_life',player=1-owner,value=20)
        for name in ('Abyssal Specter','Slith Firewalker'):
            changed=copy.deepcopy(self.registry);changed[name]['handler']='card_dummy'
            with self.assertRaisesRegex(ValueError,'implemented nonartifact'):model.validate_witnesses(self.oracle,changed)

    def test_constrained_followon_artifact_and_contrary_color_controls_and_gate(self):
        for name in ('Order of Yawgmoth','Heirs of Stromkirk'):
            for owner in (0,1):
                c=self.cases(name,[f'blocked_{owner}'])[0]
                self.has(c,'assert_matching_permanents',player=1-owner,index=self.registry['Darksteel Gargoyle']['csvid'],property='artifact',value=1)
                self.has(c,'block',ref='chosen_blocker',other='source',value=1)
                c=self.cases(name,[f'forbidden_block_{owner}'])[0]
                self.has(c,'assert_matching_permanents',player=1-owner,index=self.registry['Grizzly Bears']['csvid'],property='artifact',value=0)
                self.has(c,'assert_matching_permanents',player=1-owner,index=self.registry['Grizzly Bears']['csvid'],property='color',value=8)
                self.has(c,'block',ref='chosen_blocker',other='source',value=0)
        for name in ('Odylic Wraith','Falkenrath Exterminator'):
            d=self.declaration(name);p=model.spec(d,self.oracle[name]);self.assertNotIn('color_legal_block_0',model.variants(p))
        for name in ('Odylic Wraith','Order of Yawgmoth','Heirs of Stromkirk','Falkenrath Exterminator'):
            d=self.declaration(name);p=model.spec(d,self.oracle[name]);d['variants']=model.variants(p);before=copy.deepcopy((d,p))
            cases=model.expand(name,self.registry[name]['csvid'],d,p,d['variants'],self.registry,self.oracle)
            self.assertEqual(before,(d,p));self.assertEqual(len(cases),len(d['variants']))


if __name__=='__main__':unittest.main()
