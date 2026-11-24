"""Focused contracts for actual Aura grants and printed damage amounts."""

import json
from pathlib import Path
import unittest

from generate_cases import registry
import aura_granted_damage_recipes as model


class GrantedDamageAuraRecipes(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        data = Path(__file__).parent / 'data'
        cls.oracle = json.loads((data / 'oracle.json').read_text(encoding='utf8'))['cards']
        cls.rows = {row['name']: row for row in registry()}
        cls.homes = {}
        for name,pin in model.pins()['sources'].items():
            csvid = pin['registry']['csvid']
            path, = (data / 'cases/cards').glob(f'*/{csvid:05d}-*.json')
            cls.homes[name] = json.loads(path.read_text(encoding='utf8'))['recipes'][0]

    def cases(self,name,*selected):
        declaration = self.homes[name]
        shape = model.spec(declaration,self.oracle[name])
        return model.expand(name,self.rows[name]['csvid'],declaration,shape,selected,self.rows,self.oracle)

    def steps(self,name,variant):
        return self.cases(name,variant)[0]['steps']

    def values(self,steps,op,**fields):
        return [s['value'] for s in steps if s['op']==op and all(s.get(k)==v for k,v in fields.items())]

    def test_full_original_text_and_typed_variant_scope(self):
        model.validate_witnesses(self.oracle,self.rows)
        cases = []
        for name,declaration in self.homes.items():
            shape = model.spec(declaration,self.oracle[name])
            self.assertEqual(declaration['variants'],model.variants(shape))
            selected = self.cases(name,*declaration['variants'])
            self.assertEqual(len(selected),54 if shape['effect']=='power' else 46)
            for case in selected:
                self.assertEqual(case['_recipe']['covered_clauses'],self.oracle[name]['oracle_text'].splitlines())
            cases.extend(selected)
        self.assertEqual(len({case['name'] for case in cases}),192)
        name = 'Burning Anger'
        d,r = self.homes[name],self.oracle[name]
        with self.assertRaises(ValueError):
            model.spec(d,dict(r,oracle_id='stale'))
        for change in (dict(version=True),dict(variants=['player_0','player_0']),
                       dict(variants=['unreviewed_0']),dict(params=dict(d['params'],effect='fixed'))):
            with self.subTest(change=change), self.assertRaises(ValueError):
                model.spec(dict(d,**change),r)
        with self.assertRaises(ValueError):
            model.parameters(dict(r,oracle_text=r['oracle_text'].replace('Enchant creature','Enchant creature you control',1)))

    def test_paid_aura_is_cast_and_attached_before_grant(self):
        for name,color,generic in (('Hermetic Study',2,1),('Power of Fire',4,1),('Psionic Gift',2,1),('Burning Anger',4,4)):
            with self.subTest(name=name):
                steps = self.steps(name,'player_1')
                clear = [s for s in steps if s['op']=='set_mana'][:12]
                self.assertEqual(clear,[dict(op='set_mana',player=p,color=c,value=0) for p in (0,1) for c in range(6)])
                cast = steps.index(dict(op='cast',ref='source',value=1))
                paid = steps.index(dict(op='resolve'),cast)
                grant = steps.index(dict(op='activate',ref='source',value=1))
                self.assertIn(dict(op='choose_target',player=1,ref='host'),steps[:cast])
                self.assertIn(dict(op='set_mana',player=1,color=0,value=generic),steps[:cast])
                self.assertIn(dict(op='set_mana',player=1,color=color,value=1),steps[:cast])
                self.assertIn(dict(op='assert_mana',player=1,color=color,value=0),steps[cast:paid])
                self.assertLess(paid,grant)

    def test_wrong_price_or_type_and_late_host_failure(self):
        name = 'Hermetic Study'
        for variant in ('short_0','wrong_color_1','no_creature_0'):
            steps = self.steps(name,variant)
            self.assertIn(dict(op='cast',ref='source',value=0),steps)
            self.assertNotIn('choose_target',{s['op'] for s in steps})
        steps = self.steps(name,'cast_host_leaves_0')
        self.assertIn(dict(op='cast_response',ref='answer',value=1),steps)
        at = steps.index(dict(op='resolve_response'))
        self.assertIn(dict(op='assert_stack',value=2),steps[:at])
        self.assertIn(dict(op='assert_zone_multiplicity',player=0,zone='hand',index=221,value=1),steps[at:])
        self.assertIn(dict(op='assert_zone_multiplicity',player=0,zone='graveyard',index=3793,value=1),steps[at:])

    def test_host_pays_tap_and_target_damage_waits(self):
        steps = self.steps('Psionic Gift','nonlethal_1')
        tap = steps.index(dict(op='activate',ref='source',value=1))
        resolve = steps.index(dict(op='resolve'),tap)
        for ref,value in (('host',1),('source',0)):
            self.assertIn(dict(op='assert_card',ref=ref,property='tapped',value=value),steps[tap:resolve])
        self.assertIn(dict(op='assert_stack',value=1),steps[tap:resolve])
        self.assertEqual(self.values(steps[tap:resolve],'assert_card',ref='victim',property='damage'),[0])
        self.assertEqual(self.values(steps[resolve:],'assert_card',ref='victim',property='damage'),[1])

    def test_player_creature_and_planeswalker_routes(self):
        for aura,damage in (('Power of Fire',1),('Burning Anger',4)):
            player,creature,walker = self.cases(aura,'self_player_0','creature_0','walker_0')
            self.assertEqual(self.values(player['steps'],'assert_life',player=0)[-1],20-damage)
            self.assertIn(dict(op='assert_zone_multiplicity',player=1,zone='graveyard',index=1354,value=1),creature['steps'])
            if damage==1:
                self.assertEqual(self.values(walker['steps'],'assert_counter',ref='victim',counter_kind='loyalty')[-1],2)
            else:
                self.assertIn(dict(op='assert_zone_multiplicity',player=1,zone='graveyard',index=9185,value=1),walker['steps'])

    def test_fresh_tapped_cancel_and_untap_repeat_are_real(self):
        name = 'Power of Fire'
        for variant in ('fresh_0','tapped_1','cancel_0'):
            steps = self.steps(name,variant)
            self.assertIn(dict(op='activate',ref='source',value=0),steps)
            self.assertEqual(self.values(steps,'assert_life',player=1)[-1],20)
        steps = self.steps(name,'repeat_1')
        self.assertEqual(self.values(steps,'activate',ref='source'),[1,0,1])
        untap = steps.index(dict(op='cast',ref='untap',value=1))
        self.assertEqual(steps[untap+1]['op'],'assert_choices_consumed')
        self.assertIn(dict(op='choose_dialog',player=1,value=1),steps[untap:])
        self.assertEqual(self.values(steps,'assert_life',player=0)[-1],18)

    def test_cross_controller_uses_legacy_input_and_printed_source(self):
        for name,selector,damage in (('Hermetic Study',0,1),('Burning Anger',1,2)):
            steps = self.steps(name,'foreign_host_1')
            self.assertIn(dict(op='choose_player',player=selector,value=1),steps)
            self.assertEqual(self.values(steps,'assert_life',player=1)[-1],20-damage)
            self.assertEqual(self.values(steps,'assert_life',player=0)[-1],20+damage)
        steps = self.steps('Hermetic Study','changed_control_0')
        self.assertIn(dict(op='bind_permanent',ref='host',player=1,value=221),steps)
        self.assertIn(dict(op='choose_player',player=1,value=0),steps)
        self.assertEqual(self.values(steps,'assert_life',player=0)[-1],19)

    def test_source_color_protection_and_real_lifelink(self):
        steps = self.steps('Burning Anger','source_color_0')
        self.assertIn(dict(op='probe_activation_target',ref='source',other='protected',index=-1,value=0),steps)
        self.assertEqual(self.values(steps,'assert_life',player=0)[-1],22)
        self.assertEqual(self.values(steps,'assert_life',player=1)[-1],18)

    def test_pending_target_provider_and_host_lifecycle(self):
        for variant,harm in (('target_leaves_0',0),('aura_removed_pending_0',1),('host_removed_pending_0',1)):
            with self.subTest(variant=variant):
                steps = self.steps('Psionic Gift',variant)
                activation = steps.index(dict(op='activate',ref='source',value=1))
                cast = steps.index(dict(op='cast',ref='answer',value=1))
                self.assertLess(activation,cast)
                self.assertIn(dict(op='assert_stack',value=2),steps[cast:])
                self.assertEqual(self.values(steps,'assert_life',player=1)[-1],20-harm)
                expected_zone = 'battlefield' if variant.startswith('target_') else 'graveyard'
                self.assertIn(dict(op='assert_zone_multiplicity',player=0,zone=expected_zone,index=5663,value=1),steps[cast:])

    def test_burning_anger_uses_live_resolving_or_last_known_power(self):
        n = 'Burning Anger'
        for variant,power in (('power_up_pending_0',7),('power_down_pending_0',2),
                              ('power_zero_0',0),('power_last_known_0',7)):
            steps = self.steps(n,variant)
            at = steps.index(dict(op='activate',ref='source',value=1))
            self.assertEqual(self.values(steps,'assert_life',player=1)[-1],20-power)
            if variant!='power_zero_0':
                self.assertIn(dict(op='cast',ref='answer',value=1),steps[at:])
                self.assertIn(dict(op='assert_stack',value=2),steps[at:])

    def test_ready_and_pending_snapshot_replays(self):
        for variant,pending in (('replay_1',False),('pending_replay_1',True)):
            steps = self.steps('Hermetic Study',variant)
            at = steps.index(dict(op='capture'))
            record = steps.index(dict(op='record_state'))
            restore = steps.index(dict(op='restore'))
            self.assertEqual(steps[at+1:record],steps[restore+1:-1])
            self.assertEqual(steps[-1],dict(op='assert_recorded_state'))
            self.assertEqual(any(s['op']=='activate' for s in steps[at:record]),not pending)
