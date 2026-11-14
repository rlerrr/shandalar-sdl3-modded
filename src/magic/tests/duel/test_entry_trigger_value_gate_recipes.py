"""Real payment, artifact-count, selection, source destination and replay guards."""
import copy
import json
import unittest
from pathlib import Path

from generate_cases import OPS,PROPERTIES,registry
import entry_trigger_value_recipes as dispatch
import entry_trigger_value_gate_recipes as gate


class EntryGateTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.oracle=json.loads((Path(__file__).parent/'data/oracle.json').read_text(encoding='utf8'))['cards']
        cls.rows={row['name']:row for row in registry()}

    def declaration(self,name):
        profile=gate.pins()['sources'][name]
        return dict(family=gate.FAMILY,version=2,oracle_text=self.oracle[name]['oracle_text'],variants=['white_0' if name=='Court Hussar' else 'count0_0'],
                    params=dict(schema=4,mechanism=copy.deepcopy(profile['mechanism']),presentation=copy.deepcopy(profile['presentation'])))

    def parameters(self,name):return dispatch.spec(self.declaration(name),self.oracle[name])

    def cases(self,name,variants=None):
        d=self.declaration(name);p=dispatch.spec(d,self.oracle[name]);selected=variants or dispatch.variants(p);d['variants']=selected
        return dispatch.expand(name,self.rows[name]['csvid'],d,p,selected,self.rows,self.oracle)

    def test_both_sources_expand_valid_operations_and_whole_clauses(self):
        total=0
        for name in gate.pins()['sources']:
            cases=self.cases(name);total+=len(cases)
            self.assertEqual({int(c['_recipe']['variant'][-1]) for c in cases},{0,1})
            for c in cases:
                self.assertEqual(c['_recipe']['oracle_text'],self.oracle[name]['oracle_text'])
                for line in self.parameters(name)['gaps']:self.assertIn(line,c['_recipe']['untested_components'])
                for s in c['steps']:
                    self.assertIn(s['op'],OPS)
                    if 'property' in s:self.assertIn(s['property'],PROPERTIES)
        self.assertEqual(total,92)

    def test_source_types_costs_and_conditions_are_whole_oracle_pinned(self):
        for name in gate.pins()['sources']:
            d=self.declaration(name)
            for key,value in [('oracle_text',self.oracle[name]['oracle_text']+'\nDraw a card.'),('type_line','Artifact Creature'),('mana_cost','{1}'),('layout','split')]:
                r=dict(self.oracle[name],**{key:value})
                with self.assertRaises(ValueError):dispatch.spec(dict(d,oracle_text=r['oracle_text']),r)
            changed=copy.deepcopy(d);changed['params']['mechanism']['condition_clause']='unreviewed condition'
            with self.assertRaises(ValueError):dispatch.spec(changed,self.oracle[name])
        self.assertEqual(self.parameters('Court Hussar')['mana'],[2,0,1,0,0,0])
        self.assertEqual(self.parameters('Chromescale Drake')['mana'],[6,0,3,0,0,0])
        self.assertNotIn('Artifact',self.oracle['Chromescale Drake']['type_line'].partition(' — ')[0].split())

    def test_court_tests_both_entry_clauses_instead_of_dropping_sacrifice(self):
        p=self.parameters('Court Hussar')
        self.assertIn('put one of them into your hand',p['selection_clause'])
        self.assertEqual(p['condition_clause'],'When this creature enters, sacrifice it unless {W} was spent to cast it.')
        self.assertNotIn(p['condition_clause'],p['gaps'])
        self.assertEqual(p['gaps'],['Vigilance'])

    def test_court_real_white_and_no_white_payment_menus_precede_cast(self):
        for owner in (0,1):
            for mode,white in [('white',True),('no_white',False)]:
                s=self.cases('Court Hussar',[f'{mode}_{owner}'])[0]['steps'];cast=next(i for i,x in enumerate(s) if x['op']=='cast')
                expected=[1,0,1,0,0,1] if white else [2,0,1,0,0,0]
                for color,amount in enumerate(expected):self.assertIn(dict(op='set_mana',player=owner,color=color,value=amount),s[:cast])
                for color in range(6):self.assertIn(dict(op='assert_mana',player=owner,color=color,value=0),s[cast:])
                dialogs=[(i,x) for i,x in enumerate(s) if x['op']=='choose_dialog']
                self.assertEqual(len(dialogs),int(white and owner==0))
                if dialogs:self.assertLess(dialogs[0][0],cast);self.assertEqual(dialogs[0][1]['value'],1)
                self.assertIn(dict(op='assert_card',ref='source',property='in_play',value=int(white)),s)
                self.assertIn(dict(op='assert_zone_multiplicity',player=owner,zone='graveyard',index=self.rows['Court Hussar']['csvid'],value=int(not white)),s)

    def test_court_spent_white_is_distinct_from_available_white_or_menu_choice(self):
        spent=self.cases('Court Hussar',['generic_white_0'])[0]['steps'];unspent=self.cases('Court Hussar',['white_unspent_0'])[0]['steps']
        for s in [spent,unspent]:
            cast=next(i for i,x in enumerate(s) if x['op']=='cast')
            self.assertIn(dict(op='choose_dialog',player=0,value=0),s[:cast])
        self.assertIn(dict(op='choose_mana',player=0,color=5),spent)
        self.assertFalse(any(x['op']=='choose_mana' for x in unspent))
        self.assertIn(dict(op='assert_mana',player=0,color=5,value=0),spent)
        self.assertIn(dict(op='assert_mana',player=0,color=5,value=1),unspent)
        self.assertIn(dict(op='assert_card',ref='source',property='in_play',value=1),spent)
        self.assertIn(dict(op='assert_card',ref='source',property='in_play',value=0),unspent)
        self.assertNotIn('generic_white_1',dispatch.variants(self.parameters('Court Hussar')))
        self.assertNotIn('white_unspent_1',dispatch.variants(self.parameters('Court Hussar')))

    def test_court_empty_and_partial_windows_keep_faithful_selection_and_sacrifice(self):
        for owner in (0,1):
            for mode,selected,remaining in [('empty_library',0,0),('one_card_library',1,0),('short_library',1,1),('no_white_empty',0,0),('no_white_short',1,1)]:
                c=self.cases('Court Hussar',[f'{mode}_{owner}'])[0];s=c['steps'];cast=next(i for i,x in enumerate(s) if x['op']=='cast');resolve=next(i for i,x in enumerate(s) if x['op']=='resolve')
                self.assertEqual(sum(x['op']=='choose_zone_card' for x in s[cast:resolve]),selected)
                self.assertIn(dict(op='assert_zone',player=owner,zone='hand',value=2+selected),s[resolve:])
                self.assertIn(dict(op='assert_zone',player=owner,zone='library',value=remaining),s[resolve:])
                self.assertIn(dict(op='assert_zone_multiplicity',player=owner,zone='graveyard',index=self.rows['Court Hussar']['csvid'],value=int(mode.startswith('no_white'))),s[resolve:])

    def test_court_real_blink_does_not_inherit_cast_payment_and_recast_can_change_it(self):
        for owner in (0,1):
            for mode,spell in [('blink_uncast','blink'),('bounce_recast','bounce')]:
                c=self.cases('Court Hussar',[f'{mode}_{owner}'])[0];s=c['steps']
                self.assertIn(dict(op='cast',ref=spell,value=1),s)
                self.assertIn(dict(op='choose_target',player=owner,ref='source'),s)
                self.assertIn(dict(op='assert_zone_multiplicity',player=owner,zone='graveyard',index=self.rows['Court Hussar']['csvid'],value=1),s)
                if mode=='bounce_recast':self.assertEqual(sum(x['op']=='cast' and x.get('ref')=='source' for x in s),2)

    def test_drake_affinity_artifacts_are_real_casts_with_exact_uuu_cost_floor(self):
        for owner in (0,1):
            for count in [0,1,2,6,7]:
                c=self.cases('Chromescale Drake',[f'count{count}_{owner}'])[0];s=c['steps'];source=next(i for i,x in enumerate(s) if x['op']=='cast' and x['ref']=='source')
                artifacts=[x for x in c['setup'] if x['ref'].startswith('affinity_')]
                self.assertEqual(len(artifacts),count)
                self.assertTrue(all(x['zone']=='hand' and 'state' not in x for x in artifacts))
                for item in artifacts:
                    i=next(i for i,x in enumerate(s) if x['op']=='cast' and x['ref']==item['ref'])
                    self.assertLess(i,source)
                    self.assertTrue(any(x['op']=='resolve' for x in s[i+1:source]))
                vector={x['color']:x['value'] for x in s[:source] if x['op']=='set_mana' and x['player']==owner}
                self.assertEqual(vector,{0:max(0,6-count),1:0,2:3,3:0,4:0,5:0})
                for color in range(6):self.assertIn(dict(op='assert_mana',player=owner,color=color,value=0),s[source:])

    def test_drake_does_not_count_other_controller_off_battlefield_or_nonartifacts(self):
        for owner in (0,1):
            for mode in ['opponent_artifacts','artifact_in_hand','artifact_in_grave','nonartifact']:
                s=self.cases('Chromescale Drake',[f'{mode}_{owner}'])[0]['steps'];source=next(i for i,x in enumerate(s) if x['op']=='cast' and x['ref']=='source')
                vector={x['color']:x['value'] for x in s[:source] if x['op']=='set_mana' and x['player']==owner}
                self.assertEqual(vector,{0:6,1:0,2:3,3:0,4:0,5:0})
            c=self.cases('Chromescale Drake',[f'artifact_creature_{owner}'])[0]
            self.assertEqual([x['name'] for x in c['setup'] if x['ref'].startswith('affinity_')],['Darksteel Myr'])

    def test_drake_public_done_is_human0_for_both_controllers_and_absent_when_empty(self):
        for owner in (0,1):
            s=self.cases('Chromescale Drake',[f'count2_{owner}'])[0]['steps'];source=next(i for i,x in enumerate(s) if x['op']=='cast' and x['ref']=='source')
            done=next(i for i,x in enumerate(s[source:],source) if x['op']=='choose_zone')
            resolve=next(i for i,x in enumerate(s[source:],source) if x['op']=='resolve')
            self.assertEqual(s[done],dict(op='choose_zone',player=0,value=-1));self.assertLess(source,done);self.assertLess(done,resolve)
            self.assertFalse(any(x['op']=='choose_zone_card' for x in s[source:resolve]))
            empty=self.cases('Chromescale Drake',[f'empty_library_{owner}'])[0]['steps']
            self.assertFalse(any(x['op']=='choose_zone' for x in empty))

    def test_drake_all_artifacts_go_to_hand_and_nonartifact_goes_to_grave(self):
        for owner in (0,1):
            c=self.cases('Chromescale Drake',[f'count2_{owner}'])[0];s=c['steps']
            for name in ['Darksteel Relic','Darksteel Myr']:
                self.assertIn(dict(op='assert_zone_multiplicity',player=owner,zone='hand',index=self.rows[name]['csvid'],value=1),s)
            self.assertIn(dict(op='assert_zone_multiplicity',player=owner,zone='graveyard',index=self.rows['Forest']['csvid'],value=1),s)
            self.assertIn(dict(op='assert_zone',player=owner,zone='library',value=4),s)

    def test_drake_lost_artifact_changes_future_price_without_refunding_paid_price(self):
        for owner in (0,1):
            before=self.cases('Chromescale Drake',[f'removed_before_{owner}'])[0]['steps'];source=next(i for i,x in enumerate(before) if x['op']=='cast' and x['ref']=='source')
            latest={x['color']:x['value'] for x in before[:source] if x['op']=='set_mana' and x['player']==owner}
            self.assertEqual(latest,{0:5,1:0,2:3,3:0,4:0,5:0})
            after=self.cases('Chromescale Drake',[f'removed_after_{owner}'])[0]['steps'];ops=[x['op'] for x in after]
            self.assertLess(ops.index('announce_spell'),ops.index('cast_response'));self.assertLess(ops.index('cast_response'),ops.index('resolve_response'))
            self.assertIn(dict(op='assert_mana',player=owner,color=0,value=0),after)
            self.assertFalse(any(x['op']=='set_mana' for x in after[ops.index('cast_response'):]))

    def test_counter_responses_use_real_pending_spell_context_and_no_entry_effect(self):
        for name in gate.pins()['sources']:
            for owner in (0,1):
                s=self.cases(name,[f'countered_{owner}'])[0]['steps'];ops=[x['op'] for x in s]
                self.assertLess(ops.index('cast'),ops.index('announce_spell'));self.assertLess(ops.index('announce_spell'),ops.index('cast_response'))
                self.assertFalse(any(x['op']=='choose_target' for x in s))
                self.assertFalse(any(x['op'].startswith('choose_zone') for x in s))
                self.assertIn(dict(op='assert_zone_multiplicity',player=owner,zone='graveyard',index=self.rows[name]['csvid'],value=1),s)

    def test_gate_snapshots_repeat_exact_payment_selection_and_receipt_tails(self):
        for name in gate.pins()['sources']:
            for owner in (0,1):
                for mode in ['ready_snapshot','pending_snapshot','resolved_snapshot']:
                    s=self.cases(name,[f'{mode}_{owner}'])[0]['steps'];ops=[x['op'] for x in s]
                    a=ops.index('capture');b=ops.index('record_state');r=ops.index('restore');end=ops.index('assert_recorded_state')
                    self.assertEqual(s[a+1:b],s[r+1:end])
                    if mode!='resolved_snapshot':self.assertIsNot(s[a+1],s[r+1])

    def test_no_synthetic_payment_artifact_or_ready_fields_are_authored(self):
        for name in gate.pins()['sources']:
            for c in self.cases(name):
                self.assertTrue(all('state' not in item for item in c['setup']))
                for s in c['steps']:
                    self.assertNotIn(s['op'],['set_mana_paid','set_card_ready','set_artifact_count'])
                    self.assertNotIn(s.get('property'),['mana_paid','info_slot','targets','ready'])

    def test_full_witness_pins_and_normal_implementations_are_required(self):
        gate.validate_witnesses(self.oracle,self.rows)
        rows=copy.deepcopy(self.rows);rows['Darksteel Myr']['handler']='card_dummy'
        with self.assertRaises(ValueError):gate.validate_witnesses(self.oracle,rows)
        oracle=copy.deepcopy(self.oracle);oracle['Darksteel Relic']['type_line']='Enchantment'
        with self.assertRaises(ValueError):gate.validate_witnesses(oracle,self.rows)


if __name__=='__main__':unittest.main()
