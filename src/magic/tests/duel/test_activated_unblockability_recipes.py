"""Focused guards for native activation, real blockers, and optional untap payment."""

import json
from pathlib import Path
import unittest

from generate_cases import registry
import activated_unblockability_recipes as model


PASSAGE = "Rogue's Passage"
WAND = "Tawnos's Wand"
TOOLS = "Joven's Tools"
LINE = 'Break Through the Line'
KEY = 'Key to the City'


class ActivatedUnblockabilityRecipes(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        data = Path(__file__).parent/'data'
        cls.oracle = json.loads((data/'oracle.json').read_bytes())['cards']
        cls.rows = {row['name']:row for row in registry()}
        cls.homes = {}
        for name,pin in model.pins()['sources'].items():
            csvid = pin['registry']['csvid']
            path, = (data/'cases/cards').glob(f'*/{csvid:05d}-*.json')
            cls.homes[name] = json.loads(path.read_bytes())['recipes'][0]

    def case(self,name,variant):
        declaration = self.homes[name]
        params = model.spec(declaration,self.oracle[name])
        return model.expand(name,self.rows[name]['csvid'],declaration,params,[variant],self.rows,self.oracle)[0]

    def ops(self,name,variant,op,**fields):
        return [row for row in self.case(name,variant)['steps'] if row['op']==op and
                all(row.get(key)==value for key,value in fields.items())]

    def test_whole_printed_clauses_and_typed_parameters(self):
        model.validate_witnesses(self.oracle,self.rows)
        for name,home in self.homes.items():
            original = self.oracle[name]
            params = model.parameters(original)
            self.assertEqual(home['variants'],model.variants(params))
            self.assertEqual(home['oracle_text'],original['oracle_text'])
            with self.assertRaises(ValueError):
                model.spec(home,dict(original,oracle_text=original['oracle_text']+'\nDraw a card.'))
            with self.assertRaises(ValueError):
                model.spec(dict(home,version=True),original)
            with self.assertRaises(ValueError):
                model.spec(dict(home,params=dict(params,taps=int(params['taps']))),original)
            with self.assertRaises(ValueError):
                model.spec(dict(home,variants=['effect_0','effect_0']),original)

    def test_true_wall_exception_and_owner_crossing(self):
        for actor in (0,1):
            self.assertTrue(self.ops(TOOLS,f'wall_allowed_{actor}','block',ref='blocker',other='ally',value=1))
            self.assertTrue(self.ops(TOOLS,f'effect_{actor}','block',ref='blocker',other='ally',value=0))
            self.assertTrue(self.ops(TOOLS,f'enemy_target_{actor}','choose_target',ref='ally',player=actor))
            self.assertTrue(self.ops(TOOLS,f'offturn_{actor}','choose_target',ref='offturn_target',player=actor))
            self.assertTrue(self.ops(TOOLS,f'offturn_{actor}','block',ref='blocker',other='ally',value=1))

    def test_printed_passage_fee_and_immediate_colorless_alternate(self):
        for actor in (0,1):
            exact = self.case(PASSAGE,f'exact_fee_{actor}')['steps']
            self.assertTrue(self.ops(PASSAGE,f'exact_fee_{actor}','set_mana',player=actor,color=0,value=4))
            index, = [i for i,s in enumerate(exact) if s['op']=='choose_activation_mode']
            self.assertEqual([s['op'] for s in exact[index:index+4]],
                             ['choose_activation_mode','choose_target','activate','require_activation_mode'])
            self.assertTrue(self.ops(PASSAGE,f'exact_fee_{actor}','assert_mana',player=actor,color=0,value=0))
            self.assertFalse(self.ops(PASSAGE,f'mana_{actor}','choose_activation_mode'))
            self.assertTrue(self.ops(PASSAGE,f'mana_{actor}','assert_mana',player=actor,color=0,value=1))
            self.assertFalse(self.ops(PASSAGE,f'mana_{actor}','assert_stack',value=1))
            self.assertTrue(self.ops(PASSAGE,f'expiry_{actor}','cast',ref='mana_sink',value=1))

    def test_live_threshold_rechecks_before_resolution(self):
        for name in (WAND,LINE):
            for actor in (0,1):
                self.assertTrue(self.ops(name,f'high_only_{actor}','activate',ref='source',value=0))
                self.assertTrue(self.ops(name,f'power_boundary_{actor}','assert_card',ref='ally',property='power',value=2))
                scenario = f'power_increased_pending_{actor}'
                self.assertTrue(self.ops(name,scenario,'assert_stack',value=2))
                self.assertTrue(self.ops(name,scenario,'assert_card',ref='ally',property='power',value=4))
                self.assertTrue(self.ops(name,scenario,'block',ref='blocker',other='ally',value=1))

    def test_fresh_haste_and_expiry_are_distinct(self):
        self.assertTrue(self.ops(LINE,'fresh_without_0','attack',ref='ally',value=0))
        self.assertTrue(self.ops(LINE,'fresh_haste_0','attack',ref='ally',value=1))
        self.assertTrue(self.ops(LINE,'haste_expiry_0','cleanup'))
        self.assertTrue(self.ops(LINE,'haste_expiry_0','assert_card',ref='ally',property='sick',value=1))
        blocks = self.ops(LINE,'expiry_0','block',ref='blocker',other='ally')
        self.assertEqual([row['value'] for row in blocks],[0,1])

    def test_discard_is_paid_before_resolution_with_optional_zero_targets(self):
        for actor in (0,1):
            scenario = f'effect_{actor}'
            self.assertTrue(self.ops(KEY,scenario,'assert_zone_multiplicity',player=actor,zone='graveyard',value=1,index=self.rows['Plains']['csvid']))
            self.assertTrue(self.ops(KEY,f'no_discard_{actor}','activate',ref='source',value=0))
        steps = self.case(KEY,'zero_targets_0')['steps']
        self.assertEqual([s['op'] for s in steps if s['op'] in ('choose_zone_card','choose_cancel_target','activate')],
                         ['choose_zone_card','choose_cancel_target','activate'])
        self.assertTrue(self.ops(KEY,'zero_targets_0','block',ref='blocker',value=1))

    def test_real_untap_paid_declined_short_and_no_transition(self):
        for actor in (0,1):
            paid = f'untap_pay_{actor}'
            declined = f'untap_decline_{actor}'
            short = f'untap_short_{actor}'
            untouched = f'already_untapped_{actor}'
            for variant in (paid,declined,short,untouched):
                self.assertTrue(self.ops(KEY,variant,'cast',ref='reply',value=1))
                self.assertTrue(self.ops(KEY,variant,'choose_dialog',player=1-actor,value=1))
            self.assertTrue(self.ops(KEY,paid,'choose_dialog',player=actor,value=0))
            self.assertTrue(self.ops(KEY,declined,'choose_dialog',player=actor,value=1))
            self.assertFalse(self.ops(KEY,short,'choose_dialog',player=actor))
            self.assertFalse(self.ops(KEY,untouched,'choose_dialog',player=actor))
            self.assertTrue(self.ops(KEY,paid,'assert_zone_multiplicity',player=actor,zone='hand',value=1,index=self.rows['Forest']['csvid']))
            self.assertTrue(self.ops(KEY,declined,'assert_zone',player=actor,zone='hand',value=0))
            self.assertTrue(self.ops(KEY,f'natural_untap_{actor}','untap_step',player=actor))

    def test_source_departure_and_replay_keep_real_activation(self):
        for name in self.homes:
            self.assertTrue(self.ops(name,'source_leaves_pending_0','assert_stack',value=2))
            self.assertTrue(self.ops(name,'source_leaves_pending_0','block',ref='blocker',value=0))
            self.assertTrue(self.ops(name,'target_leaves_pending_0','assert_stack',value=2))
            self.assertFalse(self.ops(name,'target_leaves_pending_0','attack'))
            replay = self.case(name,'replay_1')['steps']
            self.assertEqual(sum(s['op']=='activate' for s in replay),2)
            self.assertEqual(sum(s['op']=='capture' for s in replay),1)
            self.assertEqual(replay[-1]['op'],'assert_recorded_state')


if __name__=='__main__':
    unittest.main()
