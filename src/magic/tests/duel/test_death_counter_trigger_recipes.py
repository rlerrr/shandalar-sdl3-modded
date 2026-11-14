import copy
import json
from pathlib import Path
import unittest

from generate_cases import registry
from recipes import expand as public_expand
import death_counter_trigger_recipes as family


class DeathCounterTriggerTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.oracle = json.loads((Path(__file__).parent/'data/oracle.json').read_text(encoding='utf8'))['cards']
        cls.rows = {r['name']: r for r in registry()}
        cls.pins = family.pins()

    def declaration(self, name='Unruly Mob', variants=None):
        return dict(family=family.FAMILY, version=2, params=dict(schema=8, **family.oracle_model(self.oracle[name])),
                    oracle_text=self.oracle[name]['oracle_text'], variants=variants or ['own_death_0'])

    def case(self, name='Unruly Mob', variant='own_death_0'):
        return public_expand(name, self.rows[name]['csvid'], self.declaration(name, [variant]),
                             self.oracle[name], self.rows, self.oracle)[0]

    def has(self, steps, **fields):
        return any(all(s.get(k) == v for k, v in fields.items()) for s in steps)

    def count(self, steps, name, player, zone, value):
        return self.has(steps, op='assert_zone_multiplicity', player=player, zone=zone,
                        index=self.rows[name]['csvid'], value=value)

    def test_whole_oracle_and_typed_counter_parameters_fail_closed(self):
        for name in self.pins['sources']:
            d = self.declaration(name)
            family.spec(d, self.oracle[name])
            for key, value in [('optional', 1), ('counters', True), ('schema', 7)]:
                bad = copy.deepcopy(d); bad['params'][key] = value
                with self.assertRaises(ValueError):
                    family.spec(bad, self.oracle[name])
            record = copy.deepcopy(self.oracle[name]); record['oracle_text'] += '\n{T}: Draw a card.'
            with self.assertRaises(ValueError):
                family.oracle_model(record)
            d['variants'] = ['invented_0']
            with self.assertRaises(ValueError):
                family.spec(d, self.oracle[name])

    def test_complete_witnesses_and_implemented_handler_gates(self):
        family.validate_witnesses(self.oracle, self.rows)
        for name, key, value in [('Boggart Shenanigans', 'type_line', 'Creature'),
                                 ('Naturalize', 'mana_cost', '{G}'), ('Virulent Wound', 'oracle_text', 'Draw a card.')]:
            oracle = copy.deepcopy(self.oracle); oracle[name][key] = value
            with self.assertRaises(ValueError):
                family.validate_witnesses(oracle, self.rows)
        rows = copy.deepcopy(self.rows); rows['Unruly Mob']['handler'] = 'card_dummy'
        with self.assertRaises(ValueError):
            family.validate_witnesses(self.oracle, rows)

    def test_kindred_goblin_is_a_permanent_predicate_not_creature(self):
        model = family.oracle_model(self.oracle['Knucklebone Witch'])
        self.assertEqual(model['predicate']['kind'], 'Permanent')
        for owner in (0, 1):
            c = self.case('Knucklebone Witch', f'kindred_goblin_{owner}'); s = c['steps']
            self.assertEqual(next(x['name'] for x in c['setup'] if x['ref'] == 'victim'), 'Boggart Shenanigans')
            self.assertEqual(next(x['name'] for x in c['setup'] if x['ref'] == 'removal'), 'Naturalize')
            self.assertTrue(self.has(s, op='cast', ref='removal', value=1))
            self.assertFalse(any(a['op'] == 'destroy' for a in s))
            self.assertTrue(self.has(s, op='assert_card', ref='source', property='plus1_counters', value=1))
            self.assertTrue(self.count(s, 'Boggart Shenanigans', owner, 'graveyard', 1))
            self.assertTrue(all(self.has(s, op='assert_life', player=p, value=20) for p in (0, 1)))
            s = self.case('Knucklebone Witch', f'opponent_kindred_{owner}')['steps']
            self.assertTrue(self.has(s, op='assert_card', ref='source', property='plus1_counters', value=0))
            self.assertFalse(self.has(s, op='choose_trigger_source'))

    def test_controller_is_independent_of_graveyard_owner(self):
        for name, mode, count in [('Unruly Mob', 'stolen_death', 1), ('Unruly Mob', 'lent_death', 0),
                                  ('Malakir Cullblade', 'lent_death', 1), ('Malakir Cullblade', 'stolen_death', 0)]:
            for owner in (0, 1):
                c = self.case(name, f'{mode}_{owner}'); s = c['steps']
                victim = next(x for x in c['setup'] if x['ref'] == 'victim')
                self.assertIn('opponent_owned', victim['state'])
                self.assertTrue(self.has(s, op='assert_card', ref='source', property='plus1_counters', value=count))
                self.assertTrue(self.count(s, 'Grizzly Bears', 1-victim['player'], 'graveyard', 2))

    def test_moriok_opponent_artifact_keeps_oracle_receipt_despite_handler_filter(self):
        for owner in (0, 1):
            c = self.case('Moriok Rigger', f'opponent_noncreature_artifact_{owner}'); s = c['steps']
            self.assertEqual(next(x['name'] for x in c['setup'] if x['ref'] == 'victim'), 'Sol Ring')
            self.assertTrue(self.has(s, op='cast', ref='removal', value=1))
            self.assertTrue(self.has(s, op='assert_card', ref='source', property='plus1_counters', value=1))
            self.assertTrue(self.has(s, op='assert_card', ref='source', property='power', value=3))
            self.assertTrue(self.has(s, op='assert_card', ref='source', property='toughness', value=3))
            self.assertEqual(self.has(s, op='choose_trigger_source', player=owner, ref='source'), owner == 0)

    def test_optional_receipt_and_unused_observer_do_not_invent_a_menu(self):
        for name in ('Knucklebone Witch', 'Moriok Rigger'):
            s = self.case(name, 'own_death_0')['steps']
            select = next(i for i, a in enumerate(s) if a['op'] == 'choose_trigger_source')
            death = next(i for i, a in enumerate(s) if a['op'] == 'destroy')
            count = next(i for i in range(death, len(s)) if s[i].get('property') == 'plus1_counters')
            observe = next(i for i in range(death, len(s)) if s[i]['op'] == 'assert_unused_choices')
            self.assertLess(select, death); self.assertLess(count, observe)
            self.assertEqual(s[observe], dict(op='assert_unused_choices', value=0))
            self.assertFalse(any(a['op'] in ('choose_dialog', 'choose_player', 'choose_trigger') for a in s))
            s = self.case(name, 'decline_0')['steps']
            self.assertFalse(self.has(s, op='choose_trigger_source'))
            self.assertTrue(self.has(s, op='assert_card', ref='source', property='plus1_counters', value=0))

    def test_human_partial_accept_is_one_of_two_real_simultaneous_deaths(self):
        for name, victim in [('Knucklebone Witch', 'Raging Goblin'), ('Moriok Rigger', 'Ornithopter')]:
            c = self.case(name, 'partial_accept_0'); s = c['steps']
            self.assertEqual([x['name'] for x in c['setup'] if x['ref'] in ('victim', 'second_victim')], [victim, victim])
            self.assertNotIn('Boggart Shenanigans', [x['name'] for x in c['setup']])
            self.assertEqual(sum(a['op'] == 'choose_trigger_source' for a in s), 1)
            self.assertFalse(any(a['op'] == 'choose_dialog' for a in s))
            self.assertFalse(any(a['op'] == 'destroy' for a in s))
            self.assertTrue(self.has(s, op='cast', ref='protection_growth', value=1))
            self.assertEqual(next(x['player'] for x in c['setup'] if x['ref'] == 'sweep'), 0)
            sweep = next(i for i, a in enumerate(s) if a['op'] == 'cast' and a['ref'] == 'sweep')
            self.assertTrue(self.has(s[sweep:], op='assert_card', ref='source', property='plus1_counters', value=1))
            self.assertFalse(self.has(s[sweep:], op='assert_card', ref='source', property='plus1_counters', value=2))
            self.assertTrue(self.has(s[sweep:], op='assert_card', ref='source', property='damage', value=2))
            self.assertTrue(self.count(s, victim, 0, 'graveyard', 2))

    def test_mandatory_simultaneous_deaths_accumulate_two_counters(self):
        for name in ('Unruly Mob', 'Rot Shambler', 'Gavony Unhallowed', 'Malakir Cullblade'):
            for owner in (0, 1):
                s = self.case(name, f'simultaneous_{owner}')['steps']
                self.assertFalse(self.has(s, op='choose_trigger_source'))
                self.assertTrue(self.has(s, op='assert_card', ref='source', property='plus1_counters', value=2))
                self.assertTrue(self.has(s, op='cast', ref='sweep', value=1))

    def test_simultaneous_departed_source_uses_zones_not_stale_instance(self):
        for name in self.pins['sources']:
            for owner in (0, 1):
                c = self.case(name, f'source_simultaneous_{owner}'); s = c['steps']
                self.assertEqual(next(x['player'] for x in c['setup'] if x['ref'] == 'sweep'), owner)
                sweep = next(i for i, a in enumerate(s) if a['op'] == 'cast' and a['ref'] == 'sweep')
                resolve = next(i for i in range(sweep, len(s)) if s[i]['op'] == 'resolve')
                self.assertFalse(any(a['op'] in ('assert_card', 'require_card', 'choose_trigger_source', 'bind_permanent') and a.get('ref') == 'source' for a in s[resolve+1:]))
                self.assertTrue(self.count(s, name, owner, 'graveyard', 1))
                self.assertTrue(self.count(s, name, owner, 'battlefield', 0))
                self.assertNotIn('Boggart Shenanigans', [x['name'] for x in c['setup']])

    def test_source_absent_controls_do_not_inspect_removed_refs(self):
        for name in self.pins['sources']:
            for mode, zone in [('source_removed', 'graveyard'), ('source_bounced', 'hand'), ('source_exiled', 'exile')]:
                s = self.case(name, mode+'_0')['steps']
                removal = next(i for i, a in enumerate(s) if a['op'] == 'cast' and a['ref'] == 'source_removal')
                resolve = next(i for i in range(removal, len(s)) if s[i]['op'] == 'resolve')
                self.assertFalse(any(a['op'] in ('assert_card', 'require_card', 'choose_trigger_source') and a.get('ref') == 'source' for a in s[resolve+1:]))
                self.assertTrue(self.count(s, name, 0, zone, 1))

    def test_counter_cancel_uses_safe_live_spell_not_extra_creature_death(self):
        for name in self.pins['sources']:
            c = self.case(name, 'counter_cancel_0'); s = c['steps']
            self.assertEqual(next(x['name'] for x in c['setup'] if x['ref'] == 'counter_cancel'), 'Virulent Wound')
            self.assertNotIn('Fume Spitter', [x['name'] for x in c['setup']])
            wound = next(i for i, a in enumerate(s) if a['op'] == 'cast' and a['ref'] == 'counter_cancel')
            self.assertTrue(self.has(s[:wound], op='require_card', ref='source', property='plus1_counters', value=1))
            self.assertTrue(self.has(s[wound:], op='assert_card', ref='source', property='plus1_counters', value=0))
            self.assertTrue(self.has(s[wound:], op='assert_card', ref='source', property='minus1_counters', value=0))
            self.assertFalse(any(a['op'] in ('destroy', 'cast') for a in s[wound+1:]))
            self.assertTrue(self.has(s[wound:], op='cleanup'))
            self.assertTrue(all(self.has(s, op='assert_poison', player=p, value=0) for p in (0, 1)))

    def test_blink_rebinds_a_counterless_new_source_before_earning_again(self):
        for name in self.pins['sources']:
            s = self.case(name, 'source_blinked_0')['steps']
            blink = next(i for i, a in enumerate(s) if a['op'] == 'cast' and a['ref'] == 'blink')
            bind = next(i for i, a in enumerate(s) if a['op'] == 'bind_permanent')
            self.assertLess(blink, bind)
            self.assertTrue(self.has(s[bind:], op='assert_card', ref='source_new', property='plus1_counters', value=0))
            self.assertTrue(self.has(s[bind:], op='assert_card', ref='source_new', property='plus1_counters', value=1))
            self.assertFalse(any(a['op'] == 'assert_card' and a.get('ref') == 'source' for a in s[bind:]))

    def test_snapshot_choices_follow_actual_event_boundary(self):
        for name in ('Unruly Mob', 'Knucklebone Witch', 'Moriok Rigger'):
            for mode in ('ready_snapshot', 'pending_snapshot', 'death_snapshot'):
                s = self.case(name, mode+'_0')['steps']
                capture = next(i for i, a in enumerate(s) if a['op'] == 'capture')
                if mode == 'ready_snapshot':
                    self.assertEqual(s[capture+1], dict(op='cast', ref='source', value=1))
                elif mode == 'pending_snapshot':
                    self.assertEqual(s[capture+1], dict(op='resolve'))
                else:
                    self.assertEqual(s[capture+1], dict(op='destroy', ref='victim'))
                self.assertEqual(s[-1], dict(op='assert_recorded_state'))

    def test_full_pilot_variants_expand_through_public_dispatch(self):
        for name in self.pins['sources']:
            d = self.declaration(name)
            d['variants'] = family.variants(family.spec(d, self.oracle[name]))
            cases = public_expand(name, self.rows[name]['csvid'], d, self.oracle[name], self.rows, self.oracle)
            self.assertEqual(len(cases), len(set(d['variants'])))
            self.assertTrue(all(c['_recipe']['oracle_text'] == self.oracle[name]['oracle_text'] for c in cases))


if __name__ == '__main__':
    unittest.main()
