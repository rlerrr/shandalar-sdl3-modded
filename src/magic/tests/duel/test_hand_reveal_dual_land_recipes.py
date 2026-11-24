import json
import unittest
from pathlib import Path

from generate_cases import registry
from hand_reveal_dual_land_recipes import COLORS, FAMILY, classify, expand, matches, spec, validate_witnesses, variants


class HandRevealDualLandRecipesTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.oracle = json.loads((Path(__file__).parent / 'data/oracle.json').read_text(encoding='utf8'))['cards']
        cls.rows = {row['name']: row for row in registry()}

    def declaration(self, name):
        record = self.oracle[name]
        return dict(family=FAMILY, version=2, oracle_text=record['oracle_text'], params=classify(record), variants=[])

    def steps(self, name, mode):
        declaration = self.declaration(name)
        model = spec(declaration, self.oracle[name])
        return expand(name, self.rows[name]['csvid'], declaration, model, [mode], self.rows, self.oracle)[0]['steps']

    def values(self, steps, op, **fields):
        return [step['value'] for step in steps if step['op'] == op and all(step.get(k) == v for k, v in fields.items())]

    def test_full_clause_and_reserved_sources(self):
        for name in ('Ancient Amphitheater', "Auntie's Hovel", 'Wanderwine Hub', 'Foreboding Ruins', 'Fortified Village', 'Port Town'):
            record = self.oracle[name]
            declaration = self.declaration(name)
            self.assertEqual(len(spec(declaration, record)['outputs']), 2)
            with self.assertRaises(ValueError):
                classify(dict(record, oracle_text=record['oracle_text'] + '\nDraw a card.'))
            with self.assertRaises(ValueError):
                spec(dict(declaration, version=True), record)
        for name, kind, types, outputs in (
                ('Secluded Glen', 'creature_type', ['Faerie'], ['U', 'B']),
                ('Game Trail', 'land_types', ['Mountain', 'Forest'], ['R', 'G'])):
            model = spec(self.declaration(name), self.oracle[name])
            self.assertEqual((model['reveal_kind'], model['subtypes'], model['outputs']), (kind, types, outputs))
        validate_witnesses(self.oracle, self.rows)

    def test_printed_types_distinguish_kindred_changeling_and_same_color_land(self):
        goblin = classify(self.oracle["Auntie's Hovel"])
        self.assertTrue(matches(self.oracle['Tarfire'], goblin))
        self.assertTrue(matches(self.oracle['Woodland Changeling'], goblin))
        self.assertFalse(matches(self.oracle['Grizzly Bears'], goblin))
        ruin = classify(self.oracle['Foreboding Ruins'])
        for name in ('Swamp', 'Mountain', 'Blood Crypt'):
            self.assertTrue(matches(self.oracle[name], ruin))
        for name in ('Rakdos Guildgate', 'Woodland Changeling', 'Tarfire'):
            self.assertFalse(matches(self.oracle[name], ruin))

    def test_actual_atlas_then_reveal_and_unchanged_hand(self):
        for owner in (0, 1):
            steps = self.steps('Wanderwine Hub', f'kindred_{owner}')
            self.assertEqual(self.values(steps, 'activate', ref='atlas'), [1])
            self.assertFalse(any(s['op'] == 'cast' for s in steps))
            self.assertEqual(self.values(steps, 'choose_zone_card', player=owner), [9304, 9061])
            self.assertEqual(self.values(steps, 'choose_dialog', player=owner), [0] if owner == 0 else [])
            self.assertEqual(self.values(steps, 'assert_offered_legal_card', index=9061), [1])
            self.assertEqual(self.values(steps, 'assert_offered_legal_card', index=106), [0])
            self.assertEqual(self.values(steps, 'assert_zone_multiplicity', player=owner, zone='hand', index=9061)[-1], 1)
            self.assertEqual(self.values(steps, 'assert_card', ref='source', property='tapped')[-1], 0)

    def test_declines_and_wrong_zone_do_not_fabricate_reveals(self):
        owner = 1
        voluntary = self.steps('Fortified Village', 'decline_1')
        self.assertEqual(self.values(voluntary, 'choose_optional_zone', player=owner), [91])
        self.assertEqual(self.values(voluntary, 'assert_offered_selection_required'), [0])
        self.assertEqual(self.values(voluntary, 'activate', ref='source'), [0])
        self.assertEqual(self.values(voluntary, 'assert_offered_legal_card', index=91), [1])
        remote = self.steps('Fortified Village', 'opponent_hand_only_1')
        self.assertEqual(self.values(remote, 'assert_offered_legal_card', index=15821), [1])
        self.assertFalse(self.values(remote, 'assert_offered_legal_card', index=91))
        self.assertEqual(self.values(remote, 'assert_zone_multiplicity', player=0, zone='hand', index=91)[-1], 1)
        atlas = self.steps('Fortified Village', 'atlas_decline_0')
        self.assertEqual(self.values(atlas, 'assert_card', ref='source', property='in_hand')[-1], 1)

    def test_both_actual_colors_fund_casts_across_real_untap(self):
        steps = self.steps('Foreboding Ruins', 'switch_colors_1')
        self.assertEqual(self.values(steps, 'choose_color', player=1), [COLORS.index('B'), COLORS.index('R')])
        self.assertEqual(self.values(steps, 'cast', ref='spender'), [1])
        self.assertEqual(self.values(steps, 'cast', ref='second_spender'), [1])
        self.assertEqual([s['player'] for s in steps if s['op'] == 'untap_step'], [0, 1])
        for color in ('B', 'R'):
            history = self.values(steps, 'assert_mana', player=1, color=COLORS.index(color))
            self.assertIn(1, history)
            self.assertEqual(history[-1], 0)

    def test_bounced_land_rechecks_a_fresh_optional_choice(self):
        steps = self.steps('Ancient Amphitheater', 'reenter_decline_0')
        self.assertEqual(self.values(steps, 'choose_zone_card', player=0), [9060, 110, 9060])
        self.assertEqual(self.values(steps, 'choose_optional_zone', player=0), [110])
        self.assertEqual(self.values(steps, 'bind_hand_copy', ref='source'), [9060])
        self.assertEqual(self.values(steps, 'assert_card', ref='source', property='tapped')[-1], 1)

    def test_pending_and_mana_replay_keep_actual_receipts(self):
        for mode in ('pending_snapshot_0', 'mana_snapshot_1'):
            steps = self.steps('Port Town', mode)
            capture = next(i for i, s in enumerate(steps) if s['op'] == 'capture')
            record = next(i for i, s in enumerate(steps) if s['op'] == 'record_state')
            restore = next(i for i, s in enumerate(steps) if s['op'] == 'restore')
            self.assertEqual(steps[capture+1:record], steps[restore+1:-1])

    def test_pilot_expands_both_owners(self):
        for name in ('Ancient Amphitheater', "Auntie's Hovel", 'Wanderwine Hub', 'Foreboding Ruins', 'Fortified Village', 'Port Town'):
            declaration = self.declaration(name)
            model = spec(declaration, self.oracle[name])
            cases = expand(name, self.rows[name]['csvid'], declaration, model, variants(model), self.rows, self.oracle)
            self.assertEqual(len({case['name'] for case in cases}), 42)

    def test_exact_followers_keep_real_qualifiers_and_nonmatching_gate(self):
        for owner in (0, 1):
            glen = self.steps('Secluded Glen', f'kindred_{owner}')
            self.assertEqual(self.values(glen, 'choose_zone_card', player=owner), [9250, 9130])
            self.assertEqual(self.values(glen, 'assert_offered_legal_card', index=9226), [1])
            self.assertEqual(self.values(glen, 'assert_offered_legal_card', index=9315), [1])
            trail = self.steps('Game Trail', f'nonbasic_typed_{owner}')
            self.assertEqual(self.values(trail, 'choose_zone_card', player=owner), [15825, 1710])
            self.assertEqual(self.values(trail, 'assert_offered_legal_card', index=164), [1])
            self.assertEqual(self.values(trail, 'assert_offered_legal_card', index=91), [1])
            self.assertEqual(self.values(trail, 'assert_offered_legal_card', index=12964), [0])
            self.assertEqual(self.values(trail, 'assert_zone_multiplicity', player=owner, zone='hand', index=1710)[-1], 1)
