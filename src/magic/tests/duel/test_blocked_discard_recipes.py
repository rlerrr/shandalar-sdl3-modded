import json
import unittest
from pathlib import Path

from blocked_discard_recipes import FAMILY, classify, expand, profiles, spec, validate_witnesses, variants
from generate_cases import registry


class BlockedDiscardRecipesTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.oracle = json.loads((Path(__file__).parent / 'data/oracle.json').read_text(encoding='utf8'))['cards']
        cls.rows = {row['name']: row for row in registry()}

    def case(self, name, mode, owner=0):
        record = self.oracle[name]
        selected = [f'{mode}_{owner}']
        declaration = dict(family=FAMILY, version=2, params=classify(record),
                           oracle_text=record['oracle_text'], variants=selected)
        case, = expand(name, self.rows[name]['csvid'], declaration, spec(declaration, record), selected,
                       self.rows, self.oracle)
        self.assertFalse(any(card.get('state') for card in case['setup']))
        self.assertFalse(any(s['op'] in ('activate', 'destroy', 'move') for s in case['steps']))
        return case['steps']

    def values(self, steps, op, **fields):
        return [s['value'] for s in steps if s['op'] == op and all(s.get(k) == v for k, v in fields.items())]

    def between(self, steps, first, last):
        ops = [s['op'] for s in steps]
        return steps[ops.index(first) + 1:ops.index(last)]

    def copies(self, steps, name, player, zone):
        return self.values(steps, 'assert_zone_multiplicity', player=player, zone=zone,
                           index=self.rows[name]['csvid'])

    def test_whole_oracle_and_typed_exact_variants(self):
        validate_witnesses(self.oracle, self.rows)
        self.assertEqual({self.rows[name]['csvid'] for name in profiles()['sources']}, {4382, 4579, 13067, 3588, 1808, 3120})
        for name in profiles()['sources']:
            record = self.oracle[name]
            params = classify(record)
            declaration = dict(family=FAMILY, version=2, params=params, oracle_text=record['oracle_text'],
                               variants=variants(params))
            self.assertEqual(spec(declaration, record), dict(params, family=FAMILY))
            with self.assertRaises(ValueError):
                spec(dict(declaration, params=dict(params, blocked=int(params['blocked']))), record)
            with self.assertRaises(ValueError):
                spec(dict(declaration, variants=declaration['variants'] + ['invented_0']), record)
        with self.assertRaises(ValueError):
            classify(dict(self.oracle['Robber Fly'], keywords=[]))

    def test_block_versus_unblocked_dispatch_once_at_declarations(self):
        for name, qualifies in (('Alley Grifters', 'blocked'), ('Abyssal Nightstalker', 'unblocked')):
            steps = self.case(name, qualifies, 1)
            before = self.between(steps, 'begin_blockers', 'advance_combat')
            after = self.between(steps, 'advance_combat', 'combat_damage')
            self.assertEqual(self.copies(before, 'Forest', 0, 'hand'), [1])
            self.assertEqual(self.copies(after, 'Forest', 0, 'hand'), [0])
            self.assertEqual(self.copies(after, 'Forest', 0, 'graveyard'), [1])
            self.assertEqual(self.copies(after, 'Forest', 0, 'exile'), [1])
            self.assertEqual(self.copies(after, 'Lightning Bolt', 0, 'hand'), [1])
            other = self.case(name, 'unblocked' if qualifies == 'blocked' else 'blocked', 1)
            self.assertEqual(self.copies(other, 'Forest', 0, 'hand')[-1], 1)
            self.assertEqual(self.copies(other, 'Forest', 0, 'graveyard')[-1], 0)

    def test_both_blockers_are_real_and_only_one_discard(self):
        steps = self.case('Slate Street Ruffian', 'multi_blockers')
        self.assertEqual(self.values(steps, 'block', other='source'), [1, 1])
        self.assertEqual(self.values(steps, 'assert_card', ref='second_blocker', property='blocking_count'), [1])
        self.assertEqual(self.copies(steps, 'Forest', 1, 'hand')[-1], 1)
        self.assertEqual(self.copies(steps, 'Forest', 1, 'graveyard')[-1], 1)
        self.assertFalse(self.values(steps, 'combat_damage'))

    def test_human_defender_chooses_exact_different_legal_card(self):
        steps = self.case('Alley Grifters', 'human_discard_high', 1)
        self.assertEqual(self.values(steps, 'discard_policy'), [2])
        self.assertEqual(self.copies(steps, 'Lightning Bolt', 0, 'graveyard')[-1], 1)
        self.assertEqual(self.copies(steps, 'Forest', 0, 'hand')[-1], 1)
        self.assertEqual(self.copies(steps, 'Swamp', 1, 'hand')[-1], 1)

    def test_robber_draws_exact_full_short_or_empty_hand(self):
        for mode, amount in (('blocked', 3), ('short_hand', 1), ('empty_hand', 0)):
            steps = self.case('Robber Fly', mode, 1)
            after = self.between(steps, 'advance_combat', 'combat_damage')
            self.assertEqual(self.values(after, 'assert_zone', player=0, zone='hand'), [amount])
            self.assertEqual(self.values(after, 'assert_zone', player=0, zone='library'), [5 - amount])
            self.assertEqual(self.values(after, 'assert_zone', player=0, zone='graveyard'), [1 + amount])
            self.assertEqual(self.values(after, 'assert_zone_card', player=0, zone='library', index=0),
                             [self.rows[('Island', 'Mountain', 'Plains', 'Counterspell')[amount]]['csvid']])
            self.assertEqual(self.copies(after, 'Island', 0, 'hand'), [int(amount > 0)])
            self.assertEqual(self.copies(after, 'Forest', 0, 'hand'), [0])

    def test_robber_flying_ground_denial_and_flyer_block(self):
        ground = self.case('Robber Fly', 'flying_ground')
        flyer = self.case('Robber Fly', 'flying_blocker')
        self.assertEqual(self.values(ground, 'block', ref='blocker'), [0])
        self.assertEqual(self.values(ground, 'assert_life', player=1)[-1], 19)
        self.assertEqual(self.copies(ground, 'Forest', 1, 'hand')[-1], 3)
        self.assertEqual(self.values(flyer, 'block', ref='blocker'), [1])
        self.assertEqual(self.copies(flyer, 'Storm Crow', 1, 'battlefield')[-1], 1)
        self.assertEqual(self.values(flyer, 'assert_card', ref='blocker', property='damage')[-1], 1)

    def test_mindstab_real_sacrifice_or_decline_and_smaller_hand(self):
        for owner in (0, 1):
            for mode, expected, damage, count in (('unblocked', 0, 0, 3), ('decline', 1, 2, 0),
                                                  ('short_hand', 0, 0, 2), ('empty_hand', 0, 0, 0)):
                steps = self.case('Mindstab Thrull', mode, owner)
                self.assertEqual(self.values(steps, 'choose_dialog', player=owner), [expected])
                self.assertEqual(self.values(steps, 'assert_unused_choices'), [0])
                self.assertEqual(self.copies(steps, 'Mindstab Thrull', owner, 'graveyard')[-1], 1 - expected)
                self.assertEqual(self.values(steps, 'assert_zone', player=1 - owner, zone='graveyard')[-1], 1 + count)
                self.assertEqual(self.values(steps, 'assert_life', player=1 - owner)[-1], 20 - damage)
            blocked = self.case('Mindstab Thrull', 'blocked', owner)
            self.assertEqual(self.values(blocked, 'choose_dialog', player=owner), [])
            self.assertEqual(self.copies(blocked, 'Mindstab Thrull', owner, 'battlefield')[-1], 1)

    def test_late_bounce_keeps_historical_block_without_retrigger(self):
        for name, count in (('Alley Grifters', 1), ('Abyssal Nightstalker', 0)):
            steps = self.case(name, 'late_blocker_bounced')
            after = self.between(steps, 'advance_combat', 'combat_damage')
            self.assertEqual(self.values(after, 'cast', ref='bounce'), [1])
            self.assertEqual(self.values(after, 'assert_card', ref='source', property='blocked'), [1, 1])
            self.assertEqual(self.copies(after, 'Forest', 1, 'graveyard'), [count, count])
            self.assertEqual(self.copies(steps, 'Wall of Stone', 1, 'hand')[-1], 1)
            self.assertEqual(self.values(steps, 'assert_life', player=1)[-1], 20)

    def test_source_can_block_without_attacking_or_discard(self):
        steps = self.case('Abyssal Nightstalker', 'source_blocks', 1)
        self.assertEqual(self.values(steps, 'block', ref='source', other='opposing'), [1])
        self.assertEqual(self.copies(steps, 'Forest', 0, 'graveyard')[-1], 0)
        self.assertEqual(self.copies(steps, 'Abyssal Nightstalker', 1, 'graveyard')[-1], 1)
        self.assertEqual(self.copies(steps, 'Grizzly Bears', 0, 'graveyard')[-1], 1)

    def test_real_snapshot_replays_exact_remaining_resolution(self):
        steps = self.case('Mindstab Thrull', 'pending_block_snapshot', 1)
        self.assertEqual(self.values(steps, 'choose_dialog', player=1), [0])
        heads = [s['op'] for s in steps]
        self.assertLess(heads.index('choose_dialog'), heads.index('capture'))
        self.assertEqual(self.between(steps, 'capture', 'record_state'),
                         self.between(steps, 'restore', 'assert_recorded_state'))

    def test_mindripper_shadow_is_symmetric_in_real_combat(self):
        for owner in (0, 1):
            for mode, blocker, attacker, legal in (('blocked', 'blocker', 'source', 1),
                    ('shadow_ground_denied', 'blocker', 'source', 0),
                    ('shadow_defends', 'source', 'opposing', 1), ('source_blocks', 'source', 'opposing', 0)):
                steps = self.case('Dauthi Mindripper', mode, owner)
                self.assertEqual(self.values(steps, 'block', ref=blocker, other=attacker), [legal])
                if mode != 'shadow_ground_denied':
                    self.assertFalse(self.values(steps, 'choose_dialog', player=owner))
                    self.assertEqual(self.values(steps, 'assert_zone', player=1 - owner, zone='hand')[-1], 4)
                if legal:
                    self.assertEqual(self.copies(steps, 'Dauthi Mindripper', owner, 'graveyard')[-1], 1)
                    self.assertEqual(self.copies(steps, 'Dauthi Marauder', 1 - owner, 'graveyard')[-1], 1)

    def test_mindripper_uses_real_trigger_choice_and_independent_discard(self):
        for owner in (0, 1):
            for mode, option, cards in (('shadow_ground_denied', 0, 3), ('short_hand', 0, 2),
                                         ('empty_hand', 0, 0), ('decline', 1, 0)):
                steps = self.case('Dauthi Mindripper', mode, owner)
                self.assertEqual(self.values(steps, 'choose_dialog', player=owner), [option])
                self.assertEqual(self.between(steps, 'advance_combat', 'assert_unused_choices'), [{'op': 'resolve'}])
                self.assertEqual(self.values(steps, 'assert_unused_choices'), [0])
                self.assertEqual(self.copies(steps, 'Dauthi Mindripper', owner, 'graveyard')[-1], 1 - option)
                self.assertEqual(self.values(steps, 'assert_zone', player=1 - owner, zone='graveyard')[-1], 1 + cards)
                self.assertEqual(self.values(steps, 'assert_life', player=1 - owner)[-1], 20 - 2 * option)
                self.assertEqual(self.values(steps, 'can_activate', ref='source'), [0] if option else [])

    def test_mindripper_multiple_shadow_blockers_and_late_removal(self):
        multiple = self.case('Dauthi Mindripper', 'multi_blockers')
        self.assertEqual(self.values(multiple, 'block', other='source'), [1, 1])
        self.assertEqual(self.copies(multiple, 'Dauthi Marauder', 1, 'battlefield')[-1], 1)
        self.assertEqual(self.copies(multiple, 'Soltari Foot Soldier', 1, 'battlefield')[-1], 1)
        self.assertFalse(self.values(multiple, 'choose_dialog'))
        late = self.case('Dauthi Mindripper', 'late_blocker_bounced', 1)
        self.assertEqual(self.values(late, 'assert_card', ref='source', property='blocked'), [1, 1])
        self.assertEqual(self.copies(late, 'Dauthi Marauder', 0, 'hand')[-1], 1)
        self.assertEqual(self.copies(late, 'Dauthi Mindripper', 1, 'battlefield')[-1], 1)
        self.assertFalse(self.values(late, 'choose_dialog'))

    def test_printed_costs_are_paid_and_countered_card_never_attacks(self):
        for name in profiles()['sources']:
            steps = self.case(name, 'countered', 1)
            self.assertEqual(self.values(steps, 'cast_response', ref='counter'), [1])
            self.assertEqual(self.copies(steps, name, 1, 'graveyard')[-1], 1)
            self.assertEqual(self.copies(steps, 'Counterspell', 0, 'graveyard')[-1], 1)
            self.assertFalse(any(s['op'] == 'begin_attackers' for s in steps))
            denied = self.case(name, 'wrong_color')
            self.assertEqual(self.values(denied, 'cast', ref='source'), [0])
            self.assertEqual(self.copies(denied, name, 0, 'hand')[-1], 1)
