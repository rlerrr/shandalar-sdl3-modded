import copy
import json
import unittest
from pathlib import Path
from generate_cases import registry
from recipes import expand,explicit_spec
from library_search_recipes import eligible,variants,PINS


class LibrarySearchRecipesTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.oracle=json.loads((Path(__file__).parent/'data/oracle.json').read_text(encoding='utf8'))['cards'];cls.rows={r['name']:r for r in registry()}

    def declaration(self,name='Goblin Matron',selected=None):
        configs={
          'Goblin Matron':dict(scope='goblin'),
          'Avarax':dict(scope='named',names=['Avarax'],keywords=['Haste']),
          'Blightspeaker':dict(mode='activated',scope='rebel_permanent',limit=dict(field='mana_value',op='le',value=3),destination='battlefield',optional=False,tap=True,activation_cost='{4}'),
          'Trinket Mage':dict(scope='artifact',limit=dict(field='mana_value',op='le',value=1)),
          'Fierce Empath':dict(scope='creature',limit=dict(field='mana_value',op='ge',value=6)),
          'Recruiter of the Guard':dict(scope='creature',limit=dict(field='toughness',op='le',value=2)),
          'Imperial Recruiter':dict(scope='creature',limit=dict(field='power',op='le',value=2),optional=False),
          'Wood Elves':dict(scope='forest',destination='battlefield',optional=False),
          'Enlightened Tutor':dict(mode='spell',scope='artifact_or_enchantment',destination='top',optional=False),
          'Demonic Tutor':dict(mode='spell',scope='card',optional=False),
          'Vampiric Tutor':dict(mode='spell',scope='card',destination='top',optional=False,lose_life=2),
          'Captain Sisay':dict(mode='activated',scope='legendary_card',optional=False,tap=True),
          'Planar Bridge':dict(mode='activated',scope='permanent',destination='battlefield',optional=False,tap=True,activation_cost='{8}'),
          'Thalia\'s Lancers':dict(scope='legendary_card',keywords=['First strike']),
        }
        p=dict(mode='entry',scope='card',limit=None,destination='hand',optional=True,activation_cost='',tap=False,sacrifice=False,pay_life=0,keywords=[],names=[],lose_life=0);p.update(configs[name])
        d=dict(family='library_search',version=2,params=p,oracle_text=self.oracle[name]['oracle_text'],variants=selected or ['search_0'])
        if p['mode']=='entry' and p['optional']:d['trigger_priority']=True
        return d

    def cases(self,name='Goblin Matron',selected=None,d=None,oracle=None):
        return expand(name,self.rows[name]['csvid'],d or self.declaration(name,selected),self.oracle[name],self.rows,oracle_context=oracle or self.oracle)

    def test_full_clause_and_parameter_types_fail_closed(self):
        d=self.declaration()
        for patch in (dict(oracle_text='changed'),dict(params={}),dict(params=dict(d['params'],optional=1)),dict(params=dict(d['params'],scope=[])),dict(params=dict(d['params'],scope='elf')),dict(params=dict(d['params'],keywords={})),dict(params=dict(d['params'],limit=dict(field='power',op='le',value=True)))):
            with self.assertRaises(ValueError):self.cases(d=dict(d,**patch))
        r=dict(self.oracle['Goblin Matron'],oracle_text=d['oracle_text']+'\nWhen this creature dies, draw a card.')
        with self.assertRaises(ValueError):explicit_spec(dict(d,oracle_text=r['oracle_text']),r)

    def test_subtype_search_includes_noncreature_kindred_and_changelings(self):
        p=explicit_spec(self.declaration(),self.oracle['Goblin Matron'])
        for nm in ("Mons's Goblin Raiders",'Tarfire','Crib Swap','Avian Changeling'):self.assertTrue(eligible(self.oracle[nm],p),nm)
        self.assertFalse(eligible(self.oracle['Grizzly Bears'],p))

    def test_mana_limits_are_inclusive_and_intersect_with_type(self):
        p=explicit_spec(self.declaration('Trinket Mage'),self.oracle['Trinket Mage'])
        for nm,allowed in [('Ornithopter',True),('Sol Ring',True),('Darksteel Citadel',True),('Vulshok Morningstar',False),('Giant Growth',False)]:self.assertEqual(eligible(self.oracle[nm],p),allowed,nm)
        p=explicit_spec(self.declaration('Fierce Empath'),self.oracle['Fierce Empath'])
        self.assertTrue(eligible(self.oracle['Phyrexian Hulk'],p));self.assertFalse(eligible(self.oracle['Air Elemental'],p));self.assertFalse(eligible(self.oracle['Gilded Lotus'],p))

    def test_power_and_toughness_limits_are_distinct(self):
        a=explicit_spec(self.declaration('Imperial Recruiter'),self.oracle['Imperial Recruiter']);b=explicit_spec(self.declaration('Recruiter of the Guard'),self.oracle['Recruiter of the Guard'])
        self.assertTrue(eligible(self.oracle['Kraken Hatchling'],a));self.assertFalse(eligible(self.oracle['Kraken Hatchling'],b))

    def test_unrestricted_search_requires_finding_but_quality_search_may_decline(self):
        a=explicit_spec(self.declaration('Demonic Tutor'),self.oracle['Demonic Tutor']);b=explicit_spec(self.declaration(),self.oracle['Goblin Matron'])
        self.assertNotIn('decline_0',variants(a));self.assertNotIn('no_match_0',variants(a));self.assertIn('decline_0',variants(b))
        s=self.cases('Demonic Tutor')[0]['steps'];self.assertIn(dict(op='assert_offered_selection_required',value=1),s)
        self.assertIn(dict(op='assert_offered_selection_required',value=0),self.cases()[0]['steps'])

    def test_top_tutors_preserve_library_quantity_and_put_chosen_card_on_top(self):
        c=self.cases('Enlightened Tutor')[0];s=c['steps']
        self.assertTrue(any(x['op']=='assert_zone_card' and x['player']==0 and x['zone']=='library' and x['index']==0 for x in s))
        self.assertFalse(any(x['op']=='bind_permanent_copy' for x in s))

    def test_spell_life_loss_is_not_an_activation_cost_or_countered_effect(self):
        s=self.cases('Vampiric Tutor')[0]['steps'];self.assertIn(dict(op='assert_life',player=0,value=18),s)
        s=self.cases('Vampiric Tutor',['countered_0'])[0]['steps'];self.assertIn(dict(op='assert_life',player=0,value=20),s)

    def test_entry_tutor_is_cast_and_stays_on_battlefield(self):
        s=self.cases()[0]['steps'];self.assertIn(dict(op='cast',ref='source',value=1),s)
        self.assertIn(dict(op='assert_card',ref='source',property='in_play',value=1),s)
        self.assertIn(dict(op='assert_card',ref='source',property='power',value=1),s)
        self.assertFalse(any(x['op']=='activate' and x['ref']=='source' for x in s))

    def test_reentry_library_copies_are_provisioned_before_execution(self):
        for v in ('bounce_recast_0','blink_1'):
            c=self.cases(selected=[v])[0];source_choices=[x for x in c['steps'] if x['op']=='choose_zone_card']
            self.assertEqual(len(source_choices),2)
            first=source_choices[0]['value'];count=sum(x['zone']=='library' and x['player']==int(v[-1]) and self.rows[x['name']]['csvid']==first for x in c['setup'])
            self.assertGreaterEqual(count,2)

    def test_destroying_source_does_not_remove_the_fetched_hand_card(self):
        s=self.cases(selected=['destroy_source_1'])[0]['steps'];self.assertIn(dict(op='destroy',ref='source'),s)
        self.assertTrue(any(x['op']=='assert_zone_multiplicity' and x['zone']=='graveyard' and x['index']==self.rows['Goblin Matron']['csvid'] and x['value']==1 for x in s))

    def test_namesake_recast_binds_a_copy_when_search_already_filled_hand(self):
        s=self.cases('Avarax',['bounce_recast_0'])[0]['steps']
        self.assertIn(dict(op='bind_hand_copy',player=0,value=self.rows['Avarax']['csvid'],index=0,ref='source'),s)
        self.assertEqual(sum(x['op']=='cast' and x['ref']=='source' for x in s),2)
        self.assertIn(dict(op='assert_zone_multiplicity',player=0,zone='hand',index=self.rows['Avarax']['csvid'],value=2),s)

    def test_other_free_ability_does_not_imply_unaffordable_tutor_is_available(self):
        p=explicit_spec(self.declaration('Blightspeaker'),self.oracle['Blightspeaker'])
        self.assertNotIn('insufficient_mana_0',variants(p))
        self.assertIn('tapped_0',variants(p))
        self.assertIn('payment_0',variants(p))

    def test_legendary_activated_source_does_not_create_illegal_duplicate(self):
        p=explicit_spec(self.declaration('Captain Sisay'),self.oracle['Captain Sisay']);self.assertNotIn('two_sources_0',variants(p));self.assertIn('repeat_0',variants(p))

    def test_battlefield_palette_defers_aura_attachment_but_filter_still_sees_auras(self):
        c=self.cases('Planar Bridge')[0]
        self.assertTrue(any(x['name']=='Pacifism' and x['zone']=='library' for x in c['setup']))
        self.assertIn(dict(op='assert_offered_legal_card',index=self.rows['Pacifism']['csvid'],value=1),c['steps'])
        # The friendly Grizzly Bears already occupies copy zero.
        self.assertIn(dict(op='bind_permanent_copy',player=0,value=self.rows['Grizzly Bears']['csvid'],index=1,ref='found_1'),c['steps'])

    def test_optional_trigger_and_menu_annotations_are_bounded(self):
        d=self.declaration()
        for patch in (dict(trigger_players=[True]),dict(trigger_players=[0,0]),dict(gate_menu=True),dict(entry_dialogs={'0':[-1]}),dict(activation_dialogs={'0':[0]})):
            with self.assertRaises(ValueError):self.cases(d=dict(d,**patch))

    def test_snapshot_and_controller_scopes(self):
        s=self.cases(selected=['pending_snapshot_1'])[0]['steps']
        for op in ('capture','record_state','ai_roundtrip','restore','assert_recorded_state'):self.assertTrue(any(x['op']==op for x in s))
        self.assertIn(dict(op='turn',player=1,value=0),s)

    def test_pins_and_inputs_are_not_silently_mutated(self):
        d=self.declaration();old=copy.deepcopy(d);self.cases(d=d);self.assertEqual(d,old)
        o=copy.deepcopy(self.oracle);o['Crib Swap']['keywords']=[]
        with self.assertRaises(ValueError):self.cases(oracle=o)
        for vs in ([],['unknown_0'],['search_0','search_0']):
            with self.assertRaises(ValueError):self.cases(d=dict(d,variants=vs))


if __name__=='__main__':unittest.main()
