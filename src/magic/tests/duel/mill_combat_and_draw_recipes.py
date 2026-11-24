"""Actual damage, life loss, draws, combat and linked zone effects drive these mill cases."""
from collections import Counter, defaultdict
from functools import lru_cache
import json
from pathlib import Path

from counter_condition_recurrence_recipes import exact
from recipes import card, cost_from_oracle, step

FAMILY = 'mill_combat_and_draw'
PROFILE = Path(__file__).with_name('data') / 'mill_combat_and_draw_wave8_profiles.json'
SIDISI, SZADEK, ALCHEMIST, WORLD = 14378, 7980, 12111, 18333
SCOUR, STUDY, SCATHE = 'Tome Scour', 'Hermetic Study', 'Scathe Zombies'
SWORDS, MURDER = 'Swords to Plowshares', 'Murder'

BELL, MIND, TUTEL, DRAGON = 7770, 11637, 15223, 14967
MODES = {
    BELL: ('opponent_spell', 'own_spell', 'combat', 'lethal', 'flight', 'prevented'),
    MIND: ('combat', 'loss', 'own_loss', 'prevented', 'short_library'),
    TUTEL: ('turn_chain', 'foe_draw', 'paid_loot', 'two_draws', 'short_library'),
    DRAGON: ('paid', 'unfunded', 'blocked', 'prevented'),
    SIDISI: ('entry_mixed', 'entry_lands', 'attack_trigger', 'external_mill', 'foe_mill'),
    SZADEK: ('first', 'repeat', 'blocked', 'noncombat'),
    ALCHEMIST: ('own_zombie', 'nonzombie', 'enemy_zombie', 'external_mill', 'own_library'),
    WORLD: ('after_attack', 'death_only', 'no_lands', 'short_library', 'exiled'),
}
ZONES = {'hand': 0, 'battlefield': 1, 'library': 2, 'graveyard': 3, 'exile': 4}
ZERO = [0] * 6
BEAR, GIANT, AIR = 'Grizzly Bears', 'Hill Giant', 'Air Elemental'
SHOCK, BOLT, FOG, PASSAGE = 'Shock', 'Lightning Bolt', 'Fog', 'Safe Passage'
BUMP, NIGHT, DIVINATION = 'Bump in the Night', "Night's Whisper", 'Divination'
GROWTH, ZOMBIE = 'Giant Growth', 'Zombie'
DECK = ('Forest', 'Plains', 'Island', 'Mountain', 'Swamp', 'Eager Cadet', BEAR, GIANT)
CHAIN = (BEAR, GROWTH, SHOCK, GIANT, 'Forest', 'Island', 'Plains', 'Mountain')
TWO_DRAWS = (BEAR, GROWTH, 'Forest', 'Island', SHOCK, AIR, 'Plains', 'Mountain')
WITNESSES = frozenset(('Air Elemental', 'Bump in the Night', 'Divination', 'Eager Cadet', 'Fog', 'Forest', 'Giant Growth', 'Grizzly Bears', 'Hermetic Study', 'Hill Giant', 'Island', 'Lightning Bolt', 'Mountain', 'Murder', "Night's Whisper", 'Plains', 'Safe Passage', 'Scathe Zombies', 'Shock', 'Swamp', 'Swords to Plowshares', 'Tome Scour', 'Zombie'))

ZONE_NAMES = {code: name for name, code in ZONES.items()}
COSTS = {BELL: '{4}{U}', MIND: '{2}', TUTEL: '{2}{U}', DRAGON: '{3}{U}{B}',
         SIDISI: '{1}{B}{G}{U}', SZADEK: '{3}{U}{U}{B}{B}',
         ALCHEMIST: '{3}{U}', WORLD: '{3}{G}'}
KEYWORDS = {csvid: {'Mill'} for csvid in COSTS}
for csvid in (BELL, DRAGON, SZADEK):
    KEYWORDS[csvid].add('Flying')
ZOMBIE_SOURCES = {DRAGON, SIDISI, ALCHEMIST}


@lru_cache(maxsize=1)
def profiles():
    return json.loads(PROFILE.read_bytes())


def classify(record, csvid):
    if (csvid not in COSTS or record.get('layout') != 'normal' or record.get('card_faces')
            or record.get('selected_face') or record.get('mana_cost') != COSTS[csvid]
            or set(record.get('keywords', ())) != KEYWORDS[csvid]):
        raise ValueError('normal whole Mill combat or draw record required')
    return dict(csvid=csvid, printed_cost=record['mana_cost'])


def variants(shape):
    csvid = shape['csvid']
    return [f'{mode}_{seat}' for seat in (0, 1) for mode in MODES[csvid]
            + (('decline',) if seat == 0 and csvid in (DRAGON, WORLD) else ())]


def validate_variants(shape, selected):
    if (type(selected) is not list or not selected or any(type(item) is not str for item in selected)
            or len(set(selected)) != len(selected) or set(selected) - set(variants(shape))):
        raise ValueError('distinct supported Mill combat and draw variants required')


def spec(declaration, record):
    pin = profiles()['sources'].get(record.get('name'))
    if not pin or not exact(pin['oracle'], record):
        raise ValueError('exact pinned Mill combat or draw primary required')
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
                    or oracle[name].get('layout') != ('token' if name == ZOMBIE else 'normal')):
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
        cases.append(dict(name=f'MillCombatDrawRecipe_{csvid}_{variant}', card=name,
                          fixture='empty_main', seed=2771, setup=board.setup, steps=board.steps,
                          _recipe=dict(family=FAMILY, version=2, variant=variant, params=declaration['params'],
                                       oracle_text=declaration['oracle_text'], untested_components=[])))
    return cases


class Board:
    def __init__(self, primary, owner, mode, rows, oracle):
        self.id, self.name = primary['csvid'], primary['name']
        self.owner, self.foe, self.mode = owner, 1-owner, mode
        self.rows, self.oracle = rows, oracle
        self.setup, self.steps, self.cards, self.libraries = [], [], {}, [[], []]
        self.life, self.pools, self.zombies = [20, 20], [ZERO.copy(), ZERO.copy()], 0
        self.add('source', self.name, owner, 'hand')
        grave_witness = 'Eager Cadet' if self.id == WORLD and mode == 'no_lands' else 'Plains'
        self.add('grave_control', grave_witness, owner, 'graveyard')
        self.add('foe_grave_control', 'Mountain', self.foe, 'graveyard')
        if self.id == BELL:
            if mode in ('opponent_spell', 'lethal'):
                self.add('bolt', BOLT, self.foe, 'hand')
            if mode == 'lethal':
                self.add('second_bolt', BOLT, self.foe, 'hand')
            if mode in ('own_spell', 'prevented'):
                self.add('shock', SHOCK, owner if mode == 'own_spell' else self.foe, 'hand')
            if mode == 'prevented':
                self.add('passage', PASSAGE, owner, 'hand')
            if mode == 'combat':
                self.add('attacker', AIR, self.foe)
            if mode == 'flight':
                self.add('blocker', GIANT, self.foe)
        elif self.id == MIND:
            if mode in ('combat', 'prevented'):
                self.add('attacker', GIANT, owner)
            if mode in ('loss', 'short_library'):
                self.add('bump', BUMP, owner, 'hand')
            if mode == 'own_loss':
                self.add('costly_draw', NIGHT, owner, 'hand')
            if mode == 'prevented':
                self.add('fog', FOG, self.foe, 'hand')
        elif self.id == TUTEL:
            if mode == 'two_draws':
                self.add('divination', DIVINATION, owner, 'hand')
        elif self.id == DRAGON:
            if mode in ('paid', 'blocked'):
                self.add('blocker', AIR if mode == 'blocked' else BEAR, self.foe)
            if mode == 'prevented':
                self.add('fog', FOG, self.foe, 'hand')
        else:
            self.add_followers()
        own_deck, foe_deck = self.follower_libraries(DECK, DECK)
        if self.id == TUTEL:
            foe_deck = {
                'turn_chain': ('Swamp', *CHAIN), 'foe_draw': CHAIN, 'two_draws': TWO_DRAWS,
                'paid_loot': (AIR, SHOCK, 'Mountain', 'Forest'),
            }.get(mode, (BEAR,))
        elif self.id == MIND and mode == 'short_library':
            foe_deck = (BEAR, 'Island')
        for player in (0, 1):
            deck = own_deck if player == self.owner else foe_deck
            for index, name in enumerate(deck):
                ref = f'library_{player}_{index}'
                self.add(ref, name, player, 'library')
                self.libraries[player].append(ref)

    def add(self, ref, name, player, zone='battlefield'):
        assert player in (0, 1)
        self.setup.append(card(name, ref, player, zone))
        self.cards[ref] = dict(name=name, player=player, zone=zone, damage=0)

    def emit(self, op, ref='', other='', prop='', *, player=0, value=0, zone=0, index=0, color=0):
        if op in ('untap_step', 'upkeep_step', 'draw_step'):
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

    def stats(self, ref):
        item = self.cards[ref]
        record = self.oracle[item['name']]
        return tuple(int(record[key]) + item.get('counters', 0) for key in ('power', 'toughness'))

    def observe(self, ref, prop, value):
        self.emit('assert_card', ref, prop=prop, value=value)

    def choices(self):
        self.emit('assert_unused_choices')
        self.emit('assert_choices_consumed')

    def target(self, actor, recipient):
        if isinstance(recipient, str):
            self.emit('choose_target', recipient, player=actor)
        else:
            self.emit('choose_player', player=actor, value=recipient)

    def wallet(self, actor, fee, *, funding=False):
        for color, value in enumerate(fee):
            self.emit('set_mana' if funding else 'assert_mana', player=actor, value=value, color=color)
        if funding:
            self.pools[actor] = list(fee)
            self.emit('refresh')

    def spent(self, actor):
        self.pools[actor] = ZERO.copy()
        self.wallet(actor, self.pools[actor])

    def cast(self, ref, target=None):
        item = self.cards[ref]
        actor = item['player']
        self.wallet(actor, cost_from_oracle(self.oracle[item['name']]), funding=True)
        if target is not None:
            self.target(actor, target)
        self.emit('cast', ref, value=1)
        self.choices()
        self.emit('assert_stack', value=1)
        self.spent(actor)
        self.resolve()
        item['zone'] = 'battlefield' if ref == 'source' else 'graveyard'

    def resolve(self):
        self.emit('resolve')
        self.emit('refresh')
        self.choices()
        self.emit('assert_stack')

    def mill(self, victim, count):
        taken = self.libraries[victim][:count]
        del self.libraries[victim][:count]
        for ref in taken:
            self.cards[ref]['zone'] = 'graveyard'
        creatures = [ref for ref in taken
                     if 'Creature' in self.oracle[self.cards[ref]['name']]['type_line'].split(' — ')[0]]
        if self.id == SIDISI and victim == self.owner and creatures:
            self.zombies += 1
        if self.id == ALCHEMIST and victim == self.foe:
            for ref in creatures:
                self.cards[ref]['zone'] = 'exile'
                self.zombies += 1

    def tutelage(self, drawer):
        if drawer != self.owner:
            return
        while self.libraries[self.foe]:
            pair = [self.oracle[self.cards[ref]['name']] for ref in self.libraries[self.foe][:2]]
            self.mill(self.foe, 2)
            if len(pair) < 2 or any('Land' in row['type_line'].split(' — ')[0] for row in pair):
                break
            if not set(pair[0]['colors']).intersection(pair[1]['colors']):
                break

    def draw(self, actor, count):
        assert len(self.libraries[actor]) >= count
        for _ in range(count):
            ref = self.libraries[actor].pop(0)
            self.cards[ref]['zone'] = 'hand'
            if self.id == TUTEL:
                self.tutelage(actor)

    def lose(self, player, amount):
        self.life[player] -= amount
        if self.id == MIND and player == self.foe:
            self.mill(player, amount)

    def damage_creature(self, ref, amount, dealer):
        item = self.cards[ref]
        item['damage'] += amount
        if item['damage'] >= self.stats(ref)[1]:
            item['zone'] = 'graveyard'
        if self.id == BELL and ref == 'source':
            self.mill(dealer, amount)

    def next_turn(self, actor, *, draw=False):
        assert self.pools == [ZERO, ZERO]
        for item in self.cards.values():
            item['damage'] = 0
        self.emit('advance_turn')
        self.emit('untap_step', player=actor)
        self.emit('upkeep_step', player=actor)
        if draw:
            self.emit('draw_step', player=actor)
            self.resolve()
            self.draw(actor, 1)
        self.emit('turn', player=actor)

    def attack(self, attacker, blocker=None, *, legal_block=True, fog=False, before_damage=None):
        self.emit('begin_attackers')
        self.emit('attack', attacker, value=1)
        self.observe(attacker, 'attacking', 1)
        if self.id == WORLD and self.owner == 0 and self.mode != 'decline':
            self.emit('choose_trigger_source', 'source', player=0)
        self.emit('finish_attackers')
        if self.id == SIDISI or (self.id == WORLD and self.mode != 'decline'):
            self.mill(self.owner, 3)
        self.emit('begin_blockers')
        if blocker:
            self.emit('block', blocker, attacker, value=int(legal_block))
            self.observe(blocker, 'blocking_count', int(legal_block))
        self.emit('advance_combat')
        if fog:
            self.cast('fog')
        if before_damage is not None:
            before_damage()
        self.emit('combat_damage')
        self.emit('resolve')
        self.emit('end_combat')
        self.emit('refresh')
        self.choices()
        self.emit('assert_stack')
        if fog:
            return
        active = self.cards[attacker]['player']
        if blocker and legal_block:
            self.damage_creature(blocker, self.stats(attacker)[0], active)
            self.damage_creature(attacker, self.stats(blocker)[0], 1-active)
        else:
            amount = self.stats(attacker)[0]
            if self.id == SZADEK and attacker == 'source':
                item = self.cards[attacker]
                item['counters'] = item.get('counters', 0) + amount
                self.mill(1-active, amount)
            elif (self.id == ALCHEMIST and active == self.owner
                  and 'Zombie' in self.oracle[self.cards[attacker]['name']]['type_line']):
                self.mill(1-active, amount)
            else:
                self.lose(1-active, amount)

    def belltower(self):
        if self.mode == 'combat':
            self.next_turn(self.foe)
            self.attack('attacker', 'source')
        elif self.mode == 'flight':
            self.next_turn(self.foe)
            self.next_turn(self.owner)
            self.observe('source', 'sick', 0)
            self.attack('source', 'blocker', legal_block=False)
        elif self.mode == 'prevented':
            self.cast('passage')
            self.cast('shock', 'source')
        else:
            spell = 'shock' if self.mode == 'own_spell' else 'bolt'
            self.cast(spell, 'source')
            self.damage_creature('source', 2 if self.mode == 'own_spell' else 3,
                                 self.cards[spell]['player'])
            self.receipts()
            if self.mode == 'lethal':
                self.cast('second_bolt', 'source')
                self.damage_creature('source', 3, self.foe)

    def mindcrank(self):
        if self.mode in ('combat', 'prevented'):
            self.attack('attacker', fog=self.mode == 'prevented')
        elif self.mode == 'own_loss':
            self.cast('costly_draw')
            self.draw(self.owner, 2)
            self.lose(self.owner, 2)
        else:
            self.cast('bump')
            self.lose(self.foe, 3)

    def draw_cases(self):
        if self.mode in ('turn_chain', 'foe_draw'):
            self.next_turn(self.foe, draw=True)
            self.receipts()
            if self.mode == 'turn_chain':
                self.next_turn(self.owner, draw=True)
        elif self.mode == 'two_draws':
            self.cast('divination')
            self.draw(self.owner, 2)
        else:
            fee = [5, 0, 1, 0, 0, 0]
            self.emit('can_activate', 'source', value=0)
            self.wallet(self.owner, fee, funding=True)
            self.emit('can_activate', 'source', value=1)
            self.emit('activate', 'source', value=1)
            self.emit('assert_stack', value=1)
            self.choices()
            self.spent(self.owner)
            self.draw(self.owner, 1)
            discarded = [item for item in self.cards.values()
                         if item['player'] == self.owner and item['zone'] == 'hand']
            item, = discarded
            if self.owner == 0:
                self.emit('choose_zone_card', player=0, value=self.rows[item['name']]['csvid'])
            self.resolve()
            item['zone'] = 'graveyard'

    def necromaster(self):
        self.next_turn(self.foe)
        self.next_turn(self.owner)
        self.observe('source', 'sick', 0)
        payable = self.mode in ('paid', 'decline')
        blocker = 'blocker' if self.mode in ('paid', 'blocked') else None
        self.wallet(self.owner, [2 if payable else 0, 0, 0, 0, 0, 0], funding=True)

        def offer():
            if self.owner == 0 and self.mode in ('paid', 'unfunded'):
                self.emit('choose_trigger_source', 'source', player=0)

        self.attack('source', blocker, legal_block=self.mode == 'blocked',
                    fog=self.mode == 'prevented', before_damage=offer)
        if self.mode == 'paid':
            self.spent(self.owner)
            self.zombies += 1
            self.mill(self.foe, 2)

    def add_followers(self):
        if self.id == SIDISI and self.mode in ('external_mill', 'foe_mill'):
            self.add('scour', SCOUR, self.owner, 'hand')
        if self.id == SZADEK:
            if self.mode in ('first', 'blocked'):
                self.add('blocker', AIR if self.mode == 'blocked' else BEAR, self.foe)
            if self.mode == 'noncombat':
                self.add('grant', STUDY, self.owner, 'hand')
        if self.id == ALCHEMIST:
            if self.mode == 'nonzombie':
                self.add('other', GIANT, self.owner)
            if self.mode == 'enemy_zombie':
                self.add('other', SCATHE, self.foe)
            if self.mode in ('external_mill', 'own_library'):
                self.add('scour', SCOUR, self.owner, 'hand')
        if self.id == WORLD:
            self.add('kill', SWORDS if self.mode == 'exiled' else MURDER, self.foe, 'hand')

    def follower_libraries(self, own_deck, foe_deck):
        batch = ('Forest', 'Island', 'Plains')
        mixed = (BEAR, AIR, 'Forest')
        mill_targets = (BEAR, 'Forest', AIR, GROWTH, 'Mountain', 'Plains', 'Island')
        if self.id == SIDISI:
            own_deck = mixed + DECK if self.mode == 'entry_mixed' else batch + mill_targets
            foe_deck = mill_targets
        if self.id == SZADEK:
            foe_deck = DECK * 3
        if self.id == ALCHEMIST:
            own_deck = foe_deck = mill_targets
        if self.id == WORLD:
            own_deck = ('Forest', AIR) if self.mode == 'short_library' else ('Forest', BEAR, 'Island', *DECK)
        return own_deck, foe_deck

    def age(self):
        self.next_turn(self.foe)
        self.next_turn(self.owner)
        self.observe('source', 'sick', 0)

    def sidisi(self):
        if self.mode == 'attack_trigger':
            self.age()
            self.attack('source')
        if self.mode in ('external_mill', 'foe_mill'):
            actor = self.owner if self.mode == 'external_mill' else self.foe
            self.cast('scour', actor)
            self.mill(actor, 5)

    def szadek(self):
        self.age()
        if self.mode == 'noncombat':
            self.cast('grant', 'source')
            self.cards['grant']['zone'] = 'battlefield'
            self.emit('can_activate', 'grant', value=1)
            self.target(self.owner, self.foe)
            self.emit('activate', 'grant', value=1)
            self.choices()
            self.emit('assert_stack', value=1)
            self.resolve()
            self.lose(self.foe, 1)
            return
        self.attack('source', blocker='blocker' if self.mode in ('first', 'blocked') else None,
                    legal_block=self.mode == 'blocked')
        self.receipts()
        if self.mode == 'repeat':
            self.age()
            self.attack('source')

    def alchemist(self):
        if self.mode == 'own_zombie':
            self.age()
            self.attack('source')
        elif self.mode == 'nonzombie':
            self.attack('other')
        elif self.mode == 'enemy_zombie':
            self.next_turn(self.foe)
            self.attack('other')
        else:
            actor = self.owner if self.mode == 'own_library' else self.foe
            self.cast('scour', actor)
            self.mill(actor, 5)

    def world_shaper(self):
        if self.mode in ('after_attack', 'short_library', 'decline'):
            self.age()
            self.attack('source')
            self.receipts()
        self.cast('kill', 'source')
        if self.mode == 'exiled':
            self.cards['source']['zone'] = 'exile'
            self.life[self.owner] += self.stats('source')[0]
            return
        self.cards['source']['zone'] = 'graveyard'
        for ref, item in self.cards.items():
            if (item['zone'] == 'graveyard' and item['player'] == self.owner
                    and 'Land' in self.oracle[item['name']]['type_line'].split(' — ')[0]):
                item['zone'] = 'battlefield'
                item['returned_tapped'] = True
                self.emit('bind_permanent', ref, player=self.owner,
                          value=self.rows[item['name']]['csvid'], index=-1)

    def receipts(self):
        self.emit('assert_stack')
        for actor in (0, 1):
            self.emit('assert_life', player=actor, value=self.life[actor])
            self.wallet(actor, self.pools[actor])
            expected = defaultdict(Counter)
            for item in self.cards.values():
                if item['player'] == actor:
                    expected[item['zone']][item['name']] += 1
            if self.id in ZOMBIE_SOURCES and actor == self.owner:
                expected['battlefield'][ZOMBIE] = self.zombies
            for zone, encoded in ZONES.items():
                counts = expected[zone]
                self.emit('assert_zone', player=actor, zone=encoded, value=counts.total())
                for name, count in sorted(counts.items()):
                    self.emit('assert_zone_multiplicity', player=actor, zone=encoded,
                              index=self.rows[name]['csvid'], value=count)
            for index, ref in enumerate(self.libraries[actor]):
                self.emit('assert_zone_card', player=actor, zone=ZONES['library'], index=index,
                          value=self.rows[self.cards[ref]['name']]['csvid'])
        for ref, item in self.cards.items():
            if item['zone'] == 'battlefield':
                self.observe(ref, 'in_play', 1)
                if item.get('returned_tapped'):
                    self.observe(ref, 'tapped', 1)
                if self.id == SZADEK and ref == 'source':
                    self.observe(ref, 'plus1_counters', item.get('counters', 0))
                    self.observe(ref, 'power', self.stats(ref)[0])
                    self.observe(ref, 'toughness', self.stats(ref)[1])
                if 'Creature' in self.oracle[item['name']]['type_line']:
                    self.observe(ref, 'damage', item['damage'])
        if self.id in ZOMBIE_SOURCES and self.zombies:
            for prop, value in (('token', 1), ('power', 2), ('toughness', 2),
                                ('color', 2), ('subtype_Zombie', 1)):
                self.emit('assert_matching_permanents', prop=prop, player=self.owner,
                          index=self.rows[ZOMBIE]['csvid'], value=value)

    def run(self):
        self.emit('turn', player=self.owner)
        for actor in (0, 1):
            self.wallet(actor, ZERO, funding=True)
        self.cast('source')
        if self.id == SIDISI:
            self.mill(self.owner, 3)
        self.receipts()
        paths = {BELL: self.belltower, MIND: self.mindcrank, TUTEL: self.draw_cases, DRAGON: self.necromaster,
                 SIDISI: self.sidisi, SZADEK: self.szadek, ALCHEMIST: self.alchemist, WORLD: self.world_shaper}
        paths[self.id]()
        self.receipts()
