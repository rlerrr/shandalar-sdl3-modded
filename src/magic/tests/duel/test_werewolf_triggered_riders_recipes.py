"""Verify the complete Oracle and variant bindings for this recipe."""
import copy
import unittest

import werewolf_triggered_riders_recipes as recipe


class PinnedRecipeTests(unittest.TestCase):
    def test_sources_bind_entire_printed_records(self):
        sources = recipe.profiles()['sources']
        self.assertEqual(len(sources), 8)
        for name, pin in sources.items():
            with self.subTest(card=name):
                record = pin['oracle']
                physical = True
                printed = record['selected_face'] if physical else record
                shape = recipe.classify(record, pin['back_oracle']) if physical else recipe.classify(record)
                variants = recipe.variants(shape)
                self.assertEqual(len(variants), len(set(variants)))
                self.assertGreater(len(variants), 2)
                declaration = dict(family=recipe.FAMILY, version=2, oracle_text=printed['oracle_text'],
                                   params=shape, variants=variants)
                self.assertEqual(recipe.spec(declaration, record)['family'], recipe.FAMILY)
                changed_oracle = copy.deepcopy(record)
                changed_printed = changed_oracle['selected_face'] if physical else changed_oracle
                changed_printed['oracle_text'] += '\nFlying'
                with self.assertRaises(ValueError):
                    recipe.spec(declaration, changed_oracle)
                declaration['variants'] = variants + [variants[0]]
                with self.assertRaises(ValueError):
                    recipe.spec(declaration, record)



class ReceiptTests(unittest.TestCase):
    @staticmethod
    def case(name, mode, owner):
        profiles = recipe.profiles()
        pins = {**profiles['sources'], **profiles['witnesses']}
        pins.update({pin['back_oracle']['engine_face']: {
            'oracle': pin['back_oracle'], 'registry': pin['back_registry']}
            for pin in profiles['sources'].values()})
        oracle = {card: pin['oracle'] for card, pin in pins.items()}
        rows = {card: pin['registry'] for card, pin in pins.items()}
        world = recipe.Riders(name, recipe.classify(oracle[name]), mode, owner, rows, oracle)
        world.run()
        return world.steps

    def test_haste_before_actual_ready_turn(self):
        for owner in (0, 1):
            steps = self.case('Geier Reach Bandit', 'own_wolf', owner)
            sick = [step['value'] for step in steps
                    if step.get('ref') == 'source' and step.get('property') == 'sick']
            self.assertTrue(sick)
            self.assertEqual(set(sick), {0})

    def test_pending_blocker_damage_uses_actual_processing_boundaries(self):
        for owner in (0, 1):
            for mode in ('front_blocks', 'front_blocked', 'back_blocks', 'back_blocked'):
                with self.subTest(owner=owner, mode=mode):
                    steps = self.case('Kessig Forgemaster', mode, owner)
                    declared = next(i for i, step in enumerate(steps)
                                    if step['op'] == 'advance_combat')
                    combat = next(i for i, step in enumerate(steps)
                                  if step['op'] == 'combat_damage')
                    between = steps[declared + 1:combat]
                    after = steps[combat + 1:]
                    opposing = 'enemy_attacker' if mode.endswith('_blocks') else 'blocker'
                    if mode.startswith('front'):
                        cast = next(i for i, step in enumerate(between) if step['op'] == 'cast')
                        resolved = next(i for i, step in enumerate(between) if step['op'] == 'resolve')
                        self.assertLess(cast, resolved)
                        who = 1 - owner if mode == 'front_blocks' else owner
                        self.assertEqual(between[cast]['ref'], f'fog_{who}_0')
                        for phase in (between[resolved + 1:], after):
                            self.assertTrue(any(step['op'] == 'assert_card' and step.get('ref') == opposing
                                                and step.get('property') == 'damage' and step['value'] == 1
                                                for step in phase))
                    else:
                        self.assertEqual(between, [])
                        self.assertTrue(any(step['op'] == 'assert_zone_multiplicity'
                                            and step.get('player') == 1 - owner
                                            and step.get('zone') == 'graveyard' and step.get('index') == 106
                                            and step['value'] == 1 for step in after))
                    self.assertTrue(any(step['op'] == 'assert_card' and step.get('ref') == 'source'
                                        and step.get('property') == 'damage' and step['value'] == 0
                                        for step in after))


if __name__ == '__main__':
    unittest.main()
