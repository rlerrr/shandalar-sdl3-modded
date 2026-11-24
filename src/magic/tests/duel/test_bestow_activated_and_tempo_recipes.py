"""Focused Bestow rider and real action checks."""
from copy import deepcopy
import unittest

from bestow_activated_and_tempo_recipes import (
    BOON, EREBOS, EVERFLAME, FAMILY, HERALD, SPIRE, TempoScene, profiles, shape, spec, validate_witnesses, variants,
)


def context():
    pins = profiles()
    oracle = {name: pin['oracle'] for entries in pins.values() for name, pin in entries.items()}
    rows = {name: pin['registry'] for entries in pins.values() for name, pin in entries.items()}
    rows.update((pin['engine_aura_face']['name'], pin['engine_aura_face']) for pin in pins['sources'].values())
    return oracle, rows


def scene(csvid, player, mode):
    oracle, rows = context()
    name = next(name for name in profiles()['sources'] if rows[name]['csvid'] == csvid)
    aura = profiles()['sources'][name]['engine_aura_face']
    world = TempoScene(name, player, mode, rows, oracle, aura)
    world.build()
    return world


class BestowRiderGuards(unittest.TestCase):
    def test_full_oracle_and_variants_fail_closed(self):
        oracle, _ = context()
        record = oracle['Spirespine']
        params = shape(record, SPIRE)
        declaration = dict(family=FAMILY, version=2, params=params, oracle_text=record['oracle_text'],
                           variants=variants(params))
        self.assertEqual(spec(declaration, record)['csvid'], SPIRE)
        with self.assertRaises(ValueError):
            spec(declaration, dict(record, oracle_text=record['oracle_text'].replace('blocks each combat', 'may block')))
        for bad in (dict(declaration, version=2.0), dict(declaration, variants=['detach_0', 'detach_0'])):
            with self.assertRaises(ValueError):
                spec(bad, record)

    def test_exact_aura_face_registry(self):
        oracle, rows = context()
        validate_witnesses(oracle, rows)
        changed = deepcopy(rows)
        changed['Spirespine Bestowed']['iid'] += 1
        with self.assertRaises(ValueError):
            validate_witnesses(oracle, changed)

    def test_real_offturn_flash_becomes_aura_or_creature(self):
        for player in (0, 1):
            for mode, form in (('flash_bestow', 'aura'), ('flash_creature', 'creature')):
                world = scene(BOON, player, mode)
                self.assertEqual(world.turn, 1 - player)
                self.assertEqual(world.form, form)
                self.assertIn(('cast_response', 'source'), [(s['op'], s.get('ref')) for s in world.steps])

    def test_detach_keeps_returned_creature_discardable(self):
        for player in (0, 1):
            world = scene(EREBOS, player, 'detach_ability')
            self.assertEqual(world.objects['host']['zone'], 'hand')
            self.assertEqual([world.objects[ref]['zone'] for ref in ('first', 'second')], ['graveyard', 'graveyard'])
            self.assertEqual([s['value'] for s in world.steps if s['op'] == 'can_activate'], [1, 1, 1])
            other = scene(EREBOS, player, 'aura_own_ability')
            self.assertEqual([s['value'] for s in other.steps if s['op'] == 'can_activate'], [1, 1, 0])

    def test_everflame_real_repeated_red_pump_and_expiry(self):
        for player in (0, 1):
            world = scene(EVERFLAME, player, 'aura_foreign_ability')
            self.assertEqual(world.objects['host']['player'], 1 - player)
            self.assertEqual(sum(s['op'] == 'activate' for s in world.steps), 2)
            self.assertEqual(world.boosts['host'], 0)
            self.assertTrue(any(s['op'] == 'assert_card' and s.get('ref') == 'host'
                                and s.get('property') == 'power' and s['value'] == 5 for s in world.steps))

    def test_herald_upkeep_and_damage_follow_actual_controllers(self):
        for player in (0, 1):
            world = scene(HERALD, player, 'aura_foreign')
            self.assertEqual(world.life[player], 14)
            self.assertEqual(world.life[1 - player], 20)

    def test_spirespine_mandatory_selector_and_illegal_block(self):
        for player in (0, 1):
            required = scene(SPIRE, player, 'aura_foreign')
            plans = [s for s in required.steps if s['op'] == 'choose_target' and s['ref'] == 'raider']
            self.assertEqual([s['player'] for s in plans], [1 - player])
            self.assertFalse(any(s['op'] == 'block' for s in required.steps))
            self.assertEqual(required.objects['raider']['zone'], 'graveyard')
            flight = scene(SPIRE, player, 'flying_attacker')
            self.assertEqual([(s['ref'], s['value']) for s in flight.steps if s['op'] == 'block'], [('source', 0)])
            self.assertEqual(flight.life[player], 18)

