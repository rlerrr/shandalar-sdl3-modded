"""Guard the exact Equipment damage-reward and Imprint decision model."""
from copy import deepcopy
import unittest

from equipment_combat_damage_hand_and_cast_recipes import (
    BINDER, FAMILY, KEY, MASK, SHROUD, Board, classify, profiles, spec, validate_witnesses, variants,
)


def inputs():
    pins = profiles()
    oracle = {name: pin['oracle'] for entries in pins.values() for name, pin in entries.items()}
    rows = {name: pin['registry'] for entries in pins.values() for name, pin in entries.items()}
    return oracle, rows


def scene(id_, owner, mode):
    oracle, rows = inputs()
    name, = [name for name in profiles()['sources'] if rows[name]['csvid'] == id_]
    board = Board(oracle[name], owner, mode, rows, oracle)
    board.build()
    return board


class EquipmentDamageGuards(unittest.TestCase):
    def test_whole_oracle_and_typed_schema_reject_drift(self):
        oracle, _ = inputs()
        record = oracle['Spellbinder']
        shape = classify(record, BINDER)
        declaration = dict(family=FAMILY, version=2, params=shape, oracle_text=record['oracle_text'],
                           variants=variants(shape))
        self.assertEqual(spec(declaration, record)['csvid'], BINDER)
        for bad in (dict(declaration, version=2.0), dict(declaration, variants=['decline_copy_1']),
                    dict(declaration, params=dict(shape, equip_generic=4.0))):
            with self.assertRaises(ValueError):
                spec(bad, record)
        with self.assertRaises(ValueError):
            spec(declaration, dict(record, oracle_text=record['oracle_text'].replace('instant', 'sorcery')))

    def test_library_rows_keep_real_player_and_zone(self):
        for owner in (0, 1):
            setup = scene(MASK, owner, 'hit').setup
            self.assertEqual([(row['player'], row['zone']) for row in setup if row['ref'].startswith('library_')],
                             [(owner, 'library')] * 3)
            self.assertEqual(next(row['player'] for row in setup if row['ref'] == 'foe_top'), 1 - owner)
            self.assertTrue(all(row['player'] in (0, 1) for row in setup))

    def test_whole_witnesses_and_registered_identity(self):
        oracle, rows = inputs()
        validate_witnesses(oracle, rows)
        changed = deepcopy(rows)
        changed['Guttersnipe']['iid'] += 1
        with self.assertRaises(ValueError):
            validate_witnesses(oracle, changed)

    def test_paid_equip_and_short_fee_use_real_activation(self):
        for owner in (0, 1):
            game = scene(BINDER, owner, 'hit')
            actions = [step for step in game.steps if step['op'] in ('cast', 'activate')]
            self.assertEqual([(step['op'], step['ref']) for step in actions],
                             [('cast', 'source'), ('activate', 'source')])
            self.assertEqual(game.mana[owner], [0] * 6)
            short = scene(BINDER, owner, 'short_equip')
            self.assertFalse(any(step['op'] == 'activate' for step in short.steps))
            self.assertEqual(short.mana[owner][0], 3)

    def test_optional_mask_loot_and_shroud_mandatory_recipient(self):
        mask = scene(MASK, 0, 'selected_discard')
        self.assertEqual(mask.cards['library_0'], ['Plains', 0, 'hand'])
        self.assertEqual(mask.cards['library_1'], ['Island', 0, 'graveyard'])
        for owner in (0, 1):
            self.assertEqual(scene(SHROUD, owner, 'hit').cards['discardable'][2], 'graveyard')
            self.assertEqual(scene(SHROUD, owner, 'noncombat').cards['discardable'][2], 'hand')
            self.assertEqual(scene(MASK, owner, 'blocked').cards['library_0'][2], 'library')
        self.assertEqual(scene(MASK, 0, 'decline').cards['library_0'][2], 'library')

    def test_spellbinder_imprint_filter_repeat_and_actual_cast_rider(self):
        for owner in (0, 1):
            hit = scene(BINDER, owner, 'repeat')
            self.assertEqual(hit.cards['imprinted'][2], 'exile')
            self.assertEqual(hit.cards['rejected'][2], 'hand')
            self.assertEqual([hit.cards[ref][2] for ref in ('library_0', 'library_1', 'library_2')],
                             ['hand', 'hand', 'library'])
            self.assertEqual(scene(BINDER, owner, 'cast_trigger').life[1-owner], 16)
            self.assertEqual(scene(BINDER, owner, 'wrong_type').cards['library_0'][2], 'library')
        declined = scene(BINDER, 0, 'decline_imprint')
        self.assertEqual(declined.cards['imprinted'][2], 'hand')
        self.assertEqual(scene(BINDER, 0, 'decline_copy').cards['library_0'][2], 'library')

    def test_key_skulk_has_real_block_attempt_and_combat_only_loot(self):
        for owner in (0, 1):
            skulk = scene(KEY, owner, 'skulk')
            block, = [step for step in skulk.steps if step['op'] == 'block']
            self.assertEqual((block['ref'], block['other'], block['value']), ('blocker', 'host', 0))
            self.assertEqual(skulk.life[1-owner], 18)
            self.assertEqual(skulk.cards['library_0'][2], 'graveyard')
            self.assertEqual(scene(KEY, owner, 'noncombat').cards['library_0'][2], 'library')

    def test_shroud_boost_kills_wall_without_player_hit_or_discard(self):
        for owner in (0, 1):
            shroud = scene(SHROUD, owner, 'blocked')
            self.assertEqual(shroud.host_stats(), (3, 2))
            self.assertEqual(shroud.cards['blocker'][2], 'graveyard')
            self.assertEqual(shroud.cards['discardable'][2], 'hand')
            self.assertEqual(shroud.life, [20, 20])
            self.assertEqual(scene(MASK, owner, 'blocked').cards['blocker'][2], 'battlefield')
