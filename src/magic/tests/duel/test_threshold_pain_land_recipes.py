import copy
import json
import unittest
from pathlib import Path

from generate_cases import registry
import threshold_pain_land_recipes as model


class ThresholdPainLandTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.oracle = json.loads((Path(__file__).parent / 'data/oracle.json').read_text(encoding='utf8'))['cards']
        cls.rows = {row['name']: row for row in registry()}

    def case(self, name, variant):
        record = self.oracle[name]
        declaration = dict(family=model.FAMILY, version=2, params=model.classify(record),
                           oracle_text=record['oracle_text'], variants=[variant])
        return model.expand(name, self.rows[name]['csvid'], declaration, model.spec(declaration, record),
                            [variant], self.rows, self.oracle)[0]

    @staticmethod
    def value(steps, op, **fields):
        return next(step['value'] for step in reversed(steps) if step['op'] == op
                    and all(step.get(key) == value for key, value in fields.items()))

    def count(self, steps, who, zone, name):
        return self.value(steps, 'assert_zone_multiplicity', player=who, zone=zone, index=self.rows[name]['csvid'])

    def test_live_normal_sources_and_fixed_witnesses(self):
        model.validate_witnesses(self.oracle, self.rows)
        self.assertEqual({self.rows[name]['csvid'] for name in model.pins()['sources']}, {5483, 5499, 5506, 5508, 5639})
        rows = copy.deepcopy(self.rows)
        rows['Cabal Pit']['handler'] = 'card_dummy'
        with self.assertRaises(ValueError):
            model.validate_witnesses(self.oracle, rows)

    def test_whole_profile_rejects_different_cost_threshold_pain_or_extra_clauses(self):
        record = self.oracle['Barbarian Ring']
        text = record['oracle_text']
        for changed in (text.replace('seven', 'six'), text.replace('your graveyard', 'any graveyard'),
                        text.replace('damage to you', 'life loss'), text.replace('{R}, {T}', '{U}, {T}'),
                        text + '\nDraw a card.'):
            with self.assertRaises(ValueError):
                model.classify(dict(record, oracle_text=changed))

    def test_only_own_threshold_counts_and_mana_still_pains(self):
        for name in model.pins()['sources']:
            color = model.classify(self.oracle[name])['color']
            for who in (0, 1):
                case = self.case(name, f'own_six_opposing_eight_{who}')
                self.assertEqual([sum(c['zone'] == 'graveyard' and c['player'] == p for c in case['setup'])
                                  for p in (who, 1 - who)], [6, 8])
                self.assertEqual(self.value(case['steps'], 'require_activation_mode'), 0)
                self.assertEqual(self.value(case['steps'], 'assert_life', player=who), 19)
                self.assertEqual(self.value(case['steps'], 'assert_mana', player=who, color=color), 3)
                self.assertEqual(self.count(case['steps'], who, 'battlefield', name), 1)

    def test_one_color_cost_sacrifice_and_effect_wait_for_resolution(self):
        for name in model.pins()['sources']:
            color = model.classify(self.oracle[name])['color']
            for who in (0, 1):
                steps = self.case(name, f'payment_{who}')['steps']
                start = next(i for i, s in enumerate(steps) if s['op'] == 'activate')
                finish = next(i for i, s in enumerate(steps) if s['op'] == 'resolve')
                pending = steps[start:finish]
                self.assertEqual(self.value(steps[:start], 'set_mana', player=who, color=color), 1)
                self.assertEqual(self.value(pending, 'assert_mana', player=who, color=color), 0)
                self.assertEqual(self.count(pending, who, 'graveyard', name), 1)
                self.assertEqual(self.count(pending, who, 'battlefield', name), 0)
                self.assertEqual(self.value(pending, 'assert_stack'), 1)
                self.assertEqual(self.value(pending, 'assert_life', player=who), 20)
                self.assertEqual(self.value(steps, 'assert_stack'), 0)

    def test_live_grave_changes_use_actual_death_and_crypt(self):
        rise = self.case('Nomad Stadium', 'seventh_card_dies_0')['steps']
        death = rise.index(dict(op='destroy', ref='seventh'))
        pay = rise.index(dict(op='activate', ref='source', value=1))
        self.assertEqual(self.value(rise[:death], 'assert_zone', player=0, zone='graveyard'), 6)
        self.assertEqual(self.value(rise[death:pay], 'assert_zone', player=0, zone='graveyard'), 7)
        self.assertEqual(self.value(rise, 'assert_life', player=0), 24)
        fall = self.case('Nomad Stadium', 'grave_exiled_1')['steps']
        crypt = fall.index(dict(op='activate', ref='crypt', value=1))
        self.assertEqual(self.value(fall[:crypt], 'assert_zone', player=1, zone='graveyard'), 7)
        self.assertEqual(self.value(fall, 'assert_zone', player=1, zone='graveyard'), 0)
        self.assertEqual(self.value(fall, 'assert_life', player=1), 19)

    def test_no_mana_same_tap_and_wrong_color_keep_source(self):
        for name in model.pins()['sources']:
            for mode in ('tapped', 'wrong_color', 'unfunded', 'mana_repeat_denied'):
                steps = self.case(name, f'{mode}_0')['steps']
                self.assertEqual(self.count(steps, 0, 'battlefield', name), 1)
                self.assertEqual(self.count(steps, 0, 'graveyard', name), 0)
                self.assertEqual(self.value(steps, 'activate', ref='source'), int(mode in ('wrong_color', 'unfunded')))

    def test_ring_any_target_seats_and_planeswalker_loyalty(self):
        for who in (0, 1):
            for mode, recipient in [('at_seven', 1 - who), ('own_player', who)]:
                steps = self.case('Barbarian Ring', f'{mode}_{who}')['steps']
                self.assertIn(dict(op='choose_player', player=who, value=recipient), steps)
                self.assertEqual(self.value(steps, 'assert_life', player=recipient), 18)
            walker = self.case('Barbarian Ring', f'planeswalker_{who}')['steps']
            self.assertIn(dict(op='choose_target', player=who, ref='target'), walker)
            self.assertEqual(self.value(walker, 'assert_counter', ref='target', counter_kind='loyalty'), 1)
            lethal = self.case('Barbarian Ring', f'lethal_creature_{who}')['steps']
            self.assertEqual(self.value(lethal, 'assert_card', ref='target', property='in_play'), 0)

    def test_stats_reduce_or_raise_independently_and_expire(self):
        for name, delta, seat in [('Cabal Pit', -2, 0), ('Centaur Garden', 3, 1)]:
            steps = self.case(name, f'cleanup_{seat}')['steps']
            cleanup = next(i for i, s in enumerate(steps) if s['op'] == 'cleanup')
            for prop in ('power', 'toughness'):
                self.assertEqual(self.value(steps[:cleanup], 'assert_card', ref='target', property=prop), 3 + delta)
                self.assertEqual(self.value(steps, 'assert_card', ref='target', property=prop), 3)
        lethal = self.case('Cabal Pit', 'lethal_creature_1')['steps']
        self.assertEqual(self.value(lethal, 'assert_card', ref='target', property='in_play'), 0)

    def test_coliseum_draws_before_discard_with_controller_or_opponent(self):
        for mode, who, recipient in [('existing_hand', 1, 0), ('own_existing_hand', 1, 1), ('own_player', 0, 0)]:
            steps = self.case('Cephalid Coliseum', f'{mode}_{who}')['steps']
            end = next(i for i, s in enumerate(steps) if s['op'] == 'resolve')
            self.assertIn(dict(op='choose_player', player=who, value=recipient), steps)
            self.assertEqual(self.value(steps[:end], 'assert_zone', player=recipient, zone='library'), len(model.LIBRARY))
            self.assertEqual(self.value(steps, 'assert_zone', player=recipient, zone='library'), len(model.LIBRARY)-3)
            self.assertEqual(self.value(steps, 'assert_discard_choices'), 3 if recipient == 0 else 0)
            self.assertEqual(self.value(steps, 'assert_zone', player=recipient, zone='hand'), 0 if mode == 'own_player' else 2)
            if recipient == 1:
                self.assertEqual(self.count(steps, recipient, 'hand', 'Forest'), 2)

    def test_nomad_gain_applies_to_source_controller_only(self):
        for who in (0, 1):
            steps = self.case('Nomad Stadium', f'recover_from_one_{who}')['steps']
            self.assertEqual(self.value(steps, 'assert_life', player=who), 5)
            self.assertEqual(self.value(steps, 'assert_life', player=1-who), 20)
            self.assertFalse(any(s['op'] in ('choose_target', 'choose_player') for s in steps))

    def test_pending_replay_uses_paid_snapshot_once_per_playout(self):
        steps = self.case('Centaur Garden', 'pending_replay_1')['steps']
        capture, record, restore = [next(i for i, s in enumerate(steps) if s['op'] == op)
                                    for op in ('capture', 'record_state', 'restore')]
        self.assertEqual(self.value(steps[:capture], 'assert_stack'), 1)
        self.assertEqual(sum(s['op'] == 'activate' for s in steps), 1)
        self.assertEqual(steps[capture+1:record], steps[restore+1:-1])


if __name__ == '__main__':
    unittest.main()
