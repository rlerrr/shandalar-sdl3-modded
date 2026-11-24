import json
import unittest
from pathlib import Path

from generate_cases import registry
from recipes import COLORS
from same_name_sweep_recipes import FAMILY, classify, expand, profiles, spec, validate_witnesses, variants


class SameNameSweepRecipesTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.oracle = json.loads((Path(__file__).parent / 'data/oracle.json').read_bytes())['cards']
        cls.registry = {row['name']: row for row in registry()}

    def case(self, card, scenario, owner):
        record = self.oracle[card]
        variant = f'{scenario}_{owner}'
        declaration = dict(family=FAMILY, version=2, oracle_text=record['oracle_text'],
                           params=classify(record), variants=[variant])
        case, = expand(card, self.registry[card]['csvid'], declaration, spec(declaration, record), [variant],
                       self.registry, self.oracle)
        return case

    def actions(self, case, op, **filters):
        return [action for action in case['steps'] if action['op'] == op
                and all(action.get(field) == value for field, value in filters.items())]

    def values(self, case, op, **filters):
        return [action['value'] for action in self.actions(case, op, **filters)]

    def last(self, case, op, **filters):
        return self.values(case, op, **filters)[-1]

    def copies(self, case, name, player, zone):
        return self.last(case, 'assert_zone_multiplicity', player=player, zone=zone, index=self.registry[name]['csvid'])

    def test_whole_grammar_strict_typed_and_exact_pins(self):
        validate_witnesses(self.oracle, self.registry)
        names = profiles()['sources']
        self.assertEqual({row['registry']['csvid'] for row in names.values()}, {13671, 6885, 10309, 4207, 15783, 6884})
        self.assertEqual(sum(len(variants(classify(self.oracle[name]))) for name in names), 168)
        self.assertEqual(classify(self.oracle['Echoing Courage'])['power'], 2)
        self.assertIn('Echoing Courage', names)
        for name in names:
            record = self.oracle[name]
            with self.assertRaises(ValueError):
                classify(dict(record, oracle_text=record['oracle_text'] + '\nDraw a card.'))
            params = classify(record)
            with self.assertRaises(ValueError):
                spec(dict(family=FAMILY, version=2, oracle_text=record['oracle_text'],
                          params=dict(params, power=float(params['power'])), variants=['effect_0']), record)

    def test_lethal_group_reaches_both_seats_but_keeps_same_stats_other_name(self):
        for owner in (0, 1):
            for name in ('Bile Blight', 'Echoing Decay', 'Maelstrom Pulse'):
                for scenario, anchor in (('effect', 1-owner), ('own_target', owner)):
                    case = self.case(name, scenario, owner)
                    self.assertEqual(self.copies(case, 'Grizzly Bears', anchor, 'graveyard'), 2)
                    self.assertEqual(self.copies(case, 'Grizzly Bears', 1-anchor, 'graveyard'), 1)
                    self.assertEqual(self.copies(case, 'Runeclaw Bear', anchor, 'battlefield'), 1)
                    self.assertTrue(self.actions(case, 'choose_target', ref='target', player=owner))

    def test_nonlethal_stats_actual_expiry_combat_and_protected_peer(self):
        for owner in (0, 1):
            for name, delta in (('Bile Blight', 3), ('Echoing Decay', 2)):
                case = self.case(name, 'expiry', owner)
                self.assertEqual(self.values(case, 'assert_card', ref='target', property='power')[-2:], [5-delta, 5])
                self.assertEqual([s['player'] for s in self.actions(case, 'untap_step')], [1-owner])
                defended = self.case(name, 'protected_peer', owner)
                self.assertEqual(self.values(defended, 'cast', ref='guard'), [1])
                self.assertTrue(self.actions(defended, 'choose_target', ref='peer', player=1-owner))
                self.assertTrue(self.actions(defended, 'choose_target', ref='target', player=owner))
                self.assertEqual(self.last(defended, 'assert_card', ref='peer', property='power'), 6-delta)
                self.assertEqual(self.last(defended, 'assert_card', ref='unrelated', property='power'), 1)
                combat = self.case(name, 'combat', owner)
                self.assertEqual(self.values(combat, 'attack', ref='other'), [1])
                self.assertEqual(self.last(combat, 'assert_life', player=1-owner), 15+delta)

    def test_spell_type_scopes_and_indestructibility(self):
        for owner in (0, 1):
            for source, variant, subject, decoy in (
                ('Maelstrom Pulse', 'artifact', 'Fountain of Youth', 'Sol Ring'),
                ('Maelstrom Pulse', 'enchantment', 'Waste Not', 'Tree of Tales'),
                ('Wake of Destruction', 'effect', 'Forest', 'Island'),
                ('Wake of Destruction', 'artifact_land', 'Tree of Tales', 'Sol Ring'),
            ):
                case = self.case(source, variant, owner)
                self.assertEqual(self.copies(case, subject, owner, 'graveyard'), 1)
                self.assertEqual(self.copies(case, subject, 1-owner, 'graveyard'), 2)
                self.assertEqual(self.copies(case, decoy, 1-owner, 'battlefield'), 1)
            for name, subject in (('Maelstrom Pulse', 'Darksteel Relic'), ('Wake of Destruction', 'Darksteel Citadel')):
                case = self.case(name, 'indestructible', owner)
                self.assertEqual(self.copies(case, subject, 1-owner, 'battlefield'), 2)
                self.assertEqual(self.copies(case, subject, owner, 'battlefield'), 1)

    def test_target_loss_and_counter_leave_every_other_copy(self):
        for owner in (0, 1):
            for name in profiles()['sources']:
                subject = 'Forest' if name == 'Wake of Destruction' else 'Grizzly Bears'
                for scenario in ('target_lost', 'countered'):
                    lost = self.case(name, scenario, owner)
                    self.assertTrue(self.actions(lost, 'announce_spell', ref='source'))
                    self.assertEqual(self.values(lost, 'cast_response', ref='reply'), [1])
                    self.assertTrue(self.actions(lost, 'resolve_response'))
                    self.assertFalse(self.actions(lost, 'choose_target', ref='source'))
                    self.assertEqual(self.copies(lost, subject, owner, 'battlefield'), 1)
                    self.assertEqual(self.copies(lost, subject, 1-owner, 'battlefield'), 1 if scenario == 'target_lost' else 2)
                    self.assertEqual(self.copies(lost, subject, 1-owner, 'hand'), int(scenario == 'target_lost'))
                    if name == 'Declaration in Stone':
                        self.assertEqual(self.copies(lost, 'Clue', 1-owner, 'battlefield'), 0)

    def test_real_token_only_and_mixed_declaration_cohorts_reward_nontokens(self):
        for owner in (0, 1):
            case = self.case('Declaration in Stone', 'token_population', owner)
            self.assertTrue(all(card['name'] not in ('Soldier', 'Clue') for card in case['setup']))
            self.assertEqual(self.values(case, 'cast', ref='spawn_target'), [1])
            self.assertEqual(self.values(case, 'cast', ref='spawn_other'), [1])
            self.assertEqual(self.copies(case, 'Soldier', 1-owner, 'battlefield'), 0)
            self.assertEqual(self.copies(case, 'Soldier', owner, 'battlefield'), 2)
            self.assertEqual(self.copies(case, 'Soldier', 1-owner, 'exile'), 0)
            self.assertEqual(self.copies(case, 'Clue', 1-owner, 'battlefield'), 0)
            for scenario, anchor in (('mixed_nontoken_anchor', 'target'), ('mixed_token_anchor', 'token_copy')):
                case = self.case('Declaration in Stone', scenario, owner)
                self.assertEqual(self.values(case, 'cast', ref='copy_spell'), [1])
                self.assertTrue(self.actions(case, 'bind_permanent_copy', ref='token_copy', player=1-owner, index=1))
                self.assertTrue(self.actions(case, 'choose_target', ref=anchor, player=owner))
                self.assertEqual(self.copies(case, 'Grizzly Bears', 1-owner, 'exile'), 1)
                self.assertEqual(self.copies(case, 'Clue', 1-owner, 'battlefield'), 1)

    def test_borrowed_controller_gets_clue_and_real_clue_ability_draws(self):
        for owner in (0, 1):
            case = self.case('Declaration in Stone', 'borrowed_target', owner)
            self.assertEqual(self.values(case, 'cast', ref='borrow'), [1])
            self.assertTrue(self.actions(case, 'bind_permanent_copy', ref='target', player=owner))
            self.assertEqual(self.copies(case, 'Grizzly Bears', 1-owner, 'battlefield'), 1)
            self.assertEqual(self.copies(case, 'Grizzly Bears', 1-owner, 'exile'), 1)
            self.assertEqual(self.copies(case, 'Grizzly Bears', owner, 'exile'), 0)
            self.assertEqual(self.copies(case, 'Clue', owner, 'battlefield'), 1)
            clue = self.case('Declaration in Stone', 'clue_use', owner)
            self.assertEqual(self.values(clue, 'activate', ref='clue'), [1])
            self.assertEqual(self.copies(clue, 'Clue', 1-owner, 'battlefield'), 1)
            self.assertEqual(self.copies(clue, 'Mountain', 1-owner, 'hand'), 1)
            self.assertEqual(self.last(clue, 'assert_zone_card', zone='library', index=0, player=1-owner), self.registry['Island']['csvid'])

    def test_payment_boundaries_and_closed_snapshot_replay(self):
        for owner in (0, 1):
            for name in profiles()['sources']:
                for variant in ('short', 'wrong_color', 'no_target'):
                    case = self.case(name, variant, owner)
                    self.assertEqual(self.values(case, 'cast', ref='source'), [0])
                    self.assertFalse(self.actions(case, 'choose_target'))
                    opening = case['steps'][1:13]
                    self.assertEqual({(s['player'], s['color']) for s in opening}, {(p, c) for p in (0, 1) for c in range(6)})
                    self.assertTrue(all(s['op'] == 'set_mana' and s['value'] == 0 for s in opening))
            case = self.case('Bile Blight', 'replay', owner)
            self.assertEqual(self.values(case, 'cast', ref='source'), [1, 1])
            self.assertTrue(self.actions(case, 'assert_recorded_state'))
            offturn = self.case('Bile Blight', 'offturn', owner)
            self.assertEqual(offturn['steps'][0]['player'], 1-owner)
            self.assertEqual(self.last(offturn, 'assert_mana', player=owner, color=COLORS['B']), 0)

    def test_positive_same_name_stat_change_and_exact_expiry(self):
        for owner in (0, 1):
            effect = self.case('Echoing Courage', 'effect', owner)
            for ref in ('target', 'peer', 'other'):
                self.assertEqual(self.last(effect, 'assert_card', ref=ref, property='power'), 4)
                self.assertEqual(self.last(effect, 'assert_card', ref=ref, property='toughness'), 4)
            self.assertEqual(self.last(effect, 'assert_card', ref='unrelated', property='power'), 2)
            expiry = self.case('Echoing Courage', 'expiry', owner)
            self.assertEqual(self.values(expiry, 'assert_card', ref='other', property='power')[-2:], [7, 5])
            self.assertTrue(self.actions(expiry, 'untap_step', player=1-owner))
            lost = self.case('Echoing Courage', 'target_lost', owner)
            self.assertEqual(self.last(lost, 'assert_card', ref='peer', property='power'), 2)
            self.assertTrue(self.actions(lost, 'cast_response', ref='reply', value=1))
            copied = self.case('Echoing Courage', 'token_population', owner)
            for ref in ('target', 'peer', 'other', 'other_peer'):
                self.assertEqual(self.last(copied, 'assert_card', ref=ref, property='power'), 3)
                self.assertEqual(self.last(copied, 'assert_card', ref=ref, property='toughness'), 3)

    def test_insufficient_wake_has_one_tapped_caster_land_and_five_mana(self):
        for owner in (0, 1):
            case = self.case('Wake of Destruction', 'short', owner)
            forests = [card for card in case['setup'] if card['player'] == owner and card['zone'] == 'battlefield']
            self.assertEqual(forests, [dict(name='Forest', ref='other', player=owner, zone='battlefield', state=['tapped'])])
            self.assertEqual(self.last(case, 'set_mana', player=owner, color=0), 2)
            self.assertEqual(self.last(case, 'set_mana', player=owner, color=COLORS['R']), 3)
            self.assertEqual(self.values(case, 'cast', ref='source'), [0])
            self.assertFalse(self.actions(case, 'choose_target'))
            self.assertEqual(self.last(case, 'assert_card', ref='other', property='tapped'), 1)
