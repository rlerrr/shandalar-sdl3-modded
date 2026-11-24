"""Guard exact records, actual phase receipts, and replay boundaries."""

import unittest

import cumulative_upkeep_nonmana_payment_recipes as model


class NonmanaUpkeepGuard(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        pins = model.profiles()
        entries = pins['sources'] | pins['witnesses']
        cls.oracle = {name: pin['oracle'] for name, pin in entries.items()}
        cls.rows = {name: pin['registry'] for name, pin in entries.items()}
        cls.names = {row['csvid']: name for name, row in cls.rows.items()}

    def case(self, csvid, mode, actor=0, replay=False):
        name = self.names[csvid]
        cost = model.classify(self.oracle[name], csvid)['casting_cost']
        duel = model.Duel(self.rows[name], actor, mode, self.rows)
        duel.build(cost, replay=replay)
        return duel

    def test_whole_records_and_typed_profile_fail_closed(self):
        model.validate_witnesses(self.oracle, self.rows)
        for csvid in model.CARDS:
            record = self.oracle[self.names[csvid]]
            shape = model.classify(record, csvid)
            declaration = dict(family=model.FAMILY, version=2, params=shape,
                               oracle_text=record['oracle_text'], variants=model.variants(shape))
            self.assertEqual(model.spec(declaration, record)['csvid'], csvid)
            with self.assertRaises(ValueError):
                model.spec(declaration, {**record, 'oracle_text': record['oracle_text'] + '\nDraw a card.'})
            with self.assertRaises(ValueError):
                model.spec({**declaration, 'version': True}, record)

    def test_land_entry_damage_and_attack_are_real(self):
        case = self.case(1949, 'decline_releases')
        self.assertEqual([step['ref'] for step in case.steps if step['op'] == 'play_land'], ['source'])
        self.assertEqual([step['ref'] for step in case.steps if step['op'] == 'cast'], ['bolt0', 'bolt1'])
        self.assertEqual([step['value'] for step in case.steps if step['op'] == 'attack'], [0, 1])
        self.assertEqual(case.life, [15, 18])
        self.assertEqual(case.cards['entry_land'][2], 'graveyard')

    def test_kraken_distinct_lands_and_insufficient_payment(self):
        pays = self.case(2067, 'pay_twice_trample')
        self.assertEqual([step['ref'] for step in pays.steps if step['op'] == 'choose_target'],
                         ['fodder0', 'fodder1', 'fodder2'])
        self.assertEqual([step['value'] for step in pays.steps if step['op'] == 'assert_card'
                          and step.get('property') == 'tapped' and step['ref'] == 'source'], [1, 0])
        self.assertEqual(pays.life[1], 11)
        short = self.case(2067, 'short_two', 1)
        self.assertEqual(short.cards['source'][2], 'graveyard')
        self.assertEqual(short.cards['enemy_land'][2], 'battlefield')

    def test_vortex_awards_cards_and_end_step_spends_only_own_zones(self):
        paid = self.case(3027, 'pay_twice_end_step')
        self.assertEqual([step['ref'] for step in paid.steps if step['op'] == 'choose_target'],
                         ['fodder0', 'fodder1', 'fodder2'])
        self.assertEqual([paid.cards[f'draw{i}'][2] for i in range(3)], ['graveyard'] * 3)
        self.assertEqual((paid.cards['enemy_hand'][2], paid.cards['enemy_land'][2]), ('hand', 'battlefield'))
        short = self.case(3027, 'short_two', 1)
        self.assertEqual([short.cards[f'draw{i}'][2] for i in range(2)], ['graveyard', 'library'])
        self.assertEqual(short.cards['source'][2], 'graveyard')

    def test_wall_uses_opponent_gain_and_actual_flying_block(self):
        paid = self.case(8469, 'pay_twice_defend')
        self.assertEqual(paid.life, [20, 23])
        self.assertEqual([(step['ref'], step['value']) for step in paid.steps if step['op'] == 'attack'],
                         [('source', 0), ('flyer', 1)])
        self.assertEqual([(step['ref'], step['other']) for step in paid.steps if step['op'] == 'block'],
                         [('source', 'flyer')])
        denied = self.case(8469, 'prevent_gain', 1)
        self.assertEqual(denied.life, [20, 20])
        self.assertEqual(denied.cards['source'][2], 'graveyard')

    def test_replay_preserves_untap_and_full_upkeep_actions(self):
        for csvid, mode in model.REPLAY.items():
            steps = self.case(csvid, mode, replay=True).steps
            capture, recorded, restore = (next(i for i, step in enumerate(steps) if step['op'] == op)
                                          for op in ('capture', 'record_state', 'restore'))
            self.assertEqual(steps[capture + 1:recorded], steps[restore + 1:-1])
            self.assertEqual([step['player'] for step in steps[restore + 1:] if step['op'] == 'upkeep_step'], [1, 0])
