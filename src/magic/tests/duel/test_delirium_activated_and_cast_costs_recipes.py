"""Focused proof of the Delirium cost recipes' rule boundaries."""

from copy import deepcopy
from unittest import TestCase

from delirium_activated_and_cast_costs_recipes import (
    DeliriumCostCase, FAMILY, MIXED, profiles, shape, spec, validate_witnesses,
)


class DeliriumCostTests(TestCase):
    def setUp(self):
        pins = profiles()
        self.oracle = {name: pin['oracle'] for name, pin in (pins['sources'] | pins['witnesses']).items()}
        self.registry = {name: pin['registry'] for name, pin in (pins['sources'] | pins['witnesses']).items()}

    def board(self, name, mode, actor=0):
        source = self.oracle[name]
        board = DeliriumCostCase(source, self.registry[name]['csvid'], actor, mode, self.registry, self.oracle)
        board.assemble()
        return board

    def test_exact_implemented_record(self):
        source = self.oracle['Crop Sigil']
        declaration = dict(family=FAMILY, version=2, oracle_text=source['oracle_text'],
                           params=shape(source), variants=['replay_0'])
        self.assertEqual(spec(declaration, source)['source'], 16062)
        with self.assertRaises(ValueError):
            spec(declaration, dict(source, mana_cost='{2}{G}'))
        with self.assertRaises(ValueError):
            spec(dict(declaration, variants=['mixed_type_0']), source)
        changed = deepcopy(self.registry)
        changed['Crop Sigil']['handler'] = 'card_dummy'
        with self.assertRaises(ValueError):
            validate_witnesses(self.oracle, changed)

    def test_gain_fourth_changes_real_cast_affordability(self):
        steps = self.board('Dusk Feaster', 'gain_fourth').steps
        casts = [(s['ref'], s['value']) for s in steps if s['op'] == 'cast']
        self.assertEqual(casts, [('source', 0), ('disenchant', 1), ('source', 1)])

    def test_exiling_the_graveyard_closes_pump_gate(self):
        steps = self.board('Stallion of Ashmouth', 'lose_fourth', actor=1).steps
        self.assertEqual([(s['op'], s['ref'], s['value']) for s in steps
                          if s['op'] in ('activate', 'can_activate')],
                         [('can_activate', 'source', 1), ('activate', 'crypt', 1),
                          ('can_activate', 'source', 0), ('activate', 'source', 0)])

    def test_mixed_identity_contributes_two_printed_types(self):
        record = self.oracle['Memnite']
        self.assertEqual(set(record['type_line'].partition(' — ')[0].split()), {'Artifact', 'Creature'})
        board = self.board('Scour the Laboratory', 'mixed_type')
        self.assertEqual([item['name'] for item in board.setup if item['zone']=='graveyard'], list(MIXED))
        self.assertIn(dict(op='cast', ref='source', value=1), board.steps)

    def test_preserve_crop_selector_and_replay_mill_guard(self):
        paid = self.board('Crop Sigil', 'four_types')
        mountain_id = self.registry['Mountain']['csvid']
        self.assertIn(dict(op='assert_offered_legal_card', index=mountain_id, value=1), paid.steps)
        replay = self.board('Crop Sigil', 'replay')
        self.assertEqual([s['value'] for s in replay.steps if s['op']=='can_activate'], [0, 1])
        self.assertIn(dict(op='upkeep_step', player=0), replay.steps)
        self.assertFalse(any(s['op']=='activate' for s in replay.steps))
