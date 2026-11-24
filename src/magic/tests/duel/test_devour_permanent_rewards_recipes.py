import json
import unittest

from import_duel_oracle import registry
import devour_permanent_rewards_recipes as model


class DevourPermanentRewardTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        data=model.PROFILE.parent
        cls.oracle=json.loads((data/'oracle.json').read_text(encoding='utf8'))['cards']
        cls.rows={r['name']:r for r in registry() if r['kind']!='internal'}
        cls.homes={}
        for name,pin in model.profiles()['sources'].items():
            i=pin['registry']['csvid']
            f,=(data/'cases/cards'/f'{i//100:03d}').glob(f'{i:05d}-*.json')
            cls.homes[name]=json.loads(f.read_text(encoding='utf8'))

    def case(self,name,variant):
        d=self.homes[name]['recipes'][0]
        p=model.spec(d,self.oracle[name])
        return model.expand(name,self.rows[name]['csvid'],d,p,[variant],self.rows,self.oracle)[0]

    def count(self,name,p,zone,n):
        return dict(op='assert_zone_multiplicity',player=p,zone=zone,index=self.rows[name]['csvid'],value=n)

    def test_full_oracle_gates_for_six_cards(self):
        model.validate_witnesses(self.oracle,self.rows)
        self.assertEqual({r['csvid'] for r in self.homes.values()},{10149,10155,10031,10085,12535,15501})
        for n,h in self.homes.items():
            self.assertEqual(h['recipes'][0]['variants'],model.variants(model.classify(self.oracle[n])))
            with self.assertRaises(ValueError):
                model.classify(dict(self.oracle[n],oracle_text=self.oracle[n]['oracle_text']+'\nDraw a card.'))

    def test_flash_is_real_response_to_the_other_players_parent(self):
        for p in (0,1):
            s=self.case('Gluttonous Slime',f'flash_response_{p}')['steps']
            self.assertEqual(s[0],dict(op='turn',player=1-p,value=0))
            a=s.index(dict(op='cast',ref='parent',value=1))
            b=s.index(dict(op='announce_spell',ref='parent'))
            c=s.index(dict(op='cast_response',ref='source',value=1))
            d=s.index(dict(op='resolve_response'))
            self.assertLess(a,b)
            self.assertLess(b,c)
            self.assertLess(c,d)
            self.assertIn(dict(op='assert_stack',value=2),s[c:d])
            self.assertIn(dict(op='choose_target',player=p,ref='meal_0'),s[c:d])
            self.assertIn(self.count('Forest',1-p,'hand',1),s[d:])
            self.assertIn(self.count('Island',1-p,'hand',1),s[d:])

    def test_hatchling_has_two_independent_conditional_combat_controls(self):
        for mode,blocked,damage in [('paid_ground',0,3),('none_ground',1,0),
                                    ('paid_trample',1,2),('none_trample',1,0)]:
            s=self.case('Hellkite Hatchling',mode+'_0')['steps']
            self.assertIn(dict(op='block',ref='blocker',other='source',value=blocked),s)
            i=s.index(dict(op='combat_damage'))
            self.assertIn(dict(op='assert_life',player=1,value=20-damage),s[i:])

    def test_predator_attacks_fresh_even_with_zero_devoured(self):
        for mode,life in [('fresh_paid_1',14),('fresh_none_1',16),('fresh_reach_1',20)]:
            s=self.case('Predator Dragon',mode)['steps']
            a=s.index(dict(op='attack',ref='source',value=1))
            self.assertNotIn('advance_turn',[x['op'] for x in s[:a]])
            i=s.index(dict(op='combat_damage'))
            self.assertIn(dict(op='assert_life',player=0,value=life),s[i:])

    def test_trample_activation_cost_and_exact_temporal_boundary(self):
        name='Thorn-Thrash Viashino'
        for mode,expired in [('activate_trample',False),('expired_trample',True)]:
            s=self.case(name,mode+'_0')['steps']
            a=s.index(dict(op='activate',ref='source',value=1))
            b=s.index(dict(op='begin_attackers'))
            self.assertEqual(('advance_turn' in [x['op'] for x in s[a:b]]),expired)
            i=s.index(dict(op='combat_damage'))
            self.assertIn(dict(op='assert_life',player=1,value=20 if expired else 19),s[i:])
        s=self.case(name,'wrong_activation_1')['steps']
        self.assertIn(dict(op='activate',ref='source',value=0),s)
        self.assertIn(dict(op='assert_mana',player=1,color=4,value=1),s)

    def test_square_scales_actual_count_and_zero_toughness_dies(self):
        name='Thromok the Insatiable'
        for mode,number in [('paid_one_1',1),('paid_two_0',2),('three_0',3)]:
            s=self.case(name,mode)['steps']
            i=s.index(dict(op='resolve'))
            self.assertEqual(sum(a['op']=='choose_target' for a in s[:i]),number)
            self.assertIn(dict(op='assert_card',ref='source',property='plus1_counters',value=number**2),s[i:])
        s=self.case(name,'none_0')['steps']
        i=s.index(dict(op='resolve'))
        self.assertIn(self.count(name,0,'graveyard',1),s[i:])
        s=self.case(name,'legend_0')['steps']
        i=s.index(dict(op='cast',ref='second_legend',value=1))
        self.assertIn(dict(op='choose_target',player=0,ref='source'),s[i:])
        self.assertIn(self.count(name,0,'battlefield',1),s[i:])
        self.assertIn(self.count(name,0,'graveyard',1),s[i:])

    def test_thrinax_later_real_entries_and_changed_counter_count(self):
        name='Bloodspore Thrinax'
        for mode,expected in [('enemy_entry_0',0),('zero_entry_0',0),('friendly_entry_0',1),('growth_entry_0',4)]:
            s=self.case(name,mode)['steps']
            i=s.index(dict(op='cast',ref='new_creature',value=1))
            if mode.startswith('growth'):
                growth=s.index(dict(op='cast',ref='growth',value=1))
                self.assertLess(growth,i)
                self.assertNotIn('advance_turn',[a['op'] for a in s[:growth]])
            self.assertIn(dict(op='assert_card',ref='new_creature',property='plus1_counters',value=expected),s[i:])
            self.assertIn(dict(op='assert_card',ref='source',property='plus1_counters',
                               value=0 if mode.startswith('zero') else 4 if mode.startswith('growth') else 1),s[i:])
        s=self.case(name,'noncast_entry_0')['steps']
        i=s.index(dict(op='cast',ref='blink',value=1))
        self.assertIn(dict(op='bind_permanent',player=0,ref='resident',value=self.rows['Hill Giant']['csvid']),s[i:])
        self.assertIn(dict(op='assert_card',ref='resident',property='plus1_counters',value=1),s[i:])

    def test_real_devour_payment_snapshot_has_no_extra_choice(self):
        s=self.case('Gluttonous Slime','snapshot_1')['steps']
        a,b,c=[s.index(dict(op=op)) for op in ('capture','record_state','restore')]
        self.assertEqual(s[a+1:b],s[c+1:-1])
        self.assertEqual(s[-1],dict(op='assert_recorded_state'))

