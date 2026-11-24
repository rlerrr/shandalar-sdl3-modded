"""Guard actual payment, mode and grave-return lifecycle scripts."""

import json
from pathlib import Path
import unittest

from generate_cases import registry
import graveyard_return_cost_modes_recipes as model


DUEL = Path(__file__).resolve().parent
ORACLE = DUEL / 'data/oracle.json'


class GraveyardReturnCostModesRecipes(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.oracle = json.loads(ORACLE.read_text(encoding='utf8'))['cards']
        cls.rows = {row['name']: row for row in registry()}
        cls.homes = {}
        for name in model.profile()['sources']:
            csvid = cls.rows[name]['csvid']
            path, = (DUEL / f'data/cases/cards/{csvid // 100:03d}').glob(f'{csvid:05d}-*.json')
            cls.homes[name] = json.loads(path.read_text(encoding='utf8'))

    def case(self, name, variant):
        home = self.homes[name]
        declaration, = home['recipes']
        params = model.spec(declaration, self.oracle[name])
        case, = model.expand(name, home['csvid'], declaration, params, [variant], self.rows, self.oracle)
        return case

    def copies(self, name, who, zone, count):
        return dict(op='assert_zone_multiplicity', player=who, zone=zone, index=self.rows[name]['csvid'], value=count)

    def test_whole_pinned_oracle_and_catalog(self):
        model.validate_witnesses(self.oracle, self.rows)
        self.assertEqual({home['csvid'] for home in self.homes.values()}, {5948, 14601, 11677, 5799})
        self.assertEqual(sum(len(home['recipes'][0]['variants']) for home in self.homes.values()), 118)
        for name, home in self.homes.items():
            recipe, = home['recipes']
            record = self.oracle[name]
            self.assertEqual(recipe['variants'], model.variants(model.spec(recipe, record)))
            with self.assertRaises(ValueError):
                model.classify(dict(record, oracle_text=record['oracle_text'] + '\nDraw a card.'))

    def test_genesis_payment_uses_real_lands_and_cannot_be_floated(self):
        for who in (0, 1):
            case = self.case('Genesis', f'effect_{who}')
            run = case['steps']
            upkeep = run.index(dict(op='upkeep_step', player=who))
            untap = run.index(dict(op='untap_step', player=who))
            self.assertIn(dict(op='prepare_graveyard_abilities'), run[:untap])
            self.assertFalse(any(s['op'] == 'set_mana' for s in run[untap:upkeep]))
            self.assertIn(dict(op='choose_dialog', player=who, value=0), run[untap:upkeep])
            self.assertEqual(sum(s['op'] == 'choose_mana_source' for s in run[untap:upkeep]), 3 if who == 0 else 0)
            for index in range(3):
                self.assertIn(dict(op='assert_card', ref=f'producer_{index}', property='tapped', value=1), run[upkeep:])
            self.assertIn(self.copies('Grizzly Bears', who, 'hand', 1), run[upkeep:])
            for variant in ('short', 'wrong_color', 'opposing_funds', 'outside_grave', 'shroud'):
                denied = self.case('Genesis', f'{variant}_{who}')['steps']
                self.assertFalse(any(s['op'] == 'choose_zone_card' for s in denied))

    def test_genesis_acts_only_from_grave_and_can_return_itself(self):
        for who in (0, 1):
            run = self.case('Genesis', f'cast_then_dies_{who}')['steps']
            death = run.index(dict(op='cast', ref='reply', value=1))
            upkeep = run.index(dict(op='upkeep_step', player=who))
            self.assertIn(self.copies('Genesis', who, 'graveyard', 1), run[death:upkeep])
            self.assertIn(self.copies('Grizzly Bears', who, 'hand', 1), run[upkeep:])
            self_return = self.case('Genesis', f'self_return_{who}')['steps']
            upkeep = self_return.index(dict(op='upkeep_step', player=who))
            self.assertIn(dict(op='choose_zone_card', player=who, value=5948), self_return[:upkeep])
            self.assertIn(self.copies('Genesis', who, 'hand', 1), self_return[upkeep:])
            decline = self.case('Genesis', f'decline_{who}')['steps']
            self.assertIn(dict(op='choose_dialog', player=who, value=1), decline)
            self.assertFalse(any(s['op'] == 'choose_mana_source' for s in decline))

    def test_palace_actual_entry_modes_and_opponent_timing(self):
        for who in (0, 1):
            for mode, choice in [('effect', 0), ('dragons', 1)]:
                run = self.case('Palace Siege', f'{mode}_{who}')['steps']
                resolve = run.index(dict(op='resolve'))
                self.assertEqual(run[resolve - 1], dict(op='choose_dialog', player=who, value=choice))
                opponent = run.index(dict(op='upkeep_step', player=1 - who))
                own = run.index(dict(op='upkeep_step', player=who))
                self.assertIn(dict(op='assert_life', player=who, value=20), run[opponent:own])
                if choice:
                    self.assertIn(dict(op='assert_life', player=who, value=22), run[own:])
                    self.assertIn(dict(op='assert_life', player=1 - who, value=18), run[own:])
                    self.assertFalse(any(step['op'] == 'choose_zone_card' for step in run))
                else:
                    self.assertIn(self.copies('Grizzly Bears', who, 'hand', 1), run[own:])

    def test_sheoldred_leaves_sacrifice_choice_to_opponent(self):
        name = 'Sheoldred, Whispering One'
        for who in (0, 1):
            run = self.case(name, f'sacrifice_alt_{who}')['steps']
            opponent = run.index(dict(op='upkeep_step', player=1 - who))
            own = run.index(dict(op='upkeep_step', player=who))
            if who == 1:
                self.assertEqual(run[opponent - 1], dict(op='choose_target', player=0, ref='alternative'))
            else:
                self.assertFalse(any(action['op'] == 'choose_target' for action in run[:opponent]))
            self.assertIn(self.copies('Hill Giant', 1 - who, 'graveyard', 1), run[opponent:own])
            self.assertIn(self.copies('Darksteel Relic', 1 - who, 'battlefield', 1), run[opponent:own])
            self.assertIn(self.copies('Grizzly Bears', who, 'battlefield', 1), run[own:])

    def test_sheoldred_swampwalk_uses_defenders_actual_land_subtype(self):
        name = 'Sheoldred, Whispering One'
        for who in (0, 1):
            for mode, succeeds in [('walk_swamp', 0), ('walk_dual', 0), ('walk_open', 1)]:
                run = self.case(name, f'{mode}_{who}')['steps']
                combat = run.index(dict(op='combat_damage'))
                self.assertIn(dict(op='block', ref='blocker', other='source', value=succeeds), run[:combat])
                self.assertIn(dict(op='assert_life', player=1 - who, value=20 if succeeds else 14), run[combat:])

    def test_dawn_real_haste_and_exile_with_independent_life_loss(self):
        for who in (0, 1):
            for mode in ('haste_exile', 'source_gone', 'target_bounced'):
                run = self.case('Dawn of the Dead', f'{mode}_{who}')['steps']
                own = run.index(dict(op='upkeep_step', player=who))
                end = run.index(dict(op='cleanup'))
                self.assertIn(dict(op='assert_life', player=who, value=19), run[own:end])
                if mode == 'target_bounced':
                    self.assertIn(self.copies('Grizzly Bears', who, 'hand', 1), run[end:])
                    self.assertIn(self.copies('Grizzly Bears', who, 'exile', 0), run[end:])
                else:
                    self.assertIn(dict(op='attack', ref='returned', value=1), run[own:end])
                    self.assertIn(self.copies('Grizzly Bears', who, 'exile', 1), run[end:])
            for mode in ('decline', 'empty', 'shroud'):
                run = self.case('Dawn of the Dead', f'{mode}_{who}')['steps']
                self.assertIn(dict(op='assert_life', player=who, value=19), run)
                self.assertFalse(any(s['op'] == 'choose_zone_card' for s in run))

    def test_replay_keeps_exact_actual_callbacks_and_destination_receipts(self):
        for name in self.homes:
            run = self.case(name, 'replay_1')['steps']
            cut = run.index(dict(op='capture'))
            record = run.index(dict(op='record_state'))
            restore = run.index(dict(op='restore'))
            self.assertEqual(run[cut + 1:record], run[restore + 1:-1])
            self.assertEqual(run[-1], dict(op='assert_recorded_state'))
