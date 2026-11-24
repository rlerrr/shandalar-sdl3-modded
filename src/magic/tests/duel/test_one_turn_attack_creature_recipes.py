"""Guard whole Oracle prices, real combat and sacrifice-specific receipts."""
import copy
import json
import unittest
from pathlib import Path

import one_turn_attack_creature_recipes as model
from generate_cases import registry


class OneTurnAttackCreatureRecipeTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.oracle=json.loads((Path(__file__).parent/'data/oracle.json').read_text(encoding='utf8'))['cards']
        cls.rows={row['name']:row for row in registry()}

    def declaration(self,name):
        original=self.oracle[name]
        return dict(family=model.FAMILY,version=2,oracle_text=original['oracle_text'],params=dict(schema=1,mechanism=model.mechanism(original)))

    def cases(self,name,*variants):
        declaration=self.declaration(name)
        parsed=model.spec(declaration,self.oracle[name])
        return model.expand(name,parsed['csvid'],declaration,parsed,variants,self.rows,self.oracle)

    def steps(self,variant,name='Groundbreaker'):
        return self.cases(name,variant)[0]['steps']

    def prop(self,ref,name,value):
        return dict(op='assert_card',ref=ref,property=name,value=value)

    def test_full_source_identities_and_whole_clauses(self):
        expected={'Arc Runner':(11090,[2,0,0,0,1,0],False,False), 'Groundbreaker':(1241,[0,0,0,3,0,0],True,False),
                  'Putrefax':(11340,[3,0,0,2,0,0],True,True), 'Spark Elemental':(1263,[0,0,0,0,1,0],True,False),
                  'Blistering Firecat':(6069,[1,0,0,0,3,0],True,False)}
        self.assertEqual(set(model.pins()['sources']),set(expected))
        model.validate_witnesses(self.oracle,self.rows)
        count=0
        for name,pin in expected.items():
            shape=model.spec(self.declaration(name),self.oracle[name])
            self.assertEqual((shape['csvid'],shape['cost'],shape['trample'],shape['infect']),pin)
            cases=self.cases(name,*model.variants(shape))
            count+=len(cases)
            for case in cases:
                self.assertEqual(case['setup'][0],dict(name=name,ref='source',player=int(case['name'][-1]),zone='hand'))
                self.assertEqual(case['_recipe']['covered_clauses'],self.oracle[name]['oracle_text'].splitlines())
        self.assertEqual(count,222)

    def test_rejects_sacrifice_and_keyword_drift(self):
        name='Groundbreaker'; record=self.oracle[name]
        changed=copy.deepcopy(self.declaration(name))
        changed['params']['mechanism']['trample']=False
        with self.assertRaises(ValueError):
            model.spec(changed,record)
        with self.assertRaises(ValueError):
            model.mechanism(dict(record,oracle_text=record['oracle_text'].replace('the end step','your end step')))
        with self.assertRaises(ValueError):
            model.mechanism(dict(record,keywords=['Haste']))

    def test_fresh_attack_real_sickness_and_full_combat(self):
        for owner in (0,1):
            steps=self.steps(f'sick_witness_{owner}')
            attacks=[s for s in steps if s['op']=='attack']
            self.assertEqual(attacks,[dict(op='attack',ref='sick',value=0),dict(op='attack',ref='source',value=1)])
            before=steps[:steps.index(dict(op='begin_attackers'))]
            self.assertIn(dict(op='cast',ref='sick',value=1),before)
            self.assertIn(dict(op='cast',ref='source',value=1),before)
            self.assertFalse(any(s['op']=='advance_turn' for s in before))
            combat=['begin_attackers','finish_attackers','begin_blockers','advance_combat','combat_damage','end_combat','cleanup']
            self.assertEqual([s['op'] for s in steps if s['op'] in combat],combat)
            pacified=self.steps(f'pacified_{owner}')
            self.assertIn(dict(op='attack',ref='source',value=0),pacified)

    def test_trample_and_infect_change_distinct_observables(self):
        for owner in (0,1):
            for name,life,poison in (('Arc Runner',20,0),('Groundbreaker',15,0),('Putrefax',20,4),('Spark Elemental',18,0)):
                steps=self.steps(f'combat_small_{owner}',name)
                damage=steps.index(dict(op='combat_damage'))
                self.assertIn(dict(op='assert_life',player=1-owner,value=life),steps[damage:])
                self.assertIn(dict(op='assert_poison',player=1-owner,value=poison),steps[damage:])
            infect=self.steps(f'combat_high_{owner}','Putrefax')
            after=infect[infect.index(dict(op='combat_damage')):]
            self.assertIn(self.prop('blocker','minus1_counters',5),after)
            self.assertIn(self.prop('blocker','toughness',2),after)
            self.assertIn(self.prop('blocker','damage',0),after)
            fog=self.steps(f'combat_fog_{owner}','Putrefax')
            self.assertIn(dict(op='cast',ref='fog',value=1),fog)
            self.assertIn(dict(op='assert_poison',player=1-owner,value=0),fog[fog.index(dict(op='combat_damage')):])

    def test_sacrifice_witness_separates_death_and_other_turn(self):
        for owner in (0,1):
            for mode,counters in (('end_own',1),('end_other',1),('source_bounced',0),('combat_small',0),('two_end_steps',1)):
                steps=self.steps(f'{mode}_{owner}')
                receipts=[s['value'] for s in steps if s.get('ref')=='witness' and s.get('property')=='plus1_counters']
                self.assertEqual((receipts[0],receipts[-1]),(0,counters))
                if mode=='end_other':
                    self.assertEqual(steps[0],dict(op='turn',player=1-owner,value=0))
                    self.assertIn(dict(op='activate',ref='piper',value=1),steps)
                    self.assertIn(dict(op='choose_zone_card',player=owner,value=1241),steps)
                    self.assertFalse(any(s['op']=='cast' for s in steps))
            indestructible=self.steps(f'indestructible_sacrifice_{owner}')
            end=indestructible.index(dict(op='cleanup'))
            self.assertIn(self.prop('source','damage',3),indestructible[:end])
            self.assertIn(self.prop('witness','plus1_counters',1),indestructible[end:])
            self.assertIn(dict(op='assert_zone_multiplicity',player=owner,zone='graveyard',index=1241,value=1),indestructible[end:])

    def test_replay_and_actual_recast_preserve_order(self):
        for owner in (0,1):
            for mode,boundary in (('replay_cast','cast'),('replay_end','cleanup')):
                steps=self.steps(f'{mode}_{owner}')
                ops=[s['op'] for s in steps]
                first=steps[ops.index('capture')+1:ops.index('record_state')]
                repeat=steps[ops.index('ai_roundtrip')+1:ops.index('assert_recorded_state')]
                self.assertEqual(first,repeat)
                self.assertTrue(any(s['op']==boundary for s in first))
            bounce=self.steps(f'bounce_recast_{owner}')
            rebound=bounce.index(dict(op='bind_hand_copy',ref='returned',player=owner,index=0,value=1241))
            self.assertIn(dict(op='cast',ref='returned',value=1),bounce[rebound:])
            self.assertIn(dict(op='attack',ref='returned',value=1),bounce[rebound:])

    def test_firecat_uses_real_costs_face_down_and_paid_turn_up(self):
        name='Blistering Firecat'
        params=self.declaration(name)['params']['mechanism']
        self.assertEqual(params['morph'],[0,0,0,0,2,0])
        for owner in (0,1):
            hidden=self.steps(f'hidden_fresh_{owner}',name)
            self.assertIn(dict(op='attack',ref='source',value=0),hidden)
            end=hidden.index(dict(op='cleanup'))
            self.assertIn(self.prop('source','csvid',model.FACE_DOWN),hidden[end:])
            self.assertIn(self.prop('source','toughness',2),hidden[end:])
            self.assertIn(self.prop('witness','plus1_counters',0),hidden[end:])
            aged=self.steps(f'hidden_blocked_{owner}',name)
            self.assertEqual(sum(s['op']=='advance_turn' for s in aged),2)
            damage=aged.index(dict(op='combat_damage'))
            self.assertIn(dict(op='block',ref='blocker',other='source',value=1),aged[:damage])
            self.assertIn(dict(op='assert_life',player=1-owner,value=20),aged[damage:])
            self.assertIn(self.prop('blocker','damage',2),aged[damage:])
            flipped=self.steps(f'morph_attack_{owner}',name)
            flip=flipped.index(dict(op='activate',ref='source',value=1))
            combat=flipped.index(dict(op='begin_attackers'))
            self.assertIn(self.prop('source','csvid',6069),flipped[flip:combat])
            self.assertIn(dict(op='assert_matching_permanents',player=owner,index=6069,property='color',value=16),flipped[flip:combat])
            self.assertIn(dict(op='assert_matching_permanents',player=owner,index=model.FACE_DOWN,property='color',value=0),hidden[end:])
            self.assertIn(dict(op='assert_mana',player=owner,color=4,value=0),flipped[flip:combat])
            self.assertIn(dict(op='attack',ref='source',value=1),flipped[combat:])
            self.assertIn(self.prop('witness','plus1_counters',1),flipped[combat:])
            for mode in ('morph_short','morph_wrong_color'):
                refused=self.steps(f'{mode}_{owner}',name)
                self.assertIn(dict(op='can_activate',ref='source',value=0),refused)
                self.assertIn(dict(op='activate',ref='source',value=0),refused)
            explicit=self.steps(f'hidden_choice_{owner}',name)
            self.assertIn(dict(op='choose_dialog',player=owner,value=0),explicit)
            self.assertIn(dict(op='assert_mana',player=owner,color=4,value=3),explicit[-60:])
