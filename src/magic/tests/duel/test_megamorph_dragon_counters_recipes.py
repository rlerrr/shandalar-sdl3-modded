"""Guard complete printed Dragon mechanics and their selected real inputs."""

import json
from pathlib import Path
import unittest

from generate_cases import registry
import megamorph_dragon_counters_recipes as model


class MegamorphDragonRecipes(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        data = Path(__file__).parent / 'data'
        cls.oracle = json.loads((data / 'oracle.json').read_text(encoding='utf8'))['cards']
        cls.rows = {row['name']: row for row in registry()}
        cls.homes = {}
        for name, pin in model.pins()['sources'].items():
            csvid = pin['registry']['csvid']
            path, = (data / 'cases/cards').glob(f'*/{csvid:05d}-*.json')
            cls.homes[name] = json.loads(path.read_text(encoding='utf8'))['recipes'][0]

    def cases(self, name, *selected):
        declaration = self.homes[name]
        shape = model.spec(declaration, self.oracle[name])
        return model.expand(name, self.rows[name]['csvid'], declaration, shape, selected,
                            self.rows, self.oracle)

    def steps(self, name, variant):
        return self.cases(name, variant)[0]['steps']

    def values(self, steps, op, **fields):
        return [s['value'] for s in steps if s['op'] == op and all(s.get(k) == v for k,v in fields.items())]

    def counters(self, steps, ref):
        return self.values(steps, 'assert_counter', ref=ref, counter_kind='plus1')

    def test_exact_cohort_original_records_and_closed_variants(self):
        model.validate_witnesses(self.oracle, self.rows)
        generated = []
        for name, declaration in self.homes.items():
            oracle = self.oracle[name]
            shape = model.spec(declaration, oracle)
            self.assertEqual(declaration['variants'], model.variants(shape))
            cases = self.cases(name, *declaration['variants'])
            self.assertEqual(len(cases), 38)
            for case in cases:
                self.assertEqual(case['_recipe']['covered_clauses'], oracle['oracle_text'].splitlines())
            generated.extend(cases)
        self.assertEqual(len({c['name'] for c in generated}), 190)
        name = 'Acid-Spewer Dragon'
        r,d = self.oracle[name],self.homes[name]
        for fields in (dict(version=True), dict(variants=['reveal_0', 'reveal_0']),
                       dict(variants=['unreviewed_1']), dict(variants=[True])):
            with self.subTest(fields=fields), self.assertRaises(ValueError):
                model.spec(dict(d, **fields), r)
        with self.assertRaises(ValueError):
            model.spec(d, dict(r, oracle_id='stale'))
        self.assertRaises(ValueError, model.parameters,
                          dict(r, oracle_text=r['oracle_text'].replace('{5}{B}{B}', '{5}{B}')))

    def test_distinct_printed_cast_reveal_and_short_payment(self):
        n = 'Belltoll Dragon'
        hidden,faceup,short,reveal = self.cases(n,'hidden_choice_0','faceup_0','flip_short_0','reveal_0')
        for case in (hidden,faceup,short,reveal):
            clear = [s for s in case['steps'] if s['op']=='set_mana'][:12]
            self.assertEqual(clear,[dict(op='set_mana',player=p,color=c,value=0) for p in (0,1) for c in range(6)])
        self.assertIn(dict(op='choose_dialog',player=0,value=0),hidden['steps'])
        self.assertIn(dict(op='choose_dialog',player=0,value=1),faceup['steps'])
        self.assertIn(dict(op='activate',ref='source',value=0),short['steps'])
        for case in (hidden,faceup,short):
            self.assertEqual(set(self.counters(case['steps'],'source')),{0})
        steps = reveal['steps']
        activation = steps.index(dict(op='activate',ref='source',value=1))
        drain = steps.index(dict(op='resolve'),activation)
        self.assertIn(dict(op='set_mana',player=0,color=2,value=2),steps[:activation])
        self.assertIn(dict(op='assert_mana',player=0,color=2,value=0),steps[activation:drain])
        self.assertIn(dict(op='assert_card',ref='source',property='csvid',value=14856),steps[activation:drain])
        self.assertEqual(self.counters(steps[activation:drain],'source'),[1])

    def test_recipient_scope_and_real_control_transfers(self):
        name = 'Stormwing Dragon'
        full = self.steps(name,'reveal_0')
        self.assertEqual([self.counters(full,r)[-1] for r in
                          ('source','own_dragon','other_dragon','foe_dragon','non_dragon')],[1,1,1,0,0])
        no_other = self.steps(name,'no_ally_0')
        self.assertEqual(self.counters(no_other,'source')[-1],1)
        self.assertEqual(self.counters(no_other,'foe_dragon')[-1],0)
        for mode,ref,actor,count in (('acquired_1','foe_dragon',1,1),('donated_1','own_dragon',0,0)):
            ss = self.steps(name,mode)
            at = ss.index(dict(op='activate',ref='trader',value=1))
            flip = ss.index(dict(op='activate',ref='source',value=1))
            row = 'Dragon Whelp' if ref=='foe_dragon' else 'Shivan Dragon'
            self.assertIn(dict(op='bind_permanent',ref=ref,player=actor,value=self.rows[row]['csvid']),ss[at:flip])
            self.assertEqual(self.counters(ss,ref)[-1],count)

    def test_facedown_and_late_dragons_are_excluded(self):
        name = 'Acid-Spewer Dragon'
        hidden = self.steps(name,'hidden_dragon_1')
        self.assertIn(dict(op='cast',ref='secondary',value=1),hidden)
        self.assertIn(dict(op='assert_card',ref='secondary',property='csvid',value=model.FACE_DOWN),hidden)
        self.assertEqual(set(self.counters(hidden,'secondary')),{0})
        self.assertEqual(self.counters(hidden,'own_dragon')[-1],1)
        late = self.steps(name,'late_dragon_1')
        self.assertLess(late.index(dict(op='activate',ref='source',value=1)),
                        late.index(dict(op='cast',ref='other_dragon',value=1)))
        self.assertEqual(set(self.counters(late,'other_dragon')),{0})

    def test_real_bounce_recast_new_source_and_existing_counters(self):
        name = 'Herdchaser Dragon'
        steps = self.steps(name,'recast_0')
        casts = [i for i,s in enumerate(steps) if s == dict(op='cast',ref='source',value=1)]
        self.assertEqual(len(casts),2)
        at = steps.index(dict(op='bind_hand_copy',ref='source',player=0,index=0,value=14936))
        self.assertIn(dict(op='cast',ref='return_spell',value=1),steps[casts[0]:at])
        self.assertLess(at,casts[1])
        self.assertEqual(set(self.counters(steps,'source')),{0,1})
        self.assertEqual(self.counters(steps,'own_dragon')[-1],2)
        self.assertEqual(self.counters(steps,'other_dragon')[-1],2)

    def test_flying_ground_block_and_damage_for_every_source(self):
        for name in self.homes:
            for mode,allowed,life in (('flying_0',0,16),('ground_hidden_0',1,20)):
                with self.subTest(name=name,mode=mode):
                    steps = self.steps(name,mode)
                    self.assertIn(dict(op='block',ref='ground',other='source',value=allowed),steps)
                    self.assertEqual(self.values(steps,'assert_life',player=1)[-1],life)

    def test_lifelink_applies_in_flying_control_for_both_owners(self):
        for name in self.homes:
            for actor in (0,1):
                with self.subTest(name=name,actor=actor):
                    steps = self.steps(name,f'flying_{actor}')
                    gain = 4 if name == 'Shieldhide Dragon' else 0
                    self.assertEqual(self.values(steps,'assert_life',player=actor)[-1],20+gain)
                    hidden = self.steps(name,f'ground_hidden_{actor}')
                    self.assertEqual(self.values(hidden,'assert_life',player=actor)[-1],20)

    def test_deathtouch_makes_nonlethal_damage_fatal(self):
        name = 'Acid-Spewer Dragon'
        active,hidden = self.cases(name,'deathtouch_0','deathtouch_hidden_0')
        dead = dict(op='assert_zone_multiplicity',player=1,zone='graveyard',index=224,value=1)
        self.assertIn(dead,active['steps'])
        self.assertNotIn(dead,hidden['steps'])
        self.assertIn(dict(op='assert_card',ref='foe_dragon',property='damage',value=2),hidden['steps'])

    def test_hexproof_measures_real_opposing_pending_and_own_target(self):
        name = 'Belltoll Dragon'
        steps = self.steps(name,'hexproof_1')
        self.assertEqual(self.values(steps,'can_target_creature',ref='opposing_spell',other='source'),[1,0])
        self.assertEqual(self.values(steps,'can_target_creature',ref='return_spell',other='source'),[1,1])
        self.assertIn(dict(op='cast',ref='return_spell',value=1),steps)
        self.assertIn(dict(op='assert_zone_multiplicity',player=1,zone='hand',index=14856,value=1),steps)
        pending = self.steps(name,'hexproof_pending_1')
        at = pending.index(dict(op='resolve_response'))
        self.assertLess(pending.index(dict(op='cast',ref='opposing_spell',value=1)),at)
        self.assertLess(pending.index(dict(op='activate',ref='source',value=1)),at)
        self.assertIn(dict(op='assert_zone_multiplicity',player=1,zone='battlefield',index=14856,value=1),pending[at:])

    def test_trample_lifelink_and_first_strike_receipts(self):
        for name,keyword,ours,foe in (('Herdchaser Dragon','trample',20,19),('Shieldhide Dragon','lifelink',24,16),
                                    ('Stormwing Dragon','first_strike',20,20)):
            steps = self.steps(name,keyword+'_0')
            hidden = self.steps(name,keyword+'_hidden_0')
            self.assertEqual(self.values(steps,'assert_life',player=0)[-1],ours)
            self.assertEqual(self.values(steps,'assert_life',player=1)[-1],foe)
            if keyword=='first_strike':
                self.assertIn(dict(op='assert_zone_multiplicity',player=1,zone='graveyard',index=221,value=1),steps)
                self.assertNotIn(dict(op='assert_zone_multiplicity',player=0,zone='graveyard',index=15039,value=1),steps)
                self.assertIn(dict(op='assert_zone_multiplicity',player=0,zone='graveyard',index=15039,value=1),hidden)
            else:
                self.assertEqual(self.values(hidden,'assert_life',player=0)[-1],20)

    def test_snapshot_replay_preserves_fresh_counters(self):
        steps = self.steps('Shieldhide Dragon','replay_1')
        capture = steps.index(dict(op='capture'))
        record = steps.index(dict(op='record_state'))
        restore = steps.index(dict(op='restore'))
        self.assertEqual(steps[capture+1:record],steps[restore+1:-1])
        self.assertEqual(self.counters(steps[capture:record],'source')[-1],1)
        self.assertEqual(self.counters(steps[capture:record],'other_dragon')[-1],1)
        self.assertEqual(steps[-1],dict(op='assert_recorded_state'))
