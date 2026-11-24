import json
import unittest
from pathlib import Path

from damage_loot_creatures_recipes import FAMILY, classify, expand, profiles, spec, validate_witnesses, variants
from generate_cases import registry


class DamageLootCreaturesRecipesTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.oracle = json.loads((Path(__file__).parent / 'data/oracle.json').read_text(encoding='utf8'))['cards']
        cls.rows = {row['name']: row for row in registry()}

    def case(self, name, mode, owner=0):
        oracle = self.oracle[name]
        selected = [f'{mode}_{owner}']
        declaration = dict(family=FAMILY, version=2, params=classify(oracle), oracle_text=oracle['oracle_text'],
                           variants=selected)
        case, = expand(name, self.rows[name]['csvid'], declaration, spec(declaration, oracle), selected,
                       self.rows, self.oracle)
        self.assertFalse(any('state' in card for card in case['setup']))
        self.assertFalse(any(step['op'] in ('destroy', 'move', 'discard', 'draw') for step in case['steps']))
        return case['steps']

    def operations(self, steps, op, **fields):
        return [step for step in steps if step['op'] == op and all(step.get(key) == value for key, value in fields.items())]

    def values(self, steps, op, **fields):
        return [step['value'] for step in self.operations(steps, op, **fields)]

    def copies(self, steps, name, player, zone):
        return self.values(steps, 'assert_zone_multiplicity', player=player, zone=zone,
                           index=self.rows[name]['csvid'])

    def test_full_oracle_typed_and_implemented(self):
        validate_witnesses(self.oracle, self.rows)
        self.assertEqual({self.rows[name]['csvid'] for name in profiles()['sources']}, {8598, 19021, 14204, 16238})
        for name in profiles()['sources']:
            record = self.oracle[name]
            params = classify(record)
            declaration = dict(family=FAMILY, version=2, params=params, oracle_text=record['oracle_text'],
                               variants=variants(params))
            self.assertEqual(spec(declaration, record), dict(params, family=FAMILY))
            with self.assertRaises(ValueError):
                spec(dict(declaration, params=dict(params, optional=int(params['optional']))), record)
            with self.assertRaises(ValueError):
                classify(dict(record, oracle_text=record['oracle_text'] + '\nFlying'))

    def test_loot_requires_draw_before_either_exact_discard(self):
        for name in profiles()['sources']:
            for mode, discarded, kept in (('discard_drawn', 'Island', 'Forest'), ('discard_old', 'Forest', 'Island')):
                steps = self.case(name, mode)
                self.assertEqual(self.values(steps, 'assert_zone_card', player=0, zone='library', index=0)[-1],
                                 self.rows['Mountain']['csvid'])
                self.assertEqual(self.copies(steps, discarded, 0, 'graveyard')[-1], 1)
                self.assertEqual(self.copies(steps, kept, 0, 'hand')[-1], 1)
                self.assertEqual(self.copies(steps, 'Forest', 1, 'hand')[-1], 1)

    def test_combat_damage_recipient_predicate(self):
        for owner in (0, 1):
            for name in profiles()['sources']:
                connect = self.case(name, 'connect', owner)
                blocked = self.case(name, 'blocked', owner)
                prevented = self.case(name, 'prevented', owner)
                self.assertTrue(self.operations(connect, 'combat_damage'))
                self.assertEqual(self.copies(connect, 'Island', owner, 'graveyard')[-1], 1)
                self.assertEqual(self.values(blocked, 'block', ref='blocker'), [1])
                self.assertEqual(self.copies(blocked, 'Island', owner, 'graveyard')[-1], 0)
                self.assertEqual(self.values(prevented, 'cast', ref='shield'), [1])
                self.assertEqual(self.values(prevented, 'assert_life', player=1-owner)[-1], 20)
                self.assertEqual(self.copies(prevented, 'Island', owner, 'graveyard')[-1], 0)

    def test_actual_same_source_noncombat_vs_self_and_combat_only(self):
        for owner in (0, 1):
            for name, can_loot in (('Looter il-Kor', True), ('Surge Mare', True), ('Abomination of Gudul', False),
                                   ('Wharf Infiltrator', False)):
                for mode, target in (('noncombat', 1-owner), ('self_damage', owner)):
                    steps = self.case(name, mode, owner)
                    self.assertEqual(self.values(steps, 'cast', ref='grant'), [1])
                    self.assertEqual(self.values(steps, 'choose_player', player=owner), [target])
                    self.assertEqual(self.values(steps, 'activate', ref='grant'), [1])
                    self.assertEqual(self.values(steps, 'assert_life', player=target)[-1], 19)
                    self.assertEqual(self.copies(steps, 'Island', owner, 'graveyard')[-1], int(can_loot and target != owner))

    def test_all_four_printed_evasion_patterns_on_real_assignment(self):
        for name in profiles()['sources']:
            steps = self.case(name, 'evasion', 1)
            self.assertEqual(self.values(steps, 'block', ref='blocker', other='source'), [0])
            self.assertEqual(self.copies(steps, 'Island', 1, 'graveyard')[-1], 1)
            defense = self.case(name, 'source_blocks', 1)
            self.assertEqual(self.values(defense, 'block', ref='source', other='opposing'), [1])
            self.assertEqual(self.copies(defense, 'Island', 1, 'graveyard')[-1], 0)

    def test_mare_real_colored_cost_activated_stats_cleanup_and_sba(self):
        for owner in (0, 1):
            zero = self.case('Surge Mare', 'zero_power', owner)
            self.assertFalse(self.operations(zero, 'activate'))
            self.assertEqual(self.values(zero, 'assert_life', player=1-owner)[-1], 20)
            duration = self.case('Surge Mare', 'pump_duration', owner)
            self.assertEqual(self.values(duration, 'activate', ref='source'), [1])
            for prop, pumped, base in (('power', 2, 0), ('toughness', 3, 5)):
                values = self.values(duration, 'assert_card', ref='source', property=prop)
                self.assertIn(pumped, values)
                self.assertEqual(values[-1], base)
            self.assertEqual(self.values(self.case('Surge Mare', 'pump_short', owner), 'activate', ref='source'), [0])
            lethal = self.case('Surge Mare', 'pump_lethal', owner)
            self.assertEqual(self.values(lethal, 'activate', ref='source'), [1, 1, 1])
            self.assertEqual(self.copies(lethal, 'Surge Mare', owner, 'graveyard')[-1], 1)

    def test_gudul_actual_hidden_combat_and_real_morph_restores_trigger(self):
        for owner in (0, 1):
            hidden = self.case('Abomination of Gudul', 'hidden_hit', owner)
            hit = self.case('Abomination of Gudul', 'morph_hit', owner)
            off_turn = self.case('Abomination of Gudul', 'morph_offturn', owner)
            self.assertEqual(self.values(hidden, 'assert_life', player=1-owner)[-1], 18)
            self.assertEqual(self.copies(hidden, 'Island', owner, 'graveyard')[-1], 0)
            for turned in (hit, off_turn):
                self.assertEqual(self.values(turned, 'activate', ref='source'), [1])
                self.assertEqual(self.values(turned, 'assert_life', player=1-owner)[-1], 17)
                self.assertEqual(self.copies(turned, 'Island', owner, 'graveyard')[-1], 1)
            denied = self.case('Abomination of Gudul', 'morph_short', owner)
            self.assertEqual(self.values(denied, 'activate', ref='source'), [0])
            self.assertEqual(self.values(denied, 'assert_card', ref='source', property='csvid')[-1],
                             self.rows['Face-Down Creature']['csvid'])

    def test_wharf_creature_discard_payment_and_native_token(self):
        for owner in (0, 1):
            for mode, token in (('wharf_funded', 1), ('wharf_unfunded', 0), ('external_discard', 1),
                                ('wharf_noncreature', 0)):
                steps = self.case('Wharf Infiltrator', mode, owner)
                self.assertEqual(self.copies(steps, 'Eldrazi Horror', owner, 'battlefield')[-1], token)
                self.assertEqual(self.copies(steps, 'Grizzly Bears' if mode != 'wharf_noncreature' else 'Island',
                                             owner, 'graveyard')[-1], 1)
                if mode == 'wharf_unfunded':
                    self.assertEqual(len(self.operations(steps, 'choose_cancel_mana', player=owner)), int(owner == 0))
                if token:
                    for property, value in (('color', 0), ('power', 3), ('toughness', 2), ('token', 1),
                                            ('subtype_Horror', 1), ('subtype_Eldrazi', 1)):
                        self.assertEqual(self.values(steps, 'assert_matching_permanents', player=owner,
                                                     property=property)[-1], value)
        cancel = self.case('Wharf Infiltrator', 'wharf_payment_cancel')
        self.assertTrue(self.operations(cancel, 'choose_cancel_mana', player=0))
        self.assertEqual(self.values(cancel, 'assert_mana', player=0, color=2)[-1], 2)
        self.assertEqual(self.copies(cancel, 'Eldrazi Horror', 0, 'battlefield')[-1], 0)

    def test_optional_decline_leaves_old_hand_and_library_and_real_damage(self):
        for name in ('Surge Mare', 'Abomination of Gudul', 'Wharf Infiltrator'):
            steps = self.case(name, 'decline_empty')
            self.assertFalse(self.operations(steps, 'choose_trigger_source'))
            self.assertEqual(self.copies(steps, 'Forest', 0, 'hand')[-1], 1)
            self.assertEqual(self.values(steps, 'assert_zone', player=0, zone='graveyard')[-1], 0)
            self.assertLess(self.values(steps, 'assert_life', player=1)[-1], 20)

    def test_snapshot_replays_exact_damage_and_discard_tail(self):
        steps = self.case('Abomination of Gudul', 'snapshot')
        ops = [step['op'] for step in steps]
        self.assertEqual(steps[ops.index('capture')+1:ops.index('record_state')],
                         steps[ops.index('restore')+1:ops.index('assert_recorded_state')])
