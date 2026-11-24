"""Focused checks for the actual required declaration and complete printed riders."""
import json
import unittest
from pathlib import Path

from generate_cases import registry
import required_attack_combat_riders_recipes as model


class RequiredAttackCombatRidersTest(unittest.TestCase):
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
        return model.expand(name,self.rows[name]['csvid'],home,shape,[f'{mode}_{owner}'],self.rows,self.oracle)[0]['steps']

    def values(self,steps,op,**criteria):
        return [x['value'] for x in steps if x['op']==op and all(x.get(k)==v for k,v in criteria.items())]

    def last(self,steps,prop,ref='source'):
        return self.values(steps,'assert_card',ref=ref,property=prop)[-1]

    def test_pins_and_whole_oracle_and_current_printed_type(self):
        model.validate_witnesses(self.oracle,self.rows)
        for name,home in self.homes.items():
            shape=model.spec(home,self.oracle[name])
            self.assertEqual(home['variants'],model.variants(shape))
            with self.assertRaises(ValueError):
                model.spec(home,dict(self.oracle[name],oracle_text=self.oracle[name]['oracle_text']+'\nFlying'))
        self.assertIsNone(model.classify(self.oracle['Darksteel Juggernaut'])['power'])

    def test_requirement_negative_and_seat_boundaries(self):
        for name in self.homes:
            for owner in (0,1):
                for variant,expected in [('omitted',[0,1]),('competing_selected',[0,1]),('two_required',[0,0,1]),
                                         ('tapped',[1]),('sick',[1]),('pacified',[1]),('opponent_turn',[1,1]),
                                         ('paid_fresh',[1]),('paid_next_turn',[0,1])]:
                    steps=self.case(name,variant,owner)
                    self.assertEqual(self.values(steps,'can_finish_attackers'),expected)
                    if variant in ('tapped','sick','pacified','paid_fresh'):
                        self.assertEqual(self.values(steps,'attack',ref='source'),[0])

    def test_bushido_on_both_actual_blocking_sides_and_cleanup(self):
        name='Battle-Mad Ronin'
        for owner in (0,1):
            for variant,blocker in [('blocked_bushido','blocker'),('defend_bushido','source')]:
                steps=self.case(name,variant,owner)
                self.assertEqual(self.values(steps,'block',ref=blocker),[1])
                self.assertEqual(self.last(steps,'power'),3)
                self.assertEqual(self.last(steps,'damage'),2)
            cleanup=self.case(name,'bushido_cleanup',owner)
            self.assertEqual(self.last(cleanup,'power'),1)
            self.assertEqual(self.last(cleanup,'damage'),0)

    def test_neonate_counts_player_combat_damage_only(self):
        for owner in (0,1):
            for variant,marks in [('player_growth',1),('repeat_hit',2),('creature_only',0),('walker_damage',0),('noncombat',0)]:
                steps=self.case('Bloodcrazed Neonate',variant,owner)
                self.assertEqual(self.last(steps,'plus1_counters'),marks)
                if variant=='walker_damage':
                    self.assertIn(dict(op='attack_with_tax',ref='source',value=1),steps)
                    self.assertEqual(self.values(steps,'assert_life',player=1-owner)[-1],20)
                if variant=='noncombat':
                    self.assertIn(dict(op='activate',ref='spell',value=1),steps)

    def test_dauthi_shadow_has_real_attack_and_block_contrasts(self):
        name='Dauthi Slayer'
        for owner in (0,1):
            for variant,ref,accepted in [('ground_evasion','blocker',0),('shadow_exchange','blocker',1),
                                         ('defend_ground','source',0),('defend_shadow','source',1)]:
                steps=self.case(name,variant,owner)
                self.assertEqual(self.values(steps,'block',ref=ref),[accepted])
                self.assertEqual(self.last(steps,'damage'),accepted)

    def test_galvanic_death_before_actual_next_untap(self):
        name='Galvanic Juggernaut'
        for owner in (0,1):
            for variant,next_attack in [('own_death',1),('foreign_death',1),('next_locked',0),
                                        ('artifact_death',0),('bounced_no_death',0)]:
                steps=self.case(name,variant,owner)
                untap,=[i for i,item in enumerate(steps) if item['op']=='untap_step']
                self.assertEqual(self.values(steps,'attack',ref='source'),[1,next_attack])
                self.assertEqual(self.last(steps[:untap],'tapped'),1-next_attack)
                self.assertEqual(self.values(steps,'assert_life',player=1-owner)[-1],10 if next_attack else 15)

    def test_dynamic_artifact_count_own_only_and_true_destruction_rules(self):
        for owner in (0,1):
            for name,without,with_one,with_two in [('Ramroller',2,4,4),('Darksteel Juggernaut',1,2,3)]:
                for variant,power in [('own_artifacts',with_two),('opponent_artifacts',without),('other_zones',without),
                                      ('gain_artifact',with_one),('lose_artifact',without)]:
                    self.assertEqual(self.last(self.case(name,variant,owner),'power'),power)
            name='Darksteel Juggernaut'
            lethal=self.case(name,'indestructible_combat',owner)
            self.assertEqual(self.last(lethal,'damage'),2)
            exile=self.case(name,'exiled',owner)
            self.assertEqual(self.values(exile,'assert_zone_multiplicity',player=owner,zone='exile',index=11237)[-1],1)

    def test_prowess_only_own_noncreature_cast_even_if_countered(self):
        for owner in (0,1):
            for variant,power in [('own_noncreature',3),('own_creature',2),('opponent_noncreature',2),
                                  ('double_noncreature',4),('countered_spell',3),('prowess_cleanup',2)]:
                steps=self.case('Mage-Ring Bully',variant,owner)
                self.assertEqual(self.last(steps,'power'),power)
                self.assertEqual(self.last(steps,'plus1_counters'),0)
                if variant=='countered_spell':
                    self.assertIn(dict(op='cast_response',ref='counter',value=1),steps)

    def test_own_pacifism_triggers_prowess_but_still_bars_attack(self):
        for owner in (0,1):
            steps=self.case('Mage-Ring Bully','pacified',owner)
            resolve=next(i for i,action in enumerate(steps) if action['op']=='resolve')
            for prop in ('power','toughness'):
                self.assertEqual(self.values(steps[:resolve],'assert_card',ref='source',property=prop)[-1],2)
                self.assertEqual(self.values(steps[resolve:],'assert_card',ref='source',property=prop),[3]*4)
            self.assertEqual(self.values(steps,'can_finish_attackers'),[1])
            self.assertEqual(self.values(steps,'attack',ref='source'),[0])

    def test_caliph_counts_combat_and_noncombat_damage_and_fog(self):
        for owner in (0,1):
            for variant,own,foe in [('trample_split',16,18),('flying_ground',16,16),
                                     ('walker_damage',16,20),('noncombat',19,19),('fogged',20,20)]:
                steps=self.case('Emberwilde Caliph',variant,owner)
                self.assertEqual(self.values(steps,'assert_life',player=owner)[-1],own)
                self.assertEqual(self.values(steps,'assert_life',player=1-owner)[-1],foe)

    def test_full_combat_boundaries_for_every_requested_case(self):
        groups=['begin_required_attackers','finish_attackers','begin_blockers','advance_combat','combat_damage','end_combat']
        for name,home in self.homes.items():
            shape=model.spec(home,self.oracle[name])
            cases=model.expand(name,self.rows[name]['csvid'],home,shape,home['variants'],self.rows,self.oracle)
            for case in cases:
                mode=case['_recipe']['variant'].rsplit('_',1)[0]
                count=0 if mode in ('unfunded','wrong_color') else 2 if mode in model.REENTRY else 1
                actual=[row['op'] for row in case['steps'] if row['op'] in groups]
                expected=groups*count
                if mode=='proposed_replay':
                    expected+=groups[1:]
                self.assertEqual(actual,expected)


if __name__=='__main__':
    unittest.main()
