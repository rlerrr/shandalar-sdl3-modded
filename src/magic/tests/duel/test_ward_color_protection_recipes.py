"""Focused Oracle, protection scope, and attachment lifecycle guards."""
import json
from pathlib import Path
import unittest

from generate_cases import registry
from recipes import expand
import ward_color_protection_recipes as family


class WardColorProtectionTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        root = Path(__file__).parent
        cls.oracle = json.loads((root/'data/oracle.json').read_text(encoding='utf8'))['cards']
        cls.rows = {r['name']:r for r in registry()}
        cls.cases, cls.declarations = {}, {}
        for name, pin in family.pins()['sources'].items():
            csvid = pin['registry']['csvid']
            path, = (root/'data/cases/cards'/f'{csvid//100:03}').glob(f'{csvid:05d}-*.json')
            declaration = json.loads(path.read_text(encoding='utf8'))['recipes'][0]
            cls.declarations[name] = declaration
            cls.cases[name] = {c['_recipe']['variant']:c for c in expand(
                name,csvid,declaration,cls.oracle[name],cls.rows,cls.oracle)}

    def steps(self, name, mode, owner):
        return self.cases[name][f'{mode}_{owner}']['steps']

    def test_whole_printed_exemption_is_required(self):
        self.assertEqual(sum(map(len,self.cases.values())),154)
        for name, declaration in self.declarations.items():
            record = self.oracle[name]
            family.spec(declaration,record)
            with self.assertRaises(ValueError):
                family.parameters(dict(record,oracle_text=record['oracle_text'].replace(" This effect doesn't remove this Aura.",'')))
            with self.assertRaises(ValueError):
                family.spec(dict(declaration,params=dict(declaration['params'],exempt_self=1)),record)

    def test_exact_paid_attachment_on_either_board_and_failed_controls(self):
        for name in self.cases:
            for owner in (0,1):
                for mode, host in [('own_attachment',owner),('foreign_attachment',1-owner)]:
                    case = self.cases[name][f'{mode}_{owner}']
                    steps = case['steps']
                    cast = steps.index(dict(op='cast',ref='source',value=1))
                    self.assertEqual(next(x['player'] for x in case['setup'] if x['ref']=='host'),host)
                    self.assertIn(dict(op='set_mana',player=owner,color=5,value=1),steps[:cast])
                    self.assertIn(dict(op='choose_target',player=owner,ref='host'),steps[:cast])
                    self.assertIn(dict(op='assert_mana',player=owner,color=5,value=0),steps[cast:])
                for mode in ('no_creature','unfunded'):
                    steps = self.steps(name,mode,owner)
                    self.assertIn(dict(op='cast',ref='source',value=0),steps)
                    self.assertFalse(any(s['op']=='choose_target' for s in steps))

    def test_targeting_denies_named_color_from_either_side_but_legal_spell_resolves(self):
        for name in self.cases:
            steps = self.steps(name,'colored_targeting',0)
            for ref in ('own_probe','probe'):
                self.assertIn(dict(op='can_target_creature',ref=ref,other='host',value=0),steps)
                self.assertIn(dict(op='can_target_creature',ref=ref,other='witness',value=1),steps)
            self.assertIn(dict(op='cast',ref='other_probe',value=1),steps)
            if name=='Blue Ward':
                self.assertIn(dict(op='assert_card',ref='host',property='power',value=5),steps)
            else:
                self.assertIn(dict(op='assert_card',ref='host',property='tapped',value=1),steps)

    def test_ordinary_aura_falls_but_ward_survives_its_own_grant(self):
        for name, declaration in self.declarations.items():
            color = declaration['params']['color']
            for owner in (0,1):
                steps = self.steps(name,'same_color_aura',owner)
                self.assertLess(steps.index(dict(op='cast',ref='ordinary_aura',value=1)),
                                steps.index(dict(op='cast',ref='source',value=1)))
                self.assertIn(dict(op='assert_card',ref='source',property='in_play',value=1),steps)
                csvid = self.rows[family.AURAS[color]]['csvid']
                self.assertIn(dict(op='assert_zone_multiplicity',player=owner,zone='graveyard',index=csvid,value=1),steps)
                if color!='white':
                    steps = self.steps(name,'recolored_exemption',owner)
                    for lens, target in [('ward_lens','source'),('ordinary_lens','ordinary_aura')]:
                        activate = steps.index(dict(op='activate',ref=lens,value=1))
                        self.assertEqual(steps[activate-1],dict(op='choose_target',player=owner,ref=target))
                        self.assertIn(dict(op='choose_color',player=owner,value=family.COLORS[color]),steps[activate:])
                    self.assertIn(dict(op='assert_card',ref='ordinary_aura',property='in_play',value=0),steps)
                else:
                    steps = self.steps(name,'independent_protection',owner)
                    equip = steps.index(dict(op='activate',ref='sword',value=1))
                    self.assertIn(dict(op='assert_card',ref='source',property='in_play',value=1),steps[:equip])
                    self.assertIn(dict(op='assert_card',ref='source',property='in_play',value=0),steps[equip:])
                    self.assertIn(dict(op='assert_card',ref='host',property='power',value=4),steps[equip:])

    def test_real_blocking_and_prevented_damage(self):
        for name in self.cases:
            for owner in (0,1):
                for mode, allowed, attacker, blocker in [('matching_block_denied',0,'host','combatant'),
                    ('other_color_block',1,'host','combatant'),('colored_combat_damage',1,'combatant','host')]:
                    steps = self.steps(name,mode,owner)
                    self.assertIn(dict(op='attack',ref=attacker,value=1),steps)
                    self.assertIn(dict(op='block',ref=blocker,other=attacker,value=allowed),steps)
                    self.assertLess(steps.index(dict(op='combat_damage')),steps.index(dict(op='end_combat')))
                    damage = 1 if mode=='other_color_block' else 0
                    self.assertIn(dict(op='assert_card',ref='host',property='damage',value=damage),steps)

    def test_bounce_recast_changes_which_creature_is_protected(self):
        for name in self.cases:
            steps = self.steps(name,'ward_bounce_recast',1)
            recast = steps.index(dict(op='choose_target',player=1,ref='witness'))
            self.assertIn(dict(op='bind_hand',ref='source',player=1,value=self.rows[name]['csvid']),steps[:recast])
            self.assertIn(dict(op='can_target_creature',ref='probe',other='host',value=1),steps[recast:])
            self.assertIn(dict(op='can_target_creature',ref='probe',other='witness',value=0),steps[recast:])

    def test_replay_starts_with_paid_pending_aura_and_rechecks_persistence(self):
        for name in self.cases:
            for owner in (0,1):
                steps = self.steps(name,'pending_replay',owner)
                start = steps.index(dict(op='capture'))
                end = steps.index(dict(op='record_state'))
                restore = steps.index(dict(op='restore'))
                self.assertIn(dict(op='cast',ref='source',value=1),steps[:start])
                self.assertIn(dict(op='assert_stack',value=1),steps[:start])
                self.assertEqual(steps[start+1],dict(op='resolve'))
                self.assertEqual(steps[start+1:end],steps[restore+1:-1])
                cleanup = steps.index(dict(op='cleanup'))
                self.assertIn(dict(op='can_target_creature',ref='probe',other='host',value=0),steps[cleanup:end])


if __name__ == '__main__':
    unittest.main()
