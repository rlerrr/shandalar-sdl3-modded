"""Focused guards for paid fresh combat and the actual end-step lifecycle."""
import json
from pathlib import Path
import unittest

from generate_cases import registry
from recipes import cost_from_oracle, expand
import haste_end_step_return_recipes as family


class HasteEndStepReturnTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        root = Path(__file__).parent
        cls.oracle = json.loads((root/'data/oracle.json').read_text(encoding='utf8'))['cards']
        cls.rows = {r['name']:r for r in registry()}
        cls.cases, cls.declarations = {}, {}
        for name, pin in family.pins()['sources'].items():
            csvid = pin['registry']['csvid']
            path, = (root/'data/cases/cards'/f'{csvid//100:03}').glob(f'{csvid:05d}-*.json')
            declaration = json.loads(path.read_text(encoding='utf8'))['recipes'][0]
            cls.declarations[name] = declaration
            cls.cases[name] = {c['_recipe']['variant']:c for c in expand(
                name,csvid,declaration,cls.oracle[name],cls.rows,cls.oracle)}

    def steps(self, name, mode, who):
        return self.cases[name][f'{mode}_{who}']['steps']

    def test_source_is_normal_and_full_clause_is_required(self):
        self.assertEqual(sum(len(c) for n,c in self.cases.items() if n!='Dust Stalker'),164)
        self.assertEqual(len(self.cases['Dust Stalker']),50)
        for name, declaration in self.declarations.items():
            record = self.oracle[name]
            family.spec(declaration,record)
            with self.assertRaises(ValueError):
                family.parameters(dict(record,oracle_text=record['oracle_text'].replace('return this creature','exile this creature')))
            with self.assertRaises(ValueError):
                family.spec(dict(declaration,params=dict(declaration['params'],flying=1)),record)
        rows = dict(self.rows,**{'Bazaar Trader':dict(self.rows['Bazaar Trader'],handler='card_dummy')})
        with self.assertRaises(ValueError):
            family.validate_witnesses(self.oracle,rows)

    def test_cast_pays_printed_red_cost_and_haste_attacks_while_fresh(self):
        for name, dec in self.declarations.items():
            for owner in (0,1):
                steps = self.steps(name,'fresh_attack',owner)
                cast = steps.index(dict(op='cast',ref='source',value=1))
                mana = [0]*6
                for step in steps[:cast]:
                    if step['op']=='set_mana' and step['player']==owner:
                        mana[step['color']] = step['value']
                self.assertEqual(mana,cost_from_oracle(self.oracle[name]))
                hit = steps.index(dict(op='attack',ref='source',value=1))
                self.assertNotIn(dict(op='advance_turn'),steps[cast:hit])
                self.assertIn(dict(op='assert_life',player=1-owner,value=20-dec['params']['power']),steps[hit:])
                control = self.steps(name,'fresh_comparison',owner)
                self.assertIn(dict(op='cast',ref='normal',value=1),control)
                self.assertIn(dict(op='attack',ref='normal',value=0),control)
                self.assertIn(dict(op='cast',ref='source',value=0),self.steps(name,'wrong_color',owner))
                self.assertFalse(any(s['op']=='require_can_cast' for s in control))
                removed = self.steps(name,'spell_death',owner)
                self.assertIn(dict(op='require_can_cast',ref='removal',value=1),removed)

    def test_end_return_on_both_active_players_with_no_attack(self):
        for name in self.cases:
            csvid = self.rows[name]['csvid']
            for owner in (0,1):
                for mode, active in [('no_attack',owner),('opposing_end',1-owner)]:
                    steps = self.steps(name,mode,owner)
                    self.assertEqual(steps[0],dict(op='turn',player=active,value=0))
                    self.assertFalse(any(s['op']=='attack' for s in steps))
                    end = steps.index(dict(op='cleanup'))
                    self.assertIn(dict(op='assert_zone_multiplicity',player=owner,zone='hand',index=csvid,value=1),steps[end:])
                    self.assertIn(dict(op='assert_card',ref='source',property='in_play',value=0),steps[end:])

    def test_real_donation_returns_to_owner_across_the_board(self):
        for name in self.cases:
            csvid = self.rows[name]['csvid']
            for owner in (0,1):
                steps = self.steps(name,'donated',owner)
                transfer = steps.index(dict(op='activate',ref='trader',value=1))
                end = steps.index(dict(op='cleanup'))
                self.assertIn(dict(op='choose_player',player=owner,value=1-owner),steps[:transfer])
                self.assertIn(dict(op='bind_permanent',ref='source',player=1-owner,value=csvid),steps[transfer:end])
                self.assertIn(dict(op='assert_zone_multiplicity',player=1-owner,zone='battlefield',index=csvid,value=1),steps[transfer:end])
                self.assertIn(dict(op='assert_zone',player=1-owner,zone='hand',value=0),steps[end:])
                self.assertIn(dict(op='assert_zone_multiplicity',player=owner,zone='hand',index=csvid,value=1),steps[end:])

    def test_archwing_checks_ground_flying_and_reach_with_real_damage(self):
        for owner in (0,1):
            for mode, blocker, attacker, legal in [('ground_block','blocker','source',0),
                ('flying_blocker','flyer','source',1),('blocking_flyer','source','flyer',1),
                ('reach_blocks','blocker','source',1)]:
                steps = self.steps('Archwing Dragon',mode,owner)
                self.assertIn(dict(op='block',ref=blocker,other=attacker,value=legal),steps)
                self.assertLess(steps.index(dict(op='combat_damage')),steps.index(dict(op='end_combat')))
                end = steps.index(dict(op='cleanup'))
                zone = 'graveyard' if mode=='reach_blocks' else 'hand'
                self.assertIn(dict(op='assert_zone_multiplicity',player=owner,zone=zone,index=12309,value=1),steps[end:])
            ground = self.steps('Viashino Sandstalker','ground_block',owner)
            self.assertIn(dict(op='block',ref='blocker',other='source',value=1),ground)

    def test_removed_creature_and_countered_spell_do_not_return(self):
        for name in self.cases:
            for owner in (0,1):
                for mode in ('spell_death','reach_blocks','countered'):
                    steps = self.steps(name,mode,owner)
                    end = steps.index(dict(op='cleanup'))
                    self.assertIn(dict(op='assert_zone_multiplicity',player=owner,zone='graveyard',index=self.rows[name]['csvid'],value=1),steps[end:])
                    self.assertIn(dict(op='assert_zone',player=owner,zone='hand',value=0),steps[end:])

    def test_later_turn_recast_is_paid_and_has_fresh_combat_again(self):
        for name, dec in self.declarations.items():
            for owner in (0,1):
                steps = self.steps(name,'single_recast',owner)
                bind = steps.index(dict(op='bind_hand',ref='source',player=owner,value=self.rows[name]['csvid']))
                self.assertEqual(steps[:bind].count(dict(op='advance_turn')),2)
                self.assertEqual(steps.count(dict(op='cast',ref='source',value=1)),2)
                self.assertEqual(steps.count(dict(op='attack',ref='source',value=1)),2)
                self.assertIn(dict(op='assert_life',player=1-owner,value=20-2*dec['params']['power']),steps[bind:])

    def test_pending_and_actual_end_step_replay_snapshot(self):
        for name in self.cases:
            for owner in (0,1):
                for mode, first in [('pending_replay','resolve'),('end_replay','cleanup')]:
                    steps = self.steps(name,mode,owner)
                    cap = steps.index(dict(op='capture'))
                    self.assertEqual(steps[cap+1]['op'],first)
                    self.assertEqual(steps[cap+1:steps.index(dict(op='record_state'))],
                                     steps[steps.index(dict(op='restore'))+1:-1])

    def test_conditional_return_counts_only_other_controlled_colorless_creatures(self):
        name, csvid = 'Dust Stalker',15314
        for owner in (0,1):
            for mode,stays,controller in (
                ('own_colorless',True,owner),('opposing_colorless',True,owner),
                ('foe_colorless',False,owner),('colored_only',False,owner),
                ('noncreature_only',False,owner),('witness_lost',False,owner),
                ('donate_retains',True,1-owner),('donate_returns',False,1-owner),
            ):
                steps = self.steps(name,mode,owner)
                end = steps.index(dict(op='cleanup'))
                self.assertIn(dict(op='assert_matching_permanents',player=controller,index=csvid,property='color',value=0),steps[:end])
                self.assertIn(dict(op='assert_zone_multiplicity',player=controller if stays else owner,
                                   zone='battlefield' if stays else 'hand',index=csvid,value=1),steps[end:])
                if mode=='witness_lost':
                    self.assertIn(dict(op='cast',ref='removal',value=1),steps[:end])
                    self.assertIn(dict(op='assert_zone_multiplicity',player=owner,zone='graveyard',index=10860,value=1),steps[end:])
                if mode=='opposing_colorless':
                    self.assertEqual(steps[0],dict(op='turn',player=1-owner,value=0))

    def test_colorless_cast_triggers_sentinel_and_bonus_expires(self):
        for owner in (0,1):
            steps = self.steps('Dust Stalker','spell_colorless',owner)
            announce = steps.index(dict(op='announce_spell',ref='source'))
            colored = steps.index(dict(op='announce_spell',ref='normal'))
            end = steps.index(dict(op='cleanup'))
            self.assertLess(steps.index(dict(op='cast',ref='source',value=1)),announce)
            self.assertIn(dict(op='assert_card',ref='color_witness',property='power',value=1),steps[colored:announce])
            self.assertNotIn(dict(op='assert_card',ref='color_witness',property='power',value=2),steps[:announce])
            self.assertIn(dict(op='assert_card',ref='color_witness',property='power',value=2),steps[announce:end])
            self.assertIn(dict(op='assert_card',ref='color_witness',property='power',value=1),steps[end:])


if __name__=='__main__':
    unittest.main()
