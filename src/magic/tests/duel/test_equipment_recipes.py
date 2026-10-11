"""Equipment scope, costs, legality and independent scenario expectations."""
import copy,json,unittest
from pathlib import Path
from generate_cases import registry
from recipes import explicit_spec,expand
from equipment_recipes import variants,validate_witnesses

class EquipmentRecipesTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.oracle=json.loads((Path(__file__).parent/'data/oracle.json').read_text())['cards']
        cls.rows={r['name']:r for r in registry()}
    def declaration(self,name='Bonesplitter',selected=None,**change):
        p=dict(power=2,toughness=0,keywords=[],equip_cost='{1}',indestructible=False);p.update(change)
        return dict(family='equipment',version=2,oracle_text=self.oracle[name]['oracle_text'],params=p,variants=selected or ['attach_0'])
    def cases(self,name='Bonesplitter',selected=None,**change):
        return expand(name,self.rows[name]['csvid'],self.declaration(name,selected,**change),self.oracle[name],self.rows,oracle_context=self.oracle)
    def test_explicit_keys_and_complete_text_pin(self):
        d=self.declaration()
        for change in (dict(params={}),dict(params=dict(d['params'],extra=0)),dict(oracle_text='changed')):
            with self.assertRaises(ValueError):explicit_spec(dict(d,**change),self.oracle['Bonesplitter'])
    def test_normal_noncreature_equipment_only(self):
        for change in (dict(type_line='Artifact'),dict(type_line='Artifact Creature — Equipment'),dict(layout='transform'),dict(card_faces=[{}])):
            with self.assertRaises(ValueError):explicit_spec(self.declaration(),dict(self.oracle['Bonesplitter'],**change))
    def test_stats_are_bounded_signed_integers(self):
        for change in (dict(power=True),dict(power=-1),dict(power=7),dict(toughness=-2),dict(toughness=7),dict(indestructible=1)):
            with self.assertRaises(ValueError):self.cases(**change)
    def test_zero_equip_and_zero_cast_supported(self):
        s=self.cases('Shuko',['cast_payment_0'],power=1,equip_cost='{0}')[0]['steps']
        self.assertIn(dict(op='cast',ref='source',value=1),s)
        self.cases('Bone Saw',power=1)
    def test_negative_toughness_supported(self):
        s=self.cases('Barbed Battlegear',power=4,toughness=-1,equip_cost='{2}')[0]['steps']
        self.assertIn(dict(op='require_card',ref='host',property='toughness',value=1),s)
    def test_equip_cost_text_must_match(self):
        for cost in ('{0}','{2}','{X}','{W}','{8}'):
            with self.assertRaises(ValueError):self.cases(equip_cost=cost)
    def test_additional_clauses_rejected(self):
        for name in ('Cranial Plating','Skullclamp','Ghostfire Blade','O-Naginata','Adventuring Gear','Sunforger'):
            with self.assertRaises(ValueError):self.cases(name)
    def test_keywords_exact_and_unique(self):
        for keys in (['Flying'],['Vigilance','Flying'],['Flying','Flying'],['Wither']):
            with self.assertRaises(ValueError):self.cases(keywords=keys)
    def test_multiple_keywords_and_reminders(self):
        self.cases('Basilisk Collar',power=0,keywords=['Deathtouch','Lifelink'],equip_cost='{2}')
        self.cases('Sword of Vengeance',keywords=['First strike','Haste','Trample','Vigilance'],equip_cost='{3}')
    def test_source_indestructibility_separate_from_host(self):
        self.cases('Darksteel Axe',equip_cost='{2}',indestructible=True)
        self.cases('Darksteel Plate',power=0,keywords=['Indestructible'],equip_cost='{2}',indestructible=True)
    def test_witnesses_pin_rules_and_characteristics(self):
        for name in ('Boomerang','Unsummon','Suppression Field','Grizzly Bears','Giant Spider'):
            o=copy.deepcopy(self.oracle);o[name]['oracle_text']='changed'
            with self.assertRaises(ValueError):validate_witnesses(o,self.rows)
    def test_activation_and_resolution_are_real_and_separate(self):
        s=self.cases()[0]['steps'];ops=[a['op'] for a in s]
        self.assertLess(ops.index('require_can_activate'),ops.index('choose_target'))
        self.assertLess(ops.index('activate'),ops.index('resolve'))
        i=ops.index('activate');self.assertEqual(s[i+1],dict(op='assert_stack',value=1))
        self.assertIn(dict(op='require_card',ref='host',property='power',value=2),s[i:ops.index('resolve')])
        self.assertIn(dict(op='require_card',ref='host',property='power',value=4),s[ops.index('resolve'):])
    def test_no_direct_attachment_action(self):
        self.assertFalse(any(a['op'] in ('attach','dispatch_event') for a in self.cases()[0]['steps']))
    def test_equipment_moves_without_stacking_old_bonus(self):
        s=self.cases(selected=['move_1'])[0]['steps']
        self.assertEqual([a['ref'] for a in s if a['op']=='choose_target'],['host','other'])
        self.assertIn(dict(op='require_card',ref='host',property='power',value=2),s)
        self.assertIn(dict(op='require_card',ref='other',property='power',value=5),s)
    def test_insufficient_input_refreshes_and_fails_before_choices(self):
        s=self.cases(selected=['insufficient_equip_mana_0'])[0]['steps'];ops=[a['op'] for a in s]
        self.assertLess(ops.index('refresh'),ops.index('require_can_activate'))
        self.assertIn(dict(op='require_can_activate',ref='source',value=0),s)
        self.assertNotIn('choose_target',ops)
    def test_generic_red_payment_scripts_human_only(self):
        for owner in (0,1):
            s=self.cases(selected=['red_payment_'+str(owner)])[0]['steps']
            self.assertEqual(sum(a['op']=='choose_mana' for a in s),int(owner==0))
            if owner==0:
                self.assertLess(next(i for i,a in enumerate(s) if a['op']=='choose_mana'),next(i for i,a in enumerate(s) if a['op']=='choose_target'))
            self.assertIn(dict(op='assert_mana',player=owner,color=4,value=0),s)
    def test_tax_constructed_legally_and_paid_exactly(self):
        c=self.cases(selected=['tax_0'])[0];s=c['steps']
        self.assertEqual(next(a['zone'] for a in c['setup'] if a['ref']=='field'),'hand')
        self.assertIn(dict(op='cast',ref='field',value=1),s)
        i=s.index(dict(op='cast',ref='field',value=1))
        self.assertEqual(s[i-1],dict(op='turn',player=1,value=0))
        self.assertIn(dict(op='turn',player=0,value=0),s[i:])
        self.assertIn(dict(op='set_mana',player=0,color=0,value=3),s)
        self.assertIn(dict(op='assert_mana',player=0,color=0,value=0),s)
    def test_wrong_controller_is_isolated(self):
        c=self.cases(selected=['wrong_controller_0'])[0]
        self.assertEqual([a['player'] for a in c['setup'] if a['name']=='Grizzly Bears'],[1])
        self.assertIn(dict(op='require_can_activate',ref='source',value=0),c['steps'])
    def test_source_survives_host_departure(self):
        s=self.cases(selected=['host_destroy_0'])[0]['steps']
        self.assertIn(dict(op='assert_card',ref='source',property='in_play',value=1),s)
        self.assertIn(dict(op='assert_zone_contains',player=0,zone='graveyard',value=self.rows['Grizzly Bears']['csvid']),s)
    def test_lost_target_has_paid_pending_ability_and_real_response(self):
        s=self.cases(selected=['lost_target_0'])[0]['steps'];ops=[a['op'] for a in s]
        self.assertLess(ops.index('activate'),next(i for i,a in enumerate(s) if a['op']=='cast' and a['ref']=='response'))
        self.assertIn(dict(op='assert_stack',value=1),s)
    def test_cleanup_preserves_bonus(self):
        s=self.cases(selected=['cleanup_0'])[0]['steps'];i=next(i for i,a in enumerate(s) if a['op']=='cleanup')
        self.assertIn(dict(op='require_card',ref='host',property='power',value=4),s[i:])
    def test_snapshots_cover_pending_and_completed_attachments(self):
        for variant in ('ready_snapshot_0','pending_snapshot_1','resolved_snapshot_0'):
            s=self.cases(selected=[variant])[0]['steps'];ops=[a['op'] for a in s]
            self.assertIn('capture',ops);self.assertIn('ai_roundtrip',ops);self.assertEqual(ops[-1],'assert_recorded_state')
            if variant.startswith('pending'):self.assertLess(ops.index('activate'),ops.index('capture'));self.assertLess(ops.index('capture'),ops.index('resolve'))
    def test_empty_zones_are_deliberate(self):
        c=self.cases(selected=['bare_zones_0'])[0]
        self.assertEqual(c['fixture'],'empty_main');self.assertIn(dict(op='assert_zone',player=1,zone='exile',value=0),c['steps'])
    def test_shroud_skips_illegal_self_retarget_and_host_bounce(self):
        p=explicit_spec(self.declaration('Lightning Greaves',power=0,keywords=['Haste','Shroud'],equip_cost='{0}'),self.oracle['Lightning Greaves'])
        self.assertNotIn('same_target_0',variants(p));self.assertNotIn('host_bounce_0',variants(p));self.assertIn('move_0',variants(p))
    def test_targeting_probe_has_only_one_creature(self):
        c=self.cases('Mask of Avacyn',['opponent_targeting_1'],power=1,toughness=2,keywords=['Hexproof'],equip_cost='{3}')[0]
        self.assertEqual(sum(a['name'] in ('Grizzly Bears','Hill Giant') for a in c['setup']),1)
        self.assertIn(dict(op='can_target_creature',ref='probe',other='host',value=0),c['steps'])
    def test_friendly_hexproof_target_probe_does_not_ask_ai_strategy(self):
        c=self.cases('Swiftfoot Boots',['owner_targeting_1'],power=0,keywords=['Haste','Hexproof'])[0]
        self.assertIn(dict(op='can_target_creature',ref='probe',other='host',value=1),c['steps'])
        self.assertNotIn('can_cast',[s['op'] for s in c['steps']])
    def test_haste_and_vigilance_are_checked_through_attack_declaration(self):
        s=self.cases('Haunted Cloak',['fresh_attack_1'],power=0,keywords=['Haste','Trample','Vigilance'])[0]['steps']
        self.assertIn(dict(op='attack',ref='host',value=1),s);self.assertIn(dict(op='assert_card',ref='host',property='tapped',value=0),s)
    def test_sick_host_does_not_require_preserving_haste_bugs(self):
        s=self.cases('Swiftfoot Boots',['sick_host_1'],power=0,keywords=['Haste','Hexproof'])[0]['steps']
        self.assertIn(dict(op='require_can_activate',ref='source',value=1),s)
        self.assertIn(dict(op='assert_card',ref='host',property='tapped',value=0),s)
        self.assertFalse(any(a.get('property')=='sick' for a in s))
    def test_trample_double_strike_and_lifelink_damage(self):
        s=self.cases('Behemoth Sledge',['trample_0'],toughness=2,keywords=['Lifelink','Trample'],equip_cost='{3}')[0]['steps']
        self.assertIn(dict(op='assert_life',player=1,value=17),s);self.assertIn(dict(op='assert_life',player=0,value=24),s)
    def test_menace_checks_complete_block_declaration(self):
        s=self.cases('Chitinous Cloak',['menace_single_0'],toughness=2,keywords=['Menace'],equip_cost='{3}')[0]['steps'];ops=[a['op'] for a in s]
        i=ops.index('advance_combat');self.assertIn(dict(op='assert_card',ref='host',property='blocked',value=0),s[i:])
    def test_unknown_variant_rejected(self):
        with self.assertRaises(ValueError):self.cases(selected=['made_up'])

if __name__=='__main__':unittest.main()
