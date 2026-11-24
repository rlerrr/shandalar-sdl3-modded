import json
import unittest
from pathlib import Path

from generate_cases import registry
from recipes import COLORS
from scarab_color_aura_recipes import FAMILY, PALETTE, classify, expand, profiles, spec, validate_witnesses, variants


class ScarabColorAuraRecipesTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.oracle = json.loads((Path(__file__).parent / 'data/oracle.json').read_text(encoding='utf8'))['cards']
        cls.rows = {row['name']: row for row in registry()}

    def declaration(self, name, selected):
        record = self.oracle[name]
        return dict(family=FAMILY, version=2, oracle_text=record['oracle_text'], params=classify(record), variants=selected)

    def steps(self, name, selected):
        declaration = self.declaration(name, [selected])
        case, = expand(name, self.rows[name]['csvid'], declaration, spec(declaration, self.oracle[name]),
                       [selected], self.rows, self.oracle)
        self.assertTrue(all('attacking' not in card.get('state', []) for card in case['setup']))
        return case['steps']

    def values(self, steps, op, **fields):
        return [step['value'] for step in steps if step['op'] == op and all(step.get(k) == v for k, v in fields.items())]

    def stats(self, steps):
        return [self.values(steps, 'assert_card', ref='host', property=stat) for stat in ('power', 'toughness')]

    def test_source_pins_and_grammar_reject_mismatched_color_or_additional_text(self):
        validate_witnesses(self.oracle, self.rows)
        self.assertEqual({self.rows[name]['csvid'] for name in profiles()['sources']}, {1873, 1876, 1960, 2079, 2154})
        for name in profiles()['sources']:
            record = self.oracle[name]
            params = classify(record)
            self.assertEqual(len(set(variants(params))), 36)
            declaration = self.declaration(name, ['matched_block_denied_0'])
            spec(declaration, record)
            with self.assertRaises(ValueError):
                spec(dict(declaration, version=2.0), record)
            with self.assertRaises(ValueError):
                spec(dict(declaration, params=dict(params, power_bonus=2.0)), record)
            with self.assertRaises(ValueError):
                classify(dict(record, oracle_text=record['oracle_text'] + '\nDraw a card.'))
        record = self.oracle['Blue Scarab']
        with self.assertRaises(ValueError):
            classify(dict(record, oracle_text=record['oracle_text'].replace('blue permanent', 'black permanent')))

    def test_actual_target_and_only_opposing_colored_permanent_changes_stats(self):
        for mode, power, toughness in (('owner_match', 5, 4), ('opponent_match', 7, 6), ('matching_land', 5, 4)):
            steps = self.steps('Red Scarab', mode + '_1')
            self.assertEqual(sum(s['op'] == 'choose_target' and s.get('ref') == 'host' and s.get('player') == 1 for s in steps), 1)
            self.assertEqual(self.values(steps, 'assert_card', ref='source', property='in_play'), [1])
            self.assertEqual([series[-1] for series in self.stats(steps)], [power, toughness])
            self.assertEqual(self.values(steps, 'assert_card', ref='alternate', property='power')[-1], 2)
        steps = self.steps('Red Scarab', 'matching_land_0')
        self.assertEqual(self.values(steps, 'assert_matching_permanents', player=1, index=self.rows['Mountain']['csvid'], property='color'), [0])

    def test_every_color_filters_actual_blocker_and_deals_boosted_damage(self):
        for name in profiles()['sources']:
            color, creature, _, _ = PALETTE[classify(self.oracle[name])['color']]
            for owner in (0, 1):
                steps = self.steps(name, f'matched_block_denied_{owner}')
                self.assertEqual(self.values(steps, 'assert_matching_permanents', player=1-owner,
                                             index=self.rows[creature]['csvid'], property='color'), [1 << COLORS[color]])
                self.assertEqual(self.values(steps, 'attack', ref='host'), [1])
                self.assertEqual(self.values(steps, 'block', ref='other', other='host'), [0])
                self.assertEqual(self.values(steps, 'assert_card', ref='host', property='blocked'), [0])
                self.assertEqual(self.values(steps, 'assert_life', player=1-owner)[-1], 13)

    def test_restriction_only_affects_blocks_of_the_host(self):
        steps = self.steps('Black Scarab', 'unenchanted_attacker_0')
        self.assertEqual(self.values(steps, 'block', ref='other', other='host'), [0])
        self.assertEqual(self.values(steps, 'block', ref='other', other='alternate'), [1])
        defend = self.steps('Black Scarab', 'host_can_block_1')
        self.assertEqual(self.values(defend, 'block', ref='host', other='other'), [1])
        self.assertEqual(self.values(defend, 'assert_card', ref='host', property='damage')[-1], 2)
        nonmatching = self.steps('Black Scarab', 'other_block_allowed_0')
        self.assertEqual(self.values(nonmatching, 'block', ref='other', other='host'), [1])
        self.assertEqual(self.stats(nonmatching)[0][-1], 7)

    def test_foreign_host_uses_aura_controller_for_stats_and_its_own_normal_attack(self):
        for owner in (0, 1):
            denied = self.steps('White Scarab', f'foreign_host_block_denied_{owner}')
            self.assertEqual([s['player'] for s in denied if s['op'] == 'untap_step'], [1-owner])
            self.assertEqual(self.values(denied, 'block', ref='other', other='host'), [0])
            self.assertEqual(self.stats(denied)[0][-1], 5)
            self.assertEqual(self.values(denied, 'assert_life', player=owner)[-1], 15)
            boosted = self.steps('White Scarab', f'foreign_host_boost_{owner}')
            self.assertEqual(self.stats(boosted)[0][-1], 7)
            self.assertEqual(self.values(boosted, 'block', ref='other', other='host'), [1])

    def test_real_permanent_cast_and_last_match_departure(self):
        arrives = self.steps('Blue Scarab', 'match_arrives_leaves_1')
        self.assertEqual(self.values(arrives, 'cast', ref='match'), [1])
        self.assertEqual(self.values(arrives, 'cast', ref='disenchant'), [1])
        self.assertEqual(self.stats(arrives), [[5, 5, 5, 7, 5], [4, 4, 4, 6, 4]])
        last = self.steps('Green Scarab', 'last_match_leaves_0')
        self.assertEqual(self.values(last, 'cast', ref='unsummon'), [1])
        self.assertEqual(self.stats(last), [[5, 7, 7, 5], [4, 6, 6, 4]])

    def test_aura_departure_restores_blocks_and_leaving_host_drops_aura(self):
        steps = self.steps('White Scarab', 'aura_removed_combat_0')
        self.assertEqual(self.values(steps, 'block', ref='other', other='host'), [1])
        self.assertEqual(self.stats(steps)[0], [5, 7, 5, 5])
        self.assertEqual(self.values(steps, 'assert_zone_multiplicity', player=0, zone='graveyard', index=2154)[-1], 1)
        returned = self.steps('Red Scarab', 'host_returns_1')
        self.assertEqual(self.values(returned, 'assert_zone_multiplicity', player=1, zone='graveyard', index=2079)[-1], 1)
        self.assertEqual(self.values(returned, 'assert_zone_multiplicity', player=1, zone='hand', index=1112)[-1], 1)

    def test_exact_white_price_and_no_target_cast_requirements(self):
        cast = self.steps('Blue Scarab', 'plain_0')
        self.assertEqual(self.values(cast, 'set_mana', player=0, color=COLORS['W']), [0, 1])
        self.assertEqual(self.values(cast, 'assert_mana', player=0, color=COLORS['W'])[-1], 0)
        for mode, available_white in (('insufficient_mana', 0), ('wrong_color', 0), ('no_creature', 1)):
            denied = self.steps('Blue Scarab', mode + '_1')
            self.assertEqual(self.values(denied, 'cast', ref='source'), [0])
            self.assertFalse(any(step['op'] == 'choose_target' for step in denied))
            self.assertEqual(self.values(denied, 'assert_mana', player=1, color=COLORS['W'])[-1], available_white)
            self.assertEqual(self.values(denied, 'assert_zone_multiplicity', player=1, zone='hand', index=1876)[-1], 1)

    def test_pending_replay_keeps_attachment_and_power_receipts(self):
        steps = self.steps('Green Scarab', 'pending_snapshot_1')
        names = [step['op'] for step in steps]
        capture, record, restore = [names.index(op) for op in ('capture', 'record_state', 'restore')]
        self.assertLess(names.index('cast'), capture)
        self.assertEqual(names[capture + 1], 'resolve')
        self.assertEqual(steps[capture + 1:record], steps[restore + 1:-1])
        self.assertEqual(self.stats(steps)[0][-1], 7)
