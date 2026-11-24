"""Focused structural guards for actual cast-trigger tests."""

import unittest

import cast_instant_or_sorcery_riders_recipes as recipe


class CastInstantOrSorceryTests(unittest.TestCase):
    def case(self, name, mode, owner=0):
        pins = recipe.profiles()['sources'] | recipe.profiles()['witnesses']
        oracle = {name: pin['oracle'] for name, pin in pins.items()}
        rows = {name: pin['registry'] for name, pin in pins.items()}
        model = recipe.classify(oracle[name])
        declaration = dict(family=recipe.FAMILY, version=2, oracle_text=oracle[name]['oracle_text'],
                           params=model, variants=[f'{mode}_{owner}'])
        return recipe.expand(name, rows[name]['csvid'], declaration, model, declaration['variants'], rows, oracle)[0]

    def items(self, case, op, **fields):
        return [step for step in case['steps'] if step['op'] == op and
                all(step.get(key) == value for key, value in fields.items())]

    def values(self, case, op, **fields):
        return [step.get('value') for step in self.items(case, op, **fields)]

    def test_all_printed_shapes_reject_modified_text(self):
        for pin in recipe.profiles()['sources'].values():
            record = pin['oracle']
            with self.assertRaises(ValueError):
                recipe.classify(record | {'oracle_text': record['oracle_text'] + '\nFlying'})

    def test_paid_modes_notify_before_counter_response(self):
        for name in recipe.profiles()['sources']:
            for owner in (0, 1):
                case = self.case(name, 'countered', owner)
                steps = case['steps']
                self.assertLess(steps.index(self.items(case, 'cast', ref='spell')[0]),
                                steps.index(self.items(case, 'announce_spell', ref='spell')[0]))
                self.assertLess(steps.index(self.items(case, 'announce_spell', ref='spell')[0]),
                                steps.index(self.items(case, 'cast_response', ref='counter')[0]))
                self.assertEqual(self.values(case, 'assert_card', ref='spell', property='color'), [4])

    def test_untap_is_caused_by_actual_own_spell(self):
        for owner in (0, 1):
            positive = self.case('Blistercoil Weird', 'instant', owner)
            negative = self.case('Blistercoil Weird', 'enemy_instant', owner)
            self.assertEqual(self.values(positive, 'cast', ref='tap_spell'), [1])
            self.assertEqual(self.values(positive, 'assert_card', ref='source', property='tapped'), [1, 0, 0])
            self.assertEqual(self.values(negative, 'assert_card', ref='source', property='tapped'), [1, 1, 1])
            self.assertEqual(self.values(positive, 'assert_card', ref='source', property='power')[-1], 2)
            self.assertEqual(self.values(self.case('Blistercoil Weird','expiry',owner), 'assert_card',
                                         ref='source',property='power')[-1], 1)

    def test_paid_scry_uses_only_legal_prefix_and_actual_fee(self):
        for owner in (0, 1):
            case = self.case('Eyes of the Watcher', 'instant', owner)
            unpaid = self.case('Eyes of the Watcher', 'unfunded', owner)
            self.assertEqual(len(self.items(case,'choose_zone_card',player=owner)),2)
            self.assertEqual(self.values(case,'assert_scry_result',player=owner),[2])
            self.assertTrue(any(step['value'] == 1 for step in self.items(case,'assert_mana',player=owner,color=0)))
            self.assertFalse(self.items(unpaid,'choose_zone_card'))
            self.assertFalse(self.items(unpaid,'assert_scry_result'))

    def test_optional_switch_and_real_flying(self):
        decline = self.case('Fluxcharger', 'decline')
        self.assertEqual(self.values(decline,'assert_card',ref='source',property='power')[-1],1)
        self.assertFalse(self.items(decline,'choose_trigger_source'))
        for owner in (0, 1):
            switch = self.case('Fluxcharger', 'double', owner)
            self.assertEqual(self.values(switch,'assert_card',ref='source',property='power'),[5,1,1])
            flying = self.case('Fluxcharger', 'flying', owner)
            self.assertEqual(self.values(flying,'block',ref='ground'),[0])
            self.assertEqual(self.values(flying,'block',ref='air'),[1])

    def test_wand_charge_and_target_classes(self):
        for owner in (0,1):
            case = self.case('Sphinx-Bone Wand', 'double', owner)
            self.assertEqual(self.values(case,'assert_counter',ref='source',counter_kind='charge'),[1,2,2])
            self.assertEqual(self.values(case,'assert_life',player=1-owner),[17])
            walker = self.case('Sphinx-Bone Wand', 'walker_target', owner)
            self.assertEqual(self.values(walker,'assert_counter',ref='victim',counter_kind='loyalty'),[3,2])

    def test_clue_uses_paid_sacrifice_and_actual_draw(self):
        for owner in (0,1):
            paid = self.case('Trail of Evidence', 'clue_paid', owner)
            denied = self.case('Trail of Evidence', 'clue_short', owner)
            self.assertEqual(self.values(paid,'activate',ref='clue_0'),[1])
            self.assertEqual(self.values(denied,'activate',ref='clue_0'),[0])
            self.assertEqual(self.values(paid,'assert_zone',zone='library',player=owner),[8])
            self.assertEqual(self.values(denied,'assert_zone',zone='library',player=owner),[9])
            self.assertTrue(self.items(paid,'assert_matching_permanents',property='subtype_Clue'))

    def test_charmbreaker_uses_actual_controller_upkeep_and_type(self):
        for owner in (0,1):
            own = self.case('Charmbreaker Devils','upkeep',owner)
            wrong = self.case('Charmbreaker Devils','upkeep_wrong_type',owner)
            self.assertEqual([s['player'] for s in self.items(own,'upkeep_step')],[1-owner,owner])
            self.assertEqual(self.values(own,'assert_zone',player=owner,zone='graveyard'),[1])
            self.assertEqual(self.values(wrong,'assert_zone',player=owner,zone='hand'),[1])

    def test_copies_are_not_casts_and_reset_after_real_turns(self):
        for owner in (0,1):
            for label in ('Mirari','Riku of Two Reflections','Swarm Intelligence'):
                retarget = self.case(label,'retarget',owner)
                self.assertEqual(self.values(retarget,'choose_player',player=owner),[1-owner,owner])
                self.assertEqual(self.values(retarget,'assert_life',player=owner),[17])
                self.assertEqual(self.values(retarget,'assert_life',player=1-owner),[17])
                self.assertEqual(len(self.items(retarget,'announce_spell',ref='spell')),1)
            storm = self.case('Thousand-Year Storm','storm_three',owner)
            reset = self.case('Thousand-Year Storm','storm_reset',owner)
            self.assertEqual(self.values(storm,'assert_zone',player=owner,zone='library'),[4])
            self.assertEqual(self.values(reset,'assert_zone',player=owner,zone='library'),[6])

    def test_riku_real_nontoken_entry_paid_and_short(self):
        for owner in (0,1):
            yes = self.case('Riku of Two Reflections','copy_entry',owner)
            no = self.case('Riku of Two Reflections','entry_short',owner)
            self.assertEqual(self.values(yes,'bind_permanent_copy',ref='bear_copy_0'),[106])
            self.assertTrue(self.items(yes,'assert_matching_permanents',property='token'))
            self.assertFalse(self.items(no,'bind_permanent_copy'))
            self.assertEqual(self.values(no,'cast',ref='spell'),[1])

    def test_defender_permission_only_last_this_turn(self):
        for owner in (0,1):
            name = 'Nivix Cyclops'
            paid = self.case(name,'attack_enabled',owner)
            self.assertEqual(self.values(paid,'attack',ref='source'),[1])
            self.assertEqual(self.values(paid,'assert_life',player=1-owner),[16])
            for scene in ('defender_only','attack_expiry'):
                self.assertEqual(self.values(self.case(name,scene,owner),'attack',ref='source'),[0])

    def test_land_animate_with_real_new_land_and_mana(self):
        for owner in (0,1):
            name = 'Noyan Dar, Roil Shaper'
            fresh = self.case(name,'fresh_land_attack',owner)
            self.assertEqual(self.values(fresh,'play_land',ref='land'),[1])
            self.assertEqual(self.values(fresh,'attack',ref='land'),[1])
            self.assertEqual(self.values(fresh,'assert_card',ref='land',property='power'),[4,4])
            self.assertEqual(self.values(self.case(name,'later_land_mana',owner),'activate',ref='land'),[1])
            self.assertFalse(self.items(self.case(name,'no_own_land',owner),'choose_target',ref='land'))

    def test_energy_and_cast_counter_receipts(self):
        for owner in (0,1):
            tower = self.case('Dynavolt Tower','spend_energy',owner)
            self.assertEqual(self.values(tower,'assert_energy',player=owner),[2,4,6,1,1])
            self.assertEqual(self.values(tower,'assert_life',player=1-owner),[17])
            combat = self.case('Surrakar Spellblade','scaled_combat',owner)
            blocked = self.case('Surrakar Spellblade','blocked_draw',owner)
            self.assertEqual(self.values(combat,'assert_zone',player=owner,zone='library'),[6])
            self.assertEqual(self.values(blocked,'assert_zone',player=owner,zone='library'),[8])

    def test_cold_expiry_and_independent_prowess(self):
        for owner in (0,1):
            name = 'Niblis of Frost'
            frozen = self.case(name,'freeze_expires',owner)
            self.assertEqual(self.values(frozen,'assert_card',ref='victim',property='tapped'),[1,1,0,0])
            artifact = self.case(name,'artifact_prowess',owner)
            self.assertEqual(self.values(artifact,'assert_card',ref='victim',property='tapped'),[0,0])
            self.assertEqual(self.values(artifact,'assert_card',ref='source',property='power'),[4,4])

    def test_construct_activation_gate_and_wizard_fresh_attack(self):
        for owner in (0,1):
            name = 'Metallurgic Summonings'
            activate = self.case(name,'reclaim',owner)
            reject = self.case(name,'below_reclaim',owner)
            self.assertEqual(self.values(activate,'activate',ref='source'),[1])
            self.assertEqual(self.values(reject,'activate',ref='source'),[0])
            self.assertTrue(self.items(activate,'assert_matching_permanents',property='subtype_Construct'))
            wizard = self.case('Adeliz, the Cinder Wind','flying_haste',owner)
            self.assertEqual(self.values(wizard,'attack',ref='source'),[1])
            self.assertEqual(self.values(wizard,'block',ref='ground'),[0])
            self.assertEqual(self.values(wizard,'assert_card',ref='enemy_wizard',property='power'),[1,1])
