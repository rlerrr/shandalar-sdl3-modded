import copy
import json
from pathlib import Path
import unittest
from unittest.mock import patch

from generate_cases import registry
from recipes import cost_from_oracle
import death_value_trigger_recipes as family


class DeathValueTriggerTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.base = Path(__file__).parent
        cls.oracle = json.loads((cls.base / 'data/oracle.json').read_text(encoding='utf8'))['cards']
        cls.rows = {row['name']: row for row in registry()}
        cls.pins = family._pins()

    def declaration(self, name='Moonlit Wake', variants=None):
        model = family.oracle_model(self.oracle[name])
        params = dict(schema=8, **model, presentation=copy.deepcopy(self.pins['presentations'][name]['presentation']))
        return dict(family=family.FAMILY, version=2, params=params,
                    oracle_text=self.oracle[name]['oracle_text'], variants=variants or ['own_death_0'])

    def case(self, name, variant):
        d = self.declaration(name, [variant])
        p = family.spec(d, self.oracle[name])
        return family.expand(name, self.rows[name]['csvid'], d, p, d['variants'], self.rows, self.oracle)[0]

    def has(self, steps, **fields):
        return any(all(step.get(key) == value for key, value in fields.items()) for step in steps)

    def count(self, steps, name, player, zone, number):
        return self.has(steps, op='assert_zone_multiplicity', index=self.rows[name]['csvid'],
                        player=player, zone=zone, value=number)

    def entity(self, name, ref, player=0, owner=0, token=False):
        r = self.oracle[name]
        return dict(name=name, ref=ref, player=player, owner=owner, token=token,
                    power=int(r.get('power', 0)), toughness=int(r.get('toughness', 0)))

    def test_whole_oracle_parameters_and_types_fail_closed(self):
        for name in self.pins['presentations']:
            d = self.declaration(name)
            family.spec(d, self.oracle[name])
            d['params']['receipt']['gain'] += 1
            with self.assertRaises(ValueError):
                family.spec(d, self.oracle[name])
        d = self.declaration();d['params']['predicate']['nontoken'] = 0
        with self.assertRaises(ValueError):
            family.spec(d, self.oracle['Moonlit Wake'])
        d = self.declaration();d['params']['invented'] = True
        with self.assertRaises(ValueError):
            family.spec(d, self.oracle['Moonlit Wake'])
        r = copy.deepcopy(self.oracle['Moonlit Wake']);r['oracle_text'] += '\nDraw a card.'
        with self.assertRaises(ValueError):
            family.oracle_model(r)
        r = copy.deepcopy(self.oracle['Moonlit Wake']);r['keywords'] = ['Flying']
        with self.assertRaises(ValueError):
            family.oracle_model(r)

    def test_witnesses_and_source_ui_identity_are_independently_pinned(self):
        family.validate_witnesses(self.oracle, self.rows)
        for name, key, value in [('Spore Frog', 'oracle_text', 'Draw a card.'),
                                 ('Scaled Wurm', 'toughness', '7'),
                                 ('Gilded Light', 'mana_cost', '{W}')]:
            oracle = copy.deepcopy(self.oracle);oracle[name][key] = value
            with self.assertRaises(ValueError):
                family.validate_witnesses(oracle, self.rows)
        rows = copy.deepcopy(self.rows);rows['Blood Artist']['handler'] = 'card_dummy'
        with self.assertRaises(ValueError):
            family.validate_witnesses(self.oracle, rows)

    def test_death_predicates_distinguish_controller_owner_source_and_tokens(self):
        source = self.entity('Zulaport Cutthroat', 'source')
        own = self.entity('Grizzly Bears', 'victim', 0, 1)
        opposing = self.entity('Grizzly Bears', 'victim', 1, 0)
        predicate = family.oracle_model(self.oracle['Zulaport Cutthroat'])['predicate']
        self.assertTrue(family.eligible(predicate, source, source, self.oracle))
        self.assertTrue(family.eligible(predicate, own, source, self.oracle))
        self.assertFalse(family.eligible(predicate, opposing, source, self.oracle))
        predicate = family.oracle_model(self.oracle['Harvester of Souls'])['predicate']
        source = self.entity('Harvester of Souls', 'source')
        self.assertFalse(family.eligible(predicate, source, source, self.oracle))
        self.assertTrue(family.eligible(predicate, opposing, source, self.oracle))
        self.assertFalse(family.eligible(predicate, dict(own, token=True), source, self.oracle))
        self.assertFalse(family.eligible(predicate, self.entity('Sol Ring', 'artifact'), source, self.oracle))

    def test_adjacent_wording_has_real_stat_and_owner_predicates(self):
        source = self.entity('Sultai Flayer', 'source')
        p = family.oracle_model(self.oracle['Sultai Flayer'])['predicate']
        self.assertFalse(family.eligible(p, self.entity('Grizzly Bears', 'bear'), source, self.oracle))
        self.assertTrue(family.eligible(p, self.entity('Scaled Wurm', 'wurm'), source, self.oracle))
        source = self.entity('Kothophed, Soul Hoarder', 'source')
        p = family.oracle_model(self.oracle['Kothophed, Soul Hoarder'])['predicate']
        self.assertTrue(family.eligible(p, self.entity('Sol Ring', 'stolen', 0, 1), source, self.oracle))
        self.assertFalse(family.eligible(p, self.entity('Sol Ring', 'lent', 1, 0), source, self.oracle))

    def test_draw_loss_and_own_controller_filter_have_independent_receipts(self):
        for owner in (0, 1):
            own = self.case('Dark Prophecy', f'own_death_{owner}')['steps']
            other = self.case('Dark Prophecy', f'opponent_death_{owner}')['steps']
            self.assertTrue(self.has(own, op='assert_life', player=owner, value=19))
            self.assertTrue(self.has(own, op='assert_zone', player=owner, zone='library', value=27))
            self.assertTrue(self.has(own, op='assert_zone', player=owner, zone='hand', value=5))
            self.assertTrue(self.has(other, op='assert_life', player=owner, value=20))
            self.assertTrue(self.has(other, op='assert_zone', player=owner, zone='library', value=28))

    def test_target_protection_keeps_targeted_and_untargeted_drain_distinct(self):
        for owner in (0, 1):
            steps = self.case('Blood Artist', f'own_death_{owner}')['steps']
            self.assertTrue(self.has(steps, op='assert_life', player=owner, value=21))
            self.assertTrue(self.has(steps, op='assert_life', player=1-owner, value=19))
            for mode in ('opponent_shrouded', 'both_players_shrouded'):
                steps = self.case('Blood Artist', f'{mode}_{owner}')['steps']
                self.assertTrue(self.has(steps, op='cast', ref=f'shield{1-owner}', value=1))
                self.assertTrue(self.has(steps, op='assert_life', player=0, value=20))
                self.assertTrue(self.has(steps, op='assert_life', player=1, value=20))
            steps = self.case('Zulaport Cutthroat', f'opponent_death_{owner}')['steps']
            self.assertTrue(self.has(steps, op='assert_life', player=owner, value=20))

    def test_nontoken_other_filter_and_sacrificed_token_destination_are_correct(self):
        for owner in (0, 1):
            normal = self.case('Harvester of Souls', f'own_death_{owner}')['steps']
            token = self.case('Harvester of Souls', f'token_death_{owner}')['steps']
            self.assertTrue(self.has(normal, op='assert_zone', player=owner, zone='library', value=27))
            self.assertTrue(self.has(token, op='assert_zone', player=owner, zone='library', value=28))
            self.assertTrue(self.count(token, 'Grizzly Bears', owner, 'graveyard', 1))
            self.assertTrue(self.has(self.case('Harvester of Souls', f'self_death_{owner}')['steps'],
                                     op='assert_zone', player=owner, zone='library', value=28))

    def test_simultaneous_deaths_use_source_last_known_information(self):
        for owner in (0, 1):
            blood = self.case('Blood Artist', f'simultaneous_{owner}')['steps']
            self.assertTrue(self.has(blood, op='cast', ref='wrath', value=1))
            self.assertFalse(any(s['op'] == 'destroy' for s in blood))
            self.assertTrue(self.has(blood, op='assert_life', player=owner, value=24))
            self.assertTrue(self.has(blood, op='assert_life', player=1-owner, value=16))
            self.assertTrue(self.count(blood, 'Blood Artist', owner, 'graveyard', 1))
            harvester = self.case('Harvester of Souls', f'simultaneous_{owner}')['steps']
            self.assertTrue(self.has(harvester, op='assert_zone', player=owner, zone='library', value=25))

    def test_optional_decline_retains_oracle_expectation_even_when_handler_is_mandatory(self):
        steps = self.case('Deathgreeter', 'decline_0')['steps']
        self.assertTrue(self.has(steps, op='destroy', ref='victim'))
        self.assertFalse(any(s['op'] in ('choose_trigger', 'choose_dialog') for s in steps))
        self.assertTrue(self.has(steps, op='assert_life', player=0, value=20))
        for owner in (0, 1):
            steps = self.case('Harvester of Souls', f'own_death_{owner}')['steps']
            self.assertTrue(self.has(steps, op='choose_dialog', player=owner, value=0))
            self.assertEqual(self.has(steps, op='choose_trigger_source', player=owner), owner == 0)

    def test_optional_live_source_is_pinned_while_dying_legacy_uses_its_real_draw_menu(self):
        live = self.case('Harvester of Souls', 'own_death_0')['steps']
        self.assertTrue(self.has(live, op='choose_trigger_source', player=0, ref='source'))
        self.assertFalse(any(s['op'] == 'choose_trigger' for s in live))
        dying = self.case('Harvester of Souls', 'simultaneous_0')['steps']
        self.assertFalse(any(s['op'] == 'choose_trigger' for s in dying))
        self.assertFalse(any(s['op'] == 'choose_trigger_source' for s in dying))
        self.assertEqual(sum(s['op'] == 'choose_dialog' and s.get('player') == 0 and s.get('value') == 0 for s in dying), 1)
        self.assertTrue(self.has(dying, op='assert_zone', player=0, zone='library', value=25))
        for name, variant in [('Harvester of Souls', 'own_death_1'),
                              ('Deathgreeter', 'own_death_0')]:
            self.assertFalse(any(s['op'] == 'choose_trigger_source' for s in self.case(name, variant)['steps']))
        decline = self.case('Harvester of Souls', 'decline_0')['steps']
        self.assertTrue(self.has(decline, op='choose_trigger_source', player=0, ref='source'))
        self.assertTrue(self.has(decline, op='choose_dialog', player=0, value=1))
        self.assertTrue(self.has(decline, op='assert_zone', player=0, zone='library', value=28))

    def test_sacrifice_is_a_real_cost_and_stolen_victim_uses_its_owners_graveyard(self):
        for owner in (0, 1):
            case = self.case('Moonlit Wake', f'sacrifice_{owner}')
            self.assertTrue(any(c['name'] == 'Spore Frog' and c['ref'] == 'victim' for c in case['setup']))
            self.assertTrue(self.has(case['steps'], op='activate', ref='victim', value=1))
            self.assertTrue(self.has(case['steps'], op='assert_life', player=owner, value=21))
            self.assertTrue(self.count(case['steps'], 'Spore Frog', owner, 'graveyard', 1))
            stolen = self.case('Moonlit Wake', f'stolen_death_{owner}')['steps']
            self.assertTrue(self.count(stolen, 'Grizzly Bears', owner, 'graveyard', 1))
            self.assertTrue(self.count(stolen, 'Grizzly Bears', 1-owner, 'graveyard', 2))

    def test_bounce_exile_and_removed_source_do_not_manufacture_deaths(self):
        for owner in (0, 1):
            for mode, expected in [('bounce_victim', 20), ('exile_victim', 22), ('source_bounced', 20)]:
                steps = self.case('Moonlit Wake', f'{mode}_{owner}')['steps']
                self.assertTrue(self.has(steps, op='assert_life', player=owner, value=expected))
            steps = self.case('Moonlit Wake', f'bounce_victim_{owner}')['steps']
            self.assertFalse(self.has(steps, op='bind_hand', ref='victim'))
            self.assertFalse(any(s['op'] == 'assert_card' and s.get('ref') == 'victim' for s in steps))
            self.assertTrue(self.count(steps, 'Grizzly Bears', owner, 'hand', 2))
            self.assertTrue(self.count(steps, 'Grizzly Bears', owner, 'battlefield', 0))
            self.assertTrue(self.count(steps, 'Grizzly Bears', owner, 'graveyard', 1))

    def test_replay_boundaries_retain_real_lifecycle_actions_and_ordered_libraries(self):
        for name in self.pins['presentations']:
            for owner in (0, 1):
                for mode in ('ready_snapshot', 'pending_snapshot', 'death_snapshot', 'resolved_snapshot'):
                    steps = self.case(name, f'{mode}_{owner}')['steps']
                    self.assertEqual(steps[0], {'op': 'reset_death_history'})
                    self.assertEqual(sum(s['op'] == 'reset_death_history' for s in steps), 1)
                    self.assertTrue(self.has(steps, op='capture'))
                    self.assertTrue(self.has(steps, op='restore'))
                    self.assertTrue(self.has(steps, op='assert_recorded_state'))
                    self.assertTrue(self.has(steps, op='assert_zone_card', zone='library', index=0))
                    if mode == 'pending_snapshot':
                        capture = next(i for i, s in enumerate(steps) if s['op'] == 'capture')
                        self.assertTrue(self.has(steps[:capture], op='cast', ref='source', value=1))
                        self.assertTrue(self.has(steps[:capture], op='assert_card', ref='source', property='in_hand', value=0))
                        self.assertFalse(any(s['op'] == 'resolve' for s in steps[:capture]))

    def test_counter_spell_has_an_actual_announced_parent_and_no_death_receipt(self):
        for owner in (0, 1):
            steps = self.case('Moonlit Wake', f'countered_{owner}')['steps']
            ops = [s['op'] for s in steps]
            self.assertLess(ops.index('announce_spell'), ops.index('cast_response'))
            self.assertTrue(self.has(steps, op='resolve_response'))
            self.assertTrue(self.count(steps, 'Counterspell', 1-owner, 'graveyard', 1))
            self.assertTrue(self.has(steps, op='assert_life', player=owner, value=20))

    def test_adding_an_unused_future_witness_does_not_rewrite_stable_case_bodies(self):
        expected = self.case('Moonlit Wake', 'own_death_0')
        pins = copy.deepcopy(self.pins)
        pins['cards']['Scaled Wurm future witness'] = copy.deepcopy(self.oracle['Scaled Wurm'])
        with patch.object(family, '_pins', return_value=pins):
            self.assertEqual(self.case('Moonlit Wake', 'own_death_0'), expected)

    def test_shared_dispatch_accepts_the_new_family_and_rejects_invented_variants(self):
        from recipes import expand
        d = self.declaration('Moonlit Wake', ['own_death_0'])
        cases = expand('Moonlit Wake', self.rows['Moonlit Wake']['csvid'], d,
                       self.oracle['Moonlit Wake'], self.rows, oracle_context=self.oracle)
        self.assertEqual(len(cases), 1)
        d['variants'] = ['invented_0']
        with self.assertRaises(ValueError):
            expand('Moonlit Wake', self.rows['Moonlit Wake']['csvid'], d,
                   self.oracle['Moonlit Wake'], self.rows, oracle_context=self.oracle)

    def test_typed_watchers_have_positive_victims_and_contrary_type_controls(self):
        for owner in (0, 1):
            artifact = self.case('Fangren Marauder', f'own_death_{owner}')
            enchantment = self.case('Femeref Enchantress', f'own_death_{owner}')
            self.assertTrue(any(c['ref'] == 'victim' and c['name'] == 'Ornithopter' for c in artifact['setup']))
            self.assertTrue(any(c['ref'] == 'victim' and c['name'] == 'Exploration' for c in enchantment['setup']))
            self.assertTrue(self.has(artifact['steps'], op='assert_life', player=owner, value=25))
            self.assertTrue(self.has(enchantment['steps'], op='assert_zone', player=owner, zone='library', value=27))
            self.assertTrue(self.has(self.case('Fangren Marauder', f'enchantment_death_{owner}')['steps'],
                                     op='assert_life', player=owner, value=20))
            self.assertTrue(self.has(self.case('Femeref Enchantress', f'artifact_death_{owner}')['steps'],
                                     op='assert_zone', player=owner, zone='library', value=28))
            self.assertTrue(self.has(self.case('Fangren Marauder', f'sacrifice_{owner}')['steps'],
                                     op='assert_life', player=owner, value=28))

    def test_self_enchantment_receipt_has_real_destruction_and_independent_other_deaths(self):
        for owner in (0, 1):
            own_other = self.case('Hatching Plans', f'own_death_{owner}')['steps']
            self.assertTrue(self.has(own_other, op='assert_zone', player=owner, zone='library', value=28))
            source = self.case('Hatching Plans', f'self_death_{owner}')['steps']
            self.assertTrue(self.has(source, op='assert_zone', player=owner, zone='library', value=25))
            spell = self.case('Hatching Plans', f'source_destroy_spell_{owner}')
            self.assertTrue(any(c['name'] == 'Naturalize' and c['ref'] == 'removal' for c in spell['setup']))
            self.assertTrue(self.has(spell['steps'], op='choose_target', ref='source', player=1-owner))
            self.assertTrue(self.has(spell['steps'], op='cast', ref='removal', value=1))
            self.assertTrue(self.has(spell['steps'], op='assert_zone', player=owner, zone='library', value=25))
            two = self.case('Hatching Plans', f'two_sources_{owner}')['steps']
            self.assertTrue(self.has(two, op='assert_zone', player=owner, zone='library', value=22))
            self.assertTrue(self.count(two, 'Hatching Plans', owner, 'graveyard', 2))

    def test_fecundity_draws_for_the_dead_creatures_controller_not_source_or_owner(self):
        for source_owner in (0, 1):
            own = self.case('Fecundity', f'own_death_{source_owner}')['steps']
            self.assertTrue(self.has(own, op='choose_number', player=0, value=int(source_owner == 0)))
            self.assertTrue(self.has(own, op='assert_zone', player=source_owner, zone='library', value=27))
            opposing = self.case('Fecundity', f'opponent_death_{source_owner}')['steps']
            self.assertTrue(self.has(opposing, op='choose_number', player=0, value=int(source_owner == 1)))
            self.assertTrue(self.has(opposing, op='assert_zone', player=1-source_owner, zone='library', value=27))
            self.assertTrue(self.has(opposing, op='assert_zone', player=source_owner, zone='library', value=28))
            stolen = self.case('Fecundity', f'stolen_death_{source_owner}')['steps']
            self.assertTrue(self.has(stolen, op='assert_zone', player=source_owner, zone='library', value=27))
            self.assertTrue(self.count(stolen, 'Grizzly Bears', 1-source_owner, 'graveyard', 2))
        declined = self.case('Fecundity', 'decline_0')['steps']
        self.assertTrue(self.has(declined, op='choose_number', player=0, value=0))
        self.assertTrue(self.has(declined, op='assert_zone', player=0, zone='library', value=28))
        self.assertFalse(any(s['op'] in ('choose_trigger', 'choose_trigger_source') for s in declined))

    def test_threshold_boundaries_and_modified_last_known_stats_are_independent(self):
        for owner in (0, 1):
            for name, expected in [('Sultai Flayer', 24), ('Mycoid Shepherd', 25)]:
                below = self.case(name, f'below_threshold_{owner}')['steps']
                self.assertTrue(self.has(below, op='assert_life', player=owner, value=20))
                for mode in ('at_threshold', 'above_threshold'):
                    self.assertTrue(self.has(self.case(name, f'{mode}_{owner}')['steps'],
                                             op='assert_life', player=owner, value=expected))
                reduced = self.case(name, f'zero_toughness_{owner}')
                self.assertTrue(any(c['name'] == 'Grasp of Darkness' for c in reduced['setup']))
                self.assertTrue(self.has(reduced['steps'], op='assert_life', player=owner, value=20))
            for name, expected in [('Sultai Flayer', 20), ('Mycoid Shepherd', 25)]:
                reduced_source = self.case(name, f'weakened_source_death_{owner}')['steps']
                self.assertTrue(self.has(reduced_source, op='assert_card', ref='source', property='power', value=1))
                self.assertTrue(self.has(reduced_source, op='assert_card', ref='source', property='toughness', value=1))
                self.assertTrue(self.has(reduced_source, op='assert_life', player=owner, value=expected))

    def test_subtype_predicate_distinguishes_same_stats_and_changeling(self):
        for owner in (0, 1):
            normal = self.case('Vengeful Dead', f'own_death_{owner}')
            wrong = self.case('Vengeful Dead', f'wrong_subtype_death_{owner}')
            changeling = self.case('Vengeful Dead', f'changeling_death_{owner}')
            for case in (normal, changeling):
                self.assertTrue(self.has(case['steps'], op='assert_life', player=1-owner, value=19))
            self.assertTrue(self.has(wrong['steps'], op='assert_life', player=1-owner, value=20))
            self.assertTrue(any(c['ref'] == 'victim' and c['name'] == 'Scathe Zombies' for c in normal['setup']))
            self.assertTrue(any(c['ref'] == 'victim' and c['name'] == 'Grizzly Bears' for c in wrong['setup']))
            self.assertTrue(any(c['ref'] == 'victim' and c['name'] == 'Woodland Changeling' for c in changeling['setup']))

    def test_ownership_scope_handles_lent_stolen_and_noncreature_permanents(self):
        for owner in (0, 1):
            stolen = self.case('Kothophed, Soul Hoarder', f'stolen_death_{owner}')['steps']
            lent = self.case('Kothophed, Soul Hoarder', f'lent_death_{owner}')['steps']
            self.assertTrue(self.has(stolen, op='assert_zone', player=owner, zone='library', value=27))
            self.assertTrue(self.has(stolen, op='assert_life', player=owner, value=19))
            self.assertTrue(self.count(stolen, 'Grizzly Bears', 1-owner, 'graveyard', 2))
            self.assertTrue(self.has(lent, op='assert_zone', player=owner, zone='library', value=28))
            self.assertTrue(self.has(lent, op='assert_life', player=owner, value=20))
            self.assertTrue(self.count(lent, 'Grizzly Bears', owner, 'graveyard', 2))
            for mode in ('opponent_artifact_death', 'opponent_enchantment_death', 'opponent_land_death'):
                steps = self.case('Kothophed, Soul Hoarder', f'{mode}_{owner}')['steps']
                self.assertTrue(self.has(steps, op='assert_zone', player=owner, zone='library', value=27))
                self.assertTrue(self.has(steps, op='assert_life', player=owner, value=19))

    def test_outer_only_decline_uses_the_real_optional_presentation_without_inventing_a_menu(self):
        for name in ('Fangren Marauder', 'Mycoid Shepherd', 'Disciple of the Vault'):
            d = self.declaration(name)
            p = family.spec(d, self.oracle[name])
            self.assertTrue(p['receipt']['optional'])
            self.assertIn('decline_0', family.variants(p))
            steps = self.case(name, 'decline_0')['steps']
            self.assertFalse(any(s['op'] in ('choose_trigger', 'choose_trigger_source', 'choose_dialog') for s in steps))
            self.assertTrue(self.has(steps, op='assert_life', player=0, value=20))
            self.assertTrue(self.has(steps, op='assert_life', player=1, value=20))
        self.assertIn('decline_0', family.variants(family.spec(self.declaration('Harvester of Souls'), self.oracle['Harvester of Souls'])))

    def test_femeref_contrast_uses_paid_real_naturalize_and_retains_draw(self):
        for owner in (0, 1):
            case = self.case('Femeref Enchantress', f'own_death_{owner}')
            steps = case['steps']
            removal = next(c for c in case['setup'] if c['ref'] == 'removal')
            self.assertEqual(removal['name'], 'Naturalize')
            self.assertEqual(removal['player'], 1-owner)
            self.assertFalse(any(s['op'] == 'destroy' for s in steps))
            cast = next(i for i, s in enumerate(steps) if s['op'] == 'cast' and s.get('ref') == 'removal')
            self.assertEqual(steps[cast-1], {'op': 'choose_target', 'player': 1-owner, 'ref': 'victim'})
            self.assertEqual(steps[cast+2], {'op': 'resolve'})
            self.assertEqual(cost_from_oracle(self.oracle['Naturalize']), [1, 0, 0, 1, 0, 0])
            self.assertTrue(self.has(steps, op='assert_mana_total', player=1-owner, value=238))
            self.assertTrue(self.count(steps, 'Naturalize', 1-owner, 'graveyard', 1))
            self.assertTrue(self.has(steps, op='assert_zone', player=owner, zone='library', value=27))

    def test_board_sweeps_handoff_to_their_legal_sorcery_caster(self):
        for name in self.pins['presentations']:
            for owner in (0, 1):
                steps = self.case(name, f'simultaneous_{owner}')['steps']
                cast = next(i for i, s in enumerate(steps) if s['op'] == 'cast' and s.get('ref') == 'wrath')
                self.assertEqual(steps[cast-1], {'op': 'turn', 'player': 1-owner, 'value': 0})

    def test_watched_enchantment_deaths_use_real_resolution_across_lifecycle_controls(self):
        profiles = {
            'Femeref Enchantress': ('own_death', 'opponent_death', 'token_death', 'stolen_death',
                                   'ready_snapshot', 'pending_snapshot', 'resolved_snapshot',
                                   'bare_zones', 'cleanup_once', 'two_deaths', 'two_sources'),
            'Hatching Plans': ('self_death', 'ready_snapshot', 'pending_snapshot', 'resolved_snapshot',
                               'bare_zones', 'cleanup_once', 'two_deaths', 'two_sources'),
            'Kothophed, Soul Hoarder': ('opponent_enchantment_death',),
        }
        for name, modes in profiles.items():
            for mode in modes:
                for owner in (0, 1):
                    case = self.case(name, f'{mode}_{owner}')
                    names = {c['ref']: c['name'] for c in case['setup']}
                    naturalizes = [s for s in case['steps'] if s['op'] == 'cast' and names[s['ref']] == 'Naturalize']
                    casts = 2 if name == 'Hatching Plans' and mode == 'two_sources' else 1
                    self.assertEqual(len(naturalizes), casts * (2 if mode in ('ready_snapshot', 'pending_snapshot') else 1))
                    self.assertFalse(any(s['op'] == 'destroy' and
                                         'Enchantment' in self.oracle[names[s['ref']]]['type_line']
                                         for s in case['steps']))
                    draw = (6 if mode == 'two_sources' else 3) if name == 'Hatching Plans' else (2 if name == 'Femeref Enchantress' and mode == 'two_sources' else 1)
                    self.assertTrue(self.has(case['steps'], op='assert_zone', player=owner, zone='library', value=28-draw))

    def test_removal_spells_always_have_legal_printed_target_types(self):
        creature_only = {'Murder', 'Disfigure', 'Grasp of Darkness', 'Diminish',
                         'Unsummon', 'Swords to Plowshares', 'Lightning Bolt', 'Lightning Blast'}
        for name in self.pins['presentations']:
            declaration = self.declaration(name)
            model = family.spec(declaration, self.oracle[name])
            for variant in family.variants(model):
                case = self.case(name, variant)
                names = {c['ref']: c['name'] for c in case['setup']}
                for index, s in enumerate(case['steps'][:-1]):
                    if s['op'] != 'choose_target':
                        continue
                    following = case['steps'][index+1]
                    self.assertEqual(following['op'], 'cast')
                    spell = names[following['ref']]
                    target_type = self.oracle[names[s['ref']]]['type_line']
                    if spell in creature_only:
                        self.assertIn('Creature', target_type, (name, variant, spell, target_type))
                    if spell in ('Naturalize', 'Return to Dust'):
                        self.assertTrue('Artifact' in target_type or 'Enchantment' in target_type)


if __name__ == '__main__':
    unittest.main()
