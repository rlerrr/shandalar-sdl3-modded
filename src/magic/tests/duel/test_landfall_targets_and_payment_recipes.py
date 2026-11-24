"""Focused guards for paid and targeted Landfall."""
import unittest

import landfall_targets_and_payment_recipes as recipe


class LandfallTargetsAndPaymentTests(unittest.TestCase):
    def case(self,name,mode,owner=0):
        pinned=recipe.profiles()['sources']|recipe.profiles()['witnesses']
        oracle={name:pin['oracle'] for name,pin in pinned.items()}
        rows={name:pin['registry'] for name,pin in pinned.items()}
        shape=recipe.classify(oracle[name])
        labels=[f'{mode}_{owner}']
        decl=dict(family=recipe.FAMILY,version=2,params=shape,variants=labels,oracle_text=oracle[name]['oracle_text'])
        return recipe.expand(name,rows[name]['csvid'],decl,shape,labels,rows,oracle)[0]

    def steps(self,case,op,**expected):
        return [s for s in case['steps'] if s['op']==op and all(s.get(k)==v for k,v in expected.items())]

    def values(self,case,op,**expected):
        return [s.get('value') for s in self.steps(case,op,**expected)]

    def test_full_oracle_gate(self):
        for name,pin in recipe.profiles()['sources'].items():
            record=pin['oracle']
            with self.subTest(card=name),self.assertRaises(ValueError):
                recipe.classify(record|{'oracle_text':record['oracle_text']+'\nFlying'})

    def test_spiral_actual_offturn_and_two_entries_in_same_turn(self):
        for name in recipe.profiles()['sources']:
            for owner in (0,1):
                off=self.case(name,'offturn',owner)
                self.assertEqual(self.values(off,'cast',ref='spiral'),[1])
                self.assertFalse(self.steps(off,'play_land'))
                self.assertEqual(self.steps(off,'turn')[-1]['player'],1-owner)
                repeat=self.case(name,'repeat',owner)
                self.assertEqual(self.values(repeat,'play_land',ref='land'),[1])
                self.assertEqual(self.values(repeat,'cast',ref='spiral'),[1])
                land=repeat['steps'].index(self.steps(repeat,'play_land')[0])
                spiral=repeat['steps'].index(self.steps(repeat,'cast',ref='spiral')[0])
                self.assertLess(land,spiral)
                self.assertFalse(any(s['op']=='advance_turn' for s in repeat['steps'][land:spiral]))

    def test_crab_targets_either_player_and_mills_actual_bounded_prefix(self):
        for owner in (0,1):
            for mode,count in (('own',3),('self_target',3),('short_library',2),('empty_library',0)):
                case=self.case('Hedron Crab',mode,owner)
                recipient=owner if mode=='self_target' else 1-owner
                self.assertEqual(self.values(case,'choose_player',player=owner),[recipient])
                self.assertEqual(self.values(case,'assert_zone',player=recipient,zone='graveyard'),[count])

    def test_life_loss_bypasses_damage_prevention_and_keeps_counter_gate(self):
        for owner in (0,1):
            source='Ob Nixilis, the Fallen'
            defense=self.case(source,'prevention',owner)
            self.assertEqual(self.values(defense,'cast',ref='shield'),[1])
            self.assertEqual(self.values(defense,'assert_life',player=1-owner),[17])
            self.assertEqual(self.values(defense,'assert_counter',ref='source',counter_kind='plus1'),[3])
            decline=self.case(source,'decline',owner)
            self.assertEqual(len(self.steps(decline,'choose_cancel_target',player=owner)),1)
            self.assertEqual(self.values(decline,'assert_counter',ref='source',counter_kind='plus1'),[0])
            self.assertEqual(self.values(self.case('Tunneling Geopede','prevention',owner),'assert_life',player=1-owner),[20])

    def test_optional_fee_uses_tapped_lands_and_exact_spend(self):
        for owner in (0,1):
            name="Seer's Sundial"
            paid=self.case(name,'own',owner)
            short=self.case(name,'fee_short',owner)
            self.assertEqual(self.values(paid,'assert_card',ref='land',property='tapped'),[1])
            self.assertEqual(self.values(paid,'assert_mana',player=owner,color=0)[-1],0)
            self.assertEqual(self.values(short,'assert_mana',player=owner,color=0)[-1],1)
            self.assertEqual(self.values(paid,'assert_zone',player=owner,zone='library'),[5])
            self.assertEqual(self.values(short,'assert_zone',player=owner,zone='library'),[6])
            ranger=self.case('Murasa Ranger','wrong_color',owner)
            self.assertEqual(self.values(ranger,'assert_mana',player=owner,color=5)[-1],1)
            self.assertEqual(self.values(ranger,'assert_counter',ref='source',counter_kind='plus1'),[0])

    def test_murasa_accumulates_from_two_actual_events(self):
        for owner in (0,1):
            case=self.case('Murasa Ranger','repeat',owner)
            self.assertEqual(self.values(case,'assert_counter',ref='source',counter_kind='plus1'),[2,4])
            self.assertEqual(self.values(case,'assert_card',ref='source',property='power'),[5,7])

    def test_actual_tap_cost_and_untap_vs_opposing_land(self):
        for owner in (0,1):
            for name in ('Stone-Seeder Hierophant','Tideforce Elemental'):
                own=self.case(name,'own',owner)
                enemy=self.case(name,'enemy',owner)
                self.assertEqual(self.values(own,'activate',ref='source'),[1])
                activate = own['steps'].index(self.steps(own,'activate',ref='source')[0])
                expected = 2 if name == 'Stone-Seeder Hierophant' else 1
                step = dict(op='choose_target', player=owner, ref='other')
                self.assertEqual(own['steps'][activate-expected:activate], [step]*expected)
                if expected == 2:
                    self.assertEqual(own['steps'][activate+1], dict(op='assert_unused_choices', value=1))
                self.assertEqual(self.values(own,'assert_card',ref='source',property='tapped'),[1,0])
                self.assertEqual(self.values(enemy,'assert_card',ref='source',property='tapped'),[1,1])
                probe=self.case(name,'ability_filter',owner)
                self.assertEqual(self.values(probe,'probe_activation_target',ref='source'),[0])
                fresh=self.case(name,'fresh',owner)
                self.assertEqual(self.values(fresh,'activate',ref='source'),[0])

    def test_missing_source_and_spiral_replay_do_not_fake_entry(self):
        for owner in (0,1):
            name="Seer's Sundial"
            removed=self.case(name,'removed',owner)
            self.assertEqual(len(self.steps(removed,'destroy',ref='source')),1)
            self.assertEqual(self.values(removed,'play_land',ref='land'),[1])
            self.assertEqual(self.values(removed,'assert_zone',player=owner,zone='library'),[6])
            replay=self.case(name,'replay',owner)
            self.assertEqual(self.values(replay,'play_land',ref='land'),[1,1])
            self.assertEqual(self.values(replay,'cast',ref='spiral'),[1,1])
            self.assertEqual(len(self.steps(replay,'assert_recorded_state')),1)
