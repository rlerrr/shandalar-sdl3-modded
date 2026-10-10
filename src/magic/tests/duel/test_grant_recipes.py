"""Oracle independence, legitimate actions, and failure boundaries for ability grants."""
import copy
import json
import unittest
from pathlib import Path
from generate_cases import registry
from grant_recipes import spec, variants
from recipes import expand, cost_from_oracle


class GrantRecipesTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.oracle=json.loads((Path(__file__).parent/'data/oracle.json').read_text(encoding='utf8'))['cards']
        cls.registry={r['name']:r for r in registry()}

    def declaration(self,name,params,selected):
        return dict(family='grant_creature',version=2,params=params,variants=selected,oracle_text=self.oracle[name]['oracle_text'])

    def cases(self,name,d,registry=None,oracle=None):
        return expand(name,self.registry[name]['csvid'],d,self.oracle[name],registry or self.registry,oracle_context=oracle or self.oracle)

    def test_unknown_keywords_unsafe_amounts_and_wrong_variants_fail_closed(self):
        p=dict(power=1,toughness=1,keywords=['Flying'])
        for change in (dict(power=True),dict(toughness=-1),dict(power=8),dict(keywords=[]),dict(keywords=['Flying','Flying']),
                       dict(keywords=['Unknown']),dict(keywords=[{}]),dict(untap=False),dict(controller='opponent'),dict(draw=True),dict(death_loss=2)):
            with self.assertRaises(ValueError):spec(dict(p,**change),[0]*6)
        d=self.declaration('Mighty Leap',p,['fresh_grant'])
        with self.assertRaises(ValueError):self.cases('Mighty Leap',d)
        d=self.declaration('Mighty Leap',p,['snapshot'])
        with self.assertRaises(ValueError):self.cases('Mighty Leap',dict(d,oracle_text='Different text'))
        o=copy.deepcopy(self.oracle);o['Giant Spider']['keywords']=[]
        with self.assertRaises(ValueError):self.cases('Mighty Leap',d,oracle=o)

    def test_registry_implementations_and_costs_cannot_supply_expectations(self):
        p=dict(power=2,toughness=2,keywords=['Trample'],draw=1)
        d=self.declaration('Wildsize',p,['payment','snapshot'])
        original=self.cases('Wildsize',d)
        altered=copy.deepcopy(self.registry)
        for r in altered.values():r['handler']='card_dummy';r['mana_cost']='{99}'
        self.assertEqual(original,self.cases('Wildsize',d,registry=altered))
        self.assertIn(dict(op='assert_recorded_state'),original[1]['steps'])

    def test_lost_target_is_a_real_response_and_fizzles_draw(self):
        p=dict(power=2,toughness=2,keywords=['Trample'],draw=1)
        d=self.declaration('Wildsize',p,['friendly_cleanup','lost_target'])
        normal,lost=self.cases('Wildsize',d)
        self.assertIn(dict(op='choose_priority',ref='response'),lost['steps'])
        self.assertFalse(any(s['op']=='destroy' for s in lost['steps']))
        self.assertIn(dict(op='assert_zone',player=0,zone='library',value=6),normal['steps'])
        self.assertIn(dict(op='assert_zone',player=0,zone='library',value=7),lost['steps'])
        self.assertIn(dict(op='assert_zone_contains',player=0,zone='hand',value=self.registry['Grizzly Bears']['csvid']),lost['steps'])
        self.assertEqual(next(c['player'] for c in lost['setup'] if c['ref']=='response'),0)

    def test_flying_and_reach_use_normal_blocker_pipeline_with_expiry_controls(self):
        d=self.declaration('Jump',dict(power=0,toughness=0,keywords=['Flying']),['flying_evasion','flying_reach','expired_flying','block_flying','expired_block_flying'])
        cs=self.cases('Jump',d)
        for c in cs:self.assertIn(dict(op='advance_combat'),c['steps'])
        for c,legal in zip(cs[:3],(0,1,1)):
            self.assertIn(dict(op='block',ref='blocker',other='recipient',value=legal),c['steps'])
        self.assertIn(dict(op='block',ref='recipient',other='attacker',value=0),cs[-1]['steps'])
        cleanup=next(i for i,s in enumerate(cs[2]['steps']) if s['op']=='cleanup')
        attack=next(i for i,s in enumerate(cs[2]['steps']) if s['op']=='attack')
        self.assertLess(cleanup,attack)
        self.assertIn(dict(op='turn',player=1,value=0),cs[2]['steps'][cleanup:attack])
        self.assertIn(dict(op='assert_card',ref='blocker',property='in_play',value=1),cs[0]['steps'])
        self.assertIn(dict(op='assert_card',ref='recipient',property='damage',value=0),cs[0]['steps'])

    def test_forced_block_runs_the_trigger_and_does_not_assign_twice(self):
        d=self.declaration('Enlarge',dict(power=7,toughness=7,keywords=['Trample'],must_be_blocked=True),['forced_block','expired_trample'])
        forced,expired=self.cases('Enlarge',d)
        steps=forced['steps']
        choice=steps.index(dict(op='choose_target',player=1,ref='blocker'))
        self.assertLess(choice,steps.index(dict(op='begin_blockers')))
        self.assertFalse(any(s['op']=='block' for s in steps))
        self.assertIn(dict(op='assert_card',ref='blocker',property='blocking_count',value=1),steps)
        self.assertIn(dict(op='assert_life',player=1,value=12),steps)
        self.assertTrue(any(s['op']=='block' for s in expired['steps']))

    def test_eligibility_defects_keep_oracle_assertion_without_illegal_host_choice(self):
        d=self.declaration('Mighty Leap',dict(power=2,toughness=2,keywords=['Flying']),['opponent_cleanup'])
        d['eligibility_only']=['opponent_cleanup']
        c=self.cases('Mighty Leap',d)[0]
        self.assertIn(dict(op='can_cast',ref='source',value=1),c['steps'])
        self.assertFalse(any(s['op'] in ('cast','choose_target') for s in c['steps']))
        self.assertEqual([x['ref'] for x in c['setup']],['source','recipient'])
        with self.assertRaises(ValueError):self.cases('Mighty Leap',dict(d,eligibility_only=['friendly_cleanup']))
        with self.assertRaises(ValueError):self.cases('Mighty Leap',dict(d,eligibility_only=['opponent_cleanup']*2))

    def test_kicker_decision_order_is_reviewed_independently_of_effects(self):
        d=self.declaration('Vigorous Charge',dict(power=0,toughness=0,keywords=['Trample']),['unblocked'])
        d.update(cast_target_first=True,cast_dialogs=dict(unblocked=[1]))
        c=self.cases('Vigorous Charge',d)[0]
        self.assertLess(c['steps'].index(dict(op='choose_target',player=0,ref='recipient')),c['steps'].index(dict(op='choose_dialog',player=0,value=1)))
        with self.assertRaises(ValueError):self.cases('Vigorous Charge',dict(d,cast_target_first=False))

    def test_haste_casts_a_creature_then_humble_preserves_additive_stats(self):
        d=self.declaration('Reckless Charge',dict(power=3,toughness=0,keywords=['Haste']),['fresh_grant','fresh_without_grant','haste_removed'])
        cs=self.cases('Reckless Charge',d)
        for c in cs:
            self.assertIn(dict(op='cast',ref='recipient',value=1),c['steps'])
            self.assertIn(dict(op='assert_card',ref='recipient',property='sick',value=1),c['steps'])
        for c,allowed in zip(cs,(1,0,0)):self.assertIn(dict(op='attack',ref='recipient',value=allowed),c['steps'])
        self.assertIn(dict(op='assert_card',ref='recipient',property='power',value=3),cs[-1]['steps'])
        self.assertIn(dict(op='assert_card',ref='recipient',property='toughness',value=1),cs[-1]['steps'])

    def test_indestructible_is_not_zero_toughness_immunity(self):
        d=self.declaration('Withstand Death',dict(power=0,toughness=0,keywords=['Indestructible']),['destroy','lethal_damage','zero_toughness','expired_destroy'])
        cs=self.cases('Withstand Death',d)
        for c,alive in zip(cs,(1,1,0,0)):
            self.assertIn(dict(op='assert_card',ref='recipient',property='in_play',value=alive),c['steps'])
        self.assertIn(dict(op='assert_card',ref='recipient',property='damage',value=4),cs[1]['steps'])
        self.assertEqual(next(c['name'] for c in cs[2]['setup'] if c['ref']=='action_spell'),'Grasp of Darkness')

    def test_hexproof_owner_access_and_opposing_lifelink_are_distinct(self):
        d=self.declaration("Ranger's Guile",dict(power=1,toughness=1,keywords=['Hexproof'],controller='self'),['opponent_targeting','owner_targeting','expired_targeting'])
        cs=self.cases("Ranger's Guile",d)
        for c,legal in zip(cs,(0,1,1)):self.assertIn(dict(op='can_cast',ref='probe',value=legal),c['steps'])
        d=self.declaration('Moment of Heroism',dict(power=2,toughness=2,keywords=['Lifelink']),['opponent_unblocked'])
        c=self.cases('Moment of Heroism',d)[0]
        self.assertEqual(c['steps'][0],dict(op='turn',player=1,value=0))
        self.assertIn(dict(op='assert_life',player=1,value=24),c['steps'])
        self.assertIn(dict(op='assert_life',player=0,value=16),c['steps'])
        d=self.declaration('Assault Strobe',dict(power=0,toughness=0,keywords=['Double strike']),['opponent_unblocked'])
        with self.assertRaises(ValueError):self.cases('Assault Strobe',d)

    def test_combat_damage_distinguishes_first_strike_trample_and_deathtouch(self):
        cases=(('Kindled Fury',dict(power=1,toughness=0,keywords=['First strike']),'first_strike',dict(op='assert_card',ref='recipient',property='damage',value=0)),
               ('Unnatural Predation',dict(power=1,toughness=1,keywords=['Trample']),'trample',dict(op='assert_life',player=1,value=18)),
               ("Serpent's Gift",dict(power=0,toughness=0,keywords=['Deathtouch']),'deathtouch',dict(op='assert_card',ref='attacker',property='in_play',value=0)))
        for name,p,v,assertion in cases:
            c=self.cases(name,self.declaration(name,p,[v]))[0]
            self.assertIn(dict(op='combat_damage'),c['steps']);self.assertIn(assertion,c['steps'])


if __name__=='__main__':unittest.main()
