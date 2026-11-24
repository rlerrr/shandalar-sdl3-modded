"""Verify Vehicle Oracle bindings and trigger/combat choreography."""

import json
from pathlib import Path
import unittest

import generate_cases
import vehicle_triggered_crew_recipes as model


BALLISTA = 'Ballista Charger'
BARGE = 'Bomat Bazaar Barge'
FLEET = 'Fleetwheel Cruiser'
FREIGHT = 'Renegade Freighter'
COPTER = "Smuggler's Copter"
GARRISON = 'Mobile Garrison'
EXPRESS = 'Untethered Express'


class TriggeredCrewTests(unittest.TestCase):
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

    def test_complete_pinned_oracle_and_variants(self):
        model.validate_witnesses(self.oracle,self.rows)
        self.assertEqual(len(self.homes),7)
        for name,home in self.homes.items():
            record = self.oracle[name]
            self.assertEqual(home['oracle_text'],record['oracle_text'])
            self.assertEqual(home['variants'],model.variants(model.spec(home,record)))
            with self.assertRaises(ValueError):
                model.spec(dict(home,params=dict(home['params'],threshold=True)),record)
            with self.assertRaises(ValueError):
                model.spec(dict(home,variants=['cast_0','cast_0']),record)
            with self.assertRaises(ValueError):
                model.spec(home,dict(record,oracle_text=record['oracle_text']+'\nFlying'))

    def test_real_crew_precedes_all_attack_triggers(self):
        for name,mode in [(BALLISTA,'creature'),(FREIGHT,'trample'),(COPTER,'attack_accept'),
                          (GARRISON,'artifact_target'),(EXPRESS,'unblocked')]:
            actions = self.case(name,mode+'_0')['steps']
            ops = [s['op'] for s in actions]
            second_phase = ops.index('begin_attackers')
            cast = ops.index('activate')
            self.assertLess(cast,ops.index('resolve'))
            self.assertLess(ops.index('resolve'),second_phase)
            self.assertIn(dict(op='assert_stack',value=1),actions[cast:second_phase])
            self.assertIn(dict(op='can_target_creature',ref='type_probe',other='source',value=1),actions[cast:second_phase])
            self.assertLess(ops.index('finish_attackers'),ops.index('begin_blockers'))

    def test_barge_dispatch_draws_actual_library_card_after_payment(self):
        actions = self.case(BARGE,'entry_repeat_1')['steps']
        draws = [s for s in actions if s==dict(op='assert_zone',player=1,zone='library',value=0)]
        self.assertTrue(draws)
        for name in ('Island','Forest'):
            self.assertIn(dict(op='assert_zone_multiplicity',player=1,zone='hand',index=self.rows[name]['csvid'],value=1),actions)
        casts = [i for i,s in enumerate(actions) if s['op']=='cast' and s['ref']=='source']
        first,second = casts
        self.assertIn(dict(op='bind_hand',ref='source',player=1,value=self.rows[BARGE]['csvid']),actions[first:second])
        no_draw = self.case(BARGE,'no_draw_crew_0')['steps']
        island = self.rows['Island']['csvid']
        self.assertTrue(all(s['value']==1 for s in no_draw if s['op']=='assert_zone' and s.get('zone')=='library' and s['player']==0))
        self.assertIn(dict(op='assert_zone_multiplicity',player=0,zone='hand',index=island,value=1),no_draw[-26:])
        setup = self.case(BARGE,'no_draw_crew_0')['setup']
        self.assertEqual([(s['ref'],s['zone']) for s in setup[:3]],[('library_0','library'),('library_1','library'),('source','battlefield')])

    def test_fleetwheel_fresh_trampler_and_entry_expiry(self):
        actions = self.case(FLEET,'entry_combat_0')['steps']
        self.assertFalse(any(s['op']=='activate' for s in actions))
        self.assertIn(dict(op='attack',ref='source',value=1),actions)
        self.assertIn(dict(op='assert_life',player=1,value=17),actions)
        at = next(i for i,s in enumerate(actions) if s['op']=='cleanup')
        self.assertIn(dict(op='can_target_creature',ref='type_probe',other='source',value=0),actions[at:])
        underpowered = self.case(FLEET,'underpowered_1')['steps']
        cut = underpowered.index(dict(op='can_activate',ref='source',value=0))
        self.assertIn(dict(op='cast',ref='source',value=1),underpowered[:cut])
        self.assertEqual(sum(s['op']=='advance_turn' for s in underpowered[:cut]),2)
        self.assertIn(dict(op='activate',ref='source',value=0),underpowered[cut:])

    def test_ballista_distinguishes_player_creature_and_walker(self):
        for mode,action,receipt in [
            ('player',dict(op='choose_player',player=0,value=1),dict(op='assert_life',player=1,value=19)),
            ('creature',dict(op='choose_target',player=0,ref='victim'),dict(op='assert_card',ref='victim',property='damage',value=1)),
            ('walker',dict(op='choose_target',player=0,ref='victim'),dict(op='assert_counter',ref='victim',counter_kind='loyalty',value=2)),
        ]:
            with self.subTest(mode=mode):
                steps = self.case(BALLISTA,mode+'_0')['steps']
                at = steps.index(dict(op='finish_attackers'))
                self.assertEqual(steps[at-1],action)
                self.assertIn(receipt,steps[at:steps.index(dict(op='begin_blockers'))])

    def test_copter_choices_depend_on_trigger_path_and_discard_real_card(self):
        for mode,trigger,decision in [
            ('attack_accept','finish_attackers',dict(op='choose_trigger_source',player=0,ref='source')),
            ('block_accept','advance_combat',dict(op='choose_dialog',player=0,value=0)),
            ('block_decline','advance_combat',dict(op='choose_dialog',player=0,value=1)),
        ]:
            steps = self.case(COPTER,mode+'_0')['steps']
            at = steps.index(dict(op=trigger))
            self.assertIn(decision,steps[:at])
            if mode.endswith('accept'):
                self.assertEqual(steps[at-1],dict(op='choose_zone_card',player=0,value=self.rows['Plains']['csvid']))
                self.assertIn(dict(op='assert_zone_multiplicity',player=0,zone='graveyard',index=self.rows['Plains']['csvid'],value=1),steps[at:])
            else:
                self.assertNotIn('choose_zone_card',[s['op'] for s in steps])
        ai_steps = self.case(COPTER,'block_accept_1')['steps']
        self.assertIn(dict(op='assert_zone_multiplicity',player=1,zone='graveyard',index=self.rows['Island']['csvid'],value=1),ai_steps)
        ai_attack = self.case(COPTER,'attack_accept_1')
        self.assertEqual(sum(x['zone']=='library' for x in ai_attack['setup']),12)
        self.assertIn(dict(op='assert_zone',player=1,zone='library',value=11),ai_attack['steps'])
        ground = self.case(COPTER,'attack_ground_0')['steps']
        self.assertIn(dict(op='block',ref='blocker',other='source',value=0),ground)

    def test_garrison_untaps_actual_cost_creature_or_artifact_and_excludes_enemy(self):
        for mode,ref in [('pilot','pilot_0'),('artifact_target','recipient')]:
            steps = self.case(GARRISON,mode+'_0')['steps']
            at = steps.index(dict(op='finish_attackers'))
            self.assertEqual(steps[at-1],dict(op='choose_target',player=0,ref=ref))
            self.assertIn(dict(op='assert_card',ref=ref,property='tapped',value=1),steps[:at])
            self.assertIn(dict(op='assert_card',ref=ref,property='tapped',value=0),steps[at:])
            self.assertIn(dict(op='assert_card',ref='source',property='tapped',value=1),steps[at:])
        steps = self.case(GARRISON,'enemy_target_1')['steps']
        at = steps.index(dict(op='finish_attackers'))
        self.assertEqual(steps[at+1],dict(op='assert_unused_choices',value=1))
        self.assertIn(dict(op='assert_card',ref='recipient',property='tapped',value=1),steps[at:])

    def test_temporary_and_persistent_attack_bonuses_split_on_cleanup(self):
        steps = self.case(FREIGHT,'cleanup_recrew_0')['steps']
        cut = steps.index(dict(op='cleanup'))
        self.assertIn(dict(op='assert_card',ref='source',property='power',value=5),steps[:cut])
        self.assertIn(dict(op='assert_card',ref='source',property='power',value=4),steps[cut:])
        steps = self.case(EXPRESS,'repeat_attack_1')['steps']
        cut = steps.index(dict(op='cleanup'))
        self.assertIn(dict(op='assert_counter',ref='source',counter_kind='plus1',value=1),steps[cut:])
        self.assertIn(dict(op='assert_counter',ref='source',counter_kind='plus1',value=2),steps[cut:])
        self.assertEqual(sum(s['op']=='finish_attackers' for s in steps),2)

    def test_replay_uses_real_trigger_dispatch(self):
        for name,mode,op in [(BARGE,'entry_replay','cast'),(BALLISTA,'trigger_replay','finish_attackers'),
                             (COPTER,'trigger_replay','finish_attackers'),(EXPRESS,'pending_replay','resolve')]:
            steps = self.case(name,mode+'_0')['steps']
            capture = steps.index(dict(op='capture'))
            record = steps.index(dict(op='record_state'))
            restore = steps.index(dict(op='restore'))
            self.assertEqual(steps[capture+1:record],steps[restore+1:-1])
            self.assertIn(op,[s['op'] for s in steps[capture+1:record]])
            self.assertEqual(steps[-1],dict(op='assert_recorded_state'))


if __name__=='__main__':
    unittest.main()
