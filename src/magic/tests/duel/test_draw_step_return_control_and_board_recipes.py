"""Guard Sarkhan's paid loyalty branches and production emblem phase order."""

import unittest

import draw_step_return_control_and_board_recipes as model


class DrawStepBoardGuard(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        pins = model.profiles()
        merged = pins['sources'] | pins['witnesses']
        cls.oracle = {name: data['oracle'] for name, data in merged.items()}
        cls.rows = {name: data['registry'] for name, data in merged.items()}
        cls.source = cls.oracle['Sarkhan, the Dragonspeaker']

    def build(self, actor, mode):
        game = model.Duel(model.SARKHAN, self.source, actor, mode, self.rows, self.oracle)
        game.build()
        return game

    def test_requires_full_sarkhan_rules(self):
        shape = model.classify(self.source, model.SARKHAN)
        declaration = dict(family=model.FAMILY, version=2, params=shape,
                           oracle_text=self.source['oracle_text'], variants=['emblem_0'])
        self.assertEqual(model.spec(declaration, self.source)['cost'], [3, 0, 0, 0, 2, 0])
        with self.assertRaises(ValueError):
            model.spec(declaration, {**self.source, 'oracle_text': self.source['oracle_text'].rsplit('\n', 1)[0]})
        with self.assertRaises(ValueError):
            model.spec({**declaration, 'variants': ['cost_scope_0']}, self.source)

    def test_emblem_is_paid_at_six_and_executes_two_complete_draw_discard_cycles(self):
        for actor in (0, 1):
            game = self.build(actor, 'emblem')
            steps = game.steps
            choices = [s['value'] for s in steps if s['op'] == 'choose_dialog']
            self.assertEqual(choices, [0, 0, 2])
            self.assertEqual(game.loyalty, 0)
            self.assertEqual(game.cards['source']['zone'], 'graveyard')
            self.assertEqual(game.drawn[actor], 8)
            self.assertEqual(game.hand_size(actor), 0)
            self.assertEqual(game.drawn[1-actor], 5)
            self.assertEqual(game.hand_size(1-actor), 5)
            visits = [s['op'] for s in steps if s['op'] in ('advance_turn', 'untap_step', 'upkeep_step', 'draw_step')]
            self.assertEqual(visits, ['advance_turn', 'untap_step', 'upkeep_step', 'draw_step'] * 9)

    def test_animation_probes_real_damage_combat_subtype_and_destruction(self):
        for actor in (0, 1):
            game = self.build(actor, 'animation')
            self.assertEqual([s['ref'] for s in game.steps if s['op'] == 'cast'],
                             ['source', 'strike', 'dragon_lord', 'judgment'])
            self.assertEqual([s['value'] for s in game.steps if s['op'] == 'can_target_creature'], [0, 1, 0])
            self.assertTrue(any(s['op'] == 'attack' and s['value'] == 1 for s in game.steps))
            self.assertTrue(any(s['op'] == 'block' and s['value'] == 0 for s in game.steps))
            self.assertEqual(game.life[1-actor], 16)
            self.assertEqual(game.loyalty, 5)
            self.assertEqual(game.cards['source']['zone'], 'battlefield')
            self.assertEqual(game.cards['ground_blocker']['zone'], 'graveyard')

    def test_actual_minus_three_pays_loyalty_before_resolving_effect(self):
        for actor in (0, 1):
            for mode in ('damage_marked', 'damage_lethal'):
                game = self.build(actor, mode)
                start = next(i for i,s in enumerate(game.steps) if s['op'] == 'activate')
                finish = next(i for i,s in enumerate(game.steps[start:], start) if s['op'] == 'resolve')
                self.assertIn(dict(op='assert_counter', ref='source', counter_kind='loyalty', value=1),
                              game.steps[start:finish])
                self.assertEqual(game.cards['loyalty_target']['zone'],
                                 'graveyard' if mode == 'damage_lethal' else 'battlefield')


if __name__ == '__main__':
    unittest.main()
