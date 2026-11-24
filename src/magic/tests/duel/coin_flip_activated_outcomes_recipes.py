"""Real seeded coin flips, activation costs, and anchored outcome receipts."""
from collections import Counter
from functools import lru_cache
import json
from pathlib import Path

from counter_condition_recurrence_recipes import exact
from recipes import card, cost_from_oracle, step


FAMILY = 'coin_flip_activated_outcomes'
PROFILE = Path(__file__).with_name('data') / 'coin_flip_activated_outcomes_wave8_profiles.json'
SCALES, BANG, LYRE, MANA = 4443, 11811, 1953, 17807
MODES = {
    SCALES: ('win', 'lose', 'pay_retry', 'decline_retry', 'short', 'replay'),
    BANG: ('win_player', 'win_creature', 'lose', 'win_own', 'win_prevented'),
    LYRE: ('win_player', 'lose_player', 'win_walker', 'lose_walker', 'win_zero', 'lose_zero'),
    MANA: ('win', 'lose', 'short', 'response_win', 'colored_fee', 'payment_gate'),
}
ZONES = {0: 'hand', 1: 'battlefield', 2: 'library', 3: 'graveyard', 4: 'exile'}
ZONE = {name: index for index, name in ZONES.items()}
SEEDS = (96910, 26200)

WITNESSES = ('Cho-Manno, Revolutionary', 'Forest', 'Grizzly Bears', 'Hill Giant', 'Jace Beleren', 'Millstone', 'Shock', 'Squire')

CARDS = {
    SCALES: ('{4}', 'Artifact'),
    BANG: ('{2}{R}{R}', 'Creature'),
    LYRE: ('{3}', 'Artifact'),
    MANA: ('{1}', 'Artifact'),
}
GAPS = {
    BANG: ('Win and loss outcomes use the actual scripted selector. Target-selection timing relative to the flip is unobserved.',),
}


@lru_cache(maxsize=1)
def profiles():
    return json.loads(PROFILE.read_bytes())


def classify(record, csvid):
    if csvid not in CARDS or record.get('layout') != 'normal' or record.get('selected_face') or record.get('card_faces'):
        raise ValueError('normal whole-record activated coin flip required')
    mana, kind = CARDS[csvid]
    if record.get('mana_cost') != mana or record.get('type_line', '').split(' — ')[0] != kind:
        raise ValueError('unsupported activated coin-flip primary type or mana cost')
    return dict(csvid=csvid, printed_cost=mana)


def variants(shape):
    return [f'{mode}_{player}' for player in (0, 1) for mode in MODES[shape['csvid']]]


def validate_variants(shape, selected):
    if (type(selected) is not list or not selected or any(type(item) is not str for item in selected)
            or len(selected) != len(set(selected)) or set(selected) - set(variants(shape))):
        raise ValueError('distinct supported activated coin-flip variants required')


def spec(declaration, record):
    pin = profiles()['sources'].get(record.get('name'))
    if not pin or not exact(pin['oracle'], record):
        raise ValueError('exact pinned activated coin-flip primary required')
    shape = classify(record, pin['registry']['csvid'])
    if (declaration.get('family') != FAMILY or type(declaration.get('version')) is not int
            or declaration['version'] != 2 or declaration.get('oracle_text') != record['oracle_text']
            or not exact(declaration.get('params'), shape)):
        raise ValueError('exact typed activated coin-flip declaration required')
    validate_variants(shape, declaration.get('variants'))
    return dict(shape, family=FAMILY)


def validate_witnesses(oracle, rows):
    pins = profiles()
    if (set(pins) != {'sources', 'witnesses'} or set(pins['witnesses']) != set(WITNESSES)
            or {pin['registry']['csvid'] for pin in pins['sources'].values()} != CARDS.keys()):
        raise ValueError('exact activated coin-flip source and witness scope required')
    for group, entries in pins.items():
        for name, pin in entries.items():
            row = rows.get(name, {})
            if (not exact(oracle.get(name), pin['oracle']) or not exact(row, pin['registry'])
                    or row.get('kind') != 'card' or row.get('handler') in (None, 'NULL', '0')
                    or oracle[name].get('layout') != 'normal'):
                raise ValueError('activated coin-flip Oracle or registry changed: ' + name)
            if group == 'sources':
                if row['handler'] == 'card_dummy':
                    raise ValueError('implemented activated coin-flip primary required')
                classify(oracle[name], row['csvid'])


class Board:
    def __init__(self, source, actor, mode, records, oracle, face):
        self.source, self.actor, self.foe, self.mode = source, actor, 1 - actor, mode
        self.records, self.oracle, self.seeded_face = records, oracle, face
        self.setup, self.steps, self.cards, self.flips = [], [], {}, []
        self.life = [20, 20]
        self.pools = [None, None]
        self.csvid = records[source]['csvid']
        self.add('source', actor, source, 'hand')
        if self.csvid in (SCALES, LYRE):
            if mode != 'win_zero':
                self.add('own_mark', actor, 'Grizzly Bears', 'battlefield')
            if mode != 'lose_zero':
                self.add('foe_mark', self.foe, 'Hill Giant', 'battlefield')
        if self.csvid == LYRE:
            if mode != 'win_zero':
                self.add('own_extra', actor, 'Squire', 'battlefield')
            if mode != 'lose_zero':
                for i in range(2):
                    self.add(f'foe_extra_{i}', self.foe, 'Squire', 'battlefield')
            if mode.endswith('_walker'):
                self.add('walker', self.foe, 'Jace Beleren', 'hand')
        if self.csvid == BANG and mode in ('win_creature', 'win_own', 'win_prevented'):
            target_name = 'Cho-Manno, Revolutionary' if mode == 'win_prevented' else 'Grizzly Bears'
            self.add('foe_mark', actor if mode == 'win_own' else self.foe, target_name, 'battlefield')
        if self.csvid == MANA and mode in ('win', 'response_win', 'colored_fee', 'payment_gate'):
            self.add('sink', actor, 'Millstone', 'hand')
        if self.csvid == MANA and mode == 'colored_fee':
            self.add('fee_land', actor, 'Forest', 'battlefield')
        if self.csvid == MANA and mode == 'response_win':
            self.add('shock', self.foe, 'Shock', 'hand')

    def add(self, ref, player, name, zone):
        self.setup.append(card(name, ref, player, zone))
        self.cards[ref] = dict(name=name, player=player, zone=zone)

    def emit(self, op, ref='', other='', prop='', *, player=0, value=0, zone=0, index=0, color=0):
        if op in ('untap_step', 'upkeep_step'):
            self.steps.append(step(op, player=player))
            return
        if op == 'activate_response':
            self.steps.append(step(op, ref=ref, value=value))
            return
        if op == 'assert_counter':
            self.steps.append(step(op, ref=ref, counter_kind=prop, value=value))
            return
        fields = dict(player=player, value=value, index=index, color=color)
        if ref:
            fields['ref'] = ref
        if other:
            fields['other'] = other
        if prop:
            fields['property'] = prop
        if op.startswith('assert_zone'):
            fields['zone'] = ZONES[zone]
        self.steps.append(step(op, **fields))

    def observe(self, ref, prop, value):
        self.emit('assert_card', ref, prop=prop, value=value)

    def mana(self, player, amounts, *, set_pool=False):
        if isinstance(amounts, int):
            amounts = [amounts, 0, 0, 0, 0, 0]
        for color, value in enumerate(amounts):
            self.emit('set_mana' if set_pool else 'assert_mana', player=player, color=color, value=value)
        if set_pool:
            self.pools[player] = amounts

    def fund(self, player, amounts):
        self.mana(player, amounts, set_pool=True)
        self.emit('refresh')

    def choices(self):
        self.emit('assert_unused_choices')
        self.emit('assert_choices_consumed')

    def receipts(self, stack=0):
        self.emit('assert_stack', value=stack)
        for player in (0, 1):
            self.emit('assert_life', player=player, value=self.life[player])
            self.mana(player, self.pools[player])
            for zone, zone_id in ZONE.items():
                names = [entry['name'] for entry in self.cards.values()
                         if entry['player'] == player and entry['zone'] == zone]
                self.emit('assert_zone', player=player, zone=zone_id, value=len(names))
                for name, count in Counter(names).items():
                    self.emit('assert_zone_multiplicity', player=player, zone=zone_id,
                              index=self.records[name]['csvid'], value=count)

    def change(self, ref, zone):
        self.cards[ref]['zone'] = zone

    def spell(self, ref, destination, *, picked=None, fund=True, pending=False):
        entry = self.cards[ref]
        player = entry['player']
        if fund:
            self.fund(player, cost_from_oracle(self.oracle[entry['name']]))
        if picked is not None:
            self.emit('choose_player', player=player, value=picked)
        self.emit('cast', ref, value=1)
        self.emit('assert_stack', value=1)
        self.pools[player] = 0
        self.mana(player, 0)
        if pending:
            self.emit('announce_spell', ref)
        else:
            self.emit('resolve')
            self.choices()
            self.change(ref, destination)

    def flip(self, win):
        call = self.seeded_face ^ int(not win)
        self.emit('choose_dialog', player=self.actor, value=call)
        self.flips.append(dict(call=call, planned_win=win))

    def begin_ability(self, fee=0):
        self.emit('can_activate', 'source', value=1)
        self.emit('activate', 'source', value=1)
        self.emit('assert_stack', value=1)
        if fee:
            self.pools[self.actor][0] -= fee
            self.mana(self.actor, self.pools[self.actor])

    def finish_ability(self):
        self.emit('resolve')
        self.choices()
        self.emit('refresh')

    def scales(self):
        available = 7 if self.mode in ('pay_retry', 'decline_retry', 'replay') else 4
        if self.mode == 'short':
            self.fund(self.actor, 3)
            self.emit('can_activate', 'source', value=0)
            self.emit('activate', 'source', value=0)
            self.observe('source', 'tapped', 0)
            self.choices()
            self.receipts()
            return
        self.fund(self.actor, available)
        self.emit('choose_target', 'foe_mark', player=self.actor)
        self.emit('choose_target', 'own_mark', player=self.actor)
        self.begin_ability(4)
        self.observe('source', 'tapped', 1)
        self.flip(self.mode == 'win')
        if self.mode in ('pay_retry', 'replay'):
            self.emit('choose_dialog', player=self.actor, value=0)
            self.flip(True)
            self.pools[self.actor][0] -= 3
        elif self.mode == 'decline_retry':
            self.emit('choose_dialog', player=self.actor, value=1)
        if self.mode == 'replay':
            self.emit('capture')
        self.finish_ability()
        self.change('foe_mark' if self.mode in ('win', 'pay_retry', 'replay') else 'own_mark', 'graveyard')
        self.observe('source', 'tapped', 1)
        self.receipts()
        if self.mode == 'replay':
            self.emit('record_state')
            self.emit('restore')
            self.finish_ability()
            self.observe('source', 'tapped', 1)
            self.receipts()
            self.emit('assert_recorded_state')

    def bang(self):
        self.emit('can_activate', 'source', value=0)
        for player in (self.foe, self.actor):
            self.emit('advance_turn')
            self.emit('untap_step', player=player)
            self.emit('upkeep_step', player=player)
        self.emit('turn', player=self.actor)
        self.observe('source', 'sick', 0)
        self.observe('source', 'power', 2)
        self.observe('source', 'toughness', 2)
        self.flip(self.mode != 'lose')
        if self.mode == 'win_player':
            self.emit('choose_player', player=self.actor, value=self.foe)
        elif self.mode in ('win_creature', 'win_own', 'win_prevented'):
            self.emit('choose_target', 'foe_mark', player=self.actor)
        self.begin_ability()
        self.observe('source', 'tapped', 1)
        self.finish_ability()
        if self.mode == 'lose':
            self.change('source', 'graveyard')
        elif self.mode == 'win_player':
            self.life[self.foe] -= 2
        elif self.mode == 'win_prevented':
            self.observe('foe_mark', 'damage', 0)
        else:
            self.change('foe_mark', 'graveyard')
        self.receipts()

    def lyre(self):
        walker = self.mode.endswith('_walker')
        if walker:
            self.emit('choose_target', 'walker', player=self.actor)
        else:
            self.emit('choose_player', player=self.actor, value=self.foe)
        self.begin_ability()
        self.change('source', 'graveyard')
        self.receipts(stack=1)
        win = self.mode.startswith('win')
        self.flip(win)
        self.finish_ability()
        if not win:
            self.life[self.actor] -= 0 if self.mode == 'lose_zero' else 3
        elif not walker:
            self.life[self.foe] -= 0 if self.mode == 'win_zero' else 2
        if walker:
            self.emit('assert_counter', 'walker', prop='loyalty', value=1 if win else 3)
        self.receipts()

    def mana_screw(self):
        if self.mode == 'colored_fee':
            self.mana_screw_colored_fee()
            return
        if self.mode == 'payment_gate':
            self.fund(self.actor, 1)
            self.emit('can_activate', 'source', value=1)
            self.emit('cast', 'sink', value=0)
            self.choices()
            self.receipts()
            return
        if self.mode == 'short':
            self.emit('can_activate', 'source', value=0)
            self.emit('activate', 'source', value=0)
            self.receipts()
            return
        pending = self.mode == 'response_win'
        if pending:
            self.emit('turn', player=self.foe)
            self.spell('shock', 'graveyard', picked=self.actor, pending=True)
        self.fund(self.actor, 1)
        self.flip(self.mode != 'lose')
        if pending:
            self.emit('activate_response', 'source', value=1)
        else:
            self.emit('can_activate', 'source', value=1)
            self.emit('activate', 'source', value=1)
        # The instant-only restriction controls when it may be activated.
        # The mana ability resolves immediately and leaves any parent pending.
        self.emit('assert_stack', value=int(pending))
        self.pools[self.actor] = 0 if self.mode == 'lose' else 2
        self.mana(self.actor, self.pools[self.actor])
        self.finish_ability()
        if pending:
            self.change('shock', 'graveyard')
            self.life[self.actor] -= 2
        if self.mode != 'lose':
            self.pools[self.actor] = 2
        self.receipts()
        if 'sink' in self.cards:
            self.emit('turn', player=self.actor)
            self.spell('sink', 'battlefield', fund=False)
            self.receipts()

    def mana_screw_colored_fee(self):
        self.emit('activate', 'fee_land', value=1)
        self.pools[self.actor] = [0, 0, 0, 1, 0, 0]
        self.observe('fee_land', 'tapped', 1)
        self.receipts()
        self.emit('refresh')
        if self.actor == 0:
            self.emit('choose_mana', player=0, color=3)
        self.flip(True)
        self.emit('can_activate', 'source', value=1)
        self.emit('activate', 'source', value=1)
        self.emit('assert_stack')
        self.mana(self.actor, 2)
        self.finish_ability()
        self.pools[self.actor] = 2
        self.receipts()
        self.spell('sink', 'battlefield', fund=False)
        self.receipts()

    def build(self):
        self.emit('turn', player=self.actor)
        for player in (0, 1):
            self.mana(player, 0, set_pool=True)
        if 'walker' in self.cards:
            self.emit('turn', player=self.foe)
            self.spell('walker', 'battlefield')
            self.emit('assert_counter', 'walker', prop='loyalty', value=3)
            self.emit('turn', player=self.actor)
        self.spell('source', 'battlefield')
        self.receipts()
        {SCALES: self.scales, BANG: self.bang, LYRE: self.lyre, MANA: self.mana_screw}[self.csvid]()


def expand(name, csvid, declaration, shape, selected, rows, oracle):
    validate_variants(shape, selected)
    validate_witnesses(oracle, rows)
    if shape['csvid'] != csvid or profiles()['sources'][name]['registry']['csvid'] != csvid:
        raise ValueError('exact activated coin-flip primary CSVID required')
    cases = []
    for variant in selected:
        mode, player = variant.rsplit('_', 1)
        player = int(player)
        board = Board(name, player, mode, rows, oracle, player)
        board.build()
        cases.append(dict(name=f'CoinFlipActivatedOutcomeRecipe_{csvid}_{variant}', card=name,
                          fixture='empty_main', seed=SEEDS[player], setup=board.setup, steps=board.steps,
                          _recipe=dict(family=FAMILY, version=2, variant=variant, params=declaration['params'],
                                       oracle_text=declaration['oracle_text'],
                                       untested_components=list(GAPS.get(csvid, ())))))
    return cases
