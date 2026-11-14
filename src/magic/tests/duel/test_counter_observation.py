"""Counter receipts are explicit, bounded, and cannot turn into state setters."""
import json
from pathlib import Path
import unittest

from generate_cases import COUNTER_KINDS, REF_OPS, validate_counter_observation


class CounterObservationContractTests(unittest.TestCase):
    def test_reviewed_kinds_support_zero_and_positive_receipts(self):
        self.assertIn('assert_counter', REF_OPS)
        self.assertIn('growth', COUNTER_KINDS)
        for kind in COUNTER_KINDS:
            for value in (0, 1, 2147483647):
                validate_counter_observation(dict(op='assert_counter', ref='source', counter_kind=kind, value=value))

    def test_invalid_receipts_fail_closed(self):
        for field, bad in (('ref', ''), ('ref', 0), ('counter_kind', 'energy'),
                           ('counter_kind', 123), ('counter_kind', []),
                           ('value', True), ('value', -1), ('value', 1.0),
                           ('value', 2147483648), ('player', 0), ('property', 'charge')):
            receipt = dict(op='assert_counter', ref='source', counter_kind='charge', value=3)
            receipt[field] = bad
            with self.subTest(field=field, bad=bad), self.assertRaises(ValueError):
                validate_counter_observation(receipt)
        for missing in ('ref', 'counter_kind', 'value'):
            receipt = dict(op='assert_counter', ref='source', counter_kind='charge', value=3)
            del receipt[missing]
            with self.assertRaises(ValueError):
                validate_counter_observation(receipt)

    def test_runtime_guard_costs_remain_pinned_to_oracle(self):
        cards = json.loads((Path(__file__).parent/'data/oracle.json').read_text(encoding='utf8'))['cards']
        horn = cards['Shriekhorn']
        self.assertEqual(horn['mana_cost'], '{1}')
        self.assertEqual(horn['oracle_text'], 'This artifact enters with three charge counters on it.\n{T}, Remove a charge counter from this artifact: Target player mills two cards.')
        jace = cards['Jace Beleren']
        self.assertEqual(jace['mana_cost'], '{1}{U}{U}')
        self.assertEqual(jace['loyalty'], '3')
        self.assertIn('+2: Each player draws a card.', jace['oracle_text'])
        ascendancy = cards['Simic Ascendancy']
        self.assertEqual(ascendancy['mana_cost'], '{G}{U}')
        self.assertIn('put that many growth counters on this enchantment.', ascendancy['oracle_text'])


if __name__ == '__main__':
    unittest.main()
