"""Focused guards for real alternate-payment and damage fixtures."""

import unittest

import kicker_alternate_payment_and_damage_recipes as recipe


class KickerAlternateDamageTests(unittest.TestCase):
    def case(self,name,mode,owner=0):
        pins=recipe.profiles()['sources']|recipe.profiles()['witnesses']
        oracle={name:pin['oracle'] for name,pin in pins.items()}
        rows={name:pin['registry'] for name,pin in pins.items()}
        record=oracle[name]
        shape=recipe.classify(record)
        label=f'{mode}_{owner}'
        declaration=dict(family=recipe.FAMILY,version=2,params=shape,variants=[label],oracle_text=record['oracle_text'])
        return recipe.expand(name,rows[name]['csvid'],declaration,shape,[label],rows,oracle)[0]

    def values(self,case,op,**fields):
        return [s.get('value') for s in case['steps'] if s['op']==op and all(s.get(k)==v for k,v in fields.items())]

    def test_exact_source_record(self):
        for pin in recipe.profiles()['sources'].values():
            record=pin['oracle']
            with self.assertRaises(ValueError):
                recipe.classify(record|{'oracle_text':record['oracle_text']+'\nDraw a card.'})

    def test_real_vampire_and_rounding_control(self):
        for owner in (0,1):
            odd=self.case('Blood Tribute','paid',owner)
            even=self.case('Blood Tribute','paid_even',owner)
            self.assertEqual(self.values(odd,'cast',ref='vampire'),[1])
            self.assertEqual(self.values(odd,'assert_card',ref='vampire',property='tapped'),[0,1,1])
            self.assertEqual(self.values(odd,'assert_life',player=1-owner),[17,8])
            self.assertEqual(self.values(even,'assert_life',player=1-owner),[18,9])
            self.assertEqual(self.values(even,'assert_life',player=owner),[20,29])

    def test_pet_exact_black_with_blue_as_unspent_control(self):
        for owner in (0,1):
            name="Dralnu's Pet"
            good=self.case(name,'paid_buffer',owner)
            self.assertEqual(self.values(good,'assert_mana',player=owner,color=1)[-1],0)
            self.assertEqual(self.values(good,'assert_mana',player=owner,color=2)[-1],1)
            self.assertEqual(self.values(good,'assert_counter',ref='source',counter_kind='plus1'),[4])
            self.assertEqual(self.values(good,'block',ref='ground'),[0])
            self.assertEqual(self.values(good,'block',ref='air'),[1])
            ordinary=self.case(name,'base_only',owner)
            self.assertFalse(self.values(ordinary,'choose_dialog'))
            self.assertEqual(self.values(ordinary,'block',ref='ground'),[1])

    def test_molten_actual_pending_mana_and_nonmana_permissions(self):
        for owner in (0,1):
            name='Molten Disaster'
            lock=self.case(name,'lockout',owner)
            self.assertEqual(self.values(lock,'choose_x',player=owner),[3])
            self.assertEqual(self.values(lock,'activate_response',ref='response'),[0])
            self.assertEqual(self.values(lock,'assert_mana',player=1-owner,color=4)[-1],1)
            mana=self.case(name,'mana_response',owner)
            self.assertEqual(self.values(mana,'activate_response',ref='mana_land'),[1])
            self.assertEqual(self.values(mana,'assert_mana',player=1-owner,color=2)[-1],1)

    def test_two_orim_target_types_and_effect(self):
        for owner in (0,1):
            name="Orim's Thunder"
            self.assertEqual(self.values(self.case(name,'paid',owner),'assert_card',ref='creature',property='damage'),[4])
            self.assertEqual(self.values(self.case(name,'enchantment',owner),'assert_card',ref='creature',property='damage'),[5])
            self.assertFalse(self.values(self.case(name,'no_creature',owner),'choose_dialog'))

    def test_footing_actual_prevention_contrast_and_unshielded_damage(self):
        for owner in (0,1):
            name='Unstable Footing'
            shield=self.case(name,'shield_only',owner)
            self.assertEqual(self.values(shield,'cast',ref='shield'),[1])
            self.assertEqual(self.values(shield,'assert_life',player=1-owner),[20])
            actual=self.case(name,'paid_unshielded',owner)
            self.assertFalse(self.values(actual,'cast',ref='shield'))
            self.assertEqual(self.values(actual,'assert_life',player=1-owner),[15,12])

    def test_rage_counter_attempt_and_ordinary_unshielded_base(self):
        for owner in (0,1):
            name="Urza's Rage"
            attempt=self.case(name,'counter_attempt',owner)
            self.assertEqual(self.values(attempt,'cast_response',ref='counter'),[1])
            self.assertEqual(self.values(attempt,'assert_life',player=1-owner),[10])
            ordinary=self.case(name,'declined_unshielded',owner)
            self.assertEqual(self.values(ordinary,'assert_life',player=1-owner),[17])

    def test_no_extra_scenarios_under_wrong_card(self):
        pin=recipe.profiles()['sources']['Blood Tribute']
        r=pin['oracle']
        declaration=dict(family=recipe.FAMILY,version=2,params=recipe.classify(r),oracle_text=r['oracle_text'],
                         variants=['counter_attempt_0'])
        with self.assertRaises(ValueError):
            recipe.spec(declaration,r)
