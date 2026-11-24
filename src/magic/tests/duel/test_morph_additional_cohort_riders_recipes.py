"""Focused checks for the new paid Morph targets and outcomes."""

from copy import deepcopy
import unittest

import morph_additional_cohort_riders_recipes as model


class MorphAdditionalCohortGuards(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        pins = model.profile()
        cards = pins['sources'] | pins['witnesses']
        cls.oracle = {name: pin['oracle'] for name, pin in cards.items()}
        cls.rows = {name: pin['registry'] for name, pin in cards.items()}
        cls.rows['Face-Down Creature'] = pins['face_down']

    def steps(self, name, variant):
        printed = self.oracle[name]
        declaration = dict(family=model.FAMILY, version=2, oracle_text=printed['oracle_text'],
                           params=model.parameters(printed), variants=[variant])
        case, = model.expand(name, self.rows[name]['csvid'], declaration, model.spec(declaration,printed),
                             [variant], self.rows, self.oracle)
        return case['steps']

    def test_accepted_scope_and_complete_no_regeneration_clause(self):
        self.assertEqual(sum(len(model.variants(model.parameters(self.oracle[name])))
                             for name in model.profile()['sources']),90)
        altered = deepcopy(self.oracle['Skinthinner'])
        altered['oracle_text'] = altered['oracle_text'].replace(" It can't be regenerated.",'')
        with self.assertRaises(ValueError):
            model.parameters(altered)

    def test_paid_morph_face_choice(self):
        for mode, faceup in [('printed',1),('hidden',0)]:
            steps = self.steps('Nantuko Vigilante',mode+'_1')
            self.assertIn(dict(op='choose_dialog',player=1,value=faceup),steps)
            csvid = self.rows['Nantuko Vigilante']['csvid'] if faceup else model.FACE_DOWN
            self.assertIn(dict(op='assert_card',ref='source',property='csvid',value=csvid),steps)
            self.assertNotIn('activate',[check['op'] for check in steps])

    def test_short_flip_preserves_real_hidden_identity(self):
        steps = self.steps('Patron of the Wild','flip_short_0')
        start = steps.index(dict(op='activate',ref='source',value=0))
        self.assertIn(dict(op='assert_card',ref='source',property='csvid',value=model.FACE_DOWN),steps[start:])
        self.assertNotIn('choose_target',[item['op'] for item in steps[start:]])

    def test_enchantment_and_artifact_destruction_targets(self):
        for mode, name, value in [('enchantment','Fervor',1),('indestructible','Darksteel Relic',0)]:
            steps = self.steps('Nantuko Vigilante',mode+'_1')
            if value:
                self.assertIn(dict(op='assert_zone_multiplicity',player=0,zone='graveyard',
                                   index=self.rows[name]['csvid'],value=1),steps)
            else:
                self.assertNotIn(dict(op='assert_zone_multiplicity',player=0,zone='graveyard',
                                      index=self.rows[name]['csvid'],value=1),steps)

    def test_patron_cleanup_removes_both_stat_increases(self):
        steps = self.steps('Patron of the Wild','expiry_0')
        end = steps.index(dict(op='cleanup'))
        for property in ('power','toughness'):
            self.assertIn(dict(op='assert_card',ref='victim',property=property,value=5),steps[:end])
            self.assertIn(dict(op='assert_card',ref='victim',property=property,value=2),steps[end:])

    def test_skinthinner_keeps_real_contrasting_regeneration(self):
        steps = self.steps('Skinthinner','cannot_regenerate_1')
        choices = [i for i, step in enumerate(steps) if step == dict(op='choose_regeneration',ref='victim')]
        control, denied = choices
        self.assertIn(dict(op='assert_card',ref='victim',property='tapped',value=1),steps[control:denied])
        self.assertIn(dict(op='assert_unused_choices',value=1),steps[denied:])
        self.assertIn(dict(op='assert_zone_multiplicity',player=0,zone='graveyard',
                           index=self.rows['Uthden Troll']['csvid'],value=1),steps[denied:])

    def test_skirk_distinguishes_player_creature_and_planeswalker(self):
        steps = self.steps('Skirk Marauder','walker_target_0')
        self.assertIn(dict(op='assert_counter',ref='victim',counter_kind='loyalty',value=1),steps)
        steps = self.steps('Skirk Marauder','own_player_1')
        self.assertIn(dict(op='choose_player',player=1,value=1),steps)
        self.assertIn(dict(op='assert_life',player=1,value=18),steps)

    def test_weaver_uses_foreign_and_own_target_checks_and_actual_flight(self):
        steps = self.steps('Mistfire Weaver','expiry_0')
        end = steps.index(dict(op='cleanup'))
        allowed = [item['value'] for item in steps if item['op']=='can_target_creature' and item['ref']=='foreign_spell']
        self.assertEqual(allowed,[1,0,1])
        self.assertIn(dict(op='can_target_creature',ref='own_spell',other='victim',value=1),steps[:end])
        for mode, allowed in [('flying_combat',0),('hidden_combat',1)]:
            steps = self.steps('Mistfire Weaver',mode+'_1')
            self.assertIn(dict(op='block',ref='blocker',other='source',value=allowed),steps)
