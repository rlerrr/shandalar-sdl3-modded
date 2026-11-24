"""Focused guards for kicker choices, additional payment, targets and replay."""
import json
from pathlib import Path
import unittest

from generate_cases import registry
import kicker_targeted_utility_recipes as model


class TargetedUtilityKickerTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        data=Path(__file__).parent/'data'
        cls.oracle=json.loads((data/'oracle.json').read_bytes())['cards']
        cls.rows={row['name']:row for row in registry()}
        cls.homes={}
        for name,pin in model.profiles()['sources'].items():
            path,=(data/'cases/cards').glob(f"*/{pin['registry']['csvid']:05d}-*.json")
            cls.homes[name]=json.loads(path.read_bytes())['recipes'][0]

    def case(self,name,variant):
        home=self.homes[name]
        return model.build(name,self.rows[name]['csvid'],home,model.spec(home,self.oracle[name]),variant,self.rows,self.oracle)

    def test_whole_sources_and_independent_witnesses(self):
        model.validate_witnesses(self.oracle,self.rows)
        for name,home in self.homes.items():
            record=self.oracle[name]
            shape=model.spec(home,record)
            self.assertEqual(home['variants'],model.variants(shape))
            with self.assertRaises(ValueError):
                model.spec(home,dict(record,oracle_text=record['oracle_text']+'\nFlying'))

    def test_spell_target_before_kicker_and_entry_targets_after(self):
        for name in ('Into the Roil','Blink of an Eye','Torch Slinger'):
            actions=[s['op'] for s in self.case(name,'paid_1')['steps']]
            trio=[op for op in actions if op in ('choose_target','choose_dialog','cast')]
            self.assertEqual(trio,['choose_target','choose_dialog','cast'] if name!='Torch Slinger' else
                             ['choose_dialog','cast','choose_target'])
        for mode in ('base_only','short_kicker'):
            self.assertFalse(any(x['op']=='choose_dialog' for x in self.case('Into the Roil',mode+'_0')['steps']))

    def test_tempest_owl_and_replay(self):
        for mode,count in [('zero_targets',0),('one_target',1),('two_targets',2),('paid',3),('declined',0)]:
            steps=self.case('Tempest Owl',mode+'_1')['steps']
            actual=[s['ref'] for s in steps if s['op']=='choose_target']
            self.assertEqual(len(actual),count)
            self.assertEqual(len(actual),len(set(actual)))
            self.assertEqual(sum(s['op']=='choose_optional_target' for s in steps),int('targets' in mode or mode=='one_target'))
        replay=self.case('Tempest Owl','replay_1')['steps']
        self.assertEqual(sum(s['op']=='choose_dialog' for s in replay),1)
        self.assertEqual(sum(s['op']=='choose_target' for s in replay),6)
        self.assertTrue(any(s['op']=='assert_recorded_state' for s in replay))

    def test_countered_bounce_and_target_loss_keep_draw_out(self):
        for name in ('Into the Roil','Blink of an Eye'):
            for mode,count in [('paid',1),('declined',0),('countered_paid',0),('lost_target',0)]:
                steps=self.case(name,mode+'_1')['steps']
                island=self.rows['Island']['csvid']
                self.assertIn(dict(op='assert_zone_multiplicity',player=1,zone='hand',index=island,value=count),steps)
                self.assertEqual(sum(s['op']=='cast_response' for s in steps),int(mode in ('countered_paid','lost_target')))
            denial=self.case(name,'land_only_1')['steps']
            self.assertIn(dict(op='require_can_cast',ref='source',value=0),denial)
            self.assertFalse(any(s['op'] in ('choose_target','choose_dialog') for s in denial))


if __name__=='__main__':
    unittest.main()
