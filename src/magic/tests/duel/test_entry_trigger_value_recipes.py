"""Independent Oracle grammar and entry exchange fixture invariants."""
import copy
import json
from pathlib import Path
import unittest

from generate_cases import registry, OPS, PROPERTIES
from entry_trigger_value_recipes import FAMILY, load_profiles, mechanism, spec, variants, expand, validate_witnesses


class EntryTriggerValueTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.oracle = json.loads((Path(__file__).parent / 'data/oracle.json').read_text(encoding='utf8'))['cards']
        cls.rows = {r['name']: r for r in registry()}

    def declaration(self, name):
        params = dict(schema=1, mechanism=mechanism(self.oracle[name]))
        profile = load_profiles()['sources'][name]
        if profile.get('presentation'): params['presentation'] = copy.deepcopy(profile['presentation'])
        return dict(family=FAMILY, version=2, oracle_text=self.oracle[name]['oracle_text'], params=params, variants=['effect_0'])

    def cases(self, name, selected=None):
        d = self.declaration(name); p = spec(d, self.oracle[name])
        d['variants'] = selected or variants(p)
        return expand(name, self.rows[name]['csvid'], d, p, d['variants'], self.rows, self.oracle)

    def test_primary_profiles_expand_all_cases_without_handler_expectations(self):
        for name in load_profiles()['sources']:
            cases = self.cases(name)
            self.assertTrue(cases)
            self.assertEqual(len(cases), len({c['name'] for c in cases}))
            self.assertEqual({int(c['_recipe']['variant'][-1]) for c in cases}, {0, 1})

    def test_whole_source_pin_and_parameters_fail_closed(self):
        for name in load_profiles()['sources']:
            d = self.declaration(name)
            for key, value in [('oracle_text', self.oracle[name]['oracle_text'] + '\nDraw a card.'),
                               ('mana_cost', '{99}'), ('power', '99'), ('layout', 'transform')]:
                r = dict(self.oracle[name], **{key: value}); changed = dict(d, oracle_text=r['oracle_text'])
                with self.assertRaises(ValueError): spec(changed, r)
            changed = copy.deepcopy(d); changed['params']['mechanism']['count'] = 7
            with self.assertRaises(ValueError): spec(changed, self.oracle[name])

    def test_generic_wording_does_not_depend_on_source_names(self):
        prototype = dict(name='Independent Source', layout='normal', type_line='Creature — Beast',
                         power='2', toughness='2', mana_cost='{2}{G}')
        for clause, kind, scope, another in [
            ('sacrifice another creature.', 'sacrifice', 'creature', True),
            ('sacrifice a land.', 'sacrifice', 'land', False),
            ('sacrifice it unless you discard a noncreature card.', 'discard_gate', 'noncreature', False),
            ("return another permanent you control to its owner's hand.", 'bounce', 'permanent', True),
            ("sacrifice it unless you return an artifact you control to its owner's hand.", 'bounce_gate', 'artifact', False),
        ]:
            p = mechanism(dict(prototype, oracle_text='When this creature enters, ' + clause))
            self.assertEqual((p['kind'], p['scope'], p['another']), (kind, scope, another))

    def test_quantities_and_colors_are_parsed_from_rules(self):
        r = dict(self.oracle['Dream Stalker'], name='Independent Source', oracle_text="When this creature enters, return three green or white creatures you control to their owner's hand.")
        p = mechanism(r)
        self.assertEqual(p['count'], 3); self.assertEqual(p['colors'], ['G', 'W'])
        self.assertFalse(p['targeted'])
        r['oracle_text'] = "When this creature enters, return two target lands you control to their owner's hand."
        p = mechanism(r); self.assertEqual(p['count'], 2); self.assertTrue(p['targeted'])

    def test_multiple_or_unmodeled_entry_clauses_are_not_silently_ignored(self):
        r = dict(self.oracle['Dream Stalker'])
        for text in [r['oracle_text'] + '\nWhen this creature enters, draw a card.',
                     "When this creature enters, return target creature an opponent controls to its owner's hand.",
                     'When this creature enters, sacrifice a creature. Then draw three cards.']:
            with self.assertRaises(ValueError): mechanism(dict(r, oracle_text=text))

    def test_witness_and_primary_identity_guards_are_independent(self):
        validate_witnesses(self.oracle, self.rows)
        oracle = copy.deepcopy(self.oracle); oracle['Grizzly Bears']['power'] = '9'
        with self.assertRaises(ValueError): validate_witnesses(oracle, self.rows)
        rows = copy.deepcopy(self.rows); rows['Accursed Centaur']['handler'] = 'card_dummy'
        with self.assertRaises(ValueError): validate_witnesses(self.oracle, rows)

    def test_live_cast_receipts_precede_entry_choices(self):
        for name in load_profiles()['sources']:
            for owner in (0, 1):
                s = self.cases(name, [f'effect_{owner}'])[0]['steps']
                cast = next(i for i, x in enumerate(s) if x['op'] == 'cast')
                resolve = next(i for i, x in enumerate(s) if x['op'] == 'resolve')
                self.assertFalse(any(x['op'] in ('choose_target', 'choose_zone_card', 'choose_optional_zone') for x in s[:cast]))
                self.assertIn(dict(op='assert_card', ref='source', property='in_hand', value=0), s[cast:resolve])
                self.assertIn(dict(op='assert_card', ref='source', property='in_play', value=0), s[cast:resolve])

    def test_sacrifice_self_is_an_actual_positive_effect_with_ai(self):
        c = self.cases('Accursed Centaur', ['effect_1'])[0]
        s = c['steps']; cut = next(i for i, x in enumerate(s) if x['op'] == 'resolve')
        self.assertFalse(any(x['op'] == 'choose_target' for x in s))
        self.assertIn(dict(op='assert_zone_multiplicity', player=1, zone='graveyard', index=self.rows['Accursed Centaur']['csvid'], value=1), s[cut:])
        self.assertIn(dict(op='assert_card', ref='enemy_creature', property='in_play', value=1), s[cut:])

    def test_foreign_owned_sacrifice_returns_to_owners_graveyard(self):
        c = self.cases('Accursed Centaur', ['foreign_owned_0'])[0]
        self.assertIn('opponent_owned', next(x for x in c['setup'] if x['ref'] == 'eligible_0')['state'])
        s = c['steps']; cut = next(i for i, x in enumerate(s) if x['op'] == 'resolve')
        self.assertIn(dict(op='assert_zone_multiplicity', player=1, zone='graveyard', index=self.rows['Grizzly Bears']['csvid'], value=1), s[cut:])
        self.assertIn(dict(op='assert_zone_multiplicity', player=0, zone='graveyard', index=self.rows['Grizzly Bears']['csvid'], value=0), s[cut:])

    def test_each_player_sacrifices_in_active_nonactive_order(self):
        c = self.cases('Fleshbag Marauder', ['effect_0'])[0]
        targets = [x for x in c['steps'] if x['op'] == 'choose_target']
        self.assertEqual(targets, [dict(op='choose_target', player=0, ref='eligible_0')])
        for player in (0, 1):
            self.assertIn(dict(op='assert_zone_multiplicity', player=player, zone='graveyard', index=self.rows['Grizzly Bears']['csvid'], value=1), c['steps'])

    def test_filter_failures_sacrifice_source_without_discarding_wrong_type(self):
        for name in ('Hidden Horror', 'Fallow Wurm', 'Drekavac'):
            for owner in (0, 1):
                c = self.cases(name, [f'no_match_{owner}'])[0]; s = c['steps']
                self.assertIn(dict(op='assert_zone_multiplicity', player=owner, zone='graveyard', index=self.rows[name]['csvid'], value=1), s)
                self.assertIn(dict(op='assert_forced_zone_choices', value=0), s)
                self.assertTrue(any(x['op'] == 'choose_optional_zone' for x in s))

    def test_gate_decline_and_empty_hand_do_not_invent_eligible_cards(self):
        for name in ('Hidden Horror', 'Fallow Wurm', 'Drekavac'):
            for owner in (0, 1):
                empty = self.cases(name, [f'empty_hand_{owner}'])[0]
                self.assertEqual([x['name'] for x in empty['setup'] if x['player'] == owner and x['zone'] == 'hand'], [name])
                self.assertFalse(any(x['op'] in ('choose_zone_card', 'choose_optional_zone') for x in empty['steps']))
                decline = self.cases(name, [f'decline_{owner}'])[0]
                self.assertTrue(any(x['op'] == 'choose_optional_zone' for x in decline['steps']))

    def test_another_creature_gate_cannot_use_the_source(self):
        for owner in (0, 1):
            c = self.cases('Faerie Impostor', [f'no_match_{owner}'])[0]
            self.assertFalse(any(x['op'] == 'choose_target' for x in c['steps']))
            self.assertIn(dict(op='assert_zone_multiplicity', player=owner, zone='graveyard', index=self.rows['Faerie Impostor']['csvid'], value=1), c['steps'])

    def test_bounce_self_and_bounce_foreign_owner_are_distinct(self):
        c = self.cases('Dream Stalker', ['source_choice_0'])[0]
        self.assertIn(dict(op='choose_target', player=0, ref='source'), c['steps'])
        c = self.cases('Dream Stalker', ['foreign_owned_1'])[0]
        self.assertIn(dict(op='assert_zone_multiplicity', player=0, zone='hand', index=self.rows['Grizzly Bears']['csvid'], value=1), c['steps'])

    def test_nonmatching_creature_entry_does_not_run_sources_entry_exchange(self):
        for name in load_profiles()['sources']:
            for owner in (0, 1):
                c = self.cases(name, [f'nonmatching_entry_{owner}'])[0]; s = c['steps']
                self.assertTrue(any(x['ref'] == 'source' and x['zone'] == 'hand' for x in c['setup']))
                first = next(i for i, x in enumerate(s) if x['op'] == 'cast' and x['ref'] == 'source')
                other = next(i for i, x in enumerate(s) if x['op'] == 'cast' and x['ref'] == 'entry_control')
                self.assertLess(first, other)
                self.assertIn(dict(op='resolve'), s[first:other])
                self.assertIn(dict(op='assert_choices_consumed'), s[first:other])
                self.assertTrue(any(x['op'] == 'assert_zone_multiplicity' for x in s[first:other]))
                self.assertFalse(any(x['op'] in ('choose_target', 'choose_zone_card', 'choose_optional_zone') for x in s[other:]))

    def test_snapshots_replay_owned_decisions_and_preserve_distinct_objects(self):
        for name in load_profiles()['sources']:
            s = self.cases(name, ['pending_snapshot_1'])[0]['steps']; ops = [x['op'] for x in s]
            a = ops.index('capture'); b = ops.index('record_state'); r = ops.index('restore')
            self.assertEqual(s[a+1:b], s[r+1:-1]); self.assertIsNot(s[a+1], s[r+1])

    def test_lifecycle_reentry_uses_real_spells_and_rebinds_hand_cards(self):
        c = self.cases('Dream Stalker', ['bounce_recast_1'])[0]
        self.assertEqual(sum(x['op'] == 'cast' and x.get('ref') == 'source' for x in c['steps']), 2)
        self.assertIn(dict(op='bind_hand', ref='source', player=1, value=self.rows['Dream Stalker']['csvid']), c['steps'])
        c = self.cases('Dream Stalker', ['blink_0'])[0]
        self.assertIn(dict(op='cast', ref='blink_spell', value=1), c['steps'])

    def test_complete_gate_payment_cannot_return_only_one_of_two_forests(self):
        for owner in (0, 1):
            c = self.cases('Bull Elephant', [f'one_match_{owner}'])[0]
            self.assertFalse(any(s['op'] == 'choose_target' for s in c['steps']))
            self.assertIn(dict(op='assert_card', ref='eligible_0', property='in_play', value=1), c['steps'])
            self.assertIn(dict(op='assert_zone_multiplicity', player=owner, zone='graveyard', index=self.rows['Bull Elephant']['csvid'], value=1), c['steps'])
            self.assertIn(dict(op='assert_zone_multiplicity', player=owner, zone='hand', index=self.rows['Forest']['csvid'], value=0), c['steps'])

    def test_mandatory_two_target_land_trigger_has_no_legal_partial_target_set(self):
        for owner in (0, 1):
            c = self.cases('Sea Drake', [f'one_match_{owner}'])[0]; s = c['steps']
            self.assertIn(dict(op='choose_target', player=owner, ref='eligible_0'), s)
            self.assertIn(dict(op='assert_unused_choices', value=1), s)
            check = next(i for i, x in enumerate(s) if x['op'] == 'assert_unused_choices')
            self.assertEqual(s[check+1], dict(op='assert_choices_consumed'))
            self.assertIn(dict(op='assert_card', ref='eligible_0', property='in_play', value=1), s)
            self.assertIn(dict(op='assert_card', ref='source', property='in_play', value=1), s)
            self.assertIn(dict(op='assert_zone_multiplicity', player=owner, zone='hand', index=self.rows['Forest']['csvid'], value=0), s)

    def test_nontargeted_three_creature_return_does_as_much_as_possible(self):
        for owner in (0, 1):
            c = self.cases('Dust Elemental', [f'source_only_{owner}'])[0]
            self.assertEqual([x for x in c['steps'] if x['op'] == 'choose_target'], [dict(op='choose_target', player=owner, ref='source')])
            self.assertIn(dict(op='assert_zone_multiplicity', player=owner, zone='hand', index=self.rows['Dust Elemental']['csvid'], value=1), c['steps'])

    def test_colored_return_exercises_both_allowed_colors_and_unchanged_wrong_color(self):
        first = self.cases('Fleetfoot Panther', ['effect_0'])[0]
        second = self.cases('Fleetfoot Panther', ['other_color_0'])[0]
        names = [next(x['name'] for x in c['setup'] if x['ref'] == 'eligible_0') for c in (first, second)]
        self.assertEqual([self.oracle[n]['colors'] for n in names], [['G'], ['W']])
        wrong = self.cases('Fleetfoot Panther', ['wrong_color_only_0'])[0]
        self.assertIn(dict(op='choose_target', player=0, ref='source'), wrong['steps'])
        self.assertIn(dict(op='assert_card', ref='wrong_color_creature', property='in_play', value=1), wrong['steps'])

    def test_another_permanent_return_with_only_source_does_nothing(self):
        for owner in (0, 1):
            c = self.cases('Invasive Species', [f'no_other_{owner}'])[0]
            self.assertEqual([x['name'] for x in c['setup'] if x['player'] == owner and x['zone'] == 'battlefield'], [])
            self.assertFalse(any(x['op'] == 'choose_target' for x in c['steps']))
            self.assertIn(dict(op='assert_card', ref='source', property='in_play', value=1), c['steps'])

    def test_scripted_sacrifice_is_separate_pinned_presentation_not_a_rules_change(self):
        name = "Ugin's Construct"
        for owner in (0, 1):
            c = self.cases(name, [f'effect_{owner}'])[0]
            self.assertIn(dict(op='choose_target', player=owner, ref='eligible_0'), c['steps'])
            self.assertIn(dict(op='assert_card', ref='own_control', property='in_play', value=1), c['steps'])
        d = self.declaration(name); d['params']['presentation']['scripted_sacrifice']['1'] = False
        with self.assertRaises(ValueError): spec(d, self.oracle[name])

    def test_faerie_refusal_preserves_intent_and_correct_no_return_self_sacrifice(self):
        home = Path(__file__).parent / 'data/cases/cards/127/12722-faerie-impostor.json'
        cases = json.loads(home.read_text(encoding='utf8'))['cases']
        self.assertEqual(len(cases), 2)
        for owner, c in enumerate(cases):
            s = c['steps']
            self.assertIn(dict(op='choose_optional_target', ref='return_candidate', player=owner), s)
            self.assertIn(dict(op='assert_forced_target_choices', value=0), s)
            self.assertIn(dict(op='assert_card', ref='return_candidate', property='in_play', value=1), s)
            self.assertIn(dict(op='assert_zone_multiplicity', player=owner, zone='graveyard', index=12722, value=1), s)
            fallback = next(x for x in c['setup'] if x['ref'] == 'return_candidate')
            self.assertEqual((fallback['name'], fallback['player'], fallback['zone']), ('Grizzly Bears', owner, 'battlefield'))
            cast = next(i for i, x in enumerate(s) if x['op'] == 'cast')
            choice = next(i for i, x in enumerate(s) if x['op'] == 'choose_optional_target')
            resolve = next(i for i, x in enumerate(s) if x['op'] == 'resolve')
            self.assertLess(cast, choice); self.assertLess(choice, resolve)

    def test_bounce_reentry_reserves_stable_matching_field_refs(self):
        for name, count in [('Fleetfoot Panther', 1), ('Dust Elemental', 3), ('Shrieking Drake', 1), ('Ancestral Statue', 1)]:
            for owner in (0, 1):
                c = self.cases(name, [f'blink_{owner}'])[0]; s = c['steps']
                fresh = [x for x in c['setup'] if x['ref'].startswith('reentry_eligible_')]
                self.assertEqual(len(fresh), count)
                self.assertTrue(all(x['zone'] == 'battlefield' and x['player'] == owner for x in fresh))
                cast = next(i for i, x in enumerate(s) if x['op'] == 'cast' and x['ref'] == 'source')
                initial = next(i for i, x in enumerate(s) if x['op'] == 'resolve')
                self.assertEqual([x['ref'] for x in s[cast:initial] if x['op'] == 'choose_target'], [f'eligible_{i}' for i in range(count)])
                blink = next(i for i, x in enumerate(s) if x['op'] == 'cast' and x['ref'] == 'blink_spell')
                resolved = next(i for i, x in enumerate(s[blink:], blink) if x['op'] == 'resolve')
                self.assertEqual([x['ref'] for x in s[blink:resolved] if x['op'] == 'choose_target'], [f'reentry_eligible_{i}' for i in range(count)])
                self.assertIn(dict(op='bind_permanent', ref='source', player=owner, value=self.rows[name]['csvid']), s[resolved:])

    def test_optional_return_refusal_controls_pay_their_own_oracle_cost(self):
        from recipes import cost_from_oracle
        for name, relative in [('Faerie Impostor', '127/12722-faerie-impostor.json'), ('Quickling', '141/14157-quickling.json')]:
            cases = json.loads((Path(__file__).parent / 'data/cases/cards' / relative).read_text(encoding='utf8'))['cases']
            for owner, c in enumerate(cases):
                supplied = [next(s['value'] for s in c['steps'] if s['op'] == 'set_mana' and s['player'] == owner and s['color'] == color) for color in range(6)]
                self.assertEqual(supplied, cost_from_oracle(self.oracle[name]))
                self.assertIn(dict(op='choose_optional_target', player=owner, ref='return_candidate'), c['steps'])
                self.assertIn(dict(op='assert_forced_target_choices', value=0), c['steps'])
                self.assertIn(dict(op='assert_zone_multiplicity', player=owner, zone='graveyard', index=self.rows[name]['csvid'], value=1), c['steps'])

    def test_all_generated_operations_are_supported(self):
        for name in load_profiles()['sources']:
            for c in self.cases(name):
                for s in c['steps']:
                    self.assertIn(s['op'], OPS)
                    if 'property' in s: self.assertIn(s['property'], PROPERTIES)


if __name__ == '__main__': unittest.main()
