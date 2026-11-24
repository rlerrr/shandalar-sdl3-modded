"""Focused guards for fixed upkeep costs, legal combat and paid history."""

import json
from pathlib import Path
import unittest

from generate_cases import registry, validate_upkeep_step
from recipes import expand
import upkeep_pay_or_sacrifice_creature_recipes as family


class FixedUpkeepTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        data = Path(__file__).parent / 'data'
        cls.oracle = json.loads((data/'oracle.json').read_bytes())['cards']
        cls.rows = {row['name']:row for row in registry()}
        cls.declarations, cls.cases = {}, {}
        for name,pin in family.pins()['sources'].items():
            csvid = pin['registry']['csvid']
            home, = (data/'cases/cards'/f'{csvid//100:03d}').glob(f'{csvid:05d}-*.json')
            declaration = json.loads(home.read_bytes())['recipes'][0]
            cls.declarations[name] = declaration
            cls.cases[name] = {case['_recipe']['variant']:case for case in expand(
                name,csvid,declaration,cls.oracle[name],cls.rows,cls.oracle)}
        follow = {'Fettergeist','Krosan Cloudscraper'}
        cls.follow_cases = {name:cls.cases.pop(name) for name in follow}
        cls.follow_declarations = {name:cls.declarations.pop(name) for name in follow}

    def case(self, name, mode, owner):
        return self.cases[name][f'{mode}_{owner}']

    def tail(self, case):
        steps = case['steps']
        return steps[max(i for i,s in enumerate(steps) if s['op']=='upkeep_step')+1:]

    def zone(self, steps, name, owner, zone):
        self.assertIn(dict(op='assert_zone_multiplicity',player=owner,zone=zone,
                           index=self.rows[name]['csvid'],value=1),steps)

    def test_full_oracle_and_independent_witness_pins(self):
        self.assertEqual(sum(map(len,self.cases.values())),300)
        for name,decl in self.declarations.items():
            record = self.oracle[name]
            family.spec(decl,record)
            for text in (record['oracle_text']+'\nDraw a card.',record['oracle_text'].replace('your upkeep','each upkeep')):
                with self.assertRaises(ValueError):
                    family.parameters(dict(record,oracle_text=text))
            with self.assertRaises(ValueError):
                family.spec(dict(decl,params=dict(decl['params'],power=True)),record)
        bad = dict(self.rows,Island=dict(self.rows['Island'],handler='card_dummy'))
        with self.assertRaises(ValueError):
            family.validate_witnesses(self.oracle,bad)

    def test_pay_uses_only_live_lands_and_actual_controller_upkeep(self):
        for name in self.cases:
            fee = self.declarations[name]['params']['upkeep_mana']
            for owner in (0,1):
                case = self.case(name,'pay',owner)
                steps = case['steps']
                self.assertEqual([s['player'] for s in steps if s['op']=='upkeep_step'],[1-owner,owner])
                at = max(i for i,s in enumerate(steps) if s['op']=='upkeep_step')
                clicks = [s['ref'] for s in steps[:at] if s['op']=='choose_mana_source']
                matching = [e['ref'] for e in case['setup'] if e['ref'].startswith('land_')]
                self.assertEqual(len(matching),sum(fee))
                self.assertEqual(clicks,matching if owner==0 else [])
                self.assertIn(dict(op='choose_dialog',player=owner,value=0),steps[:at])
                for ref in matching:
                    self.assertIn(dict(op='assert_card',ref=ref,property='tapped',value=1),steps[at:])
                self.zone(steps[at:],name,owner,'battlefield')

    def test_refusal_shortage_wrong_color_and_enemy_mana_sacrifice(self):
        for name in self.cases:
            for owner in (0,1):
                for mode in ('decline','short','wrong_color','opponent_funds'):
                    case = self.case(name,mode,owner)
                    decisions = [s for s in case['steps'] if s['op']=='choose_dialog']
                    self.assertEqual(decisions,[dict(op='choose_dialog',player=owner,value=1)] if mode=='decline' else [])
                    self.assertFalse(any(s['op']=='choose_mana_source' for s in case['steps']))
                    for land in [e['ref'] for e in case['setup'] if e['ref'].startswith('land_')]:
                        self.assertIn(dict(op='assert_card',ref=land,property='tapped',value=0),self.tail(case))
                    self.zone(self.tail(case),name,owner,'graveyard')
            cancel = self.case(name,'cancel',0)
            self.assertIn(dict(op='choose_cancel_mana',player=0),cancel['steps'])
            self.zone(self.tail(cancel),name,0,'graveyard')

    def test_each_own_upkeep_charges_again_or_sacrifices(self):
        for name in self.cases:
            for owner in (0,1):
                for mode,choices,zone in [('repeat_pay',[0,0],'battlefield'),('repeat_decline',[0,1],'graveyard'),
                                          ('repeat_short',[0],'graveyard')]:
                    case = self.case(name,mode,owner)
                    steps = case['steps']
                    self.assertEqual([s['player'] for s in steps if s['op']=='upkeep_step'],[1-owner,owner]*2)
                    self.assertEqual([s['value'] for s in steps if s['op']=='choose_dialog'],choices)
                    self.zone(self.tail(case),name,owner,zone)
                    if mode=='repeat_short':
                        removed = steps.index(dict(op='cast',ref='removal',value=1))
                        self.assertEqual(sum(s['op']=='upkeep_step' for s in steps[:removed]),2)

    def test_controller_changes_cost_and_owner_receives_sacrifice(self):
        for name in self.cases:
            for owner in (0,1):
                for mode,zone,player in [('control_pay','battlefield',1-owner),('control_short','graveyard',owner)]:
                    case = self.case(name,mode,owner)
                    steps = case['steps']
                    transfer = steps.index(dict(op='activate',ref='trader',value=1))
                    self.assertIn(dict(op='bind_permanent',ref='source',player=1-owner,value=self.rows[name]['csvid']),steps[transfer:])
                    self.assertEqual([s['player'] for s in steps if s['op']=='upkeep_step'],[1-owner,owner])
                    self.zone(self.tail(case),name,player,zone)

    def test_real_murder_contrast_and_absent_source(self):
        for name in self.cases:
            for owner in (0,1):
                for mode,zone in [('destroy_before','graveyard'),('sacrifice_indestructible','battlefield')]:
                    case = self.case(name,mode,owner)
                    steps = case['steps']
                    killed = steps.index(dict(op='cast',ref='removal',value=1))
                    first_upkeep = next(i for i,s in enumerate(steps) if s['op']=='upkeep_step')
                    self.zone(steps[killed:first_upkeep],name,owner,zone)
                    self.zone(self.tail(case),name,owner,'graveyard')
                self.zone(self.tail(self.case(name,'in_hand',owner)),name,owner,'hand')

    def test_post_upkeep_entry_does_not_trigger_until_next_turn(self):
        for source in self.cases:
            for owner in (0,1):
                steps = self.case(source,'cast_after_upkeep',owner)['steps']
                cast = steps.index(dict(op='cast',ref='source',value=1))
                self.assertEqual([s['player'] for s in steps[:cast] if s['op']=='upkeep_step'],[1-owner,owner])
                self.assertFalse(any(s['op']=='choose_dialog' for s in steps[:cast]))
                self.assertEqual([s['player'] for s in steps[cast:] if s['op']=='upkeep_step'],[1-owner,owner])
                self.assertIn(dict(op='choose_dialog',player=owner,value=0),steps[cast:])

    def test_flying_and_shadow_are_actual_two_way_combat_receipts(self):
        for owner in (0,1):
            for mode,who,other,allowed in [('attack_ground','witness','source',0),('attack_shadow','witness','source',1),
                                          ('defend_shadow','source','witness',1),('defend_ground','source','witness',0)]:
                case = self.case('Drifter il-Dal',mode,owner)
                self.assertIn(dict(op='block',ref=who,other=other,value=allowed),case['steps'])
                self.assertLess(case['steps'].index(dict(op='combat_damage')),case['steps'].index(dict(op='end_combat')))
            for name in ('Spindrift Drake','Phantasmal Forces'):
                for mode,who,other,allowed in [('attack_ground','witness','source',0),('attack_reach','witness','source',1),
                                              ('attack_flyer','witness','source',1),('defend_flyer','source','witness',1)]:
                    steps = self.case(name,mode,owner)['steps']
                    self.assertIn(dict(op='block',ref=who,other=other,value=allowed),steps)
            self.assertIn(dict(op='attack',ref='source',value=0),self.case('Hungry Mist','fresh_attack',owner)['steps'])

    def test_replays_repeat_same_upkeep_state_and_real_inputs(self):
        for name in self.cases:
            for owner in (0,1):
                for mode in ('untap_replay','paid_replay'):
                    steps = self.case(name,mode,owner)['steps']
                    capture = steps.index(dict(op='capture'))
                    record = steps.index(dict(op='record_state'))
                    restore = steps.index(dict(op='restore'))
                    self.assertEqual(steps[capture+1:record],steps[restore+1:-1])
                    self.assertIn(dict(op='ai_roundtrip'),steps[record:restore])
                    self.assertEqual(steps[-1],dict(op='assert_recorded_state'))
                    for step in steps:
                        if step['op']=='upkeep_step':
                            validate_upkeep_step(step)

    def test_fettergeist_prices_live_other_controlled_creatures(self):
        name = 'Fettergeist'
        self.assertEqual(len(self.follow_cases[name]),61)
        case = self.follow_cases[name]
        self.assertIn('Flying',self.follow_declarations[name]['params']['keywords'])
        for owner in (0,1):
            for mode,count,zone in [('zero_only',0,'battlefield'),('zero_foe',0,'battlefield'),
                                    ('zero_noncreature',0,'battlefield'),('count_removed',1,'battlefield'),
                                    ('count_two',2,'battlefield'),('count_two_short',0,'graveyard'),
                                    ('generic_colored',1,'battlefield')]:
                sample = case[f'{mode}_{owner}']
                self.assertFalse(any(s['op']=='choose_dialog' for s in sample['steps']))
                self.assertEqual(sum(s['op']=='choose_mana_source' for s in sample['steps']),count if owner==0 else 0)
                tapped = {s['ref'] for s in self.tail(sample) if s['op']=='assert_card' and s['property']=='tapped'
                          and s['value'] and s['ref'].startswith('land_')}
                self.assertEqual(len(tapped),count)
                self.zone(self.tail(sample),name,owner,zone)
            removed = case[f'count_removed_{owner}']['steps']
            attack = case[f'attack_ground_{owner}']['steps']
            self.assertIn(dict(op='choose_target',player=1-owner,ref='ally_1'),removed)
            self.assertIn(dict(op='block',ref='witness',other='source',value=0),attack)
        cancel = case['cancel_0']
        self.assertIn(dict(op='choose_cancel_mana',player=0),cancel['steps'])
        self.zone(self.tail(cancel),name,0,'graveyard')

    def test_morph_is_paid_and_changes_which_upkeep_clause_exists(self):
        name = 'Krosan Cloudscraper'
        case = self.follow_cases[name]
        self.assertEqual(len(case),67)
        fees = self.follow_declarations[name]['params']
        self.assertEqual((fees['cast_mana'],fees['morph'],fees['upkeep_mana']),
                         ([7,0,0,3,0,0],[7,0,0,2,0,0],[0,0,0,2,0,0]))
        for owner in (0,1):
            silent = case[f'hidden_upkeep_{owner}']
            self.assertFalse(any(s['op']=='choose_dialog' for s in silent['steps']))
            for key, value in [('csvid',13252),('power',2),('toughness',2)]:
                self.assertIn(dict(op='assert_card',ref='source',property=key,value=value),self.tail(silent))
            late = case[f'reveal_after_upkeep_{owner}']['steps']
            flip = late.index(dict(op='activate',ref='source',value=1))
            self.assertEqual([s['player'] for s in late[:flip] if s['op']=='upkeep_step'],[1-owner,owner])
            self.assertNotIn(dict(op='choose_dialog',player=owner,value=0),late[:flip])
            self.assertIn(dict(op='choose_dialog',player=owner,value=0),late[flip:])
            self.assertIn(dict(op='assert_card',ref='source',property='power',value=13),late[flip:])
            opposing = case[f'reveal_opposing_{owner}']['steps']
            flip = opposing.index(dict(op='activate',ref='source',value=1))
            self.assertEqual([s['player'] for s in opposing[:flip] if s['op']=='upkeep_step'],[1-owner])
            for mode in ('reveal_short','reveal_wrong_color'):
                self.assertIn(dict(op='activate',ref='source',value=0),case[f'{mode}_{owner}']['steps'])
            wrong = case[f'reveal_then_short_{owner}']
            self.zone(self.tail(wrong),name,owner,'graveyard')


if __name__=='__main__':
    unittest.main()
