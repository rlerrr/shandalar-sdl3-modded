import copy
import json
import unittest

from import_duel_oracle import registry
import targeted_rebound_recipes as model


class TargetedReboundTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        data = model.PROFILE.parent
        cls.oracle = json.loads((data / 'oracle.json').read_text(encoding='utf8'))['cards']
        cls.rows = {row['name']: row for row in registry() if row['kind'] != 'internal'}
        cls.sources = model.profiles()['sources']
        cls.homes = {}
        for path in (data / 'cases/cards').glob('*/*.json'):
            home = json.loads(path.read_text(encoding='utf8'))
            if home['card'] in cls.sources:
                cls.homes[home['card']] = home

    def projected(self, name, variant):
        home = self.homes[name]
        d = home['recipes'][0]
        p = model.spec(d, self.oracle[name])
        return model.expand(name, home['csvid'], d, p, [variant], self.rows, self.oracle)[0]

    def test_exact_oracle_profiles_and_two_seats(self):
        self.assertEqual(set(self.homes), set(self.sources))
        model.validate_witnesses(self.oracle, self.rows)
        self.assertEqual({d['registry']['csvid'] for d in self.sources.values()}, {10906, 15048, 14846, 11020})
        for name, home in self.homes.items():
            spec = model.spec(home['recipes'][0], self.oracle[name])
            self.assertEqual(len(model.variants(spec)), 19)
            self.assertEqual(('offturn_1' in model.variants(spec)), self.oracle[name]['type_line'] == 'Instant')
            self.assertNotIn('decline_1', model.variants(spec))
        name = 'Artful Maneuver'
        recipe = copy.deepcopy(self.homes[name]['recipes'][0])
        recipe['params']['toughness'] = True
        with self.assertRaises(ValueError):
            model.spec(recipe, self.oracle[name])
        bad = copy.deepcopy(self.oracle[name])
        bad['oracle_text'] += '\nDraw a card.'
        with self.assertRaises(ValueError):
            model.classify(bad)

    def test_exact_real_upkeep_recast_and_next_target(self):
        c = self.projected('Artful Maneuver', 'retarget_enemy_0')
        self.assertIn(dict(name=model.SQUIRE, ref='other', player=1, zone='battlefield'), c['setup'])
        steps = c['steps']
        hand_cast = steps.index(dict(op='cast', ref='source', value=1))
        deferred = steps.index(dict(op='choose_trigger', ref='source', player=0))
        hand_resolve = steps.index(dict(op='resolve'))
        later = steps.index(dict(op='choose_target', ref='other', player=0))
        self.assertLess(hand_cast, deferred)
        self.assertLess(deferred, hand_resolve)
        self.assertEqual([s['player'] for s in steps[hand_resolve:later] if s['op']=='upkeep_step'], [1])
        self.assertEqual(steps[later + 1], dict(op='upkeep_step', player=0))
        self.assertEqual(steps[later + 2], dict(op='assert_unused_choices', value=0))
        expected = [dict(op='assert_card', ref='other', property='power', value=3),
                    dict(op='assert_card', ref='other', property='toughness', value=4),
                    dict(op='assert_zone_multiplicity', player=0, zone='graveyard', index=14846, value=1)]
        for assertion in expected:
            self.assertIn(assertion, steps[later+1:])
        self.assertEqual(sum(s['op']=='cast' for s in steps), 1)
        self.assertNotIn('play_exiled', [s['op'] for s in steps])
        self.assertFalse(any(s['op']=='set_mana' for s in steps[hand_cast+1:]))

    def test_optional_decline_and_targetless_do_not_consume_exile(self):
        for variant in ('decline_0', 'no_target_0'):
            steps = self.projected('Distortion Strike', variant)['steps']
            if variant == 'no_target_0':
                self.assertIn(dict(op='cast', ref='wrath', value=1), steps)
                self.assertIn(dict(op='cast', ref='recovery', value=1), steps)
                self.assertIn(dict(op='choose_trigger', ref='source', player=0), steps)
            else:
                self.assertNotIn('choose_trigger', [s['op'] for s in steps])
            final = max(i for i, s in enumerate(steps) if s['op']=='upkeep_step')
            self.assertIn(dict(op='assert_zone_multiplicity', player=0, zone='exile', index=10906, value=1), steps[final:])
            self.assertIn(dict(op='assert_zone_multiplicity', player=0, zone='graveyard', index=10906, value=0), steps[final:])
            self.assertEqual(sum(s['op']=='choose_target' for s in steps), 1)

    def test_counter_and_fizzle_have_real_response_and_no_delayed_choice(self):
        for mode in ('countered_1', 'fizzle_1'):
            steps = self.projected("Prey's Vengeance", mode)['steps']
            response = steps.index(dict(op='cast_response', ref='response', value=1))
            self.assertIn(dict(op='announce_spell', ref='source'), steps[:response])
            self.assertEqual(steps[response + 1], dict(op='assert_stack', value=2))
            self.assertEqual(steps[response + 2], dict(op='resolve_response'))
            self.assertNotIn('choose_trigger', [s['op'] for s in steps])
            self.assertIn(dict(op='assert_zone_multiplicity', zone='exile', player=1, index=11020, value=0),steps[response:])
            self.assertIn(dict(op='assert_zone_multiplicity', zone='graveyard', player=1, index=11020, value=1),steps[response:])

    def test_actual_printed_combat_modifiers_and_expiry(self):
        for name, mode, blocked in (('Distortion Strike', 'combat_0', 0),
                                    ("Taigam's Strike", 'expired_block_1', 1),
                                    ('Artful Maneuver', 'combat_0', 1)):
            steps = self.projected(name, mode)['steps']
            self.assertIn(dict(op='block', ref='blocker', other='first', value=blocked), steps)
            self.assertIn(dict(op='combat_damage'), steps)
        strike = self.projected('Distortion Strike', 'combat_0')['steps']
        self.assertIn(dict(op='assert_life', player=1, value=17), strike)
        maneuver = self.projected('Artful Maneuver', 'combat_0')['steps']
        self.assertIn(dict(op='assert_card', ref='first', property='damage', value=3), maneuver)
        self.assertIn(dict(op='assert_zone_multiplicity', zone='graveyard', player=1,
                           index=self.rows[model.GIANT]['csvid'], value=1), maneuver)

    def test_snapshot_replays_real_due_upkeep_with_deferred_human_choice(self):
        steps = self.projected("Prey's Vengeance", 'snapshot_0')['steps']
        captured, recorded, restored = [steps.index(dict(op=s)) for s in ('capture', 'record_state', 'restore')]
        self.assertIn(dict(op='choose_trigger', player=0, ref='source'), steps[:captured])
        self.assertEqual(steps[captured+1:recorded], steps[restored+1:-1])
        self.assertEqual(steps[captured-1]['op'], 'assert_card')
        self.assertEqual(steps[captured+1], dict(op='choose_target', player=0, ref='other'))
        self.assertEqual(steps[captured+2], dict(op='upkeep_step', player=0))
        self.assertEqual(steps[-1], dict(op='assert_recorded_state'))

    def test_offturn_hand_cast_rebounds_on_immediately_following_upkeep(self):
        steps = self.projected("Prey's Vengeance", 'offturn_1')['steps']
        self.assertEqual(steps[0],dict(op='turn', player=0, value=0))
        first_resolve = steps.index(dict(op='resolve'))
        later = steps.index(dict(op='upkeep_step', player=1))
        self.assertEqual(sum(s['op']=='advance_turn' for s in steps[first_resolve:later]),1)
        self.assertEqual(steps[later-1],dict(op='choose_target',player=1,ref='first'))


if __name__ == '__main__':
    unittest.main()

