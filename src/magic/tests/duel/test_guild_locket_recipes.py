"""Check real mana payment and the complete Oracle sacrifice/draw sequence."""
import json
import unittest
from pathlib import Path

import guild_locket_recipes as model
from generate_cases import OPS, registry


class GuildLocketTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.oracle=json.loads((Path(__file__).parent/'data/oracle.json').read_text(encoding='utf8'))['cards']
        cls.rows={row['name']:row for row in registry()}

    def spec(self,name):
        record=self.oracle[name]
        d=dict(family=model.FAMILY,version=2,oracle_text=record['oracle_text'],
               params=dict(schema=1,mechanism=model.mechanism(record)))
        return d,model.spec(d,record)

    def case(self,name,mode,owner=0):
        d,p=self.spec(name)
        return model.expand(name,p['csvid'],d,p,[f'{mode}_{owner}'],self.rows,self.oracle)[0]

    def pool(self,steps,owner,op='assert_mana'):
        return [s['value'] for s in steps if s['op']==op and s['player']==owner][-6:]

    def test_pilots_pin_both_clauses_printed_cost_and_binding(self):
        expected={'Boros Locket':(19299,'RW'),'Dimir Locket':(19335,'UB'),'Golgari Locket':(19381,'BG'),
                  'Azorius Locket':(19547,'WU'),'Simic Locket':(19740,'GU'),
                  'Izzet Locket':(19409,'UR'),'Selesnya Locket':(19481,'GW'),'Gruul Locket':(19645,'RG'),
                  'Orzhov Locket':(19683,'WB'),'Rakdos Locket':(19700,'BR')}
        self.assertEqual(set(model.pins()['sources']),set(expected))
        for name,(csvid,colors) in expected.items():
            _,p=self.spec(name)
            self.assertEqual((p['csvid'],p['colors'],p['quantity'],p['cards_drawn'],p['cast_mana']),
                             (csvid,list(colors),4,2,[3,0,0,0,0,0]))
            for changed in (self.oracle[name]['oracle_text']+'\nDraw a card.',
                            self.oracle[name]['oracle_text'].replace('Sacrifice this artifact: ','',1)):
                with self.assertRaises(ValueError):
                    model.mechanism(dict(self.oracle[name],oracle_text=changed))
        model.validate_witnesses(self.oracle,self.rows)

    def test_both_real_output_colors_pay_an_exactly_colored_spell(self):
        for name in model.pins()['sources']:
            _,p=self.spec(name)
            for owner in (0,1):
                for mode,color in zip(('mana_first','mana_second'),p['colors']):
                    case=self.case(name,mode,owner)
                    spender=next(c for c in case['setup'] if c['ref']=='spender')
                    self.assertEqual(self.oracle[spender['name']]['mana_cost'],'{'+color+'}')
                    steps=case['steps']
                    cast=steps.index(dict(op='cast',ref='spender',value=1))
                    self.assertEqual(self.pool(steps[:cast],owner,'set_mana'),[0]*6)
                    self.assertEqual(sum(s['op']=='choose_mana_source' for s in steps[:cast]),int(owner==0))
                    self.assertIn(dict(op='assert_card',ref='source',property='tapped',value=1),steps[cast:])
                    self.assertEqual(self.pool(steps,owner),[0]*6)

    def test_all_four_symbol_allocations_include_exact_white_routes(self):
        for name in model.pins()['sources']:
            _,p=self.spec(name)
            a,b=[model.COLORS.index(c) for c in p['colors']]
            for owner in (0,1):
                for other in range(5):
                    steps=self.case(name,f'draw_{4-other}_{other}',owner)['steps']
                    activate=steps.index(dict(op='activate',ref='source',value=1))
                    expected=[0]*6;expected[a],expected[b]=4-other,other
                    self.assertEqual(self.pool(steps[:activate],owner,'set_mana'),expected)
                    self.assertIn(dict(op='choose_activation_mode',ref='source',player=owner,mode_label='Draw 2 cards',
                                       fallback_color=a,allow_auto_nonmana=True),steps[:activate])
                    self.assertEqual(steps[activate+1],dict(op='require_activation_mode',value=1))
                    self.assertEqual(self.pool(steps,owner),[0]*6)

    def test_sacrifice_is_paid_before_real_draw_and_reserve_stays_unspent(self):
        for owner in (0,1):
            name='Azorius Locket'
            steps=self.case(name,'draw_reserve',owner)['steps']
            activate=steps.index(dict(op='activate',ref='source',value=1))
            resolve=steps.index(dict(op='resolve'),activate)
            self.assertIn(dict(op='assert_zone_multiplicity',player=owner,zone='graveyard',index=19547,value=1),steps[activate:resolve])
            self.assertIn(dict(op='assert_zone',player=owner,zone='hand',value=1),steps[activate:resolve])
            self.assertIn(dict(op='assert_zone',player=owner,zone='hand',value=3),steps[resolve:])
            self.assertIn(dict(op='assert_zone_card',player=owner,zone='library',index=0,value=self.rows['Mountain']['csvid']),steps[resolve:])
            for n in ('Forest','Island'):
                self.assertIn(dict(op='assert_zone_multiplicity',player=owner,zone='hand',index=self.rows[n]['csvid'],value=1),steps[resolve:])
            self.assertEqual(self.pool(steps,owner),[0,0,0,0,0,1])

    def test_unavailable_draw_does_not_disallow_mana_or_allow_sacrifice(self):
        for owner in (0,1):
            for mode in ('draw_no_mana','draw_one_short','draw_wrong_color'):
                steps=self.case('Boros Locket',mode,owner)['steps']
                self.assertIn(dict(op='activate',ref='source',value=1),steps)
                if owner==1:
                    self.assertFalse(any(s['op']=='choose_activation_mode' for s in steps))
                    action=steps.index(dict(op='activate',ref='source',value=1))
                    self.assertEqual(steps[action-1:action+2], [dict(op='choose_color',player=1,value=4),
                                     dict(op='activate',ref='source',value=1),dict(op='assert_unused_choices',value=0)])
                else:
                    self.assertTrue(any(s.get('mode_label')=='Get mana' for s in steps))
                    self.assertIn(dict(op='require_activation_mode',value=0),steps)
                self.assertNotIn(dict(op='resolve'),steps)
                self.assertIn(dict(op='assert_card',ref='source',property='tapped',value=1),steps)
            denied=self.case('Boros Locket','tapped_denied',owner)['steps']
            self.assertIn(dict(op='activate',ref='source',value=0),denied)
            wrong=self.case('Boros Locket','wrong_mana_output',owner)['steps']
            self.assertIn(dict(op='cast',ref='spender',value=0),wrong)
        choose=self.case('Boros Locket','take_mana_with_draw')['steps']
        self.assertTrue(any(s.get('mode_label')=='Get mana' for s in choose))
        self.assertIn(dict(op='choose_color',player=0,value=4),choose)

    def test_conditional_payment_targets_the_exact_vector_after_the_named_mode(self):
        for name in model.pins()['sources']:
            _,p=self.spec(name)
            a,b=[model.COLORS.index(c) for c in p['colors']]
            for owner in (0,1):
                for n in range(5):
                    steps=self.case(name,f'draw_{4-n}_{n}',owner)['steps']
                    payments=[s for s in steps if s['op']=='choose_mode_payment']
                    if owner==0:
                        vector=((4-n) << (4*a)) | (n << (4*b))
                        self.assertEqual(payments,[dict(op='choose_mode_payment',player=owner,value=vector)])
                        at=steps.index(payments[0])
                        self.assertEqual(steps[at-1].get('mode_label'),'Draw 2 cards')
                        self.assertEqual(steps[at+1],dict(op='activate',ref='source',value=1))
                    else:
                        self.assertEqual(payments,[])

    def test_manifest_clause_coverage_and_snapshot_suffixes_are_exact(self):
        count=0
        for name in model.pins()['sources']:
            d,p=self.spec(name)
            for case in model.expand(name,p['csvid'],d,p,model.variants(p),self.rows,self.oracle):
                count+=1
                self.assertEqual(case['_recipe']['covered_clauses'],self.oracle[name]['oracle_text'].splitlines())
                steps=case['steps'];ops=[s['op'] for s in steps]
                self.assertFalse(set(ops)-OPS)
                if 'replay' in case['name']:
                    self.assertEqual(steps[ops.index('capture')+1:ops.index('record_state')],steps[ops.index('restore')+1:-1])
        self.assertEqual(count,410)
