from copy import deepcopy
import unittest

import departure_counters_and_search_recipes as recipe


class DepartureCountersAndSearchTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.pins = recipe.profiles()
        records = cls.pins['sources'] | cls.pins['witnesses']
        cls.oracle,cls.rows = ({n:p[k] for n,p in records.items()} for k in ('oracle','registry'))

    def render(self,name,variant):
        record = self.oracle[name]
        shape = recipe.classify(record,self.rows[name]['csvid'])
        declaration = dict(family=recipe.FAMILY,version=2,params=shape,variants=[variant],
                           oracle_text=record['oracle_text'])
        reviewed = recipe.spec(declaration,record)
        return declaration,recipe.expand(name,shape['csvid'],declaration,reviewed,[variant],self.rows,self.oracle)[0]

    def test_exact_oracle_and_typed_home(self):
        name = 'Lifeblood Hydra'
        declaration,_ = self.render(name,'pumped_0')
        for wrong in (dict(declaration,version=2.0),dict(declaration,variants=['pumped_0','pumped_0']),
                      dict(declaration,params=dict(declaration['params'],csvid=14471.0))):
            with self.assertRaises(ValueError):
                recipe.spec(wrong,self.oracle[name])
        changed = dict(self.oracle[name],oracle_text=self.oracle[name]['oracle_text'].replace('its power','its toughness'))
        with self.assertRaises(ValueError):
            recipe.spec(declaration,changed)

    def test_witness_record_integrity(self):
        recipe.validate_witnesses(self.oracle,self.rows)
        witnesses = deepcopy(self.oracle)
        witnesses['Giant Growth']['oracle_text'] = 'Target creature gets +2/+2 until end of turn.'
        with self.assertRaises(ValueError):
            recipe.validate_witnesses(witnesses,self.rows)

    def test_paid_sources_are_controller_scoped(self):
        for pin in self.pins['sources'].values():
            record,row = pin['oracle'],pin['registry']
            for seat in (0,1):
                variant = recipe.variants({'csvid':row['csvid']})
                first = next(case for case in variant if case.endswith('_'+str(seat)))
                _,data = self.render(row['name'],first)
                self.assertEqual(data['setup'][0]['player'],seat)
                self.assertEqual(sum(op['op']=='cast' and op.get('ref')=='source' for op in data['steps']),1)

    def test_hydra_pump_changes_power_without_counters(self):
        data = self.render('Lifeblood Hydra','pumped_0')[1]['steps']
        power = [op['value'] for op in data if (op['op'],op.get('ref'),op.get('property'))==('assert_card','source','power')]
        counters = [op['value'] for op in data if op['op']=='assert_counter' and op['ref']=='source']
        self.assertEqual(power,[3,6])
        self.assertEqual(counters,[3,3])
        self.assertEqual([op['value'] for op in data if op['op']=='assert_life' and op['player']==0][-1],26)

    def test_true_owned_upkeeps_and_expiry(self):
        data = self.render('Lost Auramancers','search_1')[1]['steps']
        self.assertEqual([op['player'] for op in data if op['op']=='upkeep_step'],[0,1,0,1,0,1])
        self.assertEqual([op['value'] for op in data if op['op']=='assert_counter' and op['counter_kind']=='time'],[3,3,2,1])

    def test_random_bottom_single_prefix_and_empty_reshaper(self):
        keeper = self.render('Treasure Keeper','cast_0')[1]['steps']
        final = [op['value'] for op in keeper if op['op']=='assert_zone_card' and op.get('player',0)==0][-2:]
        self.assertEqual(final,[self.rows['Island']['csvid'],self.rows['Forest']['csvid']])
        reshaper = self.render('Matter Reshaper','empty_1')[1]['steps']
        self.assertFalse(any(op['op']=='choose_dialog' for op in reshaper))
