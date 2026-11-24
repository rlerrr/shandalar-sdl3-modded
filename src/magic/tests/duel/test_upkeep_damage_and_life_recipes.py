"""Focused tests for linked upkeep expectations and exact Oracle scope."""

from copy import deepcopy
import unittest

import upkeep_damage_and_life_recipes as model


class UpkeepDamageAndLifeTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        pins=model.profiles()['sources']|model.profiles()['witnesses']
        cls.rows={name:pin['registry'] for name,pin in pins.items()}
        cls.oracle={name:pin['oracle'] for name,pin in pins.items()}
        cls.names={row['csvid']:name for name,row in cls.rows.items()}

    def build(self,csvid,mode,seat=0):
        board=model.Board(csvid,self.names[csvid],mode,seat,self.rows,self.oracle)
        board.run()
        return board

    def test_complete_oracle_and_unique_variant(self):
        record=self.oracle[self.names[model.CONVALESCENCE]]
        params=model.classify(record,model.CONVALESCENCE)
        declaration=dict(family=model.FAMILY,version=2,params=params,
                         oracle_text=record['oracle_text'],variants=['ten_0'])
        self.assertEqual(model.spec(declaration,record)['csvid'],model.CONVALESCENCE)
        changed=deepcopy(record)
        changed['oracle_text']=changed['oracle_text'].replace('10 or less','less than 10')
        with self.assertRaises(ValueError):
            model.spec(declaration,changed)
        with self.assertRaises(ValueError):
            model.spec(dict(declaration,variants=['ten_0']*2),record)

    def test_infection_compounds_after_paid_combat_damage_only(self):
        no_hits=self.build(model.VERMIN,'uninfected',1)
        hits=self.build(model.VERMIN,'two_hits',1)
        self.assertEqual(no_hits.life,[20,20])
        self.assertEqual(hits.life,[15,20])
        self.assertEqual(sum(step[0]=='combat_damage' for step in hits.steps),2)

    def test_enslave_damage_follows_owner_not_controller(self):
        foreign=self.build(model.ENSLAVE,'foreign_owner',1)
        own=self.build(model.ENSLAVE,'self_owner',1)
        self.assertEqual(foreign.objects['victim']['player'],1)
        self.assertEqual(foreign.life,[19,20])
        self.assertEqual(own.life,[20,19])

    def test_firemane_return_spends_six_generic_two_red_two_white(self):
        board=self.build(model.FIREMANE,'grave_return')
        selected=[s[1] for s in board.steps if s[0]=='choose_mana_source']
        self.assertEqual([board.objects[r]['name'] for r in selected],
                         ['Mountain']*2+['Plains']*2+['Wastes']*6)
        self.assertEqual(board.setup[0][2],model.ZONES['graveyard'])
        self.assertEqual(board.objects['source']['zone'],'battlefield')

    def test_felidar_real_combat_matches_lifelink_and_vigilance(self):
        board=self.build(model.FELIDAR,'combat_then_win',1)
        self.assertEqual(board.life,[16,40])
        self.assertIn(('assert_card','source','','tapped',0,0,0,0,0),board.steps)
        self.assertEqual(board.steps[-1][0:6],('advance_scheduled_turn','','','',0,-1))

    def test_color_union_and_rounding(self):
        self.assertEqual(self.build(model.SANCTUARY,'dual').life,[24,20])
        self.assertEqual(self.build(model.SANCTUARY,'foreign_only').life,[20,20])
        self.assertEqual(self.build(model.HAVOC,'repeat').life,[5,5])


if __name__=='__main__':
    unittest.main()
