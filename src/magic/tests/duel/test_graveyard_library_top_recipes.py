import copy
import json
import unittest
from pathlib import Path

from generate_cases import registry
import graveyard_library_top_recipes as recipe


class GraveyardLibraryTopTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.oracle = json.loads((Path(__file__).parent / 'data/oracle.json').read_text(encoding='utf8'))['cards']
        cls.rows = {row['name']: row for row in registry()}

    def case(self, name='Gravepurge', variant='effect_0'):
        oracle = self.oracle[name]
        declaration = dict(family=recipe.FAMILY, version=2, oracle_text=oracle['oracle_text'],
                           params=recipe.classify(oracle), variants=[variant])
        return recipe.expand(name, self.rows[name]['csvid'], declaration, recipe.spec(declaration, oracle),
                             [variant], self.rows, self.oracle)[0]

    def value(self, case, op, **fields):
        return next(step['value'] for step in reversed(case['steps']) if step['op'] == op
                    and all(step.get(key) == value for key, value in fields.items()))

    def multiplicity(self, case, name, player, zone):
        return self.value(case, 'assert_zone_multiplicity', index=self.rows[name]['csvid'], player=player, zone=zone)

    def test_complete_pins_keep_all_spells_implemented(self):
        recipe.validate_witnesses(self.oracle, self.rows)
        self.assertEqual(set(recipe.pins()['sources']), {'Reinforcements', 'Bone Harvest', 'Footbottom Feast', 'Frantic Salvage', 'Gravepurge'})
        rows = copy.deepcopy(self.rows)
        rows['Bone Harvest']['handler'] = 'card_dummy'
        with self.assertRaises(ValueError):
            recipe.validate_witnesses(self.oracle, rows)

    def test_unmodeled_clauses_fail_closed(self):
        source = self.oracle['Gravepurge']
        for record in (dict(source, layout='transform'), dict(source, oracle_text=source['oracle_text'] + '\nGain 2 life.'),
                       dict(source, oracle_text=source['oracle_text'].replace('your graveyard', 'a graveyard')),
                       dict(source, oracle_text=source['oracle_text'].replace('top of', 'bottom of'))):
            with self.assertRaises(ValueError):
                recipe.classify(record)

    def test_draw_timing_independent_of_handler_alias(self):
        expected = {'Reinforcements': 'none', 'Gravepurge': 'immediate', 'Footbottom Feast': 'immediate',
                    'Frantic Salvage': 'immediate', 'Bone Harvest': 'next_upkeep'}
        for name, timing in expected.items():
            self.assertEqual(recipe.classify(self.oracle[name])['draw'], timing)
        self.assertEqual(self.rows['Gravepurge']['handler'], self.rows['Footbottom Feast']['handler'])

    def test_immediate_draw_uses_recovered_top_for_both_players(self):
        for player in (0, 1):
            case = self.case(variant=f'effect_{player}')
            self.assertEqual(self.multiplicity(case, 'Grizzly Bears', player, 'hand'), 1)
            self.assertEqual(self.multiplicity(case, 'Grizzly Bears', player, 'graveyard'), 0)
            self.assertEqual(self.multiplicity(case, 'Grizzly Bears', 1 - player, 'graveyard'), 1)
            self.assertEqual(self.value(case, 'assert_zone_card', player=player, zone='library', index=0), self.rows['Mountain']['csvid'])

    def test_legal_zero_targets_draws_original_library_top(self):
        for mode in ('zero_targets', 'wrong_type', 'empty_graveyard', 'other_graveyard', 'graveyard_shroud'):
            case = self.case(variant=mode + '_0')
            self.assertEqual(self.multiplicity(case, 'Mountain', 0, 'hand'), 1)
            self.assertFalse(any(step['op'] == 'choose_zone_card' for step in case['steps']))
        self.assertEqual(self.multiplicity(self.case('Reinforcements', 'zero_targets_0'), 'Mountain', 0, 'hand'), 0)

    def test_shroud_uses_a_witness_with_no_entry_effect(self):
        witness = self.oracle['Silent Gravestone']
        self.assertEqual(witness['oracle_text'].splitlines()[0], "Cards in graveyards can't be the targets of spells or abilities.")
        case = self.case(variant='graveyard_shroud_1')
        self.assertTrue(any(card['name'] == witness['name'] and card['zone'] == 'battlefield' and card['player'] == 0
                            for card in case['setup']))
        self.assertFalse(any(step['op'] in ('choose_zone_card', 'choose_optional_zone') for step in case['steps']))
        self.assertEqual(self.multiplicity(case, 'Mountain', 0, 'hand'), 0)

    def test_printed_order_changes_exact_draw_and_library_top(self):
        forward = self.case(variant='order_forward_0')
        reverse = self.case(variant='order_reverse_0')
        self.assertEqual(self.multiplicity(forward, 'Grizzly Bears', 0, 'hand'), 1)
        self.assertEqual(self.multiplicity(reverse, 'Silvercoat Lion', 0, 'hand'), 1)
        self.assertEqual(self.value(forward, 'assert_zone_card', player=0, zone='library', index=0), self.rows['Silvercoat Lion']['csvid'])
        self.assertEqual(self.value(reverse, 'assert_zone_card', player=0, zone='library', index=0), self.rows['Grizzly Bears']['csvid'])

    def test_reinforcements_cap_differs_from_unlimited_target_set(self):
        capped = self.case('Reinforcements', 'three_of_four_1')
        unlimited = self.case('Gravepurge', 'four_targets_1')
        self.assertEqual(self.multiplicity(capped, 'Grizzly Bears', 1, 'graveyard'), 1)
        self.assertEqual(self.multiplicity(capped, 'Grizzly Bears', 1, 'library'), 3)
        self.assertEqual(self.multiplicity(unlimited, 'Grizzly Bears', 1, 'graveyard'), 0)
        self.assertEqual(self.multiplicity(unlimited, 'Grizzly Bears', 1, 'library'), 3)
        self.assertEqual(self.multiplicity(unlimited, 'Grizzly Bears', 1, 'hand'), 1)

    def test_artifact_creature_is_a_legal_artifact_and_other_type_is_rejected(self):
        case = self.case('Frantic Salvage', 'artifact_creature_1')
        self.assertEqual(self.multiplicity(case, 'Ornithopter', 1, 'hand'), 1)
        observed = self.case('Frantic Salvage', 'mixed_filter_1')
        checks = {step['index']: step['value'] for step in observed['steps'] if step['op'] == 'assert_offered_legal_card'}
        self.assertEqual(checks, {self.rows['Giant Growth']['csvid']: 0, self.rows['Forest']['csvid']: 0,
                                  self.rows['Darksteel Relic']['csvid']: 1})

    def test_lost_target_and_countered_spell_do_not_draw(self):
        lost = self.case(variant='target_lost_0')
        countered = self.case(variant='countered_1')
        for case, player in ((lost, 0), (countered, 1)):
            self.assertTrue(any(step['op'] == 'cast_response' for step in case['steps']))
            self.assertTrue(any(step['op'] == 'resolve_response' for step in case['steps']))
            self.assertEqual(self.multiplicity(case, 'Grizzly Bears', player, 'hand'), 0)
        self.assertEqual(self.multiplicity(lost, 'Grizzly Bears', 0, 'library'), 1)
        self.assertEqual(self.multiplicity(countered, 'Grizzly Bears', 1, 'graveyard'), 1)
        partial = self.case(variant='partially_lost_0')
        self.assertEqual(self.multiplicity(partial, 'Silvercoat Lion', 0, 'hand'), 1)
        self.assertEqual(self.multiplicity(partial, 'Grizzly Bears', 0, 'library'), 1)

    def test_bone_harvest_uses_next_actual_upkeep_and_draws_once(self):
        for mode, active in [('effect', 1), ('off_turn', 0)]:
            case = self.case('Bone Harvest', mode + '_0')
            steps = case['steps']
            upkeep = next(index for index, step in enumerate(steps) if step['op'] == 'upkeep_step')
            self.assertEqual(steps[upkeep]['player'], active)
            self.assertEqual(steps[upkeep-1]['op'], 'untap_step')
            self.assertEqual(steps[upkeep-2]['op'], 'advance_turn')
            self.assertFalse(any(step['op'] == 'assert_zone_multiplicity' and step['zone'] == 'hand'
                                 and step['player'] == 0 and step['index'] == self.rows['Grizzly Bears']['csvid']
                                 and step['value'] for step in steps[:upkeep]))
            self.assertEqual(self.multiplicity(case, 'Grizzly Bears', 0, 'hand'), 1)
            self.assertEqual(self.multiplicity(case, 'Mountain', 0, 'hand'), 0)

    def test_wrong_mana_never_queues_a_graveyard_choice(self):
        for name in recipe.pins()['sources']:
            for mode in ('wrong_color', 'insufficient_mana'):
                case = self.case(name, mode + '_0')
                self.assertIn(dict(op='cast', ref='source', value=0), case['steps'])
                self.assertFalse(any(step['op'] in ('choose_zone_card', 'choose_optional_zone', 'resolve') for step in case['steps']))
                self.assertEqual(self.multiplicity(case, name, 0, 'hand'), 1)

    def test_snapshot_replays_exact_real_resolution_from_pending_stack(self):
        case = self.case('Bone Harvest', 'pending_replay_1')
        steps = case['steps']
        first, record, restore = [next(i for i, step in enumerate(steps) if step['op'] == op)
                                  for op in ('capture', 'record_state', 'restore')]
        self.assertTrue(any(step['op'] == 'announce_spell' for step in steps[:first]))
        self.assertEqual(steps[first+1:record], steps[restore+1:-1])
        self.assertFalse(any(step['op'] == 'cast' for step in steps[restore+1:]))


if __name__ == '__main__':
    unittest.main()
