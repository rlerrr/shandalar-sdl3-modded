import json
import unittest
from pathlib import Path

from block_restriction_spells_recipes import FAMILY, classify, expand, profiles, spec, validate_witnesses, variants
from generate_cases import registry


class BlockRestrictionSpellsTest(unittest.TestCase):
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
                and all(action.get(key) == val for key, val in filters.items())]

    def values(self, case, op, **filters):
        return [action['value'] for action in self.actions(case, op, **filters)]

    def last(self, case, op, **filters):
        return self.values(case, op, **filters)[-1]

    def location(self, case, op, **filters):
        action, = self.actions(case, op, **filters)
        return case['steps'].index(action)

    def test_whole_grammar_and_exact_pins(self):
        validate_witnesses(self.oracle, self.registry)
        sources = profiles()['sources']
        self.assertEqual({pin['registry']['csvid'] for pin in sources.values()}, {3766, 13332, 14956, 3306, 16495, 16691})
        self.assertEqual(sum(len(variants(classify(self.oracle[name]))) for name in sources), 160)
        for name in sources:
            record = self.oracle[name]
            params = classify(record)
            for unsupported in ('\nScry 1.', '\nDestroy target land.'):
                with self.assertRaises(ValueError):
                    classify(dict(record, oracle_text=record['oracle_text'] + unsupported))
            declaration = dict(family=FAMILY, version=2.0, params=params, oracle_text=record['oracle_text'],
                               variants=['effect_0'])
            with self.assertRaises(ValueError):
                spec(declaration, record)

    def test_flying_and_reach_are_distinct_real_block_controls(self):
        for owner in (0, 1):
            for scenario, allowed in (('effect', 0), ('flying', 1), ('reach', 0)):
                case = self.case('Seismic Stomp', scenario, owner)
                self.assertEqual(self.values(case, 'block', ref='blocker', other='attacker'), [allowed])
                self.assertEqual(self.last(case, 'assert_life', player=1-owner), 20 if allowed else 18)
                self.assertEqual(self.last(case, 'assert_card', ref='attacker', property='blocked'), allowed)
            flyer = self.case('Renegade Tactics', 'flying_target', owner)
            self.assertEqual(self.values(flyer, 'block', ref='blocker'), [0])
            selective = self.case('Renegade Tactics', 'only_target', owner)
            self.assertEqual(self.values(selective, 'block'), [0, 1])

    def test_dynamic_membership_uses_paid_aura_and_late_flash_entry(self):
        for owner in (0, 1):
            for scenario, result, witness in (('gain_flying', 1, 'aura'), ('lose_flying', 0, 'sphere' if owner else 'aura'),
                                               ('late_ground', 0, 'blocker')):
                case = self.case('Magmatic Chasm', scenario, owner)
                self.assertLess(self.location(case, 'cast', ref='source'), self.location(case, 'cast', ref=witness))
                self.assertLess(self.location(case, 'cast', ref=witness), self.location(case, 'block', ref='blocker'))
                self.assertEqual(self.values(case, 'block', ref='blocker'), [result])
                if witness == 'aura':
                    self.assertTrue(self.actions(case, 'choose_target', ref='blocker', player=owner))
                elif witness == 'blocker':
                    late, = [card for card in case['setup'] if card['ref'] == 'blocker']
                    self.assertIn('Flash', self.oracle[late['name']]['keywords'])
                    self.assertEqual(late['zone'], 'hand')

    def test_instant_combat_windows_and_existing_block(self):
        for owner in (0, 1):
            for name in ('Falter', 'Stun'):
                pre = self.case(name, 'preblock', owner)
                self.assertLess(self.location(pre, 'finish_attackers'), self.location(pre, 'cast', ref='source'))
                self.assertLess(self.location(pre, 'cast', ref='source'), self.location(pre, 'begin_blockers'))
                self.assertEqual(self.values(pre, 'block'), [0])
                after = self.case(name, 'afterblock', owner)
                self.assertLess(self.location(after, 'advance_combat'), self.location(after, 'cast', ref='source'))
                self.assertLess(self.location(after, 'cast', ref='source'), self.location(after, 'combat_damage'))
                self.assertEqual(self.values(after, 'block'), [1])
                self.assertEqual(self.last(after, 'assert_life', player=1-owner), 20)
                defender = self.case(name, 'defend', owner)
                self.assertEqual(defender['steps'][0]['player'], 1-owner)
                self.assertEqual(self.last(defender, 'assert_life', player=owner), 18)

    def test_real_expiry_restores_blocking_after_two_untaps(self):
        for owner in (0, 1):
            for name in ('Falter', 'Stun', 'Destructive Tampering'):
                case = self.case(name, 'expiry', owner)
                self.assertEqual(self.values(case, 'block'), [0, 1])
                self.assertEqual([action['player'] for action in self.actions(case, 'untap_step')], [1-owner, owner])
                self.assertEqual(self.last(case, 'assert_life', player=1-owner), 18)

    def test_draw_is_exact_and_only_after_legal_resolution(self):
        top = self.registry['Mountain']['csvid']
        following = self.registry['Island']['csvid']
        for owner in (0, 1):
            for scenario in ('effect', 'no_target', 'removed_target', 'countered', 'one_card', 'own_target'):
                case = self.case('Stun', scenario, owner)
                draws = scenario in ('effect', 'one_card', 'own_target')
                self.assertEqual(self.last(case, 'assert_zone_multiplicity', player=owner, zone='hand', index=top), int(draws))
                if scenario != 'one_card':
                    self.assertEqual(self.last(case, 'assert_zone_card', player=owner, zone='library', index=0),
                                     following if draws else top)
                if scenario in ('removed_target', 'countered'):
                    self.assertTrue(self.actions(case, 'cast_response', ref='reply', value=1))
            own = self.case('Renegade Tactics', 'own_target', owner)
            self.assertTrue(self.actions(own, 'choose_target', ref='attacker', player=owner))
            self.assertEqual(self.values(own, 'attack', ref='attacker'), [1])

    def test_modal_exclusive_selection_payment_and_legal_artifact(self):
        for owner in (0, 1):
            for scenario, mode, cast, block in (('effect', 1, 1, 0), ('destroy', 0, 1, 1),
                                                 ('destroy_own', 0, 1, 1), ('indestructible', 0, 1, 1),
                                                 ('illegal_destroy', 0, 0, 1), ('cancel', 2, 0, 1)):
                case = self.case('Destructive Tampering', scenario, owner)
                self.assertEqual(self.values(case, 'choose_dialog', player=owner), [mode])
                self.assertEqual(self.values(case, 'cast', ref='source'), [cast])
                self.assertEqual(self.values(case, 'block'), [block])
                if scenario in ('destroy', 'destroy_own', 'indestructible'):
                    self.assertLess(self.location(case, 'choose_dialog'), self.location(case, 'choose_target', ref='artifact'))
                    linked, = [card for card in case['setup'] if card['ref'] == 'artifact']
                    zone = 'battlefield' if scenario == 'indestructible' else 'graveyard'
                    self.assertEqual(self.last(case, 'assert_zone_multiplicity', player=linked['player'], zone=zone,
                                               index=self.registry[linked['name']]['csvid']), 1)

    def test_no_payment_boundary_reaches_choices_and_replay_is_exact(self):
        for owner in (0, 1):
            for name in profiles()['sources']:
                for scenario in ('short', 'wrong_color'):
                    case = self.case(name, scenario, owner)
                    self.assertEqual(self.values(case, 'cast', ref='source'), [0])
                    self.assertFalse(self.actions(case, 'choose_dialog'))
                    self.assertFalse(self.actions(case, 'choose_target'))
                    initial = case['steps'][1:13]
                    self.assertTrue(all(action['op'] == 'set_mana' and action['value'] == 0 for action in initial))
            case = self.case('Stun', 'replay', owner)
            start = self.location(case, 'capture') + 1
            end = self.location(case, 'record_state')
            after = self.location(case, 'restore') + 1
            self.assertEqual(case['steps'][start:end], case['steps'][after:-1])
            self.assertEqual(case['steps'][-1]['op'], 'assert_recorded_state')

    def test_counter_and_real_wing_loss_inputs(self):
        for owner in (0, 1):
            for name in profiles()['sources']:
                case = self.case(name, 'countered', owner)
                self.assertTrue(self.actions(case, 'announce_spell', ref='source'))
                self.assertFalse(self.actions(case, 'choose_target', ref='source'))
                self.assertTrue(self.actions(case, 'cast_response', ref='reply', value=1))
            wing = self.case('Falter', 'lose_flying', owner)
            ref, name = ('sphere', 'Gravity Sphere') if owner else ('aura', 'Grounded')
            witness, = [card for card in wing['setup'] if card['ref'] == ref]
            self.assertEqual(witness['name'], name)
            self.assertEqual(witness['zone'], 'hand')
            self.assertEqual([s['player'] for s in self.actions(wing, 'choose_target', ref='blocker')], [] if owner else [0])
            self.assertEqual(self.values(wing, 'block', ref='blocker'), [0])
