"""Whole Morph creatures creating tokens through native entry, turn-up or death."""
import json
import re
from collections import Counter
from pathlib import Path

from library_search_recipes import wave6_exact as exact_pin
from morph_creature_recipes import COLORS, FACE_DOWN


FAMILY = 'morph_turn_up_tokens'
DATA = Path(__file__).parent / 'data/morph_turn_up_tokens_wave8_profiles.json'
MORPH = re.compile(r'Morph ((?:\{(?:[0-9]+|[XBUGRW])\})+)(?: \(You may cast this card face down as a 2/2 creature for \{3\}\. Turn it face up any time for its morph cost\.\))?')
CREATE = re.compile(r'When this creature (is turned face up|enters or is turned face up), create (three|four|X) 1/1 (green|red) (Saproling|Goblin) creature tokens\.')
TOKEN_NAMES = {'Saproling', 'Goblin', 'Snake'}
WITNESSES = {'Forest', 'Mountain', 'Counterspell', 'Unsummon', 'Terminate', 'Hunger of the Howlpack', 'Sylvan Advocate', *TOKEN_NAMES}
ANTHEM = 'All Saprolings get +1/+1.'
HYDRA = (
    'This creature enters with X +1/+1 counters on it.',
    'When this creature dies, create a 1/1 green Snake creature token for each +1/+1 counter on it.',
    'As this creature is turned face up, put five +1/+1 counters on it.',
)
COMMON = ('hidden', 'faceup', 'cast_short', 'countered', 'flip', 'offturn', 'flip_wrong_color', 'repeat', 'replay')


def pins():
    return json.loads(DATA.read_text(encoding='utf8'))


def mechanism(record):
    from recipes import cost_from_oracle

    if (not record or record.get('layout') != 'normal' or record.get('selected_face') or record.get('card_faces')
            or not record.get('type_line', '').startswith('Creature — ') or record.get('keywords') != ['Morph']
            or any(not re.fullmatch(r'[0-9]+', record.get(key, '')) for key in ('power', 'toughness'))
            or any(color not in COLORS[1:] for color in record['colors'])):
        raise ValueError('whole normal Morph creature required')
    clauses = record['oracle_text'].splitlines()
    morphs = [match for line in clauses if (match := MORPH.fullmatch(line))]
    if len(morphs) != 1:
        raise ValueError('one printed Morph fee required')
    morph = morphs[0][1]
    effects = tuple(line for line in clauses if not MORPH.fullmatch(line))
    printed = record['mana_cost']
    if effects == HYDRA and printed == '{X}{G}{G}' and morph == '{3}{G}{G}':
        kind, token, color, quantity, entry, anthem = 'counter_death', 'Snake', 'G', 0, False, False
        cast_cost = '{G}{G}'
    else:
        triggers = [match for line in effects if (match := CREATE.fullmatch(line))]
        if len(triggers) != 1 or set(effects) - {triggers[0][0], ANTHEM} or len(set(effects)) != len(effects):
            raise ValueError('unknown printed Morph token or static clause')
        trigger, count, tone, token = triggers[0].groups()
        entry = trigger.startswith('enters')
        anthem = ANTHEM in effects
        kind = 'double_x' if count == 'X' else 'fixed'
        quantity = {'three': 3, 'four': 4, 'X': 0}[count]
        color = {'green': 'G', 'red': 'R'}[tone]
        if ((kind == 'double_x') != (morph == '{X}{X}{R}') or kind == 'double_x' and (entry or anthem)
                or anthem and token != 'Saproling' or 'X' in printed):
            raise ValueError('exact paired morph and token clauses required')
        cast_cost = printed
    return dict(kind=kind, printed=cost_from_oracle(dict(record, mana_cost=cast_cost)),
                morph=cost_from_oracle(dict(record, mana_cost='{R}' if kind == 'double_x' else morph)),
                token=token, token_color=1 << COLORS.index(color), quantity=quantity,
                entry=entry, anthem=anthem, clauses=clauses, power=int(record['power']),
                toughness=int(record['toughness']), color=sum(1 << COLORS.index(c) for c in record['colors']),
                subtypes=record['type_line'].split(' — ', 1)[1].split())


def variants(shape):
    modes = list(COMMON)
    if shape['kind'] != 'double_x':
        modes += ['flip_short']
    if shape['anthem']:
        modes += ['anthem_leaves']
    if shape['entry']:
        modes += ['reenter']
    if shape['kind'] == 'double_x':
        modes += ['x_zero', 'x_one', 'x_surplus']
    if shape['kind'] == 'counter_death':
        modes += ['faceup_zero', 'hardcast_death', 'hidden_death', 'flip_death', 'grown_flip_death', 'flip_bounce']
    return [f'{mode}_{owner}' for mode in modes for owner in (0, 1)]


def spec(declaration, record):
    shape = mechanism(record)
    source = pins()['sources'].get(record['name'])
    params = declaration.get('params')
    if (not source or not exact_pin(source['oracle'], record) or declaration.get('family') != FAMILY
            or type(declaration.get('version')) is not int or declaration['version'] != 2
            or declaration.get('oracle_text') != record['oracle_text'] or not isinstance(params, dict)
            or set(params) != {'schema', 'mechanism'} or type(params['schema']) is not int or params['schema'] != 1
            or not exact_pin(params['mechanism'], shape)):
        raise ValueError('full pinned Morph declaration required')
    return dict(shape, csvid=source['csvid'], family=FAMILY)


def validate_witnesses(oracle, rows):
    data = pins()
    if set(data) != {'sources', 'witnesses', 'hidden_observer'} or set(data['witnesses']) != WITNESSES:
        raise ValueError('complete source and independent token witnesses required')
    for group in ('sources', 'witnesses'):
        for name, pin in data[group].items():
            row = rows.get(name, {})
            if (not exact_pin(pin['oracle'], oracle.get(name)) or row.get('csvid') != pin['csvid']
                    or row.get('handler') != pin['handler'] or row.get('handler') in (None, 'NULL', '0', 'card_dummy')
                    or (name not in TOKEN_NAMES and row.get('kind') != 'card')
                    or (name not in TOKEN_NAMES and oracle[name].get('layout') != 'normal')):
                raise ValueError('pinned Morph source or witness changed: ' + name)
            if group == 'sources':
                mechanism(oracle[name])
    expected = {'name': 'Face-Down Creature', 'csvid': FACE_DOWN, 'handler': 'manalinkeh_card_face_down_creature'}
    hidden = rows.get(expected['name'], {})
    if data['hidden_observer'] != expected or any(hidden.get(k) != v for k, v in expected.items()):
        raise ValueError('native face-down identity changed')


class Game:
    def __init__(self, name, shape, mode, owner, rows, oracle):
        self.name, self.shape, self.mode = name, shape, mode
        self.owner, self.foe = owner, 1 - owner
        self.rows, self.oracle = rows, oracle
        self.setup, self.steps, self.zones = [], [], {}
        self.pools = [[0] * 6 for _ in (0, 1)]
        self.tokens = [0, 0]
        self.source, self.hidden, self.counters = 'source', False, 0
        self.csvid = rows[name]['csvid']
        self.hydra = shape['kind'] == 'counter_death'
        self.dynamic = shape['kind'] == 'double_x'
        self.add(name, 'source', owner, 'hand')
        if mode == 'countered':
            self.add('Counterspell', 'counterspell', self.foe, 'hand')
        if mode in ('reenter', 'flip_bounce'):
            self.add('Unsummon', 'bounce', owner, 'hand')
        if mode in ('anthem_leaves', 'hardcast_death', 'hidden_death', 'flip_death', 'grown_flip_death'):
            self.add('Terminate', 'removal', self.foe, 'hand')
        if mode == 'grown_flip_death':
            self.add('Hunger of the Howlpack', 'grow', owner, 'hand')
        if shape['anthem']:
            for player in (0, 1):
                self.add('Saproling', f'old_saproling_{player}', player, 'battlefield', state=['untapped', 'token'])
            self.add('Sylvan Advocate', 'non_saproling', self.foe, 'battlefield')
        for player in (0, 1):
            for index, land in enumerate(('Forest', 'Mountain')):
                self.add(land, f'library_{player}_{index}', player, 'library')

    def emit(self, op, **values):
        from recipes import step

        self.steps.append(step(op, **values))

    def add(self, name, ref, player, zone, **extra):
        from recipes import card

        self.setup.append(dict(card(name, ref, player, zone), **extra))
        self.zones[ref] = zone

    def fund(self, player, budget):
        self.pools[player] = list(budget)
        for color, amount in enumerate(budget):
            self.emit('set_mana', player=player, color=color, value=amount)
        self.emit('refresh')

    def pay(self, player, cost):
        self.pools[player] = [available - paid for available, paid in zip(self.pools[player], cost)]
        if min(self.pools[player]) < 0:
            raise ValueError('legal play exceeds its actual mana budget')

    def observed_pools(self):
        for player, pool in enumerate(self.pools):
            for color, amount in enumerate(pool):
                self.emit('assert_mana', player=player, color=color, value=amount)

    def observe_source(self):
        from generate_cases import TOKEN_PROPERTIES

        visible = FACE_DOWN if self.hidden else self.csvid
        shape = self.shape
        traits = {'color': 0 if self.hidden else shape['color'], 'token': 0, 'artifact': 0}
        traits.update({f'subtype_{s}': int(not self.hidden) for s in shape['subtypes'] if f'subtype_{s}' in TOKEN_PROPERTIES})
        for prop, value in traits.items():
            self.emit('assert_matching_permanents', player=self.owner, index=visible, property=prop, value=value)
        for prop, value in [('csvid', visible), ('power', (2 if self.hidden else shape['power']) + self.counters),
                            ('toughness', (2 if self.hidden else shape['toughness']) + self.counters), ('tapped', 0),
                            ('in_play', 1), ('plus1_counters', self.counters)]:
            self.emit('assert_card', ref=self.source, property=prop, value=value)

    def observe(self):
        self.emit('assert_choices_consumed')
        self.emit('assert_stack', value=0)
        self.observed_pools()
        living_source = self.zones['source'] == 'battlefield'
        bonus = int(self.shape['anthem'] and living_source and not self.hidden)
        token_id = self.rows[self.shape['token']]['csvid']
        for player in (0, 1):
            expected = Counter()
            for row in self.setup:
                ref, name = row['ref'], row['name']
                if row['player'] != player:
                    continue
                zone = self.zones[ref]
                visible = FACE_DOWN if ref == 'source' and zone == 'battlefield' and self.hidden else self.rows[name]['csvid']
                expected[zone, visible] += 1
            expected['battlefield', token_id] += self.tokens[player]
            self.emit('assert_life', player=player, value=20)
            for zone in ('hand', 'battlefield', 'graveyard', 'exile', 'library'):
                self.emit('assert_zone', player=player, zone=zone,
                          value=sum(count for (where, _), count in expected.items() if where == zone))
            for (zone, visible), count in sorted(expected.items()):
                self.emit('assert_zone_multiplicity', player=player, zone=zone, index=visible, value=count)
            quantity = expected['battlefield', token_id]
            if quantity:
                traits = {'color': self.shape['token_color'], 'artifact': 0, 'token': 1,
                          f"subtype_{self.shape['token']}": 1}
                for prop, value in traits.items():
                    self.emit('assert_matching_permanents', player=player, index=token_id, property=prop, value=value)
                for index in range(quantity):
                    key = f'token_{player}_{index}'
                    self.emit('bind_permanent_copy', player=player, index=index, value=token_id, ref=key)
                    for prop, value in [('power', 1 + bonus), ('toughness', 1 + bonus), ('tapped', 0), ('plus1_counters', 0)]:
                        self.emit('assert_card', ref=key, property=prop, value=value)
        if living_source:
            self.observe_source()
        if self.shape['anthem']:
            for prop in ('power', 'toughness'):
                self.emit('assert_card', ref='non_saproling', property=prop, value=int(self.oracle['Sylvan Advocate'][prop]))

    def announce(self, hidden=False, *, x=0, reject=False):
        price = [3, 0, 0, 0, 0, 0] if hidden else list(self.shape['printed'])
        if self.hydra and not hidden:
            price[0] = x
        budget = [2, 0, 0, 0, 0, 0] if reject else price
        self.fund(self.owner, budget)
        if not reject and not hidden:
            if sum(price) >= 3:
                self.emit('choose_dialog', player=self.owner, value=1)
            if self.hydra:
                self.emit('choose_x', player=self.owner, value=x)
        self.emit('cast', ref=self.source, value=int(not reject))
        self.emit('assert_choices_consumed')
        self.emit('assert_stack', value=int(not reject))
        if not reject:
            self.pay(self.owner, price)
            self.zones['source'] = 'stack'
            self.hidden = hidden
            self.counters = x
            self.emit('assert_card', ref=self.source, property='csvid', value=FACE_DOWN if hidden else self.csvid)
        self.observed_pools()

    def resolve_cast(self):
        self.emit('resolve')
        self.emit('refresh')
        self.zones['source'] = 'battlefield'
        if not self.hidden:
            if self.shape['entry']:
                self.tokens[self.owner] += self.shape['quantity']
            if self.hydra and self.counters == 0:
                self.zones['source'] = 'graveyard'
        self.observe()

    def flip(self, *, x=0, spare=0, wrong=False, short=False):
        cost = list(self.shape['morph'])
        if self.dynamic:
            cost[0] = 2 * x
        budget = cost.copy()
        budget[0] += spare
        if short:
            budget[0] -= 1
        if wrong:
            colored = next(index for index in range(1, 6) if cost[index])
            alt = next(index for index in range(1, 6) if not cost[index])
            budget[alt], budget[colored] = budget[colored], 0
        self.fund(self.owner, budget)
        allowed = not wrong and not short
        self.emit('can_activate', ref=self.source, value=int(allowed))
        if allowed and self.dynamic:
            self.emit('choose_x', player=self.owner, value=x)
        self.emit('activate', ref=self.source, value=int(allowed))
        if allowed:
            self.pay(self.owner, cost)
            self.hidden = False
            if self.hydra:
                self.counters += 5
            else:
                self.tokens[self.owner] += x if self.dynamic else self.shape['quantity']
            self.emit('resolve')
            self.emit('refresh')
        self.observe()

    def cast_witness(self, ref, *, response=False, target=True):
        from recipes import cost_from_oracle

        card = next(card for card in self.setup if card['ref'] == ref)
        player = card['player']
        cost = cost_from_oracle(self.oracle[card['name']])
        self.fund(player, cost)
        if target:
            self.emit('choose_target', player=player, ref=self.source)
        self.emit('cast_response' if response else 'cast', ref=ref, value=1)
        self.pay(player, cost)
        self.emit('resolve_response' if response else 'resolve')
        self.emit('refresh')
        self.zones[ref] = 'graveyard'


def _case(name, csvid, declaration, shape, variant, rows, oracle):
    from recipes import step

    mode, seat = variant.rsplit('_', 1)
    owner = int(seat)
    game = Game(name, shape, mode, owner, rows, oracle)
    emit = game.emit
    emit('turn', player=owner, value=0)
    for player in (0, 1):
        game.fund(player, [0] * 6)
    normal_cast = mode in ('faceup', 'faceup_zero', 'hardcast_death', 'reenter')
    reject = mode == 'cast_short'
    x = 2 if game.hydra and normal_cast and mode != 'faceup_zero' else 0
    game.announce(hidden=not normal_cast, x=x, reject=reject)
    if mode == 'countered':
        emit('announce_spell', ref='source')
        game.cast_witness('counterspell', response=True, target=False)
        game.zones['source'] = 'graveyard'
        game.observe()
    elif reject:
        game.observe()
    else:
        game.resolve_cast()

    if mode == 'offturn':
        emit('advance_turn')
    if mode == 'grown_flip_death':
        game.cast_witness('grow')
        game.counters = 1
        game.observe()

    replay_from = len(game.steps) if mode == 'replay' else None
    turn_up = {'flip', 'offturn', 'flip_wrong_color', 'flip_short', 'repeat', 'replay', 'anthem_leaves',
               'x_zero', 'x_one', 'x_surplus', 'flip_death', 'grown_flip_death', 'flip_bounce'}
    if mode in turn_up:
        chosen = {'x_zero': 0, 'x_one': 1, 'x_surplus': 2}.get(mode, 3)
        game.flip(x=chosen if game.dynamic else 0, spare=int(mode == 'x_surplus'),
                  wrong=mode == 'flip_wrong_color', short=mode == 'flip_short')
    if mode == 'repeat':
        emit('can_activate', ref='source', value=0)
        emit('activate', ref='source', value=0)
        game.observe()
    if mode in ('anthem_leaves', 'hardcast_death', 'hidden_death', 'flip_death', 'grown_flip_death'):
        game.cast_witness('removal')
        game.zones['source'] = 'graveyard'
        if game.hydra and not game.hidden:
            game.tokens[owner] += game.counters
        game.observe()
    if mode in ('reenter', 'flip_bounce'):
        game.cast_witness('bounce')
        game.zones['source'] = 'hand'
        game.observe()
        if mode == 'reenter':
            game.source = 'returned'
            emit('bind_hand_copy', ref=game.source, player=owner, index=0, value=csvid)
            game.announce()
            game.resolve_cast()

    if replay_from is not None:
        suffix = game.steps[replay_from:]
        game.steps = (game.steps[:replay_from] + [step('capture')] + suffix
                      + [step('record_state'), step('restore'), step('ai_roundtrip')] + suffix + [step('assert_recorded_state')])
    metadata = dict(family=FAMILY, version=2, variant=variant, params=declaration['params'],
                    oracle_text=declaration['oracle_text'], covered_clauses=shape['clauses'], untested_components=[
                        'UI hidden-information and priority timing within the native turn-up special action.',
                        'Token copies, replacement effects, visibility into offered X beyond legal typed input, and multiplayer.'])
    return dict(name=f'MorphTurnUpTokensRecipe_{csvid}_{variant}', card=name, fixture='empty_main', seed=123,
                setup=game.setup, steps=game.steps, _recipe=metadata)


def expand(name, csvid, declaration, shape, selected, rows, oracle):
    if csvid != shape['csvid'] or not selected or len(set(selected)) != len(selected) or set(selected) - set(variants(shape)):
        raise ValueError('distinct supported Morph token variants required')
    validate_witnesses(oracle, rows)
    return [_case(name, csvid, declaration, shape, variant, rows, oracle) for variant in selected]
