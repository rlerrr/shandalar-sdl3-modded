import copy
import json
from pathlib import Path
import unittest

from generate_cases import registry
from recipes import expand as public_expand
import death_player_value_recipes as family


class DeathPlayerValueTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.oracle = json.loads((Path(__file__).parent/'data/oracle.json').read_text(encoding='utf8'))['cards']
        cls.rows = {r['name']: r for r in registry()}
        cls.data = family.pins()

    def declaration(self, name, selected):
        return dict(family=family.FAMILY, version=2, params=dict(schema=1, **family.oracle_model(self.oracle[name])),
                    oracle_text=self.oracle[name]['oracle_text'], variants=selected)

    def case(self, name, mode='effect', player=0):
        d = self.declaration(name, [f'{mode}_{player}'])
        return public_expand(name, self.rows[name]['csvid'], d, self.oracle[name], self.rows, self.oracle)[0]

    def has(self, steps, **fields):
        return any(all(s.get(k) == v for k, v in fields.items()) for s in steps)

    def final_life(self, case, player):
        return [s['value'] for s in case['steps'] if s['op'] == 'assert_life' and s['player'] == player][-1]

    def multiplicity(self, case, name, player, zone, expected):
        return self.has(case['steps'], op='assert_zone_multiplicity', player=player, zone=zone,
                        index=self.rows[name]['csvid'], value=expected)

    def test_full_oracle_model_rejects_extra_clauses_and_semantic_mutations(self):
        for name in self.data['sources']:
            d = self.declaration(name, ['effect_0']); family.spec(d, self.oracle[name])
            for key, value in [('schema', 2), ('amount', True), ('optional_effect', 1), ('mandatory_target', 1)]:
                bad = copy.deepcopy(d); bad['params'][key] = value
                with self.assertRaises(ValueError):
                    family.spec(bad, self.oracle[name])
            for extra in ('\nWhen this permanent enters, draw a card.', '\n{T}: Draw a card.'):
                with self.assertRaises(ValueError):
                    family.oracle_model(dict(self.oracle[name], oracle_text=self.oracle[name]['oracle_text']+extra))
        for name in ('Vicious Shadows', 'Proper Burial', 'Soul Net', 'Dross Harvester', 'Sangromancer'):
            with self.assertRaises(ValueError):
                family.oracle_model(self.oracle[name])

    def test_full_pins_and_implemented_sources_fail_closed(self):
        family.validate_witnesses(self.oracle, self.rows)
        for name, key, value in [('Safe Passage', 'oracle_text', 'Prevent life loss.'), ('Boggart Shenanigans', 'type_line', 'Enchantment'), ('Memnite', 'cmc', 1.0)]:
            o = copy.deepcopy(self.oracle); o[name][key] = value
            with self.assertRaises(ValueError):
                family.validate_witnesses(o, self.rows)
        rows = copy.deepcopy(self.rows); rows['Vindictive Vampire']['handler'] = 'card_dummy'
        with self.assertRaises(ValueError):
            family.validate_witnesses(self.oracle, rows)

    def test_all_pilot_variants_use_public_router_and_ordinary_operations(self):
        counts = {}
        for name, source in self.data['sources'].items():
            if source['csvid'] not in (19787, 2961, 4521, 4053, 12047, 9083):
                continue
            d = self.declaration(name, ['effect_0']); p = family.spec(d, self.oracle[name])
            selected = family.variants(p); d['variants'] = selected
            cases = public_expand(name, self.rows[name]['csvid'], d, self.oracle[name], self.rows, self.oracle)
            self.assertEqual(len(cases), len(selected)); counts[name] = len(cases)
            for c in cases:
                self.assertTrue(c['steps'])
                self.assertFalse(any(s['op'] in ('destroy', 'set_life', 'set_counter', 'dispatch_gfp') for s in c['steps']))
                self.assertTrue(self.has(c['steps'], op='assert_unused_choices', value=0))
                self.assertTrue(self.has(c['steps'], op='assert_choices_consumed'))
        self.assertEqual(len(counts), 6)
        self.assertEqual(sum(counts.values()), 448)

    def test_follow2_artifact_creature_another_and_controller_predicates(self):
        mine = family.oracle_model(self.oracle['Magnetic Mine'])
        iguanar = family.oracle_model(self.oracle['Hissing Iguanar'])
        self.assertEqual(mine['predicate'], dict(kind='artifact', another=True, controller='any', owner='any', nontoken=False))
        self.assertEqual((mine['amount'], mine['recipient'], mine['optional_effect']), (2, 'dead_controller', False))
        self.assertEqual(iguanar['predicate'], dict(kind='creature', another=True, controller='any', owner='any', nontoken=False))
        self.assertEqual((iguanar['amount'], iguanar['optional_effect'], iguanar['mandatory_target']), (1, True, True))
        for player in (0, 1):
            c = self.case('Magnetic Mine', 'artifact_death', player)
            self.assertTrue(any(x['name'] == 'Sol Ring' and x['ref'] == 'victim' for x in c['setup']))
            self.assertTrue(any(x['name'] == 'Naturalize' for x in c['setup']))
            self.assertEqual((self.final_life(c, player), self.final_life(c, 1-player)), (18, 20))
            self.assertEqual(self.final_life(self.case('Magnetic Mine', 'nonmatching', player), player), 20)
            self.assertEqual(self.final_life(self.case('Magnetic Mine', 'source_only', player), player), 20)
            c = self.case('Magnetic Mine', 'lent_death', player)
            self.assertEqual((self.final_life(c, player), self.final_life(c, 1-player)), (20, 18))
            self.assertEqual(self.final_life(self.case('Hissing Iguanar', 'opponent_death', player), 1-player), 19)
            self.assertEqual(self.final_life(self.case('Hissing Iguanar', 'source_only', player), 1-player), 20)

    def test_follow2_actual_mandatory_callback_keeps_oracle_optional_resolution(self):
        presentation = self.data['presentation']['Hissing Iguanar']
        self.assertEqual(presentation, dict(optional_outer='mandatory', live_source_selector=False, dead_legacy_selector=False))
        for player in (0, 1):
            for mode in ('effect', 'source_simultaneous'):
                c = self.case('Hissing Iguanar', mode, player)
                self.assertFalse(any(s['op'] in ('choose_trigger', 'choose_trigger_source') for s in c['steps']))
                self.assertTrue(self.has(c['steps'], op='choose_player', player=player, value=1-player))
                self.assertEqual(self.final_life(c, 1-player), 19)
            c = self.case('Hissing Iguanar', 'planeswalker', player)
            self.assertTrue(self.has(c['steps'], op='choose_target', ref='walker', player=player))
            self.assertTrue(self.has(c['steps'], op='assert_counter', ref='walker', counter_kind='loyalty', value=4))
        for mode in ('decline', 'mandatory_policy', 'partial_accept'):
            c = self.case('Hissing Iguanar', mode)
            self.assertTrue(self.has(c['steps'], op='choose_optional_target', ref='walker', player=0))
            self.assertTrue(self.has(c['steps'], op='assert_forced_target_choices', value=1))
            self.assertTrue(self.has(c['steps'], op='assert_counter', ref='walker', counter_kind='loyalty', value=5))
            self.assertEqual(self.final_life(c, 1), 19 if mode == 'partial_accept' else 20)
            self.assertFalse(self.has(c['steps'], op='choose_dialog'))

    def test_follow2_all_variants_and_real_source_removal_boundaries(self):
        total = 0
        for name in ('Magnetic Mine', 'Hissing Iguanar'):
            d = self.declaration(name, ['effect_0']); p = family.spec(d, self.oracle[name])
            d['variants'] = family.variants(p)
            cases = public_expand(name, self.rows[name]['csvid'], d, self.oracle[name], self.rows, self.oracle)
            total += len(cases)
            for c in cases:
                self.assertTrue(self.has(c['steps'], op='assert_unused_choices', value=0))
                self.assertFalse(any(s['op'] in ('destroy', 'set_life', 'set_counter') for s in c['steps']))
            for player in (0, 1):
                c = self.case(name, 'source_removed', player)
                self.assertEqual((self.final_life(c, 0), self.final_life(c, 1)), (20, 20))
                self.assertTrue(self.multiplicity(c, name, player, 'graveyard', 1))
                self.assertTrue(self.has(c['steps'], op='cast', ref='source_removal', value=1))
                c = self.case(name, 'source_simultaneous', player)
                self.assertTrue(self.multiplicity(c, name, player, 'graveyard', 1))
                self.assertEqual(self.final_life(c, player if name == 'Magnetic Mine' else 1-player), 18 if name == 'Magnetic Mine' else 19)
        self.assertEqual(total, 153)

    def test_real_death_cast_precedes_trigger_intent_and_resolution(self):
        for player in (0, 1):
            c = self.case('Boggart Shenanigans', player=player); s = c['steps']
            cast = next(i for i, x in enumerate(s) if x['op'] == 'cast' and x['ref'] == 'death_spell')
            target = next(i for i, x in enumerate(s[cast+1:], cast+1) if x['op'] == 'choose_player')
            resolve = next(i for i, x in enumerate(s[cast+1:], cast+1) if x['op'] == 'resolve')
            self.assertLess(cast, target); self.assertLess(target, resolve)
            self.assertTrue(self.multiplicity(c, 'Murder', 1-player, 'graveyard', 1))
            self.assertTrue(self.multiplicity(c, 'Goblin Piker', player, 'graveyard', 1))
            self.assertEqual(self.has(s[cast:resolve], op='choose_trigger_source', ref='source', player=0), player == 0)

    def test_owner_and_controller_recipients_are_independent(self):
        for player in (0, 1):
            dingus = self.case('Dingus Staff', 'stolen_death', player)
            liability = self.case('Liability', 'stolen_death', player)
            self.assertEqual((self.final_life(dingus, player), self.final_life(dingus, 1-player)), (18, 20))
            self.assertEqual((self.final_life(liability, player), self.final_life(liability, 1-player)), (20, 19))
            self.assertTrue(self.multiplicity(dingus, 'Grizzly Bears', 1-player, 'graveyard', 2))
            victim = next(x for x in liability['setup'] if x['ref'] == 'victim')
            self.assertEqual(victim['player'], player); self.assertIn('opponent_owned', victim['state'])
            scrap = self.case('Scrapheap', 'stolen_death', player)
            lent = self.case('Scrapheap', 'lent_death', player)
            self.assertEqual(self.final_life(scrap, player), 20)
            self.assertEqual(self.final_life(lent, player), 21)

    def test_damage_prevention_does_not_prevent_loss_or_conditional_gain(self):
        for player in (0, 1):
            for name in ('Dingus Staff', 'Liability', 'Vindictive Vampire'):
                c = self.case(name, 'prevented', player)
                self.assertTrue(any(x['name'] == 'Safe Passage' for x in c['setup']))
                self.assertTrue(self.has(c['steps'], op='cast', ref='prevention', value=1))
            self.assertEqual(self.final_life(self.case('Dingus Staff', 'prevented', player), player), 20)
            self.assertEqual(self.final_life(self.case('Liability', 'prevented', player), player), 19)
            vampire = self.case('Vindictive Vampire', 'prevented', player)
            self.assertEqual((self.final_life(vampire, player), self.final_life(vampire, 1-player)), (21, 20))

    def test_another_and_your_controller_distinctions_survive_source_death(self):
        for player in (0, 1):
            c = self.case('Vindictive Vampire', 'source_only', player)
            self.assertEqual(self.final_life(c, player), 20); self.assertEqual(self.final_life(c, 1-player), 20)
            c = self.case('Vindictive Vampire', 'opponent_death', player)
            self.assertEqual(self.final_life(c, player), 20)
            c = self.case('Vindictive Vampire', 'source_simultaneous', player)
            self.assertEqual((self.final_life(c, player), self.final_life(c, 1-player)), (21, 19))
            self.assertTrue(self.has(c['steps'], op='cast', ref='sweep', value=1))
            c = self.case('Liability', 'source_only', player)
            self.assertEqual(self.final_life(c, player), 19)

    def test_token_graveyard_entry_counts_unless_explicit_nontoken(self):
        for player in (0, 1):
            for name, expected in [('Dingus Staff', 18), ('Liability', 20), ('Scrapheap', 21)]:
                c = self.case(name, 'token_death', player)
                victim = next(x for x in c['setup'] if x['ref'] == 'victim')
                self.assertIn('token', victim['state'])
                self.assertEqual(self.final_life(c, player), expected)
                self.assertTrue(self.multiplicity(c, victim['name'], player, 'graveyard', 1 if victim['name'] in ('Grizzly Bears',) else 0))
                self.assertEqual(c['_recipe']['token_copy_fixture']['copied_mana_value'], self.oracle[victim['name']]['cmc'])
                self.assertIs(c['_recipe']['token_copy_fixture']['creation_tested'], False)

    def test_kindred_and_changeling_are_not_reduced_to_creature(self):
        for player in (0, 1):
            c = self.case('Boggart Shenanigans', 'kindred_goblin', player)
            self.assertTrue(any(x['name'] == 'Boggart Shenanigans' and x['ref'] == 'victim' for x in c['setup']))
            self.assertNotIn('Creature', self.oracle['Boggart Shenanigans']['type_line'])
            self.assertTrue(any(x['name'] == 'Naturalize' for x in c['setup']))
            self.assertEqual(self.final_life(c, 1-player), 19)
            self.assertEqual(self.final_life(self.case('Boggart Shenanigans', 'changeling', player), 1-player), 19)
            self.assertEqual(self.final_life(self.case('Boggart Shenanigans', 'nonmatching', player), 1-player), 20)

    def test_noncreature_deaths_use_real_naturalize_or_legal_land_spell(self):
        for mode, spell in [('artifact_death', 'Naturalize'), ('enchantment_death', 'Naturalize'), ('land_death', 'Stone Rain')]:
            c = self.case('Liability', mode)
            self.assertTrue(any(x['name'] == spell and x['ref'] == 'death_spell' for x in c['setup']))
            self.assertTrue(self.has(c['steps'], op='cast', ref='death_spell', value=1))
            self.assertEqual(self.final_life(c, 0), 19)

    def test_planeswalker_and_self_player_targets_remain_printed_choices(self):
        for name, amount in [('Rage Thrower', 2), ('Boggart Shenanigans', 1)]:
            for player in (0, 1):
                c = self.case(name, 'planeswalker', player)
                self.assertTrue(self.has(c['steps'], op='choose_target', ref='walker', player=player))
                self.assertTrue(self.has(c['steps'], op='assert_counter', ref='walker', counter_kind='loyalty', value=5-amount))
                self.assertEqual(self.final_life(c, 1-player), 20)
                c = self.case(name, 'target_self_player', player)
                self.assertTrue(self.has(c['steps'], op='choose_player', player=player, value=player))
                self.assertEqual(self.final_life(c, player), 20-amount)

    def test_optional_resolution_keeps_required_target_and_real_partial_acceptance(self):
        c = self.case('Boggart Shenanigans', 'decline')
        self.assertTrue(self.has(c['steps'], op='choose_trigger_source', ref='source', player=0))
        self.assertTrue(self.has(c['steps'], op='choose_optional_target', ref='walker', player=0))
        self.assertTrue(self.has(c['steps'], op='assert_forced_target_choices', value=1))
        self.assertEqual(self.final_life(c, 1), 20)
        self.assertFalse(self.has(c['steps'], op='choose_dialog'))
        c = self.case('Boggart Shenanigans', 'partial_accept')
        self.assertEqual(sum(s['op'] == 'choose_player' for s in c['steps']), 1)
        self.assertEqual(sum(s['op'] == 'choose_optional_target' for s in c['steps']), 1)
        self.assertEqual(self.final_life(c, 1), 19)
        self.assertTrue(self.multiplicity(c, 'Goblin Piker', 0, 'graveyard', 2))
        self.assertTrue(self.has(c['steps'], op='assert_counter', ref='walker', counter_kind='loyalty', value=5))

    def test_target_policy_attempt_keeps_optional_damage_declined(self):
        for name, loyalty in [('Boggart Shenanigans', 5), ('Rage Thrower', 3)]:
            c = self.case(name, 'mandatory_policy')
            self.assertTrue(self.has(c['steps'], op='choose_optional_target', ref='walker', player=0))
            self.assertTrue(self.has(c['steps'], op='assert_forced_target_choices', value=1))
            counters = [s['value'] for s in c['steps'] if s['op'] == 'assert_counter' and s['ref'] == 'walker']
            self.assertEqual(counters[0], 5)
            death = next(i for i, s in enumerate(c['steps']) if s['op'] == 'cast' and s['ref'] == 'death_spell')
            after = [s['value'] for s in c['steps'][death:] if s['op'] == 'assert_counter' and s['ref'] == 'walker']
            self.assertTrue(after); self.assertTrue(all(value == loyalty for value in after))
            self.assertEqual((self.final_life(c, 0), self.final_life(c, 1)), (20, 20))

    def test_two_deaths_have_distinct_targets_and_real_watcher_protection(self):
        for player in (0, 1):
            c = self.case('Rage Thrower', 'split_targets', player)
            s = c['steps']; casts = [(i, x['ref']) for i, x in enumerate(s) if x['op'] == 'cast']
            self.assertLess(next(i for i, ref in casts if ref == 'watcher_shield'), next(i for i, ref in casts if ref == 'creature_sweep'))
            self.assertTrue(any(x['name'] == 'Indestructibility' for x in c['setup']))
            self.assertTrue(self.has(s, op='choose_player', player=player, value=1-player))
            self.assertTrue(self.has(s, op='choose_target', ref='walker', player=player))
            self.assertTrue(self.has(s, op='assert_card', ref='source', property='in_play', value=1))
            self.assertEqual(self.final_life(c, 1-player), 18)
            self.assertTrue(self.has(s, op='assert_counter', ref='walker', counter_kind='loyalty', value=3))

    def test_generic_colored_payment_is_planned_before_human_cast(self):
        for name in ('Dingus Staff', 'Scrapheap'):
            for player in (0, 1):
                c = self.case(name, 'generic_colored_payment', player); s = c['steps']
                cast = next(i for i, x in enumerate(s) if x['op'] == 'cast' and x['ref'] == 'source')
                choices = [x for x in s[:cast] if x['op'] == 'choose_mana']
                self.assertEqual(len(choices), int(self.oracle[name]['cmc']) if player == 0 else 0)
                self.assertTrue(all(x['color'] == 1 for x in choices))
                self.assertTrue(self.has(s, op='assert_mana', player=player, color=1, value=0))

    def test_source_departure_has_its_own_receipt_then_stops_watching(self):
        for player in (0, 1):
            for name, death_total in [('Liability', 19), ('Scrapheap', 21), ('Vindictive Vampire', 20)]:
                c = self.case(name, 'source_removed', player)
                self.assertEqual(self.final_life(c, player), death_total)
                self.assertTrue(self.multiplicity(c, name, player, 'graveyard', 1))
                c = self.case(name, 'source_bounced', player)
                self.assertEqual(self.final_life(c, player), 20)
                self.assertTrue(self.multiplicity(c, name, player, 'hand', 1))
            c = self.case('Vindictive Vampire', 'source_exiled', player)
            self.assertEqual(self.final_life(c, player), 22)
            self.assertEqual(self.final_life(c, 1-player), 20)
            self.assertTrue(self.multiplicity(c, 'Vindictive Vampire', player, 'exile', 1))

    def test_union_predicate_counts_artifact_creature_exactly_once(self):
        for player in (0, 1):
            c = self.case('Scrapheap', player=player)
            self.assertTrue(any(x['name'] == 'Memnite' and x['ref'] == 'victim' for x in c['setup']))
            self.assertEqual(self.final_life(c, player), 21)
            self.assertEqual(self.final_life(self.case('Scrapheap', 'enchantment_death', player), player), 21)
            self.assertEqual(self.final_life(self.case('Scrapheap', 'land_death', player), player), 20)

    def test_dying_optional_source_uses_real_legacy_handoff_and_legal_sweep_phase(self):
        c = self.case('Boggart Shenanigans', 'source_simultaneous'); s = c['steps']
        cast = next(i for i, x in enumerate(s) if x['op'] == 'cast' and x['ref'] == 'sweep')
        selector = next(i for i, x in enumerate(s) if x['op'] == 'choose_trigger')
        resolve = next(i for i, x in enumerate(s[cast+1:], cast+1) if x['op'] == 'resolve')
        self.assertLess(cast, selector); self.assertLess(selector, resolve)
        self.assertFalse(self.has(s, op='choose_trigger_source'))
        self.assertTrue(any(x['op'] == 'turn' and x['player'] == 0 and x['value'] == 0 for x in s[:cast]))
        self.assertEqual(self.final_life(c, 1), 19)
        self.assertTrue(self.multiplicity(c, 'Boggart Shenanigans', 0, 'graveyard', 1))
        self.assertTrue(self.multiplicity(c, 'Goblin Piker', 0, 'graveyard', 1))

    def test_countered_cast_and_new_object_blink_use_actual_paid_spells(self):
        for name in self.data['sources']:
            c = self.case(name, 'countered'); s = c['steps']
            order = [x['op'] for x in s if x['op'] in ('cast', 'announce_spell', 'cast_response', 'resolve_response')]
            self.assertEqual(order, ['cast', 'announce_spell', 'cast_response', 'resolve_response'])
            c = self.case(name, 'source_blinked')
            self.assertTrue(any(x['name'] == 'Flicker' for x in c['setup']))
            self.assertTrue(self.has(c['steps'], op='bind_permanent', ref='source_new', player=0, value=self.rows[name]['csvid']))
            if name == 'Boggart Shenanigans':
                self.assertTrue(self.has(c['steps'], op='choose_trigger_source', ref='source_new', player=0))

    def test_snapshot_boundaries_repeat_real_inputs_and_no_seeded_zone_ref_properties(self):
        for name in self.data['sources']:
            for mode in ('ready_snapshot', 'pending_snapshot', 'death_snapshot', 'resolved_snapshot'):
                c = self.case(name, mode)
                self.assertTrue(self.has(c['steps'], op='capture')); self.assertTrue(self.has(c['steps'], op='restore'))
                self.assertTrue(self.has(c['steps'], op='assert_recorded_state'))
                forbidden = {x['ref'] for x in c['setup'] if x['zone'] in ('graveyard', 'exile', 'library')}
                self.assertFalse(any(s['op'] in ('assert_card', 'require_card') and s['ref'] in forbidden for s in c['steps']))


if __name__ == '__main__':
    unittest.main()
