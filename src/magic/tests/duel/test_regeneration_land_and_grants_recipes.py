"""Guard regeneration scopes and the deliberately separate hard diagnostic."""
from copy import deepcopy
import unittest

import regeneration_land_and_grants_recipes as model


class RegenerationLandAndGrantsTests(unittest.TestCase):
    def setUp(self):
        pins = model.profiles()
        entries = pins['sources'] | pins['witnesses']
        self.oracle = {name: pin['oracle'] for name, pin in entries.items()}
        self.rows = {name: pin['registry'] for name, pin in entries.items()} | pins['engine_faces']

    def board(self, csvid, mode, owner=0):
        board = model.CreatureBoard(csvid, mode, owner, self.rows, self.oracle)
        board.run()
        return board

    def test_whole_oracle_and_witness_identity(self):
        model.validate_witnesses(self.oracle, self.rows)
        changed = deepcopy(self.oracle['Crypt Sliver'])
        changed['oracle_text'] = changed['oracle_text'].replace('All Slivers', 'Other Slivers')
        with self.assertRaises(ValueError):
            model.classify(changed, model.CRYPT)
        wrong = self.rows | {model.ANIMATED: self.rows[model.ANIMATED] | {'iid': 0}}
        with self.assertRaises(ValueError):
            model.validate_witnesses(self.oracle, wrong)

    def test_swarmyard_crash_scope_excludes_only_typed_dying_targets(self):
        modes = model.variants({'csvid': model.SWARM})
        self.assertEqual(set(modes) - {f'{flag}_{p}' for flag in model.SWARM_HELD for p in (0, 1)},
                         {'mana_0', 'mana_1', 'non_tribe_0', 'non_tribe_1'})
        self.assertEqual(len(model.SWARM_HELD), 6)

    def test_sedge_checks_swamp_of_each_recipient(self):
        for owner in (0, 1):
            board = self.board(model.SEDGE, 'foreign_swamp', owner)
            latest = {s['ref']: s['value'] for s in board.steps
                      if s['op'] == 'assert_card' and s['property'] == 'power'}
            self.assertEqual(latest, {'source': 2, 'own_sliver': 2, 'foreign_sliver': 3, 'other_type': 2})
            lost = self.board(model.SEDGE, 'lose_swamp', owner)
            self.assertIn({'op': 'play_land', 'ref': 'own_land', 'value': 1}, lost.steps)
            self.assertEqual([s['value'] for s in lost.steps if s['op'] == 'assert_card' and
                              s['property'] == 'power' and s['ref'] == 'own_sliver'], [3, 2])

    def test_zombie_dialog_is_queued_for_dying_zombie_controller(self):
        for owner in (0, 1):
            for mode, player, choice in (('own_regenerate', owner, 0), ('foreign_regenerate', 1-owner, 0),
                                         ('decline', owner, 1), ('unfunded', owner, None),
                                         ('no_self_regeneration', owner, None)):
                seq = self.board(model.MASTER, mode, owner).steps
                dialogs = [s for s in seq if s['op'] == 'choose_dialog']
                self.assertEqual(dialogs, [] if choice is None else [{'op': 'choose_dialog', 'player': player, 'value': choice}])
                if dialogs:
                    step = seq.index(dialogs[0])
                    self.assertEqual(
                        seq[step+1:step+4],
                        [{'op': 'resolve'}, {'op': 'refresh'}, {'op': 'assert_unused_choices', 'value': 0}],
                    )
                    self.assertTrue(any(s['op'] == 'cast' and s['ref'] == 'removal' for s in seq[:step]))
