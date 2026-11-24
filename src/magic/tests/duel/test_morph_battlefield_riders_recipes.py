"""Whole permanent Morph receipts with no seeded rider outcomes."""
import json
import unittest
from pathlib import Path

from generate_cases import registry
import morph_battlefield_riders_recipes as model


class MorphBattlefieldRidersTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        data=Path(__file__).parent/'data'
        cls.oracle=json.loads((data/'oracle.json').read_bytes())['cards']
        cls.rows={row['name']:row for row in registry()}
        cls.homes={}
        for name,pin in model.profiles()['sources'].items():
            file,=(data/'cases/cards').glob(f"*/{pin['registry']['csvid']:05d}-*.json")
            cls.homes[name]=json.loads(file.read_bytes())['recipes'][0]

    def case(self,name,variant):
        home=self.homes[name]
        shape=model.spec(home,self.oracle[name])
        return model.expand(name,self.rows[name]['csvid'],home,shape,[variant],self.rows,self.oracle)[0]

    def values(self,steps,op,**expected):
        return [x['value'] for x in steps if x['op']==op and all(x.get(k)==v for k,v in expected.items())]

    def test_whole_oracle_and_normal_implemented_type(self):
        model.validate_witnesses(self.oracle,self.rows)
        for name,home in self.homes.items():
            shape=model.spec(home,self.oracle[name])
            self.assertEqual(home['variants'],model.variants(shape))
            with self.assertRaises(ValueError):
                model.spec(home,dict(self.oracle[name],oracle_text='Flying\n'+self.oracle[name]['oracle_text']))
        self.assertIsNone(model.classify(self.oracle['Lumithread Field'])['power'])

    def test_exact_real_face_down_and_paid_offturn_reveal(self):
        for name in self.homes:
            for owner in (0,1):
                hidden=self.case(name,f'hidden_cast_{owner}')['steps']
                self.assertEqual(self.values(hidden,'assert_card',ref='source',property='csvid')[-1],model.FACE_DOWN)
                off=self.case(name,f'offturn_reveal_{owner}')['steps']
                self.assertEqual(self.values(off,'activate',ref='source'),[1])
                self.assertEqual(self.values(off,'assert_card',ref='source',property='csvid')[-1],self.rows[name]['csvid'])
                denied=self.case(name,f'wrong_color_{owner}')['steps']
                self.assertEqual(self.values(denied,'activate',ref='source'),[0])
                self.assertEqual(self.values(denied,'assert_card',ref='source',property='csvid')[-1],model.FACE_DOWN)

    def test_demon_uses_only_real_own_upkeep_and_repeats(self):
        for owner in (0,1):
            for mode,visits,life in [('own_upkeep',2,18),('opposing_upkeep',1,20),
                                     ('hidden_upkeep',2,20),('double_upkeep',4,16)]:
                steps=self.case('Grinning Demon',f'{mode}_{owner}')['steps']
                self.assertEqual(sum(x['op']=='upkeep_step' for x in steps),visits)
                self.assertEqual(self.values(steps,'assert_life',player=owner)[-1],life)
                for x,op in enumerate(steps):
                    if op['op']=='upkeep_step':
                        self.assertEqual([a['op'] for a in steps[:x] if a['op'] in ('advance_turn','untap_step')][-2:],
                                         ['advance_turn','untap_step'])

    def test_liege_mandatory_other_creature_or_damage_and_printed_combat(self):
        for owner in (0,1):
            for scenario,loss,death in [('own_fodder',0,1),('opponent_only',7,0),
                                        ('artifact_only',7,0),('double_upkeep',7,1),('hidden_upkeep',0,0)]:
                steps=self.case('Liege of the Pit',f'{scenario}_{owner}')['steps']
                self.assertEqual(self.values(steps,'assert_life',player=owner)[-1],20-loss)
                bears=self.rows['Grizzly Bears']['csvid']
                if scenario!='artifact_only':
                    self.assertEqual(self.values(steps,'assert_zone_multiplicity',player=owner,zone='graveyard',index=bears)[-1],death)
                chooses=[s for s in steps if s['op']=='choose_target']
                self.assertEqual(len(chooses),int(owner==0 and death>0))
            combat=self.case('Liege of the Pit',f'trample_hit_{owner}')['steps']
            self.assertEqual(self.values(combat,'block',ref='blocker'),[1])
            self.assertEqual(self.values(combat,'assert_life',player=1-owner)[-1],15)
            evasion=self.case('Liege of the Pit',f'flying_evasion_{owner}')['steps']
            self.assertEqual(self.values(evasion,'block',ref='blocker'),[0])

    def test_haruspex_draws_only_for_qualifying_death(self):
        for owner in (0,1):
            for scenario,draws in [('own_death',1),('hidden_death',0),('foreign_death',0),
                                    ('self_death',0),('self_and_other',1),('token_death',0),
                                    ('exiled',0),('bounced',0),('artifact_death',0)]:
                steps=self.case('Grim Haruspex',f'{scenario}_{owner}')['steps']
                self.assertEqual(self.values(steps,'assert_zone',player=owner,zone='library')[-1],3-draws)
                self.assertEqual(self.values(steps,'assert_zone_card',player=owner,zone='library',index=0)[-1],
                                 self.rows['Mountain' if draws else 'Forest']['csvid'])
                if scenario=='token_death':
                    binds=[x for x in steps if x['op']=='bind_permanent_copy']
                    self.assertEqual([x['ref'] for x in binds],['first_soldier','second_soldier'])
                    self.assertEqual(self.values(steps,'assert_zone_multiplicity',player=owner,zone='graveyard',
                                                 index=self.rows['Soldier']['csvid'])[-1],0)

    def test_field_remains_enchantment_and_updates_only_own_toughness(self):
        for owner in (0,1):
            shown=self.case('Lumithread Field',f'printed_cast_{owner}')['steps']
            self.assertFalse(self.values(shown,'choose_dialog'))
            self.assertFalse(self.values(shown,'assert_card',ref='source',property='power'))
            self.assertEqual(self.values(shown,'assert_card',ref='ally',property='toughness')[-1],3)
            self.assertEqual(self.values(shown,'assert_card',ref='opposing',property='toughness')[-1],3)
            checked=self.case('Lumithread Field',f'target_type_{owner}')['steps']
            self.assertEqual(self.values(checked,'can_cast',ref='answer'),[0,1])
            self.assertEqual(self.values(checked,'assert_card',ref='ally',property='toughness')[-1],2)
            bears=self.rows['Grizzly Bears']['csvid']
            for case,mark,dead in [('shore_up',2,0),('hidden_damage',None,1)]:
                trace=self.case('Lumithread Field',f'{case}_{owner}')['steps']
                self.assertEqual(self.values(trace,'assert_zone_multiplicity',player=owner,zone='graveyard',index=bears)[-1],dead)
                if mark is not None:
                    self.assertEqual(self.values(trace,'assert_card',ref='ally',property='damage')[-1],mark)

    def test_fortune_thief_floor_is_only_damage_and_only_current_controller(self):
        for owner in (0,1):
            for scenario,life in [('floor',1),('nonlethal',5),('exact_one',1),('at_one',1),
                                   ('hidden_lethal',-1),('source_absent',-1),('combat_floor',1),('life_loss',-1)]:
                steps=self.case('Fortune Thief',f'{scenario}_{owner}')['steps']
                self.assertEqual(self.values(steps,'assert_life',player=owner)[-1],life)
            steps=self.case('Fortune Thief',f'other_player_{owner}')['steps']
            self.assertEqual(self.values(steps,'assert_life',player=1-owner)[-1],-1)
            self.assertEqual(self.values(steps,'assert_life',player=owner)[-1],20)

    def test_life_loss_uses_actual_automatic_recipient(self):
        for owner in (0,1):
            loss=self.case('Fortune Thief',f'life_loss_{owner}')['steps']
            self.assertNotIn(dict(op='choose_player',player=1-owner,value=owner),loss)
            self.assertIn(dict(op='cast',ref='answer',value=1),loss)
            self.assertEqual(self.values(loss,'assert_life',player=owner)[-1],-1)
            bolt=self.case('Fortune Thief',f'floor_{owner}')['steps']
            self.assertIn(dict(op='choose_player',player=1-owner,value=owner),bolt)

    def test_replays_repeat_only_real_reveal_and_full_receipts(self):
        for name in self.homes:
            steps=self.case(name,'reveal_replay_1')['steps']
            capture,record,restore=(next(i for i,action in enumerate(steps) if action['op']==op)
                                    for op in ('capture','record_state','restore'))
            self.assertEqual(steps[capture+1:record],steps[restore+1:-1])
            self.assertEqual(steps[-1]['op'],'assert_recorded_state')


if __name__=='__main__':
    unittest.main()
