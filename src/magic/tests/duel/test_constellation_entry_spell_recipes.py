import json
import unittest
from pathlib import Path

from constellation_entry_spell_recipes import (
    COINSMITH, ENCHANTMENT_CREATURE, FAMILY, classify, expand, profiles, spec, validate_witnesses, variants,
)
from generate_cases import registry
from recipes import COLORS


class ConstellationEntrySpellRecipesTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.oracle = json.loads((Path(__file__).parent / 'data/oracle.json').read_bytes())['cards']
        cls.registry = {row['name']: row for row in registry()}

    def declaration(self, name, variant):
        record = self.oracle[name]
        return dict(family=FAMILY, version=2, oracle_text=record['oracle_text'],
                    params=classify(record), variants=[variant])

    def case(self, name, mode, owner):
        variant = f'{mode}_{owner}'
        d = self.declaration(name, variant)
        case, = expand(name, self.registry[name]['csvid'], d, spec(d, self.oracle[name]),
                       [variant], self.registry, self.oracle)
        return case

    def actions(self, case, op, **fields):
        return [s for s in case['steps'] if s['op'] == op and all(s.get(k) == v for k, v in fields.items())]

    def values(self, case, op, **fields):
        return [s['value'] for s in self.actions(case, op, **fields)]

    def final_card(self, case, ref, prop):
        return self.values(case, 'assert_card', ref=ref, property=prop)[-1]

    def test_complete_pinned_profiles_and_typed_grammar(self):
        validate_witnesses(self.oracle, self.registry)
        scope = {13877, 13894, 13903, 13941, 13995, 14002, 14004}
        pins = profiles()['sources']
        self.assertEqual({pin['registry']['csvid'] for pin in pins.values()},
                         scope | {13878, 13906, 13910, 13989, 13997})
        pilot = [n for n, pin in pins.items() if pin['registry']['csvid'] in scope]
        self.assertEqual(sum(len(variants(classify(self.oracle[n]))) for n in pilot), 270)
        for name in profiles()['sources']:
            d = self.declaration(name, 'self_entry_0')
            record = self.oracle[name]
            with self.assertRaises(ValueError):
                spec(dict(d, version=2.0), record)
            with self.assertRaises(ValueError):
                power = d['params']['power']
                spec(dict(d, params=dict(d['params'], power=0 if power is None else float(power))), record)
            with self.assertRaises(ValueError):
                classify(dict(record, oracle_text=d['oracle_text'] + '\nDraw a card.'))

    def test_paid_self_own_and_enemy_entries_are_distinct(self):
        for owner in (0, 1):
            for scenario, life in (('self_entry', 19), ('own_enchantment', 18), ('own_enchantment_creature', 18),
                                   ('opponent_enchantment', 19), ('opponent_enchantment_creature', 19),
                                   ('own_noncreature', 19), ('own_creature', 19), ('alternating_turns', 18),
                                   ('countered', 19), ('source_removed', 19), ('bounce_recast', 18)):
                case = self.case('Grim Guardian', scenario, owner)
                self.assertEqual(self.values(case, 'assert_life', player=1 - owner)[-1], life)
                self.assertTrue(self.actions(case, 'cast', ref='source', value=1))
                before_cast = case['steps'][:7]
                self.assertEqual(before_cast[1:7], [dict(op='set_mana', player=1 - owner, value=0, color=i)
                                                    for i in range(6)])
                if scenario.startswith('opponent'):
                    self.assertTrue(self.actions(case, 'advance_turn'))

    def test_multiple_mandatory_watchers_and_actual_noncaster_entry(self):
        for owner in (0, 1):
            case = self.case('Grim Guardian', 'double_watchers', owner)
            self.assertEqual(self.values(case, 'assert_life', player=1 - owner)[-1], 15)
            self.assertEqual(len(self.actions(case, 'choose_trigger_source', ref='source')), 2 if owner == 0 else 0)
            piper = self.case('Grim Guardian', 'piper_entry', owner)
            self.assertEqual(self.values(piper, 'activate', ref='piper'), [1])
            self.assertFalse(self.actions(piper, 'cast', ref='first'))
            self.assertTrue(self.actions(piper, 'choose_zone_card', value=self.registry[ENCHANTMENT_CREATURE]['csvid']))
            self.assertEqual(self.values(piper, 'assert_life', player=1 - owner)[-1], 18)

    def test_shrink_scope_and_real_cleanup_or_later_flash(self):
        for owner in (0, 1):
            shrink = self.case('Doomwake Giant', 'shrink_lethal', owner)
            self.assertEqual(self.final_card(shrink, 'enemy', 'power'), 2)
            self.assertEqual(self.final_card(shrink, 'own', 'power'), 2)
            self.assertTrue(self.actions(shrink, 'assert_zone_multiplicity', player=1 - owner,
                                         zone='graveyard', index=self.registry["Mons's Goblin Raiders"]['csvid'], value=1))
            same = self.case('Doomwake Giant', 'new_opponent_creature', owner)
            self.assertEqual(self.final_card(same, 'enemy', 'power'), 2)
            self.assertEqual(self.final_card(same, 'opposing', 'power'), 2)
            self.assertTrue(self.actions(same, 'cast_response', ref='opposing'))
            expired = self.case('Doomwake Giant', 'shrink_expires', owner)
            self.assertEqual(len(self.actions(expired, 'advance_turn')), 2)
            self.assertEqual(self.final_card(expired, 'enemy', 'power'), 3)

    def test_damage_recipients_and_damage_vs_life_loss(self):
        for owner in (0, 1):
            own = self.case('Forgeborn Oreads', 'damage_own_player', owner)
            self.assertEqual(self.values(own, 'assert_life', player=owner)[-1], 19)
            self.assertTrue(self.actions(own, 'choose_player', player=owner, value=owner))
            creature = self.case('Forgeborn Oreads', 'damage_creature', owner)
            self.assertEqual(self.final_card(creature, 'enemy', 'damage'), 1)
            walker = self.case('Forgeborn Oreads', 'damage_walker', owner)
            self.assertEqual(self.values(walker, 'assert_counter', ref='walker', counter_kind='loyalty')[-1], 2)
            for card, life in [('Forgeborn Oreads', 20), ('Grim Guardian', 19)]:
                shield = self.case(card, 'safe_passage', owner)
                self.assertTrue(self.actions(shield, 'cast', ref='shield', value=1))
                self.assertEqual(self.values(shield, 'assert_life', player=1 - owner)[-1], life)

    def test_pump_stacks_targets_and_expires(self):
        for owner in (0, 1):
            for mode, ref, power in (('pump_stacks', 'own', 5), ('pump_opponent', 'enemy', 4),
                                     ('pump_expires', 'own', 2)):
                case = self.case('Oakheart Dryads', mode, owner)
                self.assertTrue(self.actions(case, 'choose_target', ref=ref, player=owner))
                self.assertEqual(self.final_card(case, ref, 'power'), power)

    def test_exact_mill_order_and_empty_short_self(self):
        for owner in (0, 1):
            for mode, target, expected in (('self_entry', 1 - owner, 'Mountain'), ('mill_self', owner, 'Mountain'),
                                           ('short_library', 1 - owner, None), ('empty_library', 1 - owner, None),
                                           ('repeat_mill', 1 - owner, 'Hill Giant')):
                case = self.case("Thassa's Devourer", mode, owner)
                self.assertTrue(self.actions(case, 'choose_player', value=target, player=owner))
                values = self.values(case, 'assert_zone_card', player=target, zone='library', index=0)
                if expected:
                    self.assertEqual(values[-1], self.registry[expected]['csvid'])
                else:
                    self.assertEqual(values, [])
                    self.assertEqual(self.values(case, 'assert_zone', player=target, zone='library')[-1], 0)

    def test_coinsmith_exact_activation_cost_and_payout_timing(self):
        self.assertTrue(self.oracle['Underworld Coinsmith']['oracle_text'].endswith(COINSMITH))
        for owner in (0, 1):
            for scenario, allowed in (('activate_paid', True), ('activate_repeat', True),
                                      ('activate_wrong_black', False), ('activate_wrong_white', False),
                                      ('activate_short', False)):
                case = self.case('Underworld Coinsmith', scenario, owner)
                count = 2 if scenario == 'activate_repeat' else 1
                self.assertEqual(self.values(case, 'activate', ref='source'), [int(allowed)] * count)
                self.assertEqual(self.values(case, 'assert_life', player=owner)[-1], 21 - count * allowed)
                self.assertEqual(self.values(case, 'assert_life', player=1 - owner)[-1], 20 - count * allowed)
                if allowed:
                    a = case['steps'].index(self.actions(case, 'activate', ref='source')[0])
                    nxt = next(i for i, s in enumerate(case['steps'][a + 1:], a + 1) if s['op'] == 'resolve')
                    pending = {'steps': case['steps'][a:nxt]}
                    self.assertEqual(self.values(pending, 'assert_life', player=owner), [20])
                    self.assertEqual(self.values(pending, 'assert_life', player=1 - owner), [20])
                    for color in (COLORS['B'], COLORS['W']):
                        self.assertEqual(self.values(pending, 'assert_mana', player=owner, color=color), [0])

    def test_naiads_real_block_legality_and_mandatory_trigger_replay(self):
        for owner in (0, 1):
            for mode, legal, life in (('unblockable_combat', 0, 18), ('unblockable_expires', 1, 20)):
                case = self.case('Whitewater Naiads', mode, owner)
                self.assertEqual(self.values(case, 'block', ref='enemy', other='own'), [legal])
                self.assertEqual(self.values(case, 'assert_life', player=1 - owner)[-1], life)
            replay = self.case('Whitewater Naiads', 'pending_replay', owner)
            self.assertEqual(len(self.actions(replay, 'choose_target', ref='own')), 2)
            self.assertEqual(len(self.actions(replay, 'assert_recorded_state')), 1)

    def test_constellation_intimidate_actual_three_blocker_types_and_expiry(self):
        name = 'Dreadbringer Lampads'
        for owner in (0, 1):
            for scenario, legal in (('intimidate_wrong_color', 0), ('intimidate_matching_color', 1),
                                    ('intimidate_artifact', 1), ('intimidate_expires', 1)):
                case = self.case(name, scenario, owner)
                self.assertEqual(self.values(case, 'block', ref='enemy', other='own'), [legal])
                self.assertEqual(self.values(case, 'assert_life', player=1 - owner)[-1], 18 if not legal else 20)
                self.assertTrue(self.actions(case, 'choose_target', ref='own', player=owner))

    def test_whole_turn_damage_prevention_only_for_selected_creature(self):
        name = 'Harvestguard Alseids'
        for owner in (0, 1):
            for scenario, protected, victim, dead in (
                    ('protect_damage', 'own', 'own', False), ('protect_other', 'own', 'enemy', True),
                    ('protect_enemy', 'enemy', 'enemy', False), ('protect_expires', 'own', 'own', True)):
                case = self.case(name, scenario, owner)
                self.assertTrue(self.actions(case, 'choose_target', ref=protected, player=owner))
                self.assertTrue(self.actions(case, 'cast', ref='bolt', value=1))
                cid = self.registry['Grizzly Bears' if victim == 'own' else 'Hill Giant']['csvid']
                player = owner if victim == 'own' else 1 - owner
                self.assertEqual(self.values(case, 'assert_zone_multiplicity', player=player, zone='graveyard',
                                             index=cid)[-1], int(dead))
            combat = self.case(name, 'protect_combat', owner)
            self.assertEqual(self.final_card(combat, 'own', 'damage'), 0)
            self.assertEqual(self.final_card(combat, 'enemy', 'damage'), 2)

    def test_team_trample_passes_excess_for_each_legal_assignment(self):
        for owner in (0, 1):
            for scenario, life in (('trample_team', 17), ('trample_expires', 20)):
                case = self.case('Humbler of Mortals', scenario, owner)
                self.assertEqual(self.values(case, 'block', ref='enemy', other='own'), [1])
                self.assertEqual(self.values(case, 'block', ref='enemy_second', other='own_second'), [1])
                self.assertEqual(self.values(case, 'assert_life', player=1 - owner)[-1], life)

    def test_grave_pump_recounts_actual_owned_creatures_for_each_trigger(self):
        name = 'Strength from the Fallen'
        for owner in (0, 1):
            for scenario, stats in (('zero_creatures', 2), ('one_creature', 3), ('opponent_graveyard', 2),
                                    ('self_entry', 4), ('grave_pump_expires', 2), ('graveyard_increases', 7)):
                case = self.case(name, scenario, owner)
                self.assertEqual(self.final_card(case, 'own', 'power'), stats)
                self.assertTrue(self.actions(case, 'choose_target', ref='own', player=owner))
                if scenario == 'graveyard_increases':
                    self.assertTrue(self.actions(case, 'cast', ref='grave_murder', value=1))

    def test_discard_consumes_one_per_trigger_until_empty(self):
        for owner in (0, 1):
            for scenario, grave in (('empty_hand', 0), ('partial_hand', 1), ('discard_sequence', 3),
                                    ('double_watchers', 5), ('opponent_enchantment', 1)):
                case = self.case('Thoughtrender Lamia', scenario, owner)
                self.assertEqual(self.values(case, 'assert_zone', player=1 - owner, zone='graveyard')[-1], grave)
                if scenario == 'opponent_enchantment':
                    self.assertTrue(self.actions(case, 'bind_hand_copy', ref='opposing', player=1 - owner))
