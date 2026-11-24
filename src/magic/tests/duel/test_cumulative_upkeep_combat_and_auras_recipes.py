"""Guard the exact upkeep and additional printed rider cases."""
import json
import unittest

from generate_cases import registry
import cumulative_upkeep_combat_and_auras_recipes as model


class CumulativeCombatAndAurasTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        data = model.PROFILE.parent
        cls.oracle = json.loads((data / 'oracle.json').read_bytes())['cards']
        cls.rows = {row['name']: row for row in registry()}
        cls.homes = {}
        for name, pin in model.profiles()['sources'].items():
            home, = (data / 'cases/cards').glob(f"*/{pin['registry']['csvid']:05d}-*.json")
            cls.homes[name] = json.loads(home.read_bytes())['recipes'][0]

    def steps(self, name, variant):
        declaration = self.homes[name]
        shape = model.spec(declaration, self.oracle[name])
        return model.build(name, self.rows[name]['csvid'], declaration, shape, variant, self.rows, self.oracle)['steps']

    def test_complete_pins_and_actual_payment_sources(self):
        model.validate_witnesses(self.oracle, self.rows)
        for name, declaration in self.homes.items():
            record = self.oracle[name]
            self.assertEqual(declaration['variants'], model.variants(model.spec(declaration, record)))
            with self.assertRaises(ValueError):
                model.spec(declaration, dict(record, oracle_text='Cumulative upkeep'))
            paid = self.steps(name, 'paid_two_0')
            chosen = [step['ref'] for step in paid if step['op'] == 'choose_mana_source']
            self.assertEqual(chosen, ['land_0', 'land_0', 'land_1'])
            self.assertIn(dict(op='play_land', ref='land_1', value=1), paid)

    def test_brand_stops_only_the_enchanted_controller_and_releases_on_decline(self):
        decline = self.steps('Brand of Ill Omen', 'decline_two_0')
        casts = [(step['ref'], step['value']) for step in decline if step['op'] == 'cast']
        self.assertEqual(casts, [('source', 1), ('foe_creature', 0), ('other_spell', 1),
                                 ('own_creature', 1), ('foe_creature', 1)])
        own = self.steps('Brand of Ill Omen', 'own_brand_1')
        self.assertIn(dict(op='choose_target', player=1, ref='host'), own)
        self.assertIn(dict(op='cast', ref='own_creature', value=0), own)
        self.assertIn(dict(op='cast', ref='foe_creature', value=1), own)

    def test_wind_attachment_control_and_pollen_expiry(self):
        for mode, seat in (('paid_one_0', 1), ('own_wind_0', 0)):
            wind = self.steps('Maddening Wind', mode)
            damaged = {s['player'] for s in wind if s['op'] == 'assert_life' and s['value'] < 20}
            self.assertEqual(damaged, {seat})
        pollen = self.steps('Fyndhorn Pollen', 'paid_two_1')
        power = [s['value'] for s in pollen if s.get('ref') == 'ally' and s.get('property') == 'power']
        self.assertEqual(power, [2, 1, 2])
        decline = self.steps('Fyndhorn Pollen', 'decline_two_0')
        self.assertIn(dict(op='assert_card', ref='ally', property='power', value=3), decline)

    def test_wind_unoffered_payment_probes_preserve_oracle(self):
        for actor in (0, 1):
            for mode in ('paid_one', 'paid_two', 'decline_two'):
                steps = self.steps('Maddening Wind', f'{mode}_{actor}')
                upkeep = steps.index(dict(op='upkeep_step', player=1-actor))
                self.assertEqual(steps[upkeep-1], dict(op='choose_dialog', player=actor, value=1))
                self.assertEqual(steps[upkeep+1], dict(op='assert_unused_choices', value=1))
        steps = self.steps('Maddening Wind', 'own_wind_1')
        upkeep = steps.index(dict(op='upkeep_step', player=1))
        self.assertEqual(steps[upkeep-2:upkeep], [dict(op='choose_dialog', player=1, value=0),
                                                  dict(op='choose_dialog', player=1, value=1)])
        self.assertEqual(steps[upkeep+1], dict(op='assert_unused_choices', value=1))

    def test_reserves_real_defending_banding_allocation(self):
        for mode, recipient in (('paid_one_0', 'source'), ('paid_two_0', 'ally')):
            steps = self.steps('Volunteer Reserves', mode)
            begin = steps.index(dict(op='begin_blockers'))
            damage = steps.index(dict(op='combat_damage'))
            self.assertIn(dict(op='block', ref='source', other='attacker', value=1), steps[begin:damage])
            self.assertIn(dict(op='block', ref='ally', other='attacker', value=1), steps[begin:damage])
            self.assertIn(dict(op='choose_target', player=0, ref=recipient), steps[begin:damage])
            self.assertIn(dict(op='assert_card', ref=recipient, property='damage', value=1), steps[damage:])


if __name__ == '__main__':
    unittest.main()
