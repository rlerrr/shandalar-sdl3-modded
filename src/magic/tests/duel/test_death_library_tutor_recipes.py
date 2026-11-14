import copy,json,unittest
from pathlib import Path
from generate_cases import registry
from recipes import expand as public_expand
import death_library_tutor_recipes as family


class DeathLibraryTutorTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.oracle=json.loads((Path(__file__).parent/'data/oracle.json').read_text(encoding='utf8'))['cards']
        cls.rows={r['name']:r for r in registry()};cls.data=family.pins()

    def declaration(self,name,variants):
        return dict(family=family.FAMILY,version=2,params=dict(schema=1,**family.oracle_model(self.oracle[name])),oracle_text=self.oracle[name]['oracle_text'],variants=variants)

    def case(self,name,mode='search',player=0):
        d=self.declaration(name,[f'{mode}_{player}'])
        return public_expand(name,self.rows[name]['csvid'],d,self.oracle[name],self.rows,self.oracle)[0]

    def has(self,c,**fields):
        return any(all(s.get(k)==v for k,v in fields.items()) for s in c['steps'])

    def count(self,c,name,player,zone,value):
        return self.has(c,op='assert_zone_multiplicity',index=self.rows[name]['csvid'],player=player,zone=zone,value=value)

    def test_whole_oracle_typed_model_and_unenrolled_sources_fail_closed(self):
        for name in self.data['sources']:
            d=self.declaration(name,['search_0']);family.spec(d,self.oracle[name])
            for key,value in [('schema',2),('maximum',True),('exile_source',1),('reveal',1)]:
                bad=copy.deepcopy(d);bad['params'][key]=value
                with self.assertRaises(ValueError):family.spec(bad,self.oracle[name])
            with self.assertRaises(ValueError):family.oracle_model(dict(self.oracle[name],oracle_text=self.oracle[name]['oracle_text']+'\n{T}: Draw a card.'))
        for name in ('Bitterheart Witch','Protean Hulk','Flagstones of Trokair','Remembrance'):
            with self.assertRaises(ValueError):family.oracle_model(self.oracle[name])
        for name in ('Primal Druid','Arena Rector'):
            family.spec(self.declaration(name,['search_0']),self.oracle[name])

    def test_complete_source_and_witness_pins(self):
        family.validate_witnesses(self.oracle,self.rows)
        for name,key,value in [('Elvish Promenade','type_line','Sorcery'),('Holy Strength','oracle_text','Enchant creature'),('Taiga','type_line','Basic Land \u2014 Forest')]:
            o=copy.deepcopy(self.oracle);o[name][key]=value
            with self.assertRaises(ValueError):family.validate_witnesses(o,self.rows)
        rows=copy.deepcopy(self.rows);rows['Academy Rector']['handler']='card_dummy'
        with self.assertRaises(ValueError):family.validate_witnesses(self.oracle,rows)

    def test_every_variant_public_router_without_rules_seeding(self):
        total=0
        for name in self.data['sources']:
            d=self.declaration(name,['search_0']);p=family.spec(d,self.oracle[name]);d['variants']=family.variants(p)
            cases=public_expand(name,self.rows[name]['csvid'],d,self.oracle[name],self.rows,self.oracle);total+=len(cases)
            for c in cases:
                self.assertFalse(any(s['op'] in ('destroy','set_life','set_counter','dispatch_gfp','move_card') for s in c['steps']))
                self.assertTrue(self.has(c,op='assert_unused_choices',value=0))
                self.assertTrue(self.has(c,op='assert_choices_consumed'))
                self.assertTrue(all(self.oracle[x['name']]['layout']=='normal' for x in c['setup'] if x['zone']=='library'))
        self.assertEqual(len(self.data['sources']),8);self.assertGreater(total,500)

    def test_primal_zero_power_full_cost_death_and_controller_land(self):
        self.assertEqual(self.data['sources']['Primal Druid']['handler'],'manalinkeh_card_viridian_emissary')
        for p in (0,1):
            c=self.case('Primal Druid','search',p)
            self.assertTrue(self.has(c,op='cast',ref='source',value=1))
            self.assertTrue(self.has(c,op='assert_card',ref='source',property='power',value=0))
            self.assertTrue(self.has(c,op='assert_card',ref='source',property='toughness',value=3))
            self.assertTrue(self.has(c,op='assert_card',ref='found_0',property='tapped',value=1))
            self.assertTrue(self.count(c,'Primal Druid',p,'graveyard',1))
            self.assertTrue(self.count(c,'Forest',p,'battlefield',1))
            c=self.case('Primal Druid','source_foreign_owner',p)
            self.assertTrue(self.count(c,'Primal Druid',1-p,'graveyard',1))
            self.assertTrue(self.count(c,'Forest',p,'battlefield',1))
            self.assertTrue(self.count(c,'Forest',1-p,'battlefield',0))

    def test_arena_real_planeswalker_entry_loyalty_and_owned_corpse_condition(self):
        for p in (0,1):
            c=self.case('Arena Rector','search',p)
            self.assertTrue(self.has(c,op='choose_zone_card',player=p,value=self.rows['Chandra, Pyrogenius']['csvid']))
            self.assertTrue(self.has(c,op='bind_permanent_copy',ref='found_0',player=p,value=self.rows['Chandra, Pyrogenius']['csvid']))
            self.assertTrue(self.has(c,op='assert_counter',ref='found_0',counter_kind='loyalty',value=5))
            self.assertTrue(self.count(c,'Arena Rector',p,'exile',1))
            self.assertTrue(self.count(c,'Chandra, Pyrogenius',p,'battlefield',1))
            self.assertTrue(self.has(c,op='assert_offered_legal_card',index=self.rows['Chandra, Pyrogenius']['csvid'],value=1))
            self.assertTrue(self.has(c,op='assert_offered_legal_card',index=self.rows['Grizzly Bears']['csvid'],value=0))
            self.assertFalse(self.has(c,op='cast',ref='found_0'))
            self.assertEqual(self.has(c,op='choose_trigger',ref='source',player=p),p==0)
            c=self.case('Arena Rector','source_foreign_owner',p)
            self.assertTrue(self.count(c,'Arena Rector',1-p,'graveyard',0))
            self.assertTrue(self.count(c,'Arena Rector',1-p,'exile',1))
            self.assertTrue(self.count(c,'Chandra, Pyrogenius',p,'battlefield',1))
            c=self.case('Arena Rector','source_token',p)
            self.assertFalse(self.has(c,op='choose_zone_card'))
            self.assertFalse(self.has(c,op='assert_counter',ref='found_0'))
            self.assertTrue(self.has(c,op='assert_shuffle_count',player=p,value=0))

    def test_real_paid_parent_before_dead_source_intent_and_search(self):
        for name in ('Viridian Emissary','Wirewood Herald','Welkin Hawk','Seedguide Ash','Academy Rector'):
            for p in (0,1):
                c=self.case(name,player=p);s=c['steps']
                cast=next(i for i,x in enumerate(s) if x['op']=='cast' and x['ref']=='death_spell')
                resolve=next(i for i,x in enumerate(s[cast+1:],cast+1) if x['op']=='resolve')
                self.assertTrue(any(x['op']=='choose_zone_card' for x in s[cast:resolve]))
                self.assertEqual(any(x['op']=='choose_trigger' for x in s[cast:resolve]),p==0)
                self.assertFalse(self.has(c,op='choose_trigger_source'))
                self.assertTrue(self.count(c,'Murder',1-p,'graveyard',1))

    def test_basic_forest_snow_and_nonbasic_are_independent(self):
        for p in (0,1):
            c=self.case('Viridian Emissary','nonbasic_forest',p)
            self.assertTrue(self.has(c,op='assert_offered_legal_card',index=self.rows['Taiga']['csvid'],value=0))
            c=self.case('Seedguide Ash','nonbasic_forest',p)
            self.assertTrue(self.has(c,op='choose_zone_card',player=p,value=self.rows['Taiga']['csvid']))
            self.assertTrue(self.count(c,'Taiga',p,'battlefield',1))
            c=self.case('Viridian Emissary','snow_basic',p)
            self.assertTrue(self.has(c,op='choose_zone_card',player=p,value=self.rows['Snow-Covered Forest']['csvid']))
            self.assertTrue(self.has(c,op='assert_card',ref='found_0',property='tapped',value=1))

    def test_elf_card_includes_kindred_spell_and_changeling_without_illegal_input(self):
        for p in (0,1):
            c=self.case('Wirewood Herald','kindred_filter',p)
            self.assertIn('Kindred Sorcery',self.oracle['Elvish Promenade']['type_line'])
            self.assertTrue(self.has(c,op='assert_offered_legal_card',index=self.rows['Elvish Promenade']['csvid'],value=1))
            self.assertFalse(self.has(c,op='choose_zone_card',value=self.rows['Elvish Promenade']['csvid']))
            self.assertTrue(self.count(c,'Elvish Mystic',p,'hand',1))
            self.assertTrue(self.has(c,op='assert_offered_legal_card',index=self.rows['Grizzly Bears']['csvid'],value=0))
            c=self.case('Wirewood Herald','changeling_filter',p)
            self.assertTrue(self.has(c,op='assert_offered_legal_card',index=self.rows['Ghostly Changeling']['csvid'],value=1))
            self.assertFalse(self.has(c,op='choose_zone_card',value=self.rows['Ghostly Changeling']['csvid']))

    def test_each_player_search_pass_and_shuffles_follow_actual_participation(self):
        for p in (0,1):
            for mode in ('search','decline_controller','decline_opponent','decline_both'):
                c=self.case('Veteran Explorer',mode,p)
                dialogs=[s for s in c['steps'] if s['op']=='choose_dialog']
                self.assertEqual([s['player'] for s in dialogs],[0,1])
                for who in (0,1):
                    declines=mode=='decline_both' or mode=='decline_controller' and who==p or mode=='decline_opponent' and who!=p
                    self.assertEqual(dialogs[who]['value'],int(declines))
                    self.assertTrue(self.has(c,op='assert_shuffle_count',player=who,value=0 if declines else 1))
                    self.assertTrue(self.count(c,'Forest',who,'battlefield',0 if declines else 1))
            c=self.case('Veteran Explorer','search',p)
            self.assertFalse(self.has(c,op='choose_trigger'))

    def test_accepted_zero_empty_no_match_shuffle_and_decline_do_not_conflate(self):
        for name in ('Viridian Emissary','Seedguide Ash','Wirewood Herald','Academy Rector'):
            for mode in ('accepted_zero','empty_library','no_match'):
                c=self.case(name,mode)
                self.assertTrue(self.has(c,op='assert_shuffle_count',player=0,value=1))
                self.assertTrue(self.has(c,op='choose_trigger',ref='source',player=0))
            c=self.case(name,'decline')
            self.assertFalse(self.has(c,op='choose_trigger'))
            self.assertTrue(self.has(c,op='assert_shuffle_count',player=0,value=0))
        c=self.case('Seedguide Ash','one_selected')
        self.assertEqual(sum(s['op']=='choose_zone_card' for s in c['steps']),1)
        self.assertEqual(sum(s['op']=='choose_optional_zone' for s in c['steps']),1)

    def test_rector_exiles_owned_corpse_but_controller_searches_and_tokens_cannot_pay(self):
        for p in (0,1):
            c=self.case('Academy Rector','source_foreign_owner',p)
            self.assertTrue(self.count(c,'Academy Rector',1-p,'graveyard',0))
            self.assertTrue(self.count(c,'Academy Rector',1-p,'exile',1))
            self.assertTrue(self.count(c,'Exploration',p,'battlefield',1))
            self.assertTrue(self.has(c,op='assert_shuffle_count',player=p,value=1))
            c=self.case('Academy Rector','source_token',p)
            self.assertTrue(self.count(c,'Academy Rector',p,'graveyard',0))
            self.assertTrue(self.count(c,'Academy Rector',p,'exile',0))
            self.assertFalse(self.has(c,op='choose_zone_card'))
            self.assertTrue(self.has(c,op='assert_shuffle_count',player=p,value=0))

    def test_real_aura_entry_shroud_no_cast_target_event_and_attachment_destination(self):
        for p in (0,1):
            for mode,power,toughness in [('aura_entry',3,4),('aura_enemy',3,4),('aura_shroud',7,3),('aura_not_target',2,5)]:
                c=self.case('Academy Rector',mode,p)
                self.assertTrue(self.has(c,op='choose_zone_card',value=self.rows['Holy Strength']['csvid']))
                self.assertTrue(self.has(c,op='choose_target',ref='receiver',player=p))
                self.assertFalse(self.has(c,op='cast',ref='found_0'))
                self.assertTrue(self.has(c,op='assert_card',ref='receiver',property='power',value=power))
                self.assertTrue(self.has(c,op='assert_card',ref='receiver',property='toughness',value=toughness))
            c=self.case('Academy Rector','aura_departure',p)
            self.assertTrue(self.count(c,'Holy Strength',p,'graveyard',1))
            self.assertTrue(self.count(c,'Grizzly Bears',p,'graveyard',2))
            self.assertTrue(self.has(c,op='cast',ref='receiver_death',value=1))

    def test_binding_duplicates_and_recast_are_real_new_objects(self):
        c=self.case('Seedguide Ash','duplicates')
        found=[s for s in c['steps'] if s['op']=='bind_permanent_copy']
        self.assertEqual([s['index'] for s in found],[0,1,2])
        self.assertFalse(any(x['ref'].startswith('found_') for x in c['setup']))
        for name in ('Wirewood Herald','Welkin Hawk'):
            c=self.case(name,'recast_found')
            self.assertTrue(self.has(c,op='bind_hand_copy',ref='recast',player=0,index=0))
            self.assertTrue(self.has(c,op='cast',ref='recast',value=1))
            self.assertTrue(self.count(c,name,0,'graveyard',1))

    def test_sacrifice_mana_cost_and_real_damage_deaths_have_faithful_inputs(self):
        for name in self.data['sources']:
            c=self.case(name,'sacrifice');s=c['steps']
            activate=next(i for i,x in enumerate(s) if x['op']=='activate' and x['ref']=='altar')
            self.assertTrue(any(x['op']=='choose_target' and x['ref']=='source' for x in s[:activate]))
            self.assertTrue(any(x['op']=='choose_trigger' for x in s[:activate]) or name=='Veteran Explorer')
            self.assertTrue(self.has(c,op='assert_mana',player=0,color=0,value=2))
            c=self.case(name,'lethal_death')
            self.assertTrue(any(x['ref']=='death_spell' and x['name'] in ('Lightning Bolt','Lightning Blast') for x in c['setup']))
            c=self.case(name,'zero_toughness')
            self.assertTrue(any(x['ref']=='death_spell' and x['name'] in ('Disfigure','Grasp of Darkness') for x in c['setup']))

    def test_shuffled_draw_and_real_land_untap_mana(self):
        for name in self.data['sources']:
            c=self.case(name,'post_search_draw')
            self.assertTrue(self.count(c,'Lightning Bolt',0,'hand',2))
            self.assertTrue(self.has(c,op='cast',ref='draw',value=1))
            self.assertFalse(any(s['op']=='assert_zone_card' and s['player']==0 and s['zone']=='library' for s in c['steps']))
        c=self.case('Viridian Emissary','mana_fetched');s=c['steps']
        self.assertTrue(self.has(c,op='advance_turn'));self.assertTrue(self.has(c,op='untap_step',player=0))
        self.assertTrue(self.has(c,op='assert_mana',player=0,color=3,value=1))
        at=next(i for i,x in enumerate(s) if x['op']=='activate' and x['ref']=='found_0')
        self.assertTrue(any(x['op']=='untap_step' for x in s[:at]))

    def test_actual_flying_and_reach_combat_drive_the_full_death_clause(self):
        for p in (0,1):
            c=self.case('Welkin Hawk','flying_combat',p)
            self.assertTrue(self.has(c,op='advance_turn'))
            self.assertTrue(self.has(c,op='block',ref='blocker',value=0))
            self.assertTrue(self.has(c,op='assert_life',player=1-p,value=19))
            self.assertTrue(self.count(c,'Welkin Hawk',p,'graveyard',1))
            self.assertTrue(self.count(c,'Welkin Hawk',p,'hand',1))
            c=self.case('Welkin Hawk','reach_combat',p)
            self.assertTrue(self.has(c,op='block',ref='blocker',value=1))
            self.assertFalse(any(x['ref']=='death_spell' for x in c['setup']))
            self.assertTrue(self.has(c,op='combat_damage'))
            self.assertTrue(self.count(c,'Welkin Hawk',p,'graveyard',1))
            self.assertTrue(self.count(c,'Welkin Hawk',p,'hand',1))

    def test_nine_corrected_fixtures_use_real_legal_and_offered_choices(self):
        for p in (0,1):
            c=self.case('Welkin Hawk','combat_death',p)
            self.assertTrue(any(x['ref']=='blocker' and x['name']=='Giant Spider' for x in c['setup']))
            self.assertTrue(self.has(c,op='block',ref='blocker',value=1))
            self.assertTrue(self.count(c,'Welkin Hawk',p,'graveyard',1))
            self.assertTrue(self.count(c,'Welkin Hawk',p,'hand',1))
            c=self.case('Veteran Explorer','mana_fetched',p)
            activation=next(s for s in c['steps'] if s['op']=='activate')
            self.assertEqual(activation['ref'],'found_0' if p==0 else 'found_2')
            self.assertTrue(self.has(c,op='assert_mana',player=p,color=3,value=1))
            self.assertTrue(self.has(c,op='assert_mana',player=1-p,color=3,value=0))
        for name in self.data['sources']:
            for p in (0,1):
                c=self.case(name,'sacrifice',p);s=c['steps']
                activate=next(i for i,x in enumerate(s) if x['op']=='activate' and x['ref']=='altar')
                sacrifice_choices=[x for x in s[:activate] if x['op']=='choose_target' and x['ref']=='source']
                self.assertEqual(len(sacrifice_choices),1 if p==0 else 0)
                board_creatures=[x for x in c['setup'] if x['zone']=='battlefield' and 'Creature' in self.oracle[x['name']]['type_line']]
                self.assertEqual(board_creatures,[])
                self.assertTrue(self.has(c,op='assert_mana',player=p,color=0,value=2))
    def test_failed_cast_counter_bounce_exile_and_other_death_never_search(self):
        for name in self.data['sources']:
            for mode in ('insufficient_mana','wrong_color','countered','source_bounced','source_exiled','other_dies'):
                c=self.case(name,mode)
                self.assertTrue(self.has(c,op='assert_shuffle_count',player=0,value=0))
                self.assertFalse(self.has(c,op='choose_zone_card'))
                self.assertFalse(self.has(c,op='choose_trigger'))
            c=self.case(name,'countered');s=c['steps']
            self.assertEqual([x['op'] for x in s if x['op'] in ('cast','announce_spell','cast_response','resolve_response')],
                             ['cast','announce_spell','cast_response','resolve_response'])
            self.assertTrue(self.count(c,name,0,'graveyard',1))
            if name=='Academy Rector':self.assertTrue(self.count(c,name,0,'exile',0))

    def test_snapshots_and_seeded_zone_refs_remain_distinct(self):
        for name in self.data['sources']:
            for mode in ('ready_snapshot','pending_snapshot','death_snapshot','resolved_snapshot'):
                c=self.case(name,mode)
                self.assertTrue(self.has(c,op='capture'));self.assertTrue(self.has(c,op='restore'));self.assertTrue(self.has(c,op='assert_recorded_state'))
                refs={x['ref'] for x in c['setup'] if x['zone'] in ('library','graveyard','exile')}
                self.assertFalse(any(s['op'] in ('assert_card','require_card') and s['ref'] in refs for s in c['steps']))


if __name__=='__main__':unittest.main()
