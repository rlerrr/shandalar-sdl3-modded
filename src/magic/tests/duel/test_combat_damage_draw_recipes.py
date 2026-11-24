import json
import unittest
from pathlib import Path

from combat_damage_draw_recipes import FACE_DOWN, FAMILY, classify, expand, profiles, spec, validate_witnesses, variants
from generate_cases import registry
from recipes import COLORS


class CombatDamageDrawRecipesTest(unittest.TestCase):
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
        declaration = self.declaration(name, variant)
        case, = expand(name, self.registry[name]['csvid'], declaration, spec(declaration, self.oracle[name]),
                       [variant], self.registry, self.oracle)
        return case

    def actions(self, case, op, **expected):
        return [s for s in case['steps'] if s['op'] == op and all(s.get(k) == v for k, v in expected.items())]

    def values(self, case, op, **expected):
        return [s['value'] for s in self.actions(case, op, **expected)]

    def cards_drawn(self, case, owner):
        counts = self.values(case, 'assert_zone', player=owner, zone='library')
        return counts[0] - counts[-1]

    def test_pinned_complete_oracle_and_eight_exact_sources(self):
        validate_witnesses(self.oracle, self.registry)
        sources = profiles()['sources']
        pilot = {13389, 995, 1005, 11754, 6662, 11819, 6180, 11618}
        self.assertEqual({p['registry']['csvid'] for p in sources.values()}, pilot | {1309})
        self.assertEqual(sum(len(variants(classify(self.oracle[name]))) for name, pin in sources.items()
                             if pin['registry']['csvid'] in pilot), 201)
        self.assertEqual(classify(self.oracle['Keen Sense'])['mode'], 'aura')
        for name in sources:
            record = self.oracle[name]
            d = self.declaration(name, 'hit_0')
            with self.assertRaises(ValueError):
                spec(dict(d, version=2.0), record)
            with self.assertRaises(ValueError):
                spec(dict(d, params=dict(d['params'], mana=[float(n) for n in d['params']['mana']])), record)
            with self.assertRaises(ValueError):
                classify(dict(record, oracle_text=record['oracle_text'] + '\nDraw a card.'))

    def test_paid_attack_requirement_uses_both_opposing_attackers(self):
        for owner in (0, 1):
            case = self.case('Bident of Thassa', 'force_attack', owner)
            self.assertEqual(self.values(case, 'activate', ref='source'), [1])
            self.assertEqual(self.values(case, 'can_finish_attackers'), [0, 0, 1])
            self.assertEqual([action['ref'] for action in self.actions(case, 'attack')], ['opposing', 'second'])
            self.assertEqual(self.values(case, 'assert_life', player=owner)[-1], 14)
            self.assertEqual(self.values(case, 'assert_mana', color=COLORS['U'], player=owner)[-1], 0)
            for mode in ('force_only_ineligible', 'force_expires', 'force_short', 'force_owner_turn'):
                control = self.case('Bident of Thassa', mode, owner)
                self.assertEqual(self.values(control, 'can_finish_attackers'), [1])
                self.assertEqual(self.values(control, 'activate', ref='source'), [int(mode != 'force_short')])
                if mode == 'force_only_ineligible':
                    self.assertTrue(self.actions(control, 'cast', ref='opposing', value=1))

    def test_team_draw_requires_real_positive_distinct_packets(self):
        for name in ('Bident of Thassa', 'Coastal Piracy', 'Edric, Spymaster of Trest'):
            for owner in (0, 1):
                for mode, draws, damage in (('hit', 1, 2), ('two_packets', 2, 5),
                                            ('two_steps', 2, 2), ('prevented', 0, 0), ('blocked', 0, 0),
                                            ('noncombat', 0, 1)):
                    case = self.case(name, mode, owner)
                    self.assertEqual(self.cards_drawn(case, owner), draws)
                    self.assertEqual(self.values(case, 'assert_life', player=1 - owner)[-1], 20 - damage)
                self.assertEqual(self.cards_drawn(self.case(name, 'other_controller', owner), owner), 0)
                self.assertEqual(self.cards_drawn(self.case(name, 'source_removed', owner), owner), 0)
            self.assertEqual(self.cards_drawn(self.case(name, 'partial_packets', 0), 0), 1)
            self.assertEqual(self.cards_drawn(self.case(name, 'decline', 0), 0), 0)

    def test_curiosity_binds_actual_host_and_noncombat_damage(self):
        for owner in (0, 1):
            for scenario, target, draws in (('hit', 'host', 1), ('noncombat', 'host', 1),
                                             ('wrong_attacker', 'host', 0), ('other_controller', 'opposing', 0),
                                             ('detach', 'host', 0), ('two_steps', 'host', 2)):
                case = self.case('Curiosity', scenario, owner)
                self.assertTrue(self.actions(case, 'choose_target', ref=target, player=owner))
                self.assertEqual(self.cards_drawn(case, owner), draws)
                if scenario == 'noncombat':
                    self.assertTrue(self.actions(case, 'activate', ref='grant', value=1))
                    self.assertFalse(self.actions(case, 'combat_damage'))

    def test_mask_requires_untapped_and_received_opponent_damage(self):
        for owner in (0, 1):
            for scenario, draws, lives in (('noncombat', 1, 18), ('self_damage', 0, 18), ('two_packets', 2, 15),
                                           ('mask_tapped', 0, 18), ('mask_untapped', 1, 18), ('prevented', 0, 20)):
                case = self.case('Farsight Mask', scenario, owner)
                self.assertEqual(self.cards_drawn(case, owner), draws)
                self.assertEqual(self.values(case, 'assert_life', player=owner)[-1], lives)
                if scenario in ('mask_tapped', 'mask_untapped'):
                    self.assertTrue(self.actions(case, 'cast', ref='disable', value=1))
                    self.assertEqual(self.values(case, 'assert_card', ref='source', property='tapped')[-1],
                                     int(scenario == 'mask_tapped'))

    def test_insight_scales_with_damage_and_uses_actual_walker_declaration(self):
        for owner in (0, 1):
            for scenario, draws in (('hit', 2), ('planeswalker', 2), ('two_steps', 2), ('wrong_attacker', 0),
                                    ('expires', 0), ('noncombat', 0), ('prevented', 0)):
                case = self.case("Hunter's Insight", scenario, owner)
                self.assertEqual(self.cards_drawn(case, owner), draws)
                self.assertFalse(self.actions(case, 'choose_trigger_source'))
                if scenario == 'planeswalker':
                    self.assertTrue(self.actions(case, 'attack_with_tax', ref='host', value=1))
                    self.assertEqual(self.values(case, 'assert_counter', ref='walker', counter_kind='loyalty')[-1], 1)
                    self.assertEqual(self.values(case, 'assert_life', player=1 - owner)[-1], 20)
            denied = self.case("Hunter's Insight", 'wrong_target', owner)
            self.assertEqual(self.values(denied, 'cast', ref='source'), [0])
            self.assertFalse(self.actions(denied, 'choose_target'))

    def test_hidden_morph_legality_damage_and_optional_draw(self):
        for owner in (0, 1):
            for scenario, draws, identity in (('hidden_attack', 0, FACE_DOWN), ('morph_short', 0, FACE_DOWN),
                                               ('morph_reveal', 1, 6180), ('morph_offturn', 1, 6180),
                                               ('trample_hit', 1, 6180)):
                case = self.case('Hystrodon', scenario, owner)
                self.assertEqual(self.cards_drawn(case, owner), draws)
                self.assertEqual(self.values(case, 'assert_card', ref='source', property='csvid')[-1], identity)
                self.assertEqual(self.values(case, 'assert_life', player=1 - owner)[-1], 18)
                if scenario.startswith('morph_'):
                    self.assertEqual(self.values(case, 'activate', ref='source'), [int(identity == 6180)])

    def test_shrike_real_flying_and_sacrifice_for_exact_three(self):
        for owner in (0, 1):
            case = self.case('Impaler Shrike', 'flying_evasion', owner)
            self.assertEqual(self.values(case, 'block', ref='blocker', other='source'), [0])
            self.assertEqual(self.values(case, 'assert_life', player=1 - owner)[-1], 17)
            self.assertEqual(self.cards_drawn(case, owner), 3)
            self.assertEqual(self.values(case, 'assert_zone_multiplicity', player=owner,
                                         zone='graveyard', index=11618)[-1], 1)
        declined = self.case('Impaler Shrike', 'decline', 0)
        self.assertEqual(self.cards_drawn(declined, 0), 0)
        self.assertEqual(self.values(declined, 'assert_zone_multiplicity', player=0, zone='battlefield', index=11618)[-1], 1)

    def test_damage_replay_uses_actual_second_combat_pass(self):
        for owner in (0, 1):
            case = self.case('Coastal Piracy', 'replay', owner)
            self.assertEqual(len(self.actions(case, 'combat_damage')), 2)
            self.assertEqual(len(self.actions(case, 'assert_recorded_state')), 1)

    def test_coastal_handler_groups_same_recipient_but_counts_all_packets(self):
        for name in ('Coastal Piracy', 'Edric, Spymaster of Trest'):
            for actor in (0, 1):
                two = self.case(name, 'two_packets', actor)
                self.assertEqual(self.values(two, 'choose_dialog'), [0])
                self.assertEqual(len(self.actions(two, 'choose_trigger_source', ref='source')), int(actor == 0))
                self.assertEqual(self.cards_drawn(two, actor), 2)
                sequential = self.case(name, 'two_steps', actor)
                self.assertEqual(self.values(sequential, 'choose_dialog'), [0, 0])
                self.assertEqual(self.cards_drawn(sequential, actor), 2)
            partial = self.case(name, 'partial_packets', 0)
            self.assertEqual(self.values(partial, 'choose_dialog'), [1])
            self.assertEqual(self.values(partial, 'choose_number'), [1])
            self.assertEqual(self.cards_drawn(partial, 0), 1)
            declined = self.case(name, 'decline', 0)
            self.assertEqual(self.values(declined, 'choose_dialog'), [1])
            self.assertEqual(self.cards_drawn(declined, 0), 0)

    def test_paid_morph_changes_identity_before_draining_bookkeeping(self):
        for owner in (0, 1):
            for mode in ('morph_reveal', 'morph_offturn'):
                case = self.case('Hystrodon', mode, owner)
                activate = next(i for i, s in enumerate(case['steps']) if s['op'] == 'activate' and s['ref'] == 'source')
                drain = next(i for i, s in enumerate(case['steps'][activate + 1:], activate + 1) if s['op'] == 'resolve')
                early = {'steps': case['steps'][activate + 1:drain]}
                self.assertEqual(self.values(early, 'assert_card', ref='source', property='csvid'), [6180])
                self.assertEqual(self.values(early, 'assert_card', ref='source', property='power'), [3])
                self.assertEqual(self.values(early, 'assert_card', ref='source', property='toughness'), [4])
                self.assertEqual(self.values(early, 'assert_mana', player=owner), [0] * 6)
                self.assertFalse(self.actions(early, 'assert_stack'))

    def test_keen_sense_reuses_combat_and_noncombat_with_its_own_green_cost(self):
        for owner in (0, 1):
            for mode, draw in (('hit', 1), ('noncombat', 1), ('other_controller', 0)):
                case = self.case('Keen Sense', mode, owner)
                cast = next(i for i, step in enumerate(case['steps']) if step['op'] == 'cast')
                pre = {'steps': case['steps'][:cast]}
                self.assertEqual(self.values(pre, 'set_mana', player=owner, color=COLORS['G']), [1])
                self.assertEqual(self.values(pre, 'set_mana', player=owner, color=COLORS['U']), [0])
                self.assertEqual(self.cards_drawn(case, owner), draw)
                if mode == 'noncombat':
                    self.assertTrue(self.actions(case, 'activate', ref='grant', value=1))
                    self.assertFalse(self.actions(case, 'combat_damage'))
