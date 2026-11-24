"""Focused guards for paid responses and genuine passive tests."""

from copy import deepcopy
import unittest

import uncounterable_creature_casts_recipes as model


class UncounterableCreatureGuards(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        profiles=model.profile()
        cls.sources=profiles['sources']
        registry=profiles['sources']|profiles['witnesses']
        cls.oracle={name:pin['oracle'] for name,pin in registry.items()}
        cls.rows={name:pin['registry'] for name,pin in registry.items()}

    def case(self,name,variant):
        printed=self.oracle[name]
        params=model.parameters(printed)
        decl=dict(family=model.FAMILY,version=2,oracle_text=printed['oracle_text'],params=params,variants=[variant])
        return model.expand(name,self.rows[name]['csvid'],decl,model.spec(decl,printed),[variant],self.rows,self.oracle)[0]

    def test_rejects_incomplete_whole_oracle(self):
        printed=deepcopy(self.oracle['Skylasher'])
        printed['oracle_text']=printed['oracle_text'].replace('Flash\n','')
        with self.assertRaises(ValueError):
            model.parameters(printed)

    def test_requires_native_counter_witness(self):
        rows=deepcopy(self.rows)
        rows['Exclude']['handler']='card_dummy'
        with self.assertRaises(ValueError):
            model.validate_witnesses(self.oracle,rows)

    def test_paid_exclude_uses_real_pending_parent_and_draws(self):
        for seat in (0,1):
            steps=self.case('Carnage Tyrant',f'exclude_{seat}')['steps']
            order=[step['op'] for step in steps]
            self.assertLess(order.index('announce_spell'),order.index('cast_response'))
            self.assertLess(order.index('cast_response'),order.index('resolve_response'))
            self.assertIn(dict(op='assert_zone_multiplicity',player=1-seat,zone='graveyard',
                               index=self.rows['Exclude']['csvid'],value=1),steps)
            self.assertIn(dict(op='assert_zone_multiplicity',player=1-seat,zone='hand',
                               index=self.rows['Forest']['csvid'],value=1),steps)

    def test_ordinary_paid_counter_moves_parent_away(self):
        case=self.case('Blurred Mongoose','ordinary_counter_0')
        self.assertIn(dict(op='assert_zone_multiplicity',player=0,zone='graveyard',
                           index=self.rows[model.NORMAL]['csvid'],value=1),case['steps'])
        protected=self.case('Blurred Mongoose','counter_0')['steps']
        self.assertIn(dict(op='assert_zone_multiplicity',player=0,zone='battlefield',
                           index=self.rows['Blurred Mongoose']['csvid'],value=1),protected)

    def test_protection_exact_color_and_independent_aura_departure(self):
        for name,black in [('Great Sable Stag',0),('Scragnoth',1),('Skylasher',1)]:
            case=self.case(name,'aura_black_1')['steps']
            membership=[step['value'] for step in case if step['op']=='assert_zone_multiplicity' and
                        step['player']==1 and step['zone']=='battlefield' and step['index']==self.rows[model.AURA]['csvid']]
            self.assertEqual(membership,[1,black])
            target=self.case(name,'target_black_1')['steps']
            self.assertIn(dict(op='can_target_creature',ref='targeter',other='source',value=black),target)

    def test_shroud_bilateral_and_hexproof_controller_scope(self):
        for name,friendly in [('Blurred Mongoose',0),('Carnage Tyrant',1)]:
            for role,legal in [('own',friendly),('foe',0)]:
                spells=self.case(name,f'{role}_spell_0')['steps']
                self.assertIn(dict(op='can_target_creature',ref='targeter',other='source',value=legal),spells)
                ability=self.case(name,f'{role}_ability_1')['steps']
                self.assertIn(dict(op='probe_activation_target',ref='targeter',other='source',index=-1,value=legal),ability)

    def test_flash_reach_uses_actual_opposing_combat(self):
        steps=self.case('Skylasher','flash_reach_0')['steps']
        ops=[step['op'] for step in steps]
        self.assertEqual(steps[0],dict(op='turn',player=1,value=0))
        self.assertLess(ops.index('finish_attackers'),ops.index('cast'))
        self.assertLess(ops.index('cast'),ops.index('begin_blockers'))
        self.assertIn(dict(op='block',ref='source',other='foe',value=1),steps)

    def test_trample_exact_excess_and_no_excess(self):
        for name,power in [('Terra Stomper',8),('Carnage Tyrant',7)]:
            for mode,life in [('trample',22-power),('trample_full',20)]:
                steps=self.case(name,f'{mode}_1')['steps']
                self.assertIn(dict(op='block',ref='foe',other='source',value=1),steps)
                self.assertIn(dict(op='assert_life',player=0,value=life),steps)


if __name__=='__main__':
    unittest.main()
