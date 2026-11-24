import copy
import json
import unittest
from pathlib import Path

from generate_cases import registry
from recipes import expand
import death_triggered_edict_recipes as family


class DeathTriggeredEdictTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.duel = Path(__file__).parent
        cls.oracle = json.loads((cls.duel/'data/oracle.json').read_text(encoding='utf8'))['cards']
        cls.registry = {row['name']:row for row in registry()}
        cls.sources = family.pins()['sources']

    def declaration(self, name, variants):
        record = self.oracle[name]
        return dict(family=family.FAMILY, version=2, oracle_text=record['oracle_text'],
                    params=dict(schema=1, **family.oracle_model(record)), variants=variants)

    def case(self, name, mode, p=0):
        d = self.declaration(name, [f'{mode}_{p}'])
        return expand(name,self.registry[name]['csvid'],d,self.oracle[name],self.registry,self.oracle)[0]

    def has(self, case, **fields):
        return any(all(s.get(k)==v for k,v in fields.items()) for s in case['steps'])

    def zone(self, case, name, p, zone, count):
        return self.has(case,op='assert_zone_multiplicity',index=self.registry[name]['csvid'],player=p,zone=zone,value=count)

    def test_whole_oracle_and_exact_typed_pins(self):
        family.validate_witnesses(self.oracle,self.registry)
        self.assertEqual({s['registry']['csvid'] for s in self.sources.values()},{2925,10736,13871,1050})
        for name in self.sources:
            d = self.declaration(name,['effect_0']); family.spec(d,self.oracle[name])
            d['params']['schema'] = True
            with self.assertRaises(ValueError): family.spec(d,self.oracle[name])
            with self.assertRaises(ValueError): family.oracle_model(dict(self.oracle[name],oracle_text='Flying\n'+self.oracle[name]['oracle_text']))
        for name in ('Burning Sands',"Martyr's Bond",'Earthlink'):
            with self.assertRaises(ValueError): family.oracle_model(self.oracle[name])
        mutated = copy.deepcopy(self.oracle); mutated['Murder']['oracle_text'] = 'changed'
        with self.assertRaises(ValueError): family.validate_witnesses(mutated,self.registry)

    def test_complete_home_matrices_use_real_actions_from_public_router(self):
        total = 0
        for name, pin in self.sources.items():
            i = pin['registry']['csvid']
            home = self.duel/f'data/cases/cards/{i//100:03}/{i:05}-{name.lower().replace(" ","-")}.json'
            d, = json.loads(home.read_text(encoding='utf8'))['recipes']
            self.assertEqual(d['variants'],family.variants(family.spec(d,self.oracle[name])))
            cases = expand(name,i,d,self.oracle[name],self.registry,self.oracle); total += len(cases)
            for case in cases:
                self.assertTrue(self.has(case,op='assert_choices_consumed'))
                self.assertFalse(any(s['op'] in ('destroy','set_counter','dispatch_gfp','move_card','set_life','choose_trigger') for s in case['steps']))
        self.assertEqual(total,214)

    def test_gatekeeper_each_player_uses_their_own_legal_choice_and_can_lose_only_one_side(self):
        for p in (0,1):
            c = self.case('Abyssal Gatekeeper','effect',p)
            self.assertTrue(self.zone(c,'Abyssal Gatekeeper',p,'graveyard',1))
            self.assertTrue(self.zone(c,'Runeclaw Bear',p,'graveyard',1))
            self.assertTrue(self.zone(c,'Craw Wurm',1-p,'graveyard',1))
            self.assertTrue(self.has(c,op='choose_target',player=0,ref='own_recipient' if p==0 else 'recipient'))
            c = self.case('Abyssal Gatekeeper','one_side_only',p)
            self.assertTrue(self.zone(c,'Runeclaw Bear',p,'graveyard',1))
            self.assertFalse(self.has(c,op='choose_target',ref='recipient'))
        self.assertTrue(self.has(self.case('Abyssal Gatekeeper','opposing_turn_death'),op='turn',player=1,value=0))

    def test_human_chooser_can_select_nondefault_creature(self):
        for name in self.sources:
            p = 0 if name=='Abyssal Gatekeeper' else 1
            c = self.case(name,'chooser_alternate',p)
            self.assertTrue(self.has(c,op='choose_target',ref='alternative',player=0))
            self.assertTrue(self.zone(c,'Slippery Bogle',0,'graveyard',1))
            default = 'Runeclaw Bear' if p==0 else 'Craw Wurm'
            self.assertTrue(self.zone(c,default,0,'battlefield',1))

    def test_controlled_creature_death_uses_controller_and_graveyard_uses_owner(self):
        for name in ('Grave Pact','Dictate of Erebos','Butcher of Malakir'):
            for p in (0,1):
                c = self.case(name,'stolen_death',p)
                self.assertTrue(self.zone(c,'Muck Rats',1-p,'graveyard',1)); self.assertTrue(self.zone(c,'Craw Wurm',1-p,'graveyard',1))
                c = self.case(name,'lent_death',p)
                self.assertTrue(self.zone(c,'Muck Rats',p,'graveyard',1)); self.assertTrue(self.zone(c,'Craw Wurm',1-p,'battlefield',1))
                c = self.case(name,'token_death',p)
                self.assertTrue(self.zone(c,'Muck Rats',p,'graveyard',0)); self.assertTrue(self.zone(c,'Craw Wurm',1-p,'graveyard',1))

    def test_creature_sacrifice_does_not_target_and_preserves_owned_receipts(self):
        for name in self.sources:
            for mode, card in [('hexproof_recipient','Slippery Bogle'),('indestructible_recipient','Darksteel Myr'),('player_shroud','Craw Wurm')]:
                c = self.case(name,mode)
                self.assertFalse(self.has(c,op='choose_player'))
                self.assertTrue(self.zone(c,card,1,'graveyard',1))
            c = self.case(name,'stolen_recipient'); self.assertTrue(self.zone(c,'Craw Wurm',0,'graveyard',1))
            c = self.case(name,'token_recipient'); self.assertTrue(self.zone(c,'Craw Wurm',1,'graveyard',0))
            c = self.case(name,'no_recipient_creatures'); self.assertTrue(self.zone(c,'Howling Mine',1,'battlefield',1))

    def test_actual_sweeps_account_for_all_simultaneous_deaths(self):
        for name in ('Grave Pact','Dictate of Erebos','Butcher of Malakir'):
            c = self.case(name,'simultaneous',1)
            self.assertTrue(self.has(c,op='cast',ref='sweep',value=1))
            self.assertTrue(self.zone(c,'Muck Rats',1,'graveyard',2))
            self.assertTrue(self.zone(c,'Craw Wurm',0,'graveyard',2))
            c = self.case(name,'simultaneous_source',1)
            self.assertTrue(self.zone(c,name,1,'graveyard',1))
            self.assertTrue(self.zone(c,'Darksteel Myr',0,'graveyard',3 if name=='Butcher of Malakir' else 2))

    def test_planar_offer_observation_and_inferno_different_production_path(self):
        for name in self.sources:
            for p in (0,1):
                for mode, sweeper in [('simultaneous_source','Planar Cleansing'),('simultaneous_damage','Inferno')]:
                    case = self.case(name,mode,p)
                    self.assertTrue(any(s['ref']=='sweep' and s['name']==sweeper for s in case['setup']))
                    steps = case['steps']
                    cast = next(i for i,s in enumerate(steps) if s['op']=='cast' and s.get('ref')=='sweep')
                    resolved = next(i for i in range(cast+1,len(steps)) if steps[i]['op']=='resolve')
                    first_receipt = steps[resolved+1]
                    if sweeper == 'Planar Cleansing':
                        self.assertEqual(first_receipt,dict(op='assert_unused_choices',value=0))
                    else:
                        self.assertEqual(first_receipt,dict(op='assert_choices_consumed'))
                        self.assertTrue(all(self.has(case,op='assert_life',player=q,value=14) for q in (0,1)))
                        survive = not family.oracle_model(self.oracle[name])['creature']
                        self.assertTrue(self.zone(case,name,p,'battlefield',int(survive)))
                        self.assertTrue(self.zone(case,name,p,'graveyard',int(not survive)))
                    wanted = {'Abyssal Gatekeeper':1,'Butcher of Malakir':3,'Dictate of Erebos':2,'Grave Pact':2}[name]
                    self.assertTrue(self.zone(case,'Darksteel Myr',1-p,'graveyard',wanted))

    def test_bounce_exile_counter_and_live_other_death_do_not_trigger(self):
        for name in self.sources:
            for mode in ('bounce','exile','other_death'):
                c = self.case(name,mode)
                self.assertTrue(self.zone(c,'Craw Wurm',1,'battlefield',1))
            c = self.case(name,'countered'); ops = [s['op'] for s in c['steps']]
            self.assertLess(ops.index('announce_spell'),ops.index('cast_response'))
            self.assertNotIn('choose_target',ops)
            self.assertTrue(self.zone(c,name,0,'graveyard',1))

    def test_flying_and_flash_use_production_combat_and_pending_spell(self):
        for p in (0,1):
            c = self.case('Butcher of Malakir','flying_unblocked',p)
            self.assertTrue(self.has(c,op='block',ref='recipient',other='source',value=0))
            self.assertTrue(self.has(c,op='assert_life',player=1-p,value=15))
            c = self.case('Butcher of Malakir','flying_reach',p)
            self.assertTrue(self.has(c,op='block',ref='recipient',other='source',value=1))
            self.assertTrue(self.zone(c,'Butcher of Malakir',p,'battlefield',1))
            c = self.case('Dictate of Erebos','flash_before_death',p); ops = [s['op'] for s in c['steps']]
            self.assertLess(ops.index('announce_spell'),ops.index('cast_response'))
            self.assertTrue(self.zone(c,'Craw Wurm',1-p,'graveyard',1))
            c = self.case('Dictate of Erebos','flash_after_death',p)
            self.assertTrue(self.zone(c,'Craw Wurm',1-p,'battlefield',1))

    def test_real_sacrifice_cost_deaths_and_pending_replay(self):
        for name in self.sources:
            c = self.case(name,'self_sacrifice' if name=='Abyssal Gatekeeper' else 'sacrifice_cost_death')
            self.assertTrue(self.has(c,op='activate',value=1)); self.assertTrue(self.zone(c,'Craw Wurm',1,'graveyard',1))
            c = self.case(name,'pending_snapshot',1); ops = [s['op'] for s in c['steps']]
            self.assertTrue(self.has(c,op='assert_recorded_state'))
            at = ops.index('capture'); self.assertEqual(c['steps'][at-1],dict(op='assert_stack',value=1))
            self.assertEqual(ops[at:].count('choose_target'),2)


if __name__ == '__main__':
    unittest.main()
