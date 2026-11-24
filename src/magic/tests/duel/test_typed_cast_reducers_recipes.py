"""Guard subtype-predicate semantics and actual cast/combat choreography."""

import json
import unittest

from generate_cases import registry
from recipes import COLORS, cost_from_oracle
import typed_cast_reducers_recipes as model


class TypedCastReducerTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        data = model.PROFILE.parent
        cls.oracle = json.loads((data/'oracle.json').read_bytes())['cards']
        cls.rows = {r['name']:r for r in registry()}
        cls.homes = {}
        for name,pin in model.pins()['sources'].items():
            path, = (data/'cases/cards').glob(f'*/{pin["registry"]["csvid"]:05d}-*.json')
            cls.homes[name] = json.loads(path.read_bytes())['recipes'][0]

    def scene(self,name,variant):
        declaration = self.homes[name]
        params = model.spec(declaration, self.oracle[name])
        scene_type = model.ColoredScene if 'colors' in params else model.TypedScene
        world = scene_type(name,params,variant[:-2],int(variant[-1]),self.rows,self.oracle)
        world.setup_cards()
        return world

    def case(self,name,variant):
        declaration = self.homes[name]
        params = model.spec(declaration,self.oracle[name])
        return model.expand(name,self.rows[name]['csvid'],declaration,params,[variant],self.rows,self.oracle)[0]

    def select(self,name,variant,op,**fields):
        return [s for s in self.case(name,variant)['steps'] if s['op']==op and all(s.get(k)==v for k,v in fields.items())]

    def test_whole_oracle_exact_params_and_unmodified_types(self):
        model.validate_witnesses(self.oracle,self.rows)
        self.assertEqual(len(self.homes),12)
        for name,home in self.homes.items():
            record = self.oracle[name]
            self.assertEqual(home['variants'],model.variants(home['params']))
            self.assertEqual(home['oracle_text'],record['oracle_text'])
            with self.assertRaises(ValueError):
                model.spec(home,dict(record,oracle_text=record['oracle_text']+'\nFlying'))
            with self.assertRaises(ValueError):
                model.spec(dict(home,params=dict(home['params'],amount=True)),record)
            with self.assertRaises(ValueError):
                model.spec(dict(home,variants=['first_match_0','first_match_0']),record)

    def test_two_independent_subtypes_and_one_discount_for_overlap(self):
        for name,home in self.homes.items():
            if len(home['params']['subtypes'])!=2:
                continue
            for seat in (0,1):
                for mode in ('first_match','second_match','overlap'):
                    world = self.scene(name,f'{mode}_{seat}')
                    printed = cost_from_oracle(self.oracle[world.probe_name()])
                    self.assertGreaterEqual(printed[0],2)
                    self.assertEqual(world.current_price('probe'),[printed[0]-1,*printed[1:]])
                self.assertTrue(self.select(name,f'overlap_payment_{seat}','assert_mana',player=seat,color=0,value=1))
                combined = self.scene(name,f'combined_{seat}')
                printed = cost_from_oracle(self.oracle[combined.probe_name()])
                self.assertEqual(combined.current_price('probe'),[printed[0]-2,*printed[1:]])

    def test_kindred_changeling_and_colored_floor(self):
        for name,home in self.homes.items():
            shapes = home['params']['subtypes']
            for mode in ('kindred_changeling','changeling_creature'):
                world = self.scene(name,mode+'_0')
                self.assertEqual(model.matching_subtypes(self.oracle[world.probe_name()],shapes),set(shapes))
                if 'colors' in home['params']:
                    expected = [1, 0, 0, 0, 0, 0] if mode == 'kindred_changeling' else [1, 0, 1, 0, 0, 0]
                else:
                    expected = [0,1,0,0,0,0] if mode=='kindred_changeling' else [0,0,1,0,0,0]
                self.assertEqual(world.current_price('probe'), expected)
            self.assertTrue(self.select(name,'kindred_changeling_0','choose_target',ref='wall',player=0))
            self.assertTrue(self.select(name,'kindred_changeling_0','assert_card',ref='wall',property='toughness',value=3))
            if tuple(shapes) in model.KINDRED:
                world = self.scene(name,'printed_kindred_1')
                record = self.oracle[world.probe_name()]
                self.assertIn('Kindred',model.card_types(record))
                self.assertNotIn('Creature',model.card_types(record))
                self.assertLess(sum(world.current_price('probe')),sum(cost_from_oracle(record)))

    def test_targeted_kindred_uses_printed_wallet_and_observes_oracle_discount(self):
        for name in self.homes:
            modes = ['kindred_changeling']
            if name=='Stinkdrinker Daredevil':
                modes += ['printed_kindred']
            for mode in modes:
                for actor in (0,1):
                    variant = f'{mode}_{actor}'
                    world = self.scene(name,variant)
                    printed = cost_from_oracle(self.oracle[world.probe_name()])
                    remainder = printed[0]-world.current_price('probe')[0]
                    case = self.case(name,variant)
                    cast = next(i for i,s in enumerate(case['steps']) if s['op']=='cast' and s['ref']=='probe')
                    prefix = case['steps'][:cast]
                    self.assertIn(dict(op='set_mana',player=actor,color=0,value=printed[0]),prefix)
                    self.assertIn(prefix[-1]['op'],('choose_target','choose_player'))
                    self.assertEqual(case['steps'][cast+1]['op'],'assert_choices_consumed')
                    values = [s['value'] for s in case['steps'][cast:] if s['op']=='assert_mana'
                              and s['player']==actor and s['color']==0]
                    self.assertEqual(values,[remainder,remainder])

    def test_caster_and_battlefield_scope_without_weakening_cast(self):
        for name in self.homes:
            for mode in ('source_grave','opposing_caster','unrelated_creature','unrelated_noncreature'):
                world = self.scene(name,mode+'_1')
                self.assertEqual(world.current_price('probe'),cost_from_oracle(self.oracle[world.probe_name()]))
            self.assertTrue(self.select(name,'source_leaves_1','cast',ref='bounce',value=1))
            self.assertTrue(self.select(name,'short_0','cast',ref='probe',value=0))
            self.assertTrue(self.select(name,'wrong_color_1','cast',ref='probe',value=0))
            shape = self.scene(name,'tapped_source_0')
            self.assertTrue(shape.entities['source']['tapped'])
            self.assertLess(sum(shape.current_price('probe')),sum(cost_from_oracle(self.oracle[shape.probe_name()])))

    def test_real_haste_uses_fresh_paid_cast_and_does_not_grant_it(self):
        for actor in (0,1):
            name = 'Frogtosser Banneret'
            self.assertTrue(self.select(name,f'fresh_haste_{actor}','cast',ref='source',value=1))
            self.assertTrue(self.select(name,f'fresh_haste_{actor}','attack',ref='source',value=1))
            self.assertTrue(self.select(name,f'fresh_haste_{actor}','assert_life',player=1-actor,value=19))
            self.assertTrue(self.select(name,f'fresh_ally_{actor}','cast',ref='probe',value=1))
            self.assertTrue(self.select(name,f'fresh_ally_{actor}','attack',ref='probe',value=0))

    def test_islandwalk_requires_a_defending_island_present_at_blocks(self):
        for actor in (0,1):
            name = 'Stonybrook Banneret'
            scenario = f'defender_island_{actor}'
            land = next(row for row in self.case(name,scenario)['setup'] if row['ref']=='land')
            self.assertEqual((land['name'],land['player']),('Island',1-actor))
            self.assertTrue(self.select(name,scenario,'block',ref='blocker',value=0))
            self.assertTrue(self.select(name,scenario,'assert_life',player=1-actor,value=19))
            for variant in ('defender_forest','own_island','island_leaves'):
                self.assertTrue(self.select(name,f'{variant}_{actor}','block',ref='blocker',value=1))
            self.assertTrue(self.select(name,f'island_leaves_{actor}','choose_target',ref='land',player=actor))

    def test_actual_counterspell_and_replayed_paid_cast(self):
        for name in self.homes:
            self.assertTrue(self.select(name,'countered_1','announce_spell',ref='probe'))
            self.assertTrue(self.select(name,'countered_1','cast_response',ref='counter',value=1))
            self.assertFalse(self.select(name,'countered_1','choose_target'))
            replay = [s['op'] for s in self.case(name,'paid_replay_0')['steps']
                      if s['op'] in ('capture','cast','restore','assert_recorded_state')]
            self.assertEqual(replay,['capture','cast','restore','cast','assert_recorded_state'])

    def test_team_bonus_includes_source_and_new_friendly_creatures(self):
        for name, power, toughness in (('Daru Warchief', 1, 2), ('Undead Warchief', 2, 1)):
            home = self.homes[name]
            witness = model.WITNESSES[tuple(home['params']['subtypes'])][0]
            record = self.oracle[witness]
            for key, delta in (('power', power), ('toughness', toughness)):
                base = int(record[key])
                own = self.select(name, 'team_late_entry_0', 'assert_card', ref='own_team', property=key)
                self.assertEqual({row['value'] for row in own}, {base + delta})
                foe = self.select(name, 'team_late_entry_0', 'assert_card', ref='foe_team', property=key)
                self.assertEqual({row['value'] for row in foe}, {base})
                self.assertTrue(self.select(name, 'team_copies_1', 'assert_card', ref='probe', property=key,
                                            value=base + 2 * delta))
                gone = self.select(name, 'team_source_removed_1', 'assert_card', ref='probe', property=key)
                self.assertEqual([row['value'] for row in gone], [base + delta, base])
                self.assertTrue(self.select(name, 'team_late_entry_0', 'assert_card', ref='source', property=key,
                                            value=1 + delta))

    def test_goblin_warchief_haste_drives_real_fresh_attacks(self):
        name = 'Goblin Warchief'
        self.assertTrue(self.select(name, 'team_self_fresh_0', 'cast', ref='source', value=1))
        self.assertTrue(self.select(name, 'team_self_fresh_0', 'attack', ref='source', value=1))
        self.assertTrue(self.select(name, 'team_late_entry_1', 'attack', ref='probe', value=1))
        for variant in ('team_source_removed_0', 'team_opponent_fresh_1', 'team_unrelated_fresh_0'):
            self.assertTrue(self.select(name, variant, 'attack', ref='probe', value=0))
        steps = self.case(name, 'team_source_removed_0')['steps']
        casts = [step['ref'] for step in steps if step['op'] == 'cast']
        self.assertEqual(casts, ['probe', 'bounce'])

    def test_colored_discount_keeps_exact_generic_and_independent_color_balances(self):
        for name in ('Edgewalker', 'Ragemonger'):
            for actor in (0, 1):
                for mode in ('first_match', 'first_color_only', 'second_color_only', 'unaffected_color',
                             'wrong_color', 'stacked_colored'):
                    variant = f'{mode}_{actor}'
                    world = self.scene(name, variant)
                    printed = cost_from_oracle(self.oracle[world.probe_name()])
                    price = world.current_price('probe')
                    colors = {COLORS[color] for color in world.model['colors']}
                    copies = 2 if mode == 'stacked_colored' else 1
                    self.assertEqual(price, [max(0, value - copies) if index in colors else value
                                             for index, value in enumerate(printed)])
                    self.assertGreater(price[0], 0)
                    self.assertTrue(self.select(name, variant, 'cast', ref='probe', value=int(mode != 'wrong_color')))
                for color in self.homes[name]['params']['colors']:
                    self.assertTrue(self.select(name, f'printed_budget_{actor}', 'assert_mana',
                                                player=actor, color=COLORS[color], value=1))


if __name__=='__main__':
    unittest.main()
