"""Whole-card, real tap boundary, and combat guards for becomes-tapped triggers."""
import json
import unittest
from pathlib import Path

import becomes_tapped_trigger_recipes as model
from generate_cases import OPS, PROPERTIES, registry


class BecomesTappedTriggerTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.oracle = json.loads((Path(__file__).parent / 'data/oracle.json').read_text(encoding='utf8'))['cards']
        cls.rows = {row['name']: row for row in registry()}

    def declaration(self, name):
        record = self.oracle[name]
        return dict(family=model.FAMILY, version=2, oracle_text=record['oracle_text'],
                    params=dict(schema=1, mechanism=model.mechanism(record)))

    def case(self, name, mode, owner=0):
        declaration = self.declaration(name)
        profile = model.spec(declaration, self.oracle[name])
        return model.expand(name, profile['csvid'], declaration, profile, [f'{mode}_{owner}'], self.rows, self.oracle)[0]

    def test_complete_independent_oracle_cards_and_costs(self):
        expected = {
            'Fallowsage': ('draw', None, [3, 0, 1, 0, 0, 0]),
            'Vampire Envoy': ('gain', 'Flying', [2, 1, 0, 0, 0, 0]),
            'Night Market Lookout': ('drain', None, [0, 1, 0, 0, 0, 0]),
            'Spireside Infiltrator': ('damage', None, [2, 0, 0, 0, 1, 0]),
            'Reckless Racer': ('loot', 'First strike', [2, 0, 0, 0, 1, 0]),
            'Renegade Wheelsmith': ('block', None, [1, 0, 0, 0, 1, 1]),
            'Surgespanner': ('paid_bounce', None, [2, 0, 2, 0, 0, 0]),
        }
        self.assertEqual(set(model.pins()['sources']), set(expected))
        model.validate_witnesses(self.oracle, self.rows)
        for name, values in expected.items():
            profile = model.spec(self.declaration(name), self.oracle[name])
            self.assertEqual(tuple(profile[k] for k in ('effect', 'keyword', 'mana')), values)
            with self.assertRaises(ValueError):
                model.mechanism(dict(self.oracle[name], oracle_text=self.oracle[name]['oracle_text'] + '\nDraw a card.'))

    def test_tap_receipts_distinguish_other_repeat_and_new_transition(self):
        for owner in (0, 1):
            for mode, gain in [('entry', 0), ('other_taps', 0), ('spell_tap', 1), ('already_tapped', 1), ('untap_retap', 2), ('opponent_turn', 1), ('fresh_attack', 0)]:
                steps = self.case('Vampire Envoy', mode, owner)['steps']
                life = [s['value'] for s in steps if s['op'] == 'assert_life' and s['player'] == owner]
                self.assertEqual(life[-1], 20 + gain, (mode, owner))
            for name in ('Fallowsage', 'Reckless Racer'):
                counts = {mode: len([s for s in self.case(name, mode, owner)['steps'] if s['op'] == 'choose_dialog'])
                          for mode in ('other_taps', 'already_tapped', 'untap_retap')}
                self.assertEqual(counts, {'other_taps': 1, 'already_tapped': 3, 'untap_retap': 5})

    def test_optional_declines_and_controller_correct_loot(self):
        for owner in (0, 1):
            draw = self.case('Fallowsage', 'decline', owner)['steps']
            self.assertIn(dict(op='choose_dialog', player=owner, value=1), draw)
            self.assertEqual([s['value'] for s in draw if s['op'] == 'assert_zone' and s['player'] == owner and s['zone'] == 'library'][-1], 3)
            loot = self.case('Reckless Racer', 'spell_tap', owner)
            self.assertEqual([c['name'] for c in loot['setup'] if c['player'] == owner and c['zone'] == 'hand'], ['Reckless Racer', 'Plains'])
            steps = loot['steps']
            self.assertIn(dict(op='assert_zone_multiplicity', player=owner, zone='graveyard', index=self.rows['Plains']['csvid'], value=1), steps)
            self.assertIn(dict(op='assert_zone_multiplicity', player=owner, zone='hand', index=self.rows['Mountain']['csvid'], value=1), steps)
            self.assertEqual([s['value'] for s in steps if s['op'] == 'assert_discard_choices'][-1], int(owner == 0))

    def test_real_earthcraft_cost_taps_fresh_creature_before_basic_untap(self):
        for owner in (0, 1):
            steps = self.case('Renegade Wheelsmith', 'cost_tap', owner)['steps']
            activate = next(i for i, s in enumerate(steps) if s['op'] == 'activate')
            self.assertNotIn('advance_turn', [s['op'] for s in steps])
            self.assertEqual(steps[activate-3:activate], [dict(op='choose_target', player=owner, ref=ref) for ref in ('source', 'land', 'blocker')])
            self.assertIn(dict(op='assert_card', ref='land', property='tapped', value=1), steps[activate:activate+5])
            self.assertIn(dict(op='assert_card', ref='land', property='tapped', value=0), steps[activate+5:])

    def test_actual_prevention_distinguishes_damage_life_loss_and_combat(self):
        for owner in (0, 1):
            for name in ('Night Market Lookout', 'Spireside Infiltrator'):
                all_damage = self.case(name, 'prevent_all_damage', owner)['steps']
                combat_damage = self.case(name, 'prevent_combat_damage', owner)['steps']
                self.assertIn(dict(op='cast', ref='shield', value=1), all_damage)
                self.assertIn(dict(op='cast', ref='shield', value=1), combat_damage)
                expected = 19 if name == 'Night Market Lookout' else 20
                self.assertEqual([s['value'] for s in all_damage if s['op'] == 'assert_life' and s['player'] == 1-owner][-1], expected)
                self.assertEqual([s['value'] for s in combat_damage if s['op'] == 'assert_life' and s['player'] == 1-owner][-1], 19)

    def test_actual_flying_first_strike_and_expiring_restriction(self):
        for owner in (0, 1):
            for mode, legal in [('ground_block_denied', 0), ('reach_block', 1)]:
                steps = self.case('Vampire Envoy', mode, owner)['steps']
                self.assertIn(dict(op='block', ref='blocker', other='source', value=legal), steps)
            first = self.case('Reckless Racer', 'first_strike', owner)['steps']
            self.assertIn(dict(op='assert_zone_multiplicity', player=1-owner, zone='graveyard', index=self.rows['Grizzly Bears']['csvid'], value=1), first)
            self.assertIn(dict(op='assert_card', ref='source', property='damage', value=0), first[first.index(dict(op='combat_damage')):])
            expiry = self.case('Renegade Wheelsmith', 'block_expires', owner)['steps']
            self.assertEqual([(s['other'], s['value']) for s in expiry if s['op'] == 'block'], [('source', 0), ('ally', 1)])

    def test_generated_cases_keep_complete_clauses_and_exact_replay(self):
        count = 0
        for name in model.pins()['sources']:
            decl = self.declaration(name)
            profile = model.spec(decl, self.oracle[name])
            for case in model.expand(name, profile['csvid'], decl, profile, model.variants(profile), self.rows, self.oracle):
                count += 1
                self.assertEqual(case['_recipe']['covered_clauses'], self.oracle[name]['oracle_text'].splitlines())
                steps = case['steps']
                for step in steps:
                    self.assertIn(step['op'], OPS)
                    if 'property' in step:
                        self.assertIn(step['property'], PROPERTIES)
                ops = [step['op'] for step in steps]
                if 'replay' in case['name']:
                    self.assertEqual(steps[ops.index('capture')+1:ops.index('record_state')], steps[ops.index('restore')+1:-1])
                if 'countered' in case['name']:
                    self.assertEqual(ops[ops.index('cast_response')-1], 'announce_spell')
                    self.assertFalse(any(op.startswith('choose_') for op in ops))
        self.assertEqual(count, 203)

    def test_paid_trigger_uses_real_cost_before_the_native_target_prompt(self):
        declaration = self.declaration('Surgespanner')
        profile = model.spec(declaration, self.oracle['Surgespanner'])
        self.assertEqual(profile['trigger_mana'], [1, 0, 1, 0, 0, 0])
        self.assertIn('decline_0', model.variants(profile))
        self.assertNotIn('decline_1', model.variants(profile))
        for owner in (0, 1):
            case = self.case('Surgespanner', 'spell_tap', owner)
            refs = [c['ref'] for c in case['setup'] if c['ref'].startswith('trigger_land_')]
            self.assertEqual([(c['name'], c['player']) for c in case['setup'] if c['ref'] in refs],
                             [('Wastes', owner), ('Island', owner)])
            choices = [s for s in case['steps'] if s['op'].startswith('choose_')]
            human_payment = [dict(op='choose_mana_source', ref=ref) for ref in refs] if owner == 0 else []
            self.assertEqual(choices, [dict(op='choose_target', ref='source', player=owner),
                                       dict(op='choose_dialog', player=owner, value=0), *human_payment,
                                       dict(op='choose_target', ref='bounce_target', player=owner)])
            for ref in refs:
                taps = [s['value'] for s in case['steps'] if s.get('ref') == ref and s.get('property') == 'tapped']
                self.assertEqual(taps, [0, 1])
            mana = [s['value'] for s in case['steps'] if s['op'] == 'assert_mana' and s['player'] == owner]
            self.assertEqual(mana[-6:], [0] * 6)

    def test_declined_and_unaffordable_payment_leave_target_in_place(self):
        cells = [('decline', 0, []), ('insufficient_mana', 0, [2]), ('insufficient_mana', 1, [2]),
                 ('wrong_color', 0, [4, 4]), ('wrong_color', 1, [4, 4])]
        for mode, owner, produced in cells:
            steps = self.case('Surgespanner', mode, owner)['steps']
            self.assertFalse(any(s.get('ref') == 'bounce_target' and s['op'] == 'choose_target' for s in steps))
            final_battlefield = [s['value'] for s in steps if s['op'] == 'assert_zone_multiplicity' and
                                 s['player'] == 1-owner and s['zone'] == 'battlefield' and
                                 s['index'] == self.rows['Horned Turtle']['csvid']]
            self.assertEqual(final_battlefield[-1], 1)
            self.assertEqual(sum(s['op'] == 'choose_cancel_mana' for s in steps), int(owner == 0))
            self.assertEqual(sum(s['op'] == 'activate' for s in steps), len(produced))
            mana = [s['value'] for s in steps if s['op'] == 'assert_mana' and s['player'] == owner]
            self.assertEqual(mana[-6:], [produced.count(color) for color in range(6)])
            if mode == 'decline':
                for i in range(2):
                    taps = [s['value'] for s in steps if s.get('ref') == f'trigger_land_{i}' and s.get('property') == 'tapped']
                    self.assertEqual(taps, [0, 0])

    def test_paid_bounce_targets_permanent_types_and_returns_to_owner(self):
        for owner in (0, 1):
            for mode, name, destination in [('spell_tap', 'Horned Turtle', 1-owner),
                                            ('bounce_land', 'Forest', owner),
                                            ('bounce_artifact', 'Darksteel Relic', 1-owner),
                                            ('bounce_enchantment', 'Earthcraft', owner),
                                            ('bounce_source', 'Surgespanner', owner),
                                            ('owned_by_other', 'Horned Turtle', owner)]:
                case = self.case('Surgespanner', mode, owner)
                target = 'source' if mode == 'bounce_source' else 'bounce_target'
                card = next(c for c in case['setup'] if c['ref'] == target)
                self.assertEqual(card['name'], name)
                if mode == 'owned_by_other':
                    self.assertEqual(card['player'], 1-owner)
                    self.assertIn('opponent_owned', card['state'])
                if mode == 'bounce_land':
                    self.assertEqual(card['player'], owner)
                    self.assertEqual(card.get('state', []), ['tapped'] if owner == 1 else [])
                values = [s['value'] for s in case['steps'] if s['op'] == 'assert_zone_multiplicity' and
                          s['player'] == destination and s['zone'] == 'hand' and s['index'] == self.rows[name]['csvid']]
                self.assertEqual(values[-1], 1)
            repeat = self.case('Surgespanner', 'untap_retap', owner)['steps']
            self.assertEqual([s['ref'] for s in repeat if s['op'] == 'choose_target' and s['ref'] != 'source'],
                             ['bounce_target', 'retap_target'])
            self.assertEqual([s['player'] for s in repeat if s['op'] == 'untap_step'], [1-owner, owner])
