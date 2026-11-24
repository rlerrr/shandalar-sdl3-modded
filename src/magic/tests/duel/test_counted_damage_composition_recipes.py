"""Guard the full typed recipient and resolution-count ledger."""
import copy
import json
import unittest
from pathlib import Path

import counted_damage_composition_recipes as model
from generate_cases import registry


class CountedDamageCompositionTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        data = Path(__file__).parent / 'data'
        cls.oracle = json.loads((data / 'oracle.json').read_bytes())['cards']
        cls.rows = {row['name']: row for row in registry()}
        cls.declarations = {}
        for name, pin in model.pins()['sources'].items():
            path, = (data / 'cases/cards').glob(f"*/{pin['csvid']:05d}-*.json")
            cls.declarations[name] = json.loads(path.read_bytes())['recipes'][0]

    def case(self, name, mode, owner):
        declaration = self.declarations[name]
        shape = model.spec(declaration, self.oracle[name])
        return model.expand(name, shape['csvid'], declaration, shape, [f'{mode}_{owner}'], self.rows, self.oracle)[0]

    def life(self, steps, player):
        return [s['value'] for s in steps if s['op'] == 'assert_life' and s['player'] == player][-1]

    def zone(self, steps, player, zone, name):
        return dict(op='assert_zone_multiplicity', player=player, zone=zone, index=self.rows[name]['csvid'], value=1) in steps

    def test_exact_sources_recipients_and_fees(self):
        model.validate_witnesses(self.oracle, self.rows)
        pairs = {'Spire Barrage': ('mountain', 'any'), 'Massive Raid': ('creature', 'any'),
                 'Mob Justice': ('creature', 'player_walker'), 'Goblin War Strike': ('Goblin', 'player_walker'),
                 'Stensia Banquet': ('Vampire', 'opponent_walker'), 'Skred': ('snow', 'creature')}
        self.assertEqual(sum(len(declaration['variants']) for declaration in self.declarations.values()), 240)
        for name, declaration in self.declarations.items():
            shape = model.spec(declaration, self.oracle[name])
            self.assertEqual((shape['scope'], shape['recipient']), pairs[name])
            self.assertEqual(declaration['variants'], model.variants(shape))
            self.assertEqual(shape['clauses'], self.oracle[name]['oracle_text'].splitlines())
        changed = copy.deepcopy(self.declarations['Stensia Banquet'])
        changed['params']['mechanism']['draw'] = False
        with self.assertRaises(ValueError):
            model.spec(changed, self.oracle['Stensia Banquet'])

    def test_player_and_planeswalker_recipients_are_actual_choices(self):
        for owner in (0, 1):
            for name in ('Spire Barrage', 'Massive Raid', 'Mob Justice', 'Goblin War Strike'):
                steps = self.case(name, 'self_player', owner)['steps']
                self.assertIn(dict(op='choose_player', player=owner, value=owner), steps)
                self.assertEqual(self.life(steps, owner), 19)
                self.assertEqual(self.life(steps, 1-owner), 20)
            for name in ('Massive Raid', 'Stensia Banquet', 'Goblin War Strike'):
                walker = self.case(name, 'own_walker', owner)
                self.assertEqual(next(card['player'] for card in walker['setup'] if card['ref'] == 'walker'), owner)
                self.assertIn(dict(op='choose_target', player=owner, ref='walker'), walker['steps'])
                self.assertIn(dict(op='assert_counter', ref='walker', counter_kind='loyalty', value=2), walker['steps'])
                dead = self.case(name, 'walker_lethal', owner)['steps']
                self.assertTrue(self.zone(dead, 1-owner, 'graveyard', 'Tibalt, the Fiend-Blooded'))
                self.assertEqual(self.life(dead, 1-owner), 20)

    def test_counts_use_own_battlefield_including_kindred_and_nonland_snow(self):
        for owner in (0, 1):
            for name, mixed in [('Spire Barrage', 2), ('Mob Justice', 2), ('Goblin War Strike', 3), ('Stensia Banquet', 3)]:
                for mode, amount in [('zero', 0), ('opponent_only', 0), ('other_zones', 0), ('own_mixed', mixed)]:
                    self.assertEqual(self.life(self.case(name, mode, owner)['steps'], 1-owner), 20-amount)
            warrior = self.case('Goblin War Strike', 'own_mixed', owner)
            self.assertTrue(self.zone(warrior['steps'], owner, 'battlefield', 'Boggart Shenanigans'))
            snow = self.case('Skred', 'own_mixed', owner)['steps']
            self.assertTrue(self.zone(snow, owner, 'battlefield', 'Phyrexian Ironfoot'))
            self.assertIn(dict(op='assert_card', ref='target', property='damage', value=2), snow)

    def test_count_changes_use_actual_tokens_lands_and_type_activation(self):
        for owner in (0, 1):
            for name, own_damage in [('Spire Barrage', 1), ('Mob Justice', 2), ('Goblin War Strike', 1), ('Stensia Banquet', 1)]:
                for mode, damage in [('gain_own', own_damage), ('gain_opponent', 1), ('lose_own', 0), ('lose_opponent', 1)]:
                    steps = self.case(name, mode, owner)['steps']
                    self.assertLess(steps.index(dict(op='announce_spell', ref='source')), steps.index(dict(op='resolve_response')))
                    self.assertEqual(self.life(steps, 1-owner), 20-damage)
            tribal = self.case('Stensia Banquet', 'gain_own', owner)['steps']
            target = tribal.index(dict(op='choose_target', player=1-owner, ref='type_subject'))
            activate = tribal.index(dict(op='activate', ref='type_ability', value=1))
            self.assertEqual(tribal[target:activate], [dict(op='choose_target', player=1-owner, ref='type_subject'),
                                                       dict(op='choose_dialog', player=1-owner, value=0)])
            lose = self.case('Goblin War Strike', 'lose_type', owner)['steps']
            self.assertIn(dict(op='choose_dialog', player=1-owner, value=1), lose)
            self.assertEqual(self.life(lose, 1-owner), 20)
            snow = self.case('Skred', 'gain_own', owner)['steps']
            self.assertTrue(self.zone(snow, owner, 'battlefield', 'Snow-Covered Mountain'))
            self.assertIn(dict(op='assert_card', ref='target', property='damage', value=1), snow)

    def test_banquet_draw_depends_on_resolution_even_at_zero(self):
        for owner in (0, 1):
            for mode in ('zero', 'own_walker', 'gain_own'):
                steps = self.case('Stensia Banquet', mode, owner)['steps']
                self.assertTrue(self.zone(steps, owner, 'hand', 'Island'))
                self.assertIn(dict(op='assert_zone_card', player=owner, zone='library', index=0, value=188), steps)
            for mode in ('countered', 'target_bounced', 'no_legal_target', 'unfunded'):
                steps = self.case('Stensia Banquet', mode, owner)['steps']
                self.assertFalse(self.zone(steps, owner, 'hand', 'Island'))
                self.assertEqual(self.life(steps, 1-owner), 20)

    def test_shroud_leaves_no_legal_recipient_and_short_mana_stays_unspent(self):
        for owner in (0, 1):
            for name in self.declarations:
                denied = self.case(name, 'no_legal_target', owner)
                self.assertIn(dict(op='can_cast', ref='source', value=0), denied['steps'])
                self.assertIn(dict(op='cast', ref='source', value=0), denied['steps'])
                self.assertFalse(any(s['op'].startswith('choose_') for s in denied['steps']))
                if name == 'Stensia Banquet':
                    self.assertEqual([(r['player'], r['ref']) for r in denied['setup'] if r['name'] == 'Ivory Mask'],
                                     [(1-owner, 'enemy_shroud')])
                unfunded = self.case(name, 'unfunded', owner)['steps']
                self.assertIn(dict(op='can_cast', ref='source', value=1), unfunded)
                self.assertIn(dict(op='cast', ref='source', value=0), unfunded)
                self.assertTrue(self.zone(unfunded, owner, 'hand', name))

    def test_creature_targets_and_actual_offturn_responses(self):
        for owner in (0, 1):
            for name in ('Spire Barrage', 'Massive Raid', 'Skred'):
                cases = self.case(name, 'own_creature', owner)
                self.assertIn(dict(op='choose_target', ref='target', player=owner), cases['steps'])
                self.assertIn(dict(op='assert_card', ref='target', property='damage', value=1), cases['steps'])
                self.assertTrue(self.zone(self.case(name, 'creature_lethal', owner)['steps'], 1-owner, 'graveyard', 'Elvish Mystic'))
            for name in ('Massive Raid', 'Skred'):
                steps = self.case(name, 'offturn_response', owner)['steps']
                start, parent, response, finish = [steps.index(dict(op=op, **fields)) for op, fields in (
                    ('advance_turn', {}), ('announce_spell', {'ref': 'support'}),
                    ('cast_response', {'ref': 'source', 'value': 1}), ('resolve_response', {}))]
                self.assertLess(start, parent)
                self.assertLess(parent, response)
                self.assertLess(response, finish)

    def test_pending_replay_includes_exact_changes_and_receipts(self):
        for owner in (0, 1):
            for name in self.declarations:
                steps = self.case(name, 'replay_update', owner)['steps']
                capture, record, ai = [steps.index(dict(op=op)) for op in ('capture', 'record_state', 'ai_roundtrip')]
                self.assertEqual(steps[capture+1:record], steps[ai+1:-1])
                self.assertIn(dict(op='resolve_response'), steps[capture:record])


if __name__ == '__main__':
    unittest.main()
