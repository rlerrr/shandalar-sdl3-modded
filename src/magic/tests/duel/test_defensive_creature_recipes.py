"""Independent defensive expectations and fail-closed authoring guards."""
import copy,json,unittest
from pathlib import Path
from generate_cases import registry
from recipes import expand,explicit_spec
from defensive_creature_recipes import variants,protects,may_target,validate_witnesses

class DefensiveCreatureRecipesTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.oracle=json.loads((Path(__file__).parent/'data/oracle.json').read_text(encoding='utf8'))['cards']
        cls.registry={r['name']:r for r in registry()}
    def declaration(self,name='Gladecover Scout',keys=None,protection=None,selected=None):
        p=dict(keywords=['Hexproof'] if keys is None else keys)
        if protection:p['protection']=protection
        return dict(family='defensive_creature',version=2,oracle_text=self.oracle[name]['oracle_text'],params=p,variants=selected or ['target_growth_0'])
    def cases(self,name,d,registry=None,oracle=None):
        return expand(name,self.registry[name]['csvid'],d,self.oracle[name],registry or self.registry,oracle_context=oracle or self.oracle)
    def test_compound_protection_and_explicit_hybrid_payments(self):
        for name,colors,pay in [('Oversoul of Dusk',['black','blue','red'],['G']*5),('Stillmoon Cavalier',['black','white'],['W']*2),('Slippery Bogle',[],['G'])]:
            d=self.declaration(name,['Hexproof'] if name=='Slippery Bogle' else [],colors,['payment','hybrid_payment_other','hybrid_payment_mixed']);d['params']['payment_colors']=pay
            p=explicit_spec(d,self.oracle[name]);self.assertEqual(p['protection'],colors);self.assertEqual(len(self.cases(name,d)),3)
            for bad in ([],['R']*len(pay),pay+[pay[0]]):
                with self.assertRaises(ValueError):self.cases(name,dict(d,params=dict(d['params'],payment_colors=bad)))
    def test_compound_protection_preserves_targeting_differences(self):
        d=self.declaration('Oversoul of Dusk',[],['black','blue','red'],['target_growth_0','target_exile_0','target_bounce_0']);d['params']['payment_colors']=['G']*5
        a,b,c=self.cases('Oversoul of Dusk',d);self.assertIn(dict(op='cast',ref='spell0',value=1),a['steps']);self.assertIn(dict(op='cast',ref='spell0',value=1),b['steps']);self.assertIn(dict(op='can_cast',ref='spell0',value=0),c['steps'])
    def test_hybrid_choices_do_not_change_global_ordinary_cost_parser(self):
        from recipes import cost_from_oracle
        with self.assertRaises(ValueError):cost_from_oracle(self.oracle['Slippery Bogle'])
        d=self.declaration();d['params']['payment_colors']=[]
        with self.assertRaises(ValueError):self.cases('Gladecover Scout',d)
    def test_hybrid_menu_choice_is_scripted_only_when_needed(self):
        d=self.declaration('Slippery Bogle',['Hexproof'],selected=['cast','payment','cast_1']);d['params']['payment_colors']=['G'];cases=self.cases('Slippery Bogle',d)
        by={c['_recipe']['variant']:c for c in cases};self.assertTrue(any(s['op']=='choose_payment' for s in by['cast']['steps']));self.assertFalse(any(s['op']=='choose_payment' for s in by['payment']['steps']+by['cast_1']['steps']))
    def test_replayed_mirror_checks_actor_mana_each_time(self):
        d=self.declaration('Slippery Bogle',['Hexproof'],selected=['snapshot_1']);d['params']['payment_colors']=['G'];c=self.cases('Slippery Bogle',d)[0];checks=[s for s in c['steps'] if s['op']=='assert_mana_total'];self.assertEqual(len(checks),2);self.assertTrue(all(s['player']==1 and s['value']==59 for s in checks))
    def test_alternate_and_mixed_hybrid_colors(self):
        d=self.declaration('Stillmoon Cavalier',[],['black','white'],['hybrid_payment_other','hybrid_payment_mixed']);d['params']['payment_colors']=['B','B'];a,b=self.cases('Stillmoon Cavalier',d);self.assertEqual(a['_recipe']['payment_colors'],['W','W']);self.assertEqual(b['_recipe']['payment_colors'],['B','W'])
    def test_pin_and_explicit_scope(self):
        d=self.declaration();self.cases('Gladecover Scout',d)
        for change in (dict(oracle_text='changed'),dict(params={}),dict(params=dict(keywords=['Shroud'])),dict(params=dict(keywords=['Hexproof'],protection=[])),dict(params=dict(keywords=['Hexproof'],other=1))):
            with self.assertRaises(ValueError):self.cases('Gladecover Scout',dict(d,**change))
        for name in ('Infantry Veteran','Hypnotic Specter','Midnight Duelist','Elite Inquisitor'):
            with self.assertRaises(ValueError):explicit_spec(dict(d,oracle_text=self.oracle[name]['oracle_text']),self.oracle[name])
    def test_hexproof_ownership_and_shroud(self):
        p=explicit_spec(self.declaration(),self.oracle['Gladecover Scout'])
        self.assertTrue(may_target(p,0,0,['G']));self.assertFalse(may_target(p,1,0,['G']))
        p=dict(p,keywords=['Shroud'])
        self.assertFalse(may_target(p,0,0,['G']));self.assertFalse(may_target(p,1,0,['G']))
    def test_protection_color_count_and_card_types(self):
        p=dict(protection=['monocolored']);self.assertTrue(protects(p,['R']));self.assertFalse(protects(p,['B','R']));self.assertFalse(protects(p,[]))
        p=dict(protection=['multicolored']);self.assertFalse(protects(p,['R']));self.assertTrue(protects(p,['B','R']))
        p=dict(protection=['creatures']);self.assertTrue(protects(p,[],creature=True));self.assertFalse(protects(p,['G']))
        p=dict(protection=['artifacts']);self.assertTrue(protects(p,[],artifact=True));self.assertFalse(protects(p,[],creature=True))
    def test_same_spell_friendly_and_opposing_targeting(self):
        cases=self.cases('Gladecover Scout',self.declaration(selected=['target_growth_0','target_growth_1']))
        self.assertIn(dict(op='cast',ref='spell0',value=1),cases[0]['steps'])
        self.assertIn(dict(op='assert_card',ref='source',property='power',value=4),cases[0]['steps'])
        self.assertIn(dict(op='can_cast',ref='spell0',value=0),cases[1]['steps'])
        self.assertNotIn(dict(op='cast',ref='spell0',value=1),cases[1]['steps'])
    def test_untargeted_destruction_ignores_protection(self):
        c=self.cases('Black Knight',self.declaration('Black Knight',['First strike'],['white'],['wrath_1']))[0]
        self.assertIn(dict(op='assert_card',ref='source',property='in_play',value=0),c['steps'])
        self.assertIn(dict(op='assert_zone_contains',player=1,zone='graveyard',value=self.registry['Black Knight']['csvid']),c['steps'])
    def test_indestructible_destroy_damage_and_zero_toughness(self):
        d=self.declaration('Darksteel Myr',['Indestructible'],selected=['target_destroy_0','target_damage_0','zero_toughness'])
        a,b,c=self.cases('Darksteel Myr',d)
        self.assertIn(dict(op='assert_card',ref='source',property='in_play',value=1),a['steps'])
        self.assertIn(dict(op='assert_card',ref='source',property='damage',value=4),b['steps'])
        self.assertIn(dict(op='assert_card',ref='source',property='in_play',value=0),c['steps'])
        p=explicit_spec(d,self.oracle['Darksteel Myr'])
        self.assertNotIn('zero_toughness',variants(dict(p,keywords=['Indestructible','Shroud'])))
        self.assertNotIn('zero_toughness',variants(dict(p,protection=['black'])))
    def test_protection_prevents_combat_damage_but_not_blocking_as_defender(self):
        c=self.cases('Beloved Chaplain',self.declaration('Beloved Chaplain',[],['creatures'],['defend_green']))[0]
        for s in (dict(op='block',ref='source',other='witness',value=1),dict(op='combat_damage'),dict(op='assert_card',ref='source',property='damage',value=0)):
            self.assertIn(s,c['steps'])
    def test_flying_and_protection_are_separate_blocking_restrictions(self):
        c=self.cases('Darksteel Gargoyle',self.declaration('Darksteel Gargoyle',['Flying','Indestructible'],selected=['attack_green','attack_flying']))
        self.assertIn(dict(op='block',ref='witness',other='source',value=0),c[0]['steps'])
        self.assertIn(dict(op='block',ref='witness',other='source',value=1),c[1]['steps'])
    def test_snapshot_stocked_zones_and_unexercised_abilities(self):
        c=self.cases('Troll Ascetic',self.declaration('Troll Ascetic',selected=['defense_snapshot']))[0]
        self.assertEqual(c['fixture'],'stocked_zones');self.assertTrue(c['_recipe']['untested_abilities'])
        for op in ('capture','record_state','ai_roundtrip','restore','assert_recorded_state'):self.assertIn(dict(op=op),c['steps'])
        for player in (0,1):self.assertIn(dict(op='assert_zone',player=player,zone='exile',value=2),c['steps'])
    def test_independent_of_handlers_and_fail_closed_witnesses(self):
        d=self.declaration();altered=copy.deepcopy(self.registry)
        for r in altered.values():r['handler']='card_dummy'
        self.assertEqual(self.cases('Gladecover Scout',d),self.cases('Gladecover Scout',d,registry=altered))
        altered=copy.deepcopy(self.oracle);altered['Terminate']['colors']=['R']
        with self.assertRaises(ValueError):validate_witnesses(altered,self.registry)
        with self.assertRaises(ValueError):self.cases('Gladecover Scout',dict(d,variants=['zero_toughness']))
        with self.assertRaises(ValueError):self.cases('Gladecover Scout',dict(d,cast_dialogs=[0]))

if __name__=='__main__':unittest.main()
