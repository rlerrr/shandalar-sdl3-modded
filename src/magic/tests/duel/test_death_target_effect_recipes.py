import copy
import json
from pathlib import Path
import unittest

from generate_cases import registry
from recipes import COLORS, expand as public_expand
import death_target_effect_recipes as family


class DeathTargetEffectTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.oracle = json.loads((Path(__file__).parent/'data/oracle.json').read_text(encoding='utf8'))['cards']
        cls.rows = {r['name']: r for r in registry()}
        cls.pins = family.pins()

    def declaration(self, name, selected):
        return dict(family=family.FAMILY, version=2, params=dict(schema=11, **family.oracle_model(self.oracle[name])),
                    oracle_text=self.oracle[name]['oracle_text'], variants=selected)

    def case(self, name, mode='effect', player=0):
        d = self.declaration(name, [f'{mode}_{player}'])
        return public_expand(name, self.rows[name]['csvid'], d, self.oracle[name], self.rows, self.oracle)[0]

    def has(self, rows, **fields):
        return any(all(r.get(k) == v for k, v in fields.items()) for r in rows)

    def count(self, rows, name, player, zone, value):
        return self.has(rows, op='assert_zone_multiplicity', player=player, zone=zone,
                        index=self.rows[name]['csvid'], value=value)

    def after_cast(self, case, ref):
        s = case['steps']
        return s[next(i for i, x in enumerate(s) if x['op'] == 'cast' and x['ref'] == ref)+1:]

    def test_complete_typed_oracle_and_mandatory_targets_fail_closed(self):
        for name in self.pins['sources']:
            d = self.declaration(name, ['effect_0']); family.spec(d, self.oracle[name])
            self.assertIs(d['params']['mandatory_target'], True)
            for key, value in [('schema', 10), ('mandatory_target', False), ('optional_effect', 1), ('amount', True)]:
                bad = copy.deepcopy(d); bad['params'][key] = value
                with self.assertRaises(ValueError):
                    family.spec(bad, self.oracle[name])
            with self.assertRaises(ValueError):
                family.oracle_model(dict(self.oracle[name], oracle_text=self.oracle[name]['oracle_text']+'\n{T}: Draw a card.'))
        for name in ('Ashen Rider', 'Keiga, the Tide Star', 'Grief Tyrant', 'Necropede'):
            with self.assertRaises(ValueError):
                family.oracle_model(self.oracle[name])

    def test_full_implemented_source_and_witness_pins(self):
        family.validate_witnesses(self.oracle, self.rows)
        for name, field, value in [('Task Force', 'oracle_text', 'This creature gets +0/+3.'), ('Bogbrew Witch', 'type_line', 'Enchantment'), ('Memnite', 'mana_cost', '{3}')]:
            o = copy.deepcopy(self.oracle); o[name][field] = value
            with self.assertRaises(ValueError):
                family.validate_witnesses(o, self.rows)
        rows = copy.deepcopy(self.rows); rows['Noxious Dragon']['handler'] = 'card_dummy'
        with self.assertRaises(ValueError):
            family.validate_witnesses(self.oracle, rows)

    def test_real_parent_target_then_callback_target_order(self):
        for name in self.pins['sources']:
            for player in (0, 1):
                c = self.case(name, player=player); s = c['steps']
                cast = next(i for i, x in enumerate(s) if x['op'] == 'cast' and x['ref'] == 'death_spell')
                self.assertTrue(self.has(s[:cast], op='choose_target', ref='source', player=1-player))
                self.assertTrue(self.has(s[cast:], op='choose_target', ref='recipient', player=player))
                self.assertFalse(self.has(s, op='choose_trigger_source'))
                self.assertEqual(sum(x['op'] == 'choose_trigger' for x in s), int(name == 'Noxious Dragon' and player == 0))
                self.assertTrue(self.count(s, name, player, 'graveyard', 1))

    def test_source_and_recipient_ownership_are_independent_of_chooser(self):
        for name in self.pins['sources']:
            for player in (0, 1):
                c = self.case(name, 'source_foreign_owner', player); s = c['steps']
                self.assertIn('opponent_owned', next(x['state'] for x in c['setup'] if x['ref'] == 'source'))
                self.assertTrue(self.has(s, op='choose_target', ref='recipient', player=player))
                self.assertTrue(self.count(s, name, 1-player, 'graveyard', 1))
                c = self.case(name, 'target_foreign_owner', player)
                self.assertIn('opponent_owned', next(x['state'] for x in c['setup'] if x['ref'] == 'recipient'))

    def test_actual_target_event_receipt_is_not_just_counter_application(self):
        expected = {'Sparring Construct': (2, 7), 'Elder Cathar': (3, 8), 'Festering Mummy': (0, 5), 'Festering Goblin': (0, 5), 'Festering Newt': (0, 5)}
        for name, (power, toughness) in expected.items():
            for player in (0, 1):
                c = self.case(name, 'target_event', player); s = c['steps']
                self.assertEqual(next(x['name'] for x in c['setup'] if x['ref'] == 'recipient'), 'Task Force')
                self.assertTrue(self.has(s, op='choose_target', ref='recipient', player=player))
                self.assertTrue(self.has(s, op='assert_card', ref='recipient', property='power', value=power))
                self.assertTrue(self.has(s, op='assert_card', ref='recipient', property='toughness', value=toughness))
                self.assertFalse(any(x['op'] in ('destroy', 'set_life', 'set_card_available') for x in s))

    def test_optional_effect_decline_retains_required_target_and_taskforce_receipt(self):
        for name in ('Festering Mummy', 'Noxious Dragon'):
            c = self.case(name, 'decline_target_event'); s = c['steps']
            self.assertTrue(self.has(s, op='choose_optional_target', ref='recipient', player=0))
            self.assertTrue(self.has(s, op='assert_forced_target_choices', value=1))
            self.assertTrue(self.has(s, op='assert_card', ref='recipient', property='toughness', value=6))
            self.assertTrue(self.has(s, op='assert_card', ref='recipient', property='minus1_counters', value=0))
            self.assertTrue(self.count(s, 'Task Force', 1, 'battlefield', 1))
            self.assertFalse(self.has(s, op='choose_cancel_target'))

    def test_mandatory_picker_uses_legal_fallback_policy_receipt(self):
        for name in ('Sparring Construct', 'Festering Goblin', 'Archon of Justice', 'Reliquary Monk'):
            s = self.case(name, 'mandatory_policy')['steps']
            self.assertTrue(self.has(s, op='choose_optional_target', ref='recipient', player=0))
            self.assertTrue(self.has(s, op='assert_forced_target_choices', value=1))
            self.assertFalse(self.has(s, op='choose_cancel_target'))

    def test_human_and_changeling_conditional_is_target_type(self):
        for mode, counters, power in [('human_target', 2, 3), ('changeling_target', 2, 4), ('nonhuman_target', 1, 3)]:
            s = self.case('Elder Cathar', mode)['steps']
            self.assertTrue(self.has(s, op='assert_card', ref='recipient', property='plus1_counters', value=counters))
            self.assertTrue(self.has(s, op='assert_card', ref='recipient', property='power', value=power))

    def test_witch_condition_uses_actual_controlled_battlefield_creature(self):
        for player in (0, 1):
            for mode, power, toughness in [('witch', 3, 2), ('witch_foreign', 3, 2), ('opponent_witch', 6, 5), ('witch_removed', 6, 5), ('witch_bounced', 6, 5)]:
                c = self.case('Festering Newt', mode, player); s = c['steps']
                self.assertTrue(self.has(s, op='assert_card', ref='recipient', property='power', value=power))
                self.assertTrue(self.has(s, op='assert_card', ref='recipient', property='toughness', value=toughness))
                if mode in ('witch_removed', 'witch_bounced'):
                    removal = next(i for i, x in enumerate(s) if x['op'] == 'cast' and x['ref'] == 'witch_removal')
                    death = next(i for i, x in enumerate(s) if x['op'] == 'cast' and x['ref'] == 'death_spell')
                    self.assertLess(removal, death)
                    self.assertTrue(self.count(s, 'Bogbrew Witch', player, 'graveyard' if mode == 'witch_removed' else 'hand', 1))

    def test_mana_value_boundary_and_token_have_printed_zero_cost(self):
        for player in (0, 1):
            for mode, selected in [('mv_zero', True), ('mv_boundary', True), ('mv_above', False), ('target_token', True)]:
                c = self.case('Noxious Dragon', mode, player); s = c['steps']
                receiver = next(x for x in c['setup'] if x['ref'] == 'recipient')
                self.assertEqual(self.has(s, op='choose_target', ref='recipient', player=player), selected)
                if mode in ('mv_zero', 'target_token'):
                    self.assertEqual(receiver['name'], 'Memnite')
                    self.assertEqual(self.oracle[receiver['name']]['cmc'], 0)
                if mode == 'mv_above':
                    self.assertEqual(receiver['name'], 'Hill Giant')
                    self.assertTrue(self.count(s, 'Hill Giant', 1-player, 'battlefield', 1))

    def test_indestructible_does_not_stop_exile_or_zero_toughness(self):
        for name, zone, count in [('Reliquary Monk', 'battlefield', 1), ('Noxious Dragon', 'battlefield', 1),
                                  ('Archon of Justice', 'exile', 1), ('Festering Goblin', 'graveyard', 1), ('Festering Mummy', 'graveyard', 1)]:
            s = self.case(name, 'target_indestructible')['steps']
            self.assertTrue(self.count(s, 'Darksteel Myr', 1, zone, count))

    def test_empty_invalid_and_shroud_cases_do_not_force_illegal_choices(self):
        for name in self.pins['sources']:
            supported = family.variants(family.oracle_model(self.oracle[name]))
            for mode in ('empty_targets', 'shroud_only' if 'shroud_only_0' in supported else 'invalid_only'):
                s = self.case(name, mode)['steps']
                self.assertFalse(self.has(s, op='choose_target', ref='recipient'))
                self.assertFalse(self.has(s, op='choose_trigger'))
                self.assertFalse(self.has(s, op='choose_optional_target'))
                self.assertTrue(self.count(s, name, 0, 'graveyard', 1))

    def test_colorless_generic_cost_accepts_colored_mana(self):
        for player in (0, 1):
            c = self.case('Sparring Construct', 'generic_colored_payment', player); s = c['steps']
            self.assertTrue(self.has(s, op='set_mana', player=player, color=COLORS['B'], value=1))
            self.assertTrue(self.has(s, op='cast', ref='source', value=1))
            self.assertTrue(self.has(s, op='assert_mana', player=player, color=COLORS['B'], value=0))
            cast = next(i for i, x in enumerate(s) if x['op'] == 'cast' and x['ref'] == 'source')
            self.assertEqual(sum(x['op'] == 'choose_mana' for x in s), int(player == 0))
            if player == 0:
                self.assertEqual(s[cast-1], dict(op='choose_mana', player=0, color=COLORS['B']))
        self.assertNotIn('wrong_color_0', family.variants(self.declaration('Sparring Construct', ['effect_0'])['params']))

    def test_two_actual_sources_have_two_effects_and_no_ambiguous_live_selector(self):
        for name in self.pins['sources']:
            c = self.case(name, 'two_sources'); s = c['steps']
            self.assertTrue(self.has(s, op='cast', ref='source_second', value=1))
            self.assertTrue(self.has(s, op='cast', ref='second_death', value=1))
            self.assertTrue(self.count(s, name, 0, 'graveyard', 2))
            self.assertTrue(self.has(s, op='choose_target', ref='recipient_second', player=0))
            self.assertFalse(self.has(s, op='choose_trigger_source'))

    def test_temporary_cleanup_and_real_opposite_counter_cancellation(self):
        c = self.case('Festering Goblin', 'cleanup'); s = c['steps']
        at = next(i for i, x in enumerate(s) if x['op'] == 'cleanup')
        self.assertTrue(self.has(s[:at], op='assert_card', ref='recipient', property='toughness', value=1))
        self.assertTrue(self.has(s[at:], op='assert_card', ref='recipient', property='toughness', value=2))
        for name, spell, plus in [('Sparring Construct', 'Virulent Wound', 0), ('Festering Mummy', 'Dragonscale Boon', 1)]:
            c = self.case(name, 'counter_cancel'); s = c['steps']
            self.assertEqual(next(x['name'] for x in c['setup'] if x['ref'] == 'cancel_counter'), spell)
            tail = self.after_cast(c, 'cancel_counter')
            self.assertTrue(self.has(tail, op='assert_card', ref='recipient', property='plus1_counters', value=plus))
            self.assertTrue(self.has(tail, op='assert_card', ref='recipient', property='minus1_counters', value=0))

    def test_source_negatives_and_simultaneous_target_do_not_callback(self):
        for name in self.pins['sources']:
            for mode in ('source_bounced', 'source_exiled', 'source_blinked', 'other_dies', 'target_simultaneous'):
                if mode+'_0' not in family.variants(family.oracle_model(self.oracle[name])):
                    continue
                s = self.case(name, mode)['steps']
                self.assertFalse(self.has(s, op='choose_target', ref='recipient'))
                self.assertFalse(self.has(s, op='choose_trigger'))
                if mode == 'target_simultaneous':
                    self.assertTrue(self.has(s, op='cast', ref='sweep', value=1))

    def test_real_flying_combat_and_pending_snapshot_choice_boundaries(self):
        for name in ('Archon of Justice', 'Noxious Dragon'):
            for player in (0, 1):
                for mode, legal in [('flying_combat', 0), ('reach_combat', 1)]:
                    c = self.case(name, mode, player); s = c['steps']
                    self.assertTrue(self.has(s, op='block', ref='blocker', other='source', value=legal))
                    self.assertEqual(sum(x['op'] == 'advance_turn' for x in s), 2)
                    self.assertTrue(self.has(s, op='untap_step', player=player))
                    self.assertTrue(self.has(s, op='combat_damage'))
        for name in self.pins['sources']:
            s = self.case(name, 'death_snapshot')['steps']; at = next(i for i, x in enumerate(s) if x['op'] == 'capture')
            self.assertEqual(s[at+1]['op'], 'choose_trigger' if name == 'Noxious Dragon' else 'choose_target')
            self.assertEqual(s[-1], dict(op='assert_recorded_state'))

    def test_all_pilot_variants_expand_with_full_oracle_and_no_fake_events(self):
        totals = {}
        for name in self.pins['sources']:
            d = self.declaration(name, ['effect_0']); d['variants'] = family.variants(d['params'])
            cases = public_expand(name, self.rows[name]['csvid'], d, self.oracle[name], self.rows, self.oracle)
            self.assertEqual(len(cases), len(set(d['variants'])))
            self.assertTrue(all(c['_recipe']['oracle_text'] == self.oracle[name]['oracle_text'] for c in cases))
            self.assertFalse(any(s['op'] in ('destroy', 'set_card_available', 'set_life') for c in cases for s in c['steps']))
            totals[self.rows[name]['csvid']] = len(cases)
        self.assertEqual(sum(v for csvid, v in totals.items() if csvid in (18513,11936,16907,6142,13287,9734,4178,14597)), 546)
        if 12441 in totals:
            self.assertEqual(sum(v for csvid, v in totals.items() if csvid in (16430,19488,15016,12498,2882,4134,12441,14537)), 491)

    def test_follow_land_and_noncreature_tokens_are_explicit_copies(self):
        for name, witness, mv in [('Polluted Dead', 'Forest', 0), ('Destructor Dragon', 'Sol Ring', 1)]:
            for player in (0, 1):
                c = self.case(name, 'target_token', player); s = c['steps']
                target = next(x for x in c['setup'] if x['ref'] == 'recipient')
                self.assertEqual(target['name'], witness)
                self.assertIn('token', target['state'])
                annotation = c['_recipe']['token_copy_fixture']
                self.assertEqual(annotation['copied_mana_value'], mv)
                self.assertEqual(annotation['copied_type_line'], self.oracle[witness]['type_line'])
                self.assertIs(annotation['creation_tested'], False)
                self.assertTrue(self.has(s, op='choose_target', ref='recipient', player=player))
                self.assertTrue(self.count(s, witness, 1-player, 'battlefield', 0))
                self.assertTrue(self.count(s, witness, 1-player, 'graveyard', 1))

    def test_noncreature_negative_is_an_artifact_creature_and_no_legal_board_target(self):
        for player in (0, 1):
            c = self.case('Destructor Dragon', 'invalid_only', player); s = c['steps']
            board = [x for x in c['setup'] if x['zone'] == 'battlefield']
            self.assertEqual([x['name'] for x in board], ['Memnite'])
            self.assertIn('Artifact Creature', self.oracle['Memnite']['type_line'])
            self.assertFalse(self.has(s, op='choose_target', ref='recipient'))
            self.assertTrue(self.count(s, 'Memnite', 1-player, 'battlefield', 1))
            self.assertTrue(self.count(s, 'Destructor Dragon', player, 'graveyard', 1))

    def test_amount_two_cleanup_and_blink_use_real_surviving_receiver(self):
        for player in (0, 1):
            c = self.case('Undead Executioner', 'cleanup', player); s = c['steps']
            self.assertEqual(next(x['name'] for x in c['setup'] if x['ref'] == 'recipient'), 'Centaur Courser')
            at = next(i for i, x in enumerate(s) if x['op'] == 'cleanup')
            self.assertTrue(self.has(s[:at], op='assert_card', ref='recipient', property='power', value=1))
            self.assertTrue(self.has(s[:at], op='assert_card', ref='recipient', property='toughness', value=1))
            self.assertTrue(self.has(s[at:], op='assert_card', ref='recipient', property='toughness', value=3))
            c = self.case('Undead Executioner', 'target_blinked', player); s = c['steps']
            self.assertTrue(self.has(s, op='cast', ref='blink', value=1))
            self.assertTrue(self.has(s, op='bind_permanent', ref='recipient_new', value=self.rows['Centaur Courser']['csvid']))
            self.assertTrue(self.has(s, op='assert_card', ref='recipient_new', property='toughness', value=3))
            lethal = self.case('Undead Executioner', 'effect', player)
            self.assertEqual(next(x['name'] for x in lethal['setup'] if x['ref'] == 'recipient'), 'Grizzly Bears')
            self.assertTrue(self.count(lethal['steps'], 'Grizzly Bears', 1-player, 'graveyard', 2))

    def test_new_source_presentations_preserve_mandatory_targets_and_optional_resolution(self):
        for name in ('Polluted Dead', 'Destructor Dragon', 'Undead Executioner'):
            for player in (0, 1):
                s = self.case(name, 'effect', player)['steps']
                self.assertFalse(self.has(s, op='choose_trigger'))
                death = next(i for i, x in enumerate(s) if x['op'] == 'cast' and x['ref'] == 'death_spell')
                self.assertTrue(self.has(s[death:], op='choose_target', ref='recipient', player=player))
        s = self.case('Undead Executioner', 'decline_target_event')['steps']
        self.assertTrue(self.has(s, op='choose_optional_target', ref='recipient', player=0))
        self.assertTrue(self.has(s, op='assert_forced_target_choices', value=1))
        self.assertTrue(self.has(s, op='assert_card', ref='recipient', property='toughness', value=6))


if __name__ == '__main__':
    unittest.main()
