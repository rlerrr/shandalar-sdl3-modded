import json
from pathlib import Path
import unittest

from generate_cases import registry
from monument_cast_rewards_recipes import FAMILY, classify, expand, profiles, spec, validate_witnesses, variants
from recipes import COLORS


class MonumentCastRewardsTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.oracle = json.loads((Path(__file__).parent/'data/oracle.json').read_bytes())['cards']
        cls.rows = {entry['name']:entry for entry in registry()}

    def make(self, name, scene, seat):
        card = self.oracle[name]
        v = f'{scene}_{seat}'
        d = dict(family=FAMILY,version=2,params=classify(card),oracle_text=card['oracle_text'],variants=[v])
        c, = expand(name,self.rows[name]['csvid'],d,spec(d,card),[v],self.rows,self.oracle)
        return c

    def actions(self, c, op, **fields):
        return [row for row in c['steps'] if row['op']==op and all(row.get(k)==v for k,v in fields.items())]

    def values(self, c, op, **fields):
        return [row['value'] for row in self.actions(c,op,**fields)]

    def test_whole_oracle_strict_color_and_population(self):
        validate_witnesses(self.oracle,self.rows)
        sources = profiles()['sources']
        self.assertEqual({row['registry']['csvid'] for row in sources.values()}, {16854,16937,16954,16984,17007})
        self.assertEqual(sum(len(variants(classify(self.oracle[name]))) for name in sources),150)
        for name in sources:
            card = self.oracle[name]
            with self.assertRaises(ValueError):
                classify(dict(card,oracle_text=card['oracle_text']+'\nDraw a card.'))
            with self.assertRaises(ValueError):
                spec(dict(family=FAMILY,version=True,params=classify(card),oracle_text=card['oracle_text'],variants=['matching_0']),card)

    def test_real_money_reduction_leaves_printed_surplus_only_when_appropriate(self):
        for name in profiles()['sources']:
            color = COLORS[classify(self.oracle[name])['color']]
            for seat in (0,1):
                c = self.make(name,'matching',seat)
                self.assertEqual(self.values(c,'set_mana',player=seat,color=0),[0,3,0])
                self.assertEqual(self.values(c,'set_mana',player=seat,color=color)[-1],1)
                self.assertEqual(self.values(c,'assert_mana',player=seat,color=0)[-1],0)
                surplus = self.make(name,'printed_budget',seat)
                self.assertEqual(self.values(surplus,'set_mana',player=seat,color=0),[0,3,1])
                self.assertEqual(self.values(surplus,'assert_mana',player=seat,color=0)[-1],1)
                floor = self.make(name,'floor',seat)
                self.assertEqual(self.values(floor,'cast',ref='member'),[0,1])

    def test_qualifying_cast_scope_distinguishes_color_controller_and_card_type(self):
        name = "Bontu's Monument"
        for seat in (0,1):
            for scene, life in (('wrong_spell_color',21),('wrong_color',20),('mixed_color',21),
                                ('opposing_match',20),('noncreature',20),('source_removed',20)):
                c = self.make(name,scene,seat)
                self.assertEqual(self.values(c,'assert_life',player=seat)[-1],life)
                self.assertEqual(self.values(c,'assert_life',player=1-seat)[-1],40-life)
            noncreature = self.make(name,'noncreature',seat)
            self.assertEqual(self.values(noncreature,'cast',ref='member'),[0,1])

    def test_cast_reward_happens_before_countering_and_is_not_damage(self):
        name = "Bontu's Monument"
        for seat in (0,1):
            countered = self.make(name,'countered',seat)
            announcement, = self.actions(countered,'announce_spell',ref='member')
            response, = self.actions(countered,'cast_response',ref='reply')
            a,b = countered['steps'].index(announcement),countered['steps'].index(response)
            self.assertIn(dict(op='assert_life',player=seat,value=21),countered['steps'][a:b])
            self.assertIn(dict(op='assert_life',player=1-seat,value=19),countered['steps'][a:b])
            self.assertEqual(self.values(countered,'assert_zone_multiplicity',player=seat,zone='graveyard',
                                         index=self.rows['Walking Corpse']['csvid'])[-1],1)
            prevented = self.make(name,'through_prevention',seat)
            self.assertEqual(self.values(prevented,'cast',ref='prevention'),[1])
            self.assertEqual(self.values(prevented,'assert_life',player=1-seat)[-1],19)

    def test_hazoret_discard_precedes_draw_and_is_optional(self):
        name = "Hazoret's Monument"
        accepted = self.make(name,'matching',0)
        self.assertEqual(self.values(accepted,'choose_zone_card',player=0),[self.rows['Island']['csvid']])
        self.assertEqual(self.values(accepted,'assert_offered_count'),[1])
        self.assertEqual(self.values(accepted,'assert_zone_multiplicity',player=0,zone='hand',
                                     index=self.rows['Forest']['csvid'])[-1],1)
        for scene in ('decline_discard','empty_hand'):
            declined = self.make(name,scene,0)
            self.assertEqual(len(self.actions(declined,'choose_optional_zone')),1)
            self.assertEqual(self.values(declined,'assert_forced_zone_choices'),[0])
            self.assertEqual(self.values(declined,'assert_zone',player=0,zone='library')[-1],2)
        decline = self.make(name,'decline_trigger',0)
        self.assertFalse(self.actions(decline,'choose_trigger_source'))

    def test_kefnet_freeze_has_no_tap_and_affects_only_next_own_untap(self):
        name = "Kefnet's Monument"
        for seat in (0,1):
            c = self.make(name,'untapped_then_tap',seat)
            self.assertEqual([row['player'] for row in self.actions(c,'untap_step')],[1-seat,seat,1-seat])
            self.assertEqual(self.values(c,'cast',ref='twiddle'),[1])
            self.assertEqual(self.values(c,'assert_card',ref='effect_target',property='tapped')[-4:],[1,1,1,0])
            self.assertEqual(self.values(c,'assert_card',ref='effect_target',property='tapped')[0],0)
            empty = self.make(name,'no_target',seat)
            self.assertFalse(self.actions(empty,'choose_target'))

    def test_oketra_vigilance_uses_real_attacking_then_blocking(self):
        name = "Oketra's Monument"
        for seat in (0,1):
            c = self.make(name,'vigilance',seat)
            attack, = self.actions(c,'attack',ref='warrior_0')
            block, = self.actions(c,'block',ref='warrior_0')
            since = c['steps'].index(attack)
            until = c['steps'].index(block)
            self.assertEqual([row['player'] for row in c['steps'][since:until] if row['op']=='untap_step'],[1-seat])
            self.assertTrue(all(val==0 for val in self.values(c,'assert_card',ref='warrior_0',property='tapped')))
            self.assertEqual(self.values(c,'assert_life',player=1-seat)[-1],19)
            fresh = self.make(name,'fresh_no_attack',seat)
            self.assertEqual(self.values(fresh,'attack',ref='warrior_0'),[0])

    def test_rhonas_actual_trample_and_power_expiry(self):
        name = "Rhonas's Monument"
        for seat in (0,1):
            c = self.make(name,'trample_damage',seat)
            self.assertEqual(self.values(c,'attack',ref='effect_target'),[1])
            self.assertEqual(self.values(c,'block',ref='combat_foe'),[1])
            self.assertEqual(self.values(c,'assert_life',player=1-seat)[-1],18)
            self.assertEqual(self.values(c,'assert_card',ref='effect_target',property='power')[-2:],[4,2])
            self.assertEqual(self.values(c,'assert_card',ref='effect_target',property='toughness')[-2:],[4,2])
