import json
import unittest

from import_duel_oracle import registry
import creature_entry_sacrifices_recipes as model


class CreatureEntrySacrificeTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        p = model.PROFILE.parent
        cls.oracle = json.loads((p/'oracle.json').read_text(encoding='utf8'))['cards']
        cls.rows = {r['name']:r for r in registry() if r['kind']!='internal'}
        cls.homes = {}
        for n,pin in model.profiles()['sources'].items():
            i = pin['registry']['csvid']
            path, = (p/'cases/cards'/f'{i//100:03d}').glob(f'{i:05d}-*.json')
            cls.homes[n] = json.loads(path.read_text(encoding='utf8'))

    def get(self, name, variant):
        d = self.homes[name]['recipes'][0]
        p = model.spec(d,self.oracle[name])
        return model.expand(name,self.rows[name]['csvid'],d,p,[variant],self.rows,self.oracle)[0]['steps']

    def count(self, name, player, zone, amount):
        return dict(op='assert_zone_multiplicity',player=player,zone=zone,index=self.rows[name]['csvid'],value=amount)

    def test_pinned_normal_sources_and_reviewed_variants(self):
        model.validate_witnesses(self.oracle,self.rows)
        self.assertEqual({r['csvid'] for n,r in self.homes.items() if model.classify(self.oracle[n])['mode']!='typed_unless'},
                         {8173,12345,12560,10542,11469,8252})
        for n,home in self.homes.items():
            d = home['recipes'][0]
            self.assertEqual(d['variants'], model.variants(model.spec(d,self.oracle[n])))
            with self.assertRaises(ValueError):
                model.spec(dict(d,oracle_text=d['oracle_text']+'\nDraw a card.'),self.oracle[n])

    def test_blue_replaces_generic_and_actual_unspent_pool_keeps_no_blue(self):
        for n in ('Azorius Herald','Patagia Viper'):
            normal = self.get(n,'ordinary_0')
            blue = self.get(n,'blue_0')
            unspent = self.get(n,'blue_left_unspent_0')
            self.assertIn(dict(op='choose_dialog',player=0,value=1),blue)
            self.assertNotIn('choose_dialog',[r['op'] for r in normal])
            self.assertIn(dict(op='choose_dialog',player=0,value=0),unspent)
            for steps in (normal,unspent):
                self.assertIn(self.count(n,0,'graveyard',1),steps[steps.index(dict(op='resolve')):])
            self.assertIn(self.count(n,0,'battlefield',1),blue[blue.index(dict(op='resolve')):])
            self.assertIn(dict(op='assert_mana',player=0,color=2,value=1),unspent)
        self.assertIn(dict(op='assert_life',player=0,value=24),self.get('Azorius Herald','ordinary_0'))
        self.assertIn(self.count('Snake',1,'battlefield',2),self.get('Patagia Viper','ordinary_1'))

    def test_real_blink_has_entry_benefit_and_no_cast_payment_history(self):
        for name in ('Azorius Herald','Patagia Viper'):
            steps = self.get(name,'blink_0')
            i = steps.index(dict(op='cast',ref='blink',value=1))
            self.assertIn(dict(op='choose_target',player=0,ref='source'),steps[:i])
            self.assertIn(self.count(name,0,'graveyard',1),steps[i:])
            self.assertIn(self.count('Cloudshift',0,'graveyard',1),steps[i:])
            if name=='Azorius Herald':
                self.assertIn(dict(op='assert_life',player=0,value=28),steps[i:])
            else:
                self.assertIn(self.count('Snake',0,'battlefield',4),steps[i:])

    def test_undying_entry_paid_again_and_second_death_stays_grave(self):
        name = 'Demonlord of Ashmouth'
        s = self.get(name,'undying_paid_twice_0')
        first = s.index(dict(op='cast',ref='kill_first',value=1))
        second = s.index(dict(op='cast',ref='kill_second',value=1))
        self.assertIn(dict(op='choose_target',player=0,ref='second_victim'),s[first:second])
        self.assertIn(dict(op='bind_permanent',ref='source',player=0,value=12345),s[first:second])
        self.assertIn(dict(op='assert_card',ref='source',property='plus1_counters',value=1),s[first:second])
        self.assertIn(self.count(name,0,'graveyard',1),s[second:])
        declined = self.get(name,'decline_0')
        self.assertIn(dict(op='choose_optional_target',player=0,ref='victim'),declined)
        self.assertIn(self.count(name,0,'exile',1),declined)

    def test_sacrifice_power_from_real_growth_and_no_self_control(self):
        s = self.get('Disciple of Bolas','boosted_power_0')
        i = s.index(dict(op='cast_response',ref='growth',value=1))
        j = s.index(dict(op='resolve_response'))
        self.assertLess(i,j)
        self.assertIn(dict(op='choose_target',player=0,ref='victim'),s[i:j])
        self.assertIn(dict(op='assert_life',player=0,value=26),s[j:])
        self.assertIn(dict(op='assert_zone',player=0,zone='hand',value=6),s[j:])
        absent = self.get('Disciple of Bolas','unavailable_0')
        self.assertIn(dict(op='assert_unused_choices',value=1),absent)
        self.assertIn(self.count('Disciple of Bolas',0,'battlefield',1),absent)
        self.assertNotIn(dict(op='assert_life',player=0,value=22),absent)

    def test_kicker_payment_and_actual_player_target_then_their_choice(self):
        s = self.get('Gatekeeper of Malakir','paid_1')
        i = s.index(dict(op='cast',ref='source',value=1))
        j = s.index(dict(op='choose_player',player=1,value=0))
        k = s.index(dict(op='choose_target',player=0,ref='victim'))
        self.assertIn(dict(op='choose_dialog',player=1,value=0),s[:i])
        self.assertLess(i,j)
        self.assertLess(j,k)
        self.assertIn(self.count('Grizzly Bears',0,'graveyard',1),s[k:])
        self.assertNotIn('choose_player',[r['op'] for r in self.get('Gatekeeper of Malakir','ordinary_0')])

    def test_flamefiend_optional_artifact_and_positive_split_total(self):
        s = self.get('Kuldotha Flamefiend','split_0')
        p = s.index(dict(op='choose_trigger',player=0,ref='source'))
        i = s.index(dict(op='choose_target',player=0,ref='artifact'))
        j = s.index(dict(op='choose_target',player=0,ref='foe_creature'))
        stop = s.index(dict(op='resolve'))
        self.assertLess(p,i)
        self.assertLess(i,j)
        self.assertEqual(s[j+1:stop],[dict(op='choose_player',player=0,value=1)]*3)
        self.assertIn(dict(op='assert_life',player=1,value=17),s[stop:])
        self.assertIn(dict(op='assert_card',ref='foe_creature',property='damage',value=1),s[stop:])
        self.assertIn(self.count('Darksteel Relic',0,'graveyard',1),s[stop:])
        decline = self.get('Kuldotha Flamefiend','decline_0')
        self.assertIn(dict(op='choose_optional_target',player=0,ref='artifact'),decline)
        self.assertNotIn('choose_player',[r['op'] for r in decline])

    def test_pending_source_snapshot_replays_real_entry(self):
        s = self.get('Patagia Viper','snapshot_1')
        a,b,c = [s.index(dict(op=op)) for op in ('capture','record_state','restore')]
        self.assertIn(dict(op='cast',ref='source',value=1),s[:a])
        self.assertEqual(s[a+1:b],s[c+1:-1])
        self.assertEqual(s[-1],dict(op='assert_recorded_state'))


    def test_typed_profiles_cover_exact_forest_swamp_mountain_land_artifact(self):
        expected = {
            'Mold Demon':('Swamp',2), 'Plant Elemental':('Forest',1),
            'Primeval Force':('Forest',3), 'Rathi Dragon':('Mountain',2),
            'Rogue Elephant':('Forest',1), 'Scythe Tiger':('land',1),
            'Vulshok War Boar':('artifact',1),
        }
        actual = {}
        for name in self.homes:
            p = model.classify(self.oracle[name])
            if p['mode']=='typed_unless':
                actual[name]=(p['resource'],p['quantity'])
        self.assertEqual(actual,expected)

    def test_actual_typed_quantity_and_short_cost_keep_independent_destinations(self):
        for name, land, quantity in (('Mold Demon','Swamp',2),('Primeval Force','Forest',3),
                                     ('Rathi Dragon','Mountain',2)):
            s = self.get(name,'paid_0')
            actual = [r for r in s if r['op']=='choose_target']
            self.assertEqual(actual,[dict(op='choose_target',player=0,ref=f'matching_{i}') for i in range(quantity)])
            self.assertIn(self.count(land,0,'graveyard',quantity),s)
            self.assertIn(self.count(land,1,'battlefield',1),s)
            self.assertIn(self.count(name,0,'battlefield',1),s)
            short = self.get(name,'short_1')
            final = short[short.index(dict(op='resolve')):]
            self.assertIn(self.count(land,1,'battlefield',quantity-1),final)
            self.assertIn(self.count(land,1,'graveyard',0),final)
            self.assertIn(self.count(name,1,'graveyard',1),final)
        primeval = self.get('Primeval Force','short_0')
        self.assertIn(dict(op='assert_unused_choices',value=2),primeval)

    def test_real_nonbasic_forest_and_typed_decline(self):
        rogue = self.get('Rogue Elephant','nonbasic_0')
        self.assertIn(dict(op='choose_target',ref='matching_0',player=0),rogue)
        self.assertIn(self.count('Bayou',0,'graveyard',1),rogue)
        self.assertIn(self.count('Island',0,'battlefield',1),rogue)
        self.assertIn(self.count('Forest',1,'battlefield',1),rogue)
        plant = self.get('Plant Elemental','decline_0')
        self.assertIn(dict(op='choose_optional_target',player=0,ref='matching_0'),plant)
        self.assertIn(dict(op='assert_forced_target_choices',value=0),plant)
        self.assertIn(self.count('Plant Elemental',0,'graveyard',1),plant)
        self.assertIn(self.count('Forest',0,'battlefield',1),plant)

    def test_artifact_includes_indestructible_creature_and_nonartifact_survives(self):
        s = self.get('Vulshok War Boar','artifact_creature_1')
        self.assertIn(self.count('Darksteel Myr',1,'graveyard',1),s)
        self.assertIn(self.count('Grizzly Bears',1,'battlefield',1),s)
        self.assertIn(self.count('Darksteel Relic',0,'battlefield',1),s)

    def test_rathi_flying_combat_and_real_scythe_shroud(self):
        for mode,block in (('combat_1',0),('reach_1',1)):
            s = self.get('Rathi Dragon',mode)
            self.assertIn(dict(op='block',ref='foe_creature',other='source',value=block),s)
            if block:
                self.assertIn(self.count('Giant Spider',0,'graveyard',1),s)
                advance = s.index(dict(op='advance_combat'))
                self.assertEqual(s[advance+1],dict(op='assert_card',ref='source',property='blocked',value=1))
            else:
                self.assertIn(dict(op='assert_life',player=0,value=15),s)
        s = self.get('Scythe Tiger','shroud_1')
        for p in (0,1):
            self.assertIn(dict(op='can_target_creature',ref=f'probe_{p}',other='source',value=0),s)
            self.assertIn(dict(op='can_target_creature',ref=f'probe_{p}',other='shroud_control',value=1),s)

