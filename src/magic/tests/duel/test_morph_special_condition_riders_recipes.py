"""Guards for actual revealed Morph activation and its lifecycle contrasts."""

from copy import deepcopy
import unittest

import morph_special_condition_riders_recipes as model


class MorphSpecialConditionGuards(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        pins = model.profile()
        cards = pins['sources'] | pins['witnesses']
        cls.oracle = {name:pin['oracle'] for name,pin in cards.items()}
        cls.rows = {name:pin['registry'] for name,pin in cards.items()}
        cls.rows['Face-Down Creature'] = pins['face_down']

    def steps(self,name,variant):
        oracle = self.oracle[name]
        dec = dict(family=model.FAMILY,version=2,params=model.parameters(oracle),variants=[variant],
                   oracle_text=oracle['oracle_text'])
        case, = model.expand(name,self.rows[name]['csvid'],dec,model.spec(dec,oracle),[variant],self.rows,self.oracle)
        return case['steps']

    def test_reject_missing_activated_rider(self):
        changed = deepcopy(self.oracle['Zoetic Cavern'])
        changed['oracle_text'] = changed['oracle_text'].partition('\n')[2]
        with self.assertRaises(ValueError):
            model.parameters(changed)

    def test_destroy_nonbasic_type_probe_and_real_payment(self):
        steps = self.steps('Dwarven Blastminer','basic_probe_0')
        self.assertIn(dict(op='probe_activation_target',ref='source',other='basic',index=-1,value=0),steps)
        self.assertIn(dict(op='choose_target',player=0,ref='victim'),steps)
        self.assertIn(dict(op='assert_zone_multiplicity',player=1,zone='graveyard',
                           index=self.rows['Wasteland']['csvid'],value=1),steps)
        self.assertIn(dict(op='assert_card',ref='source',property='tapped',value=1),steps)

    def test_mawcor_damage_to_walker_and_printed_ping(self):
        steps = self.steps('Fledgling Mawcor','walker_ping_0')
        self.assertIn(dict(op='assert_counter',ref='victim',counter_kind='loyalty',value=2),steps)
        printed = self.steps('Fledgling Mawcor','printed_effect_1')
        self.assertIn(dict(op='choose_dialog',player=1,value=1),printed)
        self.assertIn(dict(op='assert_card',ref='victim',property='damage',value=1),printed)

    def test_wall_defender_and_real_turn_down(self):
        printed = self.steps('Wall of Deceit','defender_1')
        later = self.steps('Wall of Deceit','down_attack_1')
        self.assertIn(dict(op='attack',ref='source',value=0),printed)
        self.assertIn(dict(op='attack',ref='source',value=1),later)
        start = later.index(dict(op='activate',ref='source',value=1))
        self.assertIn(dict(op='assert_card',ref='source',property='csvid',value=model.FACE_DOWN),later[start:])

    def test_cavern_actual_land_play_and_generic_output(self):
        steps = self.steps('Zoetic Cavern','land_limit_0')
        self.assertIn(dict(op='play_land',ref='source',value=1),steps)
        self.assertIn(dict(op='play_land',ref='second_land',value=0),steps)
        self.assertIn(dict(op='assert_mana',player=0,color=0,value=1),steps)

    def test_cavern_paid_morph_spends_generated_mana(self):
        steps = self.steps('Zoetic Cavern','mana_spend_1')
        self.assertIn(dict(op='cast',ref='source',value=1),steps)
        self.assertNotIn('play_land',[step['op'] for step in steps])
        payment = steps.index(dict(op='cast',ref='payment_spell',value=1))
        mana_receipt = dict(op='assert_mana',player=1,color=0,value=1)
        self.assertIn(mana_receipt,steps[:payment])
        self.assertIn(dict(mana_receipt,value=0),steps[payment:])
