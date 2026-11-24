import json
import unittest

from import_duel_oracle import registry
import devour_entry_rewards_recipes as model


class DevourEntryRewardTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        data=model.PROFILE.parent
        cls.oracle=json.loads((data/'oracle.json').read_text(encoding='utf8'))['cards']
        cls.rows={r['name']:r for r in registry() if r['kind']!='internal'}
        cls.homes={}
        for n,pin in model.profiles()['sources'].items():
            i=pin['registry']['csvid']
            f,=(data/'cases/cards'/f'{i//100:03d}').glob(f'{i:05d}-*.json')
            cls.homes[n]=json.loads(f.read_text(encoding='utf8'))

    def case(self,name,variant):
        d=self.homes[name]['recipes'][0]
        p=model.spec(d,self.oracle[name])
        return model.expand(name,self.rows[name]['csvid'],d,p,[variant],self.rows,self.oracle)[0]

    def count(self,name,player,zone,value):
        return dict(op='assert_zone_multiplicity',player=player,zone=zone,index=self.rows[name]['csvid'],value=value)

    def test_full_oracle_declarations_and_accurate_multipliers(self):
        model.validate_witnesses(self.oracle,self.rows)
        self.assertEqual({h['csvid'] for h in self.homes.values()},{9929,10070,10083,10229,10018,10087,10284})
        for n,h in self.homes.items():
            p=model.spec(h['recipes'][0],self.oracle[n])
            self.assertEqual(p['multiplier'],1 if n in ('Caldera Hellion','Skullmulcher','Voracious Dragon','Gorger Wurm') else
                             3 if n=='Thunder-Thrash Elder' else 2)
            with self.assertRaises(ValueError):
                model.classify(dict(self.oracle[n],oracle_text=self.oracle[n]['oracle_text']+'\nDraw a card.'))

    def test_actual_both_seat_enumeration_avoids_unsafe_ai_shortcut(self):
        name='Skullmulcher'
        for p in (0,1):
            case=self.case(name,f'paid_one_{p}')
            self.assertIn(dict(name=model.BEAR,ref='meal_0',player=p,zone='battlefield'),case['setup'])
            steps=case['steps']
            i=steps.index(dict(op='resolve'))
            self.assertEqual(steps[i-1],dict(op='choose_target',ref='meal_0',player=p))
            self.assertIn(self.count(model.BEAR,p,'graveyard',1),steps[i:])
        # Engine get_best_permanent_for_sacrifice uses cc[2] and the
        # nice-to-sacrifice flag. Both AI meal witnesses have static handlers.
        root=next(p for p in model.PROFILE.parents if (p/'src/magic/src/carddata.c').is_file())
        registry_lines=(root/'src/magic/src/carddata.c').read_text(encoding='utf8').splitlines()
        for name in (model.BEAR,model.GOBLIN):
            rec,=[l for l in registry_lines if f'"{name}"' in l and 'card_dummy' in l]
            self.assertRegex(rec,r'\{ \d+, \d+, 0 \}, .*card_dummy')

    def test_multiple_partial_zero_and_owned_elsewhere(self):
        name='Thunder-Thrash Elder'
        for mode,n in (('paid_two',2),('partial',1),('decline',0)):
            steps=self.case(name,mode+'_0')['steps']
            i=steps.index(dict(op='resolve'))
            self.assertEqual(sum(s['op']=='choose_target' for s in steps[:i]),n)
            self.assertEqual(sum(s['op']=='choose_cancel_target' for s in steps[:i]),int(n<2))
            self.assertIn(dict(op='assert_card',ref='source',property='plus1_counters',value=3*n),steps[i:])
        c=self.case(name,'borrowed_meal_0')
        self.assertIn(dict(name=model.BEAR,ref='meal_0',player=0,zone='battlefield',state=['opponent_owned']),c['setup'])
        i=c['steps'].index(dict(op='resolve'))
        self.assertIn(self.count(model.BEAR,1,'graveyard',1),c['steps'][i:])

    def test_hellion_hits_itself_and_both_players_creatures(self):
        name='Caldera Hellion'
        c=self.case(name,'sweep_scope_0')['steps']
        i=c.index(dict(op='resolve'))
        self.assertIn(self.count(model.GIANT,1,'graveyard',1),c[i:])
        for ref in ('source','resistant','friendly_guard'):
            self.assertIn(dict(op='assert_card',ref=ref,property='damage',value=3),c[i:])
        c=self.case(name,'none_1')['steps']
        i=c.index(dict(op='resolve'))
        self.assertIn(self.count(name,1,'graveyard',1),c[i:])
        self.assertNotIn('choose_target',[s['op'] for s in c[:i]])

    def test_reward_scales_with_number_devoured_or_printed_goblins(self):
        name='Skullmulcher'
        c=self.case(name,'paid_two_0')['steps']
        i=c.index(dict(op='resolve'))
        for n in ('Forest','Island'):
            self.assertIn(self.count(n,0,'hand',1),c[i:])
        self.assertIn(dict(op='assert_zone',player=0,zone='library',value=3),c[i:])
        for mode,enemy in (('paid_two_0',18),('two_goblins_0',16),('zero_goblins_1',20)):
            c=self.case('Voracious Dragon',mode)['steps']
            i=c.index(dict(op='resolve'))
            p=int(mode[-1])
            self.assertIn(dict(op='assert_life',player=1-p,value=enemy),c[i:])
            self.assertIn(dict(op='choose_player',player=p,value=1-p),c[:i])
        c=self.case('Voracious Dragon','creature_recipient_0')['steps']
        self.assertIn(self.count(model.GIANT,1,'graveyard',1),c)

    def test_tar_fiend_targets_player_and_discards_only_actual_hand_cards(self):
        for mode,target,fallen in (('paid_two_0',1,2),('short_hand_0',1,1),('target_self_0',0,1),('none_1',0,0)):
            c=self.case('Tar Fiend',mode)['steps']
            p=int(mode[-1])
            i=c.index(dict(op='resolve'))
            self.assertEqual(c[i-1],dict(op='choose_player',player=p,value=target))
            self.assertIn(self.count('Forest',target,'graveyard',fallen),c[i:])
            self.assertEqual(c[i+1],dict(op='assert_unused_choices',value=0))

    def test_real_upkeep_uses_external_counters_and_repeats_only_for_owner(self):
        name='Mycoloth'
        c=self.case(name,'upkeep_repeat_1')['steps']
        offsets=[i for i,s in enumerate(c) if s['op']=='upkeep_step']
        self.assertEqual([c[i]['player'] for i in offsets],[0,1,0,1])
        for i,j,n in zip(offsets,offsets[1:]+[len(c)],(0,2,2,4)):
            self.assertIn(self.count(model.TOKEN,1,'battlefield',n),c[i:j])
        c=self.case(name,'external_counter_0')['steps']
        i=c.index(dict(op='cast',ref='growth',value=1))
        self.assertIn(dict(op='assert_card',ref='source',property='plus1_counters',value=1),c[i:])
        self.assertIn(self.count(model.TOKEN,0,'battlefield',1),c[i:])
        c=self.case(name,'source_removed_1')['steps']
        i=c.index(dict(op='upkeep_step',player=1))
        self.assertIn(self.count(model.TOKEN,1,'battlefield',0),c[i:])

    def test_nonscripted_reentry_uses_real_blink_and_flying_controls(self):
        c=self.case('Thunder-Thrash Elder','blink_entry_0')['steps']
        i=c.index(dict(op='cast',ref='blink',value=1))
        self.assertIn(dict(op='choose_target',player=0,ref='meal_0'),c[i:])
        self.assertIn(dict(op='bind_permanent',player=0,ref='source',value=10087),c[i:])
        self.assertIn(dict(op='assert_card',ref='source',property='plus1_counters',value=3),c[i:])
        for mode,allowed in (('fly_ground_0',0),('fly_reach_0',1)):
            c=self.case('Voracious Dragon',mode)['steps']
            self.assertIn(dict(op='block',ref='foe_creature',other='source',value=allowed),c)

    def test_paid_entry_replay_and_counter_close_actual_stack(self):
        c=self.case('Caldera Hellion','snapshot_1')['steps']
        a,b,d=[c.index(dict(op=op)) for op in ('capture','record_state','restore')]
        self.assertEqual(c[a+1:b],c[d+1:-1])
        self.assertEqual(c[-1],dict(op='assert_recorded_state'))
        c=self.case('Caldera Hellion','countered_0')['steps']
        self.assertIn(dict(op='resolve_response'),c)
        self.assertNotIn('choose_target',[s['op'] for s in c])

    def test_gorger_uses_existing_fixed_devour_and_real_due_reentry(self):
        name='Gorger Wurm'
        expected={'paid_one_1':(1,6),'paid_two_0':(2,7),'decline_0':(0,5),'blink_entry_1':(1,6)}
        for mode,(counters,pt) in expected.items():
            c=self.case(name,mode)['steps']
            first=c.index(dict(op='cast',ref='source',value=1))
            self.assertEqual(sum(s['op']=='upkeep_step' for s in c),0)
            for prop,value in (('plus1_counters',counters),('power',pt),('toughness',pt)):
                self.assertIn(dict(op='assert_card',ref='source',property=prop,value=value),c[first:])
            if mode=='blink_entry_1':
                b=c.index(dict(op='cast',ref='blink',value=1))
                self.assertIn(dict(op='choose_target',player=1,ref='meal_0'),c[b:])
