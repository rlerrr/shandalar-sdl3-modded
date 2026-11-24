"""Paid Equipment attachment and its printed combat-damage riders."""
from collections import Counter
from functools import lru_cache
import json
from pathlib import Path

from counter_condition_recurrence_recipes import exact
from recipes import card, cost_from_oracle, step

FAMILY = 'equipment_combat_damage_hand_and_cast'
PROFILE = Path(__file__).with_name('data') / 'equipment_combat_damage_hand_and_cast_wave8_profiles.json'

MASK, SHROUD, BINDER, KEY = 6720, 6961, 6962, 15956
ZONES = {'hand': 0, 'battlefield': 1, 'library': 2, 'graveyard': 3, 'exile': 4}
EQUIP = {MASK: 1, SHROUD: 1, BINDER: 4, KEY: 2}
COMMON = ('hit', 'noncombat', 'blocked', 'bare', 'short_equip')
MIRRORED = {
    MASK: COMMON,
    SHROUD: (*COMMON, 'empty_hand'),
    BINDER: (*COMMON, 'wrong_type', 'repeat', 'cast_trigger'),
    KEY: (*COMMON, 'skulk'),
}
HUMAN = {
    MASK: ('selected_discard', 'decline'),
    BINDER: ('decline_imprint', 'decline_copy'),
    KEY: ('selected_discard', 'decline'),
}
WITNESSES = frozenset(('Divination', 'Forest', 'Grizzly Bears', 'Guttersnipe', 'Hill Giant', 'Island', 'Mountain', 'Plains', 'Prodigal Sorcerer', 'Think Twice', 'Wall of Wood'))

ZONE_NAMES = {zone: name for name, zone in ZONES.items()}
GAPS = {
    MASK: ('Optional trigger decline is exercised for the human seat; the AI accepts automatically.',),
    BINDER: ('The production copy helper offers no separate post-copy cast decline. Imprint decline and copy decline have their own cases.',),
    KEY: ('Optional trigger decline is exercised for the human seat; the AI accepts automatically.',),
}


@lru_cache(maxsize=1)
def profiles():
    return json.loads(PROFILE.read_bytes())


def classify(record, csvid):
    if (csvid not in EQUIP or record.get('layout') != 'normal' or record.get('card_faces')
            or record.get('selected_face') or record.get('type_line') != 'Artifact — Equipment'
            or set(record.get('keywords', ())) != ({'Equip', 'Imprint'} if csvid == BINDER else {'Equip'})
            or not record['oracle_text'].splitlines()[-1].startswith('Equip {' + str(EQUIP[csvid]) + '}')):
        raise ValueError('normal paid Equipment with validated damage rider required')
    return dict(csvid=csvid, equip_generic=EQUIP[csvid], printed_cost=record['mana_cost'])


def variants(shape):
    csvid = shape['csvid']
    return [f'{mode}_{actor}' for actor in (0, 1)
            for mode in MIRRORED[csvid] + (HUMAN.get(csvid, ()) if actor == 0 else ())]


def validate_variants(shape, selected):
    if (type(selected) is not list or not selected or any(type(item) is not str for item in selected)
            or len(set(selected)) != len(selected) or set(selected) - set(variants(shape))):
        raise ValueError('distinct supported Equipment damage variants required')


def spec(declaration, record):
    pin = profiles()['sources'].get(record.get('name'))
    if not pin or not exact(pin['oracle'], record):
        raise ValueError('exact pinned Equipment damage primary required')
    shape = classify(record, pin['registry']['csvid'])
    if (declaration.get('family') != FAMILY or type(declaration.get('version')) is not int
            or declaration['version'] != 2 or declaration.get('oracle_text') != record['oracle_text']
            or not exact(declaration.get('params'), shape)):
        raise ValueError('exact typed Equipment damage declaration required')
    validate_variants(shape, declaration.get('variants'))
    return dict(shape, family=FAMILY)


def validate_witnesses(oracle, rows):
    pins = profiles()
    if (set(pins) != {'sources', 'witnesses'} or set(pins['witnesses']) != WITNESSES
            or {pin['registry']['csvid'] for pin in pins['sources'].values()} != EQUIP.keys()):
        raise ValueError('exact Equipment damage sources and witnesses required')
    for group, entries in pins.items():
        for name, pin in entries.items():
            row = rows.get(name, {})
            if (not exact(oracle.get(name), pin['oracle']) or not exact(row, pin['registry'])
                    or row.get('kind') != 'card' or row.get('handler') in (None, '0', 'NULL')
                    or oracle[name].get('layout') != 'normal'):
                raise ValueError('Equipment damage Oracle or registry changed: ' + name)
            if group == 'sources':
                if row['handler'] == 'card_dummy':
                    raise ValueError('implemented Equipment damage primary required')
                classify(oracle[name], row['csvid'])


def expand(name, csvid, declaration, shape, selected, rows, oracle):
    validate_variants(shape, selected)
    validate_witnesses(oracle, rows)
    if shape['csvid'] != csvid or profiles()['sources'][name]['registry']['csvid'] != csvid:
        raise ValueError('exact Equipment damage primary CSVID required')
    cases = []
    for variant in selected:
        mode, seat = variant.rsplit('_', 1)
        board = Board(oracle[name], int(seat), mode, rows, oracle)
        board.build()
        cases.append(dict(name=f'EquipmentDamageHandCastRecipe_{csvid}_{variant}', card=name,
                          fixture='empty_main', seed=2660, setup=board.setup, steps=board.steps,
                          _recipe=dict(family=FAMILY, version=2, variant=variant, params=declaration['params'],
                                       oracle_text=declaration['oracle_text'],
                                       untested_components=list(GAPS.get(csvid, ())))))
    return cases


class Board:
    def __init__(self, record, owner, mode, rows, oracle):
        self.name, self.owner, self.foe, self.mode = record['name'], owner, 1 - owner, mode
        self.id, self.rows, self.oracle = rows[self.name]['csvid'], rows, oracle
        self.setup, self.steps, self.cards, self.witnesses = [], [], {}, set()
        self.life, self.mana = [20, 20], [[0] * 6, [0] * 6]
        self.equipped = False
        self.add('source', self.name, owner, 'hand')
        self.add('host', 'Prodigal Sorcerer' if mode == 'noncombat' else 'Grizzly Bears', owner)
        self.add('blocker', 'Hill Giant' if mode == 'skulk' else 'Wall of Wood', self.foe)
        self.library = ['Plains', 'Island' if mode == 'selected_discard' else 'Plains', 'Mountain']
        for index, name in enumerate(self.library):
            self.add(f'library_{index}', name, owner, 'library')
        self.add('foe_top', 'Mountain', self.foe, 'library')
        if self.id == SHROUD and mode != 'empty_hand':
            self.add('discardable', 'Forest', self.foe, 'hand')
        if self.id == KEY and mode == 'selected_discard':
            self.add('discardable', 'Forest', owner, 'hand')
        if self.id == BINDER:
            self.add('rejected', 'Divination', owner, 'hand')
            if mode != 'wrong_type':
                self.add('imprinted', 'Think Twice', owner, 'hand')
            if mode == 'cast_trigger':
                self.add('cast_witness', 'Guttersnipe', owner)

    def add(self, ref, name, actor, zone='battlefield'):
        self.setup.append(card(name, ref, actor, zone))
        self.cards[ref] = [name, actor, zone]
        if ref != 'source':
            self.witnesses.add(name)

    def emit(self, op, ref='', other='', prop='', *, player=0, value=0, zone=0, index=0, color=0):
        if op in ('untap_step', 'upkeep_step'):
            self.steps.append(step(op, player=player))
            return
        if op == 'choose_trigger_source':
            self.steps.append(step(op, ref=ref, player=player))
            return
        fields = dict(player=player, value=value, index=index, color=color)
        if ref:
            fields['ref'] = ref
        if other:
            fields['other'] = other
        if prop:
            fields['property'] = prop
        if op.startswith('assert_zone'):
            fields['zone'] = ZONE_NAMES[zone]
        self.steps.append(step(op, **fields))

    def observe(self, ref, prop, value):
        self.emit('assert_card', ref, prop=prop, value=value)

    def choices(self):
        self.emit('assert_unused_choices', value=0)
        self.emit('assert_choices_consumed')

    def wallet(self, player, values, *, setting=False):
        for color, value in enumerate(values):
            self.emit('set_mana' if setting else 'assert_mana', player=player, value=value, color=color)
        if setting:
            self.mana[player] = list(values)

    def fund(self, price):
        self.wallet(self.owner, price, setting=True)
        self.emit('refresh')

    def spend(self, price):
        self.mana[self.owner] = [have - fee for have, fee in zip(self.mana[self.owner], price, strict=True)]
        assert min(self.mana[self.owner]) >= 0
        self.wallet(self.owner, self.mana[self.owner])

    def attach(self):
        price = cost_from_oracle(self.oracle[self.name])
        self.fund(price)
        self.emit('cast', 'source', value=1)
        self.emit('assert_stack', value=1)
        self.spend(price)
        if self.id == BINDER:
            self.emit('observe_search_filters', value=1)
            if self.mode in ('decline_imprint', 'wrong_type'):
                self.emit('choose_zone', player=self.owner, value=-1)
            else:
                self.emit('choose_zone_card', player=self.owner, value=self.rows['Think Twice']['csvid'])
        self.emit('resolve')
        self.emit('refresh')
        self.choices()
        self.cards['source'][2] = 'battlefield'
        if self.id == BINDER:
            self.emit('assert_offered_selection_required', value=0)
            self.emit('assert_offered_legal_card', index=self.rows['Divination']['csvid'], value=0)
            if 'imprinted' in self.cards:
                self.emit('assert_offered_legal_card', index=self.rows['Think Twice']['csvid'], value=1)
                if self.mode != 'decline_imprint':
                    self.cards['imprinted'][2] = 'exile'
            self.emit('observe_search_filters')
        self.emit('assert_stack')
        self.emit('can_activate', 'source', value=0)
        if self.mode == 'bare':
            return
        fee = [EQUIP[self.id], 0, 0, 0, 0, 0]
        if self.mode == 'short_equip':
            self.fund([fee[0]-1, 0, 0, 0, 0, 0])
            self.emit('can_activate', 'source', value=0)
            return
        self.fund(fee)
        self.emit('can_activate', 'source', value=1)
        self.emit('choose_target', 'host', player=self.owner)
        self.emit('activate', 'source', value=1)
        self.choices()
        self.emit('assert_stack', value=1)
        self.spend(fee)
        self.emit('resolve')
        self.emit('refresh')
        self.choices()
        self.equipped = True

    def draw(self, count):
        still_library = [ref for ref, (_, actor, zone) in self.cards.items()
                         if actor == self.owner and zone == 'library']
        assert len(still_library) >= count
        for ref in still_library[:count]:
            self.cards[ref][2] = 'hand'

    def discard(self, actor):
        options = [ref for ref, (_, player, zone) in self.cards.items() if player == actor and zone == 'hand']
        if not options:
            return
        if actor == 0:
            selected = min(options, key=lambda ref: self.rows[self.cards[ref][0]]['csvid'])
        else:
            assert len({self.cards[ref][0] for ref in options}) == 1
            selected = options[0]
        self.cards[selected][2] = 'graveyard'

    def trigger(self):
        if self.id == SHROUD:
            self.discard(self.foe)
            return
        if self.id == BINDER and (self.mode == 'wrong_type' or self.cards['imprinted'][2] != 'exile'):
            return
        if self.mode in ('decline', 'decline_copy'):
            assert self.owner == 0
            return
        if self.owner == 0:
            self.emit('choose_trigger_source', 'source', player=0)
        if self.id == BINDER:
            self.draw(1)
            if self.mode == 'cast_trigger':
                self.life[self.foe] -= 2
        else:
            self.draw(2 if self.id == MASK else 1)
            self.discard(self.owner)

    def damage(self):
        if self.mode == 'noncombat':
            self.emit('can_activate', 'host', value=1)
            self.emit('choose_player', player=self.owner, value=self.foe)
            self.emit('activate', 'host', value=1)
            self.emit('assert_stack', value=1)
            self.choices()
            self.emit('resolve')
            self.life[self.foe] -= 1
        else:
            self.emit('begin_attackers')
            self.emit('attack', 'host', value=1)
            self.emit('finish_attackers')
            self.emit('begin_blockers')
            if self.mode in ('blocked', 'skulk'):
                self.emit('block', 'blocker', 'host', value=int(self.mode == 'blocked'))
                self.observe('blocker', 'blocking_count', int(self.mode == 'blocked'))
            self.emit('advance_combat')
            if self.mode == 'blocked':
                blocker = self.cards['blocker']
                if self.host_stats()[0] >= int(self.oracle[blocker[0]]['toughness']):
                    blocker[2] = 'graveyard'
            else:
                self.life[self.foe] -= self.host_stats()[0]
                if self.equipped:
                    self.trigger()
            self.emit('combat_damage')
            self.emit('resolve')
            self.emit('end_combat')
        self.choices()
        self.emit('refresh')
        self.packet()

    def host_stats(self):
        name = self.cards['host'][0]
        base = self.oracle[name]
        power = int(base['power']) + int(self.equipped and self.id == SHROUD)
        return power, int(base['toughness'])

    def packet(self):
        self.emit('assert_stack')
        self.observe('source', 'in_play', 1)
        for prop, value in zip(('power', 'toughness'), self.host_stats(), strict=True):
            self.observe('host', prop, value)
        for actor in (0, 1):
            self.emit('assert_life', player=actor, value=self.life[actor])
            self.wallet(actor, self.mana[actor])
            for zone, encoded in ZONES.items():
                counts = Counter(name for name, owner, area in self.cards.values() if owner == actor and area == zone)
                self.emit('assert_zone', player=actor, zone=encoded, value=counts.total())
                for name in sorted(counts):
                    self.emit('assert_zone_multiplicity', player=actor, zone=encoded,
                              index=self.rows[name]['csvid'], value=counts[name])

    def next_turn(self, player):
        self.emit('advance_turn')
        self.emit('untap_step', player=player)
        self.emit('upkeep_step', player=player)
        self.emit('turn', player=player)

    def build(self):
        self.emit('turn', player=self.owner)
        for player in (0, 1):
            self.wallet(player, self.mana[player], setting=True)
        self.emit('discard_policy', value=1)
        self.emit('refresh')
        self.attach()
        self.packet()
        self.damage()
        if self.mode == 'repeat':
            self.next_turn(self.foe)
            self.next_turn(self.owner)
            self.damage()
