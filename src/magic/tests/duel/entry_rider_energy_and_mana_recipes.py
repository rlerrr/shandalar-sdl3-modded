"""Actual entry, counters, life, type gates and printed riders."""
from collections import Counter, defaultdict
from functools import lru_cache
import json
from pathlib import Path

from counter_condition_recurrence_recipes import exact
from recipes import card, cost_from_oracle, step

FAMILY = 'entry_rider_energy_and_mana'
PROFILE = Path(__file__).with_name('data') / 'entry_rider_energy_and_mana_wave8_profiles.json'
WHALE, PALINCHRON, TREACHERY, RENEGADE, DEADLOCK, PUMMELER = 3787, 1340, 1341, 16277, 16338, 16359
HERMIT, SENTRY, HELLION, WONDER, ARTIST, WHIRLER = 16408, 16420, 16429, 16455, 16534, 16574
LAND_UNTAPPERS = {WHALE: 7, PALINCHRON: 7, TREACHERY: 5}
DISENCHANT, KOBOLD, TURTLE, JACE = 'Disenchant', 'Kobolds of Kher Keep', 'Thriving Turtle', 'Jace Beleren'
LEECH, GROWTH, SAGE_NAME = 'Putrid Leech', 'Giant Growth', "Sage of Shaila's Claim"
RING, ATLAS, THOPTER = 'Sol Ring', 'Walking Atlas', 'Thopter'
TOKEN_NAMES = {THOPTER}

SAGE, PUZZLE, SERVANT, DRAKE = 16502, 16581, 16513, 1343
MODES = {
    SAGE: ('entry', 'countered', 'recast', 'opposing'),
    PUZZLE: ('entry', 'countered', 'sacrifice', 'wrong_green', 'short', 'snapshot'),
    SERVANT: ('sick', 'countered', 'black', 'blue', 'green', 'red', 'white', 'deplete'),
    DRAKE: ('empty', 'countered', 'capped', 'partial', 'opposing', 'flying'),
}
ZONES = {'hand': 0, 'battlefield': 1, 'library': 2, 'graveyard': 3, 'exile': 4}
ZERO = [0] * 6
ENTRY_ENERGY = {SAGE: 3, PUZZLE: 3, SERVANT: 2}
COLORS = {'black': 1, 'blue': 2, 'green': 3, 'red': 4, 'white': 5}
COUNTER, UNSUMMON, BOLT = 'Counterspell', 'Unsummon', 'Lightning Bolt'
MOUNTAIN, ISLAND, FOREST = 'Mountain', 'Island', 'Forest'
CONSTRUCT, BEAR = 'Sparring Construct', 'Grizzly Bears'
ZONE_NAMES = {code: name for name, code in ZONES.items()}
WITNESSES = frozenset(('Counterspell', 'Disenchant', 'Forest', 'Giant Growth', 'Grizzly Bears', 'Island', 'Jace Beleren', 'Kobolds of Kher Keep', 'Lightning Bolt', 'Mountain', 'Putrid Leech', "Sage of Shaila's Claim", 'Sol Ring', 'Sparring Construct', 'Thopter', 'Thriving Turtle', 'Unsummon', 'Walking Atlas'))


ENTRY_ENERGY.update({
    RENEGADE: 4, DEADLOCK: 2, PUMMELER: 3, HERMIT: 4, SENTRY: 2,
    HELLION: 2, WONDER: 3, ARTIST: 1, WHIRLER: 3,
})
MODES.update({
    WHALE: ('empty', 'capped', 'partial', 'countered'),
    PALINCHRON: ('capped', 'partial', 'bounce', 'flying', 'countered'),
    TREACHERY: ('capped', 'partial', 'control', 'release', 'countered'),
    RENEGADE: ('entry', 'ping', 'burn', 'walker', 'countered'),
    DEADLOCK: ('entry', 'lock', 'planeswalker', 'countered'),
    PUMMELER: ('entry', 'double', 'scaled', 'repeat', 'countered'),
    HERMIT: ('defender', 'pay', 'expiry', 'countered'),
    SENTRY: ('entry', 'tap', 'untap', 'decline', 'countered'),
    HELLION: ('pay_then_empty', 'decline', 'countered'),
    WONDER: ('flying', 'vigilance', 'lifelink', 'expiry', 'power', 'toughness', 'countered'),
    ARTIST: ('self', 'ally', 'foe', 'countered'),
    WHIRLER: ('make', 'repeat', 'flying', 'countered'),
})
GAPS = {
    ARTIST: ('Tests actual haste-dependent attacks for each target scope. Once a creature ages, combat cannot distinguish an expired haste grant.',),
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
        cases.append(dict(name=f'EntryRiderEnergyAndManaRecipe_{csvid}_{variant}', card=name,
                          fixture='empty_main', seed=3308, setup=board.setup, steps=board.steps,
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
        self.energy = [0, 0]
        self.setup_energy_mana()
        self.setup_followers()

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

    def announce(self, ref, *, target=None, target_player=None, response=False, spent=None):
        player = self.cards[ref]['player']
        printed = cost_from_oracle(self.oracle[self.cards[ref]['name']])
        if spent is None:
            spent = printed
            self.pool(player, spent, fund=True)
        elif player == 0:
            # Specific colored costs consume their matching pools automatically.
            # The human wallet explicitly allocates extra colored mana to a generic cost.
            excess = [spent[color] - printed[color] for color in range(1, 6)]
            assert min(excess) >= 0 and sum(excess) == printed[0] - spent[0]
            for color, payment in enumerate(excess, start=1):
                for _ in range(payment):
                    self.emit('choose_mana', player=player, color=color)
        remain = [have - used for have, used in zip(self.wallet[player], spent)]
        assert min(remain) >= 0
        if target is not None:
            self.emit('choose_target', target, player=player)
        if target_player is not None:
            self.emit('choose_player', player=player, value=target_player)
        self.emit('cast_response' if response else 'cast', ref, value=1)
        self.choices()
        self.emit('assert_stack', value=2 if response else 1)
        self.wallet[player] = remain
        self.pool(player, remain)

    def resolve(self, *, response=False):
        self.emit('resolve_response' if response else 'resolve')
        self.emit('refresh')
        self.choices()
        self.emit('assert_stack')

    def spell(self, ref, *, target=None, target_player=None, destination='graveyard', spent=None):
        self.announce(ref, target=target, target_player=target_player, spent=spent)
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

    def visit(self, player):
        assert self.wallet == [ZERO, ZERO]
        self.emit('advance_turn')
        self.emit('untap_step', player=player)
        self.emit('turn', player=player)

    def bounced(self):
        self.move('source', 'hand')
        self.emit('bind_hand_copy', 'source', player=self.owner, value=self.id)

    def countered(self):
        self.announce('source', target='host' if self.id == TREACHERY else None)
        self.emit('announce_spell', 'source')
        self.announce('counter', response=True)
        self.resolve(response=True)
        self.move('source', 'graveyard')
        self.move('counter', 'graveyard')
        self.receipt()

    def setup_energy_mana(self):
        mode = self.mode
        if self.id == SAGE:
            if mode == 'recast':
                self.add('bounce', UNSUMMON, self.owner, 'hand')
            if mode == 'opposing':
                self.add('enemy_sage', self.name, self.foe, 'hand')
        if self.id == SERVANT and mode == 'deplete':
            for index in range(2):
                self.add(f'spender_{index}', CONSTRUCT, self.owner, 'hand')
        if self.id == SERVANT and mode == 'red':
            self.add('burn', BOLT, self.owner, 'hand')
        if self.id == DRAKE and mode in ('capped', 'partial', 'opposing'):
            count = 5 if mode == 'capped' else 4
            for index in range(count):
                self.add(f'land_{index}', MOUNTAIN, self.owner)
            self.add('blue_land', ISLAND, self.owner)
            if mode == 'opposing':
                self.add('enemy_land', FOREST, self.foe)
        if self.id == DRAKE and mode == 'flying':
            self.add('blocker', BEAR, self.foe)

    def enter_source(self):
        self.spell('source', destination='battlefield')
        if self.id in (PUZZLE, DEADLOCK, PUMMELER, WONDER):
            self.emit('assert_matching_permanents', prop='artifact', player=self.owner, index=self.id, value=1)
        if self.id not in (PUZZLE, DEADLOCK):
            self.stats('source')
        self.energy[self.owner] += ENTRY_ENERGY[self.id]
        self.life[self.owner] += 3 if self.id == PUZZLE else 0
        self.receipt()

    def sage(self):
        if self.mode == 'opposing':
            self.visit(self.foe)
            self.spell('enemy_sage', destination='battlefield')
            self.energy[self.foe] += ENTRY_ENERGY[SAGE]
            self.receipt()
            self.visit(self.owner)
        self.enter_source()
        if self.mode == 'recast':
            self.spell('bounce', target='source')
            self.bounced()
            self.receipt()
            self.enter_source()

    def activate_puzzle(self, payable=True):
        fee = [2, 0, 0, 1, 0, 0]
        if self.mode == 'wrong_green':
            fee = [3, 0, 0, 0, 0, 0]
        if self.mode == 'short':
            fee[0] = 1
        self.pool(self.owner, fee, fund=True)
        self.emit('can_activate', 'source', value=int(payable))
        self.emit('activate', 'source', value=int(payable))
        self.choices()
        if payable:
            self.wallet[self.owner] = ZERO.copy()
            self.move('source', 'graveyard')
            self.emit('assert_stack', value=1)
            self.pool(self.owner, ZERO)
            for player in (0, 1):
                self.emit('assert_life', player=player, value=self.life[player])
                self.emit('assert_energy', player=player, value=self.energy[player])
            self.emit('assert_zone_multiplicity', player=self.owner, zone=ZONES['graveyard'], index=self.id, value=1)
            self.resolve()
            self.life[self.owner] += 3
            self.energy[self.owner] += 3
        self.receipt()

    def puzzle(self):
        self.enter_source()
        if self.mode == 'entry':
            return
        if self.mode == 'snapshot':
            self.emit('capture')
            state = (self.life.copy(), self.energy.copy(), [wallet.copy() for wallet in self.wallet],
                     {ref: item.copy() for ref, item in self.cards.items()})
            self.activate_puzzle()
            self.emit('restore')
            self.life, self.energy, self.wallet, self.cards = state
            self.receipt()
        self.activate_puzzle(payable=self.mode not in ('wrong_green', 'short'))

    def activate_servant(self, color):
        self.emit('can_activate', 'source', value=1)
        self.emit('choose_color', player=self.owner, value=color)
        self.emit('activate', 'source', value=1)
        self.choices()
        self.emit('assert_stack')
        self.observe('source', 'tapped', 1)
        self.energy[self.owner] -= 1
        self.wallet[self.owner][color] += 1
        self.receipt()
        self.emit('can_activate', 'source', value=0)

    def servant(self):
        self.enter_source()
        self.emit('can_activate', 'source', value=0)
        if self.mode == 'sick':
            return
        self.visit(self.foe)
        self.visit(self.owner)
        if self.mode == 'deplete':
            for index in range(2):
                self.activate_servant(3)
                spent = [0, 0, 0, 1, 0, 0]
                self.spell(f'spender_{index}', destination='battlefield', spent=spent)
                self.visit(self.foe)
                self.visit(self.owner)
            self.observe('source', 'tapped', 0)
            self.emit('can_activate', 'source', value=0)
            self.emit('activate', 'source', value=0)
        else:
            self.activate_servant(COLORS[self.mode])
            if self.mode == 'red':
                self.spell('burn', target_player=self.foe, spent=[0, 0, 0, 0, 1, 0])
                self.life[self.foe] -= 3
        self.receipt()

    def tap_land(self, ref, color):
        player = self.cards[ref]['player']
        self.emit('activate', ref, value=1)
        self.choices()
        self.emit('assert_stack')
        self.wallet[player][color] += 1
        self.observe(ref, 'tapped', 1)

    def drake(self):
        mode = self.mode
        if mode in ('empty', 'flying'):
            self.spell('source', destination='battlefield')
            self.stats('source')
            self.receipt()
            if mode == 'flying':
                self.visit(self.foe)
                self.visit(self.owner)
                self.emit('begin_attackers')
                self.emit('attack', 'source', value=1)
                self.emit('finish_attackers')
                self.emit('begin_blockers')
                self.emit('block', 'blocker', 'source', value=0)
                self.emit('advance_combat')
                self.observe('source', 'blocked', 0)
                self.emit('combat_damage')
                self.resolve()
                self.life[self.foe] -= int(self.oracle[self.name]['power'])
                self.emit('end_combat_step')
                self.resolve()
                self.receipt()
            return
        lands = [ref for ref in self.cards if ref.startswith('land_')]
        for ref in lands:
            self.tap_land(ref, 4)
        self.tap_land('blue_land', 2)
        if mode == 'opposing':
            self.tap_land('enemy_land', 3)
            lands.append('enemy_land')
        self.pool(self.owner, self.wallet[self.owner])
        self.pool(self.foe, self.wallet[self.foe])
        self.announce('source', spent=[0, 0, 1, 0, 4, 0])
        untap = ('blue_land', *lands[:4]) if mode == 'capped' else ('blue_land', 'land_1') if mode == 'partial' else ('enemy_land',)
        for ref in untap:
            self.emit('choose_target', ref, player=self.owner)
        if len(untap) < 5:
            self.emit('choose_optional_target', 'land_0', player=self.owner)
        self.resolve()
        self.emit('assert_forced_target_choices')
        self.move('source', 'battlefield')
        for ref in ('blue_land', *lands):
            self.observe(ref, 'tapped', int(ref not in untap))
        if mode == 'partial':
            self.tap_land('blue_land', 2)
        self.receipt()

    def setup_followers(self):
        mode = self.mode
        if self.id in LAND_UNTAPPERS:
            if self.id == TREACHERY:
                self.add('host', BEAR, self.foe)
                if mode == 'release':
                    self.add('release', DISENCHANT, self.foe, 'hand')
            if mode in ('capped', 'partial'):
                cost = cost_from_oracle(self.oracle[self.name])
                for index in range(cost[0] + int(mode == 'capped')):
                    self.add(f'red_{index}', MOUNTAIN, self.owner)
                for index in range(cost[2]):
                    self.add(f'blue_{index}', ISLAND, self.owner)
            if mode == 'flying':
                self.add('blocker', BEAR, self.foe)
        if self.id == RENEGADE and mode in ('ping', 'burn', 'walker'):
            self.add('victim', KOBOLD, self.foe)
            if mode in ('burn', 'walker'):
                for index in range(2):
                    self.add(f'donor_{index}', TURTLE, self.owner, 'hand')
            if mode == 'walker':
                self.add('walker', JACE, self.foe)
        if self.id == DEADLOCK and mode in ('lock', 'planeswalker'):
            self.add('victim', JACE if mode == 'planeswalker' else LEECH,
                     self.owner if mode == 'planeswalker' else self.foe)
        if self.id == PUMMELER:
            if mode == 'scaled':
                self.add('growth', GROWTH, self.owner, 'hand')
            if mode == 'repeat':
                self.add('donor_0', SAGE_NAME, self.owner, 'hand')
        if self.id == SENTRY:
            if mode in ('tap', 'decline'):
                self.add('victim', BEAR, self.foe)
            if mode == 'untap':
                self.add('victim', RING, self.owner)
                self.add('spender', ATLAS, self.owner, 'hand')
        if self.id == WONDER and mode in ('flying', 'expiry'):
            self.add('blocker', BEAR, self.foe)
        if self.id == ARTIST and mode in ('ally', 'foe'):
            self.add('recipient', BEAR, self.foe if mode == 'foe' else self.owner, 'hand')
        if self.id == WHIRLER:
            if mode == 'flying':
                self.add('blocker', BEAR, self.foe)
            if mode == 'repeat':
                self.add('donor_0', SAGE_NAME, self.owner, 'hand')

    def age(self):
        self.visit(self.foe)
        self.visit(self.owner)

    def activate_ability(self, fee, *, target=None, target_player=None, mode=None, result=None):
        self.emit('can_activate', 'source', value=1)
        if mode is not None or result is not None:
            self.emit('present_ai_dialogs', value=int(self.owner == 1))
        if mode is not None:
            self.emit('choose_dialog', player=self.owner, value=mode)
        if target is not None:
            self.emit('choose_target', target, player=self.owner)
        if target_player is not None:
            self.emit('choose_player', player=self.owner, value=target_player)
        self.emit('activate', 'source', value=1)
        self.choices()
        self.emit('assert_stack', value=1)
        self.energy[self.owner] -= fee
        self.emit('assert_energy', player=self.owner, value=self.energy[self.owner])
        if result is not None:
            self.emit('choose_dialog', player=self.owner, value=result)
        self.resolve()
        if mode is not None or result is not None:
            self.emit('present_ai_dialogs', value=0)

    def enter_donor(self, ref, amount):
        self.spell(ref, destination='battlefield')
        self.energy[self.owner] += amount
        self.receipt()

    def combat_follow_on(self, attacker='source', *, player=None, power=None, flying=False,
                         vigilance=False, lifelink=False, legal=True):
        actor = self.owner if player is None else player
        power = int(self.oracle[self.cards[attacker]['name']]['power']) if power is None else power
        self.emit('begin_attackers')
        self.emit('attack', attacker, value=int(legal))
        self.emit('finish_attackers')
        if legal:
            self.observe(attacker, 'tapped', int(not vigilance))
        self.emit('begin_blockers')
        if flying:
            self.emit('block', 'blocker', attacker, value=0)
        self.emit('advance_combat')
        self.emit('combat_damage')
        self.resolve()
        if legal:
            self.life[1-actor] -= power
            if lifelink:
                self.life[actor] += power
        self.emit('end_combat_step')
        self.resolve()
        self.receipt()

    def control_host(self, player):
        self.cards['host']['player'] = player
        self.emit('bind_permanent_copy', 'host', player=player, value=self.rows[BEAR]['csvid'])

    def untapper_entry(self):
        paid_lands = [ref for ref in self.cards if ref.startswith(('red_', 'blue_'))]
        for ref in paid_lands:
            self.tap_land(ref, 4 if ref.startswith('red_') else 2)
        if paid_lands:
            cost = cost_from_oracle(self.oracle[self.name])
            spent = [0, 0, cost[2], 0, cost[0], 0]
        else:
            spent = None
        self.announce('source', target='host' if self.id == TREACHERY else None, spent=spent)
        cap = LAND_UNTAPPERS[self.id]
        chosen = paid_lands[:cap] if self.mode == 'capped' else paid_lands[:2]
        for ref in chosen:
            self.emit('choose_target', ref, player=self.owner)
        if paid_lands and len(chosen) < cap:
            self.emit('choose_optional_target', paid_lands[-1], player=self.owner)
        self.resolve()
        if paid_lands:
            self.emit('assert_forced_target_choices')
        self.move('source', 'battlefield')
        for ref in paid_lands:
            self.observe(ref, 'tapped', int(ref not in chosen))
        if self.id == TREACHERY:
            self.control_host(self.owner)
        else:
            self.stats('source')
        self.receipt()

    def untapper(self):
        self.untapper_entry()
        if self.mode == 'bounce':
            self.pool(self.owner, [2, 0, 2, 0, 0, 0], fund=True)
            self.emit('can_activate', 'source', value=1)
            self.emit('activate', 'source', value=1)
            self.emit('assert_stack', value=1)
            self.wallet[self.owner] = ZERO.copy()
            self.pool(self.owner, ZERO)
            self.resolve()
            self.bounced()
            self.receipt()
            self.untapper_entry()
        elif self.mode == 'release':
            self.spell('release', target='source')
            self.move('source', 'graveyard')
            self.control_host(self.foe)
            self.receipt()
        elif self.mode == 'flying':
            self.age()
            self.combat_follow_on(flying=True)

    def renegade(self):
        if self.mode in ('burn', 'walker'):
            self.enter_donor('donor_0', 2)
            self.enter_donor('donor_1', 2)
        self.enter_source()
        self.emit('can_activate', 'source', value=0)
        if self.mode == 'entry':
            return
        self.age()
        if self.mode == 'ping':
            self.activate_ability(2, target='victim', mode=0)
            self.move('victim', 'graveyard')
        else:
            if self.mode == 'walker':
                self.emit('assert_counter', 'walker', prop='loyalty', value=3)
                self.activate_ability(8, target='walker', mode=1)
                self.move('walker', 'graveyard')
            else:
                self.activate_ability(8, target_player=self.foe, mode=1)
                self.life[self.foe] -= 6
            self.observe('victim', 'damage', 0)
        self.observe('source', 'tapped', 1)
        self.emit('can_activate', 'source', value=0)
        self.receipt()

    def deadlock(self):
        self.enter_source()
        self.observe('source', 'tapped', 1)
        self.emit('can_activate', 'source', value=0)
        if self.mode == 'entry':
            return
        self.age()
        self.emit('can_activate', 'victim', value=1)
        self.activate_ability(1, target='victim')
        self.observe('source', 'tapped', 1)
        self.observe('victim', 'tapped', 1)
        self.emit('can_activate', 'victim', value=0)
        self.receipt()
        self.age()
        self.emit('can_activate', 'victim', value=1)
        self.receipt()

    def pummeler(self):
        if self.mode == 'repeat':
            self.enter_donor('donor_0', 3)
        self.enter_source()
        if self.mode == 'entry':
            return
        size = 1
        if self.mode == 'scaled':
            self.spell('growth', target='source')
            size += 3
            self.stats('source', size-1)
        for _ in range(2 if self.mode == 'repeat' else 1):
            self.activate_ability(3)
            size *= 2
            self.stats('source', size-1)
            self.receipt()
        self.emit('can_activate', 'source', value=0)
        self.visit(self.foe)
        self.stats('source')
        self.receipt()

    def hermit(self):
        self.enter_source()
        self.age()
        if self.mode in ('pay', 'expiry'):
            self.activate_ability(2)
        if self.mode == 'expiry':
            self.age()
        self.combat_follow_on(legal=self.mode == 'pay')

    def sentry(self):
        self.enter_source()
        self.emit('can_activate', 'source', value=0)
        if self.mode == 'entry':
            return
        self.age()
        if self.mode == 'untap':
            self.emit('activate', 'victim', value=1)
            self.emit('assert_stack')
            self.wallet[self.owner][0] = 2
            self.observe('victim', 'tapped', 1)
            self.spell('spender', destination='battlefield', spent=[2, 0, 0, 0, 0, 0])
        self.activate_ability(2, target='victim', result={'tap':0,'untap':1,'decline':2}[self.mode])
        self.observe('source', 'tapped', 1)
        self.observe('victim', 'tapped', int(self.mode == 'tap'))
        self.emit('can_activate', 'source', value=0)
        self.receipt()

    def hellion(self):
        self.enter_source()
        self.combat_follow_on()
        self.emit('present_ai_dialogs', value=int(self.owner == 1))
        self.emit('choose_dialog', player=self.owner, value=int(self.mode == 'decline'))
        self.visit(self.foe)
        self.choices()
        self.emit('present_ai_dialogs', value=0)
        if self.mode == 'decline':
            self.move('source', 'graveyard')
        else:
            self.energy[self.owner] -= 2
        self.receipt()
        if self.mode == 'pay_then_empty':
            self.visit(self.owner)
            self.receipt()
            self.visit(self.foe)
            self.move('source', 'graveyard')
            self.receipt()

    def printed_stats(self, power, toughness):
        self.observe('source', 'power', power)
        self.observe('source', 'toughness', toughness)

    def wonder(self):
        self.enter_source()
        mode = self.mode
        combat_modes = ('flying', 'vigilance', 'lifelink', 'expiry')
        if mode in combat_modes:
            self.age()
            self.activate_ability(1, mode=0, result={'flying':0,'vigilance':1,'lifelink':2,'expiry':0}[mode])
            if mode == 'expiry':
                self.age()
                self.emit('begin_attackers')
                self.emit('attack', 'source', value=1)
                self.emit('finish_attackers')
                self.emit('begin_blockers')
                self.emit('block', 'blocker', 'source', value=1)
                self.emit('advance_combat')
                self.emit('combat_damage')
                self.resolve()
                self.move('blocker', 'graveyard')
                self.observe('source', 'damage', 2)
                self.emit('end_combat_step')
                self.resolve()
            else:
                self.combat_follow_on(flying=mode=='flying', vigilance=mode=='vigilance', lifelink=mode=='lifelink')
        else:
            self.activate_ability(1, mode=1, result=int(mode == 'toughness'))
            self.printed_stats(1,5) if mode == 'toughness' else self.printed_stats(5,1)
            self.visit(self.foe)
            self.stats('source')
        self.receipt()

    def artist(self):
        self.enter_source()
        mode = self.mode
        target = 'source' if mode == 'self' else 'recipient'
        actor = self.foe if mode == 'foe' else self.owner
        if mode == 'foe':
            self.visit(actor)
        if target != 'source':
            self.spell(target, destination='battlefield')
        self.activate_ability(1, target=target)
        self.combat_follow_on(target, player=actor)
        self.emit('can_activate', 'source', value=0)

    def whirler(self):
        if self.mode == 'repeat':
            self.enter_donor('donor_0', 3)
        self.enter_source()
        for index in range(2 if self.mode == 'repeat' else 1):
            self.activate_ability(3)
            ref = f'thopter_{index}'
            self.cards[ref] = dict(name=THOPTER, player=self.owner, zone='battlefield')
            self.emit('bind_permanent_copy', ref, player=self.owner, index=index, value=self.rows[THOPTER]['csvid'])
            for prop,value in (('power',1),('toughness',1),('color',0),('artifact',1),('subtype_Thopter',1),('token',1)):
                self.emit('assert_matching_permanents', prop=prop, player=self.owner, index=self.rows[THOPTER]['csvid'], value=value)
            self.receipt()
        self.emit('can_activate', 'source', value=0)
        if self.mode == 'flying':
            self.age()
            self.combat_follow_on('thopter_0', flying=True)

    def run(self):
        self.emit('turn', player=self.owner)
        for player in (0, 1):
            self.pool(player, ZERO, fund=True)
        self.receipt()
        if self.mode == 'countered':
            self.countered()
            return
        methods = {SAGE: self.sage, PUZZLE: self.puzzle, SERVANT: self.servant, DRAKE: self.drake}
        methods.update({WHALE: self.untapper, PALINCHRON: self.untapper, TREACHERY: self.untapper,
                        RENEGADE: self.renegade, DEADLOCK: self.deadlock, PUMMELER: self.pummeler,
                        HERMIT: self.hermit, SENTRY: self.sentry, HELLION: self.hellion,
                        WONDER: self.wonder, ARTIST: self.artist, WHIRLER: self.whirler})
        methods[self.id]()
