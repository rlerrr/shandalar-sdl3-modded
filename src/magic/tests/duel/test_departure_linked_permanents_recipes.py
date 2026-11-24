"""Check whole-source rejection and critical linked-departure lifecycles."""

from copy import deepcopy
import unittest

import departure_linked_permanents_recipes as model


class LinkedDepartureTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        pins = model.profiles()
        cls.oracle = {name: row['oracle'] for name, row in (pins['sources'] | pins['witnesses']).items()}
        cls.rows = {name: row['registry'] for name, row in (pins['sources'] | pins['witnesses']).items()}
        cls.names = {row['csvid']: name for name,row in cls.rows.items()}

    def board(self, csvid, mode, actor=0):
        factory = model.Board if csvid in (model.AURIFICATION,model.THIEF,model.SHELL,model.TOMB) else model.LinkedBoard
        result = factory(self.oracle[self.names[csvid]],csvid,mode,actor,self.rows,self.oracle)
        result.play()
        return result

    def test_changed_death_rule_and_duplicate_variants_rejected(self):
        source = self.oracle[self.names[model.SHELL]]
        shape = model.classify(source,model.SHELL)
        declaration = dict(family=model.FAMILY,version=2,oracle_text=source['oracle_text'],
                           params=shape,variants=['creature_order_0'])
        self.assertEqual(model.spec(declaration,source)['csvid'],model.SHELL)
        changed = deepcopy(source)
        changed['oracle_text'] = changed['oracle_text'].replace('dies','leaves the battlefield')
        with self.assertRaises(ValueError):
            model.spec(declaration,changed)
        with self.assertRaises(ValueError):
            model.spec(dict(declaration,variants=['creature_order_0']*2),source)

    def test_shell_reports_missing_reveal_as_ordinary_and_keeps_typed_return(self):
        board = self.board(model.SHELL,'creature_order',1)
        reveal, = [i for i,s in enumerate(board.steps) if s['op']=='choose_zone']
        self.assertEqual([s['op'] for s in board.steps[reveal:reveal+3]],
                         ['choose_zone','resolve','assert_unused_choices'])
        self.assertFalse(any(s['op']=='bind_permanent' for s in board.steps))
        self.assertEqual(board.cards['library_1']['zone'],'battlefield')

    def test_parallax_restores_only_linked_exile_to_each_owner(self):
        board = self.board(model.TIDE,'both_owners')
        self.assertEqual([s['value'] for s in board.steps if s['op']=='assert_counter'],[5,4,3])
        self.assertEqual((board.cards['victim']['player'],board.cards['victim']['zone']),(1,'battlefield'))
        self.assertEqual((board.cards['second']['player'],board.cards['second']['zone']),(0,'battlefield'))
        self.assertEqual(board.cards['unlinked']['zone'],'exile')

    def test_reality_acid_each_upkeep_reduces_time_before_vanish(self):
        board = self.board(model.ACID,'vanish',1)
        self.assertEqual([s['value'] for s in board.steps if s['op']=='assert_counter'],[3,2,1])
        self.assertEqual(sum(s['op']=='upkeep_step' and s['player']==1 for s in board.steps),3)
        self.assertEqual(board.cards['host']['zone'],'graveyard')
        self.assertEqual(board.cards['other']['zone'],'battlefield')

    def test_artisan_proves_copy_lives_until_next_end_step(self):
        board = self.board(model.ARTISAN,'delayed_foreign')
        transition = [i for i,s in enumerate(board.steps) if s['op']=='advance_turn'][-1]
        expected_copy_after_bounce = board.steps[transition-1]
        self.assertEqual((expected_copy_after_bounce['op'],expected_copy_after_bounce['player'],
                          expected_copy_after_bounce['zone'],expected_copy_after_bounce['value']),
                         ('assert_zone_multiplicity',1,'battlefield',1))
        self.assertEqual(board.cards['copy']['zone'],'gone')

    def test_lich_real_damage_replacement_uses_victims_resource(self):
        board = self.board(model.NEFARIOUS,'replace_damage',1)
        self.assertEqual(board.life,[20,20])
        self.assertEqual([board.cards[f'fodder_{i}']['zone'] for i in range(3)],['exile','exile','graveyard'])
        self.assertEqual(self.board(model.NEFARIOUS,'short_grave').steps[-1],
                         {'op':'advance_scheduled_turn','value':-1})


if __name__ == '__main__':
    unittest.main()
