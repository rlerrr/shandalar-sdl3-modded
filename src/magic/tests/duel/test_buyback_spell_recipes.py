"""Pinned rules and real buyback lifecycle guards."""

import json
import unittest

from generate_cases import registry
import buyback_spell_recipes as model


class BuybackSpellTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.oracle = json.loads((model.PROFILE_PATH.parent / 'oracle.json').read_text(encoding='utf8'))['cards']
        cls.rows = {row['name']: row for row in registry()}

    def case(self, name, variant):
        record = self.oracle[name]
        declaration = dict(family=model.FAMILY, version=2, oracle_text=record['oracle_text'],
                           params=model.parameters(record), variants=[variant])
        return model.expand(name, self.rows[name]['csvid'], declaration, model.spec(declaration, record),
                            declaration['variants'], self.rows, self.oracle)[0]['steps']

    @staticmethod
    def values(steps, operation, **fields):
        return [s['value'] for s in steps if s['op'] == operation and all(s.get(k) == v for k, v in fields.items())]

    def test_complete_clauses_and_witnesses(self):
        names = {'Allay': '{3}', 'Capsize': '{3}', 'Elvish Fury': '{4}', "Evincar's Justice": '{3}',
                 'Lab Rats': '{4}', 'Mind Peel': '{2}{B}{B}', 'Mystic Speculation': '{2}', 'Whispers of the Muse': '{5}', 'Shattering Pulse': '{3}',
                 'Seething Anger': '{3}'}
        self.assertEqual(set(model.profiles()['sources']), names.keys())
        model.validate_witnesses(self.oracle, self.rows)
        for name, fee in names.items():
            record = self.oracle[name]
            self.assertEqual(model.parameters(record)['fee'], fee)
            with self.assertRaises(ValueError):
                model.parameters(dict(record, oracle_text=record['oracle_text'] + '\nDraw a card.'))
            with self.assertRaises(ValueError):
                model.parameters(dict(record, oracle_text=record['oracle_text'].replace('additional ', '')))

    def test_target_before_menu_and_exact_short_colored_fee(self):
        for owner in (0, 1):
            steps = self.case('Mind Peel', f'paid_{owner}')
            cast = steps.index(dict(op='cast', ref='source', value=1))
            self.assertEqual(steps[cast - 2:cast], [dict(op='choose_player', player=owner, value=1-owner),
                                                  dict(op='choose_dialog', player=owner, value=0)])
            self.assertEqual(self.values(steps, 'assert_mana', player=owner, color=1), [0, 0])
            short = self.case('Mind Peel', f'short_color_{owner}')
            first = short.index(dict(op='cast', ref='source', value=1))
            self.assertFalse(any(step['op'] == 'choose_dialog' for step in short[:first]))
            self.assertEqual(self.values(short, 'assert_mana', player=owner, color=0), [4, 4])

    def test_seething_anger_boost_changes_only_power_and_expires(self):
        for owner in (0, 1):
            case = self.case('Seething Anger', f'expiry_{owner}')
            self.assertEqual(self.values(case, 'assert_card', ref='target_0', property='power'), [6, 3])
            self.assertEqual(self.values(case, 'assert_card', ref='target_0', property='toughness'), [3, 3])
            self.assertEqual(self.values(case, 'bind_hand', ref='source', player=owner), [1433])

    def test_paid_spell_does_not_return_if_countered_or_fizzled(self):
        for variant in ('countered_0', 'fizzle_1'):
            owner = int(variant[-1])
            steps = self.case('Capsize', variant)
            self.assertIn(dict(op='choose_dialog', player=owner, value=0), steps)
            ops = [s['op'] for s in steps]
            self.assertLess(ops.index('announce_spell'), ops.index('cast_response'))
            self.assertLess(ops.index('cast_response'), ops.index('resolve_response'))
            self.assertFalse(any(s['op'] == 'bind_hand' and s['ref'] == 'source' for s in steps))
            self.assertEqual(self.values(steps, 'assert_zone_multiplicity', player=owner, zone='graveyard', index=1396), [1])

    def test_recast_uses_real_return_and_new_target(self):
        steps = self.case('Allay', 'recast_0')
        self.assertEqual(self.values(steps, 'choose_dialog', player=0), [0, 1])
        self.assertEqual([s['ref'] for s in steps if s['op'] == 'choose_target'], ['target_0', 'target_1'])
        self.assertEqual(self.values(steps, 'cast', ref='source'), [1, 1])
        self.assertEqual(self.values(steps, 'bind_hand', ref='source', player=0), [1426])
        self.assertEqual(self.values(steps, 'assert_zone_multiplicity', player=0, zone='graveyard', index=1426), [0, 1])
        repeats = self.case('Whispers of the Muse', 'repeat_paid_1')
        self.assertEqual(self.values(repeats, 'bind_hand', ref='source', player=1), [1436, 1436])
        self.assertEqual(self.values(repeats, 'assert_zone', player=1, zone='library'), [6, 5])

    def test_rats_and_all_creatures_players_have_independent_receipts(self):
        rats = self.case('Lab Rats', 'repeat_paid_0')
        self.assertEqual(self.values(rats, 'assert_zone_multiplicity', player=0, zone='battlefield', index=18806), [1, 2])
        self.assertEqual(self.values(rats, 'assert_matching_permanents', player=0, index=18806, property='token'), [1, 1])
        subtype = self.case('Lab Rats', 'token_subtype_1')
        self.assertEqual(self.values(subtype, 'assert_card', ref='pack', property='power'), [1, 2])
        self.assertEqual(self.values(subtype, 'assert_card', ref='non_rat', property='power'), [2])
        damage = self.case("Evincar's Justice", 'repeat_paid_1')
        for controller in (0, 1):
            self.assertEqual(self.values(damage, 'assert_life', player=controller), [18, 16])
            self.assertEqual(self.values(damage, 'assert_card', ref=f'bear_{controller}', property='in_play'), [0, 0])
            self.assertEqual(self.values(damage, 'assert_card', ref=f'giant_{controller}', property='in_play'), [1, 0])

    def test_scry_real_library_choices_and_no_draw(self):
        for owner in (0, 1):
            steps = self.case('Mystic Speculation', f'paid_{owner}')
            self.assertEqual(self.values(steps, 'choose_zone_card', player=owner), [260, 48, 145])
            self.assertEqual(self.values(steps, 'assert_scry_result', player=owner), [3])
            self.assertEqual(self.values(steps, 'assert_zone', player=owner, zone='library'), [7])
            self.assertEqual(self.values(steps, 'assert_zone_card', player=owner, zone='library', index=0), [9395])
            empty = self.case('Mystic Speculation', f'scry_empty_{owner}')
            self.assertEqual(self.values(empty, 'choose_zone_card', player=owner), [])
            self.assertEqual(self.values(empty, 'assert_zone', player=owner, zone='library'), [0])

    def test_retained_choice_snapshot_follows_payment_and_replays_only_resolution(self):
        steps = self.case('Lab Rats', 'pending_replay_1')
        ops = [s['op'] for s in steps]
        self.assertLess(ops.index('cast'), ops.index('capture'))
        self.assertLess(ops.index('capture'), ops.index('resolve'))
        self.assertEqual(ops.count('cast'), 1)
        self.assertEqual(ops.count('resolve'), 2)
        self.assertEqual(ops[-1], 'assert_recorded_state')


    def test_foreign_owner_rebinds_live_slot_and_returns_to_actual_owner(self):
        for controller in (0, 1):
            steps = self.case('Capsize', f'foreign_owner_{controller}')
            theft = steps.index(dict(op='cast', ref='steal', value=1))
            rebind = steps.index(dict(op='bind_permanent', ref='target_0', player=controller, value=110))
            second_cast = steps.index(dict(op='cast', ref='source', value=1))
            self.assertLess(theft, rebind)
            self.assertEqual(steps[rebind - 2:rebind], [dict(op='assert_zone', player=controller, zone='battlefield', value=1),
                                                         dict(op='assert_zone', player=1-controller, zone='battlefield', value=0)])
            self.assertIn(dict(op='choose_target', player=controller, ref='target_0'), steps[rebind:second_cast])
            self.assertEqual(self.values(steps, 'bind_hand', ref='target_0', player=1-controller), [110])
            self.assertEqual(self.values(steps, 'bind_hand', ref='source', player=controller), [1396])


    def test_allay_low_devotion_enchantment_survives_without_creature_stats(self):
        for owner in (0, 1):
            steps = self.case('Allay', f'indestructible_{owner}')
            self.assertEqual(self.values(steps, 'assert_card', ref='target_0', property='in_play'), [1])
            self.assertEqual(self.values(steps, 'assert_card', ref='target_0', property='power'), [])
            self.assertEqual(self.values(steps, 'assert_card', ref='target_0', property='toughness'), [])
            self.assertEqual(self.values(steps, 'can_target_creature', ref='source', other='target_0'), [0])
            self.assertEqual(self.values(steps, 'bind_hand', ref='source', player=owner), [1426])
            self.assertEqual(self.values(steps, 'assert_zone_multiplicity', player=1-owner, zone='graveyard', index=13572), [0])


    def test_artifact_scope_destroy_indestructible_and_late_fizzle(self):
        for owner in (0, 1):
            wrong = self.case('Shattering Pulse', f'wrong_type_{owner}')
            self.assertEqual(self.values(wrong, 'can_cast', ref='source'), [0])
            self.assertEqual(self.values(wrong, 'cast', ref='source'), [0])
            self.assertFalse(any(step['op'] == 'choose_target' for step in wrong))
            paid = self.case('Shattering Pulse', f'paid_{owner}')
            self.assertEqual(self.values(paid, 'assert_card', ref='target_0', property='in_play'), [0])
            self.assertEqual(self.values(paid, 'bind_hand', ref='source', player=owner), [1434])
            indestructible = self.case('Shattering Pulse', f'indestructible_{owner}')
            self.assertEqual(self.values(indestructible, 'assert_card', ref='target_0', property='in_play'), [1])
            self.assertEqual(self.values(indestructible, 'bind_hand', ref='source', player=owner), [1434])
            fizzle = self.case('Shattering Pulse', f'fizzle_{owner}')
            self.assertIn(dict(op='cast_response', ref='reply', value=1), fizzle)
            self.assertEqual(self.values(fizzle, 'assert_zone_multiplicity', player=owner, zone='graveyard', index=1434), [1])


if __name__ == '__main__':
    unittest.main()
