"""Guard real copied Equipment identity, lifecycle and qualification."""
import json
import unittest

from generate_cases import registry
import equipment_combat_counters_and_copies_recipes as model


class EquipmentCombatCountersAndCopiesTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        data = model.PROFILE.parent
        cls.oracle = json.loads((data / 'oracle.json').read_bytes())['cards']
        cls.rows = {row['name']: row for row in registry() if row['kind'] != 'internal'}
        cls.homes = {}
        for name, pin in model.profiles()['sources'].items():
            home, = (data / 'cases/cards').glob(f"*/{pin['registry']['csvid']:05d}-*.json")
            cls.homes[name] = json.loads(home.read_bytes())['recipes'][0]

    def steps(self, name, variant):
        declaration = self.homes[name]
        shape = model.spec(declaration, self.oracle[name])
        return model.build(name, self.rows[name]['csvid'], declaration, shape, variant, self.rows, self.oracle)['steps']

    def test_sources_are_whole(self):
        model.validate_witnesses(self.oracle, self.rows)
        for name, declaration in self.homes.items():
            shape = model.spec(declaration, self.oracle[name])
            self.assertEqual(declaration['variants'], model.variants(shape))
            with self.assertRaises(ValueError):
                model.spec(declaration, dict(self.oracle[name], oracle_text='Equip {2}'))

    def test_copy_is_generated_before_payment_and_moves_the_attack_bonus(self):
        name = 'Bloodforged Battle-Axe'
        for seat in (0, 1):
            steps = self.steps(name, f'clone_reequip_{seat}')
            attack = steps.index(dict(op='combat_damage'))
            removal = steps.index(dict(op='cast', ref='removal', value=1))
            bound = steps.index(dict(op='bind_permanent_copy', ref='copy', value=model.AXE, player=seat, index=0))
            equip = steps.index(dict(op='activate', ref='copy', value=1))
            self.assertLess(attack, removal)
            self.assertLess(removal, bound)
            self.assertLess(bound, equip)
            self.assertIn(dict(op='assert_matching_permanents', player=seat, index=model.AXE, property='token', value=1),
                          steps[removal:bound])
            self.assertIn(dict(op='set_mana', player=seat, color=0, value=2), steps[bound:equip])
            for ref, power in (('host', 2), ('other', 5)):
                self.assertIn(dict(op='assert_card', ref=ref, property='power', value=power), steps[equip:])

    def test_copy_triggers_only_for_equipped_combat_damage_to_player(self):
        name = 'Bloodforged Battle-Axe'
        for seat in (0, 1):
            for mode, count in (('clone_chain', 2), ('clone_blocked', 1), ('clone_noncombat', 1),
                                ('unattached_stays_idle', 3), ('other_attacker', 1)):
                steps = self.steps(name, f'{mode}_{seat}')
                observed = [item['value'] for item in steps if item['op'] == 'assert_zone_multiplicity'
                            and item['player'] == seat and item['index'] == model.AXE and item['zone'] == 'battlefield']
                self.assertEqual(observed[-1], count)


if __name__ == '__main__':
    unittest.main()
