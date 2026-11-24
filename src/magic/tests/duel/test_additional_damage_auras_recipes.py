"""Guard host attribution, real sacrifice, reciprocal damage and ordered draws."""
import json
import unittest

import additional_damage_auras_recipes as model
from generate_cases import registry


class AdditionalDamageAuraTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.oracle = json.loads((model.PROFILE.parent / 'oracle.json').read_text(encoding='utf8'))['cards']
        cls.rows = {entry['name']: entry for entry in registry()}

    def case(self, name, variant):
        oracle = self.oracle[name]
        shape = model.parameters(oracle)
        declaration = dict(family=model.FAMILY, version=2, oracle_text=oracle['oracle_text'], params=shape, variants=[variant])
        return model.expand(name, self.rows[name]['csvid'], declaration, model.spec(declaration, oracle),
                            [variant], self.rows, self.oracle)[0]

    @staticmethod
    def receipts(steps, op, **fields):
        return [s['value'] for s in steps if s['op']==op and all(s.get(name)==value for name,value in fields.items())]

    def test_scope_and_complete_text(self):
        model.validate_witnesses(self.oracle, self.rows)
        self.assertEqual({pin['registry']['csvid'] for pin in model.profiles()['sources'].values()}, {2973, 5424, 10635})
        for name, pin in model.profiles()['sources'].items():
            oracle = self.oracle[name]
            self.assertEqual(oracle, pin['oracle'])
            with self.assertRaises(ValueError):
                model.parameters(dict(oracle, oracle_text=oracle['oracle_text'] + '\nDraw a card.'))
            steps = self.case(name, 'short_0')['steps']
            self.assertIn(dict(op='cast', ref='source', value=0), steps)
            self.assertEqual({(s['player'],s['color']) for s in steps[:14] if s['op']=='set_mana'},
                             {(p,c) for p in (0,1) for c in range(6)})

    def test_fire_whip_sacrifices_for_real_without_tapping_host(self):
        for owner in (0,1):
            for branch,tapped in [('sacrifice_ready',0),('sacrifice_tapped',1),('sacrifice_fresh',0)]:
                case = self.case('Fire Whip', f'{branch}_{owner}')
                steps = case['steps']
                act = steps.index(dict(op='activate', ref='source', value=1))
                end = next(i for i,s in enumerate(steps[act:],act) if s['op']=='resolve')
                self.assertEqual([s['mode_label'] for s in steps if s['op']=='choose_activation_mode'], ['Sac Fire Whip'])
                self.assertEqual(self.receipts(steps[act:end], 'assert_zone_multiplicity', player=owner, zone='graveyard', index=2973), [1])
                self.assertEqual(self.receipts(steps[act:end], 'assert_card', ref='host', property='tapped'), [tapped])
                self.assertEqual(self.receipts(steps[end:], 'assert_life', player=1-owner), [19])
            combined = self.case('Fire Whip', f'sacrifice_after_tap_{owner}')['steps']
            self.assertEqual([s['mode_label'] for s in combined if s['op']=='choose_activation_mode'], ['Tap enchanted creature','Sac Fire Whip'])
            self.assertEqual(self.receipts(combined, 'assert_life', player=1-owner)[-1], 18)

    def test_fire_whip_damage_uses_distinct_sources_and_own_enchant_gate(self):
        for owner in (0,1):
            steps = self.case('Fire Whip', f'source_colors_{owner}')['steps']
            self.assertIn(dict(op='probe_activation_target', ref='source', other='victim', index=0, value=0), steps)
            self.assertIn(dict(op='choose_target', ref='victim', player=owner), steps)
            self.assertEqual(self.receipts(steps, 'assert_card', ref='victim', property='damage')[-1], 1)
            self.assertEqual(self.receipts(steps, 'assert_life', player=owner)[-1], 20)
            ping = self.case('Fire Whip', f'host_lifelink_{owner}')['steps']
            self.assertEqual(self.receipts(ping, 'assert_life', player=owner)[-1], 21)
            gate = self.case('Fire Whip', f'foreign_gate_{owner}')['steps']
            self.assertIn(dict(op='can_cast', ref='source', value=0), gate)
            lost = self.case('Fire Whip', f'lost_control_{owner}')['steps']
            self.assertEqual(self.receipts(lost, 'assert_zone_multiplicity', player=owner, zone='graveyard', index=2973), [1])

    def test_dagger_targets_and_draws_for_actual_host_controller(self):
        for owner in (0,1):
            walker = self.case('Quicksilver Dagger', f'walker_{owner}')['steps']
            self.assertEqual(self.receipts(walker, 'assert_counter', ref='victim', counter_kind='loyalty')[-1], 2)
            self.assertEqual(self.receipts(walker, 'assert_zone', player=owner, zone='library')[-1], 2)
            self.assertEqual(self.receipts(walker, 'assert_zone', player=1-owner, zone='library')[-1], 3)
            changed = self.case('Quicksilver Dagger', f'changed_control_{owner}')['steps']
            self.assertIn(dict(op='choose_player', player=1-owner, value=owner), changed)
            self.assertEqual(self.receipts(changed, 'assert_zone', player=1-owner, zone='library')[-1], 2)
            self.assertEqual(self.receipts(changed, 'assert_zone', player=owner, zone='library')[-1], 3)
            self.assertEqual(self.receipts(changed, 'assert_zone_card', player=1-owner, zone='library', index=0)[-1],
                             self.rows['Island' if owner else 'Mountain']['csvid'])
            illegal = self.case('Quicksilver Dagger', f'creature_denied_{owner}')['steps']
            self.assertIn(dict(op='probe_activation_target', ref='source', other='victim', index=-1, value=0), illegal)

    def test_dagger_target_loss_suppresses_damage_and_draw(self):
        for owner in (0,1):
            body = self.case('Quicksilver Dagger', f'walker_leaves_{owner}')
            self.assertIn(("Hero's Downfall",'answer'), [(s['name'],s['ref']) for s in body['setup']])
            steps = body['steps']
            self.assertEqual(self.receipts(steps, 'cast', ref='answer'), [1])
            self.assertEqual(self.receipts(steps, 'assert_stack'), [1,0,0,1,2,0,0])
            self.assertEqual(self.receipts(steps, 'assert_zone', player=owner, zone='library')[-1], 3)
            self.assertEqual(self.receipts(steps, 'assert_zone_multiplicity', player=1-owner, zone='graveyard', index=9185), [1])

    def test_predatory_urge_preserves_dual_damage_and_moved_owners(self):
        for owner in (0,1):
            both = self.case('Predatory Urge', f'double_lifelink_{owner}')['steps']
            self.assertEqual(self.receipts(both, 'assert_life', player=owner)[-1], 22)
            self.assertEqual(self.receipts(both, 'assert_life', player=1-owner)[-1], 22)
            self.assertEqual(self.receipts(both, 'assert_card', ref='victim', property='damage')[-1], 2)
            self.assertEqual(self.receipts(both, 'assert_zone_multiplicity', player=owner, zone='graveyard', index=10635), [1])
            up = self.case('Predatory Urge', f'power_up_{owner}')['steps']
            self.assertEqual(self.receipts(up, 'assert_card', ref='host', property='power')[-1], 7)
            self.assertEqual(self.receipts(up, 'assert_card', ref='host', property='damage')[-1], 5)
            zero = self.case('Predatory Urge', f'zero_power_{owner}')['steps']
            self.assertEqual(self.receipts(zero, 'assert_card', ref='host', property='damage')[-1], 1)
            self.assertEqual(self.receipts(zero, 'assert_card', ref='victim', property='damage')[-1], 0)

    def test_replays_start_before_action_or_with_real_pending_ability(self):
        for name in model.profiles()['sources']:
            for mode in ('replay','pending_replay'):
                steps = self.case(name, f'{mode}_1')['steps']
                cut = steps.index(dict(op='capture'))
                activation = steps.index(dict(op='activate', ref='source', value=1))
                self.assertEqual(cut>activation, mode=='pending_replay')
                self.assertEqual(steps[-1], dict(op='assert_recorded_state'))
