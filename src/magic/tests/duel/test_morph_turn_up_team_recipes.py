"""Guards for complete team Morph models and real bounded lifecycle inputs."""

import json
from pathlib import Path
import unittest

from generate_cases import registry
import morph_turn_up_team_recipes as model


class MorphTeamRecipes(unittest.TestCase):
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
        shape = model.spec(declaration, self.oracle[name])
        return model.expand(name, self.rows[name]['csvid'], declaration, shape, selected,
                            self.rows, self.oracle)

    def steps(self, name, variant):
        return self.cases(name, variant)[0]['steps']

    def values(self, steps, op, **fields):
        return [s['value'] for s in steps if s['op'] == op and all(s.get(k) == v for k,v in fields.items())]

    def test_whole_pinned_records_and_closed_variants(self):
        model.validate_witnesses(self.oracle, self.rows)
        generated = []
        for name, declaration in self.homes.items():
            shape = model.spec(declaration, self.oracle[name])
            self.assertEqual(declaration['variants'], model.variants(shape))
            cases = self.cases(name, *declaration['variants'])
            for case in cases:
                self.assertEqual(case['_recipe']['covered_clauses'], self.oracle[name]['oracle_text'].splitlines())
            generated.extend(cases)
        self.assertEqual(len(generated), 148)
        self.assertEqual(len({c['name'] for c in generated}), 148)
        r = self.oracle['Bane of the Living']
        d = self.homes['Bane of the Living']
        for variant in (dict(version=True), dict(variants=['x_one_reserved_0', 'x_one_reserved_0']),
                        dict(variants=['unknown_1']), dict(variants=[True])):
            with self.subTest(variant=variant), self.assertRaises(ValueError):
                model.spec(dict(d, **variant), r)
        with self.assertRaises(ValueError):
            model.parameters(dict(r, oracle_text=r['oracle_text'].replace('{X}', '{3}')))

    def test_face_down_printed_and_wrong_color_costs(self):
        hidden, faceup, wrong = self.cases('Defender of the Order', 'hidden_choice_0','faceup_0','flip_wrong_0')
        for case in (hidden,faceup,wrong):
            clear = [s for s in case['steps'] if s['op']=='set_mana'][:12]
            self.assertEqual(clear,[dict(op='set_mana',player=p,color=c,value=0) for p in (0,1) for c in range(6)])
        self.assertIn(dict(op='choose_dialog',player=0,value=0),hidden['steps'])
        self.assertIn(dict(op='assert_card',ref='source',property='csvid',value=model.FACE_DOWN),hidden['steps'])
        self.assertIn(dict(op='choose_dialog',player=0,value=1),faceup['steps'])
        self.assertIn(dict(op='assert_card',ref='source',property='csvid',value=6363),faceup['steps'])
        self.assertIn(dict(op='activate',ref='source',value=0),wrong['steps'])
        self.assertEqual(self.values(wrong['steps'],'assert_mana',player=0,color=2)[-1],1)

    def test_friendly_scope_and_source_with_cleanup(self):
        for name,power,toughness in (('Defender of the Order',1,4),('Master of Pearls',3,4)):
            with self.subTest(name=name):
                steps = self.steps(name,'team_1')
                clean = steps.index(dict(op='cleanup'))
                self.assertEqual(self.values(steps[:clean],'assert_card',ref='ally',property='power')[-1],power)
                self.assertEqual(self.values(steps[:clean],'assert_card',ref='ally',property='toughness')[-1],toughness)
                self.assertEqual(self.values(steps[:clean],'assert_card',ref='source',property='toughness')[-1],
                                 int(self.oracle[name]['toughness']) + 2)
                self.assertEqual(self.values(steps[:clean],'assert_card',ref='foe',property='toughness')[-1],4)
                self.assertEqual(self.values(steps[clean:],'assert_card',ref='ally',property='toughness')[-1],2)

    def test_real_late_creature_and_combat_boundary(self):
        name = 'Master of Pearls'
        late, battle = self.cases(name,'late_entry_0','blocked_0')
        steps = late['steps']
        cast = steps.index(dict(op='cast',ref='late',value=1))
        self.assertLess(steps.index(dict(op='activate',ref='source',value=1)),cast)
        self.assertEqual(self.values(steps[cast:],'assert_card',ref='late',property='power'),[1,1])
        steps = battle['steps']
        self.assertLess(steps.index(dict(op='advance_combat')),steps.index(dict(op='activate',ref='source',value=1)))
        self.assertLess(steps.index(dict(op='activate',ref='source',value=1)),steps.index(dict(op='combat_damage')))

    def test_tribal_actual_menu_and_both_controllers(self):
        for variant,ref,ally,foe,angel in (('goblins_1','goblin_model',3,3,4),('angels_1','angel_model',1,1,6)):
            steps = self.steps('Tribal Forcemage',variant)
            flip = steps.index(dict(op='activate',ref='source',value=1))
            self.assertEqual(steps[flip-3:flip],[
                dict(op='choose_dialog',player=1,value=2),dict(op='choose_target',player=1,ref=ref),
                dict(op='choose_dialog',player=1,value=0)])
            clean = steps.index(dict(op='cleanup'))
            for creature,power in (('ally',ally),('foe',foe),('angel_model',angel),('source',1)):
                self.assertEqual(self.values(steps[:clean],'assert_card',ref=creature,property='power')[-1],power)

    def test_trample_changes_only_actual_overflow(self):
        name = 'Tribal Forcemage'
        for mode,life in (('trample_0',18),('trample_hidden_0',20),('trample_expired_0',20)):
            steps = self.steps(name,mode)
            self.assertIn(dict(op='block',ref='blocker',other='ally',value=1),steps)
            self.assertEqual(self.values(steps,'assert_life',player=1)[-1],life)
            if mode == 'trample_expired_0':
                self.assertLess(steps.index(dict(op='cleanup')),steps.index(dict(op='begin_attackers')))

    def test_bane_chooses_zero_and_positive_x_and_pays_actual_fees(self):
        name = 'Bane of the Living'
        for mode,x,residual in (('x_zero_0',0,0),('x_one_reserved_0',1,2),('x_two_1',2,0),('x_three_1',3,0)):
            actor = int(mode[-1])
            steps = self.steps(name,mode)
            self.assertEqual(self.values(steps,'choose_x',player=actor),[x])
            self.assertEqual(self.values(steps,'assert_mana',player=actor,color=0)[-1],residual)
            self.assertEqual(self.values(steps,'assert_mana',player=actor,color=1)[-1],0)
            clean = steps.index(dict(op='cleanup'))
            self.assertEqual(self.values(steps[:clean],'assert_card',ref='foe',property='toughness')[-1],4-x)
            self.assertEqual(self.values(steps[clean:],'assert_card',ref='foe',property='toughness')[-1],4)
        lethal = self.steps(name,'x_three_1')
        self.assertIn(dict(op='assert_zone_multiplicity',player=1,zone='graveyard',index=6338,value=1),lethal)

    def test_frontline_scope_excludes_soldiers_and_spell_damage(self):
        name = 'Frontline Strategist'
        combat = self.steps(name,'prevent_attack_0')
        hidden = self.steps(name,'hidden_combat_0')
        spell = self.steps(name,'spell_damage_0')
        defense = self.steps(name,'prevent_block_0')
        expiry = self.steps(name,'expiry_0')
        self.assertEqual(self.values(combat,'assert_life',player=1)[-1],18)
        self.assertEqual(self.values(hidden,'assert_life',player=1)[-1],17)
        self.assertIn(dict(op='cast',ref='shock',value=1),spell)
        self.assertEqual(self.values(spell,'assert_life',player=1)[-1],18)
        self.assertIn(dict(op='assert_zone_multiplicity',player=0,zone='graveyard',index=2493,value=1),defense)
        self.assertNotIn(dict(op='assert_zone_multiplicity',player=1,zone='graveyard',index=6081,value=1),defense)
        self.assertEqual(self.values(expiry,'assert_life',player=1)[-1],19)

    def test_pine_other_reveal_checks_control_and_hidden_watcher(self):
        name = 'Pine Walker'
        for mode,tapped in (('ally_flip_0',0),('opposing_flip_0',1),('watcher_hidden_0',1)):
            steps = self.steps(name,mode)
            self.assertIn(dict(op='cast',ref='secondary',value=1),steps)
            self.assertIn(dict(op='choose_target',player=0,ref='secondary'),steps)
            self.assertIn(dict(op='activate',ref='secondary',value=1),steps)
            self.assertEqual(self.values(steps,'assert_card',ref='secondary',property='tapped')[-2:],[1,tapped])
        steps = self.steps(name,'self_attack_0')
        self.assertIn(dict(op='assert_card',ref='source',property='attacking',value=1),steps)
        self.assertEqual(self.values(steps,'assert_card',ref='source',property='tapped')[-1],0)
        self.assertEqual(self.values(steps,'assert_life',player=1)[-1],15)

    def test_replay_repeats_active_suffix(self):
        steps = self.steps('Pine Walker','replay_1')
        capture = steps.index(dict(op='capture'))
        record = steps.index(dict(op='record_state'))
        restore = steps.index(dict(op='restore'))
        self.assertEqual(steps[capture+1:record],steps[restore+1:-1])
        self.assertIn(dict(op='activate',ref='source',value=1),steps[capture:record])
        self.assertEqual(steps[-1],dict(op='assert_recorded_state'))
