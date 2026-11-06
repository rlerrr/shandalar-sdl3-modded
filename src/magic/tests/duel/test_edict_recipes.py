import copy
import json
import unittest
from pathlib import Path
from generate_cases import registry
import edict_recipes as family


class EdictRecipesTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.oracle=json.loads((Path(__file__).parent/'data/oracle.json').read_text(encoding='utf8'))['cards']
        cls.rows={r['name']:r for r in registry()}

    def declaration(self,name='Diabolic Edict',**patch):
        p=dict(mode='edict',scope='creature',recipient='any',count=1,receipt='none');p.update(patch)
        return dict(family='edict_spell',version=2,params=p,oracle_text=self.oracle[name]['oracle_text'],variants=['mixed_0'])

    def cases(self,name='Diabolic Edict',variants=None,**patch):
        d=self.declaration(name,**patch);p=family.spec(d,self.oracle[name])
        return family.expand(name,self.rows[name]['csvid'],d,p,variants or d['variants'],self.rows,self.oracle)

    def has(self,steps,**fields):return any(all(s.get(k)==v for k,v in fields.items()) for s in steps)

    def copies(self,steps,name,player,zone,count):
        return self.has(steps,op='assert_zone_multiplicity',index=self.rows[name]['csvid'],player=player,zone=zone,value=count)

    def test_entire_source_and_explicit_effect_must_match(self):
        for patch in (dict(scope='permanent'),dict(recipient='opponent'),dict(count=2),dict(receipt='lose1'),dict(count=True)):
            with self.assertRaises(ValueError):family.spec(self.declaration(**patch),self.oracle['Diabolic Edict'])
        for key in ('oracle_text','variants'):
            d=self.declaration();d.pop(key)
            with self.assertRaises(ValueError):family.spec(d,self.oracle['Diabolic Edict'])
        r=copy.deepcopy(self.oracle['Diabolic Edict']);r['oracle_text']+=' Draw a card.'
        d=self.declaration();d['oracle_text']=r['oracle_text']
        with self.assertRaises(ValueError):family.spec(d,r)

    def test_registered_dispatch_expands_explicit_variants(self):
        from recipes import expand
        d=self.declaration();d['variants']=family.variants(family.spec(d,self.oracle['Diabolic Edict']))
        cases=expand('Diabolic Edict',self.rows['Diabolic Edict']['csvid'],d,self.oracle['Diabolic Edict'],self.rows,oracle_context=self.oracle)
        self.assertEqual(len(cases),len(d['variants']))
        d['variants']=['invented_0']
        with self.assertRaises(ValueError):expand('Diabolic Edict',self.rows['Diabolic Edict']['csvid'],d,self.oracle['Diabolic Edict'],self.rows,oracle_context=self.oracle)

    def test_extra_mechanics_fail_closed_and_flashback_is_explicit_gap(self):
        d=self.declaration("Chainer's Edict");p=family.spec(d,self.oracle["Chainer's Edict"])
        self.assertEqual(len(p['untested_rules']),1);self.assertTrue(p['untested_rules'][0].startswith('Flashback '))
        r=copy.deepcopy(self.oracle['Diabolic Edict']);r['keywords']=['Delve']
        with self.assertRaises(ValueError):family.spec(self.declaration(),r)

    def test_witness_characteristics_are_pinned(self):
        family.validate_witnesses(self.oracle,self.rows)
        for name,key,value in (('Gladecover Scout','keywords',[]),('Darksteel Myr','toughness','2'),('Forest','type_line','Land'),('Gilded Light','oracle_text','Draw a card.')):
            o=copy.deepcopy(self.oracle);o[name][key]=value
            with self.assertRaises(ValueError):family.validate_witnesses(o,self.rows)

    def test_sacrifice_predicates_independently_check_type_color_and_flying(self):
        self.assertTrue(family.eligible(self.oracle['Darksteel Myr'],'creature'))
        self.assertFalse(family.eligible(self.oracle['Darksteel Citadel'],'creature'))
        self.assertTrue(family.eligible(self.oracle['Watchwolf'],'multicolored_permanent'))
        self.assertFalse(family.eligible(self.oracle['Grizzly Bears'],'multicolored_permanent'))
        self.assertTrue(family.eligible(self.oracle['Aven Fleetwing'],'flying_creature'))
        self.assertFalse(family.eligible(self.oracle['Gladecover Scout'],'flying_creature'))
        self.assertTrue(family.eligible(self.oracle['Savannah Lions'],'green_white_creature'))
        self.assertFalse(family.eligible(self.oracle['Scathe Zombies'],'green_white_creature'))

    def test_sacrificing_controller_selects_and_caster_selects_target_player(self):
        s=self.cases(variants=['mixed_1'])[0]['steps']
        self.assertTrue(self.has(s,op='choose_player',player=1,value=0))
        self.assertTrue(self.has(s,op='choose_target',player=0,ref='b0_0'))
        self.assertTrue(self.copies(s,'Grizzly Bears',0,'battlefield',1))
        self.assertTrue(self.copies(s,'Hill Giant',0,'battlefield',1))
        s=self.cases(variants=['alternate_choice_1'])[0]['steps']
        self.assertTrue(self.has(s,op='choose_target',player=0,ref='b0_1'))
        self.assertTrue(self.copies(s,'Hill Giant',0,'graveyard',1))
        self.assertTrue(self.copies(s,'Grizzly Bears',0,'battlefield',2))

    def test_automatic_opponent_target_has_no_queued_player_choice(self):
        name='Cruel Edict';d=self.declaration(name,recipient='opponent');d['target_choices']={'0':0,'1':0}
        p=family.spec(d,self.oracle[name]);s=family.expand(name,self.rows[name]['csvid'],d,p,['mixed_1'],self.rows,self.oracle)[0]['steps']
        self.assertFalse(any(x['op']=='choose_player' for x in s))
        self.assertTrue(self.has(s,op='choose_target',player=0,ref='b0_0'))
        d['target_choices']=True
        with self.assertRaises(ValueError):family.spec(d,self.oracle[name])
        d['target_choices']={'0':0,'1':2}
        with self.assertRaises(ValueError):family.spec(d,self.oracle[name])

    def test_ai_duplicates_preserve_exact_zone_identity_without_asserting_slot_policy(self):
        c=self.cases()[0];s=c['steps']
        self.assertFalse(any(x['op']=='choose_target' and x['player']==1 for x in s))
        self.assertTrue(self.copies(s,'Grizzly Bears',1,'battlefield',2))
        self.assertFalse(any(x['op']=='assert_card' and x.get('ref','').startswith('b1_') for x in s))

    def test_indestructible_hexproof_are_sacrificed_and_unaffected_other_player_is_exact(self):
        for v,n in (('indestructible_1','Darksteel Myr'),('hexproof_1','Gladecover Scout')):
            s=self.cases(variants=[v])[0]['steps']
            self.assertTrue(self.copies(s,n,0,'graveyard',1));self.assertTrue(self.copies(s,n,0,'battlefield',2))
            self.assertTrue(self.copies(s,n,1,'battlefield',3))

    def test_stolen_card_goes_to_owner_and_tokens_vanish(self):
        s=self.cases(variants=['stolen_1'])[0]['steps']
        self.assertTrue(self.copies(s,'Grizzly Bears',1,'graveyard',2))
        self.assertTrue(self.copies(s,'Grizzly Bears',0,'graveyard',1))
        s=self.cases(variants=['tokens_1'])[0]['steps']
        self.assertTrue(self.copies(s,'Grizzly Bears',0,'graveyard',1))
        self.assertTrue(self.copies(s,'Grizzly Bears',0,'battlefield',1))

    def test_empty_edict_still_casts_and_life_loss_does_not_require_a_sacrifice(self):
        s=self.cases("Geth's Verdict",variants=['empty_board_1'],receipt='lose1')[0]['steps']
        self.assertTrue(self.has(s,op='cast',ref='source',value=1));self.assertTrue(self.has(s,op='assert_life',player=0,value=19))
        s=self.cases('Self-Inflicted Wound',variants=['empty_board_1'],recipient='opponent',scope='green_white_creature',receipt='lose2_if')[0]['steps']
        self.assertTrue(self.has(s,op='assert_life',player=0,value=20))

    def test_cost_paid_before_resolution_and_preserved_when_countered(self):
        c=self.cases("Altar's Reap",variants=['pending_snapshot_0'],mode='cost',recipient='self',receipt='draw2')[0]
        s=c['steps'];capture=next(i for i,x in enumerate(s) if x['op']=='capture')
        self.assertTrue(self.has(s[:capture],op='assert_zone',player=0,zone='graveyard',value=5))
        self.assertTrue(self.has(s[:capture],op='assert_zone',player=0,zone='hand',value=5))
        self.assertTrue(self.has(s[capture:],op='assert_recorded_state'))
        s=self.cases("Altar's Reap",variants=['countered_0'],mode='cost',recipient='self',receipt='draw2')[0]['steps']
        self.assertTrue(self.copies(s,'Grizzly Bears',0,'graveyard',2))
        self.assertTrue(self.has(s,op='assert_zone',player=0,zone='library',value=28))

    def test_barter_short_board_and_pair_predicate_each_sacrifice_only_available_cards(self):
        s=self.cases('Barter in Blood',variants=['short_board_0'],recipient='both',count=2)[0]['steps']
        self.assertTrue(self.copies(s,'Grizzly Bears',0,'battlefield',0));self.assertTrue(self.copies(s,'Grizzly Bears',1,'battlefield',0))
        s=self.cases('Perilous Predicament',recipient='opponents',scope='artifact_nonartifact_creature')[0]['steps']
        self.assertTrue(self.copies(s,'Wall of Spears',1,'graveyard',1));self.assertTrue(self.copies(s,'Grizzly Bears',1,'graveyard',2))

    def test_union_predicates_include_noncreature_artifact_land_and_enchantment(self):
        for name,scope,mode,receipt,n in [('Tribute to the Wild','artifact_or_enchantment','edict','none','Seal of Cleansing'),('Morbid Curiosity','artifact_or_creature','cost','draw_mv','Sol Ring'),('Merciless Resolve','creature_or_land','cost','draw2','Forest')]:
            s=self.cases(name,variants=['other_type_0'],scope=scope,mode=mode,receipt=receipt,recipient='opponents' if mode=='edict' else 'self')[0]['steps']
            self.assertTrue(self.copies(s,n,1 if mode=='edict' else 0,'graveyard',2 if n in ('Forest','Sol Ring') else 1))

    def test_target_player_shroud_cancels_sacrifice_and_secondary_receipt(self):
        s=self.cases('Twisted Justice',variants=['target_shrouded_pending_1'],receipt='draw_power')[0]['steps']
        self.assertTrue(self.copies(s,'Grizzly Bears',0,'battlefield',2))
        self.assertTrue(self.has(s,op='assert_zone',player=1,zone='library',value=28))

    def test_mana_value_receipt_uses_selected_card_and_multicolor_is_symmetric(self):
        for variant,n in [('mixed_0',21),('alternate_choice_0',23)]:
            s=self.cases('Sacrifice',variants=[variant],mode='cost',recipient='self',receipt='black_mv')[0]['steps']
            self.assertTrue(self.has(s,op='assert_mana',player=0,color=1,value=n))
        s=self.cases('Renounce the Guilds',recipient='both',scope='multicolored_permanent')[0]['steps']
        for pl in (0,1):
            self.assertTrue(self.copies(s,'Watchwolf',pl,'graveyard',1))
            self.assertTrue(self.copies(s,'Scathe Zombies',pl,'battlefield',1))

    def test_token_mana_value_fixture_is_unambiguously_zero(self):
        c=self.cases('Morbid Curiosity',variants=['tokens_0'],mode='cost',recipient='self',scope='artifact_or_creature',receipt='draw_mv')[0]
        self.assertEqual(self.oracle['Memnite']['mana_cost'],'{0}');self.assertEqual(self.oracle['Memnite']['cmc'],0)
        s=c['steps'];self.assertTrue(self.has(s,op='assert_zone',player=0,zone='library',value=28))
        self.assertTrue(self.copies(s,'Memnite',0,'battlefield',2));self.assertTrue(self.copies(s,'Memnite',0,'graveyard',0))
        self.assertIn('nonzero-mana-value token copies',c['_recipe']['untested_components'])

    def extension(self,name,variant='effect_0'):
        p=copy.deepcopy(family.EXTENSION_PINS['profiles'][name]['params'])
        d=dict(family='edict_spell',version=2,params=p,oracle_text=self.oracle[name]['oracle_text'],variants=[variant])
        s=family.spec(d,self.oracle[name]);return family.expand(name,self.rows[name]['csvid'],d,s,d['variants'],self.rows,self.oracle)[0]

    def test_new_profiles_are_fully_pinned_and_do_not_loosen_legacy(self):
        n='Bone Splinters';p=copy.deepcopy(family.EXTENSION_PINS['profiles'][n]['params'])
        d=dict(family='edict_spell',version=2,params=p,oracle_text=self.oracle[n]['oracle_text'],variants=['effect_0'])
        for key,value in [('effect','exile'),('cost_count',True),('cost_scope','land')]:
            altered=copy.deepcopy(d);altered['params'][key]=value
            with self.assertRaises(ValueError):family.spec(altered,self.oracle[n])
        altered=copy.deepcopy(self.oracle[n]);altered['oracle_text']+=' Draw a card.'
        with self.assertRaises(ValueError):family.spec(d,altered)
        for vs in ([],['invented_0'],['effect_0','effect_0']):
            altered=copy.deepcopy(d);altered['variants']=vs
            with self.assertRaises(ValueError):family.spec(altered,self.oracle[n])

    def test_cost_sacrifices_and_printed_mana_are_paid_while_spell_is_pending(self):
        c=self.extension('Bone Splinters','pending_snapshot_0');s=c['steps'];capture=next(i for i,x in enumerate(s) if x['op']=='capture')
        self.assertTrue(self.has(s[:capture],op='choose_target',player=0,ref='cost0'))
        self.assertTrue(self.has(s[:capture],op='assert_zone',player=0,zone='graveyard',value=5))
        self.assertTrue(self.has(s[:capture],op='assert_zone',player=0,zone='hand',value=5))
        self.assertTrue(self.has(s[:capture],op='assert_mana',player=0,color=1,value=19))
        self.assertTrue(self.copies(s,'Grizzly Bears',0,'graveyard',2))
        self.assertTrue(self.copies(s,'War Mammoth',1,'graveyard',1))

    def test_land_cost_damage_and_actual_lost_target_response(self):
        c=self.extension('Shard Volley');s=c['steps']
        self.assertTrue(self.has(s,op='choose_target',player=0,ref='cost0'))
        self.assertTrue(self.copies(s,'Forest',0,'graveyard',2));self.assertTrue(self.has(s,op='assert_life',player=1,value=17))
        s=self.extension('Bone Splinters','target_left_0')['steps']
        self.assertTrue(self.has(s,op='cast_response',ref='response',value=1));self.assertTrue(self.has(s,op='assert_stack',value=2))
        self.assertTrue(self.copies(s,'War Mammoth',1,'hand',1));self.assertTrue(self.copies(s,'Grizzly Bears',0,'graveyard',2))

    def test_costed_discard_uses_actual_sacrificed_power_and_conservation(self):
        a=self.extension('Tormented Thoughts')['steps'];b=self.extension('Tormented Thoughts','alternate_choice_0')['steps']
        self.assertTrue(self.has(a,op='assert_zone',player=1,zone='hand',value=2));self.assertTrue(self.has(b,op='assert_zone',player=1,zone='hand',value=1))
        self.assertTrue(self.has(a,op='assert_hand_graveyard_total',player=1,index=self.rows['Grizzly Bears']['csvid'],value=2))

    def test_rebound_initial_exile_and_greatest_power_sacrifice_receipts(self):
        s=self.extension('Consuming Vapors')['steps']
        self.assertTrue(self.copies(s,'Consuming Vapors',0,'exile',1));self.assertTrue(self.has(s,op='assert_life',player=0,value=22))
        s=self.extension('Consuming Vapors','countered_0')['steps'];self.assertTrue(self.copies(s,'Consuming Vapors',0,'graveyard',1))
        s=self.extension('Crackling Doom','effect_1')['steps']
        self.assertTrue(self.has(s,op='choose_target',player=0,ref='edict0_0'));self.assertTrue(self.has(s,op='assert_life',player=0,value=18))
        self.assertTrue(self.copies(s,'War Mammoth',0,'graveyard',1));self.assertTrue(self.copies(s,'Grizzly Bears',0,'battlefield',1))
        s=self.extension('Crackling Doom','effect_0')['steps']
        self.assertTrue(self.has(s,op='choose_target',player=1,ref='edict1_0'))
        self.assertTrue(self.copies(s,'War Mammoth',1,'graveyard',1));self.assertTrue(self.copies(s,'Grizzly Bears',1,'battlefield',1))

    def test_real_token_creation_has_explicit_maker_characteristics(self):
        s=self.extension('Kuldotha Rebirth')['steps'];tid=self.rows['Goblin']['csvid']
        self.assertTrue(self.has(s,op='assert_zone_multiplicity',player=0,zone='battlefield',index=tid,value=3))
        for k,v in [('power',1),('toughness',1),('color',16),('subtype_Goblin',1)]:self.assertTrue(self.has(s,op='assert_matching_permanents',player=0,index=tid,property=k,value=v))
        self.assertTrue(self.copies(s,'Wall of Spears',0,'graveyard',1))

    def test_all_extension_profiles_expand_their_explicit_variants(self):
        total=0
        for name,pin in family.EXTENSION_PINS['profiles'].items():
            d=dict(family='edict_spell',version=2,params=copy.deepcopy(pin['params']),oracle_text=self.oracle[name]['oracle_text'],variants=['effect_0'])
            p=family.spec(d,self.oracle[name]);vs=family.variants(p);cs=family.expand(name,self.rows[name]['csvid'],d,p,vs,self.rows,self.oracle)
            self.assertEqual(len(vs),len(cs));total+=len(cs)
        self.assertGreaterEqual(total,1000)

    def test_removal_second_receipts_and_noncreature_target_union(self):
        for n,life in [('Launch Party',18),('Flesh Allergy',18)]:
            s=self.extension(n)['steps'];self.assertTrue(self.has(s,op='assert_life',player=1,value=life))
        s=self.extension('Flesh Allergy','indestructible_target_0')['steps']
        self.assertTrue(self.has(s,op='assert_life',player=1,value=19));self.assertTrue(self.copies(s,'Darksteel Myr',1,'battlefield',1))
        for variant,n in [('artifact_target_0','Sol Ring'),('enchantment_target_0','Seal of Cleansing')]:
            self.assertTrue(self.copies(self.extension('Angelic Purge',variant)['steps'],n,1,'exile',1))

    def test_all_creature_cost_has_no_choice_prompts_and_all_power_is_counted(self):
        s=self.extension('Soulblast')['steps']
        self.assertFalse(any(x['op']=='choose_target' and x.get('ref','').startswith('cost') for x in s))
        self.assertTrue(self.has(s,op='assert_life',player=1,value=13))
        s=self.extension('Soulblast','empty_cost_0')['steps']
        self.assertTrue(self.has(s,op='cast',ref='source',value=1));self.assertTrue(self.has(s,op='assert_life',player=1,value=20))

    def test_variable_land_quantity_pays_x_and_keeps_printed_base_at_zero(self):
        for x in (0,1,3,5):
            s=self.extension('Tectonic Break',f'payment_x{x}_0')['steps']
            self.assertTrue(self.has(s,op='choose_x',player=0,value=x))
            self.assertTrue(self.has(s,op='assert_mana',player=0,color=0,value=0));self.assertTrue(self.has(s,op='assert_mana',player=0,color=4,value=0))
            self.assertTrue(self.copies(s,'Forest',1,'battlefield',max(0,3-x)))

    def test_half_hand_and_history_quantities_have_independent_controls(self):
        self.assertTrue(self.copies(self.extension('Curse of the Cabal','wide_board_0')['steps'],'Grizzly Bears',1,'battlefield',2))
        self.assertTrue(self.copies(self.extension('Curse of the Cabal','one_recipient_0')['steps'],'Grizzly Bears',1,'battlefield',1))
        s=self.extension('Thoughts of Ruin')['steps'];self.assertTrue(self.copies(s,'Forest',1,'battlefield',2))
        s=self.extension('Urborg Justice','two_deaths_0')['steps'];self.assertEqual(s[0],dict(op='reset_death_history'))
        self.assertEqual(sum(x['op']=='destroy' and x['ref'].startswith('history') for x in s),2);self.assertTrue(self.copies(s,'Grizzly Bears',1,'battlefield',1))

    def test_filtered_edict_negative_queues_a_legal_bug_fallback_as_unused(self):
        s=self.extension('Exile into Darkness','wrong_value_1')['steps']
        self.assertTrue(self.has(s,op='choose_target',player=0,ref='edict0_0'));self.assertTrue(self.has(s,op='assert_unused_choices',value=1))
        self.assertTrue(self.copies(s,'War Mammoth',0,'battlefield',3))
        self.assertTrue(self.copies(s,'Craw Wurm',0,'battlefield',1))

    def test_filtered_edict_has_positive_receipts_for_both_recipient_controllers(self):
        self.assertEqual(self.oracle['War Mammoth']['cmc'],4)
        self.assertFalse(family.extension_eligible(self.oracle['War Mammoth'],'creature_mv3'))
        for owner,recipient in ((0,1),(1,0)):
            c=self.extension('Exile into Darkness',f'effect_{owner}');s=c['steps']
            self.assertTrue(self.copies(s,'Grizzly Bears',recipient,'graveyard',2))
            self.assertTrue(self.copies(s,'Grizzly Bears',recipient,'battlefield',2 if recipient==1 else 1))
            self.assertTrue(self.copies(s,'Craw Wurm',recipient,'battlefield',1))
        self.assertEqual(self.oracle['Wall of Spears']['cmc'],3)
        s=self.extension('Exile into Darkness','alternate_choice_1')['steps']
        self.assertTrue(self.has(s,op='choose_target',player=0,ref='edict0_1'))
        self.assertTrue(self.copies(s,'Wall of Spears',0,'graveyard',1))
        self.assertTrue(self.copies(s,'War Mammoth',0,'battlefield',1))

    def test_sweeps_mark_damage_and_stat_reductions_ignore_indestructibility(self):
        s=self.extension('Incendiary Sabotage')['steps']
        self.assertTrue(self.has(s,op='assert_card',ref='large_control',property='damage',value=3));self.assertTrue(self.copies(s,'Darksteel Myr',1,'battlefield',1))
        s=self.extension('Ichor Explosion')['steps']
        self.assertTrue(self.has(s,op='assert_card',ref='large_control',property='power',value=-2));self.assertTrue(self.copies(s,'Darksteel Myr',1,'graveyard',1))
        s=self.extension('Ichor Explosion','cleanup_0')['steps'];self.assertTrue(self.has(s,op='assert_card',ref='large_control',property='power',value=0))

    def test_dynamic_insect_tokens_use_sacrificed_power_and_source_maker_stats(self):
        s=self.extension('Carrion','alternate_choice_0')['steps'];tid=self.rows['Insect']['csvid']
        self.assertTrue(self.has(s,op='assert_zone_multiplicity',player=0,zone='battlefield',index=tid,value=3))
        self.assertTrue(self.has(s,op='assert_matching_permanents',player=0,index=tid,property='power',value=0))
        self.assertTrue(self.has(s,op='assert_matching_permanents',player=0,index=tid,property='toughness',value=1))
        s=self.extension('Carrion','indestructible_cost_0')['steps']
        self.assertEqual(self.oracle['Darksteel Myr']['power'],'0')
        self.assertTrue(self.has(s,op='assert_zone_multiplicity',player=0,zone='battlefield',index=tid,value=0))
        self.assertFalse(any(x['op']=='assert_matching_permanents' and x.get('index')==tid for x in s))
        self.assertTrue(self.copies(s,'Darksteel Myr',0,'graveyard',1))


    # Keep the old declarations and assertions independent of the new palette.
    def wave4(self,name,variant='effect_0'):
        pin=family.WAVE4_PINS['profiles'][name]
        d=dict(family='edict_spell',version=2,params=copy.deepcopy(pin['params']),oracle_text=self.oracle[name]['oracle_text'],variants=[variant])
        p=family.spec(d,self.oracle[name])
        return family.expand(name,self.rows[name]['csvid'],d,p,[variant],self.rows,self.oracle)[0]

    def test_wave4_profile_contract_is_whole_and_type_exact(self):
        pin=family.WAVE4_PINS['profiles']['Pox']
        d=dict(family='edict_spell',version=2,params=copy.deepcopy(pin['params']),oracle_text=self.oracle['Pox']['oracle_text'],variants=['effect_0'])
        for key,value in [('quantity','half_ceil'),('cost_count',True),('unreviewed',1)]:
            changed=copy.deepcopy(d);changed['params'][key]=value
            with self.assertRaises(ValueError):family.spec(changed,self.oracle['Pox'])
        changed=copy.deepcopy(self.oracle['Pox']);changed['oracle_text']+=' Draw a card.'
        with self.assertRaises(ValueError):family.spec(d,changed)

    def test_fractional_loss_rounds_each_current_resource_up(self):
        for owner in (0,1):
            s=self.wave4('Pox',f'wide_board_{owner}')['steps']
            self.assertTrue(self.has(s,op='assert_life',player=0,value=15));self.assertTrue(self.has(s,op='assert_life',player=1,value=13))
            self.assertTrue(self.copies(s,'Grizzly Bears',1,'battlefield',2))
            self.assertTrue(self.copies(s,'Forest',1,'battlefield',2))
            s=self.wave4('Fraying Omnipotence',f'one_card_{owner}')['steps']
            self.assertTrue(self.has(s,op='assert_life',player=0,value=11));self.assertTrue(self.has(s,op='assert_life',player=1,value=10))
            self.assertTrue(self.has(s,op='assert_zone',player=0,zone='hand',value=0));self.assertTrue(self.has(s,op='assert_zone',player=1,zone='hand',value=0))
            self.assertTrue(self.copies(s,'Forest',0,'battlefield',1));self.assertTrue(self.copies(s,'Forest',1,'battlefield',1))

    def test_zero_x_retains_printed_cost_and_all_resources(self):
        for owner in (0,1):
            s=self.wave4('Death Cloud',f'payment_x0_{owner}')['steps']
            self.assertTrue(self.has(s,op='choose_x',player=owner,value=0));self.assertTrue(self.has(s,op='assert_mana',player=owner,color=1,value=0))
            self.assertTrue(self.has(s,op='assert_life',player=0,value=23));self.assertTrue(self.has(s,op='assert_life',player=1,value=20))
            self.assertTrue(self.copies(s,'Grizzly Bears',1,'battlefield',3));self.assertTrue(self.copies(s,'Forest',1,'battlefield',3))

    def test_search_uses_observer_and_exact_both_controller_receipts(self):
        for owner in (0,1):
            s=self.wave4('Crop Rotation',f'search_nonbasic_{owner}')['steps'];iid=self.rows['Darksteel Citadel']['csvid']
            self.assertTrue(self.has(s,op='observe_search_filters',value=1));self.assertTrue(self.has(s,op='choose_zone_card',player=owner,value=iid))
            self.assertTrue(self.has(s,op='assert_offered_legal_card',index=iid,value=1));self.assertTrue(self.has(s,op='assert_offered_legal_card',index=self.rows['Lightning Bolt']['csvid'],value=0))
            self.assertTrue(self.copies(s,'Darksteel Citadel',owner,'battlefield',1));self.assertTrue(self.has(s,op='assert_card',ref='found',property='tapped',value=0))
            s=self.wave4('Diabolic Intent',f'effect_{owner}')['steps']
            self.assertTrue(self.has(s,op='choose_zone_card',player=owner,value=self.rows['Lightning Bolt']['csvid']))
            self.assertTrue(self.copies(s,'Lightning Bolt',owner,'hand',2));self.assertTrue(self.has(s,op='assert_forced_zone_choices',value=0))

    def test_search_countered_cost_and_decline_conserve_zones(self):
        for owner in (0,1):
            s=self.wave4('Crop Rotation',f'countered_{owner}')['steps']
            self.assertFalse(any(x['op'].startswith('choose_') and 'zone' in x['op'] for x in s))
            self.assertTrue(self.copies(s,'Forest',owner,'graveyard',2));self.assertTrue(self.copies(s,'Crop Rotation',owner,'graveyard',1))
            s=self.wave4('Crop Rotation',f'decline_search_{owner}')['steps']
            self.assertTrue(self.has(s,op='choose_optional_zone',player=owner,value=self.rows['Forest']['csvid']))
            self.assertTrue(self.copies(s,'Plains',owner,'library',1));self.assertTrue(self.copies(s,'Forest',owner,'graveyard',2))

    def test_tutor_limits_include_exact_bound_and_exclude_next_value(self):
        for owner in (0,1):
            for x,good,bad in [(0,'Memnite','Sol Ring'),(1,'Sol Ring','Wall of Spears'),(3,'Wall of Spears','Gilded Lotus'),(5,'Gilded Lotus','Phyrexian Hulk')]:
                s=self.wave4('Reshape',f'effect_x{x}_{owner}')['steps']
                self.assertTrue(self.has(s,op='choose_x',player=owner,value=x));self.assertTrue(self.copies(s,good,owner,'battlefield',1+(2 if good=='Wall of Spears' and owner==1 else 1 if good=='Wall of Spears' else 0)))
                self.assertTrue(self.has(s,op='assert_offered_legal_card',index=self.rows[good]['csvid'],value=1));self.assertTrue(self.has(s,op='assert_offered_legal_card',index=self.rows[bad]['csvid'],value=0))
            for variant,good in [('effect','War Mammoth'),('indestructible_cost','Air Elemental')]:
                s=self.wave4('Eldritch Evolution',f'{variant}_{owner}')['steps']
                self.assertTrue(self.copies(s,good,owner,'battlefield',1));self.assertTrue(self.copies(s,'Eldritch Evolution',owner,'exile',1))
            if owner==0:
                s=self.wave4('Eldritch Evolution','alternate_choice_0')['steps'];self.assertTrue(self.copies(s,'Craw Wurm',owner,'battlefield',1))

    def test_harrow_quantity_and_basic_type_have_distinct_receipts(self):
        for owner in (0,1):
            s=self.wave4('Harrow',f'effect_{owner}')['steps']
            for n in ('Plains','Island'):self.assertTrue(self.copies(s,n,owner,'battlefield',1))
            self.assertTrue(self.has(s,op='assert_offered_count',value=len(family.WAVE4_PINS['library_palette'])-1))
            self.assertTrue(self.has(s,op='assert_offered_legal_card',index=self.rows['Darksteel Citadel']['csvid'],value=0))
            s=self.wave4('Harrow',f'one_search_{owner}')['steps']
            self.assertTrue(self.copies(s,'Plains',owner,'battlefield',1));self.assertTrue(self.copies(s,'Island',owner,'battlefield',0))
            s=self.wave4('Natural Order',f'empty_cost_{owner}')['steps']
            self.assertTrue(self.has(s,op='require_can_cast',ref='source',value=0));self.assertTrue(self.copies(s,'Hill Giant',owner,'battlefield',1))
            self.assertTrue(self.copies(s,'Exploration',owner,'battlefield',1))
            s=self.wave4('Natural Order',f'effect_{owner}')['steps']
            self.assertTrue(self.has(s,op='assert_offered_legal_card',index=self.rows['Giant Growth']['csvid'],value=0))

    def test_symmetric_land_damage_has_own_and_opposing_survivors(self):
        for owner in (0,1):
            for name,damage,left in [('Wildfire',4,2),('Destructive Force',5,1)]:
                s=self.wave4(name,f'effect_{owner}')['steps']
                for pl in (0,1):
                    self.assertTrue(self.copies(s,'Darksteel Myr',pl,'battlefield',1));self.assertTrue(self.has(s,op='assert_card',ref=f'creature_control{pl}_2',property='damage',value=damage))
                self.assertTrue(self.copies(s,'Forest',1,'battlefield',left));self.assertTrue(self.copies(s,name,owner,'graveyard',1));self.assertTrue(self.copies(s,name,owner,'exile',0))
            s=self.wave4('Destructive Force',f'effect_{owner}')['steps'];picks=[x for x in s if x['op']=='choose_target']
            self.assertEqual([x['player'] for x in picks],[owner]*5+[1-owner]*5)

    def test_equalizing_resources_uses_minimum_and_excludes_pending_source(self):
        for owner in (0,1):
            for name in ('Balance','Balancing Act'):
                s=self.wave4(name,f'effect_{owner}')['steps']
                for pl in (0,1):self.assertTrue(self.has(s,op='assert_zone',player=pl,zone='hand',value=2))
                self.assertTrue(self.copies(s,'Grizzly Bears',1,'battlefield',2))
                s=self.wave4(name,f'equal_board_{owner}')['steps']
                self.assertTrue(self.copies(s,'Grizzly Bears',1,'battlefield',3))
            s=self.wave4("Razia's Purification",f'effect_{owner}')['steps']
            for pl in (0,1):
                for n in ('Grizzly Bears','Wall of Spears','Forest'):self.assertTrue(self.copies(s,n,pl,'battlefield',1))
                self.assertTrue(self.copies(s,'Seal of Cleansing',pl,'graveyard',1));self.assertTrue(self.copies(s,'Darksteel Relic',pl,'graveyard',1))
        s=self.wave4('Balance')['steps'];picks=[x['ref'] for x in s if x['op']=='choose_target']
        self.assertEqual(picks,['edict0_10','edict0_11','edict0_12','edict0_0','edict0_1','edict0_2'])

    def test_threshold_six_seven_does_not_count_resolving_source(self):
        for owner in (0,1):
            s=self.wave4('Epicenter',f'threshold_6_{owner}')['steps'];t=self.wave4('Epicenter',f'threshold_7_{owner}')['steps']
            self.assertTrue(self.copies(s,'Forest',1,'battlefield',2 if owner==0 else 3))
            for pl in (0,1):
                self.assertTrue(self.copies(t,'Forest',pl,'battlefield',0));self.assertTrue(self.copies(t,'Mountain',pl,'battlefield',0))
                self.assertTrue(self.copies(t,'Grizzly Bears',pl,'battlefield',1))

    def test_dragon_reveal_is_not_discard_and_life_is_independent_of_sacrifice(self):
        for owner in (0,1):
            for variant,gain in [('revealed_dragon',4),('decline_reveal',0),('controlled_dragon',4),('sacrifice_dragon',4),('empty_with_dragon',4)]:
                s=self.wave4('Foul-Tongue Invocation',f'{variant}_{owner}')['steps']
                self.assertTrue(self.has(s,op='assert_life',player=owner,value=20+gain))
                if variant in ('revealed_dragon','decline_reveal'):self.assertTrue(self.copies(s,'Shivan Dragon',owner,'hand',1))
                if variant=='revealed_dragon' and owner==0:self.assertTrue(self.has(s,op='choose_dialog',player=0,value=0))
                if variant=='sacrifice_dragon':self.assertTrue(self.copies(s,'Shivan Dragon',owner,'graveyard',1));self.assertTrue(self.copies(s,'Shivan Dragon',owner,'battlefield',0))

    def test_delirium_three_four_types_and_both_recipient_planeswalker_receipts(self):
        for owner,recipient in ((0,1),(1,0)):
            s=self.wave4('To the Slaughter',f'effect_{owner}')['steps']
            self.assertTrue(self.copies(s,'Jace Beleren',recipient,'graveyard',1));self.assertTrue(self.copies(s,'Grizzly Bears',recipient,'graveyard',2))
            c=self.wave4('To the Slaughter',f'planeswalker_choice_{owner}');s=c['steps']
            self.assertTrue(self.copies(s,'Jace Beleren',recipient,'graveyard',1))
            if recipient==1:self.assertFalse(any(x['player']==1 and x['zone']=='battlefield' and x['name']=='Grizzly Bears' for x in c['setup']))
            else:self.assertTrue(self.has(s,op='choose_target',player=0,ref='edict0_10'))
            s=self.wave4('To the Slaughter',f'below_delirium_{owner}')['steps'];self.assertTrue(self.copies(s,'Jace Beleren',recipient,'graveyard',0))

    def test_city_nine_ten_and_skull_empty_half_life_round_up(self):
        for owner,recipient in ((0,1),(1,0)):
            s=self.wave4("Vona's Hunger",f'city_below_{owner}')['steps'];t=self.wave4("Vona's Hunger",f'city_at_{owner}')['steps']
            self.assertTrue(self.copies(s,'Grizzly Bears',recipient,'graveyard',2));self.assertTrue(self.copies(t,'Grizzly Bears',recipient,'graveyard',3 if recipient==1 else 2))
            if recipient==0:self.assertTrue(self.copies(s,'Hill Giant',recipient,'graveyard',0));self.assertTrue(self.copies(t,'Hill Giant',recipient,'graveyard',1))
            s=self.wave4('Skull Storm',f'empty_recipients_{owner}')['steps']
            self.assertTrue(self.has(s,op='assert_life',player=recipient,value=10 if recipient==1 else 11))

    def test_optional_payments_have_real_cast_and_countered_cost_receipts(self):
        for owner in (0,1):
            for n in ('Worthy Cause','Pegasus Stampede'):
                s=self.wave4(n,f'effect_{owner}')['steps'];self.assertTrue(self.copies(s,n,owner,'hand',1));self.assertTrue(self.has(s,op='choose_dialog',player=owner,value=0))
                s=self.wave4(n,f'ordinary_{owner}')['steps'];self.assertTrue(self.copies(s,n,owner,'graveyard',1));self.assertTrue(self.has(s,op='choose_dialog',player=owner,value=1))
                s=self.wave4(n,f'countered_{owner}')['steps'];self.assertTrue(self.copies(s,n,owner,'graveyard',1))
            s=self.wave4('Bog Down',f'effect_{owner}')['steps'];self.assertTrue(self.has(s,op='assert_zone',player=1-owner,zone='hand',value=1))
            s=self.wave4('Bog Down',f'ordinary_{owner}')['steps'];self.assertTrue(self.has(s,op='assert_zone',player=1-owner,zone='hand',value=2))
            for n in ('Bog Down','Pegasus Stampede','Pulverize'):
                s=self.wave4(n,f'empty_cost_{owner}')['steps'];self.assertTrue(self.has(s,op='cast',ref='source',value=1))
            s=self.wave4('Worthy Cause',f'payment_{owner}')['steps']
            for c in range(6):self.assertTrue(self.has(s,op='assert_mana',player=owner,color=c,value=0))

    def test_sacrifice_result_tokens_use_actual_power_and_controller(self):
        for owner in (0,1):
            s=self.wave4('Syphon Flesh',f'empty_recipients_{owner}')['steps'];self.assertTrue(self.copies(s,'Zombie',owner,'battlefield',0))
            s=self.wave4('Reign of the Pit',f'one_empty_{owner}')['steps'];tid=self.rows['Demon']['csvid']
            self.assertTrue(self.has(s,op='assert_matching_permanents',player=owner,index=tid,property='power',value=2))
            s=self.wave4('Mercy Killing',f'stolen_target_{owner}')['steps'];tid=self.rows['Elf Warrior']['csvid']
            self.assertTrue(self.copies(s,'War Mammoth',owner,'graveyard',1));self.assertTrue(self.copies(s,'Elf Warrior',1-owner,'battlefield',3))
            self.assertTrue(self.has(s,op='assert_matching_permanents',player=1-owner,index=tid,property='color',value=40))
            s=self.wave4('Mercy Killing',f'indestructible_target_{owner}')['steps'];self.assertTrue(self.copies(s,'Darksteel Myr',1-owner,'graveyard',1));self.assertTrue(self.copies(s,'Elf Warrior',1-owner,'battlefield',0))

    def test_flying_token_blocks_actual_flyer_after_real_cast_and_turn_gates(self):
        for n in ('Reign of the Pit','Pegasus Stampede'):
            for owner in (0,1):
                s=self.wave4(n,f'flying_token_{owner}')['steps']
                self.assertTrue(self.has(s,op='cast',ref='flying_attacker',value=1));self.assertEqual(sum(x['op']=='advance_turn' for x in s),2)
                self.assertTrue(self.has(s,op='block',ref='flying_token',other='flying_attacker',value=1));self.assertTrue(self.has(s,op='assert_card',ref='flying_token',property='blocking_count',value=1))

    def test_mana_receipt_uses_printed_value_and_real_zero_value_cost(self):
        for owner in (0,1):
            for v,b,r in [('effect',21,20),('red_mana',19,22),('split_mana',20,21),('zero_value_cost',19,20)]:
                s=self.wave4('Burnt Offering',f'{v}_{owner}')['steps']
                self.assertTrue(self.has(s,op='assert_mana',player=owner,color=1,value=b));self.assertTrue(self.has(s,op='assert_mana',player=owner,color=4,value=r))
                if v=='zero_value_cost':self.assertTrue(self.copies(s,'Memnite',owner,'graveyard',1));self.assertFalse(any(x['op']=='choose_color' for x in s))
            s=self.wave4('Pulverize',f'payment_{owner}')['steps']
            self.assertTrue(self.has(s,op='cast',ref='source',value=1));self.assertTrue(self.copies(s,'Mountain',owner,'graveyard',2))
            self.assertTrue(self.has(s,op='choose_dialog',player=owner,value=0))
            for c in range(6):self.assertTrue(self.has(s,op='assert_mana',player=owner,color=c,value=0))
            s=self.wave4('Burnt Offering',f'countered_{owner}')['steps']
            self.assertTrue(self.has(s,op='cast',ref='source',value=1));self.assertTrue(self.has(s,op='cast_response',ref='counter',value=1))
            self.assertTrue(self.has(s,op='assert_stack',value=1));self.assertTrue(self.has(s,op='assert_stack',value=2));self.assertTrue(self.has(s,op='resolve_response'))
            self.assertFalse(any(x['op'] in ('choose_color','choose_priority') for x in s))
            self.assertTrue(self.has(s,op='assert_mana',player=owner,color=1,value=19))

    def test_target_hexproof_response_and_both_hybrid_payment_colors(self):
        for owner in (0,1):
            s=self.wave4('Mercy Killing',f'target_hexproof_{owner}')['steps']
            self.assertTrue(self.has(s,op='cast_response',ref='response',value=1));self.assertTrue(self.has(s,op='assert_stack',value=2));self.assertTrue(self.copies(s,'Elf Warrior',1-owner,'battlefield',0))
            self.assertTrue(self.has(s,op='assert_card',ref='victim',property='power',value=4))
            s=self.wave4('Mercy Killing',f'no_creatures_{owner}')['steps'];self.assertTrue(self.has(s,op='require_can_cast',ref='source',value=0))
            c=self.wave4('Mercy Killing',f'hybrid_other_{owner}');s=c['steps']
            self.assertTrue(self.has(s,op='set_mana',player=owner,color=3,value=0));self.assertTrue(self.has(s,op='set_mana',player=owner,color=5,value=1))


    def wave5(self,name,variant):
        p=copy.deepcopy(family.WAVE5_PINS['profiles'][name]['params'])
        d=dict(family='edict_spell',version=2,params=p,oracle_text=self.oracle[name]['oracle_text'],variants=[variant])
        from recipes import expand
        return expand(name,self.rows[name]['csvid'],d,self.oracle[name],self.rows,self.oracle)[0]

    def test_wave5_profiles_reject_source_and_parameter_drift(self):
        for name,pin in family.WAVE5_PINS['profiles'].items():
            d=dict(family='edict_spell',version=2,params=copy.deepcopy(pin['params']),oracle_text=self.oracle[name]['oracle_text'],variants=['effect_0'])
            family.spec(d,self.oracle[name]);d['params']['quantity']=99
            with self.assertRaises(ValueError):family.spec(d,self.oracle[name])
            d['params']=copy.deepcopy(pin['params']);r=copy.deepcopy(self.oracle[name]);r['oracle_text']+=' Draw a card.';d['oracle_text']=r['oracle_text']
            with self.assertRaises(ValueError):family.spec(d,r)

    def test_living_death_returns_only_initial_grave_creatures_to_their_owner(self):
        for owner in (0,1):
            s=self.wave5('Living Death',f'stolen_creatures_{owner}')['steps']
            for pl in (0,1):
                self.assertTrue(self.copies(s,'Scathe Zombies',pl,'battlefield',1));self.assertTrue(self.copies(s,'Wall of Spears',pl,'battlefield',1))
                for name in ('Grizzly Bears','Hill Giant','Darksteel Myr'):
                    self.assertTrue(self.copies(s,name,pl,'battlefield',0));self.assertTrue(self.copies(s,name,pl,'graveyard',0 if pl==owner else 2))
                self.assertTrue(self.has(s,op='assert_card',ref=f'returned{pl}_0',property='sick',value=1))
            s=self.wave5('Living Death',f'duplicate_returns_{owner}')['steps'];self.assertTrue(self.copies(s,'Scathe Zombies',owner,'battlefield',3))

    def test_trash_pays_artifact_before_counter_and_cannot_use_cost_as_initial_target(self):
        for owner in (0,1):
            s=self.wave5('Trash for Treasure',f'countered_{owner}')['steps']
            self.assertTrue(self.copies(s,'Darksteel Relic',owner,'graveyard',1));self.assertTrue(self.copies(s,'Sol Ring',owner,'graveyard',1))
            self.assertTrue(self.has(s,op='cast_response',ref='counter',value=1));self.assertTrue(self.has(s,op='assert_stack',value=2))
            s=self.wave5('Trash for Treasure',f'empty_graveyard_{owner}')['steps'];self.assertTrue(self.has(s,op='require_can_cast',ref='source',value=0));self.assertTrue(self.copies(s,'Darksteel Relic',owner,'battlefield',3))
            s=self.wave5('Trash for Treasure',f'target_removed_{owner}')['steps'];self.assertTrue(self.copies(s,'Darksteel Relic',owner,'graveyard',1));self.assertTrue(self.copies(s,'Sol Ring',owner,'battlefield',0));self.assertTrue(self.has(s,op='assert_zone_card',player=owner,zone='library',index=0,value=self.rows['Sol Ring']['csvid']))

    def test_reanimated_artifact_and_creature_have_functional_fresh_entry_actions(self):
        for owner in (0,1):
            s=self.wave5('Trash for Treasure',f'returned_activate_{owner}')['steps'];self.assertTrue(self.has(s,op='activate',ref='returned',value=1));self.assertTrue(self.has(s,op='assert_mana',player=owner,color=0,value=40))
            s=self.wave5('Trash for Treasure',f'returned_attack_{owner}')['steps'];self.assertTrue(self.has(s,op='attack',ref='returned',value=0));self.assertTrue(self.has(s,op='attack',ref='returned',value=1));self.assertTrue(self.has(s,op='assert_card',ref='returned',property='sick',value=1))

    def test_control_predicate_cost_and_original_owner_are_independent(self):
        for owner in (0,1):
            for variant in ('black_only','artifact_only','wrong_type'):
                c=self.wave5('Ritual of the Machine',f'{variant}_{owner}');self.assertTrue(self.has(c['steps'],op='require_can_cast',ref='source',value=0))
                for item in c['setup']:
                    if item['zone']!='battlefield':continue
                    r=self.oracle[item['name']];self.assertFalse('Creature' in r['type_line'] and 'Artifact' not in r['type_line'] and 'B' not in r['colors'])
            s=self.wave5('Ritual of the Machine',f'destroy_controlled_{owner}')['steps'];self.assertTrue(self.copies(s,'War Mammoth',1-owner,'graveyard',1));self.assertTrue(self.copies(s,'War Mammoth',owner,'graveyard',0))
            s=self.wave5('Ritual of the Machine',f'shroud_cost_{owner}')['steps'];self.assertTrue(self.copies(s,'Deadly Insect',owner,'graveyard',1))
        s=self.wave5('Ritual of the Machine','own_target_0')['steps'];self.assertTrue(self.has(s,op='assert_card',ref='victim',property='sick',value=0))

    def test_delve_exiles_actual_cards_and_reduces_only_generic_payment(self):
        for owner in (0,1):
            for variant,generic in [('delve_one',32),('delve_all',35)]:
                s=self.wave5('Dead Drop',f'{variant}_{owner}')['steps'];self.assertTrue(self.has(s,op='assert_mana',player=owner,color=0,value=generic));self.assertTrue(self.has(s,op='assert_mana',player=owner,color=1,value=19));self.assertTrue(self.copies(s,'Forest',owner,'exile',1))
            s=self.wave5('Dead Drop',f'delve_payment_{owner}')['steps']
            self.assertFalse(any(x['op']=='choose_dialog' for x in s));self.assertTrue(self.has(s,op='choose_zone_card',player=owner,value=self.rows['Forest']['csvid']))
            for color in range(6):self.assertTrue(self.has(s,op='assert_mana',player=owner,color=color,value=0))
            s=self.wave5('Dead Drop',f'delve_countered_{owner}')['steps'];self.assertTrue(self.copies(s,'Forest',owner,'exile',1));self.assertTrue(self.has(s,op='cast_response',ref='counter',value=1));self.assertTrue(self.copies(s,'Grizzly Bears',1-owner,'graveyard',0))

    def test_scrap_mastery_uses_artifact_predicate_and_original_grave_cohort(self):
        for owner in (0,1):
            s=self.wave5('Scrap Mastery',f'stolen_permanents_{owner}')['steps']
            for pl in (0,1):
                self.assertTrue(self.copies(s,'Sol Ring',pl,'battlefield',1));self.assertTrue(self.copies(s,'Phyrexian Hulk',pl,'battlefield',1))
                self.assertTrue(self.copies(s,'Grizzly Bears',pl,'battlefield',1));self.assertTrue(self.copies(s,'Seal of Cleansing',pl,'battlefield',1))
                for n in ('Wall of Spears','Darksteel Myr','Darksteel Citadel','Darksteel Relic'):
                    self.assertTrue(self.copies(s,n,pl,'battlefield',0));self.assertTrue(self.copies(s,n,pl,'graveyard',0 if pl==owner else 2))
            s=self.wave5('Scrap Mastery',f'duplicate_returns_{owner}')['steps'];self.assertTrue(self.copies(s,'Sol Ring',owner,'battlefield',3))
            s=self.wave5('Scrap Mastery',f'older_exile_artifact_{owner}')['steps']
            for pl in (0,1):self.assertTrue(self.copies(s,'Memnite',pl,'exile',1));self.assertTrue(self.copies(s,'Memnite',pl,'battlefield',0))

    def test_keep_choices_use_correct_chooser_and_unique_overlapping_object(self):
        s=self.wave5('Cataclysm','overlap_keep_1')['steps']
        self.assertTrue(self.has(s,op='choose_target',player=0,ref='keep_0_artifact_0'))
        self.assertEqual(sum(x.get('op')=='choose_target' and x.get('ref')=='keep_0_artifact_0' for x in s),2)
        self.assertTrue(self.copies(s,'Wall of Spears',0,'battlefield',1));self.assertTrue(self.copies(s,'Grizzly Bears',0,'battlefield',0));self.assertTrue(self.copies(s,'Hill Giant',0,'battlefield',0))
        s=self.wave5('Tragic Arrogance','overlap_keep_0')['steps']
        for pl in (0,1):
            self.assertTrue(self.has(s,op='choose_target',player=0,ref=f'keep_{pl}_artifact_0'));self.assertTrue(self.copies(s,'Forest',pl,'battlefield',1));self.assertTrue(self.copies(s,'Darksteel Citadel',pl,'battlefield',1))
        for name in ('Cataclysm','Tragic Arrogance'):
            for owner in (0,1):
                c=self.wave5(name,f'indestructible_rest_{owner}');s=c['steps'];self.assertTrue(self.copies(s,'Darksteel Relic',owner,'graveyard',1));self.assertTrue(self.copies(s,'Predator Ooze',owner,'graveyard',1))
                creatures=[x for x in c['setup'] if x['zone']=='battlefield' and 'Creature' in self.oracle[x['name']]['type_line']]
                self.assertTrue(creatures);self.assertTrue(all('Artifact' not in self.oracle[x['name']]['type_line'] for x in creatures))

    def test_keep_script_bindings_include_ai_and_preserve_oracle_own_keeps(self):
        s=self.wave5('Cataclysm','effect_1')['steps']
        for pl in (0,1):self.assertEqual(sum(x['op']=='choose_target' and x.get('player')==pl for x in s),4)
        s=self.wave5('Tragic Arrogance','effect_1')['steps'];choices=[x for x in s if x['op']=='choose_target']
        self.assertEqual(len(choices),8);self.assertTrue(all(x['player']==1 and x['ref'].startswith('keep_0_') for x in choices))
        for n in ('Sol Ring','Grizzly Bears','Seal of Cleansing','Jace Beleren'):self.assertTrue(self.copies(s,n,1,'battlefield',1))
        s=self.wave5('Tragic Arrogance','one_empty_1')['steps'];self.assertTrue(all(x['ref'].startswith('keep_1_') for x in s if x['op']=='choose_target'))

    def test_combat_edicts_declare_real_attackers_and_preserve_ready_controls(self):
        for owner in (0,1):
            s=self.wave5('Celestial Flare',f'effect_{owner}')['steps']
            self.assertTrue(self.has(s,op='begin_attackers'));self.assertTrue(self.has(s,op='finish_attackers'));self.assertTrue(self.has(s,op='attack',ref='participant0',value=1))
            self.assertTrue(self.has(s,op='assert_card',ref='ready_nonparticipant',property='in_play',value=1));self.assertTrue(self.has(s,op='assert_card',ref='ready_nonparticipant',property='tapped',value=0))
            s=self.wave5('Celestial Flare',f'blocking_{owner}')['steps'];self.assertTrue(self.has(s,op='block',ref='participant0',other='combat_attacker',value=1));self.assertTrue(self.has(s,op='advance_combat'))
            s=self.wave5('Dispense Justice',f'blocking_negative_{owner}')['steps'];self.assertTrue(self.has(s,op='assert_card',ref='participant0',property='in_play',value=1))
            s=self.wave5('Celestial Flare',f'empty_recipients_{owner}')['steps'];self.assertTrue(self.has(s,op='cast',ref='source',value=1));self.assertTrue(self.has(s,op='assert_card',ref='ready_nonparticipant',property='in_play',value=1))

    def test_metalcraft_is_checked_at_resolution_at_two_three_artifacts(self):
        for owner,recipient in ((0,1),(1,0)):
            ordinary=self.wave5('Dispense Justice',f'effect_{owner}')['steps'];at=self.wave5('Dispense Justice',f'metalcraft_at_{owner}')['steps'];removed=self.wave5('Dispense Justice',f'metalcraft_removed_{owner}')['steps']
            if recipient==1:
                self.assertTrue(self.copies(ordinary,'Grizzly Bears',recipient,'graveyard',2));self.assertTrue(self.copies(at,'Grizzly Bears',recipient,'graveyard',3));self.assertTrue(self.copies(removed,'Grizzly Bears',recipient,'graveyard',2))
            else:
                self.assertTrue(self.copies(ordinary,'Hill Giant',recipient,'graveyard',0));self.assertTrue(self.copies(at,'Hill Giant',recipient,'graveyard',1));self.assertTrue(self.copies(removed,'Hill Giant',recipient,'graveyard',0))
            self.assertTrue(self.has(removed,op='assert_stack',value=1));self.assertTrue(self.has(removed,op='destroy',ref='metalcraft0'))

    def test_cruel_ultimatum_models_all_six_ordered_resource_effects(self):
        for owner in (0,1):
            s=self.wave5('Cruel Ultimatum',f'effect_{owner}')['steps']
            self.assertTrue(self.has(s,op='assert_life',player=owner,value=25));self.assertTrue(self.has(s,op='assert_life',player=1-owner,value=15))
            self.assertTrue(self.has(s,op='assert_zone',player=owner,zone='hand',value=8));self.assertTrue(self.has(s,op='assert_zone',player=1-owner,zone='hand',value=1))
            self.assertTrue(self.copies(s,'Scathe Zombies',owner,'hand',1));self.assertTrue(self.has(s,op='assert_zone',player=owner,zone='library',value=25))
            s=self.wave5('Cruel Ultimatum',f'empty_recipients_{owner}')['steps'];self.assertTrue(self.has(s,op='assert_life',player=1-owner,value=15));self.assertTrue(self.has(s,op='assert_zone',player=1-owner,zone='hand',value=1))

    def test_offering_draws_only_for_actual_sacrifices_but_always_returns_both(self):
        for owner in (0,1):
            s=self.wave5('Infernal Offering',f'one_empty_{owner}')['steps'];self.assertTrue(self.has(s,op='assert_zone',player=owner,zone='library',value=26));self.assertTrue(self.has(s,op='assert_zone',player=1-owner,zone='library',value=28))
            for pl in (0,1):self.assertTrue(self.copies(s,'Scathe Zombies',pl,'battlefield',1));self.assertTrue(self.has(s,op='assert_card',ref=f'resource_return{pl}',property='sick',value=1))
            s=self.wave5('Infernal Offering',f'no_initial_grave_creatures_{owner}')['steps']
            for pl in (0,1):self.assertTrue(self.copies(s,'Grizzly Bears',pl,'battlefield',1));self.assertTrue(self.copies(s,'Grizzly Bears',pl,'graveyard',0));self.assertTrue(self.has(s,op='assert_card',ref=f'resource_return{pl}',property='sick',value=1))

    def test_spite_cost_excludes_source_and_counter_survives_on_opponent_side(self):
        for owner in (0,1):
            c=self.wave5("Kaervek's Spite",f'countered_{owner}');s=c['steps'];counter=next(x for x in c['setup'] if x['ref']=='counter');self.assertEqual(counter['player'],1-owner)
            self.assertTrue(self.has(s,op='assert_zone',player=owner,zone='hand',value=1));self.assertTrue(self.has(s,op='assert_zone',player=owner,zone='hand',value=0));self.assertTrue(self.has(s,op='assert_life',player=1-owner,value=20))
            for n in ('Grizzly Bears','Darksteel Myr','Forest','Sol Ring','Seal of Cleansing','Jace Beleren'):self.assertTrue(self.copies(s,n,owner,'battlefield',0))
            s=self.wave5("Kaervek's Spite",f'empty_cost_{owner}')['steps'];self.assertTrue(self.has(s,op='require_can_cast',ref='source',value=1));self.assertTrue(self.has(s,op='cast',ref='source',value=1))

    def test_runes_all_draws_precede_model_decisions_and_sacrifice_is_optional(self):
        for owner in (0,1):
            s=self.wave5('Read the Runes',f'effect_{owner}')['steps'];self.assertTrue(self.has(s,op='choose_x',player=owner,value=2));self.assertTrue(self.copies(s,'Grizzly Bears',owner,'graveyard',3));self.assertTrue(self.has(s,op='assert_zone',player=owner,zone='hand',value=6))
            s=self.wave5('Read the Runes',f'empty_recipients_{owner}')['steps'];self.assertEqual([x['value'] for x in s if x['op']=='choose_dialog'],[0,0] if owner==1 else []);self.assertTrue(self.has(s,op='assert_zone',player=owner,zone='hand',value=4))
        s=self.wave5('Read the Runes','draw_before_decisions_0')['steps']
        self.assertTrue(self.copies(s,'Phyrexian Hulk',0,'hand',1));self.assertTrue(self.copies(s,'Giant Growth',0,'hand',0));self.assertTrue(self.copies(s,'Giant Growth',0,'graveyard',2))
        self.assertEqual([x['value'] for x in s if x['op']=='choose_dialog'],[0,1])
        s=self.wave5('Read the Runes','discard_mode_1')['steps'];self.assertEqual([x['value'] for x in s if x['op']=='choose_dialog'],[0,0,0,0])
        s=self.wave5('Read the Runes','mixed_mode_1')['steps'];self.assertEqual([x['value'] for x in s if x['op']=='choose_dialog'],[1,0,0])

    def test_dreams_x_is_additional_random_discard_not_generic_mana(self):
        for owner in (0,1):
            s=self.wave5('Devastating Dreams',f'effect_x3_{owner}')['steps'];self.assertTrue(self.has(s,op='choose_number',player=owner,value=3));self.assertFalse(any(x['op']=='choose_x' for x in s))
            self.assertTrue(self.has(s,op='assert_mana',player=owner,color=0,value=40));self.assertTrue(self.has(s,op='assert_mana',player=owner,color=4,value=18));self.assertTrue(self.has(s,op='assert_zone',player=owner,zone='hand',value=1))
            for pl in (0,1):self.assertTrue(self.copies(s,'Hill Giant',pl,'graveyard',1));self.assertTrue(self.has(s,op='assert_card',ref=f'creature_control{pl}_3',property='in_play',value=1))
            s=self.wave5('Devastating Dreams',f'empty_hand_{owner}')['steps'];self.assertTrue(self.has(s,op='choose_number',player=owner,value=0));self.assertTrue(self.has(s,op='cast',ref='source',value=1))

    def test_two_target_source_and_sacrificing_controller_make_distinct_choices(self):
        for owner in (0,1):
            s=self.wave5("Barrin's Spite",f'effect_{owner}')['steps'];choices=[x for x in s if x['op']=='choose_target']
            self.assertEqual([(x['player'],x['ref']) for x in choices],[(owner,'victim'),(owner,'victim1'),(1-owner,'victim')])
            s=self.wave5("Barrin's Spite",f'stolen_target_{owner}')['steps'];self.assertTrue(self.copies(s,'War Mammoth',owner,'graveyard',1));self.assertTrue(self.copies(s,'Hill Giant',1-owner,'hand',1))
            s=self.wave5("Barrin's Spite",f'alternate_choice_{owner}')['steps'];self.assertTrue(self.copies(s,'Hill Giant',1-owner,'graveyard',1));self.assertTrue(self.copies(s,'War Mammoth',1-owner,'hand',1))

    def test_retribution_counter_is_persistent_and_zero_toughness_kills_indestructible(self):
        for owner in (0,1):
            for v in ('effect','cleanup'):
                s=self.wave5('Retribution',f'{v}_{owner}')['steps'];self.assertTrue(self.has(s,op='assert_card',ref='victim1',property='minus1_counters',value=1));self.assertTrue(self.has(s,op='assert_card',ref='victim1',property='toughness',value=2))
            s=self.wave5('Retribution',f'indestructible_target_{owner}')['steps'];self.assertTrue(self.copies(s,'Darksteel Myr',1-owner,'graveyard',2));self.assertTrue(self.copies(s,'Darksteel Myr',1-owner,'battlefield',0))

    def test_partial_two_target_effect_keeps_correct_oracle_sacrifice(self):
        for n in ("Barrin's Spite",'Retribution'):
            for owner in (0,1):
                s=self.wave5(n,f'second_target_left_{owner}')['steps'];self.assertTrue(self.copies(s,'War Mammoth',1-owner,'graveyard',1));self.assertTrue(self.copies(s,'Hill Giant',1-owner,'hand',1));self.assertTrue(self.has(s,op='resolve_response'))
                s=self.wave5(n,f'both_targets_left_{owner}')['steps'];self.assertTrue(self.copies(s,'War Mammoth',1-owner,'hand',1));self.assertTrue(self.copies(s,'Hill Giant',1-owner,'hand',1));self.assertFalse(any(x['op']=='choose_dialog' for x in s))

    def test_river_optional_cost_and_unaffected_target_are_independent(self):
        for owner in (0,1):
            s=self.wave5('Rushing River',f'ordinary_{owner}')['steps'];self.assertTrue(self.copies(s,'War Mammoth',1-owner,'hand',1));self.assertTrue(self.copies(s,'Hill Giant',1-owner,'battlefield',1));self.assertTrue(self.has(s,op='choose_dialog',player=owner,value=1))
            s=self.wave5('Rushing River',f'empty_cost_{owner}')['steps'];self.assertTrue(self.has(s,op='cast',ref='source',value=1));self.assertFalse(any(x['op']=='choose_dialog' for x in s))
            s=self.wave5('Rushing River',f'target_hexproof_{owner}')['steps'];self.assertTrue(self.copies(s,'War Mammoth',1-owner,'battlefield',1));self.assertTrue(self.copies(s,'Hill Giant',1-owner,'hand',1))
            for v,expected in [('stolen_cost','Forest'),('indestructible_cost','Darksteel Citadel')]:
                c=self.wave5('Rushing River',f'{v}_{owner}');lands=[x for x in c['setup'] if x['player']==owner and x['zone']=='battlefield' and 'Land' in self.oracle[x['name']]['type_line']]
                self.assertEqual(len(lands),3);self.assertTrue(all(x['name']==expected or owner==0 and v=='stolen_cost' and x['name']=='Mountain' for x in lands))
                if v=='stolen_cost':self.assertTrue(all('opponent_owned' in x['state'] for x in lands))

    def test_mimic_exact_name_filter_cost_and_nontoken_target_are_explicit(self):
        for owner in (0,1):
            s=self.wave5('Mask of the Mimic',f'effect_{owner}')['steps'];self.assertTrue(self.has(s,op='observe_search_filters',value=1));self.assertTrue(self.has(s,op='assert_offered_legal_card',index=self.rows['War Mammoth']['csvid'],value=1));self.assertTrue(self.has(s,op='assert_offered_legal_card',index=self.rows['Grizzly Bears']['csvid'],value=0));self.assertTrue(self.copies(s,'War Mammoth',owner,'battlefield',1));self.assertTrue(self.has(s,op='assert_card',ref='found',property='sick',value=1))
            s=self.wave5('Mask of the Mimic',f'wrong_library_{owner}')['steps'];self.assertTrue(self.copies(s,'War Mammoth',owner,'battlefield',0));self.assertTrue(self.copies(s,'Grizzly Bears',owner,'graveyard',2))
            c=self.wave5('Mask of the Mimic',f'tokens_only_{owner}');self.assertTrue(self.has(c['steps'],op='require_can_cast',ref='source',value=0));self.assertTrue(all('token' in x['state'] for x in c['setup'] if x['zone']=='battlefield' and 'Creature' in self.oracle[x['name']]['type_line']))
        s=self.wave5('Mask of the Mimic','own_target_0')['steps'];self.assertTrue(self.has(s,op='bind_permanent_copy',ref='found',index=1));self.assertTrue(self.has(s,op='assert_card',ref='victim',property='tapped',value=1));self.assertTrue(self.has(s,op='assert_card',ref='victim',property='sick',value=0));self.assertTrue(self.has(s,op='assert_card',ref='found',property='tapped',value=0));self.assertTrue(self.has(s,op='assert_card',ref='found',property='sick',value=1))

    def test_ruthless_disposal_pays_both_real_costs_and_preserves_opposing_counter(self):
        for owner in (0,1):
            c=self.wave5('Ruthless Disposal',f'countered_{owner}');s=c['steps'];self.assertEqual(next(x for x in c['setup'] if x['ref']=='counter')['player'],1-owner)
            self.assertTrue(self.copies(s,'Giant Growth',owner,'hand',0));self.assertTrue(self.copies(s,'Giant Growth',owner,'graveyard',2));self.assertTrue(self.copies(s,'Grizzly Bears',owner,'graveyard',2));self.assertTrue(self.copies(s,'War Mammoth',1-owner,'battlefield',1))
            self.assertFalse(any(x['op']=='choose_zone_card' for x in s));self.assertTrue(self.has(s,op='cast_response',ref='counter',value=1))
            s=self.wave5('Ruthless Disposal',f'empty_hand_{owner}')['steps'];self.assertTrue(self.has(s,op='require_can_cast',ref='source',value=0));self.assertFalse(any(x['op']=='cast' for x in s))

    def test_minus_thirteen_has_exact_thirteen_fourteen_and_cleanup_receipts(self):
        for owner in (0,1):
            s=self.wave5('Ruthless Disposal',f'boundary_target_{owner}')['steps'];self.assertTrue(self.copies(s,'Krosan Cloudscraper',1-owner,'graveyard',1))
            s=self.wave5('Ruthless Disposal',f'large_target_{owner}')['steps'];self.assertTrue(self.has(s,op='assert_card',ref='victim',property='power',value=-4));self.assertTrue(self.has(s,op='assert_card',ref='victim',property='toughness',value=1))
            s=self.wave5('Ruthless Disposal',f'cleanup_{owner}')['steps'];self.assertTrue(self.has(s,op='assert_card',ref='victim',property='power',value=9));self.assertTrue(self.has(s,op='assert_card',ref='victim',property='toughness',value=14))
            s=self.wave5('Ruthless Disposal',f'indestructible_target_{owner}')['steps'];self.assertTrue(self.copies(s,'Darksteel Myr',1-owner,'graveyard',2))

    def test_eliminate_x_counts_targets_and_sacrifices_with_fixed_printed_mana(self):
        for owner in (0,1):
            for x in (0,1,2,3):
                s=self.wave5('Eliminate the Competition',f'effect_x{x}_{owner}')['steps'];self.assertFalse(any(st['op']=='choose_x' for st in s));self.assertTrue(self.has(s,op='assert_mana',player=owner,color=0,value=36));self.assertTrue(self.has(s,op='assert_mana',player=owner,color=1,value=19));self.assertTrue(self.has(s,op='choose_dialog',player=owner,value=0 if x else 1))
            s=self.wave5('Eliminate the Competition',f'empty_cost_{owner}')['steps'];self.assertTrue(self.has(s,op='require_can_cast',ref='source',value=1));self.assertTrue(self.has(s,op='cast',ref='source',value=1));self.assertTrue(self.copies(s,'War Mammoth',1-owner,'battlefield',1))
            s=self.wave5('Eliminate the Competition',f'payment_x3_{owner}')['steps']
            for color in range(6):self.assertTrue(self.has(s,op='assert_mana',player=owner,color=color,value=0))

    def test_sacrificed_cost_can_be_target_without_assuming_ai_slot_policy(self):
        for n in ('Ruthless Disposal','Eliminate the Competition'):
            for owner in (0,1):
                c=self.wave5(n,f'cost_is_target_{owner}');s=c['steps'];own=[x for x in c['setup'] if x['player']==owner and x['zone']=='battlefield' and 'Creature' in self.oracle[x['name']]['type_line']]
                self.assertEqual([x['ref'] for x in own],['cost0']);self.assertTrue(self.has(s,op='choose_target',player=owner,ref='cost0'));self.assertTrue(self.copies(s,'Grizzly Bears',owner,'graveyard',2));self.assertTrue(self.copies(s,'War Mammoth',1-owner,'battlefield',1))
                self.assertTrue(self.copies(s,'Hill Giant',1-owner,'graveyard',1) if n=='Ruthless Disposal' else self.copies(s,'Hill Giant',1-owner,'battlefield',1))


if __name__=='__main__':unittest.main()
