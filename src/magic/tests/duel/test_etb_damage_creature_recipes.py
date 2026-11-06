import copy
import json
import unittest
from pathlib import Path

from generate_cases import registry
from etb_damage_creature_recipes import spec, variants, validate_witnesses, expand


class EntryDamageRecipesTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.oracle = json.loads((Path(__file__).parent / 'data/oracle.json').read_text(encoding='utf8'))['cards']
        cls.rows = {r['name']: r for r in registry()}

    def declaration(self, name='Fire Imp'):
        scopes = {'Fire Imp': ('creature', 2, 0), 'Flametongue Kavu': ('creature', 4, 0),
                  'Forge Devil': ('creature', 1, 1), 'Sparkmage Apprentice': ('any', 1, 0),
                  'Viashino Pyromancer': ('player', 2, 0), 'Cinder Hellion': ('opponent', 2, 0),
                  'Deadshot Minotaur': ('flying_creature', 3, 0)}
        scope, amount, collateral = scopes[name]; r = self.oracle[name]
        return dict(family='etb_damage_creature', version=2, oracle_text=r['oracle_text'], variants=['entry_0'],
                    params=dict(targets=scope, amount=amount, damage_you=collateral,
                                keywords=sorted(k for k in r['keywords'] if k != 'Cycling'),
                                inactive_rules=[s for s in r['oracle_text'].splitlines() if s.startswith('Cycling ')]))

    def cases(self, name='Fire Imp', modes=None):
        d = self.declaration(name)
        if modes is not None:
            d['variants'] = modes
        p = spec(d, self.oracle[name])
        return expand(name, self.rows[name]['csvid'], d, p, d['variants'], self.rows, self.oracle)

    def test_full_pin_and_exact_damage_scope_required(self):
        for field, value in [('amount', 1), ('targets', 'any'), ('damage_you', 1), ('keywords', ['Flying'])]:
            d = self.declaration(); d['params'][field] = value
            with self.assertRaises(ValueError):
                spec(d, self.oracle['Fire Imp'])
        d = self.declaration(); d['oracle_text'] = 'different'
        with self.assertRaises(ValueError):
            spec(d, self.oracle['Fire Imp'])

    def test_normal_surviving_numeric_creature_required(self):
        for change in (dict(layout='transform'), dict(toughness='0'), dict(power='*'), dict(card_faces=[{}])):
            r = dict(self.oracle['Fire Imp'], **change)
            with self.assertRaises(ValueError):
                spec(self.declaration(), r)

    def test_additional_trigger_or_unreviewed_inactive_rule_rejected(self):
        for line in ('When this creature dies, draw a card.', '{R}: This creature gets +1/+0 until end of turn.'):
            r = dict(self.oracle['Fire Imp']); r['oracle_text'] += '\n' + line
            d = self.declaration(); d['oracle_text'] = r['oracle_text']
            with self.assertRaises(ValueError):
                spec(d, r)
            d['params']['inactive_rules'] = [line]
            with self.assertRaises(ValueError):
                spec(d, r)

    def test_source_keywords_and_inactive_cycling_are_explicit_gaps(self):
        for name, gap in [('Cinder Hellion', 'Trample'), ('Deadshot Minotaur', 'Cycling {R/G}')]:
            c = self.cases(name)[0]
            self.assertTrue(any(gap in s for s in c['_recipe']['untested_components']))
            self.assertFalse(any(s['op'] in ('activate_hand', 'activate', 'advance_turn') for s in c['steps']))

    def test_witness_pins_reject_changed_effect_or_characteristic(self):
        validate_witnesses(self.oracle, self.rows)
        for name, key, value in [('Scaled Wurm', 'toughness', '5'), ('Unsummon', 'oracle_text', ''), ('Darksteel Gargoyle', 'keywords', ['Flying'])]:
            o = copy.deepcopy(self.oracle); o[name][key] = value
            with self.assertRaises(ValueError):
                validate_witnesses(o, self.rows)

    def test_both_controllers_cast_and_pay_normally(self):
        for c, owner in zip(self.cases(modes=['payment_0', 'payment_1']), (0, 1)):
            self.assertEqual(next(s for s in c['setup'] if s['ref'] == 'source')['player'], owner)
            self.assertIn(dict(op='cast', ref='source', value=1), c['steps'])
            self.assertIn(dict(op='assert_stack', value=1), c['steps'])
            for color in range(6):
                self.assertIn(dict(op='assert_mana', player=owner, color=color, value=0), c['steps'])

    def test_effect_and_unselected_creature_receipts(self):
        steps = self.cases()[0]['steps']
        self.assertIn(dict(op='assert_card', ref='recipient', property='damage', value=2), steps)
        self.assertIn(dict(op='assert_card', ref='unselected', property='damage', value=0), steps)
        self.assertIn(dict(op='assert_card', ref='source', property='power', value=2), steps)

    def test_lethal_target_moves_to_owner_graveyard(self):
        c = self.cases(modes=['lethal_1'])[0]; target = self.rows['Savannah Lions']['csvid']
        self.assertIn(dict(op='assert_card', ref='recipient', property='in_play', value=0), c['steps'])
        self.assertIn(dict(op='assert_zone_multiplicity', player=0, zone='graveyard', index=target, value=1), c['steps'])
        self.assertIn(dict(op='assert_zone_multiplicity', player=0, zone='hand', index=target, value=0), c['steps'])

    def test_indestructible_keeps_marked_damage(self):
        c = self.cases('Flametongue Kavu', ['indestructible_0'])[0]
        self.assertIn(dict(op='assert_card', ref='recipient', property='in_play', value=1), c['steps'])
        self.assertIn(dict(op='assert_card', ref='recipient', property='damage', value=4), c['steps'])

    def test_mandatory_sole_source_target_is_not_cast_prohibition(self):
        for name in ('Fire Imp', 'Forge Devil'):
            c = self.cases(name, ['only_source_0'])[0]
            self.assertIn(dict(op='cast', ref='source', value=1), c['steps'])
            self.assertIn(dict(op='choose_target', player=0, ref='source'), c['steps'])
            self.assertIn(dict(op='assert_card', ref='source', property='in_play', value=0), c['steps'])
        c = self.cases('Forge Devil', ['only_source_0'])[0]
        self.assertIn(dict(op='assert_life', player=0, value=19), c['steps'])

    def test_no_flying_target_casts_without_damage_or_choices(self):
        c = self.cases('Deadshot Minotaur', ['no_flying_1'])[0]
        self.assertIn(dict(op='cast', ref='source', value=1), c['steps'])
        self.assertFalse(any(s['op'] == 'choose_target' for s in c['steps']))
        self.assertIn(dict(op='assert_card', ref='ground', property='damage', value=0), c['steps'])

    def test_countered_creature_never_damages(self):
        c = self.cases('Forge Devil', ['countered_1'])[0]
        self.assertIn(dict(op='cast', ref='source', value=0), c['steps'])
        self.assertIn(dict(op='choose_priority', player=0, ref='counter'), c['steps'])
        self.assertIn(dict(op='assert_life', player=1, value=20), c['steps'])
        self.assertIn(dict(op='assert_card', ref='recipient', property='damage', value=0), c['steps'])

    def test_reentry_repeats_effect_but_bounce_alone_does_not(self):
        for mode, damage in [('bounce_only', 2), ('bounce_recast', 4), ('blink', 4), ('two_sources', 4)]:
            c = self.cases(modes=[mode + '_0'])[0]
            self.assertIn(dict(op='assert_card', ref='recipient', property='damage', value=damage), c['steps'])
            if mode == 'bounce_recast':
                self.assertEqual(sum(s['op'] == 'cast' and s['ref'] == 'source' for s in c['steps']), 2)

    def test_snapshot_replays_real_pending_entry_and_checks_zones(self):
        c = self.cases(modes=['pending_snapshot_1'])[0]; seq = c['steps']
        cut = next(i for i, s in enumerate(seq) if s['op'] == 'capture')
        self.assertTrue(dict(op='assert_stack', value=1) in seq[:cut])
        self.assertIn(dict(op='assert_card', ref='source', property='in_play', value=0), seq[:cut])
        self.assertIn(dict(op='assert_card', ref='recipient', property='damage', value=0), seq[:cut])
        self.assertEqual(seq[cut+1], dict(op='choose_target', player=1, ref='recipient'))
        record = next(i for i, s in enumerate(seq) if s['op'] == 'record_state')
        restore = next(i for i, s in enumerate(seq) if s['op'] == 'restore')
        self.assertTrue(any(s['op'] == 'assert_zone_multiplicity' for s in seq[:record]))
        self.assertTrue(any(s['op'] == 'assert_zone_multiplicity' for s in seq[restore:]))
        self.assertIn(dict(op='ai_roundtrip'), seq)
        self.assertIn(dict(op='assert_recorded_state'), seq)

    def test_blink_snapshot_captures_actual_cloudshift_before_entry(self):
        seq = self.cases(modes=['blink_snapshot_0'])[0]['steps']
        cut = next(i for i, s in enumerate(seq) if s['op'] == 'capture')
        self.assertTrue(any(s['op'] == 'cast' and s['ref'] == 'blink' for s in seq[:cut]))
        self.assertEqual(seq[cut+1], dict(op='choose_target', player=0, ref='recipient'))

    def test_player_and_opponent_scopes_do_not_target_creatures(self):
        for name, mode, player in [('Sparkmage Apprentice', 'own_player_0', 0), ('Viashino Pyromancer', 'own_player_1', 1), ('Cinder Hellion', 'entry_1', 0)]:
            c = self.cases(name, [mode])[0]
            self.assertIn(dict(op='choose_player', player=int(mode[-1]), value=player), c['steps'])
            self.assertFalse(any(s['op'] == 'choose_target' for s in c['steps']))
        p = spec(self.declaration('Cinder Hellion'), self.oracle['Cinder Hellion'])
        self.assertNotIn('own_player_0', variants(p))

    def test_cleanup_heals_damage_and_preserves_loss(self):
        c = self.cases('Forge Devil', ['cleanup_1'])[0]
        self.assertIn(dict(op='cleanup'), c['steps'])
        self.assertIn(dict(op='assert_card', ref='recipient', property='damage', value=0), c['steps'])
        self.assertIn(dict(op='assert_life', player=1, value=19), c['steps'])

    def test_cleanup_requires_positive_entry_damage_before_healing(self):
        for name, amount in [('Fire Imp', 2), ('Flametongue Kavu', 4), ('Forge Devil', 1), ('Deadshot Minotaur', 3)]:
            seq = self.cases(name, ['cleanup_0'])[0]['steps']
            cut = next(i for i, s in enumerate(seq) if s['op'] == 'cleanup')
            self.assertIn(dict(op='assert_card', ref='recipient', property='damage', value=amount), seq[:cut])
            self.assertIn(dict(op='assert_card', ref='recipient', property='damage', value=0), seq[cut:])

    def test_self_target_has_other_legal_surviving_controls(self):
        for name in ('Fire Imp', 'Sparkmage Apprentice', 'Forge Devil'):
            for owner in (0, 1):
                only = self.cases(name, [f'only_source_{owner}'])[0]
                chosen = self.cases(name, [f'self_target_{owner}'])[0]
                self.assertEqual([s['ref'] for s in only['setup']], ['source'])
                self.assertEqual({s['ref'] for s in chosen['setup']}, {'source', 'unselected', 'unselected_enemy'})
                for ref in ('unselected', 'unselected_enemy'):
                    self.assertIn(dict(op='assert_card', ref=ref, property='in_play', value=1), chosen['steps'])
                    self.assertIn(dict(op='assert_card', ref=ref, property='damage', value=0), chosen['steps'])
                self.assertIn(dict(op='choose_target', ref='source', player=owner), chosen['steps'])
                self.assertIn(dict(op='assert_card', ref='source', property='in_play', value=0), chosen['steps'])


if __name__ == '__main__':
    unittest.main()
