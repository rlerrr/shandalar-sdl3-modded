"""Independent Oracle and lifecycle guards for mana Auras; no runtime calls."""
import copy
import json
import unittest
from pathlib import Path
from generate_cases import OPS,PROPERTIES,registry
import aura_mana_enhancement_recipes as m

class AuraManaEnhancementTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.oracle=json.loads((Path(__file__).parent/'data/oracle.json').read_text(encoding='utf8'))['cards'];cls.rows={r['name']:r for r in registry()}
    def decl(self,n):return dict(family=m.FAMILY,version=2,oracle_text=self.oracle[n]['oracle_text'],params=dict(schema=1,mechanism=m.mechanism(self.oracle[n])))
    def case(self,n,mode='value',owner=0):
        d=self.decl(n);d['variants']=[f'{mode}_{owner}'];p=m.spec(d,self.oracle[n]);return m.expand(n,p['csvid'],d,p,d['variants'],self.rows,self.oracle)[0]
    def test_eleven_independent_full_Oracle_mechanisms(self):
        expected={'Abundant Growth':('land','grant','any_one',1,1,0),"Karametra's Favor":('creature','grant','any_one',1,1,0),'Sheltered Aerie':('land','grant','any_one',2,0,0),'Gift of Paradise':('land','grant','any_one',2,0,3),'Fertile Ground':('land','bonus','any_one',1,0,0),'Overgrowth':('land','bonus','fixed_green',2,0,0),'Utopia Sprawl':('Forest','bonus','chosen',1,0,0),'Market Festival':('land','bonus','any_combo',2,0,0),'Urban Utopia':('land','grant','any_one',1,1,0),'Verdant Haven':('land','bonus','any_one',1,0,2),"Dawn's Reflection":('land','bonus','any_combo',2,0,0)}
        self.assertEqual(set(expected),set(m.pins()['sources']))
        for n,expected in expected.items():
            p=m.spec(self.decl(n),self.oracle[n]);self.assertEqual(tuple(p[k] for k in ('scope','kind','output','amount','draw','life')),expected);self.assertEqual(p['entry_color'],n=='Utopia Sprawl')
    def test_reserved_followers_have_real_costs_and_calibrated_components(self):
        expected={'Urban Utopia':([1,0,0,1,0,0],66),'Verdant Haven':([2,0,0,1,0,0],66),"Dawn's Reflection":([3,0,0,1,0,0],68)}
        for n,(cost,count) in expected.items():
            p=m.spec(self.decl(n),self.oracle[n]);self.assertEqual(p['mana'],cost);self.assertEqual(len(m.variants(p)),count);self.assertEqual(self.oracle[n]['type_line'],'Enchantment — Aura')
        self.assertEqual(m.mechanism(self.oracle['Urban Utopia']),m.mechanism(self.oracle['Abundant Growth']))
        self.assertEqual(m.mechanism(self.oracle["Dawn's Reflection"]),m.mechanism(self.oracle['Market Festival']))
        for owner in (0,1):
            s=self.case('Verdant Haven','foreign_controller',owner)['steps'];self.assertIn(dict(op='assert_life',player=owner,value=22),s);self.assertIn(dict(op='assert_life',player=1-owner,value=20),s)
            self.assertIn(dict(op='assert_mana',player=1-owner,color=3,value=2),s);self.assertIn(dict(op='activate',ref='host',value=1),s);self.assertFalse(any(x['op']=='activate' and x.get('ref')=='source' for x in s))

    def test_full_type_cost_text_and_profile_pins(self):
        for n in m.pins()['sources']:
            for key,v in [('mana_cost','{0}'),('layout','transform'),('type_line','Creature'),('oracle_text',self.oracle[n]['oracle_text']+'\nDraw a card.')]:
                r=dict(self.oracle[n],**{key:v});d=self.decl(n);d['oracle_text']=r['oracle_text']
                with self.assertRaises(ValueError):m.spec(d,r)
            d=self.decl(n);d['params']['mechanism']['amount']=True
            with self.assertRaises(ValueError):m.spec(d,self.oracle[n])
    def test_live_primaries_and_honest_static_hosts(self):
        m.validate_witnesses(self.oracle,self.rows)
        for n in m.pins()['sources']:self.assertNotIn(self.rows[n]['handler'],('NULL','card_dummy',None))
        self.assertEqual(m.pins()['honest_static_only_dummy_witnesses'],['Grizzly Bears'])
        self.assertEqual(self.oracle['Grizzly Bears']['oracle_text'],'')
    def test_all_new_cases_have_complete_provenance_and_valid_DSL(self):
        self.assertEqual(sum(len(m.variants(m.spec(self.decl(n),self.oracle[n]))) for n in m.pins()['sources']),732)
        for n in m.pins()['sources']:
            d=self.decl(n);p=m.spec(d,self.oracle[n]);vs=m.variants(p);self.assertEqual({v[-1] for v in vs},{'0','1'})
            for c in m.expand(n,p['csvid'],d,p,vs,self.rows,self.oracle):
                self.assertEqual(c['_recipe']['covered_clauses'],self.oracle[n]['oracle_text'].splitlines());self.assertEqual(len(c['setup']),len({e['ref'] for e in c['setup']}))
                for s in c['steps']:
                    self.assertIn(s['op'],OPS)
                    if 'property' in s:self.assertIn(s['property'],PROPERTIES)
    def test_grants_use_provider_only_for_actual_same_controller_host(self):
        for n in ('Abundant Growth',"Karametra's Favor",'Sheltered Aerie','Gift of Paradise'):
            for o in (0,1):
                c=self.case(n,owner=o);s=c['steps'];self.assertIn(dict(op='activate',ref='source',value=1),s);self.assertEqual(next(e['player'] for e in c['setup'] if e['ref']=='host'),o)
                self.assertIn(dict(op='assert_card',ref='host',property='tapped',value=1),s)
                self.assertIn(dict(op='assert_card',ref='source',property='tapped',value=0),s)
    def test_additional_mana_uses_real_host_and_actual_controller(self):
        for n in ('Fertile Ground','Overgrowth','Utopia Sprawl','Market Festival'):
            for o in (0,1):
                c=self.case(n,'foreign_controller',o);s=c['steps'];self.assertEqual(next(e['player'] for e in c['setup'] if e['ref']=='host'),1-o);self.assertIn(dict(op='activate',ref='host',value=1),s);self.assertNotIn(dict(op='activate',ref='source',value=1),s)
                self.assertIn(dict(op='assert_mana',player=1-o,color=3,value=3 if n in ('Overgrowth','Market Festival') else 2),s)
    def test_manual_tap_does_not_claim_mana_or_trigger_bonus(self):
        for n in m.pins()['sources']:
            s=self.case(n,'manual_tap')['steps'];self.assertIn(dict(op='cast',ref='twiddle',value=1),s);self.assertFalse(any(x['op']=='activate' for x in s))
            for o in (0,1):self.assertIn(dict(op='assert_mana',player=o,color=3,value=0),s)
    def test_foreign_land_query_never_queues_illegal_target_or_fake_actor(self):
        for n in ('Abundant Growth','Sheltered Aerie','Gift of Paradise'):
            for o in (0,1):
                s=self.case(n,'foreign_land_legality',o)['steps'];self.assertIn(dict(op='can_cast',ref='source',value=1),s);self.assertFalse(any(x['op'] in ('choose_target','cast','activate') for x in s))
    def test_cross_controller_pair_is_real_host_actor_output_and_replay(self):
        for o in (0,1):
            c=self.case("Karametra's Favor",'host_grant_availability',o);s=c['steps'];self.assertIn(dict(op='cast',ref='source',value=1),s)
            self.assertFalse(any(x['op'] in ('can_activate','activate') for x in s));self.assertEqual(sum(x==dict(op='activate_granted_mana',ref='host',other='source',value=1) for x in s),2)
            self.assertEqual(sum(x==dict(op='choose_color',player=1-o,value=3) for x in s),2)
            self.assertIn(dict(op='assert_mana',player=1-o,color=3,value=1),s);self.assertIn(dict(op='assert_mana',player=o,color=3,value=0),s)
            self.assertIn(dict(op='assert_card',ref='host',property='tapped',value=1),s);self.assertIn(dict(op='assert_card',ref='source',property='tapped',value=0),s)
            self.assertIn(dict(op='assert_recorded_state'),s);destroy=next(i for i,x in enumerate(s) if x['op']=='destroy')
            self.assertIn(dict(op='activate_granted_mana',ref='host',other='source',value=0),s[destroy:]);self.assertTrue(any('UI ability discovery' in x for x in c['_recipe']['untested_components']))
    def test_pair_schema_contains_no_actor_payment_or_extra_fields(self):
        from generate_cases import validate_granted_mana_action
        for o in (0,1):
            for s in self.case("Karametra's Favor",'host_grant_availability',o)['steps']:
                if s['op'] in ('can_activate_granted_mana','activate_granted_mana'):
                    self.assertEqual(set(s),{'op','ref','other','value'});validate_granted_mana_action(s)
                    for k,v in [('player',o),('actor',o),('payer',o),('value',True),('other','')]:
                        with self.assertRaises(ValueError):validate_granted_mana_action(dict(s,**{k:v}))
    def test_real_cast_affordability_and_target_absence_do_not_use_raw_query(self):
        for n in m.pins()['sources']:
            for mode in ('insufficient_mana','wrong_color','no_legal_host'):
                s=self.case(n,mode)['steps'];self.assertIn(dict(op='cast',ref='source',value=0),s);self.assertFalse(any(x['op'].startswith('choose_') or x['op']=='can_cast' for x in s))
    def test_unaffordable_casts_have_no_live_host_mana_escape(self):
        for n in m.pins()['sources']:
            for mode in ('insufficient_mana','wrong_color'):
                c=self.case(n,mode);self.assertIn('tapped',next(e['state'] for e in c['setup'] if e['ref']=='host'))
                self.assertFalse(any(e['zone']=='battlefield' and e['ref']!='host' for e in c['setup']))

    def test_real_source_costs_have_no_free_production(self):
        costs={'Abundant Growth':[0,0,0,1,0,0],"Karametra's Favor":[1,0,0,1,0,0],'Sheltered Aerie':[2,0,0,1,0,0],'Gift of Paradise':[2,0,0,1,0,0],'Fertile Ground':[1,0,0,1,0,0],'Overgrowth':[2,0,0,1,0,0],'Utopia Sprawl':[0,0,0,1,0,0],'Market Festival':[3,0,0,1,0,0]}
        for n,v in costs.items():
            p=m.spec(self.decl(n),self.oracle[n]);self.assertEqual(p['mana'],v);s=self.case(n,'payment')['steps'];self.assertFalse(any(x['op']=='activate' for x in s));self.assertIn(dict(op='assert_mana',player=0,color=3,value=0),s)
    def test_five_color_production_and_same_color_two_mana(self):
        for n in ('Abundant Growth','Sheltered Aerie','Gift of Paradise'):
            for color in 'BUGRW':
                s=self.case(n,'color_'+color.lower())['steps'];self.assertIn(dict(op='choose_color',player=0,value=m.COLORS[color]),s)
                self.assertIn(dict(op='assert_mana',player=0,color=m.COLORS[color],value=1 if n=='Abundant Growth' else 2),s)
    def test_mixed_output_two_separate_choices_plus_base_green(self):
        s=self.case('Market Festival','mixed_colors')['steps'];a=next(i for i,x in enumerate(s) if x['op']=='activate');self.assertEqual([x['value'] for x in s[:a] if x['op']=='choose_color'],[2,4])
        for color in (2,3,4):self.assertIn(dict(op='assert_mana',player=0,color=color,value=1),s[a:])
    def test_Forest_predicate_includes_nonbasic_and_rejects_wrong_land(self):
        p=m.spec(self.decl('Utopia Sprawl'),self.oracle['Utopia Sprawl']);self.assertTrue(m.eligible(self.oracle['Tropical Island'],p));self.assertFalse(m.eligible(self.oracle['Island'],p));s=self.case('Utopia Sprawl','nonbasic_forest')['steps'];a=next(i for i,x in enumerate(s) if x['op']=='activate');self.assertIn(dict(op='choose_color',player=0,value=3),s[:a]);self.assertIn(dict(op='can_cast',ref='source',value=0),self.case('Utopia Sprawl','wrong_land_type_query')['steps'])
    def test_entry_color_is_before_resolve_and_not_rechosen_for_bonus(self):
        s=self.case('Utopia Sprawl','color_b')['steps'];cast=next(i for i,x in enumerate(s) if x['op']=='cast');resolve=next(i for i,x in enumerate(s) if x['op']=='resolve');colors=[i for i,x in enumerate(s) if x['op']=='choose_color'];self.assertEqual(len(colors),1);self.assertLess(cast,colors[0]);self.assertLess(colors[0],resolve);self.assertIn(dict(op='assert_mana',player=0,color=1,value=1),s)
    def test_next_untap_spends_every_real_produced_point_first(self):
        for n in m.pins()['sources']:
            s=self.case(n,'real_next_untap')['steps'];first=next(i for i,x in enumerate(s) if x['op']=='untap_step');spent=[x for x in s[:first] if x['op']=='cast' and x['ref'].startswith('spender_')];p=m.spec(self.decl(n),self.oracle[n]);self.assertEqual(len(spent),p['amount']+(p['kind']=='bonus'));self.assertEqual(sum(x['op']=='advance_turn' for x in s),2);self.assertEqual(sum(x['op']=='activate' and x['value']==1 for x in s),2)
    def test_creature_sickness_uses_real_entry_and_later_turns(self):
        s=self.case("Karametra's Favor",'fresh_creature')['steps'];self.assertIn(dict(op='cast',ref='host',value=1),s);self.assertIn(dict(op='activate',ref='source',value=0),s);s=self.case("Karametra's Favor",'aged_creature')['steps'];self.assertEqual(sum(x['op']=='advance_turn' for x in s),2);self.assertIn(dict(op='activate',ref='source',value=1),s)
    def test_departed_grant_does_not_give_vanilla_host_printed_mana(self):
        for mode in ('source_destroyed','source_bounced','base_ability'):
            s=self.case("Karametra's Favor",mode)['steps'];self.assertIn(dict(op='activate',ref='host',value=0),s)
    def test_two_sources_grants_are_alternatives_but_bonuses_stack(self):
        for n,total in [('Abundant Growth',1),('Sheltered Aerie',2),('Fertile Ground',3),('Overgrowth',5),('Market Festival',5)]:
            s=self.case(n,'two_sources')['steps'];self.assertIn(dict(op='assert_mana',player=0,color=3,value=total),s)
    def test_replay_has_actual_actions_and_observed_state_equality(self):
        for n in m.pins()['sources']:
            for mode in ('ready_snapshot','pending_snapshot','attached_snapshot'):
                s=self.case(n,mode)['steps'];self.assertEqual(sum(x['op']=='capture' for x in s),1);self.assertEqual(sum(x['op']=='restore' for x in s),1);self.assertEqual(s[-1],dict(op='assert_recorded_state'));self.assertEqual(sum(x['op']=='activate' for x in s),2)
    def test_actual_bounce_destinations_preserve_original_owner(self):
        for n in m.pins()['sources']:
            c=self.case(n,'foreign_owned_host');s=c['steps'];self.assertIn('opponent_owned',next(e['state'] for e in c['setup'] if e['ref']=='host'));target=next(e['name'] for e in c['setup'] if e['ref']=='host');self.assertIn(dict(op='assert_zone_multiplicity',player=1,zone='hand',index=self.rows[target]['csvid'],value=1),s)
    def test_counterspell_uses_native_pending_parent_without_unoffered_target(self):
        for n in m.pins()['sources']:
            for owner in (0,1):
                s=self.case(n,'countered',owner)['steps'];a=next(i for i,x in enumerate(s) if x['op']=='cast_response');self.assertEqual(s[a-1],dict(op='announce_spell',ref='source'));self.assertEqual([x for x in s if x['op']=='choose_target'],[dict(op='choose_target',player=owner,ref='host')]);self.assertIn(dict(op='resolve_response'),s);self.assertFalse(any(x['op']=='choose_color' for x in s))

if __name__=='__main__':unittest.main()
