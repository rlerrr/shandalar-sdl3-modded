"""Exact whole faces and actual preceding-turn activity for Werewolf fixtures."""
import json
import unittest
from pathlib import Path

from generate_cases import registry
import werewolf_spell_history_recipes as model


class WerewolfHistoryTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        data=Path(__file__).parent/'data'
        cls.oracle=json.loads((data/'oracle.json').read_bytes())['cards']
        cls.rows={row['name']:row for row in registry()}
        cls.homes={}
        for name,pin in model.profiles()['sources'].items():
            file,=(data/'cases/cards').glob(f"*/{pin['registry']['csvid']:05d}-*.json")
            cls.homes[name]=json.loads(file.read_bytes())['recipes'][0]

    def case(self,name,mode,owner):
        home=self.homes[name]
        shape=model.spec(home,self.oracle[name])
        return model.expand(name,self.rows[name]['csvid'],home,shape,[f'{mode}_{owner}'],self.rows,self.oracle)[0]

    def values(self,steps,op,**criteria):
        return [x['value'] for x in steps if x['op']==op and all(x.get(k)==v for k,v in criteria.items())]

    def source_values(self,steps,prop):
        return self.values(steps,'assert_card',ref='source',property=prop)

    def test_exact_full_front_and_back_bindings(self):
        model.validate_witnesses(self.oracle,self.rows)
        self.assertEqual({x['registry']['csvid'] for x in model.profiles()['sources'].values()},
                         {11975,12053,12098,12119,12121,15770,15852,15961})
        for name,home in self.homes.items():
            pin=model.profiles()['sources'][name]
            shape=model.spec(home,self.oracle[name])
            self.assertEqual(home['variants'],model.variants(shape))
            self.assertEqual(home['oracle_text'],shape['front']['clauses'][-1] if not shape['front']['keywords']
                             else '\n'.join(shape['front']['clauses']))
            with self.assertRaises(ValueError):
                model.spec(home,pin['back_oracle'])

    def test_per_player_spell_count_boundary_for_each_face(self):
        for name,home in self.homes.items():
            front=self.rows[name]['csvid']
            back=front+1
            for owner in (0,1):
                for scenario,result in [('front_zero',back),('front_one_owner',front),('front_one_foe',front),
                    ('front_two_owner',front),('front_two_foe',front),('front_split',front),('back_zero',back),
                    ('back_one_owner',back),('back_one_foe',back),('back_two_owner',front),('back_two_foe',front),
                    ('back_split',back)]:
                    with self.subTest(name=name,scenario=scenario,owner=owner):
                        case=self.case(name,scenario,owner)
                        steps=case['steps']
                        self.assertEqual(self.source_values(steps,'csvid')[-1],result)
                        threshold=max(i for i,step in enumerate(steps) if step['op']=='advance_turn')
                        inbound=max(i for i,step in enumerate(steps[:threshold]) if step['op']=='upkeep_step')
                        ref_players={c['ref']:c['player'] for c in case['setup']}
                        actual=[ref_players[x['ref']] for x in steps[inbound:threshold] if x['op']=='cast']
                        wanted=[owner if actor=='owner' else 1-owner for actor in model.PATTERNS[scenario]]
                        self.assertEqual(actual,wanted)

    def test_upkeep_is_after_real_untap_and_each_side_can_trigger(self):
        for owner in (0,1):
            for scenario,result in [('front_zero_on_foe',15771),('back_two_on_owner',15770)]:
                steps=self.case('Convicted Killer',scenario,owner)['steps']
                trips=[(i,x['player']) for i,x in enumerate(steps) if x['op']=='upkeep_step']
                self.assertEqual(self.source_values(steps,'csvid')[-1],result)
                self.assertEqual(trips[-1][1],1-owner if scenario=='front_zero_on_foe' else owner)
                for at,player in trips:
                    latest=[x for x in steps[:at] if x['op'] in ('advance_turn','turn','untap_step')][-2:]
                    self.assertEqual([x['op'] for x in latest],['advance_turn','untap_step'])
                    self.assertEqual(latest[-1]['player'],player)

    def test_real_cast_conditions_counters_and_paid_bounce(self):
        for owner in (0,1):
            denied=self.case('Reckless Waif','unfunded_activity',owner)['steps']
            self.assertEqual([x['value'] for x in denied if x['op']=='cast'],[1,0])
            self.assertEqual(self.source_values(denied,'csvid')[-1],12054)
            count=self.case('Reckless Waif','countered_second',owner)['steps']
            self.assertIn(dict(op='cast_response',ref='counter',value=1),count)
            self.assertEqual(self.source_values(count,'csvid')[-1],12053)
            kept=self.case('Reckless Waif','keep_counters',owner)['steps']
            self.assertIn(dict(op='choose_target',player=owner,ref='source'),kept)
            self.assertIn(12054,self.source_values(kept,'csvid'))
            self.assertEqual(self.source_values(kept,'plus1_counters')[-1],1)
            recast=self.case('Reckless Waif','bounce_recast',owner)['steps']
            self.assertEqual(self.values(recast,'bind_hand',ref='source'),[12053])
            self.assertEqual(self.values(recast,'cast',ref='source'),[1,1])
            self.assertEqual(self.source_values(recast,'csvid')[-1],12053)

    def test_first_strike_and_trample_real_combat_both_faces(self):
        for owner in (0,1):
            for name,scenario,our_damage,foe_life in [
                ('Village Ironsmith','front_first_strike',0,20),('Village Ironsmith','back_first_strike',0,20),
                ('Hinterland Logger','front_no_trample',None,20),('Hinterland Logger','back_trample',1,17)]:
                steps=self.case(name,scenario,owner)['steps']
                self.assertIn(dict(op='block',ref='blocker',other='source',value=1),steps)
                self.assertEqual(self.values(steps,'assert_life',player=1-owner)[-1],foe_life)
                damage=next(i for i,x in enumerate(steps) if x['op']=='combat_damage')
                if our_damage is None:
                    self.assertEqual(self.values(steps[damage:],'assert_zone_multiplicity',player=owner,
                                                 zone='graveyard',index=self.rows[name]['csvid'])[-1],1)
                else:
                    self.assertEqual(self.source_values(steps[damage:],'damage')[-1],our_damage)

    def test_replay_starts_at_ready_upkeep_and_repeats_identically(self):
        for mode,identity in [('front_replay',11976),('back_replay',11975)]:
            steps=self.case('Grizzled Outcasts',mode,0)['steps']
            at,record,restore=(next(i for i,x in enumerate(steps) if x['op']==op)
                               for op in ('capture','record_state','restore'))
            self.assertEqual(steps[at+1]['op'],'upkeep_step')
            self.assertEqual(steps[at+1:record],steps[restore+1:-1])
            self.assertEqual(self.source_values(steps,'csvid')[-1],identity)


if __name__=='__main__':
    unittest.main()
