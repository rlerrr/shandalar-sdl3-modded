import copy
import json
import unittest

from import_duel_oracle import registry
import untargeted_rebound_recipes as model


class UntargetedReboundTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        data = model.PROFILE.parent
        cls.oracle = json.loads((data / 'oracle.json').read_text(encoding='utf8'))['cards']
        cls.rows = {row['name']: row for row in registry() if row['kind'] != 'internal'}
        cls.homes = {}
        for name, pin in model.profiles()['sources'].items():
            i = pin['registry']['csvid']
            paths = list((data / 'cases/cards' / f'{i//100:03d}').glob(f'{i:05d}-*.json'))
            if len(paths) != 1:
                raise ValueError(f'expected exact source home for {i}')
            cls.homes[name] = json.loads(paths[0].read_text(encoding='utf8'))

    def cases(self, name, *modes):
        d = self.homes[name]['recipes'][0]
        p = model.spec(d, self.oracle[name])
        return model.expand(name, self.rows[name]['csvid'], d, p, list(modes), self.rows, self.oracle)

    @staticmethod
    def after_upkeeps(steps):
        offsets = [i for i, s in enumerate(steps) if s['op'] == 'upkeep_step']
        return [steps[i:j] for i, j in zip(offsets, offsets[1:] + [len(steps)])]

    def test_exact_oracle_and_registry_pins(self):
        model.validate_witnesses(self.oracle, self.rows)
        self.assertEqual({self.rows[name]['csvid'] for name in self.homes},
                         {11005, 14974, 11060, 14926, 11059, 15024})
        old = copy.deepcopy(self.oracle['Survival Cache'])
        old['oracle_text'] += '\nDraw a card.'
        with self.assertRaises(ValueError):
            model.classify(old)

    def test_assembly_recounts_actual_tokens_only_for_caster(self):
        name = "Nomads' Assembly"
        i = self.rows['Kor Soldier']['csvid']
        full, empty = self.cases(name, 'scheduled_1', 'enemy_only_0')
        steps = full['steps']
        first, due, *_ = self.after_upkeeps(steps)
        def count(p, n):
            return dict(op='assert_zone_multiplicity', player=p, zone='battlefield', index=i, value=n)
        self.assertIn(count(1, 1), first)
        self.assertIn(count(1, 3), due)
        self.assertIn(count(0, 0), due)
        self.assertIn(count(0, 0), self.after_upkeeps(empty['steps'])[1])
        self.assertEqual(sum(s['op']=='cast' for s in steps), 1)

    def test_subtypes_independently_affect_real_coat_witnesses(self):
        for name, first_stats in (("Nomads' Assembly", (2,1)), ("Ojutai's Summons", (5,3))):
            steps = self.cases(name, 'subtypes_0')[0]['steps']
            first, due, *_ = self.after_upkeeps(steps)
            token_count = 3 if name.startswith('Nomads') else 2
            for w, power in enumerate(first_stats):
                self.assertIn(dict(op='assert_card', ref=f'type_witness_{w}', property='power', value=power+1), first)
                self.assertIn(dict(op='assert_card', ref=f'type_witness_{w}', property='power', value=power+token_count), due)

    def test_first_summons_token_is_bound_before_recast_and_flies(self):
        flying, reach = self.cases("Ojutai's Summons", 'flying_0', 'reach_1')
        for case, blocked in ((flying, 0), (reach, 1)):
            steps = case['steps']
            bind = next(i for i, s in enumerate(steps) if s['op']=='bind_permanent')
            first_upkeep = next(i for i, s in enumerate(steps) if s['op']=='upkeep_step')
            self.assertLess(bind, first_upkeep)
            self.assertIn(dict(op='attack', ref='djinn', value=1), steps)
            self.assertIn(dict(op='block', ref='blocker', other='djinn', value=blocked), steps)

    def test_life_comparison_after_gain_is_strict_with_real_draw_receipts(self):
        equal, behind = self.cases('Survival Cache', 'equal_then_ahead_0', 'still_behind_1')
        old, due, *_ = self.after_upkeeps(equal['steps'])
        self.assertIn(dict(op='assert_life', player=0, value=20), old)
        self.assertIn(dict(op='assert_zone', player=0, zone='hand', value=0), old)
        self.assertIn(dict(op='assert_life', player=0, value=22), due)
        self.assertIn(dict(op='assert_zone_multiplicity', player=0, zone='hand', index=self.rows['Forest']['csvid'], value=1), due)
        later = self.after_upkeeps(behind['steps'])[1]
        self.assertIn(dict(op='assert_life', player=1, value=19), later)
        self.assertIn(dict(op='assert_zone', player=1, zone='library', value=2), later)

    def test_decree_pumps_only_casters_real_team_and_expires(self):
        steps = self.cases("Great Teacher's Decree", 'scheduled_1')[0]['steps']
        before, due, next_turn, *_ = self.after_upkeeps(steps)
        for ref, power, toughness in (('first', 4, 3), ('other', 3, 3), ('blocker', 3, 3)):
            for stat, value in (('power', power), ('toughness',toughness)):
                self.assertIn(dict(op='assert_card', ref=ref, property=stat, value=value), due)
        self.assertIn(dict(op='assert_card', ref='first', property='power', value=2), before)
        self.assertIn(dict(op='assert_card', ref='other', property='power', value=1), next_turn)

    def test_counter_decline_and_snapshot_have_expected_source_zones(self):
        i = self.rows['Survival Cache']['csvid']
        counter, decline, replay = self.cases('Survival Cache', 'countered_0', 'decline_0', 'snapshot_0')
        for case, zone in ((counter,'graveyard'), (decline,'exile')):
            steps = case['steps']
            self.assertNotIn('choose_trigger', [s['op'] for s in steps])
            self.assertIn(dict(op='assert_zone_multiplicity', player=0, zone=zone, index=i, value=1),
                          self.after_upkeeps(steps)[1])
        steps = replay['steps']
        capture, record, restore = [steps.index(dict(op=s)) for s in ('capture', 'record_state', 'restore')]
        self.assertIn(dict(op='choose_trigger', player=0, ref='source'), steps[:capture])
        self.assertEqual(steps[capture+1:record], steps[restore+1:-1])
        self.assertEqual(steps[capture+1], dict(op='upkeep_step', player=0))
        self.assertNotIn('choose_target', [s['op'] for s in steps])

    def test_sight_uses_real_ordered_selections_twice(self):
        steps = self.cases('Sight Beyond Sight', 'choose_second_1')[0]['steps']
        forest, island, mountain, plains, swamp = (self.rows[name]['csvid'] for name in model.LIBRARY)
        first, due, *_ = self.after_upkeeps(steps)
        for block, order in ((first, (mountain, plains, swamp, forest)),
                             (due, (swamp, forest, mountain))):
            for index, identity in enumerate(order):
                self.assertIn(dict(op='assert_zone_card', player=1, zone='library', index=index, value=identity),
                              block)
        self.assertEqual([s['value'] for s in steps if s['op'] == 'choose_zone_card'], [island, plains])

    def test_memoir_random_first_return_preserves_each_instant_and_the_sorcery(self):
        steps = self.cases('Surreal Memoir', 'scheduled_0')[0]['steps']
        first, due, *_ = self.after_upkeeps(steps)
        self.assertIn(dict(op='assert_zone', player=0, zone='hand', value=1), first)
        self.assertIn(dict(op='assert_zone', player=0, zone='hand', value=2), due)
        for name in model.INSTANTS:
            identity = self.rows[name]['csvid']
            self.assertIn(dict(op='assert_hand_graveyard_total', player=0, index=identity, value=1), first)
            self.assertIn(dict(op='assert_zone_multiplicity', player=0, zone='hand', index=identity, value=1), due)
        self.assertIn(dict(op='assert_zone_multiplicity', player=0, zone='graveyard',
                           index=self.rows[model.SORCERY]['csvid'], value=1), due)
        no_instant = self.cases('Surreal Memoir', 'no_instant_0')[0]['steps']
        self.assertIn(dict(op='can_cast', ref='source', value=1), no_instant)

