import json
from pathlib import Path
import unittest

from cast_reducers_with_riders_recipes import FAMILY, classify, expand, profiles, spec, validate_witnesses, variants
from generate_cases import registry
from recipes import COLORS


class CastReducersWithRidersTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.oracle = json.loads((Path(__file__).parent/'data/oracle.json').read_bytes())['cards']
        cls.rows = {entry['name']:entry for entry in registry()}

    def case(self, name, scene, seat):
        record = self.oracle[name]
        chosen = f'{scene}_{seat}'
        d = dict(family=FAMILY,version=2,params=classify(record),oracle_text=record['oracle_text'],variants=[chosen])
        c, = expand(name,self.rows[name]['csvid'],d,spec(d,record),[chosen],self.rows,self.oracle)
        return c

    def acts(self,c,op,**fields):
        return [s for s in c['steps'] if s['op']==op and all(s.get(k)==v for k,v in fields.items())]

    def values(self,c,op,**fields):
        return [s['value'] for s in self.acts(c,op,**fields)]

    def zone(self,c,player,zone,name):
        return self.values(c,'assert_zone_multiplicity',player=player,zone=zone,index=self.rows[name]['csvid'])[-1]

    def test_strict_whole_oracle_and_implemented_source_profiles(self):
        validate_witnesses(self.oracle,self.rows)
        names = profiles()['sources']
        self.assertEqual({entry['registry']['csvid'] for entry in names.values()},{6545,7297,17247,13732})
        self.assertEqual(sum(len(variants(classify(self.oracle[name]))) for name in names),118)
        for name in names:
            record = self.oracle[name]
            with self.assertRaises(ValueError):
                classify(dict(record,oracle_text=record['oracle_text']+'\nYou gain 2 life.'))
            with self.assertRaises(ValueError):
                spec(dict(family=FAMILY,version=2.0,params=classify(record),oracle_text=record['oracle_text'],variants=['match_0']),record)

    def test_real_full_budget_residual_and_insufficient_matching_pool(self):
        for name in profiles()['sources']:
            for seat in (0,1):
                scene = self.case(name,'surplus',seat)
                self.assertEqual(self.values(scene,'cast',ref='member'),[1])
                self.assertEqual(self.values(scene,'assert_mana',player=seat,color=0)[-1],classify(self.oracle[name])['reduction'])
                denied = self.case(name,'short',seat)
                self.assertEqual(self.values(denied,'cast',ref='member'),[0])
                self.assertFalse(self.acts(denied,'announce_spell',ref='member'))
                for variant in ('removed','nonmatch','opponent'):
                    no = self.case(name,variant,seat)
                    self.assertEqual(self.values(no,'cast',ref='member'),[0,1])

    def test_warchief_real_enemy_beast_and_wrong_recipient(self):
        name = 'Krosan Warchief'
        for seat in (0,1):
            enemy = self.case(name,'save_enemy',seat)
            request, = self.acts(enemy,'choose_regeneration',ref='source')
            target, = self.acts(enemy,'choose_target',ref='recipient',player=seat)
            self.assertEqual(enemy['steps'].index(target),enemy['steps'].index(request)+1)
            self.assertEqual(self.zone(enemy,1-seat,'battlefield','Rumbling Baloth'),1)
            self.assertEqual(self.values(enemy,'assert_card',ref='recipient',property='tapped')[-1],1)
            self.assertEqual(self.values(enemy,'assert_mana',player=seat,color=COLORS['G'])[-1],0)
            if seat == 1:
                end, = self.acts(enemy,'end_combat')
                removal, = self.acts(enemy,'cast',ref='removal')
                self.assertLess(enemy['steps'].index(end),enemy['steps'].index(removal))
            else:
                self.assertFalse(self.acts(enemy,'end_combat'))
            wrong = self.case(name,'wrong_type',seat)
            self.assertIn(1,self.values(wrong,'assert_unused_choices'))
            self.assertEqual(self.zone(wrong,seat,'graveyard','Grizzly Bears'),1)
            proactive = self.case(name,'proactive',seat)
            self.assertEqual(self.values(proactive,'can_activate',ref='source'),[1])
            self.assertFalse(self.acts(proactive,'activate'))
            self.assertFalse(self.acts(proactive,'choose_target'))
            self.assertEqual(self.zone(proactive,seat,'battlefield','Rumbling Baloth'),1)

    def test_gohei_reduces_arcane_and_only_own_spirits_grow(self):
        name = 'Long-Forgotten Gohei'
        for owner in (0,1):
            c = self.case(name,'match',owner)
            self.assertEqual(self.values(c,'set_mana',player=owner,color=0)[-1],0)
            self.assertEqual(self.values(c,'assert_life',player=1-owner)[-1],18)
            self.assertEqual(self.values(c,'assert_card',ref='spirit_own',property='power')[-1],2)
            self.assertEqual(self.values(c,'assert_card',ref='spirit_foe',property='power')[-1],1)
            self.assertEqual(self.values(c,'assert_card',ref='unboosted',property='power')[-1],2)
            later = self.case(name,'spirit_late',owner)
            self.assertEqual(self.values(later,'cast',ref='member'),[0,1])
            self.assertEqual(self.values(later,'assert_card',ref='member',property='toughness')[-1],8)
            removed = self.case(name,'removed',owner)
            self.assertEqual(self.values(removed,'assert_card',ref='spirit_own',property='toughness')[-1],7)

    def test_unesh_self_other_and_real_noncast_entries(self):
        name = 'Unesh, Criosphinx Sovereign'
        for seat in (0,1):
            c = self.case(name,'other_piles',seat)
            self.assertEqual(self.zone(c,seat,'hand','Island'),2)
            self.assertEqual(self.zone(c,seat,'graveyard','Island'),2)
            self.assertEqual(self.zone(c,seat,'hand','Forest'),1)
            self.assertEqual(len(self.acts(c,'choose_zone',player=0)),2 if seat==0 else 5)
            stopped = self.case(name,'countered',seat)
            self.assertEqual(self.values(stopped,'assert_zone',player=seat,zone='library')[-1],1)
            placed = self.case(name,'reanimate_piles',seat)
            self.assertEqual(self.values(placed,'cast',ref='revival'),[1])
            choose, = self.acts(placed,'choose_zone_card',player=seat)
            cast, = self.acts(placed,'cast',ref='revival')
            self.assertEqual(placed['steps'].index(choose)+1,placed['steps'].index(cast))
            self.assertTrue(self.acts(placed,'bind_permanent_copy',ref='revived'))
            self.assertEqual(self.zone(placed,seat,'hand','Forest'),1)

    def test_unesh_actual_ground_negative_and_reach_block(self):
        name = 'Unesh, Criosphinx Sovereign'
        for seat in (0,1):
            ground = self.case(name,'fly_ground',seat)
            self.assertEqual(self.values(ground,'block',ref='blocker'),[0])
            self.assertEqual(self.values(ground,'assert_life',player=1-seat)[-1],16)
            reach = self.case(name,'fly_reach',seat)
            self.assertEqual(self.values(reach,'block',ref='blocker'),[1])
            self.assertEqual(self.values(reach,'assert_card',ref='source',property='damage')[-1],2)

    def test_heroic_target_scope_and_paid_aura_counter(self):
        name = 'Hero of Iroas'
        for seat in (0,1):
            for scene, counters in (('match',1),('countered',1),('offcolor_aura',1),('other_target',0),('opponent_target',0),('instant_heroic',1)):
                c = self.case(name,scene,seat)
                self.assertEqual(self.values(c,'assert_counter',ref='source',counter_kind='plus1'),[counters])
                self.assertEqual(self.values(c,'assert_card',ref='source',property='plus1_counters')[-1],counters)
            c = self.case(name,'instant_heroic',seat)
            self.assertEqual(self.values(c,'assert_card',ref='source',property='power')[-2:],[6,3])
            c = self.case(name,'counter_persists',seat)
            self.assertEqual(self.zone(c,seat,'hand','Pacifism'),1)
            self.assertEqual(self.values(c,'assert_card',ref='source',property='power')[-1],3)

    def test_replay_repeats_the_exact_sequence(self):
        for name in profiles()['sources']:
            for seat in (0,1):
                c = self.case(name,'replay',seat)
                steps = c['steps']
                a,b = steps.index(dict(op='capture')),steps.index(dict(op='record_state'))
                self.assertEqual(steps[a+1:b],steps[b+3:-1])
                self.assertEqual(steps[-1]['op'],'assert_recorded_state')
