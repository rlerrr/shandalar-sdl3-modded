"""Focused guards for the Vivid land clauses and actual activation receipts."""
import json
from pathlib import Path
import unittest

from generate_cases import registry
from recipes import expand
import vivid_land_mana_recipes as family


class VividLandManaTests(unittest.TestCase):
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

    def steps(self, name, mode, owner):
        return self.cases[name][f'{mode}_{owner}']['steps']

    def test_full_oracle_identity_and_strict_parameters(self):
        self.assertEqual(sum(map(len,self.cases.values())),140)
        for name, declaration in self.declarations.items():
            oracle = self.oracle[name]
            family.spec(declaration,oracle)
            wrong = dict(declaration['params'],entry_charge=1)
            with self.assertRaises(ValueError):
                family.spec(dict(declaration,params=wrong),oracle)
            wrong = dict(oracle,oracle_text=oracle['oracle_text']+'\nDraw a card.')
            with self.assertRaises(ValueError):
                family.parameters(wrong)
            with self.assertRaises(ValueError):
                family.spec(dict(declaration,variants=['entry_tapped_0','entry_tapped_0']),oracle)
        bad = dict(self.rows, **{'Walking Atlas':dict(self.rows['Walking Atlas'],handler='card_dummy')})
        with self.assertRaises(ValueError):
            family.validate_witnesses(self.oracle,bad)

    def test_real_atlas_entry_requires_a_tapped_source_and_two_charges(self):
        for name in self.cases:
            csvid = self.rows[name]['csvid']
            for owner in (0,1):
                case = self.cases[name][f'entry_tapped_{owner}']
                self.assertEqual(next(x for x in case['setup'] if x['ref']=='source')['zone'],'hand')
                steps = case['steps']
                atlas = steps.index(dict(op='activate',ref='atlas',value=1))
                choice = steps.index(dict(op='choose_zone_card',player=owner,value=csvid))
                entry = steps.index(dict(op='require_card',ref='source',property='in_play',value=1))
                self.assertLess(atlas,choice)
                self.assertLess(choice,entry)
                self.assertIn(dict(op='assert_stack',value=1),steps[atlas:choice])
                self.assertIn(dict(op='assert_counter',ref='source',counter_kind='charge',value=2),steps[entry:])
                self.assertIn(dict(op='require_card',ref='source',property='tapped',value=1),steps[entry:])
                self.assertIn(dict(op='activate',ref='source',value=0),steps[entry:])

    def test_every_off_color_is_named_and_pays_a_charge(self):
        for name, declaration in self.declarations.items():
            fixed = declaration['params']['fixed_color']
            for owner in (0,1):
                for color, number in family.COLORS.items():
                    mode = 'fixed_charged' if color==fixed else 'alternate_'+color.lower()
                    steps = self.steps(name,mode,owner)
                    activation = steps.index(dict(op='activate',ref='source',value=1))
                    self.assertEqual(steps[activation-1],dict(op='choose_color',player=owner,value=number))
                    self.assertIn(dict(op='assert_mana',player=owner,color=number,value=1),steps[activation:])
                    self.assertEqual(steps[activation+3]['value'],2 if color==fixed else 1)
                    self.assertIn(dict(op='untap_step',player=owner),steps[:activation])

    def test_interleaved_free_activations_and_depletion(self):
        for name, declaration in self.declarations.items():
            fixed = family.COLORS[declaration['params']['fixed_color']]
            for owner in (0,1):
                for mode, counts, fixed_uses in [('deplete',[1,0,0],1),('free_interleaved',[2,1,1,0,0],3)]:
                    steps = self.steps(name,mode,owner)
                    activates = [i for i,s in enumerate(steps) if s==dict(op='activate',ref='source',value=1)]
                    self.assertEqual([steps[i+3]['value'] for i in activates],counts)
                    self.assertTrue(all(steps[i-1]['op']=='choose_color' for i in activates[:-1]))
                    self.assertEqual(steps[activates[-1]-1]['op'],'require_can_activate')
                    self.assertIn(dict(op='assert_mana',player=owner,color=fixed,value=fixed_uses),steps[activates[-1]:])
                    self.assertEqual(sum(s['op']=='cast' and s['ref'].startswith('twiddle_') for s in steps),len(counts)-1)

    def test_generated_blue_is_spent_without_direct_funding(self):
        for name in self.cases:
            for owner in (0,1):
                steps = self.steps(name,'spend_blue',owner)
                activate = steps.index(dict(op='activate',ref='source',value=1))
                payment = steps.index(dict(op='cast',ref='blue_payment',value=1))
                self.assertFalse(any(s['op']=='set_mana' and s['value'] for s in steps))
                self.assertIn(dict(op='assert_mana',player=owner,color=2,value=1),steps[activate:payment])
                self.assertIn(dict(op='assert_mana',player=owner,color=2,value=0),steps[payment:])
                self.assertIn(dict(op='assert_card',ref='source',property='tapped',value=0),steps[payment:])

    def test_bounce_reentry_gets_new_charges_and_tappedness(self):
        for name in self.cases:
            for owner in (0,1):
                steps = self.steps(name,'reentry_reset',owner)
                csvid = self.rows[name]['csvid']
                bind = steps.index(dict(op='bind_hand',player=owner,value=csvid,ref='source'))
                self.assertIn(dict(op='cast',ref='bounce',value=1),steps[:bind])
                self.assertIn(dict(op='assert_counter',ref='source',counter_kind='charge',value=1),steps[:bind])
                self.assertIn(dict(op='activate',ref='atlas',value=1),steps[bind:])
                self.assertIn(dict(op='choose_zone_card',player=owner,value=csvid),steps[bind:])
                self.assertIn(dict(op='assert_counter',ref='source',counter_kind='charge',value=2),steps[bind:])
                self.assertIn(dict(op='activate',ref='source',value=0),steps[bind:])

    def test_replay_uses_pending_entry_or_one_charge_state(self):
        for name in self.cases:
            for owner in (0,1):
                for mode in ('pending_entry_replay','one_charge_replay'):
                    steps = self.steps(name,mode,owner)
                    cut = steps.index(dict(op='capture'))
                    restore = steps.index(dict(op='restore'))
                    recorded = steps.index(dict(op='record_state'))
                    self.assertEqual(steps[cut+1:recorded],steps[restore+1:-1])
                    if mode=='pending_entry_replay':
                        self.assertIn(dict(op='assert_stack',value=1),steps[:cut])
                        self.assertEqual(steps[cut+1]['op'],'choose_zone_card')
                    else:
                        self.assertIn(dict(op='assert_counter',ref='source',counter_kind='charge',value=1),steps[:cut])
                        self.assertIn(dict(op='assert_counter',ref='source',counter_kind='charge',value=0),steps[cut:recorded])


if __name__ == '__main__':
    unittest.main()
