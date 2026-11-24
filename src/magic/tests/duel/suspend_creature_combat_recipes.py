"""Creature Suspend costs, real upkeep countdown and ordinary combat."""

from collections import Counter
from functools import lru_cache
import json
from pathlib import Path

from library_search_recipes import wave6_exact as exact
from recipes import card, cost_from_oracle, step


FAMILY = 'suspend_creature_combat'
PROFILE = Path(__file__).parent / 'data' / f'{FAMILY}_wave8_profiles.json'
ZONE = {'hand': 0, 'battlefield': 1, 'library': 2, 'graveyard': 3, 'exile': 4}
ZONE_NAMES = {number: name for name, number in ZONE.items()}
ZERO = [0] * 6

SUSPEND_REMINDER = (
    ' (Rather than cast this card from your hand, you may pay {fee} and exile it with {count} '
    'time counters on it. At the beginning of your upkeep, remove a time counter. '
    'When the last is removed, you may cast it without paying its mana cost. It has haste.)'
)
FLANKING = "flanking (Whenever a creature without flanking blocks this creature, the blocking creature gets -1/-1 until end of turn.)"
CARDS = {
    8531: (3, '{1}{W}', ('Flying, protection from black',), (), True),
    8786: (4, '{1}{G}', ('Flying',), (), True),
    8582: (4, '{W}{U}', ('Vigilance', '{T}: Untap target attacking creature. Prevent all combat damage that would be dealt '
                        'to and dealt by that creature this turn.'), (), False),
    8955: (3, '{W}', ('Flying; ' + FLANKING,), (), True),
    8837: (3, '{1}{W}{W}', ('Protection from black; ' + FLANKING,),
           ("When the last time counter is removed from this card while it's exiled, create a 2/2 black Knight creature "
            'token with flanking, protection from white, and haste.',), False),
    8846: (3, '{W}', ('{W}: This creature gets +1/+1 until end of turn.',), (), True),
}
MODES = ('parked', 'countdown', 'unfunded')
EXTRA = {8837: ('token_combat',), 8846: ('wrong_pump',)}
NOVEL = ('wrong_suspend', 'normal', 'countered', 'replay')
WITNESSES = {'Wall of Roots', 'Dusk Imp', 'Wind Drake', 'Goblin Tinkerer', 'Hill Giant',
             'Suntail Hawk', 'Muck Rats', 'Island', 'Mountain', 'Grizzly Bears', 'Plains',
             'Knight', 'Counterspell'}
STATIC_DUMMY_WITNESSES = {'Grizzly Bears', 'Muck Rats', 'Suntail Hawk', 'Wind Drake', 'Hill Giant', 'Dusk Imp'}


@lru_cache(maxsize=1)
def profiles():
    return json.loads(PROFILE.read_bytes())


def classify(record, csvid):
    duration, fee, before, after, reminder = CARDS[csvid]
    printed = f'Suspend {duration}—{fee}'
    if reminder:
        printed += SUSPEND_REMINDER.format(fee=fee, count={3: 'three', 4: 'four'}[duration])
    clauses = [*before, printed, *after]
    if (record['oracle_text'] != '\n'.join(clauses) or record['layout'] != 'normal'
            or 'Suspend' not in record['keywords'] or record.get('card_faces') or record.get('selected_face')
            or 'Creature' not in record['type_line'].split(' — ')[0]):
        raise ValueError('Exact whole printed Suspend creature required')
    pt = [int(record[k]) for k in ('power', 'toughness')]
    return dict(csvid=csvid, duration=duration, fee=cost_from_oracle({'mana_cost': fee}),
                printed=cost_from_oracle(record), pt=pt, clauses=clauses)


def variants(model):
    modes = (*MODES, *EXTRA.get(model['csvid'], ()), *NOVEL)
    return [f'{mode}_{seat}' for mode in modes for seat in (0, 1)]


def validate_witnesses(oracle, rows):
    pins = profiles()
    if (set(pins) != {'sources', 'witnesses'} or set(pins['witnesses']) != WITNESSES
            or {p['registry']['csvid'] for p in pins['sources'].values()} != CARDS.keys()):
        raise ValueError('Exact independent Suspend source and witness pins required')
    for group in pins.values():
        for name, pin in group.items():
            record, row = oracle.get(name, {}), rows.get(name, {})
            handler = row.get('handler')
            if (not exact(pin, {'oracle': record, 'registry': row})
                    or record.get('layout') != ('token' if name == 'Knight' else 'normal')
                    or row.get('kind') != 'card' or handler in (None, '0', 'NULL')
                    or handler == 'card_dummy' and name not in STATIC_DUMMY_WITNESSES):
                raise ValueError(f'Pinned Suspend source or witness changed: {name}')


def spec(declaration, record):
    pin = profiles()['sources'].get((record or {}).get('name'))
    if not pin or not exact(record, pin['oracle']):
        raise ValueError('Complete pinned Suspend creature required')
    model = classify(record, pin['registry']['csvid'])
    picked = declaration.get('variants')
    if (declaration.get('family') != FAMILY or type(declaration.get('version')) is not int
            or declaration['version'] != 2 or declaration.get('oracle_text') != record['oracle_text']
            or not exact(declaration.get('params'), model) or not isinstance(picked, list) or not picked
            or any(type(v) is not str for v in picked) or len(picked) != len(set(picked))
            or set(picked) - set(variants(model))):
        raise ValueError('Exact printed Suspend parameters and distinct variants required')
    return dict(model, family=FAMILY)


class SuspendBoard:
    def add(self, ref, player, name, zone):
        self.setup.append(card(name, ref, player, zone))
        self.cards[ref] = dict(name=name, player=player, zone=zone)

    def emit(self, op, ref='', other='', prop='', *, zone=None, **fields):
        row = step(op, **fields)
        for key, field in [('ref', ref), ('other', other), ('property', prop)]:
            if field:
                row[key] = field
        if zone is not None:
            row['zone'] = ZONE_NAMES[zone]
        self.steps.append(row)

    def observe(self, ref, prop, value):
        self.emit('assert_card', ref, prop=prop, value=value)

    def mana(self, op, player, amounts):
        for color, amount in enumerate(amounts):
            self.emit(op, player=player, color=color, value=amount)

    def receipts(self, *, ordered_library=True):
        self.emit('assert_stack')
        names = sorted({card['name'] for card in self.cards.values()})
        for seat in (0, 1):
            self.emit('assert_life', player=seat, value=self.life[seat])
            for zone in ZONE:
                contents = self.deck[seat] if zone == 'library' else [
                    card['name'] for card in self.cards.values() if card['player'] == seat and card['zone'] == zone
                ]
                self.emit('assert_zone', player=seat, zone=ZONE[zone], value=len(contents))
                counts = Counter(contents)
                for name in names:
                    self.emit('assert_zone_multiplicity', player=seat, zone=ZONE[zone], index=self.rows[name]['csvid'],
                              value=counts[name])
                if zone == 'library' and (ordered_library or seat == self.foe):
                    for index, name in enumerate(contents):
                        self.emit('assert_zone_card', player=seat, zone=ZONE[zone], index=index,
                                  value=self.rows[name]['csvid'])

    def upkeep(self, player, *, last=False):
        self.emit('advance_turn')
        self.emit('untap_step', player=player)
        self.emit('upkeep_step', player=player)
        self.emit('resolve')
        self.emit('refresh')
        self.emit('assert_unused_choices', value=0)
        self.emit('assert_choices_consumed')
        for seat in (0, 1):
            self.mana('assert_mana', seat, [0] * 6)
        if not last:
            self.receipts()



class CombatSuspend(SuspendBoard):
    def __init__(self, csvid, record, actor, mode, rows, oracle):
        self.csvid, self.record, self.actor, self.foe, self.mode = csvid, record, actor, 1 - actor, mode
        self.rows, self.oracle = rows, oracle
        self.model = classify(record, csvid)
        self.duration, self.payment = self.model['duration'], self.model['fee']
        self.setup, self.steps, self.cards = [], [], {}
        self.life = [20, 20]
        self.snapshot = None
        self.deck = {actor: ['Island', 'Mountain', 'Grizzly Bears', 'Plains'],
                     self.foe: ['Island', 'Plains']}
        self.add('source', actor, record['name'], 'hand')
        for seat, cards in self.deck.items():
            for pos, name in enumerate(cards):
                self.add(f'd_{seat}_{pos}', seat, name, 'library')
        if mode in ('normal', 'countered'):
            if mode == 'countered':
                self.add('counter', self.foe, 'Counterspell', 'hand')
            return
        if csvid in (8531, 8786, 8955):
            self.add('ground', self.foe, 'Wall of Roots', 'battlefield')
        if csvid == 8531:
            self.add('black', self.foe, 'Dusk Imp', 'battlefield')
        if csvid == 8955:
            self.add('blocker', self.foe, 'Wind Drake', 'battlefield')
        if csvid == 8582:
            self.add('ally', actor, 'Goblin Tinkerer', 'battlefield')
            self.add('blocker', self.foe, 'Hill Giant', 'battlefield')
        if csvid == 8837:
            self.add('blocker', self.foe, 'Wall of Roots', 'battlefield')
            self.add('protected', self.foe, 'Suntail Hawk' if mode == 'token_combat' else 'Muck Rats', 'battlefield')

    def wrong_suspend_budget(self):
        budget = self.payment.copy()
        needed = next(i for i, value in enumerate(budget) if i and value)
        wrong = next(i for i in range(1, 6) if not budget[i] and not self.model['printed'][i])
        budget[needed] -= 1
        budget[wrong] = 1
        return budget

    def suspend_from_hand(self):
        self.emit('turn', player=self.actor)
        self.mana('set_mana', self.foe, ZERO)
        parked = self.mode not in ('unfunded', 'wrong_suspend')
        budget = self.wrong_suspend_budget() if self.mode == 'wrong_suspend' else self.payment if parked else ZERO
        self.mana('set_mana', self.actor, budget)
        self.emit('refresh')
        both_affordable = self.payment == self.model['printed']
        if parked and (self.actor == 1 or both_affordable):
            # Riftmarked's equal printed and alternate costs offer both rows even to player zero.
            self.emit('choose_dialog', player=self.actor, value=1)
        self.emit('cast', 'source', value=0)
        self.emit('assert_choices_consumed')
        self.mana('assert_mana', self.actor, ZERO if parked else budget)
        self.mana('assert_mana', self.foe, ZERO)
        if parked:
            self.cards['source']['zone'] = 'exile'
        self.receipts()

    def normal_cast(self):
        self.emit('turn', player=self.actor)
        self.mana('set_mana', self.foe, ZERO)
        self.mana('set_mana', self.actor, self.model['printed'])
        self.emit('refresh')
        # Full printed funding makes both the regular and Suspend alternatives payable.
        self.emit('choose_dialog', player=self.actor, value=0)
        self.emit('cast', 'source', value=1)
        self.emit('assert_choices_consumed')
        self.emit('assert_stack', value=1)
        self.mana('assert_mana', self.actor, ZERO)
        self.mana('assert_mana', self.foe, ZERO)
        self.observe('source', 'in_play', 0)
        if self.mode == 'countered':
            self.emit('announce_spell', 'source')
            self.mana('set_mana', self.foe, cost_from_oracle(self.oracle['Counterspell']))
            self.emit('refresh')
            self.emit('cast_response', 'counter', value=1)
            self.emit('assert_choices_consumed')
            self.emit('assert_stack', value=2)
            self.mana('assert_mana', self.foe, ZERO)
            self.emit('resolve_response')
            self.emit('assert_choices_consumed')
            self.cards['source']['zone'] = self.cards['counter']['zone'] = 'graveyard'
        else:
            self.emit('resolve')
            self.emit('refresh')
            self.cards['source']['zone'] = 'battlefield'
            self.observe('source', 'in_play', 1)
            self.observe_pt('source', *self.model['pt'])
            self.observe('source', 'sick', 1)
            self.emit('begin_attackers')
            self.emit('attack', 'source', value=0)
            self.observe('source', 'attacking', 0)
            self.emit('finish_attackers')
            self.emit('begin_blockers')
            self.emit('advance_combat')
            self.complete_damage()
        if self.csvid == 8837:
            self.emit('assert_zone_multiplicity', player=self.actor, zone=ZONE['battlefield'],
                      index=self.rows['Knight']['csvid'], value=0)
        self.receipts()

    def accept_cast_outcome(self):
        self.cards['source']['zone'] = 'battlefield'
        if self.csvid == 8837:
            self.cards['knight'] = dict(name='Knight', player=self.actor, zone='battlefield')
        self.receipts()
        self.emit('bind_permanent_copy', 'played', player=self.actor, index=0, value=self.csvid)
        self.observe('played', 'in_play', 1)
        self.observe_pt('played', int(self.record['power']), int(self.record['toughness']))
        if self.csvid == 8837:
            csvid = self.rows['Knight']['csvid']
            self.emit('bind_permanent_copy', 'knight', player=self.actor, index=0, value=csvid)
            self.observe_pt('knight', 2, 2)
            for prop, value in (('token', 1), ('color', 2), ('subtype_Knight', 1), ('artifact', 0)):
                self.emit('assert_matching_permanents', prop=prop, player=self.actor, index=csvid, value=value)
        self.emit('turn', player=self.actor)

    def observe_pt(self, ref, power, toughness):
        self.observe(ref, 'power', power)
        self.observe(ref, 'toughness', toughness)

    def complete_damage(self):
        self.emit('combat_damage')
        self.emit('resolve')
        self.emit('end_combat')
        self.emit('assert_choices_consumed')

    def attack(self, attacker, power, *, denied=(), flanking=False):
        self.emit('begin_attackers')
        self.emit('attack', attacker, value=1)
        self.observe(attacker, 'attacking', 1)
        self.observe(attacker, 'tapped', 1)
        self.emit('finish_attackers')
        self.emit('begin_blockers')
        for creature in denied:
            self.emit('block', creature, attacker, value=0)
        if flanking:
            self.emit('block', 'blocker', attacker, value=1)
        self.emit('advance_combat')
        self.observe(attacker, 'blocked', int(flanking))
        if flanking:
            ref = self.oracle[self.cards['blocker']['name']]
            blocker_power, blocker_toughness = (int(ref[k]) - 1 for k in ('power', 'toughness'))
            self.observe_pt('blocker', blocker_power, blocker_toughness)
        self.complete_damage()
        if not flanking:
            self.life[self.foe] -= power
            return
        self.observe(attacker, 'damage', max(0, blocker_power))
        if power >= blocker_toughness:
            self.cards['blocker']['zone'] = 'graveyard'
        else:
            self.observe('blocker', 'damage', power)

    def protect_attacking_ally(self):
        self.emit('can_activate', 'played', value=0)
        self.emit('begin_attackers')
        self.emit('attack', 'played', value=1)
        self.emit('attack', 'ally', value=1)
        self.observe('played', 'attacking', 1)
        self.observe('played', 'tapped', 0)
        self.observe('ally', 'tapped', 1)
        self.emit('finish_attackers')
        self.emit('begin_blockers')
        self.emit('block', 'blocker', 'ally', value=1)
        self.emit('advance_combat')
        self.observe('ally', 'blocked', 1)
        self.observe('played', 'blocked', 0)
        self.emit('can_activate', 'played', value=1)
        self.emit('choose_target', 'ally', player=self.actor)
        self.emit('activate', 'played', value=1)
        self.emit('assert_choices_consumed')
        self.emit('assert_stack', value=1)
        self.observe('played', 'tapped', 1)
        self.observe('ally', 'tapped', 1)
        self.emit('resolve')
        self.emit('refresh')
        self.emit('assert_stack', value=0)
        self.observe('ally', 'tapped', 0)
        self.observe('ally', 'attacking', 1)
        self.complete_damage()
        self.observe('ally', 'damage', 0)
        self.observe('blocker', 'damage', 0)
        self.life[self.foe] -= int(self.record['power'])

    def shade(self):
        wrong = self.mode in ('wrong_pump', 'replay')
        fee = [0, 0, 0, 1, 0, 0] if wrong else cost_from_oracle({'mana_cost': '{W}'})
        power, toughness = (int(self.record[k]) for k in ('power', 'toughness'))
        self.mana('set_mana', self.actor, fee)
        self.emit('refresh')
        self.emit('can_activate', 'played', value=int(not wrong))
        self.emit('activate', 'played', value=int(not wrong))
        self.emit('assert_choices_consumed')
        self.emit('assert_stack', value=int(not wrong))
        self.mana('assert_mana', self.actor, fee if wrong else ZERO)
        self.observe_pt('played', power, toughness)
        self.emit('resolve')
        self.emit('refresh')
        self.emit('assert_stack', value=0)
        self.observe_pt('played', power + int(not wrong), toughness + int(not wrong))
        self.attack('played', power + int(not wrong))
        # The separate mana receipt above detects an unpaid pump. Clear the
        # fixture pool at the turn boundary so that observation never masks cleanup.
        self.mana('set_mana', self.actor, ZERO)
        self.emit('refresh')
        self.emit('advance_turn')
        self.emit('untap_step', player=self.foe)
        self.observe_pt('played', power, toughness)

    def build(self):
        if self.mode in ('normal', 'countered'):
            self.normal_cast()
            return
        self.suspend_from_hand()
        if self.mode in ('unfunded', 'wrong_suspend'):
            return
        if self.mode == 'parked':
            self.upkeep(self.foe)
            return
        if self.mode == 'replay':
            self.snapshot = len(self.steps)
        for owner_turn in range(1, self.duration + 1):
            self.upkeep(self.foe)
            self.upkeep(self.actor, last=owner_turn == self.duration)
        self.accept_cast_outcome()
        power = int(self.record['power'])
        if self.csvid == 8531:
            self.attack('played', power, denied=('ground', 'black'))
        elif self.csvid == 8786:
            self.attack('played', power, denied=('ground',))
        elif self.csvid == 8955:
            self.attack('played', power, denied=('ground',), flanking=True)
        elif self.csvid == 8582:
            self.protect_attacking_ally()
        elif self.csvid == 8837:
            atk = 'knight' if self.mode == 'token_combat' else 'played'
            self.attack(atk, 2, denied=('protected',), flanking=True)
        elif self.csvid == 8846:
            self.shade()
        self.receipts()


def expand(name, csvid, declaration, model, selected, rows, oracle):
    validate_witnesses(oracle, rows)
    if rows[name]['csvid'] != csvid or model['csvid'] != csvid or set(selected) - set(variants(model)):
        raise ValueError('Exact Suspend source and scoped variants required')
    cases = []
    for variant in selected:
        mode, seat = variant.rsplit('_', 1)
        game = CombatSuspend(csvid, oracle[name], int(seat), mode, rows, oracle)
        game.build()
        actions = game.steps
        if game.snapshot is not None:
            cut = game.snapshot
            suffix = actions[cut:]
            actions = actions[:cut] + [step('capture')] + suffix + [
                step('record_state'), step('restore'), step('ai_roundtrip')
            ] + suffix + [step('assert_recorded_state')]
        gaps = ['Optional decline of the last-counter cast has no offered input',
                'Counterspell is exercised against the ordinary paid cast only']
        metadata = dict(family=FAMILY, version=2, variant=variant, params=declaration['params'],
                        oracle_text=declaration['oracle_text'], covered_clauses=model['clauses'],
                        untested_components=gaps)
        cases.append(dict(name=f'SuspendCreatureCombatRecipe_{csvid}_{variant}', card=name, fixture='empty_main',
                          seed=2439, setup=game.setup, steps=actions, _recipe=metadata))
    return cases

