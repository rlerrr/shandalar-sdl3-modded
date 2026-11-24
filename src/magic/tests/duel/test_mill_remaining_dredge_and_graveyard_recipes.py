"""Focused tests for real Dredge and the two printed historic riders."""

from copy import deepcopy
import unittest

import mill_remaining_dredge_and_graveyard_recipes as model


class DredgeAndGraveyardTests(unittest.TestCase):
    def scene(self, cid, seat, mode):
        data = model.pins()
        all_pins = data['witnesses'] | data['sources']
        rows = {name: pin['registry'] for name, pin in all_pins.items()}
        oracle = {name: pin['oracle'] for name, pin in all_pins.items()}
        source, = [row for row in rows.values() if row['csvid'] == cid]
        scene = model.Scene(source, seat, mode, rows, oracle)
        scene.play()
        return scene

    def test_whole_oracle_is_required(self):
        for pin in model.pins()['sources'].values():
            record, cid = pin['oracle'], pin['registry']['csvid']
            self.assertEqual(model.classify(record, cid)['dredge'], model.AMOUNTS[cid])
            changed = deepcopy(record)
            changed['oracle_text'] += ' '
            with self.assertRaises(ValueError):
                model.classify(changed, cid)

    def test_necroplasm_counts_and_ends_remain_real(self):
        for seat in (0, 1):
            scene = self.scene(model.NECROPLASM, seat, 'growth_self')
            counters = [s['value'] for s in scene.steps if s['op'] == 'assert_counter']
            self.assertEqual(counters, [0, 0, 0, 1, 1, 1, 1, 2, 2, 2, 2, 3, 3])
            self.assertEqual([s['player'] for s in scene.steps if s['op'] == 'upkeep_step'],
                             [1 - seat, seat] * 3)
            self.assertEqual(len([s for s in scene.steps if s['op'] == 'advance_turn']), 7)
            self.assertIn('Necroplasm', scene.zones[seat]['graveyard'])
            self.assertEqual(scene.zones[seat]['battlefield'], ['Forest'])
            self.assertEqual(scene.zones[1 - seat]['battlefield'], ["Tormod's Crypt"])

    def test_necroplasm_opponent_and_zero_turn_are_distinct(self):
        for seat in (0, 1):
            opponent = self.scene(model.NECROPLASM, seat, 'opponent_end')
            own = self.scene(model.NECROPLASM, seat, 'zero_end')
            self.assertIn('Memnite', opponent.zones[seat]['battlefield'])
            self.assertIn('Ornithopter', opponent.zones[1 - seat]['battlefield'])
            self.assertIn('Memnite', own.zones[seat]['graveyard'])
            self.assertIn('Ornithopter', own.zones[1 - seat]['graveyard'])
            self.assertIn('Necroplasm', own.zones[seat]['battlefield'])

    def test_imp_combat_and_fog_are_discriminated(self):
        for seat in (0, 1):
            attack = self.scene(model.IMP, seat, 'attack_destroy')
            self.assertIn('Wall of Air', attack.zones[1 - seat]['graveyard'])
            self.assertIn(dict(op='assert_card', ref='source', property='damage', value=1), attack.steps)
            defend = self.scene(model.IMP, seat, 'defend_destroy')
            self.assertIn('Stinkweed Imp', defend.zones[seat]['graveyard'])
            self.assertIn('Phantom Monster', defend.zones[1 - seat]['graveyard'])
            fog = self.scene(model.IMP, seat, 'combat_prevented')
            self.assertIn('Wall of Air', fog.zones[1 - seat]['battlefield'])
            self.assertIn('Fog', fog.zones[1 - seat]['graveyard'])

    def test_imp_noncombat_damage_has_no_destruction(self):
        for seat in (0, 1):
            scene = self.scene(model.IMP, seat, 'noncombat')
            self.assertIn(dict(op='activate', ref='grant', value=1), scene.steps)
            self.assertNotIn(dict(op='combat_damage'), scene.steps)
            self.assertIn(dict(op='assert_card', ref='recipient', property='damage', value=1), scene.steps)
            self.assertIn('Wall of Air', scene.zones[1 - seat]['battlefield'])

    def test_exact_and_short_dredge_differ_by_one(self):
        for cid in (model.NECROPLASM, model.IMP):
            for seat in (0, 1):
                exact = self.scene(cid, seat, 'exact')
                short = self.scene(cid, seat, 'short')
                self.assertEqual(len(exact.zones[seat]['graveyard']), 1 + model.AMOUNTS[cid])
                self.assertEqual(exact.zones[seat]['hand'], [exact.name])
                self.assertEqual(short.zones[seat]['hand'], ['Forest'])
                self.assertIn(short.name, short.zones[seat]['graveyard'])
