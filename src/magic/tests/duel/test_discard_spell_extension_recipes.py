import copy
import json
import unittest
from pathlib import Path
from generate_cases import registry
import discard_spell_extension_recipes as f


class DiscardSpellExtensionTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.o=json.loads((Path(__file__).parent/'data/oracle.json').read_text(encoding='utf8'))['cards']
        cls.rows={r['name']:r for r in registry()}

    def declaration(self,name,variant='effect_0'):
        d=dict(family='discard_spell_extension',version=2,params=copy.deepcopy(f.PROFILES[name]['params']),oracle_text=self.o[name]['oracle_text'],variants=[variant])
        if name in ('Ostracize',"Rakshasa's Secret",'Diplomacy of the Wastes','Lay Bare the Heart',"Mire's Malice"):d['target_choices']={'0':0,'1':0}
        return d

    def cases(self,name='Distress',variant='effect_0',d=None):
        d=d or self.declaration(name,variant);p=f.spec(d,self.o[name])
        return f.expand(name,self.rows[name]['csvid'],d,p,d['variants'],self.rows,self.o)

    def has(self,steps,**fields):return any(all(s.get(k)==v for k,v in fields.items()) for s in steps)

    def copies(self,s,n,pl,zone,count):return self.has(s,op='assert_zone_multiplicity',index=self.rows[n]['csvid'],player=pl,zone=zone,value=count)

    def test_all_pilot_profiles_expand_both_controllers(self):
        f.validate_witnesses(self.o,self.rows)
        for n in f.PROFILES:
            d=self.declaration(n);p=f.spec(d,self.o[n]);vs=f.variants(p)
            cs=f.expand(n,self.rows[n]['csvid'],d,p,vs,self.rows,self.o)
            self.assertEqual(len(cs),len(vs));self.assertTrue(any(v.endswith('_1') for v in vs))

    def test_source_and_complete_parameters_are_pinned(self):
        for key,value in [('oracle_text','Target player discards a card.'),('version',1),('family','hand_exchange')]:
            d=self.declaration('Distress');d[key]=value
            with self.assertRaises(ValueError):self.cases(d=d)
        d=self.declaration('Thoughtseize');d['params']['life_loss']=0
        with self.assertRaises(ValueError):self.cases('Thoughtseize',d=d)
        d=self.declaration('Distress');d['params']['life_loss']=False
        with self.assertRaises(ValueError):self.cases(d=d)
        r=copy.deepcopy(self.o['Distress']);r['oracle_text']+=' You gain 3 life.'
        with self.assertRaises(ValueError):f.spec(self.declaration('Distress'),r)
        d=self.declaration('Distress');d['setup']=[]
        with self.assertRaises(ValueError):self.cases(d=d)

    def test_variants_are_explicit_distinct_and_reviewed(self):
        for variants in ([],['invented_0'],['effect_0','effect_0'],[True]):
            d=self.declaration('Distress');d['variants']=variants
            with self.assertRaises(ValueError):self.cases(d=d)

    def test_witness_pin_rejects_type_value_and_keyword_changes(self):
        for n,key,val in [('Grizzly Bears','type_line','Land'),('Craw Wurm','cmc',3),('Isamaru, Hound of Konda','type_line','Creature — Dog')]:
            o=copy.deepcopy(self.o);o[n][key]=val
            with self.assertRaises(ValueError):f.validate_witnesses(o,self.rows)

    def test_chosen_card_identity_and_caster_choice_are_exact(self):
        s=self.cases()[0]['steps'];self.assertTrue(self.has(s,op='choose_zone_card',player=0,value=self.rows['Grizzly Bears']['csvid']))
        self.assertTrue(self.copies(s,'Grizzly Bears',1,'hand',0));self.assertTrue(self.copies(s,'Grizzly Bears',1,'graveyard',2))
        s=self.cases(variant='alternate_choice_0')[0]['steps']
        self.assertTrue(self.has(s,op='choose_zone_card',player=0,value=self.rows['Isamaru, Hound of Konda']['csvid']))
        self.assertTrue(self.copies(s,'Grizzly Bears',1,'hand',1))

    def test_offered_hand_predicates_include_ineligible_controls(self):
        s=self.cases('Ostracize')[0]['steps']
        for n,val in [('Grizzly Bears',1),('Craw Wurm',1),('Sol Ring',0),('Plains',0),('Lightning Bolt',0)]:
            self.assertTrue(self.has(s,op='assert_offered_legal_card',index=self.rows[n]['csvid'],value=val))
        self.assertFalse(any(x['op']=='choose_player' for x in s))

    def test_empty_and_ineligible_hands_still_allow_legal_spell(self):
        for v in ('empty_hand_0','no_eligible_0'):
            s=self.cases('Ostracize',v)[0]['steps'];self.assertTrue(self.has(s,op='cast',ref='source',value=1))
            self.assertFalse(any(x['op']=='choose_zone_card' for x in s))
        self.assertTrue(self.has(self.cases('Ostracize','no_eligible_0')[0]['steps'],op='choose_optional_zone',player=0))

    def test_thoughtseize_life_receipt_is_after_legal_resolution(self):
        for v,life in [('effect_0',18),('no_eligible_0',18),('empty_hand_0',18),('countered_0',20),('target_shrouded_pending_0',20)]:
            self.assertTrue(self.has(self.cases('Thoughtseize',v)[0]['steps'],op='assert_life',player=0,value=life))

    def test_pending_spell_stays_physically_in_hand_and_snapshots_replay(self):
        c=self.cases(variant='pending_snapshot_1')[0];s=c['steps'];capture=next(i for i,x in enumerate(s) if x['op']=='capture')
        self.assertTrue(self.has(s[:capture],op='assert_zone',player=1,zone='hand',value=8))
        self.assertEqual(sum(x['op']=='resolve' for x in s),2);self.assertTrue(self.has(s,op='assert_recorded_state'))

    def test_ordinary_pipeline_keeps_full_oracle_and_component_gaps(self):
        c=self.cases("Raven's Crime")[0]
        self.assertIn('Retrace',c['_recipe']['oracle_text']);self.assertTrue(any('Retrace' in s for s in c['_recipe']['untested_components']))
        self.assertTrue(self.has(c['steps'],op='discard_policy',value=1))
        self.assertTrue(self.has(self.cases('Horrifying Revelation')[0]['steps'],op='assert_zone',player=1,zone='library',value=27))

    def test_unrestricted_choice_includes_land_and_omits_redundant_negative(self):
        d=self.declaration('Nightmare Void');p=f.spec(d,self.o['Nightmare Void'])
        self.assertNotIn('no_eligible_0',f.variants(p))
        s=self.cases('Nightmare Void','land_choice_0')[0]['steps']
        self.assertTrue(self.has(s,op='choose_zone_card',player=0,value=self.rows['Plains']['csvid']))
        self.assertTrue(self.copies(s,'Plains',1,'hand',0));self.assertTrue(self.copies(s,'Plains',1,'graveyard',1))

    def test_value_boundary_legend_and_planeswalker_predicates_are_independent(self):
        for n,want in [('Divination',True),('Craw Wurm',False),('Plains',False)]:self.assertEqual(f.eligible(self.o[n],'nonland_mv3'),want)
        self.assertFalse(f.eligible(self.o['Isamaru, Hound of Konda'],'nonlegendary_nonland'))
        self.assertTrue(f.eligible(self.o['Jace Beleren'],'creature_or_planeswalker'))
        s=self.cases('Inquisition of Kozilek','value_boundary_1')[0]['steps']
        self.assertTrue(self.has(s,op='choose_zone_card',player=1,value=self.rows['Divination']['csvid']))
        s=self.cases('Despise','planeswalker_0')[0]['steps']
        self.assertTrue(self.copies(s,'Jace Beleren',1,'graveyard',1))

    def test_artifact_hand_choice_is_not_confused_with_creature_only(self):
        s=self.cases('Divest','artifact_choice_0')[0]['steps']
        self.assertTrue(self.has(s,op='choose_zone_card',player=0,value=self.rows['Sol Ring']['csvid']))
        self.assertTrue(self.copies(s,'Sol Ring',1,'graveyard',2))

    def test_main_phase_branch_changes_discard_amount(self):
        main=self.cases('Haunting Hymn')[0]['steps'];outside=self.cases('Haunting Hymn','opposing_turn_0')[0]['steps']
        self.assertTrue(self.has(main,op='assert_zone',player=1,zone='hand',value=0))
        self.assertTrue(self.has(outside,op='assert_zone',player=1,zone='hand',value=2))
        main=self.cases('Careful Consideration')[0]['steps'];outside=self.cases('Careful Consideration','opposing_turn_0')[0]['steps']
        self.assertTrue(self.has(main,op='assert_zone',player=1,zone='hand',value=6))
        self.assertTrue(self.has(outside,op='assert_zone',player=1,zone='hand',value=5))

    def test_no_deaths_context_is_reset_before_the_real_cast(self):
        c=self.cases('Gruesome Discovery')[0]
        self.assertEqual(c['steps'][0],dict(op='reset_death_history'))
        self.assertFalse(any(x['op']=='destroy' for x in c['steps']))

    def test_inactive_land_attack_and_conspire_contexts_are_constructed(self):
        for n in ('Frantic Search','Pore Over the Pages','Tragic Lesson',"Mire's Malice"):
            c=self.cases(n)[0]
            self.assertFalse(any(x['zone']=='battlefield' and 'Land' in self.o[x['name']]['type_line'] for x in c['setup']))
            self.assertTrue(any('land' in gap for gap in c['_recipe']['untested_components']))
        self.assertFalse(any(x['op']=='attack' for x in self.cases('Chart a Course')[0]['steps']))
        c=self.cases('Ghastly Discovery')[0]
        self.assertFalse(any(x['zone']=='battlefield' and x['player']==0 and 'U' in self.o[x['name']]['colors'] for x in c['setup']))

    def test_named_copy_scaling_and_future_mechanics_are_gaps(self):
        for n,key in [('Mind Burst','earlier copies'),('Mind Ravel','Delayed draw'),('Nightmare Void','Dredge'),('Drill Bit','Spectacle')]:
            c=self.cases(n)[0];self.assertTrue(any(key in gap for gap in c['_recipe']['untested_components']))
        self.assertFalse(any(x['zone']=='graveyard' and x['name']=='Mind Burst' for x in self.cases('Mind Burst')[0]['setup']))

    def test_optional_resolution_inputs_are_queued_per_controller_and_replay(self):
        for pl in (0,1):
            s=self.cases('Pore Over the Pages',f'pending_snapshot_{pl}')[0]['steps']
            self.assertEqual(sum(x['op']=='choose_cancel_target' for x in s),2)
            for i,x in enumerate(s):
                if x['op']=='resolve':self.assertEqual(s[i-1],dict(op='choose_cancel_target',player=pl))
            s=self.cases('Pore Over the Pages',f'countered_cast_{pl}')[0]['steps']
            self.assertFalse(any(x['op']=='choose_cancel_target' for x in s))
        self.assertFalse(any(x['op']=='choose_dialog' for x in self.cases('Tragic Lesson')[0]['steps']))
        s=self.cases('Tragic Lesson','pending_snapshot_1')[0]['steps']
        self.assertEqual(sum(x==dict(op='choose_dialog',player=1,value=0) for x in s),2)
        self.assertFalse(any(x['op']=='choose_dialog' for x in self.cases('Tragic Lesson','countered_cast_1')[0]['steps']))


if __name__=='__main__':unittest.main()
