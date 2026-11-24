"""Check response, token scope, and graveyard safety in the token-entry family."""
from copy import deepcopy
import unittest

import entry_trigger_tokens_recipes as model


class EntryTokenCases(unittest.TestCase):
    def setUp(self):
        pins=model.profiles()
        entries=pins['sources'] | pins['witnesses']
        self.oracle={name:pin['oracle'] for name,pin in entries.items()}
        self.rows={name:pin['registry'] for name,pin in entries.items()}
        self.ids={row['csvid']:row for row in self.rows.values()}

    def board(self, card, mode, seat):
        kind=model.FollowBoard if card in model.FOLLOW_MODES else model.Board
        game=kind(self.ids[card],mode,seat,self.rows,self.oracle)
        game.run()
        return game

    def test_modified_pins_are_rejected(self):
        model.validate_witnesses(self.oracle,self.rows)
        oracle=deepcopy(self.oracle)
        oracle[model.KNIGHT]['type_line']='Token Creature — Warrior'
        with self.assertRaises(ValueError):
            model.validate_witnesses(oracle,self.rows)

    def test_native_response_drains_both_spells(self):
        for card,mode in ((model.ELITE,'elf_removed'),(model.BOND,'target_gone')):
            for seat in (0,1):
                steps=self.board(card,mode,seat).steps
                edge=next(i for i,step in enumerate(steps) if step['op']=='resolve_response')
                self.assertEqual(steps[edge+3],dict(op='assert_stack',value=0))
                self.assertFalse(any(step['op']=='bind_permanent_copy' for step in steps))

    def test_servant_uses_graveyard_count_without_active_slot_assumptions(self):
        for seat in (0,1):
            for mode,count in (('three',3),('foreign',0)):
                game=self.board(model.SERVANT,mode,seat)
                self.assertEqual(sum(step['op']=='bind_permanent_copy' for step in game.steps),count)
                self.assertFalse(any(step['op']=='assert_card' and step['ref'] in game.zone_only
                                     for step in game.steps))

    def test_real_phase_riders_are_distinct_from_token_birth(self):
        for seat in (0,1):
            game=self.board(model.CARAVAN,'own_attacked',seat)
            ops=[step['op'] for step in game.steps]
            self.assertLess(ops.index('end_combat'),ops.index('cast'))
            self.assertIn(dict(op='assert_card',ref='token_0',property='tapped',value=0),game.steps)
            game=self.board(model.PRIEST,'upkeep',seat)
            self.assertEqual([s['player'] for s in game.steps if s['op']=='upkeep_step'],[1-seat,seat])
            self.assertEqual(game.life[seat],18)

    def test_scion_preexisting_copy_avoids_unbound_same_callback_bird(self):
        self.assertNotIn('bird_only_0',model.variants({'csvid':model.SCION}))
        game=self.board(model.SCION,'populate_existing',0)
        choices=[s['ref'] for s in game.steps if s['op']=='choose_target']
        self.assertEqual(choices,['centaur_0'])
        self.assertEqual(sum(name==model.CENTAUR for name,_,_ in game.cards.values()),2)
