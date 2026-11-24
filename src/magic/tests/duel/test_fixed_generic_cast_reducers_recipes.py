"""Focused guards for fixed generic costs and real action receipts."""

from copy import deepcopy
import unittest

import fixed_generic_cast_reducers_recipes as model


class FixedReducerGuards(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        pins=model.pins()
        entries=pins['sources']|pins['witnesses']
        cls.oracle={name:item['oracle'] for name,item in entries.items()}
        cls.rows={name:item['registry'] for name,item in entries.items()}

    def case(self,name,variant):
        record=self.oracle[name]
        params=model.parameters(record)
        declaration=dict(family=model.FAMILY,version=2,oracle_text=record['oracle_text'],params=params,variants=[variant])
        return model.expand(name,self.rows[name]['csvid'],declaration,model.spec(declaration,record),
                            [variant],self.rows,self.oracle)[0]

    def steps(self,name,variant):
        return self.case(name,variant)['steps']

    def cost(self,spell,source,controllers=(0,),actor=0,tax=0):
        return model.price(self.oracle[spell],model.parameters(self.oracle[source]),controllers,actor,tax=tax)

    def test_preserves_whole_devoid_and_flying_sources(self):
        for name in ('Herald of Kozilek','Warden of Evos Isle'):
            edited=deepcopy(self.oracle[name])
            edited['oracle_text']=edited['oracle_text'].splitlines()[-1]
            with self.assertRaises(ValueError):
                model.parameters(edited)

    def test_both_player_discounts_and_controller_only_symmetry(self):
        for actor in (0,1):
            self.assertEqual(self.cost('Divination','Helm of Awakening',(1-actor,),actor),[1,0,1,0,0,0])
            self.assertEqual(self.cost('Divination','Stone Calendar',(1-actor,),actor),[2,0,1,0,0,0])
            self.assertEqual(self.cost('Divination','Helm of Awakening',(0,1),actor),[0,0,1,0,0,0])
            self.assertEqual(self.cost('Divination','Stone Calendar',(0,1),actor),[1,0,1,0,0,0])

    def test_multicolored_actual_color_and_one_generic_floor(self):
        name="Urza's Filter"
        self.assertEqual(self.cost('Cloudblazer',name),[1,0,1,0,0,1])
        self.assertEqual(self.cost('Centaur Healer',name),[0,0,0,1,0,1])
        self.assertEqual(self.cost('Herald of Kozilek',name),[1,0,1,0,1,0])
        self.assertEqual(self.cost('Water Elemental',name,(0,1)),[3,0,2,0,0,0])
        self.assertIn(dict(op='assert_matching_permanents',player=0,index=15348,property='color',value=0),
                      self.steps(name,'devoid_negative_0'))

    def test_mana_value_six_type_and_owner_boundaries(self):
        name='Krosan Drover'
        for spell,price in [('Spined Wurm',4),('Craw Wurm',2),('Dreamstone Hedron',6),('Steel Hellkite',4)]:
            self.assertEqual(self.cost(spell,name)[0],price)
        self.assertEqual(self.cost('Craw Wurm',name,(1,))[0],4)

    def test_arcane_kindred_instant_and_flash_creature(self):
        name='Arcane Melee'
        self.assertEqual(self.cost('Nameless Inversion',name),[0,1,0,0,0,0])
        self.assertEqual(self.cost('Ambush Viper',name),[1,0,0,1,0,0])
        self.assertIn(dict(op='choose_target',ref='victim',player=0),self.steps(name,'kindred_instant_0'))
        self.assertEqual(self.steps(name,'flash_creature_0')[0],dict(op='turn',player=1,value=0))

    def test_herald_keeps_colored_symbols_and_opposing_full_cost(self):
        name='Herald of Kozilek'
        self.assertEqual(self.cost(name,name),[0,0,1,0,1,0])
        self.assertEqual(self.cost('Maw of Kozilek',name,(1,)),[3,0,0,0,1,0])
        self.assertEqual(self.cost('Maw of Kozilek',name,(0,1)),[2,0,0,0,1,0])
        self.assertEqual(self.cost('Hill Giant',name),[3,0,0,0,1,0])

    def test_warden_exact_price_and_real_flying_reach(self):
        name='Warden of Evos Isle'
        self.assertEqual(self.cost('Wind Drake',name),[1,0,1,0,0,0])
        self.assertEqual(self.cost('Horned Turtle',name),[2,0,1,0,0,0])
        for mode,allowed in [('rider_flying',0),('rider_reach',1)]:
            self.assertIn(dict(op='block',ref='blocker',other='source',value=allowed),self.steps(name,mode+'_1'))

    def test_material_tax_and_real_counter_keep_exact_payment(self):
        name='Arcane Melee'
        self.assertEqual(self.cost('Visions of Beyond',name,tax=1),[0,0,1,0,0,0])
        case=self.case(name,'tax_0')
        self.assertTrue(any(card['name']=='Sphere of Resistance' for card in case['setup']))
        steps=self.steps(name,'countered_0')
        ops=[step['op'] for step in steps]
        self.assertLess(ops.index('announce_spell'),ops.index('cast_response'))
        self.assertIn(dict(op='resolve_response'),steps)
        self.assertIn(dict(op='assert_mana',player=0,color=0,value=2),steps)


if __name__=='__main__':
    unittest.main()
