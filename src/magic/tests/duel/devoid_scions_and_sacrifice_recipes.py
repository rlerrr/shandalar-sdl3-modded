"""Actual Scion creation and sacrifices, with complete printed Devoid riders."""

import copy
import json
from collections import Counter
from functools import lru_cache
from pathlib import Path

from devoid_case_support import DevoidBoard, devoid_rules

FAMILY = 'devoid_scions_and_sacrifice'
PROFILE = Path(__file__).with_name('data') / 'devoid_scions_and_sacrifice_wave8_profiles.json'
ONE = 'create a 1/1 colorless Eldrazi Scion creature token. It has "Sacrifice this token: Add {C}."'
TWO = 'Create two 1/1 colorless Eldrazi Scion creature tokens. They have "Sacrifice this token: Add {C}."'
SCRY = ('Whenever another creature you control dies, scry 1. '
        '(Look at the top card of your library. You may put that card on the bottom.)')
SHAPES = {
    'call': ('manalinkeh_card_call_the_scions', 'Sorcery', (), (TWO,)),
    'sifter': ('manalinkeh_card_catacomb_sifter', 'Creature', ('Scry',), ('When this creature enters, ' + ONE, SCRY)),
    'beyond': ('manalinkeh_card_from_beyond', 'Enchantment', (), (
        'At the beginning of your upkeep, ' + ONE,
        '{1}{G}, Sacrifice this enchantment: Search your library for an Eldrazi card, reveal it, put it into your hand, then shuffle.')),
    'birth': ('manalinkeh_card_grave_birthing', 'Instant', (), (
        'Target opponent exiles a card from their graveyard. You ' + ONE, 'Draw a card.')),
    'abomination': ('manalinkeh_card_smothering_abomination', 'Creature', ('Flying',), (
        'Flying', 'At the beginning of your upkeep, sacrifice a creature.', 'Whenever you sacrifice a creature, draw a card.')),
    'drone': ('manalinkeh_card_flayer_drone', 'Creature', ('First strike',), (
        'First strike', 'Whenever another colorless creature you control enters, target opponent loses 1 life.')),
    'tyrant': ('manalinkeh_card_barrage_tyrant', 'Creature', (), (
        "{2}{R}, Sacrifice another colorless creature: This creature deals damage equal to the sacrificed creature's power to any target.",)),
    'redeemer': ('manalinkeh_card_vile_redeemer', 'Creature', ('Flash',), (
        'Flash', 'When you cast this spell, you may pay {C}. If you do, create a 1/1 colorless Eldrazi Scion creature token '
        'for each nontoken creature that died under your control this turn. Those tokens have "Sacrifice this token: Add {C}."')),
}
SCENES = {
    'call': ('create', 'spend', 'countered', 'short', 'replay'),
    'sifter': ('entry', 'scry_mana', 'enemy_dies', 'own_dies', 'replay'),
    'beyond': ('opponent_upkeep', 'own_upkeep', 'repeated', 'tutor', 'kindred_tutor', 'replay'),
    'birth': ('opponent_choice', 'empty_grave', 'offturn', 'spend', 'replay'),
    'abomination': ('own_upkeep', 'opponent_upkeep', 'sacrifice_mana', 'sacrifice_nonmana', 'enemy_sacrifice', 'flying'),
    'drone': ('scion_pair', 'colored_entry', 'enemy_entry', 'first_strike', 'replay'),
    'tyrant': ('own_power', 'scion_power', 'colored_denied', 'creature_target', 'replay'),
    'redeemer': ('on_cast', 'no_colorless', 'decline', 'offturn', 'zero_deaths', 'replay'),
}
SCION = 'Eldrazi Scion'
CALL = 'Call the Scions'
ELDRAZI = "Kozilek's Pathfinder"
KINDRED = 'All Is Dust'
BEAR = 'Grizzly Bears'
KILL = 'Murder'
COUNTER = 'Counterspell'
HAWK = 'Suntail Hawk'
COLORLESS = 'Steel Wall'
FANATIC = 'Mogg Fanatic'
DECK = ('Forest', 'Island', 'Mountain', 'Plains')
WITNESSES = {SCION, CALL, ELDRAZI, KINDRED, BEAR, KILL, COUNTER, HAWK, COLORLESS, FANATIC, *DECK}
VANILLA = {BEAR, HAWK, COLORLESS}
GAPS = {
    'beyond': ('The tutor checks legal offered cards and real shuffle. The reveal window has no direct presentation receipt.',),
    'abomination': ('The mandatory-upkeep fixture uses the sole source as the eligible sacrifice. '
                    'Choosing among multiple differently valued eligible creatures is outside the fixture.',),
    'redeemer': ('The cost choice and actual cast-event tokens are observable. '
                 'There is no independent scripted priority inside the implementation’s inline callback.',),
}


@lru_cache(maxsize=1)
def profiles():
    return json.loads(PROFILE.read_text(encoding='utf8'))


def classify(record):
    from recipes import cost_from_oracle

    rules = tuple(devoid_rules(record))
    card_type = record['type_line'].split(' — ')[0]
    flag = next((key for key, (_, kind, keywords, clauses) in SHAPES.items()
                 if (kind, clauses) == (card_type, rules) and set(record['keywords']) == {'Devoid', *keywords}), None)
    if flag is None:
        raise ValueError('whole supported Scion or sacrifice rider required')
    return dict(role=flag, mana=cost_from_oracle(record))


def variants(params):
    return [f'{scene}_{owner}' for owner in (0, 1) for scene in SCENES[params['role']]]


def check_variants(params, selected):
    if (type(selected) is not list or not selected or any(type(v) is not str for v in selected)
            or len(set(selected)) != len(selected) or set(selected) - set(variants(params))):
        raise ValueError('distinct reviewed Scion variants required')


def spec(declaration, record):
    params = classify(record)
    if (declaration.get('family') != FAMILY or type(declaration.get('version')) is not int or declaration['version'] != 2
            or profiles()['sources'].get(record['name'], {}).get('oracle') != record
            or declaration.get('oracle_text') != record['oracle_text']
            or json.dumps(declaration.get('params'), sort_keys=True) != json.dumps(params, sort_keys=True)):
        raise ValueError('typed declaration must equal whole pinned Scion Oracle')
    check_variants(params, declaration.get('variants'))
    return dict(params, family=FAMILY)


def validate_witnesses(oracle, rows):
    data = profiles()
    skipped = {r['csvid'] for r in json.loads((PROFILE.parent / 'unimplemented.json').read_text(
        encoding='utf8'))['cases'].values()}
    if data.get('version') != 1 or not data.get('sources') or set(data.get('witnesses', ())) != WITNESSES:
        raise ValueError('exact reviewed Scion fixture profile required')
    for group in ('sources', 'witnesses'):
        for name, pin in data[group].items():
            row, record = rows.get(name, {}), oracle.get(name, {})
            if (pin != dict(oracle=record, registry=row) or row.get('kind') != 'card' or row['csvid'] in skipped
                    or record.get('layout') != ('token' if name == SCION else 'normal')
                    or (row['handler'] in (None, 'NULL', '0', 'card_dummy') and name not in VANILLA)):
                raise ValueError(f'changed exact Scion or sacrifice witness: {name}')
            if group == 'sources' and row['handler'] != SHAPES[classify(record)['role']][0]:
                raise ValueError(f'changed Scion production callback: {name}')
    scion = oracle[SCION]
    if (rows[SCION]['csvid'], scion['type_line'], scion['power'], scion['toughness'], scion['colors'], scion['oracle_text']) != (
            18724, 'Token Creature — Eldrazi Scion', '1', '1', [], 'Sacrifice this creature: Add {C}.'):
        raise ValueError('exact independently pinned colorless Scion token required')
    if 'Eldrazi' not in oracle[KINDRED]['type_line'] or not oracle[KINDRED]['type_line'].startswith('Kindred Sorcery'):
        raise ValueError('noncreature Eldrazi witness must retain printed Kindred subtype')


class ScionBoard(DevoidBoard):
    def __init__(self, name, params, scene, owner, rows, oracle):
        super().__init__(name, params, scene, owner, rows, oracle)
        self.name = name
        self.role = params['role']
        self.current = owner
        self.life = [20, 20]
        self.deck = [[], []]
        self.unordered = set()
        self.shuffles = [0, 0]
        self.created = [0, 0]

    def seed_library(self, player, cards):
        self.deck[player] = list(cards)
        for count, name in enumerate(cards):
            self.add(name, f'deck_{player}_{count}', player, 'library')

    def take_library(self, player, name):
        self.deck[player].remove(name)
        ref = next(ref for ref, card in self.cards.items() if card['player'] == player and card['name'] == name
                   and card['zone'] == 'library')
        self.cards[ref]['zone'] = 'hand'

    def draw(self, who):
        self.take_library(who, self.deck[who][0])

    def source_count(self, player, zone, name, value):
        self.emit('assert_zone_multiplicity', player=player, zone=zone, index=self.rows[name]['csvid'], value=value)

    def observe(self, ref, prop, value):
        self.emit('assert_card', ref=ref, property=prop, value=value)

    def receipts(self):
        self.emit('assert_unused_choices', value=0)
        self.emit('assert_stack', value=0)
        self.emit('refresh')
        self.observe_mana()
        universe = {card['name'] for card in self.cards.values()} | {SCION}
        for p in (0, 1):
            self.emit('assert_life', player=p, value=self.life[p])
            self.emit('assert_shuffle_count', player=p, value=self.shuffles[p])
            for zone in ('battlefield', 'hand', 'graveyard', 'exile', 'library'):
                contents = self.deck[p] if zone == 'library' else [card['name'] for card in self.cards.values()
                    if card['player'] == p and card['zone'] == zone]
                self.emit('assert_zone', player=p, zone=zone, value=len(contents))
                if zone == 'library' and p not in self.unordered:
                    for pos, name in enumerate(contents):
                        self.emit('assert_zone_card', player=p, zone=zone, index=pos, value=self.rows[name]['csvid'])
                else:
                    counts = Counter(contents)
                    for name in sorted(universe):
                        self.source_count(p, zone, name, counts[name])
            if self.tokens(p):
                for prop, val in (('token', 1), ('power', 1), ('toughness', 1), ('color', 0), ('artifact', 0), ('subtype_Scion', 1)):
                    self.emit('assert_matching_permanents', player=p, index=self.rows[SCION]['csvid'], property=prop, value=val)

    def tokens(self, player):
        return [card for card in self.cards.values()
                if card['name'] == SCION and card['player'] == player and card['zone'] == 'battlefield']

    def create_scions(self, player, count):
        for _ in range(count):
            ref = f'actual_scion_{player}_{self.created[player]}'
            self.created[player] += 1
            self.cards[ref] = dict(name=SCION, player=player, zone='battlefield')

    def sacrifice_scion(self, player, choose=None):
        token = self.tokens(player)[0]
        self.emit('bind_permanent_copy', ref='mana_scion', player=player, value=self.rows[SCION]['csvid'], index=0)
        if choose is not None:
            choose()
        self.emit('can_activate', ref='mana_scion', value=1)
        self.emit('activate', ref='mana_scion', value=1)
        self.observe('mana_scion', 'in_play', 0)
        token['zone'] = 'ceased'
        self.pools[player][0] += 1
        self.emit('assert_mana', player=player, color=0, value=self.pools[player][0])
        self.emit('resolve')

    def spell(self, ref, *, target=None, funding=None, allowed=True):
        self.cast(ref, target=target, funding=funding, allowed=allowed)
        if allowed and ref == 'source' and self.role in ('call', 'birth'):
            self.observe(ref, 'color', 0)

    def settle(self, ref, zone='graveyard'):
        self.emit('resolve')
        self.emit('refresh')
        self.cards[ref]['zone'] = zone

    def next_turn(self, upkeep=False, plan=None):
        if any(any(pool) for pool in self.pools):
            raise ValueError('real turn transition requires spent mana')
        self.emit('advance_turn')
        self.current = 1 - self.current
        self.emit('untap_step', player=self.current)
        if upkeep:
            if plan is not None:
                plan()
            self.emit('upkeep_step', player=self.current)
        self.emit('turn', player=self.current, value=0)

    def call_support(self, player):
        self.spell('caller')
        self.settle('caller')
        self.create_scions(player, 2)

    def remove_with_murder(self, ref, before_resolve=None):
        self.spell('removal', target=ref)
        if before_resolve is not None:
            before_resolve()
        self.settle('removal')
        self.cards[ref]['zone'] = 'graveyard'


def start(w):
    w.emit('turn', player=w.owner, value=0)
    for who in (0, 1):
        w.fund(who, [0] * 6)


def call(w):
    start(w)
    if w.mode == 'short':
        w.spell('source', funding=[1, 0, 0, 1, 0, 0], allowed=False)
        w.receipts()
        return None
    w.spell('source')
    cut = len(w.steps) if w.mode == 'replay' else None
    if w.mode == 'countered':
        w.emit('announce_spell', ref='source')
        w.cast('counter', response=True)
        w.emit('resolve_response')
        w.cards['counter']['zone'] = 'graveyard'
        w.cards['source']['zone'] = 'graveyard'
    else:
        w.settle('source')
        w.create_scions(w.owner, 2)
    w.receipts()
    if w.mode == 'spend':
        w.sacrifice_scion(w.owner)
        w.receipts()
        w.sacrifice_scion(w.owner)
        w.receipts()
    return cut


def sifter(w):
    w.play_source()
    w.create_scions(w.owner, 1)
    w.receipts()
    cut = len(w.steps) if w.mode == 'replay' else None
    if w.mode in ('scry_mana', 'replay', 'own_dies'):
        top = w.deck[w.owner][0]

        def scry_choice():
            if w.owner == 0:
                w.emit('choose_zone', player=0, value=0)
            else:
                # The actual AI scry offers nonlands when controlling fewer than four lands.
                w.emit('choose_zone_card', player=1, value=w.rows[top]['csvid'])

        if w.mode == 'own_dies':
            w.remove_with_murder('victim', scry_choice)
        else:
            w.sacrifice_scion(w.owner, scry_choice)
        w.deck[w.owner].append(w.deck[w.owner].pop(0))
        w.receipts()
    if w.mode == 'enemy_dies':
        w.remove_with_murder('victim')
        w.receipts()
    return cut


def beyond(w):
    w.play_source()
    w.receipts()
    w.next_turn(upkeep=True)
    w.receipts()
    if w.mode == 'opponent_upkeep':
        return None
    w.next_turn(upkeep=True)
    w.create_scions(w.owner, 1)
    w.receipts()
    if w.mode == 'repeated':
        w.next_turn(upkeep=True)
        w.receipts()
        w.next_turn(upkeep=True)
        w.create_scions(w.owner, 1)
        w.receipts()
    if w.mode not in ('tutor', 'kindred_tutor', 'replay'):
        return None
    cut = len(w.steps) if w.mode == 'replay' else None
    w.fund(w.owner, [0, 0, 0, 1, 0, 0])
    w.sacrifice_scion(w.owner)
    w.emit('refresh')
    w.emit('activate', ref='source', value=1)
    w.emit('assert_stack', value=1)
    w.cards['source']['zone'] = 'graveyard'
    w.pools[w.owner] = [0] * 6
    w.source_count(w.owner, 'graveyard', w.name, 1)
    w.source_count(w.owner, 'battlefield', SCION, 0)
    w.observe_mana()
    sought = KINDRED if w.mode == 'kindred_tutor' else ELDRAZI
    offered = list(w.deck[w.owner])
    w.emit('observe_search_filters', value=1)
    w.emit('choose_zone_card', player=w.owner, value=w.rows[sought]['csvid'])
    w.emit('resolve')
    w.emit('assert_offered_selection_required', value=0)
    for name in offered:
        w.emit('assert_offered_legal_card', index=w.rows[name]['csvid'], value=int(name in (ELDRAZI, KINDRED)))
    w.take_library(w.owner, sought)
    w.shuffles[w.owner] += 1
    w.unordered.add(w.owner)
    w.receipts()
    return cut


def birth(w):
    start(w)
    if w.mode == 'offturn':
        w.next_turn(upkeep=True)
    # The actual opponent-only generic cast assigns its sole legal target.
    w.spell('source')
    cut = len(w.steps) if w.mode == 'replay' else None
    if w.mode != 'empty_grave':
        chosen = w.cards['chosen_grave']
        w.emit('choose_zone_card', player=w.foe, value=w.rows[chosen['name']]['csvid'])
        chosen['zone'] = 'exile'
    w.settle('source')
    w.create_scions(w.owner, 1)
    w.draw(w.owner)
    w.receipts()
    if w.mode == 'spend':
        w.sacrifice_scion(w.owner)
        w.receipts()
    return cut


def combat(w, flying):
    # Advance the real age/untap gates. The rule-specific upkeep is exercised independently.
    w.next_turn()
    w.next_turn()
    w.emit('begin_attackers')
    w.emit('attack', ref='source', value=1)
    w.emit('finish_attackers')
    w.emit('begin_blockers')
    w.emit('block', ref='ground', other='source', value=int(not flying))
    if flying:
        w.emit('block', ref='air', other='source', value=1)
    w.emit('advance_combat')
    w.observe('source', 'blocked', 1)
    w.emit('combat_damage')
    w.emit('resolve')
    w.emit('end_combat')
    w.cards['air' if flying else 'ground']['zone'] = 'graveyard'
    w.observe('source', 'damage', int(flying))
    w.receipts()


def abomination(w):
    w.play_source()
    w.receipts()
    if w.mode == 'flying':
        combat(w, True)
    elif w.mode == 'sacrifice_nonmana':
        w.emit('choose_player', player=w.owner, value=w.foe)
        w.emit('activate', ref='fanatic', value=1)
        w.emit('assert_unused_choices', value=0)
        w.emit('assert_stack', value=1)
        w.cards['fanatic']['zone'] = 'graveyard'
        w.source_count(w.owner, 'graveyard', FANATIC, 1)
        w.emit('resolve')
        w.draw(w.owner)
        w.life[w.foe] -= 1
        w.receipts()
    elif w.mode in ('sacrifice_mana', 'enemy_sacrifice'):
        player = w.owner
        if w.mode == 'enemy_sacrifice':
            w.next_turn(upkeep=True)
            player = w.foe
        w.call_support(player)
        w.receipts()
        w.sacrifice_scion(player)
        if player == w.owner:
            w.draw(w.owner)
        w.receipts()
    else:
        w.next_turn(upkeep=True)
        w.receipts()
        if w.mode == 'own_upkeep':
            plan = (lambda: w.emit('choose_target', ref='source', player=0)) if w.owner == 0 else None
            w.next_turn(upkeep=True, plan=plan)
            w.cards['source']['zone'] = 'graveyard'
            w.draw(w.owner)
            w.receipts()
    return None


def drone(w):
    w.play_source()
    w.receipts()
    if w.mode == 'first_strike':
        combat(w, False)
        return None
    cut = len(w.steps) if w.mode == 'replay' else None
    if w.mode == 'colored_entry':
        w.spell('colorful')
        w.settle('colorful', 'battlefield')
    else:
        player = w.owner
        if w.mode == 'enemy_entry':
            w.next_turn(upkeep=True)
            player = w.foe
        w.call_support(player)
        if player == w.owner:
            w.life[w.foe] -= 2
    w.receipts()
    return cut


def tyrant(w):
    w.play_source()
    w.receipts()
    sacrifice = 'meal'
    power = int(w.oracle[ELDRAZI]['power'])
    if w.mode == 'scion_power':
        w.call_support(w.owner)
        w.emit('bind_permanent_copy', ref='meal', player=w.owner, index=0, value=w.rows[SCION]['csvid'])
        power = 1
    w.fund(w.owner, [2, 0, 0, 0, 1, 0])
    legal = w.mode != 'colored_denied'
    w.emit('can_activate', ref='source', value=int(legal))
    if not legal:
        w.emit('activate', ref='source', value=0)
        w.receipts()
        return None
    if w.owner == 0:
        w.emit('choose_target', ref=sacrifice, player=w.owner)
    if w.mode == 'creature_target':
        w.emit('choose_target', ref='victim', player=w.owner)
    else:
        w.emit('choose_player', player=w.owner, value=w.foe)
    w.emit('activate', ref='source', value=1)
    w.emit('assert_unused_choices', value=0)
    w.emit('assert_stack', value=1)
    w.pools[w.owner] = [0] * 6
    w.observe_mana()
    if w.mode == 'scion_power':
        w.tokens(w.owner)[0]['zone'] = 'ceased'
        w.source_count(w.owner, 'battlefield', SCION, 1)
    else:
        w.cards[sacrifice]['zone'] = 'graveyard'
        w.source_count(w.owner, 'graveyard', ELDRAZI, 1)
    w.emit('assert_life', player=w.foe, value=20)
    cut = len(w.steps) if w.mode == 'replay' else None
    w.emit('resolve')
    if w.mode == 'creature_target':
        w.cards['victim']['zone'] = 'graveyard'
    else:
        w.life[w.foe] -= power
    w.receipts()
    return cut


def redeemer(w):
    start(w)
    if w.mode == 'offturn':
        w.next_turn(upkeep=True)
    w.emit('reset_death_history')
    died = int(w.mode != 'zero_deaths')
    if died:
        w.remove_with_murder('victim')
    w.receipts()
    cut = len(w.steps) if w.mode == 'replay' else None
    has_colorless = w.mode != 'no_colorless'
    takes = has_colorless and w.mode != 'decline'
    w.fund(w.owner, [2 + int(has_colorless), 0, 0, 1, 0, 0])
    if has_colorless:
        # The actual inline EVENT_CAST_SPELL callback opens the cost menu
        # as part of cast, before the separately scriptable announcement.
        w.emit('choose_dialog', player=w.owner, value=int(not takes))
    w.emit('cast', ref='source', value=1)
    w.emit('assert_unused_choices', value=0)
    w.cards['source']['zone'] = 'stack'
    w.pools[w.owner] = [int(has_colorless and not takes), 0, 0, 0, 0, 0]
    if takes:
        w.create_scions(w.owner, died)
    w.emit('assert_stack', value=1)
    w.observe('source', 'color', 0)
    w.source_count(w.owner, 'battlefield', SCION, died if takes else 0)
    w.observe_mana()
    w.settle('source', 'battlefield')
    w.emit('assert_matching_permanents', player=w.owner, index=w.rows[w.name]['csvid'], property='color', value=0)
    w.receipts()
    return cut


RUN = {'call': call, 'sifter': sifter, 'beyond': beyond, 'birth': birth,
       'abomination': abomination, 'drone': drone, 'tyrant': tyrant, 'redeemer': redeemer}


def fixtures(w):
    role, mode, owner, foe = w.role, w.mode, w.owner, w.foe
    library = (BEAR, *DECK[:3]) if role == 'sifter' else (ELDRAZI, KINDRED, *DECK[:2]) if role == 'beyond' else DECK
    w.seed_library(owner, library)
    w.seed_library(foe, ('Plains', 'Mountain', 'Island'))
    if role == 'call' and mode == 'countered':
        w.add(COUNTER, 'counter', foe, 'hand')
    if role == 'sifter' and mode in ('enemy_dies', 'own_dies') or role == 'redeemer' and mode != 'zero_deaths':
        w.add(KILL, 'removal', owner, 'hand')
        w.add(BEAR, 'victim', foe if mode == 'enemy_dies' else owner)
    if role == 'abomination' and mode == 'sacrifice_nonmana':
        w.add(FANATIC, 'fanatic', owner)
    if role == 'birth' and mode != 'empty_grave':
        w.add('Mountain', 'chosen_grave', foe, 'graveyard')
        w.add(BEAR, 'other_grave', foe, 'graveyard')
        w.add('Forest', 'own_grave', owner, 'graveyard')
    if mode in ('flying', 'first_strike'):
        w.add(BEAR, 'ground', foe)
        if mode == 'flying':
            w.add(HAWK, 'air', foe)
    if role in ('abomination', 'drone') and mode in ('sacrifice_mana', 'enemy_sacrifice', 'scion_pair', 'enemy_entry', 'replay'):
        w.add(CALL, 'caller', foe if mode in ('enemy_sacrifice', 'enemy_entry') else owner, 'hand')
    if role == 'drone' and mode == 'colored_entry':
        w.add(BEAR, 'colorful', owner, 'hand')
    if role == 'tyrant':
        if mode == 'scion_power':
            w.add(CALL, 'caller', owner, 'hand')
        else:
            w.add(BEAR if mode == 'colored_denied' else ELDRAZI, 'meal', owner)
        if mode == 'creature_target':
            w.add(BEAR, 'victim', foe)


def expand(name, csvid, declaration, params, selected, rows, oracle):
    from recipes import step

    check_variants(params, selected)
    validate_witnesses(oracle, rows)
    if profiles()['sources'][name]['registry']['csvid'] != csvid:
        raise ValueError('exact Devoid Scion source CSVID required')
    cases = []
    for variant in selected:
        scene, seat = variant.rsplit('_', 1)
        world = ScionBoard(name, params, scene, int(seat), rows, oracle)
        fixtures(world)
        cut = RUN[params['role']](world)
        steps = world.steps
        if cut is not None:
            suffix = steps[cut:]
            steps = (steps[:cut] + [step('capture')] + suffix + [step('record_state'), step('ai_roundtrip'), step('restore')]
                     + copy.deepcopy(suffix) + [step('assert_recorded_state')])
        cases.append(dict(name=f'DevoidScionsAndSacrificeRecipe_{csvid}_{variant}', card=name,
                          fixture='empty_main', seed=2172, setup=world.setup, steps=steps,
                          _recipe=dict(family=FAMILY, version=2, variant=variant, params=declaration['params'],
                              oracle_text=declaration['oracle_text'], untested_components=list(GAPS.get(params['role'], ())))))
    return cases
