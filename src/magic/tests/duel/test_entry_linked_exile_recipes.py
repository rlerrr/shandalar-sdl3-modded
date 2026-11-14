"""Independent linked ownership, source lifecycle, filter and callback guards."""
import copy
import json
import unittest
from pathlib import Path

from generate_cases import OPS,PROPERTIES,registry
import entry_linked_exile_recipes as model


class EntryLinkedExileTest(unittest.TestCase):
    PILOT=('Banisher Priest','Banishing Light','Suspension Field','Silkwrap','Seal Away','Thopter Arrest')
    FOLLOW=('Fairgrounds Warden',"Hieromancer's Cage",'Stasis Snare','Isolation Zone')
    @classmethod
    def setUpClass(cls):
        cls.oracle=json.loads((Path(__file__).parent/'data/oracle.json').read_text(encoding='utf8'))['cards'];cls.rows={r['name']:r for r in registry()}

    def declaration(self,n):
        s=model.pins()['sources'][n]
        return dict(family=model.FAMILY,version=2,oracle_text=self.oracle[n]['oracle_text'],variants=['exile_0'],params=dict(schema=1,mechanism=model.mechanism(self.oracle[n]),presentation=copy.deepcopy(s['presentation'])))

    def parameters(self,n):return model.spec(self.declaration(n),self.oracle[n])

    def case(self,n,mode='exile',owner=0):
        d=self.declaration(n);d['variants']=[f'{mode}_{owner}'];p=model.spec(d,self.oracle[n]);return model.expand(n,self.rows[n]['csvid'],d,p,d['variants'],self.rows,self.oracle)[0]

    def test_six_whole_oracle_sources_expand_381_valid_cases(self):
        count=0
        for n in self.PILOT:
            d=self.declaration(n);p=model.spec(d,self.oracle[n]);d['variants']=model.variants(p);cases=model.expand(n,self.rows[n]['csvid'],d,p,d['variants'],self.rows,self.oracle);count+=len(cases)
            self.assertEqual({int(c['_recipe']['variant'][-1]) for c in cases},{0,1})
            for c in cases:
                self.assertEqual(c['_recipe']['oracle_text'],self.oracle[n]['oracle_text']);self.assertEqual(set(c['_recipe']['covered_clauses']),set(self.oracle[n]['oracle_text'].splitlines()))
                for s in c['steps']:
                    self.assertIn(s['op'],OPS)
                    if 'property' in s:self.assertIn(s['property'],PROPERTIES)
        self.assertEqual(count,381)

    def test_four_exact_followers_expand_252_valid_whole_oracle_cases(self):
        self.assertEqual(set(model.pins()['sources']),set(self.PILOT+self.FOLLOW));count=0
        for n in self.FOLLOW:
            d=self.declaration(n);p=model.spec(d,self.oracle[n]);d['variants']=model.variants(p);cases=model.expand(n,self.rows[n]['csvid'],d,p,d['variants'],self.rows,self.oracle);count+=len(cases)
            self.assertEqual({int(c['_recipe']['variant'][-1]) for c in cases},{0,1})
            for c in cases:
                self.assertEqual(c['_recipe']['oracle_text'],self.oracle[n]['oracle_text']);self.assertEqual(set(c['_recipe']['covered_clauses']),set(self.oracle[n]['oracle_text'].splitlines()))
                for s in c['steps']:
                    self.assertIn(s['op'],OPS)
                    if 'property' in s:self.assertIn(s['property'],PROPERTIES)
        self.assertEqual(count,252)

    def test_followers_retain_actual_source_costs_stats_flash_and_enchantment_union(self):
        expected={'Fairgrounds Warden':[2,0,0,0,0,1],"Hieromancer's Cage":[3,0,0,0,0,1],'Stasis Snare':[1,0,0,0,0,2],'Isolation Zone':[2,0,0,0,0,2]}
        for n,cost in expected.items():self.assertEqual(self.parameters(n)['mana'],cost)
        p=self.parameters('Stasis Snare');self.assertTrue(p['flash']);self.assertFalse(p['creature']);self.assertFalse(p['optional'])
        for owner in (0,1):
            c=self.case('Stasis Snare','offturn',owner);self.assertEqual(c['steps'][0],dict(op='turn',player=1-owner,value=0));self.assertIn(dict(op='cast',ref='source',value=1),c['steps'])
            s=self.case('Fairgrounds Warden',owner=owner)['steps'];self.assertIn(dict(op='assert_card',ref='source',property='power',value=1),s);self.assertIn(dict(op='assert_card',ref='source',property='toughness',value=3),s)
        p=self.parameters('Isolation Zone')
        for n in ['Seal of Cleansing','Ornithopter','Dryad Arbor']:self.assertTrue(model.eligible(self.oracle[n],p,0,1),n)
        for n in ['Sol Ring','Darksteel Citadel']:self.assertFalse(model.eligible(self.oracle[n],p,0,1),n)
        self.assertFalse(model.eligible(self.oracle['Seal of Cleansing'],p,0,0))

    def test_full_normal_source_types_costs_and_clauses_are_pinned(self):
        for n in model.pins()['sources']:
            d=self.declaration(n)
            for key,value in [('oracle_text',self.oracle[n]['oracle_text']+'\nDraw a card.'),('mana_cost','{0}'),('type_line','Instant'),('layout','split')]:
                r=dict(self.oracle[n],**{key:value})
                with self.assertRaises(ValueError):model.spec(dict(d,oracle_text=r['oracle_text']),r)
        self.assertTrue(self.parameters('Banisher Priest')['creature']);self.assertFalse(self.parameters('Banishing Light')['creature']);self.assertTrue(self.parameters('Seal Away')['flash'])

    def test_until_duration_is_not_rewritten_to_separate_leave_trigger(self):
        for n in model.pins()['sources']:
            r=copy.deepcopy(self.oracle[n]);r['oracle_text']=r['oracle_text'].replace('until this creature leaves the battlefield.','then return it.').replace('until this enchantment leaves the battlefield.','then return it.')
            with self.assertRaises(ValueError):model.mechanism(r)
        d=self.declaration('Suspension Field');d['params']['mechanism']['predicate']['toughness_min']=True
        with self.assertRaises(ValueError):model.spec(d,self.oracle['Suspension Field'])

    def test_predicate_union_land_exclusion_and_mv_are_independent(self):
        priest=self.parameters('Banisher Priest');light=self.parameters('Banishing Light');arrest=self.parameters('Thopter Arrest');silk=self.parameters('Silkwrap')
        for n in ['Ornithopter','Dryad Arbor']:
            self.assertTrue(model.eligible(self.oracle[n],priest,0,1));self.assertTrue(model.eligible(self.oracle[n],silk,0,1))
        self.assertFalse(model.eligible(self.oracle['Dryad Arbor'],light,0,1));self.assertFalse(model.eligible(self.oracle['Darksteel Citadel'],light,0,1))
        self.assertTrue(model.eligible(self.oracle['Darksteel Citadel'],arrest,0,1));self.assertFalse(model.eligible(self.oracle['Seal of Cleansing'],arrest,0,1))
        self.assertTrue(model.eligible(self.oracle['Horned Turtle'],silk,0,1));self.assertFalse(model.eligible(self.oracle['Giant Spider'],silk,0,1))
        self.assertFalse(model.eligible(self.oracle['Elvish Mystic'],priest,0,0))

    def test_current_toughness_and_any_controller_and_tapped_predicates(self):
        field=self.parameters('Suspension Field');seal=self.parameters('Seal Away')
        self.assertTrue(model.eligible(self.oracle['Hill Giant'],field,0,0));self.assertFalse(model.eligible(self.oracle['Ornithopter'],field,0,1))
        self.assertTrue(model.eligible(self.oracle['Elvish Mystic'],field,0,0,toughness=4));self.assertFalse(model.eligible(self.oracle['Elvish Mystic'],field,0,0,toughness=1))
        self.assertFalse(model.eligible(self.oracle['Elvish Mystic'],seal,0,1,tapped=False));self.assertTrue(model.eligible(self.oracle['Elvish Mystic'],seal,0,1,tapped=True))

    def test_exact_cost_and_color_denial_do_not_use_owned_white_producers(self):
        for n in model.pins()['sources']:
            p=self.parameters(n)
            for owner in (0,1):
                s=self.case(n,'payment',owner)['steps'];cast=next(i for i,x in enumerate(s) if x['op']=='cast')
                for color,a in enumerate(p['mana']):self.assertIn(dict(op='set_mana',player=owner,color=color,value=a),s[:cast])
                for mode in ['insufficient_mana','wrong_color']:
                    c=self.case(n,mode,owner);self.assertIn(dict(op='cast',ref='source',value=0),c['steps']);self.assertFalse(any(x['op'].startswith('choose_') for x in c['steps']))
                    ownlands=[x['name'] for x in c['setup'] if x['zone']=='battlefield' and x['player']==owner and 'Land' in self.oracle[x['name']]['type_line'].partition(' — ')[0].split()]
                    self.assertEqual(ownlands,['Swamp']);self.assertIn(dict(op='set_mana',player=owner,color=5,value=0),c['steps'])

    def test_real_exile_and_source_leave_restore_original_owner_not_previous_controller(self):
        for owner in (0,1):
            c=self.case('Banisher Priest','ownership_return',owner);s=c['steps'];target=next(x for x in c['setup'] if x['ref']=='target');i=self.rows[target['name']]['csvid']
            self.assertEqual(target['player'],1-owner);self.assertIn('opponent_owned',target['state'])
            self.assertIn(dict(op='assert_zone_multiplicity',player=owner,zone='exile',index=i,value=1),s)
            destroy=next(j for j,x in enumerate(s) if x['op']=='destroy');bind=next(j for j,x in enumerate(s[destroy:],destroy) if x['op']=='bind_permanent' and x['ref']=='target')
            self.assertEqual(s[bind]['player'],owner)
            self.assertIn(dict(op='assert_zone_multiplicity',player=owner,zone='battlefield',index=i,value=1),s[bind:]);self.assertIn(dict(op='assert_zone_multiplicity',player=1-owner,zone='battlefield',index=i,value=0),s[bind:])

    def test_source_absence_uses_owned_zones_not_stale_physical_slot(self):
        for n in model.pins()['sources']:
            s=self.case(n,'source_destroyed')['steps'];destroy=next(i for i,x in enumerate(s) if x['op']=='destroy')
            self.assertFalse(any(x['op']=='assert_card' and x['ref']=='source' for x in s[destroy:]))
            i=self.rows[n]['csvid'];self.assertIn(dict(op='assert_zone_multiplicity',player=0,zone='battlefield',index=i,value=0),s[destroy:]);self.assertIn(dict(op='assert_zone_multiplicity',player=0,zone='graveyard',index=i,value=1),s[destroy:])

    def test_real_returned_entry_has_initial_cast_and_two_mandatory_draws(self):
        for n in ['Banisher Priest','Suspension Field']:
            for owner in (0,1):
                c=self.case(n,'returned_etb',owner);s=c['steps'];target=next(x for x in c['setup'] if x['ref']=='target');self.assertEqual(target['zone'],'hand')
                self.assertEqual(target['name'],'Wall of Omens' if n=='Suspension Field' else 'Elvish Visionary')
                cast_target=next(i for i,x in enumerate(s) if x['op']=='cast' and x['ref']=='target');cast_source=next(i for i,x in enumerate(s) if x['op']=='cast' and x['ref']=='source')
                self.assertLess(cast_target,cast_source);self.assertIn(dict(op='advance_turn'),s[cast_target:cast_source])
                forest=self.rows['Forest']['csvid'];self.assertIn(dict(op='assert_zone_multiplicity',player=1-owner,zone='hand',index=forest,value=1),s[:cast_source])
                self.assertIn(dict(op='assert_zone_multiplicity',player=1-owner,zone='hand',index=forest,value=2),s[cast_source:])
                self.assertIn(dict(op='assert_zone',player=1-owner,zone='library',value=0),s[cast_source:])

    def test_suspension_real_pump_makes_legal_target_and_return_is_new_base_object(self):
        for owner in (0,1):
            s=self.case('Suspension Field','toughness_pump_return',owner)['steps'];pump=next(i for i,x in enumerate(s) if x['op']=='cast' and x['ref']=='pump');source=next(i for i,x in enumerate(s) if x['op']=='cast' and x['ref']=='source')
            self.assertLess(pump,source);self.assertIn(dict(op='assert_card',ref='target',property='toughness',value=4),s[pump:source])
            bind=max(i for i,x in enumerate(s) if x['op']=='bind_permanent' and x['ref']=='target')
            self.assertIn(dict(op='assert_card',ref='target',property='toughness',value=1),s[bind:]);self.assertIn(dict(op='assert_card',ref='target',property='power',value=1),s[bind:])

    def test_two_sources_preserve_distinct_links_and_return_one_target_at_a_time(self):
        for n in model.pins()['sources']:
            c=self.case(n,'two_sources');s=c['steps'];a=next(x['name'] for x in c['setup'] if x['ref']=='target');b=next(x['name'] for x in c['setup'] if x['ref']=='alternate');self.assertNotEqual(a,b)
            deaths=[i for i,x in enumerate(s) if x['op']=='destroy'];self.assertEqual([s[i]['ref'] for i in deaths],['source','second'])
            between=s[deaths[0]:deaths[1]];self.assertIn(dict(op='assert_zone_multiplicity',player=1,zone='exile',index=self.rows[a]['csvid'],value=0),between);self.assertIn(dict(op='assert_zone_multiplicity',player=1,zone='exile',index=self.rows[b]['csvid'],value=1),between)
            self.assertIn(dict(op='assert_zone_multiplicity',player=0,zone='graveyard',index=self.rows[n]['csvid'],value=2),s[deaths[-1]:])

    def test_mandatory_refusal_uses_real_policy_receipt_and_optional_refusal_uses_zero(self):
        for owner in (0,1):
            for n in model.pins()['sources']:
                if self.parameters(n)['optional']:continue
                s=self.case(n,'mandatory_cancel',owner)['steps'];self.assertEqual(sum(x['op']=='choose_optional_target' for x in s),1);self.assertIn(dict(op='assert_forced_target_choices',value=1),s)
            s=self.case('Suspension Field','decline',owner)['steps'];self.assertIn(dict(op='assert_forced_target_choices',value=0),s);self.assertEqual(sum(x['op']=='choose_trigger' for x in s),int(owner==0));self.assertEqual(sum(x['op']=='choose_optional_target' for x in s),1)

    def test_no_targets_and_untapped_target_have_no_fake_cast_target_restriction(self):
        for n in model.pins()['sources']:
            for mode in ['no_targets']+(['untapped_target'] if self.parameters(n)['predicate']['tapped'] else []):
                s=self.case(n,mode)['steps'];self.assertIn(dict(op='cast',ref='source',value=1),s)
                resolve=next(i for i,x in enumerate(s) if x['op']=='resolve');unused=next(i for i,x in enumerate(s) if x['op']=='assert_unused_choices');self.assertLess(resolve,unused);self.assertEqual(s[unused]['value'],1)

    def test_blink_reentry_chooses_stable_alternate_and_rebinds_returned_target_after_resolution(self):
        s=self.case('Banisher Priest','source_blinked')['steps'];cast=next(i for i,x in enumerate(s) if x['op']=='cast' and x['ref']=='blink');resolve=next(i for i,x in enumerate(s[cast:],cast) if x['op']=='resolve')
        self.assertIn(dict(op='choose_target',player=0,ref='alternate'),s[cast:resolve]);self.assertFalse(any(x['op'].startswith('bind_') for x in s[cast:resolve]))
        self.assertIn(dict(op='bind_permanent',player=1,value=self.rows['Elvish Mystic']['csvid'],ref='target'),s[resolve:])

    def test_flash_enchantment_is_actually_cast_offturn_and_source_exile_returns_link(self):
        for owner in (0,1):
            c=self.case('Seal Away','offturn',owner);self.assertEqual(c['steps'][0],dict(op='turn',player=1-owner,value=0));self.assertEqual(next(x['zone'] for x in c['setup'] if x['ref']=='source'),'hand')
            s=self.case('Thopter Arrest','source_exiled',owner)['steps'];i=self.rows['Thopter Arrest']['csvid'];self.assertIn(dict(op='cast',ref='exiler',value=1),s);self.assertIn(dict(op='assert_zone_multiplicity',player=owner,zone='exile',index=i,value=1),s)

    def test_link_snapshot_replays_real_departure_and_counter_uses_pending_context(self):
        for n in model.pins()['sources']:
            for mode in ['ready_snapshot','pending_snapshot','linked_snapshot']:
                s=self.case(n,mode)['steps'];capture=next(i for i,x in enumerate(s) if x['op']=='capture');record=next(i for i,x in enumerate(s) if x['op']=='record_state');restore=next(i for i,x in enumerate(s) if x['op']=='restore');self.assertEqual(s[capture+1:record],s[restore+1:-1])
            s=self.case(n,'countered')['steps'];ops=[x['op'] for x in s];self.assertLess(ops.index('announce_spell'),ops.index('cast_response'));self.assertLess(ops.index('cast_response'),ops.index('resolve_response'));self.assertFalse(any(x['op'].startswith('choose_') for x in s))


if __name__=='__main__':unittest.main()
