import json
from pathlib import Path
import unittest

from generate_cases import registry
from next_upkeep_cantrip_riders_recipes import FAMILY, classify, expand, profiles, spec, validate_witnesses, variants


class NextUpkeepCantripRidersTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.oracle=json.loads((Path(__file__).parent/'data/oracle.json').read_bytes())['cards']
        cls.rows={row['name']:row for row in registry()}

    def case(self,name,scene,seat):
        r=self.oracle[name]
        variant=f'{scene}_{seat}'
        d=dict(family=FAMILY,version=2,oracle_text=r['oracle_text'],params=classify(r),variants=[variant])
        result,=expand(name,self.rows[name]['csvid'],d,spec(d,r),[variant],self.rows,self.oracle)
        return result

    def acts(self,c,op,**fields):
        return [s for s in c['steps'] if s['op']==op and all(s.get(k)==v for k,v in fields.items())]

    def values(self,c,op,**fields):
        return [s['value'] for s in self.acts(c,op,**fields)]

    def zone(self,c,seat,zone,name):
        return self.values(c,'assert_zone_multiplicity',player=seat,zone=zone,index=self.rows[name]['csvid'])[-1]

    def test_exact_whole_spell_and_population(self):
        validate_witnesses(self.oracle,self.rows)
        self.assertEqual({r['registry']['csvid'] for r in profiles()['sources'].values()},
                         {2068,1959,2212,2228,2076,2137,2141})
        self.assertEqual(sum(len(variants(classify(self.oracle[n]))) for n in profiles()['sources']),111)
        for name in profiles()['sources']:
            r=self.oracle[name]
            with self.assertRaises(ValueError):
                classify(dict(r,oracle_text=r['oracle_text']+'\nYou gain 1 life.'))
            with self.assertRaises(ValueError):
                spec(dict(family=FAMILY,version=2.0,oracle_text=r['oracle_text'],params=classify(r),variants=['effect_0']),r)

    def test_delayed_draw_on_next_actual_upkeep_and_never_for_counter(self):
        for name in profiles()['sources']:
            for seat in (0,) if name=='Gravebind' else (0,1):
                c=self.case(name,'effect',seat)
                upkeeps=self.acts(c,'upkeep_step')
                self.assertEqual([s['player'] for s in upkeeps],[1-seat,seat])
                first=c['steps'].index(upkeeps[0])
                self.assertEqual(self.values(dict(steps=c['steps'][:first]),'assert_zone',player=seat,zone='hand')[-1],0)
                self.assertEqual(self.values(c,'assert_zone',player=seat,zone='hand')[-1],1)
                self.assertEqual(self.values(c,'assert_zone',player=1-seat,zone='hand')[-1],0)
                n=self.case(name,'countered',seat)
                self.assertEqual(self.values(n,'cast_response',ref='reply'),[1])
                self.assertEqual(self.values(n,'assert_zone',player=seat,zone='hand')[-1],0)
                pair=self.case(name,'repeat',seat)
                self.assertEqual(self.values(pair,'assert_zone',player=seat,zone='hand')[-1],2)

    def test_offturn_ray_draws_for_caster_on_actual_caster_upkeep(self):
        for seat in (0,1):
            c=self.case('Ray of Erasure','offturn',seat)
            self.assertEqual([s['player'] for s in self.acts(c,'upkeep_step')],[1-seat,seat,1-seat])
            cast,=self.acts(c,'cast',ref='source')
            self.assertGreater(c['steps'].index(cast),c['steps'].index(self.acts(c,'upkeep_step')[0]))
            self.assertEqual(self.zone(c,seat,'hand','Forest'),1)
            self.assertEqual(self.zone(c,1-seat,'graveyard','Swamp'),1)
            own=self.case('Ray of Erasure','own_library',seat)
            self.assertEqual(self.zone(own,seat,'graveyard','Forest'),1)
            self.assertEqual(self.zone(own,seat,'hand','Island'),1)

    def test_portent_distinct_order_before_own_draw_and_opponent_shuffle(self):
        own=self.case('Portent','self_order',0)
        self.assertEqual(self.values(own,'choose_zone_card'),[self.rows['Mountain']['csvid']])
        self.assertEqual(self.zone(own,0,'hand','Mountain'),1)
        self.assertEqual(self.values(own,'assert_shuffle_count',player=0)[-1],0)
        for seat in (0,1):
            c=self.case('Portent','shuffle',seat)
            self.assertEqual(self.values(c,'assert_shuffle_count',player=1-seat)[-1],1)
            self.assertEqual(self.zone(c,seat,'hand','Forest'),1)
            short=self.case('Portent','short_target',seat)
            self.assertEqual(self.values(short,'choose_zone',player=seat),[-1])

    def test_headstone_handles_both_zones_and_a_real_invalidating_response(self):
        for seat in (0,1):
            c=self.case('Headstone','effect',seat)
            self.assertEqual(self.zone(c,1-seat,'exile','Grizzly Bears'),1)
            self.assertEqual(self.zone(c,seat,'graveyard','Mountain'),1)
            own=self.case('Headstone','own_grave',seat)
            self.assertEqual(self.zone(own,seat,'exile','Mountain'),1)
            invalid=self.case('Headstone','lost_target',seat)
            self.assertEqual(self.values(invalid,'cast_response',ref='reply'),[1])
            self.assertEqual(self.values(invalid,'assert_zone',player=seat,zone='hand')[-1],0)
            self.assertEqual(self.zone(invalid,1-seat,'hand','Swamp'),1)

    def test_gravebind_scope_and_expiry_use_real_destruction(self):
        record=self.oracle['Gravebind']
        declaration=dict(family=FAMILY,version=2,oracle_text=record['oracle_text'],
                         params=classify(record),variants=['prevent_1'])
        with self.assertRaises(ValueError):
            spec(declaration,record)
        self.assertEqual(self.values(self.case('Gravebind','short',1),'cast',ref='source'),[0])
        lost=self.case('Gravebind','prevent',0)
        self.assertEqual(self.values(lost,'assert_unused_choices')[-1],1)
        self.assertEqual(self.zone(lost,1,'graveyard','Drudge Skeletons'),1)
        unrelated=self.case('Gravebind','unrelated',0)
        self.assertEqual(self.zone(unrelated,1,'battlefield','Drudge Skeletons'),2)
        expired=self.case('Gravebind','expiry',0)
        request,=self.acts(expired,'choose_regeneration',ref='recipient')
        self.assertGreater(expired['steps'].index(request),expired['steps'].index(self.acts(expired,'upkeep_step')[0]))
        self.assertEqual(self.zone(expired,0,'hand','Forest'),1)

    def test_other_riders_use_actual_life_loyalty_and_combat(self):
        for seat in (0,1):
            for scene,life in (('effect',21),('nonland',20),('empty_target',20)):
                prophecy=self.case('Prophecy',scene,seat)
                self.assertFalse(self.acts(prophecy,'choose_player'))
                self.assertEqual(self.values(prophecy,'assert_life',player=seat)[-1],life)
                self.assertEqual(self.values(prophecy,'assert_shuffle_count',player=1-seat)[-1],1)
            walker=self.case('Touch of Death','walker',seat)
            self.assertEqual(self.values(walker,'assert_counter',ref='walker',counter_kind='loyalty')[-1],2)
            self.assertEqual(self.values(walker,'assert_life',player=seat)[-1],21)
            ground=self.case('Updraft','expiry',seat)
            self.assertEqual(self.values(ground,'block',ref='blocker'),[0,1])
            self.assertEqual(self.values(ground,'assert_life',player=1-seat)[-1],18)

    def test_snapshot_repeats_immediate_and_delayed_parts(self):
        for name in profiles()['sources']:
            for seat in (0,) if name=='Gravebind' else (0,1):
                scene=self.case(name,'replay',seat)
                steps=scene['steps']
                a,b=steps.index(dict(op='capture')),steps.index(dict(op='record_state'))
                self.assertEqual(steps[a+1:b],steps[b+3:-1])
                self.assertEqual(steps[-1]['op'],'assert_recorded_state')
