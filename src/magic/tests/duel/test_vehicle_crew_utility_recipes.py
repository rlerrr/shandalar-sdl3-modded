"""Guard the paid Crew and independent Vehicle rider boundaries."""

import json
from pathlib import Path
import unittest

import generate_cases
import vehicle_crew_utility_recipes as model


CARAVAN = "Cultivator's Caravan"
STOMPER = 'Demolition Stomper'
DRAGSTER = 'Daredevil Dragster'
COLOSSUS = 'Peacewalker Colossus'


class VehicleUtilityTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.oracle = json.loads((Path(generate_cases.__file__).parent/'data/oracle.json').read_bytes())['cards']
        cls.rows = {r['name']:r for r in generate_cases.registry() if r['kind']!='internal'}
        cls.homes = {}
        for name,pin in model.pins()['sources'].items():
            path, = (model.PROFILE.parent/'cases/cards').glob(f'*/{pin["registry"]["csvid"]:05d}-*.json')
            cls.homes[name] = json.loads(path.read_bytes())['recipes'][0]

    def case(self,name,variant):
        home = self.homes[name]
        return model.expand(name,self.rows[name]['csvid'],home,model.spec(home,self.oracle[name]),[variant],self.rows,self.oracle)[0]

    def test_oracle_exact_and_effect_specific(self):
        model.validate_witnesses(self.oracle,self.rows)
        self.assertEqual(len(self.homes),4)
        for name,home in self.homes.items():
            record = self.oracle[name]
            self.assertEqual(home['oracle_text'],record['oracle_text'])
            self.assertEqual(home['variants'],model.variants(model.spec(home,record)))
            with self.assertRaises(ValueError):
                model.spec(dict(home,params=dict(home['params'],threshold=True)),record)
            with self.assertRaises(ValueError):
                model.spec(home,dict(record,oracle_text=record['oracle_text']+'\nHaste'))
            with self.assertRaises(ValueError):
                model.spec(dict(home,variants=['cast_0','cast_0']),record)

    def test_actual_crew_uses_in_play_cost_creatures(self):
        for name,home in self.homes.items():
            for actor in (0,1):
                case = self.case(name,f'ordered_exact_{actor}')
                steps = case['steps']
                chosen = [s['ref'] for s in steps if s['op']=='choose_target']
                setup = {s['ref']:s for s in case['setup']}
                total = sum(int(self.oracle[setup[ref]['name']]['power']) for ref in chosen)
                self.assertEqual(total,home['params']['threshold'])
                self.assertTrue(all(setup[ref]['player']==actor and setup[ref]['zone']=='battlefield' for ref in chosen))
                paid = steps.index(dict(op='activate',ref='source',value=1))
                resolve = steps.index(dict(op='resolve'))
                for ref in chosen:
                    self.assertIn(dict(op='assert_card',ref=ref,property='tapped',value=1),steps[paid:resolve])
                self.assertIn(dict(op='assert_stack',value=1),steps[paid:resolve])
                self.assertIn(dict(op='can_target_creature',ref='type_probe',other='source',value=1),steps[resolve:])

    def test_caravan_real_palette_and_blue_spending(self):
        for i,mode in enumerate(('mana_black','mana_blue','mana_green','mana_red','mana_white'),1):
            steps = self.case(CARAVAN,mode+'_0')['steps']
            at = steps.index(dict(op='activate',ref='source',value=1))
            self.assertEqual(steps[at-2:at],[dict(op='choose_dialog',player=0,value=0),dict(op='choose_color',player=0,value=i)])
            self.assertIn(dict(op='assert_stack',value=0),steps[at:])
            self.assertIn(dict(op='assert_card',ref='source',property='tapped',value=1),steps[at:])
            self.assertIn(dict(op='assert_mana',player=0,color=i,value=1),steps[at:])
        steps = self.case(CARAVAN,'spend_blue_1')['steps']
        activate = steps.index(dict(op='activate',ref='source',value=1))
        cast = steps.index(dict(op='cast',ref='spend',value=1))
        self.assertLess(activate,cast)
        self.assertNotIn('set_mana',[s['op'] for s in steps[activate:cast]])
        self.assertIn(dict(op='assert_mana',player=1,color=2,value=0),steps[cast:])

    def test_fresh_animated_and_unanimated_caravan_have_different_tap_legality(self):
        normal = self.case(CARAVAN,'fresh_mana_0')['steps']
        self.assertIn(dict(op='cast',ref='source',value=1),normal)
        self.assertIn(dict(op='activate',ref='source',value=1),normal)
        animated = self.case(CARAVAN,'fresh_crewed_denied_0')['steps']
        effects = [(s['op'],s.get('value')) for s in animated if s['op'] in ('cast','activate','assert_unused_choices')]
        self.assertEqual(effects,[('cast',1),('activate',1),('activate',0),('assert_unused_choices',1)])
        self.assertIn(dict(op='assert_card',ref='later_0',property='tapped',value=0),animated)

    def test_stomper_uses_current_blocking_power(self):
        for mode,legal,life in [('power_one',0,10),('power_two',0,10),('power_three',1,20),
                                ('power_rises',1,20),('power_falls',0,10)]:
            steps = self.case(STOMPER,mode+'_1')['steps']
            at = steps.index(dict(op='begin_blockers'))
            self.assertEqual(steps[at+1],dict(op='block',ref='blocker',other='source',value=legal))
            self.assertIn(dict(op='assert_life',player=0,value=life),steps[at:])
            if mode in ('power_rises','power_falls'):
                self.assertIn(dict(op='cast',ref='change',value=1),steps[:at])

    def test_velocity_can_be_earned_by_either_combat_role_only_at_end(self):
        for mode,expected in [('attack_once',1),('block_once',1),('no_participation',0)]:
            steps = self.case(DRAGSTER,mode+'_0')['steps']
            at = steps.index(dict(op='end_combat'))
            k = dict(op='assert_counter',ref='source',counter_kind='velocity',value=0)
            self.assertTrue(all(s==k for s in steps[:at] if s['op']=='assert_counter'))
            self.assertIn(dict(k,value=expected),steps[at:])
            self.assertLess(steps.index(dict(op='combat_damage')),at)
        for mode in ('attack_twice','attack_then_block'):
            steps = self.case(DRAGSTER,mode+'_1')['steps']
            ends = [i for i,s in enumerate(steps) if s['op']=='end_combat']
            self.assertEqual(len(ends),2)
            self.assertIn(dict(op='assert_counter',ref='source',counter_kind='velocity',value=1),steps[ends[0]:ends[1]])
            self.assertIn(dict(op='assert_zone_multiplicity',player=1,zone='graveyard',index=self.rows[DRAGSTER]['csvid'],value=1),steps[ends[1]:])
            self.assertIn(dict(op='assert_zone',player=1,zone='hand',value=2),steps[ends[1]:])
            self.assertIn(dict(op='assert_zone',player=1,zone='library',value=1),steps[ends[1]:])
            if mode=='attack_then_block':
                block = steps.index(dict(op='block',ref='source',other='attacker',value=1))
                cast = steps.index(dict(op='cast',ref='untap',value=1))
                self.assertLess(ends[0],cast)
                self.assertLess(cast,block)
                self.assertIn(dict(op='assert_card',ref='source',property='tapped',value=0),steps[cast:block])
                self.assertIn(dict(op='assert_zone_multiplicity',player=1,zone='graveyard',index=self.rows['Twiddle']['csvid'],value=1),steps[cast:block])

    def test_colossus_costs_and_scope_are_separate_from_crew(self):
        steps = self.case(COLOSSUS,'both_other_0')['steps']
        at = steps.index(dict(op='activate',ref='source',value=1))
        resolve = steps.index(dict(op='resolve'))
        self.assertEqual(steps[at-2:at],[dict(op='choose_dialog',player=0,value=0),dict(op='choose_target',player=0,ref='recipient')])
        self.assertIn(dict(op='assert_stack',value=1),steps[at:resolve])
        for color in (0,5):
            self.assertIn(dict(op='assert_mana',player=0,color=color,value=1),steps[:at])
            self.assertIn(dict(op='assert_mana',player=0,color=color,value=0),steps[at:resolve])
        self.assertIn(dict(op='can_target_creature',ref='type_probe',other='recipient',value=0),steps[at:resolve])
        self.assertIn(dict(op='can_target_creature',ref='type_probe',other='recipient',value=1),steps[resolve:])
        for mode in model.DENIED_PAID:
            denied = self.case(COLOSSUS,mode+'_0')['steps']
            self.assertIn(dict(op='activate',ref='source',value=0),denied)
            self.assertFalse(any(s['op']=='choose_target' for s in denied))

    def test_colossus_real_response_distinguishes_source_from_target_loss(self):
        for mode,ref in [('other_target_lost','recipient'),('other_source_lost','source')]:
            steps = self.case(COLOSSUS,mode+'_1')['steps']
            response = steps.index(dict(op='cast',ref='bounce',value=1))
            self.assertEqual(steps[response-1],dict(op='choose_target',ref=ref,player=0))
            self.assertIn(dict(op='assert_stack',value=2),steps[response:])
            self.assertIn(dict(op='assert_zone',zone='hand',player=1,value=1),steps[response:])
            if mode=='other_source_lost':
                self.assertIn(dict(op='can_target_creature',ref='type_probe',other='recipient',value=1),steps[response:])

    def test_replay_repeats_real_actions(self):
        for name,variant in [(STOMPER,'block_replay_0'),(DRAGSTER,'attack_replay_1'),(COLOSSUS,'other_replay_0')]:
            steps = self.case(name,variant)['steps']
            self.assertEqual(steps[steps.index(dict(op='capture'))+1:steps.index(dict(op='record_state'))],
                             steps[steps.index(dict(op='restore'))+1:-1])


if __name__=='__main__':
    unittest.main()
