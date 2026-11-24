import json
import unittest
from pathlib import Path

from generate_cases import registry
from laccolith_creature_recipes import FAMILY, RULE, classify, expand, profiles, spec, validate_witnesses, variants


class LaccolithCreatureRecipesTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.oracle = json.loads((Path(__file__).parent / 'data/oracle.json').read_text(encoding='utf8'))['cards']
        cls.rows = {row['name']:row for row in registry()}

    def case(self, name, mode, owner=0):
        record = self.oracle[name]
        selected = [f'{mode}_{owner}']
        declaration = dict(family=FAMILY, version=2, params=classify(record), oracle_text=RULE, variants=selected)
        case, = expand(name, self.rows[name]['csvid'], declaration, spec(declaration, record), selected, self.rows, self.oracle)
        self.assertTrue(all(not card.get('state') for card in case['setup']))
        self.assertFalse(self.actions(case['steps'], 'activate'))
        return case

    def actions(self, steps, op, **fields):
        return [s for s in steps if s['op'] == op and all(s.get(k) == value for k,value in fields.items())]

    def values(self, steps, op, **fields):
        return [s['value'] for s in self.actions(steps,op,**fields)]

    def stage(self, steps, a, b):
        ops = [s['op'] for s in steps]
        return steps[ops.index(a)+1:ops.index(b)]

    def test_whole_pinned_source_bounds_and_typed_values(self):
        validate_witnesses(self.oracle, self.rows)
        names = profiles()['sources']
        self.assertEqual({self.rows[name]['csvid'] for name in names}, {4705, 4707, 4708, 4709})
        for name in names:
            oracle = self.oracle[name]
            params = classify(oracle)
            declaration = dict(family=FAMILY,version=2,params=params,oracle_text=RULE,variants=variants(params))
            self.assertEqual(spec(declaration,oracle)['power'], int(oracle['power']))
            self.assertNotIn('decline_1', variants(params))
            with self.assertRaises(ValueError):
                spec(dict(declaration,params=dict(params,power=float(params['power']))), oracle)
        with self.assertRaises(ValueError):
            classify(self.oracle["Farrel's Zealot"])
        self.assertNotIn('Laccolith Rig', profiles()['witnesses'])

    def test_real_block_and_separate_target_then_optional_resolution_observation(self):
        c = self.case('Laccolith Grunt','take')
        steps = c['steps']
        block, = self.actions(steps, 'block', ref='blocker', other='source', value=1)
        target, = self.actions(steps,'choose_target',ref='victim',player=0)
        take, = self.actions(steps,'choose_dialog',player=0,value=0)
        observed, = self.actions(steps,'assert_unused_choices',value=0)
        advance, = self.actions(steps,'advance_combat')
        indices = [steps.index(event) for event in (block,target,take,advance,observed)]
        self.assertEqual(indices, sorted(indices))
        self.assertEqual(self.values(steps,'can_activate',ref='source'),[0,0])
        middle = self.stage(steps,'advance_combat','combat_damage')
        self.assertEqual(self.values(middle,'assert_card',ref='victim',property='damage'),[2])
        self.assertEqual(self.values(steps,'assert_card',ref='blocker',property='damage')[-1],0)

    def test_decline_still_has_target_and_actual_combat_damage(self):
        steps = self.case('Laccolith Warrior','decline')['steps']
        self.assertTrue(self.actions(steps,'choose_target',ref='victim',player=0))
        self.assertEqual(self.values(steps,'choose_dialog',player=0),[1])
        self.assertEqual(self.values(steps,'assert_card',ref='victim',property='damage')[-1],0)
        self.assertEqual(self.values(steps,'assert_card',ref='blocker',property='damage')[-1],3)

    def test_only_real_ready_attacker_becoming_blocked_can_offer(self):
        for owner in (0,1):
            unblocked = self.case('Laccolith Warrior','unblocked',owner)['steps']
            defending = self.case('Laccolith Warrior','source_blocks',owner)['steps']
            self.assertFalse(self.actions(unblocked,'choose_target'))
            self.assertFalse(self.actions(defending,'choose_target'))
            self.assertEqual(self.values(unblocked,'assert_life',player=1-owner)[-1],17)
            self.assertEqual(self.values(defending,'block',ref='source',other='opposing'),[1])
            self.assertEqual(self.values(defending,'assert_card',ref='source',property='damage')[-1],2)
            self.assertEqual(self.values(defending,'can_activate',ref='source'),[0])

    def test_power_buff_and_blocker_damage_are_independent(self):
        boosted = self.case('Laccolith Whelp','boost_before_block',1)['steps']
        self.assertTrue(self.actions(boosted,'cast',ref='growth',value=1))
        before_damage = self.stage(boosted,'advance_combat','combat_damage')
        self.assertEqual(self.values(before_damage,'assert_card',ref='victim',property='damage'),[4])
        self.assertEqual(self.values(before_damage,'assert_card',ref='source',property='power'),[4])
        retaliates = self.case('Laccolith Warrior','blocker_returns_damage',1)['steps']
        self.assertEqual(self.values(retaliates,'assert_card',ref='source',property='damage')[-1],2)
        self.assertEqual(self.values(retaliates,'assert_card',ref='blocker',property='damage')[-1],0)

    def test_destroyed_and_bounced_blockers_preclude_unblocked_damage(self):
        for mode, player in (('kill_blocker',1), ('blocker_bounced',0)):
            steps = self.case('Laccolith Whelp',mode,player)['steps']
            mid = self.stage(steps,'advance_combat','combat_damage')
            index = 163 if mode == 'kill_blocker' else 272
            zone = 'graveyard' if mode == 'kill_blocker' else 'hand'
            self.assertIn(1,self.values(mid,'assert_zone_multiplicity',player=1-player,zone=zone,index=index))
            self.assertEqual(self.values(steps,'assert_card',ref='source',property='blocked')[-1],1)
            self.assertEqual(self.values(steps,'assert_life',player=1-player)[-1],20)

    def test_friendly_and_legal_targets_and_late_flash_boundary(self):
        own = self.case('Laccolith Warrior','own_target',1)
        target, = [card for card in own['setup'] if card['ref'] == 'victim']
        self.assertEqual(target['player'],1)
        legal = self.case('Laccolith Warrior','legal_target',1)['steps']
        self.assertEqual(self.values(legal,'can_target_creature',ref='source',other='protected'),[0])
        self.assertEqual(self.values(legal,'can_target_creature',ref='source',other='victim'),[1])
        self.assertFalse(self.actions(legal,'choose_target',ref='protected'))
        late = self.case('Laccolith Warrior','late_flash_target',1)['steps']
        stages = [s['op'] for s in late]
        flash, = self.actions(late,'cast',ref='late',value=1)
        self.assertTrue(stages.index('advance_combat') < late.index(flash) < stages.index('combat_damage'))
        self.assertEqual(self.values(late,'can_target_creature',ref='source',other='late'),[0,1])
        self.assertFalse(self.actions(late,'choose_target',ref='late'))

    def test_real_second_turn_restores_face_damage(self):
        steps = self.case('Laccolith Warrior','next_turn_restore',1)['steps']
        self.assertEqual(len(self.actions(steps,'advance_turn')),4)
        self.assertEqual(self.values(steps,'assert_card',ref='source',property='blocked'),[1,0])
        self.assertEqual(self.values(steps,'assert_life',player=0)[-1],17)

    def test_paid_countered_and_untargeted_denial_boundaries(self):
        denied = self.case('Laccolith Titan','wrong_color')['steps']
        self.assertEqual(self.values(denied,'cast',ref='source'),[0])
        self.assertEqual(self.values(denied,'assert_mana',player=0,color=0)[-1],7)
        countered = self.case('Laccolith Titan','countered',1)['steps']
        self.assertEqual(self.values(countered,'cast_response',ref='counter'),[1])
        self.assertEqual(self.values(countered,'assert_zone_multiplicity',player=1,zone='graveyard',index=4707)[-1],1)

    def test_actual_pinned_bridge_callbacks(self):
        for owner in (0,1):
            rust = self.case('Laccolith Grunt','declare_blockers_bridge',owner)['steps']
            mantle = self.case('Laccolith Grunt','damage_option_bridge',owner)['steps']
            mid = self.stage(rust,'advance_combat','combat_damage')
            self.assertEqual(self.values(mid,'assert_zone_multiplicity',player=1-owner,zone='graveyard',index=119),[1])
            self.assertEqual(self.values(rust,'assert_card',ref='blocker',property='damage')[-1],4)
            self.assertTrue(self.actions(mantle,'choose_dialog',player=owner,value=0))
            self.assertTrue(self.actions(mantle,'choose_target',player=owner,ref='victim'))
            pending = self.stage(mantle,'advance_combat','combat_damage')
            self.assertEqual(self.values(pending,'assert_card',ref='victim',property='damage'),[])
            self.assertEqual(self.values(mantle,'assert_card',ref='victim',property='damage')[-1],4)
            self.assertEqual(self.values(mantle,'assert_life',player=1-owner)[-1],20)

    def test_pending_declaration_snapshot_replays_real_boundary(self):
        steps = self.case('Laccolith Grunt','pending_block_snapshot')['steps']
        ops = [s['op'] for s in steps]
        capture, record, restore = map(ops.index,('capture','record_state','restore'))
        self.assertEqual(ops[capture+1], 'advance_combat')
        self.assertEqual(steps[capture+1:record], steps[restore+1:-1])
