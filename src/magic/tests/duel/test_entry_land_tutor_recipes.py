"""Independent land predicate, destination, payment and lifecycle invariants."""
import copy
import json
import unittest
from pathlib import Path

from generate_cases import OPS,PROPERTIES,registry
import entry_land_tutor_recipes as model


class EntryLandTutorTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.oracle=json.loads((Path(__file__).parent/'data/oracle.json').read_text(encoding='utf8'))['cards']
        cls.rows={r['name']:r for r in registry()}

    def declaration(self,name):
        p=model.pins()['sources'][name]
        return dict(family=model.FAMILY,version=2,oracle_text=self.oracle[name]['oracle_text'],variants=['search_0'],params=dict(schema=1,mechanism=model.mechanism(self.oracle[name]),presentation=copy.deepcopy(p['presentation'])))

    def parameters(self,name):return model.spec(self.declaration(name),self.oracle[name])

    def case(self,name,mode='search',owner=0):
        d=self.declaration(name);d['variants']=[f'{mode}_{owner}'];p=model.spec(d,self.oracle[name])
        return model.expand(name,self.rows[name]['csvid'],d,p,d['variants'],self.rows,self.oracle)[0]

    def test_all_eight_whole_oracle_sources_expand_574_valid_operations(self):
        count=0
        for name in model.pins()['sources']:
            d=self.declaration(name);p=model.spec(d,self.oracle[name]);d['variants']=model.variants(p)
            cases=model.expand(name,self.rows[name]['csvid'],d,p,d['variants'],self.rows,self.oracle);count+=len(cases)
            self.assertEqual({int(c['_recipe']['variant'][-1]) for c in cases},{0,1})
            for c in cases:
                self.assertEqual(c['_recipe']['oracle_text'],self.oracle[name]['oracle_text'])
                for line in p['gaps']:self.assertIn(line,c['_recipe']['untested_components'])
                for s in c['steps']:
                    self.assertIn(s['op'],OPS)
                    if 'property' in s:self.assertIn(s['property'],PROPERTIES)
        self.assertEqual(count,574)

    def test_whole_source_oracle_typing_and_cost_cannot_be_rewritten(self):
        for name in model.pins()['sources']:
            d=self.declaration(name)
            for key,value in [('oracle_text',self.oracle[name]['oracle_text']+'\nDraw a card.'),('type_line','Sorcery'),('mana_cost','{0}'),('layout','split')]:
                r=dict(self.oracle[name],**{key:value})
                with self.assertRaises(ValueError):model.spec(dict(d,oracle_text=r['oracle_text']),r)
        self.assertFalse(self.parameters('Mycosynth Wellspring')['creature'])
        self.assertTrue(self.parameters('Solemn Simulacrum')['creature'])
        self.assertEqual(self.oracle['Mycosynth Wellspring']['type_line'],'Artifact')
        self.assertEqual(self.oracle['Solemn Simulacrum']['type_line'],'Artifact Creature — Golem')

    def test_profile_schema_predicate_and_choice_annotations_are_exact(self):
        for mutate in [lambda d:d['params'].__setitem__('schema',True),lambda d:d['params']['mechanism']['predicate'].__setitem__('basic',1),lambda d:d['params']['presentation'].__setitem__('human_entry_trigger',1)]:
            d=self.declaration('Mycosynth Wellspring');mutate(d)
            with self.assertRaises(ValueError):model.spec(d,self.oracle['Mycosynth Wellspring'])

    def test_subtype_search_accepts_nonbasic_dual_and_snow_basic(self):
        for name,yes,no in [("Liliana's Shade",['Swamp','Snow-Covered Swamp','Underground Sea'],['Forest','Tundra','Elvish Mystic']),
                            ('Yavimaya Dryad',['Forest','Snow-Covered Forest','Taiga','Dryad Arbor'],['Swamp','Tarfire']),
                            ('Knight of the White Orchid',['Plains','Snow-Covered Plains','Savannah','Tundra'],['Forest','Darksteel Citadel'])]:
            predicate=self.parameters(name)['predicate']
            for n in yes:self.assertTrue(model.land_eligible(self.oracle[n],predicate),n)
            for n in no:self.assertFalse(model.land_eligible(self.oracle[n],predicate),n)

    def test_basic_and_any_land_distinguish_real_multitype_land_cards(self):
        basic=self.parameters('Solemn Simulacrum')['predicate'];any_land=self.parameters('Ulvenwald Hydra')['predicate']
        for n in ['Forest','Snow-Covered Plains']:self.assertTrue(model.land_eligible(self.oracle[n],basic))
        for n in ['Taiga','Darksteel Citadel','Dryad Arbor','Scrying Sheets']:
            self.assertFalse(model.land_eligible(self.oracle[n],basic));self.assertTrue(model.land_eligible(self.oracle[n],any_land))
        self.assertFalse(model.land_eligible(self.oracle['Tarfire'],any_land))

    def test_exact_source_payment_and_failed_payments_do_not_search(self):
        for owner in (0,1):
            for name in model.pins()['sources']:
                p=self.parameters(name);s=self.case(name,'payment',owner)['steps'];cast=next(i for i,x in enumerate(s) if x['op']=='cast')
                for color,n in enumerate(p['mana']):self.assertIn(dict(op='set_mana',player=owner,color=color,value=n),s[:cast])
                for color in range(6):self.assertIn(dict(op='assert_mana',player=owner,color=color,value=0),s[cast:])
                for mode in ['insufficient_mana']+(['wrong_color'] if any(p['mana'][1:]) else []):
                    bad=self.case(name,mode,owner)['steps'];self.assertIn(dict(op='cast',ref='source',value=0),bad)
                    self.assertFalse(any(x['op'].startswith('choose_') for x in bad))
                    self.assertIn(dict(op='assert_shuffle_count',player=owner,value=0),bad)

    def test_counterspell_uses_actual_pending_context_without_fake_target(self):
        for name in model.pins()['sources']:
            s=self.case(name,'countered')['steps'];ops=[x['op'] for x in s]
            self.assertLess(ops.index('cast'),ops.index('announce_spell'));self.assertLess(ops.index('announce_spell'),ops.index('cast_response'));self.assertLess(ops.index('cast_response'),ops.index('resolve_response'))
            self.assertFalse(any(x['op'].startswith('choose_') for x in s))
            self.assertIn(dict(op='assert_zone_multiplicity',player=0,zone='graveyard',index=self.rows[name]['csvid'],value=1),s)

    def test_granger_wrong_color_cannot_pay_missing_green_from_owned_real_lands(self):
        for owner in (0,1):
            c=self.case('Yavimaya Granger','wrong_color',owner)
            lands=[x for x in c['setup'] if x['zone']=='battlefield' and x['player']==owner and 'Land' in self.oracle[x['name']]['type_line'].partition(' — ')[0].split()]
            self.assertEqual(len(lands),1)
            self.assertFalse(any('Forest' in self.oracle[x['name']]['type_line'].partition(' — ')[2].split() for x in lands))
            self.assertEqual(lands[0]['name'],'Swamp')
            self.assertIn(dict(op='set_mana',player=owner,color=3,value=0),c['steps'])
            self.assertIn(dict(op='cast',ref='source',value=0),c['steps'])

    def test_source_bounce_rebinds_returned_hand_copy_before_unchanged_hand_assertion(self):
        for name in ['Mycosynth Wellspring','Solemn Simulacrum']:
            for owner in (0,1):
                s=self.case(name,'source_bounced',owner)['steps'];bounce=next(i for i,x in enumerate(s) if x['op']=='cast' and x['ref']=='bounce')
                resolve=next(i for i,x in enumerate(s[bounce:],bounce) if x['op']=='resolve')
                bind=next(i for i,x in enumerate(s) if x['op']=='bind_hand_copy' and x['ref']=='source')
                hand=next(i for i,x in enumerate(s[bind:],bind) if x==dict(op='assert_card',ref='source',property='in_hand',value=1))
                self.assertLess(resolve,bind);self.assertLess(bind,hand)
                self.assertEqual(s[bind]['value'],self.rows[name]['csvid'])
                self.assertIn(dict(op='assert_shuffle_count',player=owner,value=1),s[bind:])

    def test_hand_vs_untapped_vs_tapped_destinations_are_independent(self):
        for name,dest in [("Liliana's Shade",'hand'),('Knight of the White Orchid','battlefield'),('Solemn Simulacrum','battlefield_tapped')]:
            self.assertEqual(self.parameters(name)['destination'],dest);s=self.case(name)['steps']
            bound=[x for x in s if x['op']=='bind_permanent_copy']
            self.assertEqual(len(bound),int(dest!='hand'))
            if bound:self.assertIn(dict(op='assert_card',ref=bound[0]['ref'],property='tapped',value=int(dest.endswith('_tapped'))),s)

    def test_optional_decline_and_accepted_fail_to_find_have_different_shuffle_receipts(self):
        for owner in (0,1):
            s=self.case("Liliana's Shade",'accepted_zero',owner)['steps']
            self.assertEqual(sum(x['op']=='choose_optional_zone' for x in s),1)
            self.assertIn(dict(op='assert_shuffle_count',player=owner,value=1),s)
        s=self.case("Liliana's Shade",'whole_decline')['steps']
        self.assertFalse(any(x['op'].startswith('choose_') for x in s))
        self.assertIn(dict(op='assert_shuffle_count',player=0,value=0),s)

    def test_knight_intervening_if_false_and_equal_never_search(self):
        for owner in (0,1):
            for mode in ['equal_lands','own_more_lands']:
                s=self.case('Knight of the White Orchid',mode,owner)['steps']
                self.assertFalse(any(x['op'].startswith('choose_') for x in s))
                self.assertIn(dict(op='assert_shuffle_count',player=owner,value=0),s)
            self.assertIn(dict(op='assert_shuffle_count',player=owner,value=1),self.case('Knight of the White Orchid',owner=owner)['steps'])

    def test_oreskos_true_zero_search_shuffles_once_without_invented_card_choice(self):
        p=self.parameters('Oreskos Explorer');self.assertFalse(p['optional']);self.assertEqual(p['quantity'],'land_comparison')
        for owner in (0,1):
            s=self.case('Oreskos Explorer','equal_lands',owner)['steps']
            self.assertFalse(any(x['op'] in ('choose_zone_card','choose_optional_zone') for x in s))
            self.assertIn(dict(op='assert_shuffle_count',player=owner,value=1),s)
            self.assertNotIn(f'whole_decline_{owner}',model.variants(p))

    def test_dryad_real_player_target_precedes_library_selection_and_control_differs_from_owner(self):
        for owner in (0,1):
            s=self.case('Yavimaya Dryad','recipient_owner_return',owner)['steps']
            target=next(i for i,x in enumerate(s) if x['op']=='choose_player');zone=next(i for i,x in enumerate(s) if x['op']=='choose_zone_card')
            self.assertLess(target,zone);self.assertEqual(s[target]['value'],1-owner)
            found=next(x for x in s if x['op']=='bind_permanent_copy');self.assertEqual(found['player'],1-owner)
            self.assertIn(dict(op='choose_target',player=owner,ref=found['ref']),s)
            forest=self.rows['Forest']['csvid'];self.assertIn(dict(op='assert_zone_multiplicity',player=owner,zone='hand',index=forest,value=1),s)
            self.assertIn(dict(op='assert_zone_multiplicity',player=1-owner,zone='hand',index=forest,value=0),s)

    def test_wellspring_both_disjuncts_search_and_non_death_departure_does_not(self):
        p=self.parameters('Mycosynth Wellspring');self.assertTrue(p['death_search']);self.assertIn('enters or is put into a graveyard',p['entry_clause'])
        for owner in (0,1):
            for mode,count in [('source_destroyed',2),('other_destroyed',1),('source_bounced',1)]:
                s=self.case('Mycosynth Wellspring',mode,owner)['steps'];self.assertIn(dict(op='assert_shuffle_count',player=owner,value=count),s)
                self.assertEqual(sum(x['op']=='choose_zone_card' for x in s),count)

    def test_solemn_real_death_draw_is_exact_despite_search_shuffle(self):
        for owner in (0,1):
            c=self.case('Solemn Simulacrum','source_destroyed',owner);s=c['steps'];forest=self.rows['Forest']['csvid']
            self.assertEqual([x['name'] for x in c['setup'] if x['zone']=='library' and x['player']==owner],['Forest']*4)
            self.assertIn(dict(op='destroy',ref='source'),s)
            self.assertIn(dict(op='assert_zone_multiplicity',player=owner,zone='hand',index=forest,value=1),s)
            self.assertIn(dict(op='assert_zone_multiplicity',player=owner,zone='library',index=forest,value=2),s)
            self.assertTrue(self.parameters('Solemn Simulacrum')['death_draw'])
            destroy=next(i for i,x in enumerate(s) if x['op']=='destroy')
            self.assertEqual(sum(x['op']=='choose_trigger' for x in s[:destroy]),2 if owner==0 else 0)

    def test_hydra_uses_printed_stars_and_real_land_counts_including_multitype_once(self):
        self.assertEqual(self.oracle['Ulvenwald Hydra']['power'],'*');self.assertTrue(self.parameters('Ulvenwald Hydra')['dynamic_stats'])
        for owner in (0,1):
            for mode in ['search','artifact_land_count','land_creature_count']:
                s=self.case('Ulvenwald Hydra',mode,owner)['steps']
                for key in ('power','toughness'):self.assertIn(dict(op='assert_card',ref='source',property=key,value=2),s)
            s=self.case('Ulvenwald Hydra','land_removed',owner)['steps']
            self.assertIn(dict(op='destroy',ref=f'board_land_{owner}_0'),s)
            self.assertIn(dict(op='assert_card',ref='source',property='power',value=1),s)

    def test_printed_black_pump_is_paid_and_expires_at_actual_cleanup(self):
        s=self.case("Liliana's Shade",'pump_expiry')['steps'];activate=next(i for i,x in enumerate(s) if x['op']=='activate');cleanup=next(i for i,x in enumerate(s) if x['op']=='cleanup')
        self.assertLess(activate,cleanup);self.assertIn(dict(op='set_mana',player=0,color=1,value=1),s[:activate])
        self.assertIn(dict(op='assert_mana',player=0,color=1,value=0),s[activate:cleanup])
        self.assertIn(dict(op='assert_card',ref='source',property='power',value=2),s[activate:cleanup])
        self.assertIn(dict(op='assert_card',ref='source',property='power',value=1),s[cleanup:])

    def test_reentry_and_snapshots_use_real_events_and_queue_replay(self):
        for name in model.pins()['sources']:
            p=self.parameters(name)
            for mode in ['ready_snapshot','pending_snapshot']:
                s=self.case(name,mode)['steps'];capture=next(i for i,x in enumerate(s) if x['op']=='capture');restore=next(i for i,x in enumerate(s) if x['op']=='restore')
                first=s[capture+1:next(i for i,x in enumerate(s) if x['op']=='record_state')];second=s[restore+1:-1];self.assertEqual(first,second)
            s=self.case(name,'bounce_recast')['steps'];self.assertEqual(sum(x['op']=='cast' and x['ref']=='source' for x in s),2)
            if p['condition']=='none':self.assertIn(dict(op='assert_shuffle_count',player=0,value=2),s)
            if not p['creature']:self.assertNotIn('blink_source_0',model.variants(p))


if __name__=='__main__':unittest.main()
