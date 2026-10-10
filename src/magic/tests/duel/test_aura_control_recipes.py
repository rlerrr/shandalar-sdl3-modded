"""Signed Aura stats and restrictions checked through real engine actions."""
import copy,json,unittest
from pathlib import Path
from generate_cases import registry
from recipes import expand,explicit_spec
from grant_recipes import variants

class AuraControlRecipesTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.oracle=json.loads((Path(__file__).parent/'data/oracle.json').read_text(encoding='utf8'))['cards']
        cls.registry={r['name']:r for r in registry()}

    def declaration(self,name,p,selected=None):
        d=dict(family='aura_control',version=2,params=dict(power=0,toughness=0,keywords=[],scope='creature',**p),oracle_text=self.oracle[name]['oracle_text'],variants=selected or ['payment'])
        return d

    def cases(self,name,d,oracle=None,registry=None):
        return expand(name,self.registry[name]['csvid'],d,self.oracle[name],registry or self.registry,oracle_context=oracle or self.oracle)

    def test_scope_pin_parameters_and_flags_fail_closed(self):
        d=self.declaration('Pacifism',dict(no_attack=True,no_block=True))
        self.cases('Pacifism',d)
        for change in (dict(params=[]),dict(params=dict(d['params'],scope=[])),dict(oracle_text='changed'),dict(params=dict(d['params'],scope='permanent')),dict(params=dict(d['params'],no_attack=False)),dict(params=dict(d['params'],power=-14)),dict(params=dict(d['params'],keywords=['Haste'])),dict(params=dict(d['params'],mana_exception=True))):
            with self.assertRaises(ValueError):self.cases('Pacifism',dict(d,**change))
        with self.assertRaises(ValueError):explicit_spec(d,self.oracle['Giant Growth'])

    def test_signed_stats_survive_and_have_small_lethal_probe(self):
        d=self.declaration('Dead Weight',{})
        d['params'].update(power=-2,toughness=-2);d['variants']=['friendly_cleanup','lethal']
        live,dead=self.cases('Dead Weight',d)
        self.assertEqual(next(c['name'] for c in live['setup'] if c['ref']=='recipient'),'Scaled Wurm')
        self.assertIn(dict(op='assert_card',ref='recipient',property='toughness',value=4),live['steps'])
        self.assertEqual(next(c['name'] for c in dead['setup'] if c['ref']=='recipient'),'Suntail Hawk')
        self.assertIn(dict(op='assert_card',ref='recipient',property='in_play',value=0),dead['steps'])
        self.assertIn(dict(op='assert_card',ref='source',property='in_play',value=0),dead['steps'])

    def test_negative_power_is_observed_but_damage_is_zero(self):
        d=self.declaration('Chant of the Skifsang',{},['small_survivor']);d['params']['power']=-13
        c=self.cases('Chant of the Skifsang',d)[0]
        self.assertIn(dict(op='assert_card',ref='recipient',property='power',value=-11),c['steps'])
        self.assertIn(dict(op='attack',ref='recipient',value=1),c['steps'])
        self.assertIn(dict(op='assert_life',player=1,value=20),c['steps'])

    def test_restrictions_and_removal_use_real_combat(self):
        d=self.declaration('Pacifism',dict(no_attack=True,no_block=True),['unblocked','removed_attack','block_ground','removed_block'])
        a,r,b,br=self.cases('Pacifism',d)
        self.assertIn(dict(op='attack',ref='recipient',value=0),a['steps'])
        self.assertIn(dict(op='attack',ref='recipient',value=1),r['steps'])
        self.assertIn(dict(op='block',ref='recipient',other='attacker',value=0),b['steps'])
        self.assertIn(dict(op='block',ref='recipient',other='attacker',value=1),br['steps'])
        for c in (r,br):
            self.assertLess(c['steps'].index(dict(op='cast',ref='removal',value=1)),c['steps'].index(dict(op='begin_attackers')))
            self.assertFalse(any(s['op']=='event' for s in c['steps']))

    def test_mana_exception_and_untargeted_activation_control(self):
        p=dict(no_attack=True,no_block=True,no_activate=True,mana_exception=True,scope='permanent')
        d=self.declaration("Faith's Fetters",{k:v for k,v in p.items() if k!='scope'},['activate','mana_activate','removed_activate']);d['params']['scope']='permanent'
        blocked,mana,removed=self.cases("Faith's Fetters",d)
        self.assertIn(dict(op='can_activate',ref='recipient',value=0),blocked['steps'])
        self.assertIn(dict(op='can_activate',ref='activation_control',value=1),blocked['steps'])
        self.assertIn(dict(op='activate',ref='recipient',value=1),mana['steps'])
        self.assertIn(dict(op='assert_card',ref='recipient',property='tapped',value=1),mana['steps'])
        self.assertIn(dict(op='activate',ref='recipient',value=1),removed['steps'])

    def test_lost_target_real_response_and_snapshot_replay(self):
        d=self.declaration('Arrest',dict(no_attack=True,no_block=True,no_activate=True),['lost_target','snapshot'])
        lost,snapshot=self.cases('Arrest',d)
        self.assertIn(dict(op='choose_priority',ref='response'),lost['steps'])
        self.assertIn(dict(op='assert_card',ref='source',property='in_play',value=0),lost['steps'])
        self.assertIn(dict(op='ai_roundtrip'),snapshot['steps'])
        self.assertIn(dict(op='assert_recorded_state'),snapshot['steps'])

    def test_all_zones_ownership_and_old_positive_guard_preserved(self):
        d=self.declaration('Despondency',dict(return_aura=True),['host_bounced','opponent_cleanup']);d['params']['power']=-2
        bounced,opponent=self.cases('Despondency',d)
        self.assertIn(dict(op='assert_zone_contains',player=0,zone='hand',value=3742),bounced['steps'])
        self.assertEqual(next(c['player'] for c in opponent['setup'] if c['ref']=='recipient'),1)
        for p in (0,1):
            self.assertIn(dict(op='assert_zone',player=p,zone='exile',value=2),opponent['steps'])
            self.assertIn(dict(op='assert_zone',player=p,zone='library',value=7),opponent['steps'])
        with self.assertRaises(ValueError):explicit_spec(dict(d,family='aura_creature'),self.oracle['Despondency'])

    def test_witness_and_registry_independence(self):
        d=self.declaration('Arrest',dict(no_activate=True),['activate'])
        altered=copy.deepcopy(self.registry)
        for r in altered.values():r['handler']='card_dummy';r['mana_cost']='{99}'
        self.assertEqual(self.cases('Arrest',d),self.cases('Arrest',d,registry=altered))
        changed=copy.deepcopy(self.oracle);changed['Furnace Whelp']['oracle_text']='{R}: Add {R}.'
        with self.assertRaises(ValueError):self.cases('Arrest',d,oracle=changed)

    def test_negative_toughness_activation_probe_uses_a_surviving_host(self):
        d=self.declaration("Viper's Kiss",dict(no_activate=True),['mana_activate','removed_mana_activate']);d['params'].update(power=-1,toughness=-1)
        for c in self.cases("Viper's Kiss",d):
            self.assertEqual(next(x['name'] for x in c['setup'] if x['ref']=='recipient'),'Wirewood Elf')
            self.assertIn(dict(op='assert_card',ref='recipient',property='toughness',value=1),c['steps'])
        d['params']['toughness']=-3
        with self.assertRaises(ValueError):self.cases("Viper's Kiss",d)

if __name__=='__main__':unittest.main()
