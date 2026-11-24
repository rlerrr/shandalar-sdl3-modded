"""Focused guards for paid infect and live rider fixtures."""

import json
from pathlib import Path
import unittest

from generate_cases import registry
from infect_combat_riders_recipes import FAMILY, classify, expand, profiles, spec, validate_witnesses, variants


class InfectCombatRidersTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.oracle=json.loads((Path(__file__).parent/'data/oracle.json').read_bytes())['cards']
        cls.rows={row['name']:row for row in registry()}

    def case(self,name,mode,seat):
        record=self.oracle[name]
        shape=classify(record)
        variant=f'{mode}_{seat}'
        d=dict(family=FAMILY,version=2,oracle_text=record['oracle_text'],params=shape,variants=[variant])
        game,=expand(name,self.rows[name]['csvid'],d,spec(d,record),[variant],self.rows,self.oracle)
        return game['steps']

    def values(self,actions,op,**fields):
        return [s['value'] for s in actions if s['op']==op and all(s.get(key)==value for key,value in fields.items())]

    def test_complete_oracle_and_typed_identity(self):
        validate_witnesses(self.oracle,self.rows)
        self.assertEqual(sum(len(variants(classify(self.oracle[n]))) for n in profiles()['sources']),240)
        for name in profiles()['sources']:
            record=self.oracle[name]
            with self.assertRaises(ValueError):
                classify(dict(record,oracle_text=record['oracle_text']+'\nFlying'))
            params=classify(record)
            with self.assertRaises(ValueError):
                spec(dict(family=FAMILY,version=2,oracle_text=record['oracle_text'],
                          params=dict(params,power=float(params['power'])),variants=['unblocked_0']),record)

    def test_agent_evasion_does_not_stop_agent_blocking(self):
        for seat in (0,1):
            attack=self.case('Blighted Agent','blocked',seat)
            self.assertEqual(self.values(attack,'block',ref='blocker',other='source'),[0])
            self.assertEqual(self.values(attack,'assert_poison',player=1-seat)[-1],1)
            self.assertEqual(self.values(attack,'assert_life',player=1-seat)[-1],20)
            defend=self.case('Blighted Agent','defending',seat)
            self.assertEqual(self.values(defend,'block',ref='source',other='attacker'),[1])
            self.assertEqual(self.values(defend,'assert_card',ref='attacker',property='minus1_counters')[-1],1)

    def test_chained_attack_requires_defender_poison_from_real_combat(self):
        for seat in (0,1):
            positive=self.case('Chained Throatseeker','unblocked',seat)
            self.assertEqual(self.values(positive,'attack',ref='primer'),[1])
            self.assertEqual(self.values(positive,'attack',ref='source'),[1])
            self.assertEqual(self.values(positive,'assert_poison',player=1-seat)[-1],6)
            own=self.case('Chained Throatseeker','self_only',seat)
            self.assertEqual(self.values(own,'assert_poison',player=seat)[-1],1)
            self.assertEqual(self.values(own,'assert_poison',player=1-seat)[-1],0)
            self.assertEqual(self.values(own,'attack',ref='source'),[0])
            defense=self.case('Chained Throatseeker','defending',seat)
            self.assertEqual(self.values(defense,'block',ref='source'),[1])

    def test_ichor_rats_paid_etb_both_recast_and_counter(self):
        for seat in (0,1):
            replay=self.case('Ichor Rats','recast',seat)
            self.assertEqual(self.values(replay,'cast',ref='source'),[1,1])
            self.assertEqual(self.values(replay,'cast',ref='return_spell'),[1])
            self.assertEqual(self.values(replay,'assert_poison',player=seat)[-1],2)
            self.assertEqual(self.values(replay,'assert_poison',player=1-seat)[-1],2)
            cancelled=self.case('Ichor Rats','countered',seat)
            self.assertEqual(self.values(cancelled,'cast_response',ref='counter'),[1])
            self.assertFalse(any(self.values(cancelled,'assert_poison',player=who)[-1] for who in (0,1)))

    def test_myr_block_pumps_before_persistent_damage_and_resets(self):
        for seat in (0,1):
            steps=self.case('Ichorclaw Myr','cleanup',seat)
            first=steps.index(dict(op='advance_combat'))
            second=steps.index(dict(op='combat_damage'))
            self.assertEqual(self.values(steps[first:second],'assert_card',ref='source',property='power'),[3])
            self.assertEqual(self.values(steps,'assert_card',ref='blocker',property='minus1_counters')[-1],3)
            self.assertEqual(self.values(steps,'assert_card',ref='source',property='power')[-1],1)

    def test_septic_rats_foe_only_status_and_cleanup(self):
        for seat in (0,1):
            base=self.case('Septic Rats','self_only',seat)
            self.assertEqual(self.values(base,'assert_poison',player=1-seat)[-1],2)
            positive=self.case('Septic Rats','foe_poisoned',seat)
            self.assertEqual(self.values(positive,'assert_poison',player=1-seat)[-1],4)
            block=self.case('Septic Rats','poisoned_block',seat)
            self.assertEqual(self.values(block,'assert_card',ref='source',property='power')[-1],2)
            self.assertEqual(self.values(block,'assert_card',ref='blocker',property='minus1_counters')[-1],3)

    def test_reaper_counts_each_source_controller_and_prevention(self):
        for seat in (0,1):
            steps=self.case('Reaper of Sheoldred','mixed_sources',seat)
            self.assertEqual(self.values(steps,'cast',ref='enemy_spell'),[1])
            self.assertEqual(self.values(steps,'cast',ref='friendly_spell'),[1])
            self.assertEqual(self.values(steps,'assert_card',ref='source',property='damage')[-1],4)
            self.assertEqual(self.values(steps,'assert_poison',player=seat)[-1],1)
            self.assertEqual(self.values(steps,'assert_poison',player=1-seat)[-1],1)
            prevented=self.case('Reaper of Sheoldred','fogged',seat)
            self.assertEqual(self.values(prevented,'cast',ref='fog'),[1])
            self.assertEqual(self.values(prevented,'assert_poison',player=1-seat)[-1],0)

    def test_snapshot_reexecutes_actual_paid_and_combat_paths(self):
        for seat in (0,1):
            steps=self.case('Ichorclaw Myr','replay',seat)
            a,b=steps.index(dict(op='capture')),steps.index(dict(op='record_state'))
            self.assertEqual(steps[a+1:b],steps[b+3:-1])
            self.assertEqual(steps[-1]['op'],'assert_recorded_state')

    def test_bypass_keeps_inverse_damage_and_persistent_infect_distinct(self):
        for owner in (0,1):
            bypass=self.case('Spinebiter','bypass',owner)
            normal=self.case('Spinebiter','normal',owner)
            self.assertEqual(self.values(bypass,'choose_dialog',player=owner),[1])
            self.assertEqual(self.values(normal,'choose_dialog',player=owner),[0])
            self.assertEqual(self.values(bypass,'assert_poison',player=1-owner)[-1],3)
            self.assertEqual(self.values(bypass,'assert_card',ref='blocker',property='minus1_counters')[-1],0)
            self.assertEqual(self.values(normal,'assert_card',ref='blocker',property='minus1_counters')[-1],3)

    def test_paid_regeneration_and_real_upkeep_receipts(self):
        for owner in (0,1):
            for name,price in (('Blight Mamba',[1,0,0,1,0,0]),('Toxic Nim',[0,1,0,0,0,0])):
                case=self.case(name,'regen_paid',owner)
                destroy=case.index(dict(op='destroy',ref='source'))
                self.assertIn(dict(op='choose_regeneration',ref='source'),case[destroy-2:destroy])
                self.assertEqual(self.values(case[destroy:],'assert_card',ref='source',property='tapped'),[1])
                self.assertEqual(self.values(case[destroy-8:destroy],'set_mana',player=owner),price)
            each=self.case('Phyrexian Vatmother','upkeep_each',owner)
            removed=self.case('Phyrexian Vatmother','upkeep_removed',owner)
            self.assertEqual(self.values(each,'assert_poison',player=owner)[-1],2)
            self.assertEqual(self.values(removed,'assert_poison',player=owner)[-1],1)

    def test_wolf_uses_actual_death_of_damaged_creature_before_cleanup(self):
        for owner in (0,1):
            for mode,draw in (('wolf_draw',1),('wolf_decline',0),('wolf_next_turn',0),('wolf_unrelated',0)):
                steps=self.case('Rot Wolf',mode,owner)
                self.assertEqual(self.values(steps,'assert_zone',player=owner,zone='hand')[-1],draw)
                self.assertEqual(self.values(steps,'assert_zone',player=owner,zone='library')[-1],1-draw)
                self.assertIn(dict(op='destroy',ref='other_creature' if mode=='wolf_unrelated' else 'blocker'),steps)

    def test_hand_poison_is_from_paid_cast_before_counter(self):
        for owner in (0,1):
            steps=self.case('Hand of the Praetors','infect_countered',owner)
            index=steps.index(dict(op='cast_response',ref='counter',value=1))
            self.assertEqual(self.values(steps[:index],'assert_poison',player=1-owner)[-1],1)
            self.assertEqual(self.values(steps,'assert_card',ref='friendly',property='power')[-1],2)
            self.assertEqual(self.values(steps,'assert_card',ref='foreign',property='power')[-1],1)

    def test_specter_discard_quantity_uses_new_poison_and_offer(self):
        for owner in (0,1):
            take=self.case('Whispering Specter','discard_two',owner)
            decline=self.case('Whispering Specter','decline',owner)
            self.assertEqual(self.values(take,'assert_poison',player=1-owner)[-1],2)
            self.assertEqual(self.values(take,'assert_zone',player=1-owner,zone='hand')[-1],1)
            self.assertEqual(self.values(decline,'assert_zone',player=1-owner,zone='hand')[-1],3)
            self.assertEqual(self.values(take,'choose_dialog',player=owner),[0])
            self.assertEqual(self.values(decline,'choose_dialog',player=owner),[1])

    def test_swarm_counts_opponent_only_and_insect_combat_is_infect(self):
        for owner in (0,1):
            scale=self.case('Phyrexian Swarmlord','swarm_scale',owner)
            deny=self.case('Phyrexian Swarmlord','swarm_self_only',owner)
            self.assertEqual(self.values(scale,'bind_permanent_copy',player=owner),[18770]*6)
            self.assertEqual(self.values(scale,'attack',ref='insect_0'),[1])
            self.assertEqual(self.values(scale,'assert_poison',player=1-owner)[-1],6)
            self.assertEqual(self.values(scale,'attack',ref='source'),[1])
            self.assertEqual(self.values(scale,'assert_card',ref='source',property='tapped')[-1],0)
            replay=self.case('Phyrexian Swarmlord','replay',owner)
            self.assertEqual(self.values(replay,'assert_matching_permanents',property='subtype_Phyrexian'),[1,1,1,1])
            self.assertEqual(self.values(replay,'attack',ref='primer'),[1,1,1,1])
            self.assertEqual(self.values(replay,'cast',ref='primer_growth'),[1,1])
            self.assertEqual(self.values(replay,'assert_card',ref='source',property='tapped')[-1],0)
            direct=self.case('Phyrexian Swarmlord','unblocked',owner)
            self.assertEqual(self.values(direct,'attack',ref='source'),[1])
            self.assertEqual(self.values(direct,'assert_poison',player=1-owner)[-1],4)
            self.assertFalse(self.values(deny,'bind_permanent_copy',player=owner))
