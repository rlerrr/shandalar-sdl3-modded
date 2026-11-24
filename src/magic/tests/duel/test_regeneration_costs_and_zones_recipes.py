"""Focused guards for real regeneration costs and full printed follow-ons."""

from copy import deepcopy
import unittest

import regeneration_costs_and_zones_recipes as model


class RegenerationCostsTests(unittest.TestCase):
    def play(self, cid, mode, owner):
        pins = model.pins()['sources'] | model.pins()['witnesses']
        registry = {name: pin['registry'] for name, pin in pins.items()}
        oracle = {name: pin['oracle'] for name, pin in pins.items()}
        row, = [row for row in registry.values() if row['csvid'] == cid]
        scene = model.Scene(row, owner, mode, registry, oracle)
        scene.play()
        return scene

    def test_profile_requires_full_oracle(self):
        for pin in model.pins()['sources'].values():
            record, cid = pin['oracle'], pin['registry']['csvid']
            self.assertEqual(model.classify(record, cid)['csvid'], cid)
            changed = deepcopy(record)
            changed['oracle_text'] = record['oracle_text'][1:]
            with self.assertRaisesRegex(ValueError, 'complete normal regeneration rules'):
                model.classify(changed, cid)

    def test_proactive_ability_and_later_destruction_are_distinct(self):
        for cid in model.MODES:
            for owner in (0, 1):
                scene = self.play(cid, 'proactive', owner)
                ops = [step['op'] for step in scene.steps]
                self.assertLess(ops.index('activate'), max(i for i, op in enumerate(ops) if op == 'cast'))
                self.assertNotIn('choose_regeneration', ops)
                self.assertIn(dict(op='can_activate', ref='source', value=1), scene.steps)
                taps = [step['value'] for step in scene.steps if step.get('property') == 'tapped']
                self.assertEqual(taps, [0, 0, 1])

    def test_kathari_flies_but_reach_damage_can_regenerate(self):
        scene = self.play(model.KATHARI, 'flight', 0)
        self.assertIn(dict(op='block', ref='ground', other='source', value=0), scene.steps)
        self.assertIn(dict(op='block', ref='reach', other='source', value=1), scene.steps)
        self.assertIn(dict(op='combat_damage'), scene.steps)
        self.assertIn(dict(op='assert_card', ref='reach', property='damage', value=3), scene.steps)
        self.assertIn('Grizzly Bears', scene.zones[0]['graveyard'])
        self.assertIn('Skeletal Kathari', scene.zones[0]['battlefield'])
        self.assertEqual(scene.pools[0], model.ZERO)

    def test_wurm_cost_and_damage_are_independent(self):
        for owner in (0, 1):
            scene = self.play(model.WURM, 'damage', owner)
            self.assertEqual(scene.zones[owner]['graveyard'], ['Forest'])
            self.assertEqual(scene.zones[1-owner]['graveyard'], ['Lightning Bolt'] * 2)
            damage = [s['value'] for s in scene.steps if s.get('property') == 'damage']
            self.assertEqual(damage, [0, 3, 0])

    def test_zombie_uses_newest_creature_before_older_creature(self):
        for owner in (0, 1):
            first = self.play(model.SCAVENGERS, 'paid', owner)
            second = self.play(model.SCAVENGERS, 'grave_second', owner)
            self.assertEqual(first.zones[owner]['exile'], ['Grizzly Bears'])
            self.assertEqual(first.zones[owner]['graveyard'], ['Bog Imp', 'Unsummon'])
            self.assertEqual(second.zones[owner]['exile'], ['Grizzly Bears', 'Bog Imp'])
            self.assertEqual(second.zones[owner]['graveyard'], ['Unsummon'])

    def test_varolz_scavenge_uses_printed_fee_and_power(self):
        for owner in (0, 1):
            for mode, card, power in (('scavenge_paid', 'Hill Giant', 3),
                                      ('scavenge_zero', 'Memnite', 1),
                                      ('scavenge_other_target', 'Hill Giant', 3)):
                scene = self.play(model.VAROLZ, mode, owner)
                self.assertIn(dict(op='activate', ref='source', value=1), scene.steps)
                self.assertEqual(scene.zones[owner]['exile'], [card])
                self.assertEqual(scene.zones[1-owner]['graveyard'], ['Walking Corpse'])
                self.assertIn(dict(op='assert_card', ref='recipient', property='plus1_counters', value=power), scene.steps)
                used = next(i for i, s in enumerate(scene.steps) if s.get('ref') == 'source' and s['op']=='activate')
                exiled = next(i for i, s in enumerate(scene.steps) if s.get('zone')=='exile')
                self.assertLess(used, exiled)
            for mode in ('scavenge_other_grave', 'scavenge_offturn', 'scavenge_wrong_color'):
                scene = self.play(model.VAROLZ, mode, owner)
                self.assertIn(dict(op='can_activate', ref='source', value=0), scene.steps)
                self.assertEqual(scene.zones[owner]['exile'], [])
