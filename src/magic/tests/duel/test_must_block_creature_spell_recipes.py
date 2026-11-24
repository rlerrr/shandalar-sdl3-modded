"""Focused guards for printed forced-block effects and real production phases."""

import copy
import json
import unittest
from pathlib import Path

import must_block_creature_spell_recipes as model
from generate_cases import registry


DUEL = Path(__file__).parent


class MustBlockCreatureSpellRecipes(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.oracle = json.loads((DUEL / 'data/oracle.json').read_text(encoding='utf8'))['cards']
        cls.rows = {row['name']: row for row in registry()}
        cls.homes = {}
        for name in model.profile()['sources']:
            csvid = cls.rows[name]['csvid']
            home, = (DUEL / f'data/cases/cards/{csvid // 100:03d}').glob(f'{csvid:05d}-*.json')
            cls.homes[name] = json.loads(home.read_text(encoding='utf8'))

    def case(self, name, variant):
        home = self.homes[name]
        recipe, = home['recipes']
        params = model.spec(recipe, self.oracle[name])
        case, = model.expand(name, home['csvid'], recipe, params, [variant], self.rows, self.oracle)
        return case

    def first_run(self, name, variant):
        steps = self.case(name, variant)['steps']
        return steps[:steps.index(dict(op='record_state'))]

    @staticmethod
    def index(steps, op):
        return next(i for i, step in enumerate(steps) if step['op'] == op)

    def has_count(self, run, player, zone, name, value):
        return dict(op='assert_zone_multiplicity', player=player, zone=zone, index=self.rows[name]['csvid'], value=value) in run

    def test_full_oracle_and_all_exact_variants(self):
        model.validate_witnesses(self.oracle, self.rows)
        self.assertEqual({home['csvid'] for home in self.homes.values()}, {2862, 4346, 5690, 5720, 6626, 6924, 7745, 7907, 9790, 13758, 14353})
        total = 0
        for name, home in self.homes.items():
            recipe, = home['recipes']
            self.assertEqual(recipe['oracle_text'], self.oracle[name]['oracle_text'])
            params = model.spec(recipe, self.oracle[name])
            self.assertEqual(recipe['variants'], model.variants(params))
            total += len(recipe['variants'])
        self.assertEqual(total, 274)

    def test_mutated_cost_and_extra_clauses_are_rejected(self):
        name = 'Roar of Challenge'
        recipe, = self.homes[name]['recipes']
        changed = copy.deepcopy(recipe)
        changed['params']['ferocious'] = 3
        with self.assertRaises(ValueError):
            model.spec(changed, self.oracle[name])
        for name in ('Nemesis Mask', "Shinen of Life's Roar", 'Mortipede'):
            with self.assertRaises(ValueError):
                model.classify(self.oracle[name] | {'oracle_text': self.oracle[name]['oracle_text'] + '\nHaste'})

    def test_real_block_selection_competing_and_ineligible(self):
        for seat in (0, 1):
            run = self.first_run('Alluring Scent', f'competing_{seat}')
            begin = self.index(run, 'begin_blockers')
            self.assertIn(dict(op='choose_target', ref='blocker', player=1-seat), run[:begin])
            self.assertIn(dict(op='block', ref='blocker', other='ally', value=0), run)
            self.assertIn(dict(op='assert_card', ref='carrier', property='blocked', value=1), run)
            self.assertIn(dict(op='assert_card', ref='ally', property='blocked', value=0), run)
            self.assertIn(dict(op='assert_life', player=1-seat, value=18), run)
            for mode in ('all_tapped', 'illegal_blocker'):
                run = self.first_run('Taunting Challenge', f'{mode}_{seat}')
                self.assertIn(dict(op='block', ref='blocker', other='carrier', value=0), run)
                self.assertIn(dict(op='assert_life', player=1-seat, value=17), run)

    def test_multiple_blockers_and_human_damage_choices(self):
        for seat in (0, 1):
            run = self.first_run('Alluring Scent', f'force_two_{seat}')
            begin, damage = (self.index(run, op) for op in ('begin_blockers', 'combat_damage'))
            self.assertEqual(run[begin-2:begin], [dict(op='choose_target', ref=ref, player=1-seat) for ref in ('blocker', 'blocker_two')])
            self.assertIn(dict(op='assert_card', ref='blocker_two', property='blocking_count', value=1), run)
            human = [dict(op='choose_target', ref='blocker', player=seat)] * 3 if seat == 0 else []
            self.assertEqual([row for row in run[begin:damage] if row['op'] == 'choose_target'], human)
            self.assertTrue(self.has_count(run[damage:], seat, 'graveyard', 'Hill Giant', 1))

    def test_target_permission_and_temporal_window(self):
        for seat in (0, 1):
            run = self.first_run('Bloodscent', f'instant_defending_{seat}')
            self.assertIn(dict(op='turn', player=1-seat, value=0), run)
            lo, hi = (self.index(run, op) for op in ('finish_attackers', 'begin_blockers'))
            self.assertIn(dict(op='cast', ref='source', value=1), run[lo:hi])
            self.assertIn(dict(op='choose_target', ref='carrier', player=seat), run[lo:hi])
            only_shroud = self.first_run('Taunting Challenge', f'shroud_target_{seat}')
            self.assertIn(dict(op='can_cast', ref='source', value=0), only_shroud)
            self.assertFalse(any(row['op'] == 'choose_target' for row in only_shroud))

    def test_equip_uses_real_cost_replacement_and_sorcery_gate(self):
        for seat in (0, 1):
            run = self.first_run('Nemesis Mask', f'cast_equip_{seat}')
            cast, equip = (self.index(run, op) for op in ('cast', 'activate'))
            self.assertIn(dict(op='set_mana', player=seat, color=0, value=6), run[:cast])
            self.assertIn(dict(op='assert_mana', player=seat, color=0, value=3), run[cast:equip])
            self.assertIn(dict(op='assert_mana', player=seat, color=0, value=0), run[equip:])
            move = self.first_run('Nemesis Mask', f'retarget_equip_{seat}')
            self.assertIn(dict(op='choose_target', ref='ally', player=seat), move)
            self.assertIn(dict(op='assert_card', ref='carrier', property='blocked', value=0), move)
            self.assertIn(dict(op='assert_card', ref='ally', property='blocked', value=1), move)
            late = self.first_run('Nemesis Mask', f'during_combat_{seat}')
            cut = self.index(late, 'finish_attackers')
            self.assertIn(dict(op='can_activate', ref='source', value=0), late[cut:])

    def test_channel_pays_discards_and_resolves_separately_from_static_creature(self):
        name = "Shinen of Life's Roar"
        for seat in (0, 1):
            run = self.first_run(name, f'channel_{seat}')
            first, act, resolve = (self.index(run, op) for op in ('choose_dialog', 'activate_hand', 'resolve'))
            self.assertEqual(run[first:act], [dict(op='choose_dialog', player=seat, value=1), dict(op='choose_target', ref='carrier', player=seat)])
            self.assertIn(dict(op='set_mana', player=seat, color=3, value=2), run[:first])
            self.assertTrue(self.has_count(run[act:resolve], seat, 'graveyard', name, 1))
            static = self.first_run(name, f'cast_then_force_{seat}')
            self.assertIn(dict(op='cast', ref='source', value=1), static)
            self.assertIn(dict(op='assert_card', ref='source', property='blocked', value=1), static)
            self.assertFalse(any(row['op'] == 'activate_hand' for row in static))

    def test_mortipede_pays_green_without_tapping_for_ability_and_expires(self):
        for seat in (0, 1):
            run = self.first_run('Mortipede', f'activate_after_attack_{seat}')
            attack = self.index(run, 'attack')
            self.assertIn(dict(op='set_mana', player=seat, color=3, value=1), run[:attack])
            self.assertIn(dict(op='activate', ref='source', value=1), run[attack:])
            expired = self.first_run('Mortipede', f'after_cleanup_{seat}')
            self.assertEqual([row['op'] for row in expired].count('untap_step'), 2)
            self.assertIn(dict(op='assert_card', ref='source', property='blocked', value=0), expired)

    def test_ferocious_reads_actual_controller_and_resolution_boundary(self):
        for seat in (0, 1):
            for mode in ('power_three', 'power_four', 'enemy_power_four', 'support_lost', 'support_after'):
                run = self.first_run('Roar of Challenge', f'{mode}_{seat}')
                damage = self.index(run, 'combat_damage')
                survives = mode in ('power_four', 'support_after')
                self.assertTrue(self.has_count(run[damage:], seat, 'graveyard', 'Hill Giant', int(not survives)))
                if mode.startswith('support_'):
                    kill = run.index(dict(op='destroy', ref='support'))
                    self.assertEqual(kill < self.index(run, 'resolve'), mode == 'support_lost')
            expired = self.first_run('Roar of Challenge', f'destroy_after_expiry_{seat}')
            self.assertGreater(expired.index(dict(op='destroy', ref='carrier')), self.index(expired, 'cleanup'))
            self.assertTrue(self.has_count(expired, seat, 'graveyard', 'Hill Giant', 1))


    def test_setons_desire_checks_aura_controller_not_enchanted_controller(self):
        for seat in (0, 1):
            name = "Seton's Desire"
            for mode, forced in [('threshold_six', False), ('threshold_seven', True), ('enemy_seven', False),
                                 ('foreign_host', True), ('foreign_enemy_seven', False)]:
                run = self.first_run(name, f'{mode}_{seat}')
                self.assertIn(dict(op='assert_card', ref='host', property='power', value=5), run)
                self.assertIn(dict(op='assert_card', ref='host', property='blocked', value=int(forced)), run)
            removed = self.first_run(name, f'aura_removed_{seat}')
            destroy = removed.index(dict(op='destroy', ref='source'))
            self.assertIn(dict(op='assert_card', ref='host', property='power', value=3), removed[destroy:])

    def test_basilisk_checks_independent_combat_damage_and_end_combat(self):
        name = 'Stone-Tongue Basilisk'
        for seat in (0, 1):
            for mode in ('six_manual', 'seven_forced'):
                run = self.first_run(name, f'{mode}_{seat}')
                damage, end = (self.index(run, op) for op in ('combat_damage', 'end_combat'))
                self.assertIn(dict(op='assert_card', ref='blocker', property='damage', value=4), run[damage:end])
                self.assertTrue(self.has_count(run[damage:end], 1-seat, 'battlefield', 'Wall of Stone', 1))
                self.assertTrue(self.has_count(run[end:], 1-seat, 'graveyard', 'Wall of Stone', 1))
                self.assertFalse(any(row['op'] == 'destroy' for row in run))
            fog = self.first_run(name, f'prevented_{seat}')
            self.assertIn(dict(op='cast', ref='fog', value=1), fog)
            self.assertTrue(self.has_count(fog[self.index(fog, 'end_combat'):], 1-seat, 'battlefield', 'Wall of Stone', 1))
            indestructible = self.first_run(name, f'indestructible_{seat}')
            self.assertTrue(self.has_count(indestructible[self.index(indestructible, 'end_combat'):],
                                           1-seat, 'battlefield', 'Darksteel Myr', 1))

    def test_gift_independent_color_bonuses_deadliness_and_payment(self):
        for seat in (0, 1):
            for mode, power, color, forced, killed in [('black_paid_b',3,1,False,True), ('green_paid_g',3,3,True,False),
                   ('both_paid_b',4,1,True,True), ('neither_paid_g',1,3,False,False)]:
                run = self.first_run('Gift of the Deity', f'{mode}_{seat}')
                self.assertIn(dict(op='set_mana', player=seat, color=color, value=1), run)
                self.assertIn(dict(op='assert_card', ref='host', property='power', value=power), run)
                self.assertIs(any(row['op'] == 'choose_target' and row['ref'] == 'blocker' for row in run), forced)
                self.assertTrue(self.has_count(run[self.index(run, 'combat_damage'):], 1-seat, 'graveyard', 'Wall of Stone', int(killed)))

    def test_noble_quarry_bestow_identity_bonus_and_detachment(self):
        name = 'Noble Quarry'
        for seat in (0, 1):
            bestowed = self.first_run(name, f'bestow_{seat}')
            self.assertIn(dict(op='set_mana', player=seat, color=0, value=5), bestowed)
            self.assertIn(dict(op='choose_dialog', player=seat, value=0), bestowed)
            self.assertIn(dict(op='assert_card', ref='source', property='csvid', value=13759), bestowed)
            self.assertIn(dict(op='assert_card', ref='host', property='power', value=4), bestowed)
            self.assertIn(dict(op='assert_card', ref='host', property='blocked', value=1), bestowed)
            detach = self.first_run(name, f'detach_{seat}')
            destroy = detach.index(dict(op='destroy', ref='host'))
            self.assertIn(dict(op='assert_card', ref='source', property='csvid', value=13758), detach[destroy:])
            self.assertIn(dict(op='assert_card', ref='source', property='blocked', value=1), detach[destroy:])
            self.assertIn(dict(op='attack', ref='source', value=1), detach[destroy:])


if __name__ == '__main__':
    unittest.main()
