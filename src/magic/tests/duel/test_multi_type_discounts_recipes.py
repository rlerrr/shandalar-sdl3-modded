"""Guard whole Oracle cost layers and the actual cast-trigger choreography."""

import json
from pathlib import Path
import unittest

from generate_cases import registry
import multi_type_discounts_recipes as model


GOBLIN = 'Goblin Electromancer'
HEARTLESS = 'Heartless Summoning'
MATRIX = 'Mana Matrix'
GATE = 'Planar Gate'
HERALD = 'Herald of the Pantheon'
JACE = "Jace's Sanctum"


class MultiTypeDiscountTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        data = Path(__file__).parent/'data'
        cls.oracle = json.loads((data/'oracle.json').read_bytes())['cards']
        cls.rows = {r['name']:r for r in registry()}
        cls.homes = {}
        for name,pin in model.pins()['sources'].items():
            path, = (data/'cases/cards').glob(f'*/{pin["registry"]["csvid"]:05d}-*.json')
            cls.homes[name] = json.loads(path.read_bytes())['recipes'][0]

    def case(self,name,variant):
        home = self.homes[name]
        params = model.spec(home,self.oracle[name])
        return model.expand(name,self.rows[name]['csvid'],home,params,[variant],self.rows,self.oracle)[0]

    def steps(self,name,variant):
        return self.case(name,variant)['steps']

    def budget(self,name,variant):
        recipe = self.case(name,variant)
        steps = recipe['steps']
        end = next(i for i,s in enumerate(steps) if s['op']=='cast' and s['ref']=='probe')
        actor = next(e['player'] for e in recipe['setup'] if e['ref']=='probe')
        wallet = [0]*6
        for s in steps[:end]:
            if s['op']=='set_mana' and s['player']==actor:
                wallet[s['color']] = s['value']
        return wallet

    def select(self,name,variant,operation,**fields):
        return [s for s in self.steps(name,variant) if s['op']==operation and all(s.get(k)==n for k,n in fields.items())]

    def test_complete_pinned_oracle_and_typed_scope(self):
        model.validate_witnesses(self.oracle,self.rows)
        for name,home in self.homes.items():
            self.assertEqual(home['oracle_text'],self.oracle[name]['oracle_text'])
            self.assertEqual(home['variants'],model.variants(home['params']))
            with self.assertRaises(ValueError):
                model.spec(dict(home,params=dict(home['params'],amount=True)),self.oracle[name])
            with self.assertRaises(ValueError):
                model.spec(dict(home,variants=['payment_0','payment_0']),self.oracle[name])
            with self.assertRaises(ValueError):
                model.spec(home,dict(self.oracle[name],oracle_text=home['oracle_text']+'\nDraw a card.'))

    def test_scope_instant_sorcery_enchantment_and_mixed_type(self):
        for actor in (0,1):
            def c(name,scenario):
                return self.budget(name,f'{scenario}_{actor}')[0]
            self.assertEqual((c(MATRIX,'eligible_instant'),c(MATRIX,'eligible_enchantment'),c(MATRIX,'excluded_sorcery')),(0,1,2))
            self.assertEqual((c(GOBLIN,'eligible_instant'),c(GOBLIN,'eligible_sorcery'),c(GOBLIN,'excluded_enchantment')),(1,1,3))
            self.assertEqual((c(GATE,'artifact_creature'),c(GATE,'enchantment_creature'),c(GATE,'excluded_artifact')),(3,1,2))
            self.assertEqual((c(HERALD,'artifact_creature'),c(HERALD,'enchantment_creature'),c(HERALD,'excluded_creature')),(5,2,3))

    def test_zero_total_colored_floor_and_unpaid_controls(self):
        self.assertEqual(self.budget(GATE,'zero_total_1'),[0]*6)
        self.assertTrue(self.select(GATE,'zero_total_1','cast',ref='probe',value=1))
        self.assertEqual(self.budget(HERALD,'colored_floor_0'),[0,0,0,0,0,1])
        self.assertEqual(self.budget(MATRIX,'wrong_color_1'),[2,0,0,0,0,0])
        self.assertTrue(self.select(MATRIX,'wrong_color_1','cast',ref='probe',value=0))
        self.assertTrue(self.select(MATRIX,'wrong_color_1','assert_mana',player=1,color=0,value=2))
        self.assertTrue(self.select(GOBLIN,'one_short_0','cast',ref='probe',value=0))
        self.assertTrue(self.select(GOBLIN,'payment_0','assert_mana',player=0,color=0,value=1))

    def test_own_caster_battlefield_scope_and_real_composition(self):
        for name,home in self.homes.items():
            if home['params']['effect']!='static_discount':
                continue
            self.assertTrue(self.select(name,'opposing_caster_0','turn',player=1))
            self.assertEqual(self.budget(name,'opposing_caster_0'),self.budget(name,'source_bounce_0'))
            self.assertEqual(self.budget(name,'opposing_caster_0'),self.budget(name,'source_grave_0'))
            self.assertTrue(self.select(name,'source_bounce_0','cast',ref='bounce',value=1))
            self.assertEqual(self.budget(name,'combined_1')[0],0)
        self.assertTrue(self.select(GOBLIN,'offturn_instant_1','turn',player=0))
        probe = next(e for e in self.case(GOBLIN,'offturn_instant_1')['setup'] if e['ref']=='probe')
        self.assertEqual(probe['player'],1)

    def test_shrink_affects_only_own_creatures_and_actual_state_based_death(self):
        steps = self.steps(HEARTLESS,'source_cast_0')
        self.assertIn(dict(op='assert_card',ref='own',property='toughness',value=2),steps)
        self.assertIn(dict(op='assert_card',ref='own',property='toughness',value=1),steps)
        self.assertFalse(self.select(HEARTLESS,'source_cast_0','assert_card',ref='enemy',property='toughness',value=1))
        steps = self.steps(HEARTLESS,'state_based_death_1')
        self.assertFalse(any(s['op']=='destroy' for s in steps))
        self.assertTrue(self.select(HEARTLESS,'state_based_death_1','cast',ref='probe',value=1))
        self.assertTrue(self.select(HEARTLESS,'state_based_death_1','assert_zone_multiplicity',player=1,
                                    zone='graveyard',index=self.rows['Mogg Fanatic']['csvid'],value=1))

    def test_herald_triggers_on_cast_even_when_original_spell_is_countered(self):
        for actor in (0,1):
            steps = self.steps(HERALD,f'countered_{actor}')
            announce = next(i for i,s in enumerate(steps) if s['op']=='announce_spell')
            reply = next(i for i,s in enumerate(steps) if s['op']=='cast_response')
            self.assertIn(dict(op='assert_life',player=actor,value=21),steps[announce:reply])
            self.assertFalse(self.select(HERALD,f'excluded_instant_{actor}','assert_life',player=actor,value=21))
            self.assertTrue(self.select(HERALD,f'enchantment_creature_{actor}','assert_life',player=actor,value=21))

    def test_scry_uses_actual_bottom_input_with_exact_order_before_resolution(self):
        for actor in (0,1):
            steps = self.steps(JACE,f'scry_bottom_{actor}')
            scry, = [i for i,s in enumerate(steps) if s['op']=='choose_zone_card']
            announce = next(i for i,s in enumerate(steps) if s['op']=='announce_spell')
            resolve = next(i for i,s in enumerate(steps) if s['op']=='resolve')
            self.assertEqual((scry,steps[scry]['player'],steps[scry]['value']),
                             (announce-1,actor,self.rows['Mogg Fanatic']['csvid']))
            self.assertIn(dict(op='assert_zone_card',player=actor,zone='library',index=0,value=self.rows['Elvish Mystic']['csvid']),steps[announce:resolve])
            self.assertTrue(self.select(JACE,f'countered_{actor}','assert_scry_result',player=actor,value=1))
            self.assertFalse(self.select(JACE,f'scry_empty_{actor}','choose_zone'))
            self.assertFalse(self.select(JACE,f'excluded_enchantment_{actor}','record_library'))

    def test_restore_reexecutes_paid_spell_and_announced_trigger(self):
        steps = self.steps(JACE,'paid_replay_1')
        replay = [s['op'] for s in steps if s['op'] in ('capture','cast','announce_spell','restore','assert_recorded_state')]
        self.assertEqual(replay,['capture','cast','announce_spell','restore','cast','announce_spell','assert_recorded_state'])


    def test_familiar_union_reduces_a_two_color_spell_exactly_once(self):
        from recipes import cost_from_oracle

        names = [name for name,home in self.homes.items() if home['params']['effect']=='color_discount']
        self.assertEqual(len(names),5)
        for name in names:
            colors = set(self.homes[name]['params']['colors'])
            for mode,match in [('first_color',1),('second_color',1),('both_colors',2),('nonmatching_color',0),('colorless',0)]:
                scenario = f'{mode}_0'
                probe = next(e['name'] for e in self.case(name,scenario)['setup'] if e['ref']=='probe')
                record = self.oracle[probe]
                self.assertEqual(len(colors.intersection(record['colors'])),match)
                fee = cost_from_oracle(record)
                if match>0:
                    fee[0] -= 1
                self.assertEqual(self.budget(name,scenario),fee)
                if match==2:
                    self.assertGreaterEqual(fee[0],1)
            for actor in (0,1):
                self.assertTrue(self.select(name,f'both_payment_{actor}','assert_mana',player=actor,color=0,value=1))
                self.assertFalse(self.select(name,f'both_colors_{actor}','choose_zone'))
                self.assertTrue(self.select(name,f'opposing_caster_{actor}','turn',player=1-actor))

    def test_familiar_regeneration_pays_the_ability_fee_and_heals_actual_damage(self):
        name = 'Nightscape Familiar'
        self.assertEqual(self.homes[name]['params']['regeneration_cost'],[1,1,0,0,0,0])
        for actor in (0,1):
            scenario = f'regenerate_paid_{actor}'
            steps = self.steps(name,scenario)
            action = [row['op'] for row in steps if row['op'] in ('cast','choose_regeneration','resolve')]
            self.assertEqual(action,['cast','choose_regeneration','resolve'])
            self.assertTrue(self.select(name,scenario,'assert_card',ref='source',property='tapped',value=1))
            self.assertTrue(self.select(name,scenario,'assert_card',ref='source',property='damage',value=0))
            for variant in ('regenerate_short','regenerate_wrong_color','regenerate_forbidden'):
                self.assertFalse(self.select(name,f'{variant}_{actor}','choose_regeneration'))
                self.assertTrue(self.select(name,f'{variant}_{actor}','assert_zone_multiplicity',player=actor,
                                            zone='graveyard',index=self.rows[name]['csvid'],value=1))
            self.assertTrue(self.select(name,f'regenerate_proactive_{actor}','activate',ref='source',value=1))

    def test_familiar_keywords_use_real_declaration_and_damage_paths(self):
        for actor in (0,1):
            self.assertTrue(self.select('Stormscape Familiar',f'ground_denied_{actor}','block',value=0))
            self.assertTrue(self.select('Stormscape Familiar',f'flying_block_{actor}','block',value=1))
            self.assertTrue(self.select('Stormscape Familiar',f'ground_denied_{actor}','assert_life',player=1-actor,value=19))
            self.assertTrue(self.select('Sunscape Familiar',f'defender_attack_{actor}','attack',ref='source',value=0))
            self.assertTrue(self.select('Sunscape Familiar',f'defender_block_{actor}','block',ref='source',other='enemy',value=1))
            for variant in ('first_strike_attack','first_strike_block'):
                scenario = f'{variant}_{actor}'
                self.assertTrue(self.select('Thunderscape Familiar',scenario,'combat_damage'))
                self.assertTrue(self.select('Thunderscape Familiar',scenario,'assert_zone_multiplicity',player=1-actor,
                                            zone='graveyard',index=self.rows['Mogg Fanatic']['csvid'],value=1))
                self.assertTrue(self.select('Thunderscape Familiar',scenario,'assert_card',ref='source',property='damage',value=0))


if __name__=='__main__':
    unittest.main()
