"""Focused guards for paid tapped modifiers, native untap, and linked duration."""

import json
from pathlib import Path
import unittest

from generate_cases import registry
import tapped_duration_artifacts_recipes as model


class TappedDurationRecipes(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        data = Path(__file__).parent / 'data'
        cls.oracle = json.loads((data / 'oracle.json').read_text(encoding='utf8'))['cards']
        cls.rows = {row['name']: row for row in registry()}
        cls.homes = {}
        for name,pin in model.pins()['sources'].items():
            csvid = pin['registry']['csvid']
            path, = (data / 'cases/cards').glob(f'*/{csvid:05d}-*.json')
            cls.homes[name] = json.loads(path.read_text(encoding='utf8'))['recipes'][0]

    def cases(self,name,*selected):
        declaration = self.homes[name]
        shape = model.spec(declaration,self.oracle[name])
        return model.expand(name,self.rows[name]['csvid'],declaration,shape,selected,self.rows,self.oracle)

    def steps(self,name,variant):
        return self.cases(name,variant)[0]['steps']

    def values(self,steps,op,**fields):
        return [s['value'] for s in steps if s['op']==op and all(s.get(k)==v for k,v in fields.items())]

    def prop(self,steps,ref,field):
        return self.values(steps,'assert_card',ref=ref,property=field)

    def test_independent_whole_oracle_and_exact_range(self):
        model.validate_witnesses(self.oracle,self.rows)
        widths = {
            'Spirit Shield':(3,2,0,2),
            "Tawnos's Weaponry":(2,2,1,1),
            'Zelyon Sword':(3,3,2,0),
        }
        cases = []
        for name,declaration in self.homes.items():
            shape = model.spec(declaration,self.oracle[name])
            self.assertEqual(declaration['variants'],model.variants(shape))
            self.assertEqual((shape['cast_mana'][0],shape['activation_generic'],
                              shape['power_delta'],shape['toughness_delta']),widths[name])
            for case in self.cases(name,*declaration['variants']):
                self.assertEqual(case['_recipe']['covered_clauses'],self.oracle[name]['oracle_text'].splitlines())
                cases.append(case['name'])
        self.assertEqual(len(set(cases)),150)
        name = 'Spirit Shield'
        d,r = self.homes[name],self.oracle[name]
        for change in (dict(version=True),dict(variants=['untap_choice_1']),
                       dict(variants=['combat_0','combat_0']),dict(params=dict(d['params'],activation_generic=3))):
            with self.subTest(change=change), self.assertRaises(ValueError):
                model.spec(dict(d,**change),r)
        with self.assertRaises(ValueError):
            model.parameters(dict(r,oracle_text=r['oracle_text'].replace('remains tapped','remains in play')))

    def test_actual_payment_and_timing(self):
        for name,declaration in self.homes.items():
            steps = self.steps(name,'activate_0')
            clear = [s for s in steps if s['op']=='set_mana'][:12]
            self.assertEqual(clear,[dict(op='set_mana',player=p,color=c,value=0) for p in (0,1) for c in range(6)])
            cast = steps.index(dict(op='cast',ref='source',value=1))
            act = steps.index(dict(op='activate',ref='source',value=1))
            resolve = steps.index(dict(op='resolve'),act)
            shape = declaration['params']
            self.assertIn(dict(op='set_mana',player=0,color=0,value=shape['cast_mana'][0]),steps[:cast])
            self.assertIn(dict(op='set_mana',player=0,color=0,value=shape['activation_generic']),steps[cast:act])
            self.assertIn(dict(op='assert_stack',value=1),steps[act:resolve])
            self.assertIn(dict(op='assert_mana',player=0,color=0,value=0),steps[act:resolve])
            self.assertEqual(self.prop(steps[act:resolve],'source','tapped'),[1])
            self.assertEqual(self.prop(steps[act:resolve],'ally','toughness'),[2])
            self.assertEqual(self.prop(steps[resolve:],'ally','toughness'),[2+shape['toughness_delta']])

    def test_denied_without_unoffered_target_or_consumed_price(self):
        n = 'Zelyon Sword'
        short,empty,cancel,tapped = self.cases(n,'fee_short_1','no_creature_1','cancel_1','initial_tapped_1')
        for case,left in ((short,2),(empty,3),(cancel,3),(tapped,3)):
            steps = case['steps']
            self.assertIn(dict(op='activate',ref='source',value=0),steps)
            self.assertEqual(self.values(steps,'assert_mana',player=1,color=0)[-1],left)
            self.assertNotIn(dict(op='choose_target',player=1,ref='ally'),steps)
        self.assertIn(dict(op='choose_cancel_target',player=1),cancel['steps'])
        self.assertIn(dict(op='choose_dialog',player=0,value=0),tapped['steps'])
        self.assertEqual(self.prop(tapped['steps'],'source','tapped')[-1],1)

    def test_target_may_be_opposing_and_activation_offturn(self):
        name = "Tawnos's Weaponry"
        steps = self.steps(name,'enemy_target_1')
        self.assertIn(dict(op='choose_target',player=1,ref='enemy'),steps)
        self.assertEqual(self.prop(steps,'enemy','power')[-1],5)
        self.assertEqual(self.prop(steps,'ally','power')[-1],1)
        steps = self.steps(name,'offturn_1')
        self.assertLess(steps.index(dict(op='advance_turn')),steps.index(dict(op='activate',ref='source',value=1)))

    def test_real_optional_and_mandatory_untap_paths(self):
        name = 'Spirit Shield'
        keep,accept,ai = self.cases(name,'keep_tapped_0','untap_choice_0','keep_tapped_1')
        self.assertIn(dict(op='choose_priority',player=0,value=-2),keep['steps'])
        self.assertIn(dict(op='choose_priority',ref='source'),accept['steps'])
        self.assertNotIn('choose_priority',{s['op'] for s in ai['steps']})
        for case,owner,tapped,bonus in ((keep,0,1,2),(accept,0,0,0),(ai,1,1,2)):
            steps = case['steps']
            at = steps.index(dict(op='untap_step',player=owner))
            self.assertEqual(self.prop(steps[at:],'ready_witness','tapped'),[0])
            self.assertEqual(self.prop(steps[at:],'source','tapped'),[tapped])
            self.assertEqual(self.prop(steps[at:],'ally','toughness'),[2+bonus])
        auto = self.steps(name,'untap_after_target_leaves_1')
        self.assertEqual(self.prop(auto,'source','tapped')[-1],0)
        self.assertNotIn('choose_priority',{s['op'] for s in auto})
        self.assertIn(dict(op='cast',ref='removal',value=1),auto)

    def test_opponent_untap_and_cleanup_do_not_end_duration(self):
        steps = self.steps('Spirit Shield','opponent_untap_0')
        cleanup = steps.index(dict(op='cleanup'))
        untap = steps.index(dict(op='untap_step',player=1))
        self.assertLess(cleanup,untap)
        self.assertEqual(self.prop(steps[cleanup:],'ally','toughness'),[4,4])
        self.assertEqual(self.prop(steps[untap:],'source','tapped'),[1])
        self.assertEqual(self.prop(steps[untap:],'opponent_witness','tapped'),[0])
        self.assertEqual(self.prop(steps[untap:],'ready_witness','tapped'),[1])

    def test_external_untap_retap_and_reactivation_track_new_target(self):
        name = 'Zelyon Sword'
        retap = self.steps(name,'retap_0')
        self.assertEqual(self.values(retap,'choose_dialog',player=1),[1,0])
        self.assertEqual(self.prop(retap,'source','tapped')[-3:],[1,0,1])
        self.assertEqual(self.prop(retap,'ally','power')[-3:],[3,1,1])
        reactivate = self.steps(name,'reactivate_0')
        self.assertEqual(self.values(reactivate,'activate',ref='source'),[1,1])
        self.assertEqual(self.prop(reactivate,'ally','power')[-1],1)
        self.assertEqual(self.prop(reactivate,'enemy','power')[-1],6)

    def test_pending_resolution_requires_live_tapped_source_and_live_target(self):
        for variant,answer in (('untap_pending_1','twiddle'),('source_leaves_pending_1','removal'),
                               ('target_leaves_pending_1','removal')):
            steps = self.steps("Tawnos's Weaponry",variant)
            b = steps.index(dict(op='activate',ref='source',value=1))
            a = steps.index(dict(op='cast',ref=answer,value=1))
            self.assertLess(b,a)
            self.assertIn(dict(op='assert_stack',value=2),steps[a:])
            self.assertNotIn(2,self.prop(steps[a:],'ally','power'))

    def test_live_source_and_target_departures_expire_link(self):
        name = "Tawnos's Weaponry"
        lost_source = self.steps(name,'source_leaves_1')
        self.assertEqual(self.prop(lost_source,'ally','power')[-2:],[2,1])
        self.assertIn(dict(op='assert_zone_multiplicity',player=1,zone='graveyard',index=533,value=1),lost_source)
        lost_target = self.steps(name,'target_bounce_1')
        self.assertIn(dict(op='bind_hand_copy',ref='ally',player=1,index=0,value=2493),lost_target)
        self.assertIn(dict(op='cast',ref='ally',value=1),lost_target)
        self.assertEqual(self.prop(lost_target,'ally','power')[-1],1)
        self.assertEqual(self.prop(lost_target,'source','tapped')[-1],1)

    def test_actual_combat_composition_distinguishes_power_and_toughness(self):
        for name,dead,owner in (('Spirit Shield',2493,0),("Tawnos's Weaponry",2493,0),('Zelyon Sword',67,1)):
            positive,control = self.cases(name,'combat_0','combat_no_boost_0')
            a = positive['steps']
            begin = a.index(dict(op='advance_combat'))
            activate = a.index(dict(op='activate',ref='source',value=1))
            damage = a.index(dict(op='combat_damage'))
            self.assertLess(begin,activate)
            self.assertLess(activate,damage)
            receipt = dict(op='assert_zone_multiplicity',player=owner,zone='graveyard',index=dead,value=1)
            self.assertEqual(receipt in a,name=='Zelyon Sword')
            self.assertEqual(receipt in control['steps'],name!='Zelyon Sword')

    def test_ready_pending_and_optional_choice_restore_exact_tail(self):
        for variant,starts in (('replay_0','can_activate'),('pending_replay_1','resolve'),
                               ('untap_choice_replay_0','choose_priority')):
            steps = self.steps('Spirit Shield',variant)
            at = steps.index(dict(op='capture'))
            observe = steps.index(dict(op='record_state'))
            restore = steps.index(dict(op='restore'))
            self.assertEqual(steps[at+1]['op'],starts)
            self.assertEqual(steps[at+1:observe],steps[restore+1:-1])
            self.assertEqual(steps[-1],dict(op='assert_recorded_state'))
