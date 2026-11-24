"""Guard printed Morph parameters, actual choice order and state receipts."""
import copy
import json
import unittest
from pathlib import Path

import morph_creature_recipes as model
from generate_cases import registry


class MorphCreatureRecipeTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.oracle=json.loads((Path(__file__).parent/'data/oracle.json').read_text(encoding='utf8'))['cards']
        cls.rows={row['name']:row for row in registry()}

    def declaration(self, name):
        original=self.oracle[name]
        return dict(family=model.FAMILY,version=2,oracle_text=original['oracle_text'],params=dict(schema=1,mechanism=model.mechanism(original)))

    def cases(self,name,*variants):
        declaration=self.declaration(name)
        shape=model.spec(declaration,self.oracle[name])
        return model.expand(name,self.rows[name]['csvid'],declaration,shape,variants,self.rows,self.oracle)

    def steps(self, variant, name='Glacial Stalker'):
        return self.cases(name,variant)[0]['steps']

    def test_complete_original_sources_and_no_seeded_hidden_form(self):
        identities={'Canyon Lurkers':(14240,[3,0,0,0,1,0]), 'Glacial Stalker':(14275,[4,0,1,0,0,0]),
                    'Krumar Bond-Kin':(14309,[4,1,0,0,0,0]), 'War Behemoth':(14424,[4,0,0,0,0,1]),
                    'Woolly Loxodon':(14436,[5,0,0,1,0,0]), 'Treespring Lorian':(6301,[5,0,0,1,0,0]),
                    'Krosan Colossus':(6194,[6,0,0,2,0,0]),'Scornful Egotist':(6575,[0,0,1,0,0,0]),
                    'Witness of the Ages':(14435,[5,0,0,0,0,0])}
        self.assertEqual(set(model.pins()['sources']), set(identities))
        model.validate_witnesses(self.oracle,self.rows)
        all_cases=[]
        for name,(csvid,fee) in identities.items():
            shape=model.spec(self.declaration(name),self.oracle[name])
            self.assertEqual((shape['csvid'],shape['unmorph']),(csvid,fee))
            cases=self.cases(name,*model.variants(shape))
            all_cases.extend(cases)
            for case in cases:
                self.assertEqual(case['setup'][0],dict(name=name,ref='source',player=int(case['name'][-1]),zone='hand'))
                self.assertFalse(any(item['name']=='Face-Down Creature' or 'state' in item for item in case['setup']))
                self.assertEqual(case['_recipe']['covered_clauses'],self.oracle[name]['oracle_text'].splitlines())
        self.assertEqual(len(all_cases),382)

    def test_wrong_clause_and_mutated_payment_rejected(self):
        name='Canyon Lurkers'; original=self.oracle[name]
        declaration=copy.deepcopy(self.declaration(name))
        declaration['params']['mechanism']['unmorph'][0]+=1
        with self.assertRaises(ValueError):
            model.spec(declaration,original)
        with self.assertRaises(ValueError):
            model.mechanism(dict(original,oracle_text=original['oracle_text']+'\nFlying'))
        with self.assertRaises(ValueError):
            model.spec(self.declaration(name),dict(original,oracle_id='stale'))

    def test_real_menu_only_if_both_cast_modes_are_affordable(self):
        for owner in (0,1):
            for mode,choice,identity in (('cast_faceup',1,14275),('choose_facedown',0,model.FACE_DOWN),
                                         ('cancel_cast',2,None),('cast_default',None,model.FACE_DOWN),
                                         ('cast_colored',None,model.FACE_DOWN),('cast_short',None,None)):
                steps=self.steps(f'{mode}_{owner}')
                cast=next(i for i,step in enumerate(steps) if step['op']=='cast')
                self.assertEqual(steps[cast],dict(op='cast',ref='source',value=int(identity is not None)))
                menu=[s for s in steps if s['op']=='choose_dialog']
                self.assertEqual(menu,[] if choice is None else [dict(op='choose_dialog',player=owner,value=choice)])
                if choice is not None:
                    self.assertEqual(steps[cast-1],menu[0])
                mana=[s for s in steps if s['op']=='choose_mana']
                self.assertEqual(mana, [dict(op='choose_mana',player=0,color=1)]*3 if mode=='cast_colored' and owner==0 else [])
                if mana:
                    self.assertEqual(steps[cast-3:cast],mana)
                if identity is not None:
                    self.assertIn(dict(op='assert_card',ref='source',property='csvid',value=identity),steps[cast:])
                if identity == model.FACE_DOWN:
                    self.assertIn(dict(op='assert_card',ref='source',property='power',value=2),steps[cast:])
                    self.assertIn(dict(op='assert_matching_permanents',player=owner,index=model.FACE_DOWN,property='color',value=0),steps[cast:])

    def test_flip_is_measured_before_legacy_carrier_and_on_actual_parent(self):
        for owner in (0,1):
            for mode in ('pending_shock','pending_smother'):
                steps=self.steps(f'{mode}_{owner}')
                act=steps.index(dict(op='activate',ref='source',value=1))
                parent=steps.index(dict(op='cast',ref='response',value=1))
                resolve=steps.index(dict(op='resolve_response'))
                self.assertLess(parent,act)
                self.assertLess(act,resolve)
                self.assertIn(dict(op='assert_card',ref='source',property='csvid',value=14275),steps[act:resolve])
                self.assertIn(dict(op='assert_mana',player=owner,color=2,value=0),steps[act:resolve])
                self.assertFalse(any(s['op']=='assert_stack' for s in steps[act:resolve]))
                self.assertIn(dict(op='assert_card',ref='source',property='damage',value=2 if mode=='pending_shock' else 0),steps[resolve:])

    def test_failed_fees_and_actual_mana_value_targeting(self):
        for owner in (0,1):
            for mode in ('short_fee','wrong_color','already_up'):
                steps=self.steps(f'{mode}_{owner}')
                self.assertIn(dict(op='can_activate',ref='source',value=0),steps)
                self.assertIn(dict(op='activate',ref='source',value=0),steps)
            for mode,expected in (('mv_facedown',1),('mv_faceup',0)):
                steps=self.steps(f'{mode}_{owner}')
                self.assertIn(dict(op='can_cast',ref='response',value=expected),steps)
                self.assertIn(dict(op='cast',ref='response',value=expected),steps)
                self.assertEqual(sum(s['op']=='choose_target' for s in steps),expected)

    def test_replay_and_bounce_restore_actual_identity(self):
        for owner in (0,1):
            for mode in ('ready_replay','face_down_replay'):
                steps=self.steps(f'{mode}_{owner}')
                ops=[s['op'] for s in steps]
                self.assertEqual(steps[ops.index('capture')+1:ops.index('record_state')],
                                 steps[ops.index('ai_roundtrip')+1:ops.index('assert_recorded_state')])
            bounced=self.steps(f'bounce_recast_{owner}')
            rebound=bounced.index(dict(op='bind_hand_copy',ref='returned',player=owner,index=0,value=14275))
            self.assertIn(dict(op='cast',ref='returned',value=1),bounced[rebound:])
            self.assertIn(dict(op='assert_card',ref='returned',property='csvid',value=model.FACE_DOWN),bounced[rebound:])

    def test_both_unobserved_subtypes_have_real_coat_receipts(self):
        for name in ('Krumar Bond-Kin','Woolly Loxodon'):
            declaration=self.declaration(name)
            for owner in (0,1):
                steps=self.steps(f'subtype_kin_{owner}',name)
                coat=steps.index(dict(op='cast',ref='coat',value=1))
                activate=steps.index(dict(op='activate',ref='source',value=1))
                self.assertLess(coat,activate)
                self.assertIn(dict(op='assert_card',ref='source',property='power',value=2),steps[coat:activate])
                source_power=declaration['params']['mechanism']['power']
                self.assertIn(dict(op='assert_card',ref='source',property='power',value=source_power+1),steps[activate:])


    def test_held_can_use_genuine_artifact_and_distinct_fee_vectors(self):
        for owner in (0,1):
            witness = self.steps(f'colored_generic_fee_{owner}','Witness of the Ages')
            act = witness.index(dict(op='activate',ref='source',value=1))
            self.assertIn(dict(op='set_mana',player=owner,color=1,value=5),witness[:act])
            self.assertIn(dict(op='assert_mana',player=owner,color=1,value=0),witness[act:])
            self.assertEqual([s for s in witness if s['op']=='choose_mana'],
                             [dict(op='choose_mana',player=0,color=1)]*5 if owner==0 else [])
            self.assertIn(dict(op='assert_matching_permanents',player=owner,index=14435,property='artifact',value=1),witness[act:])
            self.assertFalse(any('wrong_color' in v for v in model.variants(model.mechanism(self.oracle['Witness of the Ages']))))
            egotist = self.steps(f'short_fee_{owner}','Scornful Egotist')
            self.assertIn(dict(op='activate',ref='source',value=0),egotist)
            colossus = self.steps(f'unmorph_{owner}','Krosan Colossus')
            self.assertIn(dict(op='set_mana',player=owner,color=3,value=2),colossus)
