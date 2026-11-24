import json
import unittest
from pathlib import Path

from combat_wide_fog_recipes import FAMILY, classify, expand, spec, validate_witnesses, variants
from generate_cases import registry


class CombatWideFogRecipesTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.oracle = json.loads((Path(__file__).parent / 'data/oracle.json').read_text(encoding='utf8'))['cards']
        cls.rows = {row['name']: row for row in registry()}

    def declaration(self, name):
        record = self.oracle[name]
        declaration = dict(family=FAMILY, version=2, oracle_text=record['oracle_text'],
                           params=classify(record), variants=[])
        return declaration, spec(declaration, record)

    def case(self, name, variant):
        declaration, params = self.declaration(name)
        return expand(name, self.rows[name]['csvid'], declaration, params, [variant], self.rows, self.oracle)[0]

    def values(self, steps, op, **fields):
        return [step['value'] for step in steps if step['op'] == op and all(step.get(k) == v for k, v in fields.items())]

    def test_full_clause_types_and_pins(self):
        names = {'Constant Mists': 'land_buyback', 'Dawnstrider': 'tap_discard', 'Batwing Brume': 'color_fog_loss',
                 'Pollen Lullaby': 'clash_untap', 'Arachnogenesis': 'spider_fog'}
        for name, model in names.items():
            record = self.oracle[name]
            declaration, params = self.declaration(name)
            self.assertEqual(params['model'], model)
            with self.assertRaises(ValueError):
                classify(dict(record, oracle_text=record['oracle_text'] + '\nDraw a card.'))
            with self.assertRaises(ValueError):
                spec(dict(declaration, version=2.0), record)
        for name in ('Spore Flower', 'Sewers of Estark', 'Loyal Unicorn'):
            with self.assertRaises(ValueError):
                classify(self.oracle[name])
        validate_witnesses(self.oracle, self.rows)
        changed = dict(self.oracle, Spider=dict(self.oracle['Spider'], toughness='3'))
        with self.assertRaises(ValueError):
            validate_witnesses(changed, self.rows)

    def test_buyback_moves_land_while_pending_then_returns_instant(self):
        steps = self.case('Constant Mists', 'buyback_0')['steps']
        cast = next(i for i, step in enumerate(steps) if step['op'] == 'cast')
        resolve = next(i for i, step in enumerate(steps) if step['op'] == 'resolve')
        self.assertIn(dict(op='choose_target', player=0, ref='buyback_land'), steps[:cast])
        self.assertIn(1, self.values(steps[cast:resolve], 'assert_zone_multiplicity', player=0, zone='graveyard', index=91))
        self.assertEqual(self.values(steps[resolve:], 'assert_zone_multiplicity', player=0, zone='hand', index=3350)[-1], 1)
        steps = self.case('Constant Mists', 'foreign_land_only_0')['steps']
        self.assertFalse(any(step['op'] == 'choose_dialog' for step in steps))
        self.assertEqual(self.values(steps, 'assert_zone_multiplicity', player=1, zone='battlefield', index=91)[-1], 1)

    def test_dawnstrider_pays_tap_and_discard_even_when_source_leaves(self):
        steps = self.case('Dawnstrider', 'source_lost_pending_1')['steps']
        activate = next(i for i, step in enumerate(steps) if step['op'] == 'activate')
        destroy = next(i for i, step in enumerate(steps) if step['op'] == 'destroy')
        self.assertEqual(self.values(steps[:destroy], 'assert_card', ref='source', property='tapped')[-1], 1)
        self.assertEqual(self.values(steps[activate:destroy], 'assert_zone_multiplicity', player=1, zone='graveyard', index=188)[-1], 1)
        self.assertEqual(self.values(steps, 'assert_life', player=1)[-1], 20)
        missing = self.case('Dawnstrider', 'no_discard_0')['steps']
        self.assertEqual(self.values(missing, 'activate', ref='source'), [0])
        self.assertEqual(self.values(missing, 'assert_life', player=0)[-1], 17)

    def test_batwing_payment_colors_independently_control_fog_and_loss(self):
        for branch, mana_color, attacker_life, defender_life in (
                ('defend', 0, 20, 20), ('black', 1, 18, 17), ('both', 2, 18, 20)):
            steps = self.case('Batwing Brume', f'{branch}_0')['steps']
            self.assertEqual(self.values(steps, 'choose_dialog', player=0), [mana_color])
            self.assertEqual(self.values(steps, 'assert_life', player=1)[-1], attacker_life)
            self.assertEqual(self.values(steps, 'assert_life', player=0)[-1], defender_life)
        own = self.case('Batwing Brume', 'both_own_attack_0')['steps']
        self.assertEqual(self.values(own, 'assert_life', player=0)[-1], 18)

    def test_pollen_win_skips_exactly_one_opponent_untap_and_moves_human_top(self):
        for mode, expected in (('clash_win', [1, 0]), ('clash_lose', [0]), ('clash_tie', [0])):
            steps = self.case('Pollen Lullaby', f'{mode}_0')['steps']
            visits = [i for i, step in enumerate(steps) if step['op'] == 'untap_step' and step['player'] == 1]
            state = [self.values(steps[i:i+150], 'assert_card', ref='large', property='tapped')[0] for i in visits]
            self.assertEqual(state, expected)
        steps = self.case('Pollen Lullaby', 'human_bottom_0')['steps']
        self.assertEqual(self.values(steps, 'assert_zone_card', player=0, zone='library', index=0)[-1], 91)
        self.assertEqual(self.values(steps, 'assert_zone_card', player=0, zone='library', index=2)[-1], 49)

    def test_spider_count_reach_and_spider_source_damage(self):
        reach = self.case('Arachnogenesis', 'reach_block_0')['steps']
        self.assertEqual(self.values(reach, 'assert_zone_multiplicity', player=0, zone='battlefield', index=18827)[-1], 2)
        self.assertIn(dict(op='block', ref='spider_0', other='large', value=1), reach)
        self.assertEqual(self.values(reach, 'assert_card', ref='large', property='damage')[-1], 1)
        spider = self.case('Arachnogenesis', 'spider_block_0')['steps']
        self.assertEqual(self.values(spider, 'assert_zone_multiplicity', player=0, zone='battlefield', index=18827)[-1], 1)
        own = self.case('Arachnogenesis', 'own_attack_0')['steps']
        self.assertEqual(self.values(own, 'assert_zone_multiplicity', player=0, zone='battlefield', index=18827)[-1], 0)

    def test_real_turn_expiry_and_pending_roundtrip(self):
        turns = self.case('Constant Mists', 'after_turn_0')['steps']
        windows = [i for i, step in enumerate(turns) if step['op'] == 'combat_damage']
        self.assertEqual(len(windows), 2)
        self.assertEqual([step['player'] for step in turns[windows[0]:windows[1]] if step['op'] == 'untap_step'], [0, 1])
        self.assertEqual(self.values(turns, 'assert_life', player=0)[-1], 11)
        duel = self.case('Dawnstrider', 'pending_snapshot_1')['steps']
        captured = next(i for i, step in enumerate(duel) if step['op'] == 'capture')
        recorded = next(i for i, step in enumerate(duel) if step['op'] == 'record_state')
        restored = next(i for i, step in enumerate(duel) if step['op'] == 'restore')
        self.assertEqual(duel[captured+1:recorded], duel[restored+1:-1])

    def test_pilot_variants_expand_for_both_controllers(self):
        counts = {'Constant Mists': 24, 'Dawnstrider': 28, 'Batwing Brume': 30, 'Pollen Lullaby': 24, 'Arachnogenesis': 24}
        for name, count in counts.items():
            declaration, params = self.declaration(name)
            cases = expand(name, self.rows[name]['csvid'], declaration, params, variants(params), self.rows, self.oracle)
            self.assertEqual(len({case['name'] for case in cases}), count)
