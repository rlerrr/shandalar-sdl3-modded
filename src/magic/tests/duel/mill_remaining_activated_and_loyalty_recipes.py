"""Paid loyalty and resource activations with exact Mill outcomes."""
from collections import Counter, defaultdict
from functools import lru_cache
import json
from pathlib import Path

from counter_condition_recurrence_recipes import exact
from recipes import card, cost_from_oracle, step

FAMILY = 'mill_remaining_activated_and_loyalty'
PROFILE = Path(__file__).with_name('data') / 'mill_remaining_activated_and_loyalty_wave8_profiles.json'
HELM, HERETIC = 2297, 11984
AIR, JACE, UNSUMMON = 'Air Elemental', 'Jace Beleren', 'Unsummon'

HOPE, MAJESTY, DEATH, ESTRID = 16143, 16962, 18958, 19091
MODELS = {
    HOPE: (3, (1, -2, -7), ('weaken', 'no_target', 'mill_return', 'mill_decline', 'emblem')),
    MAJESTY: (5, (1, -3, -7), ('token', 'short_mill', 'reanimate', 'wipe')),
    DEATH: (4, (1, -2, -3), ('drain', 'miss', 'shrink', 'permission')),
    ESTRID: (3, (2, -1, -7), ('untap', 'mask', 'replenish', 'replenish_short')),
    HELM: (0, (), ('creature_first', 'creature_second', 'limit', 'short_deck', 'zero')),
    HERETIC: (0, (), ('player', 'creature', 'walker', 'land_only', 'short_deck', 'lost_target')),
}
ZONES = {'hand': 0, 'battlefield': 1, 'library': 2, 'graveyard': 3}
DECK = ('Forest', 'Plains', 'Mountain', 'Swamp', 'Island', 'Eager Cadet', 'Forest', 'Mountain')
BEAR, GIANT, SCATHE, CORPSE = 'Grizzly Bears', 'Hill Giant', 'Scathe Zombies', 'Walking Corpse'
HOLY, ANTHEM, MURDER = 'Holy Strength', 'Glorious Anthem', 'Murder'
ZOMBIE, MASK = 'Zombie', 'Mask'
ZERO = [0] * 6
WITNESSES = frozenset(('Air Elemental', 'Eager Cadet', 'Forest', 'Glorious Anthem', 'Grizzly Bears', 'Hill Giant', 'Holy Strength', 'Island', 'Jace Beleren', 'Mask', 'Mountain', 'Murder', 'Plains', 'Scathe Zombies', 'Swamp', 'Unsummon', 'Walking Corpse', 'Zombie'))

ZONE_NAMES = {code: name for name, code in ZONES.items()}
COSTS = {HOPE: '{1}{B}{B}', MAJESTY: '{3}{B}{B}', DEATH: '{2}{B}{B}', ESTRID: '{1}{G}{W}{U}',
         HELM: '{4}', HERETIC: '{4}{R}'}
WALKERS = {HOPE, MAJESTY, DEATH, ESTRID}
GAPS = {
    ESTRID: ('Commander deck configuration is outside this two-seat duel fixture.',),
    DEATH: ('The paid -3 resolves. Casting an actual Zombie through its unattached activated legacy '
            'needs a selector for that dedicated source.',),
}


@lru_cache(maxsize=1)
def profiles():
    return json.loads(PROFILE.read_bytes())


def classify(record, csvid):
    if (csvid not in COSTS or record.get('layout') != 'normal' or record.get('card_faces')
            or record.get('selected_face') or record.get('mana_cost') != COSTS[csvid]
            or record.get('keywords') != ['Mill']
            or ('Planeswalker' in record['type_line']) != (csvid in WALKERS)):
        raise ValueError('normal whole Mill activated or loyalty source required')
    return dict(csvid=csvid, printed_cost=record['mana_cost'])


def variants(shape):
    return [f'{mode}_{seat}' for seat in (0, 1) for mode in MODELS[shape['csvid']][2]]


def validate_variants(shape, selected):
    if (type(selected) is not list or not selected or any(type(item) is not str for item in selected)
            or len(set(selected)) != len(selected) or set(selected) - set(variants(shape))):
        raise ValueError('distinct supported Mill activated and loyalty variants required')


def spec(declaration, record):
    pin = profiles()['sources'].get(record.get('name'))
    if not pin or not exact(pin['oracle'], record):
        raise ValueError('exact pinned Mill activated or loyalty primary required')
    shape = classify(record, pin['registry']['csvid'])
    if (declaration.get('family') != FAMILY or type(declaration.get('version')) is not int
            or declaration['version'] != 2 or declaration.get('oracle_text') != record['oracle_text']
            or not exact(declaration.get('params'), shape)):
        raise ValueError('exact typed Mill declaration required')
    validate_variants(shape, declaration.get('variants'))
    return dict(shape, family=FAMILY)


def validate_witnesses(oracle, rows):
    pins = profiles()
    if (set(pins) != {'sources', 'witnesses'} or set(pins['witnesses']) != WITNESSES
            or {pin['registry']['csvid'] for pin in pins['sources'].values()} != COSTS.keys()):
        raise ValueError('exact Mill sources and witnesses required')
    for group, entries in pins.items():
        for name, pin in entries.items():
            row = rows.get(name, {})
            if (not exact(oracle.get(name), pin['oracle']) or not exact(row, pin['registry'])
                    or row.get('kind') != 'card' or row.get('handler') in (None, '0', 'NULL')
                    or oracle[name].get('layout') != ('token' if name in (ZOMBIE, MASK) else 'normal')):
                raise ValueError('Mill Oracle or registry changed: ' + name)
            if group == 'sources':
                if row['handler'] == 'card_dummy':
                    raise ValueError('implemented Mill primary required')
                classify(oracle[name], row['csvid'])


def expand(name, csvid, declaration, shape, selected, rows, oracle):
    validate_variants(shape, selected)
    validate_witnesses(oracle, rows)
    if shape['csvid'] != csvid or profiles()['sources'][name]['registry']['csvid'] != csvid:
        raise ValueError('exact Mill primary CSVID required')
    cases = []
    for variant in selected:
        mode, seat = variant.rsplit('_', 1)
        board = Board(rows[name], int(seat), mode, rows, oracle)
        board.run()
        cases.append(dict(name=f'MillRemainingActivatedLoyaltyRecipe_{csvid}_{variant}', card=name,
                          fixture='empty_main', seed=2809, setup=board.setup, steps=board.steps,
                          _recipe=dict(family=FAMILY, version=2, variant=variant, params=declaration['params'],
                                       oracle_text=declaration['oracle_text'], untested_components=list(GAPS.get(csvid, ())))))
    return cases


class Board:
    def __init__(self, source, owner, mode, rows, oracle):
        self.id, self.name, self.owner, self.foe, self.mode = source['csvid'], source['name'], owner, 1-owner, mode
        self.rows, self.oracle = rows, oracle
        self.setup, self.steps, self.cards, self.libraries = [], [], {}, [[], []]
        self.life, self.pools, self.active = [20, 20], [ZERO.copy(), ZERO.copy()], owner
        self.loyalty, self.costs, _ = MODELS[self.id]
        self.zombies = self.masks = 0
        self.add('source', self.name, owner, 'hand')
        self.add('grave_land', 'Swamp', owner, 'graveyard')
        self.add('foe_grave_land', 'Plains', self.foe, 'graveyard')
        library = foe_library = DECK
        if self.id == HOPE:
            if mode in ('weaken', 'no_target', 'emblem'):
                self.add('host', GIANT, self.foe)
            if mode in ('mill_return', 'mill_decline'):
                self.add('older', SCATHE, owner, 'graveyard')
                library = (BEAR, *DECK)
            if mode == 'emblem':
                self.add('owned_zombie', CORPSE, owner)
        elif self.id == MAJESTY:
            if mode == 'short_mill':
                library = ('Forest',)
            if mode == 'reanimate':
                self.add('target', BEAR, owner, 'graveyard')
                self.add('foe_creature_card', GIANT, self.foe, 'graveyard')
            if mode == 'wipe':
                self.add('own_non_zombie', BEAR, owner)
                self.add('foe_non_zombie', GIANT, self.foe)
                self.add('foe_zombie', SCATHE, self.foe)
        elif self.id == DEATH:
            if mode == 'drain':
                library = (SCATHE, CORPSE, *DECK)
            if mode == 'shrink':
                self.add('host', GIANT, self.foe)
                self.add('zombie_0', SCATHE, owner)
                self.add('zombie_1', CORPSE, owner)
                self.add('foreign_zombie', SCATHE, self.foe)
            if mode == 'permission':
                self.add('zombie_card', SCATHE, owner, 'graveyard')
        elif self.id == ESTRID:
            self.add('host', BEAR, owner, tapped=mode == 'untap')
            self.add('foe_host', GIANT, self.foe, tapped=mode == 'untap')
            if mode == 'untap':
                self.add('idle', GIANT, owner, tapped=True)
                self.add('own_aura', HOLY, owner, 'hand')
                self.add('foe_aura', HOLY, owner, 'hand')
            if mode == 'mask':
                self.add('removal', MURDER, self.foe, 'hand')
            if mode == 'replenish':
                library = (ANTHEM, HOLY, *DECK)
            if mode == 'replenish_short':
                self.add('old_anthem', ANTHEM, owner, 'graveyard')
                self.add('old_aura', HOLY, owner, 'graveyard')
                library = (BEAR, 'Island')
        else:
            foe_library = self.build_activated()
            if self.id == HERETIC:
                if mode == 'short_deck':
                    library = (BEAR,)
                elif mode != 'land_only':
                    library = ('Forest', GIANT, BEAR, *DECK)
        for player in (0, 1):
            for index, name in enumerate(library if player == owner else foe_library):
                ref = f'library_{player}_{index}'
                self.add(ref, name, player, 'library')
                self.libraries[player].append(ref)

    def add(self, ref, name, player, zone='battlefield', *, tapped=False):
        assert player in (0, 1)
        row = card(name, ref, player, zone)
        if tapped:
            row['state'] = ['tapped']
        self.setup.append(row)
        self.cards[ref] = dict(name=name, player=player, zone=zone, tapped=tapped, adjustment=(0, 0))

    def emit(self, op, ref='', other='', prop='', *, player=0, value=0, zone=0, index=0, color=0):
        if op in ('untap_step', 'upkeep_step'):
            self.steps.append(step(op, player=player))
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
            fields['zone'] = ZONE_NAMES[zone]
        self.steps.append(step(op, **fields))

    def observe(self, ref, prop, value):
        self.emit('assert_card', ref, prop=prop, value=int(value))

    def choices(self):
        self.emit('assert_unused_choices')
        self.emit('assert_choices_consumed')

    def wallet(self, player, amounts, *, funding=False):
        for color, value in enumerate(amounts):
            self.emit('set_mana' if funding else 'assert_mana', player=player, value=value, color=color)
        if funding:
            self.pools[player] = list(amounts)
            self.emit('refresh')

    def resolve(self):
        self.emit('resolve')
        self.emit('refresh')
        self.choices()
        self.emit('assert_stack')

    def cast(self, ref, destination, *, target=None, stack=1):
        item = self.cards[ref]
        actor = item['player']
        self.wallet(actor, cost_from_oracle(self.oracle[item['name']]), funding=True)
        if target is not None:
            self.emit('choose_target', target, player=actor)
        self.emit('cast', ref, value=1)
        self.choices()
        self.emit('assert_stack', value=stack)
        self.pools[actor] = ZERO.copy()
        self.wallet(actor, ZERO)
        self.resolve()
        item['zone'] = destination

    def observe_loyalty(self):
        self.emit('assert_counter', 'source', prop='loyalty', value=self.loyalty)

    def activate(self, mode, *, targets=(), resolution=(), decline_target=False, card_in_grave=None):
        assert self.active == self.owner and self.loyalty + self.costs[mode] >= 0
        self.emit('can_activate', 'source', value=1)
        self.emit('present_ai_dialogs', value=1)
        self.emit('choose_dialog', player=self.owner, value=mode)
        for ref in targets:
            self.emit('choose_target', ref, player=self.owner)
        if card_in_grave is not None:
            self.emit('choose_zone_card', player=self.owner, value=self.rows[card_in_grave]['csvid'])
        if decline_target:
            self.emit('choose_cancel_target', player=self.owner)
        self.emit('activate', 'source', value=1)
        self.choices()
        self.loyalty += self.costs[mode]
        if self.loyalty:
            self.observe_loyalty()
        self.emit('assert_stack', value=1)
        self.wallet(self.owner, ZERO)
        for op, ref, other, prop, player, value, zone, index, color in resolution:
            self.emit(op, ref, other, prop, player=player, value=value, zone=zone, index=index, color=color)
        self.resolve()
        self.emit('present_ai_dialogs')
        if self.loyalty:
            self.emit('can_activate', 'source', value=0)
        else:
            self.cards['source']['zone'] = 'graveyard'

    def next_turn(self):
        assert self.pools == [ZERO, ZERO]
        self.active = 1-self.active
        self.emit('advance_turn')
        self.emit('untap_step', player=self.active)
        self.emit('upkeep_step', player=self.active)
        self.emit('turn', player=self.active)
        for item in self.cards.values():
            if item['zone'] == 'battlefield' and item['player'] == self.active:
                item['tapped'] = False

    def next_owner(self):
        self.next_turn()
        self.next_turn()

    def mill(self, count):
        refs = self.libraries[self.owner][:count]
        del self.libraries[self.owner][:count]
        for ref in refs:
            self.cards[ref]['zone'] = 'graveyard'
        return refs

    def bind(self, ref):
        card = self.cards[ref]
        self.emit('bind_permanent', ref, player=card['player'], value=self.rows[card['name']]['csvid'], index=-1)

    def hope(self):
        if self.mode in ('weaken', 'no_target'):
            affect = self.mode == 'weaken'
            self.activate(0, targets=('host',) if affect else (), decline_target=not affect)
            if affect:
                self.cards['host']['adjustment'] = (-2, -1)
            self.receipts()
            self.next_turn()
            self.receipts()
            self.next_turn()
            self.cards['host']['adjustment'] = (0, 0)
        elif self.mode in ('mill_return', 'mill_decline'):
            choice = ('choose_zone_card', '', '', '', self.owner, self.rows[BEAR]['csvid'], 0, 0, 0)
            if self.mode == 'mill_decline':
                choice = ('choose_zone', '', '', '', self.owner, -1, 0, 0, 0)
            self.activate(1, resolution=(choice,))
            milled = self.mill(2)
            if self.mode == 'mill_return':
                self.cards[milled[0]]['zone'] = 'hand'
        else:
            while self.loyalty < 7:
                self.activate(0, targets=('host',))
                self.next_owner()
            self.activate(2)
            self.receipts()
            for _ in range(2):
                self.next_turn()
                self.zombies += 2 + (1 + self.zombies)
                self.receipts()
                self.next_turn()
                self.receipts()

    def majesty(self):
        def plus():
            self.activate(0)
            self.zombies += 1
            self.mill(2)

        if self.mode in ('token', 'short_mill'):
            plus()
        elif self.mode == 'reanimate':
            self.activate(1, card_in_grave=BEAR)
            self.cards['target']['zone'] = 'battlefield'
            self.bind('target')
            for prop, value in (('color', 10), ('subtype_Zombie', 1), ('subtype_Bear', 1)):
                self.emit('assert_matching_permanents', prop=prop, player=self.owner,
                          index=self.rows[BEAR]['csvid'], value=value)
        else:
            while self.loyalty < 7:
                plus()
                self.receipts()
                self.next_owner()
            self.activate(2)
            for ref in ('own_non_zombie', 'foe_non_zombie'):
                self.cards[ref]['zone'] = 'graveyard'

    def death(self):
        if self.mode in ('drain', 'miss'):
            self.activate(0)
            self.mill(3)
            if self.mode == 'drain':
                self.life[self.owner] += 2
                self.life[self.foe] -= 2
        elif self.mode == 'shrink':
            self.activate(1, targets=('host',))
            self.cards['host']['adjustment'] = (-2, -2)
            self.receipts()
            self.next_turn()
            self.cards['host']['adjustment'] = (0, 0)
        else:
            # The callback creates an unattached activated legacy. A real
            # activation through its dedicated menu is outside this archive.
            self.activate(2)
            self.receipts()
            self.next_owner()

    def estrid(self):
        if self.mode == 'untap':
            self.cast('own_aura', 'battlefield', target='host')
            self.cast('foe_aura', 'battlefield', target='foe_host')
            self.cards['host']['adjustment'] = self.cards['foe_host']['adjustment'] = (1, 2)
            self.receipts()
            self.activate(0)
            self.cards['host']['tapped'] = False
        elif self.mode == 'mask':
            choice = ('choose_target', 'host', '', '', self.owner, 0, 0, 0, 0)
            self.activate(1, resolution=(choice,))
            self.masks = 1
            self.receipts()
            self.cast('removal', 'graveyard', target='host')
            self.masks = 0
        else:
            while self.loyalty < 7:
                self.activate(0)
                self.next_owner()
            choice = ('choose_target', 'host', '', '', self.owner, 0, 0, 0, 0)
            self.activate(2, resolution=(choice,))
            self.mill(7)
            for ref, item in self.cards.items():
                if item['zone'] == 'graveyard' and item['player'] == self.owner and item['name'] in (ANTHEM, HOLY):
                    item['zone'] = 'battlefield'
                    self.bind(ref)
            self.cards['host']['adjustment'] = (2, 3)

    def build_activated(self):
        if self.id == HELM:
            if self.mode == 'creature_first':
                return (BEAR, 'Mountain', 'Swamp', GIANT)
            if self.mode == 'creature_second':
                return ('Forest', BEAR, 'Island', 'Mountain', GIANT)
            if self.mode == 'short_deck':
                return ('Forest',)
            return ('Forest', 'Mountain', BEAR, *DECK)
        if self.mode == 'creature':
            self.add('victim', AIR, self.foe)
            self.add('control', GIANT, self.foe)
        if self.mode == 'walker':
            self.add('victim', JACE, self.foe, 'hand')
        if self.mode == 'lost_target':
            self.add('victim', AIR, self.foe)
            self.add('bounce', UNSUMMON, self.foe, 'hand')
        return DECK

    def helm(self):
        if self.mode == 'zero':
            x, pool = 0, 3
        elif self.mode == 'creature_first':
            x, pool = 2, 3
        elif self.mode == 'limit':
            x, pool = 2, 3
        else:
            x, pool = 3, 4
        mana = [pool, 0, 0, 0, 0, 0]
        self.wallet(self.owner, mana, funding=True)
        self.emit('can_activate', 'source', value=1)
        self.emit('choose_x', player=self.owner, value=x)
        self.emit('activate', 'source', value=int(bool(x)))
        self.choices()
        self.emit('assert_stack', value=int(bool(x)))
        self.pools[self.owner][0] -= x
        self.wallet(self.owner, self.pools[self.owner])
        self.cards['source']['tapped'] = bool(x)
        self.observe('source', 'tapped', bool(x))
        if not x:
            return
        self.resolve()
        for ref in self.libraries[self.foe][:x]:
            self.libraries[self.foe].remove(ref)
            item = self.cards[ref]
            item['zone'] = 'graveyard'
            if 'Creature' in self.oracle[item['name']]['type_line']:
                item.update(player=self.owner, zone='battlefield')
                self.cards['source']['zone'] = 'graveyard'
                self.bind(ref)
                break
        if self.mode == 'limit':
            self.emit('can_activate', 'source', value=0)

    def heretic(self):
        if self.mode == 'walker':
            self.next_turn()
            self.cast('victim', 'battlefield')
            self.emit('assert_counter', 'victim', prop='loyalty', value=3)
        self.emit('can_activate', 'source', value=0)
        self.wallet(self.owner, [3, 0, 0, 0, 1, 0], funding=True)
        self.emit('can_activate', 'source', value=1)
        target = 'victim' if self.mode in ('creature', 'walker', 'lost_target') else None
        if target:
            self.emit('choose_target', target, player=self.owner)
        else:
            self.emit('choose_player', player=self.owner, value=self.foe)
        self.emit('activate', 'source', value=1)
        self.choices()
        self.emit('assert_stack', value=1)
        self.pools[self.owner] = ZERO.copy()
        self.wallet(self.owner, ZERO)
        self.receipts(stack=1)
        if self.mode == 'lost_target':
            self.cast('bounce', 'graveyard', target='victim', stack=2)
            self.cards['victim']['zone'] = 'hand'
            return
        self.resolve()
        milled = self.mill(3)
        amount = max((int(self.oracle[self.cards[ref]['name']]['cmc']) for ref in milled), default=0)
        if target:
            self.cards[target]['zone'] = 'graveyard'
        else:
            self.life[self.foe] -= amount

    def receipts(self, *, stack=0):
        self.emit('assert_stack', value=stack)
        if self.id in WALKERS and self.cards['source']['zone'] == 'battlefield':
            self.observe_loyalty()
        for player in (0, 1):
            self.emit('assert_life', player=player, value=self.life[player])
            self.wallet(player, self.pools[player])
            counts = defaultdict(Counter)
            for item in self.cards.values():
                if item['player'] == player:
                    counts[item['zone']][item['name']] += 1
            if player == self.owner:
                if self.zombies:
                    counts['battlefield'][ZOMBIE] += self.zombies
                if self.masks:
                    counts['battlefield'][MASK] += self.masks
            for zone, index in ZONES.items():
                self.emit('assert_zone', player=player, value=counts[zone].total(), zone=index)
                for name, count in sorted(counts[zone].items()):
                    self.emit('assert_zone_multiplicity', player=player, zone=index,
                              index=self.rows[name]['csvid'], value=count)
            for index, ref in enumerate(self.libraries[player]):
                self.emit('assert_zone_card', player=player, zone=ZONES['library'], index=index,
                          value=self.rows[self.cards[ref]['name']]['csvid'])
        for ref, item in self.cards.items():
            if item['zone'] != 'battlefield':
                continue
            self.observe(ref, 'in_play', 1)
            if 'Creature' in self.oracle[item['name']]['type_line']:
                for prop, adjustment in zip(('power', 'toughness'), item['adjustment'], strict=True):
                    self.observe(ref, prop, int(self.oracle[item['name']][prop]) + adjustment)
                self.observe(ref, 'tapped', item['tapped'])
        if self.zombies:
            for prop, value in (('token', 1), ('power', 2), ('toughness', 2), ('color', 2), ('subtype_Zombie', 1)):
                self.emit('assert_matching_permanents', prop=prop, player=self.owner,
                          index=self.rows[ZOMBIE]['csvid'], value=value)
        if self.masks:
            for prop, value in (('token', 1), ('color', 32)):
                self.emit('assert_matching_permanents', prop=prop, player=self.owner,
                          index=self.rows[MASK]['csvid'], value=value)

    def run(self):
        self.emit('turn', player=self.owner)
        for player in (0, 1):
            self.wallet(player, ZERO, funding=True)
        self.cast('source', 'battlefield')
        self.receipts()
        paths = {HOPE: self.hope, MAJESTY: self.majesty, DEATH: self.death,
                 ESTRID: self.estrid, HELM: self.helm, HERETIC: self.heretic}
        paths[self.id]()
        self.receipts()
