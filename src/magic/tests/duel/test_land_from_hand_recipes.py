"""Verify actual choice, turn and ownership provenance for land placement."""
import copy
import json
import unittest
from pathlib import Path

import land_from_hand_recipes as recipe
from generate_cases import registry


class LandFromHandRecipes(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        data = Path(__file__).parent / 'data'
        cls.oracle = json.loads((data / 'oracle.json').read_text(encoding='utf8'))['cards']
        cls.rows = {row['name']: row for row in registry()}
        profile = recipe.pins()
        recipe.validate_witnesses(cls.oracle, cls.rows)
        cls.cases = {}
        cls.creatures = set()
        cls.triggers = set()
        for name, pin in profile['sources'].items():
            csvid = pin['csvid']
            path, = (data / 'cases/cards').glob(f'*/{csvid:05d}-*.json')
            declaration, = json.loads(path.read_text(encoding='utf8'))['recipes']
            model = recipe.spec(declaration, cls.oracle[name])
            if model.get('trigger'):
                cls.triggers.add(name)
            elif not model.get('spell'):
                cls.creatures.add(name)
            variants = recipe.variants(model)
            if declaration['variants'] != variants:
                raise AssertionError(name + ' did not enroll the full reviewed matrix')
            cls.cases[name] = {cell['_recipe']['variant']: cell for cell in
                               recipe.expand(name, csvid, declaration, model, variants, cls.rows, cls.oracle)}

    def steps(self, name, mode, player):
        return self.cases[name][f'{mode}_{player}']['steps']

    def test_whole_printed_rules_identity_and_witness_pins(self):
        for name in sorted(self.creatures):
            cells = self.cases[name]
            record = self.oracle[name]
            model = recipe.mechanism(record)
            self.assertEqual(model['artifact'], name == 'Walking Atlas')
            self.assertEqual(model['sorcery'], name == 'Skyshroud Ranger')
            self.assertEqual(cells['choose_basic_0']['_recipe']['covered_clauses'], [record['oracle_text']])
            for text in (record['oracle_text'].replace('You may ', 'You must '),
                         record['oracle_text'].replace('{T}', '{G}'),
                         record['oracle_text'].replace('battlefield.', 'battlefield tapped.')):
                with self.assertRaises(ValueError):
                    recipe.mechanism(dict(record, oracle_text=text))
        changed = copy.copy(self.oracle)
        changed['Kabira Crossroads'] = dict(changed['Kabira Crossroads'], oracle_text='This land enters tapped.')
        with self.assertRaisesRegex(ValueError, 'Kabira Crossroads'):
            recipe.validate_witnesses(changed, self.rows)

    def test_activation_is_paid_before_each_optional_selection(self):
        for name in sorted(self.creatures):
            for player in (0, 1):
                for mode in ('choose_basic', 'choose_artifact', 'decline', 'nonland_only'):
                    steps = self.steps(name, mode, player)
                    activation = steps.index(dict(op='activate', ref='source', value=1))
                    picker = next(i for i, action in enumerate(steps) if action['op'].startswith('choose_'))
                    self.assertLess(activation, picker)
                    self.assertEqual(steps[picker]['op'],
                                     'choose_optional_zone' if mode in ('decline', 'nonland_only') else 'choose_zone_card')
                    self.assertIn(dict(op='assert_card', ref='source', property='tapped', value=1), steps[activation:picker])
                    self.assertIn(dict(op='assert_offered_legal_card', index=self.rows['Shock']['csvid'], value=0), steps)
                    self.assertIn(dict(op='assert_offered_selection_required', value=0), steps)
                    self.assertIn(dict(op='assert_forced_zone_choices', value=0), steps)
                empty = self.steps(name, 'empty', player)
                self.assertIn(dict(op='assert_offered_count', value=0), empty)
                self.assertTrue(all(not step['op'].startswith('choose_') for step in empty))
                self.assertIn(dict(op='assert_card', ref='source', property='tapped', value=1), empty)

    def test_land_timing_and_intrinsic_entry(self):
        for name in sorted(self.creatures):
            for player in (0, 1):
                sorcery = name == 'Skyshroud Ranger'
                for mode in ('opposing_turn', 'combat', 'stack_busy', 'second_main'):
                    steps = self.steps(name, mode, player)
                    activate = dict(op='activate', ref='source', value=int(not sorcery or mode == 'second_main'))
                    self.assertIn(activate, steps)
                    if mode in ('combat', 'second_main'):
                        self.assertEqual([s['op'] for s in steps if s['op'] in ('advance_combat','combat_damage','end_combat')],
                                         ['advance_combat','combat_damage','end_combat'])
                    if mode == 'stack_busy':
                        self.assertLess(steps.index(dict(op='cast', ref='instant', value=1)), steps.index(activate))
                tapped = self.steps(name, 'tapped_entry', player)
                self.assertIn(dict(op='assert_card', ref='land', property='tapped', value=1), tapped)
                self.assertIn(dict(op='assert_life', player=player, value=22), tapped)
                untapped = self.steps(name, 'immediate_mana', player)
                self.assertIn(dict(op='assert_card', ref='land', property='tapped', value=0), untapped)
                self.assertLess(untapped.index(dict(op='activate', ref='land', value=1)),
                                untapped.index(dict(op='cast', ref='instant', value=1)))

    def test_creature_is_fresh_until_the_real_next_own_turn(self):
        for name in sorted(self.creatures):
            for player in (0, 1):
                steps = self.steps(name, 'fresh_then_ready', player)
                start = steps.index(dict(op='cast', ref='source', value=1))
                denied = steps.index(dict(op='activate', ref='source', value=0))
                real_untap = steps.index(dict(op='untap_step', player=player))
                after = steps.index(dict(op='activate', ref='source', value=1))
                self.assertTrue(start < denied < real_untap < after)
                self.assertEqual(sum(s['op'] == 'advance_turn' for s in steps), 2)

    def test_placement_neither_needs_nor_spends_the_normal_land_allowance(self):
        for name in sorted(self.creatures):
            for player in (0, 1):
                for mode in ('before_normal', 'after_normal', 'next_turn_slot'):
                    steps = self.steps(name, mode, player)
                    place = steps.index(dict(op='choose_zone_card', player=player, value=self.rows['Forest']['csvid']))
                    ordinary = steps.index(dict(op='play_land', ref='normal', value=1))
                    denied = steps.index(dict(op='play_land', ref='second', value=0))
                    self.assertEqual(place < ordinary, mode == 'before_normal')
                    self.assertLess(ordinary, denied)
                    if mode == 'next_turn_slot':
                        self.assertLess(steps.index(dict(op='untap_step', player=player)),
                                        steps.index(dict(op='play_land', ref='second', value=1)))

    def test_pending_source_departure_and_control_use_real_opposite_actors(self):
        for name in sorted(self.creatures):
            for player in (0, 1):
                pending = self.steps(name, 'source_leaves_pending', player)
                activated = pending.index(dict(op='activate', ref='source', value=1))
                response = pending.index(dict(op='cast', ref='bounce', value=1))
                selected = pending.index(dict(op='choose_zone_card', player=player, value=self.rows['Forest']['csvid']))
                self.assertTrue(activated < response < selected)
                self.assertIn(dict(op='assert_offered_count', value=2), pending[selected:])
                self.assertIn(dict(op='assert_offered_legal_card', index=self.rows[name]['csvid'], value=0),
                              pending[selected:])
                moved = self.steps(name, 'foreign_controller', player)
                source = self.rows[name]['csvid']
                rebound = moved.index(dict(op='bind_permanent_copy', ref='source', player=1-player, index=0, value=source))
                land = self.rows['Mountain']['csvid']
                selected = moved.index(dict(op='choose_zone_card', player=1-player, value=land))
                returned = moved.index(dict(op='bind_hand_copy', ref='foreign_land', player=1-player, index=0, value=land))
                self.assertTrue(rebound < selected < returned)
                self.assertIn(dict(op='assert_offered_count', value=1), moved)
                self.assertIn(dict(op='assert_zone_multiplicity', player=player, zone='hand', index=self.rows['Forest']['csvid'],
                                   value=1), moved[returned:])

    def test_pending_snapshot_replays_the_same_bound_choice_and_receipts(self):
        for name in sorted(self.creatures):
            for player in (0, 1):
                steps = self.steps(name, 'pending_replay', player)
                ops = [s['op'] for s in steps]
                self.assertLess(ops.index('activate'), ops.index('capture'))
                self.assertEqual(steps[ops.index('capture')+1:ops.index('record_state')],
                                 steps[ops.index('ai_roundtrip')+1:ops.index('assert_recorded_state')])

    def test_spell_prices_whole_clauses_and_normal_allowance(self):
        spells = self.cases.keys()-self.creatures-self.triggers
        self.assertEqual(spells, {'Growth Spiral', 'Swell of Growth', 'Broken Bond'})
        for name in sorted(spells):
            record = self.oracle[name]
            model = recipe.mechanism(record)
            self.assertIn(model['spell'], ('draw', 'pump', 'destroy'))
            for text in (record['oracle_text'].replace('You may ', 'You must '),
                         record['oracle_text'] + ' Draw a card.'):
                with self.assertRaises(ValueError):
                    recipe.mechanism(dict(record, oracle_text=text))
            for player in (0, 1):
                for mode in ('before_normal', 'after_normal'):
                    steps = self.steps(name, mode, player)
                    placement = steps.index(dict(op='choose_zone_card', player=player, value=self.rows['Forest']['csvid']))
                    ordinary = steps.index(dict(op='play_land', ref='normal', value=1))
                    self.assertEqual(placement < ordinary, mode == 'before_normal')
                    self.assertIn(dict(op='play_land', ref='second', value=0), steps)
                for mode in ('cast_short', 'wrong_color'):
                    denied = self.steps(name, mode, player)
                    self.assertIn(dict(op='cast', ref='source', value=0), denied)
                    self.assertFalse(any(s['op'].startswith('choose_') for s in denied))
                    self.assertIn(dict(op='assert_zone_multiplicity', player=player, zone='hand',
                                       index=self.rows[name]['csvid'], value=1), denied)

    def test_growth_spiral_can_select_the_land_drawn_during_resolution(self):
        for player in (0, 1):
            name = 'Growth Spiral'
            for mode, choice, life in (('drawn_land', 'choose_zone_card', 22),
                                       ('drawn_declined', 'choose_optional_zone', 20)):
                cell = self.cases[name][f'{mode}_{player}']
                self.assertIn(dict(name='Kabira Crossroads', ref='drawn', player=player, zone='library'), cell['setup'])
                self.assertFalse(any(entry['zone'] == 'hand' and entry['player'] == player and entry['ref'] != 'source'
                                     for entry in cell['setup']))
                actions = cell['steps']
                self.assertIn(dict(op=choice, player=player, value=self.rows['Kabira Crossroads']['csvid']), actions)
                self.assertIn(dict(op='assert_life', player=player, value=life), actions)
                self.assertIn(dict(op='assert_zone_card', player=player, zone='library', index=0,
                                   value=self.rows['Plains']['csvid']), actions)

    def test_swell_pump_and_broken_bond_survive_optional_decline(self):
        for player in (0, 1):
            for mode in ('decline', 'empty', 'foe_target'):
                actions = self.steps('Swell of Growth', mode, player)
                self.assertIn(dict(op='choose_target', ref='target', player=player), actions)
                expiry = actions.index(dict(op='cleanup'))
                for field in ('power', 'toughness'):
                    self.assertIn(dict(op='assert_card', ref='target', property=field, value=4), actions[:expiry])
                    self.assertIn(dict(op='assert_card', ref='target', property=field, value=2), actions[expiry:])
            for mode, witness, location in (('decline', 'Spellbook', 'graveyard'),
                                             ('target_enchantment', 'Fervor', 'graveyard'),
                                             ('target_indestructible', 'Darksteel Citadel', 'battlefield')):
                actions = self.steps('Broken Bond', mode, player)
                self.assertIn(dict(op='assert_zone_multiplicity', player=1-player, zone=location,
                                   index=self.rows[witness]['csvid'], value=1), actions)
                if mode == 'target_indestructible':
                    self.assertIn(dict(op='choose_zone_card', player=player, value=self.rows['Forest']['csvid']), actions)

    def test_target_loss_and_counter_use_actual_two_spell_stack(self):
        for player in (0, 1):
            for name in ('Swell of Growth', 'Broken Bond'):
                actions = self.steps(name, 'target_gone', player)
                moved = next(i for i, op in enumerate(actions) if op['op'] == 'bind_hand_copy')
                self.assertLess(actions.index(dict(op='resolve_response')), moved)
                self.assertLess(moved, actions.index(dict(op='assert_offered_count', value=0)))
                self.assertFalse(any(s['op'] == 'choose_zone_card' for s in actions))
                self.assertIn(dict(op='assert_zone_multiplicity', player=player, zone='hand',
                                   index=self.rows['Forest']['csvid'], value=1), actions)
            for name in ('Growth Spiral', 'Swell of Growth', 'Broken Bond'):
                actions = self.steps(name, 'countered', player)
                self.assertIn(dict(op='announce_spell', ref='source'), actions)
                self.assertIn(dict(op='cast_response', ref='response', value=1), actions)
                self.assertIn(dict(op='assert_stack', value=2), actions)
                self.assertIn(dict(op='assert_offered_count', value=0), actions)

    def test_trigger_cards_retain_whole_oracle_type_scope_and_destination(self):
        self.assertEqual(self.triggers, {'Burgeoning', 'Loam Dweller'})
        for name in sorted(self.triggers):
            record = self.oracle[name]
            model = recipe.mechanism(record)
            self.assertEqual(model['trigger'], 'opponent_land' if name == 'Burgeoning' else 'spirit_or_arcane')
            self.assertEqual(self.cases[name]['source_short_0']['_recipe']['covered_clauses'], [record['oracle_text']])
            for text in (record['oracle_text'].replace('you may', 'you must'),
                         record['oracle_text'].replace('an opponent plays a land', 'a land enters under your control'),
                         record['oracle_text'].replace('Spirit or Arcane', 'Spirit'),
                         record['oracle_text'].replace('battlefield tapped.', 'battlefield.')):
                if text != record['oracle_text']:
                    with self.assertRaises(ValueError):
                        recipe.mechanism(dict(record, oracle_text=text))
            for player in (0, 1):
                self.assertIn(dict(op='cast', ref='source', value=0), self.steps(name, 'source_short', player))
            self.assertNotIn('trigger_decline_1', self.cases[name])

    def test_burgeoning_tracks_opponent_land_play_and_not_land_placement(self):
        name = 'Burgeoning'
        forest = self.rows['Forest']['csvid']
        for player in (0, 1):
            actions = self.steps(name, 'opponent_play', player)
            event = actions.index(dict(op='play_land', ref='foe_land', value=1))
            selection = actions.index(dict(op='choose_zone_card', player=player, value=forest))
            self.assertLess(selection, event)
            self.assertEqual(dict(op='choose_trigger', ref='source', player=player) in actions[:selection], player == 0)
            self.assertIn(dict(op='bind_permanent_copy', ref='land', player=player, index=0, value=forest), actions[event:])
            for mode in ('own_play', 'opponent_put'):
                quiet = self.steps(name, mode, player)
                self.assertNotIn(dict(op='choose_zone_card', player=player, value=forest), quiet)
                self.assertIn(dict(op='assert_zone_multiplicity', player=player, zone='hand', index=forest, value=1), quiet)
            self.assertIn(dict(op='activate', ref='scout', value=1), self.steps(name, 'opponent_put', player))
            denied = self.steps(name, 'opponent_second_denied', player)
            self.assertIn(dict(op='play_land', ref='extra_foe_land', value=0), denied)
            self.assertEqual(sum(s['op'] == 'choose_zone_card' for s in denied), 1)

    def test_loam_cast_trigger_places_land_before_spell_resolves(self):
        name = 'Loam Dweller'
        for player in (0, 1):
            for mode, witness in (('spirit', 'Wandering Ones'), ('arcane', 'Reach Through Mists')):
                cell = self.cases[name][f'{mode}_{player}']
                self.assertIn(dict(name=witness, ref='cause', player=player, zone='hand'), cell['setup'])
                actions = cell['steps']
                announce = actions.index(dict(op='announce_spell', ref='cause'))
                tapped = actions.index(dict(op='assert_card', ref='land', property='tapped', value=1))
                resolution = next(i for i, op in enumerate(actions[announce+1:], announce+1) if op['op'] == 'resolve')
                self.assertTrue(announce < tapped < resolution)
                self.assertEqual(dict(op='choose_trigger', ref='source', player=player) in actions[:announce], player == 0)
            snapshot = self.steps(name, 'arcane_draw_no_land', player)
            self.assertIn(dict(op='assert_offered_count', value=0), snapshot)
            self.assertNotIn('choose_zone_card', [s['op'] for s in snapshot])
            self.assertIn(dict(op='assert_zone_multiplicity', player=player, zone='hand', value=1,
                               index=self.rows['Kabira Crossroads']['csvid']), snapshot)
            counter = self.steps(name, 'cast_countered', player)
            tapped = counter.index(dict(op='assert_card', ref='land', property='tapped', value=1))
            self.assertLess(tapped, counter.index(dict(op='cast_response', ref='counter', value=1)))
            self.assertIn(dict(op='resolve_response'), counter)
            for mode in ('enemy_spirit', 'non_spirit', 'non_arcane'):
                self.assertIn(dict(op='assert_offered_count', value=0), self.steps(name, mode, player))

    def test_real_trigger_decline_and_entry_receipts_for_both_seats(self):
        for name in sorted(self.triggers):
            for player in (0, 1):
                declined = self.steps(name, 'decline', player)
                forest = self.rows['Forest']['csvid']
                self.assertIn(dict(op='choose_optional_zone', player=player, value=forest), declined)
                self.assertIn(dict(op='assert_zone_multiplicity', player=player, zone='hand', index=forest, value=1), declined)
                entry = self.steps(name, 'tapped_entry', player)
                self.assertIn(dict(op='assert_life', player=player, value=22), entry)
                self.assertIn(dict(op='assert_card', ref='land', property='tapped', value=1), entry)

if __name__ == '__main__':
    unittest.main()
