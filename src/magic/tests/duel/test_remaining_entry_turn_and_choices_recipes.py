"""Check protected pilot semantics and the additional real choices."""

import unittest

import remaining_entry_turn_and_choices_recipes as model


class RemainingEntryChoiceTests(unittest.TestCase):
    def setUp(self):
        pins = model.profiles()
        cards = pins['sources'] | pins['witnesses'] | pins['tokens']
        self.oracle = {name: item['oracle'] for name, item in cards.items()}
        self.rows = {name: item['registry'] for name, item in cards.items()}
        self.names = {row['csvid']: name for name, row in self.rows.items()}

    def case(self, csvid, variant):
        name = self.names[csvid]
        record = self.oracle[name]
        shape = model.classify(record, csvid)
        declaration = dict(family=model.FAMILY, version=2, params=shape,
                           oracle_text=record['oracle_text'], variants=[variant])
        self.assertEqual(model.spec(declaration, record), dict(shape, family=model.FAMILY))
        case, = model.expand(name, csvid, declaration, shape, [variant], self.rows, self.oracle)
        return case

    def test_whole_oracle_and_pinned_witness_guard(self):
        name = self.names[model.CLOAK]
        record = self.oracle[name]
        with self.assertRaises(ValueError):
            model.classify(dict(record, oracle_text=record['oracle_text'].splitlines()[0]), model.CLOAK)
        with self.assertRaises(ValueError):
            model.validate_witnesses(self.oracle, dict(self.rows, **{model.TOKEN: dict(self.rows[model.TOKEN], csvid=0)}))

    def test_wumpus_generalizes_existing_wurm_without_change_to_picks(self):
        wurm = self.case(model.WURM, 'opponent_sacrifice_0')['steps']
        wumpus = self.case(model.WUMPUS, 'opponent_sacrifice_0')['steps']
        predicate = lambda step: step['op'] in ('choose_target', 'choose_dialog')
        self.assertEqual(list(filter(predicate, wurm)), list(filter(predicate, wumpus)))
        self.assertIn(dict(op='choose_dialog', player=1, value=0), wumpus)

    def test_squealing_devils_two_mana_windows_and_actual_expiry(self):
        case = self.case(model.DEVIL, 'paid_x_0')['steps']
        cast = case.index(dict(op='cast', ref='source', value=1))
        self.assertIn(dict(op='choose_mana', player=0, color=1), case[:cast])
        self.assertIn(dict(op='choose_x', player=0, value=2), case[cast:])
        self.assertIn(dict(op='assert_card', ref='recipient', property='power', value=2), case[cast:])
        self.assertIn(dict(op='assert_card', ref='recipient', property='power', value=0), case[cast:])

    def test_skips_and_seeded_upkeep_have_authoritative_source_cases(self):
        case = self.case(model.EATER, 'schedule_1')['steps']
        self.assertEqual([step['value'] for step in case if step['op'] == 'advance_scheduled_turn'], [0, 0, 0, 1])
        point = self.case(model.SPARK, 'point_match_0')
        first, second, third, _ = model.totals(point['seed'])
        self.assertEqual((second, third), (first, 7))
        self.assertEqual(sum(step['op'] == 'upkeep_step' and step['player'] == 0 for step in point['steps']), 2)
        self.assertNotIn('point_match_1', model.variants(dict(csvid=model.SPARK)))

    def test_paid_granted_costs_and_real_aura_upkeep(self):
        spell = self.case(model.THIRST, 'pay_0')['steps']
        self.assertIn(dict(op='choose_mana_source', ref='fee'), spell)
        self.assertIn(dict(op='assert_card', ref='fee', property='tapped', value=1), spell)
        trial = self.case(model.REBEL, 'own_host_1')['steps']
        self.assertIn(dict(op='activate_granted_legacy', ref='host', index=model.REBEL, value=1), trial)
        expiry = self.case(model.SPY, 'control_loss_0')['steps']
        self.assertIn(dict(op='can_activate_granted_legacy', ref='host', index=model.SPY, value=0), expiry)
        token = self.case(model.KONG, 'token_0')['steps']
        activate = token.index(dict(op='activate', ref='source', value=1))
        self.assertEqual([step['ref'] for step in token[:activate] if step['op'] == 'choose_target'], ['source', 'ape'])
        self.assertIn(dict(op='assert_matching_permanents', player=0, index=18689, property='subtype_Ape', value=1), token)
