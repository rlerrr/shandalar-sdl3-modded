"""Independent expectations and fail-closed scope for continuous boosts."""
import copy
import json
import unittest
from pathlib import Path
from generate_cases import registry
from recipes import explicit_spec, expand
from lord_creature_recipes import variants, validate_witnesses


class LordCreatureRecipesTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.oracle=json.loads((Path(__file__).parent/'data/oracle.json').read_text(encoding='utf8'))['cards']
        cls.rows={r['name']:r for r in registry()}

    def declaration(self,name='Battle Sliver',selected=None,**changes):
        params=dict(keywords=[],predicate='subtype',value='Sliver',scope='own',other=False,power=2,toughness=0)
        params.update(changes)
        return dict(family='lord_creature',version=2,params=params,oracle_text=self.oracle[name]['oracle_text'],variants=selected or ['static_0'])

    def cases(self,name,d,oracle=None):
        return expand(name,self.rows[name]['csvid'],d,self.oracle[name],self.rows,oracle_context=oracle or self.oracle)

    def test_parameters_are_explicit_and_pinned(self):
        d=self.declaration()
        for change in (dict(oracle_text='changed'),dict(params={}),dict(params=dict(d['params'],extra=True)),dict(variants=['unknown'])):
            with self.assertRaises(ValueError):self.cases('Battle Sliver',dict(d,**change))

    def test_scope_and_self_exclusion_cannot_be_inferred(self):
        for p in (dict(scope='both'),dict(other=True),dict(value='Myr'),dict(predicate='color',value='blue')):
            with self.assertRaises(ValueError):self.cases('Battle Sliver',self.declaration(**p))

    def test_integer_positive_bounds(self):
        for change in (dict(power=True),dict(power=2.0),dict(power=-1),dict(power=4),dict(other=1),dict(power=0,toughness=0)):
            with self.assertRaises(ValueError):self.cases('Battle Sliver',self.declaration(**change))

    def test_card_characteristics_are_fail_closed(self):
        for change in (dict(power='*'),dict(toughness='0'),dict(layout='transform'),dict(card_faces=[{}]),dict(mana_cost='{R/G}')):
            with self.assertRaises(ValueError):explicit_spec(self.declaration(),dict(self.oracle['Battle Sliver'],**change))

    def test_extra_trigger_or_passive_clause_rejected(self):
        for extra in ('\nOther Slivers have haste.','\nWhenever this creature attacks, draw a card.','\nSliver creatures get +1/+1.'):
            r=dict(self.oracle['Battle Sliver'],oracle_text=self.oracle['Battle Sliver']['oracle_text']+extra)
            with self.assertRaises(ValueError):explicit_spec(dict(self.declaration(),oracle_text=r['oracle_text']),r)

    def test_extra_activated_ability_is_visible_gap(self):
        c=self.cases('Elvish Archdruid',self.declaration('Elvish Archdruid',value='Elf',other=True,power=1,toughness=1))[0]
        self.assertEqual(c['_recipe']['untested_abilities'],['{T}: Add {G} for each Elf you control.'])
        self.assertFalse(any(s['op']=='activate' for s in c['steps']))

    def test_source_keywords_are_visible_component_gaps(self):
        c=self.cases('Battleground Geist',self.declaration('Battleground Geist',value='Spirit',other=True,power=1,keywords=['Flying']))[0]
        self.assertEqual(c['_recipe']['untested_keywords'],['Flying'])

    def test_witness_characteristics_are_pinned(self):
        for field,value in (('colors',['R']),('type_line','Creature — Myr'),('power','8'),('oracle_text','Draw a card.')):
            o=copy.deepcopy(self.oracle);o['Metallic Sliver'][field]=value
            with self.assertRaises(ValueError):validate_witnesses(o,self.rows)

    def test_spell_expectations_are_pinned(self):
        o=copy.deepcopy(self.oracle);o['Giant Growth']['oracle_text']='Target creature gets +4/+4 until end of turn.'
        with self.assertRaises(ValueError):validate_witnesses(o,self.rows)

    def test_owned_scope_self_and_opponent_stats(self):
        steps=self.cases('Battle Sliver',self.declaration())[0]['steps']
        for ref,power in (('source',5),('own_match',3),('opponent_match',1),('own_other',3),('opponent_other',3)):
            self.assertIn(dict(op='assert_card',ref=ref,property='power',value=power),steps)

    def test_global_scope_includes_opposing_witness(self):
        steps=self.cases('Muscle Sliver',self.declaration('Muscle Sliver',scope='both',power=1,toughness=1))[0]['steps']
        self.assertIn(dict(op='assert_card',ref='opponent_match',property='power',value=2),steps)

    def test_other_excludes_source_but_stacks_from_second(self):
        d=self.declaration('Chief of the Edge',['static_0','two_sources_0'],value='Warrior',other=True,power=1)
        one,two=self.cases('Chief of the Edge',d)
        self.assertIn(dict(op='assert_card',ref='source',property='power',value=3),one['steps'])
        self.assertIn(dict(op='assert_card',ref='source',property='power',value=4),two['steps'])
        self.assertIn(dict(op='assert_card',ref='own_match',property='power',value=4),two['steps'])

    def test_all_predicate_boosts_both_owned_witness_types(self):
        steps=self.cases('Veteran Armorer',self.declaration('Veteran Armorer',predicate='all',value='',other=True,power=0,toughness=1))[0]['steps']
        for ref,n in (('own_match',3),('own_other',4),('opponent_match',2),('opponent_other',3)):
            self.assertIn(dict(op='assert_card',ref=ref,property='toughness',value=n),steps)

    def test_color_and_artifact_predicates_have_negative_witness(self):
        for name,args in [('Paragon of Gathering Mists',dict(predicate='color',value='blue',other=True,power=1,toughness=1)),
                          ('Chief of the Foundry',dict(predicate='artifact',value='',other=True,power=1,toughness=1))]:
            c=self.cases(name,self.declaration(name,**args))[0]
            self.assertEqual(len([s for s in c['setup'] if s['ref'].endswith('_other')]),2)

    def test_legendary_duplicate_owned_sources_excluded(self):
        p=explicit_spec(self.declaration('Kaysa',predicate='color',value='green',power=1,toughness=1),self.oracle['Kaysa'])
        self.assertNotIn('two_sources_0',variants(p));self.assertIn('opposing_source_1',variants(p))

    def test_bounce_rebinds_returned_source(self):
        steps=self.cases('Battle Sliver',self.declaration(selected=['bounce_recast_1']))[0]['steps']
        bind=next(i for i,s in enumerate(steps) if s['op']=='bind_hand')
        cast=next(i for i,s in enumerate(steps) if s['op']=='cast' and s['ref']=='source')
        self.assertLess(bind,cast)
        self.assertIn(dict(op='assert_card',ref='own_match',property='power',value=1),steps[bind:cast])

    def test_cleanup_removes_temporary_pump_only(self):
        steps=self.cases('Battle Sliver',self.declaration(selected=['pump_0']))[0]['steps'];cut=next(i for i,s in enumerate(steps) if s['op']=='cleanup')
        self.assertIn(dict(op='assert_card',ref='own_match',property='power',value=6),steps[:cut])
        self.assertIn(dict(op='assert_card',ref='own_match',property='power',value=3),steps[cut:])

    def test_blink_rebinds_the_new_permanent(self):
        for c in self.cases('Battle Sliver',self.declaration(selected=['blink_source_1','blink_witness_0'])):
            ops=[s['op'] for s in c['steps']]
            self.assertLess(ops.index('resolve'),ops.index('bind_permanent'))

    def test_cast_bonus_waits_for_resolution(self):
        steps=self.cases('Battle Sliver',self.declaration(selected=['cast_0']))[0]['steps'];cut=next(i for i,s in enumerate(steps) if s['op']=='resolve')
        self.assertIn(dict(op='assert_card',ref='own_match',property='power',value=1),steps[:cut])
        self.assertIn(dict(op='assert_card',ref='own_match',property='power',value=3),steps[cut:])

    def test_snapshot_replays_real_combat_after_ai(self):
        steps=self.cases('Battle Sliver',self.declaration(selected=['combat_snapshot_0']))[0]['steps'];ops=[s['op'] for s in steps]
        self.assertEqual(ops.count('combat_damage'),2)
        self.assertLess(ops.index('advance_combat'),ops.index('capture'))
        self.assertIn('ai_roundtrip',ops);self.assertEqual(ops[-1],'assert_recorded_state')

    def test_zones_and_empty_variant_are_deliberate(self):
        full,empty=self.cases('Battle Sliver',self.declaration(selected=['static_0','empty_zones_0']))
        self.assertEqual(full['fixture'],'stocked_zones');self.assertEqual(empty['fixture'],'empty_main')
        for player in (0,1):
            self.assertIn(dict(op='assert_zone',player=player,zone='library',value=7),full['steps'])
            self.assertIn(dict(op='assert_zone',player=player,zone='library',value=0),empty['steps'])

    def test_exact_payment_and_illegal_cast_preserve_board(self):
        cases=self.cases('Battle Sliver',self.declaration(selected=['payment','insufficient_mana','wrong_color']))
        for c in cases[1:]:
            self.assertIn(dict(op='cast',ref='source',value=0),c['steps'])
            self.assertIn(dict(op='assert_card',ref='own_match',property='power',value=1),c['steps'])

    def test_wrong_color_has_no_alternative_thrull_mana(self):
        c=self.cases('Thrull Champion',self.declaration('Thrull Champion',selected=['wrong_color'],scope='both',value='Thrull',power=1,toughness=1))[0]
        self.assertEqual([s['state'] for s in c['setup'] if s['name']=='Basal Thrull'],[['tapped'],['tapped']])


if __name__=='__main__':unittest.main()
