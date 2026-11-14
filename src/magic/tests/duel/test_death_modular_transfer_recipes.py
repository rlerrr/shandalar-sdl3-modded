import copy
import json
import unittest
from pathlib import Path
from generate_cases import registry
from recipes import expand as public_expand, cost_from_oracle
import death_modular_transfer_recipes as family


class DeathModularTransferTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.oracle = json.loads((Path(__file__).parent/'data/oracle.json').read_text(encoding='utf8'))['cards']
        cls.rows = {r['name']: r for r in registry()}
        cls.data = family.pins()

    def declaration(self, name, variants):
        return dict(family=family.FAMILY, version=2, params=dict(schema=1, **family.oracle_model(self.oracle[name])),
                    oracle_text=self.oracle[name]['oracle_text'], variants=variants)

    def case(self, name='Arcbound Worker', mode='transfer', player=0):
        d = self.declaration(name, [f'{mode}_{player}'])
        return public_expand(name, self.rows[name]['csvid'], d, self.oracle[name], self.rows, self.oracle)[0]

    def has(self, c, **fields):
        return any(all(s.get(k) == v for k, v in fields.items()) for s in c['steps'])

    def receipt(self, c, ref, prop, value):
        return self.has(c, op='assert_card', ref=ref, property=prop, value=value)

    def zone(self, c, name, player, zone, value):
        return self.has(c, op='assert_zone_multiplicity', index=self.rows[name]['csvid'], player=player, zone=zone, value=value)

    def test_whole_oracle_and_typed_parameters_fail_closed(self):
        for n in self.data['sources']:
            d = self.declaration(n, ['transfer_0']); family.spec(d, self.oracle[n])
            for k, v in [('schema', True), ('initial_counters', True), ('sunburst', 0), ('keywords', ['Trample'])]:
                bad = copy.deepcopy(d); bad['params'][k] = v
                with self.assertRaises(ValueError): family.spec(bad, self.oracle[n])
            for text in (self.oracle[n]['oracle_text']+' Draw a card.', self.oracle[n]['oracle_text']+'\nFlying'):
                with self.assertRaises(ValueError): family.oracle_model(dict(self.oracle[n], oracle_text=text))
            for field, value in [('layout', 'transform'), ('power', '1'), ('type_line', 'Creature — Construct')]:
                with self.assertRaises(ValueError): family.oracle_model(dict(self.oracle[n], **{field: value}))
        for n in ('Arcbound Crusher', 'Arcbound Overseer', 'Arcbound Reclaimer', 'Arcbound Slith'):
            with self.assertRaises(ValueError): family.oracle_model(self.oracle[n])

    def test_complete_pins_and_implemented_normal_sources(self):
        family.validate_witnesses(self.oracle, self.rows)
        self.assertEqual({x['csvid'] for x in self.data['sources'].values()}, {6854, 6853, 6847, 7004})
        for n in ('Memnite', 'Sol Ring', 'Burst of Strength', 'Fume Spitter', 'Control Magic'):
            changed = copy.deepcopy(self.oracle); changed[n]['oracle_text'] += ' changed'
            with self.assertRaises(ValueError): family.validate_witnesses(changed, self.rows)
        for n in self.data['sources']:
            changed = copy.deepcopy(self.rows); changed[n]['handler'] = 'card_dummy'
            with self.assertRaises(ValueError): family.validate_witnesses(self.oracle, changed)

    def test_four_reserved_homes_declare_the_complete_variant_matrix(self):
        for n, source in self.data['sources'].items():
            ident = source['csvid']; slug = n.lower().replace(' ', '-')
            path = Path(__file__).parent/f'data/cases/cards/{ident//100:03}/{ident:05}-{slug}.json'
            home = json.loads(path.read_text(encoding='utf8'))
            self.assertEqual((home['card'], home['csvid']), (n, ident))
            self.assertEqual(len(home['recipes']), 1)
            d = home['recipes'][0]; m = family.spec(d, self.oracle[n])
            self.assertEqual(d['variants'], family.variants(m))

    def test_all_variants_route_without_rule_seeding(self):
        total = 0
        forbidden = {'destroy', 'move_card', 'set_counter', 'dispatch_gfp', 'set_life', 'set_budget'}
        for n, source in self.data['sources'].items():
            d = self.declaration(n, ['transfer_0']); model = family.spec(d, self.oracle[n]); d['variants'] = family.variants(model)
            cases = public_expand(n, source['csvid'], d, self.oracle[n], self.rows, self.oracle); total += len(cases)
            for c in cases:
                self.assertFalse(any(s['op'] in forbidden for s in c['steps']))
                self.assertTrue(self.has(c, op='assert_unused_choices', value=0))
                self.assertTrue(self.has(c, op='assert_choices_consumed'))
                self.assertEqual(c['_recipe']['scope_gaps'], family.GAPS)
        self.assertEqual(total, 236)

    def test_actual_printed_entry_and_cost_receipts(self):
        for n in self.data['sources']:
            for p in (0, 1):
                c = self.case(n, player=p); m = family.oracle_model(self.oracle[n]); amount = 1 if m['sunburst'] else m['initial_counters']
                self.assertTrue(self.receipt(c, 'source', 'plus1_counters', amount))
                self.assertTrue(self.receipt(c, 'recipient', 'plus1_counters', amount))
                self.assertTrue(self.zone(c, n, p, 'graveyard', 1))
                self.assertTrue(self.zone(c, 'Murder', 1-p, 'graveyard', 1))
                self.assertTrue(self.has(c, op='cast', ref='source', value=1))
                self.assertTrue(all(self.has(c, op='assert_mana', player=p, color=i, value=0) for i in range(6)))
                d = self.declaration(n, ['transfer_0']); self.assertEqual(family.spec(d, self.oracle[n])['mana'], cost_from_oracle(self.oracle[n]))

    def test_affordability_and_real_announced_counter_response(self):
        for n in self.data['sources']:
            for p in (0, 1):
                c = self.case(n, 'insufficient_mana', p)
                self.assertTrue(self.has(c, op='cast', ref='source', value=0))
                self.assertTrue(self.zone(c, n, p, 'hand', 1))
                c = self.case(n, 'countered', p); s = c['steps']
                announce = next(i for i, x in enumerate(s) if x['op'] == 'announce_spell')
                response = next(i for i, x in enumerate(s) if x['op'] == 'cast_response')
                self.assertLess(announce, response)
                self.assertTrue(self.zone(c, n, p, 'graveyard', 1))
                self.assertFalse(any(x['op'].startswith('choose_trigger') for x in s))
                self.assertFalse(self.receipt(c, 'recipient', 'plus1_counters', 1))

    def test_artifact_and_creature_scope_and_enemy_recipient(self):
        for p in (0, 1):
            for mode, witness in [('nonartifact_recipient', 'Grizzly Bears'), ('noncreature_recipient', 'Sol Ring')]:
                c = self.case(mode=mode, player=p)
                self.assertTrue(any(x['ref'] == 'recipient' and x['name'] == witness for x in c['setup']))
                self.assertFalse(any(x['op'] == 'choose_target' and x.get('ref') == 'recipient' for x in c['steps']))
                self.assertTrue(self.receipt(c, 'recipient', 'plus1_counters', 0))
            c = self.case(mode='recipient_enemy', player=p)
            self.assertTrue(any(x['ref'] == 'recipient' and x['player'] == 1-p for x in c['setup']))
            self.assertTrue(self.has(c, op='choose_target', player=p, ref='recipient'))
            self.assertTrue(self.receipt(c, 'recipient', 'plus1_counters', 1))

    def test_counterspell_uses_actual_announced_stack_context_without_unoffered_target(self):
        for name in self.data['sources']:
            for p in (0, 1):
                c = self.case(name, 'countered', p)
                self.assertFalse(any(s['op'] == 'choose_target' for s in c['steps']))
                ops = [s['op'] for s in c['steps']]
                self.assertLess(ops.index('cast'), ops.index('announce_spell'))
                self.assertLess(ops.index('announce_spell'), ops.index('cast_response'))
                self.assertLess(ops.index('cast_response'), ops.index('resolve_response'))
                self.assertTrue(self.zone(c, 'Counterspell', 1-p, 'graveyard', 1))
                self.assertTrue(self.zone(c, name, p, 'graveyard', 1))
                self.assertTrue(self.receipt(c, 'recipient', 'plus1_counters', 0))

    def test_shroud_comes_from_real_aura_and_uses_no_illegal_target(self):
        for p in (0, 1):
            c = self.case(mode='recipient_shrouded', player=p)
            self.assertTrue(any(x['name'] == 'Robe of Mirrors' for x in c['setup']))
            self.assertTrue(self.has(c, op='cast', ref='shroud_aura', value=1))
            murder = next(i for i, s in enumerate(c['steps']) if s['op'] == 'cast' and s.get('ref') == 'death_spell')
            self.assertFalse(any(s['op'] == 'choose_target' and s.get('ref') == 'recipient' for s in c['steps'][murder:]))
            self.assertTrue(self.receipt(c, 'recipient', 'plus1_counters', 0))

    def test_lki_counts_real_counters_not_initial_amount_or_modified_power(self):
        for p in (0, 1):
            c = self.case(mode='source_added_counter', player=p)
            self.assertTrue(self.has(c, op='cast', ref='growth', value=1)); self.assertTrue(self.receipt(c, 'recipient', 'plus1_counters', 2))
            c = self.case(mode='recipient_added_counter', player=p)
            self.assertTrue(self.receipt(c, 'recipient', 'plus1_counters', 2))
            c = self.case(mode='minus_counter', player=p)
            self.assertTrue(any(x['name'] == 'Fume Spitter' for x in c['setup']))
            self.assertTrue(self.has(c, op='activate', ref='spitter', value=1))
            self.assertTrue(self.receipt(c, 'recipient', 'plus1_counters', 1))
            self.assertTrue(self.zone(c, 'Fume Spitter', p, 'graveyard', 1))
            c = self.case(mode='zero_toughness', player=p)
            self.assertTrue(any(x['name'] == 'Disfigure' for x in c['setup']))
            self.assertTrue(self.receipt(c, 'recipient', 'plus1_counters', 1))

    def test_sunburst_spent_colors_and_real_generic_payment(self):
        for mode, count, clicks in [('transfer', 1, 6), ('sunburst_two', 2, 6), ('sunburst_five', 5, 5)]:
            c = self.case('Arcbound Wanderer', mode)
            self.assertEqual(sum(x['op'] == 'choose_mana' for x in c['steps']), clicks)
            self.assertTrue(self.receipt(c, 'source', 'plus1_counters', count))
            self.assertTrue(self.receipt(c, 'recipient', 'plus1_counters', count))
        c = self.case('Arcbound Wanderer', 'sunburst_zero')
        self.assertFalse(any(x['op'] == 'bind_permanent' for x in c['steps']))
        self.assertFalse(self.receipt(c, 'source', 'in_play', 1))
        self.assertTrue(self.zone(c, 'Arcbound Wanderer', 0, 'graveyard', 1))
        self.assertTrue(self.receipt(c, 'recipient', 'plus1_counters', 0))
        for n in ('Arcbound Worker', 'Arcbound Hybrid', 'Arcbound Stinger'):
            c = self.case(n, 'generic_colored')
            self.assertEqual(sum(x['op'] == 'choose_mana' for x in c['steps']), int(self.oracle[n]['cmc']))

    def test_real_source_ownership_theft_and_death_destinations(self):
        for n in self.data['sources']:
            for p in (0, 1):
                c = self.case(n, 'source_foreign_owner', p)
                self.assertTrue(any(x['ref'] == 'source' and x['player'] == 1-p and x['zone'] == 'hand' for x in c['setup']))
                self.assertTrue(self.has(c, op='cast', ref='control', value=1))
                self.assertTrue(self.has(c, op='bind_permanent', ref='controlled_source', player=p, value=self.rows[n]['csvid']))
                self.assertTrue(self.zone(c, n, 1-p, 'graveyard', 1)); self.assertTrue(self.zone(c, n, p, 'graveyard', 0))
                self.assertTrue(self.zone(c, 'Control Magic', p, 'graveyard', 1))

    def test_real_bounce_recast_is_new_object_with_fresh_entry_counters(self):
        for n in self.data['sources']:
            c = self.case(n, 'recast_bounced')
            self.assertTrue(self.has(c, op='cast', ref='growth', value=1))
            self.assertTrue(self.has(c, op='cast', ref='bounce', value=1))
            self.assertTrue(self.has(c, op='bind_hand', ref='recast_source', value=self.rows[n]['csvid']))
            self.assertTrue(self.has(c, op='cast', ref='recast_source', value=1))
            amount = 1 if family.oracle_model(self.oracle[n])['sunburst'] else family.oracle_model(self.oracle[n])['initial_counters']
            self.assertTrue(self.receipt(c, 'recast_source', 'plus1_counters', amount))
            self.assertTrue(self.receipt(c, 'recipient', 'plus1_counters', amount))
            self.assertTrue(self.zone(c, n, 0, 'graveyard', 1))

    def test_haste_flying_reach_use_actual_combat_and_real_turn_gate(self):
        c = self.case('Arcbound Hybrid', 'fresh_attack', 1)
        self.assertTrue(self.has(c, op='attack', ref='source', value=1)); self.assertTrue(self.has(c, op='assert_life', player=0, value=18))
        self.assertFalse(any(s['op'] == 'advance_turn' for s in c['steps']))
        c = self.case('Arcbound Stinger', 'flying_unblocked')
        self.assertTrue(self.has(c, op='block', ref='blocker', other='source', value=0))
        self.assertTrue(self.has(c, op='assert_life', player=1, value=19))
        c = self.case('Arcbound Stinger', 'combat_death'); s = c['steps']
        self.assertTrue(any(x['ref'] == 'blocker' and x['name'] == 'Giant Spider' for x in c['setup']))
        self.assertTrue(self.has(c, op='block', ref='blocker', other='source', value=1))
        untap = next(i for i, x in enumerate(s) if x['op'] == 'untap_step')
        attack = next(i for i, x in enumerate(s) if x['op'] == 'begin_attackers')
        self.assertTrue(any(x['op'] == 'turn' and x.get('player') == 0 and x.get('value') == 0 for x in s[untap+1:attack]))
        self.assertEqual(sum(x['op'] == 'advance_turn' for x in s[:untap]), 2)

    def test_sacrifice_source_is_only_controlled_creature_for_ai(self):
        for p in (0, 1):
            c = self.case(mode='sacrifice', player=p)
            self.assertTrue(self.has(c, op='activate', ref='altar', value=1))
            self.assertTrue(self.has(c, op='assert_mana', player=p, color=0, value=2))
            self.assertTrue(any(x['ref'] == 'recipient' and x['player'] == 1-p for x in c['setup']))
            self.assertEqual(any(s['op'] == 'choose_target' and s.get('ref') == 'source' for s in c['steps']), p == 0)

    def test_optional_decline_uses_real_cancel_without_forcing_optional_transfer(self):
        c = self.case(mode='decline')
        self.assertFalse(any(s['op'].startswith('choose_trigger') for s in c['steps']))
        self.assertTrue(self.has(c, op='choose_optional_target', ref='recipient', player=0))
        self.assertTrue(self.has(c, op='assert_forced_target_choices', value=0))
        self.assertTrue(self.receipt(c, 'recipient', 'plus1_counters', 0))

    def test_unsupported_announcement_policy_is_rejected_and_not_enrolled(self):
        for name in self.data['sources']:
            d = self.declaration(name, ['transfer_0']); model = family.spec(d, self.oracle[name])
            self.assertNotIn('target_policy_0', family.variants(model))
            with self.assertRaises(ValueError): self.case(name, 'target_policy')

    def test_mandatory_dead_source_callback_queues_only_actual_targets(self):
        for name in self.data['sources']:
            for p in (0, 1):
                c = self.case(name, 'transfer', p)
                self.assertFalse(any(s['op'].startswith('choose_trigger') for s in c['steps']))
                self.assertTrue(self.has(c, op='choose_target', ref='recipient', player=p))
                for mode in ('nonartifact_recipient', 'noncreature_recipient', 'recipient_shrouded'):
                    c = self.case(name, mode, p)
                    self.assertFalse(any(s['op'].startswith('choose_trigger') for s in c['steps']))
                    cast = next(i for i, s in enumerate(c['steps']) if s['op'] == 'cast' and s.get('ref') == 'death_spell')
                    self.assertFalse(any(s['op'].startswith('choose_') for s in c['steps'][cast+1:]))

    def test_decline_intent_precedes_real_resolution_without_changing_no_transfer_receipts(self):
        for name in self.data['sources']:
            c = self.case(name, 'decline'); s = c['steps']
            intent = next(i for i, x in enumerate(s) if x['op'] == 'choose_optional_target')
            self.assertEqual(s[intent+1]['op'], 'resolve')
            self.assertEqual(s[intent-1], dict(op='assert_stack', value=1))
            self.assertTrue(self.receipt(c, 'recipient', 'power', 1))
            self.assertTrue(self.receipt(c, 'recipient', 'toughness', 1))
            self.assertTrue(self.receipt(c, 'recipient', 'plus1_counters', 0))
            self.assertTrue(self.has(c, op='assert_forced_target_choices', value=0))

    def test_replay_captures_actual_cast_pending_and_postdeath_states(self):
        for mode in ('ready_snapshot', 'pending_snapshot', 'death_snapshot', 'resolved_snapshot'):
            c = self.case(mode=mode); s = c['steps']
            for op in ('capture', 'restore', 'record_state', 'ai_roundtrip', 'assert_recorded_state'):
                self.assertTrue(self.has(c, op=op))
            capture = next(i for i, x in enumerate(s) if x['op'] == 'capture')
            if mode == 'pending_snapshot':
                self.assertEqual(s[capture-1], dict(op='assert_stack', value=1))
            if mode == 'death_snapshot':
                self.assertTrue(any(x['op'] == 'resolve' for x in s[:capture]))
                self.assertTrue(any(x['op'] == 'cleanup' for x in s[capture:]))


if __name__ == '__main__':
    unittest.main()
