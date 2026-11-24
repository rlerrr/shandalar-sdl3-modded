import json
import unittest
from pathlib import Path

from chosen_type_effects_recipes import FAMILY, classify, expand, profiles, spec, validate_witnesses, variants
from generate_cases import registry
from recipes import COLORS


class ChosenTypeEffectsRecipesTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.oracle = json.loads((Path(__file__).parent / 'data/oracle.json').read_bytes())['cards']
        cls.registry = {row['name']:row for row in registry()}

    def case(self, name, scenario, owner):
        row = self.oracle[name]
        variant = f'{scenario}_{owner}'
        declaration = dict(family=FAMILY, version=2, oracle_text=row['oracle_text'], params=classify(row), variants=[variant])
        case, = expand(name, self.registry[name]['csvid'], declaration, spec(declaration, row), [variant], self.registry, self.oracle)
        return case

    def actions(self, case, op, **filters):
        return [action for action in case['steps'] if action['op'] == op
                and all(action.get(key) == value for key, value in filters.items())]

    def values(self, case, op, **filters):
        return [action['value'] for action in self.actions(case, op, **filters)]

    def last(self, case, op, **filters):
        return self.values(case, op, **filters)[-1]

    def test_exact_whole_oracle_and_typed_declarations(self):
        validate_witnesses(self.oracle, self.registry)
        names = profiles()['sources']
        self.assertEqual({row['registry']['csvid'] for row in names.values()},
                         {11771, 9347, 3987, 6257, 6285, 4206, 11719, 16751, 17577, 2175})
        self.assertEqual(sum(len(variants(classify(self.oracle[name]))) for name in names), 204)
        for name in names:
            row = self.oracle[name]
            with self.assertRaises(ValueError):
                classify(dict(row, oracle_text=row['oracle_text'] + '\nDraw a card.'))
            with self.assertRaises(ValueError):
                spec(dict(family=FAMILY, version=2.0, oracle_text=row['oracle_text'], params=classify(row), variants=['goblins_0']), row)

    def test_native_entry_choice_is_paid_and_seat_specific(self):
        for name in profiles()['sources']:
            for owner in (0, 1):
                for scene, ref in (('goblins', 'goblin_model'), ('angels', 'angel_model')):
                    case = self.case(name, scene, owner)
                    self.assertEqual(self.values(case, 'choose_dialog', player=owner), [2, 0])
                    choose, = self.actions(case, 'choose_target', ref=ref, player=owner)
                    cast, = self.actions(case, 'cast', ref='source')
                    self.assertLess(case['steps'].index(cast), case['steps'].index(choose))
                for scene in ('short', 'countered'):
                    denied = self.case(name, scene, owner)
                    self.assertFalse(self.actions(denied, 'choose_dialog'))

    def test_automaton_keeps_construct_and_buffs_only_other_own_matches(self):
        for owner in (0, 1):
            case = self.case('Adaptive Automaton', 'goblins', owner)
            self.assertEqual(self.last(case, 'assert_card', ref='source', property='power'), 2)
            self.assertEqual(self.last(case, 'assert_card', ref='matching_own', property='power'), 4)
            self.assertEqual(self.last(case, 'assert_card', ref='matching_foe', property='power'), 3)
            for kind, value in (('Construct', 1), ('Goblin', 1), ('Angel', 0)):
                self.assertEqual(self.last(case, 'assert_matching_permanents', player=owner, property=f'subtype_{kind}'), value)
            truth = self.case('Shared Triumph', 'goblins', owner)
            self.assertEqual(self.last(truth, 'assert_card', ref='matching_foe', property='power'), 4)

    def test_plague_reaps_lethal_matches_on_entry_and_late_cast(self):
        witness = self.registry['Mogg Fanatic']['csvid']
        for owner in (0, 1):
            lethal = self.case('Engineered Plague', 'lethal', owner)
            for seat in (0, 1):
                self.assertEqual(self.last(lethal, 'assert_zone_multiplicity', player=seat, zone='graveyard', index=witness), 1)
            self.assertEqual(self.last(lethal, 'assert_card', ref='matching_foe', property='toughness'), 1)
            late = self.case('Engineered Plague', 'late_lethal', owner)
            self.assertEqual(self.values(late, 'cast', ref='member'), [1])
            self.assertEqual(self.last(late, 'assert_zone_multiplicity', player=owner, zone='graveyard', index=witness), 2)

    def test_shroud_blocks_both_controllers_but_not_untargeted_damage(self):
        for owner in (0, 1):
            for scene, op in (('goblins', 'can_target_creature'), ('ability_scope', 'probe_activation_target')):
                case = self.case('Steely Resolve', scene, owner)
                self.assertEqual(self.values(case, op, other='matching_own'), [0, 0])
                self.assertEqual(self.values(case, op, other='matching_foe'), [0, 0])
                self.assertEqual(self.values(case, op, other='angel_model'), [1, 1])
            removed = self.case('Steely Resolve', 'source_removed', owner)
            self.assertEqual(self.values(removed, 'can_target_creature', other='matching_foe'), [0, 0, 1, 1])
            burned = self.case('Steely Resolve', 'nontargeted_damage', owner)
            self.assertEqual(self.values(burned, 'cast', ref='burn'), [1])
            self.assertEqual(self.last(burned, 'assert_zone_multiplicity', player=1-owner, zone='graveyard',
                                       index=self.registry['Goblin Roughrider']['csvid']), 1)

    def test_door_counts_real_matching_casts_and_countered_kindred_paths(self):
        for owner in (0, 1):
            for scene, charges in (('matching_casts', 2), ('kindred', 1), ('countered_match', 1),
                                   ('opposing_cast', 0), ('nonmatching_cast', 0), ('failed_cast', 0), ('turn_persist', 1)):
                door = self.case('Door of Destinies', scene, owner)
                self.assertEqual(self.last(door, 'assert_counter', ref='source', counter_kind='charge'), charges)
                self.assertEqual(self.last(door, 'assert_card', ref='matching_own', property='power'), 3+charges)
                self.assertEqual(self.last(door, 'assert_card', ref='matching_foe', property='power'), 3)
            countered = self.case('Door of Destinies', 'countered_match', owner)
            self.assertTrue(self.actions(countered, 'announce_spell', ref='member'))
            self.assertEqual(self.values(countered, 'cast_response', ref='reply'), [1])

    def test_incubator_reduces_only_generic_for_matching_creatures_both_seats(self):
        for owner in (0, 1):
            for scene, expected, caster, generic, red in (
                ('late_match', 1, owner, 0, 1), ('opposing_discount', 1, 1-owner, 0, 1),
                ('short_on_discount', 0, owner, 0, 1), ('colored_floor', 0, owner, 1, 0),
                ('floor_is_zero', 1, owner, 0, 1), ('kindred_not_discounted', 0, owner, 0, 1),
                ('nonmatching', 0, owner, 0, 0), ('remaining_generic', 1, owner, 1, 1),
            ):
                case = self.case("Urza's Incubator", scene, owner)
                self.assertEqual(self.values(case, 'cast', ref='member'), [expected])
                self.assertEqual(self.last(case, 'set_mana', player=caster, color=0), generic)
                self.assertEqual(self.last(case, 'set_mana', player=caster, color=COLORS['R']), red)

    def test_closed_replay_and_one_copy_alternate_type(self):
        for owner in (0, 1):
            for name in profiles()['sources']:
                case = self.case(name, 'replay', owner)
                self.assertEqual(self.values(case, 'choose_dialog', player=owner), [2, 0, 2, 0])
                self.assertTrue(self.actions(case, 'assert_recorded_state'))
            angels = self.case('Shared Triumph', 'angels', owner)
            self.assertEqual(self.last(angels, 'assert_card', ref='angel_foe', property='power'), 5)
            self.assertEqual(self.last(angels, 'assert_card', ref='matching_foe', property='power'), 3)

    def test_xenograft_preserves_old_types_and_tracks_controller(self):
        angel = self.registry['Serra Angel']['csvid']
        drake = self.registry['Wind Drake']['csvid']
        for owner in (0, 1):
            late = self.case('Xenograft', 'late_nonmatch', owner)
            self.assertEqual(self.last(late, 'assert_matching_permanents', player=owner, index=drake, property='subtype_Goblin'), 1)
            self.assertEqual(self.last(late, 'assert_matching_permanents', player=owner, index=drake, property='subtype_Drake'), 1)
            self.assertEqual(self.last(late, 'assert_matching_permanents', player=1-owner, index=angel, property='subtype_Goblin'), 0)
            removed = self.case('Xenograft', 'source_removed', owner)
            self.assertEqual(self.values(removed, 'assert_matching_permanents', player=owner, index=angel, property='subtype_Goblin'),
                             [0, 1, 0, 0])

    def test_mimic_counter_is_permanent_and_own_matching_only(self):
        for owner in (0, 1):
            removed = self.case('Metallic Mimic', 'source_removed', owner)
            self.assertEqual(self.values(removed, 'assert_card', ref='member', property='plus1_counters'), [1, 1, 1])
            self.assertEqual(self.last(removed, 'assert_card', ref='second', property='plus1_counters'), 0)
            for scene in ('nonmatching_entry', 'opposing_entry'):
                case = self.case('Metallic Mimic', scene, owner)
                self.assertEqual(self.last(case, 'assert_card', ref='member', property='plus1_counters'), 0)
            shape = self.case('Metallic Mimic', 'original_type', owner)
            self.assertEqual([action['ref'] for action in self.actions(shape, 'choose_target', player=owner)],
                             ['goblin_model', 'shape_model'])
            self.assertEqual(self.last(shape, 'assert_card', ref='source', property='power'), 3)
            self.assertEqual(self.last(shape, 'assert_matching_permanents', player=owner, property='subtype_Goblin'), 1)

    def test_banner_draws_on_real_cast_even_if_countered(self):
        for owner in (0, 1):
            name = "Vanquisher's Banner"
            for scene, remaining in (('matching_casts', 1), ('countered_match', 2), ('kindred', 3), ('opposing_cast', 3),
                                     ('nonmatching_cast', 3), ('source_removed', 3), ('failed_cast', 3)):
                case = self.case(name, scene, owner)
                self.assertEqual(self.last(case, 'assert_zone', player=owner, zone='library'), remaining)
                self.assertEqual(self.last(case, 'assert_zone', player=1-owner, zone='library'), 3)
            case = self.case(name, 'countered_match', owner)
            self.assertEqual(self.values(case, 'cast_response', ref='reply'), [1])
            self.assertEqual(self.last(case, 'assert_card', ref='matching_own', property='power'), 4)
            self.assertEqual(self.last(case, 'assert_card', ref='matching_foe', property='power'), 3)
            announced, = self.actions(case, 'announce_spell', ref='member')
            cutoff = case['steps'].index(announced)
            response, = self.actions(case, 'cast_response', ref='reply')
            window = dict(case, steps=case['steps'][cutoff:case['steps'].index(response)])
            self.assertEqual(self.values(window, 'assert_card', ref='member', property='in_hand'), [0])
            self.assertEqual(self.values(window, 'assert_zone', player=owner, zone='library'), [2])
            self.assertEqual(self.values(window, 'assert_zone_multiplicity', player=owner, zone='hand',
                                         index=self.registry['Island']['csvid']), [1])

    def test_ruins_uses_real_untaps_and_accepts_manual_untap(self):
        for owner in (0, 1):
            name = 'An-Zerrin Ruins'
            case = self.case(name, 'goblins', owner)
            self.assertEqual([action['player'] for action in self.actions(case, 'untap_step')], [1-owner, owner])
            self.assertEqual(self.last(case, 'assert_card', ref='matching_own', property='tapped'), 1)
            self.assertEqual(self.last(case, 'assert_card', ref='matching_foe', property='tapped'), 1)
            self.assertEqual(self.last(case, 'assert_card', ref='angel_model', property='tapped'), 0)
            manual = self.case(name, 'manual_untap', owner)
            self.assertEqual(self.values(manual, 'cast', ref='twiddle'), [1])
            self.assertEqual(self.last(manual, 'assert_card', ref='matching_own', property='tapped'), 0)
            self.assertEqual(self.last(manual, 'assert_card', ref='matching_foe', property='tapped'), 1)
            removed = self.case(name, 'source_removed', owner)
            self.assertEqual(self.last(removed, 'assert_card', ref='matching_own', property='tapped'), 0)
            self.assertEqual(self.last(removed, 'assert_card', ref='matching_foe', property='tapped'), 0)
            late = self.case(name, 'late_match', owner)
            member, = [entry for entry in late['setup'] if entry['ref'] == 'member']
            self.assertEqual(member['zone'], 'hand')
            self.assertNotIn('tapped', member.get('state', []))
            self.assertEqual(self.values(late, 'cast', ref='member'), [1])
            self.assertEqual(self.last(late, 'assert_card', ref='member', property='tapped'), 0)
