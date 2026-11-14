"""Whole-source restrictions, action/owner semantics and legal phase guards."""
import copy
import json
import unittest
from pathlib import Path

from generate_cases import OPS,PROPERTIES,registry
import aura_restriction_recipes as model


class AuraRestrictionTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.oracle=json.loads((Path(__file__).parent/'data/oracle.json').read_text(encoding='utf8'))['cards'];cls.rows={r['name']:r for r in registry()}

    def declaration(self,n):return dict(family=model.FAMILY,version=2,oracle_text=self.oracle[n]['oracle_text'],variants=['attached_0'],params=dict(schema=1,mechanism=model.mechanism(self.oracle[n])))

    def case(self,n,mode='attached',owner=0):
        d=self.declaration(n);d['variants']=[f'{mode}_{owner}'];p=model.spec(d,self.oracle[n])
        return model.expand(n,self.rows[n]['csvid'],d,p,d['variants'],self.rows,self.oracle)[0]

    def test_nine_complete_sources_expand794_valid_both_controller_cases(self):
        count=0
        for n in model.pins()['sources']:
            d=self.declaration(n);p=model.spec(d,self.oracle[n]);d['variants']=model.variants(p);cases=model.expand(n,p['csvid'],d,p,d['variants'],self.rows,self.oracle);count+=len(cases)
            self.assertEqual({int(c['_recipe']['variant'][-1]) for c in cases},{0,1})
            for c in cases:
                self.assertEqual(c['_recipe']['covered_clauses'],self.oracle[n]['oracle_text'].splitlines());self.assertEqual(len(c['setup']),len({x['ref'] for x in c['setup']}))
                if next(x['name'] for x in c['setup'] if x['ref']=='host')=='Dragon Whelp':self.assertLessEqual(sum(x['op']=='activate' and x.get('ref')=='host' for x in c['steps']),2)
                for s in c['steps']:
                    self.assertIn(s['op'],OPS)
                    if 'property' in s:self.assertIn(s['property'],PROPERTIES)
        self.assertEqual(count,794)

    def test_full_pins_reject_unmodeled_cost_type_or_clause_changes(self):
        for n in model.pins()['sources']:
            d=self.declaration(n)
            for key,value in [('mana_cost','{0}'),('type_line','Creature'),('layout','transform'),('oracle_text',self.oracle[n]['oracle_text']+'\nDraw a card.')]:
                r=dict(self.oracle[n],**{key:value})
                with self.assertRaises(ValueError):model.spec(dict(d,oracle_text=r['oracle_text']),r)
        d=self.declaration('Stupefying Touch');d['params']['mechanism']['draw']=True
        with self.assertRaises(ValueError):model.spec(d,self.oracle['Stupefying Touch'])

    def test_flags_are_independently_derived_from_full_printed_text(self):
        expected={"Lawmage's Binding":(True,True,True,False,0),'Stasis Cocoon':(True,True,True,False,0),'Encrust':(False,False,True,True,0),
                  'Stupefying Touch':(False,False,True,False,1),'Bonds of Quicksilver':(False,False,False,True,0),'Eternity Snare':(False,False,False,True,1)}
        for n,values in expected.items():
            p=model.spec(self.declaration(n),self.oracle[n]);self.assertEqual(tuple(p[k] for k in ('no_attack','no_block','no_activate','no_untap','draw')),values)
            self.assertNotIn('tap enchanted',self.oracle[n]['oracle_text'])

    def test_live_primary_gates_and_static_only_dummy_witnesses_are_honest(self):
        for n,p in model.pins()['sources'].items():self.assertEqual(self.rows[n]['csvid'],p['csvid']);self.assertNotIn(self.rows[n]['handler'],('NULL','card_dummy',None))
        actual={n for n in model.pins()['witnesses'] if self.rows[n]['handler']=='card_dummy'}
        self.assertEqual(actual,set(model.pins()['honest_static_only_dummy_witnesses']))
        for n in actual:self.assertIn(self.oracle[n]['oracle_text'],('','Flying'))

    def test_actual_native_aura_costs_and_flash(self):
        expected={"Lawmage's Binding":[1,0,1,0,0,1],'Stasis Cocoon':[1,0,0,0,0,1],'Encrust':[1,0,2,0,0,0],
                  'Stupefying Touch':[1,0,1,0,0,0],'Bonds of Quicksilver':[3,0,1,0,0,0],'Eternity Snare':[5,0,1,0,0,0]}
        for n,cost in expected.items():
            p=model.spec(self.declaration(n),self.oracle[n]);self.assertEqual(p['mana'],cost);self.assertEqual(p['flash'],n in ("Lawmage's Binding",'Bonds of Quicksilver'))
            self.assertEqual(self.oracle[n]['type_line'],'Enchantment — Aura')
            for owner in (0,1):
                s=self.case(n,'payment',owner)['steps'];cast=next(i for i,x in enumerate(s) if x['op']=='cast')
                for color,amount in enumerate(cost):self.assertIn(dict(op='set_mana',player=owner,color=color,value=amount),s[:cast])

    def test_exact_followers_keep_real_costs_predicates_and_only_untap_restriction(self):
        expected={'Dehydration':([3,0,1,0,0,0],'creature'),'Paralyzing Grasp':([2,0,1,0,0,0],'creature'),'Ice Over':([1,0,1,0,0,0],'artifact_or_creature')}
        for n,(mana,scope) in expected.items():
            p=model.spec(self.declaration(n),self.oracle[n]);self.assertEqual(p['mana'],mana);self.assertEqual(p['scope'],scope);self.assertEqual(len(model.variants(p)),88)
            self.assertTrue(p['no_untap']);self.assertEqual([p[k] for k in ('flash','draw','no_attack','no_block','no_activate')],[False,0,False,False,False])
            self.assertEqual(p['clauses'],self.oracle[n]['oracle_text'].splitlines())
            for owner in (0,1):
                s=self.case(n,'mana_activation',owner)['steps'];self.assertIn(dict(op='activate',ref='host',value=1),s)
                self.assertIn(dict(op='cast',ref='host_spender',value=1),s)
                s=self.case(n,'attack',owner)['steps'];self.assertIn(dict(op='attack',ref='host',value=1),s)
        p=model.spec(self.declaration('Ice Over'),self.oracle['Ice Over'])
        for n in ('Sol Ring','Darksteel Citadel','Copper Myr','Elvish Mystic'):self.assertTrue(model.eligible(self.oracle[n],p))
        self.assertFalse(model.eligible(self.oracle['Forest'],p))

    def test_whole_enchant_predicates_include_both_artifact_and_creature_roles(self):
        creature=model.spec(self.declaration("Lawmage's Binding"),self.oracle["Lawmage's Binding"]);artifact=model.spec(self.declaration('Stasis Cocoon'),self.oracle['Stasis Cocoon']);union=model.spec(self.declaration('Encrust'),self.oracle['Encrust'])
        for n in ['Copper Myr','Ornithopter']:self.assertTrue(model.eligible(self.oracle[n],creature));self.assertTrue(model.eligible(self.oracle[n],artifact))
        for n in ['Sol Ring','Darksteel Citadel']:self.assertFalse(model.eligible(self.oracle[n],creature));self.assertTrue(model.eligible(self.oracle[n],artifact));self.assertTrue(model.eligible(self.oracle[n],union))
        self.assertTrue(model.eligible(self.oracle['Dryad Arbor'],creature));self.assertFalse(model.eligible(self.oracle['Dryad Arbor'],artifact));self.assertTrue(model.eligible(self.oracle['Dryad Arbor'],union))
        for n in ['Forest','Seal of Cleansing']:self.assertFalse(model.eligible(self.oracle[n],union))

    def test_actual_cast_denial_does_not_misread_raw_affordability_query(self):
        for n in model.pins()['sources']:
            for owner in (0,1):
                for mode in ('insufficient_mana','wrong_color','no_legal_host','shroud_host'):
                    s=self.case(n,mode,owner)['steps'];self.assertIn(dict(op='cast',ref='source',value=0),s)
                    self.assertFalse(any(x['op'].startswith('choose_') for x in s));self.assertFalse(any(x['op'] in ('can_cast','require_can_cast') for x in s))

    def test_one_cast_time_target_and_never_an_entry_tap_or_second_target(self):
        for n in model.pins()['sources']:
            for owner in (0,1):
                s=self.case(n,'attached',owner)['steps'];picks=[i for i,x in enumerate(s) if x['op']=='choose_target'];cast=next(i for i,x in enumerate(s) if x['op']=='cast')
                self.assertEqual(len(picks),1);self.assertLess(picks[0],cast);self.assertEqual(s[picks[0]],dict(op='choose_target',player=owner,ref='host'))
                self.assertIn(dict(op='assert_card',ref='host',property='tapped',value=0),s[cast:])

    def test_all_activated_abilities_include_mana_and_nonmana_actual_actions(self):
        for n in model.pins()['sources']:
            p=model.spec(self.declaration(n),self.oracle[n])
            for owner in (0,1):
                for mode in ('mana_activation','nonmana_activation'):
                    c=self.case(n,mode,owner);s=c['steps'];allowed=int(not p['no_activate'])
                    self.assertIn(dict(op='activate',ref='host',value=allowed),s)
                    self.assertIn(dict(op='activate',ref='activation_control',value=1),s)
                    self.assertFalse(any(x['op']=='require_can_activate' for x in s))
                    target=next(x['name'] for x in c['setup'] if x['ref']=='host')
                    self.assertNotEqual(self.rows[target]['handler'],'card_dummy')
                    if mode=='nonmana_activation':self.assertEqual(target,'Jayemdae Tome' if p['scope']=='artifact' else 'Dragon Whelp')

    def test_actual_native_mana_is_receipted_then_paid_on_real_fog(self):
        for n in model.pins()['sources']:
            p=model.spec(self.declaration(n),self.oracle[n])
            for owner in (0,1):
                c=self.case(n,'mana_activation',owner);s=c['steps'];self.assertEqual(next(x['name'] for x in c['setup'] if x['ref']=='control_spender'),'Fog')
                control=next(i for i,x in enumerate(s) if x['op']=='activate' and x['ref']=='activation_control');paid=next(i for i,x in enumerate(s) if x['op']=='cast' and x['ref']=='control_spender')
                self.assertLess(control,paid);self.assertIn(dict(op='assert_mana',player=1-owner,color=3,value=1),s[control:paid]);self.assertFalse(any(x['op']=='set_mana' for x in s[control:paid]))
                host_paid=[i for i,x in enumerate(s) if x['op']=='cast' and x['ref']=='host_spender'];self.assertEqual(len(host_paid),int(not p['no_activate']))

    def test_source_departure_restores_actual_activation_permissions(self):
        for n in model.pins()['sources']:
            for owner in (0,1):
                for mode in ('mana_after_source_departure','nonmana_after_source_departure'):
                    s=self.case(n,mode,owner)['steps'];destroy=next(i for i,x in enumerate(s) if x['op']=='destroy');act=next(i for i,x in enumerate(s) if x['op']=='activate' and x['ref']=='host')
                    self.assertLess(destroy,act);self.assertEqual(s[act]['value'],1)

    def test_attack_denial_and_its_negative_flags_use_real_complete_combat(self):
        for n in model.pins()['sources']:
            p=model.spec(self.declaration(n),self.oracle[n])
            for owner in (0,1):
                for mode in ('attack','attack_after_source_departure'):
                    s=self.case(n,mode,owner)['steps'];allowed=not p['no_attack'] or mode.endswith('departure')
                    self.assertIn(dict(op='attack',ref='host',value=int(allowed)),s);self.assertIn(dict(op='attack',ref='unselected',value=1),s)
                    self.assertIn(dict(op='assert_life',player=owner,value=18-int(allowed)),s)
                    self.assert_complete_combat(s)

    def assert_complete_combat(self,s):
        advance=next(i for i,x in enumerate(s) if x['op']=='advance_combat');damage=next(i for i,x in enumerate(s) if x['op']=='combat_damage');end=next(i for i,x in enumerate(s) if x['op']=='end_combat')
        self.assertLess(advance,damage);self.assertLess(damage,end)
        for i,x in enumerate(s):
            if x['op']=='assert_card' and x.get('property')=='blocked':self.assertLess(advance,i)

    def test_block_denial_local_control_damage_and_aura_on_host_death(self):
        for n in model.pins()['sources']:
            p=model.spec(self.declaration(n),self.oracle[n])
            for owner in (0,1):
                s=self.case(n,'block',owner)['steps'];allowed=not p['no_block']
                self.assertIn(dict(op='block',ref='host',other='attacker_0',value=int(allowed)),s);self.assertIn(dict(op='block',ref='unselected',other='attacker_1',value=1),s)
                self.assertIn(dict(op='assert_card',ref='unselected',property='damage',value=1),s)
                self.assertIn(dict(op='assert_card',ref='attacker_1',property='damage',value=2),s)
                if allowed:self.assertIn(dict(op='assert_zone_multiplicity',player=owner,zone='graveyard',index=self.rows[n]['csvid'],value=1),s)
                else:self.assertIn(dict(op='assert_life',player=1-owner,value=19),s)
                self.assert_complete_combat(s)

    def test_true_no_entry_tap_and_real_first_second_untaps(self):
        for n in model.pins()['sources']:
            p=model.spec(self.declaration(n),self.oracle[n])
            for owner in (0,1):
                s=self.case(n,'first_second_untap',owner)['steps'];untaps=[i for i,x in enumerate(s) if x['op']=='untap_step' and x['player']==1-owner];self.assertEqual(len(untaps),2)
                for i in untaps:
                    self.assertIn(dict(op='assert_card',ref='host',property='tapped',value=int(p['no_untap'])),s[i:])
                    self.assertIn(dict(op='assert_card',ref='unselected',property='tapped',value=0),s[i:])
                    self.assertEqual(s[i-1]['op'],'advance_turn')

    def test_manual_untap_is_legal_even_when_activation_or_normal_untap_is_blocked(self):
        for n in model.pins()['sources']:
            for owner in (0,1):
                s=self.case(n,'manual_untap',owner)['steps'];self.assertIn(dict(op='present_ai_dialogs',value=1),s)
                for ref,value in [('untapper',1),('retapper',0)]:
                    cast=next(i for i,x in enumerate(s) if x['op']=='cast' and x['ref']==ref)
                    self.assertEqual(s[cast-1],dict(op='choose_target',player=owner,ref='host'));self.assertIn(dict(op='choose_dialog',player=owner,value=value),s[cast:cast+5])

    def test_entry_draw_goes_to_aura_caster_not_host_controller_or_owner(self):
        for n in ('Stupefying Touch','Eternity Snare'):
            for owner in (0,1):
                s=self.case(n,'exact_library',owner)['steps'];self.assertIn(dict(op='assert_zone',player=owner,zone='library',value=0),s)
                self.assertIn(dict(op='assert_zone_multiplicity',player=owner,zone='hand',index=self.rows['Forest']['csvid'],value=1),s)
                self.assertIn(dict(op='assert_zone',player=1-owner,zone='library',value=4),s)
                c=self.case(n,'foreign_owned_host',owner);host=next(x for x in c['setup'] if x['ref']=='host');self.assertIn('opponent_owned',host['state'])
                s=c['steps'];self.assertIn(dict(op='assert_zone_multiplicity',player=owner,zone='hand',index=self.rows['Forest']['csvid'],value=1),s)

    def test_host_returns_to_original_owner_aura_goes_to_its_owner_graveyard(self):
        for n in model.pins()['sources']:
            for owner in (0,1):
                c=self.case(n,'foreign_owned_host',owner);target=next(x['name'] for x in c['setup'] if x['ref']=='host');s=c['steps']
                self.assertIn(dict(op='assert_zone_multiplicity',player=1-owner,zone='hand',index=self.rows[target]['csvid'],value=1),s)
                self.assertIn(dict(op='assert_zone_multiplicity',player=owner,zone='graveyard',index=self.rows[n]['csvid'],value=1),s)

    def test_overlapping_sources_release_only_after_both_actual_departures(self):
        for n in model.pins()['sources']:
            p=model.spec(self.declaration(n),self.oracle[n]);s=self.case(n,'two_sources')['steps'];deaths=[i for i,x in enumerate(s) if x['op']=='destroy'];acts=[i for i,x in enumerate(s) if x['op']=='activate' and x['ref']=='host']
            self.assertEqual([s[i]['ref'] for i in deaths],['source','second']);self.assertEqual(len(acts),2)
            self.assertLess(deaths[0],acts[0]);self.assertLess(acts[0],deaths[1]);self.assertLess(deaths[1],acts[1])
            self.assertEqual(s[acts[0]]['value'],int(not p['no_activate']));self.assertEqual(s[acts[1]]['value'],1)
            if p['no_untap']:
                s=self.case(n,'two_freeze_sources')['steps'];deaths=[i for i,x in enumerate(s) if x['op']=='destroy'];self.assertEqual(len(deaths),2)
                self.assertIn(dict(op='assert_card',ref='host',property='tapped',value=1),s[deaths[0]:deaths[1]])
                self.assertIn(dict(op='assert_card',ref='host',property='tapped',value=0),s[deaths[1]:])

    def test_counter_and_pending_target_inputs_follow_real_order(self):
        for n in model.pins()['sources']:
            for mode in ('countered','host_bounced_pending'):
                s=self.case(n,mode)['steps'];announce=next(i for i,x in enumerate(s) if x['op']=='announce_spell');response=next(i for i,x in enumerate(s) if x['op']=='cast_response');self.assertLess(announce,response)
                self.assertIn(dict(op='resolve_response'),s[response:])
                if mode=='host_bounced_pending':self.assertEqual(s[response-1],dict(op='choose_target',player=0,ref='host'))
                else:self.assertFalse(any(x['op'].startswith('choose_') for x in s[announce:]))

    def test_real_reentry_has_two_actual_aura_casts_and_second_entry_draw(self):
        for n in model.pins()['sources']:
            s=self.case(n,'bounce_recast')['steps'];casts=[i for i,x in enumerate(s) if x['op']=='cast' and x['ref']=='source'];bind=next(i for i,x in enumerate(s) if x['op']=='bind_hand_copy')
            self.assertEqual(len(casts),2);self.assertLess(casts[0],bind);self.assertLess(bind,casts[1])
            if model.spec(self.declaration(n),self.oracle[n])['draw']:
                self.assertIn(dict(op='assert_zone',player=0,zone='library',value=2),s[casts[1]:])

    def test_snapshot_replay_checks_permission_without_zero_cost_query_affordability_confusion(self):
        for n in model.pins()['sources']:
            for mode in ('ready_snapshot','pending_snapshot','attached_snapshot'):
                c=self.case(n,mode);s=c['steps'];cap=next(i for i,x in enumerate(s) if x['op']=='capture');rec=next(i for i,x in enumerate(s) if x['op']=='record_state');restore=next(i for i,x in enumerate(s) if x['op']=='restore')
                self.assertEqual(s[cap+1:rec],s[restore+1:-1]);self.assertTrue(any(x['op']=='can_activate' and x['ref']=='host' for x in s[cap:rec]))
                self.assertTrue(any('no generic attachment' in g for g in c['_recipe']['untested_components']))
                self.assertTrue(any('unavailable priority' in g for g in c['_recipe']['untested_components']))

    def test_native_flash_both_controllers_and_plain_auras_no_offturn_variant(self):
        for n in model.pins()['sources']:
            p=model.spec(self.declaration(n),self.oracle[n])
            if p['flash']:
                for owner in (0,1):self.assertEqual(self.case(n,'offturn',owner)['steps'][0],dict(op='turn',player=1-owner,value=0))
            else:self.assertFalse(any(v.startswith('offturn') for v in model.variants(p)))


if __name__=='__main__':unittest.main()
