import copy
import json
import unittest
from pathlib import Path

from generate_cases import registry
import gempalm_tribal_cycling_recipes as recipe


class GempalmTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.oracle = json.loads((Path(__file__).parent / 'data/oracle.json').read_text(encoding='utf8'))['cards']
        cls.rows = {row['name']: row for row in registry()}

    def case(self, name='Gempalm Strider', variant='effect_0'):
        record = self.oracle[name]
        declaration = dict(family=recipe.FAMILY, version=2, oracle_text=record['oracle_text'],
                           params=recipe.classify(record), variants=[variant])
        return recipe.expand(name, self.rows[name]['csvid'], declaration, recipe.spec(declaration, record),
                             [variant], self.rows, self.oracle)[0]

    def value(self, case, operation, **fields):
        return next(step['value'] for step in reversed(case['steps']) if step['op'] == operation
                    and all(step.get(key) == value for key, value in fields.items()))

    def count(self, case, name, who, zone):
        return self.value(case, 'assert_zone_multiplicity', player=who, zone=zone, index=self.rows[name]['csvid'])

    def stats(self, case, ref):
        return tuple(self.value(case, 'assert_card', ref=ref, property=field) for field in ('power', 'toughness'))

    def test_five_whole_pins_and_primary_handlers(self):
        recipe.validate_witnesses(self.oracle, self.rows)
        self.assertEqual({self.rows[name]['csvid'] for name in recipe.pins()['sources']}, set(range(6379, 6384)))
        rows = copy.deepcopy(self.rows)
        rows['Gempalm Avenger']['handler'] = 'card_dummy'
        with self.assertRaises(ValueError):
            recipe.validate_witnesses(self.oracle, rows)

    def test_scope_reminder_or_extra_clause_drift_fails_closed(self):
        record = self.oracle['Gempalm Avenger']
        text = record['oracle_text']
        for altered in (text + '\nFlying', text.replace('Soldier creatures', 'Soldier creatures you control'),
                        text.replace('Discard this card', 'Sacrifice this card'), text.replace('until end of turn', 'this combat')):
            with self.assertRaises(ValueError):
                recipe.classify(dict(record, oracle_text=altered))

    def test_exact_cycle_cost_is_distinct_from_creature_cast_cost(self):
        for name in recipe.pins()['sources']:
            p = recipe.classify(self.oracle[name])
            for mode, fee in (('effect', p['cycling_mana']), ('source_cast', p['mana'])):
                case = self.case(name, mode + '_1')
                seeded = [x['value'] for x in case['steps'][:14] if x['op'] == 'set_mana' and x['player'] == 1]
                self.assertEqual(seeded, fee)
                self.assertEqual([self.value(case, 'assert_mana', player=1, color=i) for i in range(6)], [0] * 6)
            self.assertEqual(self.count(self.case(name, 'source_cast_1'), name, 1, 'battlefield'), 1)

    def test_cycling_discards_before_resolution_and_draws_after(self):
        case = self.case(variant='effect_1')
        steps = case['steps']
        first = next(i for i, step in enumerate(steps) if step['op'] == 'activate_hand')
        end = next(i for i, step in enumerate(steps) if step['op'] == 'resolve')
        self.assertIn(dict(op='assert_zone', player=1, zone='library', value=4), steps[first:end])
        self.assertIn(dict(op='assert_zone_multiplicity', player=1, zone='graveyard', index=6383, value=1), steps[first:end])
        self.assertEqual(self.count(case, 'Grizzly Bears', 1, 'hand'), 1)

    def test_cancel_denial_and_plain_discard_have_no_reward_or_draw(self):
        for mode in ('cancel_cycle', 'insufficient_mana', 'wrong_color', 'plain_discard'):
            case = self.case(variant=mode + '_0')
            self.assertEqual(self.value(case, 'assert_zone', player=0, zone='library'), 4)
            self.assertEqual(self.stats(case, 'member'), (2, 3))
            self.assertEqual(self.count(case, 'Gempalm Strider', 0, 'graveyard'), int(mode == 'plain_discard'))
        unrelated = self.case(variant='unrelated_cycle_1')
        self.assertEqual(self.stats(unrelated, 'member'), (2, 3))
        self.assertEqual(self.count(unrelated, 'Grizzly Bears', 1, 'hand'), 1)

    def test_tribal_scope_both_controllers_and_changeling(self):
        for who in (0, 1):
            result = self.case(variant=f'changeling_{who}')
            self.assertEqual(self.stats(result, 'member'), (4, 5))
            self.assertEqual(self.stats(result, 'enemy_member'), (4, 5))
            self.assertEqual(self.stats(result, 'changeling'), (3, 3))
            self.assertEqual(self.stats(result, 'neutral'), (2, 2))
        repeated = self.case(variant='repeated_1')
        self.assertEqual(self.stats(repeated, 'enemy_member'), (6, 7))
        self.assertEqual(self.count(repeated, 'Gempalm Strider', 1, 'graveyard'), 2)
        cleanup = self.case(variant='cleanup_1')
        self.assertEqual(self.stats(cleanup, 'enemy_member'), (2, 3))

    def test_damage_counts_goblins_even_when_noncreature_and_pending_update(self):
        name = 'Gempalm Incinerator'
        regular = self.case(name, 'effect_0')
        kindred = self.case(name, 'kindred_goblin_0')
        changed = self.case(name, 'count_changes_pending_0')
        self.assertEqual(self.value(regular, 'assert_card', ref='target', property='damage'), 2)
        self.assertEqual(self.count(kindred, 'Hill Giant', 1, 'graveyard'), 1)
        self.assertEqual(self.count(kindred, 'Boggart Shenanigans', 1, 'battlefield'), 1)
        self.assertEqual(self.value(changed, 'assert_card', ref='target', property='damage'), 1)
        operations = [step['op'] for step in changed['steps']]
        self.assertLess(operations.index('activate_hand'), operations.index('destroy'))
        self.assertLess(operations.index('destroy'), operations.index('resolve'))

    def test_polluter_uses_life_loss_of_either_player_and_exact_count(self):
        name = 'Gempalm Polluter'
        for mode, who, life in (('own_target', 0, 18), ('effect', 1, 18), ('opponent_only', 1, 19),
                               ('count_changes_pending', 1, 19), ('zero_tribe', 1, 20), ('changeling', 1, 17)):
            result = self.case(name, mode + '_0')
            self.assertEqual(self.value(result, 'assert_life', player=who), life)

    def test_optional_effect_decline_does_not_cancel_cycling_draw(self):
        for name in ('Gempalm Incinerator', 'Gempalm Polluter'):
            for who in (0, 1):
                case = self.case(name, 'decline_effect_' + str(who))
                self.assertIn(dict(op='choose_cancel_target', player=who), case['steps'])
                self.assertEqual(self.count(case, name, who, 'graveyard'), 1)
                self.assertEqual(self.count(case, 'Grizzly Bears', who, 'hand'), 1)
                self.assertEqual(self.value(case, 'assert_life', player=who), 20)

    def test_effect_before_draw_is_a_distinguishable_life_receipt(self):
        name = 'Gempalm Incinerator'
        killed = self.case(name, 'effect_before_draw_0')
        survives = self.case(name, 'draw_watcher_survives_0')
        self.assertEqual(self.count(killed, 'Horizon Chimera', 0, 'graveyard'), 1)
        self.assertEqual(self.value(killed, 'assert_life', player=0), 20)
        self.assertEqual(self.count(survives, 'Horizon Chimera', 0, 'battlefield'), 1)
        self.assertEqual(self.value(survives, 'assert_life', player=0), 21)

    def test_avenger_real_first_strike_changes_combat_survival(self):
        name = 'Gempalm Avenger'
        for mode, lives in (('first_strike', 1), ('base_combat', 0), ('expired_first_strike', 0)):
            result = self.case(name, mode + '_0')
            self.assertIn(dict(op='combat_damage'), result['steps'])
            self.assertEqual(self.count(result, 'Elite Vanguard', 0, 'battlefield'), lives)
            self.assertEqual(self.count(result, 'Grizzly Bears', 1, 'graveyard'), 1)

    def test_sorcerer_flying_reach_and_expiry(self):
        name = 'Gempalm Sorcerer'
        for mode, block in (('flying', 0), ('reach_blocks', 1), ('expired_flying', 1)):
            result = self.case(name, mode + '_1')
            self.assertEqual(self.value(result, 'block', ref='blocker'), block)
            self.assertEqual(self.value(result, 'assert_life', player=0), 19 if mode == 'flying' else 20)
            self.assertEqual(self.count(result, 'Crafty Pathmage', 1, 'battlefield'), 1 - block)

    def test_pumped_opposing_tribe_receives_combat_keywords(self):
        avenger = self.case('Gempalm Avenger', 'enemy_first_strike_0')
        self.assertEqual(self.count(avenger, 'Elite Vanguard', 1, 'battlefield'), 1)
        self.assertEqual(self.count(avenger, 'Grizzly Bears', 0, 'graveyard'), 1)
        for mode, legal in (('enemy_flying_blocks', 1), ('expired_enemy_flying', 0)):
            sorcerer = self.case('Gempalm Sorcerer', mode + '_1')
            self.assertEqual(self.value(sorcerer, 'block', ref='enemy_member', other='flyer'), legal)
            self.assertEqual(self.value(sorcerer, 'assert_card', ref='flyer', property='damage'), legal)

    def test_pending_replay_preserves_actual_payment_and_exact_resolution(self):
        case = self.case('Gempalm Polluter', 'pending_replay_1')
        steps = case['steps']
        capture, record, restore = [next(i for i, step in enumerate(steps) if step['op'] == op)
                                    for op in ('capture', 'record_state', 'restore')]
        self.assertTrue(any(step['op'] == 'activate_hand' for step in steps[:capture]))
        self.assertEqual(steps[capture+1:record], steps[restore+1:-1])
        self.assertTrue(any(step['op'] == 'choose_player' for step in steps[restore+1:]))


if __name__ == '__main__':
    unittest.main()
