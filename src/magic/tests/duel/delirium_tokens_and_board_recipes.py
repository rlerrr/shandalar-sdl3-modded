"""Exercise Delirium token, stat, damage, and blocking clauses in the duel engine."""
from copy import deepcopy
from functools import lru_cache
import json
from pathlib import Path

from counter_condition_recurrence_recipes import exact
from recipes import card, cost_from_oracle, step


FAMILY = 'delirium_tokens_and_board'
PROFILE = Path(__file__).with_name('data') / 'delirium_tokens_and_board_wave8_profiles.json'
ZONE = {'hand': 0, 'battlefield': 1, 'library': 2, 'graveyard': 3, 'exile': 4}
ZONE_NAMES = {number: name for name, number in ZONE.items()}
THREE = ('Mountain', 'Grizzly Bears', 'Giant Growth', 'Mountain')
FOUR = ('Mountain', 'Grizzly Bears', 'Giant Growth', "Traveler's Amulet")
WITNESSES = set(THREE + FOUR) | {
    'Island', 'Plains', 'Wind Drake', 'Giant Spider', 'Hill Giant', 'Shock',
    'Swords to Plowshares', 'Counterspell', 'Disenchant',
}
TOKENS = {'Angel', 'Ooze', 'Eldrazi Horror', 'Spider'}
BRANCHES = {15785: 'descend', 15862: 'blob', 15964: 'swallower', 16075: 'sentry',
            16136: 'ishkanah', 16159: 'mourn'}
GAP = 'Priority between individual declaration/upkeep triggers and their resolution is outside the tests.'
DENIED = ('short', 'wrong_color', 'countered')
FRESH = DENIED + ('gain_fourth',)
REPLAY = {15862, 15964, 16075}
CARD_TYPES = {'Artifact', 'Battle', 'Creature', 'Enchantment', 'Instant',
              'Kindred', 'Land', 'Planeswalker', 'Sorcery'}


DESCEND, BLOB, SOUL, SENTRY, ISHKANAH, MOURN = 15785, 15862, 15964, 16075, 16136, 16159
GATE = 'if there are four or more card types among cards in your graveyard'
DELIRIUM = 'Delirium — '
CLAUSES = {
    DESCEND: ('Exile all creatures.\n' + DELIRIUM +
              f'Create a 4/4 white Angel creature token with flying {GATE}.'),
    BLOB: (DELIRIUM + f'Whenever this creature attacks, {GATE}, ' +
           "create a 3/3 green Ooze creature token that's tapped and attacking."),
    SOUL: ('Trample\n' + DELIRIUM + f'At the beginning of your upkeep, {GATE}, ' +
           'put three +1/+1 counters on this creature.'),
    SENTRY: ('When this creature dies, create a 3/2 colorless Eldrazi Horror creature token.\n' +
             DELIRIUM + 'This creature gets +3/+0 as long as there are four or more card types '
             'among cards in your graveyard.'),
    ISHKANAH: ('Reach\n' + DELIRIUM + f'When Ishkanah enters, {GATE}, create three 1/2 green Spider '
               'creature tokens with reach.\n{6}{B}: Target opponent loses 1 life for each Spider you control.'),
    MOURN: ('Haste\n' + DELIRIUM + f'When this creature enters, {GATE}, '
            "creatures with power 2 or less can't block this turn."),
}
COMMON = ('three_duplicates', 'four_types', 'opponent_only')
EXTRAS = {DESCEND: 'angel_flying', BLOB: 'other_attacks', SOUL: 'repeat_upkeep',
          SENTRY: 'exiled', ISHKANAH: 'spider_reach', MOURN: 'power_changes'}



@lru_cache(maxsize=1)
def profiles():
    return json.loads(PROFILE.read_bytes())


def classify(record, csvid):
    if (csvid not in CLAUSES or record.get('oracle_text') != CLAUSES[csvid]
            or record.get('layout') != 'normal' or record.get('selected_face') or record.get('card_faces')
            or 'Delirium' not in record.get('keywords', ())):
        raise ValueError('complete normal Delirium primary and printed clauses required')
    if (csvid == DESCEND and record['type_line'] != 'Sorcery'
            or csvid != DESCEND and not all(record.get(stat, '').isdigit() for stat in ('power', 'toughness'))):
        raise ValueError('printed primary spell type and numeric creature stats required')
    return dict(csvid=csvid, branch=BRANCHES[csvid], mana=cost_from_oracle(record))


def modes(csvid):
    original = (*COMMON, EXTRAS[csvid], *(('expires',) if csvid == MOURN else ()))
    return original + FRESH + (('replay',) if csvid in REPLAY else ())


def variants(shape):
    return [f'{mode}_{owner}' for mode in modes(shape['csvid']) for owner in (0, 1)]


def validate_variants(shape, selected):
    if (type(selected) is not list or not selected or any(type(mode) is not str for mode in selected)
            or len(set(selected)) != len(selected) or set(selected) - set(variants(shape))):
        raise ValueError('distinct supported Delirium board variants required')


def spec(declaration, record):
    pinned = profiles()['sources'].get(record.get('name'))
    if not pinned or not exact(pinned['oracle'], record):
        raise ValueError('exact complete Delirium primary required')
    shape = classify(record, pinned['registry']['csvid'])
    if (declaration.get('family') != FAMILY or type(declaration.get('version')) is not int
            or declaration['version'] != 2 or declaration.get('oracle_text') != record['oracle_text']
            or not exact(declaration.get('params'), shape)):
        raise ValueError('exact typed Delirium board declaration required')
    validate_variants(shape, declaration.get('variants'))
    return dict(shape, family=FAMILY)


def types_for(cards, oracle):
    return {kind for name in cards for kind in oracle[name]['type_line'].split(' — ', 1)[0].split()
            if kind in CARD_TYPES}


def validate_witnesses(oracle, rows):
    pins = profiles()
    if (set(pins) != {'sources', 'witnesses', 'token_registry'} or set(pins['witnesses']) != WITNESSES
            or set(pins['token_registry']) != TOKENS
            or {pin['registry']['csvid'] for pin in pins['sources'].values()} != CLAUSES.keys()):
        raise ValueError('complete independent Delirium primary, witnesses and token identities required')
    for group in ('sources', 'witnesses'):
        for name, pin in pins[group].items():
            row = rows.get(name, {})
            if (not exact(oracle.get(name), pin['oracle']) or not exact(row, pin['registry'])
                    or row.get('kind') != 'card' or row.get('handler') in (None, '0', 'NULL')
                    or oracle[name].get('layout') != 'normal'):
                raise ValueError('exact Delirium primary or witness changed: ' + name)
            if group == 'sources':
                if row['handler'] == 'card_dummy':
                    raise ValueError('implemented Delirium primary required')
                classify(oracle[name], row['csvid'])
    for name, pin in pins['token_registry'].items():
        if not exact(rows.get(name), pin):
            raise ValueError('exact Delirium token registry identity changed: ' + name)
    assert len(THREE) == len(FOUR) == 4 and len(types_for(THREE, oracle)) == 3
    assert types_for(FOUR, oracle) == {'Artifact', 'Creature', 'Instant', 'Land'}
    assert types_for(THREE + ('Disenchant',), oracle) == types_for(THREE, oracle)


class Board:
    def __init__(self, record, csvid, actor, mode, rows, oracle):
        self.name, self.actor, self.foe, self.mode = record['name'], actor, 1 - actor, mode
        self.csvid, self.registry, self.oracle = csvid, rows, oracle
        self.active = mode in ('four_types', EXTRAS[csvid], 'expires', 'replay', *DENIED)
        self.setup, self.steps, self.cards, self.turn = [], [], {}, actor
        self.add('source', actor, self.name, 'hand')
        for seat, cards in enumerate(self.graves()):
            for i, name in enumerate(cards):
                self.add(f'g_{seat}_{i}', seat, name, 'graveyard')
        for seat in (0, 1):
            for i, name in enumerate(('Island', 'Plains')):
                self.add(f'd_{seat}_{i}', seat, name, 'library')
        self.witnesses()
        if mode == 'countered':
            self.add('counter', self.foe, 'Counterspell', 'hand')
        if mode == 'gain_fourth':
            self.add('threshold_amulet', actor, "Traveler's Amulet", 'battlefield')
            self.add('disenchant', actor, 'Disenchant', 'hand')

    def graves(self):
        result = [(), ()]
        result[self.actor] = FOUR if self.active else THREE
        if self.mode == 'opponent_only':
            result[self.foe] = FOUR
        return result

    def add(self, ref, player, name, zone):
        self.setup.append(card(name, ref, player, zone))
        self.cards[ref] = dict(player=player, name=name)

    def emit(self, op, ref='', other='', prop='', *, player=0, value=0, zone=0, index=0, color=0):
        if op in ('untap_step', 'upkeep_step'):
            self.steps.append(step(op, player=player))
            return
        fields = dict(player=player, value=value, index=index, color=color)
        if ref:
            fields['ref'] = ref
        if other:
            fields['other'] = other
        if prop:
            fields['property'] = prop
        if op in ('assert_zone', 'assert_zone_multiplicity'):
            fields['zone'] = ZONE_NAMES[zone]
        self.steps.append(step(op, **fields))

    def observe(self, ref, prop, value):
        self.emit('assert_card', ref, prop=prop, value=value)

    def mana(self, op, player, pool):
        for color, amount in enumerate(pool):
            self.emit(op, player=player, color=color, value=amount)

    def witnesses(self):
        if self.csvid == DESCEND:
            self.add('friendly', self.actor, 'Grizzly Bears', 'battlefield')
            self.add('enemy', self.foe, 'Wind Drake', 'battlefield')
            if self.mode not in ('short', 'wrong_color'):
                self.add('land', self.actor, 'Mountain', 'battlefield')
            self.add('artifact', self.foe, "Traveler's Amulet", 'battlefield')
            if self.mode == 'angel_flying':
                self.add('ground', self.foe, 'Grizzly Bears', 'hand')
        elif self.csvid == BLOB and self.mode == 'other_attacks':
            self.add('other', self.actor, 'Grizzly Bears', 'battlefield')
        elif self.csvid == SOUL:
            self.add('blocker', self.foe, 'Grizzly Bears', 'battlefield')
        elif self.csvid == SENTRY:
            name = 'Swords to Plowshares' if self.mode == 'exiled' else 'Shock'
            self.add('removal', self.actor, name, 'hand')
        elif self.csvid == ISHKANAH:
            self.add('own_spider', self.actor, 'Giant Spider', 'battlefield')
            self.add('enemy_spider', self.foe, 'Giant Spider', 'battlefield')
            self.add('friendly', self.actor, 'Grizzly Bears', 'battlefield')
            self.add('flyer', self.foe, 'Wind Drake', 'battlefield')
        elif self.csvid == MOURN:
            self.add('small', self.foe, 'Grizzly Bears', 'battlefield')
            self.add('large', self.foe, 'Hill Giant', 'battlefield')
            if self.mode == 'power_changes':
                self.add('pump', self.foe, 'Giant Growth', 'hand')
            if self.mode == 'countered':
                self.add('attacker', self.actor, 'Grizzly Bears', 'battlefield')

    def casts(self, ref, *, target=None, pending=False):
        player = self.cards[ref]['player']
        self.fund(player, self.oracle[self.cards[ref]['name']]['mana_cost'])
        if target:
            self.emit('choose_target', target, player=player)
        self.emit('cast', ref, value=1)
        self.emit('assert_choices_consumed')
        self.emit('assert_stack', value=1)
        self.mana('assert_mana', player, [0] * 6)
        if not pending:
            self.resolve()

    def fund(self, player, mana_cost):
        self.mana('set_mana', player, cost_from_oracle({'mana_cost': mana_cost}))
        self.emit('refresh')

    def resolve(self):
        self.emit('resolve')
        self.emit('refresh')
        self.emit('assert_choices_consumed')
        self.emit('assert_stack')

    def receipt(self, name, player, zone, expected):
        self.emit('assert_zone_multiplicity', player=player, zone=ZONE[zone],
                  index=self.registry[name]['csvid'], value=expected)

    def token(self, name, count, traits=(), *, player=None):
        player = self.actor if player is None else player
        self.receipt(name, player, 'battlefield', count)
        if count:
            for prop, expected in (('token', 1), *traits):
                self.emit('assert_matching_permanents', player=player,
                          index=self.registry[name]['csvid'], prop=prop, value=expected)

    def bind(self, name, ref):
        self.emit('bind_permanent_copy', ref, player=self.actor, index=0, value=self.registry[name]['csvid'])

    def life(self, player, value):
        self.emit('assert_life', player=player, value=value)

    def after_turn(self, *, upkeep=False):
        self.turn = 1 - self.turn
        self.emit('advance_turn')
        self.emit('untap_step', player=self.turn)
        if upkeep:
            self.emit('upkeep_step', player=self.turn)
            self.emit('assert_choices_consumed')
        self.emit('turn', player=self.turn)
        self.emit('refresh')

    def stats(self, power, toughness, ref='source'):
        self.observe(ref, 'power', power)
        self.observe(ref, 'toughness', toughness)

    def attack(self, ref):
        self.emit('begin_attackers')
        self.emit('attack', ref, value=1)
        self.observe(ref, 'attacking', 1)
        self.emit('finish_attackers')
        self.emit('assert_choices_consumed')
        self.emit('begin_blockers')

    def block(self, blocker, attacker, legal):
        self.emit('block', blocker, attacker, value=int(legal))

    def damage(self):
        self.emit('advance_combat')
        self.emit('combat_damage')
        self.emit('resolve')
        self.emit('end_combat')
        self.emit('refresh')
        self.emit('assert_choices_consumed')

    def descend(self):
        self.receipt('Grizzly Bears', self.actor, 'exile', 1)
        self.receipt('Wind Drake', self.foe, 'exile', 1)
        self.receipt('Grizzly Bears', self.actor, 'graveyard', 1)
        self.receipt('Wind Drake', self.foe, 'graveyard', 0)
        self.observe('land', 'in_play', 1)
        self.observe('artifact', 'in_play', 1)
        self.receipt(self.name, self.actor, 'graveyard', 1)
        self.token('Angel', int(self.active),
                   (('color', 32), ('subtype_Angel', 1), ('power', 4), ('toughness', 4),
                    ('tapped', 0), ('attacking', 0)))
        self.token('Angel', 0, player=self.foe)
        if self.mode == 'angel_flying':
            self.bind('Angel', 'angel')
            self.after_turn()
            self.casts('ground')
            self.after_turn()
            self.attack('angel')
            self.block('ground', 'angel', False)
            self.damage()
            self.life(self.foe, 16)

    def blob(self):
        self.stats(3, 3)
        self.token('Ooze', 0)
        self.after_turn()
        self.after_turn()
        ref = 'other' if self.mode == 'other_attacks' else 'source'
        self.attack(ref)
        count = int(self.active and ref == 'source')
        self.token('Ooze', count,
                   (('color', 8), ('subtype_Ooze', 1), ('power', 3), ('toughness', 3),
                    ('tapped', 1), ('attacking', 1)))
        self.token('Ooze', 0, player=self.foe)
        self.damage()
        self.life(self.foe, 20 - (2 if ref == 'other' else 3 + count * 3))
        self.token('Ooze', count, (('attacking', 0), ('tapped', 1)))

    def swallower(self):
        count = 0
        visits = 4 if self.mode == 'repeat_upkeep' else 2
        self.observe('source', 'plus1_counters', 0)
        for _ in range(visits):
            self.after_turn(upkeep=True)
            if self.turn == self.actor and self.active:
                count += 3
            self.observe('source', 'plus1_counters', count)
            self.stats(3 + count, 3 + count)
        self.attack('source')
        self.block('blocker', 'source', True)
        self.damage()
        self.life(self.foe, 20 - (1 + count))
        self.observe('source', 'damage', 2)
        self.observe('source', 'plus1_counters', count)

    def sentry(self):
        power = 4 if self.active else 1
        self.stats(power, 2)
        self.token('Eldrazi Horror', 0)
        self.casts('removal', target='source')
        exiled = self.mode == 'exiled'
        self.receipt(self.name, self.actor, 'exile', int(exiled))
        self.receipt(self.name, self.actor, 'graveyard', int(not exiled))
        self.life(self.actor, 20 + (power if exiled else 0))
        self.token('Eldrazi Horror', int(not exiled),
                   (('color', 0), ('subtype_Eldrazi', 1), ('subtype_Horror', 1),
                    ('power', 3), ('toughness', 2)))
        self.token('Eldrazi Horror', 0, player=self.foe)

    def ishkanah(self):
        self.stats(3, 5)
        tokens = 3 if self.active else 0
        self.token('Spider', tokens,
                   (('color', 8), ('subtype_Spider', 1), ('power', 1), ('toughness', 2), ('tapped', 0)))
        self.token('Spider', 0, player=self.foe)
        self.fund(self.actor, '{5}{B}')
        self.emit('can_activate', 'source', value=0)
        self.fund(self.actor, '{6}{B}')
        self.emit('can_activate', 'source', value=1)
        # The only legal target is set by GAA_CAN_ONLY_TARGET_OPPONENT.
        self.emit('activate', 'source', value=1)
        self.emit('assert_choices_consumed')
        self.emit('assert_stack', value=1)
        self.mana('assert_mana', self.actor, [0] * 6)
        self.life(self.foe, 20)
        self.resolve()
        self.life(self.actor, 20)
        self.life(self.foe, 20 - 2 - tokens)
        self.observe('source', 'tapped', 0)
        self.after_turn()
        self.attack('flyer')
        ref = 'source'
        if self.mode == 'spider_reach':
            ref = 'spider'
            self.bind('Spider', ref)
        self.block(ref, 'flyer', True)
        self.emit('advance_combat')
        self.observe(ref, 'blocking_count', 1)

    def mourn(self):
        self.stats(3, 2)
        self.observe('source', 'sick', 0)
        boosted = self.mode == 'power_changes'
        if boosted:
            self.casts('pump', target='small')
            self.stats(5, 5, 'small')
        self.attack('source')
        if self.mode == 'expires':
            self.block('small', 'source', False)
            self.damage()
            self.life(self.foe, 17)
            self.after_turn()
            self.after_turn()
            self.attack('source')
            self.block('small', 'source', True)
            blocker = 'small'
        elif self.active and not boosted:
            self.block('small', 'source', False)
            self.block('large', 'source', True)
            blocker = 'large'
        else:
            self.block('small', 'source', True)
            blocker = 'small'
        self.emit('advance_combat')
        self.observe(blocker, 'blocking_count', 1)



    def deny_entry(self):
        if self.mode == 'countered':
            self.casts('source', pending=True)
            self.emit('announce_spell', 'source')
            self.fund(self.foe, '{U}{U}')
            self.emit('cast_response', 'counter', value=1)
            self.emit('assert_stack', value=2)
            self.emit('resolve_response')
            self.emit('assert_choices_consumed')
            self.emit('assert_stack')
            self.mana('assert_mana', self.foe, [0] * 6)
            self.receipt('Counterspell', self.foe, 'graveyard', 1)
            self.receipt(self.name, self.actor, 'graveyard', 1)
        else:
            payment = cost_from_oracle(self.oracle[self.name])
            color = next(i for i in range(1, 6) if payment[i])
            if self.mode == 'short':
                payment[0 if payment[0] else color] -= 1
            else:
                substitute = next(i for i in range(1, 6) if not payment[i])
                payment[color] -= 1
                payment[substitute] += 1
            self.mana('set_mana', self.actor, payment)
            self.emit('refresh')
            self.emit('cast', 'source')
            self.emit('assert_choices_consumed')
            self.emit('assert_stack')
            self.mana('assert_mana', self.actor, payment)
            self.receipt(self.name, self.actor, 'hand', 1)
            self.receipt(self.name, self.actor, 'graveyard', 0)

        self.receipt(self.name, self.actor, 'battlefield', 0)
        for token in sorted(TOKENS):
            self.token(token, 0)
        self.life(self.actor, 20)
        self.life(self.foe, 20)
        if self.csvid == DESCEND:
            self.observe('friendly', 'in_play', 1)
            self.observe('enemy', 'in_play', 1)
            self.receipt('Grizzly Bears', self.actor, 'exile', 0)
            self.receipt('Wind Drake', self.foe, 'exile', 0)
        if self.csvid == MOURN and self.mode == 'countered':
            self.attack('attacker')
            self.block('small', 'attacker', True)
            self.emit('advance_combat')
            self.observe('small', 'blocking_count', 1)

    def gain_fourth(self):
        self.receipt("Traveler's Amulet", self.actor, 'graveyard', 0)
        self.casts('disenchant', target='threshold_amulet')
        self.receipt("Traveler's Amulet", self.actor, 'graveyard', 1)
        self.receipt('Disenchant', self.actor, 'graveyard', 1)
        self.active = True

    def play(self):
        cross = self.mode == 'gain_fourth'
        if cross and self.csvid in (DESCEND, ISHKANAH, MOURN):
            self.gain_fourth()
        self.casts('source')
        if self.csvid != DESCEND:
            self.observe('source', 'in_play', 1)
        if cross and self.csvid in (BLOB, SOUL, SENTRY):
            if self.csvid == SENTRY:
                self.stats(1, 2)
            self.gain_fourth()
        handlers = {DESCEND: self.descend, BLOB: self.blob, SOUL: self.swallower,
                    SENTRY: self.sentry, ISHKANAH: self.ishkanah, MOURN: self.mourn}
        handlers[self.csvid]()

    def build(self):
        self.emit('turn', player=self.actor)
        for player, cards in enumerate(self.graves()):
            self.mana('set_mana', player, [0] * 6)
            for name in sorted(set(cards)):
                self.receipt(name, player, 'graveyard', cards.count(name))
            self.emit('assert_zone', player=player, zone=ZONE['graveyard'], value=len(cards))
        if self.mode in DENIED:
            self.deny_entry()
            return
        cut = len(self.steps)
        self.play()
        if self.mode == 'replay':
            suffix = self.steps[cut:]
            self.steps[cut:] = ([step('capture')] + suffix + [step('record_state'), step('ai_roundtrip'), step('restore')]
                                + deepcopy(suffix) + [step('assert_recorded_state')])


def expand(name, csvid, declaration, shape, selected, rows, oracle):
    validate_variants(shape, selected)
    validate_witnesses(oracle, rows)
    if shape['csvid'] != csvid or profiles()['sources'][name]['registry']['csvid'] != csvid:
        raise ValueError('exact Delirium board primary CSVID required')
    cases = []
    for variant in selected:
        mode, player = variant.rsplit('_', 1)
        board = Board(oracle[name], csvid, int(player), mode, rows, oracle)
        board.build()
        cases.append(dict(
            name=f'DeliriumTokensAndBoardRecipe_{csvid}_{variant}', card=name, fixture='empty_main', seed=2370,
            setup=board.setup, steps=board.steps,
            _recipe=dict(family=FAMILY, version=2, variant=variant, params=declaration['params'],
                         oracle_text=declaration['oracle_text'],
                         untested_components=[GAP] if csvid in (BLOB, SOUL) else []),
        ))
    return cases
