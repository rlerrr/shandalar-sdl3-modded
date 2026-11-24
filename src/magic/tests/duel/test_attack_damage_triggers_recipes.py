"""Check printed clauses, meaningful attack partitions, and complete combat."""
import json
import unittest
from pathlib import Path

from generate_cases import registry
import attack_damage_triggers_recipes as model


class AttackDamageTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        data = Path(__file__).parent / 'data'
        cls.oracle = json.loads((data / 'oracle.json').read_bytes())['cards']
        cls.rows = {row['name']: row for row in registry()}
        cls.declarations = {}
        for name, pin in model.pins()['sources'].items():
            path, = (data / 'cases/cards').glob(f"*/{pin['csvid']:05d}-*.json")
            cls.declarations[name] = json.loads(path.read_bytes())['recipes'][0]

    def case(self, name, variant):
        spec = self.declarations[name]
        shape = model.spec(spec, self.oracle[name])
        return model.expand(name, shape['csvid'], spec, shape, [variant], self.rows, self.oracle)[0]

    def trigger_receipts(self, steps):
        return steps[next(i for i, item in enumerate(steps) if item['op'] == 'finish_attackers') + 1:
                     next(i for i, item in enumerate(steps) if item['op'] == 'begin_blockers')]

    def test_whole_record_and_selected_variant_pins(self):
        for name, spec in self.declarations.items():
            shape = model.spec(spec, self.oracle[name])
            self.assertEqual(spec['variants'], model.variants(shape))
            self.assertEqual(shape['clauses'], self.oracle[name]['oracle_text'].splitlines())
            with self.assertRaises(ValueError):
                model.spec(spec, dict(self.oracle[name], oracle_text=self.oracle[name]['oracle_text'] + '\nFlying'))

    def test_caltrops_own_and_enemy_attacks(self):
        steps = self.case('Caltrops', 'multiple_0')['steps']
        receipts = self.trigger_receipts(steps)
        self.assertIn(dict(op='assert_card', ref='host', property='damage', value=1), receipts)
        self.assertIn(dict(op='assert_zone_multiplicity', player=0, zone='graveyard',
                           index=self.rows['Elvish Mystic']['csvid'], value=1), receipts)
        enemy = self.trigger_receipts(self.case('Caltrops', 'opponent_attack_0')['steps'])
        self.assertIn(dict(op='assert_card', ref='enemy', property='damage', value=1), enemy)

    def test_bow_real_equip_and_defender_only_target(self):
        steps = self.case('Heart-Piercer Bow', 'hit_1')['steps']
        self.assertEqual([s['ref'] for s in steps if s['op'] == 'choose_target'], ['host', 'victim'])
        self.assertIn(dict(op='assert_card', ref='victim', property='damage', value=1), self.trigger_receipts(steps))
        no_target = self.case('Heart-Piercer Bow', 'no_target_1')['steps']
        self.assertEqual([s['ref'] for s in no_target if s['op'] == 'choose_target'], ['host'])
        denied = self.case('Heart-Piercer Bow', 'offturn_equip_1')['steps']
        self.assertIn(dict(op='activate', ref='source', value=0), denied)

    def test_slayer_uses_actual_attacked_walker_and_equipped_source(self):
        steps = self.case('Mage Slayer', 'walker_attack_0')['steps']
        attack = steps.index(dict(op='attack_with_tax', ref='host', value=1))
        self.assertEqual(steps[attack-1], dict(op='choose_dialog', player=0, value=1))
        self.assertIn(dict(op='assert_counter', ref='walker', counter_kind='loyalty', value=1),
                      self.trigger_receipts(steps))
        for name, life in [('Mage Slayer', 22), ('Heart-Piercer Bow', 20)]:
            receipts = self.trigger_receipts(self.case(name, 'lifelink_0')['steps'])
            self.assertIn(dict(op='assert_life', player=0, value=life), receipts)

    def test_raid_checks_live_inclusive_power_and_controller(self):
        for variant, life in [('hit_0', 19), ('zero_power_0', 19), ('above_threshold_0', 20),
                              ('higher_power_0', 20), ('multiple_0', 18)]:
            receipts = self.trigger_receipts(self.case('Raid Bombardment', variant)['steps'])
            self.assertIn(dict(op='assert_life', player=1, value=life), receipts)
        receipts = self.trigger_receipts(self.case('Raid Bombardment', 'opponent_attack_0')['steps'])
        self.assertIn(dict(op='assert_life', player=0, value=20), receipts)

    def test_salamander_option_nonflying_and_untargeted_hexproof(self):
        for variant, marked in [('hit_0', 1), ('decline_0', 0), ('hit_1', 1)]:
            steps = self.case('Scalding Salamander', variant)['steps']
            self.assertEqual(sum(s['op'] == 'choose_trigger_source' for s in steps), int(variant == 'hit_0'))
            self.assertFalse(any(s['op'] == 'choose_target' for s in steps))
            receipts = self.trigger_receipts(steps)
            self.assertIn(dict(op='assert_card', ref='ground', property='damage', value=marked), receipts)
            self.assertIn(dict(op='assert_card', ref='flyer', property='damage', value=0), receipts)
            if marked:
                self.assertIn(dict(op='assert_zone_multiplicity', player=1-int(variant[-1]), zone='graveyard',
                                   index=self.rows['Slippery Bogle']['csvid'], value=1), receipts)

    def test_thorncaster_per_sliver_choices_and_real_walker_recipient(self):
        for variant, choices in [('multiple_0', 2), ('peer_sliver_1', 1), ('non_sliver_0', 0), ('opponent_attack_1', 0)]:
            steps = self.case('Thorncaster Sliver', variant)['steps']
            self.assertEqual(sum(s['op'] == 'choose_player' for s in steps), choices)
        steps = self.case('Thorncaster Sliver', 'walker_target_1')['steps']
        self.assertIn(dict(op='choose_dialog', player=1, value=0), steps)
        self.assertIn(dict(op='choose_target', player=1, ref='walker'), steps)
        self.assertIn(dict(op='assert_counter', ref='walker', counter_kind='loyalty', value=2),
                      self.trigger_receipts(steps))

    def test_cavalcade_uses_the_printed_one_power_boundary(self):
        for variant, life in [('hit_0', 19), ('zero_power_0', 19), ('above_threshold_0', 20),
                              ('higher_power_0', 20), ('multiple_0', 18)]:
            steps = self.case('Cavalcade of Calamity', variant)['steps']
            self.assertIn(dict(op='assert_life', player=1, value=life), self.trigger_receipts(steps))
        steps = self.case('Cavalcade of Calamity', 'walker_attack_0')['steps']
        self.assertIn(dict(op='assert_counter', ref='walker', counter_kind='loyalty', value=2),
                      self.trigger_receipts(steps))

    def test_regisaur_damage_requires_its_own_attack(self):
        for variant, choices in [('hit_0', 1), ('other_attacker_0', 0), ('opponent_attack_0', 0)]:
            steps = self.case('Raging Regisaur', variant)['steps']
            self.assertEqual(sum(s['op'] == 'choose_player' for s in steps), choices)
        for variant, target in [('own_creature_0', 'bystander'), ('opponent_creature_0', 'victim'), ('walker_target_0', 'walker')]:
            steps = self.case('Raging Regisaur', variant)['steps']
            self.assertIn(dict(op='choose_target', ref=target, player=0), steps)

    def test_defensive_flight_and_real_attacked_object(self):
        for name, host_mark, peer_mark in [('Circle of Flame', 1, 0), ('Raking Canopy', 4, 0)]:
            for variant, expected in [('filter_0', host_mark), ('own_attack_1', 0),
                                      ('only_excluded_0', 0), ('walker_attack_1', host_mark if host_mark == 1 else 0)]:
                steps = self.case(name, variant)['steps']
                receipts = self.trigger_receipts(steps)
                self.assertIn(dict(op='assert_card', ref='host', property='damage', value=expected), receipts)
                if 'filter' in variant:
                    self.assertIn(dict(op='assert_card', ref='peer', property='damage', value=peer_mark), receipts)
                if 'walker' in variant:
                    idx = steps.index(dict(op='attack_with_tax', ref='host', value=1))
                    self.assertEqual(steps[idx-1], dict(op='choose_dialog', player=0, value=1))
                    self.assertIn(dict(op='assert_counter', ref='walker', counter_kind='loyalty', value=3), receipts)

    def test_minefield_block_marks_follow_real_declaration(self):
        steps = self.case('Powerstone Minefield', 'both_marked_0')['steps']
        self.assertIn(dict(op='assert_card', ref='host', property='damage', value=2), self.trigger_receipts(steps))
        declared = next(i for i,x in enumerate(steps) if x['op']=='advance_combat')
        damage = next(i for i,x in enumerate(steps) if x['op']=='combat_damage')
        self.assertIn(dict(op='cast', ref='window_spell', value=1), steps[declared:damage])
        self.assertIn(dict(op='assert_card', ref='victim', property='damage', value=2), steps[declared:damage])
        steps = self.case('Powerstone Minefield', 'both_lethal_1')['steps']
        declared = next(i for i,x in enumerate(steps) if x['op']=='advance_combat')
        damage = next(i for i,x in enumerate(steps) if x['op']=='combat_damage')
        self.assertIn(dict(op='assert_zone_multiplicity', player=0, zone='graveyard',
                           index=self.rows['Elvish Mystic']['csvid'], value=1), steps[declared:damage])
        self.assertEqual([s['value'] for s in steps if s['op']=='assert_life' and s['player']==0][-1], 20)

    def test_gladiator_return_damage_and_decline(self):
        for variant, mark in [('hit_0', 1), ('zero_return_1', 0), ('decline_0', 0)]:
            steps = self.case('Cyclops Gladiator', variant)['steps']
            self.assertIn(dict(op='assert_card', ref='source', property='damage', value=mark),
                          self.trigger_receipts(steps))
        receipt = self.trigger_receipts(self.case('Cyclops Gladiator', 'return_lethal_1')['steps'])
        self.assertIn(dict(op='assert_zone_multiplicity', player=1, zone='graveyard',
                           index=self.rows['Cyclops Gladiator']['csvid'], value=1), receipt)

    def test_attack_lifecycle_witness_types_are_independent(self):
        siege = self.case('Siege Dragon', 'cast_attack_1')
        by_ref = {entry['ref']: self.oracle[entry['name']] for entry in siege['setup']}
        self.assertNotIn('Wall', by_ref['ground']['type_line'].partition(' — ')[2].split())
        self.assertFalse({'Flying', 'Reach'} & set(by_ref['ground']['keywords']))
        for ref in ('own_wall', 'wall', 'other_wall'):
            self.assertIn('Wall', by_ref[ref]['type_line'].partition(' — ')[2].split())
        forger = self.case('Rage Forger', 'cast_attack_0')
        by_ref = {entry['ref']: self.oracle[entry['name']] for entry in forger['setup']}
        for ref in ('shaman', 'second_shaman', 'foreign_shaman'):
            self.assertIn('Shaman', by_ref[ref]['type_line'].partition(' — ')[2].split())
        self.assertNotIn('Shaman', by_ref['ordinary']['type_line'].partition(' — ')[2].split())

    def test_siege_real_wall_prohibition_and_entry(self):
        for variant, mark in [('wall_present_0', 0), ('own_wall_only_1', 2), ('cast_attack_1', 2)]:
            steps = self.case('Siege Dragon', variant)['steps']
            receipt = self.trigger_receipts(steps)
            self.assertIn(dict(op='assert_card', ref='ground', property='damage', value=mark), receipt)
            self.assertIn(dict(op='assert_card', ref='flyer', property='damage', value=0), receipt)
        steps = self.case('Siege Dragon', 'ground_block_0')['steps']
        self.assertIn(dict(op='block', ref='ground', other='source', value=0), steps)

    def test_rage_forger_tracks_real_receiving_shaman_and_damage_source(self):
        steps = self.case('Rage Forger', 'cast_attack_0')['steps']
        for ref, count in [('shaman', 1), ('second_shaman', 1), ('ordinary', 0), ('foreign_shaman', 0), ('source', 0)]:
            self.assertIn(dict(op='assert_card', ref=ref, property='plus1_counters', value=count),
                          self.trigger_receipts(steps))
        steps = self.case('Rage Forger', 'counted_lifelink_0')['steps']
        self.assertIn(dict(op='choose_target', ref='ally', player=0), steps)
        self.assertIn(dict(op='assert_life', player=0, value=21), self.trigger_receipts(steps))
        steps = self.case('Rage Forger', 'uncounted_0')['steps']
        self.assertFalse(any(step['op'] == 'choose_trigger_source' for step in steps))

    def test_responder_uses_actual_untap_then_exact_paid_activation(self):
        steps = self.case('Mage-Ring Responder', 'paid_reentry_0')['steps']
        natural = steps.index(dict(op='untap_step', player=0))
        paid = steps.index(dict(op='activate', ref='source', value=1))
        self.assertIn(dict(op='assert_card', ref='source', property='tapped', value=1), steps[natural:paid])
        self.assertEqual([s['ref'] for s in steps if s['op']=='choose_target'], ['victim', 'second'])
        steps = self.case('Mage-Ring Responder', 'short_untap_1')['steps']
        self.assertIn(dict(op='attack', ref='source', value=0), steps)

    def test_titan_splits_and_pays_for_temporary_power(self):
        entry = self.case('Inferno Titan', 'entry_split_0')['steps']
        cast = entry.index(dict(op='cast', ref='source', value=1))
        resolve = next(i for i,s in enumerate(entry) if i > cast and s['op'] == 'resolve')
        self.assertEqual([s['op'] for s in entry[cast:resolve] if s['op'].startswith('choose')],
                         ['choose_player', 'choose_target', 'choose_target'])
        steps = self.case('Inferno Titan', 'pump_attack_1')['steps']
        self.assertIn(dict(op='assert_card', ref='source', property='power', value=7), self.trigger_receipts(steps))
        self.assertEqual([s['value'] for s in steps if s['op']=='assert_card' and s.get('property')=='power'][-1], 6)

    def test_titan_combat_fixtures_pay_and_resolve_entry_separately(self):
        for mode in model.LIFECYCLES['titan']:
            if mode == 'entry_split':
                continue
            for owner in (0, 1):
                with self.subTest(mode=mode, owner=owner):
                    case = self.case('Inferno Titan', f'{mode}_{owner}')
                    source = next(item for item in case['setup'] if item['ref'] == 'source')
                    self.assertEqual(source['zone'], 'hand')
                    steps = case['steps']
                    cast = steps.index(dict(op='cast', ref='source', value=1))
                    finish = next(i for i, step in enumerate(steps) if step['op']=='finish_attackers')
                    begin = next(i for i, step in enumerate(steps) if step['op']=='begin_attackers')
                    resolves = next(i for i in range(cast+1, begin) if steps[i]['op']=='resolve')
                    self.assertEqual([step for step in steps[cast:resolves] if step['op'].startswith('choose')],
                                     [dict(op='choose_player', player=owner, value=owner)] * 3)
                    self.assertIn(dict(op='assert_life', player=owner, value=17), steps[resolves:begin])
                    self.assertEqual(sum(step['op']=='advance_turn' for step in steps[resolves:begin]),
                                     1 if mode == 'opponent_attack' else 2)
                    if mode == 'opponent_attack':
                        self.assertEqual([step for step in steps[begin:finish] if step['op'].startswith('choose')], [])
                    else:
                        self.assertEqual(sum(step['op'] in ('choose_target', 'choose_player')
                                             for step in steps[begin:finish]), 3)

    def test_preyseizer_selects_and_counts_genuine_sacrifices(self):
        for variant, count in [('devour_one_0', 2), ('devour_two_0', 4), ('devour_pass_0', 0)]:
            steps = self.case('Preyseizer Dragon', variant)['steps']
            paid = [s for s in steps if s['op']=='choose_target' and s['ref'].endswith('_meal')]
            self.assertEqual(len(paid), count//2)
            self.assertEqual(sum(s['op']=='choose_cancel_target' for s in steps), int(count<4))
            self.assertIn(dict(op='assert_card', ref='source', property='plus1_counters', value=count), steps)
        steps = self.case('Preyseizer Dragon', 'hit_1')['steps']
        self.assertIn(dict(op='choose_player', player=1, value=0), steps)
        self.assertIn(dict(op='assert_life', player=0, value=20), self.trigger_receipts(steps))

    def test_every_combat_and_replay_closes(self):
        combat = ['begin_attackers', 'finish_attackers', 'begin_blockers', 'advance_combat', 'combat_damage', 'end_combat']
        for name, spec in self.declarations.items():
            shape = model.spec(spec, self.oracle[name])
            for case in model.expand(name, shape['csvid'], spec, shape, spec['variants'], self.rows, self.oracle):
                variant = case['_recipe']['variant']
                steps = case['steps']
                mode = variant.rsplit('_', 1)[0]
                if shape['kind'] in model.LIFECYCLES:
                    count = 0 if mode in {'cast', 'short_cast', 'wrong_color', 'entry_split', 'devour_pass'} else 1
                    if mode in ('locked_next_turn', 'paid_reentry', 'short_untap', 'replay'):
                        count = 2
                else:
                    count = 2 if mode == 'replay' else int(mode not in model.CASTS - {'fresh_attack'})
                self.assertEqual([s['op'] for s in steps if s['op'] in combat], combat * count)
                if mode == 'replay':
                    capture, = [i for i,s in enumerate(steps) if s['op'] == 'capture']
                    record, = [i for i,s in enumerate(steps) if s['op'] == 'record_state']
                    self.assertEqual(steps[capture+1:record], steps[record+3:-1])
                    self.assertEqual(steps[-1]['op'], 'assert_recorded_state')


if __name__ == '__main__':
    unittest.main()
