"""Checks for live eligibility, mana alternatives, and exact draw receipts."""
import json
import unittest
from pathlib import Path

import gated_tap_draw_recipes as model
from generate_cases import OPS, registry


class GatedTapDrawTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.oracle = json.loads((Path(__file__).parent / 'data/oracle.json').read_text(encoding='utf8'))['cards']
        cls.rows = {row['name']: row for row in registry()}

    def spec(self, name):
        record = self.oracle[name]
        declaration = dict(family=model.FAMILY, version=2, oracle_text=record['oracle_text'],
                           params=dict(schema=1, mechanism=model.mechanism(record)))
        return declaration, model.spec(declaration, record)

    def case(self, name, mode, player=0):
        d, p = self.spec(name)
        return model.expand(name, p['csvid'], d, p, [f'{mode}_{player}'], self.rows, self.oracle)[0]

    def test_whole_clauses_and_exact_fee_shapes(self):
        expected = {'Library of Alexandria': (438, 'Land', 7, True, 0, 0, False),
                    'Magus of the Library': (1284, 'Creature', 7, True, 0, 0, False),
                    "Fool's Tome": (3152, 'Artifact', 0, False, 2, 0, False),
                    'Tapestry of the Ages': (15050, 'Artifact', None, False, 2, 0, False),
                    'Sea Gate Wreckage': (15674, 'Land', 0, True, 2, 1, False),
                    'Seeker of Insight': (17018, 'Creature', None, False, 0, 0, True)}
        self.assertEqual(set(model.pins()['sources']), set(expected))
        for name, values in expected.items():
            _, p = self.spec(name)
            self.assertEqual(tuple(p[k] for k in ('csvid', 'card_type', 'hand_size', 'mana_mode', 'generic', 'colorless', 'discard')), values)
            with self.assertRaises(ValueError):
                model.mechanism(dict(self.oracle[name], oracle_text=self.oracle[name]['oracle_text'] + '\nDraw a card.'))
        model.validate_witnesses(self.oracle, self.rows)

    def test_exact_seven_uses_live_count_on_both_sides_of_activation(self):
        name = 'Library of Alexandria'
        for owner in (0, 1):
            for mode, allowed in [('effect', 1), ('hand_below', 0), ('hand_above', 0), ('hand_leaves_limit', 0),
                                  ('opponent_hand', 0), ('hand_enters_limit', 1), ('pending_hand_changes', 1)]:
                steps = self.case(name, mode, owner)['steps']
                self.assertIn(dict(op='require_activation_mode', value=allowed), steps)
                zones = [s['value'] for s in steps if s['op'] == 'assert_zone' and s['zone'] == 'library' and s['player'] == owner]
                self.assertEqual(zones[-1], 3-allowed)
            pending = self.case(name, 'pending_hand_changes', owner)['steps']
            self.assertLess(pending.index(dict(op='activate', ref='source', value=1)), pending.index(dict(op='cast', ref='fog', value=1)))
            self.assertIn(dict(op='assert_stack', value=2), pending)

    def test_zero_hand_gate_can_open_from_a_real_cast(self):
        for owner in (0, 1):
            steps = self.case("Fool's Tome", 'hand_enters_limit', owner)['steps']
            self.assertLess(steps.index(dict(op='cast', ref='fog', value=1)), steps.index(dict(op='activate', ref='source', value=1)))
            self.assertIn(dict(op='activate', ref='source', value=0), self.case("Fool's Tome", 'hand_above', owner)['steps'])
            for name in ("Fool's Tome", 'Sea Gate Wreckage'):
                steps = self.case(name, 'effect', owner)['steps']
                self.assertIn(dict(op='assert_zone', player=owner, zone='library', value=2), steps)
                self.assertIn(dict(op='assert_zone_card', player=owner, zone='library', index=0, value=self.rows['Island']['csvid']), steps)

    def test_mana_alternative_and_true_colorless_remain_distinct(self):
        name = 'Sea Gate Wreckage'
        for owner in (0, 1):
            for mode, label, result, mana in [('effect', 'Draw a card', 1, 1), ('exact_colorless', 'Draw a card', 1, 0),
                                              ('exact_mixed', 'Draw a card', 1, 0),
                                              ('mana_at_limit', 'Get mana', 1, 5)]:
                steps = self.case(name, mode, owner)['steps']
                intent = next(s for s in steps if s['op'] == 'choose_activation_mode')
                self.assertEqual((intent['mode_label'], intent['player']), (label, owner))
                self.assertIn(dict(op='require_activation_mode', value=result), steps)
                colorless = [s['value'] for s in steps if s['op'] == 'assert_mana' and s['player'] == owner and s['color'] == 0]
                self.assertEqual(colorless[-1], mana)
            self.assertEqual(sum(s['op'] == 'choose_mana' for s in self.case(name, 'exact_mixed', owner)['steps']), 2*(owner == 0))
            for mode in ('hand_above', 'insufficient_mana', 'mana_outside_limit', 'opponent_hand', 'wrong_color'):
                steps = self.case(name, mode, owner)['steps']
                intent = next(s for s in steps if s['op'] == 'choose_activation_mode')
                self.assertEqual(intent['mode_label'], 'Get mana' if owner == 1 else 'Draw a card')
                self.assertIn(dict(op='require_activation_mode', value=owner), steps)
                assert_mana = [s['value'] for s in steps if s['op'] == 'assert_mana' and s['player'] == owner and s['color'] == 0]
                self.assertEqual(assert_mana[-1] - assert_mana[0], 1)

    def test_spell_gate_is_actor_and_turn_specific_and_counts_countered_casts(self):
        for owner in (0, 1):
            for name in ('Tapestry of the Ages', 'Seeker of Insight'):
                for mode, legal in [('no_spell', 0), ('creature_spell', 0), ('other_caster', 0), ('prior_turn', 0),
                                    ('effect', 1), ('offturn', 1), ('countered_spell', 1)]:
                    self.assertIn(dict(op='activate', ref='source', value=legal), self.case(name, mode, owner)['steps'])
                prior = self.case(name, 'prior_turn', owner)['steps']
                self.assertIn(dict(op='untap_step', player=1-owner), prior)
                countered = self.case(name, 'countered_spell', owner)['steps']
                self.assertLess(countered.index(dict(op='announce_spell', ref='fog')), countered.index(dict(op='resolve_response')))
            fresh = self.case('Seeker of Insight', 'fresh_denied', owner)['steps']
            self.assertIn(dict(op='assert_card', ref='source', property='sick', value=1), fresh)
            self.assertIn(dict(op='activate', ref='source', value=0), fresh)

    def test_mandatory_loot_proves_order_and_human_discard_selection(self):
        for mode, discarded, retained in [('effect', 'Mountain', None), ('discard_original', 'Forest', 'Mountain')]:
            steps = self.case('Seeker of Insight', mode)['steps']
            self.assertIn(dict(op='assert_zone_multiplicity', player=0, zone='graveyard', index=self.rows[discarded]['csvid'], value=1), steps)
            self.assertIn(dict(op='assert_discard_choices', value=1), steps)
            if retained:
                self.assertIn(dict(op='assert_zone_multiplicity', player=0, zone='hand', index=self.rows[retained]['csvid'], value=1), steps)

    def test_every_case_keeps_the_whole_cards_and_replays_exactly(self):
        count = 0
        for name in model.pins()['sources']:
            d, p = self.spec(name)
            for case in model.expand(name, p['csvid'], d, p, model.variants(p), self.rows, self.oracle):
                count += 1
                self.assertEqual(case['_recipe']['covered_clauses'], self.oracle[name]['oracle_text'].splitlines())
                steps = case['steps']
                ops = [s['op'] for s in steps]
                self.assertFalse(set(ops) - OPS)
                if 'replay' in case['name']:
                    self.assertEqual(steps[ops.index('capture')+1:ops.index('record_state')], steps[ops.index('restore')+1:-1])
        self.assertEqual(count, 189)
