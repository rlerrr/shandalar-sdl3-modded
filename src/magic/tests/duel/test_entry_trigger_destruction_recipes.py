"""Guard full Oracle identity and distinct destructive entry choices."""

from copy import deepcopy
import unittest

import entry_trigger_destruction_recipes as model


class EntryDestructionTests(unittest.TestCase):
    def setUp(self):
        pins = model.profiles()
        records = pins['sources'] | pins['witnesses']
        self.oracle = {name: item['oracle'] for name, item in records.items()}
        self.rows = {name: item['registry'] for name, item in records.items()}

    def selection(self, name, variant):
        row = self.rows[name]
        record = self.oracle[name]
        shape = model.classify(record, row['csvid'])
        declaration = dict(family=model.FAMILY, version=2, params=shape,
                           oracle_text=record['oracle_text'], variants=[variant])
        return row, record, shape, declaration

    def case(self, name, variant):
        row, _, shape, declaration = self.selection(name, variant)
        case, = model.expand(name, row['csvid'], declaration, shape, declaration['variants'], self.rows, self.oracle)
        return case

    def test_full_source_and_witness_record_guards(self):
        _, record, shape, declaration = self.selection('Absolver Thrull', 'haunt_death_0')
        self.assertEqual(model.spec(declaration, record), dict(shape, family=model.FAMILY))
        with self.assertRaises(ValueError):
            model.spec(declaration, dict(record, oracle_text=record['oracle_text'].splitlines()[1]))
        with self.assertRaises(ValueError):
            model.spec(dict(declaration, version=2.0), record)
        model.validate_witnesses(self.oracle, self.rows)
        bad = deepcopy(self.oracle)
        bad['Tropical Island']['type_line'] = 'Basic Land — Forest Island'
        with self.assertRaises(ValueError):
            model.validate_witnesses(bad, self.rows)

    def test_tribute_is_opponents_choice_and_counters_replace_destroy(self):
        pay = self.case('Nessian Demolok', 'paid_1')['steps']
        leave = self.case('Nessian Demolok', 'foe_artifact_1')['steps']
        self.assertIn(dict(op='choose_dialog', player=0, value=0), pay)
        self.assertIn(dict(op='assert_card', ref='source', property='plus1_counters', value=3), pay)
        self.assertNotIn('choose_target', [step['op'] for step in pay])
        self.assertIn(dict(op='choose_dialog', player=0, value=1), leave)
        self.assertIn(dict(op='choose_target', ref='victim', player=1), leave)

    def test_exit_lifecycles_use_actual_paid_actions(self):
        haunt = self.case('Absolver Thrull', 'haunt_death_1')['steps']
        self.assertIn(dict(op='cast', ref='kill_source', value=1), haunt)
        self.assertIn(dict(op='assert_zone_multiplicity', player=1, zone='exile', index=model.THRULL, value=1), haunt)
        self.assertIn(dict(op='cast', ref='exit_carrier', value=1), haunt)
        titan = self.case('Sundering Titan', 'leaves_bounce_0')['steps']
        spell = titan.index(dict(op='cast', ref='exit_spell', value=1))
        self.assertEqual([step['ref'] for step in titan[:spell] if step['op'] == 'play_land'], ['later_land', 'foe_land'])
        self.assertEqual([step['ref'] for step in titan[spell:] if step['op'] == 'choose_target'], ['foe_land', 'later_land'])
        self.assertIn(dict(op='assert_zone_multiplicity', player=0, zone='hand', index=model.TITAN, value=1), titan[spell:])

    def test_nonbasic_and_total_power_threshold_stay_explicit(self):
        wurm = self.case('Harvest Wurm', 'nonbasic_only_0')['steps']
        self.assertIn(dict(op='assert_offered_legal_card', index=self.rows[model.DUAL]['csvid'], value=0), wurm)
        self.assertIn(dict(op='assert_card', ref='source', property='in_play', value=0), wurm)
        for mode, total in (('exact_twelve_0', 12), ('below_twelve_0', 11)):
            case = self.case('Phyrexian Dreadnought', mode)
            chosen = [card['name'] for card in case['setup'] if card['ref'] in ('first', 'second')]
            self.assertEqual(sum(int(self.oracle[name]['power']) for name in chosen), total)
            self.assertIn(dict(op='assert_card', ref='source', property='in_play', value=int(total >= 12)), case['steps'])

    def test_auto_sacrifice_keeps_the_paid_indestructibility_check(self):
        case = self.case('Harvester Troll', 'indestructible_1')
        steps = case['steps']
        source = steps.index(dict(op='cast', ref='source', value=1))
        shield = steps.index(dict(op='cast', ref='shield', value=1))
        destruction = steps.index(dict(op='cast', ref='destroy_probe', value=1))
        self.assertLess(shield, destruction)
        self.assertLess(destruction, source)
        self.assertIn(dict(op='assert_card', ref='fodder', property='in_play', value=1), steps[destruction:source])
        self.assertIn(dict(op='assert_card', ref='fodder', property='in_play', value=0), steps[source:])
        self.assertIn(dict(op='assert_card', ref='source', property='plus1_counters', value=2), steps[source:])
        self.assertIn(model.SQUEE, {item['name'] for item in case['setup']})
