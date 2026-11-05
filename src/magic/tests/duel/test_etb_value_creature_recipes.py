"""Independent entry expectations and fail-closed component scope."""
import copy
import json
import unittest
from pathlib import Path
from generate_cases import registry
from recipes import expand, explicit_spec
from etb_value_creature_recipes import variants, validate_witnesses


class EtbValueCreatureRecipesTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.oracle = json.loads((Path(__file__).parent/'data/oracle.json').read_text(encoding='utf8'))['cards']
        cls.registry = {r['name']: r for r in registry()}

    def declaration(self, name='Elvish Visionary', draw=1, gain=0, loss=0, keys=None, selected=None):
        return dict(family='etb_value_creature', version=2, oracle_text=self.oracle[name]['oracle_text'],
                    params=dict(keywords=keys or [], draw=draw, gain_life=gain, lose_life=loss), variants=selected or ['cast'])

    def cases(self, name, d, registry=None):
        return expand(name, self.registry[name]['csvid'], d, self.oracle[name], registry or self.registry, oracle_context=self.oracle)

    def test_explicit_amounts_cost_scope_and_pin(self):
        d = self.declaration(); self.cases('Elvish Visionary', d)
        for change in (dict(oracle_text='changed'), dict(params={}), dict(params=dict(d['params'], draw=2)),
                       dict(params=dict(d['params'], draw=True)), dict(params=dict(d['params'], gain_life=1)),
                       dict(params=dict(d['params'], lose_life=1)), dict(params=dict(d['params'], optional=True))):
            with self.assertRaises(ValueError): self.cases('Elvish Visionary', dict(d, **change))

    def test_extra_triggers_costs_optional_and_modes_rejected(self):
        for name in ('Mulldrifter', 'Sea Gate Oracle', 'Thragtusk', 'Kitchen Finks', 'Azorius Herald', 'Raven Familiar'):
            with self.assertRaises(ValueError): explicit_spec(self.declaration(name), self.oracle[name])
        d = self.declaration(); r = dict(self.oracle['Elvish Visionary'], layout='token')
        with self.assertRaises(ValueError): explicit_spec(d, r)
        for text in ('When this creature enters, you may draw a card.', 'When this creature dies, draw a card.',
                     'When another creature enters, draw a card.', 'When this creature enters, draw a card.\n{T}: Draw a card.'):
            r = dict(self.oracle['Elvish Visionary'], oracle_text=text); d['oracle_text'] = text
            with self.assertRaises(ValueError): explicit_spec(d, r)

    def test_self_reference_and_old_entry_wording(self):
        d = self.declaration(); r = dict(self.oracle['Elvish Visionary'], oracle_text='When Elvish Visionary enters the battlefield, draw a card.')
        d['oracle_text'] = r['oracle_text']; self.assertEqual(explicit_spec(d, r)['draw'], 1)

    def test_ordered_draws_check_multiplicity_and_opponent_zones(self):
        d = self.declaration('Phyrexian Gargantua', draw=2, loss=2)
        c = self.cases('Phyrexian Gargantua', d)[0]
        self.assertIn(dict(op='assert_life', player=0, value=18), c['steps'])
        self.assertIn(dict(op='assert_zone', player=0, zone='hand', value=6), c['steps'])
        self.assertIn(dict(op='assert_hand_graveyard_total', player=0, index=self.registry['Plains']['csvid'], value=2), c['steps'])
        self.assertIn(dict(op='assert_zone_card', player=0, zone='library', index=0, value=self.registry['Grizzly Bears']['csvid']), c['steps'])
        self.assertIn(dict(op='assert_zone', player=1, zone='library', value=7), c['steps'])

    def test_life_gain_not_damage_and_both_controllers(self):
        d = self.declaration('Lone Missionary', draw=0, gain=4, selected=['injured', 'opponent_injured'])
        for c, owner in zip(self.cases('Lone Missionary', d), (0, 1)):
            self.assertIn(dict(op='set_life', player=owner, value=7), c['steps'])
            self.assertIn(dict(op='assert_life', player=owner, value=11), c['steps'])
            self.assertIn(dict(op='assert_life', player=1-owner, value=20), c['steps'])

    def test_life_loss_without_draw_is_not_life_gain_or_damage(self):
        d=self.declaration('Foul Imp',draw=0,gain=0,loss=2,keys=['Flying'],selected=['cast','bounce_recast'])
        cast,recast=self.cases('Foul Imp',d)
        self.assertIn(dict(op='assert_life',player=0,value=18),cast['steps'])
        self.assertIn(dict(op='assert_life',player=0,value=16),recast['steps'])
        self.assertIn(dict(op='assert_zone',player=0,zone='hand',value=4),cast['steps'])
        with self.assertRaises(ValueError):self.cases('Foul Imp',dict(d,params=dict(d['params'],gain_life=2,lose_life=0)))

    def test_bounce_recasts_real_card_with_fresh_hand_binding(self):
        c = self.cases('Elvish Visionary', self.declaration(selected=['bounce_recast']))[0]
        self.assertIn(dict(op='cast', ref='bounce', value=1), c['steps'])
        self.assertIn(dict(op='bind_hand', player=0, value=self.registry['Elvish Visionary']['csvid'], ref='source'), c['steps'])
        self.assertEqual(sum(s == dict(op='cast', ref='source', value=1) for s in c['steps']), 2)
        self.assertIn(dict(op='assert_zone', player=0, zone='library', value=5), c['steps'])
        c = self.cases('Elvish Visionary', self.declaration(selected=['bounce_only']))[0]
        self.assertIn(dict(op='assert_zone', player=0, zone='library', value=6), c['steps'])

    def test_blink_uses_real_exile_and_reentry(self):
        c = self.cases('Elvish Visionary', self.declaration(selected=['blink']))[0]
        self.assertIn(dict(op='cast', ref='blink', value=1), c['steps'])
        self.assertIn(dict(op='bind_permanent', player=0, value=self.registry['Elvish Visionary']['csvid'], ref='source'), c['steps'])
        self.assertIn(dict(op='assert_zone', player=0, zone='exile', value=2), c['steps'])
        self.assertIn(dict(op='assert_zone', player=0, zone='hand', value=6), c['steps'])

    def test_countered_creature_has_no_entry_effect(self):
        c = self.cases('Elvish Visionary', self.declaration(selected=['countered']))[0]
        self.assertIn(dict(op='choose_priority', ref='counter'), c['steps'])
        self.assertLess(c['steps'].index(dict(op='choose_priority', ref='counter')), c['steps'].index(dict(op='cast', ref='source', value=0)))
        self.assertFalse(any(s['op'] == 'choose_target' for s in c['steps']))
        self.assertIn(dict(op='assert_card', ref='source', property='in_play', value=0), c['steps'])
        self.assertIn(dict(op='assert_zone', player=0, zone='library', value=7), c['steps'])
        self.assertIn(dict(op='assert_zone', player=0, zone='graveyard', value=6), c['steps'])

    def test_empty_zones_are_deliberate_and_last_card_draw_is_ordered(self):
        d = self.declaration(selected=['empty_hand', 'empty_graveyard', 'empty_exile', 'short_library'])
        for c, zone, count in zip(self.cases('Elvish Visionary', d), ('hand', 'graveyard', 'exile', 'library'), (1, 0, 0, 0)):
            self.assertEqual(c['fixture'], 'empty_main')
            self.assertIn(dict(op='assert_zone', player=0, zone=zone, value=count), c['steps'])
            self.assertIn(dict(op='assert_zone', player=1, zone='library', value=7), c['steps'])

    def test_pending_snapshot_is_after_payment_before_resolution(self):
        for v in ('pending_snapshot', 'opponent_pending_snapshot', 'blink_snapshot'):
            c = self.cases('Elvish Visionary', self.declaration(selected=[v]))[0]
            i = c['steps'].index(dict(op='capture'))
            self.assertTrue(any(s['op'] == 'cast' for s in c['steps'][:i]))
            self.assertEqual(c['steps'][i+1], dict(op='resolve'))
            for op in ('record_state', 'ai_roundtrip', 'restore', 'assert_recorded_state'): self.assertIn(dict(op=op), c['steps'])

    def test_cleanup_and_other_entries_do_not_repeat_the_source_trigger(self):
        d = self.declaration('Lone Missionary', draw=0, gain=4, selected=['cleanup_once', 'other_creature', 'destroy'])
        for c in self.cases('Lone Missionary', d): self.assertIn(dict(op='assert_life', player=0, value=24), c['steps'])

    def test_battlefield_setup_receipt_is_observed_before_and_after_combat(self):
        c = self.cases('Lone Missionary', self.declaration('Lone Missionary', draw=0, gain=4, selected=['unblocked']))[0]
        life = dict(op='assert_life', player=0, value=24)
        self.assertEqual(c['steps'][0], life)
        self.assertGreaterEqual(sum(s == life for s in c['steps']), 3)
        c = self.cases('Phyrexian Rager', self.declaration('Phyrexian Rager', loss=1, selected=['block_ground']))[0]
        self.assertEqual(c['steps'][0], dict(op='assert_life', player=0, value=19))
        self.assertIn(dict(op='assert_zone', player=0, zone='library', value=6), c['steps'])

    def test_witnesses_and_expectations_are_handler_independent(self):
        r = copy.deepcopy(self.registry)
        for row in r.values(): row['handler'] = 'card_dummy'
        d = self.declaration(); self.assertEqual(self.cases('Elvish Visionary', d), self.cases('Elvish Visionary', d, r))
        o = copy.deepcopy(self.oracle); o['Cloudshift']['oracle_text'] = 'Draw a card.'
        with self.assertRaises(ValueError): validate_witnesses(o, self.registry)
        with self.assertRaises(ValueError): self.cases('Elvish Visionary', dict(d, variants=['unknown']))
        with self.assertRaises(ValueError): self.cases('Elvish Visionary', dict(d, resolve_dialogs=[0]))


if __name__ == '__main__': unittest.main()
