"""Verify the complete Oracle and variant bindings for this recipe."""
import copy
import unittest

import spore_upkeep_additional_riders_recipes as recipe


class PinnedRecipeTests(unittest.TestCase):
    def test_sources_bind_entire_printed_records(self):
        sources = recipe.profiles()['sources']
        self.assertEqual(len(sources), 4)
        for name, pin in sources.items():
            with self.subTest(card=name):
                record = pin['oracle']
                physical = False
                printed = record['selected_face'] if physical else record
                shape = recipe.classify(record, pin['back_oracle']) if physical else recipe.classify(record)
                variants = recipe.variants(shape)
                self.assertEqual(len(variants), len(set(variants)))
                self.assertGreater(len(variants), 2)
                declaration = dict(family=recipe.FAMILY, version=2, oracle_text=printed['oracle_text'],
                                   params=shape, variants=variants)
                self.assertEqual(recipe.spec(declaration, record)['family'], recipe.FAMILY)
                changed_oracle = copy.deepcopy(record)
                changed_printed = changed_oracle['selected_face'] if physical else changed_oracle
                changed_printed['oracle_text'] += '\nFlying'
                with self.assertRaises(ValueError):
                    recipe.spec(declaration, changed_oracle)
                declaration['variants'] = variants + [variants[0]]
                with self.assertRaises(ValueError):
                    recipe.spec(declaration, record)


    def world(self, name, mode, owner):
        from pathlib import Path
        import json
        from generate_cases import registry

        oracle=json.loads((Path(recipe.__file__).parent/'data/oracle.json').read_bytes())['cards']
        rows={entry['name']:entry for entry in registry()}
        record=oracle[name]
        params=recipe.classify(record)
        variant=f'{mode}_{owner}'
        declaration=dict(family=recipe.FAMILY,version=2,oracle_text=record['oracle_text'],params=params,variants=[variant])
        case,=recipe.expand(name,rows[name]['csvid'],declaration,params,[variant],rows,oracle)
        return case

    def test_fungus_regeneration_uses_one_live_target_and_sacrifice(self):
        for owner in (0,1):
            case=self.world('Savage Thallid','own_fungus',owner)
            steps=case['steps']
            priority=steps.index(dict(op='choose_regeneration',ref='source'))
            destroy=steps.index(dict(op='destroy',ref='victim'))
            refs=['victim','saproling_0'] if owner==0 else ['victim']
            self.assertEqual([s['ref'] for s in steps[priority+1:destroy]],refs)
            self.assertEqual(steps[destroy+1],dict(op='assert_unused_choices',value=0))
            self.assertIn(dict(op='assert_card',ref='victim',property='in_play',value=1),steps[destroy+1:])
            self.assertIn(dict(op='assert_card',ref='victim',property='tapped',value=1),steps[destroy+1:])
            self.assertIn(dict(op='assert_zone_multiplicity',player=owner,zone='battlefield',index=18813,value=1),
                          steps[destroy+1:])

    def test_bounce_affordability_and_true_permanent_target(self):
        for owner in (0,1):
            burst=self.world('Saproling Burst','bounced',owner)
            feral=self.world('Feral Thallid','recast',owner)
            self.assertEqual(next(c['name'] for c in burst['setup'] if c['ref']=='reply'),'Boomerang')
            self.assertEqual(next(c['name'] for c in feral['setup'] if c['ref']=='reply'),'Unsummon')
            for case,due in ((burst,2),(feral,1)):
                steps=case['steps']
                cast=steps.index(dict(op='cast',ref='reply',value=1))
                self.assertEqual(steps[cast-1],dict(op='choose_target',ref='source',player=1-owner))
                self.assertIn(dict(op='set_mana',player=1-owner,color=2,value=due),steps[cast-8:cast])


    def test_victim_counters_follow_the_actual_victim_controller(self):
        for owner in (0,1):
            for mode,controller in (('own_fungus',owner),('enemy_fungus',1-owner)):
                steps=self.world('Savage Thallid',mode,owner)['steps']
                count=0
                checkpoints=[]
                for s in steps:
                    if s['op']=='upkeep_step' and s['player']==controller:
                        count+=1
                    if s['op']=='assert_card' and s['ref']=='victim' and s['property'] in ('power','toughness'):
                        printed = 6 if s['property']=='power' else 3
                        self.assertEqual(s['value'],printed+count)
                        checkpoints.append(count)
                self.assertEqual(min(checkpoints),0)
                self.assertEqual(max(checkpoints),3)


if __name__ == '__main__':
    unittest.main()
