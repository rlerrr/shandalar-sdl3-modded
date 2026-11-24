"""Focused checks for sacrifices, typed riders, and foreign exile permission."""
from copy import deepcopy
import unittest

import regeneration_follow_on_sacrifice_and_riders_recipes as model


class SacrificeRegenerationTests(unittest.TestCase):
    def play(self, cid, mode, owner):
        pins = model.pins()['sources'] | model.pins()['witnesses']
        rows = {name: pin['registry'] for name, pin in pins.items()}
        oracle = {name: pin['oracle'] for name, pin in pins.items()}
        row, = (row for row in rows.values() if row['csvid'] == cid)
        scene = model.Scene(row, owner, mode, rows, oracle)
        scene.play()
        return scene

    def test_all_sources_guard_complete_record(self):
        self.assertEqual(set(model.MODES), {p['registry']['csvid'] for p in model.pins()['sources'].values()})
        for pin in model.pins()['sources'].values():
            record, cid = pin['oracle'], pin['registry']['csvid']
            self.assertEqual(model.classify(record, cid)['csvid'], cid)
            changed = deepcopy(record)
            changed['oracle_text'] = record['oracle_text'][1:]
            with self.assertRaisesRegex(ValueError, 'complete normal regeneration rules'):
                model.classify(changed, cid)

    def test_artifact_creature_payment_changes_nim_power_only(self):
        for seat in (0, 1):
            scene = self.play(model.NIM, 'paid', seat)
            power = [s['value'] for s in scene.steps if s.get('property') == 'power']
            self.assertEqual(power, [4, 3])
            self.assertEqual(scene.zones[seat]['graveyard'], ['Ornithopter'])
            self.assertEqual(scene.zones[1-seat]['battlefield'], ['Darksteel Relic'])
            self.assertEqual([s['value'] for s in scene.steps if s.get('property') == 'toughness'], [1, 1])

    def test_rhys_counts_only_own_elves_and_own_attacks(self):
        for seat in (0, 1):
            for mode, life in (('attack_self', 21), ('attack_count', 22), ('other_attacks', 20)):
                scene = self.play(model.RHYS, mode, seat)
                self.assertEqual(scene.life[seat], life)
                self.assertEqual(scene.zones[1-seat]['battlefield'], ['Elvish Archers'])
                self.assertIn(dict(op='attack', ref='non_elf' if mode == 'other_attacks' else 'source', value=1),
                              scene.steps)

    def test_wildcats_count_typed_forests_and_last_payment(self):
        for seat in (0, 1):
            typed = self.play(model.WILDCATS, 'typed_forest', seat)
            self.assertEqual(typed.zones[seat]['graveyard'], ['Taiga'])
            self.assertEqual([s['value'] for s in typed.steps if s.get('property') == 'power'], [2, 1])
            last = self.play(model.WILDCATS, 'last_forest', seat)
            self.assertEqual(last.zones[seat]['graveyard'], ['Forest', 'Uktabi Wildcats'])
            growth = self.play(model.WILDCATS, 'forest_growth', seat)
            self.assertIn(dict(op='play_land', ref='new_forest', value=1), growth.steps)
            self.assertEqual([s['value'] for s in growth.steps if s.get('property') == 'power'], [1, 2])

    def test_fiend_plays_foreign_card_with_actual_fee_after_source_dies(self):
        for seat in (0, 1):
            foe = 1-seat
            scene = self.play(model.FIEND, 'exile_persists', seat)
            pick = next(s for s in scene.steps if s['op'] == 'choose_zone_card')
            use, = [s for s in scene.steps if s['op'] == 'play_exiled']
            self.assertEqual((pick['player'], use['player'], use['owner']), (foe, seat, foe))
            self.assertIn('Fiend of the Shadows', scene.zones[seat]['graveyard'])
            self.assertIn(model.EXILED_SPELL, scene.zones[foe]['graveyard'])
            self.assertEqual(scene.zones[seat]['hand'], ['Island'])
            ops = [s['op'] for s in scene.steps]
            play = ops.index('play_exiled')
            self.assertLess(ops.index('combat_damage'), play)
            self.assertEqual(scene.steps[play+1], dict(op='resolve'))
            last_untap = max(i for i, step in enumerate(scene.steps) if step['op'] == 'untap_step')
            self.assertEqual(scene.steps[last_untap:last_untap+2],
                             [dict(op='untap_step', player=foe), dict(op='turn', player=foe)])
            live = self.play(model.FIEND, 'exile_play', seat)
            live_play = next(i for i, step in enumerate(live.steps) if step['op'] == 'play_exiled')
            self.assertEqual(live.steps[live_play+1], dict(op='resolve'))
            blocked = self.play(model.FIEND, 'reach_no_hit', seat)
            self.assertIn(dict(op='block', ref='reach_foil', other='source', value=1), blocked.steps)
            self.assertEqual(blocked.zones[foe]['exile'], [])
            self.assertEqual(blocked.life[foe], 20)

    def test_proactive_regeneration_uses_no_destruction_window_input(self):
        for cid in model.MODES:
            for seat in (0, 1):
                scene = self.play(cid, 'proactive', seat)
                ops = [s['op'] for s in scene.steps]
                self.assertIn(dict(op='can_activate', ref='source', value=1), scene.steps)
                self.assertNotIn('choose_regeneration', ops)
                self.assertLess(ops.index('activate'), max(i for i, op in enumerate(ops) if op == 'cast'))
