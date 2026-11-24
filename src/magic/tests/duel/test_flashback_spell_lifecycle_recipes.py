import copy
import json
import unittest
from pathlib import Path

from generate_cases import registry
import flashback_spell_lifecycle_recipes as recipe


class FlashbackLifecycleTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.oracle = json.loads((Path(__file__).parent / 'data/oracle.json').read_text(encoding='utf8'))['cards']
        cls.rows = {row['name']: row for row in registry()}

    def case(self, name='Cackling Counterpart', variant='grave_effect_0'):
        record = self.oracle[name]
        declaration = dict(family=recipe.FAMILY, version=2, oracle_text=record['oracle_text'],
                           params=recipe.classify(record), variants=[variant])
        return recipe.expand(name, self.rows[name]['csvid'], declaration, recipe.spec(declaration, record),
                             [variant], self.rows, self.oracle)[0]

    @staticmethod
    def value(case, operation, **fields):
        return next(step['value'] for step in reversed(case['steps']) if step['op'] == operation
                    and all(step.get(key) == value for key, value in fields.items()))

    def copies(self, case, name, who, zone):
        return self.value(case, 'assert_zone_multiplicity', player=who, zone=zone, index=self.rows[name]['csvid'])

    def test_complete_registered_oracle_and_effect_cohort(self):
        recipe.validate_witnesses(self.oracle, self.rows)
        self.assertEqual({self.rows[name]['csvid'] for name in recipe.pins()['sources']}, {5852, 11897, 11933, 12281})
        rows = copy.deepcopy(self.rows)
        rows['Divine Reckoning']['handler'] = 'card_dummy'
        with self.assertRaises(ValueError):
            recipe.validate_witnesses(self.oracle, rows)

    def test_unmodeled_text_and_flashback_costs_fail_closed(self):
        record = self.oracle['Cackling Counterpart']
        text = record['oracle_text']
        for altered in (text + '\nDraw a card.', text.replace('you control', 'an opponent controls'),
                        text.replace('exile it', 'return it to your hand'), text.replace('{5}{U}{U}', '{X}{U}{U}')):
            with self.assertRaises(ValueError):
                recipe.classify(dict(record, oracle_text=altered))

    def test_real_hand_and_grave_costs_and_destinations(self):
        for name in recipe.pins()['sources']:
            params = recipe.classify(self.oracle[name])
            for mode, cost, zone in (('hand_effect', params['mana'], 'graveyard'),
                                     ('grave_effect', params['flashback_mana'], 'exile')):
                case = self.case(name, mode + '_1')
                seeds = [step['value'] for step in case['steps'][:14] if step['op'] == 'set_mana' and step['player'] == 1]
                self.assertEqual(seeds, cost)
                self.assertEqual(self.copies(case, name, 1, zone), 1)
                self.assertEqual([self.value(case, 'assert_mana', player=1, color=color) for color in range(6)], [0] * 6)

    def test_connected_recast_checks_real_graveyard_location_and_exiles(self):
        for name, slot in [('Cackling Counterpart', 0), ('Parallel Evolution', 0),
                           ('Divine Reckoning', 0), ("Tracker's Instincts", 3)]:
            for who in (0, 1):
                case = self.case(name, f'same_card_recast_{who}')
                steps = case['steps']
                action = next(i for i, step in enumerate(steps) if step['op'] == 'cast_graveyard')
                self.assertTrue(any(step['op'] == 'cast' for step in steps[:action]))
                self.assertIn(dict(op='assert_zone_card', player=who, zone='graveyard', index=slot,
                                   value=self.rows[name]['csvid']), steps[:action])
                self.assertEqual(steps[action]['ref'], 'grave_cast_slot')
                self.assertFalse(any(card['name'] == name and card['zone'] == 'graveyard' for card in case['setup']))
                self.assertEqual(self.copies(case, name, who, 'graveyard'), 0)
                self.assertEqual(self.copies(case, name, who, 'exile'), 1)
                if name == 'Divine Reckoning':
                    self.assertEqual(self.copies(case, 'Hill Giant', who, 'graveyard'), 1)
                    self.assertEqual(self.copies(case, 'Suntail Hawk', 1 - who, 'graveyard'), 1)

    def test_dovescape_counters_real_flashback_and_creates_printed_mv_birds(self):
        self.assertEqual(self.rows['Dovescape']['csvid'], 8198)
        self.assertEqual(self.rows['Bird']['csvid'], 18696)
        for name, mana_value, unchanged in [
                ('Cackling Counterpart', 3, ('Grizzly Bears', 'battlefield', 1)),
                ('Parallel Evolution', 5, ('Soldier', 'battlefield', 1)),
                ('Divine Reckoning', 4, ('Hill Giant', 'battlefield', 1)),
                ("Tracker's Instincts", 2, ('Grizzly Bears', 'hand', 0))]:
            hand = self.case(name, 'hand_countered_0')
            self.assertIn(dict(op='resolve_response'), hand['steps'])
            self.assertEqual(self.copies(hand, name, 0, 'graveyard'), 1)
            for who in (0, 1):
                grave = self.case(name, f'grave_countered_{who}')
                self.assertFalse(any(step['op'] in ('choose_priority', 'resolve', 'resolve_response') for step in grave['steps']))
                self.assertEqual(self.value(grave, 'cast_graveyard', ref='source'), 1)
                self.assertEqual(self.copies(grave, name, who, 'exile'), 1)
                self.assertEqual(self.copies(grave, name, who, 'graveyard'), 0)
                self.assertEqual(self.copies(grave, 'Dovescape', 1 - who, 'battlefield'), 1)
                self.assertEqual(self.copies(grave, 'Bird', who, 'battlefield'), mana_value)
                self.assertEqual(self.copies(grave, 'Bird', 1 - who, 'battlefield'), 0)
                self.assertEqual(self.value(grave, 'assert_matching_permanents', player=who, index=18696, property='color'), 0x24)
                self.assertEqual(self.value(grave, 'assert_matching_permanents', player=who, index=18696, property='subtype_Bird'), 1)
                self.assertEqual(self.copies(grave, unchanged[0], who, unchanged[1]), unchanged[2])
                self.assertEqual([self.value(grave, 'assert_mana', player=who, color=color) for color in range(6)], [0] * 6)

    def test_flashback_timings_cost_denials_shroud_and_exile(self):
        for name in recipe.pins()['sources']:
            params = recipe.classify(self.oracle[name])
            for mode in ('grave_insufficient', 'grave_wrong_color', 'grave_printed_cost_only', 'grave_off_turn'):
                case = self.case(name, mode + '_0')
                allowed = mode == 'grave_off_turn' and params['instant']
                self.assertEqual(self.value(case, 'cast_graveyard', ref='source'), int(allowed))
                self.assertEqual(self.copies(case, name, 0, 'graveyard'), int(not allowed))
            shrouded = self.case(name, 'grave_shrouded_1')
            self.assertEqual(self.copies(shrouded, name, 1, 'exile'), 1)
            spent = self.case(name, 'grave_absent_after_exile_0')
            actions = [step['value'] for step in spent['steps'] if step['op'] == 'cast_graveyard']
            self.assertEqual(actions, [1, 0, 1, 0])

    def test_copy_target_scope_and_target_loss(self):
        for mode in ('grave_no_own_target', 'hand_no_own_target', 'hand_target_lost'):
            case = self.case(variant=mode + '_0')
            self.assertEqual(self.copies(case, 'Suntail Hawk', 1, 'battlefield'), 1)
            self.assertFalse(any(step['op'] == 'choose_target' for step in case['steps']) if mode.endswith('no_own_target') else False)
        stolen = self.case(variant='grave_copy_stolen_0')
        self.assertEqual(self.copies(stolen, 'Grizzly Bears', 1, 'graveyard'), 1)
        self.assertEqual(self.copies(stolen, 'Grizzly Bears', 0, 'battlefield'), 1)
        self.assertEqual(self.value(stolen, 'assert_matching_permanents', player=0,
                                    index=self.rows['Grizzly Bears']['csvid'], property='token'), 1)

    def test_copy_uses_artifact_characteristics_and_does_not_copy_tapped_state(self):
        art = self.case(variant='grave_artifact_copy_1')
        self.assertEqual(self.value(art, 'assert_matching_permanents', player=1,
                                    index=self.rows['Ornithopter']['csvid'], property='artifact'), 1)
        tapped = self.case(variant='hand_copy_tapped_0')
        self.assertEqual(self.value(tapped, 'assert_matching_permanents', player=0,
                                    index=self.rows['Grizzly Bears']['csvid'], property='tapped'), 0)

    def test_parallel_copies_once_for_each_original_both_controllers(self):
        name = 'Parallel Evolution'
        for who in (0, 1):
            case = self.case(name, f'grave_several_tokens_{who}')
            self.assertEqual(self.copies(case, 'Soldier', who, 'battlefield'), 4)
            self.assertEqual(self.copies(case, 'Servo', 1 - who, 'battlefield'), 2)
            self.assertEqual(self.copies(case, 'Grizzly Bears', who, 'battlefield'), 1)
            self.assertEqual(self.copies(case, 'Hill Giant', 1 - who, 'battlefield'), 1)
        artifact = self.case(name, 'grave_noncreature_token_0')
        self.assertEqual(self.copies(artifact, 'Clue', 0, 'battlefield'), 1)

    def test_reckoning_preserves_each_choice_in_active_order_and_real_owner(self):
        name = 'Divine Reckoning'
        case = self.case(name, 'grave_other_survivors_1')
        inputs = [step['player'] for step in case['steps'] if step['op'] == 'choose_target']
        self.assertEqual(inputs, [1, 0, 1, 0])
        self.assertEqual(self.copies(case, 'Hill Giant', 1, 'battlefield'), 1)
        self.assertEqual(self.copies(case, 'Suntail Hawk', 0, 'battlefield'), 1)
        self.assertEqual(self.copies(self.case(name, 'grave_indestructible_1'), 'Darksteel Myr', 1, 'battlefield'), 1)
        self.assertEqual(self.copies(self.case(name, 'grave_actual_owner_1'), 'Hill Giant', 0, 'graveyard'), 1)

    def test_trackers_reveal_actual_prefix_and_filter_for_creatures(self):
        name = "Tracker's Instincts"
        case = self.case(name, 'grave_other_pick_1')
        self.assertEqual(self.copies(case, 'Hill Giant', 1, 'hand'), 1)
        self.assertEqual(self.copies(case, 'Grizzly Bears', 1, 'graveyard'), 1)
        self.assertEqual(self.value(case, 'assert_zone_card', player=1, zone='library', index=0), self.rows['Plains']['csvid'])
        no_eligible = self.case(name, 'grave_no_eligible_0')
        self.assertEqual(self.copies(no_eligible, 'Hill Giant', 0, 'library'), 1)
        self.assertEqual(self.value(no_eligible, 'assert_zone', player=0, zone='library'), 1)
        empty = self.case(name, 'grave_empty_library_0')
        self.assertFalse(any(step['op'] in ('choose_zone', 'choose_zone_card') for step in empty['steps']))

    def test_hand_pending_replay_preserves_paid_stack_state(self):
        steps = self.case("Tracker's Instincts", 'hand_pending_replay_1')['steps']
        capture, record, restore = [next(i for i, step in enumerate(steps) if step['op'] == op)
                                    for op in ('capture', 'record_state', 'restore')]
        self.assertTrue(any(step['op'] == 'cast' for step in steps[:capture]))
        self.assertEqual(steps[capture+1:record], steps[restore+1:-1])
        self.assertTrue(any(step['op'] == 'choose_zone_card' for step in steps[restore+1:]))


if __name__ == '__main__':
    unittest.main()
