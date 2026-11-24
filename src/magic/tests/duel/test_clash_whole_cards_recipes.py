"""Protect the whole-card text and exact reveal, action, and zone receipts."""

import json
import unittest

import clash_whole_cards_recipes as model
from generate_cases import registry


class ClashWholeCardsTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.oracle = json.loads((model.PROFILE_PATH.parent / 'oracle.json').read_text(encoding='utf8'))['cards']
        cls.rows = {row['name']: row for row in registry()}

    def case(self, name, variant):
        record = self.oracle[name]
        declaration = dict(family=model.FAMILY, version=2, oracle_text=record['oracle_text'],
                           params=model.parameters(record), variants=[variant])
        return model.expand(name, self.rows[name]['csvid'], declaration, model.spec(declaration, record),
                            declaration['variants'], self.rows, self.oracle)[0]

    @staticmethod
    def values(steps, operation, **fields):
        return [step['value'] for step in steps if step['op'] == operation and all(step.get(key) == value for key, value in fields.items())]

    def test_whole_clauses_and_real_cost_setup(self):
        expected = {'Adder-Staff Boggart': 'counter', 'Gilt-Leaf Ambush': 'ambush', "Hoarder's Greed": 'greed', 'Lash Out': 'lash',
                    'Redeem the Lost': 'protect', 'Release the Ants': 'ants', 'Research the Deep': 'draw', 'Revive the Fallen': 'revive',
                    'Oaken Brawler': 'counter', 'Paperfin Rascal': 'counter'}
        model.validate_witnesses(self.oracle, self.rows)
        self.assertEqual(set(model.profiles()['sources']), expected.keys())
        for name, effect in expected.items():
            record = self.oracle[name]
            self.assertEqual(model.parameters(record)['effect'], effect)
            with self.assertRaises(ValueError):
                model.parameters(dict(record, oracle_text=record['oracle_text'] + '\nDraw a card.'))
            steps = self.case(name, 'win_1')['steps']
            casting = steps.index(dict(op='cast', ref='source', value=1))
            self.assertEqual({(s['player'], s['color']) for s in steps[:casting] if s['op'] == 'set_mana'},
                             {(player, color) for player in (0, 1) for color in range(6)})

    def test_win_tie_loss_and_bottom_are_independent_results(self):
        for owner in (0, 1):
            for mode, counters in [('win', 1), ('tie', 0), ('loss', 0), ('human_bottom', 1)]:
                steps = self.case('Adder-Staff Boggart', f'{mode}_{owner}')['steps']
                self.assertEqual(self.values(steps, 'assert_card', ref='source', property='plus1_counters'), [counters])
                self.assertEqual(self.values(steps, 'choose_dialog', player=owner), [0, 0])
                if mode == 'human_bottom':
                    moved = 'Craw Wurm' if not owner else 'Island'
                    self.assertEqual(self.values(steps, 'choose_zone_card', player=0), [self.rows[moved]['csvid']])
                    self.assertEqual(self.values(steps, 'assert_zone_card', player=0, zone='library', index=0), [91])
                else:
                    self.assertEqual(self.values(steps, 'choose_zone', player=0), [-1])

    def test_greed_repeats_after_actual_first_win(self):
        for owner in (0, 1):
            won = self.case("Hoarder's Greed", f'win_{owner}')['steps']
            tied = self.case("Hoarder's Greed", f'tie_{owner}')['steps']
            self.assertEqual(self.values(won, 'choose_dialog', player=owner), [0, 0, 0, 0])
            self.assertEqual(self.values(tied, 'choose_dialog', player=owner), [0, 0])
            self.assertEqual(self.values(won, 'assert_life', player=owner), [16])
            self.assertEqual(self.values(tied, 'assert_life', player=owner), [18])
            self.assertEqual(self.values(won, 'assert_zone', player=owner, zone='hand'), [6])
            self.assertEqual(self.values(tied, 'assert_zone', player=owner, zone='hand'), [4])

    def test_ambush_deathtouch_both_tokens_and_cleanup(self):
        for mode in ('win_0', 'loss_1', 'expiry_0'):
            steps = self.case('Gilt-Leaf Ambush', mode)['steps']
            owner = int(mode[-1])
            self.assertEqual(self.values(steps, 'assert_matching_permanents', player=owner, index=18732, property='subtype_Warrior'), [1])
            for index in range(2):
                self.assertEqual(self.values(steps, 'block', ref=f'elf_{index}', other=f'other_{index}'), [1])
                self.assertEqual(self.values(steps, 'assert_card', ref=f'other_{index}', property='in_play'), [int(mode != 'win_0')])
            self.assertEqual(sum(s['op'] == 'advance_turn' for s in steps), 2 if mode == 'expiry_0' else 0)

    def test_lash_follows_target_controller_and_fizzle_does_not_clash(self):
        steps = self.case('Lash Out', 'own_target_1')['steps']
        self.assertEqual(self.values(steps, 'assert_life', player=1), [17])
        self.assertEqual(self.values(steps, 'assert_life', player=0), [20])
        steps = self.case('Lash Out', 'fizzle_0')['steps']
        i = next(i for i, s in enumerate(steps) if s['op'] == 'resolve_response')
        self.assertEqual(steps[i+1], dict(op='assert_unused_choices', value=4))
        self.assertEqual(self.values(steps, 'assert_life', player=1), [20])

    def test_protection_color_expiry_and_walker_damage(self):
        steps = self.case('Redeem the Lost', 'expiry_1')['steps']
        self.assertEqual(self.values(steps, 'choose_color', player=1), [4])
        self.assertEqual(self.values(steps, 'can_target_creature', ref='red_probe', other='target_0'), [0, 1])
        self.assertEqual(self.values(steps, 'can_target_creature', ref='blue_probe', other='target_0'), [1])
        steps = self.case('Release the Ants', 'walker_1')['steps']
        self.assertEqual(self.values(steps, 'assert_counter', ref='target_0', counter_kind='loyalty'), [2])

    def test_real_winning_recast_and_distinct_grave_owners(self):
        for name in ('Redeem the Lost', 'Release the Ants', 'Research the Deep', 'Revive the Fallen'):
            steps = self.case(name, 'recast_0')['steps']
            self.assertEqual(self.values(steps, 'cast', ref='source'), [1, 1])
            self.assertEqual(self.values(steps, 'bind_hand', ref='source'), [self.rows[name]['csvid']] * 2)
        revive = self.case('Revive the Fallen', 'recast_1')['steps']
        self.assertEqual(self.values(revive, 'choose_player', player=1), [0])
        self.assertEqual(self.values(revive, 'bind_hand', ref='recovered_0', player=0), [106])
        self.assertEqual(self.values(revive, 'bind_hand', ref='recovered_1', player=1), [110])


    def test_clash_picker_uses_caster_then_other_with_real_ai_done(self):
        for owner in (0, 1):
            steps = self.case('Adder-Staff Boggart', f'human_bottom_{owner}')['steps']
            pickers = [s for s in steps if s['op'] in ('choose_zone', 'choose_zone_card')]
            human = dict(op='choose_zone_card', player=0, value=self.rows['Island' if owner else 'Craw Wurm']['csvid'])
            ai = dict(op='choose_zone', player=1, value=-1)
            self.assertEqual(pickers, [ai, human] if owner else [human, ai])
            repeat = self.case("Hoarder's Greed", f'win_{owner}')['steps']
            self.assertEqual(self.values(repeat, 'choose_zone', player=1), [-1, -1])


    def test_exact_etb_aliases_keep_independent_cost_stats_and_outcomes(self):
        for owner in (0, 1):
            for name, generic, color, base_toughness in [('Oaken Brawler', 3, 5, 4), ('Paperfin Rascal', 2, 2, 2)]:
                for mode, counters in [('win', 1), ('tie', 0), ('loss', 0)]:
                    steps = self.case(name, f'{mode}_{owner}')['steps']
                    self.assertIn(dict(op='set_mana', player=owner, color=0, value=generic), steps)
                    self.assertIn(dict(op='set_mana', player=owner, color=color, value=1), steps)
                    self.assertIn(dict(op='cast', ref='source', value=1), steps)
                    self.assertEqual(self.values(steps, 'assert_card', ref='source', property='plus1_counters'), [counters])
                    self.assertEqual(self.values(steps, 'assert_card', ref='source', property='toughness'), [base_toughness + counters])


if __name__ == '__main__':
    unittest.main()
