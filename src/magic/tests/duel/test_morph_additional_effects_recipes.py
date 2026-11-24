import json
import unittest
from pathlib import Path

from generate_cases import registry
from morph_additional_effects_recipes import FACE_DOWN, FAMILY, classify, expand, profiles, spec, validate_witnesses, variants


class MorphAdditionalEffectsTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.oracle = json.loads((Path(__file__).parent / 'data/oracle.json').read_bytes())['cards']
        cls.registry = {row['name']: row for row in registry()}

    def case(self, card, scenario, owner):
        record = self.oracle[card]
        variant = f'{scenario}_{owner}'
        proposal = dict(family=FAMILY, version=2, oracle_text=record['oracle_text'], params=classify(record),
                        variants=[variant])
        case, = expand(card, self.registry[card]['csvid'], proposal, spec(proposal, record), [variant],
                       self.registry, self.oracle)
        return case

    def actions(self, case, op, **filters):
        return [action for action in case['steps'] if action['op'] == op
                and all(action.get(key) == value for key, value in filters.items())]

    def values(self, case, op, **filters):
        return [action['value'] for action in self.actions(case, op, **filters)]

    def last(self, case, op, **filters):
        return self.values(case, op, **filters)[-1]

    def after(self, case, op, **filters):
        action = self.actions(case, op, **filters)[0]
        return {'steps': case['steps'][case['steps'].index(action) + 1:]}

    def test_pinned_whole_oracle_and_strict_parameters(self):
        validate_witnesses(self.oracle, self.registry)
        sources = profiles()['sources']
        self.assertEqual({pin['registry']['csvid'] for pin in sources.values()}, {6074, 8847, 14294, 14291})
        self.assertEqual(sum(len(variants(classify(self.oracle[name]))) for name in sources), 109)
        for name in sources:
            record = self.oracle[name]
            params = classify(record)
            declaration = dict(family=FAMILY, version=2.0, params=params,
                               oracle_text=record['oracle_text'], variants=['effect_0'])
            with self.assertRaises(ValueError):
                spec(declaration, record)
            with self.assertRaises(ValueError):
                classify(dict(record, oracle_text=record['oracle_text'] + '\nDraw a card.'))

    def test_cast_and_reveal_are_real_and_color_correct(self):
        for name in profiles()['sources']:
            for owner in (0, 1):
                choice = self.case(name, 'face_down_choice', owner)
                self.assertEqual(self.values(choice, 'choose_dialog', player=owner), [0])
                self.assertEqual(self.last(choice, 'assert_card', ref='source', property='csvid'), FACE_DOWN)
                opening = choice['steps'][1:13]
                self.assertEqual({(x['player'], x['color']) for x in opening}, {(p, c) for p in (0, 1) for c in range(6)})
                self.assertTrue(all(x['op'] == 'set_mana' and x['value'] == 0 for x in opening))
                self.assertEqual(self.values(self.case(name, 'face_down_short', owner), 'cast', ref='source'), [0])
                for scenario in ('reveal_wrong_color', 'reveal_short'):
                    failed = self.case(name, scenario, owner)
                    self.assertEqual(self.values(failed, 'activate', ref='source'), [0])
                    self.assertEqual(self.last(failed, 'assert_card', ref='source', property='csvid'), FACE_DOWN)

    def test_damage_amount_controls_real_unseeded_insect_count(self):
        token_id = profiles()['insect_engine_identity']['csvid']
        for owner in (0, 1):
            for scenario, total in (('effect', 1), ('damage_two', 2), ('damage_lethal', 2), ('combat_damage', 2),
                                    ('two_packets', 3), ('damage_prevented', 0), ('face_down_damage', 0), ('damage_other', 0)):
                case = self.case('Broodhatch Nantuko', scenario, owner)
                self.assertFalse(any(card['name'] == 'Insect' for card in case['setup']))
                self.assertEqual(self.last(case, 'assert_zone_multiplicity', player=owner, zone='battlefield', index=token_id), total)
                self.assertEqual(self.last(case, 'assert_zone_multiplicity', player=1-owner, zone='battlefield', index=token_id), 0)
                if total:
                    for field, value in (('power', 1), ('toughness', 1), ('token', 1), ('color', 8), ('subtype_Insect', 1)):
                        self.assertEqual(self.last(case, 'assert_matching_permanents', player=owner, index=token_id,
                                                   property=field), value)
            combat = self.case('Broodhatch Nantuko', 'combat_damage', owner)
            self.assertTrue(self.actions(combat, 'combat_damage'))
            self.assertEqual(self.values(combat, 'block', ref='source', other='victim'), [1])

    def test_optional_nantuko_decline_keeps_the_damage_and_omits_the_acceptance(self):
        allowed = self.case('Broodhatch Nantuko', 'effect', 0)
        self.assertTrue(self.actions(allowed, 'choose_trigger_source', ref='source', player=0))
        declined = self.case('Broodhatch Nantuko', 'damage_decline', 0)
        self.assertFalse(self.actions(declined, 'choose_trigger_source'))
        self.assertEqual(self.last(declined, 'assert_card', ref='source', property='damage'), 1)
        prevented = self.case('Broodhatch Nantuko', 'damage_prevented', 1)
        self.assertEqual(self.values(prevented, 'cast', ref='shield'), [1])
        self.assertEqual(self.last(prevented, 'assert_card', ref='source', property='damage'), 0)

    def test_shaper_both_modes_lethal_toughness_and_actual_expiry(self):
        for owner in (0, 1):
            for scenario, mode, stats in (('effect', 0, (4, 2)), ('negative_power', 1, (0, 6))):
                case = self.case('Shaper Parasite', scenario, owner)
                self.assertEqual(self.values(case, 'choose_dialog', player=owner), [mode])
                tp, = self.actions(case, 'choose_target', ref='victim')
                menu, = self.actions(case, 'choose_dialog', player=owner)
                self.assertLess(case['steps'].index(tp), case['steps'].index(menu))
                self.assertEqual(tuple(self.last(case, 'assert_card', ref='victim', property=f)
                                       for f in ('power', 'toughness')), stats)
            lethal = self.case('Shaper Parasite', 'lethal_toughness', owner)
            self.assertEqual(self.last(lethal, 'assert_zone_multiplicity', player=1-owner, zone='graveyard', index=106), 1)
            expiry = self.case('Shaper Parasite', 'expiry', owner)
            self.assertTrue(self.actions(expiry, 'advance_turn'))
            self.assertEqual(self.values(expiry, 'assert_card', ref='victim', property='power')[-2:], [4, 2])

    def test_jeering_changes_controller_untaps_and_gives_real_attack_then_returns(self):
        for owner in (0, 1):
            stolen = self.case('Jeering Instigator', 'return_after_attack', owner)
            self.assertEqual(self.values(stolen, 'cast', ref='twiddle'), [1])
            self.assertEqual(self.values(stolen, 'assert_card', ref='victim', property='tapped')[-4:], [1, 0, 1, 0])
            self.assertEqual([s['player'] for s in self.actions(stolen, 'bind_permanent_copy', ref='victim')], [owner, 1-owner])
            self.assertEqual(self.values(stolen, 'attack', ref='victim'), [1])
            offturn = self.after(self.case('Jeering Instigator', 'offturn', owner), 'can_activate', ref='source')
            self.assertFalse(self.actions(offturn, 'choose_target'))
            self.assertEqual(self.last(offturn, 'assert_card', ref='victim', property='tapped'), 1)
            self_ban = self.case('Jeering Instigator', 'effect', owner)
            self.assertEqual(self.values(self_ban, 'probe_activation_target', other='source'), [0])

    def test_aven_optional_choice_distinguishes_no_other_and_decline(self):
        for owner in (0, 1):
            declined = self.case('Icefeather Aven', 'decline', owner)
            self.assertTrue(self.actions(declined, 'choose_optional_target', ref='victim', player=owner))
            self.assertEqual(self.values(declined, 'assert_forced_target_choices'), [0])
            self.assertEqual(self.last(declined, 'assert_zone_multiplicity', player=1-owner, zone='battlefield', index=106), 1)
            absent = self.case('Icefeather Aven', 'no_other', owner)
            for ref in ('source', 'protected'):
                self.assertEqual(self.values(absent, 'probe_activation_target', other=ref), [0])
            self.assertEqual(self.last(absent, 'assert_unused_choices'), 1)
            self.assertEqual(self.last(absent, 'assert_card', ref='source', property='csvid'), 14291)

    def test_aven_returns_to_owner_and_flying_changes_actual_blocks(self):
        for owner in (0, 1):
            taken = self.case('Icefeather Aven', 'to_owner', owner)
            self.assertEqual(self.values(taken, 'cast', ref='treason'), [1])
            self.assertTrue(self.actions(taken, 'bind_permanent_copy', ref='victim', player=owner))
            self.assertEqual(self.last(taken, 'assert_zone_multiplicity', player=1-owner, zone='hand', index=106), 1)
            self.assertEqual(self.last(taken, 'assert_zone_multiplicity', player=owner, zone='hand', index=106), 0)
            for scenario, accepted in (('flying_ground', 0), ('flying_reach', 1), ('hidden_ground', 1)):
                case = self.case('Icefeather Aven', scenario, owner)
                self.assertEqual(self.values(case, 'block', ref='blocker', other='source'), [accepted])
