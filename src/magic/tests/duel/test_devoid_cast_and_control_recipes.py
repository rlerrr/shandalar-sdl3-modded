"""Validate exact Devoid phrases, target probes and real timing fixtures."""
import json
from pathlib import Path
import unittest

from generate_cases import registry
import devoid_cast_and_control_recipes as model


class DevoidCastAndControlTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        data = Path(__file__).parent/'data'
        cls.oracle = json.loads((data/'oracle.json').read_bytes())['cards']
        cls.rows = {row['name']:row for row in registry()}
        cls.homes = {}
        for name, pin in model.profiles()['sources'].items():
            path, = (data/'cases/cards').glob(f"*/{pin['registry']['csvid']:05d}-*.json")
            cls.homes[name] = json.loads(path.read_bytes())['recipes'][0]

    def case(self, name, scenario):
        declaration = self.homes[name]
        return model.build(name,self.rows[name]['csvid'],declaration,model.spec(declaration,self.oracle[name]),
                           scenario,self.rows,self.oracle)

    def test_exact_whole_oracle_and_independent_witnesses(self):
        model.validate_witnesses(self.oracle,self.rows)
        for name,declaration in self.homes.items():
            record = self.oracle[name]
            self.assertEqual(model.variants(model.spec(declaration,record)),declaration['variants'])
            with self.assertRaises(ValueError):
                model.spec(declaration,dict(record,oracle_text=record['oracle_text']+'\nFlying'))

    def test_target_probe_uses_actual_selector(self):
        case = self.case('Forerunner of Slaughter','target_probes_1')
        probes = {s['other']:s['value'] for s in case['steps'] if s['op']=='probe_activation_target'}
        self.assertEqual(probes,dict(source=1,support=1,foreign=1,colored=0,relic=0))
        case = self.case('Forerunner of Slaughter','grant_enemy_1')
        source = next(s for s in case['setup'] if s['ref']=='support')
        self.assertEqual((source['name'],source['player'],source['zone']),('Culling Drone',0,'hand'))
        self.assertIn(dict(op='choose_target',ref='support',player=1),case['steps'])
        self.assertIn(dict(op='attack',ref='support',value=1),case['steps'])

    def test_cast_reward_observes_controller_and_cost_color_separately(self):
        name = "Kozilek's Sentinel"
        for label, expected, name_of_spell, actor in [
                ('own_devoid_0',2,'Culling Drone',0), ('own_artifact_0',2,'Bronze Sable',0),
                ('own_colored_0',1,'Grizzly Bears',0), ('enemy_devoid_0',1,'Culling Drone',1)]:
            case = self.case(name,label)
            witness = next(s for s in case['setup'] if s['ref']=='spell')
            self.assertEqual((witness['name'],witness['player']),(name_of_spell,actor))
            self.assertIn(dict(op='announce_spell',ref='spell'),case['steps'])
            power = [s['value'] for s in case['steps'] if s['op']=='assert_card' and
                     (s['ref'],s['property'])==('source','power')]
            self.assertEqual(power[-1],expected)

    def test_real_upkeep_and_support_transitions(self):
        late = self.case('Reaver Drone','late_gain_0')
        self.assertEqual([s['player'] for s in late['steps'] if s['op']=='upkeep_step'],[1,0,1,0])
        self.assertEqual([s['value'] for s in late['steps'] if s['op']=='assert_life' and s['player']==0][-1],19)
        for mode, allowed in [('gain_after_1',1),('lose_ally_1',0),('noncreature_1',0),('enemy_only_1',0)]:
            case = self.case('Eldrazi Aggressor',mode)
            self.assertIn(dict(op='attack',ref='source',value=allowed),case['steps'])

    def test_distinct_regeneration_cost_and_live_colorless_gate(self):
        for mode, unused in [('regen_devoid_1',0),('ally_colored_1',1),('removed_support_1',1)]:
            case = self.case('Skitterskin',mode)
            self.assertIn(dict(op='choose_regeneration',ref='source'),case['steps'])
            self.assertIn(dict(op='assert_unused_choices',value=unused),case['steps'])

    def test_library_exile_recipient_and_trample_connection(self):
        for card, mode, hit in [('Thought Harvester','own_devoid_1',True),
                                 ('Thought Harvester','protected_opponent_1',False),
                                 ('Vile Aggregate','trample_excess_1',True),
                                 ('Vile Aggregate','blocked_1',False)]:
            case = self.case(card,mode)
            exile = [s['value'] for s in case['steps'] if s['op']=='assert_zone' and
                     s['zone']=='exile' and s['player']==0]
            self.assertEqual(exile[-1],int(hit))


if __name__ == '__main__':
    unittest.main()
