"""Pin full Oracle and verify that hexproof fixtures use normal game actions."""

import json
import unittest

import player_hexproof_recipes as model
from generate_cases import registry


class PlayerHexproofTest(unittest.TestCase):
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
    def values(steps, op, **fields):
        return [step['value'] for step in steps if step['op'] == op and all(step.get(k) == v for k, v in fields.items())]

    def test_whole_oracle_and_aliases(self):
        model.validate_witnesses(self.oracle, self.rows)
        expected = {'Aegis of the Gods': ('{1}{W}', 'static'), 'Imperial Mask': ('{4}{W}', 'team'),
                    'Leyline of Sanctity': ('{2}{W}{W}', 'opening'), 'Spirit of the Hearth': ('{4}{W}{W}', 'flying'),
                    'Witchbane Orb': ('{4}', 'curses')}
        self.assertEqual(set(model.profiles()['sources']), expected.keys())
        for name, pair in expected.items():
            record = self.oracle[name]
            params = model.parameters(record)
            self.assertEqual((params['mana'], params['branch']), pair)
            with self.assertRaises(ValueError):
                model.parameters(dict(record, oracle_text=record['oracle_text'] + '\nDraw a card.'))

    def test_real_spell_or_ability_denial_with_positive_controls(self):
        name = 'Aegis of the Gods'
        for owner in (0, 1):
            open_ = self.case(name, f'unprotected_{owner}')['steps']
            shielded = self.case(name, f'opponent_blocked_{owner}')['steps']
            self.assertEqual(self.values(open_, 'can_cast', ref='probe'), [1])
            self.assertEqual(self.values(shielded, 'can_cast', ref='probe'), [0])
            self.assertEqual(self.values(shielded, 'cast', ref='probe'), [0])
            self.assertFalse(any(s['op'] == 'choose_player' for s in shielded))
            open_ = self.case(name, f'ability_unprotected_{owner}')['steps']
            shielded = self.case(name, f'ability_blocked_{owner}')['steps']
            self.assertEqual(self.values(open_, 'can_activate', ref='ability'), [1])
            self.assertEqual(self.values(shielded, 'activate', ref='ability'), [0])
            self.assertEqual(self.values(open_, 'assert_life', player=owner), [19])
            self.assertEqual(self.values(shielded, 'assert_life', player=owner), [20])

    def test_lawful_self_target_and_symmetric_nontarget(self):
        for owner in (0, 1):
            own = self.case('Imperial Mask', f'own_target_{owner}')['steps']
            self.assertEqual(self.values(own, 'choose_player', player=owner), [owner])
            self.assertEqual(self.values(own, 'assert_life', player=owner), [17])
            sweep = self.case('Imperial Mask', f'symmetric_damage_{owner}')['steps']
            self.assertFalse(any(s['op'] == 'choose_player' for s in sweep))
            self.assertEqual(self.values(sweep, 'assert_life', player=owner), [16])
            self.assertEqual(self.values(sweep, 'assert_life', player=1-owner), [16])

    def test_real_reentry_invalidates_previously_legal_target(self):
        for owner in (0, 1):
            steps = self.case('Leyline of Sanctity', f'late_entry_{owner}')['steps']
            self.assertIn(dict(op='choose_target', player=1-owner, ref='source'), steps)
            raw = [s['op'] for s in steps]
            self.assertLess(raw.index('announce_spell'), raw.index('cast_response'))
            self.assertLess(raw.index('cast_response'), raw.index('resolve_response'))
            self.assertEqual(self.values(steps, 'assert_card', ref='source', property='in_play'), [0, 1, 1])
            self.assertEqual(self.values(steps, 'assert_life', player=owner), [20])
            self.assertEqual(self.values(steps, 'assert_zone_multiplicity', player=1-owner, zone='graveyard', index=11894), [1])

    def test_leyline_opening_choice_uses_real_first_human_upkeep(self):
        for owner in (0, 1):
            for mode, accepts in [('opening_place', True), ('opening_decline', False)]:
                case = self.case('Leyline of Sanctity', f'{mode}_{owner}')
                self.assertEqual([row['zone'] for row in case['setup'] if row['ref'] == 'source'], ['hand'])
                steps = case['steps']
                i = steps.index(dict(op='upkeep_step', player=0))
                self.assertEqual(steps[i-2:i], [dict(op='untap_step', player=0),
                                                dict(op='choose_dialog', player=owner, value=int(not accepts))])
                self.assertEqual(self.values(steps, 'cast', ref='source'), [])
                self.assertEqual(self.values(steps, 'can_cast', ref='probe'), [int(not accepts)])
                self.assertEqual(self.values(steps, 'assert_life', player=owner), [20 if accepts else 17])

    def test_curses_keep_controller_and_attachment_recipient_distinct(self):
        for owner in (0, 1):
            case = self.case('Witchbane Orb', f'curse_scope_{owner}')
            steps = case['steps']
            self.assertEqual(self.values(steps, 'choose_player', player=1-owner), [owner, 1-owner])
            self.assertEqual(self.values(steps, 'assert_card', ref='curse_0', property='in_play'), [1, 0])
            self.assertEqual(self.values(steps, 'assert_card', ref='curse_1', property='in_play'), [1, 1])
            self.assertEqual(self.values(steps, 'assert_zone_multiplicity', player=1-owner, zone='graveyard', index=11919), [1])
            self.assertEqual(self.values(steps, 'assert_card', ref='noncurse', property='in_play'), [1])

    def test_flying_and_team_scope_use_actual_duel_state(self):
        steps = self.case('Spirit of the Hearth', 'flying_blocked_1')['steps']
        self.assertEqual(self.values(steps, 'block', ref='ground', other='source'), [0])
        self.assertEqual(self.values(steps, 'block', ref='flyer', other='source'), [1])
        self.assertEqual(self.values(steps, 'assert_card', ref='source', property='damage'), [2])
        mask = self.case('Imperial Mask', 'no_teammates_0')
        self.assertIn('positive teammate-copy', mask['_recipe']['untested_components'][0])
        self.assertEqual(self.values(mask['steps'], 'assert_zone_multiplicity', player=1, zone='battlefield', index=8947), [0])


    def test_clears_both_default_mana_pools_before_real_actions(self):
        for owner in (0, 1):
            expected = [dict(op='set_mana', player=player, color=color, value=0)
                        for player in (owner, 1-owner) for color in range(6)]
            for name, mode in [('Aegis of the Gods', 'own_target'), ('Leyline of Sanctity', 'opening_place')]:
                steps = self.case(name, f'{mode}_{owner}')['steps']
                self.assertEqual(steps[:12], expected)
                action = 'untap_step' if mode == 'opening_place' else 'cast'
                self.assertGreater(next(i for i, s in enumerate(steps) if s['op'] == action), 11)


if __name__ == '__main__':
    unittest.main()
