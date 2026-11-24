"""Guard the whole Oracle, offered menu choices and real Bestow lifecycles."""
import json
from pathlib import Path
import unittest

from generate_cases import registry
from recipes import cost_from_oracle, expand
import bestow_stat_aura_recipes as family


class BestowStatAuraTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        root = Path(__file__).parent
        cls.oracle = json.loads((root/'data/oracle.json').read_text(encoding='utf8'))['cards']
        cls.rows = {row['name']:row for row in registry()}
        cls.cases, cls.declarations = {}, {}
        for name, pin in family.pins()['sources'].items():
            csvid = pin['registry']['csvid']
            path, = (root/'data/cases/cards'/f'{csvid//100:03}').glob(f'{csvid:05d}-*.json')
            declaration = json.loads(path.read_text(encoding='utf8'))['recipes'][0]
            cls.declarations[name] = declaration
            cls.cases[name] = {case['_recipe']['variant']:case for case in expand(
                name,csvid,declaration,cls.oracle[name],cls.rows,cls.oracle)}

    def steps(self, name, mode, player):
        return self.cases[name][f'{mode}_{player}']['steps']

    def test_full_oracle_and_paired_aura_binding_are_pinned(self):
        self.assertEqual(sum(map(len,self.cases.values())),180)
        for name, declaration in self.declarations.items():
            record = self.oracle[name]
            family.spec(declaration,record)
            with self.assertRaises(ValueError):
                family.parameters(dict(record,oracle_text=record['oracle_text']+'\nDraw a card.'))
            wrong = dict(declaration['params'],bestow_cost='{9}{W}')
            with self.assertRaises(ValueError):
                family.spec(dict(declaration,params=wrong),record)
            alias = name+' Bestowed'
            wrong_rows = dict(self.rows,**{alias:dict(self.rows[alias],handler='card_dummy')})
            with self.assertRaises(ValueError):
                family.validate_witnesses(self.oracle,wrong_rows)

    def test_both_printed_prices_and_real_menu_paths(self):
        for name, declaration in self.declarations.items():
            normal = cost_from_oracle(self.oracle[name])
            bestow = cost_from_oracle({'mana_cost':declaration['params']['bestow_cost']})
            residual = [a-b for a,b in zip(bestow,normal)]
            for owner in (0,1):
                for mode, paid, menu, rest in [('regular_exact',normal,None,[0]*6),('regular_from_menu',bestow,1,residual),
                    ('no_host_regular',bestow,None,residual),('bestow_own',bestow,0,[0]*6)]:
                    steps = self.steps(name,mode,owner)
                    idx = steps.index(dict(op='cast',ref='source',value=1))
                    wallet = [0]*6
                    for s in steps[:idx]:
                        if s['op']=='set_mana' and s['player']==owner:
                            wallet[s['color']] = s['value']
                    self.assertEqual(wallet,paid)
                    self.assertEqual([s['value'] for s in steps[:idx] if s['op']=='choose_dialog'],[] if menu is None else [menu])
                    self.assertEqual(any(s['op']=='choose_target' for s in steps[:idx]),mode=='bestow_own')
                    for color, amount in enumerate(rest):
                        self.assertIn(dict(op='assert_mana',player=owner,color=color,value=amount),steps[idx:])

    def test_paid_aura_attaches_and_ordinary_source_has_own_creature_stats(self):
        for name, dec in self.declarations.items():
            base = self.rows[name]['csvid']
            for owner in (0,1):
                for mode, controller in [('bestow_own',owner),('bestow_foreign',1-owner)]:
                    case = self.cases[name][f'{mode}_{owner}']
                    self.assertEqual(next(x['player'] for x in case['setup'] if x['ref']=='host'),controller)
                    steps = case['steps']
                    ready = steps.index(dict(op='resolve'))
                    self.assertIn(dict(op='require_card',ref='source',property='csvid',value=base+1),steps[:ready])
                    self.assertIn(dict(op='can_target_creature',ref='probe',other='source',value=0),steps[ready:])
                    for prop, printed, bonus in zip(('power','toughness'),(6,5),dec['params']['bonus']):
                        self.assertIn(dict(op='assert_card',ref='host',property=prop,value=printed+bonus),steps[ready:])
                regular = self.steps(name,'regular_exact',owner)
                self.assertIn(dict(op='can_target_creature',ref='probe',other='source',value=1),regular)
                for prop, value in zip(('power','toughness'),dec['params']['creature']):
                    self.assertIn(dict(op='assert_card',ref='source',property=prop,value=value),regular)

    def test_unfunded_and_two_real_cancel_paths_leave_hand_intact(self):
        for name in self.cases:
            for owner in (0,1):
                for mode, choice in [('cancel_mode',2),('cancel_target',0),('unfunded',None)]:
                    steps = self.steps(name,mode,owner)
                    cast = steps.index(dict(op='cast',ref='source',value=0))
                    self.assertEqual([s['value'] for s in steps[:cast] if s['op']=='choose_dialog'],[] if choice is None else [choice])
                    self.assertEqual(any(s['op']=='choose_cancel_target' for s in steps[:cast]),mode=='cancel_target')
                    self.assertIn(dict(op='assert_zone_multiplicity',player=owner,zone='hand',index=self.rows[name]['csvid'],value=1),steps[cast:])

    def test_loss_pending_and_later_detachment_preserve_source_as_creature(self):
        for name in self.cases:
            for owner in (0,1):
                for mode in ('pending_host_lost','detach_bounce','detach_death'):
                    steps = self.steps(name,mode,owner)
                    if mode=='pending_host_lost':
                        self.assertIn(dict(op='announce_spell',ref='source'),steps)
                        self.assertIn(dict(op='cast_response',ref='helper',value=1),steps)
                        drained = steps.index(dict(op='resolve_response'))
                        self.assertEqual(steps[drained+3],dict(op='assert_stack',value=0))
                    self.assertIn(dict(op='assert_card',ref='source',property='csvid',value=self.rows[name]['csvid']),steps)
                    self.assertIn(dict(op='can_target_creature',ref='probe',other='source',value=1),steps)
                    zone = 'graveyard' if mode=='detach_death' else 'hand'
                    self.assertIn(dict(op='assert_zone_multiplicity',player=owner,zone=zone,index=6196,value=1),steps)

    def test_removing_the_aura_restores_host_and_regular_recast(self):
        for name in self.cases:
            for owner in (0,1):
                steps = self.steps(name,'aura_destroyed',owner)
                removed = steps.index(dict(op='cast',ref='helper',value=1))
                self.assertIn(dict(op='assert_card',ref='host',property='power',value=6),steps[removed:])
                self.assertIn(dict(op='assert_zone_multiplicity',player=owner,zone='graveyard',index=self.rows[name]['csvid'],value=1),steps[removed:])
                steps = self.steps(name,'aura_bounce_regular',owner)
                rebound = steps.index(dict(op='bind_hand',ref='source',player=owner,value=self.rows[name]['csvid']))
                self.assertIn(dict(op='cast',ref='source',value=1),steps[rebound:])
                self.assertFalse(any(s['op']=='choose_dialog' for s in steps[rebound:]))
                self.assertIn(dict(op='can_target_creature',ref='probe',other='source',value=1),steps[rebound:])

    def test_persistence_altered_form_counter_and_pending_replay(self):
        for name in self.cases:
            for owner in (0,1):
                for mode, spell in [('counter_bestowed','Negate'),('counter_regular','Essence Scatter')]:
                    case = self.cases[name][f'{mode}_{owner}']
                    self.assertEqual(next(x['name'] for x in case['setup'] if x['ref']=='helper'),spell)
                    self.assertIn(dict(op='cast_response',ref='helper',value=1),case['steps'])
                    self.assertIn(dict(op='assert_stack',value=2),case['steps'])
                steps = self.steps(name,'bestow_persistent',owner)
                end = steps.index(dict(op='upkeep_step',player=owner))
                self.assertIn(dict(op='can_target_creature',ref='probe',other='source',value=0),steps[end:])
                replay = self.steps(name,'pending_replay',owner)
                cut = replay.index(dict(op='capture'))
                self.assertIn(dict(op='assert_stack',value=1),replay[:cut])
                self.assertEqual(replay[cut+1:replay.index(dict(op='record_state'))],
                                 replay[replay.index(dict(op='restore'))+1:-1])


if __name__=='__main__':
    unittest.main()
