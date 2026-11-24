"""Check full monarch riders and real payment, turn and destination scripts."""

import json
from pathlib import Path
import unittest

from generate_cases import registry
import monarch_additional_riders_recipes as recipe


DUEL = Path(__file__).resolve().parent
ORACLE = DUEL / 'data/oracle.json'


class MonarchAdditionalRidersRecipes(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.oracle = json.loads(ORACLE.read_text(encoding='utf8'))['cards']
        cls.rows = {r['name']: r for r in registry()}
        cls.homes = {}
        for name in recipe.profile()['sources']:
            csvid = cls.rows[name]['csvid']
            home, = (DUEL / f'data/cases/cards/{csvid // 100:03d}').glob(f'{csvid:05d}-*.json')
            cls.homes[name] = json.loads(home.read_text(encoding='utf8'))

    def actions(self, name, mode, who):
        home = self.homes[name]
        declaration, = home['recipes']
        params = recipe.spec(declaration, self.oracle[name])
        case, = recipe.expand(name, home['csvid'], declaration, params, [f'{mode}_{who}'], self.rows, self.oracle)
        return case['steps']

    def copies(self, name, who, zone, n):
        return dict(op='assert_zone_multiplicity', zone=zone, player=who, index=self.rows[name]['csvid'], value=n)

    def test_whole_oracle_and_catalog(self):
        recipe.validate_witnesses(self.oracle, self.rows)
        self.assertEqual({h['csvid'] for h in self.homes.values()}, {16388,16426,16442,16485,16491,16553,16554})
        self.assertEqual(sum(len(h['recipes'][0]['variants']) for h in self.homes.values()), 128)
        for name, home in self.homes.items():
            declaration, = home['recipes']
            self.assertEqual(declaration['variants'], recipe.variants(recipe.spec(declaration, self.oracle[name])))
            with self.assertRaises(ValueError):
                recipe.classify(dict(self.oracle[name], oracle_text=declaration['oracle_text']+'\nDraw a card.'))

    def test_knights_checks_actual_turn_start_and_two_crown_transitions(self):
        name = 'Knights of the Black Rose'
        for who in (0, 1):
            for mode, after in [('etb_loss', 22), ('midturn', 20), ('earlier_opponent', 20), ('gone', 20)]:
                steps = self.actions(name, mode, who)
                loss = steps.index(dict(op='cast', ref='enemy_claim', value=1))
                self.assertEqual(sum(s['op']=='advance_turn' for s in steps[:loss]),
                                 {'etb_loss': 1, 'midturn': 0, 'earlier_opponent': 2, 'gone': 1}[mode])
                self.assertIn(dict(op='assert_life', player=who, value=after), steps[loss:])
            damage = self.actions(name, 'combat_loss', who)
            hit = damage.index(dict(op='combat_damage'))
            self.assertIn(dict(op='assert_life', player=who, value=20), damage[hit:])
            self.assertIn(dict(op='assert_life', player=1-who, value=18), damage[hit:])

    def test_regal_activates_real_land_and_spends_the_uncopied_pool(self):
        for who in (0, 1):
            for mode, color in recipe.BONUS_COLOR.items():
                steps = self.actions('Regal Behemoth', mode, who)
                tap = steps.index(dict(op='activate', ref='producer', value=1))
                self.assertEqual(steps[tap-1], dict(op='choose_color', player=who, value=color))
                self.assertIn(dict(op='assert_mana', player=who, color=color, value=2 if color==3 else 1), steps[tap:])
            white = self.actions('Regal Behemoth', 'white', who)
            tap = white.index(dict(op='activate', ref='producer', value=1))
            cast = white.index(dict(op='cast', ref='spend', value=1))
            self.assertFalse(any(x['op']=='set_mana' for x in white[tap:cast]))
            self.assertIn(self.copies('Watchwolf', who, 'battlefield', 1), white[cast:])
            for mode in ('lost', 'nonland'):
                self.assertFalse(any(x['op']=='choose_color' for x in self.actions('Regal Behemoth', mode, who)))
            for mode in ('opponent_land', 'nonmana'):
                self.assertIn(dict(op='assert_unused_choices', value=1), self.actions('Regal Behemoth', mode, who))

    def test_trample_separates_lethal_damage_from_overflow(self):
        for who in (0, 1):
            run = self.actions('Regal Behemoth', 'trample', who)
            self.assertIn(dict(op='block', ref='blocker', other='source', value=1), run)
            hit = run.index(dict(op='combat_damage'))
            self.assertIn(dict(op='assert_life', player=1-who, value=17), run[hit:])
            self.assertIn(self.copies('Grizzly Bears', 1-who, 'graveyard', 1), run[hit:])

    def test_decree_counts_attacks_on_players_or_actual_walkers_before_damage(self):
        for who in (0, 1):
            for mode, drain in [('planeswalker', 1), ('fogged', 1), ('attack_twice', 2)]:
                steps = self.actions("Marchesa's Decree", mode, who)
                finish = steps.index(dict(op='finish_attackers'))
                damage = steps.index(dict(op='combat_damage'))
                self.assertIn(dict(op='assert_life', player=1-who, value=20-drain), steps[finish:damage])
                if mode == 'planeswalker':
                    self.assertIn(dict(op='attack_with_tax', ref='enemy_attacker', value=1), steps[:finish])
                    self.assertIn(dict(op='assert_counter', ref='walker', counter_kind='loyalty', value=1), steps[damage:])
                    self.assertIn(dict(op='assert_life', player=who, value=20), steps[damage:])

    def test_queen_enemy_crown_controls_and_actual_assassin_behavior(self):
        for who in (0, 1):
            self.assertIn(dict(op='attack', ref='source', value=1), self.actions('Queen Marchesa', 'haste', who))
            own = self.actions('Queen Marchesa', 'own_upkeep', who)
            self.assertNotIn(self.copies('Assassin', who, 'battlefield', 1), own)
            for mode in ('assassin_haste', 'assassin_touch'):
                steps = self.actions('Queen Marchesa', mode, who)
                upkeep = steps.index(dict(op='upkeep_step', player=who))
                self.assertIn(self.copies('Assassin', who, 'battlefield', 1), steps[upkeep:])
                self.assertIn(dict(op='assert_matching_permanents', player=who, index=18691, property='color', value=2), steps[upkeep:])
                self.assertIn(dict(op='attack', ref='assassin', value=1), steps[upkeep:])
                if mode == 'assassin_touch':
                    self.assertIn(self.copies('Giant Spider', 1-who, 'graveyard', 1), steps[upkeep:])
                    self.assertNotIn(self.copies('Assassin', who, 'graveyard', 1), steps)

    def test_end_step_riders_follow_own_end_and_actual_title(self):
        for who in (0, 1):
            for name in ('Garrulous Sycophant', 'Throne Warden'):
                start = self.actions(name, 'crowned', who)
                end = start.index(dict(op='cleanup'))
                self.assertIn(dict(op='cast', ref='claim', value=1), start[:end])
                self.assertIn(self.copies('Island', who, 'hand', 1), start[end:])
                if name=='Garrulous Sycophant':
                    self.assertIn(dict(op='assert_life', player=who, value=21), start[end:])
                else:
                    self.assertIn(dict(op='assert_card', ref='source', property='plus1_counters', value=1), start[end:])
                lost = self.actions(name, 'lost', who)
                self.assertIn(self.copies('Swamp', 1-who, 'hand', 1), lost)

    def test_throne_real_land_payment_and_colorless_cast_use(self):
        name='Throne of the High City'
        for who in (0, 1):
            steps = self.actions(name, 'paid', who)
            land = steps.index(dict(op='play_land', ref='source', value=1))
            activate = steps.index(dict(op='activate', ref='source', value=1))
            pool = [s['value'] for s in steps[land:activate] if s['op']=='set_mana' and s['player']==who]
            self.assertEqual(pool, [4,0,0,0,0,0])
            self.assertIn(dict(op='assert_stack', value=1), steps[activate:])
            self.assertIn(self.copies(name, who, 'graveyard', 1), steps[activate:])
            self.assertIn(self.copies('Island', who, 'hand', 1), steps[activate:])
            mana = self.actions(name, 'mana', who)
            tap = mana.index(dict(op='activate', ref='source', value=1))
            use = mana.index(dict(op='cast', ref='spend', value=1))
            self.assertFalse(any(s['op']=='set_mana' for s in mana[tap:use]))
            self.assertIn(dict(op='activate', ref='source', value=0), self.actions(name, 'tapped', who))

    def test_replay_uses_identical_main_actions_and_end_step(self):
        for name in self.homes:
            for who in (0, 1):
                steps = self.actions(name, 'replay', who)
                cut, record, restore = [steps.index(dict(op=op)) for op in ('capture','record_state','restore')]
                self.assertEqual(steps[cut+1:record], steps[restore+1:-1])
                self.assertIn(dict(op='cleanup'), steps[cut+1:record])


if __name__ == '__main__':
    unittest.main()
