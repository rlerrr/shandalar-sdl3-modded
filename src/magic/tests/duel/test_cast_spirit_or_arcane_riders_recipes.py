"""Check the exact Oracle gates and triggering cast fixture projections."""
import json
from pathlib import Path
import unittest

from generate_cases import registry
import cast_spirit_or_arcane_riders_recipes as model


class SpiritOrArcaneTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        data=Path(__file__).parent/'data'
        cls.oracle=json.loads((data/'oracle.json').read_bytes())['cards']
        cls.rows={row['name']:row for row in registry()}
        cls.homes={}
        for name,pin in model.profiles()['sources'].items():
            path,=(data/'cases/cards').glob(f"*/{pin['registry']['csvid']:05d}-*.json")
            cls.homes[name]=json.loads(path.read_bytes())['recipes'][0]

    def case(self,name,variant):
        declaration=self.homes[name]
        return model.build(name,self.rows[name]['csvid'],declaration,model.spec(declaration,self.oracle[name]),
                           variant,self.rows,self.oracle)

    def test_full_pinned_oracle(self):
        model.validate_witnesses(self.oracle,self.rows)
        for name,home in self.homes.items():
            oracle=self.oracle[name]
            shape=model.spec(home,oracle)
            self.assertEqual(model.variants(shape),home['variants'])
            with self.assertRaises(ValueError):
                model.spec(home,dict(oracle,oracle_text=oracle['oracle_text']+'\nFlying'))

    def test_controller_and_explicit_paid_spell_announcement(self):
        for variant,name,owner in [('own_spirit_0',model.SPIRIT,0),('own_arcane_0',model.ARCANE,0),
                                   ('own_other_creature_0',model.OTHER_CREATURE,0),('enemy_arcane_0',model.ARCANE,1)]:
            case=self.case('Earthshaker',variant)
            witness,=(item for item in case['setup'] if item['ref']=='spell')
            self.assertEqual((witness['name'],witness['player']),(name,owner))
            ops=[s['op'] for s in case['steps'] if s.get('ref')=='spell']
            self.assertLess(ops.index('cast'),ops.index('announce_spell'))

    def test_both_sides_and_source_mana_value(self):
        case=self.case('Celestial Kirin','self_mv_0')
        deaths={(s['player'],s['index']) for s in case['steps']
                if s['op']=='assert_zone_multiplicity' and s['zone']=='graveyard'}
        self.assertEqual(deaths,{(0,7612),(0,self.rows['Hill Giant']['csvid']),(1,self.rows['Hill Giant']['csvid'])})

    def test_tutor_arcane_search_and_real_optional_gate(self):
        case=self.case('Tallowisp','own_arcane_0')
        witness,=(item for item in case['setup'] if item['ref']=='spell')
        self.assertEqual(witness['name'],model.ARCANE_TUTOR)
        self.assertIn(dict(op='choose_trigger_source',player=0,ref='source'),case['steps'])
        self.assertIn(dict(op='assert_shuffle_count',player=0,value=1),case['steps'])
        start=case['steps'].index(dict(op='observe_search_filters',value=1))
        search=next(i for i,s in enumerate(case['steps']) if s['op']=='choose_zone_card')
        probe=next(i for i,s in enumerate(case['steps']) if s['op']=='assert_offered_legal_card')
        self.assertLess(start,search)
        self.assertLess(search,probe)

        declined=self.case('Tallowisp','decline_0')
        self.assertFalse(any(s['op'] in ('choose_trigger_source','choose_zone_card') for s in declined['steps']))
        self.assertFalse(any(s['op']=='assert_shuffle_count' and s['value'] for s in declined['steps']))

    def test_followers_keep_lifecycle_and_recipient(self):
        blink=self.case('Hikari, Twilight Guardian','own_spirit_0')
        steps=blink['steps']
        self.assertTrue(any(s['op']=='assert_zone_multiplicity' and s['zone']=='exile' and s['index']==7232 for s in steps))
        self.assertLess(next(i for i,s in enumerate(steps) if s['op']=='cleanup'),
                        next(i for i,s in enumerate(steps) if s['op']=='bind_permanent'))
        steal=self.case('Skyfire Kirin','own_spirit_1')
        bear=self.rows[model.OTHER_CREATURE]['csvid']
        power=[s['player'] for s in steal['steps'] if s.get('property')=='power' and
               s.get('index')==bear and s['op']=='assert_matching_permanents']
        self.assertEqual(power,[0,1,0])
        planned=steal['steps'].index(dict(op='choose_target',player=1,ref='recipient'))
        receipt=steal['steps'].index(dict(op='assert_unused_choices',value=0))
        self.assertLess(planned,receipt)
        self.assertFalse(any(s['op']=='bind_permanent' for s in steal['steps']))
        safe=self.case('Skyfire Kirin','wrong_mv_1')
        self.assertFalse(any(s['op']=='choose_target' for s in safe['steps']))

    def test_followers_type_keywords_and_hand_scope(self):
        art=self.case('Jade Idol','expires_0')
        self.assertEqual([s['value'] for s in art['steps'] if s['op']=='assert_matching_permanents'
                          and s['property']=='subtype_Spirit'],[1,0])
        fear=self.case('Kami of the Waning Moon','black_blocks_1')
        self.assertIn(dict(op='block',ref='blocker',other='ally',value=1),fear['steps'])
        exile=self.case("Kyoki, Sanity's Eclipse",'own_arcane_1')
        self.assertIn(dict(op='choose_zone_card',player=0,value=self.rows['Forest']['csvid']),exile['steps'])
        opposed=self.case("Kyoki, Sanity's Eclipse",'own_arcane_0')
        self.assertIn(dict(op='choose_zone_card',player=1,value=self.rows['Forest']['csvid']),opposed['steps'])
        tutor=self.case('Tallowisp','own_spirit_1')
        self.assertIn(dict(op='choose_zone_card',player=1,value=self.rows[model.AURA]['csvid']),tutor['steps'])


if __name__=='__main__':
    unittest.main()
