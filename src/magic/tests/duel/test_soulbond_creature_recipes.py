"""Focused checks for Soulbond event provenance and independent combat grants."""
import copy
import json
import unittest
from pathlib import Path

import soulbond_creature_recipes as recipe
from generate_cases import registry


class SoulbondRecipes(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        data = Path(__file__).parent / 'data'
        cls.oracle = json.loads((data / 'oracle.json').read_text(encoding='utf8'))['cards']
        cls.rows = {row['name']: row for row in registry()}
        cls.profile = recipe.pins()
        recipe.validate_witnesses(cls.oracle, cls.rows)
        cls.cases = {}
        for name, pin in cls.profile['sources'].items():
            csvid = pin['csvid']
            path, = (data / 'cases/cards').glob(f'*/{csvid:05d}-*.json')
            declaration, = json.loads(path.read_text(encoding='utf8'))['recipes']
            model = recipe.spec(declaration, cls.oracle[name])
            variants = recipe.variants(model)
            if declaration['variants'] != variants:
                raise AssertionError(name + ' did not enroll the full reviewed matrix')
            cells = recipe.expand(name, csvid, declaration, model, variants, cls.rows, cls.oracle)
            cls.cases[name] = {cell['_recipe']['variant']: cell for cell in cells}

    def steps(self, name, mode, actor):
        return self.cases[name][f'{mode}_{actor}']['steps']

    def test_whole_oracle_and_independent_witness_pins(self):
        for name in self.cases:
            record = self.oracle[name]
            self.assertEqual(record['oracle_text'].splitlines(), recipe.mechanism(record)['clauses'])
            for text in (record['oracle_text'] + '\n{T}: Draw a card.',
                         record['oracle_text'].replace('unpaired creature', 'creature'),
                         record['oracle_text'].replace('as long as you control both', 'until end of turn for both')):
                with self.assertRaises(ValueError):
                    recipe.mechanism(dict(record, oracle_text=text))
        oracle = copy.copy(self.oracle)
        oracle['Suntail Hawk'] = dict(oracle['Suntail Hawk'], oracle_text='')
        with self.assertRaisesRegex(ValueError, 'Suntail Hawk'):
            recipe.validate_witnesses(oracle, self.rows)

    def test_source_and_partner_entry_choose_only_when_offered(self):
        for name in self.cases:
            for owner in (0, 1):
                source = self.steps(name, 'source_entry', owner)
                partner = self.steps(name, 'partner_entry', owner)
                intent = dict(op='choose_trigger', ref='source', player=0)
                pick = dict(op='choose_target', ref='partner', player=owner)
                self.assertEqual(intent in source, owner == 0)
                self.assertEqual(intent in partner, owner == 0)
                self.assertLess(source.index(dict(op='cast', ref='source', value=1)), source.index(pick))
                self.assertLess(source.index(pick), source.index(dict(op='resolve')))
                self.assertFalse(any(s['op'] == 'choose_target' for s in partner))
                if owner == 0:
                    self.assertLess(partner.index(dict(op='cast', ref='partner', value=1)), partner.index(intent))
                opposed = self.steps(name, 'enemy_entry', owner)
                self.assertFalse(any(s['op'].startswith('choose_') for s in opposed))

    def test_decline_and_shroud_share_actual_nontargeting_picker(self):
        for name in self.cases:
            for owner in (0, 1):
                steps = self.steps(name, 'source_decline', owner)
                self.assertIn(dict(op='choose_optional_target', ref='partner', player=owner), steps)
                self.assertIn(dict(op='assert_forced_target_choices', value=0), steps)
                shroud = self.cases[name][f'shroud_partner_{owner}']
                self.assertIn(dict(name='Deadly Insect', ref='partner', player=owner, zone='battlefield'), shroud['setup'])
                self.assertIn(dict(op='choose_target', ref='partner', player=owner), shroud['steps'])
            self.assertFalse(any(s['op'].startswith('choose_') for s in self.steps(name, 'partner_decline', 0)))

    def test_real_link_loss_control_and_new_object_entries(self):
        for name in self.cases:
            for owner in (0, 1):
                for ref in ('source', 'partner'):
                    if name != 'Elgaud Shieldmate':
                        transfer = self.steps(name, 'control_' + ref, owner)
                        self.assertIn(dict(op='choose_target', ref=ref, player=1-owner), transfer)
                        self.assertTrue(any(s['op'] == 'bind_permanent_copy' and s['player'] == 1-owner and
                                            s['ref'] == ref for s in transfer))
                    recast = self.steps(name, ref + '_reentry', owner)
                    rebound = next(i for i, s in enumerate(recast) if s['op'] == 'bind_hand_copy' and s['ref'] == ref)
                    self.assertIn(dict(op='cast', ref=ref, value=1), recast[rebound+1:])
                previous = self.steps(name, 'already_paired_only', owner)
                self.assertEqual([s['ref'] for s in previous if s['op'] == 'choose_target'], ['partner'])
                self.assertLess(previous.index(dict(op='cast', ref='bond_witness', value=1)),
                                previous.index(dict(op='cast', ref='source', value=1)))

    def test_stats_are_applied_only_to_the_pair(self):
        name = "Druid's Familiar"
        for owner in (0, 1):
            source = self.steps(name, 'source_entry', owner)
            for ref, power in (('source', 4), ('partner', 4), ('newcomer', 3)):
                self.assertIn(dict(op='assert_card', property='power', ref=ref, value=power), source)
            returned = self.steps(name, 'partner_reentry', owner)
            values = [s['value'] for s in returned if s['op'] == 'assert_card' and
                      s['property'] == 'power' and s['ref'] == 'source']
            self.assertEqual(values, [4, 2, 4])

    def test_four_grants_are_observed_as_real_combat(self):
        for owner in (0, 1):
            for paired, mode in ((1, 'source_entry'), (0, 'source_decline')):
                reach = self.steps('Geist Trappers', mode, owner)
                self.assertIn(dict(op='block', ref='source', other='flyer', value=paired), reach)
                haste = self.steps('Lightning Mauler', mode, owner)
                self.assertIn(dict(op='attack', ref='source', value=paired), haste)
                self.assertFalse(any(s['op'] == 'advance_turn' for s in haste))
                first_strike = self.steps('Hanweir Lancer', mode, owner)
                ending = first_strike.index(dict(op='end_combat'))
                self.assertEqual(dict(op='assert_card', ref='source', property='damage', value=0) in
                                 first_strike[ending:], bool(paired))
                lifelink = self.steps('Nearheath Pilgrim', mode, owner)
                self.assertIn(dict(op='assert_life', player=owner, value=20+2*paired), lifelink)
            partner = self.steps('Lightning Mauler', 'partner_entry', owner)
            self.assertIn(dict(op='attack', ref='partner', value=1), partner)
            self.assertFalse(any(s['op'] == 'advance_turn' for s in partner))

    def test_pending_replay_and_complete_combat_lifecycle(self):
        for name, cases in self.cases.items():
            for cell in cases.values():
                actions = cell['steps']
                ops = [s['op'] for s in actions]
                lifecycle = [op for op in ops if op in ('advance_combat', 'combat_damage', 'end_combat')]
                self.assertEqual(lifecycle, ['advance_combat', 'combat_damage', 'end_combat'] * ops.count('end_combat'))
                if cell['_recipe']['variant'].startswith('pending_replay_'):
                    self.assertLess(ops.index('cast'), ops.index('capture'))
                    self.assertEqual(actions[ops.index('capture')+1:ops.index('record_state')],
                                     actions[ops.index('ai_roundtrip')+2:ops.index('assert_recorded_state')])

    def at_combat_end(self, name, mode, player):
        actions = self.steps(name, mode, player)
        return actions[actions.index(dict(op='end_combat'))+1:]

    def test_single_damage_mechanics_and_grant_not_shared_with_others(self):
        for owner in (0, 1):
            for mode, granted in (('source_entry', True), ('partner_entry', True), ('source_decline', False),
                                  ('already_paired_entrant', False)):
                death = self.at_combat_end('Nightshade Peddler', mode, owner)
                wall = dict(op='assert_zone_multiplicity', player=1-owner, zone='graveyard', value=1,
                            index=self.rows['Dazzling Ramparts']['csvid'])
                self.assertEqual(wall in death, granted)
                trample = self.at_combat_end('Pathbreaker Wurm', mode, owner)
                life = 20 - ({'source_entry': 5, 'partner_entry': 1}.get(mode, 0))
                self.assertIn(dict(op='assert_life', player=1-owner, value=life), trample)
                vigilance = self.at_combat_end('Spectral Gateguards', mode, owner)
                recipient = {'partner_entry': 'partner', 'already_paired_entrant': 'newcomer'}.get(mode, 'source')
                self.assertIn(dict(op='assert_card', ref=recipient, property='tapped', value=int(not granted)),
                              vigilance)

    def test_double_strike_both_damage_steps_and_block_sticks(self):
        name = 'Silverblade Paladin'
        for owner in (0, 1):
            for mode, life in (('source_entry', 16), ('partner_entry', 16), ('source_decline', 18),
                               ('double_sole_blocker', 20), ('double_two_hits', 20)):
                self.assertIn(dict(op='assert_life', player=1-owner, value=life),
                              self.at_combat_end(name, mode, owner))
            self.assertIn(dict(op='assert_card', ref='blocker', property='damage', value=4),
                          self.at_combat_end(name, 'double_two_hits', owner))
            self.assertIn(dict(op='assert_card', ref='source', property='damage', value=0),
                          self.at_combat_end(name, 'double_sole_blocker', owner))

    def test_flying_respects_reach_and_normal_blocking_after_pair_loss(self):
        for owner in (0, 1):
            for mode, ref, legal in (('source_entry', 'source', 0), ('partner_entry', 'partner', 0),
                                     ('partner_leave', 'source', 1), ('flying_reach', 'source', 1)):
                actions = self.steps('Wingcrafter', mode, owner)
                self.assertIn(dict(op='block', ref='blocker', other=ref, value=legal), actions)
            reach = self.cases['Wingcrafter'][f'flying_reach_{owner}']['setup']
            self.assertIn(dict(name='Giant Spider', ref='blocker', player=1-owner, zone='battlefield'), reach)

    def test_hexproof_source_and_partner_block_only_opposing_targets(self):
        name = 'Elgaud Shieldmate'
        for owner in (0, 1):
            rejected = self.steps(name, 'hexproof_rejects_control', owner)
            self.assertIn(dict(op='cast', ref='response', value=0), rejected)
            self.assertNotIn('choose_target', [s['op'] for s in rejected if s.get('player') == 1-owner])
            for ref in ('source', 'partner'):
                self.assertIn(dict(op='can_target_creature', ref='owner_probe', other=ref, value=1), rejected)
                self.assertIn(dict(op='can_target_creature', ref='foe_probe', other=ref, value=0), rejected)
                stages = self.steps(name, 'hexproof_mass_control', owner)
                moves = [s['player'] for s in stages if s['op'] == 'bind_permanent_copy' and s['ref'] == ref]
                self.assertEqual(moves, [1-owner, owner])
                probes = [s for s in stages if s['op'] == 'can_target_creature' and s.get('other') == ref]
                self.assertEqual([s['value'] for s in probes], [1, 1, 1, 1])
            own_bounce = self.steps(name, 'partner_leave', owner)
            self.assertEqual([s['player'] for s in own_bounce if s['op'] == 'choose_target' and s['ref'] == 'partner'],
                             [owner, owner])


if __name__ == '__main__':
    unittest.main()
