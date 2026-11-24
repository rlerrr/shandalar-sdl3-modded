"""Paid attacks, counters, temporary stats, and printed riders."""
from collections import Counter, defaultdict
from functools import lru_cache
import json
from pathlib import Path

from counter_condition_recurrence_recipes import exact
from recipes import card, cost_from_oracle, step

FAMILY = 'entry_trigger_life_and_counters'
PROFILE = Path(__file__).with_name('data') / 'entry_trigger_life_and_counters_wave8_profiles.json'
EVISCERATOR, FIRSTBLADE, MINOTAUR, BLISTER, FARBROG, MEDICINE = 3989, 13242, 13485, 9922, 12178, 9597
SNAKE, SEEDSCULPTOR, KUDU, BANSHEE, FAMILIAR, TUSKER = 13809, 16427, 16988, 7891, 16376, 16785
HASTE = {FIRSTBLADE: (2, 2), MINOTAUR: (2, 0)}
SHRINK = {BLISTER: 1, FARBROG: 2, BANSHEE: 2}
SINGLE = {SEEDSCULPTOR: ('plus1', 1), KUDU: ('minus1', -1)}
SWORDS, MURDER, SPHERE, BEETLE = 'Swords to Plowshares', 'Murder', 'Sphere of the Suns', 'Clockwork Beetle'
ISLAND, PLAINS = 'Island', 'Plains'

WALKER, SNUFFERS, SKINRENDER, SPHINX = 16781, 9875, 11360, 10167
PREFIX = 'EntryTriggerLifeAndCountersRecipe'
GROUP = 'life-and-counters'
COMMON = ('short', 'countered')
MODES = {
    WALKER: (*COMMON, 'entry', 'recast', 'snapshot'),
    SNUFFERS: (*COMMON, 'each', 'lethal', 'protected', 'recast'),
    SKINRENDER: (*COMMON, 'target_enemy', 'target_own', 'self_only', 'lethal', 'protected', 'snapshot'),
    SPHINX: (*COMMON, 'opponent_down', 'self_down', 'opponent_up', 'self_up', 'flying', 'reach'),
}
ZONES = {'hand': 0, 'battlefield': 1, 'library': 2, 'graveyard': 3, 'exile': 4}
ZERO = [0] * 6
BALOTH, BEAR, KNIGHT, DRONE = 'Rumbling Baloth', 'Grizzly Bears', 'Black Knight', 'Spike Drone'
WHITE_KNIGHT = 'White Knight'
KOBOLD, THOPTER, INGOT = 'Kobolds of Kher Keep', 'Ornithopter', 'Darksteel Ingot'
BOUNCE, COUNTER, BOLT = 'Unsummon', 'Counterspell', 'Lightning Bolt'
SPIDER, LAND = 'Giant Spider', 'Mountain'
ZONE_NAMES = {code: name for name, code in ZONES.items()}
WITNESSES = frozenset(('Black Knight', 'Clockwork Beetle', 'Counterspell', 'Darksteel Ingot', 'Giant Spider', 'Grizzly Bears', 'Island', 'Kobolds of Kher Keep', 'Lightning Bolt', 'Mountain', 'Murder', 'Ornithopter', 'Plains', 'Rumbling Baloth', 'Sphere of the Suns', 'Spike Drone', 'Swords to Plowshares', 'Unsummon', 'White Knight'))


MODES.update({
    EVISCERATOR: ('life', 'protection', 'countered'),
    FIRSTBLADE: ('hit', 'blocked', 'countered'),
    MINOTAUR: ('hit', 'blocked', 'countered'),
    BLISTER: ('target_enemy', 'target_own', 'lethal', 'self', 'countered'),
    FARBROG: ('target_enemy', 'target_own', 'lethal', 'self', 'countered'),
    MEDICINE: ('white_charge', 'green_enemy', 'decline', 'countered'),
    SNAKE: ('paid', 'decline', 'recast', 'countered'),
    SEEDSCULPTOR: ('own', 'self', 'recast', 'countered'),
    KUDU: ('own', 'self', 'lethal', 'recast', 'countered'),
    BANSHEE: ('target_enemy', 'target_own', 'lethal', 'self', 'countered', 'flying'),
    FAMILIAR: ('entry', 'dies', 'bounce', 'countered'),
    TUSKER: ('team', 'solo', 'recast', 'countered'),
})
HUMAN_CHOICES = {(MEDICINE, 'decline')}
GAPS = {
    MEDICINE: ('Seat 1 automatically accepts the optional trigger. Human mode exercises refusal; only one present counter kind is offered at a time.',),
    SNAKE: ('In a two-player duel the only opposing player is the tribute chooser.',),
    SEEDSCULPTOR: ('Effects demonstrate own creature scope against a live opposing control; rejected direct ETB targets have no separate probe.',),
    KUDU: ('Effects demonstrate own creature scope against a live opposing control; rejected direct ETB targets have no separate probe.',),
}


@lru_cache(maxsize=1)
def profiles():
    return json.loads(PROFILE.read_bytes())


def classify(record, csvid):
    if (csvid not in MODES or record.get('layout') != 'normal' or record.get('card_faces')
            or record.get('selected_face') or 'Creature' not in record['type_line']
            or 'enters' not in record['oracle_text'].lower()):
        raise ValueError('normal whole entry-trigger creature required')
    return dict(csvid=csvid, printed_cost=record['mana_cost'])


def variants(shape):
    csvid = shape['csvid']
    return [f'{mode}_{owner}' for owner in (0, 1) for mode in MODES[csvid]
            if owner == 0 or (csvid, mode) not in HUMAN_CHOICES]


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
                    or oracle[name].get('layout') != 'normal'):
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
        cases.append(dict(name=f'EntryTriggerLifeAndCountersRecipe_{csvid}_{variant}', card=name,
                          fixture='empty_main', seed=3152, setup=board.setup, steps=board.steps,
                          _recipe=dict(family=FAMILY, version=2, variant=variant, params=declaration['params'],
                                       oracle_text=declaration['oracle_text'], untested_components=list(GAPS.get(csvid, ())))))
    return cases

class Board:
    def __init__(self, record, owner, mode, rows, oracle):
        self.id, self.name = record['csvid'], record['name']
        self.owner, self.foe, self.mode = owner, 1 - owner, mode
        self.rows, self.oracle = rows, oracle
        self.setup, self.steps, self.cards = [], [], {}
        self.life, self.energy = [20, 20], [0, 0]
        self.wallet = [ZERO.copy(), ZERO.copy()]
        self.base = self.size(self.name)
        self.add('source', self.name, owner, 'hand')
        if mode != 'short':
            self.add('own_land', LAND, owner)
        self.add('foe_land', LAND, self.foe)
        if mode == 'countered':
            self.add('counter', COUNTER, self.foe, 'hand')
        if mode == 'recast':
            self.add('bounce', BOUNCE, owner, 'hand')
        if self.id == SNUFFERS:
            self.add('own', BALOTH, owner)
            self.add('other', THOPTER, self.foe)
            self.add('ingot', INGOT, self.foe)
            if mode == 'lethal':
                self.add('dies', KOBOLD, owner)
                self.add('drone', DRONE, owner, 'hand')
            if mode == 'protected':
                self.add('black_knight', KNIGHT, self.foe)
        if self.id == SKINRENDER and mode != 'self_only':
            self.add('own', BALOTH, owner)
            self.add('other', BEAR if mode == 'lethal' else BALOTH, self.foe)
            if mode == 'protected':
                self.add('black_knight', WHITE_KNIGHT, self.foe)
        if self.id == SPHINX:
            if mode.endswith('_up'):
                for index in range(4):
                    self.add(f'bolt_{index}', BOLT, owner, 'hand')
            if mode in ('flying', 'reach'):
                self.add('blocker', BEAR if mode == 'flying' else SPIDER, self.foe)
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
        if op == 'choose_trigger':
            self.steps.append(step(op, ref=ref, player=player))
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

    def size(self, name):
        return tuple(int(self.oracle[name][field]) for field in ('power', 'toughness'))

    def stats(self, ref, minus=0):
        for prop, value in zip(('power', 'toughness'), self.size(self.cards[ref]['name'])):
            self.observe(ref, prop, value-minus)

    def counter(self, ref, count, kind='minus1'):
        self.emit('assert_counter', ref, prop=kind, value=count)

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

    def announce(self, ref, *, target=None, target_player=None, short=False, response=False):
        player = self.cards[ref]['player']
        if ref == 'source' and self.id == MEDICINE:
            record = self.oracle[self.name]
            if record['mana_cost'] != '{1}{G/W}':
                raise ValueError('exact green/white hybrid required')
            cost = ZERO.copy()
            cost[0] = 1
            cost[3 if self.mode == 'green_enemy' else 5] = 1
        else:
            cost = list(cost_from_oracle(self.oracle[self.cards[ref]['name']]))
        if short:
            assert cost[0] > 0
            cost[0] -= 1
        self.pool(player, cost, fund=True)
        if target is not None:
            self.emit('choose_target', target, player=player)
        if target_player is not None:
            self.emit('choose_player', player=player, value=target_player)
        self.emit('cast_response' if response else 'cast', ref, value=int(not short))
        self.choices()
        self.emit('assert_stack', value=0 if short else 2 if response else 1)
        if not short:
            self.wallet[player] = ZERO.copy()
        self.pool(player, self.wallet[player])

    def resolve(self, *, response=False):
        self.emit('resolve_response' if response else 'resolve')
        self.emit('refresh')
        self.choices()
        self.emit('assert_stack')

    def spell(self, ref, *, target=None, target_player=None, destination='graveyard'):
        self.announce(ref, target=target, target_player=target_player)
        self.resolve()
        self.move(ref, destination)

    def receipt(self):
        self.emit('assert_stack')
        for player in (0, 1):
            self.emit('assert_life', player=player, value=self.life[player])
            self.emit('assert_energy', player=player, value=self.energy[player])
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

    def unaffected(self):
        for ref in ('source', 'own', 'other', 'ingot', 'own_land', 'foe_land'):
            if ref in self.cards and self.cards[ref]['zone'] == 'battlefield':
                self.counter(ref, 0)

    def enter(self, *, target=None, target_player=None):
        self.announce('source')
        if target is not None:
            self.emit('choose_target', target, player=self.owner)
        if target_player is not None:
            self.emit('choose_player', player=self.owner, value=target_player)
        self.resolve()
        self.move('source', 'battlefield')
        if self.id in (WALKER, SPHINX):
            self.emit('assert_matching_permanents', prop='artifact', player=self.owner, index=self.id, value=1)
            self.stats('source')

    def bounced(self):
        self.spell('bounce', target='source')
        self.move('source', 'hand')
        self.emit('bind_hand_copy', 'source', player=self.owner, value=self.id)
        self.receipt()

    def walker(self):
        self.enter()
        self.life[self.owner] += 3
        self.energy[self.owner] += 3
        self.receipt()

    def snuffers(self, count):
        self.enter()
        self.counter('source', 1)
        self.stats('source', 1)
        self.counter('own', count)
        self.stats('own', count)
        if count == self.size(THOPTER)[1]:
            self.move('other', 'graveyard')
        else:
            self.counter('other', count)
            self.stats('other', count)
        for ref in ('own_land', 'foe_land', 'ingot'):
            self.counter(ref, 0)
        if self.mode == 'protected':
            self.counter('black_knight', 1)
            self.stats('black_knight', 1)
        if self.mode == 'lethal':
            self.move('dies', 'graveyard')
            self.move('drone', 'graveyard')
        self.receipt()

    def skinrender(self):
        target = {'target_own': 'own', 'self_only': 'source'}.get(self.mode, 'other')
        self.enter(target=target)
        for ref in ('source', 'own', 'other', 'black_knight'):
            if ref not in self.cards:
                continue
            count = 3 if ref == target else 0
            if self.size(self.cards[ref]['name'])[1] <= count:
                self.move(ref, 'graveyard')
            else:
                self.counter(ref, count)
                self.stats(ref, count)
        if self.mode == 'protected':
            self.emit('can_target_creature', 'source', 'black_knight')
            self.emit('can_target_creature', 'source', 'other', value=1)
        self.receipt()

    def sphinx(self):
        target = self.owner if self.mode.startswith('self_') else self.foe
        if self.mode.endswith('_up'):
            for index in range(4):
                self.spell(f'bolt_{index}', target_player=target)
                self.life[target] -= 3
                self.emit('assert_life', player=target, value=self.life[target])
        self.enter(target_player=target)
        self.life[target] = 10
        self.receipt()
        if self.mode not in ('flying', 'reach'):
            return
        for player in (self.foe, self.owner):
            self.emit('advance_turn')
            self.emit('untap_step', player=player)
            self.emit('turn', player=player)
        self.emit('begin_attackers')
        self.emit('attack', 'source', value=1)
        self.emit('finish_attackers')
        self.emit('begin_blockers')
        blocked = self.mode == 'reach'
        self.emit('block', 'blocker', 'source', value=int(blocked))
        self.emit('advance_combat')
        self.emit('assert_card', 'source', prop='blocked', value=int(blocked))
        self.emit('combat_damage')
        self.resolve()
        if blocked:
            self.move('blocker', 'graveyard')
            self.observe('source', 'damage', 2)
        else:
            self.life[self.foe] -= self.base[0]
        self.emit('end_combat_step')
        self.resolve()
        self.receipt()

    def replay(self, action):
        self.emit('capture')
        state = self.life.copy(), self.energy.copy(), {ref: item.copy() for ref, item in self.cards.items()}
        action()
        self.emit('restore')
        self.life, self.energy, self.cards = state
        self.receipt()
        action()

    def setup_follower(self):
        mode = self.mode
        if self.id == EVISCERATOR and mode == 'protection':
            self.add('white_probe', SWORDS, self.foe, 'hand')
            self.add('black_probe', MURDER, self.foe, 'hand')
            self.add('legal_creature', BALOTH, self.owner)
        if self.id in HASTE and mode == 'blocked':
            self.add('blocker', SPIDER, self.foe)
        if self.id in SHRINK and mode not in ('self', 'countered'):
            self.add('own', BALOTH, self.owner)
            victim = (KOBOLD if SHRINK[self.id] == 1 else BEAR) if mode == 'lethal' else BALOTH
            self.add('other', victim, self.foe)
            if mode == 'flying':
                self.add('blocker', BEAR, self.foe)
        if self.id == MEDICINE:
            if mode in ('white_charge', 'decline'):
                self.add('countered_artifact', SPHERE, self.owner, 'hand')
            if mode == 'green_enemy':
                self.add('countered_creature', BEETLE, self.foe, 'hand')
        if self.id in SINGLE and mode != 'countered':
            self.add('own', KOBOLD if mode == 'lethal' else BALOTH, self.owner)
            self.add('other', BALOTH, self.foe)
        if self.id == FAMILIAR:
            for player in (0, 1):
                self.add(f'deck_top_{player}', ISLAND, player, 'library')
                self.add(f'deck_bottom_{player}', PLAINS, player, 'library')
            if mode == 'dies':
                self.add('destroy_spell', MURDER, self.foe, 'hand')
            if mode == 'bounce':
                self.add('bounce', BOUNCE, self.owner, 'hand')
        if self.id == TUSKER and mode != 'countered':
            if mode != 'solo':
                self.add('own', BALOTH, self.owner)
                self.add('teammate', THOPTER, self.owner)
            self.add('other', BALOTH, self.foe)
            self.add('ingot', INGOT, self.owner)

    def visit_follower(self, player):
        self.emit('advance_turn')
        self.emit('untap_step', player=player)
        self.emit('turn', player=player)

    def combat_follower(self, power, blocker=None, *, legal=True, dies=False, damage=0):
        self.emit('begin_attackers')
        self.emit('attack', 'source', value=1)
        self.emit('finish_attackers')
        self.emit('begin_blockers')
        if blocker is not None:
            self.emit('block', blocker, 'source', value=int(legal))
        self.emit('advance_combat')
        self.observe('source', 'blocked', int(blocker is not None and legal))
        self.emit('combat_damage')
        self.resolve()
        if blocker is None or not legal:
            self.life[self.foe] -= power
        else:
            self.move(blocker, 'graveyard')
            if dies:
                self.move('source', 'graveyard')
            else:
                self.observe('source', 'damage', damage)
        self.emit('end_combat_step')
        self.resolve()
        self.receipt()

    def eviscerator(self):
        self.enter()
        self.life[self.owner] -= 5
        self.stats('source')
        if self.mode == 'protection':
            for source, legal in (('white_probe', 0), ('black_probe', 1)):
                self.emit('can_target_creature', source, 'source', value=legal)
                self.emit('can_target_creature', source, 'legal_creature', value=1)
        self.receipt()

    def self_pump(self):
        power, toughness = HASTE[self.id]
        self.enter()
        self.observe('source', 'power', self.base[0] + power)
        self.observe('source', 'toughness', self.base[1] + toughness)
        if self.mode == 'blocked':
            self.combat_follower(self.base[0] + power, 'blocker',
                                 dies=self.base[1] + toughness <= 2, damage=2)
        else:
            self.combat_follower(self.base[0] + power)
        self.visit_follower(self.foe)
        if self.cards['source']['zone'] == 'battlefield':
            self.stats('source')
        self.receipt()

    def shrink(self):
        amount = SHRINK[self.id]
        target = {'target_own': 'own', 'self': 'source'}.get(self.mode, 'other')
        self.enter(target=target)
        for ref in ('source', 'own', 'other'):
            if ref not in self.cards:
                continue
            change = amount if ref == target else 0
            if self.size(self.cards[ref]['name'])[1] <= change:
                self.move(ref, 'graveyard')
            else:
                self.stats(ref, change)
                self.counter(ref, 0)
        self.receipt()
        self.visit_follower(self.foe)
        for ref in ('source', 'own', 'other'):
            if ref in self.cards and self.cards[ref]['zone'] == 'battlefield':
                self.stats(ref)
        if self.mode == 'flying':
            self.visit_follower(self.owner)
            self.combat_follower(self.base[0], 'blocker', legal=False)
        self.receipt()

    def medicine(self):
        ref = 'countered_creature' if self.mode == 'green_enemy' else 'countered_artifact'
        kind, counters = ('plus1', 2) if self.mode == 'green_enemy' else ('charge', 3)
        if self.mode == 'green_enemy':
            self.visit_follower(self.foe)
        self.spell(ref, destination='battlefield')
        self.counter(ref, counters, kind)
        if self.mode == 'green_enemy':
            self.visit_follower(self.owner)
        self.announce('source')
        if self.mode != 'decline':
            if self.owner == 0:
                self.emit('choose_trigger', 'source', player=0)
            self.emit('choose_target', ref, player=self.owner)
            self.emit('choose_dialog', player=self.owner, value=0)
            self.emit('present_ai_dialogs', value=int(self.owner == 1))
            counters -= 1
        self.resolve()
        if self.mode != 'decline':
            self.emit('present_ai_dialogs', value=0)
        self.move('source', 'battlefield')
        self.stats('source')
        self.counter(ref, counters, kind)
        self.receipt()

    def tribute(self, paid):
        self.announce('source')
        self.emit('present_ai_dialogs', value=int(self.foe == 1))
        self.emit('choose_dialog', player=self.foe, value=int(not paid))
        self.resolve()
        self.emit('present_ai_dialogs', value=0)
        self.move('source', 'battlefield')
        self.counter('source', 3 if paid else 0, 'plus1')
        self.stats('source', -3 if paid else 0)
        self.life[self.owner] += 0 if paid else 4
        self.receipt()

    def count_target(self, *, replay=False):
        kind, signed = SINGLE[self.id]
        target = 'source' if self.mode == 'self' or replay else 'own'
        self.enter(target=target)
        for ref in ('source', 'own', 'other'):
            count = int(ref == target or (replay and ref == 'own'))
            if self.size(self.cards[ref]['name'])[1] + signed*count <= 0:
                self.move(ref, 'graveyard')
            else:
                self.counter(ref, count, kind)
                self.stats(ref, -signed*count)
        self.receipt()

    def familiar(self):
        self.enter()
        self.life[self.owner] += 2
        self.emit('assert_matching_permanents', prop='artifact', player=self.owner, index=self.id, value=1)
        self.stats('source')
        self.receipt()
        if self.mode == 'dies':
            self.spell('destroy_spell', target='source')
            self.move('source', 'graveyard')
            self.move(f'deck_top_{self.owner}', 'hand')
            self.emit('assert_zone_card', player=self.owner, zone=ZONES['library'],
                      value=self.rows[PLAINS]['csvid'])
        elif self.mode == 'bounce':
            self.bounced()
            self.emit('assert_zone_card', player=self.owner, zone=ZONES['library'],
                      value=self.rows[ISLAND]['csvid'])
        self.receipt()

    def tusker(self, counters):
        self.enter()
        for ref in ('source', 'own', 'teammate', 'other', 'ingot', 'own_land', 'foe_land'):
            if ref in self.cards:
                count = counters if ref in ('own', 'teammate') else 0
                self.counter(ref, count, 'plus1')
                if ref in ('source', 'own', 'teammate', 'other'):
                    self.stats(ref, -count)
        self.receipt()

    def run_follower(self):
        if self.id == EVISCERATOR:
            self.eviscerator()
        elif self.id in HASTE:
            self.self_pump()
        elif self.id in SHRINK:
            self.shrink()
        elif self.id == MEDICINE:
            self.medicine()
        elif self.id == SNAKE:
            self.tribute(self.mode == 'paid')
            if self.mode == 'recast':
                self.bounced()
                self.tribute(True)
        elif self.id in SINGLE:
            self.count_target()
            if self.mode == 'recast':
                self.bounced()
                self.count_target(replay=True)
        elif self.id == FAMILIAR:
            self.familiar()
        elif self.id == TUSKER:
            self.tusker(1)
            if self.mode == 'recast':
                self.bounced()
                self.tusker(2)
        else:
            raise ValueError('unsupported entry follower')

    def run(self):
        self.emit('turn', player=self.owner)
        for player in (0, 1):
            self.pool(player, ZERO, fund=True)
        self.unaffected()
        self.receipt()
        if self.mode == 'short':
            self.announce('source', short=True)
            self.unaffected()
            self.receipt()
            return
        if self.mode == 'countered':
            self.announce('source')
            self.emit('announce_spell', 'source')
            self.announce('counter', response=True)
            self.resolve(response=True)
            self.move('source', 'graveyard')
            self.move('counter', 'graveyard')
            self.unaffected()
            self.receipt()
            return
        if self.id == WALKER:
            if self.mode == 'snapshot':
                self.replay(self.walker)
            else:
                self.walker()
                if self.mode == 'recast':
                    self.bounced()
                    self.walker()
        elif self.id == SNUFFERS:
            if self.mode == 'lethal':
                self.spell('drone', destination='battlefield')
                self.counter('drone', 1, kind='plus1')
                self.receipt()
            self.snuffers(1)
            if self.mode == 'recast':
                self.bounced()
                self.snuffers(2)
        elif self.id == SKINRENDER:
            if self.mode == 'snapshot':
                self.replay(self.skinrender)
            else:
                self.skinrender()
        elif self.id == SPHINX:
            self.sphinx()
        else:
            self.run_follower()
