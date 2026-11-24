"""Guard the strongest independent entry scope and payment distinctions."""

from copy import deepcopy
import unittest

import entry_trigger_damage_and_tap_recipes as model


class EntryDamageAndTapGuards(unittest.TestCase):
    def fixtures(self):
        pairs={}
        for group in model.profiles().values():
            pairs.update(group)
        return {name:p['oracle'] for name,p in pairs.items()}, {name:p['registry'] for name,p in pairs.items()}

    def case(self,name,mode,seat):
        oracle,rows=self.fixtures()
        board=model.FollowOn(oracle[name],rows[name]['csvid'],mode,seat,rows,oracle)
        board.play()
        return board

    def test_whole_primary_and_exact_variants(self):
        oracle,rows=self.fixtures()
        model.validate_witnesses(oracle,rows)
        record=oracle['Timbermare']
        model_params=model.classify(record,model.TIMBER)
        recipe=dict(family=model.FAMILY,version=2,oracle_text=record['oracle_text'],
                    params=model_params,variants=model.variants(model_params))
        self.assertEqual(model.spec(recipe,record)['csvid'],model.TIMBER)
        changed=deepcopy(record)
        changed['oracle_text']=changed['oracle_text'].replace('Echo {5}{G}','Echo {5}{R}')
        with self.assertRaises(ValueError):
            model.spec(recipe,changed)
        with self.assertRaises(ValueError):
            model.spec(recipe | {'variants':['haste_0','haste_0']},record)

    def test_area_scope_and_self_damage_distinct(self):
        for name,expected in (('Raging Swordtooth',0),('Magma Giant',2),('Crater Hellion',0),('Thunder Dragon',0)):
            board=self.case(name,'scope',0)
            values={s['ref']:s['value'] for s in board.steps if s['op']=='assert_card' and s.get('property')=='damage'}
            self.assertEqual(values['source'],expected)
            self.assertEqual(board.life,[18,18] if expected else [20,20])
            self.assertEqual(board.cards['ally']['zone'],'graveyard')
            self.assertEqual(board.cards['relic']['zone'],'battlefield')

    def test_printed_echo_colors_independent_of_engine_handler(self):
        for seat,lands in ((0,['Wastes']*5+['Forest']),(1,['Forest']*6)):
            board=self.case('Timbermare','echo_pay',seat)
            self.assertEqual([board.cards[ref]['name'] for ref in board.echo_lands],lands)
            self.assertEqual(board.cards['source']['zone'],'battlefield')
            self.assertEqual(sum(s['op']=='choose_mana_source' for s in board.steps),6 if seat==0 else 0)

    def test_red_devotion_at_resolution_and_control(self):
        for mode,life in (('self_only',19),('own_symbols',17),('hybrid',16),('support_lost',19)):
            board=self.case('Fanatic of Mogis',mode,0)
            self.assertEqual(board.life,[20,life])

    def test_separate_aura_target_and_damage_target(self):
        board=self.case('Lightning Diadem','lethal',0)
        self.assertEqual([s['ref'] for s in board.steps if s['op']=='choose_target'],['host','guard'])
        self.assertEqual(board.cards['guard']['zone'],'graveyard')
        self.assertEqual(board.cards['host']['zone'],'battlefield')


if __name__=='__main__':
    unittest.main()

