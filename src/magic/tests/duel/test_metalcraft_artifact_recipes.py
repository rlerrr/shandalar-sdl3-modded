"""Focused guards for actual artifact thresholds and the complete card clauses."""

import json
from pathlib import Path
import unittest

from generate_cases import registry
from recipes import expand
import metalcraft_artifact_recipes as family


class MetalcraftArtifactTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        data = Path(__file__).parent/'data'
        cls.oracle = json.loads((data/'oracle.json').read_bytes())['cards']
        cls.rows = {row['name']:row for row in registry()}
        cls.declarations,cls.cases = {},{}
        for name,pin in family.pins()['sources'].items():
            csvid = pin['registry']['csvid']
            home, = (data/'cases/cards'/f'{csvid//100:03d}').glob(f'{csvid:05d}-*.json')
            declaration = json.loads(home.read_bytes())['recipes'][0]
            cls.declarations[name] = declaration
            cls.cases[name] = {case['_recipe']['variant']:case for case in expand(
                name,csvid,declaration,cls.oracle[name],cls.rows,cls.oracle)}

    def case(self,name,mode,owner):
        return self.cases[name][f'{mode}_{owner}']

    def steps(self,name,mode,owner):
        return self.case(name,mode,owner)['steps']

    def test_full_clauses_and_pins(self):
        self.assertEqual(sum(map(len,self.cases.values())),178)
        for name,declaration in self.declarations.items():
            record = self.oracle[name]
            self.assertEqual(declaration['oracle_text'],record['oracle_text'])
            with self.assertRaises(ValueError):
                family.parameters(dict(record,oracle_text=record['oracle_text']+'\nDraw a card.'))
            with self.assertRaises(ValueError):
                family.spec(dict(declaration,params=dict(declaration['params'],artifact=1)),record)
        changed = dict(self.rows,**{'Shimmer Myr':dict(self.rows['Shimmer Myr'],handler='card_dummy')})
        with self.assertRaises(ValueError):
            family.validate_witnesses(self.oracle,changed)

    def test_control_and_type_scope_and_self_inclusion_at_entry(self):
        for name,declaration in self.declarations.items():
            for owner in (0,1):
                for mode,after in [('cast_below',2),('cast_threshold',3),('enemy_scope',2)]:
                    case = self.case(name,mode,owner)
                    count = sum(entry['player']==owner and 'Artifact' in self.oracle[entry['name']]['type_line'].split(' — ',1)[0]
                                for entry in case['setup'])
                    self.assertEqual(count,after)
                    if mode=='enemy_scope':
                        self.assertTrue(any(e['ref']=='own_nonartifact' and e['player']==owner for e in case['setup']))
                        self.assertTrue(any(e['ref']=='enemy_artifact' and e['player']==1-owner for e in case['setup']))

    def test_sphinx_pays_blue_exiles_until_actual_end_and_returns_to_activator(self):
        name = 'Argent Sphinx'
        for owner in (0,1):
            for mode,actor in [('cast_threshold',owner),('offturn',owner),('control_return',1-owner),('pending_drop',owner)]:
                steps = self.steps(name,mode,owner)
                activation = steps.index(dict(op='activate',ref='source',value=1))
                end = steps.index(dict(op='cleanup'))
                bind = steps.index(dict(op='bind_permanent',ref='source',player=actor,value=11199),end+1)
                self.assertIn(dict(op='set_mana',player=actor,color=2,value=1),steps[:activation])
                self.assertIn(dict(op='assert_stack',value=1),steps[activation:end])
                self.assertIn(dict(op='assert_zone_multiplicity',player=owner,zone='exile',index=11199,value=1),steps[activation:end])
                self.assertIn(dict(op='assert_zone_multiplicity',player=actor,zone='battlefield',index=11199,value=1),steps[bind:])
                self.assertLess(end,bind)
            self.assertIn(dict(op='activate',ref='source',value=0),self.steps(name,'cast_below',owner))
            self.assertIn(dict(op='activate',ref='source',value=0),self.steps(name,'fee_short',owner))

    def test_real_entry_responses_and_haste_observe_current_artifacts(self):
        for owner in (0,1):
            for card in ('Blade-Tribe Berserkers','Lumengrid Drake'):
                for mode in ('pending_gain','pending_drop'):
                    steps = self.steps(card,mode,owner)
                    self.assertIn(dict(op='cast_response',ref='response',value=1),steps)
                    self.assertIn(dict(op='resolve_response'),steps)
                if card=='Blade-Tribe Berserkers':
                    for mode,legal in [('cast_threshold',1),('pending_gain',1),('pending_drop',0),('late_gain',0),('drop',1)]:
                        self.assertIn(dict(op='attack',ref='source',value=legal),self.steps(card,mode,owner))
                    duration = self.steps(card,'duration',owner)
                    end = duration.index(dict(op='cleanup'))
                    for value,window in [(6,duration[:end]),(3,duration[end:])]:
                        self.assertIn(dict(op='assert_card',ref='source',property='power',value=value),window)
                else:
                    self.assertIn(dict(op='choose_target',player=owner,ref='victim'),self.steps(card,'pending_gain',owner))
                    self.assertNotIn(dict(op='choose_target',player=owner,ref='victim'),self.steps(card,'pending_drop',owner))

    def test_drake_uses_own_target_and_returns_stolen_permanent_to_owner(self):
        for owner in (0,1):
            for mode in ('own_target','stolen_target','self_target'):
                target = 'source' if mode=='self_target' else 'victim'
                identity = 11300 if target=='source' else self.rows['Goblin Tinkerer']['csvid']
                steps = self.steps('Lumengrid Drake',mode,owner)
                self.assertIn(dict(op='choose_target',player=owner,ref=target),steps)
                self.assertIn(dict(op='assert_zone_multiplicity',player=owner,zone='hand',index=identity,value=1),steps[-36:])
                if mode=='stolen_target':
                    self.assertIn(dict(op='bind_permanent',ref='victim',player=1-owner,value=identity),steps)

    def test_protection_checks_every_color_and_allows_colorless_damage(self):
        for owner in (0,1):
            for mode,result in [('cast_below',1),('cast_threshold',0),('gain',0),('drop',1)]:
                steps = self.steps('Etched Champion',mode,owner)
                for i in range(5):
                    self.assertIn(dict(op='can_target_creature',ref=f'color_probe_{i}',other='source',value=result),steps)
            ping = self.steps('Etched Champion','colorless',owner)
            self.assertIn(dict(op='activate',ref='rod',value=1),ping)
            self.assertIn(dict(op='assert_card',ref='source',property='damage',value=1),ping)
            blocking = self.steps('Etched Champion','prevent_damage',owner)
            self.assertIn(dict(op='block',ref='source',other='blocker',value=1),blocking)
            self.assertIn(dict(op='assert_card',ref='source',property='damage',value=0),blocking)

    def test_shroud_affects_owned_artifacts_even_from_own_caster(self):
        name = 'Indomitable Archangel'
        for owner in (0,1):
            for mode,result in [('cast_below',1),('gain',0),('drop',1),('source_removed',1)]:
                for ref in ('own_probe','foe_probe'):
                    self.assertIn(dict(op='can_target_creature',ref=ref,other='artifact_0',value=result),self.steps(name,mode,owner))
            noncreature = self.steps(name,'noncreature',owner)
            for caster in (0,1):
                self.assertIn(dict(op='can_cast',ref=f'art_probe_{caster}',value=0),noncreature)
            scope = self.steps(name,'scope',owner)
            self.assertIn(dict(op='choose_target',player=owner,ref='enemy_artifact'),scope)
            self.assertIn(dict(op='can_target_creature',ref='foe_probe',other='source',value=1),scope)

    def test_relic_remains_artifact_and_is_a_5_5_golem_only_at_three(self):
        name = 'Rusted Relic'
        for owner in (0,1):
            for mode,active in [('cast_below',0),('cast_threshold',1),('gain',1),('drop',0)]:
                steps = self.steps(name,mode,owner)
                for key,value in [('artifact',1),('subtype_Golem',active)]:
                    self.assertIn(dict(op='assert_matching_permanents',player=owner,index=11350,property=key,value=value),steps)
                self.assertIn(dict(op='can_target_creature',ref='foe_probe',other='source',value=active),steps)
                self.assertEqual(any(s['op']=='assert_card' and s['ref']=='source' and s['property']=='power' and s['value']==5 for s in steps[-34:]),bool(active))
            self.assertIn(dict(op='attack',ref='source',value=1),self.steps(name,'old_attack',owner))
            self.assertIn(dict(op='attack',ref='source',value=0),self.steps(name,'fresh_attack',owner))

    def test_certarch_paid_tap_target_scope_and_no_resolution_recheck(self):
        name = 'Vedalken Certarch'
        for owner in (0,1):
            for mode in ('target_artifact','target_land','target_creature','pending_drop'):
                steps = self.steps(name,mode,owner)
                cast = steps.index(dict(op='activate',ref='source',value=1))
                resolve = steps.index(dict(op='resolve'))
                self.assertIn(dict(op='assert_card',ref='source',property='tapped',value=1),steps[cast:resolve])
                self.assertIn(dict(op='assert_card',ref='victim',property='tapped',value=1),steps[resolve:])
                if mode=='pending_drop':
                    self.assertLess(cast,steps.index(dict(op='activate',ref='dropper',value=1)))
            self.assertNotIn(dict(op='activate',ref='source',value=1),self.steps(name,'sick',owner))
            self.assertIn(dict(op='probe_activation_target',ref='source',other='invalid',index=-1,value=0),
                          self.steps(name,'invalid_enchantment',owner))
            for mode in ('drop','pending_drop'):
                steps = self.steps(name,mode,owner)
                self.assertFalse(any(s['op']=='assert_card' and s.get('ref')=='crypt_grave' for s in steps))
                for zone in ('graveyard','exile'):
                    self.assertIn(dict(op='assert_zone_multiplicity',player=1-owner,zone=zone,
                                       index=self.rows['Plains']['csvid'],value=1),steps)

    def test_replay_uses_same_actions_and_complete_board_receipts(self):
        for name in self.cases:
            for owner in (0,1):
                steps = self.steps(name,'replay',owner)
                cap = steps.index(dict(op='capture'))
                record = steps.index(dict(op='record_state'))
                restore = steps.index(dict(op='restore'))
                self.assertEqual(steps[cap+1:record],steps[restore+1:-1])
                self.assertEqual(steps[-1],dict(op='assert_recorded_state'))


if __name__=='__main__':
    unittest.main()
