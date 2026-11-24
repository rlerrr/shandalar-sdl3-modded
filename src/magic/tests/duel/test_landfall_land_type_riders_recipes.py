"""Guard actual typed-land plays and printed effects in the five-card projection."""
import json
from pathlib import Path
import unittest

from generate_cases import registry
import landfall_land_type_riders_recipes as model


class LandTypeTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        data=Path(__file__).parent/'data'
        cls.oracle=json.loads((data/'oracle.json').read_bytes())['cards']
        cls.rows={row['name']:row for row in registry()}
        cls.homes={}
        for name,source in model.profiles()['sources'].items():
            path,=(data/'cases/cards').glob(f"*/{source['registry']['csvid']:05d}-*.json")
            cls.homes[name]=json.loads(path.read_bytes())['recipes'][0]

    def case(self,name,variant):
        home=self.homes[name]
        return model.build(name,self.rows[name]['csvid'],home,model.spec(home,self.oracle[name]),
                           variant,self.rows,self.oracle)

    @staticmethod
    def checks(case,op,**fields):
        return [item['value'] for item in case['steps'] if item['op']==op and
                all(item.get(key)==value for key,value in fields.items())]

    def test_whole_pinned_profile_and_normal_primaries(self):
        model.validate_witnesses(self.oracle,self.rows)
        for name,home in self.homes.items():
            oracle=self.oracle[name]
            shape=model.spec(home,oracle)
            self.assertEqual(home['variants'],model.variants(shape))
            with self.assertRaises(ValueError):
                model.spec(home,dict(oracle,oracle_text=oracle['oracle_text']+'\nFlying'))

    def test_true_basic_type_for_ordinary_and_two_type_land(self):
        for name,expected in [('Akoum Hellkite',(19,18)),('Guul Draz Overseer',(3,4))]:
            case=self.case(name,'ordinary_0')
            dual=self.case(name,'both_subtypes_0')
            for row in (case,dual):
                self.assertEqual(self.checks(row,'play_land',ref='land'),[1])
            if name=='Akoum Hellkite':
                values=[self.checks(row,'assert_life',player=1)[-1] for row in (case,dual)]
            else:
                values=[self.checks(row,'assert_card',ref='ally',property='power')[-1] for row in (case,dual)]
            self.assertEqual(values,list(expected))

    def test_plains_graveyard_filter_destinations(self):
        cases=[self.case('Emeria Shepherd',mode+'_0') for mode in ('ordinary','matched','matched_hand','no_legal_grave')]
        for case,signals in zip(cases,([], [1], [0], [])):
            self.assertEqual(self.checks(case,'choose_dialog'),signals)
        selected=self.rows[model.BEAR]['csvid']
        self.assertIn(dict(op='assert_zone_multiplicity',player=0,zone='battlefield',index=selected,value=1),cases[1]['steps'])
        self.assertIn(dict(op='assert_zone_multiplicity',player=0,zone='hand',index=selected,value=1),cases[2]['steps'])
        self.assertFalse(any(item['op']=='choose_zone_card' for item in cases[3]['steps']))

    def test_trap_and_locked_untap_use_real_turns(self):
        for mode,expected in [('ordinary_0',[0,1,0,0,0]),('both_subtypes_0',[0,1,1,1,0])]:
            row=self.case('Guardian of Tazeem',mode)
            self.assertEqual(self.checks(row,'assert_card',ref='victim',property='tapped'),expected)
            self.assertEqual([s['player'] for s in row['steps'] if s['op']=='untap_step'],[1,0,1])
        for mode,life in [('matched_0',17),('different_land_0',17),('next_turn_0',19),('foreign_0',19)]:
            row=self.case('Searing Blaze',mode)
            self.assertEqual(self.checks(row,'assert_life',player=1)[-1],life)


if __name__=='__main__':
    unittest.main()
