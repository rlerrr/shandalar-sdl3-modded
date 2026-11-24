"""Focused rules, phase and receipt guards for activated Level Up."""

from copy import deepcopy
import unittest

import level_activated_riders_recipes as model


class LevelActivatedRiderGuards(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        pins = model.profiles()
        cards = pins['sources'] | pins['witnesses']
        cls.oracle = {name:pin['oracle'] for name,pin in cards.items()}
        cls.rows = {name:pin['registry'] for name,pin in cards.items()}

    def steps(self,name,variant):
        oracle = self.oracle[name]
        dec = dict(family=model.FAMILY,version=2,params=model.parameters(oracle),variants=[variant],
                   oracle_text=oracle['oracle_text'])
        case, = model.expand(name,self.rows[name]['csvid'],dec,model.spec(dec,oracle),[variant],self.rows,self.oracle)
        return case['steps']

    def test_full_oracle_rider_required(self):
        record = deepcopy(self.oracle['Echo Mage'])
        record['oracle_text'] = record['oracle_text'].replace('twice','three times')
        with self.assertRaises(ValueError):
            model.parameters(record)

    def test_brimstone_tiers_pay_each_level_and_tap_for_damage(self):
        for variant,amount,levels in [('first_0',1,1),('upper_1',3,3)]:
            steps = self.steps('Brimstone Mage',variant)
            self.assertEqual(steps.count(dict(op='require_can_activate',ref='source',value=1)),levels)
            self.assertIn(dict(op='assert_card',ref='source',property='tapped',value=1),steps)
            self.assertIn(dict(op='assert_life',player=1-int(variant[-1]),value=20-amount),steps)

    def test_echo_real_parent_and_copies_draw_for_correct_player(self):
        steps = self.steps('Echo Mage','upper_1')
        paid = steps.index(dict(op='cast',ref='parent',value=1))
        announce = steps.index(dict(op='announce_spell',ref='parent'))
        response = steps.index(dict(op='activate_response',ref='source',value=1))
        self.assertLess(paid,announce)
        self.assertLess(announce,response)
        self.assertIn(dict(op='assert_stack',value=2),steps[response:])
        self.assertIn(dict(op='assert_zone',player=1,zone='hand',value=2),steps[response:])
        self.assertIn(dict(op='assert_zone',player=0,zone='hand',value=1),steps[response:])

    def test_echo_new_copy_target_is_after_paid_response(self):
        steps = self.steps('Echo Mage','retarget_0')
        original = steps.index(dict(op='choose_player',player=1,value=0))
        activate = steps.index(dict(op='activate_response',ref='source',value=1))
        copy = steps.index(dict(op='choose_player',player=0,value=1))
        self.assertLess(original,activate)
        self.assertLess(activate,copy)
        self.assertIn(dict(op='assert_life',player=0,value=17),steps[copy:])
        self.assertIn(dict(op='assert_life',player=1,value=17),steps[copy:])

    def test_enclave_discard_is_only_in_lower_band(self):
        left = self.steps('Enclave Cryptologist','last_lower_0')
        right = self.steps('Enclave Cryptologist','upper_0')
        self.assertIn(dict(op='assert_discard_choices',value=1),left)
        self.assertIn(dict(op='assert_zone_multiplicity',player=0,zone='graveyard',index=self.rows['Forest']['csvid'],value=1),left)
        self.assertIn(dict(op='assert_zone_multiplicity',player=0,zone='hand',index=self.rows['Forest']['csvid'],value=1),right)

    def test_assassin_reductions_follow_toughness_and_cleanup(self):
        expiry = self.steps('Guul Draz Assassin','expiry_1')
        boundary = expiry.index(dict(op='cleanup'))
        for prop in ('power','toughness'):
            self.assertIn(dict(op='assert_card',ref='victim',property=prop,value=3),expiry[:boundary])
            self.assertIn(dict(op='assert_card',ref='victim',property=prop,value=7),expiry[boundary:])
        lethal = self.steps('Guul Draz Assassin','lethal_1')
        self.assertIn(dict(op='assert_zone_multiplicity',player=0,zone='graveyard',index=self.rows['Grizzly Bears']['csvid'],value=1),lethal)

    def test_tuskcaller_token_shape_comes_from_source_rule(self):
        token = self.rows[model.ELEPHANT]['csvid']
        for mode,count in [('last_lower_0',1),('upper_0',2)]:
            steps = self.steps('Kazandu Tuskcaller',mode)
            self.assertIn(dict(op='assert_zone_multiplicity',player=0,zone='battlefield',index=token,value=count),steps)
            self.assertIn(dict(op='assert_matching_permanents',player=0,index=token,property='power',value=3),steps)
            self.assertIn(dict(op='assert_matching_permanents',player=0,index=token,property='subtype_Elephant',value=1),steps)

    def test_null_regeneration_real_window_and_exile_contrast(self):
        protected = self.steps('Null Champion','upper_1')
        denied = self.steps('Null Champion','exile_1')
        cast = protected.index(dict(op='cast',ref='removal',value=1))
        self.assertIn(dict(op='choose_regeneration',ref='source'),protected[cast:])
        self.assertIn(dict(op='assert_card',ref='source',property='tapped',value=1),protected[cast:])
        self.assertIn(dict(op='assert_unused_choices',value=1),denied)
        self.assertIn(dict(op='assert_zone_multiplicity',player=1,zone='exile',index=self.rows['Null Champion']['csvid'],value=1),denied)
