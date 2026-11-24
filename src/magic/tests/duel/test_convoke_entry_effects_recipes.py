"""Guard exact Convoke payment order and source-specific entry expectations."""

import unittest

import convoke_entry_effects_recipes as model


class ConvokeEntryGuard(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        pins = model.profiles()
        merged = pins['sources'] | pins['witnesses']
        cls.oracle = {name: pin['oracle'] for name, pin in merged.items()}
        cls.rows = {name: pin['registry'] for name, pin in merged.items()}
        cls.sources = {pin['registry']['csvid']: pin['oracle'] for pin in pins['sources'].values()}

    def scenario(self, csvid, mode, seat):
        game = model.Duel(csvid, self.sources[csvid], seat, mode, self.rows, self.oracle)
        game.build()
        return game

    def test_complete_entry_rule_and_exact_declaration_are_required(self):
        record = self.sources[model.TOTEM]
        shape = model.classify(record, model.TOTEM)
        decl = dict(family=model.FAMILY, version=2, params=shape, oracle_text=record['oracle_text'],
                    variants=['mixed_0', 'foe_target_1'])
        self.assertEqual(model.spec(decl, record)['mana'], [3, 0, 0, 1, 0, 0])
        with self.assertRaises(ValueError):
            model.spec(decl, dict(record, oracle_text=record['oracle_text'].replace('another target', 'target')))
        with self.assertRaises(ValueError):
            model.spec(dict(decl, version=2.0), record)
        with self.assertRaises(ValueError):
            model.spec(dict(decl, variants=['mixed_0', 'mixed_0']), record)

    def test_real_convoke_decisions_precede_cast_only_for_seat_zero(self):
        for seat in (0, 1):
            game = self.scenario(model.LOXODON, 'full', seat)
            cast = game.steps.index(dict(op='cast', ref='source', value=1))
            plans = [s['ref'] for s in game.steps[:cast] if s['op'] == 'choose_target']
            self.assertEqual(plans, game.sponsors if seat == 0 else [])
            ready = [c['ref'] for c in game.setup if c['player'] == seat and c.get('state') == [model.SICK]]
            self.assertEqual(ready, game.sponsors + ['spare'])
            self.assertEqual(game.pool, [0] * 6)
            self.assertGreaterEqual(sum(c['player'] == seat and c['zone'] in ('hand', 'battlefield')
                                        for c in game.setup), 18)

    def test_loxodon_only_counts_actual_convokers_after_entry(self):
        for seat in (0, 1):
            for mode in ('full', 'mixed', 'normal', 'countered'):
                game = self.scenario(model.LOXODON, mode, seat)
                self.assertEqual(game.counters, {} if mode == 'countered' else dict.fromkeys(game.sponsors, 1))
                self.assertNotIn('spent', game.counters)
                self.assertNotIn('spare', game.counters)
                self.assertEqual(game.cards['source']['zone'], 'graveyard' if mode == 'countered' else 'battlefield')

    def test_phalanx_counts_controlled_creatures_only_after_entry(self):
        for seat in (0, 1):
            for mode, expected in (('mixed', 24), ('alone', 21), ('countered', 20)):
                game = self.scenario(model.PHALANX, mode, seat)
                self.assertEqual(game.life[seat], expected)
                self.assertEqual(game.life[1 - seat], 20)
                self.assertFalse(any(s['op'] == 'set_life' for s in game.steps))

    def test_totem_accepts_another_creature_on_either_side_and_can_decline(self):
        for seat in (0, 1):
            for mode, counters in (('mixed', {'payer_0': 1}), ('foe_target', {'opponent_creature': 1}),
                                   ('decline', {}), ('alone', {})):
                game = self.scenario(model.TOTEM, mode, seat)
                self.assertEqual(game.counters, counters)
                self.assertNotIn('source', game.counters)

    def test_tribunal_leaves_land_and_returns_artifact_after_actual_removal(self):
        for seat in (0, 1):
            only_land = self.scenario(model.TRIBUNAL, 'only_land', seat)
            self.assertEqual(only_land.cards['opponent_land']['zone'], 'battlefield')
            returned = self.scenario(model.TRIBUNAL, 'artifact_return', seat)
            entry = returned.steps.index(dict(op='resolve'))
            removal = returned.steps.index(dict(op='cast', ref='removal', value=1))
            cardinal = returned.steps.index(dict(op='bind_permanent_copy', ref='returned', player=1 - seat,
                                                  value=self.rows[model.GEAR]['csvid'], index=0))
            self.assertLess(entry, removal)
            self.assertLess(removal, cardinal)
            self.assertEqual(returned.cards['victim']['zone'], 'battlefield')
