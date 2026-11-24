"""Focused guards for actual draw, replacement and skip boundaries."""

import unittest

import draw_step_skip_and_trigger_recipes as model


class DrawStepSkipAndTriggerGuard(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        profiles = model.profiles()
        pins = profiles['sources'] | profiles['witnesses']
        cls.oracle = {name: pin['oracle'] for name, pin in pins.items()}
        cls.rows = {name: pin['registry'] for name, pin in pins.items()}
        cls.names = {row['csvid']: name for name, row in cls.rows.items()}

    def game(self, csvid, mode, owner=0):
        game = model.Duel(self.rows[self.names[csvid]], owner, mode, self.rows, self.oracle)
        game.build()
        return game

    def test_all_rules_and_witnesses_are_exact(self):
        model.validate_witnesses(self.oracle, self.rows)
        for csvid in model.CASES:
            record = self.oracle[self.names[csvid]]
            shape = model.classify(record, csvid)
            declaration = dict(family=model.FAMILY, version=2, params=shape,
                               oracle_text=record['oracle_text'], variants=model.variants(shape))
            self.assertEqual(model.spec(declaration, record)['csvid'], csvid)
            with self.assertRaises(ValueError):
                model.spec(declaration, {**record, 'oracle_text': record['oracle_text'] + '\nDraw a card.'})
            with self.assertRaises(ValueError):
                model.spec({**declaration, 'version': True}, record)

    def test_sanctuary_replacement_uses_actual_attack_permissions_and_expiry(self):
        for owner in (0, 1):
            game = self.game(127, 'protect_expires', owner)
            attacks = [(step['ref'], step['value']) for step in game.steps if step['op'] == 'attack']
            self.assertEqual(attacks, [('ground', 0), ('air', 1), ('walk', 1),
                                       ('ground', 1), ('air', 1), ('walk', 1)])
            cast_refs = [step['ref'] for step in game.steps if step['op'] == 'cast']
            self.assertEqual(cast_refs, ['source', 'removal'])
            self.assertEqual(game.cards['source'][2], 'graveyard')
            self.assertEqual(game.draw_counts[owner], 1)
            self.assertEqual(game.life[owner], 6)

    def test_fasting_crosses_fifth_own_upkeep_and_can_die_from_outside_draw(self):
        for owner in (0, 1):
            game = self.game(317, 'hunger_five', owner)
            self.assertEqual(game.hunger, 5)
            self.assertEqual(game.life[owner], 28)
            self.assertEqual(game.draw_counts[owner], 1)
            choices = [step['value'] for step in game.steps if step['op'] == 'choose_dialog']
            self.assertEqual(choices, [0] * 4)
            self.assertEqual(game.cards['source'][2], 'graveyard')
            outside = self.game(317, 'outside_draw', owner)
            self.assertEqual(outside.hunger, 0)
            self.assertEqual(outside.draw_counts[owner], 3)
            self.assertEqual(outside.cards['source'][2], 'graveyard')

    def test_recycle_requires_real_land_and_opposing_spell_control(self):
        own_land = self.game(3243, 'own_land')
        self.assertEqual(own_land.draw_counts, [1, 1])
        self.assertEqual([step['ref'] for step in own_land.steps if step['op'] == 'play_land'], ['land'])
        opposing = self.game(3243, 'opposing_spell')
        self.assertEqual(opposing.draw_counts, [0, 3])

    def test_prepared_draw_seat_matches_untap_and_upkeep(self):
        for csvid in model.CASES:
            for variant in model.variants({'csvid': csvid}):
                mode, owner = variant.rsplit('_', 1)
                game = self.game(csvid, mode, int(owner))
                phases = [(step['op'], step['player']) for step in game.steps
                          if step['op'] in ('untap_step', 'upkeep_step', 'draw_step')]
                for i in range(0, len(phases), 3):
                    seat = phases[i][1]
                    self.assertEqual(phases[i:i + 3], [(op, seat) for op in ('untap_step', 'upkeep_step', 'draw_step')])
