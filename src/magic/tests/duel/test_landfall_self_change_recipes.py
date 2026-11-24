"""Targeted guards for real land transitions and combat contrasts."""
import copy
import json
from pathlib import Path
import unittest

from generate_cases import registry
from recipes import expand
import landfall_self_change_recipes as family


class LandfallSelfChangeTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        base = Path(__file__).parent
        cls.oracle = json.loads((base/'data/oracle.json').read_text(encoding='utf8'))['cards']
        cls.rows = {r['name']: r for r in registry()}
        cls.declarations, cls.cases = {}, {}
        for name, pin in family.pins()['sources'].items():
            csvid = pin['registry']['csvid']
            home = next((base/'data/cases/cards'/f'{csvid//100:03d}').glob(f'{csvid:05d}-*.json'))
            declaration = json.loads(home.read_text(encoding='utf8'))['recipes'][0]
            cls.declarations[name] = declaration
            expanded = expand(name, csvid, declaration, cls.oracle[name], cls.rows, cls.oracle)
            cls.cases[name] = {case['_recipe']['variant']:case for case in expanded}

    def steps(self, name, variant):
        return self.cases[name][variant]['steps']

    def test_exact_whole_clause_and_implementation_pins(self):
        self.assertEqual(len(self.cases), 8)
        for name, declaration in self.declarations.items():
            oracle = self.oracle[name]
            family.spec(declaration, oracle)
            for new in (dict(declaration, version=True), dict(declaration, variants=['own_land_0']*2)):
                with self.assertRaises(ValueError):
                    family.spec(new, oracle)
            for text in (oracle['oracle_text']+'\nDraw a card.', oracle['oracle_text'].replace('a land you control', 'any land')):
                with self.assertRaises(ValueError):
                    family.oracle_model(dict(oracle, oracle_text=text))
        rows = copy.deepcopy(self.rows)
        rows['Rampant Growth']['handler'] = 'card_dummy'
        with self.assertRaises(ValueError):
            family.validate_witnesses(self.oracle, rows)

    def test_actual_land_entry_follows_pending_paid_spell_and_binds_owner(self):
        for name in self.cases:
            for owner in (0, 1):
                steps = self.steps(name, f'own_land_{owner}')
                cast = steps.index(dict(op='cast', ref='search_0', value=1))
                choice = steps.index(dict(op='choose_zone_card', player=-1, value=91))
                bind = steps.index(dict(op='bind_permanent', ref='land_0', player=owner, value=91))
                self.assertLess(cast, choice)
                self.assertLess(choice, bind)
                self.assertEqual(steps[choice+1], dict(op='resolve'))
                self.assertIn(dict(op='assert_stack', value=1), steps[cast:choice])
                self.assertIn(dict(op='assert_mana', player=owner, color=3, value=0), steps[cast:choice])
                self.assertIn(dict(op='assert_card', ref='land_0', property='tapped', value=1), steps[bind:])

    def test_opponent_nonland_failed_and_countered_searches_never_apply_bonus(self):
        for name in self.cases:
            expected = int(self.oracle[name]['power'])
            for owner in (0, 1):
                enemy = 1-owner
                for variant in ('opponent_land', 'other_source', 'nonland_spell', 'empty_search', 'countered_search'):
                    steps = self.steps(name, f'{variant}_{owner}')
                    ref = 'opposing_copy' if variant=='other_source' else 'source'
                    self.assertEqual({s['value'] for s in steps if s['op']=='assert_card' and s.get('property')=='power'
                                      and s.get('ref')==ref}, {expected})
                    if variant=='opponent_land':
                        self.assertIn(dict(op='bind_permanent', ref='land_0', player=enemy, value=91), steps)
                    elif variant=='countered_search':
                        self.assertIn(dict(op='resolve_response'), steps)
                        self.assertFalse(any(s['op'] in ('choose_zone_card', 'bind_permanent') for s in steps))

    def test_forest_bonus_reads_subtype_and_real_multiple_entries_accumulate(self):
        for owner in (0, 1):
            for variant, expected in [('own_land', [2]), ('nonforest', [1]), ('nonbasic_forest', [2]),
                                       ('consecutive_lands', [2, 3, 4])]:
                steps = self.steps('Oran-Rief Hydra', f'{variant}_{owner}')
                amounts = []
                for i, action in enumerate(steps):
                    if action['op']=='bind_permanent':
                        receipt = next(s for s in steps[i:] if s['op']=='assert_counter')
                        amounts.append(receipt['value'])
                self.assertEqual(amounts, expected)
            nonbasic = self.steps('Oran-Rief Hydra', f'nonbasic_forest_{owner}')
            self.assertIn(dict(op='bind_permanent', ref='land_0', player=owner, value=8916), nonbasic)
            self.assertIn(dict(op='assert_card', ref='land_0', property='tapped', value=0), nonbasic)

    def test_cleanup_preserves_real_counters_and_expires_temporary_stats(self):
        for name, final, counter in [('Vinelasher Kudzu',2,1), ('Oran-Rief Hydra',7,2),
                                     ('Baloth Woodcrasher',4,0), ('Hagra Crocodile',3,0)]:
            for owner in (0, 1):
                steps = self.steps(name, f'cleanup_{owner}')
                cleanup = steps.index(dict(op='cleanup'))
                self.assertIn(dict(op='assert_counter', ref='source', counter_kind='plus1', value=counter), steps[cleanup:])
                self.assertIn(dict(op='assert_card', ref='source', property='power', value=final), steps[cleanup:])
        for name, powers in [('Baloth Woodcrasher', [8,12,16]), ('Hagra Crocodile', [5,7,9])]:
            steps = self.steps(name, 'consecutive_lands_1')
            result = []
            for i,s in enumerate(steps):
                if s['op']=='bind_permanent':
                    result.append(next(a['value'] for a in steps[i:] if a.get('property')=='power'))
            self.assertEqual(result, powers)

    def test_keyword_combat_covers_pre_entry_post_entry_and_expiry(self):
        for owner in (0, 1):
            for name in ('Geyser Glider','Fledgling Griffin'):
                for mode, legal in [('combat_before',1),('combat_after',0),('combat_expired',1),('flying_blocker',1)]:
                    self.assertIn(dict(op='block', ref='blocker', other='source', value=legal),
                                  self.steps(name, f'{mode}_{owner}'))
            for mode, legal in [('combat_before',0),('combat_after',1),('combat_expired',0)]:
                self.assertIn(dict(op='attack', ref='source', value=legal), self.steps('Shoal Serpent', f'{mode}_{owner}'))
            for name, mode, life in [('Baloth Woodcrasher','combat_after',17),
                                      ('Baloth Woodcrasher','combat_expired',20),
                                      ('Oran-Rief Hydra','combat_expired',18)]:
                self.assertIn(dict(op='assert_life', player=1-owner, value=life), self.steps(name, f'{mode}_{owner}'))
            for mode, leaves in [('combat_before',1),('combat_after',0),('combat_expired',1)]:
                self.assertIn(dict(op='assert_zone_multiplicity', player=owner, zone='graveyard', index=15270, value=leaves),
                              self.steps('Belligerent Whiptail', f'{mode}_{owner}'))
            steps = self.steps('Hagra Crocodile', f'cannot_block_{owner}')
            self.assertIn(dict(op='block', ref='source', other='opposing_attacker', value=0), steps)
            self.assertIn(dict(op='assert_life', player=owner, value=19), steps)

    def test_replay_captures_real_paid_search_and_replays_target_and_entry(self):
        for name in self.cases:
            for owner in (0, 1):
                steps = self.steps(name, f'pending_replay_{owner}')
                capture, restore = steps.index(dict(op='capture')), steps.index(dict(op='restore'))
                record = steps.index(dict(op='record_state'))
                self.assertIn(dict(op='cast', ref='search_0', value=1), steps[:capture])
                self.assertEqual(steps[capture+1:record], steps[restore+1:-1])
                self.assertEqual(steps[-1], dict(op='assert_recorded_state'))


if __name__ == '__main__':
    unittest.main()
