"""Guards for pinned targeted kicker timing and payments."""

import unittest

import kicker_targeted_entry_recipes as kicker


class TargetedKickerGuard(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        pins = kicker.profiles()
        cards = pins['sources'] | pins['witnesses']
        cls.oracle = {name: pin['oracle'] for name, pin in cards.items()}
        cls.registry = {name: pin['registry'] for name, pin in cards.items()}

    def declaration(self, name, *variants):
        record = self.oracle[name]
        return dict(family=kicker.FAMILY, version=2, params=kicker.classify(record),
                    oracle_text=record['oracle_text'], variants=list(variants))

    def actions(self, name, variant):
        decl = self.declaration(name, variant)
        shape = kicker.spec(decl, self.oracle[name])
        csvid = self.registry[name]['csvid']
        return kicker.expand(name, csvid, decl, shape, [variant], self.registry, self.oracle)[0]['steps']

    def test_rejects_unreviewed_rules_and_payment_metadata(self):
        for name in kicker.profiles()['sources']:
            record = self.oracle[name]
            declaration = self.declaration(name, 'paid_0')
            with self.assertRaises(ValueError):
                kicker.classify({**record, 'oracle_text': record['oracle_text'] + '\nDraw a card.'})
            with self.assertRaises(ValueError):
                kicker.spec({**declaration, 'variants': ['paid_0', 'paid_0']}, record)
            wrong = dict(declaration['params'], kicker=[False] * 6)
            with self.assertRaises(ValueError):
                kicker.spec({**declaration, 'params': wrong}, record)

    def test_paid_target_is_entry_input_and_absent_on_real_counter(self):
        for name in kicker.profiles()['sources']:
            for seat in (0, 1):
                paid = self.actions(name, f'paid_{seat}')
                cast = next(i for i, action in enumerate(paid) if action['op'] == 'cast')
                target = next(i for i, action in enumerate(paid) if action['op'] == 'choose_target')
                resolve = next(i for i, action in enumerate(paid) if action['op'] == 'resolve')
                self.assertLess(cast, target)
                self.assertLess(target, resolve)
                countered = self.actions(name, f'countered_{seat}')
                self.assertFalse(any(action['op'] in ('choose_target', 'choose_trigger') for action in countered))
                ordered = [a['op'] for a in countered if a['op'] in ('cast', 'announce_spell', 'cast_response', 'resolve_response')]
                self.assertEqual(ordered, ['cast', 'announce_spell', 'cast_response', 'resolve_response'])
