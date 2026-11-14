"""Mana payment sources and granted abilities cannot override native actors."""
import unittest

from generate_cases import (OPS, REF_OPS, validate_granted_mana_action,
                            validate_mana_source_choice)


class LiveManaInputContractTests(unittest.TestCase):
    def test_live_source_choice_has_no_pool_or_actor_override(self):
        self.assertIn('choose_mana_source', OPS & REF_OPS)
        validate_mana_source_choice(dict(op='choose_mana_source', ref='real_forest'))
        for key, value in (('player', 1), ('color', 3), ('value', -3),
                           ('index', 0), ('other', 'opponent_land')):
            with self.subTest(key=key), self.assertRaises(ValueError):
                validate_mana_source_choice(dict(op='choose_mana_source', ref='real_forest', **{key: value}))

    def test_live_source_choice_requires_a_reference(self):
        for ref in ('', None, 0, True, [], {}):
            with self.subTest(ref=ref), self.assertRaises(ValueError):
                validate_mana_source_choice(dict(op='choose_mana_source', ref=ref))
        with self.assertRaises(ValueError):
            validate_mana_source_choice(dict(op='choose_mana_source'))
        with self.assertRaises(ValueError):
            validate_mana_source_choice(dict(op='choose_mana', ref='real_forest'))

    def test_granted_mana_pair_keeps_host_and_provider_distinct_from_actor_fields(self):
        for op in ('can_activate_granted_mana', 'activate_granted_mana'):
            self.assertIn(op, OPS & REF_OPS)
            for value in (0, 1):
                validate_granted_mana_action(dict(op=op, ref='actual_host', other='attached_aura', value=value))
            for key in ('player', 'payer', 'mana_receiver', 'color', 'mode_label'):
                with self.subTest(op=op, key=key), self.assertRaises(ValueError):
                    validate_granted_mana_action(dict(op=op, ref='actual_host', other='attached_aura', value=1, **{key: 0}))

    def test_granted_mana_receipts_and_references_are_strict(self):
        good = dict(op='activate_granted_mana', ref='actual_host', other='attached_aura', value=1)
        for field, bad in (('value', True), ('value', 1.0), ('value', -1), ('value', 2),
                           ('ref', ''), ('ref', []), ('other', None), ('other', 0), ('op', 'activate')):
            with self.subTest(field=field, bad=bad), self.assertRaises(ValueError):
                validate_granted_mana_action(dict(good, **{field: bad}))
        for field in ('ref', 'other', 'value'):
            missing = dict(good)
            del missing[field]
            with self.subTest(missing=field), self.assertRaises(ValueError):
                validate_granted_mana_action(missing)


if __name__ == '__main__':
    unittest.main()
