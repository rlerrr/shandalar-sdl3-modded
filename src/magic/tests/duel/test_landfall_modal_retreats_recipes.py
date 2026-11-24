"""Guard typed Retreat declarations and ordered mode inputs."""

import copy
import unittest

import landfall_modal_retreats_recipes as model


class RetreatGuard(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        records = model.pins()['sources'] | model.pins()['witnesses']
        cls.oracle = {name: pin['oracle'] for name, pin in records.items()}
        cls.registry = {name: pin['registry'] for name, pin in records.items()}

    def case(self, name, mode, seat):
        record = self.oracle[name]
        shape = model.classify(record)
        variant = f'{mode}_{seat}'
        declaration = dict(family=model.FAMILY, version=2, params=shape, oracle_text=record['oracle_text'],
                           variants=[variant])
        return model.expand(name, self.registry[name]['csvid'], declaration, model.spec(declaration, record),
                            [variant], self.registry, self.oracle)[0]

    def test_whole_clauses_and_distinct_variants(self):
        for record in model.pins()['sources'].values():
            oracle = record['oracle']
            shape = model.classify(oracle)
            name, *_ = model.variants(shape)
            declaration = dict(family=model.FAMILY, version=2, params=shape, oracle_text=oracle['oracle_text'],
                               variants=[name])
            self.assertEqual(model.spec(declaration, oracle)['mana'], shape['mana'])
            for change in ({'params': {**shape, 'mana': [0] * 6}}, {'variants': [name, name]}):
                with self.assertRaises(ValueError):
                    model.spec({**declaration, **change}, oracle)
            changed = copy.copy(oracle)
            changed['oracle_text'] += '\nDraw a card.'
            with self.assertRaises(ValueError):
                model.classify(changed)

    def test_coralhelm_mode_target_and_effect_are_ordered(self):
        for seat in (0, 1):
            for mode, target, action in (('tap_foe', 'foe_creature', 0), ('untap_own', 'ally', 1),
                                         ('pass', 'ally', 2)):
                steps = self.case('Retreat to Coralhelm', mode, seat)['steps']
                land = next(i for i, step in enumerate(steps) if step['op'] == 'play_land')
                self.assertEqual([step['op'] for step in steps[land - 3:land]],
                                 ['choose_dialog', 'choose_target', 'choose_dialog'])
                self.assertEqual(steps[land - 2]['ref'], target)
                self.assertEqual(steps[land - 1]['value'], action)
                self.assertTrue(all(step['player'] == seat for step in steps[land - 3:land]))

    def test_real_combat_and_durable_counter(self):
        for seat in (0, 1):
            for mode, block in (('no_block', 0), ('other_blocks', 1), ('block_expiry', 1)):
                steps = self.case('Retreat to Valakut', mode, seat)['steps']
                land = next(i for i, step in enumerate(steps) if step['op'] == 'play_land')
                seen = {step['op']: i for i, step in enumerate(steps)}
                self.assertLess(land, seen['block'])
                self.assertLess(seen['block'], seen['combat_damage'])
                self.assertEqual(steps[seen['block']]['value'], block)
            steps = self.case('Retreat to Kazandu', 'counter_then_life', seat)['steps']
            first, second = [i for i, step in enumerate(steps) if step['op'] == 'play_land']
            self.assertEqual(sum(step['op'] == 'advance_turn' for step in steps[first:second]), 2)
            self.assertTrue(any(step['op'] == 'assert_card' and step['ref'] == 'ally'
                                and step['property'] == 'plus1_counters' and step['value'] == 1
                                for step in steps[second:]))
