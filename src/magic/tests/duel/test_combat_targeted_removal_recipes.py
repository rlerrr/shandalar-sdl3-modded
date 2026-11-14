"""Focused Oracle, real-cost, menu lifecycle and exact-removal guards."""
import copy
import json
import unittest
from pathlib import Path

from generate_cases import OPS, registry
import combat_targeted_removal_recipes as model
import return_saboteur_trigger_recipes as returns


class CombatTargetedRemovalTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.oracle=json.loads((Path(__file__).parent/'data/oracle.json').read_text(encoding='utf8'))['cards']
        cls.rows={r['name']:r for r in registry()}

    def declaration(self,name):
        return dict(family=model.FAMILY,version=2,params=model.parameters(self.oracle[name]),oracle_text=self.oracle[name]['oracle_text'],variants=['unblocked_0'])

    def case(self,name,variant):
        d=self.declaration(name);d['variants']=[variant];p=model.spec(d,self.oracle[name])
        return model.expand(name,self.rows[name]['csvid'],d,p,[variant],self.rows,self.oracle)[0]

    def has(self,c,op,**kw):self.assertIn(dict(op=op,**kw),c['steps'])

    def test_seven_whole_oracle_mechanisms_and_secondary_clauses(self):
        expected={'Caustic Wasps':('destroy','artifact'),'Soltari Visionary':('destroy','enchantment'),
                  'Throat Slitter':('destroy','nonblack_creature'),'Ashling, the Extinguisher':('targeted_sacrifice','creature'),
                  'Blind Zealot':('sacrifice_source_then_destroy','creature'),'Numot, the Devastator':('pay_then_destroy_up_to_two','land'),
                  'Sunder Shaman':('destroy','artifact_or_enchantment'),'Rustmouth Ogre':('destroy','artifact'),
                  'Trygon Predator':('destroy','artifact_or_enchantment')}
        self.assertEqual(set(model.profiles()['sources']),set(expected))
        for n,w in expected.items():
            p=model.parameters(self.oracle[n]);self.assertEqual((p['effect'],p['selector']),w)
        self.assertEqual(model.parameters(self.oracle['Soltari Visionary'])['scope'],'any')
        self.assertEqual(model.parameters(self.oracle['Throat Slitter'])['ninjutsu'],'{2}{B}')
        self.assertEqual(model.parameters(self.oracle['Sunder Shaman'])['maxblockers'],1)
        self.assertEqual(model.parameters(self.oracle['Numot, the Devastator'])['payment'],'{2}{R}')

    def test_strict_complete_oracle_version_and_parameter_types(self):
        n='Blind Zealot';d=self.declaration(n)
        for extra in (dict(version=2.0),dict(version=True),dict(family='return_saboteur_trigger'),dict(variants=[]),dict(variants=['unblocked_0']*2),dict(oracle_text=d['oracle_text']+' ')):
            with self.assertRaises(ValueError):model.spec(dict(d,**extra),self.oracle[n])
        bad=copy.deepcopy(d);bad['params']['optional']=1
        with self.assertRaises(ValueError):model.spec(bad,self.oracle[n])
        for n in model.profiles()['sources']:
            r=copy.deepcopy(self.oracle[n]);r['oracle_text']+='\nDraw a card.'
            with self.assertRaises(ValueError):model.parameters(r)

    def test_source_and_effect_witnesses_are_independent_full_pins(self):
        model.validate_witnesses(self.oracle,self.rows)
        for name in ('Darksteel Myr','Fervor','Control Magic','Hermetic Study','Dauthi Slayer'):
            o=copy.deepcopy(self.oracle);o[name]['oracle_text']+=' '
            with self.assertRaises(ValueError):model.validate_witnesses(o,self.rows)
        rows=copy.deepcopy(self.rows);rows['Caustic Wasps']['handler']='card_dummy'
        with self.assertRaises(ValueError):model.validate_witnesses(self.oracle,rows)
        self.assertEqual(self.rows['Caustic Wasps']['handler'],self.rows['Rustmouth Ogre']['handler'])

    def test_all_variants_use_registered_real_actions_and_preserve_full_record(self):
        total=0
        for n in model.profiles()['sources']:
            d=self.declaration(n);p=model.spec(d,self.oracle[n]);vs=model.variants(p)
            cs=model.expand(n,self.rows[n]['csvid'],d,p,vs,self.rows,self.oracle);total+=len(cs)
            self.assertEqual(len({c['name'] for c in cs}),len(cs))
            for c in cs:
                self.assertLessEqual({s['op'] for s in c['steps']},OPS)
                self.assertEqual(c['_recipe']['oracle_text'],self.oracle[n]['oracle_text'])
                self.assertFalse(any(s['op'] in ('set_life','destroy','choose_priority','remove_from_combat','reset_death_history') for s in c['steps']))
                self.assertFalse(any('attacking' in s.get('state',[]) for s in c['setup']))
        self.assertEqual(total,443)

    def test_exact_follow_on_presentations_do_not_inherit_blind_empty_menu(self):
        self.assertTrue(model.profiles()['sources']['Rustmouth Ogre']['input_profile']['outer_optional'])
        self.assertFalse(model.profiles()['sources']['Trygon Predator']['input_profile']['outer_optional'])
        for n in ('Rustmouth Ogre','Trygon Predator'):
            c=self.case(n,'no_target_1')
            self.assertFalse(any(s['op'].startswith('choose_') for s in c['steps']))
            self.has(c,'assert_card',ref='source',property='in_play',value=1)
        c=self.case('Trygon Predator','declined_0')
        self.has(c,'choose_optional_target',player=0,ref='victim')
        self.assertFalse(any(s['op']=='choose_trigger_source' for s in c['steps']))
        self.has(c,'assert_card',ref='victim',property='in_play',value=1)

    def test_follow_on_printed_rr_and_gu_payments_have_independent_full_pools(self):
        for n,want in [('Rustmouth Ogre',[4,0,0,0,2,0]),('Trygon Predator',[1,0,1,1,0,0])]:
            for owner in (0,1):
                c=self.case(n,'payment_'+str(owner))
                for color,value in enumerate(want):self.has(c,'set_mana',player=owner,color=color,value=value)
                self.has(c,'assert_mana_total',player=owner,value=0)

    def test_mixed_artifact_enchantment_home_compositions_preserve_other_type(self):
        path=Path(__file__).parent/'data/cases/cards/083/08307-trygon-predator.json'
        cases=json.loads(path.read_text(encoding='utf8'))['cases'];self.assertEqual(len(cases),4)
        for c in cases:
            owner=int(c['name'][-1]);artifact='_mixed_artifact_first_' in c['name']
            first='Sol Ring' if artifact else 'Fervor';other='Fervor' if artifact else 'Sol Ring'
            self.assertFalse(any(k.startswith('_') for k in c))
            self.assertLessEqual({s['op'] for s in c['steps']},OPS)
            setup={s['ref']:s for s in c['setup']}
            self.assertEqual(setup['victim']['name'],first);self.assertEqual(setup['later_victim']['name'],other)
            self.assertEqual(setup['victim']['player'],1-owner);self.assertEqual(setup['later_victim']['player'],1-owner)
            self.has(c,'choose_target',player=owner,ref='victim')
            self.assertEqual(sum(s['op']=='choose_target' for s in c['steps']),1)
            self.has(c,'assert_card',ref='victim',property='in_play',value=0)
            self.has(c,'assert_card',ref='later_victim',property='in_play',value=1)
            self.has(c,'assert_zone_multiplicity',player=1-owner,zone='graveyard',index=self.rows[first]['csvid'],value=1)
            self.has(c,'assert_zone_multiplicity',player=1-owner,zone='graveyard',index=self.rows[other]['csvid'],value=0)

    def test_real_source_costs_and_exact_payment_receipts(self):
        for n in model.profiles()['sources']:
            for owner in (0,1):
                c=self.case(n,'payment_'+str(owner));self.has(c,'cast',ref='source',value=1)
                self.has(c,'assert_mana_total',player=owner,value=0)
                c=self.case(n,'insufficient_mana_'+str(owner));self.has(c,'cast',ref='source',value=0)
                self.has(c,'assert_card',ref='source',property='in_hand',value=1)

    def test_source_sacrifice_and_indestructible_victim_are_distinct_receipts(self):
        c=self.case('Blind Zealot','indestructible_1')
        self.has(c,'assert_card',ref='source',property='in_play',value=0)
        self.has(c,'assert_card',ref='victim',property='in_play',value=1)
        self.has(c,'assert_zone_multiplicity',player=1,zone='graveyard',index=11569,value=1)
        self.has(c,'assert_zone_multiplicity',player=0,zone='graveyard',index=11238,value=0)
        self.has(c,'assert_life',player=0,value=18)

    def test_targeted_sacrifice_really_kills_indestructible(self):
        c=self.case('Ashling, the Extinguisher','indestructible_0')
        self.has(c,'assert_card',ref='source',property='in_play',value=1)
        self.has(c,'assert_card',ref='victim',property='in_play',value=0)
        self.has(c,'assert_zone_multiplicity',player=1,zone='graveyard',index=11238,value=1)
        c=self.case('Throat Slitter','indestructible_0');self.has(c,'assert_card',ref='victim',property='in_play',value=1)

    def test_numot_real_payment_distinct_mixed_owner_targets(self):
        for owner in (0,1):
            c=self.case('Numot, the Devastator','mixed_lands_'+str(owner))
            self.has(c,'choose_target',player=owner,ref='victim');self.has(c,'choose_target',player=owner,ref='own_land')
            self.has(c,'assert_card',ref='victim2',property='in_play',value=1)
            self.has(c,'assert_mana',player=owner,color=0,value=8);self.has(c,'assert_mana',player=owner,color=4,value=9)
            self.has(c,'assert_mana_total',player=owner,value=57)
            self.has(c,'assert_zone_multiplicity',player=owner,zone='graveyard',index=self.rows['Island']['csvid'],value=2)

    def test_numot_zero_one_and_two_are_actual_up_to_choices(self):
        for mode,n in (('zero_targets',0),('one_target',1),('mixed_lands',2)):
            c=self.case('Numot, the Devastator',mode+'_0')
            self.assertEqual(sum(s['op']=='choose_target' for s in c['steps']),n)
            self.assertEqual(sum(s['op']=='choose_optional_target' for s in c['steps']),int(n<2))
            self.has(c,'assert_mana_total',player=0,value=57)

    def test_numot_empty_controls_do_not_invent_accepted_optional_payment(self):
        for owner in (0,1):
            for mode in ('no_target','wrong_type'):
                c=self.case('Numot, the Devastator',mode+'_'+str(owner))
                self.has(c,'assert_mana_total',player=owner,value=60)
                self.assertFalse(any(s['op'].startswith('choose_') for s in c['steps']))
                self.has(c,'assert_card',ref='source',property='in_play',value=1)
            # Paying and choosing zero is independently covered, rather than
            # treating a library of absent options as an accepted payment.
            c=self.case('Numot, the Devastator','zero_targets_'+str(owner))
            self.has(c,'assert_mana_total',player=owner,value=57)
            self.has(c,'choose_optional_target',player=owner,ref='victim')

    def test_blind_empty_ai_selector_uses_only_actual_cancel_and_keeps_source(self):
        for mode in ('no_target','other_controller','shroud_only','wrong_type'):
            c=self.case('Blind Zealot',mode+'_1')
            self.has(c,'choose_cancel_target',player=1)
            self.assertFalse(any(s['op'] in ('choose_target','choose_optional_target') for s in c['steps']))
            self.has(c,'assert_card',ref='source',property='in_play',value=1)
            c=self.case('Blind Zealot',mode+'_0')
            self.assertFalse(any(s['op']=='choose_cancel_target' for s in c['steps']))

    def test_numot_decline_uses_real_unfloated_lands_and_no_fake_ai_decline(self):
        c=self.case('Numot, the Devastator','declined_0')
        self.has(c,'choose_cancel_mana',player=0);self.has(c,'assert_mana_total',player=0,value=0)
        self.assertEqual([s['name'] for s in c['setup'] if s['ref'].startswith('payment_land_')],['Forest','Forest','Mountain'])
        d=self.declaration('Numot, the Devastator');p=model.spec(d,self.oracle['Numot, the Devastator'])
        self.assertNotIn('declined_1',model.variants(p))
        self.assertFalse(any(s['op']=='choose_cancel_mana' for s in self.case('Numot, the Devastator','trigger_insufficient_0')['steps']))

    def test_decline_is_outer_trigger_or_actual_optional_target_as_offered(self):
        for n in ('Caustic Wasps','Blind Zealot'):
            c=self.case(n,'declined_0');self.assertFalse(any(s['op'].startswith('choose_') for s in c['steps']))
            self.has(c,'assert_card',ref='victim',property='in_play',value=1)

    def test_contrary_targets_are_not_forced_invalid_ui_choices(self):
        for n,mode in [('Throat Slitter','black_only'),('Throat Slitter','shroud_only'),('Blind Zealot','wrong_type'),('Ashling, the Extinguisher','other_controller'),('Sunder Shaman','wrong_type'),('Caustic Wasps','no_target')]:
            c=self.case(n,mode+'_0')
            self.assertFalse(any(s['op'] in ('choose_target','choose_optional_target','probe_activation_target','can_target_creature') for s in c['steps']))
            self.has(c,'assert_life',player=1,value=20-int(self.oracle[n]['power']))

    def test_target_decision_player_is_chooser_and_ref_supplies_target_location(self):
        for name in model.profiles()['sources']:
            d=self.declaration(name);p=model.spec(d,self.oracle[name])
            for variant in model.variants(p):
                owner=int(variant[-1]);c=self.case(name,variant)
                decisions=[s for s in c['steps'] if s['op']=='choose_target']
                if variant.startswith('owner_destination_'):
                    self.assertEqual(decisions[0],dict(op='choose_target',player=1-owner,ref='victim'))
                    decisions=decisions[1:]
                self.assertTrue(all(s['player']==owner for s in decisions),(name,variant,decisions))
        for owner in (0,1):
            c=self.case('Caustic Wasps','unblocked_'+str(owner))
            self.has(c,'choose_target',player=owner,ref='victim')
            victim=next(s for s in c['setup'] if s['ref']=='victim')
            self.assertEqual(victim['player'],1-owner)

    def test_real_noncombat_source_tap_and_any_damage_selector(self):
        c=self.case('Soltari Visionary','noncombat_player_1')
        for op in ('cast','activate','resolve'):self.assertTrue(any(s['op']==op for s in c['steps']))
        self.has(c,'assert_card',ref='source',property='tapped',value=1);self.has(c,'assert_card',ref='grant',property='tapped',value=0)
        self.has(c,'assert_card',ref='victim',property='in_play',value=0)
        self.has(c,'assert_mana',player=1,color=2,value=9)
        for n in ('Caustic Wasps','Blind Zealot','Throat Slitter'):
            c=self.case(n,'noncombat_player_0');self.has(c,'assert_card',ref='victim',property='in_play',value=1)
            self.assertFalse(any(s['op']=='choose_target' and s.get('ref')=='victim' for s in c['steps']))

    def test_any_damage_self_hit_selects_actual_own_aura(self):
        c=self.case('Soltari Visionary','noncombat_self_0')
        self.has(c,'choose_target',player=0,ref='grant');self.has(c,'assert_card',ref='grant',property='in_play',value=0)
        self.has(c,'assert_zone_multiplicity',player=0,zone='graveyard',index=self.rows['Hermetic Study']['csvid'],value=1)
        self.has(c,'assert_card',ref='victim',property='in_play',value=1)

    def test_fresh_granted_creature_cannot_pay_real_tap_cost(self):
        c=self.case('Soltari Visionary','noncombat_sick_0');self.has(c,'can_activate',ref='grant',value=0)
        self.assertFalse(any(s['op']=='activate' for s in c['steps']))

    def test_actual_ninjutsu_uses_immutable_lifecycle_and_printed_payment(self):
        c=self.case('Throat Slitter','ninjutsu_payment_0')
        self.has(c,'activate_hand',ref='source',value=1);self.has(c,'assert_stack',value=1)
        for prop in ('tapped','attacking','sick'):self.has(c,'assert_card',ref='source',property=prop,value=1)
        self.has(c,'assert_zone_multiplicity',player=0,zone='hand',index=106,value=1)
        self.has(c,'assert_mana_total',player=0,value=0);self.has(c,'assert_card',ref='victim',property='in_play',value=0)

    def test_real_controller_transfer_uses_actual_aura_cast_then_owner_grave(self):
        for n in ('Throat Slitter','Ashling, the Extinguisher','Blind Zealot'):
            c=self.case(n,'owner_destination_0')
            self.has(c,'turn',player=1,value=0);self.has(c,'cast',ref='theft',value=1)
            self.has(c,'assert_mana_total',player=1,value=56)
            self.has(c,'bind_permanent_copy',player=1,value=106,index=0,ref='victim')
            self.has(c,'assert_zone_multiplicity',player=0,zone='graveyard',index=106,value=1)
            self.has(c,'assert_zone_multiplicity',player=1,zone='graveyard',index=44,value=1)
            ops=[s['op'] for s in c['steps']];self.assertLess(ops.index('advance_turn'),ops.index('begin_attackers'))
            advance=ops.index('advance_turn');paid=next(i for i,s in enumerate(c['steps']) if s==dict(op='assert_mana_total',player=1,value=56))
            for player in (0,1):
                for color in range(6):self.assertIn(dict(op='set_mana',player=player,color=color,value=0),c['steps'][paid:advance])

    def test_sunder_second_block_rejection_preserves_first(self):
        c=self.case('Sunder Shaman','second_block_0')
        self.has(c,'block',ref='wall',other='source',value=1);self.has(c,'block',ref='second_blocker',other='source',value=0)
        self.has(c,'assert_card',ref='second_blocker',property='blocking_count',value=0)
        self.has(c,'assert_card',ref='source',property='damage',value=2)
        self.has(c,'assert_card',ref='victim',property='in_play',value=1);self.has(c,'assert_life',player=1,value=20)

    def test_shadow_flying_and_intimidate_blocks_are_real(self):
        for n,mode in [('Soltari Visionary','keyword_block'),('Caustic Wasps','keyword_block'),('Blind Zealot','artifact_block'),('Blind Zealot','color_block')]:
            self.has(self.case(n,mode+'_0'),'block',ref='wall',other='source',value=1)
        for n in ('Soltari Visionary','Caustic Wasps','Blind Zealot'):
            self.has(self.case(n,'contrary_block_0'),'block',ref='wall',other='source',value=0)

    def test_two_hits_keep_actual_consecutive_turns_and_synchronized_pools(self):
        c=self.case('Numot, the Devastator','two_hits_1');steps=c['steps'];ops=[s['op'] for s in steps]
        self.assertEqual(ops.count('advance_turn'),2);self.assertEqual(ops.count('untap_step'),2)
        turn=next(i for i,s in enumerate(steps) if i>ops.index('advance_turn') and s['op']=='turn')
        self.assertEqual(steps[turn],dict(op='turn',player=1,value=0));self.has(c,'assert_mana_total',player=1,value=57)
        self.has(c,'choose_cancel_target',player=1)
        p=model.spec(self.declaration('Blind Zealot'),self.oracle['Blind Zealot']);self.assertNotIn('two_hits_0',model.variants(p))

    def test_numot_observed_second_hit_reoffer_does_not_change_oracle_payment(self):
        for owner,done_count in ((0,2),(1,1)):
            c=self.case('Numot, the Devastator','two_hits_'+str(owner))
            self.assertEqual(sum(s['op']=='choose_cancel_target' for s in c['steps']),done_count)
            self.has(c,'assert_mana_total',player=owner,value=57)
            self.has(c,'assert_mana',player=owner,color=0,value=8)
            self.has(c,'assert_mana',player=owner,color=4,value=9)
            self.assertFalse(any(s['op']=='assert_mana_total' and s['value']==54 for s in c['steps']))
            self.has(c,'assert_life',player=1-owner,value=8)

    def test_snapshots_replay_real_effect_and_choices_before_strict_empty_check(self):
        for n,mode in [('Ashling, the Extinguisher','combat_snapshot'),('Soltari Visionary','noncombat_snapshot'),('Throat Slitter','ninjutsu_pending_snapshot')]:
            c=self.case(n,mode+'_0');ops=[s['op'] for s in c['steps']]
            self.assertEqual(ops.count('capture'),1);self.assertEqual(ops.count('restore'),1);self.assertEqual(ops.count('assert_recorded_state'),1)
            index=max(i for i,op in enumerate(ops) if op=='assert_choices_consumed')
            self.assertIn('assert_unused_choices',ops[:index])

    def test_mistblade_is_exact_existing_parser_and_forty_two_variants(self):
        p=returns.parameters(self.oracle['Mistblade Shinobi']);self.assertEqual((p['effect'],p['ninjutsu']),('bounce_one','{U}'))
        self.assertEqual(len(returns.variants(p)),42)
        returns.validate_witnesses(self.oracle,self.rows)


if __name__=='__main__':unittest.main()
