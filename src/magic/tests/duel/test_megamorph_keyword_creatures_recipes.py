"""Guard complete printed Megamorph and actual combat/payment receipts."""
import copy
import json
import unittest
from pathlib import Path

import megamorph_keyword_creatures_recipes as model
from generate_cases import registry


class MegamorphKeywordCreatureRecipesTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        data = Path(__file__).parent / 'data'
        cls.oracle = json.loads((data / 'oracle.json').read_text(encoding='utf8'))['cards']
        cls.rows = {row['name']: row for row in registry()}
        cls.declarations = {}
        for name, pin in model.pins()['sources'].items():
            path, = (data / 'cases/cards').glob(f"*/{pin['csvid']:05d}-*.json")
            cls.declarations[name] = json.loads(path.read_text(encoding='utf8'))['recipes'][0]

    def case(self, name, mode, seat):
        declaration = self.declarations[name]
        shape = model.spec(declaration, self.oracle[name])
        return model.expand(name, shape['csvid'], declaration, shape, [f'{mode}_{seat}'], self.rows, self.oracle)[0]['steps']

    def assert_card(self, steps, ref, prop, value):
        self.assertIn(dict(op='assert_card', ref=ref, property=prop, value=value), steps)

    def test_whole_records_and_pairing(self):
        model.validate_witnesses(self.oracle, self.rows)
        for name, declaration in self.declarations.items():
            shape = model.spec(declaration, self.oracle[name])
            self.assertEqual(declaration['variants'], model.variants(shape))
            self.assertEqual([a.rsplit('_',1)[0] for a in declaration['variants'][::2]],
                             [a.rsplit('_',1)[0] for a in declaration['variants'][1::2]])
            self.assertEqual(shape['clauses'], self.oracle[name]['oracle_text'].splitlines())

    def test_reject_changes_to_fee_and_unsupported_printed_clauses(self):
        name = 'Misthoof Kirin'
        changed = copy.deepcopy(self.declarations[name])
        changed['params']['mechanism']['fee'][5] = 0
        with self.assertRaises(ValueError):
            model.spec(changed, self.oracle[name])
        for prefix in ('First strike\n', 'Vigilance\n'):
            with self.assertRaises(ValueError):
                model.mechanism(dict(self.oracle[name], oracle_text=prefix + self.oracle[name]['oracle_text']))
        with self.assertRaises(ValueError):
            model.validate_witnesses(self.oracle, dict(self.rows, **{'Welkin Tern': dict(self.rows['Welkin Tern'], handler='card_dummy')}))

    def test_native_megamorph_pays_fee_and_persists(self):
        for actor in (0, 1):
            for name in self.declarations:
                shape = model.mechanism(self.oracle[name])
                steps = self.case(name, 'counter_persists', actor)
                flip = steps.index(dict(op='activate', ref='source', value=1))
                for color, value in enumerate(shape['fee']):
                    self.assertIn(dict(op='set_mana', player=actor, color=color, value=value), steps[flip-8:flip])
                    self.assertIn(dict(op='assert_mana', player=actor, color=color, value=0), steps[flip:])
                last = max(index for index, step in enumerate(steps) if step['op'] == 'advance_turn')
                self.assert_card(steps[last:], 'source', 'plus1_counters', 1)
                self.assert_card(steps[last:], 'source', 'power', shape['power']+1)
                self.assert_card(self.case(name, 'ordinary', actor), 'source', 'plus1_counters', 0)

    def test_counter_adds_to_existing_and_new_object_resets(self):
        for actor in (0, 1):
            steps = self.case('Marsh Hulk', 'grow_and_flip', actor)
            self.assertIn(dict(op='choose_target', player=actor, ref='source'), steps)
            self.assert_card(steps, 'source', 'plus1_counters', 2)
            self.assert_card(steps, 'source', 'power', 6)
            reset = self.case('Marsh Hulk', 'bounce_reset', actor)
            rebound = reset.index(dict(op='bind_hand_copy', ref='returned', player=actor, index=0, value=14958))
            self.assert_card(reset[:rebound], 'source', 'plus1_counters', 1)
            self.assert_card(reset[rebound:], 'returned', 'plus1_counters', 0)

    def test_actual_reach_defender_and_unblockable_combat(self):
        for actor in (0, 1):
            for mode, permitted in [('keyword_hidden', 0), ('keyword_ordinary', 1), ('keyword_flipped', 1)]:
                steps = self.case('Aerie Bowmasters', mode, actor)
                self.assertIn(dict(op='block', ref='source', other='witness', value=permitted), steps)
            for mode, permitted in [('keyword_hidden', 1), ('keyword_ordinary', 0), ('keyword_flipped', 0)]:
                steps = self.case('Dirgur Nemesis', mode, actor)
                self.assertIn(dict(op='attack', ref='source', value=permitted), steps)
            self.assertIn(dict(op='block', ref='source', other='witness', value=1),
                          self.case('Dirgur Nemesis', 'keyword_defender_blocks', actor))
            self.assertIn(dict(op='block', ref='witness', other='source', value=0),
                          self.case('Gudul Lurker', 'keyword_reach', actor))

    def test_flying_vigilance_double_strike_and_trample(self):
        for actor in (0, 1):
            vigilant = self.case('Misthoof Kirin', 'keyword_flipped', actor)
            attack = vigilant.index(dict(op='attack', ref='source', value=1))
            self.assertEqual(vigilant[attack+1], dict(op='assert_card', ref='source', property='tapped', value=0))
            self.assertIn(dict(op='block', ref='witness', other='source', value=0), vigilant)
            double = self.case('Aven Sunstriker', 'keyword_reach', actor)
            self.assertIn(dict(op='block', ref='witness', other='source', value=1), double)
            self.assert_card(double, 'witness', 'damage', 4)
            for name, hits in [('Aven Sunstriker', 4), ('Stormcrag Elemental', 5)]:
                steps = self.case(name, 'keyword_flipped', actor)
                self.assertIn(dict(op='assert_life', player=1-actor, value=20-hits), steps)

    def test_each_generated_combat_finishes_real_declaration_damage_and_cleanup(self):
        for name, declaration in self.declarations.items():
            for suffix in declaration['variants']:
                if not suffix.startswith('keyword_'):
                    continue
                mode, seat = suffix.rsplit('_',1)
                steps = self.case(name, mode, int(seat))
                opcodes = [step['op'] for step in steps]
                phases = ['begin_attackers','attack','finish_attackers','begin_blockers','advance_combat','combat_damage','end_combat']
                self.assertEqual(sorted(phases, key=opcodes.index), phases)
                self.assertEqual(opcodes.count('combat_damage'), 1)
                for index, step in enumerate(steps):
                    if step.get('property') == 'blocked':
                        self.assertGreater(index, opcodes.index('advance_combat'))

    def test_snapshot_replays_paid_turn_up_and_counter(self):
        for name in self.declarations:
            for actor in (0, 1):
                steps = self.case(name, 'replay', actor)
                capture, record, replay = [steps.index(dict(op=op)) for op in ('capture','record_state','ai_roundtrip')]
                self.assertEqual(steps[capture+1:record], steps[replay+1:-1])
                self.assert_card(steps[replay+1:], 'source', 'plus1_counters', 1)


if __name__ == '__main__':
    unittest.main()
