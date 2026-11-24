"""Check actual cost choreography, Vehicle type layers and printed combat abilities."""

import json
from pathlib import Path
import unittest

import generate_cases
import vehicle_crew_combat_recipes as model


SCHOONER = 'Sleek Schooner'
CRUSHER = 'Irontread Crusher'
SKIFF = 'Sky Skiff'
EXPRESS = 'Aradara Express'
DREADNOUGHT = 'Dusk Legion Dreadnought'
DRAGSTER = 'Ovalchase Dragster'


class VehicleCrewCombatTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        data = model.PROFILE.parent
        oracle = Path(generate_cases.__file__).parent/'data/oracle.json'
        cls.oracle = json.loads(oracle.read_bytes())['cards']
        cls.rows = {r['name']:r for r in generate_cases.registry()}
        cls.homes = {}
        for name,pin in model.pins()['sources'].items():
            path, = (data/'cases/cards').glob(f'*/{pin["registry"]["csvid"]:05d}-*.json')
            cls.homes[name] = json.loads(path.read_bytes())['recipes'][0]

    def case(self,name,variant):
        home = self.homes[name]
        shape = model.spec(home,self.oracle[name])
        return model.expand(name,self.rows[name]['csvid'],home,shape,[variant],self.rows,self.oracle)[0]

    def steps(self,name,variant,op,**fields):
        return [s for s in self.case(name,variant)['steps'] if s['op']==op and all(s.get(k)==v for k,v in fields.items())]

    def test_pinned_whole_vehicle_and_typed_thresholds(self):
        model.validate_witnesses(self.oracle,self.rows)
        self.assertEqual(len(self.homes),6)
        for name,home in self.homes.items():
            record = self.oracle[name]
            self.assertEqual(home['oracle_text'],record['oracle_text'])
            self.assertEqual(home['variants'],model.variants(home['params'],name))
            with self.assertRaises(ValueError):
                model.spec(dict(home,variants=['cast_0','cast_0']),record)
            with self.assertRaises(ValueError):
                model.spec(dict(home,params=dict(home['params'],threshold=True)),record)
            with self.assertRaises(ValueError):
                model.spec(home,dict(record,oracle_text=record['oracle_text']+'\nHaste'))

    def test_actual_ordered_cost_is_paid_before_animation(self):
        for actor in (0,1):
            sequence = self.case(CRUSHER,f'ordered_exact_{actor}')['steps']
            at = next(i for i,s in enumerate(sequence) if s['op']=='activate')
            self.assertEqual([(s['ref'],s['player']) for s in sequence[at-2:at]],[('pilot_0',actor),('pilot_1',actor)])
            stop = next(i for i,s in enumerate(sequence[at:],at) if s['op']=='resolve')
            pending = sequence[at:stop]
            for ref,value in [('pilot_0',1),('pilot_1',1),('source',0)]:
                self.assertIn(dict(op='assert_card',ref=ref,property='tapped',value=value),pending)
            self.assertIn(dict(op='assert_stack',value=1),pending)
            self.assertIn(dict(op='can_target_creature',ref='type_probe',other='source',value=0),pending)
            self.assertIn(dict(op='can_target_creature',ref='type_probe',other='source',value=1),sequence[stop:])

    def test_short_or_canceled_crewing_has_no_cost(self):
        for name in self.homes:
            for actor in (0,1):
                for mode in model.DENIED:
                    v = f'{mode}_{actor}'
                    self.assertTrue(self.steps(name,v,'can_activate',value=0))
                    self.assertFalse(self.steps(name,v,'choose_target'))
                cancel = f'partial_cancel_{actor}'
                self.assertTrue(self.steps(name,cancel,'choose_cancel_target',player=actor))
                self.assertTrue(self.steps(name,cancel,'activate',value=0))
                self.assertFalse(self.steps(name,cancel,'assert_card',ref='pilot_0',property='tapped',value=1))

    def test_owner_tap_shroud_and_noncreature_use_actual_cost_selector(self):
        for actor in (0,1):
            v = f'selector_{actor}'
            for ref,value in [('ready_extra',1),('enemy_pilot',0),('tapped_extra',0),('noncreature',0),('shroud',1),('source',0)]:
                self.assertTrue(self.steps(CRUSHER,v,'probe_activation_target',other=ref,index=-1,value=value))
            self.assertFalse(self.steps(CRUSHER,v,'assert_card',ref='ready_extra',property='tapped',value=1))
            v = f'shroud_pilot_{actor}'
            self.assertTrue(self.steps(CRUSHER,v,'can_target_creature',other='pilot_0',value=0))
            self.assertTrue(self.steps(CRUSHER,v,'choose_target',ref='pilot_0',player=actor))
            self.assertTrue(self.steps(CRUSHER,v,'assert_card',ref='pilot_0',property='tapped',value=1))

    def test_fresh_pilot_can_pay_and_real_growth_changes_availability(self):
        for actor in (0,1):
            fresh = f'fresh_pilot_{actor}'
            self.assertTrue(self.steps(EXPRESS,fresh,'cast',ref='pilot_0',value=1))
            self.assertTrue(self.steps(EXPRESS,fresh,'assert_card',ref='pilot_0',property='sick',value=1))
            self.assertTrue(self.steps(EXPRESS,fresh,'activate',ref='source',value=1))
            growth = f'live_power_{actor}'
            self.assertEqual([s['value'] for s in self.steps(EXPRESS,growth,'can_activate')],[0,1])
            self.assertTrue(self.steps(EXPRESS,growth,'cast',ref='growth',value=1))
            self.assertTrue(self.steps(EXPRESS,growth,'assert_card',ref='pilot_0',property='power',value=4))

    def test_source_and_cost_creature_departure_and_cleanup(self):
        for actor in (0,1):
            for mode in ('source_leaves_pending','pilot_leaves_pending'):
                v = f'{mode}_{actor}'
                self.assertTrue(self.steps(CRUSHER,v,'assert_stack',value=2))
                self.assertTrue(self.steps(CRUSHER,v,'choose_target',ref='source' if mode=='source_leaves_pending' else 'pilot_0',player=1-actor))
                self.assertTrue(self.steps(CRUSHER,v,'assert_card',ref='pilot_1',property='tapped',value=1))
            v = f'expires_{actor}'
            self.assertTrue(self.steps(CRUSHER,v,'cleanup'))
            self.assertEqual([s['value'] for s in self.steps(CRUSHER,v,'can_target_creature',other='source')][-2:],[1,0])

    def test_crewing_again_needs_a_different_ready_creature(self):
        for actor in (0,1):
            once = f'recrew_self_only_{actor}'
            self.assertEqual([s['value'] for s in self.steps(CRUSHER,once,'can_activate')],[1,0])
            self.assertFalse(self.steps(CRUSHER,once,'choose_target',ref='source'))
            twice = f'recrew_other_{actor}'
            self.assertEqual([s['value'] for s in self.steps(CRUSHER,twice,'can_activate')],[1,1])
            self.assertTrue(self.steps(CRUSHER,twice,'choose_target',ref='later_0',player=actor))

    def test_printed_keywords_use_actual_attack_block_and_damage(self):
        for actor in (0,1):
            self.assertTrue(self.steps(SCHOONER,f'attack_combat_{actor}','block',ref='blocker',value=1))
            self.assertTrue(self.steps(SKIFF,f'attack_combat_{actor}','block',ref='blocker',value=0))
            for mode in ('flying_blocker','reach_blocker'):
                self.assertTrue(self.steps(SKIFF,f'{mode}_{actor}','block',ref='blocker',value=1))
            self.assertTrue(self.steps(EXPRESS,f'attack_combat_{actor}','block',ref='blocker',value=0))
            self.assertTrue(self.steps(EXPRESS,f'menace_pair_{actor}','block',ref='second_blocker',value=1))
            seq = self.case(DREADNOUGHT,f'attack_combat_{actor}')['steps']
            at = next(i for i,s in enumerate(seq) if s['op']=='finish_attackers')
            self.assertEqual(seq[at+1],dict(op='assert_card',ref='source',property='tapped',value=0))
            self.assertTrue(self.steps(DRAGSTER,f'fresh_vehicle_{actor}','attack',ref='source',value=1))
            self.assertTrue(self.steps(DRAGSTER,f'fresh_vehicle_{actor}','assert_life',player=1-actor,value=16))
            self.assertTrue(self.steps(CRUSHER,f'fresh_vehicle_{actor}','attack',ref='source',value=0))
            self.assertTrue(self.steps(CRUSHER,f'offturn_block_{actor}','block',ref='source',other='attacker',value=1))

    def test_additional_legal_crew_cost_has_an_ordinary_choice_receipt(self):
        for actor in (0,1):
            variant = f'overcrew_{actor}'
            case = self.case(SCHOONER,variant)
            seq = case['steps']
            at = next(i for i,s in enumerate(seq) if s['op']=='activate')
            self.assertEqual([(s['ref'],s['player']) for s in seq[at-2:at]],[('pilot_0',actor),('pilot_1',actor)])
            self.assertEqual(seq[at+1],dict(op='assert_unused_choices',value=0))
            stop = next(i for i,s in enumerate(seq[at:],at) if s['op']=='resolve')
            for ref in ('pilot_0','pilot_1'):
                self.assertIn(dict(op='assert_card',ref=ref,property='tapped',value=1),seq[at:stop])
            self.assertIn(dict(op='assert_stack',value=1),seq[at:stop])
            self.assertIn(dict(op='can_target_creature',ref='type_probe',other='source',value=0),seq[at:stop])
            self.assertIn(dict(op='can_target_creature',ref='type_probe',other='source',value=1),seq[stop:])
            self.assertEqual(case['_recipe']['untested_components'],['Multiplayer'])
        with self.assertRaises(ValueError):
            model.spec(dict(self.homes[SKIFF],variants=['overcrew_0']),self.oracle[SKIFF])

    def test_activation_and_pending_snapshots_reexecute_correct_boundaries(self):
        for mode,activation_count in [('activation_replay',2),('pending_replay',1)]:
            variant = f'{mode}_1'
            self.assertEqual(len(self.steps(CRUSHER,variant,'activate',ref='source')),activation_count)
            self.assertEqual(len(self.steps(CRUSHER,variant,'resolve')),2)
            self.assertEqual(self.case(CRUSHER,variant)['steps'][-1]['op'],'assert_recorded_state')


if __name__=='__main__':
    unittest.main()
