"""Focused guards for the global upkeep grant and payment fixture."""

import copy
import json
from pathlib import Path
import unittest

from generate_cases import registry
import grant_upkeep_sacrifice_recipes as model


DUEL = Path(__file__).resolve().parent


class GrantUpkeepSacrificeRecipes(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.oracle = json.loads((DUEL / 'data/oracle.json').read_text(encoding='utf8'))['cards']
        cls.rows = {row['name']: row for row in registry()}
        cls.homes = {}
        for name in model.pins()['sources']:
            csvid = cls.rows[name]['csvid']
            home, = (DUEL / f'data/cases/cards/{csvid // 100:03d}').glob(f'{csvid:05d}-*.json')
            cls.homes[name] = json.loads(home.read_text(encoding='utf8'))

    def case(self, name, variant):
        home = self.homes[name]
        recipe, = home['recipes']
        params = model.spec(recipe, self.oracle[name])
        case, = model.expand(name, home['csvid'], recipe, params, [variant], self.rows, self.oracle)
        return case

    @staticmethod
    def first(case):
        return case['steps'][:case['steps'].index({'op': 'record_state'})]

    def zone(self, run, card, player, zone):
        csvid = self.rows[card]['csvid']
        return [s['value'] for s in run if s['op'] == 'assert_zone_multiplicity' and s['player'] == player
                and s['zone'] == zone and s['index'] == csvid][-1]

    def test_complete_oracle_family_and_typed_pins(self):
        self.assertEqual({row['csvid'] for row in self.homes.values()}, {494, 806, 1287, 3024, 3969, 7675})
        self.assertEqual(sum(len(row['recipes'][0]['variants']) for row in self.homes.values()), 180)
        model.validate_witnesses(self.oracle, self.rows)
        for name, home in self.homes.items():
            recipe, = home['recipes']
            oracle = self.oracle[name]
            params = model.spec(recipe, oracle)
            self.assertEqual(set(recipe['variants']), set(model.variants(params)))
            bad = copy.deepcopy(recipe)
            bad['params']['unpaid'] = 'sacrifice' if params['unpaid'] == 'destroy' else 'destroy'
            with self.assertRaises(ValueError):
                model.spec(bad, oracle)
            with self.assertRaises(ValueError):
                model.classify(dict(oracle, oracle_text=oracle['oracle_text'] + '\nFlying'))

    def test_real_upkeep_and_replay_for_both_payers(self):
        for owner in (0, 1):
            name = 'Pendrell Mists'
            payer = 1 - owner
            case = self.case(name, f'paid_other_{owner}')
            run = self.first(case)
            ops = [step['op'] for step in run]
            self.assertLess(ops.index('advance_turn'), ops.index('untap_step'))
            self.assertLess(ops.index('untap_step'), ops.index('choose_dialog'))
            self.assertLess(ops.index('choose_dialog'), ops.index('upkeep_step'))
            self.assertEqual(run[ops.index('upkeep_step')]['player'], payer)
            turn = ops.index('turn')
            starting_pools = [dict(op='set_mana', player=seat, color=color, value=0)
                              for seat in (0, 1) for color in range(6)]
            self.assertEqual(run[turn+1:turn+14], starting_pools + [dict(op='refresh')])
            self.assertEqual([s for s in run if s['op'] == 'set_mana'], starting_pools)
            self.assertIn(dict(op='assert_card', ref='producer_0', property='tapped', value=1), run)
            self.assertEqual(self.zone(run, 'Grizzly Bears', payer, 'battlefield'), 1)
            self.assertEqual(case['steps'][len(run)+2:], run[1:] + [dict(op='assert_recorded_state')])

    def test_actual_refusal_and_insufficient_payment(self):
        name = 'Aura Flux'
        refused = self.first(self.case(name, 'decline_other_1'))
        short = self.first(self.case(name, 'short_other_1'))
        self.assertIn(dict(op='choose_dialog', player=0, value=1), refused)
        self.assertNotIn('choose_dialog', [s['op'] for s in short])
        for run in (refused, short):
            self.assertEqual(self.zone(run, 'Fervor', 0, 'graveyard'), 1)
            self.assertEqual(self.zone(run, name, 1, 'battlefield'), 1)
            self.assertNotIn('choose_mana_source', [s['op'] for s in run])
            self.assertNotIn(dict(op='assert_card', ref='producer_0', property='tapped', value=1), run)

    def test_aura_exemption_and_magus_self_grant(self):
        for owner in (0, 1):
            flux = 'Aura Flux'
            excluded = self.first(self.case(flux, f'self_excluded_{owner}'))
            taxed = self.first(self.case(flux, f'short_own_{owner}'))
            self.assertEqual(self.zone(excluded, flux, owner, 'battlefield'), 1)
            self.assertEqual(self.zone(taxed, flux, owner, 'battlefield'), 1)
            self.assertEqual(self.zone(taxed, 'Fervor', owner, 'graveyard'), 1)
            magus = 'Magus of the Tabernacle'
            unpaid = self.first(self.case(magus, f'short_own_{owner}'))
            self.assertEqual(self.zone(unpaid, magus, owner, 'graveyard'), 1)

    def test_tabernacle_indestructibility_differs_from_sacrifice(self):
        for name, moves in [('The Tabernacle at Pendrell Vale', False), ('Pendrell Mists', True),
                            ('Magus of the Tabernacle', True), ('Energy Flux', True), ("Kataki, War's Wage", True)]:
            run = self.first(self.case(name, 'dual_type_1'))
            self.assertEqual(self.zone(run, 'Darksteel Myr', 0, 'battlefield'), int(not moves))
            self.assertEqual(self.zone(run, 'Darksteel Myr', 0, 'graveyard'), int(moves))

    def test_two_payments_and_automatic_energy_flux_choice(self):
        name = 'Energy Flux'
        human = self.first(self.case(name, 'two_paid_1'))
        choices = [s for s in human if s['op'] in ('choose_dialog', 'choose_mana_source')]
        self.assertEqual([s['op'] for s in choices], ['choose_dialog', 'choose_mana_source',
            'choose_mana_source', 'choose_dialog', 'choose_mana_source', 'choose_mana_source'])
        self.assertEqual(self.zone(human, 'Darksteel Myr', 0, 'battlefield'), 1)
        ai = self.first(self.case(name, 'paid_other_0'))
        self.assertFalse(any(s['op'] in ('choose_dialog', 'choose_mana_source') for s in ai))
        # Automatic pay chooses a positive-value artifact. Human payment and
        # zero-budget cases retain the independently pinned noncreature Relic.
        for variant in ('paid_other_0', 'paid_own_1', 'colored_mana_0', 'repeat_pay_1'):
            paid = self.first(self.case(name, variant))
            self.assertEqual(self.zone(paid, 'Darksteel Myr', 1, 'battlefield'), 1)
            self.assertNotIn('choose_dialog', [s['op'] for s in paid])
        self.assertEqual(self.zone(self.first(self.case(name, 'paid_other_1')), 'Darksteel Relic', 0, 'battlefield'), 1)
        self.assertEqual(self.zone(self.first(self.case(name, 'short_other_0')), 'Darksteel Relic', 1, 'graveyard'), 1)
        for i in (0, 1):
            self.assertIn(dict(op='assert_card', ref=f'producer_{i}', property='tapped', value=1), ai)

    def test_spell_and_land_entry_and_removal_are_real(self):
        for name in ('Pendrell Mists', 'The Tabernacle at Pendrell Vale'):
            land = name.startswith('The Tabernacle')
            enters = self.first(self.case(name, 'source_enters_1'))
            entry_op = 'play_land' if land else 'cast'
            self.assertIn(dict(op=entry_op, ref='source', value=1), enters)
            self.assertEqual(self.zone(enters, 'Grizzly Bears', 0, 'graveyard'), 1)
            removed = self.first(self.case(name, 'source_removed_0'))
            self.assertIn(dict(op='cast', ref='removal', value=1), removed)
            self.assertIn(dict(op='choose_target', ref='source', player=1), removed)
            self.assertEqual(self.zone(removed, name, 0, 'graveyard'), 1)
            self.assertEqual(self.zone(removed, 'Grizzly Bears', 0, 'battlefield'), 1)

    def test_actor_and_type_scope_and_subsequent_upkeep(self):
        name = 'Pendrell Mists'
        scoped = self.first(self.case(name, 'scope_0'))
        self.assertEqual(self.zone(scoped, 'Grizzly Bears', 0, 'battlefield'), 1)
        self.assertEqual(self.zone(scoped, 'Grizzly Bears', 1, 'graveyard'), 1)
        self.assertEqual(self.zone(scoped, 'Fervor', 1, 'battlefield'), 1)
        repeated = self.first(self.case(name, 'repeat_pay_0'))
        self.assertEqual([s['player'] for s in repeated if s['op'] == 'upkeep_step'], [0, 1, 0])
        self.assertEqual([s['player'] for s in repeated if s['op'] == 'choose_dialog'], [0, 0])
        self.assertEqual(self.zone(repeated, 'Grizzly Bears', 0, 'battlefield'), 1)


if __name__ == '__main__':
    unittest.main()
