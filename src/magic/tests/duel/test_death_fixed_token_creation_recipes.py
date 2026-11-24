import copy
import json
import unittest
from pathlib import Path

from generate_cases import registry
from recipes import expand
import death_fixed_token_creation_recipes as family


class DeathFixedTokenCreationTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.duel = Path(__file__).parent
        cls.oracle = json.loads((cls.duel/'data/oracle.json').read_text(encoding='utf8'))['cards']
        cls.registry = {r['name']:r for r in registry()}
        cls.sources = family.pins()['sources']

    def declaration(self,name,mode):
        r = self.oracle[name]
        return dict(family=family.FAMILY,version=2,params=dict(schema=1,**family.oracle_model(r)),
                    oracle_text=r['oracle_text'],variants=[mode])

    def case(self,name,mode,p=0):
        d = self.declaration(name,f'{mode}_{p}')
        return expand(name,self.registry[name]['csvid'],d,self.oracle[name],self.registry,self.oracle)[0]

    def has(self,c,**fields):
        return any(all(s.get(k)==v for k,v in fields.items()) for s in c['steps'])

    def token_count(self,c,p,tid,n):
        return self.has(c,op='assert_zone_multiplicity',player=p,zone='battlefield',index=tid,value=n)

    def zone(self,c,p,name,zone,n):
        return self.has(c,op='assert_zone_multiplicity',player=p,zone=zone,index=self.registry[name]['csvid'],value=n)

    def test_typed_whole_pins_and_implemented_source_handlers(self):
        family.validate_witnesses(self.oracle,self.registry)
        for n in self.sources:
            d = self.declaration(n,'effect_0'); family.spec(d,self.oracle[n]); d['params']['token']['count']=True
            with self.assertRaises(ValueError): family.spec(d,self.oracle[n])
            with self.assertRaises(ValueError): family.oracle_model(dict(self.oracle[n],oracle_text=self.oracle[n]['oracle_text']+' Draw a card.'))
        for n in ('Chasm Skulker','Mitotic Slime'):
            with self.assertRaises(ValueError): family.oracle_model(self.oracle[n])
        changed = copy.deepcopy(self.registry); changed['Dog']['csvid']=1
        with self.assertRaises(ValueError): family.validate_witnesses(self.oracle,changed)

    def test_all_selected_homes_generate_complete_matrix(self):
        total = 0
        for n,pin in self.sources.items():
            i = pin['registry']['csvid']
            path, = (self.duel/f'data/cases/cards/{i//100:03}').glob(f'{i:05}-*.json')
            d, = json.loads(path.read_text(encoding='utf8'))['recipes']
            self.assertEqual(d['variants'],family.variants(family.spec(d,self.oracle[n])))
            cases = expand(n,i,d,self.oracle[n],self.registry,self.oracle); total += len(cases)
            self.assertFalse(any(s['op'] in ('destroy','set_counter','dispatch_gfp','move_card','set_life') for c in cases for s in c['steps']))
        self.assertEqual(total,218)

    def test_created_tokens_check_identity_type_color_stats_tapped_and_card_absence(self):
        for n,pin in self.sources.items():
            for p in (0,1):
                c = self.case(n,'effect',p); t=family.oracle_model(self.oracle[n])['token']; tid=pin['token_registry']['csvid']
                count=t['count']+int(n=='Servo Schematic')
                self.assertTrue(self.token_count(c,p,tid,count)); self.assertTrue(self.token_count(c,1-p,tid,0))
                for prop,v in [('token',1),('artifact',int(t['artifact'])),('power',t['power']),('toughness',t['toughness']),('tapped',int(t['tapped'])),('color',t['color'])]:
                    self.assertTrue(self.has(c,op='assert_matching_permanents',player=p,index=tid,property=prop,value=v))
                self.assertTrue(all(self.has(c,op='assert_zone_multiplicity',player=p,zone=z,index=tid,value=0)
                                    for z in ('hand','graveyard','exile','library')))

    def test_mongrel_pack_only_real_combat_death_creates_dogs(self):
        for p in (0,1):
            c=self.case('Mongrel Pack','effect',p); s=c['steps']
            begin=next(i for i,a in enumerate(s) if a['op']=='begin_attackers'); cast=next(i for i,a in enumerate(s) if a['op']=='cast' and a['ref']=='removal')
            end=next(i for i,a in enumerate(s) if a['op']=='end_combat'); self.assertLess(begin,cast); self.assertLess(cast,end)
            self.assertTrue(self.token_count(c,p,18763,4)); self.assertFalse(self.token_count(self.case('Mongrel Pack','noncombat_death',p),p,18763,4))
            c=self.case('Mongrel Pack','combat_blocked',p); self.assertTrue(self.has(c,op='block',ref='blocker',other='source',value=1))
            self.assertTrue(self.zone(c,1-p,'Runeclaw Bear','graveyard',1));self.assertTrue(self.token_count(c,p,18763,4))

    def test_tuktuk_haste_and_named_legendary_artifact_token_use_real_actions(self):
        for p in (0,1):
            c=self.case('Tuktuk the Explorer','fresh_haste',p); self.assertTrue(self.has(c,op='attack',ref='source',value=1))
            self.assertFalse(self.has(c,op='advance_turn'));self.assertTrue(self.has(c,op='assert_life',player=1-p,value=19))
            c=self.case('Tuktuk the Explorer','legend_once',p); self.assertTrue(self.has(c,op='bind_permanent_copy',ref='old_token',value=18845))
            self.assertTrue(self.has(c,op='choose_target',player=p,ref='old_token'))
            self.assertTrue(self.zone(c,p,'Tuktuk the Explorer','graveyard',2));self.assertTrue(self.token_count(c,p,18845,1))
            self.assertTrue(all(self.has(c,op='assert_matching_permanents',index=18845,property='subtype_'+x,value=1) for x in ('Goblin','Golem')))

    def test_servo_real_entry_and_artifact_destruction_add_separate_tokens(self):
        for p in (0,1):
            c=self.case('Servo Schematic','entry_then_death',p)
            self.assertTrue(self.has(c,op='cast',ref='source',value=1));self.assertTrue(self.has(c,op='cast',ref='removal',value=1))
            self.assertTrue(any(x['name']=='Naturalize' for x in c['setup']))
            self.assertTrue(self.token_count(c,p,18816,1));self.assertTrue(self.token_count(c,p,18816,2))
            self.assertTrue(self.zone(c,p,'Servo Schematic','graveyard',1))
            self.assertFalse(self.token_count(self.case('Servo Schematic','bounce',p),p,18816,2))

    def test_dunes_real_land_entry_mana_and_destruction_are_separate(self):
        for p in (0,1):
            c=self.case('Dunes of the Dead','land_entry',p);self.assertTrue(self.has(c,op='activate',ref='atlas',value=1))
            self.assertTrue(self.zone(c,p,'Dunes of the Dead','battlefield',1));self.assertTrue(self.token_count(c,p,18860,0))
            c=self.case('Dunes of the Dead','death_after_mana',p);self.assertTrue(self.has(c,op='activate',ref='source',value=1))
            self.assertTrue(self.has(c,op='assert_mana',player=p,color=0,value=1));self.assertTrue(self.has(c,op='require_can_activate',ref='source',value=0))
            self.assertTrue(self.zone(c,p,'Dunes of the Dead','graveyard',1));self.assertTrue(self.token_count(c,p,18860,1))

    def test_promise_only_creates_after_its_actual_sacrifice_and_once_on_double_death(self):
        for p in (0,1):
            for mode in ('simultaneous','effect','token_victim','cost_death'):
                c=self.case('Promise of Bunrei',mode,p);self.assertTrue(self.token_count(c,p,18829,4))
                self.assertTrue(self.zone(c,p,'Promise of Bunrei','graveyard',1));self.assertFalse(self.token_count(c,p,18829,8))
            for mode in ('opponent_death','noncreature_death','source_departed'):
                self.assertFalse(self.token_count(self.case('Promise of Bunrei',mode,p),p,18829,4))

    def test_xathrid_self_controlled_human_and_token_human_exclusions(self):
        for p in (0,1):
            for mode in ('effect','own_human','token_victim','stolen_victim'):
                c=self.case('Xathrid Necromancer',mode,p);self.assertTrue(self.token_count(c,p,18860,1))
                self.assertTrue(self.has(c,op='assert_matching_permanents',index=18860,property='tapped',value=1))
            for mode in ('opponent_death','nonhuman','source_departed'):
                self.assertFalse(self.token_count(self.case('Xathrid Necromancer',mode,p),p,18860,1))
            c=self.case('Xathrid Necromancer','simultaneous',p);self.assertTrue(self.token_count(c,p,18860,3))

    def test_sekkuar_actual_deaths_respect_another_nontoken_and_controller(self):
        name = "Sek'Kuar, Deathkeeper"
        for p in (0,1):
            q = 1-p
            for mode in ('self_death','opponent_death','noncreature_death','source_departed','token_victim','bounce','exile'):
                case = self.case(name,mode,p)
                self.assertTrue(self.token_count(case,p,18753,0))
                self.assertFalse(self.token_count(case,p,18753,1))
            case = self.case(name,'token_victim',p)
            self.assertTrue(any(item['ref']=='victim' and 'token' in item['state'] for item in case['setup']))
            self.assertTrue(self.zone(case,p,'Eager Cadet','graveyard',0))
            case = self.case(name,'stolen_victim',p)
            self.assertTrue(self.zone(case,q,'Eager Cadet','graveyard',1))
            self.assertTrue(self.token_count(case,p,18753,1))
            case = self.case(name,'source_simultaneous',p)
            self.assertTrue(any(item['ref']=='sweep' and item['name']=='Inferno' for item in case['setup']))
            self.assertTrue(self.has(case,op='cast',ref='sweep',value=1))
            self.assertTrue(self.zone(case,p,name,'graveyard',1))
            self.assertTrue(self.zone(case,p,'Eager Cadet','graveyard',1))
            self.assertTrue(self.token_count(case,p,18753,1))
            self.assertFalse(self.token_count(case,p,18753,2))
            self.assertTrue(all(self.has(case,op='assert_life',player=player,value=14) for player in (p,q)))
            case = self.case(name,'repeated_deaths',p)
            self.assertTrue(self.has(case,op='cast',ref='second_removal',value=1))
            self.assertTrue(self.zone(case,p,'Eager Cadet','graveyard',2))
            self.assertTrue(self.token_count(case,p,18753,2))

    def test_sekkuar_graveborn_attacks_on_actual_creation_turn(self):
        for p in (0,1):
            case = self.case("Sek'Kuar, Deathkeeper",'fresh_token_haste',p)
            steps = case['steps']
            death = next(i for i,s in enumerate(steps) if s['op']=='cast' and s['ref']=='removal')
            created = next(i for i,s in enumerate(steps) if s['op']=='bind_permanent_copy' and s['value']==18753)
            attacked = next(i for i,s in enumerate(steps) if s['op']=='attack' and s['ref']=='haste_token' and s['value']==1)
            self.assertLess(death,created); self.assertLess(created,attacked)
            self.assertFalse(self.has(case,op='advance_turn'))
            self.assertTrue(self.has(case,op='assert_life',player=1-p,value=17))
            self.assertTrue(self.has(case,op='assert_matching_permanents',index=18753,property='tapped',value=1))

    def test_owner_destination_replay_and_announced_counterspell(self):
        for n,pin in self.sources.items():
            c=self.case(n,'owned_elsewhere'); self.assertTrue(any(x['ref']=='source' and 'opponent_owned' in x['state'] for x in c['setup']))
            c=self.case(n,'pending_snapshot');s=c['steps'];i=next(i for i,x in enumerate(s) if x['op']=='capture')
            self.assertEqual(s[i-1],dict(op='assert_stack',value=1));self.assertTrue(self.has(c,op='assert_recorded_state'))
            if n!='Dunes of the Dead':
                c=self.case(n,'countered');ops=[s['op'] for s in c['steps']]
                self.assertLess(ops.index('announce_spell'),ops.index('cast_response'));self.assertNotIn('choose_target',ops)
                self.assertTrue(self.zone(c,0,n,'graveyard',1))


if __name__=='__main__':
    unittest.main()
