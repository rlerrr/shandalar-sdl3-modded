import json
import unittest
from pathlib import Path

from generate_cases import registry
from morph_alternate_payments_recipes import FAMILY, classify, expand, profiles, spec, validate_witnesses, variants
from morph_creature_recipes import FACE_DOWN


class MorphAlternatePaymentsTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.oracle = json.loads((Path(__file__).parent / 'data/oracle.json').read_text(encoding='utf8'))['cards']
        cls.rows = {card['name']: card for card in registry()}

    def case(self, card, scenario, owner=0):
        oracle = self.oracle[card]
        selected = [f'{scenario}_{owner}']
        declaration = dict(family=FAMILY, version=2, params=classify(oracle), oracle_text=oracle['oracle_text'],
                           variants=selected)
        case, = expand(card, self.rows[card]['csvid'], declaration, spec(declaration, oracle), selected,
                       self.rows, self.oracle)
        self.assertFalse(any(set(item) - {'name', 'ref', 'player', 'zone'} for item in case['setup']))
        self.assertFalse(any(step['op'] in ('destroy', 'move', 'draw', 'discard') for step in case['steps']))
        return case['steps']

    def actions(self, steps, op, **fields):
        return [step for step in steps if step['op'] == op and all(step.get(key) == value for key, value in fields.items())]

    def values(self, steps, op, **fields):
        return [step['value'] for step in self.actions(steps, op, **fields)]

    def copies(self, steps, name, player, zone):
        return self.values(steps, 'assert_zone_multiplicity', player=player, zone=zone, index=self.rows[name]['csvid'])

    def variants_for(self, name):
        return variants(classify(self.oracle[name]))

    def test_whole_typed_printed_cost_and_companions(self):
        validate_witnesses(self.oracle, self.rows)
        self.assertEqual({self.rows[name]['csvid'] for name in profiles()['sources']},
                         {8541, 8931, 6563, 14357, 14404, 14427, 6609, 14288})
        for name in profiles()['sources']:
            oracle = self.oracle[name]
            params = classify(oracle)
            declaration = dict(family=FAMILY, version=2, params=params, oracle_text=oracle['oracle_text'],
                               variants=self.variants_for(name))
            self.assertEqual(spec(declaration, oracle), dict(params, family=FAMILY))
            with self.assertRaises(ValueError):
                spec(dict(declaration, params=dict(params, power=float(params['power']))), oracle)
            with self.assertRaises(ValueError):
                classify(dict(oracle, oracle_text=oracle['oracle_text'] + '\nFlying'))

    def test_real_facedown_and_printed_cast_have_distinct_characteristics(self):
        for name in profiles()['sources']:
            for test, identity in (('hidden', FACE_DOWN), ('printed', self.rows[name]['csvid'])):
                steps = self.case(name, test)
                self.assertEqual(self.values(steps, 'cast', ref='source'), [1])
                self.assertFalse(self.actions(steps, 'activate'))
                self.assertEqual(self.values(steps, 'assert_card', ref='source', property='csvid')[-1], identity)
            denied = self.case(name, 'cast_short')
            self.assertEqual(self.values(denied, 'cast', ref='source'), [0])
            self.assertEqual(self.copies(denied, name, 0, 'hand')[-1], 1)

    def test_fathom_exact_islands_return_draw_and_scope(self):
        for owner in (0, 1):
            steps = self.case('Fathom Seer', 'paid', owner)
            self.assertEqual([step['ref'] for step in self.actions(steps, 'choose_target', player=owner)],
                             ['island', 'dual_island'])
            for name in ('Island', 'Tropical Island', 'Mountain', 'Plains'):
                self.assertEqual(self.copies(steps, name, owner, 'hand')[-1], 1)
            self.assertEqual(self.copies(steps, 'Island', 1-owner, 'battlefield')[-1], 1)
            self.assertEqual(self.copies(steps, 'Forest', owner, 'battlefield')[-1], 1)
            self.assertEqual(self.values(steps, 'assert_zone', player=owner, zone='library')[-1], 1)
            wrong = self.case('Fathom Seer', 'denied', owner)
            self.assertEqual(self.values(wrong, 'activate', ref='source'), [0])
            self.assertFalse(self.actions(wrong, 'choose_target'))

    def test_fathom_cast_routes_when_real_lands_can_fund_both(self):
        for owner in (0, 1):
            modes = [('paid', [0]), ('printed', [1]), ('cast_short', [])]
            if owner == 0:
                modes.append(('cancel', [0]))
            for mode, expected in modes:
                steps = self.case('Fathom Seer', mode, owner)
                cast, = self.actions(steps, 'cast', ref='source')
                self.assertEqual(self.values(steps[:steps.index(cast)], 'choose_dialog', player=owner), expected)
                if mode == 'cast_short':
                    self.assertEqual(self.values(steps, 'assert_zone', player=owner, zone='battlefield')[-1], 0)
                    self.assertEqual(self.copies(steps, 'Island', 1-owner, 'battlefield')[-1], 1)

    def test_cancel_cost_preserves_hidden_identity_and_payment(self):
        for name in ('Fathom Seer', 'Putrid Raptor', 'Ruthless Ripper', 'Temur Charger', 'Watcher of the Roost'):
            steps = self.case(name, 'cancel')
            self.assertEqual(self.values(steps, 'can_activate', ref='source'), [1])
            self.assertEqual(self.values(steps, 'activate', ref='source'), [0])
            self.assertEqual(self.values(steps, 'assert_card', ref='source', property='csvid')[-1], FACE_DOWN)
            self.assertEqual(self.values(steps, 'assert_zone', player=0, zone='graveyard')[-1], 0)
            self.assertEqual(self.values(steps, 'assert_zone', player=0, zone='library')[-1], 3)
            self.assertEqual(self.values(steps, 'assert_life', player=0)[-1], 20)

    def test_discard_cost_and_dynamic_hellbent(self):
        for owner in (0, 1):
            paid = self.case('Gathan Raiders', 'paid', owner)
            self.assertEqual(self.copies(paid, 'Forest', owner, 'graveyard')[-1], 1)
            self.assertEqual(self.values(paid, 'assert_card', ref='source', property='power')[-1], 5)
            not_empty = self.case('Gathan Raiders', 'hellbent_nonempty', owner)
            self.assertEqual(self.values(not_empty, 'assert_card', ref='source', property='power')[-1], 3)
            changed = self.case('Gathan Raiders', 'hellbent_changes', owner)
            self.assertTrue(self.actions(changed, 'cast', ref='bounce', value=1))
            self.assertEqual(self.values(changed, 'assert_card', ref='source', property='power')[-2:], [5, 3])

    def test_zombie_and_reveal_validate_actual_offers_and_exact_zones(self):
        cards = (('Putrid Raptor', 'Scathe Zombies'), ('Ruthless Ripper', 'Dark Ritual'),
                 ('Temur Charger', 'Giant Growth'), ('Watcher of the Roost', 'Healing Salve'))
        for owner in (0, 1):
            for name, payment in cards:
                steps = self.case(name, 'paid', owner)
                payment_id = self.rows[payment]['csvid']
                self.assertEqual(self.values(steps, 'choose_zone_card', player=owner), [payment_id])
                self.assertEqual(self.values(steps, 'assert_offered_legal_card', index=payment_id), [1])
                self.assertEqual(self.values(steps, 'assert_offered_legal_card', index=self.rows['Mountain']['csvid']), [0])
                self.assertEqual(self.copies(steps, payment, owner, 'graveyard')[-1], int(name == 'Putrid Raptor'))
                self.assertEqual(self.copies(steps, payment, owner, 'hand')[-1], int(name != 'Putrid Raptor'))
                wrong = self.case(name, 'denied', owner)
                self.assertEqual(self.values(wrong, 'activate', ref='source'), [0])

    def test_real_ripper_target_life_and_deathtouch(self):
        for owner in (0, 1):
            flipped = self.case('Ruthless Ripper', 'self_loss', owner)
            self.assertEqual(self.values(flipped, 'choose_player', player=owner), [owner])
            self.assertEqual(self.values(flipped, 'assert_life', player=owner)[-1], 18)
            combat = self.case('Ruthless Ripper', 'deathtouch_combat', owner)
            self.assertEqual(self.values(combat, 'choose_player', player=owner), [1-owner])
            self.assertEqual(self.values(combat, 'block', ref='blocker'), [1])
            self.assertEqual(self.copies(combat, 'Hill Giant', 1-owner, 'graveyard')[-1], 1)
            self.assertEqual(self.copies(combat, 'Ruthless Ripper', owner, 'graveyard')[-1], 1)

    def test_temur_real_trample_damage_self_target_and_expiry(self):
        for owner in (0, 1):
            for mode, target, life in (('trample_hit', 'ally', 18), ('trample_expiry', 'ally', 20),
                                       ('self_trample', 'source', 18)):
                steps = self.case('Temur Charger', mode, owner)
                self.assertEqual([action['ref'] for action in self.actions(steps, 'choose_target')], [target])
                self.assertEqual(self.values(steps, 'attack', ref=target), [1])
                self.assertEqual(self.values(steps, 'assert_life', player=1-owner)[-1], life)

    def test_watcher_gain_and_actual_flying_ground_or_reach(self):
        for owner in (0, 1):
            for mode, allowed in (('flying_evasion', 0), ('reachable', 1)):
                steps = self.case('Watcher of the Roost', mode, owner)
                self.assertEqual(self.values(steps, 'block', ref='blocker'), [allowed])
                self.assertEqual(self.values(steps, 'assert_life', player=owner)[-1], 22)
                self.assertEqual(self.values(steps, 'assert_life', player=1-owner)[-1], 18 + 2*allowed)

    def test_cutthroat_real_life_cost_and_insufficient_threshold(self):
        for owner in (0, 1):
            for mode, initial, expected, allowed in (('paid', [], 15, 1), ('denied', [4], 4, 0), ('exact_life', [5], 0, 1)):
                steps = self.case('Zombie Cutthroat', mode, owner)
                self.assertEqual(self.values(steps, 'set_life', player=owner), initial)
                self.assertEqual(self.values(steps, 'activate', ref='source'), [allowed])
                self.assertEqual(self.values(steps, 'assert_life', player=owner)[-1], expected)

    def test_horde_actual_reveal_and_selected_preblock_denial_expire(self):
        for owner in (0, 1):
            for mode, target, can_block in (('deny_block', 'blocker', 0), ('block_expiry', 'blocker', 1),
                                            ('untargeted_block', 'ally', 1)):
                steps = self.case('Horde Ambusher', mode, owner)
                self.assertEqual(self.values(steps, 'choose_zone_card', player=owner), [self.rows['Shock']['csvid']])
                choice, = self.actions(steps, 'choose_target', ref=target, player=owner)
                block, = self.actions(steps, 'block', ref='blocker', other='source')
                self.assertLess(steps.index(choice), steps.index(block))
                self.assertEqual(block['value'], can_block)
                self.assertEqual(self.values(steps, 'assert_life', player=1-owner)[-1], 18 + 2*can_block)
                self.assertEqual(self.copies(steps, 'Shock', owner, 'hand')[-1], 1)

    def test_horde_actual_block_damage_is_preventable(self):
        for owner in (0, 1):
            damaged = self.case('Horde Ambusher', 'blocks_self_damage', owner)
            shielded = self.case('Horde Ambusher', 'blocks_prevented', owner)
            for steps in (damaged, shielded):
                self.assertEqual(self.values(steps, 'block', ref='source', other='opposing'), [1])
                self.assertEqual(self.values(steps, 'assert_card', ref='opposing', property='damage')[-1], 2)
            self.assertEqual(self.values(damaged, 'assert_life', player=owner)[-1], 19)
            self.assertEqual(self.copies(damaged, 'Horde Ambusher', owner, 'graveyard')[-1], 1)
            self.assertEqual(self.values(shielded, 'cast', ref='shield'), [1])
            self.assertEqual(self.values(shielded, 'assert_life', player=owner)[-1], 20)
            self.assertEqual(self.copies(shielded, 'Horde Ambusher', owner, 'battlefield')[-1], 1)

    def test_pending_real_spell_and_exact_replay(self):
        for name in profiles()['sources']:
            steps = self.case(name, 'pending_spell', 1)
            cast, = self.actions(steps, 'cast', ref='shock')
            flip, = self.actions(steps, 'activate', ref='source')
            resolve, = self.actions(steps, 'resolve_response')
            self.assertLess(steps.index(cast), steps.index(flip))
            self.assertLess(steps.index(flip), steps.index(resolve))
            self.assertEqual(self.copies(steps, 'Shock', 0, 'graveyard')[-1], 1)
        replay = self.case('Ruthless Ripper', 'replay')
        ops = [step['op'] for step in replay]
        self.assertEqual(replay[ops.index('capture')+1:ops.index('record_state')],
                         replay[ops.index('restore')+1:ops.index('assert_recorded_state')])
