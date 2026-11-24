"""Guards for real paid turn-up inputs and complete printed target effects."""

import json
from pathlib import Path
import unittest

from generate_cases import registry
import morph_turn_up_target_recipes as model


class TurnUpTargetRecipes(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        data = Path(__file__).parent / 'data'
        cls.oracle = json.loads((data / 'oracle.json').read_text(encoding='utf8'))['cards']
        cls.rows = {row['name']: row for row in registry()}
        cls.homes = {}
        for name, pin in model.pins()['sources'].items():
            csvid = pin['registry']['csvid']
            path, = (data / 'cases/cards').glob(f'*/{csvid:05d}-*.json')
            cls.homes[name] = json.loads(path.read_text(encoding='utf8'))['recipes'][0]

    def cases(self, name, *selected):
        declaration = self.homes[name]
        record = self.oracle[name]
        return model.expand(name, self.rows[name]['csvid'], declaration, model.spec(declaration, record),
                            selected, self.rows, self.oracle)

    def steps(self, name, variant):
        return self.cases(name, variant)[0]['steps']

    def test_exact_cohort_and_whole_oracle(self):
        model.validate_witnesses(self.oracle, self.rows)
        effects = set()
        total = 0
        for name, declaration in self.homes.items():
            shape = model.spec(declaration, self.oracle[name])
            effects.add(shape['effect'])
            self.assertEqual(declaration['variants'], model.variants(shape))
            cases = self.cases(name, *declaration['variants'])
            total += len(cases)
            for case in cases:
                self.assertEqual(case['_recipe']['covered_clauses'], self.oracle[name]['oracle_text'].splitlines())
        self.assertEqual(effects, {'shrink', 'toggle', 'destroy', 'bounce', 'exchange', 'pump'})
        self.assertEqual(total, 174)

    def test_rejects_partial_or_changed_oracle_and_untyped_variant(self):
        name = 'Efreet Weaponmaster'
        record = self.oracle[name]
        declaration = self.homes[name]
        with self.assertRaises(ValueError):
            model.spec(declaration, dict(record, oracle_id='stale'))
        with self.assertRaises(ValueError):
            model.spec(declaration, dict(record, oracle_text=record['oracle_text'].removeprefix('First strike\n')))
        for changes in (dict(version=True), dict(family='other'), dict(variants=['flip_0', 'flip_0']),
                        dict(variants=['wrong_1']), dict(variants=[0])):
            with self.subTest(changes=changes), self.assertRaises(ValueError):
                model.spec(dict(declaration, **changes), record)

    def test_hidden_face_up_and_short_casts(self):
        name = 'Daru Sanctifier'
        hidden, faceup, short = self.cases(name, 'hidden_choice_1', 'faceup_1', 'hidden_short_1')
        for case in (hidden, faceup, short):
            clear = [(s['player'], s['color'], s['value']) for s in case['steps'] if s['op']=='set_mana'][:12]
            self.assertEqual(clear, [(player, color, 0) for player in (0, 1) for color in range(6)])
        self.assertIn(dict(op='choose_dialog',player=1,value=0), hidden['steps'])
        self.assertIn(dict(op='assert_card',ref='source',property='csvid',value=model.FACE_DOWN), hidden['steps'])
        self.assertIn(dict(op='choose_dialog',player=1,value=1), faceup['steps'])
        self.assertIn(dict(op='assert_card',ref='source',property='csvid',value=6360), faceup['steps'])
        self.assertIn(dict(op='cast',ref='source',value=0), short['steps'])
        self.assertNotIn('choose_dialog', {s['op'] for s in short['steps']})

    def test_reveal_cost_and_identity_precede_response_drain(self):
        steps = self.steps('Echo Tracer', 'pending_spell_1')
        i = steps.index(dict(op='activate',ref='source',value=1))
        j = next(j for j in range(i,len(steps)) if steps[j]['op']=='resolve_response')
        expected_mana = [dict(op='assert_mana',player=1,color=color,value=0) for color in range(6)]
        for expected in expected_mana+[dict(op='assert_card',ref='source',property='csvid',value=6371)]:
            self.assertIn(expected, steps[i:j])
        self.assertIn(dict(op='assert_stack',value=1), steps[:i])
        self.assertIn(dict(op='assert_zone_multiplicity',player=1,zone='hand',index=6371,value=1), steps[j:])

    def test_wrong_color_and_short_reveal_leave_hidden_source(self):
        for variant in ('flip_wrong_0', 'flip_short_1'):
            steps = self.steps('Efreet Weaponmaster', variant)
            self.assertIn(dict(op='activate',ref='source',value=0), steps)
            self.assertNotIn('choose_target', {s['op'] for s in steps})
            self.assertIn(dict(op='assert_card',ref='source',property='csvid',value=model.FACE_DOWN), steps)

    def test_lethal_and_temporary_shrink_receipts(self):
        name = 'Aphetto Exterminator'
        nonlethal, lethal = self.cases(name, 'nonlethal_0','flip_0')
        steps = nonlethal['steps']
        at = steps.index(dict(op='cleanup'))
        self.assertIn(dict(op='assert_card',ref='victim',property='toughness',value=1), steps[:at])
        self.assertIn(dict(op='assert_card',ref='victim',property='toughness',value=4), steps[at:])
        csvid = self.rows['Goblin Tinkerer']['csvid']
        self.assertIn(dict(op='assert_zone_multiplicity',player=1,zone='graveyard',index=csvid,value=1), lethal['steps'])
        self.assertNotIn(dict(op='assert_card',ref='victim',property='in_play',value=0), lethal['steps'])

    def test_toggle_and_optional_selection(self):
        name = 'Coral Trickster'
        flip, untap, decline = self.cases(name, 'flip_1','untap_1','decline_1')
        self.assertIn(dict(op='choose_cancel_target', player=1), decline['steps'])
        self.assertIn(dict(op='choose_target',player=1,ref='victim'), flip['steps'])
        for case, value in ((flip,1), (untap,0), (decline,0)):
            receipts = [s['value'] for s in case['steps'] if s['op']=='assert_card'
                        and s['ref']=='victim' and s['property']=='tapped']
            self.assertEqual(receipts[-1], value)

    def test_late_selection_and_ownership(self):
        daru = self.steps('Daru Sanctifier','target_changed_1')
        targets = [(s['player'],s['ref']) for s in daru if s['op']=='choose_target']
        self.assertEqual(targets, [(0,'victim'),(1,'fallback')])
        echo = self.steps('Echo Tracer','foreign_owner_1')
        csvid = self.rows['Goblin Tinkerer']['csvid']
        self.assertIn(dict(op='bind_permanent',ref='victim',player=0,value=csvid), echo)
        self.assertIn(dict(op='assert_zone_multiplicity',player=1,zone='hand',index=csvid,value=1), echo)
        crab = self.steps('Chromeshell Crab','decline_second_0')
        self.assertEqual([s['op'] for s in crab if s['op'] in ('choose_target','choose_cancel_target')],
                         ['choose_target', 'choose_cancel_target'])
        swap = self.steps('Chromeshell Crab','source_swap_1')
        self.assertIn(dict(op='bind_permanent',ref='source',player=0,value=6350), swap)

    def test_crab_changed_target_has_distinct_lifecycle_ids(self):
        for owner in (0,1):
            with self.subTest(owner=owner):
                case, = self.cases('Chromeshell Crab', f'target_changed_{owner}')
                setup = {entry['ref']: entry['name'] for entry in case['setup']}
                self.assertEqual((setup['ally'], setup['victim'], setup['fallback']),
                                 ('Goblin Tinkerer', 'Serra Angel', 'Indomitable Archangel'))
                steps = case['steps']
                self.assertEqual([s['ref'] for s in steps if s['op']=='choose_target'],
                                 ['ally', 'fallback', 'ally'])
                bindings = [i for i, step in enumerate(steps) if step['op']=='bind_permanent']
                self.assertEqual([steps[i] for i in bindings], [
                    dict(op='bind_permanent',ref='ally',player=1-owner,value=2493),
                    dict(op='bind_permanent',ref='fallback',player=1-owner,value=11283),
                    dict(op='bind_permanent',ref='ally',player=owner,value=2493),
                ])
                for ref, controller in (('ally',owner), ('fallback',1-owner), ('victim',1-owner)):
                    index = self.rows[setup[ref]]['csvid']
                    self.assertIn(dict(op='assert_zone_multiplicity',player=controller,zone='battlefield',
                                       index=index,value=1), steps[bindings[-1]+1:])

    def test_efreet_scope_combat_and_expiration(self):
        name = 'Efreet Weaponmaster'
        steps = self.steps(name,'eligibility_0')
        for ref,value in (('source',0), ('wrong_target',0), ('victim',1)):
            self.assertIn(dict(op='probe_activation_target',ref='source',other=ref,index=-1,value=value), steps)
        hidden, revealed, entry = self.cases(name,'hidden_combat_0','first_strike_0','entry_cleanup_0')
        self.assertNotIn('activate', {s['op'] for s in hidden['steps']})
        self.assertIn(dict(op='block',ref='blocker',other='source',value=1), revealed['steps'])
        self.assertIn(dict(op='assert_zone_multiplicity',player=0,zone='graveyard',index=14264,value=1), hidden['steps'])
        csvid = self.rows['Serra Angel']['csvid']
        self.assertIn(dict(op='assert_zone_multiplicity',player=1,zone='graveyard',index=csvid,value=1), revealed['steps'])
        power = [s['value'] for s in entry['steps'] if s['op']=='assert_card'
                 and s['ref']=='victim' and s['property']=='power']
        self.assertEqual(power, [4,1])

    def test_replay_repeats_the_real_paid_turn_up(self):
        steps = self.steps('Aphetto Exterminator','replay_1')
        capture = steps.index(dict(op='capture'))
        record = steps.index(dict(op='record_state'))
        restore = steps.index(dict(op='restore'))
        self.assertEqual(steps[capture+1:record], steps[restore+1:-1])
        self.assertIn(dict(op='ai_roundtrip'), steps[record:restore])
        self.assertEqual(steps[-1], dict(op='assert_recorded_state'))
