"""Entry hand-flow grammar, recipient, optional-input and replay invariants."""
import copy
import json
from pathlib import Path
import unittest

from generate_cases import registry, OPS, PROPERTIES
import entry_trigger_value_handflow_recipes as handflow
import entry_trigger_value_recipes as dispatch


class EntryHandFlowTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.oracle = json.loads((Path(__file__).parent / 'data/oracle.json').read_text(encoding='utf8'))['cards']
        cls.rows = {r['name']: r for r in registry()}

    def declaration(self, name):
        profile = handflow.pins()['sources'][name]
        return dict(family=handflow.FAMILY, version=2, oracle_text=self.oracle[name]['oracle_text'],
                    params=dict(schema=2, mechanism=copy.deepcopy(profile['mechanism']), presentation=copy.deepcopy(profile['presentation'])),
                    variants=['effect_0'])

    def cases(self, name, selected=None):
        d = self.declaration(name); p = dispatch.spec(d, self.oracle[name]); selected = selected or dispatch.variants(p)
        d['variants'] = selected
        return dispatch.expand(name, self.rows[name]['csvid'], d, p, selected, self.rows, self.oracle)

    def test_sources_expand_both_controllers_with_supported_operations(self):
        for name in handflow.pins()['sources']:
            cases = self.cases(name)
            self.assertEqual({int(c['_recipe']['variant'][-1]) for c in cases}, {0, 1})
            self.assertEqual(len(cases), len({c['name'] for c in cases}))
            for c in cases:
                self.assertEqual(c['_recipe']['oracle_text'], self.oracle[name]['oracle_text'])
                for clause in handflow.pins()['sources'][name]['mechanism']['gaps']:
                    self.assertIn(clause, c['_recipe']['untested_components'])
                for s in c['steps']:
                    self.assertIn(s['op'], OPS)
                    if 'property' in s: self.assertIn(s['property'], PROPERTIES)

    def test_whole_oracle_and_presentation_are_pinned_separately(self):
        for name in handflow.pins()['sources']:
            d = self.declaration(name)
            for key, value in [('oracle_text', self.oracle[name]['oracle_text'] + '\nGain 1 life.'), ('mana_cost', '{99}'), ('layout', 'split')]:
                record = dict(self.oracle[name], **{key: value})
                with self.assertRaises(ValueError): dispatch.spec(dict(d, oracle_text=record['oracle_text']), record)
            changed = copy.deepcopy(d); changed['params']['mechanism']['discard'] += 1
            with self.assertRaises(ValueError): dispatch.spec(changed, self.oracle[name])
            changed = copy.deepcopy(d); changed['params']['presentation']['human_optional_trigger'] = not changed['params']['presentation']['human_optional_trigger']
            with self.assertRaises(ValueError): dispatch.spec(changed, self.oracle[name])

    def test_mechanism_grammar_is_independent_of_named_sources(self):
        r = dict(name='Independent Source', layout='normal', type_line='Enchantment', mana_cost='{2}{U}', oracle_text='When this enchantment enters, draw four cards, then discard a card.')
        p = handflow.mechanism(r)
        self.assertEqual((p['kind'], p['draw'], p['discard']), ('draw_discard', 4, 1))
        r['oracle_text'] += '\nWhen this enchantment enters, draw a card.'
        with self.assertRaises(ValueError): handflow.mechanism(r)

    def test_enchantments_remain_noncreatures_with_no_creature_only_blink(self):
        names = [name for name in handflow.pins()['sources'] if 'Enchantment' in self.oracle[name]['type_line']]
        self.assertEqual(set(names), {'Oath of Jace', 'Trial of Knowledge'})
        for name in names:
            d = self.declaration(name); p = dispatch.spec(d, self.oracle[name])
            self.assertFalse(p['creature'])
            self.assertEqual(p['legendary'], 'Legendary' in self.oracle[name]['type_line'])
            self.assertFalse(any(v.startswith('blink_source_') for v in dispatch.variants(p)))
            c = self.cases(name, ['effect_0'])[0]
            self.assertEqual(c['_recipe']['oracle_text'], self.oracle[name]['oracle_text'])
            self.assertFalse(any(s.get('ref') == 'source' and s.get('property') in ('power', 'toughness') for s in c['steps']))

    def test_draw_then_discard_occurs_even_from_an_empty_starting_hand(self):
        for owner in (0, 1):
            c = self.cases('Oath of Jace', [f'own_empty_{owner}'])[0]; s = c['steps']
            cut = next(i for i, x in enumerate(s) if x['op'] == 'resolve')
            library = sum(x['player'] == owner and x['zone'] == 'library' for x in c['setup'])
            self.assertIn(dict(op='assert_zone', player=owner, zone='library', value=library-3), s[cut:])
            self.assertIn(dict(op='assert_zone', player=owner, zone='hand', value=1), s[cut:])

    def test_loot_needs_a_discard_before_it_can_draw(self):
        for owner in (0, 1):
            c = self.cases('Viashino Racketeer', [f'own_empty_{owner}'])[0]; s = c['steps']
            cut = next(i for i, x in enumerate(s) if x['op'] == 'resolve')
            library = sum(x['player'] == owner and x['zone'] == 'library' for x in c['setup'])
            self.assertIn(dict(op='assert_zone', player=owner, zone='library', value=library), s[cut:])
            self.assertIn(dict(op='assert_zone', player=owner, zone='hand', value=0), s[cut:])

    def test_optional_trigger_and_decline_follow_the_single_ordered_queue(self):
        positive = self.cases('Viashino Racketeer', ['effect_0'])[0]['steps']
        cast = next(i for i, x in enumerate(positive) if x['op'] == 'cast')
        trigger = next(i for i, x in enumerate(positive) if x['op'] == 'choose_trigger')
        resolve = next(i for i, x in enumerate(positive) if x['op'] == 'resolve')
        self.assertLess(cast, trigger); self.assertLess(trigger, resolve)
        for owner in (0, 1):
            s = self.cases('Viashino Racketeer', [f'decline_{owner}'])[0]['steps']
            self.assertIn(dict(op='discard_policy', value=0), s)
            decline = next(i for i, x in enumerate(s) if x['op'] == 'choose_optional_zone')
            resolve = next(i for i, x in enumerate(s) if x['op'] == 'resolve')
            self.assertLess(decline, resolve)
            self.assertIn(dict(op='assert_forced_zone_choices', value=0), s[resolve:])
            if owner == 0:
                trigger = next(i for i, x in enumerate(s) if x['op'] == 'choose_trigger')
                self.assertLess(trigger, decline)

    def test_optional_opponent_discard_never_changes_the_sources_hand(self):
        for owner in (0, 1):
            c = self.cases('Brutal Nightstalker', [f'effect_{owner}'])[0]; s = c['steps']
            own = sum(x['player'] == owner and x['zone'] == 'hand' for x in c['setup'])-1
            enemy = sum(x['player'] == 1-owner and x['zone'] == 'hand' for x in c['setup'])-1
            cut = next(i for i, x in enumerate(s) if x['op'] == 'resolve')
            self.assertIn(dict(op='assert_zone', player=owner, zone='hand', value=own), s[cut:])
            self.assertIn(dict(op='assert_zone', player=1-owner, zone='hand', value=enemy), s[cut:])
            self.assertFalse(any(x['op'] == 'choose_player' for x in s))

    def test_pending_spells_have_left_the_observable_hand(self):
        for name in handflow.pins()['sources']:
            for owner in (0, 1):
                s = self.cases(name, [f'effect_{owner}'])[0]['steps']
                cast = next(i for i, x in enumerate(s) if x['op'] == 'cast')
                self.assertEqual(s[cast+1], dict(op='assert_card', ref='source', property='in_hand', value=0))

    def test_printed_cast_costs_are_paid_by_only_the_source_controller(self):
        costs = {'Viashino Racketeer': [2, 0, 0, 0, 1, 0], 'Pale Rider of Trostad': [1, 1, 0, 0, 0, 0],
                 'Oath of Jace': [2, 0, 1, 0, 0, 0], 'Brutal Nightstalker': [3, 2, 0, 0, 0, 0]}
        for name, cost in costs.items():
            for owner in (0, 1):
                s = self.cases(name, [f'payment_{owner}'])[0]['steps']
                cast = next(i for i, x in enumerate(s) if x['op'] == 'cast')
                for color, amount in enumerate(cost):
                    self.assertIn(dict(op='set_mana', player=owner, color=color, value=amount), s[:cast])
                    self.assertIn(dict(op='assert_mana', player=owner, color=color, value=0), s[cast:])
                    self.assertIn(dict(op='assert_mana', player=1-owner, color=color, value=20), s[cast:])

    def test_mandatory_self_discard_has_no_optional_inputs_or_opponent_effect(self):
        for owner in (0, 1):
            c = self.cases('Pale Rider of Trostad', [f'effect_{owner}'])[0]; s = c['steps']
            own = sum(x['player'] == owner and x['zone'] == 'hand' for x in c['setup'])-2
            enemy = sum(x['player'] == 1-owner and x['zone'] == 'hand' for x in c['setup'])
            cut = next(i for i, x in enumerate(s) if x['op'] == 'resolve')
            self.assertIn(dict(op='assert_zone', player=owner, zone='hand', value=own), s[cut:])
            self.assertIn(dict(op='assert_zone', player=1-owner, zone='hand', value=enemy), s[cut:])
            self.assertFalse(any(x['op'] in ('choose_trigger', 'choose_optional_zone') for x in s))

    def test_optional_intent_replays_at_each_snapshot_boundary(self):
        for name in handflow.pins()['sources']:
            for owner in (0, 1):
                for variant in ('ready_snapshot', 'pending_snapshot', 'resolved_snapshot'):
                    s = self.cases(name, [f'{variant}_{owner}'])[0]['steps']; ops = [x['op'] for x in s]
                    a = ops.index('capture'); b = ops.index('record_state'); r = ops.index('restore')
                    self.assertEqual(s[a+1:b], s[r+1:-1])
                    self.assertIsNot(s[a+1], s[r+1])

    def test_source_and_witness_implementation_guards(self):
        handflow.validate_witnesses(self.oracle, self.rows)
        rows = copy.deepcopy(self.rows); rows['Oath of Jace']['handler'] = 'card_dummy'
        with self.assertRaises(ValueError): handflow.validate_witnesses(self.oracle, rows)
        oracle = copy.deepcopy(self.oracle); oracle['Temporal Adept']['mana_cost'] = '{99}'
        with self.assertRaises(ValueError): handflow.validate_witnesses(oracle, self.rows)


if __name__ == '__main__': unittest.main()
