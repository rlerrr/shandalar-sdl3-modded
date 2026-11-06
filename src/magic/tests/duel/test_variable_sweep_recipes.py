"""Keep X sweeps bound to complete Oracle effects and real payment boundaries."""
import copy
import json
import unittest
from pathlib import Path

from generate_cases import registry
from variable_sweep_recipes import expand, spec, variants, validate_witnesses


class VariableSweepRecipesTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.oracle = json.loads((Path(__file__).parent / 'data/oracle.json').read_text(encoding='utf8'))['cards']
        cls.registry = {row['name']: row for row in registry()}

    def declaration(self, name='Earthquake', selected=None):
        flying = 'without' if name in ('Earthquake', 'Fault Line') else 'all' if name == 'Savage Twister' else 'with'
        return dict(family='variable_sweep', version=2, params=dict(flying=flying, players=name not in ('Savage Twister', 'Windstorm')),
                    oracle_text=self.oracle[name]['oracle_text'], variants=selected or ['mixed_x3_0'])

    def cases(self, name='Earthquake', selected=None, declaration=None, oracle=None, rows=None):
        d = declaration or self.declaration(name, selected)
        p = spec(d, self.oracle[name])
        context = oracle or self.oracle
        rows = rows or self.registry
        validate_witnesses(context, rows)
        return expand(name, rows[name]['csvid'], d, p, d['variants'], rows, context)

    def test_complete_scope_fails_closed(self):
        for patch in (dict(flying='with'), dict(players=False), dict(players=1), dict(extra=True), dict(flying='unknown')):
            d = self.declaration(); d['params'].update(patch)
            with self.assertRaises(ValueError): self.cases(declaration=d)
        d = self.declaration(); d['oracle_text'] += ' Draw a card.'
        with self.assertRaises(ValueError): self.cases(declaration=d)

    def test_extra_casting_mechanics_and_multiple_x_are_rejected(self):
        d = self.declaration()
        for patch in (dict(mana_cost='{X}{X}{R}'), dict(mana_cost='{X}{R/P}'), dict(keywords=['Kicker']),
                      dict(type_line='Creature'), dict(layout='transform'), dict(card_faces=[{}])):
            with self.assertRaises(ValueError): spec(d, dict(self.oracle['Earthquake'], **patch))

    def test_all_six_reviewed_cards_validate_independently(self):
        for name in ('Earthquake', 'Fault Line', 'Hurricane', 'Savage Twister', 'Windstorm', 'Squall Line'):
            p = spec(self.declaration(name), self.oracle[name])
            self.assertIn('payment_x0_0', variants(p))
            self.assertIn('mixed_x8_1', variants(p))
            self.assertEqual('opposing_turn_x3_0' in variants(p), self.oracle[name]['type_line'] == 'Instant')

    def test_zero_damage_preserves_board_and_still_pays_base_mana(self):
        case = self.cases(selected=['payment_x0_0'])[0]
        self.assertIn(dict(op='choose_x', player=0, value=0), case['steps'])
        self.assertIn(dict(op='set_mana', player=0, color=4, value=1), case['steps'])
        self.assertIn(dict(op='assert_mana_total', player=0, value=0), case['steps'])
        for setup in case['setup']:
            if setup['zone'] == 'battlefield':
                self.assertIn(dict(op='assert_card', ref=setup['ref'], property='in_play', value=1), case['steps'])

    def test_flying_damage_and_indestructible_mortality_are_separate(self):
        for name, dead, spared in (('Earthquake', 'Savannah Lions', 'Suntail Hawk'),
                                   ('Hurricane', 'Suntail Hawk', 'Savannah Lions')):
            c = self.cases(name)[0]
            refs = {s['name']: s['ref'] for s in c['setup'] if s.get('player') == 1 and s['zone'] == 'battlefield'}
            self.assertIn(dict(op='assert_card', ref=refs[dead], property='in_play', value=0), c['steps'])
            self.assertIn(dict(op='assert_card', ref=refs[spared], property='in_play', value=1), c['steps'])
            self.assertIn(dict(op='assert_card', ref=refs['Darksteel Myr'], property='in_play', value=1), c['steps'])

    def test_eight_damage_kills_large_eligible_survivor(self):
        c = self.cases('Savage Twister', ['mixed_x8_1'])[0]
        target = next(s['ref'] for s in c['setup'] if s['name'] == 'Scaled Wurm' and s['player'] == 0 and s['zone'] == 'battlefield')
        self.assertIn(dict(op='assert_card', ref=target, property='in_play', value=0), c['steps'])
        self.assertIn(dict(op='assert_life', player=0, value=20), c['steps'])

    def test_payment_includes_x_and_all_fixed_colors(self):
        steps = self.cases('Savage Twister', ['payment_x5_1'])[0]['steps']
        for color, value in ((0, 5), (3, 1), (4, 1)):
            self.assertIn(dict(op='set_mana', player=1, color=color, value=value), steps)
        self.assertIn(dict(op='assert_mana_total', player=1, value=0), steps)
        self.assertIn(dict(op='choose_x', player=1, value=5), steps)

    def test_failed_fixed_payment_does_not_queue_unconsumable_x(self):
        for v in ('insufficient_mana_x3_0', 'wrong_color_x3_1'):
            steps = self.cases(selected=[v])[0]['steps']
            self.assertFalse(any(s['op'] == 'choose_x' for s in steps))
            self.assertIn(dict(op='cast', ref='source', value=0), steps)
            self.assertFalse(any(s['op'] == 'resolve' for s in steps))

    def test_snapshot_boundaries_replay_only_required_payment_and_resolution(self):
        expected = {'ready_snapshot': (2, 2), 'pending_snapshot': (1, 1), 'resolved_snapshot': (1, 1)}
        for boundary, counts in expected.items():
            steps = self.cases(selected=[boundary + '_x5_1'])[0]['steps']
            self.assertEqual(sum(s['op'] == 'choose_x' for s in steps), counts[0])
            self.assertEqual(sum(s['op'] == 'cast' for s in steps), counts[1])
            for op in ('capture', 'restore', 'record_state', 'assert_recorded_state', 'ai_roundtrip'):
                self.assertTrue(any(s['op'] == op for s in steps))

    def test_countered_cast_retains_payment_and_source_destination_receipts(self):
        c = self.cases(selected=['countered_cast_x3_1'])[0]
        self.assertIn(dict(op='cast', ref='source', value=0), c['steps'])
        self.assertIn(dict(op='choose_priority', ref='counter'), c['steps'])
        self.assertIn(dict(op='assert_mana_total', player=0, value=56), c['steps'])
        self.assertIn(dict(op='assert_mana_total', player=1, value=56), c['steps'])
        self.assertIn(dict(op='assert_zone_contains', player=1, zone='graveyard', value=75), c['steps'])

    def test_source_and_witness_context_are_not_mutated_or_handler_derived(self):
        d = self.declaration(); before = copy.deepcopy(d)
        expected = self.cases(declaration=d)
        self.assertEqual(d, before)
        rows = copy.deepcopy(self.registry); rows['Earthquake']['handler'] = 'unrelated'
        self.assertEqual(self.cases(rows=rows), expected)
        oracle = copy.deepcopy(self.oracle); oracle['Suntail Hawk']['keywords'] = []
        with self.assertRaises(ValueError): self.cases(oracle=oracle)

    def test_shared_dispatch_preserves_family_and_rejects_incomplete_selection(self):
        from recipes import expand as dispatch
        d = self.declaration(selected=['pending_snapshot_x5_1'])
        row = self.registry['Earthquake']
        self.assertEqual(dispatch('Earthquake', row['csvid'], d, self.oracle['Earthquake'], self.registry,
                                  oracle_context=self.oracle), self.cases(declaration=d))
        for selected in ([], ['mixed_x20_0'], ['mixed_x3_0', 'mixed_x3_0']):
            with self.assertRaises(ValueError):
                dispatch('Earthquake', row['csvid'], dict(d, variants=selected), self.oracle['Earthquake'], self.registry,
                         oracle_context=self.oracle)


if __name__ == '__main__':
    unittest.main()
