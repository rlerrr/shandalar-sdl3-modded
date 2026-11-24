"""Focused controls for the shared Devoid input and zone model."""

from copy import deepcopy
import unittest

import devoid_zone_and_spell_choices_recipes as recipe


class DevoidZoneAndSpellChoicesTests(unittest.TestCase):
    def board(self, name, scene, owner=0):
        profile = recipe.profiles()
        all_cards = profile['sources'] | profile['witnesses']
        oracle = {name: pin['oracle'] for name, pin in all_cards.items()}
        registry = {name: pin['registry'] for name, pin in all_cards.items()}
        source = oracle[name]
        model = recipe.classify(source)
        board = recipe.ChoiceBoard(name, model, scene, owner, registry, oracle)
        board.run()
        return board

    def values(self, board, op, **where):
        return [step.get('value') for step in board.steps if step['op'] == op and
                all(step.get(key) == value for key, value in where.items())]

    def test_whole_oracle_exact(self):
        oracle = recipe.profiles()['sources']['Witness the End']['oracle']
        shape = recipe.classify(oracle)
        valid = dict(family=recipe.FAMILY, version=2, params=shape, oracle_text=oracle['oracle_text'], variants=['choose_two_0'])
        self.assertEqual(recipe.spec(valid, oracle)['model'], 'two')
        for changes in ({'version': 2.0}, {'variants': ['invented_0']}, {'oracle_text': 'Devoid'}):
            with self.assertRaises(ValueError):
                recipe.spec(valid | changes, oracle)
        changed = deepcopy(oracle)
        changed['oracle_text'] = changed['oracle_text'].replace('2 life', '3 life')
        with self.assertRaises(ValueError):
            recipe.classify(changed)

    def test_live_stack_color_follows_paid_cast_for_every_model(self):
        modes = {
            'Brutal Expulsion': 'both', 'Salvage Drone': 'unblocked', 'Sire of Stagnation': 'opposing_land',
            'Transgress the Mind': 'boundary', 'Unnatural Aggression': 'both_die',
            'Ruin in Their Wake': 'to_hand', 'Witness the End': 'choose_two',
        }
        for name, scene in modes.items():
            for owner in (0, 1):
                with self.subTest(card=name, seat=owner):
                    board = self.board(name, scene, owner)
                    ops = [s for s in board.steps if s.get('ref') == 'source']
                    cast = next(i for i, s in enumerate(ops) if s['op'] == 'cast')
                    self.assertEqual(ops[cast+1], dict(op='assert_card', ref='source', property='color', value=0))
                    denied = self.board(name, 'short', owner)
                    self.assertEqual(self.values(denied, 'cast', ref='source'), [0])
                    self.assertFalse(self.values(denied, 'assert_card', ref='source', property='color'))

    def test_modal_stack_and_separate_planeswalker(self):
        for owner in (0, 1):
            stack = self.board('Brutal Expulsion', 'stack_bounce', owner)
            self.assertEqual(self.values(stack, 'cast_response', ref='source'), [1])
            self.assertEqual(stack.cards['parent']['zone'], 'hand')
            self.assertEqual(stack.cards['source']['zone'], 'graveyard')
            both = self.board('Brutal Expulsion', 'both', owner)
            self.assertEqual((both.cards['first']['zone'], both.cards['second']['zone']), ('hand', 'exile'))
            walk = self.board('Brutal Expulsion', 'planeswalker', owner)
            self.assertEqual(self.values(walk, 'assert_counter', ref='second', counter_kind='loyalty'), [3, 1])

    def test_damage_replacement_expires_and_is_opponent_only(self):
        for name in ('Brutal Expulsion', 'Unnatural Aggression'):
            for owner in (0, 1):
                self.assertEqual(self.board(name, 'later_death', owner).cards['second']['zone'], 'exile')
                expiry = self.board(name, 'expiry', owner)
                self.assertEqual(expiry.cards['second']['zone'], 'graveyard')
                self.assertEqual(self.values(expiry, 'cleanup'), [None])
        fight = self.board('Unnatural Aggression', 'both_die')
        self.assertEqual(fight.cards['first']['zone'], 'graveyard')
        self.assertEqual(fight.cards['second']['zone'], 'exile')
        self.assertEqual(self.values(self.board('Unnatural Aggression', 'no_own'), 'can_cast', ref='source'), [0])

    def test_combat_and_death_are_distinct_actual_events(self):
        for owner in (0, 1):
            unblocked = self.board('Salvage Drone', 'unblocked', owner)
            self.assertEqual(self.values(unblocked, 'attack', ref='source'), [1])
            self.assertEqual(unblocked.life[1-owner], 19)
            self.assertEqual(unblocked.in_zone(1-owner, 'exile'), [f'library_{1-owner}_0'])
            died = self.board('Salvage Drone', 'blocked', owner)
            self.assertEqual(self.values(died, 'block', ref='blocker'), [1])
            self.assertFalse(died.in_zone(1-owner, 'exile'))
            if owner == 0:
                self.assertEqual(died.cards['discard']['zone'], 'graveyard')
                self.assertIn('library_0_0', died.in_zone(owner, 'hand'))
                self.assertEqual(len(self.values(died, 'choose_zone_card', player=0)), 1)
                self.assertEqual(self.values(died, 'discard_policy'), [0])
            else:
                self.assertNotIn('discard', died.cards)
                self.assertEqual(died.cards['library_1_0']['zone'], 'graveyard')
                self.assertFalse(self.values(died, 'choose_zone_card'))
        decline = self.board('Salvage Drone', 'decline')
        self.assertEqual(decline.cards['discard']['zone'], 'hand')
        self.assertEqual(decline.cards['library_0_0']['zone'], 'library')

    def test_real_land_play_compares_both_seats_and_short_library(self):
        for owner in (0, 1):
            enemy = self.board('Sire of Stagnation', 'opposing_land', owner)
            self.assertEqual(self.values(enemy, 'play_land', ref='entry'), [1])
            self.assertEqual(len(enemy.in_zone(1-owner, 'exile')), 2)
            self.assertEqual(len(enemy.in_zone(owner, 'hand')), 2)
            self.assertEqual(len(self.board('Sire of Stagnation', 'opposing_short_library', owner).in_zone(1-owner,'exile')), 1)
            own = self.board('Sire of Stagnation', 'own_land', owner)
            self.assertEqual(self.values(own, 'play_land', ref='entry'), [1])
            self.assertFalse(own.in_zone(1-owner, 'exile'))
            other = self.board('Sire of Stagnation', 'opposing_nonland', owner)
            self.assertEqual(self.values(other, 'cast', ref='nonland'), [1])
            self.assertFalse(other.in_zone(1-owner, 'exile'))

    def test_threshold_and_affected_player_choice(self):
        for owner in (0, 1):
            threshold = self.board('Transgress the Mind', 'boundary', owner)
            self.assertEqual(self.values(threshold, 'assert_offered_legal_card'), [0, 1, 1])
            self.assertEqual(threshold.cards['candidate_1']['zone'], 'exile')
            self.assertEqual(threshold.cards['candidate_0']['zone'], 'hand')
            no_match = self.board('Transgress the Mind', 'no_qualifying', owner)
            self.assertEqual(self.values(no_match, 'assert_offered_selection_required'), [0])
            self.assertFalse(no_match.in_zone(1-owner,'exile'))
            victim = self.board('Witness the End', 'choose_two', owner)
            self.assertEqual(len(self.values(victim, 'choose_zone_card', player=1-owner)), 2)
            self.assertFalse(self.values(victim, 'choose_player'))
            self.assertEqual(victim.life[1-owner], 18)
            self.assertEqual(len(victim.in_zone(1-owner, 'exile')), 2)

    def test_search_box_includes_nonbasic_rejects_and_real_tapped_entry(self):
        for owner in (0, 1):
            accept = self.board('Ruin in Their Wake', 'wastes_tapped', owner)
            self.assertEqual(self.values(accept, 'assert_offered_legal_card'), [0, 0, 1])
            self.assertEqual(self.values(accept, 'assert_shuffle_count', player=owner), [1])
            self.assertEqual(self.values(accept, 'choose_dialog', player=owner), [0] if owner == 0 else [0, 0])
            decline = self.board('Ruin in Their Wake', 'wastes_decline', owner)
            self.assertEqual(self.values(decline, 'choose_dialog', player=owner), [1] if owner == 0 else [0, 1])
            self.assertEqual(self.values(accept, 'assert_card', ref=f'library_{owner}_2', property='tapped'), [1])
            for mode in ('to_hand', 'wastes_decline', 'enemy_wastes'):
                board = self.board('Ruin in Their Wake', mode, owner)
                self.assertEqual(board.cards[f'library_{owner}_2']['zone'], 'hand')

    def test_spike_distinct_packets_and_target_loss(self):
        for owner in (0, 1):
            spike = self.board('Serpentine Spike', 'triple', owner)
            targets = [s['ref'] for s in spike.steps if s['op'] == 'choose_target']
            self.assertEqual(targets, ['second', 'third', 'fourth'])
            self.assertEqual(spike.in_zone(owner, 'exile'), ['third'])
            self.assertEqual(spike.in_zone(1-owner, 'exile'), ['second', 'fourth'])
            lost = self.board('Serpentine Spike', 'lost_middle', owner)
            self.assertEqual(lost.cards['third']['zone'], 'hand')
            self.assertEqual(lost.in_zone(1-owner, 'exile'), ['second', 'fourth'])
            self.assertEqual(self.board('Serpentine Spike', 'expiry', owner).cards['second']['zone'], 'graveyard')

    def test_inverter_checks_processor_visibility_and_actual_flying(self):
        for owner in (0, 1):
            hidden = self.board('Inverter of Truth', 'pool_exchange', owner)
            self.assertEqual(self.values(hidden, 'can_activate', ref='processor'), [0, 0])
            self.assertEqual(hidden.in_zone(owner, 'hidden_exile'),
                             [f'library_{owner}_{index}' for index in range(3)])
            self.assertEqual(hidden.in_zone(owner, 'library'), ['grave_one', 'grave_two'])
            self.assertEqual(hidden.in_zone(owner, 'exile'), [])
            faceup = self.board('Inverter of Truth', 'public_exile_control', owner)
            self.assertEqual(self.values(faceup, 'can_activate', ref='processor'), [1])
            self.assertEqual(faceup.in_zone(owner, 'exile'), ['ordinary_exile'])
            air = self.board('Inverter of Truth', 'flying', owner)
            self.assertEqual(self.values(air, 'block', ref='ground'), [0])
            self.assertEqual(self.values(air, 'block', ref='air'), [1])
