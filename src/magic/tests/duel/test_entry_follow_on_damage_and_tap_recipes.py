"""Guard exact printed colors, counted subtype and duel-action distinctions."""

from copy import deepcopy
import unittest

import entry_follow_on_damage_and_tap_recipes as model


class EntryFollowOnGuards(unittest.TestCase):
    def fixtures(self):
        pins={}
        for group in model.profiles().values():
            pins.update(group)
        return ({n:p['oracle'] for n,p in pins.items()},{n:p['registry'] for n,p in pins.items()})

    def case(self,name,mode,seat):
        oracle,rows=self.fixtures()
        board=model.FollowOn(oracle[name],rows[name]['csvid'],mode,seat,rows,oracle)
        board.play()
        return board

    def test_exact_printed_scope(self):
        oracle,rows=self.fixtures()
        model.validate_witnesses(oracle,rows)
        record=oracle['Spawn of Thraxes']
        params=model.classify(record,model.SPAWN)
        declaration=dict(family=model.FAMILY,version=2,oracle_text=record['oracle_text'],
                         params=params,variants=model.variants(params))
        self.assertEqual(model.spec(declaration,record)['csvid'],model.SPAWN)
        changed=deepcopy(record)
        changed['oracle_text']=changed['oracle_text'].replace('Mountains','basic Mountains')
        with self.assertRaises(ValueError):
            model.spec(declaration,changed)
        with self.assertRaises(ValueError):
            model.spec(declaration | {'variants':['zero_1','zero_1']},record)

    def test_mountains_include_actual_nonbasic_subtype(self):
        board=self.case('Spawn of Thraxes','mountains',0)
        self.assertEqual(board.cards['nonbasic_mountain']['name'],'Taiga')
        self.assertEqual(board.life,[20,18])
        self.assertEqual(self.case('Spawn of Thraxes','zero',0).life,[20,20])

    def test_real_black_and_blue_production_after_tapped_entry(self):
        for name,color in (('Skyline Cascade',2),('Piranha Marsh',1)):
            board=self.case(name,'mana',1)
            self.assertEqual(board.wallet[1][color],1)
            untap,=(i for i,s in enumerate(board.steps) if s['op']=='untap_step')
            self.assertEqual(board.steps[untap]['player'],1)
            self.assertEqual(sum(s['op']=='play_land' for s in board.steps),1)

    def test_aura_and_multitarget_choices_stay_distinct(self):
        board=self.case('Glaring Aegis','attach_enemy',0)
        self.assertEqual([s['ref'] for s in board.steps if s['op']=='choose_target'],['host','guard'])
        fire=self.case('Rakdos Firewheeler','decline',1)
        self.assertEqual([s['op'] for s in fire.steps if s['op'] in ('choose_player','choose_cancel_target')],
                         ['choose_player','choose_cancel_target'])
        self.assertEqual(fire.life,[18,20])

    def test_indestructible_flyer_excluded_from_death_count(self):
        board=self.case('Whiptongue Hydra','indestructible',0)
        self.assertEqual(board.cards['gargoyle']['zone'],'battlefield')
        self.assertEqual(board.cards['own_flyer']['zone'],'graveyard')
        self.assertEqual([s['value'] for s in board.steps if s['op']=='assert_counter'],[2])


if __name__=='__main__':
    unittest.main()
