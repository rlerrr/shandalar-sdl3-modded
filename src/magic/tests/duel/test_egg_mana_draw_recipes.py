import json
import unittest
from pathlib import Path

import egg_mana_draw_recipes as family
from generate_cases import registry
from recipes import expand


class EggManaDrawTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.duel = Path(__file__).parent
        cls.oracle = json.loads((cls.duel/'data/oracle.json').read_text(encoding='utf8'))['cards']
        cls.rows = {r['name']:r for r in registry()}
        cls.sources = family.profiles()['sources']
        cls.fixed_sources = [name for name in cls.sources if 'output' in family.parameters(cls.oracle[name])]

    def declaration(self,name):
        csvid = self.rows[name]['csvid']
        path, = (self.duel/f'data/cases/cards/{csvid//100:03}').glob(f'{csvid:05}-*.json')
        declaration, = json.loads(path.read_text(encoding='utf8'))['recipes']
        return declaration

    def case(self,name,mode,p=0):
        d = dict(self.declaration(name),variants=[f'{mode}_{p}'])
        return expand(name,self.rows[name]['csvid'],d,self.oracle[name],self.rows,self.oracle)[0]

    def test_full_printed_one_mana_artifact_and_distinct_output_per_source(self):
        family.validate_witnesses(self.oracle,self.rows)
        expected = {'Darkwater Egg':(1,2),'Mossfire Egg':(3,4),'Shadowblood Egg':(1,4),
                    'Skycloud Egg':(2,5),'Sungrass Egg':(3,5)}
        for name, pair in expected.items():
            record = self.oracle[name]
            params = family.parameters(record)
            self.assertEqual(params['output'],[int(c in pair) for c in range(6)])
            self.assertEqual(params['activation_generic'],2)
            self.assertEqual(params['timing'],'instant')
            self.assertTrue(params['tap'] and params['sacrifice'] and params['draw']==1)
            with self.assertRaises(ValueError): family.parameters(dict(record,oracle_text=record['oracle_text'].removesuffix(' (Activate only as an instant.)')))
        with self.assertRaises(ValueError): family.parameters(self.oracle['Chromatic Star'])

    def test_exact_both_controller_matrix_never_uses_synthetic_gameplay(self):
        total = 0
        for name in self.sources:
            d = self.declaration(name)
            self.assertEqual(d['variants'],family.variants(family.spec(d,self.oracle[name])))
            cases = expand(name,self.rows[name]['csvid'],d,self.oracle[name],self.rows,self.oracle)
            total += len(cases)
            self.assertFalse(any(s['op'] in ('set_life','destroy','set_counter','move_card','dispatch_gfp')
                                 for c in cases for s in c['steps']))
        self.assertEqual(total,171)

    def test_two_mana_and_sacrifice_are_paid_before_delayed_draw_and_mana(self):
        for name in self.fixed_sources:
            for p in (0,1):
                steps = self.case(name,'exact_two',p)['steps']
                c = self.rows[name]['csvid']
                at = next(i for i,s in enumerate(steps) if s['op']=='activate')
                resolve = next(i for i,s in enumerate(steps) if s['op']=='resolve')
                self.assertEqual(steps[at],{'op':'activate','ref':'source','value':1})
                self.assertIn({'op':'set_mana','player':p,'color':0,'value':2},steps[:at])
                before = steps[at:resolve]
                self.assertIn({'op':'assert_stack','value':1},before)
                self.assertIn({'op':'assert_zone_multiplicity','player':p,'zone':'graveyard','index':c,'value':1},before)
                self.assertIn({'op':'assert_zone','player':p,'zone':'library','value':7},before)
                self.assertIn({'op':'assert_zone','player':p,'zone':'library','value':6},steps[resolve:])
                for color,amount in enumerate(family.parameters(self.oracle[name])['output']):
                    self.assertIn({'op':'assert_mana','player':p,'color':color,'value':0},before)
                    self.assertIn({'op':'assert_mana','player':p,'color':color,'value':amount},steps[resolve:])

    def test_off_turn_actual_pending_spell_and_own_cast_then_activation(self):
        for p in (0,1):
            response = self.case('Darkwater Egg','respond_to_spell',p)['steps']
            self.assertIn({'op':'turn','player':1-p,'value':0},response)
            self.assertIn({'op':'assert_stack','value':2},response)
            parent = response.index({'op':'announce_spell','ref':'parent'})
            egg = response.index({'op':'activate','ref':'source','value':1})
            self.assertLess(parent,egg)
            fresh = self.case('Skycloud Egg','cast_then_activate',p)['steps']
            self.assertIn({'op':'set_mana','player':p,'color':0,'value':1},fresh)
            self.assertLess(fresh.index({'op':'cast','ref':'source','value':1}),fresh.index({'op':'activate','ref':'source','value':1}))

    def test_tapped_unfunded_and_foreign_owner_receipts(self):
        name = 'Mossfire Egg'
        csvid = self.rows[name]['csvid']
        for p in (0,1):
            for mode in ('tapped','unfunded'):
                c = self.case(name,mode,p)
                self.assertIn({'op':'can_activate','ref':'source','value':0},c['steps'])
                self.assertIn({'op':'activate','ref':'source','value':0},c['steps'])
                self.assertIn({'op':'assert_zone_multiplicity','player':p,'zone':'battlefield','index':csvid,'value':1},c['steps'])
                if mode=='tapped': self.assertIn('tapped',next(x for x in c['setup'] if x['ref']=='source')['state'])
            moved = self.case(name,'foreign_owned',p)['steps']
            self.assertIn({'op':'assert_zone_multiplicity','player':1-p,'zone':'graveyard','index':csvid,'value':1},moved)
            self.assertIn({'op':'assert_zone','player':p,'zone':'library','value':6},moved)

    def test_two_pending_eggs_replay_and_draw_boundary(self):
        name = 'Sungrass Egg'
        for p in (0,1):
            double = self.case(name,'two_eggs',p)['steps']
            self.assertIn({'op':'activate','ref':'second','value':1},double)
            self.assertIn({'op':'assert_stack','value':2},double)
            self.assertIn({'op':'assert_zone','player':p,'zone':'library','value':5},double)
            for mode,life in [('last_card',20),('empty_library',-99)]:
                c = self.case(name,mode,p)['steps']
                self.assertIn({'op':'assert_zone','player':p,'zone':'library','value':0},c)
                self.assertIn({'op':'assert_life','player':p,'value':life},c)
            replay = self.case(name,'pending_snapshot',p)['steps']
            self.assertLess(replay.index({'op':'activate','ref':'source','value':1}),replay.index({'op':'capture'}))
            self.assertEqual(replay.count({'op':'resolve'}),2)
            self.assertEqual(replay[-1],{'op':'assert_recorded_state'})

    def test_real_cast_payment_attempt_can_fall_back_to_basic(self):
        for name in self.fixed_sources:
            c = self.case(name,'payment_window_probe')
            steps = c['steps']
            cast = steps.index({'op':'cast','ref':'spender','value':1})
            self.assertEqual(steps[cast-2:cast],[{'op':'choose_mana_source','ref':'source'},
                                                {'op':'choose_mana_source','ref':'ordinary_mana_source'}])
            self.assertEqual(steps[cast+1],{'op':'assert_unused_choices','value':0})
            self.assertNotIn({'op':'activate','ref':'source','value':1},steps)
            self.assertIn({'op':'assert_card','ref':'source','property':'tapped','value':0},steps[cast:])
            self.assertIn({'op':'assert_card','ref':'ordinary_mana_source','property':'tapped','value':1},steps[cast:])
            ordinary = self.case(name,'ordinary_spell_payment')['steps']
            self.assertNotIn({'op':'choose_mana_source','ref':'source'},ordinary)

    def test_sphere_pays_one_and_keeps_color_and_draw_pending(self):
        name = 'Chromatic Sphere'
        self.assertEqual(family.parameters(self.oracle[name])['output_colors'],[1,2,3,4,5])
        for player in (0,1):
            for symbol,color in family.COLORS.items():
                steps = self.case(name,'color_'+symbol,player)['steps']
                activate = steps.index({'op':'activate','ref':'source','value':1})
                resolve = steps.index({'op':'resolve'})
                self.assertIn({'op':'choose_color','player':player,'value':color},steps[:activate])
                self.assertIn({'op':'assert_stack','value':1},steps[activate:resolve])
                self.assertIn({'op':'assert_mana','player':player,'color':0,'value':3},steps[activate:resolve])
                self.assertIn({'op':'assert_zone','player':player,'zone':'library','value':7},steps[activate:resolve])
                self.assertIn({'op':'assert_mana','player':player,'color':color,'value':1},steps[resolve:])
                self.assertIn({'op':'assert_zone','player':player,'zone':'library','value':6},steps[resolve:])
            exact = self.case(name,'exact_one',player)['steps']
            self.assertIn({'op':'set_mana','player':player,'color':0,'value':1},exact)
            unfunded = self.case(name,'unfunded',player)['steps']
            self.assertIn({'op':'can_activate','ref':'source','value':0},unfunded)
            self.assertNotIn({'op':'activate','ref':'source','value':0},unfunded)
            self.assertFalse(any(s['op']=='choose_color' for s in unfunded))


if __name__=='__main__':
    unittest.main()
