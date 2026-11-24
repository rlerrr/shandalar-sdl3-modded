"""Focused checks for actual Megamorph zone and ability actions."""

import copy
import json
from pathlib import Path
import unittest

from generate_cases import registry
import megamorph_zone_effects_recipes as model


DUEL = Path(__file__).resolve().parent


class MegamorphZoneEffectsRecipes(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.oracle = json.loads((DUEL / 'data/oracle.json').read_text(encoding='utf8'))['cards']
        cls.rows = {row['name']: row for row in registry()}
        cls.homes = {}
        for name in model.pins()['sources']:
            csvid = cls.rows[name]['csvid']
            home, = (DUEL / f'data/cases/cards/{csvid // 100:03d}').glob(f'{csvid:05d}-*.json')
            cls.homes[name] = json.loads(home.read_text(encoding='utf8'))

    def steps(self, name, variant):
        home = self.homes[name]
        recipe, = home['recipes']
        params = model.spec(recipe, self.oracle[name])
        case, = model.expand(name, home['csvid'], recipe, params, [variant], self.rows, self.oracle)
        return case['steps']

    def copies(self, steps, name, owner, zone, value):
        self.assertIn(dict(op='assert_zone_multiplicity', player=owner, zone=zone,
                           index=self.rows[name]['csvid'], value=value), steps)

    def test_complete_pinned_oracle_and_typed_costs(self):
        model.validate_witnesses(self.oracle, self.rows)
        self.assertEqual({home['csvid'] for home in self.homes.values()}, {14881, 14883, 14942, 14962, 14997, 15020})
        self.assertEqual(sum(len(home['recipes'][0]['variants']) for home in self.homes.values()), 208)
        for name, home in self.homes.items():
            recipe, = home['recipes']
            record = self.oracle[name]
            params = model.spec(recipe, record)
            self.assertEqual(set(recipe['variants']), set(model.variants(params)))
            changed = copy.deepcopy(recipe)
            changed['params']['flip_mana'][0] += 1
            with self.assertRaises(ValueError):
                model.spec(changed, record)
            with self.assertRaises(ValueError):
                model.parameters(dict(record, oracle_text=record['oracle_text'] + '\nFlying'))

    def test_megamorph_real_price_once_counter_both_seats_and_replay(self):
        for name in self.homes:
            for who in (0, 1):
                steps = self.steps(name, f'flip_{who}')
                action = steps.index(dict(op='activate', ref='source', value=1))
                self.assertIn(dict(op='assert_card', ref='source', property='csvid', value=model.FACE_DOWN), steps[:action])
                self.assertIn(dict(op='assert_counter', ref='source', counter_kind='plus1', value=1), steps[action:])
                denied = self.steps(name, f'flip_short_{who}')
                self.assertIn(dict(op='activate', ref='source', value=0), denied)
                self.assertNotIn(dict(op='assert_counter', ref='source', counter_kind='plus1', value=1), denied)
            check = self.steps(name, 'replay_1')
            capture = check.index(dict(op='capture'))
            record = check.index(dict(op='record_state'))
            restored = check.index(dict(op='restore'))
            self.assertEqual(check[capture+1:record], check[restored+1:-1])

    def test_raptor_reentry_requires_actual_death_and_friendly_flip(self):
        name = 'Deathmist Raptor'
        for owner in (0, 1):
            for mode, expect in [('return_up', 0), ('return_down', 1), ('return_decline', 2)]:
                run = self.steps(name, f'{mode}_{owner}')
                death = run.index(dict(op='cast', ref='removal', value=1))
                flip = run.index(dict(op='activate', ref='trigger', value=1))
                self.copies(run[death:flip], name, owner, 'graveyard', 1)
                self.assertIn(dict(op='choose_dialog', player=owner, value=expect), run[death:flip])
                if expect < 2:
                    self.assertIn(dict(op='bind_permanent', ref='source', player=owner,
                                       value=model.FACE_DOWN if expect else 14881), run[flip:])
                    self.assertIn(dict(op='assert_counter', ref='source', counter_kind='plus1', value=0), run[flip:])
            run = self.steps(name, f'opponent_flip_{owner}')
            flip = run.index(dict(op='activate', ref='trigger', value=1))
            self.copies(run[flip:], name, owner, 'graveyard', 1)
            self.assertFalse(any(s['op'] == 'bind_permanent' for s in run[flip:]))
        run = self.steps(name, 'return_down_reveal_0')
        self.assertIn(dict(op='activate', ref='source', value=1), run)
        self.assertIn(dict(op='assert_counter', ref='source', counter_kind='plus1', value=1), run)

    def test_raptor_deathtouch_is_live_only_face_up(self):
        for mode in ('deathtouch', 'hidden_damage'):
            run = self.steps('Deathmist Raptor', f'{mode}_0')
            self.assertIn(dict(op='block', ref='blocker', other='source', value=1), run)
            self.assertIn(dict(op='combat_damage'), run)
            self.copies(run[run.index(dict(op='combat_damage')):], 'Wall of Stone', 1,
                        'graveyard' if mode == 'deathtouch' else 'battlefield', 1)

    def test_recovery_scopes_and_no_selection_when_illegal(self):
        for who in (0, 1):
            run = self.steps('Den Protector', f'recover_land_{who}')
            self.assertIn(dict(op='choose_zone_card', player=who, value=91), run)
            self.copies(run, 'Forest', who, 'hand', 1)
            run = self.steps('Monastery Loremaster', f'artifact_target_{who}')
            self.assertIn(dict(op='assert_offered_legal_card', index=91, value=0), run)
            self.assertIn(dict(op='assert_offered_legal_card', index=2493, value=0), run)
            self.assertIn(dict(op='choose_zone_card', player=who, value=11586), run)
            self.copies(run, 'Darksteel Relic', who, 'hand', 1)
            for mode in ('empty_grave', 'foe_grave', 'no_match', 'shroud'):
                refused = self.steps('Monastery Loremaster', f'{mode}_{who}')
                self.assertFalse(any(s['op'] == 'choose_zone_card' for s in refused))

    def test_den_threshold_and_ire_menace_through_real_declaration(self):
        for name, selections in [
            ('Den Protector', [('lower_blocker', 0), ('equal_blocker', 1), ('higher_blocker', 1),
                               ('hidden_blocker', 1), ('printed_blocker', 1)]),
            ('Ire Shaman', [('menace_single', 0), ('menace_pair', 1), ('hidden_blocker', 1)]),
        ]:
            for mode, legal in selections:
                run = self.steps(name, f'{mode}_1')
                self.assertIn(dict(op='block', ref='blocker', other='source', value=legal), run)
                if mode == 'menace_pair':
                    self.assertIn(dict(op='block', ref='second_blocker', other='source', value=1), run)
                self.assertIn(dict(op='assert_card', ref='source', property='blocked', value=legal), run)

    def test_ire_real_permission_for_land_and_paid_spell(self):
        for who in (0, 1):
            for mode, card, destination in [('play_land', 'Forest', 'battlefield'),
                                            ('play_spell', 'Reach Through Mists', 'graveyard')]:
                run = self.steps('Ire Shaman', f'{mode}_{who}')
                csvid = self.rows[card]['csvid']
                play = run.index(dict(op='play_exiled', player=who, index=csvid, value=1))
                self.copies(run[:play], card, who, 'exile', 1)
                self.copies(run[play:], card, who, destination, 1)
                self.assertIn(dict(op='play_exiled', player=who, index=csvid, value=0), run[play:])
                if mode == 'play_spell':
                    self.assertIn(dict(op='set_mana', player=who, color=2, value=1), run[:play])
                    self.assertIn(dict(op='assert_mana', player=who, color=2, value=0), run[play:])
                    self.copies(run[play:], 'Island', who, 'hand', 1)

    def test_ire_exact_card_seat_timing_and_expiry_rejections(self):
        for mode, actor, csvid in [('wrong_player', 1, 91), ('wrong_card', 0, 126), ('offturn_land', 0, 91),
                                   ('short_spell', 0, 7353), ('spent_land_drop', 0, 91), ('expiry', 0, 91)]:
            run = self.steps('Ire Shaman', f'{mode}_0')
            failed = run.index(dict(op='play_exiled', player=actor, index=csvid, value=0))
            if mode == 'expiry':
                self.assertIn(dict(op='can_play_exiled', player=0, index=91, value=1), run[:failed])
                self.assertIn(dict(op='cleanup'), run[:failed])
                self.copies(run[failed:], 'Forest', 0, 'exile', 1)

    def test_other_creature_counters_independent_of_megamorph(self):
        for mode, value in [('own_flip', 2), ('opponent_flip', 0), ('hidden_watcher', 0),
                            ('two_watchers', 4), ('megamorph_stack', 3), ('counter_persistence', 2)]:
            run = self.steps('Salt Road Ambushers', f'{mode}_0')
            flip = run.index(dict(op='activate', ref='trigger', value=1))
            self.assertIn(dict(op='assert_counter', ref='trigger', counter_kind='plus1', value=value), run[flip:])
            self.assertNotIn(dict(op='assert_counter', ref='source', counter_kind='plus1', value=value),
                             run[flip:] if value else [])
        run = self.steps('Salt Road Ambushers', 'flip_0')
        self.assertIn(dict(op='assert_counter', ref='source', counter_kind='plus1', value=1), run)
        self.assertNotIn(dict(op='assert_counter', ref='source', counter_kind='plus1', value=3), run)

    def test_shorecrasher_real_paid_blink_and_temporary_pt_directions(self):
        name = 'Shorecrasher Elemental'
        for who in (0, 1):
            run = self.steps(name, f'blink_reset_{who}')
            self.assertEqual(len([s for s in run if s['op'] == 'choose_activation_mode']), 2)
            bind = run.index(dict(op='bind_permanent', ref='source', player=who, value=model.FACE_DOWN))
            self.assertIn(dict(op='assert_counter', ref='source', counter_kind='plus1', value=0), run[bind:])
            self.assertIn(dict(op='assert_counter', ref='source', counter_kind='plus1', value=1), run[bind:])
            for mode, p, t in [('pump_up', 4, 2), ('pump_down', 2, 4)]:
                run = self.steps(name, f'{mode}_{who}')
                self.assertIn(dict(op='assert_card', ref='source', property='power', value=p), run)
                self.assertIn(dict(op='assert_card', ref='source', property='toughness', value=t), run)
            lethal = self.steps(name, f'lethal_shrink_{who}')
            self.copies(lethal, name, who, 'graveyard', 1)


if __name__ == '__main__':
    unittest.main()
