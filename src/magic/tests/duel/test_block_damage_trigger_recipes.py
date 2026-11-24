"""Guard printed trigger branches, recipient counts and engine phase ordering."""
import copy
import json
import unittest
from pathlib import Path

import block_damage_trigger_recipes as model
from generate_cases import registry


class BlockDamageTriggerTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        data = Path(__file__).parent / 'data'
        cls.oracle = json.loads((data / 'oracle.json').read_bytes())['cards']
        cls.rows = {row['name']: row for row in registry()}
        cls.declarations = {}
        for name, pin in model.pins()['sources'].items():
            path, = (data / 'cases/cards').glob(f"*/{pin['csvid']:05d}-*.json")
            cls.declarations[name] = json.loads(path.read_bytes())['recipes'][0]

    def case(self, name, mode, owner):
        declaration = self.declarations[name]
        shape = model.spec(declaration, self.oracle[name])
        return model.expand(name, shape['csvid'], declaration, shape, [f'{mode}_{owner}'], self.rows, self.oracle)[0]

    def final_life(self, steps, who):
        return [s['value'] for s in steps if s['op'] == 'assert_life' and s['player'] == who][-1]

    def final_zone(self, steps, who, zone, name, value=1):
        index = max(i for i, item in enumerate(steps) if item['op'] in ('resolve', 'end_combat'))
        self.assertIn(dict(op='assert_zone_multiplicity', player=who, zone=zone, index=self.rows[name]['csvid'], value=value),
                      steps[index+1:])

    def test_whole_printed_clauses_and_mirrored_matrix(self):
        model.validate_witnesses(self.oracle, self.rows)
        self.assertEqual(sum(len(deck['variants']) for deck in self.declarations.values()), 170)
        for name, declaration in self.declarations.items():
            shape = model.spec(declaration, self.oracle[name])
            self.assertEqual(shape['clauses'], self.oracle[name]['oracle_text'].splitlines())
            self.assertEqual(declaration['variants'], model.variants(shape))
            self.assertEqual([v[:-2] for v in declaration['variants'][::2]], [v[:-2] for v in declaration['variants'][1::2]])

    def test_mutated_amount_and_unknown_clauses_are_rejected(self):
        example = 'Assembled Alphas'
        declaration = copy.deepcopy(self.declarations[example])
        declaration['params']['mechanism']['controller_damage'] = 0
        with self.assertRaises(ValueError):
            model.spec(declaration, self.oracle[example])
        invalid = dict(self.oracle[example], oracle_text=self.oracle[example]['oracle_text']+'\nDraw a card.')
        with self.assertRaises(ValueError):
            model.mechanism(invalid)

    def test_complete_native_phases_and_independent_precombat_receipts(self):
        for owner in (0, 1):
            for name in ('Ashmouth Hound', 'Assembled Alphas', 'Acolyte of the Inferno'):
                shape = model.mechanism(self.oracle[name])
                for mode in ('attack_marked', 'defend_marked'):
                    steps = self.case(name, mode, owner)['steps']
                    phases = [dict(op='begin_attackers'), dict(op='finish_attackers'), dict(op='begin_blockers'),
                              dict(op='advance_combat'), dict(op='cast', ref='window_spell', value=1),
                              dict(op='combat_damage'), dict(op='end_combat')]
                    self.assertEqual(sorted(phases, key=steps.index), phases)
                    between = steps[steps.index(phases[4])+1:steps.index(phases[5])]
                    trigger = mode == 'attack_marked' or shape['defense']
                    self.assertIn(dict(op='assert_card', ref='peer', property='damage', value=shape['damage'] if trigger else 0), between)
                    self.assertIn(dict(op='assert_life', player=1-owner, value=20-(shape['controller_damage'] if trigger else 0)), between)
                    self.assertIn(dict(op='assert_zone_multiplicity', player=1-owner, zone='hand', index=126, value=1), between)

    def test_first_strike_blocker_and_two_blockers_die_without_retaliation(self):
        for owner in (0, 1):
            for name in self.declarations:
                first = self.case(name, 'attack_first_strike', owner)['steps']
                self.final_zone(first, 1-owner, 'graveyard', 'Thalia, Guardian of Thraben')
                self.final_zone(first, owner, 'battlefield', name)
                self.assertIn(dict(op='assert_card', ref='source', property='damage', value=0),
                              first[first.index(dict(op='end_combat')):])
                two = self.case(name, 'attack_two', owner)['steps']
                self.assertIn(dict(op='block', ref='peer', other='source', value=1), two)
                self.assertIn(dict(op='block', ref='peer_2', other='source', value=1), two)
                self.final_zone(two, 1-owner, 'graveyard', 'Elvish Mystic', 2)
                self.assertEqual(self.final_life(two, 1-owner), 14 if name == 'Assembled Alphas' else 20)

    def test_blocking_only_and_hexproof_controls(self):
        for owner in (0, 1):
            for name in self.declarations:
                shape = model.mechanism(self.oracle[name])
                defense = self.case(name, 'defend_lethal', owner)['steps']
                self.assertIn(dict(op='block', ref='source', other='peer', value=1), defense)
                self.final_zone(defense, owner, 'battlefield' if shape['defense'] else 'graveyard', name)
                blocked = self.case(name, 'hexproof_blocker', owner)['steps']
                self.assertIn(dict(op='block', ref='peer', other='source', value=1), blocked)
                self.assertFalse(any(s['op'] == 'choose_target' for s in blocked))
                self.final_zone(blocked, 1-owner, 'graveyard', 'Slippery Bogle')
                quiet = self.case(name, 'unrelated', owner)['steps']
                self.assertIn(dict(op='block', ref='peer', other='ally', value=1), quiet)
                self.assertEqual(self.final_life(quiet, 1-owner), 20)

    def test_renown_is_once_per_object_and_only_player_combat_damage(self):
        name = 'Acolyte of the Inferno'
        for owner in (0, 1):
            twice = self.case(name, 'renown_twice', owner)['steps']
            observed = [s['value'] for s in twice if s.get('ref') == 'source' and s.get('property') == 'plus1_counters']
            self.assertEqual(observed, [0, 1, 1])
            self.assertEqual(self.final_life(twice, 1-owner), 13)
            self.assertEqual(sum(s['op'] == 'combat_damage' for s in twice), 2)
            block = self.case(name, 'attack_lethal', owner)['steps']
            self.assertEqual([s['value'] for s in block if s.get('ref') == 'source' and s.get('property') == 'plus1_counters'], [0,0])

    def test_actual_printed_payment_and_fresh_attack_denial(self):
        for name in self.declarations:
            fees = model.mechanism(self.oracle[name])['printed']
            for owner in (0, 1):
                live = self.case(name, 'fresh_attack', owner)['steps']
                cast = live.index(dict(op='cast', ref='source', value=1))
                self.assertEqual([s['value'] for s in live[:cast] if s['op'] == 'set_mana'][-6:], fees)
                self.assertIn(dict(op='attack', ref='source', value=0), live[cast:])
                for mode in ('wrong_color', 'unfunded'):
                    self.assertIn(dict(op='cast', ref='source', value=0), self.case(name, mode, owner)['steps'])

    def test_snapshot_replay_recreates_real_declarations(self):
        for name in self.declarations:
            for owner in (0, 1):
                steps = self.case(name, 'replay', owner)['steps']
                capture, record, ai = [steps.index(dict(op=op)) for op in ('capture','record_state','ai_roundtrip')]
                self.assertEqual(steps[capture+1:record], steps[ai+1:-1])
                self.assertIn(dict(op='block', ref='peer', other='source', value=1), steps[capture:record])


if __name__ == '__main__':
    unittest.main()
