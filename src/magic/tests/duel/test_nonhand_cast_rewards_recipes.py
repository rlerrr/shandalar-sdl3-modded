"""Focused whole-card, cast-zone and historic fixture guards."""

from copy import deepcopy
import unittest

import nonhand_cast_rewards_recipes as model


class NonhandCastRewardGuards(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        pins=model.profile()
        entries=pins['sources']|pins['witnesses']
        cls.oracle={name:entry['oracle'] for name,entry in entries.items()}
        cls.rows={name:entry['registry'] for name,entry in entries.items()}

    def case(self,name,variant):
        record=self.oracle[name]
        params=model.parameters(record)
        decl=dict(family=model.FAMILY,version=2,oracle_text=record['oracle_text'],params=params,variants=[variant])
        return model.expand(name,self.rows[name]['csvid'],decl,model.spec(decl,record),[variant],self.rows,self.oracle)[0]

    def test_harness_cannot_lose_cost_rider(self):
        record=deepcopy(self.oracle['Harness the Storm'])
        record['oracle_text']=record['oracle_text'].removesuffix(' (You still pay its costs.)')
        with self.assertRaises(ValueError):
            model.parameters(record)

    def test_witness_registry_and_oracle_pins(self):
        rows=deepcopy(self.rows)
        rows[model.FLASH]['handler']='card_dummy'
        with self.assertRaises(ValueError):
            model.validate_witnesses(self.oracle,rows)

    def test_burning_uses_real_flashback_and_damage_target(self):
        for player in (0,1):
            case=self.case('Burning Vengeance',f'creature_{player}')
            ops=[step['op'] for step in case['steps']]
            self.assertLess(ops.index('choose_target'),ops.index('cast_graveyard'))
            self.assertIn(dict(op='assert_zone_multiplicity',player=1-player,zone='graveyard',
                               index=self.rows['Grizzly Bears']['csvid'],value=1),case['steps'])

    def test_secrets_separates_countered_grave_from_typed_exile(self):
        case=self.case('Secrets of the Dead','countered_1')
        self.assertIn(dict(op='assert_zone',player=1,zone='hand',value=1),case['steps'])
        self.assertIn(dict(op='assert_zone_multiplicity',player=1,zone='battlefield',
                           index=self.rows['Bird']['csvid'],value=2),case['steps'])
        exile=self.case('Secrets of the Dead','exile_1')['steps']
        self.assertTrue(any(step['op']=='play_exiled' for step in exile))
        self.assertFalse(any(step['op']=='cast_graveyard' for step in exile))

    def test_harness_paid_short_and_decline_have_distinct_ledgers(self):
        for mode,expected in [('paid',2),('underpaid',1),('decline',1)]:
            steps=self.case('Harness the Storm',f'{mode}_1')['steps']
            hand=[step['value'] for step in steps if step['op']=='assert_zone' and step['player']==1 and step['zone']=='hand']
            self.assertEqual(hand[-1],expected)
            self.assertIn('choose_zone' if mode=='decline' else 'choose_zone_card',[step['op'] for step in steps])

    def test_teshar_exact_boundary_and_counter_order(self):
        steps=self.case("Teshar, Ancestor's Apostle",'boundary_0')['steps']
        for name,legal in [(model.BOUNDARY,1),(model.BIG,0),(model.FLASH,0)]:
            self.assertIn(dict(op='assert_offered_legal_card',index=self.rows[name]['csvid'],value=legal),steps)
        counter=self.case("Teshar, Ancestor's Apostle",'countered_1')['steps']
        ops=[step['op'] for step in counter]
        self.assertLess(ops.index('announce_spell'),ops.index('cast_response'))
        self.assertIn(dict(op='assert_zone_multiplicity',player=1,zone='battlefield',
                           index=self.rows[model.SMALL]['csvid'],value=1),counter)

    def test_assistant_actual_scry_keep_bottom_and_next_turn_negative(self):
        keep=self.case("Artificer's Assistant",'keep_0')['steps']
        self.assertIn(dict(op='choose_zone',player=0,value=-1),keep)
        next_turn=self.case("Artificer's Assistant",'next_turn_1')['steps']
        self.assertIn(dict(op='choose_zone_card',player=1,value=self.rows[model.BIG]['csvid']),next_turn)
        self.assertEqual(sum(step['op']=='assert_scry_result' for step in next_turn),1)
        self.assertEqual(sum(step['op']=='advance_turn' for step in next_turn),2)
        self.assertTrue(any(step['op']=='cast' and step['ref']=='later' for step in next_turn))

    def test_both_historic_sources_use_real_flying_with_reach_contrast(self):
        for name in ("Teshar, Ancestor's Apostle","Artificer's Assistant"):
            for mode,allowed in [('flying',0),('reach',1)]:
                steps=self.case(name,f'{mode}_1')['steps']
                self.assertIn(dict(op='block',ref='blocker',other='source',value=allowed),steps)
                self.assertIn(dict(op='combat_damage'),steps)


if __name__=='__main__':
    unittest.main()
