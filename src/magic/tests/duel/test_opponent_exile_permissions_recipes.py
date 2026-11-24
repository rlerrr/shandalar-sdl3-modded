"""Focused generation guards for face-up opposing exile permissions."""

from copy import deepcopy
import unittest

import opponent_exile_permissions_recipes as model


class OpponentExilePermissionGuards(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        profile = model.profile()
        cls.sources = profile['sources']
        records = cls.sources | profile['witnesses']
        cls.oracle = {name: entry['oracle'] for name, entry in records.items()}
        cls.rows = {name: entry['registry'] for name, entry in records.items()}

    def case(self, name, variant):
        record = self.oracle[name]
        params = model.parameters(record)
        decl = dict(family=model.FAMILY, version=2, oracle_text=record['oracle_text'], params=params, variants=[variant])
        typed = model.spec(decl, record)
        return model.expand(name, self.rows[name]['csvid'], decl, typed, [variant], self.rows, self.oracle)[0]

    def test_exact_full_oracle_including_goad(self):
        name = 'Grenzo, Havoc Raiser'
        modified = deepcopy(self.oracle[name])
        modified['oracle_text'] = modified['oracle_text'].replace('• Goad target creature that player controls.\n', '')
        with self.assertRaises(ValueError):
            model.parameters(modified)

    def test_exact_live_witness_pin(self):
        rows = deepcopy(self.rows)
        rows[model.DRAW]['handler'] = 'card_dummy'
        with self.assertRaises(ValueError):
            model.validate_witnesses(self.oracle, rows)

    def test_own_upkeep_and_true_owner(self):
        for actor in (0, 1):
            case = self.case('Stolen Strategy', f'alternate_color_{actor}')
            self.assertEqual([s['player'] for s in case['steps'] if s['op']=='upkeep_step'], [actor])
            played = [s for s in case['steps'] if s['op']=='play_exiled' and s['value']]
            self.assertEqual([(s['player'],s['owner']) for s in played], [(actor,1-actor)])
            self.assertIn(dict(op='set_mana', player=actor, color=4, value=1), case['steps'])

    def test_goad_mode_has_actual_attack_omission_receipt_and_expiry(self):
        steps = self.case('Grenzo, Havoc Raiser', 'goad_expiry_1')['steps']
        self.assertEqual([s['value'] for s in steps if s['op']=='can_finish_attackers'], [0,1,1,1])
        self.assertIn(dict(op='assert_unused_choices', value=0), steps)
        self.assertEqual(sum(s['op']=='advance_turn' for s in steps), 3)

    def test_stolen_goods_free_and_real_owner_graveyard(self):
        steps = self.case('Stolen Goods', 'land_prefix_0')['steps']
        cast = next(i for i,s in enumerate(steps) if s['op']=='play_exiled' and s['value']==1)
        self.assertTrue(all(s['value']==0 for s in steps[:cast] if s['op']=='set_mana' and s['color']==4))
        for land in ('Forest','Island'):
            self.assertIn(dict(op='assert_zone_multiplicity',player=1,zone='exile',index=self.rows[land]['csvid'],value=1),steps)
        self.assertIn(dict(op='assert_zone_multiplicity',player=1,zone='graveyard',index=self.rows[model.DRAW]['csvid'],value=1),steps[cast:])

    def test_thada_land_play_gated_and_printed_shuffle(self):
        steps = self.case('Thada Adel, Acquisitor', 'land_spent_0')['steps']
        spent = next(i for i,s in enumerate(steps) if s['op']=='play_land')
        self.assertIn(dict(op='assert_shuffle_count',player=1,value=1),steps[:spent])
        self.assertTrue(all(s['value']==0 for s in steps[spent:] if s['op']=='play_exiled'))

    def test_psychic_chooses_grave_after_legal_hand_decline(self):
        steps = self.case('Psychic Intrusion', 'decline_hand_1')['steps']
        self.assertEqual([s['op'] for s in steps if s['op'] in ('choose_zone','choose_zone_card')],
                         ['choose_zone','choose_zone_card'])
        self.assertIn(dict(op='assert_offered_legal_card',index=self.rows['Mountain']['csvid'],value=0),steps)

    def test_hostage_uses_different_color_and_return_releases_permission(self):
        fees = self.case('Hostage Taker', 'alternate_color_0')['steps']
        self.assertIn(dict(op='set_mana',player=0,color=3,value=3),fees)
        return_steps = self.case('Hostage Taker', 'return_1')['steps']
        self.assertEqual([s['value'] for s in return_steps if s['op']=='can_play_exiled'],[1,0])
        self.assertIn(dict(op='choose_target',player=1,ref='source'),return_steps)


if __name__ == '__main__':
    unittest.main()
