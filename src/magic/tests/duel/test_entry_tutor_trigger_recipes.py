"""Whole-source, hidden search, quantity, conservation and real-entry guards."""
import copy
import json
import unittest
from pathlib import Path

from generate_cases import OPS, PROPERTIES, registry
import entry_tutor_trigger_recipes as tutor


class EntryTutorTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.oracle=json.loads((Path(__file__).parent/'data/oracle.json').read_text(encoding='utf8'))['cards']
        cls.rows={r['name']:r for r in registry()}

    def declaration(self,name):
        p=tutor.pins()['sources'][name]
        return dict(family=tutor.FAMILY,version=2,oracle_text=self.oracle[name]['oracle_text'],variants=['search_0'],
                    params=dict(schema=1,mechanism=copy.deepcopy(p['mechanism']),presentation=copy.deepcopy(p['presentation'])))

    def parameters(self,name):return tutor.spec(self.declaration(name),self.oracle[name])

    def case(self,name,mode='search',owner=0):
        d=self.declaration(name);p=tutor.spec(d,self.oracle[name]);d['variants']=[f'{mode}_{owner}']
        return tutor.expand(name,self.rows[name]['csvid'],d,p,d['variants'],self.rows,self.oracle)[0]

    def test_all_six_expand_291_valid_whole_oracle_cases(self):
        total=0
        for name in tutor.pins()['sources']:
            d=self.declaration(name);p=tutor.spec(d,self.oracle[name]);d['variants']=tutor.variants(p)
            cases=tutor.expand(name,self.rows[name]['csvid'],d,p,d['variants'],self.rows,self.oracle);total+=len(cases)
            self.assertEqual({int(c['_recipe']['variant'][-1]) for c in cases},{0,1})
            for c in cases:
                self.assertEqual(c['_recipe']['oracle_text'],self.oracle[name]['oracle_text'])
                for line in p['gaps']:self.assertIn(line,c['_recipe']['untested_components'])
                for s in c['steps']:
                    self.assertIn(s['op'],OPS)
                    if 'property' in s:self.assertIn(s['property'],PROPERTIES)
        self.assertEqual(total,291)

    def test_full_identity_type_cost_and_unmodeled_clauses_are_pinned(self):
        for name in tutor.pins()['sources']:
            d=self.declaration(name)
            for key,value in [('oracle_text',self.oracle[name]['oracle_text']+'\nDraw a card.'),('type_line','Sorcery'),('mana_cost','{0}'),('layout','split'),('power','99')]:
                r=dict(self.oracle[name],**{key:value})
                with self.assertRaises(ValueError):tutor.spec(dict(d,oracle_text=r['oracle_text']),r)
            self.assertTrue(self.parameters(name)['creature'])
        self.assertEqual(self.parameters('Grozoth')['mana'],[6,0,3,0,0,0])
        self.assertEqual(len(self.parameters('Grozoth')['gaps']),3)

    def test_recipe_mechanism_and_presentation_cannot_drift(self):
        for change in [lambda d:d['params']['mechanism'].__setitem__('quantity',True),
                       lambda d:d['params']['presentation'].__setitem__('human_optional_trigger',1),
                       lambda d:d['params'].__setitem__('schema',True)]:
            d=self.declaration('Howling Wolf');change(d)
            with self.assertRaises(ValueError):tutor.spec(d,self.oracle['Howling Wolf'])

    def test_name_filter_is_exact_not_generic_creature(self):
        p=self.parameters('Howling Wolf')
        self.assertTrue(tutor.matches(self.oracle['Howling Wolf'],p))
        for n in ['Nesting Wurm','Squadron Hawk','Hill Giant','Forest']:
            self.assertFalse(tutor.matches(self.oracle[n],p))

    def test_ranger_has_real_multitype_positives_and_noncreature_boundaries(self):
        p=self.parameters('Ranger of Eos')
        for n in ['Dryad Arbor','Signal Pest','Champion of the Parish']:self.assertTrue(tutor.matches(self.oracle[n],p))
        for n in ['Tarfire','Darksteel Relic','Elvish Visionary','Forest']:self.assertFalse(tutor.matches(self.oracle[n],p))
        self.assertIn('Land Creature',self.oracle['Dryad Arbor']['type_line'])
        self.assertIn('Artifact Creature',self.oracle['Signal Pest']['type_line'])

    def test_grozoth_card_wording_includes_noncreature_nine_and_excludes_eight_ten(self):
        p=self.parameters('Grozoth')
        for n in ['Grozoth','Artisan of Kozilek','Blasphemous Act']:self.assertTrue(tutor.matches(self.oracle[n],p))
        for n in ['Griselbrand','Jin-Gitaxias, Core Augur','Forest']:self.assertFalse(tutor.matches(self.oracle[n],p))
        self.assertEqual(self.oracle['Blasphemous Act']['type_line'],'Sorcery')

    def test_actual_optional_trigger_choice_precedes_zone_selection_and_resolve(self):
        s=self.case('Howling Wolf')['steps'];cast=next(i for i,x in enumerate(s) if x['op']=='cast');trigger=next(i for i,x in enumerate(s) if x['op']=='choose_trigger')
        zone=next(i for i,x in enumerate(s) if x['op']=='choose_zone_card');resolve=next(i for i,x in enumerate(s) if x['op']=='resolve')
        self.assertLess(cast,trigger);self.assertLess(trigger,zone);self.assertLess(zone,resolve)
        self.assertFalse(any(x['op']=='choose_trigger' for x in self.case('Howling Wolf',owner=1)['steps']))
        self.assertFalse(any(x['op']=='choose_trigger' for x in self.case('Grozoth')['steps']))

    def test_whole_decline_zero_shuffle_is_not_accepted_hidden_fail_one_shuffle(self):
        declined=self.case('Howling Wolf','whole_decline')['steps'];accepted=self.case('Howling Wolf','accepted_zero')['steps']
        self.assertFalse(any(x['op'].startswith('choose_') for x in declined))
        self.assertIn(dict(op='assert_shuffle_count',player=0,value=0),declined)
        self.assertIn(dict(op='choose_trigger',player=0,ref='source'),accepted)
        self.assertEqual(sum(x['op']=='choose_optional_zone' for x in accepted),1)
        self.assertIn(dict(op='assert_shuffle_count',player=0,value=1),accepted)
        self.assertIn(dict(op='assert_zone',player=0,zone='hand',value=2),accepted)

    def test_quantity_caps_and_early_stop_use_actual_optional_zone(self):
        for name,cap in [('Howling Wolf',3),('Ranger of Eos',2),('Grozoth',3)]:
            for mode,n in [('search',cap),('one_selected',1),('two_selected',2),('accepted_zero',0)]:
                s=self.case(name,mode)['steps']
                self.assertEqual(sum(x['op']=='choose_zone_card' for x in s),n)
                self.assertEqual(sum(x['op']=='choose_optional_zone' for x in s),int(n<cap or name=='Grozoth'))
                self.assertIn(dict(op='assert_zone',player=0,zone='hand',value=2+n),s)
                self.assertIn(dict(op='assert_shuffle_count',player=0,value=1),s)

    def test_accepted_empty_library_and_grozoth_unoffered_intents_are_distinct(self):
        for owner in (0,1):
            s=self.case('Howling Wolf','empty_library',owner)['steps']
            self.assertFalse(any(x['op'] in ('choose_zone_card','choose_optional_zone') for x in s))
            self.assertIn(dict(op='assert_shuffle_count',player=owner,value=1),s)
        modes=tutor.variants(self.parameters('Grozoth'))
        self.assertFalse(any('whole_decline' in x or 'empty_library' in x for x in modes))
        pending=tutor.pins()['pending_controls'];self.assertEqual(len(pending),4)
        self.assertEqual({p['status'] for p in pending},{'unexecuted_noncoverage'})
        self.assertEqual({p['expected']['own_shuffle_count'] for p in pending},{0,1})
        for p in pending:self.assertEqual(p['oracle'],self.oracle['Grozoth'])

    def test_search_observer_checks_full_offered_oracle_predicate_without_forcing(self):
        for name in tutor.pins()['sources']:
            s=self.case(name,'accepted_zero')['steps']
            self.assertEqual(s[0],dict(op='observe_search_filters',value=1))
            self.assertIn(dict(op='assert_offered_selection_required',value=0),s)
            legal={x['value'] for x in s if x['op']=='assert_offered_legal_card'}
            self.assertEqual(legal,{0,1})
            self.assertIn(dict(op='assert_forced_zone_choices',value=0),s)

    def test_wrong_zones_and_other_controller_library_cannot_satisfy_search(self):
        for owner in (0,1):
            c=self.case('Ranger of Eos','none_match',owner);s=c['steps'];positive=self.rows['Champion of the Parish']['csvid']
            self.assertIn(dict(op='assert_zone_multiplicity',player=owner,zone='exile',index=positive,value=1),s)
            self.assertIn(dict(op='assert_zone_multiplicity',player=1-owner,zone='library',index=positive,value=1),s)
            self.assertFalse(any(x['op']=='choose_zone_card' for x in s))
            self.assertIn(dict(op='assert_shuffle_count',player=1-owner,value=0),s)

    def test_post_shuffle_conservation_retains_multiplicity_without_inventing_order(self):
        s=self.case('Howling Wolf')['steps'];r=next(i for i,x in enumerate(s) if x['op']=='resolve')
        self.assertFalse(any(x['op']=='assert_zone_card' and x['zone']=='library' and x['player']==0 for x in s[r:]))
        self.assertTrue(any(x['op']=='assert_zone_card' and x['zone']=='library' and x['player']==1 for x in s[r:]))
        self.assertIn(dict(op='assert_zone_multiplicity',player=0,zone='hand',index=4497,value=3),s)
        self.assertIn(dict(op='assert_zone_multiplicity',player=0,zone='library',index=4497,value=2),s)

    def test_real_reentry_searches_again_and_other_entry_does_not(self):
        for owner in (0,1):
            for mode,ref in [('bounce_recast','bouncer'),('blink_source','blinker')]:
                s=self.case('Ranger of Eos',mode,owner)['steps']
                self.assertTrue(any(x['op']=='activate' and x['ref']==ref for x in s))
                self.assertIn(dict(op='assert_shuffle_count',player=owner,value=2),s)
            s=self.case('Ranger of Eos','nonmatching_entry',owner)['steps']
            self.assertIn(dict(op='cast',ref='nonmatching',value=1),s)
            self.assertNotIn(dict(op='assert_shuffle_count',player=owner,value=2),s)

    def test_snapshot_replay_restores_search_count_and_queue(self):
        for mode in ['ready_snapshot','pending_snapshot','resolved_snapshot']:
            s=self.case('Ranger of Eos',mode)['steps'];capture=next(i for i,x in enumerate(s) if x['op']=='capture');restore=next(i for i,x in enumerate(s) if x['op']=='restore')
            self.assertLess(capture,restore)
            self.assertNotIn(dict(op='assert_shuffle_count',player=0,value=2),s)
            self.assertIn(dict(op='assert_shuffle_count',player=0,value=1),s[restore:])

    def test_failed_cost_countering_and_source_destruction_keep_real_lifecycle(self):
        for owner in (0,1):
            for mode in ['insufficient_mana','wrong_color','countered']:
                s=self.case('Ranger of Eos',mode,owner)['steps']
                self.assertFalse(any(x['op'] in ('choose_trigger','choose_zone_card','choose_optional_zone') for x in s))
                self.assertIn(dict(op='assert_shuffle_count',player=owner,value=0),s)
            s=self.case('Grozoth','source_destroyed',owner)['steps']
            self.assertIn(dict(op='destroy',ref='source'),s)
            self.assertIn(dict(op='assert_zone_multiplicity',player=owner,zone='graveyard',index=7872,value=1),s)


if __name__=='__main__':unittest.main()
