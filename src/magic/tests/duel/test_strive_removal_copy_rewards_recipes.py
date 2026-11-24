"""Guards for whole Oracle and executable Strive target boundaries."""

from copy import deepcopy
import unittest

import strive_removal_copy_rewards_recipes as model
from strive_target_rewards_recipes import total


class StriveRemovalCopyRewardGuards(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        pinned = model.profile()
        entries = pinned['sources'] | pinned['witnesses']
        cls.oracle = {name: pin['oracle'] for name, pin in entries.items()}
        cls.rows = {name: pin['registry'] for name, pin in entries.items()}

    def case(self, name, variant):
        record = self.oracle[name]
        declaration = dict(family=model.FAMILY, version=2, oracle_text=record['oracle_text'],
                           params=model.parameters(record), variants=[variant])
        return model.expand(name, self.rows[name]['csvid'], declaration, model.spec(declaration, record),
                            [variant], self.rows, self.oracle)[0]

    def test_complete_clause_and_increment(self):
        for name, phrase in [('Twinflame', 'except it has haste'), ('Silence the Believers', 'all Auras'),
                             ('Polymorphous Rush', 'until end of turn'), ("Kiora's Dismissal", 'owners')]:
            with self.subTest(name=name):
                record = deepcopy(self.oracle[name])
                record['oracle_text'] = record['oracle_text'].replace(phrase, '')
                with self.assertRaises(ValueError):
                    model.parameters(record)
        silence = model.parameters(self.oracle['Silence the Believers'])
        self.assertEqual(total(silence, 3), [6, 4, 0, 0, 0, 0])
        kiora = model.parameters(self.oracle["Kiora's Dismissal"])
        self.assertEqual(total(kiora, 3), [0, 0, 3, 0, 0, 0])

    def test_funded_done_and_short_increment(self):
        for mode, choices, done in [('zero', [], True), ('stop_one', ['first'], True),
                                    ('short_color', ['first'], False)]:
            steps = self.case('Consign to Dust', mode + '_0')['steps']
            self.assertEqual([step['ref'] for step in steps if step['op'] == 'choose_target'], choices)
            self.assertEqual(any(step['op'] == 'choose_done_target' for step in steps), done)

    def test_shrouded_model_is_only_selected_for_resolution(self):
        steps = self.case('Polymorphous Rush', 'shrouded_model_1')['steps']
        targets = [(i, step['ref']) for i, step in enumerate(steps) if step['op'] == 'choose_target']
        self.assertEqual([ref for _, ref in targets], ['first', 'second', 'model'])
        cast = next(i for i, step in enumerate(steps) if step['op'] == 'cast')
        self.assertLess(targets[1][0], cast)
        self.assertLess(cast, targets[2][0])
        cleanup = steps.index(dict(op='cleanup'))
        self.assertIn(dict(op='assert_card', ref='second', property='power', value=2), steps[:cleanup])
        self.assertIn(dict(op='assert_card', ref='second', property='power', value=3), steps[cleanup:])

    def test_indestructible_artifact_stays_while_enchantment_dies(self):
        steps = self.case('Consign to Dust', 'indestructible_1')['steps']
        for name, who, zone in [('Darksteel Relic', 1, 'battlefield'), ('Fervor', 0, 'graveyard')]:
            self.assertIn(dict(op='assert_zone_multiplicity', zone=zone, player=who,
                               index=self.rows[name]['csvid'], value=1), steps)

    def test_silence_exiles_both_attached_auras_and_spares_unrelated(self):
        case = self.case('Silence the Believers', 'foreign_auras_0')
        self.assertEqual([(step['player'], step['ref']) for step in case['steps'] if step['op'] == 'choose_target'][:3],
                         [(1, 'first'), (0, 'first'), (0, 'third')])
        for name, who, zone in [('Flight', 1, 'exile'), ('Spirit Link', 0, 'exile'), ('Pacifism', 0, 'battlefield')]:
            self.assertIn(dict(op='assert_zone_multiplicity', zone=zone, player=who,
                               index=self.rows[name]['csvid'], value=1), case['steps'])

    def test_copy_token_attacks_before_production_end_step_expiry(self):
        steps = self.case('Twinflame', 'same_turn_haste_1')['steps']
        self.assertIn(dict(op='attack', ref='token_first', value=1), steps)
        end = steps.index(dict(op='cleanup'))
        bears = dict(op='assert_zone_multiplicity', zone='battlefield', player=1,
                     index=self.rows['Grizzly Bears']['csvid'])
        self.assertIn(dict(bears, value=2), steps[:end])
        self.assertIn(dict(bears, value=1), steps[end:])
        self.assertIn(dict(op='assert_life', player=0, value=18), steps[end:])

    def test_foreign_control_does_not_change_owner_hand(self):
        case = self.case("Kiora's Dismissal", 'foreign_owner_1')
        self.assertIn('opponent_owned', next(entry for entry in case['setup'] if entry['ref'] == 'first')['state'])
        self.assertIn(dict(op='assert_zone_multiplicity', zone='hand', player=0,
                           index=self.rows['Fervor']['csvid'], value=1), case['steps'])

    def test_partial_response_uses_real_pending_spell_and_preserves_paid_cost(self):
        steps = self.case('Silence the Believers', 'late_auras_1')['steps']
        self.assertEqual([step['ref'] for step in steps if step['op'] == 'choose_target'][-3:],
                         ['first', 'second', 'first'])
        self.assertLess(steps.index(dict(op='announce_spell', ref='source')),
                        steps.index(dict(op='cast_response', ref='response', value=1)))
        self.assertIn(dict(op='assert_zone_multiplicity', zone='exile', player=0,
                           index=self.rows['Centaur Courser']['csvid'], value=1), steps)
        self.assertIn(dict(op='assert_zone_multiplicity', zone='graveyard', player=1,
                           index=self.rows['Flight']['csvid'], value=1), steps)
