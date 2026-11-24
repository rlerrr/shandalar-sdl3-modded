"""Guard the alternate payment and linked departure receipts."""

import unittest

import departure_damage_and_removal_recipes as model


class DepartureDamageTests(unittest.TestCase):
    def setUp(self):
        pins = model.profiles()
        cards = pins['sources'] | pins['witnesses']
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

    def test_full_oracle_and_actual_shield_witness(self):
        record = self.oracle[self.names[model.ABU]]
        with self.assertRaises(ValueError):
            model.classify(dict(record, oracle_text=record['oracle_text'].split(' They')[0]), model.ABU)
        case = self.case(model.ABU, 'regeneration_block_1')
        self.assertIn(dict(ref='shield', player=0, name='Wrap in Vigor', zone='hand'), case['setup'])
        self.assertIn(dict(op='cast', ref='shield', value=1), case['steps'])
        self.assertIn(dict(op='block', ref='source', other='hit', value=1), case['steps'])

    def test_membrane_and_spitebellows_use_typed_cast_and_real_death_choices(self):
        pay = self.case(model.MEMBRANE, 'life_1')['steps']
        self.assertIn(dict(op='choose_number', player=1, value=1), pay)
        self.assertIn(dict(op='assert_life', player=1, value=18), pay)
        evoke = self.case(model.SPITE, 'evoke_0')['steps']
        cast = evoke.index(dict(op='cast', ref='source', value=1))
        self.assertIn(dict(op='set_mana', player=0, color=4, value=2), evoke[:cast])
        self.assertIn(dict(op='assert_card', ref='victim', property='damage', value=6), evoke[cast:])

    def test_same_name_death_chain_and_unrelated_damage_are_distinct(self):
        chained = self.case(model.EFFIGY, 'chain_1')['steps']
        self.assertIn(dict(op='choose_target', ref='source', player=1), chained)
        self.assertIn(dict(op='assert_card', ref='recipient', property='damage', value=6), chained)
        unrelated = self.case(model.EFFIGY, 'unrelated_damage_0')['steps']
        self.assertIn(dict(op='cast', ref='ping', value=1), unrelated)
        self.assertIn(dict(op='assert_card', ref='recipient', property='damage', value=3), unrelated)

    def test_neutralize_real_counters_and_bound_two_player_modes(self):
        reset = self.case(model.TYRANT, 'zero_counters_1')['steps']
        self.assertIn(dict(op='activate', ref='hex', value=1), reset)
        self.assertIn(dict(op='assert_card', ref='victim', property='minus1_counters', value=0), reset)
        modes = self.case(model.LICH, 'discard_1')['steps']
        self.assertEqual(sum(step['op'] == 'choose_zone_card' for step in modes), 2)
        self.assertNotIn(dict(op='assert_life', player=0, value=15), modes)

    def test_ransom_and_upkeep_use_ordinary_phase_boundaries(self):
        accepted = self.case(model.ALBATROSS, 'paid_ransom_0')['steps']
        self.assertIn(dict(op='choose_dialog', player=1, value=1), accepted)
        self.assertIn(dict(op='assert_life', player=1, value=18), accepted)
        upkeep = self.case(model.KEG, 'own_upkeep_1')['steps']
        self.assertIn(dict(op='upkeep_step', player=1), upkeep)
        self.assertIn(dict(op='assert_life', player=0, value=17), upkeep)

    def test_forced_blocker_and_planeswalker_receipts_use_real_choice_boundaries(self):
        combat = self.case(model.CATOBLEPAS, 'paid_block_0')['steps']
        begin = combat.index(dict(op='begin_blockers'))
        self.assertEqual(combat[begin - 1], dict(op='choose_target', ref='blocker', player=1))
        self.assertIn(dict(op='assert_card', ref='blocker', property='blocking_count', value=1), combat[begin:])
        self.assertNotIn(dict(op='block', ref='blocker', other='source', value=1), combat[begin:])
        probe = self.case(model.IMPLEMENT, 'walker_1')['steps']
        self.assertIn(dict(op='probe_activation_target', ref='source', other='walker', index=-1, value=1), probe)
        self.assertNotIn(dict(op='choose_target', ref='walker', player=1), probe)
        self.assertIn(dict(op='assert_mana', player=1, color=4, value=1), probe)
