import json
import unittest

from import_duel_oracle import registry
import spore_upkeep_riders_recipes as model


class SporeUpkeepRiderTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        data = model.PROFILE.parent
        cls.oracle = json.loads((data / 'oracle.json').read_text(encoding='utf8'))['cards']
        cls.rows = {row['name']: row for row in registry() if row['kind'] != 'internal'}
        cls.homes = {}
        for name, pin in model.profiles()['sources'].items():
            i = pin['registry']['csvid']
            home, = (data / f'cases/cards/{i // 100:03}').glob(f'{i:05}-*.json')
            cls.homes[name] = json.loads(home.read_text(encoding='utf8'))

    def case(self, name, variant):
        home = self.homes[name]
        declaration = home['recipes'][0]
        params = model.spec(declaration, self.oracle[name])
        return model.expand(name, home['csvid'], declaration, params, [variant], self.rows, self.oracle)[0]

    def test_full_oracle_gates(self):
        model.validate_witnesses(self.oracle, self.rows)
        self.assertEqual({h['csvid'] for h in self.homes.values()}, {1827, 8698, 8516, 8697, 8875, 1824})
        for name, home in self.homes.items():
            record = self.oracle[name]
            declaration = home['recipes'][0]
            self.assertEqual(declaration['variants'], model.variants(model.classify(record)))
            self.assertIsNotNone(model.spec(declaration, record))
            with self.assertRaises(ValueError):
                model.classify(dict(record, oracle_text=record['oracle_text'] + '\nDraw a card.'))

    def test_first_three_own_upkeeps_and_two_earlier_denials(self):
        for p in (0, 1):
            case = self.case('Thallid', f'threshold_{p}')
            steps = case['steps']
            self.assertFalse(any(card['name'] == 'Saproling' for card in case['setup']))
            self.assertEqual([row['player'] for row in steps if row['op'] == 'upkeep_step'], [1 - p, p] * 3)
            self.assertEqual([row['value'] for row in steps if row['op'] == 'can_activate'], [0] * 12 + [1, 0, 0])
            self.assertEqual([row['value'] for row in steps if row['op'] == 'activate'], [0, 1])
            self.assertNotIn('assert_counter', {row['op'] for row in steps})

    def test_bank_four_requires_two_more_own_upkeeps(self):
        for name in self.homes:
            steps = self.case(name, 'bank_four_0')['steps']
            self.assertEqual([r['player'] for r in steps if r['op'] == 'upkeep_step'], [1, 0] * 6)
            paid = [i for i, row in enumerate(steps) if row['op'] == 'activate' and row['value'] == 1]
            self.assertEqual(len(paid), 3 if model.classify(self.oracle[name])['shape'] in model.RIDERS else 2)
            self.assertEqual([r['value'] for r in steps[paid[-2]:paid[-1]] if r['op'] == 'can_activate'][-4:], [0, 0, 0, 1])

    def test_token_identity_and_replay(self):
        steps = self.case('Thallid', 'snapshot_1')['steps']
        self.assertEqual(steps[-1], dict(op='assert_recorded_state'))
        begin = steps.index(dict(op='capture'))
        end = steps.index(dict(op='record_state'))
        self.assertEqual(steps[begin + 1:end], steps[end + 3:-1])
        matches = {'token': 1, 'artifact': 0, 'power': 1, 'toughness': 1, 'color': 8, 'subtype_Saproling': 1}
        self.assertEqual({r['property']: r['value'] for r in steps if r['op'] == 'assert_matching_permanents'
                          and r['index'] == 18813 and r['player'] == 1}, matches)

    def test_actual_sacrifice_before_target_effect_and_no_token_card_in_grave(self):
        steps = self.case('Deathspore Thallid', 'lethal_0')['steps']
        self.assertEqual([r['mode_label'] for r in steps if r['op'] == 'choose_activation_mode'],
                         ['Generate a Saproling', 'Sac & give -1/-1'])
        target = steps.index(dict(op='choose_target', player=0, ref='enemy'))
        token = steps.index(dict(op='choose_target', player=0, ref='saproling_0'))
        cast = steps.index(dict(op='activate', ref='source', value=1), target)
        resolve = steps.index(dict(op='resolve'), cast)
        self.assertLess(target, token)
        self.assertLess(token, cast)
        self.assertEqual(steps[cast + 1], dict(op='require_activation_mode', value=1))
        for zone in ('battlefield', 'graveyard'):
            self.assertIn(dict(op='assert_zone_multiplicity', player=0, zone=zone, index=18813, value=0), steps[cast:resolve])
        self.assertIn(dict(op='assert_zone_multiplicity', player=1, zone='graveyard', index=self.rows[model.GOBLIN]['csvid'], value=1),
                      steps[resolve:])

    def test_germinator_can_pump_the_enemy_and_real_turn_expires_it(self):
        steps = self.case('Thallid Germinator', 'enemy_expiry_1')['steps']
        target = steps.index(dict(op='choose_target', player=1, ref='enemy'))
        expiry = steps.index(dict(op='advance_turn'), target)
        for prop in ('power', 'toughness'):
            self.assertIn(dict(op='assert_card', ref='enemy', property=prop, value=4), steps[target:expiry])
            self.assertIn(dict(op='assert_card', ref='enemy', property=prop, value=3), steps[expiry:])

    def test_defender_and_haste_are_real_attack_observations(self):
        defender = self.case('Thallid Shell-Dweller', 'defender_0')['steps']
        self.assertIn(dict(op='attack', ref='source', value=0), defender)
        self.assertIn(dict(op='attack', ref='ally', value=1), defender)
        for mode, allowed in (('fresh_haste', 1), ('haste_expiry', 0)):
            steps = self.case('Vitaspore Thallid', mode + '_1')['steps']
            fresh = steps.index(dict(op='cast', ref='fresh', value=1))
            attack = steps.index(dict(op='attack', ref='fresh', value=allowed))
            self.assertNotIn(dict(op='advance_turn'), steps[fresh:attack])
            self.assertEqual(dict(op='cleanup') in steps[fresh:attack], not allowed)
            self.assertIn(dict(op='attack', ref='fresh_control', value=0), steps[fresh:])

    def test_fog_covers_both_combat_recipients_but_not_bolt_or_later_turns(self):
        for mode, life, damage in (('fog_combat', 17, 0), ('fog_expired', 18, 3), ('fog_enemy_turn', 20, 0)):
            steps = self.case('Spore Flower', mode + '_0')['steps']
            combat = steps.index(dict(op='combat_damage'))
            self.assertIn(dict(op='block', ref='blocker', other='attacker', value=1), steps[:combat])
            self.assertIn(dict(op='assert_life', player=int(mode != 'fog_enemy_turn'), value=life), steps[combat:])
            self.assertIn(dict(op='assert_card', ref='blocker', property='damage', value=damage), steps[combat:])
            if mode == 'fog_combat':
                self.assertIn(dict(op='assert_life', player=1, value=17), steps[:combat])
