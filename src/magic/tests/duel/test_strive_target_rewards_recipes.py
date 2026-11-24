"""Focused guards for Strive paid selection and reward boundaries."""

from copy import deepcopy
import unittest

import strive_target_rewards_recipes as model


class StriveTargetRewardGuards(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        pinned=model.profile()
        entries=pinned['sources']|pinned['witnesses']
        cls.oracle={name:item['oracle'] for name,item in entries.items()}
        cls.rows={name:item['registry'] for name,item in entries.items()}

    def case(self,name,variant):
        record=self.oracle[name]
        params=model.parameters(record)
        declaration=dict(family=model.FAMILY,version=2,oracle_text=record['oracle_text'],params=params,variants=[variant])
        return model.expand(name,self.rows[name]['csvid'],declaration,model.spec(declaration,record),
                            [variant],self.rows,self.oracle)[0]

    def test_whole_effect_and_strive_cost_retained(self):
        changed=deepcopy(self.oracle["Ajani's Presence"])
        changed['oracle_text']=changed['oracle_text'].replace(' and gain indestructible','')
        with self.assertRaises(ValueError):
            model.parameters(changed)

    def test_exact_base_and_per_additional_target(self):
        aerial=model.parameters(self.oracle['Aerial Formation'])
        self.assertEqual([model.total(aerial,n) for n in range(4)],
                         [[0,0,1,0,0,0],[0,0,1,0,0,0],[2,0,2,0,0,0],[4,0,3,0,0,0]])
        hour=model.parameters(self.oracle['Hour of Need'])
        self.assertEqual(model.total(hour,3),[4,0,3,0,0,0])

    def test_actual_done_for_zero_and_stopping_early(self):
        for mode,targets in [('zero',[]),('stop_one',['first'])]:
            steps=self.case('Aerial Formation',mode+'_0')['steps']
            self.assertEqual([step['ref'] for step in steps if step['op']=='choose_target'],targets)
            done=next(index for index,step in enumerate(steps) if step['op']=='choose_done_target')
            self.assertEqual(steps[done],dict(op='choose_done_target',player=0))
            self.assertIn(dict(op='assert_mana',player=0,color=0,value=4),steps[done:])

    def test_insufficient_increment_selects_one_and_never_invents_a_prompt(self):
        for mode in ('short_generic','short_color'):
            steps=self.case('Cruel Feeding',mode+'_1')['steps']
            self.assertEqual([step['ref'] for step in steps if step['op']=='choose_target'],['first'])
            self.assertNotIn('choose_done_target',[step['op'] for step in steps])
        exhausted=self.case('Cruel Feeding','spare_all_1')['steps']
        self.assertEqual([step['ref'] for step in exhausted if step['op']=='choose_target'],['first','second'])
        self.assertNotIn('choose_done_target',[step['op'] for step in exhausted])

    def test_all_unavailable_is_separate_from_player_done(self):
        steps=self.case('Phalanx Formation','all_unavailable_1')['steps']
        self.assertEqual([step['value'] for step in steps if step['op']=='can_target_creature'],[0,0,0])
        self.assertIn(dict(op='can_cast',ref='source',value=1),steps)
        self.assertNotIn('choose_done_target',[step['op'] for step in steps])

    def test_real_partial_bounce_retains_paid_fee_and_other_target(self):
        steps=self.case('Aerial Formation','late_first_0')['steps']
        self.assertEqual([step['ref'] for step in steps if step['op']=='choose_target'],['first','second','first'])
        names=[step['op'] for step in steps]
        self.assertLess(names.index('announce_spell'),names.index('cast_response'))
        self.assertIn(dict(op='assert_card',ref='second',property='power',value=4),steps[names.index('resolve_response'):])
        self.assertIn(dict(op='assert_mana',player=0,color=2,value=0),steps)

    def test_riders_use_actual_damage_and_true_turn_expiry(self):
        ajani=self.case("Ajani's Presence",'destroy_protection_0')['steps']
        ops=[step.get('ref') for step in ajani if step['op']=='cast']
        self.assertEqual(ops,['source','bolt','destroy','after_cleanup'])
        self.assertIn(dict(op='untap_step',player=1),ajani)
        formation=self.case('Phalanx Formation','combat_1')['steps']
        self.assertIn(dict(op='combat_damage'),formation)
        self.assertIn(dict(op='assert_life',player=0,value=10),formation)
        feeding=self.case('Cruel Feeding','lifelink_enemy_1')['steps']
        self.assertIn(dict(op='assert_life',player=0,value=27),feeding)

    def test_hour_keeps_token_controller_distinct_from_exile_owner(self):
        foreign=self.case('Hour of Need','foreign_owner_0')['steps']
        self.assertIn(dict(op='assert_zone',zone='exile',player=0,value=2),foreign)
        for actor in (0,1):
            self.assertIn(dict(op='assert_zone_multiplicity',zone='battlefield',player=actor,
                               index=self.rows[model.TOKEN]['csvid'],value=1),foreign)
        flight=self.case('Hour of Need','token_flying_0')['steps']
        self.assertIn(dict(op='block',ref='blocker',other='token',value=0),flight)
        self.assertIn(dict(op='assert_card',ref='token',property='tapped',value=1),flight)


if __name__=='__main__':
    unittest.main()
