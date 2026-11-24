"""Focused guards for target timing, real combat and entry-repeat boundaries."""

import json
from pathlib import Path
import unittest

from generate_cases import registry
import entry_target_unblockability_recipes as model


NEUROK = 'Neurok Invisimancer'
SCOUT = 'Subterranean Scout'


class EntryTargetUnblockabilityRecipes(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        data = Path(__file__).parent/'data'
        cls.oracle = json.loads((data/'oracle.json').read_bytes())['cards']
        cls.rows = {row['name']:row for row in registry()}
        cls.homes = {}
        for name,pin in model.pins()['sources'].items():
            path, = (data/'cases/cards').glob(f'*/{pin["registry"]["csvid"]:05d}-*.json')
            cls.homes[name] = json.loads(path.read_bytes())['recipes'][0]

    def case(self,name,variant):
        declaration = self.homes[name]
        params = model.spec(declaration,self.oracle[name])
        return model.expand(name,self.rows[name]['csvid'],declaration,params,[variant],self.rows,self.oracle)[0]

    def select(self,name,variant,operation,**fields):
        return [row for row in self.case(name,variant)['steps'] if row['op']==operation and
                all(row.get(label)==value for label,value in fields.items())]

    def test_profile_preserves_all_clauses_and_type_checks(self):
        model.validate_witnesses(self.oracle,self.rows)
        for name,home in self.homes.items():
            record = self.oracle[name]
            params = model.parameters(record)
            self.assertEqual(home['params'],params)
            self.assertEqual(home['variants'],model.variants(params))
            self.assertEqual(home['oracle_text'],record['oracle_text'])
            self.assertEqual(params['intrinsic'],name==NEUROK)
            self.assertEqual(params['power_limit'],2 if name==SCOUT else None)
            with self.assertRaises(ValueError):
                model.spec(dict(home,params=dict(params,power=True)),record)
            with self.assertRaises(ValueError):
                model.spec(dict(home,variants=['effect_0','effect_0']),record)
            with self.assertRaises(ValueError):
                model.spec(home,dict(record,oracle_text=record['oracle_text'].replace('enters','attacks')))

    def test_native_entry_target_and_combat_are_after_paid_announcement(self):
        for name in self.homes:
            for seat in (0,1):
                scenario = f'effect_{seat}'
                subset = [s for s in self.case(name,scenario)['steps'] if s['op'] in ('cast','choose_target','resolve','block')]
                self.assertEqual([row['op'] for row in subset[:4]],['cast','choose_target','resolve','block'])
                self.assertEqual((subset[1]['player'],subset[1]['ref']),(seat,'ally'))
                self.assertEqual(subset[3]['value'],0)
                self.assertTrue(self.select(name,scenario,'assert_life',player=1-seat,value=19))
                enemy_case = self.case(name,f'enemy_target_{seat}')
                self.assertEqual((enemy_case['steps'][0]['op'],enemy_case['steps'][0]['player']),('turn',seat))
                controllers = {entry['ref']:entry['player'] for entry in enemy_case['setup']}
                self.assertEqual((controllers['source'],controllers['ally'],controllers['blocker']),(seat,seat,1-seat))
                self.assertTrue(self.select(name,f'enemy_target_{seat}','choose_target',ref='blocker',player=seat))
                self.assertTrue(self.select(name,f'enemy_target_{seat}','block',ref='blocker',value=1))

    def test_paid_boundary_and_alternative_target_at_live_power(self):
        self.assertTrue(self.select(SCOUT,'power_boundary_1','assert_card',ref='ally',property='power',value=2))
        self.assertTrue(self.select(SCOUT,'power_boundary_1','block',ref='blocker',value=0))
        for scenario,power in (('above_boundary_alternative_1',3),('live_power_alternative_0',4)):
            self.assertTrue(self.select(SCOUT,scenario,'assert_card',ref='ally',property='power',value=power))
            self.assertTrue(self.select(SCOUT,scenario,'choose_target',ref='source'))
            self.assertTrue(self.select(SCOUT,scenario,'block',ref='blocker',value=1))
            setup = {row['ref']:row['name'] for row in self.case(SCOUT,scenario)['setup']}
            self.assertEqual(setup['blocker'],'Gladecover Scout')
        self.assertTrue(self.select(SCOUT,'self_only_0','attack',ref='source',value=0))

    def test_true_repeat_entry_preserves_source_departure_and_expiry(self):
        for name in self.homes:
            repeat = self.case(name,'bounce_recast_0')['steps']
            self.assertEqual(sum(s['op']=='cast' and s['ref']=='source' for s in repeat),2)
            self.assertEqual(sum(s['op']=='choose_target' and s['ref']=='ally' for s in repeat),2)
            self.assertEqual([row['value'] for row in self.select(name,'bounce_recast_0','block')],[0,0])
            self.assertEqual([row['value'] for row in self.select(name,'expiry_1','block')],[0,1])
            self.assertTrue(self.select(name,'source_leaves_1','bind_hand',ref='source'))
            self.assertTrue(self.select(name,'source_leaves_1','block',value=0))
        self.assertTrue(self.select(NEUROK,'intrinsic_1','untap_step',player=1))
        self.assertTrue(self.select(NEUROK,'intrinsic_1','block',other='source',value=0))

    def test_counter_target_is_automatic_and_no_entry_effect_applies(self):
        for name in self.homes:
            for seat in (0,1):
                scenario = f'countered_{seat}'
                self.assertFalse(self.select(name,scenario,'choose_target'))
                self.assertTrue(self.select(name,scenario,'cast_response',ref='counter',value=1))
                self.assertTrue(self.select(name,scenario,'assert_stack',value=2))
                self.assertTrue(self.select(name,scenario,'assert_zone_multiplicity',player=seat,zone='graveyard',index=self.rows[name]['csvid'],value=1))
                self.assertTrue(self.select(name,scenario,'block',value=1))
        self.assertFalse(self.select(SCOUT,'wrong_color_1','choose_target'))
        self.assertTrue(self.select(SCOUT,'short_0','cast',ref='source',value=0))

    def test_replay_reexecutes_entry_target_and_combat(self):
        seq = self.case(SCOUT,'pending_replay_1')['steps']
        actions = [row['op'] for row in seq if row['op'] in ('capture','choose_target','block','restore','assert_recorded_state')]
        self.assertEqual(actions,['capture','choose_target','block','restore','choose_target','block','assert_recorded_state'])


if __name__=='__main__':
    unittest.main()
