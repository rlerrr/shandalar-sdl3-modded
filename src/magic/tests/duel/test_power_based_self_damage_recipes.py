import json
import unittest
from pathlib import Path

import power_based_self_damage_recipes as family
from generate_cases import registry
from recipes import expand


class PowerBasedSelfDamageTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.duel = Path(__file__).parent
        cls.oracle = json.loads((cls.duel/'data/oracle.json').read_text(encoding='utf8'))['cards']
        cls.rows = {r['name']:r for r in registry()}
        cls.sources = family.profiles()['sources']

    def declaration(self,name):
        csvid = self.rows[name]['csvid']
        path, = (self.duel/f'data/cases/cards/{csvid//100:03}').glob(f'{csvid:05}-*.json')
        declaration, = json.loads(path.read_text(encoding='utf8'))['recipes']
        return declaration

    def case(self,name,mode,p=0):
        d = dict(self.declaration(name),variants=[f'{mode}_{p}'])
        return expand(name,self.rows[name]['csvid'],d,self.oracle[name],self.rows,self.oracle)[0]

    def has(self,case,**fields):
        return any(all(s.get(k)==v for k,v in fields.items()) for s in case['steps'])

    def zone_card(self,case,p,zone,name,n):
        self.assertTrue(self.has(case,op='assert_zone_multiplicity',player=p,zone=zone,index=self.rows[name]['csvid'],value=n))

    def test_whole_oracle_and_exact_implementation_pins(self):
        family.validate_witnesses(self.oracle,self.rows)
        self.assertEqual(set(self.sources),{'Repentance',"Kiku's Shadow",'Wrack with Madness','Inner Struggle','Justice Strike'})
        for name in self.sources:
            r = self.oracle[name]
            self.assertEqual(family.spec(self.declaration(name),r)['amount'],'power_on_resolution')
            with self.assertRaises(ValueError): family.parameters(dict(r,oracle_text=r['oracle_text']+' Draw a card.'))
        with self.assertRaises(ValueError): family.parameters(self.oracle['Prey Upon'])

    def test_exact_matrix_without_synthetic_gameplay_and_one_selected_target(self):
        total = 0
        for name in self.sources:
            d = self.declaration(name)
            self.assertEqual(d['variants'],family.variants(family.spec(d,self.oracle[name])))
            csvid = self.rows[name]['csvid']
            cases = expand(name,csvid,d,self.oracle[name],self.rows,self.oracle)
            total += len(cases)
            self.assertFalse(any(s['op'] in ('destroy','dispatch_gfp','move_card','set_counter','set_life')
                                 for c in cases for s in c['steps']))
            for p in (0,1):
                c = self.case(name,'survives',p)
                self.assertEqual([s for s in c['steps'] if s['op']=='choose_target'],[dict(op='choose_target',player=p,ref='victim')])
                self.assertTrue(self.has(c,op='assert_card',ref='alternate',property='damage',value=0))
        self.assertEqual(total,188)

    def test_only_instant_cases_cast_on_other_turn_or_over_a_real_parent(self):
        for name in self.sources:
            d = self.declaration(name)
            instant = self.oracle[name]['type_line']=='Instant'
            self.assertEqual(any(x.startswith('off_turn') for x in d['variants']),instant)
            self.assertEqual(any(x.startswith('instant_response') for x in d['variants']),instant)
            for p in (0,1):
                if instant:
                    c = self.case(name,'instant_response',p)
                    self.assertTrue(self.has(c,op='turn',player=1-p,value=0))
                    steps = c['steps']
                    a = steps.index(dict(op='announce_spell',ref='parent'))
                    b = steps.index(dict(op='cast_response',ref='source',value=1))
                    self.assertLess(a,b)
                    drain = steps.index(dict(op='resolve_response'))
                    self.assertIn(dict(op='assert_stack',value=2),steps[b:drain])
                    self.assertNotIn(dict(op='assert_stack',value=1),steps[drain:])
                    self.assertNotIn(dict(op='resolve'),steps[drain:])
                    self.zone_card(c,1-p,'graveyard','Reach Through Mists',1)
                else:
                    self.assertTrue(self.has(self.case(name,'survives',p),op='turn',player=p,value=0))

    def test_resolving_uses_changed_power_and_clamps_negative_damage_to_zero(self):
        for p in (0,1):
            positive = self.case('Inner Struggle','power_up_pending',p)
            self.assertTrue(self.has(positive,op='cast_response',ref='response',value=1))
            self.assertTrue(self.has(positive,op='assert_card',ref='victim',property='power',value=4))
            self.assertTrue(self.has(positive,op='assert_card',ref='victim',property='damage',value=4))
            steps = positive['steps']
            drain = steps.index(dict(op='resolve_response'))
            before_damage = dict(op='assert_card',ref='victim',property='damage',value=0)
            self.assertIn(before_damage,steps[:drain])
            self.assertNotIn(before_damage,steps[drain:])
            negative = self.case('Repentance','negative_power_pending',p)
            for prop,value in [('power',-1),('toughness',2),('damage',0)]:
                self.assertTrue(self.has(negative,op='assert_card',ref='victim',property=prop,value=value))
            zero = self.case('Justice Strike','zero_power',p)
            self.assertTrue(self.has(zero,op='assert_card',ref='victim',property='power',value=0))
            self.assertTrue(self.has(zero,op='assert_card',ref='victim',property='damage',value=0))

    def test_current_target_controller_gains_lifelink_even_with_foreign_ownership(self):
        for p in (0,1):
            for mode,owner in [('lifelink',1-p),('foreign_lifelink',p)]:
                c = self.case("Kiku's Shadow",mode,p)
                self.assertTrue(self.has(c,op='assert_life',player=1-p,value=22))
                self.assertTrue(self.has(c,op='assert_life',player=p,value=20))
                self.zone_card(c,owner,'graveyard','Child of Night',1)
                self.zone_card(c,1-owner,'graveyard','Child of Night',0)

    def test_deathtouch_indestructibility_and_own_color_protection(self):
        for p in (0,1):
            dead = self.case('Wrack with Madness','deathtouch',p)
            self.assertTrue(self.has(dead,op='assert_card',ref='victim',property='toughness',value=2))
            self.zone_card(dead,1-p,'graveyard','Deadly Recluse',1)
            iron = self.case('Wrack with Madness','indestructible',p)
            self.zone_card(iron,1-p,'battlefield','Darksteel Sentinel',1)
            self.assertTrue(self.has(iron,op='assert_card',ref='victim',property='damage',value=3))
            ward = self.case('Justice Strike','own_color_protection',p)
            self.assertTrue(self.has(ward,op='cast',ref='ward',value=1))
            self.assertTrue(self.has(ward,op='can_target_creature',ref='source',other='victim',value=1))
            self.assertTrue(self.has(ward,op='cast',ref='source',value=1))
            self.assertTrue(self.has(ward,op='assert_card',ref='victim',property='damage',value=0))

    def test_invalid_targets_and_countering_retain_paid_spells_and_exact_zones(self):
        name = 'Inner Struggle'
        for p in (0,1):
            for mode in ('target_hexproof','target_bounced','countered'):
                c = self.case(name,mode,p)
                self.assertTrue(self.has(c,op='announce_spell',ref='source'))
                self.assertTrue(self.has(c,op='cast_response',ref='response',value=1))
                self.zone_card(c,p,'graveyard',name,1)
                victim_zone = 'hand' if mode=='target_bounced' else 'battlefield'
                self.zone_card(c,1-p,victim_zone,'Horned Turtle',1)
            for mode in ('no_creature','shroud_only','insufficient_mana'):
                c = self.case(name,mode,p)
                self.assertFalse(self.has(c,op='choose_target'))
                self.assertTrue(self.has(c,op='cast',ref='source',value=0))
                self.zone_card(c,p,'hand',name,1)

    def test_pending_replay_wraps_actual_resolution(self):
        c = self.case('Repentance','pending_snapshot')['steps']
        self.assertLess(c.index(dict(op='cast',ref='source',value=1)),c.index(dict(op='capture')))
        self.assertLess(c.index(dict(op='capture')),c.index(dict(op='resolve')))
        self.assertEqual(c.count(dict(op='resolve')),2)
        self.assertEqual(c[-1],dict(op='assert_recorded_state'))


if __name__=='__main__':
    unittest.main()
