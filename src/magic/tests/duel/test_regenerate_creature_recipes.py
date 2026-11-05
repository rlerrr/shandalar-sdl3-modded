"""Fail-closed Oracle scope and independent regeneration recipe expectations."""
import copy
import json
import unittest
from pathlib import Path
from generate_cases import registry
from recipes import expand, explicit_spec
from regenerate_creature_recipes import TIMING, variants, validate_witnesses


class RegenerateCreatureRecipesTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.oracle = json.loads((Path(__file__).parent/'data/oracle.json').read_text(encoding='utf8'))['cards']
        cls.registry = {r['name']: r for r in registry()}

    def declaration(self, name='Drudge Skeletons', cost='{B}', keys=None, selected=None):
        return dict(family='regenerate_creature', version=2, oracle_text=self.oracle[name]['oracle_text'],
                    params=dict(keywords=keys or [], activation_cost=cost, timing=TIMING), variants=selected or ['destroy'])

    def cases(self, name, d, registry=None):
        return expand(name, self.registry[name]['csvid'], d, self.oracle[name], registry or self.registry, oracle_context=self.oracle)

    def test_explicit_cost_keywords_pin_and_timing_required(self):
        d = self.declaration(); self.cases('Drudge Skeletons', d)
        for change in (dict(oracle_text='changed'), dict(params={}), dict(params=dict(d['params'], activation_cost='{G}')),
                       dict(params=dict(d['params'], timing='modern_shield')), dict(params=dict(d['params'], keywords=['Flying']))):
            with self.assertRaises(ValueError): self.cases('Drudge Skeletons', dict(d, **change))
        for cost in ('{X}', '{S}', '{T}', 'Pay 2 life', '{B}, Sacrifice a creature', '{B/P}', '{0}'):
            with self.assertRaises(ValueError): self.cases('Drudge Skeletons', dict(d, params=dict(d['params'], activation_cost=cost)))

    def test_extra_modes_conditions_and_triggers_are_rejected(self):
        for name in ('Sea Troll', 'Masticore', 'Darkling Stalker', 'Twisted Abomination', 'Spiny Starfish', 'Deepwood Ghoul'):
            with self.assertRaises(ValueError): explicit_spec(self.declaration(name), self.oracle[name])
        token = dict(self.oracle['Drudge Skeletons'], layout='token')
        with self.assertRaises(ValueError): explicit_spec(self.declaration(), token)
        d = self.declaration(); record = dict(self.oracle['Drudge Skeletons'], power='*')
        with self.assertRaises(ValueError): explicit_spec(d, record)

    def test_same_ability_self_reference_and_reviewed_reminder(self):
        d = self.declaration(); r = dict(self.oracle['Drudge Skeletons'])
        r['oracle_text'] = r['oracle_text'].replace('this creature', 'Drudge Skeletons', 1); d['oracle_text'] = r['oracle_text']
        self.assertEqual(explicit_spec(d, r)['activation_mana'], [0, 1, 0, 0, 0, 0])
        r['oracle_text'] += '\nActivate only during your turn.'; d['oracle_text'] = r['oracle_text']
        with self.assertRaises(ValueError): explicit_spec(d, r)

    def test_legacy_gate_does_not_fake_a_modern_shield(self):
        c = self.cases('Drudge Skeletons', self.declaration(selected=['legacy_timing']))[0]
        self.assertIn(dict(op='can_activate', ref='source', value=0), c['steps'])
        self.assertIn(dict(op='activate', ref='source', value=0), c['steps'])
        self.assertEqual(c['_recipe']['legacy_exception'], TIMING)

    def test_legendary_short_self_reference_is_not_a_target(self):
        d=self.declaration('Silvos, Rogue Elemental','{G}',['Trample'])
        self.assertEqual(explicit_spec(d,self.oracle['Silvos, Rogue Elemental'])['activation_mana'],[0,0,0,1,0,0])
        self.assertEqual(len(self.cases('Silvos, Rogue Elemental',d)),1)
        r=dict(self.oracle['Silvos, Rogue Elemental'],oracle_text='Trample\n{G}: Regenerate target creature.')
        with self.assertRaises(ValueError):explicit_spec(dict(d,oracle_text=r['oracle_text']),r)

    def test_destruction_uses_real_priority_and_verifies_all_effects(self):
        c = self.cases('Drudge Skeletons', self.declaration())[0]
        self.assertLess(c['steps'].index(dict(op='choose_regeneration', ref='source')), c['steps'].index(dict(op='destroy', ref='source')))
        for prop, value in (('in_play', 1), ('tapped', 1), ('damage', 0)):
            self.assertIn(dict(op='assert_card', ref='source', property=prop, value=value), c['steps'])
        self.assertIn(dict(op='assert_mana', player=0, color=1, value=9), c['steps'])

    def test_exact_mixed_and_generic_costs_are_independent_of_cast_cost(self):
        c = self.cases('Trestle Troll', self.declaration('Trestle Troll', '{1}{B}{G}', ['Defender', 'Reach'], ['regeneration_payment']))[0]
        for color in range(6): self.assertIn(dict(op='assert_mana', player=0, color=color, value=0), c['steps'])
        c = self.cases('Clay Statue', self.declaration('Clay Statue', '{2}', selected=['regeneration_payment']))[0]
        self.assertIn(dict(op='set_mana', player=0, color=0, value=2), c['steps'])
        self.assertIn(dict(op='assert_mana_total', player=0, value=0), c['steps'])

    def test_repetition_and_spent_budget_do_not_reuse_a_free_shield(self):
        for variant, responses, alive in (('repeat', 2, 1), ('spent_mana', 1, 0), ('no_response', 0, 0), ('insufficient', 0, 0), ('wrong_color', 0, 0)):
            c = self.cases('Drudge Skeletons', self.declaration(selected=[variant]))[0]
            self.assertEqual(sum(s['op'] == 'choose_regeneration' for s in c['steps']), responses)
            self.assertIn(dict(op='assert_card', ref='source', property='in_play', value=alive), c['steps'])
        d = self.declaration(selected=['tapped_sick']); c = self.cases('Drudge Skeletons', d)[0]
        self.assertEqual(c['setup'][0]['state'], ['sick', 'tapped'])

    def test_combat_uses_phases_real_buffs_and_removes_both_roles(self):
        for variant, prop in (('combat_attack', 'attacking'), ('combat_block', 'blocking_count')):
            c = self.cases('Drudge Skeletons', self.declaration(selected=[variant]))[0]
            for op in ('begin_attackers', 'finish_attackers', 'begin_blockers', 'advance_combat', 'combat_damage'):
                self.assertTrue(any(s['op'] == op for s in c['steps']))
            self.assertIn(dict(op='assert_card', ref='source', property=prop, value=0), c['steps'])
            self.assertEqual(sum(s['op'] == 'cast' for s in c['steps']), 2)
        d = self.declaration('Living Wall', '{1}', ['Defender'], ['combat_attack'])
        with self.assertRaises(ValueError): self.cases('Living Wall', d)

    def test_lethal_damage_clears_damage_but_nonlethal_does_not(self):
        d = self.declaration('Living Wall', '{1}', ['Defender'], ['lethal_damage', 'nonlethal_damage', 'heal_existing_damage'])
        lethal, nonlethal, healed = self.cases('Living Wall', d)
        self.assertEqual(sum(s['op'] == 'cast' for s in lethal['steps']), 2)
        self.assertIn(dict(op='assert_card', ref='source', property='damage', value=0), lethal['steps'])
        self.assertFalse(any(s['op'] == 'choose_regeneration' for s in nonlethal['steps']))
        self.assertIn(dict(op='assert_card', ref='source', property='damage', value=2), nonlethal['steps'])
        self.assertIn(dict(op='assert_card', ref='source', property='damage', value=0), healed['steps'])

    def test_destruction_prohibition_and_zero_toughness_need_no_regen_choice(self):
        for variant, spell in (('cannot_regenerate', 'Terminate'), ('zero_toughness', 'Last Gasp')):
            c = self.cases('Drudge Skeletons', self.declaration(selected=[variant]))[0]
            self.assertTrue(any(s['name'] == spell for s in c['setup']))
            self.assertFalse(any(s['op'] == 'choose_regeneration' for s in c['steps']))
            self.assertIn(dict(op='assert_card', ref='source', property='in_play', value=0), c['steps'])

    def test_snapshot_and_populated_zones_survive_ai_backup(self):
        c = self.cases('Drudge Skeletons', self.declaration(selected=['regeneration_snapshot']))[0]
        capture = c['steps'].index(dict(op='capture'))
        self.assertEqual(c['steps'][capture-1], dict(op='choose_regeneration', ref='source'))
        self.assertTrue(any(s['op'] == 'cast' for s in c['steps'][:capture]))
        self.assertEqual(c['steps'][capture+1], dict(op='resolve'))
        self.assertEqual(sum(s['op'] == 'choose_regeneration' for s in c['steps']), 1)
        for op in ('capture', 'record_state', 'ai_roundtrip', 'restore', 'assert_recorded_state'):
            self.assertIn(dict(op=op), c['steps'])
        for player in (0, 1): self.assertIn(dict(op='assert_zone', player=player, zone='library', value=7), c['steps'])

    def test_witness_scope_and_handler_independence(self):
        r = copy.deepcopy(self.registry)
        for row in r.values(): row['handler'] = 'card_dummy'
        d = self.declaration(); self.assertEqual(self.cases('Drudge Skeletons', d), self.cases('Drudge Skeletons', d, r))
        o = copy.deepcopy(self.oracle); o['Air Elemental']['power'] = '8'
        with self.assertRaises(ValueError): validate_witnesses(o, self.registry)
        with self.assertRaises(ValueError): self.cases('Drudge Skeletons', dict(d, variants=['fake']))
        with self.assertRaises(ValueError): self.cases('Drudge Skeletons', dict(d, resolve_dialogs=[0]))


if __name__ == '__main__': unittest.main()
