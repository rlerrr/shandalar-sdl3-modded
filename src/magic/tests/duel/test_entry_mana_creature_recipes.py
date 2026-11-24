"""Checks for the real cast, combat, departure, and mana-spending witnesses."""
import json
import unittest
from pathlib import Path

import entry_mana_creature_recipes as model
from generate_cases import OPS, registry


class EntryManaCreatureTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.oracle = json.loads((Path(__file__).parent / 'data/oracle.json').read_text(encoding='utf8'))['cards']
        cls.rows = {row['name']: row for row in registry()}

    def spec(self, name):
        record = self.oracle[name]
        d = dict(family=model.FAMILY, version=2, oracle_text=record['oracle_text'],
                 params=dict(schema=1, mechanism=model.mechanism(record)))
        return d, model.spec(d, record)

    def case(self, name, mode, owner=0):
        d, p = self.spec(name)
        return model.expand(name, p['csvid'], d, p, [f'{mode}_{owner}'], self.rows, self.oracle)[0]

    def last_mana(self, steps, owner):
        return [s['value'] for s in steps if s['op']=='assert_mana' and s['player']==owner][-6:]

    def test_whole_pins_preserve_gate_fee_and_produced_colors(self):
        expected = {'Burning-Tree Emissary': ('entry', [0,0,0,0,2,0], [0,0,0,1,1,0]),
                    'Coal Stoker': ('cast_hand', [3,0,0,0,1,0], [0,0,0,0,3,0]),
                    'Mardu Warshrieker': ('raid', [3,0,0,0,1,0], [0,1,0,0,1,1]),
                    'Priest of Gix': ('entry', [2,1,0,0,0,0], [0,3,0,0,0,0]),
                    'Priest of Urabrask': ('entry', [2,0,0,0,1,0], [0,0,0,0,3,0]),
                    'Hidden Herbalists': ('revolt', [1,0,0,1,0,0], [0,0,0,2,0,0])}
        self.assertEqual(set(model.pins()['sources']), set(expected))
        for name, values in expected.items():
            _, p = self.spec(name)
            self.assertEqual(tuple(p[k] for k in ('gate', 'mana', 'produced')), values)
            with self.assertRaises(ValueError):
                model.mechanism(dict(self.oracle[name], oracle_text=self.oracle[name]['oracle_text']+'\nDraw a card.'))
        model.validate_witnesses(self.oracle, self.rows)

    def test_real_hybrid_alternatives_pay_for_the_same_printed_output(self):
        name = 'Burning-Tree Emissary'
        for owner in (0, 1):
            for mode, payment in [('cast',[0,0,0,0,2,0]), ('pay_green',[0,0,0,2,0,0]), ('pay_mixed',[0,0,0,1,1,0])]:
                steps = self.case(name, mode, owner)['steps']
                casting = steps.index(dict(op='cast', ref='source', value=1))
                self.assertEqual([s['value'] for s in steps[:casting] if s['op']=='set_mana' and s['player']==owner][-6:], payment)
                self.assertEqual(self.last_mana(steps, owner), [0,0,0,1,1,0])

    def test_cast_only_condition_is_distinct_from_actual_blink_and_reanimation(self):
        for owner in (0, 1):
            for name, reanimate, blink in [('Coal Stoker', 0, 3), ('Priest of Urabrask', 3, 6)]:
                alternate = self.case(name, 'reanimate', owner)
                self.assertEqual(alternate['setup'][0]['zone'], 'graveyard')
                self.assertIn(dict(op='choose_zone_card', player=owner, value=self.rows[name]['csvid']), alternate['steps'])
                self.assertNotIn(dict(op='cast', ref='source', value=1), alternate['steps'])
                self.assertEqual(self.last_mana(alternate['steps'], owner)[4], reanimate)
                returning = self.case(name, 'blink', owner)['steps']
                self.assertLess(returning.index(dict(op='cast',ref='source',value=1)), returning.index(dict(op='cast',ref='blink',value=1)))
                self.assertEqual(self.last_mana(returning,owner)[4], blink)

    def test_raid_counts_actual_declaration_without_needing_damage(self):
        for owner in (0, 1):
            for mode, has_mana in [('attack_no_damage', True), ('removed_attacker', True), ('damage_only', False),
                                   ('opponent_attack', False), ('prior_turn', False)]:
                steps = self.case('Mardu Warshrieker', mode, owner)['steps']
                self.assertEqual(self.last_mana(steps, owner), [0,1,0,0,1,1] if has_mana else [0]*6)
                ops = [s['op'] for s in steps]
                if mode == 'damage_only':
                    self.assertIn(dict(op='cast',ref='shock',value=1), steps)
                    self.assertNotIn('attack',ops)
                else:
                    combat = [op for op in ops if op in ('attack', 'finish_attackers', 'begin_blockers', 'advance_combat', 'combat_damage','end_combat')]
                    self.assertEqual(combat,['attack','finish_attackers','begin_blockers','advance_combat','combat_damage','end_combat'])
                if mode == 'removed_attacker':
                    self.assertLess(steps.index(dict(op='cast',ref='remove',value=1)), ops.index('begin_blockers'))
                if mode in ('attack_no_damage','removed_attacker'):
                    self.assertEqual({s['value'] for s in steps if s['op']=='assert_life' and s['player']==1-owner},{20})

    def test_revolt_counts_controlled_departures_and_true_owner_destination(self):
        for owner in (0, 1):
            for mode, enabled in [('opponent_bounce',False),('stolen_bounce',True),('own_death',True),
                                  ('own_exile',True),('prior_turn',False),('blink',True)]:
                case = self.case('Hidden Herbalists',mode,owner)
                self.assertEqual(self.last_mana(case['steps'],owner)[3], 2*enabled)
                if mode == 'stolen_bounce':
                    self.assertEqual([(o['player'],o.get('state')) for o in case['setup'] if o['ref']=='witness'],
                                     [(owner,['untapped','opponent_owned'])])
                    self.assertIn(dict(op='assert_zone_multiplicity',player=1-owner,zone='hand',index=self.rows['Grizzly Bears']['csvid'],value=1),case['steps'])

    def test_output_alone_can_pay_for_other_real_spells(self):
        for owner in (0, 1):
            for name in model.pins()['sources']:
                steps = self.case(name,'spend',owner)['steps']
                resolved = steps.index(dict(op='resolve'),steps.index(dict(op='cast',ref='source',value=1)))
                spend = steps[resolved+1:]
                self.assertTrue(any(s['op']=='cast' and s.get('ref','').startswith('spend_') for s in spend))
                self.assertFalse(any(s['op']=='set_mana' for s in spend))
                self.assertEqual(self.last_mana(spend,owner),[0]*6)

    def test_raw_hand_observer_includes_the_pending_instance_only_until_resolution(self):
        for owner in (0, 1):
            for name, mode, ref, visible in [('Priest of Gix', 'spend', 'source', 'Priest of Gix'),
                                             ('Priest of Urabrask', 'reanimate', 'reanimate', 'Zombify')]:
                steps = self.case(name,mode,owner)['steps']
                cast = steps.index(dict(op='cast',ref=ref,value=1))
                resolve = steps.index(dict(op='resolve'),cast)
                observation = dict(op='assert_zone_multiplicity',player=owner,zone='hand',index=self.rows[visible]['csvid'])
                self.assertIn(dict(observation,value=1),steps[cast:resolve])
                self.assertIn(dict(observation,value=0),steps[resolve:])

    def test_all_variants_keep_whole_clause_and_identical_replay(self):
        count = 0
        for name in model.pins()['sources']:
            d,p = self.spec(name)
            for case in model.expand(name,p['csvid'],d,p,model.variants(p),self.rows,self.oracle):
                count += 1
                self.assertEqual(case['_recipe']['covered_clauses'],[self.oracle[name]['oracle_text']])
                steps = case['steps']
                ops = [s['op'] for s in steps]
                self.assertFalse(set(ops)-OPS)
                if 'replay' in case['name']:
                    self.assertEqual(steps[ops.index('capture')+1:ops.index('record_state')],steps[ops.index('restore')+1:-1])
        self.assertEqual(count,160)
