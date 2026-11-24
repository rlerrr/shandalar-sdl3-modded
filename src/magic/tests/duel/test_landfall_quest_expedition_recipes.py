"""Focused guards for the shared threshold and distinct effect timing."""
import copy
import json
from pathlib import Path
import unittest

from generate_cases import registry
from recipes import expand
import landfall_quest_expedition_recipes as family


class LandfallQuestExpeditionTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        root = Path(__file__).parent
        cls.oracle = json.loads((root/'data/oracle.json').read_text(encoding='utf8'))['cards']
        cls.rows = {r['name']:r for r in registry()}
        cls.cases, cls.declarations = {}, {}
        for name, pin in family.pins()['sources'].items():
            csvid = pin['registry']['csvid']
            path = next((root/'data/cases/cards'/f'{csvid//100:03d}').glob(f'{csvid:05d}-*.json'))
            declaration = json.loads(path.read_text(encoding='utf8'))['recipes'][0]
            cls.declarations[name] = declaration
            cases = expand(name, csvid, declaration, cls.oracle[name], cls.rows, cls.oracle)
            cls.cases[name] = {c['_recipe']['variant']:c for c in cases}

    def steps(self, name, variant):
        return self.cases[name][variant]['steps']

    def test_whole_oracle_scope_threshold_and_sacrifice_are_exact(self):
        self.assertEqual(len(self.cases), 5)
        for name, decl in self.declarations.items():
            oracle = self.oracle[name]
            family.spec(decl, oracle)
            for old, new in [('you may put', 'put'), ('you control', 'an opponent controls'),
                             ('three quest counters', 'two quest counters'), (' and sacrifice it', '')]:
                text = oracle['oracle_text'].replace(old, new)
                with self.assertRaises(ValueError):
                    family.oracle_model(dict(oracle, oracle_text=text))
            invalid = copy.deepcopy(decl)
            invalid['params']['quest_cost'] = True
            with self.assertRaises(ValueError):
                family.spec(invalid, oracle)

    def test_real_land_entries_cross_threshold_and_preserve_optional_decline(self):
        for name in self.cases:
            for owner in (0, 1):
                steps = self.steps(name, f'threshold_{owner}')
                received = []
                for i in range(3):
                    land = steps.index(dict(op='assert_card', ref=f'land_{i}', property='in_play', value=1))
                    received.append(steps[land+2]['value'])
                    self.assertIn(dict(op='activate', ref=f'atlas_{i}', value=1), steps[:land])
                    self.assertIn(dict(op='assert_card', ref=f'atlas_{i}', property='tapped', value=1), steps[:land])
                self.assertEqual(received, [1,2,3])
                self.assertEqual([s['value'] for s in steps if s['op']=='activate' and s['ref']=='source'], [0,0])
                enemy_land = self.steps(name, f'opponent_land_{owner}')
                self.assertEqual({s['value'] for s in enemy_land if s['op']=='assert_counter'}, {0})
            declined = self.steps(name, 'decline_then_pay_0')
            first = declined.index(dict(op='assert_card', ref='land_0', property='in_play', value=1))
            self.assertFalse(any(s['op']=='choose_trigger' for s in declined[:first]))
            self.assertEqual(sum(s['op']=='choose_trigger' for s in declined), 3)

    def test_sacrifice_paid_before_resolution_or_payoff(self):
        for name in self.cases:
            for owner in (0,1):
                steps = self.steps(name, f'extra_counter_{owner}')
                activation = steps.index(dict(op='activate', ref='source', value=1))
                resolve = steps.index(dict(op='resolve'), activation)
                self.assertIn(dict(op='assert_counter', ref='source', counter_kind='quest', value=4), steps[:activation])
                self.assertIn(dict(op='assert_stack', value=1), steps[activation:resolve])
                self.assertIn(dict(op='assert_card', ref='source', property='in_play', value=0), steps[activation:resolve])
                self.assertIn(dict(op='assert_zone_multiplicity', player=owner, zone='graveyard', index=self.rows[name]['csvid'], value=1),
                              steps[activation:resolve])

    def test_khalni_search_resolves_after_cost_and_returns_only_basics(self):
        for owner in (0,1):
            steps = self.steps('Khalni Heart Expedition', f'activate_{owner}')
            activation = steps.index(dict(op='activate', ref='source', value=1))
            picks = [s['value'] for s in steps[activation:] if s['op']=='choose_zone_card']
            self.assertEqual(picks, [self.rows[n]['csvid'] for n in ('Swamp','Mountain')])
            for i in range(2):
                self.assertIn(dict(op='assert_card', ref=f'fetched_{i}', property='tapped', value=1), steps)
            self.assertIn(dict(op='assert_shuffle_count', player=owner, value=1), steps[activation:])
            self.assertIn(dict(op='assert_zone_multiplicity', player=owner, zone='library', index=8916, value=1), steps[activation:])
        zero = self.steps('Khalni Heart Expedition', 'choose_none_0')
        self.assertIn(dict(op='choose_optional_zone', player=0, value=self.rows['Swamp']['csvid']), zero)
        self.assertFalse(any(s['op']=='bind_permanent_copy' for s in zero))

    def test_soul_stair_targets_own_creatures_at_activation(self):
        for owner in (0,1):
            steps = self.steps('Soul Stair Expedition', f'activate_{owner}')
            activation = steps.index(dict(op='activate', ref='source', value=1))
            chosen = [s['value'] for s in steps[activation-2:activation]]
            self.assertEqual(chosen, [self.rows[n]['csvid'] for n in ('Elvish Mystic','Walking Atlas')])
            self.assertFalse(any(s['op']=='choose_zone_card' for s in steps[activation:]))
            self.assertIn(dict(op='assert_zone_multiplicity', player=1-owner, zone='graveyard', index=12582, value=1), steps[activation:])
            self.assertIn(dict(op='assert_zone_multiplicity', player=owner, zone='hand', index=13284, value=1), steps[activation:])
        empty = self.steps('Soul Stair Expedition', 'no_match_1')
        self.assertIn(dict(op='activate', ref='source', value=1), empty)
        self.assertNotIn(dict(op='choose_zone_card', player=1, value=12582), empty)

    def test_draw_and_life_effects_reach_the_right_recipient(self):
        for owner in (0,1):
            draw = self.steps('Ior Ruin Expedition', f'activate_{owner}')
            self.assertIn(dict(op='assert_zone_card', player=owner, zone='library', index=0, value=10860), draw)
            for name,count in [('Darksteel Relic',2), ('Elvish Mystic',1)]:
                self.assertIn(dict(op='assert_zone_multiplicity', player=owner, zone='hand', index=self.rows[name]['csvid'], value=count), draw)
            life = self.steps('Sunspring Expedition', f'low_life_{owner}')
            self.assertIn(dict(op='assert_life', player=owner, value=11), life)
            self.assertIn(dict(op='assert_life', player=1-owner, value=20), life)

    def test_zektar_token_creation_haste_trample_and_expiry(self):
        for owner in (0,1):
            steps = self.steps('Zektar Shrine Expedition', f'haste_trample_{owner}')
            bind = steps.index(dict(op='bind_permanent', ref='token', player=owner, value=18726))
            attack = steps.index(dict(op='attack', ref='token', value=1))
            expiry = steps.index(dict(op='cleanup'))
            self.assertLess(bind, attack)
            self.assertLess(attack, expiry)
            self.assertFalse(any(s['op']=='advance_turn' for s in steps[bind:attack]))
            self.assertIn(dict(op='assert_life', player=1-owner, value=18), steps[attack:expiry])
            self.assertIn(dict(op='assert_card', ref='token', property='in_play', value=0), steps[expiry:])
            for zone in ('battlefield','graveyard','exile'):
                self.assertIn(dict(op='assert_zone_multiplicity', player=owner, zone=zone, index=18726, value=0), steps[expiry:])

    def test_paid_pending_and_expiry_snapshots_replay_actual_tail(self):
        for name in self.cases:
            for variant in ('pending_replay_0', 'pending_replay_1'):
                steps = self.steps(name, variant)
                capture = steps.index(dict(op='capture'))
                self.assertIn(dict(op='activate', ref='source', value=1), steps[:capture])
                self.assertEqual(steps[capture+1:steps.index(dict(op='record_state'))],
                                 steps[steps.index(dict(op='restore'))+1:-1])
                self.assertEqual(steps[-1], dict(op='assert_recorded_state'))
        expiry = self.steps('Zektar Shrine Expedition','expiry_replay_1')
        self.assertEqual(expiry[expiry.index(dict(op='capture'))+1], dict(op='cleanup'))


if __name__ == '__main__':
    unittest.main()
