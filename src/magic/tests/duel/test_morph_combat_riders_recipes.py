"""Keep Morph fees, declared attacks, and printed damage rider receipts independent."""
import json
import unittest
from pathlib import Path

from generate_cases import registry
import morph_combat_riders_recipes as model
from recipes import COLORS


class MorphCombatRidersTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        data = Path(__file__).parent / 'data'
        cls.oracle = json.loads((data / 'oracle.json').read_bytes())['cards']
        cls.rows = {row['name']: row for row in registry()}
        cls.homes = {}
        for name, item in model.profiles()['sources'].items():
            home, = (data / 'cases/cards').glob(f"*/{item['registry']['csvid']:05d}-*.json")
            cls.homes[name] = json.loads(home.read_bytes())['recipes'][0]

    def case(self, name, variant):
        declaration = self.homes[name]
        shape = model.spec(declaration, self.oracle[name])
        return model.expand(name, self.rows[name]['csvid'], declaration, shape, [variant], self.rows, self.oracle)[0]

    def values(self, steps, op, **expected):
        return [x['value'] for x in steps if x['op'] == op and all(x.get(k) == v for k,v in expected.items())]

    def through_attack(self, steps):
        return steps[:next(i for i,s in enumerate(steps) if s['op']=='begin_blockers')]

    def test_full_pinned_records_are_required(self):
        model.validate_witnesses(self.oracle, self.rows)
        for name, declaration in self.homes.items():
            self.assertEqual(declaration['variants'], model.variants(model.classify(self.oracle[name])))
            with self.assertRaises(ValueError):
                model.spec(declaration, dict(self.oracle[name], oracle_text='Flying\n'+self.oracle[name]['oracle_text']))

    def test_reaper_both_odd_life_events_and_nonretroactivity(self):
        for owner in (0, 1):
            steps = self.case('Ebonblade Reaper', f'odd_life_{owner}')['steps']
            first = self.through_attack(steps)
            self.assertEqual(self.values(first, 'assert_life', player=owner)[-2:], [17, 8])
            self.assertEqual(self.values(first, 'assert_life', player=1-owner)[-1], 20)
            self.assertEqual(self.values(steps, 'assert_life', player=1-owner)[-1], 9)
            late = self.case('Ebonblade Reaper', f'late_flip_hit_{owner}')['steps']
            self.assertEqual(self.values(late, 'assert_life', player=owner)[-1], 20)
            self.assertEqual(self.values(late, 'assert_life', player=1-owner)[-1], 9)

    def test_cadaver_accept_pass_and_human_short_and_empty(self):
        for mode,count,sac,dialog in [('revealed_hit',3,1,0),('short_hand',1,1,0),
                                      ('empty_hand',0,1,0),('decline',0,0,1)]:
            steps = self.case('Haunted Cadaver', f'{mode}_0')['steps']
            self.assertEqual(self.values(steps, 'choose_dialog', player=0), [dialog])
            self.assertEqual(self.values(steps, 'assert_zone', player=1, zone='graveyard')[-1], count)
            self.assertEqual(self.values(steps, 'assert_zone_multiplicity', player=0, zone='graveyard', index=6176)[-1], sac)
        ai = self.case('Haunted Cadaver','revealed_hit_1')['steps']
        self.assertFalse(self.values(ai, 'choose_dialog'))
        self.assertEqual(self.values(ai,'assert_discard_choices')[-1],3)

    def test_cadaver_sacrifice_checks_original_ref_and_graveyard(self):
        for mode, sacrificed in [('revealed_hit', 1), ('empty_hand', 1), ('short_hand', 1), ('decline', 0)]:
            steps = self.case('Haunted Cadaver', f'{mode}_0')['steps']
            end = next(i for i, action in enumerate(steps) if action['op'] == 'end_combat')
            final = steps[end+1:]
            self.assertEqual(self.values(final, 'assert_card', ref='source', property='in_play'), [1-sacrificed])
            self.assertEqual(self.values(final, 'assert_zone', player=0, zone='battlefield'),
                             [] if sacrificed else [1])
            self.assertEqual(self.values(final, 'assert_zone_multiplicity', player=0,
                                         zone='graveyard', index=6176), [sacrificed])
            self.assertEqual(self.values(final, 'assert_zone', player=1, zone='battlefield'), [0])

    def test_real_discard_choices_and_same_source_noncombat_denial(self):
        for name,amount in [('Headhunter',1),('Riptide Pilferer',1),('Silent Specter',2)]:
            high = self.case(name,'human_high_1')['steps']
            low = self.case(name,'revealed_hit_1')['steps']
            self.assertEqual(self.values(high,'discard_policy'),[2])
            self.assertEqual(self.values(low,'discard_policy'),[1])
            for steps in (high,low):
                self.assertEqual(self.values(steps,'assert_discard_choices')[-1],amount)
                self.assertEqual(self.values(steps,'assert_zone',player=0,zone='hand')[-1],1)
            ids = sorted(self.rows[card]['csvid'] for card in model.HAND[:amount+1])
            for ident in ids:
                high_check = self.values(high, 'assert_zone_multiplicity', player=0, zone='graveyard', index=ident)
                low_check = self.values(low, 'assert_zone_multiplicity', player=0, zone='graveyard', index=ident)
                self.assertEqual(high_check[-1], int(ident in ids[-amount:]))
                self.assertEqual(low_check[-1], int(ident in ids[:amount]))
            noncombat = self.case(name,'noncombat_damage_1')['steps']
            self.assertIn(dict(op='activate',ref='grant',value=1),noncombat)
            self.assertEqual(self.values(noncombat,'assert_discard_choices')[-1],0)
            self.assertEqual(self.values(noncombat,'assert_life',player=0)[-1],19)

    def test_raven_moves_exact_prefix_and_preserves_remainder(self):
        for owner in (0,1):
            for mode,total,left in [('revealed_hit',10,2),('exact_library',10,0),('short_library',7,0),('empty_library',0,0)]:
                steps = self.case('Raven Guild Master',f'{mode}_{owner}')['steps']
                self.assertEqual(self.values(steps,'assert_zone',player=1-owner,zone='exile')[-1],total)
                self.assertEqual(self.values(steps,'assert_zone',player=1-owner,zone='library')[-1],left)
                if left:
                    for i,name in enumerate(model.DECK[-left:]):
                        self.assertEqual(self.values(steps,'assert_zone_card',player=1-owner,zone='library',index=i)[-1],
                                         self.rows[name]['csvid'])

    def test_lower_curve_cast_and_exact_menu_payment(self):
        for owner in (0,1):
            for name in ('Headhunter','Riptide Pilferer'):
                steps = self.case(name,f'printed_cast_{owner}')['steps']
                self.assertEqual(self.values(steps,'choose_dialog'),[])
                self.assertEqual(self.values(steps,'assert_card',ref='source',property='csvid')[-1],self.rows[name]['csvid'])
                steps = self.case(name,f'hidden_choice_{owner}')['steps']
                self.assertEqual(self.values(steps,'choose_dialog',player=owner),[0])
                self.assertEqual(self.values(steps,'assert_card',ref='source',property='csvid')[-1],model.FACE_DOWN)
            raven = self.case('Raven Guild Master',f'hidden_choice_{owner}')['steps']
            picks=[s for s in raven if s['op']=='choose_mana']
            self.assertEqual(picks,[dict(op='choose_mana',player=0,color=COLORS['U'])]*(2 if owner==0 else 0))

    def test_actual_planeswalker_attack_and_fog_suppress_hit_triggers(self):
        for name in self.homes:
            walker = self.case(name,'walker_hit_0')['steps']
            self.assertIn(dict(op='attack_with_tax',ref='source',value=1),walker)
            self.assertEqual(self.values(walker,'assert_life',player=1)[-1],20)
            self.assertEqual(self.values(walker,'assert_zone',player=1,zone='exile')[-1],0)
            fog = self.case(name,'fogged_0')['steps']
            self.assertIn(dict(op='cast',ref='fog',value=1),fog)
            self.assertEqual(self.values(fog,'assert_life',player=1)[-1],20)
        specter = self.case('Silent Specter','ground_evasion_1')['steps']
        self.assertIn(dict(op='block',ref='blocker',other='source',value=0),specter)
        self.assertEqual(self.values(specter,'assert_discard_choices')[-1],2)

    def test_every_selected_case_emits_complete_combat(self):
        ops='begin_attackers finish_attackers begin_blockers advance_combat combat_damage end_combat'.split()
        for name,d in self.homes.items():
            shape=model.spec(d,self.oracle[name])
            for case in model.expand(name,self.rows[name]['csvid'],d,shape,d['variants'],self.rows,self.oracle):
                mode=case['_recipe']['variant'].rsplit('_',1)[0]
                copies=0 if mode in model.OPENING or mode=='noncombat_damage' else 2 if mode=='replay_hit' else 1
                self.assertEqual([s['op'] for s in case['steps'] if s['op'] in ops],ops*copies)


if __name__=='__main__':
    unittest.main()
