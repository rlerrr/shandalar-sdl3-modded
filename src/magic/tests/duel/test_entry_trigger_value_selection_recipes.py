"""Whole-source, typed lifecycle, Oracle filtering, order and replay guards."""
import copy
import json
import unittest
from collections import Counter
from pathlib import Path

from generate_cases import OPS, PROPERTIES, registry
import entry_trigger_value_recipes as dispatch
import entry_trigger_value_selection_recipes as selection


class EntrySelectionTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.oracle = json.loads((Path(__file__).parent/'data/oracle.json').read_text(encoding='utf8'))['cards']
        cls.rows = {row['name']:row for row in registry()}

    def declaration(self,name):
        profile=selection.pins()['sources'][name]
        return dict(family=selection.FAMILY,version=2,oracle_text=self.oracle[name]['oracle_text'],variants=['effect_0'],
                    params=dict(schema=3,mechanism=copy.deepcopy(profile['mechanism']),presentation=copy.deepcopy(profile['presentation'])))

    def parameters(self,name):
        return dispatch.spec(self.declaration(name),self.oracle[name])

    def cases(self,name,variants=None):
        d=self.declaration(name);p=dispatch.spec(d,self.oracle[name]);selected=variants or dispatch.variants(p);d['variants']=selected
        return dispatch.expand(name,self.rows[name]['csvid'],d,p,selected,self.rows,self.oracle)

    def test_every_source_expands_supported_ops_both_controllers_and_full_receipts(self):
        for name in selection.pins()['sources']:
            cases=self.cases(name)
            self.assertEqual({int(c['_recipe']['variant'][-1]) for c in cases},{0,1})
            self.assertEqual(len(cases),len({c['name'] for c in cases}))
            for c in cases:
                self.assertEqual(c['_recipe']['oracle_text'],self.oracle[name]['oracle_text'])
                for line in self.parameters(name)['gaps']:self.assertIn(line,c['_recipe']['untested_components'])
                for s in c['steps']:
                    self.assertIn(s['op'],OPS)
                    if 'property' in s:self.assertIn(s['property'],PROPERTIES)

    def test_whole_source_and_reviewed_parameters_are_independently_pinned(self):
        for name in selection.pins()['sources']:
            d=self.declaration(name)
            for key,value in [('oracle_text',self.oracle[name]['oracle_text']+'\nDraw a card.'),('type_line','Creature'),('mana_cost','{99}'),('layout','split')]:
                record=dict(self.oracle[name],**{key:value})
                with self.assertRaises(ValueError):dispatch.spec(dict(d,oracle_text=record['oracle_text']),record)
            changed=copy.deepcopy(d);changed['params']['mechanism']['window']+=1
            with self.assertRaises(ValueError):dispatch.spec(changed,self.oracle[name])
            changed=copy.deepcopy(d);changed['params']['presentation']['selector']='unreviewed'
            with self.assertRaises(ValueError):dispatch.spec(changed,self.oracle[name])

    def test_grammar_uses_complete_clauses_not_source_names_or_passive_keywords(self):
        r=dict(name='Independent Archive',layout='normal',type_line='Artifact',mana_cost='{3}',oracle_text='When this artifact enters, look at the top four cards of your library, then put them back in any order.')
        self.assertEqual(selection.mechanism(r)['window'],4)
        for text in ['When this artifact enters, look at the top four cards of your library, then draw a card.',r['oracle_text']+'\n'+r['oracle_text']]:
            with self.assertRaises(ValueError):selection.mechanism(dict(r,oracle_text=text))

    def test_real_land_and_enchantment_types_do_not_inherit_creature_only_actions(self):
        for name in ('Halimar Depths','Oath of Nissa'):
            p=self.parameters(name);self.assertFalse(p['creature'])
            self.assertFalse(any(v.startswith('blink_source') for v in dispatch.variants(p)))
            self.assertEqual(p['land'],name=='Halimar Depths')
            self.assertEqual(p['legendary'],name=='Oath of Nissa')
            for owner in (0,1):
                c=self.cases(name,[f'effect_{owner}'])[0]
                self.assertEqual(next(x['name'] for x in c['setup'] if x['ref']=='source'),name)
                self.assertFalse(any(s.get('ref')=='source' and s.get('property') in ('power','toughness') for s in c['steps']))
        self.assertFalse(any(v.startswith(('countered_cast','insufficient_mana','wrong_color')) for v in dispatch.variants(self.parameters('Halimar Depths'))))

    def test_halimar_replay_requires_real_later_own_turn_and_tapped_entry(self):
        for owner in (0,1):
            c=self.cases('Halimar Depths',[f'bounce_recast_{owner}'])[0];s=c['steps']
            casts=[i for i,x in enumerate(s) if x['op']=='cast' and x.get('ref')=='source']
            self.assertEqual(len(casts),2)
            self.assertEqual([x['op'] for x in s[casts[1]-2:casts[1]]],['advance_turn','advance_turn'])
            self.assertFalse(any(x['name']=='Walking Atlas' for x in c['setup']))
            self.assertIn(dict(op='assert_card',ref='source',property='tapped',value=1),s)
            self.assertEqual(sum(x['op']=='advance_turn' for x in s),2)

    def test_printed_source_costs_are_exact_and_other_controller_pools_unchanged(self):
        costs={'Augur of Bolas':[1,0,1,0,0,0],'Satyr Wayfinder':[1,0,0,1,0,0],
               'Sea Gate Oracle':[2,0,1,0,0,0],'Tower Geist':[3,0,1,0,0,0],
               'Goblin Ringleader':[3,0,0,0,1,0],'Halimar Depths':[0]*6,
               'Architects of Will':[2,1,1,0,0,0],'Oath of Nissa':[0,0,0,1,0,0]}
        for name,cost in costs.items():
            self.assertEqual(self.parameters(name)['mana'],cost)
            for owner in (0,1):
                s=self.cases(name,[f'payment_{owner}'])[0]['steps'];cast=next(i for i,x in enumerate(s) if x['op']=='cast')
                for color,amount in enumerate(cost):
                    self.assertIn(dict(op='set_mana',player=owner,color=color,value=amount),s[:cast])
                    self.assertIn(dict(op='assert_mana',player=owner,color=color,value=0),s[cast:])
                    self.assertIn(dict(op='assert_mana',player=1-owner,color=color,value=20),s[cast:])

    def test_pending_sources_leave_observable_hand_before_resolution(self):
        for name in selection.pins()['sources']:
            for owner in (0,1):
                s=self.cases(name,[f'effect_{owner}'])[0]['steps'];cast=next(i for i,x in enumerate(s) if x['op']=='cast')
                self.assertEqual(s[cast+1],dict(op='assert_card',ref='source',property='in_hand',value=0))

    def test_predicates_preserve_type_unions_and_noncreature_subtypes(self):
        self.assertTrue(selection.matches(self.oracle['Tarfire'],'goblin'))
        self.assertFalse(selection.matches(self.oracle['Grizzly Bears'],'goblin'))
        for name in ['Lightning Bolt','Counterspell','Divination']:self.assertTrue(selection.matches(self.oracle[name],'instant_or_sorcery'))
        self.assertFalse(selection.matches(self.oracle['Grizzly Bears'],'instant_or_sorcery'))
        for name in ['Grizzly Bears','Forest','Jace Beleren']:self.assertTrue(selection.matches(self.oracle[name],'creature_land_planeswalker'))
        for name in ['Darksteel Relic','Counterspell']:self.assertFalse(selection.matches(self.oracle[name],'creature_land_planeswalker'))

    def test_subtype_cards_include_noncreature_kindred_and_changeling_cards(self):
        for scope in ['human','soldier','zombie','kavu','elf','merfolk']:
            for name in ['Crib Swap','Nameless Inversion']:
                self.assertNotIn('Creature',self.oracle[name]['type_line'].partition(' — ')[0].split())
                self.assertTrue(selection.matches(self.oracle[name],scope))
                self.assertFalse(selection.matches(self.oracle[name],'creature'))
        for name,scope in [("Veteran's Armaments",'soldier'),('Elvish Promenade','elf'),("Aquitect's Will",'merfolk')]:
            self.assertIn('Kindred',self.oracle[name]['type_line'])
            self.assertTrue(selection.matches(self.oracle[name],scope))
            self.assertFalse(selection.matches(self.oracle[name],'creature'))
        self.assertFalse(selection.matches(self.oracle['Tarfire'],'human'))

    def test_island_predicate_includes_nonbasic_island_and_excludes_creature_types(self):
        r=self.oracle['Tropical Island']
        self.assertEqual(r['type_line'],'Land — Forest Island')
        self.assertTrue(selection.matches(r,'island'))
        self.assertTrue(selection.matches(self.oracle['Island'],'island'))
        for name in ['Forest','Crib Swap','Nameless Inversion','Lord of Atlantis']:
            self.assertFalse(selection.matches(self.oracle[name],'island'))
        for owner in (0,1):
            c=self.cases('Merfolk Wayfinder',[f'effect_{owner}'])[0]
            for name in ['Island','Tropical Island']:
                self.assertIn(dict(op='assert_zone_multiplicity',player=owner,zone='hand',index=self.rows[name]['csvid'],value=1),c['steps'])

    def test_whole_card_wording_is_not_silently_narrowed_to_creature_cards(self):
        name='Courageous Outrider';r=self.oracle[name]
        self.assertIn('a Human card',r['oracle_text'])
        self.assertEqual(self.parameters(name)['scope'],'human')
        changed=dict(r,oracle_text=r['oracle_text'].replace('a Human card','a Human creature card'))
        with self.assertRaises(ValueError):selection.mechanism(changed)
        for owner in (0,1):
            s=self.cases(name,[f'alternate_choice_{owner}'])[0]['steps']
            self.assertEqual(next(x['value'] for x in s if x['op']=='choose_zone_card'),self.rows['Crib Swap']['csvid'])
            self.assertIn(dict(op='assert_zone_multiplicity',player=owner,zone='hand',index=self.rows['Crib Swap']['csvid'],value=1),s)
            decline=self.cases(name,[f'decline_{owner}'])[0]['steps']
            self.assertTrue(any(x['op']=='choose_optional_zone' for x in decline))
            self.assertIn(dict(op='assert_forced_zone_choices',value=0),decline)

    def test_all_subtype_effects_have_independent_noncreature_positive_receipts(self):
        positives={'Enlistment Officer':"Veteran's Armaments",'Grave Defiler':'Crib Swap','Kavu Howler':'Crib Swap',
                   'Sylvan Messenger':'Elvish Promenade','Tidal Courier':"Aquitect's Will"}
        for name,positive in positives.items():
            p=self.parameters(name)
            self.assertEqual(p['kind'],'all')
            self.assertNotIn('Creature',self.oracle[positive]['type_line'].partition(' — ')[0].split())
            for owner in (0,1):
                c=self.cases(name,[f'effect_{owner}'])[0]
                self.assertIn(dict(op='assert_zone_multiplicity',player=owner,zone='hand',index=self.rows[positive]['csvid'],value=1),c['steps'])
                library=[x['name'] for x in c['setup'] if x['player']==owner and x['zone']=='library']
                self.assertIn(positive,library[:p['window']])

    def test_new_subtype_palettes_are_scoped_and_normal_implemented_witnesses(self):
        for scope in ['any','creature','artifact','goblin','land','creature_land_planeswalker','instant_or_sorcery']:
            self.assertEqual(selection.predicate_palette(dict(scope=scope)),selection.PALETTE)
        for scope,names in selection.SUBTYPE_WITNESSES.items():
            for name in names:
                r=self.oracle[name];row=self.rows[name]
                self.assertEqual(r['layout'],'normal')
                self.assertEqual(row['kind'],'card')
                self.assertNotIn(row['handler'],['card_dummy','NULL',None])
                self.assertTrue(selection.matches(r,scope))

    def test_each_new_subtype_fixture_enforces_the_original_window_boundary(self):
        for name in ['Enlistment Officer','Grave Defiler','Kavu Howler','Sylvan Messenger','Tidal Courier','Merfolk Wayfinder','Courageous Outrider']:
            p=self.parameters(name);library=selection.library_fixture(p,'outside_window',self.oracle)
            self.assertFalse(any(selection.matches(self.oracle[n],p['scope']) for n in library[:p['window']]))
            self.assertTrue(selection.matches(self.oracle[library[p['window']]],p['scope']))

    def test_outside_window_match_is_excluded_and_all_match_and_duplicates_remain_bounded(self):
        for name in ['Augur of Bolas','Satyr Wayfinder','Goblin Ringleader','Oath of Nissa']:
            p=self.parameters(name);n=p['window']
            library=selection.library_fixture(p,'outside_window',self.oracle)
            self.assertFalse(any(selection.matches(self.oracle[x],p['scope']) for x in library[:n]))
            self.assertTrue(selection.matches(self.oracle[library[n]],p['scope']))
            library=selection.library_fixture(p,'all_match',self.oracle)
            self.assertTrue(all(selection.matches(self.oracle[x],p['scope']) for x in library[:n]))
            self.assertEqual(len(set(selection.library_fixture(p,'duplicates',self.oracle)[:n])),1)

    def test_optional_decline_and_filter_receipts_follow_actual_resolution_queue(self):
        for name in ['Augur of Bolas','Satyr Wayfinder','Oath of Nissa']:
            for owner in (0,1):
                for mode in ['decline','predicate_match','predicate_no_match']:
                    s=self.cases(name,[f'{mode}_{owner}'])[0]['steps'];ops=[x['op'] for x in s]
                    self.assertLess(ops.index('cast'),ops.index('choose_optional_zone'))
                    self.assertLess(ops.index('choose_optional_zone'),ops.index('resolve'))
                    self.assertIn(dict(op='assert_forced_zone_choices',value=0),s[ops.index('resolve'):])
                    if mode.startswith('predicate'):
                        self.assertIn(dict(op='assert_offered_selection_required',value=0),s)
                        self.assertEqual(next(x['value'] for x in s if x['op']=='assert_offered_legal_card'),int(mode=='predicate_match'))

    def test_human_selected_card_reveal_follows_choice_before_remainder(self):
        for name in ['Satyr Wayfinder','Oath of Nissa']:
            s=self.cases(name,['effect_0'])[0]['steps'];ops=[x['op'] for x in s]
            selected=ops.index('choose_zone_card');resolve=ops.index('resolve')
            self.assertEqual(s[selected+1],dict(op='choose_dialog',player=0,value=0))
            self.assertLess(selected+1,resolve)
            self.assertEqual(sum(x['op']=='choose_dialog' for x in s),1)
            if name=='Oath of Nissa':
                self.assertEqual(s[selected+2]['op'],'choose_zone_card')
                self.assertLess(selected+2,resolve)
            else:self.assertEqual(s[selected+2]['op'],'resolve')

    def test_selected_reveal_has_no_ack_on_decline_empty_or_ai_default_paths(self):
        for name in ['Satyr Wayfinder','Oath of Nissa']:
            for owner in (0,1):
                for mode in ['decline','empty_library','predicate_match','predicate_no_match']:
                    self.assertFalse(any(x['op']=='choose_dialog' for x in self.cases(name,[f'{mode}_{owner}'])[0]['steps']))
            self.assertFalse(any(x['op']=='choose_dialog' for x in self.cases(name,['effect_1'])[0]['steps']))
            d=self.declaration(name);d['params']['presentation']['human_selected_reveal']=1
            with self.assertRaises(ValueError):dispatch.spec(d,self.oracle[name])
        for name,profile in selection.pins()['sources'].items():
            if profile['presentation'].get('human_selected_reveal'):continue
            for owner in (0,1):self.assertFalse(any(x['op']=='choose_dialog' for x in self.cases(name,[f'effect_{owner}'])[0]['steps']))

    def test_artifact_expansion_uses_real_positive_and_decline_fixtures(self):
        relic=self.rows['Darksteel Relic']['csvid']
        for name in ['Faerie Mechanist','Glint-Nest Crane']:
            p=self.parameters(name)
            self.assertEqual(p['scope'],'artifact')
            for owner in (0,1):
                c=self.cases(name,[f'all_match_{owner}'])[0];s=c['steps']
                window=[x['name'] for x in c['setup'] if x['player']==owner and x['zone']=='library'][:p['window']]
                self.assertEqual(window,['Darksteel Relic']*p['window'])
                self.assertIn(dict(op='assert_zone_multiplicity',player=owner,zone='hand',index=relic,value=1),s)
                self.assertIn(dict(op='assert_zone_multiplicity',player=1-owner,zone='hand',index=relic,value=0),s)
                decline=self.cases(name,[f'predicate_match_{owner}'])[0]['steps']
                self.assertIn(dict(op='choose_optional_zone',player=owner,value=relic),decline)
                self.assertIn(dict(op='assert_offered_legal_card',index=relic,value=1),decline)
                self.assertIn(dict(op='assert_zone_multiplicity',player=owner,zone='hand',index=relic,value=0),decline)

    def test_foul_emissary_creature_positive_reveal_and_emerge_gap_are_explicit(self):
        name='Foul Emissary';p=self.parameters(name)
        self.assertEqual(p['scope'],'creature')
        self.assertTrue(p['presentation']['human_selected_reveal'])
        clause='When you sacrifice this creature while casting a spell with emerge, create a 3/2 colorless Eldrazi Horror creature token.'
        self.assertIn(clause,p['gaps'])
        for owner in (0,1):
            c=self.cases(name,[f'effect_{owner}'])[0];s=c['steps'];chosen=next(i for i,x in enumerate(s) if x['op']=='choose_zone_card')
            self.assertEqual(s[chosen]['value'],self.rows['Grizzly Bears']['csvid'])
            self.assertIn(clause,c['_recipe']['untested_components'])
            if owner==0:self.assertEqual(s[chosen+1],dict(op='choose_dialog',player=0,value=0))
            else:self.assertFalse(any(x['op']=='choose_dialog' for x in s))
            decline=self.cases(name,[f'decline_{owner}'])[0]['steps']
            self.assertTrue(any(x['op']=='choose_optional_zone' for x in decline))
            self.assertFalse(any(x['op']=='choose_dialog' for x in decline))

    def test_mandatory_choice_with_one_card_moves_exactly_one(self):
        for name in ['Sea Gate Oracle','Tower Geist']:
            for owner in (0,1):
                c=self.cases(name,[f'one_card_library_{owner}'])[0];s=c['steps'];resolve=next(i for i,x in enumerate(s) if x['op']=='resolve')
                initial=sum(x['zone']=='hand' and x['player']==owner for x in c['setup'])
                self.assertIn(dict(op='assert_zone',player=owner,zone='hand',value=initial),s[resolve:])
                self.assertIn(dict(op='assert_zone',player=owner,zone='library',value=0),s[resolve:])
                self.assertFalse(any(x['op']=='choose_optional_zone' for x in s))

    def test_positive_hand_grave_and_bottom_receipts_follow_oracle_independent_examples(self):
        chosen={'Augur of Bolas':['Lightning Bolt'],'Satyr Wayfinder':['Forest'],'Sea Gate Oracle':['Forest'],
                'Tower Geist':['Forest'],'Goblin Ringleader':['Tarfire',"Mons's Goblin Raiders"],
                'Halimar Depths':[],'Architects of Will':[],'Oath of Nissa':['Grizzly Bears']}
        for name,picked in chosen.items():
            p=self.parameters(name)
            for owner in (0,1):
                c=self.cases(name,[f'effect_{owner}'])[0];s=c['steps']
                initial_hand=[x['name'] for x in c['setup'] if x['zone']=='hand' and x['player']==owner];initial_hand.remove(name)
                self.assertIn(dict(op='assert_zone',player=owner,zone='hand',value=len(initial_hand)+len(picked)),s)
                expected=Counter(initial_hand+picked)
                for nm in selection.PALETTE:
                    self.assertIn(dict(op='assert_zone_multiplicity',player=owner,zone='hand',index=self.rows[nm]['csvid'],value=expected[nm]),s)
                library=[x['name'] for x in c['setup'] if x['zone']=='library' and x['player']==owner]
                if p['kind']!='reorder':
                    prefix=library[:p['window']]
                    for nm in picked:prefix.remove(nm)
                    expected_library=library[p['window']:]+(prefix if p['remainder']=='bottom' else [])
                    for i,nm in enumerate(expected_library):self.assertIn(dict(op='assert_zone_card',player=owner,zone='library',index=i,value=self.rows[nm]['csvid']),s)

    def test_architects_target_and_order_are_chosen_by_actual_source_controller(self):
        for owner in (0,1):
            c=self.cases('Architects of Will',[f'opponent_target_{owner}'])[0];s=c['steps'];ops=[x['op'] for x in s]
            target=next(x for x in s if x['op']=='choose_player')
            self.assertEqual(target,dict(op='choose_player',player=owner,value=1-owner))
            self.assertLess(ops.index('cast'),ops.index('choose_player'));self.assertLess(ops.index('choose_player'),ops.index('resolve'))
            if owner==0:
                library=[x['name'] for x in c['setup'] if x['player']==1-owner and x['zone']=='library']
                for i,nm in enumerate(reversed(library[:3])):
                    self.assertIn(dict(op='assert_zone_card',player=1-owner,zone='library',index=i,value=self.rows[nm]['csvid']),s)
                self.assertTrue(all(x['player']==owner for x in s if x['op']=='choose_zone_card'))

    def test_ai_reorder_proves_prefix_permutation_without_scry_bottom_permissions(self):
        for name in ['Architects of Will','Halimar Depths']:
            c=self.cases(name,['effect_1'])[0];s=c['steps'];p=self.parameters(name)
            library=[x['name'] for x in c['setup'] if x['player']==1 and x['zone']=='library']
            self.assertFalse(any(x['op']=='assert_scry_result' for x in s))
            self.assertFalse(any(x['op']=='assert_zone_card' and x['player']==1 and x['index']<p['window'] for x in s))
            for i,nm in enumerate(library[p['window']:],p['window']):
                self.assertIn(dict(op='assert_zone_card',player=1,zone='library',index=i,value=self.rows[nm]['csvid']),s)
            for nm,count in Counter(library).items():
                self.assertIn(dict(op='assert_zone_multiplicity',player=1,zone='library',index=self.rows[nm]['csvid'],value=count),s)

    def test_nonmatching_entry_pays_initial_entry_before_casting_control(self):
        for name in selection.pins()['sources']:
            for owner in (0,1):
                s=self.cases(name,[f'nonmatching_entry_{owner}'])[0]['steps']
                casts=[(i,x['ref']) for i,x in enumerate(s) if x['op']=='cast']
                self.assertEqual([x[1] for x in casts],['source','nonmatching'])
                self.assertTrue(any(x['op']=='resolve' for x in s[casts[0][0]+1:casts[1][0]]))
                self.assertFalse(any(x['op'].startswith('choose_') for x in s[casts[1][0]+1:]))

    def test_snapshot_replays_repeat_equal_choice_and_receipt_bodies(self):
        for name in selection.pins()['sources']:
            for owner in (0,1):
                for mode in ['ready_snapshot','pending_snapshot','resolved_snapshot']:
                    s=self.cases(name,[f'{mode}_{owner}'])[0]['steps'];ops=[x['op'] for x in s]
                    capture=ops.index('capture');record=ops.index('record_state');restore=ops.index('restore');end=ops.index('assert_recorded_state')
                    self.assertEqual(s[capture+1:record],s[restore+1:end])
                    if mode!='resolved_snapshot':self.assertIsNot(s[capture+1],s[restore+1])

    def test_full_witness_and_implementation_changes_are_rejected(self):
        selection.validate_witnesses(self.oracle,self.rows)
        rows=copy.deepcopy(self.rows);rows['Halimar Depths']['handler']='card_dummy'
        with self.assertRaises(ValueError):selection.validate_witnesses(self.oracle,rows)
        oracle=copy.deepcopy(self.oracle);oracle['Tarfire']['type_line']='Instant'
        with self.assertRaises(ValueError):selection.validate_witnesses(oracle,self.rows)


if __name__=='__main__':unittest.main()
