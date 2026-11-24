"""Focused guards for the required-attack lifecycle fixture protocols."""

import json
from pathlib import Path
import unittest

from generate_cases import registry
from mandatory_attack_lifecycle_riders_recipes import FAMILY, classify, expand, profiles, spec, validate_witnesses, variants


class MandatoryAttackLifecycleRidersTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.oracle=json.loads((Path(__file__).parent/'data/oracle.json').read_bytes())['cards']
        cls.rows={row['name']:row for row in registry()}

    def case(self,name,scene,seat):
        variant=f'{scene}_{seat}'
        record=self.oracle[name]
        d=dict(family=FAMILY,version=2,oracle_text=record['oracle_text'],params=classify(record),variants=[variant])
        result,=expand(name,self.rows[name]['csvid'],d,spec(d,record),[variant],self.rows,self.oracle)
        return result['steps']

    def matching(self,steps,op,**fields):
        return [s for s in steps if s['op']==op and all(s.get(key)==value for key,value in fields.items())]

    def values(self,steps,op,**fields):
        return [s['value'] for s in self.matching(steps,op,**fields)]

    def zones(self,steps,seat,zone,name):
        return self.values(steps,'assert_zone_multiplicity',player=seat,zone=zone,index=self.rows[name]['csvid'])

    def test_exact_whole_sources_and_variants(self):
        validate_witnesses(self.oracle,self.rows)
        ids={int(s['registry']['csvid']) for s in profiles()['sources'].values()}
        self.assertEqual(ids,{747,4070,11069,11499,13419,15035})
        self.assertEqual(sum(len(variants(classify(self.oracle[name]))) for name in profiles()['sources']),136)
        for name in profiles()['sources']:
            record=self.oracle[name]
            with self.assertRaises(ValueError):
                classify(dict(record,oracle_text=record['oracle_text']+'\nFlying'))
            shape=classify(record)
            with self.assertRaises(ValueError):
                spec(dict(family=FAMILY,version=2,oracle_text=record['oracle_text'],
                          params=dict(shape,power=float(shape['power'])),variants=['omitted_0']),record)

    def test_required_acceptance_tracks_real_cast_disability_and_owner(self):
        for name in profiles()['sources']:
            for owner in (0,1):
                for scene,legal in (('omitted',1),('tapped',0),('sick',0),('pacified',0),('paid_fresh',0)):
                    steps=self.case(name,scene,owner)
                    self.assertEqual(self.values(steps,'can_finish_attackers'),[1-legal,1])
                    self.assertEqual(self.values(steps,'attack',ref='source'),[legal])
                off=self.case(name,'opponent_turn',owner)
                self.assertFalse(self.matching(off,'attack',ref='source'))
                self.assertEqual(self.values(off,'attack',ref='ally'),[1])

    def test_ooze_scales_real_counters_before_payment_and_tapping(self):
        for owner in (0,1):
            paid=self.case('Primordial Ooze','upkeep_pay',owner)
            self.assertEqual([s['player'] for s in self.matching(paid,'upkeep_step')],[1-owner,owner])
            self.assertEqual(self.values(paid,'assert_counter',ref='source',counter_kind='plus1')[-1],1)
            self.assertEqual(len(self.matching(paid,'choose_mana_source')),int(owner==0))
            self.assertEqual(self.values(paid,'assert_life',player=owner)[-1],20)
            self.assertEqual(self.values(paid,'assert_life',player=1-owner)[-1],18)
            short=self.case('Primordial Ooze','upkeep_scaled',owner)
            self.assertEqual(self.values(short,'assert_counter',ref='source',counter_kind='plus1')[-1],2)
            self.assertEqual(self.values(short,'assert_life',player=owner)[-1],17)
            self.assertEqual(self.values(short,'attack',ref='source'),[0,0])
            off=self.case('Primordial Ooze','upkeep_opponent',owner)
            self.assertFalse(self.matching(off,'assert_counter',ref='source'))
        cancel=self.case('Primordial Ooze','upkeep_cancel',0)
        self.assertEqual(len(self.matching(cancel,'choose_cancel_mana')),1)
        self.assertEqual(self.values(cancel,'assert_life',player=0)[-1],19)

    def test_echo_is_paid_once_following_real_entry(self):
        for owner in (0,1):
            paid=self.case('Thran War Machine','echo_once',owner)
            self.assertEqual(self.values(paid,'cast',ref='source'),[1])
            self.assertEqual([s['player'] for s in self.matching(paid,'upkeep_step')],[1-owner,owner,1-owner,owner])
            self.assertEqual(self.values(paid,'choose_dialog',player=owner),[0])
            self.assertEqual(len(self.matching(paid,'choose_mana_source')),4 if owner==0 else 0)
            self.assertEqual(self.values(paid,'attack',ref='source'),[1,1])
            decline=self.case('Thran War Machine','echo_decline',owner)
            self.assertEqual(self.values(decline,'choose_dialog',player=owner),[1])
            self.assertFalse(self.matching(decline,'attack',ref='source'))
            self.assertEqual(self.zones(decline,owner,'graveyard','Thran War Machine')[-1],1)

    def test_annihilator_declaration_selects_defenders_permanents_before_damage(self):
        for owner in (0,1):
            two=self.case("Ulamog's Crusher",'annihilator_two',owner)
            picks=self.matching(two,'choose_target')
            self.assertEqual([s['player'] for s in picks],[0,0] if owner==1 else [])
            finish,=self.matching(two,'finish_attackers')
            damage,=self.matching(two,'combat_damage')
            receipts=two[two.index(finish):two.index(damage)]
            self.assertEqual(self.zones(receipts,1-owner,'graveyard','Island'),[1])
            self.assertEqual(self.zones(receipts,1-owner,'graveyard','Sol Ring'),[1])
            self.assertEqual(self.zones(two,owner,'battlefield','Forest')[-1],1)
        alternative=self.case("Ulamog's Crusher",'annihilator_choice',1)
        self.assertEqual(self.zones(alternative,0,'battlefield','Island')[-1],1)
        self.assertEqual(self.zones(alternative,0,'graveyard','Mountain')[-1],2)

    def test_infect_gets_poison_and_persistent_toughness_counters(self):
        for owner in (0,1):
            player=self.case('Phyrexian Juggernaut','omitted',owner)
            self.assertEqual(self.values(player,'assert_poison',player=1-owner)[-1],5)
            self.assertEqual(self.values(player,'assert_life',player=1-owner)[-1],20)
            blocked=self.case('Phyrexian Juggernaut','infect_blocked',owner)
            self.assertEqual(self.values(blocked,'assert_counter',ref='blocker',counter_kind='minus1')[-1],5)
            self.assertEqual(self.values(blocked,'assert_card',ref='blocker',property='toughness')[-1],2)
            other=self.case('Phyrexian Juggernaut','infect_noncombat',owner)
            self.assertEqual(self.values(other,'activate',ref='aura'),[1])
            self.assertEqual(self.values(other,'assert_counter',ref='blocker',counter_kind='minus1')[-1],1)

    def test_regeneration_checks_paid_save_short_wrong_color_and_proactive_window(self):
        for owner in (0,1):
            for scene,paid in (('regenerate_paid',True),('regenerate_short',False),('regenerate_wrong_color',False)):
                steps=self.case('Deathbellow Raider',scene,owner)
                self.assertEqual(len(self.matching(steps,'destroy',ref='source')),1)
                self.assertEqual(self.values(steps,'assert_unused_choices')[-1],int(not paid))
                self.assertEqual(self.zones(steps,owner,'graveyard','Deathbellow Raider')[-1],int(not paid))
            proactive=self.case('Deathbellow Raider','regenerate_proactive',owner)
            self.assertEqual(self.values(proactive,'can_activate',ref='source'),[1])
            self.assertFalse(self.matching(proactive,'destroy'))

    def test_dash_cost_haste_end_and_replay_are_distinct_from_normal(self):
        for owner in (0,1):
            exact=self.case('Sprinting Warbrute','dash_exact',owner)
            self.assertFalse(self.matching(exact,'choose_dialog'))
            self.assertEqual(self.values(exact,'attack',ref='source'),[1])
            self.assertEqual(self.values(exact,'assert_card',ref='source',property='in_hand')[-1],1)
            menu=self.case('Sprinting Warbrute','dash_menu',owner)
            self.assertEqual(self.values(menu,'choose_dialog',player=owner),[0])
            self.assertEqual(self.values(menu,'assert_mana_total',player=owner),[1])
            normal=self.case('Sprinting Warbrute','normal_menu',owner)
            self.assertEqual(self.values(normal,'choose_dialog',player=owner),[1])
            self.assertEqual(self.values(normal,'attack',ref='source'),[0])
            self.assertEqual(self.zones(normal,owner,'battlefield','Sprinting Warbrute')[-1],1)
            replay=self.case('Sprinting Warbrute','dash_replay',owner)
            a,b=replay.index(dict(op='capture')),replay.index(dict(op='record_state'))
            self.assertEqual(replay[a+1:b],replay[b+3:-1])
            self.assertEqual(replay[-1]['op'],'assert_recorded_state')
