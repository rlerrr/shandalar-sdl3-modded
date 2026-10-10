"""Fail-closed Oracle scope and independently specified tap damage expectations."""
import copy
import json
import unittest
from pathlib import Path
from generate_cases import registry
from recipes import explicit_spec, expand
from tap_damage_creature_recipes import variants,validate_witnesses


class TapDamageCreatureRecipesTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.oracle=json.loads((Path(__file__).parent/'data/oracle.json').read_text(encoding='utf8'))['cards']
        cls.rows={r['name']:r for r in registry()}

    def declaration(self,name='Prodigal Sorcerer',selected=None,**changes):
        p=dict(keywords=[],activation_cost='{T}',damage=1,targets='any',damage_you=0,damage_self=0,controller_life_loss=0,tap_target=False)
        p.update(changes)
        return dict(family='tap_damage_creature',version=2,params=p,oracle_text=self.oracle[name]['oracle_text'],variants=selected or ['survivor'])

    def cases(self,name,d,rows=None):
        return expand(name,self.rows[name]['csvid'],d,self.oracle[name],rows or self.rows,oracle_context=self.oracle)

    def test_pin_amount_target_cost_and_side_effect_validation(self):
        d=self.declaration()
        for change in (dict(oracle_text='changed'),dict(params={}),dict(params=dict(d['params'],damage=True)),
                       dict(params=dict(d['params'],damage=2)),dict(params=dict(d['params'],targets='creature')),
                       dict(params=dict(d['params'],activation_cost='{R}, {T}')),dict(params=dict(d['params'],damage_you=1)),
                       dict(params=dict(d['params'],extra=1))):
            with self.assertRaises(ValueError):self.cases('Prodigal Sorcerer',dict(d,**change))

    def test_triggered_conditional_multiple_ability_and_nonordinary_costs_rejected(self):
        for n in ('Blood Cultist','Gelectrode','Bloodshot Trainee','Goblin Test Pilot','Cuombajj Witches','Shadow Guildmage','Endbringer'):
            with self.assertRaises(ValueError):self.cases(n,self.declaration(n))
        for cost in ('{X}, {T}','{C}, {T}','{R/G}, {T}','{T}, Sacrifice this creature'):
            with self.assertRaises(ValueError):self.cases('Prodigal Sorcerer',self.declaration(activation_cost=cost))

    def test_structured_casting_cost_and_numeric_stats(self):
        d=self.declaration();r=self.oracle['Prodigal Sorcerer']
        for change in (dict(power='*'),dict(toughness='0'),dict(layout='transform'),dict(card_faces=[{}]),dict(mana_cost='{2/U}')):
            with self.assertRaises(ValueError):explicit_spec(d,dict(r,**change))

    def test_cost_and_tap_precede_resolution_and_damage_does_not(self):
        c=self.cases('Anaba Shaman',self.declaration('Anaba Shaman',activation_cost='{R}, {T}'))[0];steps=c['steps'];cut=steps.index(dict(op='resolve'))
        self.assertIn(dict(op='assert_card',ref='source',property='tapped',value=1),steps[:cut])
        self.assertIn(dict(op='assert_card',ref='recipient',property='damage',value=0),steps[:cut])
        self.assertIn(dict(op='assert_mana_total',player=0,value=59),steps[:cut])
        self.assertIn(dict(op='assert_card',ref='recipient',property='damage',value=1),steps[cut:])

    def test_targets_cover_both_controllers_and_friendly_creatures(self):
        for v,owner,target_owner in (('friendly',0,0),('opponent_controller',1,0),('survivor',0,1)):
            c=self.cases('Prodigal Sorcerer',self.declaration(selected=[v]))[0]
            self.assertEqual(c['setup'][0]['player'],owner);self.assertEqual(c['setup'][1]['player'],target_owner)
            self.assertIn(dict(op='choose_target',player=owner,ref='recipient'),c['steps'])

    def test_lethal_damage_and_indestructibility_use_owned_graveyards(self):
        lethal,shield=self.cases('Prodigal Sorcerer',self.declaration(selected=['lethal','indestructible']))
        self.assertIn(dict(op='assert_card',ref='recipient',property='in_play',value=0),lethal['steps'])
        self.assertIn(dict(op='assert_zone_contains',player=1,zone='graveyard',value=self.rows['Muck Rats']['csvid']),lethal['steps'])
        self.assertIn(dict(op='assert_card',ref='recipient',property='in_play',value=1),shield['steps'])
        self.assertIn(dict(op='assert_card',ref='recipient',property='damage',value=1),shield['steps'])

    def test_backlash_hits_correct_controller_and_self_damage_can_kill_source(self):
        c=self.cases('Orcish Artillery',self.declaration('Orcish Artillery',damage=2,damage_you=3,selected=['opponent_controller']))[0]
        self.assertIn(dict(op='assert_life',player=1,value=17),c['steps'])
        c=self.cases('Psionic Entity',self.declaration('Psionic Entity',damage=2,damage_self=3))[0]
        self.assertIn(dict(op='assert_card',ref='source',property='in_play',value=0),c['steps'])
        self.assertIn(dict(op='assert_zone_contains',player=0,zone='graveyard',value=self.rows['Psionic Entity']['csvid']),c['steps'])

    def test_controller_life_loss_also_applies_when_source_targets_itself(self):
        c=self.cases('Rakdos Ickspitter',self.declaration('Rakdos Ickspitter',targets='creature',controller_life_loss=1,selected=['self_target']))[0]
        self.assertIn(dict(op='assert_life',player=0,value=19),c['steps'])

    def test_stun_sniper_taps_selected_target_only(self):
        c=self.cases('Stun Sniper',self.declaration('Stun Sniper',targets='creature',tap_target=True,activation_cost='{1}, {T}'))[0]
        self.assertIn(dict(op='assert_card',ref='recipient',property='tapped',value=1),c['steps'])
        self.assertIn(dict(op='assert_card',ref='unselected',property='damage',value=0),c['steps'])

    def test_flying_only_negative_has_no_flying_creature_and_self_flying_is_not_ignored(self):
        d=self.declaration('Centaur Archer',targets='flying_creature',selected=['no_flying_targets']);c=self.cases('Centaur Archer',d)[0]
        self.assertTrue(all(x['name'] in ('Centaur Archer','Scaled Wurm') for x in c['setup']))
        self.assertIn(dict(op='can_activate',ref='source',value=0),c['steps'])
        d=self.declaration('Storm Spirit',targets='creature',damage=2,keywords=['Flying'])
        p=explicit_spec(d,self.oracle['Storm Spirit'])
        # A future flying-only shooter can target itself if it flies. Such a
        # card must not receive the all-ground "no targets" board.
        self.assertNotIn('no_flying_targets',variants(dict(p,targets='flying_creature')))
        self.assertIn('no_flying_targets',variants(dict(p,targets='flying_creature',keywords=[])))

    def test_fresh_haste_uses_real_casting_and_ordinary_creature_cannot_tap(self):
        for n,keys,allowed in (('Cunning Sparkmage',['Haste'],1),('Prodigal Sorcerer',[],0)):
            c=self.cases(n,self.declaration(n,keywords=keys,selected=['fresh_activation']))[0]
            ops=[s['op'] for s in c['steps']];self.assertLess(ops.index('cast'),ops.index('can_activate'))
            self.assertIn(dict(op='can_activate',ref='source',value=allowed),c['steps'])

    def test_target_loss_fizzles_all_effects_but_source_loss_does_not(self):
        d=self.declaration('Orcish Artillery',damage=2,damage_you=3,selected=['target_bounced','source_bounced','source_destroyed'])
        for c,life,damage in zip(self.cases('Orcish Artillery',d),(20,17,17),(None,2,2)):
            self.assertIn(dict(op='assert_life',player=0,value=life),c['steps'])
            if damage:self.assertIn(dict(op='assert_card',ref='recipient',property='damage',value=damage),c['steps'])

    def test_snapshot_captures_paid_pending_activation_then_replays_after_ai_search(self):
        c=self.cases('Prodigal Sorcerer',self.declaration(selected=['pending_snapshot']))[0];ops=[s['op'] for s in c['steps']]
        self.assertLess(ops.index('activate'),ops.index('capture'));self.assertLess(ops.index('capture'),ops.index('resolve'))
        self.assertEqual(ops.count('activate'),1);self.assertEqual(ops.count('resolve'),2)
        for op in ('record_state','ai_roundtrip','restore','assert_recorded_state'):self.assertIn(op,ops)

    def test_real_untapping_allows_another_activation_and_cleanup_does_not_untap(self):
        c=self.cases('Prodigal Sorcerer',self.declaration(selected=['untap_repeat']))[0];self.assertEqual(sum(s['op']=='activate' for s in c['steps']),2)
        self.assertIn(dict(op='cast',ref='untap_spell',value=1),c['steps'])
        self.assertIn(dict(op='assert_card',ref='recipient',property='damage',value=2),c['steps'])
        c=self.cases('Prodigal Sorcerer',self.declaration(selected=['cleanup_tapped']))[0]
        self.assertIn(dict(op='cleanup'),c['steps']);self.assertEqual(c['steps'][-1],dict(op='can_activate',ref='source',value=0))

    def test_empty_zones_and_target_cancellation_do_not_silently_supply_choices(self):
        empty,cancel=self.cases('Prodigal Sorcerer',self.declaration(selected=['empty_zones','cancel_target']))
        self.assertEqual(empty['fixture'],'empty_main')
        self.assertIn(dict(op='choose_cancel_target',player=0),cancel['steps'])
        self.assertIn(dict(op='assert_mana_total',player=0,value=60),cancel['steps'])

    def test_witness_pin_and_handler_independence(self):
        oracle=copy.deepcopy(self.oracle);oracle['Air Elemental']['toughness']='3'
        with self.assertRaises(ValueError):validate_witnesses(oracle,self.rows)
        d=self.declaration();rows=copy.deepcopy(self.rows);rows['Prodigal Sorcerer']['handler']='card_dummy'
        self.assertEqual(self.cases('Prodigal Sorcerer',d),self.cases('Prodigal Sorcerer',d,rows))

    def test_no_target_probe_legally_grants_and_independently_verifies_shroud(self):
        c=self.cases('Rakdos Ickspitter',self.declaration('Rakdos Ickspitter',targets='creature',controller_life_loss=1,selected=['shroud_only_target']))[0]
        self.assertEqual([s['name'] for s in c['setup']],['Rakdos Ickspitter','Robe of Mirrors','Unsummon'])
        ops=[s['op'] for s in c['steps']];self.assertLess(ops.index('cast'),ops.index('can_cast'))
        self.assertIn(dict(op='can_cast',ref='probe_spell',value=0),c['steps'])
        self.assertIn(dict(op='can_activate',ref='source',value=0),c['steps'])
        self.assertFalse(any(s['op']=='activate' for s in c['steps']))


if __name__=='__main__':unittest.main()
