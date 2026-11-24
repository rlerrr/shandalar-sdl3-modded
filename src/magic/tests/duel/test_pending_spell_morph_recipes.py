"""Check paid real pending histories, typed turn-ups and cross-owner exile."""
import json
import unittest

import pending_spell_morph_recipes as model
from generate_cases import registry


class PendingSpellMorphTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.oracle=json.loads((model.PROFILE.parent/'oracle.json').read_text(encoding='utf8'))['cards']
        cls.rows={row['name']:row for row in registry()}

    def case(self,name,variant):
        oracle=self.oracle[name]
        shape=model.parameters(oracle)
        decl=dict(family=model.FAMILY,version=2,oracle_text=oracle['oracle_text'],params=shape,variants=[variant])
        return model.expand(name,self.rows[name]['csvid'],decl,model.spec(decl,oracle),[variant],self.rows,self.oracle)[0]

    @staticmethod
    def values(steps,op,**keys):
        return [s['value'] for s in steps if s['op']==op and all(s.get(k)==v for k,v in keys.items())]

    def test_complete_pending_printed_clauses_and_profile(self):
        model.validate_witnesses(self.oracle,self.rows)
        self.assertEqual({r['registry']['csvid'] for r in model.profiles()['sources'].values()},{6457,14305,15031,15040})
        for name,source in model.profiles()['sources'].items():
            record=self.oracle[name]
            self.assertEqual(source['oracle'],record)
            with self.assertRaises(ValueError):
                model.parameters(dict(record,oracle_text=record['oracle_text']+'\nDraw a card.'))

    def test_real_hidden_cost_and_short_pending_special_action(self):
        for name in model.profiles()['sources']:
            for seat in (0,1):
                steps=self.case(name,f'short_flip_{seat}')['steps']
                self.assertIn(dict(op='set_mana',player=seat,color=0,value=3),steps[:25])
                hidden=steps.index(dict(op='cast',ref='source',value=1))
                self.assertEqual(steps[hidden-1],dict(op='choose_dialog',player=seat,value=0))
                remaining=self.values(steps[hidden:],'assert_mana',player=seat,color=2)
                self.assertEqual(remaining[0],model.parameters(self.oracle[name])['cast_mana'][2])
                ordinary=self.case(name,f'ordinary_{seat}')['steps']
                normal=ordinary.index(dict(op='cast',ref='source',value=1))
                self.assertEqual(ordinary[normal-1],dict(op='choose_dialog',player=seat,value=1))
                self.assertEqual(self.values(ordinary[normal:],'assert_mana',player=seat,color=2),[0])
                self.assertIn(dict(op='activate_response',ref='source',value=0),steps)
                self.assertEqual(set(self.values(steps,'assert_card',ref='source',property='csvid')),{13252})
                self.assertEqual(self.values(steps,'assert_life',player=seat)[-1],17)
                no_spell=self.case(name,f'no_spell_{seat}')['steps']
                self.assertIn(dict(op='activate',ref='source',value=1),no_spell)
                self.assertFalse(any(s['op']=='activate_response' for s in no_spell))
                self.assertEqual(self.values(no_spell,'assert_card',ref='source',property='plus1_counters')[-1],
                                 int(model.parameters(self.oracle[name])['megamorph']))

    def test_announced_actual_parent_precedes_native_activation(self):
        for name in model.profiles()['sources']:
            for owner in (0,1):
                steps=self.case(name,f'instant_{owner}')['steps']
                announce=steps.index(dict(op='announce_spell',ref='parent'))
                flip=steps.index(dict(op='activate_response',ref='source',value=1))
                self.assertIn(dict(op='assert_stack',value=1),steps[announce:flip])
                self.assertIn(dict(op='assert_card',ref='parent',property='in_hand',value=0),steps[announce:flip])
                self.assertFalse(any(s['op']=='resolve' for s in steps[announce:flip]))
                self.assertEqual(self.values(steps,'assert_life',player=owner)[-1],20)

    def test_tax_actual_payer_short_pay_and_decline(self):
        for owner in (0,1):
            for mode,prompt,remaining,life in [('tax_paid',[0],0,17),('tax_declined',[1],4,20),('tax_short',[],2,20)]:
                steps=self.case('Silumgar Spell-Eater',f'{mode}_{owner}')['steps']
                self.assertEqual(self.values(steps,'choose_dialog',player=1-owner),prompt)
                self.assertEqual(self.values(steps,'assert_mana',player=1-owner,color=0)[-1],remaining)
                self.assertEqual(self.values(steps,'assert_life',player=owner)[-1],life)

    def test_typed_permanent_survives_and_flying_changes(self):
        for owner in (0,1):
            name='Stratus Dancer'
            permanent=self.case(name,f'permanent_{owner}')['steps']
            self.assertIn(dict(op='assert_card',ref='parent',property='in_play',value=1),permanent)
            self.assertEqual(self.values(permanent,'assert_card',ref='source',property='plus1_counters')[-1],1)
            flying=self.case(name,f'flying_{owner}')['steps']
            self.assertEqual(self.values(flying,'block',ref='ground',other='source'),[0])
            self.assertEqual(self.values(flying,'block',ref='reach',other='source'),[1])
            hidden=self.case(name,f'hidden_ground_{owner}')['steps']
            self.assertEqual(self.values(hidden,'block',ref='ground',other='source'),[1])

    def test_free_cross_owner_cast_has_exact_exile_and_grave_receipts(self):
        for owner in (0,1):
            steps=self.case('Kheru Spellsnatcher',f'play_foreign_{owner}')['steps']
            same=dict(op='play_exiled',player=owner,owner=1-owner,index=11865,value=1)
            pos=steps.index(same)
            self.assertIn(dict(op='assert_zone_multiplicity',player=1-owner,zone='exile',index=11865,value=1),steps[:pos])
            self.assertIn(dict(op='assert_zone_multiplicity',player=1-owner,zone='graveyard',index=11865,value=1),steps[pos:])
            self.assertIn(dict(same,value=0),steps[pos:])
            self.assertEqual(set(self.values(steps[pos:],'assert_mana',player=owner)),{0})
            top=91 if owner==0 else 188
            self.assertEqual(self.values(steps,'assert_zone_multiplicity',player=owner,zone='hand',index=top)[-1],1)
            self.assertEqual(self.values(steps,'assert_zone',player=owner,zone='library')[-1],2)
            self.assertEqual(self.values(steps,'assert_zone',player=1-owner,zone='library')[-1],3)
            own=self.case('Kheru Spellsnatcher',f'play_own_{owner}')['steps']
            self.assertIn(dict(op='play_exiled',player=owner,index=11865,value=1),own)

    def test_free_creature_timing_and_permission_survives_bounce(self):
        for owner in (0,1):
            steps=self.case('Kheru Spellsnatcher',f'creature_offturn_{owner}')['steps']
            index=self.rows['Crater Elemental']['csvid']
            deny=steps.index(dict(op='play_exiled',player=owner,owner=1-owner,index=index,value=0))
            paid=steps.index(dict(op='play_exiled',player=owner,owner=1-owner,index=index,value=1))
            self.assertTrue(any(s['op']=='advance_turn' for s in steps[deny:paid]))
            self.assertIn(dict(op='bind_permanent',ref='parent',player=owner,value=index),steps[paid:])
            keep=self.case('Kheru Spellsnatcher',f'permission_persists_{owner}')['steps']
            self.assertLess(keep.index(dict(op='bind_hand',ref='source',player=owner,value=14305)),
                            keep.index(dict(op='play_exiled',player=owner,owner=1-owner,index=11865,value=1)))

    def test_replay_surrounds_real_parent_or_real_exile(self):
        for name in model.profiles()['sources']:
            steps=self.case(name,'snapshot_1')['steps']
            self.assertLess(steps.index(dict(op='announce_spell',ref='parent')),steps.index(dict(op='capture')))
            self.assertEqual(steps[-1],dict(op='assert_recorded_state'))
        stolen=self.case('Kheru Spellsnatcher','play_replay_0')['steps']
        self.assertLess(stolen.index(dict(op='capture')),stolen.index(dict(op='play_exiled',player=0,owner=1,index=11865,value=1)))
        self.assertEqual(stolen[-1],dict(op='assert_recorded_state'))
