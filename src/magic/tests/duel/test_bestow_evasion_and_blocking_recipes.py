"""Check that the added Reach clauses use real casts and declarations."""

import unittest

import bestow_evasion_and_blocking_recipes as model


class BestowReachGuard(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        pins = model.profiles()
        cls.source = pins['sources']['Leafcrown Dryad']
        merged = pins['sources'] | pins['witnesses']
        cls.oracle = {name: pin['oracle'] for name, pin in merged.items()}
        cls.rows = {name: pin['registry'] for name, pin in merged.items()}
        cls.shape = model.classify(cls.source['oracle'], model.DRYAD)

    def game(self, owner, mode):
        duel = model.Duel('Leafcrown Dryad', owner, mode, self.shape,
                          self.rows, self.oracle, self.source['engine_aura_face']['csvid'])
        duel.build()
        return duel

    def test_requires_complete_pinned_intrinsic_and_granted_reach(self):
        record = self.source['oracle']
        decl = dict(family=model.FAMILY, version=2, params=self.shape, oracle_text=record['oracle_text'],
                    variants=['regular_0', 'bestowed_1'])
        self.assertEqual(model.spec(decl, record)['bestowed'], [3, 0, 0, 1, 0, 0])
        with self.assertRaises(ValueError):
            model.spec(decl, {**record, 'oracle_text': record['oracle_text'].removesuffix(' and has reach.')})
        with self.assertRaises(ValueError):
            model.spec({**decl, 'variants': ['regular_pair_0']}, record)

    def test_intrinsic_and_granted_reach_block_a_real_flyer_on_both_seats(self):
        for owner in (0, 1):
            for mode, blocker in (('regular', 'source'), ('bestowed', 'host')):
                game = self.game(owner, mode)
                self.assertIn(dict(op='attack', ref='swing', value=1), game.steps)
                self.assertIn(dict(op='block', ref=blocker, other='swing', value=1), game.steps)
                self.assertEqual(game.life, [20, 20])
                self.assertEqual(game.objects['swing']['zone'], 'graveyard')
                self.assertEqual(game.objects[blocker]['zone'], 'graveyard' if mode == 'regular' else 'battlefield')

    def test_actual_aura_removal_clears_reach_and_pending_loss_restores_intrinsic(self):
        for owner in (0, 1):
            removed = self.game(owner, 'removed_aura')
            self.assertIn(dict(op='choose_target', player=1-owner, ref='source'), removed.steps)
            self.assertIn(dict(op='block', ref='host', other='swing', value=0), removed.steps)
            self.assertEqual(removed.life[owner], 18)
            self.assertEqual(removed.objects['source']['zone'], 'graveyard')
            fallback = self.game(owner, 'pending_fallback')
            self.assertEqual([s['op'] for s in fallback.steps if s['op'] in
                              ('announce_spell', 'cast_response', 'resolve_response')],
                             ['announce_spell', 'cast_response', 'resolve_response'])
            self.assertEqual(fallback.objects['host']['zone'], 'hand')
            self.assertIn(dict(op='block', ref='source', other='swing', value=1), fallback.steps)


if __name__ == '__main__':
    unittest.main()
