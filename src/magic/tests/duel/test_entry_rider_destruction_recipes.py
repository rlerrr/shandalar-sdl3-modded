"""Guard the exact variants and effect boundaries for destructive entry riders."""

import unittest

import entry_rider_destruction_recipes as model


class DestructionRiderTests(unittest.TestCase):
    def setUp(self):
        pinned = model.profiles()
        self.names = {item['registry']['csvid']: name for name, item in pinned['sources'].items()}
        cards = pinned['sources'] | pinned['witnesses']
        self.oracle = {name: item['oracle'] for name, item in cards.items()}
        self.rows = {name: item['registry'] for name, item in cards.items()}

    def case(self, csvid, variant):
        name = self.names[csvid]
        record = self.oracle[name]
        shape = model.classify(record, csvid)
        declaration = dict(family=model.FAMILY, version=2, params=shape,
                           oracle_text=record['oracle_text'], variants=[variant])
        self.assertEqual(model.spec(declaration, record), dict(shape, family=model.FAMILY))
        case, = model.expand(name, csvid, declaration, shape, [variant], self.rows, self.oracle)
        return case

    def test_exact_source_and_adjacent_witness_pins(self):
        name = self.names[model.SHAMBLER]
        record = self.oracle[name]
        with self.assertRaises(ValueError):
            model.classify(dict(record, oracle_text=record['oracle_text'].splitlines()[0]), model.SHAMBLER)
        with self.assertRaises(ValueError):
            model.validate_witnesses(self.oracle, dict(self.rows, **{
                model.SNOW: dict(self.rows[model.SNOW], handler='card_dummy')}))

    def test_manticore_records_signed_power(self):
        steps = self.case(model.MANTICORE, 'own_walker_0')['steps']
        self.assertIn(dict(op='assert_card', ref='own', property='power', value=-1), steps)
        self.assertIn(dict(op='assert_card', ref='own', property='toughness', value=6), steps)
        self.assertIn(dict(op='choose_target', ref='walker', player=0), steps)

    def test_yeti_uses_snow_source_before_return_and_real_offturn(self):
        paid = self.case(model.YETI, 'snow_return_0')['steps']
        pay = paid.index(dict(op='activate', ref='source', value=1))
        self.assertIn(dict(op='choose_target', ref='snow', player=0), paid[:pay])
        self.assertIn(dict(op='assert_card', ref='snow', property='tapped', value=1), paid[pay:])
        self.assertIn(dict(op='bind_hand_copy', ref='returned', player=0, index=0, value=model.YETI), paid[pay:])
        offturn = self.case(model.YETI, 'offturn_1')['steps']
        self.assertIn(dict(op='can_activate', ref='source', value=0), offturn)
        self.assertEqual(sum(step['op'] == 'advance_turn' for step in offturn), 3)

    def test_bounce_leave_mark_and_echo_fee_are_independent(self):
        bounce = self.case(model.SHAMBLER, 'bounce_0')['steps']
        cast = bounce.index(dict(op='cast', ref='exit', value=1))
        self.assertIn(dict(op='assert_card', ref='ground', property='damage', value=1), bounce[:cast])
        self.assertIn(dict(op='assert_card', ref='ground', property='damage', value=2), bounce[cast:])
        self.assertNotIn(dict(op='assert_card', ref='air', property='damage', value=1), bounce)
        paid = self.case(model.SHAMBLER, 'echo_pay_1')
        land_names = [card['name'] for card in paid['setup'] if card['ref'].startswith('echo_land_')]
        self.assertEqual(land_names, ['Forest', 'Forest', 'Forest', 'Mountain'])
        echo = paid['steps'].index(dict(op='upkeep_step', player=1))
        self.assertIn(dict(op='assert_mana', color=4, player=1, value=0), paid['steps'][echo:])
        hellion = self.case(model.HELLION, 'echo_pay_0')
        self.assertEqual(sum(card['ref'].startswith('echo_land_') for card in hellion['setup']), 17)
        echo = len(hellion['steps']) - 1 - hellion['steps'][::-1].index(dict(op='upkeep_step', player=0))
        self.assertEqual(sum(step['op'] == 'choose_mana_source' for step in hellion['steps'][:echo]), 17)

    def test_paid_choices_use_actual_combat_or_zone_receipts(self):
        steps = self.case(model.TORNADO, 'ignore_blocker_1')['steps']
        block = steps.index(dict(op='block', ref='ground', other='source', value=1))
        self.assertIn(dict(op='choose_dialog', player=1, value=1), steps[block:])
        self.assertIn(dict(op='assert_life', player=0, value=14), steps[block:])
        steps = self.case(model.HECATOMB, 'swamp_0')['steps']
        self.assertEqual(sum(step['op'] == 'choose_target' and step.get('ref', '').startswith('fodder_')
                             for step in steps), 4)
        self.assertIn(dict(op='assert_card', ref='land', property='tapped', value=1), steps)
        steps = self.case(model.REVOLUTIONARY, 'indestructible_1')['steps']
        self.assertIn(dict(op='assert_card', ref='artifact', property='in_play', value=0), steps)
        self.assertIn(dict(op='assert_life', player=0, value=18), steps)
