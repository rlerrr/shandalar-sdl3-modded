"""Focused checks for exact kicker sweeper declarations and receipts."""

import unittest

import kicker_sweepers_recipes as recipe


class KickerSweeperTests(unittest.TestCase):
    def load_case(self, name, mode, actor=0):
        pins=recipe.profiles()['sources']|recipe.profiles()['witnesses']
        oracle={name:pin['oracle'] for name,pin in pins.items()}
        rows={name:pin['registry'] for name,pin in pins.items()}
        record=oracle[name]
        shape=recipe.classify(record)
        chosen=f'{mode}_{actor}'
        declaration=dict(family=recipe.FAMILY,version=2,params=shape,variants=[chosen],oracle_text=record['oracle_text'])
        return recipe.expand(name,rows[name]['csvid'],declaration,shape,[chosen],rows,oracle)[0]

    def values(self,case,op,**fields):
        return [s.get('value') for s in case['steps'] if s['op']==op and
                all(s.get(key)==value for key,value in fields.items())]

    def last_zones(self,case,actor,csvid,zone):
        return self.values(case,'assert_zone_multiplicity',player=actor,index=csvid,zone=zone)[-1]

    def test_complete_oracle_and_typed_kicker(self):
        card=recipe.profiles()['sources']['Breath of Darigaaz']['oracle']
        shape=recipe.classify(card)
        good=dict(family=recipe.FAMILY,version=2,oracle_text=card['oracle_text'],params=shape,variants=['paid_0'])
        self.assertEqual(recipe.spec(good,card)['kicker_mana'],[2,0,0,0,0,0])
        for update in ({'version':2.0},{'params':shape|{'flight':0}},{'variants':['invented_0']}):
            with self.assertRaises(ValueError):
                recipe.spec(good|update,card)
        for pinned in recipe.profiles()['sources'].values():
            oracle=pinned['oracle']
            with self.assertRaises(ValueError):
                recipe.classify(oracle|{'oracle_text':oracle['oracle_text']+'\nDraw a card.'})

    def test_each_source_casts_at_base_and_rejects_one_short(self):
        for name in recipe.profiles()['sources']:
            for actor in (0,1):
                base=self.load_case(name,'base_only',actor)
                self.assertEqual(self.values(base,'cast',ref='source'),[1])
                self.assertEqual(self.values(base,'choose_dialog'),[])
                denied=self.load_case(name,'cast_short',actor)
                self.assertEqual(self.values(denied,'cast',ref='source'),[0])
                self.assertEqual(self.values(denied,'assert_card',ref='source',property='in_hand'),[1])

    def test_flying_partition_and_life(self):
        for actor in (0,1):
            for source,active,spared in (
                    ('Breath of Darigaaz',106,1197),('Canopy Surge',1197,106)):
                paid=self.load_case(source,'paid',actor)
                declined=self.load_case(source,'declined',actor)
                for seat in (0,1):
                    self.assertEqual(self.values(paid,'assert_life',player=seat),[16])
                    self.assertEqual(self.values(declined,'assert_life',player=seat),[19])
                    self.assertEqual(self.last_zones(paid,seat,active,'graveyard'),1)
                    self.assertEqual(self.last_zones(paid,seat,spared,'battlefield'),1)

    def test_targeted_shrink_applies_and_expires(self):
        for actor in (0,1):
            case=self.load_case('Marsh Casualties','self_target',actor)
            self.assertEqual(self.values(case,'choose_player',player=actor),[actor])
            self.assertEqual(self.values(case,'assert_card',ref=f'p{actor}_three',property='power'),[1,3])
            self.assertEqual(self.values(case,'assert_card',ref=f'p{1-actor}_three',property='power'),[3,3])
            self.assertEqual(self.last_zones(case,actor,106,'graveyard'),1)

    def test_overload_uses_printed_breakpoints_and_full_offer(self):
        artifact=recipe.profiles()['witnesses']
        for actor in (0,1):
            for mode,name,destroyed in (
                    ('threshold_two','Mind Stone',1),('threshold_three','Jalum Tome',0),
                    ('threshold_three_paid','Jalum Tome',1),('paid','Gilded Lotus',1),
                    ('threshold_six','Dreamstone Hedron',0),('optional_low','Mind Stone',1)):
                case=self.load_case('Overload',mode,actor)
                victim=artifact[name]['registry']['csvid']
                self.assertEqual(self.last_zones(case,1-actor,victim,'graveyard'),destroyed)
            low=self.load_case('Overload','optional_low',actor)
            cast=next(i for i,s in enumerate(low['steps']) if s['op']=='cast')
            self.assertEqual([s['op'] for s in low['steps'][cast-2:cast+2]],[
                'choose_target','choose_dialog','cast','assert_unused_choices'])
            self.assertEqual(self.values(low,'assert_mana',player=actor,color=0)[-1],0)

    def test_wrong_white_retains_extra_generic_and_ordinary_entry(self):
        for actor in (0,1):
            for name in ('Desolation Angel','Desolation Giant'):
                case=self.load_case(name,'wrong_white',actor)
                self.assertFalse(self.values(case,'choose_dialog'))
                self.assertEqual(self.values(case,'assert_mana',player=actor,color=0)[-1],2)
                self.assertEqual(self.values(case,'assert_card',ref='source',property='in_play')[:2],[0,1])

    def test_paid_counter_preserves_all_effect_recipients(self):
        for name in recipe.profiles()['sources']:
            for actor in (0,1):
                case=self.load_case(name,'countered',actor)
                relevant=[s['op'] for s in case['steps'] if s['op'] in ('cast','announce_spell','cast_response','resolve_response')]
                self.assertEqual(relevant,['cast','announce_spell','cast_response','resolve_response'])
                self.assertEqual(self.values(case,'assert_life',player=1-actor),[20])
                self.assertEqual(self.last_zones(case,1-actor,48,'graveyard'),1)
                self.assertTrue(all(s['value']==0 for s in case['steps'] if s['op']=='assert_card' and s.get('property')=='damage'))

    def test_replay_uses_identical_paid_actions(self):
        for name in recipe.profiles()['sources']:
            case=self.load_case(name,'replay')
            ops=[step['op'] for step in case['steps']]
            first,record,restored=map(ops.index,('capture','record_state','ai_roundtrip'))
            self.assertEqual(case['steps'][first+1:record],case['steps'][restored+1:-1])
            self.assertEqual(ops[-1],'assert_recorded_state')
