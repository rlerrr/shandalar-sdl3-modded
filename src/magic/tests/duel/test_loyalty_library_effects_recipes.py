import json
from pathlib import Path
import unittest

from generate_cases import registry
from loyalty_library_effects_recipes import FAMILY, classify, expand, profiles, spec, validate_witnesses, variants


class LoyaltyLibraryEffectsTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.oracle = json.loads((Path(__file__).parent / 'data/oracle.json').read_bytes())['cards']
        cls.rows = {row['name']:row for row in registry()}

    def make(self, name, scene, owner):
        record, row = self.oracle[name], self.rows[name]
        variant = f'{scene}_{owner}'
        declaration = dict(family=FAMILY, version=2, params=classify(record), oracle_text=record['oracle_text'], variants=[variant])
        case, = expand(name, row['csvid'], declaration, spec(declaration, record), [variant], self.rows, self.oracle)
        return case

    def actions(self, case, op, **props):
        return [row for row in case['steps'] if row['op'] == op and all(row.get(key) == value for key,value in props.items())]

    def values(self, case, op, **props):
        return [row['value'] for row in self.actions(case, op, **props)]

    def zone(self, case, who, zone, name):
        return self.values(case, 'assert_zone_multiplicity', player=who, zone=zone, index=self.rows[name]['csvid'])[-1]

    def test_exact_three_printed_abilities_and_fully_pinned_registry(self):
        validate_witnesses(self.oracle,self.rows)
        sources = profiles()['sources']
        self.assertEqual({entry['registry']['csvid'] for entry in sources.values()}, {12757,14120,11539,15567})
        self.assertEqual(sum(len(variants(classify(self.oracle[name]))) for name in sources), 98)
        for name in sources:
            record = self.oracle[name]
            with self.assertRaises(ValueError):
                classify(dict(record, oracle_text=record['oracle_text'] + '\nYou gain 1 life.'))
            with self.assertRaises(ValueError):
                spec(dict(family=FAMILY, version=2.0, params=classify(record), oracle_text=record['oracle_text'], variants=['short_0']), record)

    def test_real_entry_spends_both_wallets_and_once_per_turn(self):
        for name in profiles()['sources']:
            for seat in (0,1):
                c = self.make(name,'once_per_turn',seat)
                prefix = c['steps'][1:13]
                self.assertEqual({(item['player'],item['color']) for item in prefix}, {(p,v) for p in (0,1) for v in range(6)})
                self.assertTrue(all(item['op']=='set_mana' and item['value']==0 for item in prefix))
                self.assertEqual(self.values(c,'assert_counter',counter_kind='loyalty')[0], int(self.oracle[name]['loyalty']))
                self.assertEqual(self.values(c,'activate',ref='source'), [1,0])
                self.assertEqual(self.values(c,'can_activate',ref='source'), [1,0,0,1])
                self.assertEqual(len(self.actions(c,'advance_turn')),2)

    def test_jace_piles_and_opponent_attack_expiry(self):
        for seat in (0,1):
            name = 'Jace, Architect of Thought'
            c = self.make(name,'pick_pile',seat)
            self.assertEqual(len(self.actions(c,'choose_zone',player=0)),3)
            self.assertEqual(self.zone(c,seat,'hand','Island'),seat+1)
            self.assertEqual(self.values(c,'assert_zone_card',player=seat,zone='library',index=0)[-1],self.rows['Mountain']['csvid'])
            weaken = self.make(name,'weaken',seat)
            self.assertEqual(self.values(weaken,'assert_card',ref='attacker',property='power'),[3,2,3,3])
            self.assertEqual(self.values(weaken,'assert_life',player=seat)[-1],15)

    def test_jace_ultimate_uses_accumulated_loyalty_and_owner_exile(self):
        for seat in (0,1):
            name = 'Jace, Architect of Thought'
            retain = self.make(name,'search_decline',seat)
            self.assertEqual(self.values(retain,'choose_dialog',player=seat),[0,0,0,0,2,2])
            self.assertEqual(len(self.actions(retain,'advance_turn')),8)
            for library, card in enumerate(('Goblin Piker','Wind Drake')):
                self.assertEqual(self.zone(retain,library,'exile',card),1)
                self.assertEqual(self.values(retain,'assert_shuffle_count',player=library)[-1],1)
                free = self.make(name,'search_cast',seat)
                self.assertEqual(self.zone(free,library,'exile',card),0)
                self.assertEqual(self.zone(free,seat,'battlefield',card),1)
                self.assertEqual(self.values(free,'cast',ref='source'),[1])

    def test_guildpact_bilateral_wheel_has_complete_hand_graveyard_and_shuffle_receipts(self):
        for seat in (0,1):
            name = 'Jace, the Living Guildpact'
            c = self.make(name,'wheel',seat)
            self.assertEqual(self.values(c,'choose_dialog',player=seat),[0,0,0,2])
            self.assertEqual(self.values(c,'assert_zone',player=seat,zone='hand')[-1],7)
            self.assertEqual(self.values(c,'assert_zone',player=seat,zone='library')[-1],0)
            self.assertEqual(self.zone(c,seat,'hand',name),1)
            self.assertEqual(self.zone(c,seat,'hand','Island'),3)
            self.assertEqual(self.values(c,'assert_zone',player=1-seat,zone='library')[-1],5)
            self.assertEqual(self.zone(c,seat,'hand','Forest'),1)
            self.assertEqual(self.zone(c,1-seat,'library','Mountain'),1)
            self.assertEqual(self.zone(c,1-seat,'graveyard','Swamp'),0)
            plus = self.make(name,'top_two',seat)
            self.assertEqual(self.zone(plus,seat,'graveyard','Forest'),1)

    def test_tezzeret_library_artifact_creature_and_controller_count(self):
        for seat in (0,1):
            name = 'Tezzeret, Agent of Bolas'
            library = self.make(name,'look_five',seat)
            self.assertEqual(self.zone(library,seat,'hand','Sol Ring'),1)
            self.assertEqual(self.values(library,'assert_zone_card',player=seat,zone='library',index=0)[-1],self.rows['Forest']['csvid'])
            animation = self.make(name,'animate_foe',seat)
            self.assertEqual(self.values(animation,'attack',ref='artifact'),[1])
            self.assertEqual(self.values(animation,'assert_life',player=seat)[-1],15)
            drain = self.make(name,'drain',seat)
            self.assertEqual(self.values(drain,'assert_life',player=seat)[-1],24)
            self.assertEqual(self.values(drain,'assert_life',player=1-seat)[-1],16)

    def test_chandra_hasty_tokens_wheel_and_exact_x(self):
        for seat in (0,1):
            name = 'Chandra, Flamecaller'
            tokens = self.make(name,'tokens_combat',seat)
            self.assertEqual([op['ref'] for op in self.actions(tokens,'attack')],['token_0','token_1'])
            self.assertEqual(self.values(tokens,'assert_life',player=1-seat)[-1],14)
            self.assertEqual(self.zone(tokens,seat,'battlefield','Elemental'),0)
            wheel = self.make(name,'wheel',seat)
            self.assertEqual(self.zone(wheel,seat,'graveyard','Sol Ring'),1)
            self.assertEqual(self.values(wheel,'assert_zone',player=seat,zone='hand')[-1],3)
            planned = [self.rows[name]['csvid'] for name in ('Goblin Piker', 'Sol Ring')] if seat == 0 else []
            self.assertEqual(self.values(wheel,'choose_zone_card',player=seat),planned)
            self.assertEqual(self.values(self.make(name,'empty_hand',seat),'choose_zone_card',player=seat),[])
            damage = self.make(name,'sweep_two',seat)
            self.assertEqual(self.values(damage,'choose_number',player=seat),[2])
            self.assertEqual(self.zone(damage,1-seat,'graveyard','Goblin Roughrider'),1)
            self.assertEqual(self.values(damage,'assert_card',ref='large',property='damage')[-1],2)
            illegal = self.make(name,'too_much_loyalty',seat)
            self.assertEqual(self.values(illegal,'activate',ref='source'),[0])

    def test_replays_consume_same_plan(self):
        for name in profiles()['sources']:
            for seat in (0,1):
                c = self.make(name,'replay',seat)
                steps = c['steps']
                capture = next(i for i,v in enumerate(steps) if v['op']=='capture')
                recorded = next(i for i,v in enumerate(steps) if v['op']=='record_state')
                self.assertEqual(steps[capture+1:recorded],steps[recorded+3:-1])
                self.assertEqual(steps[-1]['op'],'assert_recorded_state')

