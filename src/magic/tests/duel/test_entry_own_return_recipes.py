"""Independent Oracle, ownership, payment and composed ETB fixture guards."""
import copy
import json
import unittest
from pathlib import Path

from generate_cases import OPS, PROPERTIES, registry
import entry_own_return_recipes as model


class EntryOwnReturnTest(unittest.TestCase):
    PILOT=('Aviary Mechanic','Stonecloaker','Khalni Gem','Marsh Crocodile','Sawtooth Loon','Sparkcaster')
    @classmethod
    def setUpClass(cls):
        cls.oracle=json.loads((Path(__file__).parent/'data/oracle.json').read_text(encoding='utf8'))['cards']
        cls.rows={r['name']:r for r in registry()}

    def declaration(self,n):
        pin=model.pins()['sources'][n]
        return dict(family=model.FAMILY,version=2,oracle_text=self.oracle[n]['oracle_text'],variants=['effect_0'],
                    params=dict(schema=1,mechanism=model.mechanism(self.oracle[n]),presentation=copy.deepcopy(pin['presentation'])))

    def case(self,n,mode='effect',owner=0):
        d=self.declaration(n);d['variants']=[f'{mode}_{owner}'];p=model.spec(d,self.oracle[n])
        return model.expand(n,self.rows[n]['csvid'],d,p,d['variants'],self.rows,self.oracle)[0]

    def test_all_six_sources_whole_clauses_both_controllers_and_valid_steps(self):
        count=0
        for n in self.PILOT:
            d=self.declaration(n);p=model.spec(d,self.oracle[n]);d['variants']=model.variants(p)
            cases=model.expand(n,self.rows[n]['csvid'],d,p,d['variants'],self.rows,self.oracle);count+=len(cases)
            self.assertEqual({int(c['_recipe']['variant'][-1]) for c in cases},{0,1})
            for c in cases:
                self.assertEqual(set(c['_recipe']['covered_clauses']),set(self.oracle[n]['oracle_text'].splitlines()))
                self.assertEqual(c['_recipe']['oracle_text'],self.oracle[n]['oracle_text'])
                for s in c['steps']:
                    self.assertIn(s['op'],OPS)
                    if 'property' in s:self.assertIn(s['property'],PROPERTIES)
        self.assertEqual(count,261)

    def test_full_source_pins_reject_missing_secondary_and_fabricated_types_costs(self):
        for n in self.PILOT:
            d=self.declaration(n)
            for k,v in [('oracle_text',self.oracle[n]['oracle_text'].splitlines()[0]),('mana_cost','{0}'),('type_line','Instant'),('layout','transform')]:
                altered=dict(self.oracle[n],**{k:v})
                if altered==self.oracle[n]:continue
                with self.assertRaises(ValueError):model.spec(dict(d,oracle_text=altered['oracle_text']),altered)
        d=self.declaration('Khalni Gem');d['params']['mechanism']['count']=True
        with self.assertRaises(ValueError):model.spec(d,self.oracle['Khalni Gem'])

    def test_nontargeted_grammar_rejects_target_substitution_and_unhandled_clauses(self):
        for n in self.PILOT:
            r=copy.deepcopy(self.oracle[n]);r['oracle_text']=r['oracle_text'].replace('return a ','return a target ').replace('return another ','return another target ').replace('return two lands','return two target lands')
            with self.assertRaises(ValueError):model.mechanism(r)
            r=dict(self.oracle[n],oracle_text=self.oracle[n]['oracle_text']+'\nDraw a card.')
            with self.assertRaises(ValueError):model.mechanism(r)

    def test_actual_source_costs_and_creature_artifact_types(self):
        expected={'Aviary Mechanic':[1,0,0,0,0,1],'Stonecloaker':[2,0,0,0,0,1],'Khalni Gem':[4,0,0,0,0,0],
                  'Marsh Crocodile':[2,1,1,0,0,0],'Sawtooth Loon':[2,0,1,0,0,1],'Sparkcaster':[2,0,0,1,1,0]}
        for n,cost in expected.items():
            self.assertEqual(model.spec(self.declaration(n),self.oracle[n])['mana'],cost)
            s=self.case(n,'payment')['steps'];cast=next(i for i,x in enumerate(s) if x['op']=='cast')
            for color,amount in enumerate(cost):self.assertIn(dict(op='set_mana',player=0,color=color,value=amount),s[:cast])
        self.assertFalse(model.spec(self.declaration('Khalni Gem'),self.oracle['Khalni Gem'])['creature'])

    def test_payment_denials_have_no_untapped_land_or_choice_workaround(self):
        for n in self.PILOT:
            modes=['insufficient_mana']+(['wrong_color'] if n!='Khalni Gem' else [])
            for mode in modes:
                for owner in (0,1):
                    c=self.case(n,mode,owner)
                    self.assertIn(dict(op='cast',ref='source',value=0),c['steps'])
                    self.assertFalse(any(s['op'].startswith('choose_') for s in c['steps']))
                    for item in c['setup']:
                        if item['player']==owner and item['zone']=='battlefield' and 'Land' in self.oracle[item['name']]['type_line']:
                            self.assertIn('tapped',item['state'])

    def test_countered_sources_have_no_unconsumed_entry_choices(self):
        for n in self.PILOT:
            for owner in (0,1):
                s=self.case(n,'countered',owner)['steps']
                announce=next(i for i,x in enumerate(s) if x['op']=='announce_spell')
                response=next(i for i,x in enumerate(s) if x['op']=='cast_response')
                resolve=next(i for i,x in enumerate(s) if x['op']=='resolve_response')
                self.assertLess(announce,response);self.assertLess(response,resolve)
                self.assertFalse(any(x['op'].startswith('choose_') for x in s))
                self.assertIn(dict(op='assert_zone_multiplicity',player=owner,zone='graveyard',index=self.rows[n]['csvid'],value=1),s[resolve:])

    def test_foreign_owned_return_goes_to_original_owner_hand(self):
        for n in self.PILOT:
            for owner in (0,1):
                c=self.case(n,'foreign_owned',owner);item=next(x for x in c['setup'] if x['ref']=='target_0')
                self.assertEqual(item['player'],owner);self.assertIn('opponent_owned',item['state'])
                name=item['name'];value=self.rows[name]['csvid'];s=c['steps']
                # Crocodile's AI discard may move the returned card again;
                # its original-owner hand+grave conservation remains exact.
                self.assertIn(dict(op='assert_hand_graveyard_total',player=1-owner,index=value,value=1),s)
                self.assertIn(dict(op='assert_zone_multiplicity',player=owner,zone='battlefield',index=value,value=0),s)

    def test_source_self_return_is_mandatory_without_other_matching_permanent(self):
        for n in ('Stonecloaker','Marsh Crocodile','Sawtooth Loon','Sparkcaster'):
            for owner in (0,1):
                c=self.case(n,'source_choice',owner);s=c['steps']
                self.assertFalse(any(x['zone']=='battlefield' and x['player']==owner for x in c['setup']))
                self.assertIn(dict(op='choose_target',player=owner,ref='source'),s)
                self.assertIn(dict(op='assert_zone_multiplicity',player=owner,zone='battlefield',index=self.rows[n]['csvid'],value=0),s)
                if n=='Stonecloaker':self.assertTrue(any(x['op']=='choose_zone_card' for x in s))

    def test_optional_decline_and_mandatory_policy_receipts(self):
        for owner in (0,1):
            s=self.case('Aviary Mechanic','decline',owner)['steps']
            self.assertIn(dict(op='choose_optional_target',player=owner,ref='target_0'),s)
            self.assertIn(dict(op='assert_forced_target_choices',value=0),s)
            for n in ('Stonecloaker','Khalni Gem','Marsh Crocodile','Sawtooth Loon','Sparkcaster'):
                s=self.case(n,'mandatory_cancel',owner)['steps'];count=2 if n=='Khalni Gem' else 1
                self.assertEqual(sum(x['op']=='choose_optional_target' for x in s),count)
                self.assertIn(dict(op='assert_forced_target_choices',value=count),s)
        s=self.case('Aviary Mechanic','whole_decline')['steps'];self.assertFalse(any(x['op'].startswith('choose_') for x in s))

    def test_another_excludes_source_and_empty_others_do_not_force_return(self):
        for owner in (0,1):
            c=self.case('Aviary Mechanic','no_other',owner);s=c['steps']
            self.assertFalse(any(x['zone']=='battlefield' and x['player']==owner for x in c['setup']))
            self.assertFalse(any(x['op'] in ('choose_target','choose_optional_target') for x in s))
            self.assertIn(dict(op='assert_card',ref='source',property='in_play',value=1),s)

    def test_type_color_and_shroud_protection_witnesses_are_independent(self):
        for n in self.PILOT:
            p=model.spec(self.declaration(n),self.oracle[n])
            self.assertFalse(model.eligible(self.oracle['Counterspell'],p))
            if p['scope']=='land':
                self.assertTrue(model.eligible(self.oracle['Forest'],p));self.assertFalse(model.eligible(self.oracle['Khalni Gem'],p));continue
            for mode in ('shroud_choice','protection_choice'):
                c=self.case(n,mode);w=next(x['name'] for x in c['setup'] if x['ref']=='target_0')
                self.assertIn('Shroud' if mode=='shroud_choice' else 'Protection',self.oracle[w]['keywords'])
                self.assertNotEqual(self.rows[w]['handler'],'card_dummy');self.assertTrue(model.eligible(self.oracle[w],p))
                self.assertIn(dict(op='choose_target',player=0,ref='target_0'),c['steps'])
        for n in ('Marsh Crocodile','Sawtooth Loon','Sparkcaster'):
            p=model.spec(self.declaration(n),self.oracle[n]);a=next(x['name'] for x in self.case(n)['setup'] if x['ref']=='target_0');b=next(x['name'] for x in self.case(n,'other_color')['setup'] if x['ref']=='target_0')
            self.assertNotEqual(self.oracle[a]['colors'],self.oracle[b]['colors']);self.assertTrue(model.eligible(self.oracle[b],p));self.assertFalse(model.eligible(self.oracle['Ornithopter'],p))

    def test_khalni_partial_distinct_lands_and_real_mana_color_activation(self):
        for owner in (0,1):
            for mode,count in [('zero_lands',0),('one_land',1),('effect',2),('three_lands',2)]:
                c=self.case('Khalni Gem',mode,owner);picks=[x for x in c['steps'] if x['op']=='choose_target']
                self.assertEqual(len(picks),count);self.assertEqual(len({x['ref'] for x in picks}),count)
            for color,value in zip('BUGRW',(1,2,3,4,5)):
                s=self.case('Khalni Gem','mana_'+color,owner)['steps']
                choice=next(i for i,x in enumerate(s) if x['op']=='choose_color')
                self.assertEqual(s[choice]['value'],value);self.assertEqual(s[choice+1],dict(op='activate',ref='source',value=1))
                self.assertIn(dict(op='assert_mana',player=owner,color=value,value=2),s[choice:])
                self.assertIn(dict(op='require_can_activate',ref='source',value=0),s[choice:])

    def test_reentry_uses_real_adept_activation_bind_and_fresh_battlefield_choices(self):
        for n in self.PILOT:
            for owner in (0,1):
                s=self.case(n,'bounce_recast',owner)['steps'];casts=[i for i,x in enumerate(s) if x['op']=='cast' and x['ref']=='source'];self.assertEqual(len(casts),2)
                act=next(i for i,x in enumerate(s) if x['op']=='activate' and x['ref']=='bouncer');bind=next(i for i,x in enumerate(s) if x['op']=='bind_hand_copy')
                self.assertLess(casts[0],act);self.assertLess(act,bind);self.assertLess(bind,casts[1])
                first=[x['ref'] for x in s[casts[0]:act] if x['op']=='choose_target' and x['ref'].startswith('target_')]
                second=[x['ref'] for x in s[casts[1]:] if x['op']=='choose_target' and x['ref'].startswith('target_')]
                self.assertTrue(set(first).isdisjoint(second))

    def test_stonecloaker_actual_graveyard_menus_and_no_empty_probe(self):
        for owner in (0,1):
            for mode,expected_player in [('own_grave',owner),('opponent_grave',1-owner)]:
                s=self.case('Stonecloaker',mode,owner)['steps'];self.assertFalse(any(x['op']=='choose_player' for x in s))
                self.assertIn(dict(op='assert_zone_multiplicity',player=expected_player,zone='exile',index=self.rows['Mountain']['csvid'],value=1),s)
            self.assertFalse(any(x['op']=='choose_zone_card' for x in self.case('Stonecloaker','empty_graves',owner)['steps']))
            self.assertIn(dict(op='choose_player',player=owner,value=1-owner),self.case('Stonecloaker','both_graves',owner)['steps'])

    def test_handflow_draw_bottom_order_and_ai_discard_conservation(self):
        for owner in (0,1):
            s=self.case('Sawtooth Loon','exact_library',owner)['steps']
            self.assertEqual(sum(x['op']=='choose_zone_card' for x in s),2)
            self.assertIn(dict(op='assert_zone',player=owner,zone='library',value=2),s)
            self.assertIn(dict(op='assert_zone_card',player=owner,zone='library',index=0,value=self.rows['Plains']['csvid']),s)
            c=self.case('Marsh Crocodile',owner=owner);s=c['steps']
            self.assertFalse(any(x['op']=='choose_zone_card' for x in s));self.assertIn(dict(op='discard_policy',value=1),s)
            self.assertIn(dict(op='assert_zone',player=1,zone='graveyard',value=3),s)
            self.assertFalse(any(x['op']=='assert_zone_multiplicity' and x['player']==1 and x['zone'] in ('hand','graveyard') for x in s))
            self.assertTrue(any(x['op']=='assert_hand_graveyard_total' and x['player']==1 for x in s))

    def test_sparkcaster_player_and_real_planeswalker_damage_precedes_gating(self):
        for owner in (0,1):
            s=self.case('Sparkcaster','self_damage',owner)['steps'];a=next(i for i,x in enumerate(s) if x['op']=='choose_player');b=next(i for i,x in enumerate(s) if x['op']=='choose_target')
            self.assertLess(a,b);self.assertEqual(s[a]['value'],owner);self.assertIn(dict(op='assert_life',player=owner,value=19),s)
            s=self.case('Sparkcaster','planeswalker_damage',owner)['steps'];self.assertIn(dict(op='cast',ref='walker',value=1),s)
            self.assertIn(dict(op='assert_counter',ref='walker',counter_kind='loyalty',value=2),s)

    def test_flash_flying_phase_and_damage_lifecycles_complete(self):
        for owner in (0,1):
            s=self.case('Stonecloaker','offturn',owner)['steps'];self.assertEqual(s[0],dict(op='turn',player=1-owner,value=0))
            for n in ('Stonecloaker','Sawtooth Loon'):
                for mode in ('flying_evasion','flying_reach'):
                    s=self.case(n,mode,owner)['steps'];untaps=[i for i,x in enumerate(s) if x['op']=='untap_step'];begin=next(i for i,x in enumerate(s) if x['op']=='begin_attackers')
                    self.assertEqual(len(untaps),2);self.assertLess(untaps[-1],begin);self.assertEqual(s[begin-1],dict(op='turn',player=owner,value=0))
                    advance=next(i for i,x in enumerate(s) if x['op']=='advance_combat');damage=next(i for i,x in enumerate(s) if x['op']=='combat_damage');end=next(i for i,x in enumerate(s) if x['op']=='end_combat')
                    self.assertLess(advance,damage);self.assertLess(damage,end)
                    blocked=next(i for i,x in enumerate(s) if x['op']=='assert_card' and x.get('property')=='blocked');self.assertLess(advance,blocked)

    def test_adjacent_two_full_oracle_sources_expand88_valid_cases(self):
        count=0
        for n in ('Doomsday Specter','Denizen of the Deep'):
            d=self.declaration(n);p=model.spec(d,self.oracle[n]);d['variants']=model.variants(p)
            cases=model.expand(n,self.rows[n]['csvid'],d,p,d['variants'],self.rows,self.oracle);count+=len(cases)
            for c in cases:
                self.assertEqual(set(c['_recipe']['covered_clauses']),set(self.oracle[n]['oracle_text'].splitlines()))
                for s in c['steps']:
                    self.assertIn(s['op'],OPS)
                    if 'property' in s:self.assertIn(s['property'],PROPERTIES)
        self.assertEqual(count,88)
        self.assertTrue(any('planeswalker attack declaration' in g for g in self.case('Doomsday Specter','combat_choice')['_recipe']['untested_components']))

    def test_adjacent_full_source_pins_reject_extra_clauses_and_wrong_type(self):
        for n in ('Doomsday Specter','Denizen of the Deep'):
            d=self.declaration(n)
            for k,v in [('mana_cost','{0}'),('type_line','Artifact'),('oracle_text',self.oracle[n]['oracle_text']+'\nDraw a card.'),('layout','flip')]:
                r=dict(self.oracle[n],**{k:v})
                with self.assertRaises(ValueError):model.spec(dict(d,oracle_text=r['oracle_text']),r)
        p=model.spec(self.declaration('Doomsday Specter'),self.oracle['Doomsday Specter']);self.assertEqual(p['mana'],[2,1,1,0,0,0]);self.assertEqual(p['secondary'],'combat_discard');self.assertTrue(p['flying'])
        p=model.spec(self.declaration('Denizen of the Deep'),self.oracle['Denizen of the Deep']);self.assertEqual(p['mana'],[6,0,2,0,0,0]);self.assertTrue(p['mass'])

    def test_specter_combat_chooser_is_source_controller_discard_owner_is_damaged_player(self):
        for owner in (0,1):
            for mode,chosen in [('combat_choice','Ornithopter'),('combat_land','Plains'),('combat_one','Forest')]:
                c=self.case('Doomsday Specter',mode,owner);s=c['steps'];value=self.rows[chosen]['csvid']
                self.assertIn(dict(op='choose_zone_card',player=owner,value=value),s)
                self.assertIn(dict(op='assert_zone_multiplicity',player=1-owner,zone='graveyard',index=value,value=1),s)
                self.assertIn(dict(op='assert_life',player=1-owner,value=18),s)
                advance=next(i for i,x in enumerate(s) if x['op']=='advance_combat');selection=next(i for i,x in enumerate(s) if x['op']=='choose_zone_card');damage=next(i for i,x in enumerate(s) if x['op']=='combat_damage')
                self.assertLess(advance,selection);self.assertLess(selection,damage)
                self.assertFalse(any(x['op']=='choose_player' for x in s))

    def test_specter_mandatory_coercion_fallback_and_empty_hand_have_distinct_receipts(self):
        for owner in (0,1):
            s=self.case('Doomsday Specter','combat_cancel',owner)['steps']
            self.assertIn(dict(op='choose_optional_zone',player=owner,value=self.rows['Scathe Zombies']['csvid']),s)
            self.assertIn(dict(op='assert_forced_zone_choices',value=1),s)
            c=self.case('Doomsday Specter','combat_empty',owner);s=c['steps']
            self.assertFalse(any(x['zone']=='hand' and x['player']==1-owner for x in c['setup']))
            self.assertFalse(any(x['op'] in ('choose_zone_card','choose_optional_zone') for x in s))
            self.assertIn(dict(op='assert_life',player=1-owner,value=18),s)

    def test_specter_blocked_combat_survives_with_two_damage_and_does_not_discard(self):
        for owner in (0,1):
            s=self.case('Doomsday Specter','flying_reach',owner)['steps']
            self.assertFalse(any(x['op'] in ('choose_zone_card','choose_optional_zone') for x in s))
            self.assertIn(dict(op='assert_card',ref='source',property='in_play',value=1),s)
            self.assertIn(dict(op='assert_card',ref='source',property='damage',value=2),s)
            self.assertIn(dict(op='assert_card',ref='blocker',property='damage',value=2),s)
            self.assertIn(dict(op='assert_life',player=1-owner,value=20),s)

    def test_specter_noncombat_boundary_uses_real_aura_cast_and_activation_after_untap(self):
        for owner in (0,1):
            s=self.case('Doomsday Specter','noncombat_source',owner)['steps']
            cast=next(i for i,x in enumerate(s) if x['op']=='cast' and x['ref']=='grant');activate=next(i for i,x in enumerate(s) if x['op']=='activate' and x['ref']=='grant')
            self.assertLess(cast,activate);self.assertEqual(sum(x['op']=='untap_step' for x in s[:activate]),2)
            self.assertIn(dict(op='choose_player',player=owner,value=1-owner),s[cast:activate])
            self.assertIn(dict(op='assert_life',player=1-owner,value=19),s[activate:])
            self.assertFalse(any(x['op'] in ('choose_zone_card','choose_optional_zone','combat_damage') for x in s))

    def test_specter_combat_snapshot_replays_actual_combat_and_named_discard(self):
        s=self.case('Doomsday Specter','combat_snapshot')['steps'];cap=next(i for i,x in enumerate(s) if x['op']=='capture');rec=next(i for i,x in enumerate(s) if x['op']=='record_state');restore=next(i for i,x in enumerate(s) if x['op']=='restore')
        self.assertEqual(s[cap+1:rec],s[restore+1:-1]);self.assertEqual(s[cap+1]['op'],'begin_attackers')
        self.assertEqual(sum(x['op']=='combat_damage' for x in s),2);self.assertEqual(sum(x['op']=='choose_zone_card' for x in s),2)

    def test_denizen_mass_other_returns_artifact_land_and_protected_creatures_without_targets(self):
        for owner in (0,1):
            for mode,names in [('effect',('Elvish Mystic','Scathe Zombies')),('artifact_creature',('Ornithopter','Scathe Zombies')),('land_creature',('Dryad Arbor','Scathe Zombies')),('protected_others',('Progenitus','Calcite Snapper'))]:
                s=self.case('Denizen of the Deep',mode,owner)['steps']
                self.assertFalse(any(x['op'].startswith('choose_') for x in s))
                for n in names:self.assertIn(dict(op='assert_zone_multiplicity',player=owner,zone='hand',index=self.rows[n]['csvid'],value=1),s)
                self.assertIn(dict(op='assert_card',ref='source',property='in_play',value=1),s)
                self.assertIn(dict(op='assert_card',ref=f'artifact_{owner}',property='in_play',value=1),s)
                self.assertIn(dict(op='assert_card',ref=f'land_{owner}',property='in_play',value=1),s)
                self.assertIn(dict(op='assert_card',ref='enemy_0',property='in_play',value=1),s)

    def test_denizen_owner_destination_and_empty_others_leave_source_and_opponents_unchanged(self):
        for owner in (0,1):
            c=self.case('Denizen of the Deep','foreign_owned',owner);s=c['steps'];friend=next(x for x in c['setup'] if x['ref']=='friend_0');self.assertIn('opponent_owned',friend['state'])
            self.assertIn(dict(op='assert_zone_multiplicity',player=1-owner,zone='hand',index=self.rows['Elvish Mystic']['csvid'],value=1),s)
            for mode in ('empty_others','opponent_only'):
                c=self.case('Denizen of the Deep',mode,owner);s=c['steps']
                self.assertFalse(any(x['ref'].startswith('friend_') for x in c['setup']))
                self.assertFalse(any(x['op'].startswith('choose_') for x in s))
                self.assertIn(dict(op='assert_card',ref='source',property='power',value=11),s)
                self.assertIn(dict(op='assert_card',ref='source',property='toughness',value=11),s)

    def test_denizen_reentry_uses_actual_returned_creature_recast_before_source_recast(self):
        for owner in (0,1):
            s=self.case('Denizen of the Deep','bounce_recast',owner)['steps'];source_casts=[i for i,x in enumerate(s) if x['op']=='cast' and x['ref']=='source'];friend_cast=next(i for i,x in enumerate(s) if x['op']=='cast' and x['ref']=='friend_0');bounce=next(i for i,x in enumerate(s) if x['op']=='cast' and x['ref']=='bounce')
            self.assertEqual(len(source_casts),2);self.assertLess(source_casts[0],friend_cast);self.assertLess(friend_cast,bounce);self.assertLess(bounce,source_casts[1])
            self.assertIn(dict(op='bind_hand_copy',player=owner,value=self.rows['Elvish Mystic']['csvid'],index=0,ref='friend_0'),s[:friend_cast])
            self.assertIn(dict(op='assert_zone_multiplicity',player=owner,zone='hand',index=self.rows['Elvish Mystic']['csvid'],value=1),s[source_casts[1]:])

    def test_snapshots_replay_same_tail_and_priority_gaps_stay_explicit(self):
        for n in self.PILOT:
            for mode in ('ready_snapshot','pending_snapshot'):
                c=self.case(n,mode);s=c['steps'];cap=next(i for i,x in enumerate(s) if x['op']=='capture');rec=next(i for i,x in enumerate(s) if x['op']=='record_state');restore=next(i for i,x in enumerate(s) if x['op']=='restore')
                self.assertEqual(s[cap+1:rec],s[restore+1:-1]);self.assertEqual(s[-1]['op'],'assert_recorded_state')
                self.assertTrue(any('trigger ordering' in g for g in c['_recipe']['untested_components']))
                self.assertTrue(any('unavailable priority' in g for g in c['_recipe']['untested_components']))


if __name__=='__main__':unittest.main()
