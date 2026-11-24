import json
import unittest

from generate_cases import registry
import next_upkeep_cantrip_additional_riders_recipes as model


class AdditionalDelayedCantripTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        data = model.PROFILE.parent
        cls.oracle = json.loads((data / 'oracle.json').read_bytes())['cards']
        cls.rows = {r['name']: r for r in registry() if r['kind'] != 'internal'}
        cls.homes = {}
        for name, pin in model.profiles()['sources'].items():
            i = pin['registry']['csvid']
            home, = (data / f'cases/cards/{i // 100:03}').glob(f'{i:05}-*.json')
            cls.homes[name] = json.loads(home.read_bytes())

    def case(self, name, variant):
        home = self.homes[name]
        decl = home['recipes'][0]
        params = model.spec(decl, self.oracle[name])
        case, = model.expand(name, home['csvid'], decl, params, [variant], self.rows, self.oracle)
        return case

    def test_complete_pinned_rules_for_all_four_cards(self):
        model.validate_witnesses(self.oracle, self.rows)
        self.assertEqual({h['csvid'] for h in self.homes.values()}, {2392, 1891, 2293, 1932})
        for name, home in self.homes.items():
            record = self.oracle[name]
            self.assertEqual(home['recipes'][0]['variants'], model.variants(model.classify(record)))
            with self.assertRaises(ValueError):
                model.classify(dict(record, oracle_text=record['oracle_text'] + '\nDraw a card.'))

    def test_opposite_calls_share_identical_seed_and_pre_flip_state(self):
        for player in (0, 1):
            win = self.case('Aleatory', f'win_{player}')
            lose = self.case('Aleatory', f'lose_{player}')
            self.assertEqual(win['seed'], model.COIN_SEED)
            self.assertEqual(win['seed'], lose['seed'])
            self.assertEqual(win['setup'], lose['setup'])
            at = next(i for i,r in enumerate(win['steps']) if r['op'] == 'choose_dialog')
            self.assertEqual(win['steps'][:at], lose['steps'][:at])
            self.assertIn(dict(op='advance_combat'), win['steps'][:at])
            self.assertEqual(win['steps'][at], dict(op='choose_dialog', player=player, value=model.actual_call(True)))
            self.assertEqual(lose['steps'][at]['value'], 1 - win['steps'][at]['value'])
            self.assertIn(dict(op='assert_card', ref='attacker', property='power', value=3), win['steps'][at:])
            self.assertNotIn(dict(op='assert_card', ref='attacker', property='power', value=3), lose['steps'][at:])

    def test_losing_coin_and_winning_coin_both_draw_only_at_next_upkeep(self):
        for mode in ('win', 'lose', 'offturn_win'):
            steps = self.case('Aleatory', mode + '_0')['steps']
            play = steps.index(dict(op='cast', ref='source', value=1))
            combat = steps.index(dict(op='combat_damage'), play)
            player = 0 if mode == 'offturn_win' else 1
            upkeep = steps.index(dict(op='upkeep_step', player=player), combat)
            top = self.rows['Forest']['csvid']
            no_draw = dict(op='assert_zone_multiplicity', player=0, zone='hand', index=top, value=0)
            draw = dict(no_draw, value=1)
            self.assertIn(no_draw, steps[combat:upkeep])
            self.assertNotIn(draw, steps[:upkeep])
            self.assertIn(draw, steps[upkeep:])

    def test_both_restricted_windows_are_actual_failed_casts(self):
        for name, mode, preceding in (('Aleatory', 'before_blocks', 'finish_attackers'),
                                      ('Gorilla War Cry', 'after_blocks', 'advance_combat')):
            steps = self.case(name, mode + '_1')['steps']
            deny = steps.index(dict(op='cast', ref='source', value=0))
            self.assertIn(dict(op=preceding), steps[:deny])
            self.assertNotIn('upkeep_step', [r['op'] for r in steps[deny:]])

    def test_menace_single_pair_and_expiry_use_real_declarations(self):
        for mode, expected in (('one_block', 0), ('pair_blocks', 1), ('expiry', 0)):
            steps = self.case('Gorilla War Cry', mode + '_0')['steps']
            resolve = steps.index(dict(op='resolve'))
            self.assertIn(dict(op='block', ref='blocker', other='attacker', value=expected), steps[resolve:])
            if mode == 'pair_blocks':
                self.assertIn(dict(op='block', ref='second_blocker', other='attacker', value=1), steps[resolve:])
            if mode == 'expiry':
                last_upkeep = max(i for i,r in enumerate(steps) if r['op'] == 'upkeep_step')
                self.assertIn(dict(op='block', ref='blocker', other='attacker', value=1), steps[last_upkeep:])

    def test_banding_allows_defender_to_pick_other_blocker(self):
        for mode, recipient in (('assign_holder', 'blocker'), ('assign_ally', 'second_blocker')):
            steps = self.case('Formation', mode + '_1')['steps']
            cast = steps.index(dict(op='cast', ref='source', value=1))
            damage = steps.index(dict(op='combat_damage'), cast)
            self.assertIn(dict(op='choose_target', player=1, ref='blocker'), steps[:cast])
            self.assertEqual(steps[damage - 1], dict(op='choose_target', player=0, ref=recipient))
            self.assertIn(dict(op='assert_card', ref=recipient, property='damage', value=1), steps[damage:])
        steps = self.case('Formation', 'expired_0')['steps']
        damage = steps.index(dict(op='combat_damage'))
        self.assertIn(dict(op='assert_unused_choices', value=1), steps[damage:])

    def test_clairvoyance_explicitly_receives_both_real_target_choices(self):
        for mode, target in (('own_hand', 1), ('effect', 0), ('empty_hand', 0)):
            case = self.case('Clairvoyance', mode + '_1')
            steps = case['steps']
            paid = steps.index(dict(op='cast', ref='source', value=1))
            self.assertIn(dict(op='choose_player', player=1, value=target), steps[:paid])
            self.assertIn(dict(op='upkeep_step', player=0), steps[paid:])
            self.assertIn('headless', case['_recipe']['untested_components'][0])

    def test_replay_repeats_real_coin_and_delayed_draw(self):
        steps = self.case('Aleatory', 'replay_0')['steps']
        first = steps.index(dict(op='capture'))
        record = steps.index(dict(op='record_state'))
        self.assertEqual(steps[first + 1:record], steps[record + 3:-1])
