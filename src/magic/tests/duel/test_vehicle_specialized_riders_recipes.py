"""Guard complete legendary Vehicle rules and real choice/phase boundaries."""

import json
from pathlib import Path
import unittest

import generate_cases
import vehicle_specialized_riders_recipes as model


SKY = 'Skysovereign, Consul Flagship'
HEART = 'Heart of Kiran'
WEATHERLIGHT = 'Weatherlight'


class SpecializedVehicleTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.oracle = json.loads((Path(generate_cases.__file__).parent/'data/oracle.json').read_bytes())['cards']
        cls.rows = {r['name']:r for r in generate_cases.registry() if r['kind']!='internal'}
        cls.homes = {}
        for name,pin in model.pins()['sources'].items():
            path, = (model.PROFILE.parent/'cases/cards').glob(f'*/{pin["registry"]["csvid"]:05d}-*.json')
            cls.homes[name] = json.loads(path.read_bytes())['recipes'][0]

    def case(self,name,variant):
        home = self.homes[name]
        return model.expand(name,self.rows[name]['csvid'],home,model.spec(home,self.oracle[name]),[variant],self.rows,self.oracle)[0]

    def test_complete_printed_clauses_and_variant_guards(self):
        model.validate_witnesses(self.oracle,self.rows)
        self.assertEqual(set(self.homes),{SKY,HEART,WEATHERLIGHT})
        for name,home in self.homes.items():
            record = self.oracle[name]
            self.assertEqual(home['oracle_text'],record['oracle_text'])
            self.assertEqual(home['variants'],model.variants(model.spec(home,record)))
            with self.assertRaises(ValueError):
                model.spec(dict(home,params=dict(home['params'],threshold=True)),record)
            with self.assertRaises(ValueError):
                model.spec(dict(home,variants=['cast_1','cast_1']),record)
            with self.assertRaises(ValueError):
                model.spec(home,dict(record,oracle_text=record['oracle_text']+'\nHaste'))

    def test_paid_crew_and_actual_type_boundary(self):
        for name in self.homes:
            for seat in (0,1):
                steps = self.case(name,f'ordered_exact_{seat}')['steps']
                activate = steps.index(dict(op='activate',ref='source',value=1))
                resolve = steps.index(dict(op='resolve'))
                self.assertIn(dict(op='assert_stack',value=1),steps[activate:resolve])
                self.assertIn(dict(op='assert_card',ref='pilot_0',property='tapped',value=1),steps[activate:resolve])
                self.assertIn(dict(op='can_target_creature',ref='type_probe',other='source',value=0),steps[activate:resolve])
                self.assertIn(dict(op='can_target_creature',ref='type_probe',other='source',value=1),steps[resolve:])

    def test_sky_entry_and_attack_target_real_opposing_permanents(self):
        for mode,phase,receipt in [
                ('entry_creature','resolve',dict(op='assert_card',ref='victim',property='damage',value=3)),
                ('attack_walker','finish_attackers',dict(op='assert_counter',ref='victim',counter_kind='loyalty',value=3)),
                ('attack_walker_lethal','finish_attackers',dict(op='assert_zone_multiplicity',player=0,zone='graveyard',index=self.rows['Jace Beleren']['csvid'],value=1))]:
            c = self.case(SKY,mode+'_1')
            steps = c['steps']
            cut = steps.index(dict(op=phase))
            victim, = [s for s in c['setup'] if s['ref']=='victim']
            friendly, = [s for s in c['setup'] if s['ref']=='friendly']
            self.assertEqual((victim['player'],friendly['player']),(0,1))
            self.assertEqual(steps[cut-1],dict(op='choose_target',ref='victim',player=1))
            self.assertIn(receipt,steps[cut:])
            self.assertIn(dict(op='assert_card',ref='friendly',property='damage',value=0),steps[cut:])
        for m in ('entry_no_enemy','attack_no_enemy'):
            steps = self.case(SKY,m+'_0')['steps']
            self.assertFalse(any(s['op']=='choose_target' and s.get('ref')=='victim' for s in steps))

    def test_heart_loyalty_payment_precedes_resolution_and_removes_no_other_resources(self):
        steps = self.case(HEART,'both_loyalty_0')['steps']
        at = steps.index(dict(op='activate',ref='source',value=1))
        resolution = steps.index(dict(op='resolve'))
        self.assertEqual(steps[at-2:at],[dict(op='choose_dialog',player=0,value=0),dict(op='choose_target',player=0,ref='walker')])
        self.assertIn(dict(op='assert_counter',ref='walker',counter_kind='loyalty',value=3),steps[:at])
        self.assertIn(dict(op='assert_counter',ref='walker',counter_kind='loyalty',value=2),steps[at:resolution])
        self.assertIn(dict(op='assert_card',ref='pilot_0',property='tapped',value=0),steps[at:resolution])
        self.assertIn(dict(op='can_target_creature',ref='type_probe',other='source',value=1),steps[resolution:])
        foreign = self.case(HEART,'foreign_walker_1')['steps']
        self.assertIn(dict(op='activate',ref='source',value=0),foreign)
        self.assertNotIn('choose_target',[s['op'] for s in foreign])

    def test_heart_last_loyalty_uses_damage_then_actual_cost_with_pending_activation(self):
        steps = self.case(HEART,'last_loyalty_0')['steps']
        damage = steps.index(dict(op='cast',ref='damage_spell',value=1))
        at = steps.index(dict(op='activate',ref='source',value=1))
        resolution = steps.index(dict(op='resolve'),at)
        self.assertLess(damage,at)
        self.assertIn(dict(op='assert_counter',ref='walker',counter_kind='loyalty',value=1),steps[damage:at])
        walker = self.rows['Jace Beleren']['csvid']
        self.assertIn(dict(op='assert_zone_multiplicity',player=0,zone='graveyard',index=walker,value=1),steps[at:resolution])
        self.assertIn(dict(op='assert_stack',value=1),steps[at:resolution])

    def test_flying_and_vigilance_through_actual_attack_and_block(self):
        steps = self.case(HEART,'crew_ground_0')['steps']
        attack = steps.index(dict(op='attack',ref='source',value=1))
        self.assertIn(dict(op='assert_card',ref='source',property='tapped',value=0),steps[attack:])
        self.assertIn(dict(op='block',ref='blocker',other='source',value=0),steps[attack:])
        for name,variant in [(SKY,'attack_air_1'),(HEART,'loyalty_air_0'),(WEATHERLIGHT,'blocked_1')]:
            self.assertIn(dict(op='block',ref='blocker',other='source',value=1),self.case(name,variant)['steps'])

    def test_all_three_historic_classes_use_real_optional_selector(self):
        for mode,name in [('artifact',model.ARTIFACT),('legendary',model.LEGENDARY),('saga',model.SAGA)]:
            steps = self.case(WEATHERLIGHT,mode+'_0')['steps']
            hit = steps.index(dict(op='combat_damage'))
            csvid = self.rows[name]['csvid']
            self.assertIn(dict(op='choose_zone_card',player=0,value=csvid),steps[:hit])
            self.assertIn(dict(op='choose_dialog',player=0,value=0),steps[:hit])
            self.assertIn(dict(op='assert_offered_selection_required',value=0),steps[hit:])
            self.assertIn(dict(op='assert_offered_legal_card',index=csvid,value=1),steps[hit:])
            self.assertIn(dict(op='assert_offered_legal_card',index=self.rows['Forest']['csvid'],value=0),steps[hit:])
            self.assertIn(dict(op='assert_zone_multiplicity',player=0,zone='hand',index=csvid,value=1),steps[hit:])
        for mode in ('blocked','walker','empty'):
            self.assertFalse(any(s['op']=='choose_zone_card' for s in self.case(WEATHERLIGHT,mode+'_0')['steps']))

    def test_top_five_moves_below_unchanged_prefix_without_an_order_choice(self):
        for mode,remaining,top in [('fifth',3,'Plains'),('sixth_locked',4,model.ARTIFACT)]:
            c = self.case(WEATHERLIGHT,mode+'_0')
            steps = c['steps']
            hit = steps.index(dict(op='combat_damage'))
            self.assertIn(dict(op='assert_unused_choices',value=remaining),steps[hit:])
            self.assertIn(dict(op='assert_zone_card',player=0,zone='library',index=0,value=self.rows[top]['csvid']),steps[hit:])
            self.assertIn(dict(op='assert_zone_card',player=0,zone='library',index=1,value=self.rows['Mountain']['csvid']),steps[hit:])
            self.assertIn(dict(op='assert_zone_multiplicity',player=0,zone='library',index=self.rows['Island']['csvid'],value=1),steps[hit:])
        steps = self.case(WEATHERLIGHT,'decline_1')['steps']
        decline = dict(op='choose_optional_zone',player=1,value=self.rows[model.ARTIFACT]['csvid'])
        self.assertEqual(steps.count(decline),2)
        self.assertIn(dict(op='assert_unused_choices',value=0),steps)
        self.assertEqual(sum(s['op']=='choose_optional_zone' for s in self.case(WEATHERLIGHT,'decline_0')['steps']),1)
        self.assertIn(dict(op='assert_zone',player=1,zone='hand',value=0),steps[-32:])

    def test_replay_keeps_real_lifecycle_choices(self):
        for name,variant,action in [(SKY,'entry_replay_0','cast'),(HEART,'loyalty_pending_replay_1','resolve'),(WEATHERLIGHT,'replay_0','combat_damage')]:
            steps = self.case(name,variant)['steps']
            before = steps[steps.index(dict(op='capture'))+1:steps.index(dict(op='record_state'))]
            self.assertEqual(before,steps[steps.index(dict(op='restore'))+1:-1])
            self.assertIn(action,[s['op'] for s in before])


if __name__=='__main__':
    unittest.main()
