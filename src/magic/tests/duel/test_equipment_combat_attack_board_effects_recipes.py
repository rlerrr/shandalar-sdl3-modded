"""Focused paid attachment, sacrifice, and natural untap controls for Graft."""

from copy import deepcopy
import unittest

from equipment_combat_attack_board_effects_recipes import (
    FAMILY, GRAFT, GraftBattle, classify, profiles, spec, variants,
)


class EquipmentAttackBoardTests(unittest.TestCase):
    def setUp(self):
        pins = profiles()['sources'] | profiles()['witnesses']
        self.rows = {name: pin['registry'] for name, pin in pins.items()}
        self.oracle = {name: pin['oracle'] for name, pin in pins.items()}
        self.record = self.oracle["Stitcher's Graft"]

    def board(self, mode, owner):
        game = GraftBattle(self.record, GRAFT, owner, mode, self.rows, self.oracle)
        game.play()
        return game

    def untap_receipts(self, game):
        receipts = []
        for start, step in enumerate(game.steps):
            if step['op'] != 'untap_step':
                continue
            for observed in game.steps[start + 1:]:
                if observed.get('ref') == 'host' and observed.get('property') == 'tapped':
                    receipts.append((step['player'], observed['value']))
                    break
        return receipts

    def test_graft_skips_one_real_untap_only_after_its_equipped_host_attacks(self):
        for owner in (0, 1):
            freeze = self.board('freeze_once', owner)
            self.assertEqual(self.untap_receipts(freeze),
                             [(1 - owner, 1), (owner, 1), (1 - owner, 1), (owner, 0)])
            unaffected = self.board('tapped_other_attacks', owner)
            self.assertEqual(self.untap_receipts(unaffected), [(1 - owner, 1), (owner, 0)])
            self.assertIn(dict(op='activate', ref='tapper', value=1), unaffected.steps)

    def test_paid_reequip_sacrifices_old_indestructible_host(self):
        game = self.board('reequip_sacrifice', 1)
        self.assertEqual([s['ref'] for s in game.steps if s['op'] == 'choose_target'], ['host', 'backup'])
        self.assertEqual(game.cards['host']['name'], 'Darksteel Myr')
        self.assertEqual(game.cards['host']['zone'], 'graveyard')
        self.assertEqual(game.equipped, 'backup')
        self.assertEqual(game.life, [16, 20])

    def test_actual_source_removal_and_host_departure_have_separate_outcomes(self):
        for mode, source, host in (
                ('destroy_source', 'graveyard', 'graveyard'),
                ('bounce_source_recast', 'battlefield', 'graveyard'),
                ('host_departure', 'battlefield', 'hand'),
                ('short_unattached_removal', 'graveyard', 'battlefield')):
            game = self.board(mode, 0)
            self.assertEqual((game.cards['source']['zone'], game.cards['host']['zone']), (source, host))
        returned = self.board('bounce_source_recast', 0)
        self.assertEqual([s['ref'] for s in returned.steps if s['op'] == 'cast'], ['source', 'reply', 'source'])
        self.assertIn(dict(op='bind_hand', ref='source', player=0, value=GRAFT), returned.steps)

    def test_exact_whole_oracle_distinguishes_cast_and_equip_prices(self):
        model = classify(self.record, GRAFT)
        declaration = dict(family=FAMILY, version=2, params=model, oracle_text=self.record['oracle_text'],
                           variants=variants(model))
        parsed = spec(declaration, self.record)
        self.assertEqual(parsed['cast_mana'][0], 1)
        self.assertEqual(parsed['equip_generic'], 2)
        wrong = deepcopy(declaration)
        wrong['params']['equip_generic'] = 1
        with self.assertRaises(ValueError):
            spec(wrong, self.record)
