import copy
import json
from pathlib import Path
import unittest

from generate_cases import registry, validate_upkeep_step
from upkeep_permanent_count_recipes import FAMILY, _CountWorld, classify, expand, spec, validate_witnesses, variants

NAMES = ['Honden of Cleansing Fire', 'Honden of Infinite Rage', "Honden of Life's Web", "Honden of Night's Reach", 'Honden of Seeing Winds', 'Graveborn Muse', 'Ancient Runes', 'Touch of the Eternal']


class UpkeepPermanentCountRecipesTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.o = json.loads((Path(__file__).parent/'data/oracle.json').read_text(encoding='utf8'))['cards']
        cls.rows = {r['name']: r for r in registry()}

    def declaration(self, nm):
        return dict(family=FAMILY, version=2, oracle_text=self.o[nm]['oracle_text'], params=classify(self.o[nm]), variants=['effect_0'])

    def case(self, nm, mode='effect', owner=0):
        d = self.declaration(nm)
        return expand(nm, self.rows[nm]['csvid'], d, spec(d, self.o[nm]), [f'{mode}_{owner}'], self.rows, self.o)[0]

    def last(self, c):
        s = c['steps']; return s[max(i for i, x in enumerate(s) if x['op']=='upkeep_step'):]

    def life(self, c, player, value):
        self.assertIn(dict(op='assert_life', player=player, value=value), self.last(c))

    def test_complete_oracle_grammar_and_fail_closed_scope(self):
        for nm in NAMES:
            d = self.declaration(nm); self.assertTrue(spec(d, self.o[nm]))
            with self.assertRaises(ValueError): classify(dict(self.o[nm], oracle_text=self.o[nm]['oracle_text']+'\nDraw a card.'))
        r = dict(self.o['Honden of Cleansing Fire'], name='A Different Shrine')
        self.assertEqual(classify(r)['selector'], 'shrine')
        for nm in ('Endless Ranks of the Dead', 'Kemba, Kha Regent', 'Power Surge', 'Fettergeist', 'Junkyo Bell', 'Phyrexian Arena'):
            with self.assertRaises(ValueError): classify(self.o[nm])
        for changes in (dict(layout='transform'), dict(card_faces=[{}]), dict(selected_face={}), dict(type_line='Token Enchantment — Shrine')):
            # An empty selected-face object is also an invalid whole record.
            changes = dict(changes)
            if 'selected_face' in changes: changes['selected_face'] = {'name':'front'}
            with self.assertRaises(ValueError): classify(dict(self.o[NAMES[0]], **changes))

    def test_exact_json_types_and_independent_full_source_pins(self):
        d = self.declaration(NAMES[0]); r = self.o[NAMES[0]]
        for key, value in [('version',2.0), ('version',True), ('family','upkeep_value_trigger'), ('oracle_text','gain 2 life')]:
            with self.assertRaises(ValueError): spec(dict(d, **{key:value}), r)
        for key, value in [('multiplier',2.0), ('multiplier',True), ('gate',[]), ('selector',False), ('keywords',()), ('extra',1)]:
            bad = copy.deepcopy(d); bad['params'][key] = value
            with self.assertRaises(ValueError): spec(bad, r)
        with self.assertRaises(ValueError): spec(d, dict(r, colors=['U']))

    def test_witness_pins_binding_and_token_variant_are_independent(self):
        validate_witnesses(self.o, self.rows)
        bad = copy.deepcopy(self.o); bad['Spirit']['colors'] = []
        with self.assertRaises(ValueError): validate_witnesses(bad, self.rows)
        bad = copy.deepcopy(self.rows); bad['Memnite']['handler'] = 'NULL'
        with self.assertRaises(ValueError): validate_witnesses(self.o, bad)
        self.assertEqual(self.o['Spirit']['keywords'], ['Flying'])
        self.assertEqual(classify(self.o["Honden of Life's Web"])['effect'], 'spirit')

    def test_released_follow2_use_same_each_upkeep_damage_grammar(self):
        for nm, selector in [('Karma','swamp'), ('Primal Order','nonbasic')]:
            self.assertEqual(classify(self.o[nm])['selector'], selector)
            p=spec(self.declaration(nm),self.o[nm]);self.assertEqual(p['gate'],'each');self.assertEqual(p['effect'],'damage_active')
            self.assertEqual(p['ui'],dict(status='calibrated',trigger_accept_players=[],target_choice_players=[]))

    def test_selector_truth_not_creature_changeling_as_shrine(self):
        nm = NAMES[0]; d = self.declaration(nm); w = _CountWorld(nm, spec(d,self.o[nm]),0,self.rows,self.o)
        w.add(nm,'shrine',0); w.add('Woodland Changeling','changeling',0); w.add('Memnite','artifact_creature',0); w.add('Forest','land',0)
        w.add(nm,'opposing',1); w.add(nm,'grave',0,'graveyard')
        self.assertEqual(w.count('shrine',0),1); self.assertEqual(w.count('zombie',0),1)
        self.assertEqual(w.count('artifact',0),1); self.assertEqual(w.count('permanent',0),4)

    def test_all382_use_real_phase_provenance_and_no_synthetic_effects(self):
        total = 0
        for nm in NAMES:
            d=self.declaration(nm); p=spec(d,self.o[nm]); cases=expand(nm,self.rows[nm]['csvid'],d,p,variants(p),self.rows,self.o); total+=len(cases)
            self.assertEqual(len({c['name'] for c in cases}),len(cases))
            for c in cases:
                self.assertFalse(any(x.get('state') for x in c['setup']))
                ready=saved=None
                for s in c['steps']:
                    self.assertNotIn(s['op'],('set_life','set_power','set_counter','draw'))
                    if s['op']=='advance_turn': ready=None
                    elif s['op']=='untap_step': ready=s['player']
                    elif s['op']=='capture': saved=ready
                    elif s['op']=='restore': ready=saved
                    elif s['op']=='upkeep_step':
                        validate_upkeep_step(s); self.assertEqual(ready,s['player']); self.assertEqual(set(s),{'op','player'}); ready=None
                    elif s['op'] in ('turn','cast','activate','cleanup'): ready=None
                    if s['op']=='assert_life': self.assertGreater(s['value'],0)
        self.assertEqual(total,382)

    def test_three_real_shrines_account_all_callbacks_and_library(self):
        for owner in (0,1):
            c=self.case('Honden of Cleansing Fire','count_three',owner); s=self.last(c)
            self.life(c,owner,26); self.life(c,1-owner,17)
            self.assertIn(dict(op='assert_zone',player=owner,zone='library',value=21),s)
            self.assertEqual(sum(x['op']=='cast' and x['value']==1 for x in c['steps']),3)
            self.assertEqual(sum(x['op']=='upkeep_step' for x in c['steps']),2)

    def test_opposing_shrine_gets_its_own_actual_upkeep_but_not_own_count(self):
        for owner in (0,1):
            c=self.case('Honden of Cleansing Fire','opposing_count',owner)
            self.life(c,owner,22); self.life(c,1-owner,22)
            self.assertEqual([x['player'] for x in c['steps'] if x['op']=='upkeep_step'],[1-owner,owner])
            c=self.case('Honden of Cleansing Fire','nonmatching',owner); self.life(c,owner,22)

    def test_damage_prevention_is_distinct_from_muse_lifeloss(self):
        for owner in (0,1):
            c=self.case('Graveborn Muse','armor',owner); self.life(c,owner,19)
            self.assertIn(dict(op='assert_zone',player=owner,zone='library',value=23),self.last(c))
            c=self.case('Ancient Runes','armor',owner); self.life(c,owner,20); self.life(c,1-owner,20)
            c=self.case('Honden of Infinite Rage','armor',owner); self.life(c,owner,20)
            self.assertIn(dict(op='cast',ref='armor',value=1),c['steps'])

    def test_artifact_counts_zero_two_three_and_active_controller(self):
        for owner in (0,1):
            self.life(self.case('Ancient Runes',owner=owner),owner,20)
            self.life(self.case('Ancient Runes','count_two',owner),owner,18)
            self.life(self.case('Ancient Runes','count_three',owner),owner,17)
            c=self.case('Ancient Runes','opposing_count',owner); self.life(c,owner,20); self.life(c,1-owner,19)

    def test_muse_count_is_self_plus_native_zombies_and_exact_draw(self):
        for owner in (0,1):
            c=self.case('Graveborn Muse','count_three',owner); self.life(c,owner,17)
            self.assertIn(dict(op='assert_zone',player=owner,zone='library',value=21),self.last(c))
            self.life(self.case('Graveborn Muse','opposing_count',owner),owner,19)

    def test_life_total_becomes_count_from_above_below_and_tokens(self):
        for owner in (0,1):
            for mode,value in [('effect',1),('count_three',3),('count_high',22),('tokens_count',3),('armor',2),('nonmatching',3)]:
                self.life(self.case('Touch of the Eternal',mode,owner),owner,value)

    def test_draw_is_exact_prefix_and_can_empty_without_draw_step(self):
        for owner in (0,1):
            c=self.case('Honden of Seeing Winds','exact_library',owner)
            self.assertIn(dict(op='assert_zone',player=owner,zone='library',value=0),self.last(c))
            self.assertIn(dict(op='assert_zone_multiplicity',player=owner,zone='hand',index=self.rows['Forest']['csvid'],value=5),self.last(c))

    def test_discard_short_empty_and_count_three_retains_oracle_destinations(self):
        for owner in (0,1):
            for mode,count in [('effect',1),('short_hand',1),('exact_hand',3),('empty_hand',0),('count_three',3)]:
                c=self.case("Honden of Night's Reach",mode,owner)
                self.assertIn(dict(op='assert_zone_multiplicity',player=1-owner,zone='graveyard',index=self.rows['Forest']['csvid'],value=count),self.last(c))
            self.assertFalse(any(x['op']=='choose_player' for x in self.case("Honden of Night's Reach",owner=owner)['steps']))

    def test_removal_and_late_entry_preserve_callback_history(self):
        for owner in (0,1):
            for mode,life in [('source_absent',20),('source_in_hand',20),('source_enters_after',22),('source_destroyed_after',22),('count_added_after',26),('count_removed',22),('repeat',24)]:
                c=self.case('Honden of Cleansing Fire',mode,owner); self.life(c,owner,life)
                if mode in ('source_absent','source_destroyed_after'):
                    s=c['steps']; cut=max(i for i,x in enumerate(s) if x['op']=='destroy' and x['ref']=='source')
                    self.assertFalse(any(x['op']=='assert_card' and x['ref']=='source' for x in s[cut+1:]))

    def test_spirit_true_characteristics_nonattacking_entry_and_real_combat(self):
        for owner in (0,1):
            c=self.case("Honden of Life's Web",owner=owner); s=self.last(c); tid=self.rows['Spirit']['csvid']
            for prop,val in [('token',1),('artifact',0),('color',0),('subtype_Spirit',1)]: self.assertIn(dict(op='assert_matching_permanents',player=owner,index=tid,property=prop,value=val),s)
            for prop,val in [('power',1),('toughness',1),('sick',1),('attacking',0)]: self.assertIn(dict(op='assert_card',ref='token_0',property=prop,value=val),s)
            c=self.case("Honden of Life's Web",'token_ground_block',owner)
            self.assertIn(dict(op='block',ref='blocker',other='token_0',value=1),c['steps'])
            c=self.case("Honden of Life's Web",'token_defend_flying',owner)
            self.assertIn(dict(op='block',ref='token_0',other='flying',value=0),c['steps']); self.life(c,owner,16)

    def test_token_destinations_cease_and_exile_lifegain_is_retained(self):
        for owner in (0,1):
            for mode in ('token_destroy','token_bounce','token_exile'):
                c=self.case("Honden of Life's Web",mode,owner); s=self.last(c)
                for zone in ('hand','graveyard','exile','battlefield'):
                    self.assertIn(dict(op='assert_zone_multiplicity',player=owner,zone=zone,index=self.rows['Spirit']['csvid'],value=0),s)
                if mode=='token_exile': self.life(c,owner,21)

    def test_target_ledger_mandatory_fallback_and_real_paid_walker(self):
        for owner in (0,1):
            c=self.case('Honden of Infinite Rage','target_own_player',owner); self.life(c,owner,19)
            for mode in ('target_creature','target_own_creature'):
                c=self.case('Honden of Infinite Rage',mode,owner)
                self.assertIn(dict(op='choose_target',player=owner,ref='recipient'),c['steps'])
                self.assertIn(dict(op='assert_card',ref='recipient',property='damage',value=1),self.last(c))
            c=self.case('Honden of Infinite Rage','mandatory_cancel',owner)
            self.assertIn(dict(op='assert_forced_target_choices',value=1),c['steps'])
            c=self.case('Honden of Infinite Rage','target_planeswalker',owner)
            self.assertIn(dict(op='cast',ref='walker',value=1),c['steps'])
            self.assertIn(dict(op='assert_counter',ref='walker',counter_kind='loyalty',value=1),self.last(c))

    def test_snapshots_and_real_mana_cancellation(self):
        for mode in ('ready_snapshot','untap_snapshot','resolved_snapshot'):
            c=self.case('Honden of Seeing Winds',mode); s=c['steps']; self.assertEqual(sum(x['op']=='capture' for x in s),1)
            self.assertIn(dict(op='ai_roundtrip'),s); self.assertIn(dict(op='assert_recorded_state'),s)
            if mode=='untap_snapshot':
                cut=s.index(dict(op='capture'))
                self.assertEqual(next(x for x in reversed(s[:cut]) if x['op'] in ('untap_step','upkeep_step','turn')),dict(op='untap_step',player=0))
        c=self.case('Touch of the Eternal','cast_cancel');s=c['steps']
        self.assertEqual(sum(x['ref'].startswith('payment_land_') for x in c['setup']),7)
        i=s.index(dict(op='choose_cancel_mana',player=0)); self.assertEqual(s[i+1],dict(op='cast',ref='source',value=0)); self.assertEqual(s[i+2],dict(op='assert_unused_choices',value=0))
        self.assertFalse(any(x['op']=='upkeep_step' for x in s))

    def test_zero_cost_count_witness_is_really_cast_with_zero_pool(self):
        c=self.case('Ancient Runes','count_three');s=c['steps']
        for ref in ('count_0','count_1','count_2'):
            i=s.index(dict(op='cast',ref=ref,value=1))
            self.assertEqual(s[i+1],dict(op='assert_stack',value=1))
            self.assertEqual(s[i+2],dict(op='resolve'))
            for color in range(6):self.assertIn(dict(op='set_mana',player=0,color=color,value=0),s[i-6:i])

    def test_follow2_basic_snow_dual_and_nonbasic_oracle_filters(self):
        for owner in (0,1):
            for nm,expected in [('Karma',dict(basic_snow=18,dual_swamps=18,mixed_lands=16,snow_nonbasic=19)),('Primal Order',dict(basic_snow=20,dual_swamps=18,mixed_lands=17,snow_nonbasic=19))]:
                for mode,life in expected.items():
                    c=self.case(nm,mode,owner);self.life(c,owner,life);self.life(c,1-owner,20)
                    self.assertFalse(any(s['op']=='activate' for s in c['steps']))
            c=self.case('Karma','mixed_lands',owner)
            self.assertEqual([x['name'] for x in c['setup'] if x['ref'].startswith(f'land_{owner}_')],['Swamp','Snow-Covered Swamp','Badlands','Bayou','Wastes','Snow-Covered Forest','Mouth of Ronom'])

    def test_follow2_controller_count_zeros_other_zones_and_prevention(self):
        for owner in (0,1):
            for nm in ('Karma','Primal Order'):
                for mode in ('zero_count','nonmatching','count_removed','source_absent','source_in_hand'):
                    c=self.case(nm,mode,owner);self.life(c,owner,20);self.life(c,1-owner,20)
                c=self.case(nm,'controller_split',owner);self.life(c,owner,18);self.life(c,1-owner,17)
                c=self.case(nm,'opposing_count',owner);self.life(c,owner,20);self.life(c,1-owner,18)
                c=self.case(nm,'armor_three',owner);self.life(c,owner,18)
                self.assertIn(dict(op='cast',ref='armor',value=1),c['steps'])
                c=self.case(nm,'count_three',owner);self.life(c,owner,17)
                c=self.case(nm,'repeat',owner);self.life(c,owner,18)
                self.assertIn(dict(op='assert_zone_multiplicity',player=owner,zone='hand',index=self.rows['Badlands']['csvid'],value=1),self.last(self.case(nm,'zero_count',owner)))

    def test_all110_followons_real_turn_boundaries_and_no_fake_land_casts(self):
        total=0
        for nm in ('Karma','Primal Order'):
            d=self.declaration(nm);p=spec(d,self.o[nm]);cs=expand(nm,self.rows[nm]['csvid'],d,p,variants(p),self.rows,self.o);total+=len(cs)
            self.assertNotIn('count_added_after_0',variants(p))
            for c in cs:
                ready=saved=None
                for s in c['steps']:
                    if s['op']=='cast':self.assertIn(s['ref'],('source','armor'))
                    self.assertNotIn(s['op'],('set_life','draw','activate'))
                    if s['op']=='advance_turn':ready=None
                    elif s['op']=='untap_step':ready=s['player']
                    elif s['op']=='capture':saved=ready
                    elif s['op']=='restore':ready=saved
                    elif s['op']=='upkeep_step':validate_upkeep_step(s);self.assertEqual(ready,s['player']);ready=None
                    elif s['op'] in ('turn','cast','cleanup'):ready=None
        self.assertEqual(total,110)


if __name__ == '__main__': unittest.main()
