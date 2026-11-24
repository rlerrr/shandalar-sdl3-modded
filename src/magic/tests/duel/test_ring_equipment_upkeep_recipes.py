"""Check whole Oracle selection, real phase transitions and action receipts."""
import copy
import json
from pathlib import Path
import unittest

from generate_cases import registry
from recipes import expand
import ring_equipment_upkeep_recipes as family


class RingEquipmentUpkeepTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        root = Path(__file__).parent
        cls.oracle = json.loads((root/'data/oracle.json').read_text(encoding='utf8'))['cards']
        cls.rows = {row['name']:row for row in registry()}
        cls.cases, cls.declarations = {}, {}
        for name, pin in family.pins()['sources'].items():
            csvid = pin['registry']['csvid']
            path, = (root/'data/cases/cards/126').glob(f'{csvid:05d}-*.json')
            declaration = json.loads(path.read_text(encoding='utf8'))['recipes'][0]
            cls.declarations[name] = declaration
            cls.cases[name] = {case['_recipe']['variant']:case for case in expand(
                name,csvid,declaration,cls.oracle[name],cls.rows,cls.oracle)}

    def steps(self, name, mode, player):
        return self.cases['Ring of '+name][f'{mode}_{player}']['steps']

    def test_full_oracle_and_normal_registry_are_pinned(self):
        self.assertEqual(sum(map(len,self.cases.values())),150)
        for name, declaration in self.declarations.items():
            oracle = self.oracle[name]
            family.spec(declaration,oracle)
            with self.assertRaises(ValueError):
                family.parameters(dict(oracle,oracle_text=oracle['oracle_text']+'\nDraw a card.'))
            with self.assertRaises(ValueError):
                family.spec(dict(declaration,variants=['growth_twice_0',0]),oracle)
            bad = copy.deepcopy(declaration)
            bad['params']['equip'] = 2
            with self.assertRaises(ValueError):
                family.spec(bad,oracle)
        with self.assertRaises(ValueError):
            family.validate_witnesses(self.oracle,dict(self.rows,Rootwalla=dict(self.rows['Rootwalla'],handler='card_dummy')))

    def test_exact_costs_and_equip_declines(self):
        for name in self.cases:
            for owner in (0,1):
                good = self.steps(name.removeprefix('Ring of '),'growth_twice',owner)
                fund = [s['value'] for s in good if s['op']=='set_mana' and s['value']>0]
                self.assertEqual(fund,[2,1])
                attach = good.index(dict(op='activate',ref='source',value=1))
                self.assertEqual(good[attach-2:attach], [dict(op='choose_dialog',player=owner,value=0),
                                                        dict(op='choose_target',player=owner,ref='host')])
                for mode in ('equip_unfunded','equip_wrong_controller','equip_wrong_type','equip_offturn'):
                    bad = self.steps(name.removeprefix('Ring of '),mode,owner)
                    self.assertIn(dict(op='activate',ref='source',value=0),bad)
                    self.assertFalse(any(s['op'].startswith('choose') for s in bad))

    def test_matching_upkeeps_accumulate_and_other_upkeeps_do_not(self):
        for name in self.cases:
            for owner in (0,1):
                key = name.removeprefix('Ring of ')
                good = self.steps(key,'growth_twice',owner)
                self.assertEqual([s['player'] for s in good if s['op']=='upkeep_step'],[1-owner,owner,1-owner,owner])
                for i, step in enumerate(good):
                    if step['op']=='upkeep_step':
                        self.assertEqual(good[i-1],dict(op='untap_step',player=step['player']))
                self.assertEqual([s['value'] for s in good if s['op']=='assert_counter'],[0,1,1,2,2])
                for mode in ('wrong_color','unattached','equip_target_leaves','equipment_removed'):
                    contrary = self.steps(key,mode,owner)
                    self.assertTrue(all(s['value']==0 for s in contrary if s['op']=='assert_counter'))
        for key in ('Valkas','Thune'):
            steps = self.steps(key,'multicolor',0)
            self.assertIn(dict(op='assert_counter',ref='host',counter_kind='plus1',value=1),steps)

    def test_real_reattach_and_snapshot(self):
        for name in self.cases:
            key = name.removeprefix('Ring of ')
            for owner in (0,1):
                moved = self.steps(key,'move_to_match',owner)
                intents = [s['ref'] for s in moved if s['op']=='choose_target']
                self.assertEqual(intents,['host','other_host'])
                self.assertIn(dict(op='assert_counter',ref='other_host',counter_kind='plus1',value=1),moved)
                self.assertNotIn(dict(op='assert_counter',ref='host',counter_kind='plus1',value=1),moved)
                run = self.steps(key,'pending_equip_replay',owner)
                before = run.index(dict(op='capture'))
                first_end = run.index(dict(op='record_state'))
                replay = run.index(dict(op='restore'))
                self.assertIn(dict(op='activate',ref='source',value=1),run[:before])
                self.assertEqual(run[before+1:first_end],run[replay+1:-1])

    def test_real_keywords_change_combat_and_fresh_actions(self):
        for owner in (0,1):
            for mode, life in [('trample_combat',19),('trample_removed',20)]:
                combat = self.steps('Kalonia',mode,owner)
                self.assertIn(dict(op='block',ref='blocker',other='host',value=1),combat)
                self.assertLess(combat.index(dict(op='combat_damage')),combat.index(dict(op='end_combat')))
                self.assertIn(dict(op='assert_life',player=1-owner,value=life),combat)
            quiet = self.steps('Thune','vigilance_attack',owner)
            self.assertIn(dict(op='attack',ref='host',value=1),quiet)
            self.assertIn(dict(op='assert_card',ref='host',property='tapped',value=0),quiet)
            fast = self.steps('Valkas','haste_fresh_attack',owner)
            self.assertIn(dict(op='cast',ref='host',value=1),fast)
            self.assertIn(dict(op='assert_card',ref='host',property='sick',value=1),fast)
            self.assertIn(dict(op='attack',ref='host',value=1),fast)
            tap = self.steps('Valkas','haste_fresh_tap',owner)
            self.assertIn(dict(op='activate',ref='host',value=1),tap)
            self.assertIn(dict(op='assert_mana',player=owner,color=3,value=1),tap)
            self.assertIn(dict(op='attack',ref='host',value=0),self.steps('Valkas','haste_removed',owner))

    def test_mandatory_entry_life_applies_to_the_fixture_creatures_controller(self):
        cases = self.cases['Ring of Thune'].values()
        affected = [case for case in cases if any(s['name']=='Lone Missionary' for s in case['setup'])]
        self.assertEqual(len(affected),24)
        for case in affected:
            witness, = [s for s in case['setup'] if s['name']=='Lone Missionary']
            self.assertEqual(witness['zone'],'battlefield')
            self.assertIn(dict(op='assert_life',player=witness['player'],value=24),case['steps'])
        for owner in (0,1):
            checks = [s for s in self.steps('Thune','vigilance_attack',owner) if s['op']=='assert_life']
            self.assertCountEqual(checks,[dict(op='assert_life',player=owner,value=24),
                                          dict(op='assert_life',player=1-owner,value=18)])

    def test_evos_eligibility_owner_opponent_and_cleanup(self):
        for owner in (0,1):
            steps = self.steps('Evos Isle','hexproof_offturn',owner)
            probe = dict(op='can_target_creature',ref='enemy_probe',other='host',value=0)
            self.assertIn(dict(op='can_target_creature',ref='own_probe',other='host',value=1),steps)
            self.assertLess(steps.index(probe),steps.index(dict(op='cleanup')))
            self.assertGreater(steps.index(dict(probe,value=1)),steps.index(dict(op='cleanup')))
            self.assertIn(dict(probe,value=1),self.steps('Evos Isle','hexproof_unfunded',owner))

    def test_xathrid_real_damage_destruction_and_proactive_oracle(self):
        for owner in (0,1):
            steps = self.steps('Xathrid','regenerate_window',owner)
            plan = steps.index(dict(op='choose_regeneration',ref='source'))
            self.assertEqual(steps[plan+1],dict(op='choose_dialog',player=owner,value=0))
            self.assertIn(dict(op='assert_card',ref='host',property='damage',value=2),steps[:plan])
            for prop, value in [('attacking',0),('damage',0),('tapped',1)]:
                self.assertIn(dict(op='assert_card',ref='host',property=prop,value=value),steps[plan:])
            self.assertIn(dict(op='cast',ref='second_destruction',value=1),steps[plan:])
            proactive = self.steps('Xathrid','regenerate_proactive',owner)
            self.assertNotIn(dict(op='choose_regeneration',ref='source'),proactive)
            self.assertLess(proactive.index(dict(op='turn',player=1-owner,value=0)),
                            proactive.index(dict(op='cast',ref='destroy_host',value=1)))
            self.assertLess(proactive.index(dict(op='activate',ref='source',value=1),
                                            proactive.index(dict(op='turn',player=1-owner,value=0))),
                            proactive.index(dict(op='cast',ref='destroy_host',value=1)))


if __name__ == '__main__':
    unittest.main()
