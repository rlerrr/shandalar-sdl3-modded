import copy
import json
import unittest
from pathlib import Path
from unittest.mock import patch

from generate_cases import registry, OPS, PROPERTIES, TOKEN_PROPERTIES, validate_library_observation
from recipes import expand, explicit_spec
import entry_value_extension_recipes as model


class EntryValueExtensionTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.oracle = json.loads((Path(__file__).parent/'data/oracle.json').read_text(encoding='utf8'))['cards']
        cls.rows = {r['name']: r for r in registry()}

    def declaration(self, name='Omenspeaker', selected=None):
        d = dict(family='entry_value_extension', version=2, params=dict(schema=1, profile=name),
                 oracle_text=self.oracle[name]['oracle_text'], variants=['effect_0'])
        d['variants'] = selected if selected is not None else model.variants(explicit_spec(d, self.oracle[name]))
        return d

    def cases(self, name='Omenspeaker', selected=None, declaration=None, oracle=None):
        return expand(name, self.rows[name]['csvid'], declaration or self.declaration(name, selected),
                      (oracle or self.oracle)[name], self.rows, oracle_context=oracle or self.oracle)

    def test_whole_source_pin_and_private_schema_fail_closed(self):
        d = self.declaration()
        for patching in (dict(oracle_text='changed'), dict(params={}), dict(params=dict(schema=True,profile='Omenspeaker')),
                         dict(params=dict(schema=1,profile='Augury Owl')), dict(params=dict(schema=1,profile='Omenspeaker',draw=2))):
            with self.assertRaises(ValueError): self.cases(declaration=dict(d, **patching))
        for field, value in [('colors',['R']),('power','9'),('mana_cost','{U}'),('type_line','Token Creature — Human'),('oracle_id','changed')]:
            r = copy.deepcopy(self.oracle); r['Omenspeaker'][field] = value
            with self.assertRaises(ValueError): self.cases(oracle=r)
        for version in (1,True,2.0,'2'):
            with self.assertRaises(ValueError):explicit_spec(dict(d,version=version),self.oracle['Omenspeaker'])
        with self.assertRaises(ValueError):model.spec(dict(d,family='other'),self.oracle['Omenspeaker'])

    def test_real_cast_result_is_used_instead_of_raw_handler_permission_query(self):
        for name in model.pins()['sources']:
            for owner in (0,1):
                for variant,allowed in [('effect',1),('insufficient_mana',0),('wrong_color',0)]:
                    steps=self.cases(name,[f'{variant}_{owner}'])[0]['steps']
                    self.assertFalse(any(s['op']=='require_can_cast' for s in steps))
                    self.assertIn(dict(op='cast',ref='source',value=allowed),steps)

    def test_every_private_witness_field_is_pinned(self):
        for field, value in [('power','7'),('oracle_id','changed'),('colors',['B']),('type_line','Token Creature — Bear')]:
            r=copy.deepcopy(self.oracle); r['Grizzly Bears'][field]=value
            with self.assertRaises(ValueError): self.cases(oracle=r)

    def test_variants_unique_both_controllers_and_fail_closed(self):
        for n in model.pins()['sources']:
            vs=self.declaration(n)['variants'];self.assertEqual(len(vs),len(set(vs)))
            self.assertEqual({v.rsplit('_',1)[1] for v in vs},{'0','1'})
        for vs in ([],['bogus_0'],['effect_0','effect_0']):
            with self.assertRaises(ValueError): self.cases(selected=vs)

    def test_public_expansion_uses_only_supported_operations_and_properties(self):
        for name in model.pins()['sources']:
            for case in self.cases(name):
                refs={x['ref'] for x in case['setup']};recorded=set()
                for s in case['steps']:
                    self.assertIn(s['op'],OPS)
                    if 'ref' in s:self.assertIn(s['ref'],refs)
                    if 'other' in s:self.assertIn(s['other'],refs)
                    if 'property' in s:self.assertIn(s['property'],PROPERTIES | (TOKEN_PROPERTIES if s['op']=='assert_matching_permanents' else set()))
                    if s['op'] in ('record_library','assert_scry_result'):validate_library_observation(s,recorded)

    def test_human_bottom_and_reorder_are_real_positive_inputs(self):
        s=self.cases('Augury Owl',['effect_0'])[0]['steps']
        self.assertEqual(sum(x==dict(op='choose_zone',player=0,value=0) for x in s),3)
        self.assertIn(dict(op='assert_zone_card',player=0,zone='library',index=0,value=self.rows['Mountain']['csvid']),s)
        s=self.cases('Omenspeaker',['reorder_0'])[0]['steps']
        self.assertIn(dict(op='choose_zone',player=0,value=1),s)
        self.assertIn(dict(op='assert_zone_card',player=0,zone='library',index=0,value=self.rows['Plains']['csvid']),s)

    def test_ai_scry_retains_policy_and_has_conservation_observation(self):
        s=self.cases('Augury Owl',['effect_1'])[0]['steps']
        self.assertFalse(any(x['op']=='choose_zone' for x in s))
        self.assertIn(dict(op='record_library',player=1),s)
        self.assertIn(dict(op='assert_scry_result',player=1,value=3),s)
        self.assertIn(dict(op='choose_optional_zone',player=1,value=self.rows['Forest']['csvid']),s)
        self.assertIn(dict(op='assert_forced_zone_choices',value=0),s)

    def test_empty_and_short_scry_have_bounded_recorded_libraries(self):
        for v,num in [('empty_library_0',0),('short_library_0',1)]:
            c=self.cases('Augury Owl',[v])[0]
            self.assertEqual(sum(x['zone']=='library' and x['player']==0 for x in c['setup']),num)
            self.assertIn(dict(op='assert_scry_result',player=0,value=3),c['steps'])

    def test_counted_life_excludes_source_and_enemy_and_freezes_receipt(self):
        for v,amount in [('effect_0',24),('no_qualifier_0',20),('enemy_only_0',20),('late_creature_0',24),('destroy_other_0',24)]:
            s=self.cases('Goldnight Redeemer',[v])[0]['steps']
            self.assertIn(dict(op='assert_life',player=0,value=amount),s)
            self.assertEqual(s[-1]['op'],'assert_zone_card')

    def test_counter_condition_uses_actual_counter_spell_and_nonretroactive_draw(self):
        for v,hand in [('effect_0',1),('enemy_counter_0',0),('counter_removed_0',0),('late_counter_0',0)]:
            c=self.cases('Chronicler of Heroes',[v])[0];s=c['steps']
            self.assertTrue(any(x['name']=='Arbor Armament' for x in c['setup']))
            last=[x for x in s if x['op']=='assert_zone_multiplicity' and x.get('zone')=='hand' and x.get('player')==0 and x.get('index')==self.rows['Forest']['csvid']][-1]
            self.assertEqual(last['value'],hand)
        self.assertIn('pending entry predicate changes',self.cases('Chronicler of Heroes',['effect_0'])[0]['_recipe']['untested_components'])

    def test_watchers_ignore_existing_and_self_but_observe_both_entry_controllers(self):
        for v in ('effect_0','enemy_entry_0','effect_1','enemy_entry_1'):
            owner=int(v[-1]);s=self.cases('Soul Warden',[v])[0]['steps']
            self.assertIn(dict(op='assert_life',player=owner,value=20),s)
            self.assertIn(dict(op='assert_life',player=owner,value=21),s)
        s=self.cases('Essence Warden',['noncreature_0'])[0]['steps']
        self.assertFalse(any(x['op']=='assert_life' and x.get('value')!=20 for x in s))

    def test_watcher_source_removed_has_positive_then_unaffected_entry(self):
        s=self.cases('Soul Warden',['source_removed_0'])[0]['steps']
        bounce=next(i for i,x in enumerate(s) if x.get('ref')=='bounce_spell' and x['op']=='cast')
        self.assertIn(dict(op='assert_life',player=0,value=21),s[:bounce])
        self.assertIn(dict(op='assert_life',player=0,value=21),s[bounce:])
        self.assertEqual(sum(x['op']=='cast' and x.get('ref') in ('event','event_second') for x in s),2)

    def test_exact_payment_receipts_precede_other_actions_for_both_owners(self):
        for name in model.pins()['sources']:
            for owner in (0,1):
                s=self.cases(name,[f'payment_{owner}'])[0]['steps']
                self.assertIn(dict(op='assert_mana_total',player=owner,value=0),s)
                for c in range(6):self.assertIn(dict(op='assert_mana',player=owner,color=c,value=0),s)

    def test_pending_inspection_excludes_spell_but_physical_hand_contains_it(self):
        for name in model.pins()['sources']:
            for owner in (0,1):
                s=self.cases(name,[f'pending_snapshot_{owner}'])[0]['steps']
                cast=next(i for i,x in enumerate(s) if x['op']=='cast' and x.get('ref')=='source')
                resolve=next(i for i in range(cast+1,len(s)) if s[i]['op']=='resolve')
                self.assertIn(dict(op='assert_card',ref='source',property='in_hand',value=0),s[cast:resolve])
                self.assertIn(dict(op='assert_zone_multiplicity',player=owner,zone='hand',index=self.rows[name]['csvid'],value=1),s[cast:resolve])
                before=[x for x in s[:cast] if x['op']=='assert_zone_multiplicity' and x.get('zone')=='hand' and x.get('player')==owner and x.get('index')==self.rows[name]['csvid']]
                self.assertEqual(before[-1]['value'],1)

    def test_snapshot_tails_preserve_both_choice_owners_and_payment_receipts(self):
        for name in model.pins()['sources']:
            for owner in (0,1):
                s=self.cases(name,[f'ready_snapshot_{owner}'])[0]['steps']
                start=next(i for i,x in enumerate(s) if x['op']=='capture')
                record=next(i for i,x in enumerate(s) if x['op']=='record_state')
                restore=next(i for i,x in enumerate(s) if x['op']=='restore')
                self.assertEqual(s[start+1:record],s[restore+1:-1])
                for a,b in zip(s[start+1:record],s[restore+1:-1]):self.assertIsNot(a,b)

    def test_fresh_cast_source_blocks_with_real_stats_and_flying_controls(self):
        for n,allowed in [('Omenspeaker',0),('Augury Owl',1),('Goldnight Redeemer',1)]:
            s=self.cases(n,['block_flying_0'])[0]['steps']
            self.assertIn(dict(op='block',ref='source',other='attacker',value=allowed),s)
            self.assertTrue(any(x['op']=='combat_damage' for x in s))
        s=self.cases('Omenspeaker',['block_ground_1'])[0]['steps']
        self.assertIn(dict(op='block',ref='source',other='attacker',value=1),s)

    def test_cleanup_lifecycle_retains_fixture_paid_pools_and_entry_receipts(self):
        for name in model.pins()['sources']:
            for owner in (0,1):
                s=self.cases(name,[f'cleanup_{owner}'])[0]['steps']
                cut=next(i for i,x in enumerate(s) if x['op']=='cleanup')
                for pl in (0,1):
                    before=[x['value'] for x in s[:cut] if x['op']=='assert_mana_total' and x['player']==pl][-1]
                    after=[x['value'] for x in s[cut:] if x['op']=='assert_mana_total' and x['player']==pl][0]
                    self.assertEqual(before,after)

    def test_new_private_witnesses_do_not_expand_existing_case_universes(self):
        a=self.cases('Omenspeaker',['effect_0'])
        data=model.pins();data['witnesses']['Elvish Mystic']=dict(csvid=self.rows['Elvish Mystic']['csvid'],oracle=model.pin(self.oracle['Elvish Mystic']))
        with patch.object(model,'pins',return_value=data):b=self.cases('Omenspeaker',['effect_0'])
        self.assertEqual(a,b)

    def test_artifact_count_includes_source_and_noncreature_but_excludes_opponent(self):
        for v,total in [('effect_0',26),('no_qualifier_0',23),('enemy_only_0',23),('two_copies_0',35),('late_creature_0',26)]:
            s=self.cases('Filigree Angel',[v])[0]['steps']
            self.assertIn(dict(op='assert_life',player=0,value=total),s)

    def test_green_creature_count_excludes_green_enchantment_and_includes_source(self):
        for v,drawn in [('effect_0',2),('no_qualifier_0',1),('two_copies_0',5)]:
            s=self.cases('Regal Force',[v])[0]['steps']
            last=[x['value'] for x in s if x['op']=='assert_zone' and x.get('zone')=='library' and x.get('player')==0][-1]
            self.assertEqual(last,len(model.BASE_LIBRARY)-drawn)
        self.assertTrue(any(e['name']=='Exploration' for e in self.cases('Regal Force',['effect_0'])[0]['setup']))

    def test_devotion_counts_own_permanent_symbols_not_opponent_or_older_zones(self):
        for v,amount in [('effect_0',6),('no_qualifier_0',2),('enemy_only_0',2),('two_copies_0',14)]:
            s=self.cases('Gray Merchant of Asphodel',[v])[0]['steps']
            self.assertIn(dict(op='assert_life',player=0,value=20+amount),s)
            self.assertIn(dict(op='assert_life',player=1,value=20-amount),s)

    def test_gate_threshold_optional_accept_decline_and_real_removed_gate(self):
        for v,drawn in [('effect_0',1),('no_qualifier_0',0),('enemy_only_0',0),('threshold_one_0',0),('condition_removed_0',0),('decline_0',0)]:
            s=self.cases('Opal Lake Gatekeepers',[v])[0]['steps']
            self.assertEqual(any(x['op']=='choose_trigger' for x in s),bool(drawn))
            last=[x['value'] for x in s if x['op']=='assert_zone_multiplicity' and x.get('zone')=='hand' and x.get('player')==0 and x.get('index')==self.rows['Forest']['csvid']][-1]
            self.assertEqual(last,drawn)
        s=self.cases('Opal Lake Gatekeepers',['condition_removed_0'])[0]['steps']
        self.assertIn(dict(op='cast',ref='gate_bounce',value=1),s)
        self.assertIn(dict(op='bind_hand',ref='gate_0',player=0,value=self.rows['Boros Guildgate']['csvid']),s)

    def test_own_only_watcher_differs_from_optional_global_watcher(self):
        for name,total in [('Healer of the Pride',20),('Auriok Champion',21)]:
            s=self.cases(name,['enemy_entry_0'])[0]['steps']
            self.assertIn(dict(op='assert_life',player=0,value=total),s)
            self.assertEqual(any(x['op']=='choose_trigger' for x in s),name=='Auriok Champion')
        s=self.cases('Auriok Champion',['decline_0'])[0]['steps']
        self.assertFalse(any(x['op']=='choose_trigger' for x in s))
        self.assertFalse(any(x['op']=='assert_life' and x['value']!=20 for x in s))

    def test_gate_mana_sources_are_tapped_in_real_payment_denial_controls(self):
        for owner in (0,1):
            for v in ('insufficient_mana','wrong_color'):
                c=self.cases('Opal Lake Gatekeepers',[f'{v}_{owner}'])[0]
                gates=[e for e in c['setup'] if e['name'] in ('Boros Guildgate','Simic Guildgate')]
                self.assertEqual(len(gates),4)
                self.assertTrue(all('tapped' in e.get('state',[]) for e in gates))
                self.assertIn(dict(op='cast',ref='source',value=0),c['steps'])

    def test_pin_numbers_require_exact_types_including_zero_and_one(self):
        for name,field,value in [('Soul Warden','cmc',True),('Omenspeaker','cmc',2),('Forest','cmc',False),('Grizzly Bears','cmc',2)]:
            o=copy.deepcopy(self.oracle);o[name][field]=value
            with self.assertRaises(ValueError):self.cases('Soul Warden' if name=='Soul Warden' else 'Omenspeaker',selected=['effect_0'],oracle=o)

    def test_self_including_creature_life_and_green_devotion_are_distinct(self):
        for name,v,total in [('Angel of Renewal','effect_0',23),('Dwarven Priest','no_qualifier_0',21),("Nylea's Disciple",'effect_0',27),("Nylea's Disciple",'no_qualifier_0',22)]:
            self.assertIn(dict(op='assert_life',player=0,value=total),self.cases(name,[v])[0]['steps'])

    def test_vampire_draw_and_life_loss_are_both_mandatory(self):
        for v,drawn in [('effect_0',2),('no_qualifier_0',1),('two_copies_0',5)]:
            s=self.cases('Champion of Dusk',[v])[0]['steps']
            self.assertIn(dict(op='assert_life',player=0,value=20-drawn),s)
            remaining=[x['value'] for x in s if x['op']=='assert_zone' and x.get('zone')=='library' and x.get('player')==0][-1]
            self.assertEqual(remaining,len(model.BASE_LIBRARY)-drawn)

    def test_linear_gate_gain_and_nongate_control(self):
        for v,total in [('one_matching_0',22),('effect_0',24),('three_matching_0',26),('nongate_land_control_0',20)]:
            self.assertIn(dict(op='assert_life',player=0,value=total),self.cases('Archway Angel',[v])[0]['steps'])
        for name in ('Opal Lake Gatekeepers','Saruli Gatekeepers'):
            s=self.cases(name,['nongate_land_control_0'])[0]['steps']
            self.assertFalse(any(x['op']=='choose_trigger' for x in s))
            self.assertFalse(any(x['op']=='assert_life' and x.get('value')!=20 for x in s))

    def test_counter_bearing_creature_count_is_not_total_counter_count(self):
        for v,drawn in [('effect_0',1),('two_counters_one_creature_0',1),('two_counted_creatures_0',2),('enemy_only_0',0)]:
            s=self.cases('Armorcraft Judge',[v])[0]['steps']
            remaining=[x['value'] for x in s if x['op']=='assert_zone' and x.get('zone')=='library' and x.get('player')==0][-1]
            self.assertEqual(remaining,len(model.BASE_LIBRARY)-drawn)
        s=self.cases('Armorcraft Judge',['two_counters_one_creature_0'])[0]['steps']
        self.assertIn(dict(op='assert_card',ref='ally',property='plus1_counters',value=2),s)

    def test_other_spirit_excludes_source_and_enchantment_condition_is_boolean(self):
        for name in ('Apothecary Geist','Lagonna-Band Elder'):
            for v,total in [('effect_0',23),('no_qualifier_0',20),('enemy_only_0',20),('two_qualifiers_0',23),('condition_removed_0',20)]:
                self.assertIn(dict(op='assert_life',player=0,value=total),self.cases(name,[v])[0]['steps'])

    def test_named_qualifiers_are_really_cast_and_not_seeded_with_lost_entry_effects(self):
        for name in ('Cleric of the Forward Order','Faerie Miscreant'):
            c=self.cases(name,['effect_0'])[0]
            self.assertTrue(all(e['zone']!='battlefield' for e in c['setup'] if e['name']==name))
            casts=[x['ref'] for x in c['steps'] if x['op']=='cast']
            self.assertEqual(casts[:2],['qualifier','source'])

    def test_named_life_samples_each_real_entry_including_source(self):
        for v,life in [('effect_0',26),('no_qualifier_0',22),('enemy_only_0',22),('qualifier_removed_0',24),('two_copies_0',32)]:
            s=self.cases('Cleric of the Forward Order',[v])[0]['steps']
            self.assertEqual([x['value'] for x in s if x['op']=='assert_life' and x['player']==0][-1],life)

    def test_named_reentry_observes_all_copies_not_only_first_name_binding(self):
        s=self.cases('Cleric of the Forward Order',['blink_0'])[0]['steps']
        self.assertIn(dict(op='assert_zone_multiplicity',player=0,zone='battlefield',index=self.rows['Cleric of the Forward Order']['csvid'],value=2),s)
        self.assertIn(dict(op='assert_matching_permanents',player=0,index=self.rows['Cleric of the Forward Order']['csvid'],property='power',value=2),s)
        self.assertIn(dict(op='bind_permanent_copy',player=0,value=self.rows['Cleric of the Forward Order']['csvid'],index=0,ref='source'),s)

    def test_named_blink_binding_is_observation_only_and_symmetric(self):
        for name in ('Cleric of the Forward Order','Faerie Miscreant'):
            for owner in (0,1):
                steps=self.cases(name,[f'blink_snapshot_{owner}'])[0]['steps']
                first=next(i for i,s in enumerate(steps) if s['op']=='bind_permanent_copy')
                source_actions=[s['op'] for s in steps[first:] if s.get('ref')=='source']
                self.assertTrue(set(source_actions)<= {'assert_card','require_card','bind_permanent_copy'})
                for prop in ('in_play','tapped','power','toughness','damage','plus1_counters'):
                    sv=[s['value'] for s in steps[first:] if s.get('ref')=='source' and s.get('property')==prop]
                    qv=[s['value'] for s in steps[first:] if s.get('ref')=='qualifier' and s.get('property')==prop]
                    self.assertTrue(sv);self.assertEqual(sv,qv)

    def test_another_named_condition_excludes_self_hand_grave_and_enemy(self):
        for v,draw in [('effect_0',1),('no_qualifier_0',0),('enemy_only_0',0),('condition_removed_0',0),('two_copies_0',2)]:
            s=self.cases('Faerie Miscreant',[v])[0]['steps']
            remaining=[x['value'] for x in s if x['op']=='assert_zone' and x.get('zone')=='library' and x.get('player')==0][-1]
            self.assertEqual(remaining,len(model.BASE_LIBRARY)-draw)

    def test_black_permanent_is_current_color_not_black_mana_symbol(self):
        for v,gain in [('effect_0',2),('noncreature_qualifier_0',2),('no_qualifier_0',0),('enemy_only_0',0),('colorless_symbol_0',0),('condition_removed_0',0)]:
            s=self.cases('Parasitic Strix',[v])[0]['steps']
            self.assertEqual([x['value'] for x in s if x['op']=='assert_life' and x['player']==0][-1],20+gain)
            self.assertEqual(any(x['op']=='choose_player' for x in s),bool(gain))
        s=self.cases('Parasitic Strix',['colorless_symbol_0'])[0]['steps']
        self.assertIn(dict(op='assert_matching_permanents',player=0,index=self.rows['Silent Skimmer']['csvid'],property='color',value=0),s)

    def test_targeted_drains_allow_self_and_test_artifact_threshold_types(self):
        for name in ('Parasitic Strix','Bleak Coven Vampires'):
            for owner in (0,1):
                s=self.cases(name,[f'self_target_{owner}'])[0]['steps']
                self.assertIn(dict(op='choose_player',player=owner,value=owner),s)
                self.assertEqual([x['value'] for x in s if x['op']=='assert_life' and x['player']==owner][-1],20)
        for v,gain in [('effect_0',4),('two_artifacts_0',0),('mixed_artifacts_0',4),('four_artifacts_0',4),('condition_removed_0',0)]:
            s=self.cases('Bleak Coven Vampires',[v])[0]['steps']
            self.assertEqual([x['value'] for x in s if x['op']=='assert_life' and x['player']==0][-1],20+gain)

    def test_incoming_toughness_watcher_counts_self_real_events_and_each_copy(self):
        for v,gain in [('effect_0',7),('no_other_owned_creatures_0',5),('enemy_entry_0',5),('nonmatching_entry_0',5),('artifact_creature_entry_0',8),('repeat_entry_0',10),('source_removed_0',7),('two_copies_0',19)]:
            s=self.cases("Verdant Sun's Avatar",[v])[0]['steps']
            self.assertEqual([x['value'] for x in s if x['op']=='assert_life' and x['player']==0][-1],20+gain)

    def test_constellation_self_and_noncreature_enchantment_entries_draw(self):
        for v,drawn in [('effect_0',2),('no_other_owned_creatures_0',1),('enemy_entry_0',1),('nonmatching_entry_0',1),('repeat_entry_0',3),('source_removed_0',2),('two_copies_0',5)]:
            s=self.cases('Eidolon of Blossoms',[v])[0]['steps']
            remaining=[x['value'] for x in s if x['op']=='assert_zone' and x.get('zone')=='library' and x.get('player')==0][-1]
            self.assertEqual(remaining,len(model.BASE_LIBRARY)-drawn)

    def test_ally_rally_includes_self_and_rejects_other_controller_or_type(self):
        for v,drain in [('effect_0',2),('no_other_owned_creatures_0',1),('enemy_entry_0',1),('nonmatching_entry_0',1),('repeat_entry_0',3),('two_copies_0',5)]:
            s=self.cases('Kalastria Healer',[v])[0]['steps']
            self.assertEqual([x['value'] for x in s if x['op']=='assert_life' and x['player']==0][-1],20+drain)
            self.assertEqual([x['value'] for x in s if x['op']=='assert_life' and x['player']==1][-1],20-drain)

    def test_zombie_watcher_excludes_self_and_stops_after_positive_source_removal(self):
        for v,drain in [('effect_0',1),('no_other_owned_creatures_0',0),('enemy_entry_0',0),('nonmatching_entry_0',0),('repeat_entry_0',2),('source_removed_0',1),('two_copies_0',3)]:
            s=self.cases('Wayward Servant',[v])[0]['steps']
            self.assertEqual([x['value'] for x in s if x['op']=='assert_life' and x['player']==0][-1],20+drain)
        s=self.cases('Wayward Servant',['source_removed_0'])[0]['steps']
        bounce=next(i for i,x in enumerate(s) if x['op']=='cast' and x.get('ref')=='bounce_spell')
        self.assertIn(dict(op='assert_life',player=0,value=21),s[:bounce])

    def test_colored_pool_generic_payment_after_two_expensive_casts_is_real(self):
        for v in ('bounce_recast','two_copies'):
            s=self.cases("Verdant Sun's Avatar",[v+'_0'])[0]['steps']
            self.assertEqual([x for x in s if x['op']=='choose_mana'],[dict(op='choose_mana',player=0,color=1)])
            payment=next(i for i,x in enumerate(s) if x['op']=='choose_mana')
            event=next(i for i,x in enumerate(s) if x['op']=='cast' and x.get('ref')=='event')
            self.assertLess(payment,event)
            self.assertFalse(any(x['op']=='choose_mana' for x in self.cases("Verdant Sun's Avatar",[v+'_1'])[0]['steps']))

    def test_land_entries_use_actual_paid_tap_ability_not_synthetic_placement(self):
        for name in ('Jaddi Offshoot','Grazing Gladehart','Shattered Angel','Tatyova, Benthic Druid'):
            s=self.cases(name,['effect_0'])[0]['steps']
            activate=next(i for i,x in enumerate(s) if x['op']=='activate' and x.get('ref')=='atlas')
            resolve=next(i for i in range(activate+1,len(s)) if s[i]['op']=='resolve')
            self.assertIn(dict(op='assert_stack',value=1),s[activate:resolve])
            self.assertIn(dict(op='assert_card',ref='atlas',property='tapped',value=1),s[activate:resolve])
            self.assertIn(dict(op='assert_card',ref='land',property='in_hand',value=1),s[activate:resolve])
            self.assertFalse(any(x['op']=='cast' and x.get('ref')=='land' for x in s))

    def test_land_controller_scope_and_nonland_controls(self):
        for name,gain,landowner in [('Jaddi Offshoot',1,0),('Grazing Gladehart',2,0),('Shattered Angel',3,1),('Tatyova, Benthic Druid',1,0)]:
            c=self.cases(name,['effect_0'])[0];s=c['steps']
            self.assertEqual(next(e['player'] for e in c['setup'] if e['ref']=='atlas'),landowner)
            self.assertEqual([x['value'] for x in s if x['op']=='assert_life' and x['player']==0][-1],20+gain)
            for mode in ('nonmatching_controller','nonland_entry'):
                s=self.cases(name,[mode+'_0'])[0]['steps']
                self.assertEqual([x['value'] for x in s if x['op']=='assert_life' and x['player']==0][-1],20)

    def test_actual_creature_land_triggers_landfall(self):
        c=self.cases('Jaddi Offshoot',['creature_land_0'])[0]
        self.assertTrue(any(e['ref']=='land' and e['name']=='Dryad Arbor' and e['zone']=='hand' for e in c['setup']))
        self.assertIn(dict(op='choose_zone_card',player=0,value=self.rows['Dryad Arbor']['csvid']),c['steps'])
        self.assertEqual([x['value'] for x in c['steps'] if x['op']=='assert_life' and x['player']==0][-1],21)

    def test_tapped_and_sick_atlas_are_observed_and_actually_denied(self):
        for owner in (0,1):
            for mode,prop in [('atlas_tapped','tapped'),('atlas_sick','sick')]:
                s=self.cases('Jaddi Offshoot',[mode+'_'+str(owner)])[0]['steps']
                self.assertIn(dict(op='assert_card',ref='atlas',property=prop,value=1),s)
                self.assertIn(dict(op='require_can_activate',ref='atlas',value=0),s)
                self.assertIn(dict(op='activate',ref='atlas',value=0),s)

    def test_land_repeat_uses_real_turns_exact_untap_and_main_before_second_action(self):
        s=self.cases('Jaddi Offshoot',['repeat_land_0'])[0]['steps']
        advances=[i for i,x in enumerate(s) if x['op']=='advance_turn']
        self.assertEqual(len(advances),2)
        for index,pl in zip(advances,(1,0)):self.assertEqual(s[index+1],dict(op='untap_step',player=pl))
        self.assertEqual(s[advances[-1]+2],dict(op='turn',player=0,value=0))
        self.assertEqual(sum(x['op']=='activate' and x.get('ref')=='atlas' for x in s),2)
        self.assertEqual([x['value'] for x in s if x['op']=='assert_life' and x['player']==0][-1],22)
        self.assertTrue(all(x['value']==0 for x in s[:advances[0]] if x['op']=='set_mana'))

    def test_same_turn_repeat_uses_real_twiddle_and_keeps_both_landfall_values(self):
        s=self.cases('Grazing Gladehart',['same_turn_repeat_0'])[0]['steps']
        self.assertIn(dict(op='cast',ref='untap_spell',value=1),s)
        self.assertIn(dict(op='choose_dialog',player=0,value=1),s)
        self.assertFalse(any(x['op']=='advance_turn' for x in s))
        self.assertEqual([x['value'] for x in s if x['op']=='assert_life' and x['player']==0][-1],24)

    def test_land_optional_policy_and_negative_offer_probe_are_distinct(self):
        accept=self.cases('Grazing Gladehart',['effect_0'])[0]['steps']
        decline=self.cases('Grazing Gladehart',['decline_0'])[0]['steps']
        negative=self.cases('Grazing Gladehart',['nonmatching_controller_0'])[0]['steps']
        self.assertIn(dict(op='choose_trigger',player=0,ref='source'),accept)
        self.assertFalse(any(x['op']=='choose_trigger' for x in decline))
        self.assertIn(dict(op='choose_trigger',player=0,ref='source'),negative)
        self.assertIn(dict(op='assert_unused_choices',value=1),negative)
        self.assertIn('decline_land_0',self.declaration('Grazing Gladehart')['variants'])
        self.assertNotIn('decline_land_1',self.declaration('Grazing Gladehart')['variants'])
        self.assertIn('decline_land_1',self.declaration('Shattered Angel')['variants'])

    def test_land_source_removed_after_positive_then_real_second_land(self):
        s=self.cases('Tatyova, Benthic Druid',['source_removed_0'])[0]['steps']
        bounce=next(i for i,x in enumerate(s) if x['op']=='cast' and x.get('ref')=='bounce_spell')
        self.assertIn(dict(op='assert_life',player=0,value=21),s[:bounce])
        self.assertEqual(sum(x['op']=='activate' and x.get('ref')=='atlas' for x in s),2)
        self.assertEqual([x['value'] for x in s if x['op']=='assert_life' and x['player']==0][-1],21)
        self.assertNotIn('two_copies_0',self.declaration('Tatyova, Benthic Druid')['variants'])

    def test_entry_max_counters_draw_samples_other_own_current_power(self):
        for v,counters,drawn in [('effect_0',3,4),('no_qualifier_0',0,1),('enemy_only_0',0,1),('boost_before_entry_0',5,6),('late_creature_0',3,4)]:
            c=self.cases('Prime Speaker Zegana',[v])[0];s=c['steps']
            self.assertIn(dict(op='assert_card',ref='source',property='plus1_counters',value=counters),s)
            self.assertIn(dict(op='assert_card',ref='source',property='power',value=1+counters),s)
            self.assertEqual([x['value'] for x in s if x['op']=='assert_zone' and x.get('zone')=='library' and x.get('player')==0][-1],21-drawn)
        s=self.cases('Prime Speaker Zegana',['boost_before_entry_0'])[0]['steps']
        self.assertIn(dict(op='cast',ref='growth_spell',value=1),s)
        self.assertIn(dict(op='assert_card',ref='ally',property='power',value=5),s)

    def test_entry_counter_palette_is_local_and_blink_recomputes_without_stacking(self):
        c=self.cases('Prime Speaker Zegana',['blink_0'])[0]
        self.assertEqual(sum(e['zone']=='library' and e['player']==0 for e in c['setup']),21)
        s=c['steps'];self.assertEqual([x['value'] for x in s if x['op']=='assert_zone' and x.get('zone')=='library' and x.get('player')==0][-1],13)
        self.assertEqual([x['value'] for x in s if x.get('ref')=='source' and x.get('property')=='plus1_counters'][-1],3)
        self.assertEqual(sum(e['zone']=='library' and e['player']==0 for e in self.cases('Omenspeaker',['effect_0'])[0]['setup']),7)
        self.assertNotIn('two_copies_0',self.declaration('Prime Speaker Zegana')['variants'])

    def test_counter_increased_power_deals_actual_simultaneous_combat_damage(self):
        s=self.cases('Prime Speaker Zegana',['block_ground_0'])[0]['steps']
        self.assertIn(dict(op='combat_damage'),s)
        self.assertIn(dict(op='assert_card',ref='source',property='damage',value=3),s)
        self.assertIn(dict(op='assert_card',ref='source',property='power',value=4),s)
        self.assertIn(dict(op='assert_zone_multiplicity',player=1,zone='graveyard',index=self.rows['Hill Giant']['csvid'],value=2),s)


    def test_global_optional_watchers_keep_controller_and_enemy_entry_receipts(self):
        for name,gain in [('Totem Speaker',3),("Soul's Attendant",1),('Leonin Elder',1)]:
            for owner in (0,1):
                s=self.cases(name,[f'enemy_entry_{owner}'])[0]['steps']
                self.assertEqual([x['value'] for x in s if x['op']=='assert_life' and x['player']==owner][-1],20+gain)
                self.assertEqual([x['value'] for x in s if x['op']=='assert_life' and x['player']==1-owner][-1],20)
        for owner in (0,1):
            s=self.cases('Wirewood Savage',[f'enemy_entry_{owner}'])[0]['steps']
            self.assertEqual([x['value'] for x in s if x['op']=='assert_zone' and x.get('zone')=='library' and x.get('player')==owner][-1],6)
            self.assertEqual([x['value'] for x in s if x['op']=='assert_zone' and x.get('zone')=='library' and x.get('player')==1-owner][-1],7)

    def test_treefolk_toughness_and_flying_power_have_distinct_self_predicates(self):
        for name,rows in [('Orchard Warden',[('effect_0',5),('repeat_entry_0',11),('no_other_owned_creatures_0',0),('enemy_entry_0',0),('nonmatching_entry_0',0)]),('Archon of Redemption',[('effect_0',7),('repeat_entry_0',9),('no_other_owned_creatures_0',3),('enemy_entry_0',3),('nonmatching_entry_0',3)])]:
            for variant,gain in rows:
                s=self.cases(name,[variant])[0]['steps']
                self.assertEqual([x['value'] for x in s if x['op']=='assert_life' and x['player']==0][-1],20+gain)
        c=self.cases('Orchard Warden',['repeat_entry_0'])[0]
        self.assertEqual([(e['name'],e['zone']) for e in c['setup'] if e['ref'] in ('event','event_second')],[('Ironroot Treefolk','hand'),('Redwood Treefolk','hand')])

    def test_optional_legal_intent_is_asserted_before_terminal_consumption(self):
        for name in ('Totem Speaker','Wirewood Savage','Orchard Warden','Archon of Redemption',"Soul's Attendant",'Leonin Elder'):
            s=self.cases(name,['effect_0'])[0]['steps']
            request=next(i for i,x in enumerate(s) if x['op']=='choose_trigger')
            resolve=next(i for i in range(request+1,len(s)) if s[i]['op']=='resolve')
            self.assertEqual(s[resolve+1],dict(op='assert_unused_choices',value=0))
            self.assertEqual(s[resolve+2],dict(op='assert_choices_consumed'))
            declined=self.cases(name,['decline_0'])[0]['steps']
            self.assertFalse(any(x['op']=='choose_trigger' for x in declined))
            self.assertEqual([x['value'] for x in declined if x['op']=='assert_life' and x['player']==0][-1],20)
            self.assertFalse(any(x['op']=='choose_trigger' for x in self.cases(name,['effect_1'])[0]['steps']))

    def test_artifact_watcher_includes_creature_and_noncreature_actual_entries(self):
        for variant,n,gain in [('effect_0','Darksteel Relic',1),('artifact_creature_entry_0','Brass Man',1),('nonmatching_entry_0','Grizzly Bears',0)]:
            c=self.cases('Leonin Elder',[variant])[0]
            self.assertEqual(next(e['name'] for e in c['setup'] if e['ref']=='event'),n)
            self.assertIn(dict(op='cast',ref='event',value=1),c['steps'])
            self.assertEqual([x['value'] for x in c['steps'] if x['op']=='assert_life' and x['player']==0][-1],20+gain)

    def test_optional_self_watcher_multiple_copies_declines_human_but_ai_accepts(self):
        for owner,gain in ((0,3),(1,9)):
            s=self.cases('Archon of Redemption',[f'two_copies_{owner}'])[0]['steps']
            self.assertEqual([x['value'] for x in s if x['op']=='assert_life' and x['player']==owner][-1],20+gain)
            self.assertEqual(sum(x['op']=='choose_trigger' for x in s),int(owner==0))


if __name__=='__main__':unittest.main()
