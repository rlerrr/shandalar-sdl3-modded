"""Validate oracle tier boundaries and the exact live Level Up fixtures."""

import json
from pathlib import Path
import unittest

from generate_cases import registry
from level_resource_riders_recipes import FAMILY, classify, expand, profiles, spec, validate_witnesses, variants


class LevelResourceTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.oracle=json.loads((Path(__file__).parent/'data/oracle.json').read_bytes())['cards']
        cls.rows={row['name']:row for row in registry()}

    def case(self,name,mode,owner):
        record=self.oracle[name]
        shape=classify(record)
        variant=f'{mode}_{owner}'
        declaration=dict(family=FAMILY,version=2,oracle_text=record['oracle_text'],params=shape,variants=[variant])
        case,=expand(name,self.rows[name]['csvid'],declaration,spec(declaration,record),[variant],self.rows,self.oracle)
        return case['steps']

    def steps(self,case,op,**fields):
        return [x for x in case if x['op']==op and all(x.get(k)==v for k,v in fields.items())]

    def values(self,case,op,**fields):
        return [x['value'] for x in self.steps(case,op,**fields)]

    def test_complete_level_bands_pinned_and_typed(self):
        validate_witnesses(self.oracle,self.rows)
        sources=list(profiles()['sources'])
        self.assertEqual(sum(len(variants(classify(self.oracle[name]))) for name in sources),88)
        for name in sources:
            record=self.oracle[name]
            shape=classify(record)
            with self.assertRaises(ValueError):
                classify(dict(record,oracle_text=record['oracle_text']+'\nTrample'))
            corrupt=dict(shape,activation=[float(n) for n in shape['activation']])
            with self.assertRaises(ValueError):
                spec(dict(family=FAMILY,version=2,oracle_text=record['oracle_text'],params=corrupt,
                          variants=['short_0']),record)

    def test_level_up_stays_paid_separate_from_resolution(self):
        steps=self.case('Hedron-Field Purists','two_per_packet',0)
        acts=self.steps(steps,'activate',ref='source')
        self.assertEqual(len(acts),5)
        cast_end=next(i for i,s in enumerate(steps) if s['op']=='cast')
        self.assertLess(cast_end,steps.index(acts[0]))
        positions=[i for i,s in enumerate(steps) if s['op']=='activate' and s.get('ref')=='source']
        stats=[]
        for index in positions:
            resolve=next(i for i in range(index,len(steps)) if steps[i]['op']=='resolve')
            self.assertEqual(self.values(steps[index:resolve],'assert_stack'),[1])
            stats.append(self.values(steps[index:resolve],'assert_card',ref='source',property='power')[-1])
        self.assertEqual(stats,[0,1,1,1,1])
        for seat in (0,1):
            for mode in ('short','opponent_timing'):
                denied=self.case('Hedron-Field Purists',mode,seat)
                self.assertEqual(self.values(denied,'activate',ref='source'),[0])

    def test_coralhelm_flight_and_actual_lord_scope(self):
        for seat in (0,1):
            old=self.case('Coralhelm Commander','before_tier',seat)
            fly=self.case('Coralhelm Commander','fly',seat)
            air=self.case('Coralhelm Commander','fly_blocked',seat)
            self.assertEqual(self.values(old,'block'),[1])
            self.assertEqual(self.values(fly,'block'),[0])
            self.assertEqual(self.values(air,'block'),[1])
            leave=self.case('Coralhelm Commander','lord_removed',seat)
            for ref,expected in [('ally',[2,1]),('foreign',[1,1]),('different',[2,2])]:
                self.assertEqual(self.values(leave,'assert_card',ref=ref,property='power'),expected)

    def test_prevention_is_per_real_packet_and_scoped_to_own(self):
        for seat in (0,1):
            one=self.case('Hedron-Field Purists','one_per_packet',seat)
            self.assertEqual(self.values(one,'assert_life',player=seat),[18,16,16,16])
            two=self.case('Hedron-Field Purists','two_per_packet',seat)
            self.assertEqual(self.values(two,'assert_card',ref='ally',property='damage')[-1],1)
            self.assertEqual(self.values(two,'assert_card',ref='foreign',property='damage')[-1],3)
            walker=self.case('Hedron-Field Purists','walker',seat)
            self.assertEqual(self.values(walker,'assert_counter',ref='walker',counter_kind='loyalty'),[3])
            self.assertEqual(self.values(walker,'assert_zone_contains',player=seat,zone='graveyard'),
                             [self.rows['Jace Beleren']['csvid']])

    def test_elf_mana_production_funds_real_follow_on(self):
        for seat in (0,1):
            proxy=self.case('Joraga Treespeaker','mana_proxy',seat)
            cost=proxy.index(dict(op='cast',ref='spend',value=1))
            tap=next(i for i in range(cost-1,-1,-1) if proxy[i]['op']=='activate')
            self.assertEqual(proxy[tap-1],dict(op='choose_target',player=seat,ref='ally'))
            self.assertIn(dict(op='assert_mana',player=seat,color=3,value=2),proxy[tap:cost])
            self.assertFalse(self.steps(proxy[tap:cost],'set_mana'))
            generic=[dict(op='choose_mana',player=0,color=3)] if seat==0 else []
            self.assertEqual(self.steps(proxy[tap:cost],'choose_mana'),generic)
            self.assertEqual(proxy[cost-1],dict(op='choose_target',player=seat,ref='artifact'))
            bearer=self.case('Joraga Treespeaker','mana_bearer',seat)
            self.assertEqual(self.values(bearer,'can_activate',ref='ally'),[1])
            self.assertEqual(self.values(bearer,'can_activate',ref='foreign'),[0])

    def test_kabira_levels_preserve_source_and_enemy_scope(self):
        for seat in (0,1):
            low=self.case('Kabira Vindicator','tier_one',seat)
            high=self.case('Kabira Vindicator','lord_removed',seat)
            self.assertEqual(self.values(low,'assert_card',ref='ally',property='power'),[2])
            self.assertEqual(self.values(high,'assert_card',ref='ally',property='power'),[3,1])
            self.assertEqual(self.values(high,'assert_card',ref='foreign',property='power'),[1,1])
            self.assertEqual(self.values(low,'assert_card',ref='source',property='power')[-1],3)

    def test_lighthouse_leaves_real_scheduler_state_live(self):
        name='Lighthouse Chronologist'
        for seat in (0,1):
            high=self.case(name,'turn_grant',seat)
            self.assertEqual(self.values(high,'advance_scheduled_turn'),[1-seat,seat,seat,1-seat])
            first=next(i for i,s in enumerate(high) if s['op']=='advance_scheduled_turn')
            self.assertFalse(self.steps(high[first:],'turn'))
            for mode in ('before_tier','removed_timer'):
                steps=self.case(name,mode,seat)
                self.assertEqual(self.values(steps,'advance_scheduled_turn'),[1-seat,seat,1-seat])

    def test_sweep_marks_the_actual_defender_before_combat_damage(self):
        for seat in (0,1):
            top=self.case('Lord of Shatterskull Pass','attack_trigger',seat)
            finish=top.index(dict(op='finish_attackers'))
            block=top.index(dict(op='begin_blockers'))
            receipt=top[finish:block]
            self.assertEqual(self.values(receipt,'assert_card',ref='wall',property='damage'),[6])
            self.assertEqual(self.values(receipt,'assert_life',player=1-seat),[20])
            self.assertEqual(self.values(receipt,'assert_zone_contains',player=1-seat,zone='graveyard'),
                             [self.rows['Grizzly Bears']['csvid']])
            self.assertEqual(self.values(top,'assert_life',player=1-seat)[-1],14)
            lower=self.case('Lord of Shatterskull Pass','before_tier',seat)
            self.assertEqual(self.values(lower,'assert_card',ref='wall',property='damage')[-1],0)
