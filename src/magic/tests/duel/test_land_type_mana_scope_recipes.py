"""Focused guards for Oracle scope, payment, and subtype receipts."""
import copy
import json
from pathlib import Path
import unittest

from generate_cases import registry
from recipes import expand
import land_type_mana_scope_recipes as family


class LandTypeManaScopeTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        root = Path(__file__).parent
        cls.oracle = json.loads((root/'data/oracle.json').read_text(encoding='utf8'))['cards']
        cls.rows = {r['name']:r for r in registry()}
        cls.cases, cls.declarations = {}, {}
        for name, pin in family.pins()['sources'].items():
            csvid = pin['registry']['csvid']
            path = next((root/'data/cases/cards'/f'{csvid//100:03d}').glob(f'{csvid:05d}-*.json'))
            declaration = json.loads(path.read_text(encoding='utf8'))['recipes'][0]
            cls.declarations[name] = declaration
            cls.cases[name] = {c['_recipe']['variant']:c for c in expand(
                name, csvid, declaration, cls.oracle[name], cls.rows, cls.oracle)}

    def case(self, name, variant):
        return self.cases[name][variant]

    def steps(self, name, variant):
        return self.case(name, variant)['steps']

    def test_whole_clause_scope_and_type_change_are_pinned(self):
        self.assertEqual(sum(map(len,self.cases.values())),180)
        for name, declaration in self.declarations.items():
            oracle = self.oracle[name]
            family.spec(declaration,oracle)
            bad = copy.deepcopy(declaration)
            bad['params']['scope'] = 'anything'
            with self.assertRaises(ValueError):
                family.spec(bad,oracle)
            bad = dict(oracle,oracle_text=oracle['oracle_text']+'\nDraw a card.')
            with self.assertRaises(ValueError):
                family.oracle_model(bad)
        bad_rows = copy.deepcopy(self.rows)
        bad_rows['Matca Rioters']['handler'] = 'card_dummy'
        with self.assertRaises(ValueError):
            family.validate_witnesses(self.oracle,bad_rows)

    def test_real_domain_separates_global_and_owner_only_effects(self):
        counts = {'Blanket of Night':(2,2), 'Blood Moon':(2,2), 'Magus of the Moon':(2,2),
                  'Prismatic Omen':(5,1), "Nylea's Presence":(5,1), 'Urborg, Tomb of Yawgmoth':(2,2)}
        for name, (mine, theirs) in counts.items():
            for owner in (0,1):
                steps = self.steps(name,f'domain_{owner}')
                for ref, expected in [('owner_domain',mine),('enemy_domain',theirs)]:
                    powers = [s['value'] for s in steps if s['op']=='assert_card' and s.get('ref')==ref and s.get('property')=='power']
                    self.assertEqual(powers[0],1)
                    self.assertEqual(powers[-1],expected)

    def test_moon_replaces_mana_without_changing_unaffected_basics(self):
        for name in ('Blood Moon','Magus of the Moon'):
            for owner in (0,1):
                for mode, ref, actor, expected in [('owner_nonbasic','owner_nonbasic',owner,4),
                                                    ('enemy_nonbasic','enemy_nonbasic',1-owner,4),
                                                    ('owner_basic','owner_basic',owner,3),
                                                    ('enemy_basic','enemy_basic',1-owner,5)]:
                    steps = self.steps(name,f'{mode}_{owner}')
                    activate = steps.index(dict(op='activate',ref=ref,value=1))
                    self.assertFalse(any(s['op']=='choose_color' for s in steps))
                    for color in range(6):
                        self.assertIn(dict(op='assert_mana',player=actor,color=color,value=int(color==expected)),steps[activate:])
                artifact = self.steps(name,f'preserve_artifact_type_{owner}')
                self.assertIn(dict(op='choose_target',player=1-owner,ref='owner_nonbasic'),artifact)
                self.assertIn(dict(op='assert_zone_multiplicity',player=owner,zone='graveyard',index=1216,value=1),artifact)

    def test_named_subtype_and_existing_mana_are_available_through_real_actions(self):
        for name in ('Blanket of Night','Urborg, Tomb of Yawgmoth'):
            for owner in (0,1):
                for mode, color in [('owner_nonbasic',1),('original_mana',2)]:
                    steps = self.steps(name,f'{mode}_{owner}')
                    use = steps.index(dict(op='activate',ref='owner_nonbasic',value=1))
                    self.assertEqual(steps[use-1],dict(op='choose_color',player=owner,value=color))
                    self.assertIn(dict(op='assert_mana',player=owner,color=color,value=1),steps[use:])
                unaffected = self.steps(name,f'nonland_unchanged_{owner}')
                self.assertIn(dict(op='assert_mana',player=owner,color=0,value=2),unaffected)

    def test_urborg_is_put_into_play_and_itself_adds_black(self):
        for owner in (0,1):
            steps = self.steps('Urborg, Tomb of Yawgmoth',f'source_self_mana_{owner}')
            found = steps.index(dict(op='choose_zone_card',player=owner,value=8870))
            use = steps.index(dict(op='activate',ref='source',value=1))
            self.assertIn(dict(op='activate',ref='source_atlas',value=1),steps[:found])
            self.assertLess(found,use)
            self.assertIn(dict(op='assert_mana',player=owner,color=1,value=1),steps[use:])

    def test_aura_draw_scope_and_foreign_target_remain_oracle_correct(self):
        name = "Nylea's Presence"
        for owner in (0,1):
            for symbol,color in [('b',1),('u',2),('g',3),('r',4),('w',5)]:
                steps = self.steps(name,f'color_{symbol}_{owner}')
                self.assertIn(dict(op='activate_granted_mana',ref='owner_nonbasic',other='source',value=1),steps)
                self.assertIn(dict(op='assert_mana',player=owner,color=color,value=1),steps)
                self.assertIn(dict(op='assert_zone_multiplicity',player=owner,zone='hand',index=11586,value=1),steps)
            steps = self.steps(name,f'foreign_target_{owner}')
            self.assertIn(dict(op='require_can_cast',ref='source',value=1),steps)
            self.assertIn(dict(op='choose_target',player=owner,ref='enemy_nonbasic'),steps)
            self.assertIn(dict(op='assert_card',ref='enemy_domain',property='power',value=5),steps)
            setup = self.case(name,f'foreign_target_{owner}')['setup']
            self.assertFalse(any(s['player']==owner and 'Land' in self.oracle[s['name']]['type_line'] and s['zone']=='battlefield' for s in setup))

    def test_real_later_entry_and_removal_restore_correct_mana_source(self):
        for name in self.cases:
            for owner in (0,1):
                steps = self.steps(name,f'later_enemy_entry_{owner}')
                entry = steps.index(dict(op='choose_zone_card',player=1-owner,value=1217))
                self.assertIn(dict(op='activate',ref='entrant_atlas',value=1),steps[:entry])
                self.assertIn(dict(op='activate',ref='entrant',value=1),steps[entry:])
                steps = self.steps(name,f'restore_after_removal_{owner}')
                removed = steps.index(dict(op='assert_card',ref='source',property='in_play',value=0))
                self.assertIn(dict(op='activate',ref='owner_nonbasic',value=1),steps[removed:])
                self.assertIn(dict(op='assert_mana',player=owner,color=2,value=1),steps[removed:])
                self.assertIn(dict(op='assert_card',ref='owner_domain',property='power',value=1),steps[removed:])

    def test_pending_replay_captures_real_unresolved_spell_or_ability(self):
        for name in self.cases:
            for owner in (0,1):
                steps = self.steps(name,f'pending_replay_{owner}')
                capture = steps.index(dict(op='capture'))
                action = dict(op='activate',ref='source_atlas',value=1) if name.startswith('Urborg') else dict(op='cast',ref='source',value=1)
                self.assertIn(action,steps[:capture])
                self.assertIn(dict(op='assert_stack',value=1),steps[:capture])
                self.assertEqual(steps[capture+1:steps.index(dict(op='record_state'))],
                                 steps[steps.index(dict(op='restore'))+1:-1])
                self.assertEqual(steps[-1],dict(op='assert_recorded_state'))


if __name__ == '__main__':
    unittest.main()
