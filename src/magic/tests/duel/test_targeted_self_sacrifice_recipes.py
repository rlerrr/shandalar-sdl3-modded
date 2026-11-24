"""Focused guardrails for the real targeting boundaries and whole clauses."""

import json
import unittest

from generate_cases import registry
import targeted_self_sacrifice_recipes as model


class TargetedSelfSacrificeTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.oracle = json.loads((model.PROFILE_PATH.parent / 'oracle.json').read_text(encoding='utf8'))['cards']
        cls.rows = {row['name']: row for row in registry()}
        cls.sources = model.profiles()['sources']

    def steps(self, name, variant):
        source = self.sources[name]
        record = self.oracle[name]
        declaration = dict(family=model.FAMILY, version=2, params=source['params'], oracle_text=record['oracle_text'], variants=[variant])
        parameters = model.spec(declaration, record)
        return model.expand(name, source['registry']['csvid'], declaration, parameters, [variant], self.rows, self.oracle)[0]['steps']

    def test_full_oracle_and_implemented_shared_callback(self):
        model.validate_witnesses(self.oracle, self.rows)
        self.assertTrue({11834, 2612, 11835, 11019, 14556, 4606} <= {pin['registry']['csvid'] for pin in self.sources.values()})
        for name in ('Labyrinth Guardian', 'Phantasmal Image', 'Crystalline Nautilus'):
            with self.subTest(name=name), self.assertRaises(ValueError):
                model.parameters(self.oracle[name])
        for name, keyword in [('Phantasmal Bear', []), ('Skulking Ghost', ['Flying']), ('Phantasmal Abomination', ['Defender'])]:
            self.assertEqual(model.parameters(self.oracle[name])['keywords'], keyword)

    def test_cast_target_sacrifice_precedes_targeting_spell_resolution(self):
        steps = self.steps('Phantasmal Bear', 'enemy_bounce_0')
        target = steps.index(dict(op='choose_target', player=1, ref='source'))
        announce = steps.index(dict(op='announce_spell', ref='spell'))
        resolve = steps.index(dict(op='resolve'))
        self.assertEqual(steps[target + 1], dict(op='cast', ref='spell', value=1))
        self.assertIn(dict(op='assert_card', ref='source', property='in_play', value=1), steps[target:announce])
        self.assertIn(dict(op='assert_stack', value=1), steps[announce:resolve])
        self.assertIn(dict(op='assert_zone_multiplicity', player=0, zone='graveyard', index=11834, value=1), steps[announce:resolve])
        self.assertIn(dict(op='assert_zone_multiplicity', player=0, zone='hand', index=11834, value=0), steps[resolve:])

    def test_counterspell_is_cast_after_sacrifice_and_snapshot_has_single_cast_payment(self):
        steps = self.steps('Skulking Ghost', 'countered_targeting_spell_1')
        sacrifice = steps.index(dict(op='assert_zone_multiplicity', player=1, zone='graveyard', index=2612, value=1))
        response = steps.index(dict(op='cast_response', ref='counter', value=1))
        self.assertLess(sacrifice, response)
        self.assertIn(dict(op='assert_stack', value=2), steps[response:])
        self.assertIn(dict(op='resolve_response'), steps[response:])
        for mode, announces in [('spell_before_announcement_snapshot', 2), ('spell_after_announcement_snapshot', 1)]:
            steps = self.steps('Skulking Ghost', mode + '_0')
            self.assertEqual(steps.count(dict(op='cast', ref='spell', value=1)), 1)
            self.assertEqual(steps.count(dict(op='announce_spell', ref='spell')), announces)
            self.assertIn(dict(op='assert_recorded_state'), steps)

    def test_targeted_ability_and_other_target_have_distinct_pending_and_damage_receipts(self):
        target_steps = self.steps('Phantasmal Bear', 'own_ability_0')
        active = target_steps.index(dict(op='activate', ref='provider', value=1))
        resolve = target_steps.index(dict(op='resolve'))
        self.assertIn(dict(op='assert_mana_total', player=0, value=57), target_steps[active:resolve])
        self.assertIn(dict(op='assert_zone_multiplicity', player=0, zone='graveyard', index=11834, value=1), target_steps[active:resolve])
        other = self.steps('Phantasmal Bear', 'other_ability_target_0')
        self.assertIn(dict(op='assert_card', ref='source', property='in_play', value=1), other[other.index(dict(op='resolve')):])
        self.assertIn(dict(op='assert_card', ref='decoy', property='damage', value=1), other)

    def test_real_untargeted_grant_cancel_and_indestructible_sacrifice(self):
        anthem = self.steps('Phantasmal Bear', 'untargeted_anthem_0')
        self.assertFalse(any(step['op'] == 'choose_target' for step in anthem))
        self.assertIn(dict(op='assert_card', ref='source', property='power', value=3), anthem)
        cancel = self.steps('Phantasmal Bear', 'cancel_target_0')
        self.assertIn(dict(op='choose_cancel_target', player=0), cancel)
        self.assertIn(dict(op='cast', ref='spell', value=0), cancel)
        self.assertIn(dict(op='assert_mana_total', player=0, value=60), cancel)
        indestructible = self.steps('Phantasmal Bear', 'indestructible_sacrifice_0')
        destruction = indestructible.index(dict(op='cast', ref='destruction', value=1))
        targeted = indestructible.index(dict(op='choose_target', player=0, ref='source'))
        self.assertIn(dict(op='assert_card', ref='source', property='in_play', value=1), indestructible[destruction:targeted])
        self.assertIn(dict(op='assert_card', ref='decoy', property='in_play', value=0), indestructible[destruction:targeted])
        self.assertIn(dict(op='assert_card', ref='source', property='in_play', value=0), indestructible[targeted:])

    def test_flying_and_defender_use_real_combat(self):
        for mode, result in [('ground_combat', 0), ('flying_blocker', 1), ('reach_blocker', 1)]:
            steps = self.steps('Phantasmal Dragon', mode + '_1')
            self.assertIn(dict(op='block', ref='opponent', other='source', value=result), steps)
        cannot_attack = self.steps('Phantasmal Abomination', 'ground_combat_0')
        self.assertIn(dict(op='attack', ref='source', value=0), cannot_attack)
        can_defend = self.steps('Phantasmal Abomination', 'defend_ground_1')
        self.assertIn(dict(op='block', ref='source', other='opponent', value=1), can_defend)


if __name__ == '__main__':
    unittest.main()
