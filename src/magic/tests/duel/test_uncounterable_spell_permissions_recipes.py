"""Focused guards for paid targeting and independent counter outcomes."""

from copy import deepcopy
import unittest

import uncounterable_spell_permissions_recipes as model


class SpellPermissionGuards(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        data=model.profile()
        pins=data['sources']|data['witnesses']
        cls.oracle={name:pin['oracle'] for name,pin in pins.items()}
        cls.rows={name:pin['registry'] for name,pin in pins.items()}

    def case(self,name,variant):
        printed=self.oracle[name]
        params=model.parameters(printed)
        declaration=dict(family=model.FAMILY,version=2,oracle_text=printed['oracle_text'],params=params,variants=[variant])
        return model.expand(name,self.rows[name]['csvid'],declaration,model.spec(declaration,printed),
                            [variant],self.rows,self.oracle)[0]

    def test_whole_oracle_requires_other_only_trample(self):
        printed=deepcopy(self.oracle['Surrak Dragonclaw'])
        printed['oracle_text']=printed['oracle_text'].replace('Other creatures','Creatures')
        with self.assertRaises(ValueError):
            model.parameters(printed)

    def test_exact_decree_production_witness(self):
        rows=deepcopy(self.rows)
        rows[model.DECREE]['handler']='card_dummy'
        with self.assertRaises(ValueError):
            model.validate_witnesses(self.oracle,rows)

    def test_paid_targeting_experiment_uses_actual_legal_stack(self):
        for seat in (0,1):
            steps=self.case('Root Sliver',f'paid_plated_{seat}')['steps']
            ops=[step['op'] for step in steps]
            self.assertLess(ops.index('announce_spell'),ops.index('capture'))
            self.assertLess(ops.index('cast_response'),ops.index('restore'))
            self.assertIn(dict(op='cast_response',ref='counter',value=1),steps)
            self.assertIn(dict(op='assert_stack',value=2),steps[ops.index('cast_response'):ops.index('restore')])
            self.assertIn(dict(op='set_mana',player=1-seat,color=0,value=2),steps[ops.index('capture'):ops.index('restore')])
            self.assertNotIn('resolve_response',ops)

    def test_source_removed_has_real_bounce_then_fully_resolved_exclude(self):
        steps=self.case('Prowling Serpopard','source_removed_1')['steps']
        chain=[step['op'] for step in steps]
        self.assertLess(chain.index('bind_hand'),chain.index('announce_spell'))
        self.assertIn(dict(op='choose_target',player=1,ref='source'),steps)
        self.assertIn(dict(op='resolve_response'),steps)
        self.assertIn(dict(op='assert_zone_multiplicity',player=1,zone='graveyard',
                           index=self.rows[model.NORMAL]['csvid'],value=1),steps)

    def test_root_bilateral_sliver_and_kindred_are_real_casts(self):
        for mode,actor in [('decree_plated',0),('decree_opposing_plated',1)]:
            steps=self.case('Root Sliver',mode+'_0')['steps']
            self.assertIn(dict(op='cast',ref='subject',value=1),steps)
            self.assertIn(dict(op='assert_counter',ref='decree',counter_kind='depletion',value=1),steps)
            self.assertIn(dict(op='assert_zone_multiplicity',player=actor,zone='battlefield',
                               index=self.rows[model.SLIVER]['csvid'],value=1),steps)
        case=self.case('Root Sliver','decree_opposing_kindred_0')
        self.assertIn(dict(op='choose_target',player=1,ref='victim'),case['steps'])
        self.assertIn(dict(op='assert_zone_multiplicity',player=0,zone='graveyard',
                           index=self.rows['Hill Giant']['csvid'],value=1),case['steps'])

    def test_power_four_five_and_opposing_five_are_independent(self):
        for mode,expected,actor,name in [('power_four','graveyard',0,model.VANILLA_FOUR),
                ('power_five','battlefield',0,model.VANILLA_FIVE),('opposing_five','graveyard',1,model.VANILLA_FIVE)]:
            steps=self.case('Spellbreaker Behemoth','decree_'+mode+'_0')['steps']
            self.assertIn(dict(op='assert_zone_multiplicity',player=actor,zone=expected,index=self.rows[name]['csvid'],value=1),steps)
            self.assertIn(dict(op='set_mana',player=actor,color=0,value=4),steps)

    def test_surrak_paid_flash_and_other_own_trample_only(self):
        flash=self.case('Surrak Dragonclaw','flash_response_1')['steps']
        self.assertEqual(flash[0],dict(op='turn',player=0,value=0))
        self.assertIn(dict(op='cast_response',ref='source',value=1),flash)
        for mode,who,life in [('trample_own',0,19),('trample_self',0,20),('trample_enemy',1,20)]:
            steps=self.case('Surrak Dragonclaw',mode+'_1')['steps']
            self.assertIn(dict(op='block',ref='blocker',other='source' if mode=='trample_self' else 'attacker',value=1),steps)
            self.assertIn(dict(op='assert_life',player=who,value=life),steps)

    def test_sphinx_type_and_riders_have_actual_contrasts(self):
        name='Sphinx of the Final Word'
        self.assertTrue(model.grants('spells',self.oracle[model.SORCERY],True))
        self.assertFalse(model.grants('spells',self.oracle[model.FLASH],True))
        self.assertIn(dict(op='cast_graveyard',ref='subject',value=1),self.case(name,'decree_flashback_1')['steps'])
        for mode,allowed in [('own_spell',1),('foe_spell',0)]:
            self.assertIn(dict(op='can_target_creature',ref='targeter',other='source',value=allowed),self.case(name,mode+'_0')['steps'])
        for mode,legal in [('flying',0),('reach',1)]:
            self.assertIn(dict(op='block',ref='blocker',other='source',value=legal),self.case(name,mode+'_1')['steps'])


if __name__=='__main__':
    unittest.main()
