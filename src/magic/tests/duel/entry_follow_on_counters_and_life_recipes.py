"""Actual entry, counters, life, type gates and printed riders."""
from collections import Counter, defaultdict
from functools import lru_cache
import json
from pathlib import Path

from counter_condition_recurrence_recipes import exact
from recipes import card, cost_from_oracle, step

FAMILY = 'entry_follow_on_counters_and_life'
PROFILE = Path(__file__).with_name('data') / 'entry_follow_on_counters_and_life_wave8_profiles.json'
ABZAN, PITH, ONI, SANCTUARY, PATRON, SANDCRAFTER = 14205, 11659, 7722, 12463, 15193, 14999
RELIEF, LAGAC, STROMKIRK, THALIA, AMMIT, CROCODILE = 15667, 15672, 15978, 15983, 16843, 16876
SUPPORT = {RELIEF: 3, LAGAC: 2}
RAISE, SOLDIER, SPIRIT, DEMON = 'Raise the Alarm', 'Soldier', 'Spirit', 'Razorjaw Oni'
ANGEL, BEETLE, KOBOLD, KNIGHT = 'Serra Angel', 'Clockwork Beetle', 'Kobolds of Kher Keep', 'Black Knight'
VAMPIRE, HUMAN = 'Vampire Interloper', 'Elite Vanguard'
TOKEN_NAMES = {SOLDIER, SPIRIT}

DELUSIONS, GLIMMERPOST, HUNTER, WURM = 3984, 11264, 13537, 11015
MODES = {
    DELUSIONS: ('entry', 'destroy', 'exile', 'recast', 'countered', 'replay_departure'),
    GLIMMERPOST: ('solo', 'friendly', 'opposing', 'mixed', 'mana', 'second_turn'),
    HUNTER: ('alone', 'owner', 'foe', 'mixed', 'recast', 'countered'),
    WURM: ('entry', 'countered', 'die_draw', 'bounce', 'trample', 'prevented', 'combat_death'),
}
ZONES = {'hand': 0, 'battlefield': 1, 'library': 2, 'graveyard': 3, 'exile': 4}
ZERO = [0] * 6
CLOUDPOST, MOUNTAIN, FOREST = 'Cloudpost', 'Mountain', 'Forest'
ISLAND, PLAINS, CONSTRUCT = 'Island', 'Plains', 'Sparring Construct'
BEAR, BALOTH, HEAVY, MATCHED = 'Grizzly Bears', 'Rumbling Baloth', 'Leatherback Baloth', 'Enormous Baloth'
COUNTER, BOUNCE, UNSUMMON = 'Counterspell', 'Boomerang', 'Unsummon'
DISENCHANT, REVOKE, MURDER, FOG = 'Disenchant', 'Revoke Existence', 'Murder', 'Fog'
ZONE_NAMES = {code: name for name, code in ZONES.items()}
WITNESSES = frozenset(('Black Knight', 'Boomerang', 'Clockwork Beetle', 'Cloudpost', 'Counterspell', 'Disenchant', 'Elite Vanguard', 'Enormous Baloth', 'Fog', 'Forest', 'Glimmerpost', 'Grizzly Bears', 'Island', 'Kobolds of Kher Keep', 'Leatherback Baloth', 'Mountain', 'Murder', 'Plains', 'Raise the Alarm', 'Razorjaw Oni', 'Revoke Existence', 'Rumbling Baloth', 'Serra Angel', 'Soldier', 'Sparring Construct', 'Spirit', 'Unsummon', 'Vampire Interloper'))


MODES.update({
    ABZAN: ('entry', 'own_death', 'enemy_death', 'token_death', 'countered'),
    PITH: ('black', 'life', 'own', 'self', 'countered'),
    ONI: ('no_demon', 'own_demon', 'foe_demon', 'exorcised', 'countered'),
    SANCTUARY: ('entry', 'mana', 'own_angel', 'foe_angel', 'nonangel'),
    PATRON: ('alone', 'boost', 'flying', 'countered'),
    SANDCRAFTER: ('alone', 'weak', 'tie', 'countered'),
    RELIEF: ('cap', 'partial', 'decline', 'empty', 'countered'),
    LAGAC: ('cap', 'partial', 'decline', 'empty', 'countered'),
    STROMKIRK: ('scope', 'unmatched', 'countered'),
    THALIA: ('entry', 'own_entry', 'foe_entry', 'nonhuman', 'countered'),
    AMMIT: ('own', 'self', 'combat', 'countered'),
    CROCODILE: ('own', 'self', 'combat', 'countered'),
})
GAPS = {
    DELUSIONS: ('Departure uses the same controller that gained life; control transfers are outside this pilot.',),
}

@lru_cache(maxsize=1)
def profiles():
    return json.loads(PROFILE.read_bytes())


def classify(record, csvid):
    if (csvid not in MODES or record.get('layout') != 'normal' or record.get('card_faces')
            or record.get('selected_face') or 'enters' not in record['oracle_text'].lower()):
        raise ValueError('normal whole entry-trigger primary required')
    return dict(csvid=csvid, printed_cost=record['mana_cost'])


def variants(shape):
    csvid = shape['csvid']
    return [f'{mode}_{owner}' for owner in (0, 1) for mode in MODES[csvid]
            ]


def validate_variants(shape, chosen):
    if (type(chosen) is not list or not chosen or any(type(item) is not str for item in chosen)
            or len(set(chosen)) != len(chosen) or set(chosen) - set(variants(shape))):
        raise ValueError('distinct supported entry-trigger variants required')


def spec(declaration, record):
    pin = profiles()['sources'].get(record.get('name'))
    if not pin or not exact(pin['oracle'], record):
        raise ValueError('exact pinned entry-trigger primary required')
    shape = classify(record, pin['registry']['csvid'])
    if (declaration.get('family') != FAMILY or type(declaration.get('version')) is not int
            or declaration['version'] != 2 or declaration.get('oracle_text') != record['oracle_text']
            or not exact(declaration.get('params'), shape)):
        raise ValueError('exact typed entry-trigger declaration required')
    validate_variants(shape, declaration.get('variants'))
    return dict(shape, family=FAMILY)


def validate_witnesses(oracle, rows):
    pins = profiles()
    if (set(pins) != {'sources', 'witnesses'} or set(pins['witnesses']) != WITNESSES
            or {pin['registry']['csvid'] for pin in pins['sources'].values()} != MODES.keys()):
        raise ValueError('exact entry-trigger sources and witnesses required')
    for group, entries in pins.items():
        for name, pin in entries.items():
            row = rows.get(name, {})
            if (not exact(oracle.get(name), pin['oracle']) or not exact(row, pin['registry'])
                    or row.get('kind') != 'card' or row.get('handler') in (None, '0', 'NULL')
                    or oracle[name].get('layout') != ('token' if name in TOKEN_NAMES else 'normal')):
                raise ValueError('entry-trigger Oracle or registry changed: ' + name)
            if group == 'sources':
                if row['handler'] == 'card_dummy':
                    raise ValueError('implemented entry-trigger primary required')
                classify(oracle[name], row['csvid'])


def expand(name, csvid, declaration, shape, chosen, rows, oracle):
    validate_variants(shape, chosen)
    validate_witnesses(oracle, rows)
    if shape['csvid'] != csvid or profiles()['sources'][name]['registry']['csvid'] != csvid:
        raise ValueError('exact entry-trigger primary CSVID required')
    cases = []
    for variant in chosen:
        mode, seat = variant.rsplit('_', 1)
        board = Board(rows[name], int(seat), mode, rows, oracle)
        board.run()
        cases.append(dict(name=f'EntryFollowOnCountersAndLifeRecipe_{csvid}_{variant}', card=name,
                          fixture='empty_main', seed=3212, setup=board.setup, steps=board.steps,
                          _recipe=dict(family=FAMILY, version=2, variant=variant, params=declaration['params'],
                                       oracle_text=declaration['oracle_text'], untested_components=list(GAPS.get(csvid, ())))))
    return cases

class Board:
    def __init__(self, record, owner, mode, rows, oracle):
        self.id, self.name = record['csvid'], record['name']
        self.owner, self.foe, self.mode = owner, 1 - owner, mode
        self.rows, self.oracle = rows, oracle
        self.setup, self.steps, self.cards = [], [], {}
        self.life = [20, 20]
        self.wallet = [ZERO.copy(), ZERO.copy()]
        self.add('source', self.name, owner, 'hand')
        if mode == 'countered':
            self.add('counter', COUNTER, self.foe, 'hand')
        if self.id == DELUSIONS:
            if mode in ('destroy', 'replay_departure'):
                self.add('removal', DISENCHANT, self.foe, 'hand')
            if mode == 'exile':
                self.add('removal', REVOKE, owner, 'hand')
            if mode == 'recast':
                self.add('removal', BOUNCE, owner, 'hand')
        if self.id == GLIMMERPOST:
            self.add('not_locus_own', MOUNTAIN, owner)
            self.add('not_locus_foe', ISLAND, self.foe)
            if mode in ('friendly', 'mixed'):
                self.add('own_locus', CLOUDPOST, owner)
            if mode in ('opposing', 'mixed'):
                self.add('foe_locus', CLOUDPOST, self.foe)
            if mode == 'mana':
                self.add('colorless_purchase', CONSTRUCT, owner, 'hand')
            if mode == 'second_turn':
                self.add('second', self.name, owner, 'hand')
        if self.id == HUNTER:
            self.add('own_land', FOREST, owner)
            self.add('another_color', CONSTRUCT, owner)
            if mode in ('owner', 'mixed', 'recast'):
                self.add('own_green', HEAVY, owner)
            if mode in ('foe', 'mixed'):
                self.add('foe_green', HEAVY, self.foe)
            if mode == 'mixed':
                self.add('extra_green', BALOTH, owner)
            if mode == 'recast':
                self.add('murder', MURDER, self.foe, 'hand')
                self.add('bounce', UNSUMMON, owner, 'hand')
        if self.id == WURM:
            for player in (0, 1):
                self.add(f'first_{player}', ISLAND, player, 'library')
                self.add(f'next_{player}', PLAINS, player, 'library')
            if mode == 'die_draw':
                self.add('removal', MURDER, self.foe, 'hand')
            if mode == 'bounce':
                self.add('removal', UNSUMMON, owner, 'hand')
            if mode in ('trample', 'prevented', 'combat_death'):
                self.add('blocker', MATCHED if mode == 'combat_death' else BEAR, self.foe)
            if mode == 'prevented':
                self.add('fog', FOG, self.foe, 'hand')

        self.setup_follower()

    def add(self, ref, name, player, zone='battlefield'):
        self.setup.append(card(name, ref, player, zone))
        self.cards[ref] = dict(name=name, player=player, zone=zone)

    def emit(self, op, ref='', other='', prop='', *, player=0, value=0, zone=0, index=0, color=0):
        if op == 'untap_step':
            self.steps.append(step(op, player=player))
            return
        if op == 'end_combat_step':
            self.steps.append(step(op, value=value))
            return
        if op == 'play_land':
            self.steps.append(step(op, ref=ref, value=value))
            return
        if op == 'choose_optional_target':
            self.steps.append(step(op, ref=ref, player=player))
            return
        if op == 'assert_forced_target_choices':
            self.steps.append(step(op, value=value))
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
        self.emit('assert_card', ref, prop=prop, value=value)

    def stats(self, ref, addition=0):
        for prop in ('power', 'toughness'):
            value = int(self.oracle[self.cards[ref]['name']][prop])
            self.observe(ref, prop, value + addition)

    def count(self, ref, value, prop='plus1'):
        self.emit('assert_counter', ref, prop=prop, value=value)

    def move(self, ref, zone):
        self.cards[ref]['zone'] = zone

    def choices(self):
        self.emit('assert_unused_choices')
        self.emit('assert_choices_consumed')

    def pool(self, player, amount, *, fund=False):
        for color, value in enumerate(amount):
            self.emit('set_mana' if fund else 'assert_mana', player=player, value=value, color=color)
        if fund:
            self.wallet[player] = amount.copy()
            self.emit('refresh')

    def announce(self, ref, *, target=None, response=False, prefunded=False):
        player = self.cards[ref]['player']
        pith_payment = self.id == PITH and ref == 'source'
        life_payment = pith_payment and self.mode == 'life'
        if pith_payment:
            if self.oracle[self.name]['mana_cost'] != '{4}{B/P}':
                raise ValueError('exact black Phyrexian fee required')
            cost = [4, int(not life_payment), 0, 0, 0, 0]
        else:
            cost = cost_from_oracle(self.oracle[self.cards[ref]['name']])
        if prefunded:
            assert self.wallet[player] == cost
        else:
            self.pool(player, cost, fund=True)
        if target is not None:
            self.emit('choose_target', target, player=player)
        if pith_payment:
            self.emit('choose_number', player=player, value=int(life_payment))
        self.emit('cast_response' if response else 'cast', ref, value=1)
        if life_payment:
            self.life[player] -= 2
        self.choices()
        self.emit('assert_stack', value=2 if response else 1)
        self.wallet[player] = ZERO.copy()
        self.pool(player, ZERO)

    def resolve(self, *, response=False):
        self.emit('resolve_response' if response else 'resolve')
        self.emit('refresh')
        self.choices()
        self.emit('assert_stack')

    def spell(self, ref, *, target=None, destination='graveyard', prefunded=False):
        self.announce(ref, target=target, prefunded=prefunded)
        self.resolve()
        self.move(ref, destination)

    def receipt(self):
        self.emit('assert_stack')
        for player in (0, 1):
            self.emit('assert_life', player=player, value=self.life[player])
            self.pool(player, self.wallet[player])
            for zone, code in ZONES.items():
                counts = Counter(item['name'] for item in self.cards.values()
                                 if item['player'] == player and item['zone'] == zone)
                self.emit('assert_zone', player=player, zone=code, value=counts.total())
                for name, count in sorted(counts.items()):
                    self.emit('assert_zone_multiplicity', player=player, zone=code,
                              index=self.rows[name]['csvid'], value=count)
        for ref, item in self.cards.items():
            if item['zone'] == 'battlefield':
                self.observe(ref, 'in_play', 1)

    def visit(self, player):
        assert self.wallet == [ZERO, ZERO]
        self.emit('advance_turn')
        self.emit('untap_step', player=player)
        self.emit('turn', player=player)

    def bounced(self):
        self.move('source', 'hand')
        self.emit('bind_hand_copy', 'source', player=self.owner, value=self.id)

    def countered(self):
        self.announce('source')
        self.emit('announce_spell', 'source')
        self.announce('counter', response=True)
        self.resolve(response=True)
        self.move('source', 'graveyard')
        self.move('counter', 'graveyard')
        self.receipt()

    def delusions(self):
        self.spell('source', destination='battlefield')
        self.life[self.owner] += 10
        self.receipt()
        mode = self.mode
        if mode == 'entry':
            return

        def departure():
            self.spell('removal', target='source')
            zone = {'exile': 'exile', 'recast': 'hand'}.get(mode, 'graveyard')
            self.move('source', zone)
            self.life[self.owner] -= 10
            self.receipt()

        if mode == 'replay_departure':
            self.emit('capture')
            state = self.life.copy(), {ref: item.copy() for ref, item in self.cards.items()}
            departure()
            self.emit('restore')
            self.life, self.cards = state
            self.receipt()
        departure()
        if mode == 'recast':
            self.bounced()
            self.spell('source', destination='battlefield')
            self.life[self.owner] += 10
            self.receipt()

    def land_entry(self, ref):
        self.emit('play_land', ref, value=1)
        self.resolve()
        self.move(ref, 'battlefield')
        loci = sum(item['zone'] == 'battlefield' and 'Locus' in self.oracle[item['name']]['type_line']
                   for item in self.cards.values())
        self.life[self.owner] += loci
        self.observe(ref, 'tapped', 0)
        self.receipt()

    def mana_purchase(self):
        self.emit('can_activate', 'source', value=1)
        self.emit('activate', 'source', value=1)
        self.choices()
        self.emit('assert_stack')
        self.wallet[self.owner][0] = 1
        self.pool(self.owner, self.wallet[self.owner])
        self.observe('source', 'tapped', 1)
        self.emit('can_activate', 'source', value=0)
        self.spell('colorless_purchase', destination='battlefield', prefunded=True)
        self.receipt()

    def glimmerpost(self):
        self.land_entry('source')
        if self.mode == 'mana':
            self.mana_purchase()
        if self.mode == 'second_turn':
            self.emit('play_land', 'second')
            self.receipt()
            self.visit(self.foe)
            self.visit(self.owner)
            self.land_entry('second')

    def hunter_entry(self):
        self.spell('source', destination='battlefield')
        counters = sum(self.oracle[item['name']]['mana_cost'].count('{G}') for item in self.cards.values()
                       if item['player'] == self.owner and item['zone'] == 'battlefield')
        self.count('source', counters)
        self.stats('source', counters)
        for ref in ('own_green', 'foe_green', 'extra_green', 'another_color'):
            if ref in self.cards and self.cards[ref]['zone'] == 'battlefield':
                self.count(ref, 0)
        self.receipt()
        return counters

    def hunter(self):
        count = self.hunter_entry()
        if self.mode == 'recast':
            self.spell('murder', target='own_green')
            self.move('own_green', 'graveyard')
            self.count('source', count)
            self.stats('source', count)
            self.spell('bounce', target='source')
            self.bounced()
            self.receipt()
            self.hunter_entry()

    def observe_draw(self, drawn):
        name = PLAINS if drawn else ISLAND
        if drawn:
            self.move(f'first_{self.owner}', 'hand')
        self.emit('assert_zone_card', player=self.owner, zone=ZONES['library'], value=self.rows[name]['csvid'])
        self.emit('assert_zone_card', player=self.foe, zone=ZONES['library'], value=self.rows[ISLAND]['csvid'])

    def wurm(self):
        self.spell('source', destination='battlefield')
        self.life[self.owner] += 7
        self.stats('source')
        self.receipt()
        mode = self.mode
        if mode in ('die_draw', 'bounce'):
            self.spell('removal', target='source')
            self.move('source', 'graveyard' if mode == 'die_draw' else 'hand')
            self.observe_draw(mode == 'die_draw')
            self.receipt()
        if mode not in ('trample', 'prevented', 'combat_death'):
            return
        self.visit(self.foe)
        self.visit(self.owner)
        self.emit('begin_attackers')
        self.emit('attack', 'source', value=1)
        self.emit('finish_attackers')
        if mode == 'prevented':
            self.spell('fog')
        self.emit('begin_blockers')
        self.emit('block', 'blocker', 'source', value=1)
        self.emit('advance_combat')
        self.observe('source', 'blocked', 1)
        self.emit('combat_damage')
        self.resolve()
        if mode != 'prevented':
            self.move('blocker', 'graveyard')
        if mode == 'combat_death':
            self.move('source', 'graveyard')
        else:
            self.observe('source', 'damage', 0 if mode == 'prevented' else 2)
        if mode == 'trample':
            self.life[self.foe] -= 5
        self.observe_draw(mode == 'combat_death')
        self.emit('end_combat_step')
        self.resolve()
        self.receipt()

    def setup_follower(self):
        mode = self.mode
        if self.id == ABZAN:
            self.add('ally', BEAR, self.owner)
            self.add('teammate', BALOTH, self.owner)
            self.add('opposing', BEAR, self.foe)
            if mode == 'token_death':
                self.add('token_spell', RAISE, self.owner, 'hand')
            if mode in ('own_death', 'enemy_death', 'token_death'):
                self.add('kill_spell', MURDER, self.owner if mode == 'enemy_death' else self.foe, 'hand')
        if self.id == PITH and mode != 'self':
            self.add('own', BALOTH, self.owner)
            self.add('other', BALOTH, self.foe)
        if self.id == ONI:
            if mode in ('own_demon', 'exorcised'):
                self.add('demon', DEMON, self.owner)
            if mode == 'foe_demon':
                self.add('demon', DEMON, self.foe)
            if mode == 'exorcised':
                self.add('exorcism', MURDER, self.foe, 'hand')
            if mode != 'countered':
                self.add('kill_spell', MURDER, self.foe, 'hand')
        if self.id == SANCTUARY:
            if mode == 'mana':
                self.add('colorless_purchase', CONSTRUCT, self.owner, 'hand')
            if mode in ('own_angel', 'foe_angel', 'nonangel'):
                player = self.foe if mode == 'foe_angel' else self.owner
                self.add('entrant', BEAR if mode == 'nonangel' else ANGEL, player, 'hand')
        if self.id == PATRON:
            if mode == 'boost':
                self.add('with_counter', BEETLE, self.owner, 'hand')
                self.add('enemy_counter', BEETLE, self.foe, 'hand')
                self.add('without_counter', BEAR, self.owner)
            if mode == 'flying':
                self.add('blocker', BEAR, self.foe)
        if self.id == SANDCRAFTER:
            self.add('foe_weak', KOBOLD, self.foe)
            if mode == 'weak':
                self.add('weak', KOBOLD, self.owner)
            if mode == 'tie':
                self.add('chosen', KNIGHT, self.owner)
                self.add('other_tie', BEAR, self.owner)
        if self.id in SUPPORT:
            if mode == 'cap':
                for index in range(SUPPORT[self.id]):
                    self.add(f'pick_{index}', BEAR, self.foe if index == 1 else self.owner)
                self.add('unselected', BALOTH, self.foe)
            if mode in ('partial', 'decline'):
                self.add('pick_0', BEAR, self.owner)
                self.add('unselected', BALOTH, self.foe)
        if self.id == STROMKIRK:
            if mode == 'scope':
                self.add('own_vampire', VAMPIRE, self.owner)
            self.add('foe_vampire', VAMPIRE, self.foe)
            self.add('nonvampire', BEAR, self.owner)
        if self.id == THALIA:
            self.add('own_human', HUMAN, self.owner)
            self.add('foe_human', HUMAN, self.foe)
            self.add('nonhuman', BEAR, self.owner)
            if mode in ('own_entry', 'foe_entry', 'nonhuman'):
                player = self.foe if mode == 'foe_entry' else self.owner
                self.add('entrant', BEAR if mode == 'nonhuman' else HUMAN, player, 'hand')
        if self.id in (AMMIT, CROCODILE):
            self.add('own', BALOTH, self.owner)
            self.add('other', BEAR, self.foe)

    def choose_entry(self, targets=(), *, optional=()):
        self.announce('source')
        for ref in targets:
            self.emit('choose_target', ref, player=self.owner)
        for fallback in optional:
            self.emit('choose_optional_target', fallback, player=self.owner)
        self.resolve()
        self.move('source', 'battlefield')

    def typed_token(self, ref, name, *, ordinal=0):
        self.cards[ref] = dict(name=name, player=self.owner, zone='battlefield')
        self.emit('bind_permanent_copy', ref, player=self.owner, index=ordinal, value=self.rows[name]['csvid'])

    def elder_abzan(self):
        if self.mode == 'token_death':
            self.spell('token_spell')
            for index in range(2):
                self.typed_token(f'soldier_{index}', SOLDIER, ordinal=index)
        self.choose_entry()
        for ref in ('ally', 'teammate', 'opposing', 'soldier_0', 'soldier_1'):
            if ref in self.cards:
                count = int(ref != 'opposing')
                self.count(ref, count)
                self.stats(ref, count)
        self.receipt()
        targets = {'own_death': 'ally', 'enemy_death': 'opposing', 'token_death': 'soldier_0'}
        if self.mode not in targets:
            return
        victim = targets[self.mode]
        self.spell('kill_spell', target=victim)
        if self.mode == 'token_death':
            self.cards.pop(victim)
        else:
            self.move(victim, 'graveyard')
        if self.mode != 'own_death':
            self.receipt()
            return
        self.typed_token('spirit', SPIRIT)
        for attr, value in (('token', 1), ('color', 32), ('subtype_Spirit', 1)):
            self.emit('assert_matching_permanents', prop=attr, player=self.owner, index=self.rows[SPIRIT]['csvid'], value=value)
        self.stats('spirit')
        self.receipt()
        self.visit(self.foe)
        self.visit(self.owner)
        self.single_combat('spirit', 'opposing', 1, block=False)

    def pith(self):
        target = 'source' if self.mode == 'self' else 'own' if self.mode == 'own' else 'other'
        self.choose_entry((target,))
        self.emit('assert_matching_permanents', prop='artifact', player=self.owner, index=self.id, value=1)
        for ref in ('source', 'own', 'other'):
            if ref in self.cards:
                count = int(ref == target)
                self.count(ref, count, 'minus1')
                self.stats(ref, -count)
        self.receipt()

    def raving_oni(self):
        has_demon = self.mode in ('own_demon', 'exorcised')
        self.choose_entry()
        self.life[self.owner] -= 0 if has_demon else 3
        self.stats('source')
        self.receipt()
        if self.mode == 'exorcised':
            self.spell('exorcism', target='demon')
            self.move('demon', 'graveyard')
            has_demon = False
            self.receipt()
        self.spell('kill_spell', target='source')
        self.move('source', 'graveyard')
        self.life[self.owner] -= 0 if has_demon else 3
        self.receipt()

    def sanctuary(self):
        self.emit('play_land', 'source', value=1)
        self.resolve()
        self.move('source', 'battlefield')
        self.life[self.owner] += 1
        self.observe('source', 'tapped', 0)
        self.receipt()
        if self.mode == 'mana':
            self.mana_purchase()
        if 'entrant' in self.cards:
            if self.mode == 'foe_angel':
                self.visit(self.foe)
            self.spell('entrant', destination='battlefield')
            self.life[self.owner] += int(self.mode == 'own_angel')
            self.receipt()

    def patron(self):
        if self.mode == 'boost':
            self.spell('with_counter', destination='battlefield')
            self.count('with_counter', 2)
            self.visit(self.foe)
            self.spell('enemy_counter', destination='battlefield')
            self.count('enemy_counter', 2)
            self.visit(self.owner)
        self.choose_entry()
        self.count('source', 0)
        if self.mode == 'boost':
            for ref, count in (('with_counter', 3), ('enemy_counter', 2), ('without_counter', 0)):
                self.count(ref, count)
                self.stats(ref, count)
        self.receipt()
        if self.mode == 'flying':
            self.visit(self.foe)
            self.visit(self.owner)
            self.single_combat('source', 'blocker', self.size_power(), block=False)

    def bolster(self):
        target = {'weak': 'weak', 'tie': 'chosen'}.get(self.mode, 'source')
        self.choose_entry((target,) if self.mode == 'tie' else ())
        for ref in ('source', 'weak', 'foe_weak', 'chosen', 'other_tie'):
            if ref in self.cards:
                count = int(ref == target)
                self.count(ref, count)
                self.stats(ref, count)
        self.receipt()

    def support(self):
        cap = SUPPORT[self.id]
        if self.mode == 'cap':
            targets = tuple(f'pick_{i}' for i in range(cap))
            self.choose_entry(targets)
        elif self.mode in ('partial', 'decline'):
            targets = ('pick_0',) if self.mode == 'partial' else ()
            self.choose_entry(targets, optional=('unselected',) * (cap - len(targets)))
            self.emit('assert_forced_target_choices')
        else:
            targets = ()
            self.choose_entry()
        for ref in self.cards:
            if self.cards[ref]['zone'] == 'battlefield':
                count = int(ref in targets)
                self.count(ref, count)
                self.stats(ref, count)
        self.receipt()

    def stromkirk(self):
        target = ('own_vampire',) if self.mode == 'scope' else ()
        self.choose_entry(target)
        for ref in ('source', 'own_vampire', 'foe_vampire', 'nonvampire'):
            if ref in self.cards:
                count = int(ref in target)
                self.count(ref, count)
                self.stats(ref, count)
        self.receipt()

    def lieutenant(self):
        self.choose_entry()
        for ref, count in (('source', 0), ('own_human', 1), ('foe_human', 0), ('nonhuman', 0)):
            self.count(ref, count)
            self.stats(ref, count)
        self.receipt()
        if 'entrant' in self.cards:
            if self.mode == 'foe_entry':
                self.visit(self.foe)
            self.spell('entrant', destination='battlefield')
            self.count('source', int(self.mode == 'own_entry'))
            self.count('entrant', 0)
            self.count('own_human', 1)
            self.count('foe_human', 0)
            self.receipt()

    def size_power(self):
        return int(self.oracle[self.name]['power'])

    def single_combat(self, attacker, blocker, power, *, block, lifelink=False):
        self.emit('begin_attackers')
        self.emit('attack', attacker, value=1)
        self.emit('finish_attackers')
        self.emit('begin_blockers')
        if blocker:
            self.emit('block', blocker, attacker, value=int(block))
        self.emit('advance_combat')
        self.observe(attacker, 'blocked', int(block))
        self.emit('combat_damage')
        self.resolve()
        if block:
            self.move(blocker, 'graveyard')
            self.observe(attacker, 'damage', int(self.oracle[self.cards[blocker]['name']]['power']))
        else:
            self.life[self.foe] -= power
        if lifelink:
            self.life[self.owner] += power
        self.emit('end_combat_step')
        self.resolve()
        self.receipt()

    def own_minus_counter(self):
        target = 'source' if self.mode == 'self' else 'own'
        self.choose_entry((target,))
        for ref in ('source', 'own', 'other'):
            count = int(ref == target)
            self.count(ref, count, 'minus1')
            self.stats(ref, -count)
        self.receipt()
        if self.mode == 'combat':
            lifelink = self.id == AMMIT
            if lifelink:
                self.visit(self.foe)
                self.visit(self.owner)
            self.single_combat('source', 'other' if lifelink else None, self.size_power(),
                               block=lifelink, lifelink=lifelink)

    def run(self):
        self.emit('turn', player=self.owner)
        for player in (0, 1):
            self.pool(player, ZERO, fund=True)
        self.receipt()
        if self.mode == 'countered':
            self.countered()
            return
        methods = {DELUSIONS: self.delusions, GLIMMERPOST: self.glimmerpost,
                   HUNTER: self.hunter, WURM: self.wurm}
        methods.update({ABZAN: self.elder_abzan, PITH: self.pith, ONI: self.raving_oni,
                        SANCTUARY: self.sanctuary, PATRON: self.patron, SANDCRAFTER: self.bolster,
                        RELIEF: self.support, LAGAC: self.support, STROMKIRK: self.stromkirk,
                        THALIA: self.lieutenant, AMMIT: self.own_minus_counter, CROCODILE: self.own_minus_counter})
        methods[self.id]()
