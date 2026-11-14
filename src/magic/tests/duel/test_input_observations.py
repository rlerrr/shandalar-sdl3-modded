"""Opt-in input observations require explicit intent and bounded receipts."""
import unittest

from generate_cases import (REF_OPS, validate_named_choice, validate_forced_target_observation,
                            validate_activation_mode_choice, validate_activation_mode_observation)


class InputObservationContractTests(unittest.TestCase):
    def test_mode_intent_requires_a_source_label_and_explicit_auto_policy(self):
        action=dict(op='choose_activation_mode',ref='source',player=0,mode_label='Draw',
                    fallback_color=0,allow_auto_nonmana=True)
        validate_activation_mode_choice(action)
        for field,bad in (('ref',''),('player',True),('mode_label',''),('mode_label','Draw\nMana'),
                          ('mode_label','x'*128),('fallback_color',7),('fallback_color',False),
                          ('allow_auto_nonmana',1),('value',0)):
            changed=dict(action);changed[field]=bad
            with self.subTest(field=field,bad=bad),self.assertRaises(ValueError):
                validate_activation_mode_choice(changed)
        for value in (0,1):validate_activation_mode_observation(dict(op='require_activation_mode',value=value))
        for value in (True,-1,2,1.0):
            with self.assertRaises(ValueError):validate_activation_mode_observation(dict(op='require_activation_mode',value=value))

    def test_named_choices_require_existing_references_and_explicit_players(self):
        for op in ('choose_trigger_source', 'choose_optional_target'):
            self.assertIn(op, REF_OPS)
            for player in (0, 1):
                validate_named_choice(dict(op=op, ref='source', player=player))
            for field, bad in (('player', True), ('player', -1), ('player', 2),
                               ('player', '0'), ('ref', ''), ('ref', 0), ('index', 1), ('value', 0)):
                action = dict(op=op, ref='source', player=0)
                action[field] = bad
                with self.subTest(op=op, field=field, bad=bad), self.assertRaises(ValueError):
                    validate_named_choice(action)
            for field in ('ref', 'player'):
                action = dict(op=op, ref='source', player=0)
                del action[field]
                with self.assertRaises(ValueError):
                    validate_named_choice(action)

    def test_forced_target_receipts_are_bounded_counts_without_state_fields(self):
        for value in (0, 1, 2147483647):
            validate_forced_target_observation(dict(op='assert_forced_target_choices', value=value))
        for value in (True, -1, 2147483648, 1.0, '1', None):
            with self.subTest(value=value), self.assertRaises(ValueError):
                validate_forced_target_observation(dict(op='assert_forced_target_choices', value=value))
        for field in ('player', 'ref', 'index'):
            with self.assertRaises(ValueError):
                validate_forced_target_observation(dict(op='assert_forced_target_choices', value=0, **{field:0}))


if __name__ == '__main__':
    unittest.main()
