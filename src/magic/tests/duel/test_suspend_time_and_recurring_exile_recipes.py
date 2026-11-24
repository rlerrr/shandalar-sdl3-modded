"""Focused timing and input checks for the recurring Suspend pack."""

from copy import deepcopy
from unittest import TestCase

from suspend_time_and_recurring_exile_recipes import (
    FAMILY, SuspendTimingCase, profile, shape, spec, validate_witnesses,
)


class SuspendTimingTests(TestCase):
    def setUp(self):
        pin = profile()
        merged = pin['sources'] | pin['witnesses']
        self.oracle = {name: item['oracle'] for name,item in merged.items()}
        self.rows = {name: item['registry'] for name,item in merged.items()}

    def board(self, name, mode, actor):
        board = SuspendTimingCase(self.rows[name]['csvid'], self.oracle[name],
                                  actor, mode, self.rows, self.oracle)
        board.build()
        return board

    def test_complete_pinned_record_and_implementation(self):
        source = self.oracle['Pardic Dragon']
        declaration = dict(family=FAMILY, version=2, oracle_text=source['oracle_text'],
                           params=shape(source), variants=['opponent_declines_1'])
        self.assertEqual(spec(declaration, source)['duration'], 2)
        with self.assertRaises(ValueError):
            spec(dict(declaration, variants=['opponent_declines_0']), source)
        with self.assertRaises(ValueError):
            spec(declaration, dict(source, power='6'))
        changed = deepcopy(self.rows)
        changed['Pardic Dragon']['handler'] = 'card_dummy'
        with self.assertRaises(ValueError):
            validate_witnesses(self.oracle, changed)

    def test_real_exile_loss_has_one_named_selection_before_upkeep_deadline(self):
        board = self.board('Pardic Dragon', 'exile_loss', 1)
        steps = board.steps
        pull = next(i for i,s in enumerate(steps) if s['op']=='cast' and s['ref']=='pull')
        zone = next(i for i,s in enumerate(steps) if s['op']=='choose_zone_card')
        self.assertEqual(steps[zone], dict(op='choose_zone_card', player=0, value=8626))
        self.assertIn(dict(op='choose_dialog', player=0, value=1),steps[pull:zone])
        self.assertGreaterEqual(sum(s==dict(op='upkeep_step',player=1) for s in steps[zone:]),3)
        self.assertEqual(board.cards['unselected_exile']['zone'],'exile')

    def test_countered_normal_cast_never_runs_the_recurring_effect(self):
        board = self.board('Reality Strobe','countered',0)
        steps = board.steps
        self.assertIn(dict(op='choose_target',ref='first',player=0),steps)
        operations=[s['op'] for s in steps]
        self.assertLess(operations.index('announce_spell'),operations.index('cast_response'))
        self.assertIn(dict(op='resolve_response'),steps)
        self.assertEqual(board.cards['first']['zone'],'battlefield')
        self.assertEqual(board.cards['source']['zone'],'graveyard')

    def test_full_price_creature_cannot_attack_immediately(self):
        steps=self.board('Deep-Sea Kraken','normal_entry',0).steps
        self.assertIn(dict(op='cast',ref='source',value=1),steps)
        self.assertIn(dict(op='attack',ref='source',value=0),steps)
        self.assertFalse(any(s['op']=='advance_turn' for s in steps))
