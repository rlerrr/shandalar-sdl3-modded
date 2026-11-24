from copy import deepcopy
import unittest

import upkeep_counters_recipes as recipe


class UpkeepCountersRecipesTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        records = recipe.profiles()['sources'] | recipe.profiles()['witnesses']
        cls.oracle,cls.rows = ({name:pin[k] for name,pin in records.items()} for k in ('oracle','registry'))

    def render(self,name,variant):
        record,row = self.oracle[name],self.rows[name]
        shape = recipe.classify(record,row['csvid'])
        home = dict(family=recipe.FAMILY,version=2,params=shape,oracle_text=record['oracle_text'],variants=[variant])
        checked = recipe.spec(home,record)
        return home,recipe.expand(name,row['csvid'],home,checked,[variant],self.rows,self.oracle)[0]

    def test_typed_declaration_pins_full_print(self):
        name = 'Aether Vial'
        home,_ = self.render(name,'ramp_0')
        for wrong in (dict(home,version=2.0),dict(home,params=dict(home['params'],csvid=6856.0)),
                      dict(home,variants=['ramp_0','ramp_0']),dict(home,oracle_text='At the beginning of each upkeep')):
            with self.assertRaises(ValueError):
                recipe.spec(wrong,self.oracle[name])

    def test_witness_is_exact(self):
        recipe.validate_witnesses(self.oracle,self.rows)
        rows = deepcopy(self.rows)
        rows['Sphere of the Suns']['handler'] = 'card_dummy'
        with self.assertRaises(ValueError):
            recipe.validate_witnesses(self.oracle,rows)

    def test_every_primary_has_both_actual_owner_casts(self):
        for name,pin in recipe.profiles()['sources'].items():
            model = recipe.classify(pin['oracle'],pin['registry']['csvid'])
            for seat in (0,1):
                variant = next(v for v in recipe.variants(model) if v.endswith(f'_{seat}'))
                _,case = self.render(name,variant)
                self.assertEqual(case['setup'][0]['player'],seat)
                self.assertEqual(sum(o['op']=='cast' and o.get('ref')=='source' for o in case['steps']),1)

    def test_zero_charge_selector_checks_four_wrong_candidates(self):
        for seat in (0,1):
            steps = self.render('Aether Vial',f'miss_{seat}')[1]['steps']
            self.assertEqual([(a['player'],a['value']) for a in steps if a['op']=='choose_zone'],[(seat,-1)])
            eligible = [a['value'] for a in steps if a['op']=='assert_offered_legal_card']
            self.assertEqual(eligible,[0,0,0,0])

    def test_exact_terminal_upkeep_counts(self):
        for name,mode,cycles in (("Azor's Elocutors",'white',5),("Azor's Elocutors",'setback',6),
                                 ('Darksteel Reactor','threshold',20)):
            steps = self.render(name,mode+'_0')[1]['steps']
            self.assertEqual([a['player'] for a in steps if a['op']=='upkeep_step'],[1,0]*cycles)
            self.assertEqual([a['value'] for a in steps if a['op']=='advance_scheduled_turn'],[-1])

    def test_cyclone_spends_real_lands_at_upkeep(self):
        steps = self.render('Cyclone','scale_0')[1]['steps']
        self.assertEqual([a['ref'] for a in steps if a['op']=='choose_mana_source'],['land_0','land_0','land_1'])
        self.assertEqual([a['value'] for a in steps if a['op']=='assert_life' and a['player']==0][-1],17)

    def test_dirge_decline_keeps_real_selector_contract(self):
        steps = self.render('Discordant Dirge','decline_0')[1]['steps']
        self.assertEqual(len([a for a in steps if a['op']=='choose_optional_zone']),1)
        self.assertEqual([a['value'] for a in steps if a['op']=='assert_forced_zone_choices'],[0])
        self.assertNotIn('decline_1',recipe.variants({'csvid':recipe.DIRGE}))
