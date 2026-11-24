"""Whole Megamorph creatures through native payment, durable counters and combat."""
import json
import re
from pathlib import Path

from library_search_recipes import wave6_exact as exact_pin
from morph_creature_recipes import COLORS, FACE_DOWN


FAMILY = 'megamorph_keyword_creatures'
DATA = Path(__file__).parent / 'data/megamorph_keyword_creatures_wave8_profiles.json'
MEGAMORPH = re.compile(r'Megamorph ((?:\{(?:[0-9]+|[BUGRW])\})+) \(You may cast this card face down as a 2/2 creature for \{3\}\. Turn it face up any time for its megamorph cost and put a \+1/\+1 counter on it\.\)')
ABILITY_LINES = {
    'Reach (This creature can block creatures with flying.)': ('Reach',),
    'Flying': ('Flying',),
    'Double strike (This creature deals both first-strike and regular combat damage.)': ('Double strike',),
    'Defender': ('Defender',),
    "This creature can't be blocked.": ('Unblockable',),
    'Flying, vigilance': ('Flying', 'Vigilance'),
    'Trample': ('Trample',),
}
WITNESSES = {'Forest', 'Island', 'Unsummon', 'Counterspell', 'Hunger of the Howlpack',
             'Elvish Mystic', 'Welkin Tern', 'Nessian Asp'}
COMMON = ('hidden', 'ordinary', 'choose_hidden', 'cast_short', 'countered', 'flip', 'wrong_color', 'offturn',
          'counter_persists', 'already_up', 'grow_and_flip', 'bounce_reset', 'replay')


def pins():
    return json.loads(DATA.read_text(encoding='utf8'))


def mechanism(record):
    from recipes import cost_from_oracle

    if (not record or record.get('layout') != 'normal' or record.get('selected_face') or record.get('card_faces')
            or not record.get('type_line', '').startswith('Creature — ')
            or any(not re.fullmatch(r'[0-9]+', record.get(stat, '')) for stat in ('power', 'toughness'))
            or any(color not in COLORS[1:] for color in record['colors'])):
        raise ValueError('whole normal Megamorph creature required')
    clauses = record['oracle_text'].splitlines()
    match = MEGAMORPH.fullmatch(clauses[-1]) if clauses else None
    if not match or any(line not in ABILITY_LINES for line in clauses[:-1]):
        raise ValueError('unrecognized printed ability or complete Megamorph fee')
    traits = [trait for line in clauses[:-1] for trait in ABILITY_LINES[line]]
    if (len(traits) != len(set(traits)) or sorted([t for t in traits if t != 'Unblockable'] + ['Megamorph']) != sorted(record['keywords'])
            or tuple(traits) not in ((), ('Reach',), ('Flying',), ('Flying', 'Double strike'), ('Defender',),
                                     ('Unblockable',), ('Flying', 'Vigilance'), ('Trample',))):
        raise ValueError('unknown combination or inaccurate keyword metadata')
    return dict(printed=cost_from_oracle(record), fee=cost_from_oracle(dict(record, mana_cost=match[1])),
                traits=traits, power=int(record['power']), toughness=int(record['toughness']), clauses=clauses,
                color=sum(1 << COLORS.index(color) for color in record['colors']),
                subtypes=record['type_line'].split(' — ', 1)[1].split())


def variants(shape):
    modes = list(COMMON)
    if shape['fee'][0]:
        modes.append('fee_short')
    traits = shape['traits']
    if traits:
        modes += ['keyword_hidden', 'keyword_ordinary', 'keyword_flipped']
    if 'Flying' in traits or 'Unblockable' in traits:
        modes.append('keyword_reach')
    if 'Defender' in traits:
        modes.append('keyword_defender_blocks')
    return [f'{mode}_{seat}' for mode in modes for seat in (0, 1)]


def spec(declaration, record):
    shape = mechanism(record)
    source = pins()['sources'].get(record['name'])
    params = declaration.get('params')
    if (not source or not exact_pin(source['oracle'], record) or declaration.get('family') != FAMILY
            or type(declaration.get('version')) is not int or declaration['version'] != 2
            or declaration.get('oracle_text') != record['oracle_text'] or not isinstance(params, dict)
            or set(params) != {'schema', 'mechanism'} or type(params['schema']) is not int or params['schema'] != 1
            or not exact_pin(params['mechanism'], shape)):
        raise ValueError('full pinned Megamorph fees and printed abilities required')
    return dict(shape, csvid=source['csvid'], family=FAMILY)


def validate_witnesses(oracle, rows):
    data = pins()
    if set(data) != {'sources', 'witnesses', 'hidden_observer'} or set(data['witnesses']) != WITNESSES:
        raise ValueError('complete independent Megamorph witness pool required')
    for group in ('sources', 'witnesses'):
        for name, pin in data[group].items():
            row = rows.get(name, {})
            if (not exact_pin(pin['oracle'], oracle.get(name)) or row.get('csvid') != pin['csvid']
                    or row.get('handler') != pin['handler'] or row.get('kind') != 'card'
                    or row.get('handler') in (None, 'NULL', '0', 'card_dummy') or oracle[name].get('layout') != 'normal'):
                raise ValueError('pinned implemented source or witness changed: ' + name)
            if group == 'sources':
                mechanism(oracle[name])
    hidden = {'name': 'Face-Down Creature', 'csvid': FACE_DOWN, 'handler': 'manalinkeh_card_face_down_creature'}
    if data['hidden_observer'] != hidden or any(rows.get(hidden['name'], {}).get(k) != v for k, v in hidden.items()):
        raise ValueError('native hidden creature identity changed')


class Game:
    def __init__(self, name, shape, mode, owner, rows, oracle):
        self.shape, self.owner, self.foe = shape, owner, 1-owner
        self.rows, self.oracle = rows, oracle
        self.steps, self.setup, self.zones = [], [], {}
        self.pools = [[0]*6 for _ in (0, 1)]
        self.life, self.damage, self.tapped = [20, 20], {}, set()
        self.source, self.hidden, self.counters = 'source', False, 0
        self.add(name, 'source', owner, 'hand')
        companions = {'countered': ('Counterspell', 'counterspell', self.foe),
                      'bounce_reset': ('Unsummon', 'bounce', owner),
                      'grow_and_flip': ('Hunger of the Howlpack', 'grow', owner)}
        if mode in companions:
            self.add(*companions[mode], 'hand')
        if mode.startswith('keyword_'):
            witness = 'Nessian Asp' if mode == 'keyword_reach' else 'Welkin Tern' if 'Reach' in shape['traits'] else 'Elvish Mystic'
            self.add(witness, 'witness', self.foe, 'battlefield')
        for player in (0, 1):
            for index, name in enumerate(('Forest', 'Island')):
                self.add(name, f'library_{player}_{index}', player, 'library')

    def emit(self, op, **values):
        from recipes import step

        self.steps.append(step(op, **values))

    def add(self, name, ref, player, zone):
        from recipes import card

        self.setup.append(card(name, ref, player, zone))
        self.zones[ref] = zone

    def fund(self, player, pool):
        self.pools[player] = list(pool)
        for color, amount in enumerate(pool):
            self.emit('set_mana', player=player, color=color, value=amount)
        self.emit('refresh')

    def pay(self, player, cost):
        self.pools[player] = [have - spent for have, spent in zip(self.pools[player], cost)]
        if min(self.pools[player]) < 0:
            raise ValueError('legal choice exceeds available mana')

    def mana(self):
        for player, pool in enumerate(self.pools):
            for color, value in enumerate(pool):
                self.emit('assert_mana', player=player, color=color, value=value)

    def cast_source(self, hidden, *, both=False, short=False):
        printed = self.shape['printed']
        price = [3, 0, 0, 0, 0, 0] if hidden else printed
        pool = list(printed if both else price)
        if both and sum(pool) < 3:
            pool[0] += 3
        elif both:
            # Keep generic-only payment exact for the human UI, even when the
            # printed alternative contains enough colored mana to pay generic.
            pool[0] = max(pool[0], 3)
        if short:
            pool = [2, 0, 0, 0, 0, 0]
        self.fund(self.owner, pool)
        if both or (not hidden and sum(printed) >= 3):
            self.emit('choose_dialog', player=self.owner, value=int(not hidden))
        self.emit('cast', ref=self.source, value=int(not short))
        self.emit('assert_choices_consumed')
        self.emit('assert_stack', value=int(not short))
        if not short:
            self.pay(self.owner, price)
            self.hidden, self.counters = hidden, 0
            self.zones['source'] = 'stack'
            self.emit('assert_card', ref=self.source, property='csvid', value=FACE_DOWN if hidden else self.shape['csvid'])
        self.mana()

    def resolve_cast(self):
        self.emit('resolve')
        self.emit('refresh')
        self.zones['source'] = 'battlefield'
        self.receipts()

    def flip(self, *, wrong=False, short=False):
        cost = self.shape['fee']
        pool = list(cost)
        if wrong:
            color = next(i for i in range(1, 6) if pool[i])
            wrong_color = next(i for i in range(1, 6) if not pool[i])
            pool[wrong_color], pool[color] = pool[color], 0
        if short:
            pool[0] -= 1
        self.fund(self.owner, pool)
        allowed = not (wrong or short)
        self.emit('can_activate', ref=self.source, value=int(allowed))
        self.emit('activate', ref=self.source, value=int(allowed))
        if allowed:
            self.pay(self.owner, cost)
            self.hidden = False
            self.counters += 1
            # Keep the rules-visible face-up transition and later cleanup
            # distinct from the engine's temporary activation bookkeeping.
            self.emit('refresh')
            self.check_source()
            self.emit('resolve')
            self.emit('refresh')
        self.receipts()

    def witness_spell(self, ref, *, response=False):
        from recipes import cost_from_oracle

        witness = next(row for row in self.setup if row['ref'] == ref)
        player = witness['player']
        cost = cost_from_oracle(self.oracle[witness['name']])
        self.fund(player, cost)
        if not response:
            self.emit('choose_target', player=player, ref=self.source)
        self.emit('cast_response' if response else 'cast', ref=ref, value=1)
        self.pay(player, cost)
        self.emit('resolve_response' if response else 'resolve')
        self.emit('refresh')
        self.zones[ref] = 'graveyard'

    def check_source(self):
        from generate_cases import TOKEN_PROPERTIES

        visible = FACE_DOWN if self.hidden else self.shape['csvid']
        stats = dict(csvid=visible, in_play=1, power=(2 if self.hidden else self.shape['power'])+self.counters,
                     toughness=(2 if self.hidden else self.shape['toughness'])+self.counters, plus1_counters=self.counters,
                     tapped=int('source' in self.tapped), damage=self.damage.get('source', 0))
        for prop, value in stats.items():
            self.emit('assert_card', ref=self.source, property=prop, value=value)
        traits = {'color': 0 if self.hidden else self.shape['color'], 'token': 0, 'artifact': 0}
        traits.update({f'subtype_{s}': int(not self.hidden) for s in self.shape['subtypes'] if f'subtype_{s}' in TOKEN_PROPERTIES})
        for prop, value in traits.items():
            self.emit('assert_matching_permanents', player=self.owner, index=visible, property=prop, value=value)

    def receipts(self):
        self.emit('assert_choices_consumed')
        self.emit('assert_stack', value=0)
        self.mana()
        for player in (0, 1):
            self.emit('assert_life', player=player, value=self.life[player])
            for zone in ('battlefield', 'hand', 'graveyard', 'exile', 'library'):
                self.emit('assert_zone', player=player, zone=zone, value=sum(
                    row['player'] == player and self.zones[row['ref']] == zone for row in self.setup))
        for row in self.setup:
            ref = row['ref']
            zone = self.zones[ref]
            expected = FACE_DOWN if ref == 'source' and self.hidden and zone == 'battlefield' else self.rows[row['name']]['csvid']
            self.emit('assert_zone_multiplicity', player=row['player'], zone=zone, index=expected, value=1)
        if self.zones['source'] == 'battlefield':
            self.check_source()
        if self.zones.get('witness') == 'battlefield':
            for prop, value in [('tapped', int('witness' in self.tapped)), ('damage', self.damage.get('witness', 0))]:
                self.emit('assert_card', ref='witness', property=prop, value=value)

    def combat(self, mode):
        printed = self.shape['traits']
        abilities = [] if self.hidden else printed
        defensive = 'Reach' in printed or mode == 'keyword_defender_blocks'
        witness = next(row['name'] for row in self.setup if row['ref'] == 'witness')
        observed = self.oracle[witness]
        stats = {'source': (2, 2) if self.hidden else (self.shape['power']+self.counters, self.shape['toughness']+self.counters),
                 'witness': (int(observed['power']), int(observed['toughness']))}
        attacker, blocker = ('witness', 'source') if defensive else ('source', 'witness')
        self.emit('advance_turn')
        if not defensive:
            self.emit('advance_turn')
        self.emit('begin_attackers')
        attack_legal = defensive or 'Defender' not in abilities
        self.emit('attack', ref=attacker, value=int(attack_legal))
        if attack_legal and (defensive or 'Vigilance' not in abilities):
            self.tapped.add(attacker)
        if attack_legal:
            self.emit('assert_card', ref=attacker, property='tapped', value=int(attacker in self.tapped))
        self.emit('finish_attackers')
        self.emit('begin_blockers')
        can_block = ('Flying' not in observed['keywords'] or 'Reach' in abilities) if defensive else (
            'Unblockable' not in abilities and ('Flying' not in abilities or 'Reach' in observed['keywords']))
        if attack_legal and (defensive or 'Defender' not in printed):
            self.emit('block', ref=blocker, other=attacker, value=int(can_block))
        else:
            can_block = False
        self.emit('advance_combat')
        defending = self.owner if defensive else self.foe
        damage, toughness = stats[attacker]
        hits = 2 if not defensive and 'Double strike' in abilities else 1
        if attack_legal:
            self.emit('assert_card', ref=attacker, property='blocked', value=int(can_block))
            if can_block:
                return_damage, blocker_toughness = stats[blocker]
                spill = max(0, damage-blocker_toughness) if not defensive and 'Trample' in abilities else 0
                assigned = (damage-spill) * hits
                if hits == 2 and damage >= blocker_toughness:
                    return_damage = 0
                self.damage.update({attacker: return_damage, blocker: assigned})
                if assigned >= blocker_toughness:
                    self.zones[blocker] = 'graveyard'
                if return_damage >= toughness:
                    self.zones[attacker] = 'graveyard'
                self.life[defending] -= spill
            else:
                self.life[defending] -= damage * hits
        self.emit('combat_damage')
        self.emit('end_combat')
        self.receipts()


def _case(name, csvid, declaration, shape, variant, rows, oracle):
    from recipes import step

    mode, seat = variant.rsplit('_', 1)
    owner = int(seat)
    game = Game(name, shape, mode, owner, rows, oracle)
    emit = game.emit
    emit('turn', player=owner, value=0)
    for player in (0, 1):
        game.fund(player, [0]*6)
    normal = mode in ('ordinary', 'keyword_ordinary')
    game.cast_source(not normal, both=mode == 'choose_hidden', short=mode == 'cast_short')
    if mode == 'countered':
        emit('announce_spell', ref='source')
        game.witness_spell('counterspell', response=True)
        game.zones['source'] = 'graveyard'
        game.receipts()
    elif mode == 'cast_short':
        game.receipts()
    else:
        game.resolve_cast()

    if mode == 'offturn':
        emit('advance_turn')
    if mode == 'grow_and_flip':
        game.witness_spell('grow')
        game.counters = 1
        game.receipts()
    replay_start = len(game.steps) if mode == 'replay' else None
    turn_up = {'flip', 'wrong_color', 'fee_short', 'offturn', 'counter_persists', 'already_up', 'grow_and_flip',
               'bounce_reset', 'replay', 'keyword_flipped', 'keyword_reach', 'keyword_defender_blocks'}
    if mode in turn_up:
        game.flip(wrong=mode == 'wrong_color', short=mode == 'fee_short')
    if mode == 'counter_persists':
        emit('advance_turn')
        emit('advance_turn')
        game.receipts()
    if mode == 'already_up':
        game.fund(owner, shape['fee'])
        emit('can_activate', ref='source', value=0)
        emit('activate', ref='source', value=0)
        game.receipts()
    if mode == 'bounce_reset':
        game.witness_spell('bounce')
        game.zones['source'] = 'hand'
        game.receipts()
        game.source = 'returned'
        emit('bind_hand_copy', ref=game.source, player=owner, index=0, value=csvid)
        game.cast_source(False)
        game.resolve_cast()
    if mode.startswith('keyword_'):
        game.combat(mode)

    if replay_start is not None:
        suffix = game.steps[replay_start:]
        game.steps = (game.steps[:replay_start] + [step('capture')] + suffix
                      + [step('record_state'), step('restore'), step('ai_roundtrip')] + suffix + [step('assert_recorded_state')])
    metadata = dict(family=FAMILY, version=2, variant=variant, params=declaration['params'],
                    oracle_text=declaration['oracle_text'], covered_clauses=shape['clauses'], untested_components=[
                        'Hidden-information UI, revealing cards outside the battlefield, and internal priority during turn-up.',
                        'Alternate damage allocations, transformed or copied permanents, and multiplayer.'])
    return dict(name=f'MegamorphKeywordCreatureRecipe_{csvid}_{variant}', card=name, fixture='empty_main', seed=123,
                setup=game.setup, steps=game.steps, _recipe=metadata)


def expand(name, csvid, declaration, shape, selected, rows, oracle):
    if csvid != shape['csvid'] or not selected or len(set(selected)) != len(selected) or set(selected) - set(variants(shape)):
        raise ValueError('distinct supported Megamorph variants required')
    validate_witnesses(oracle, rows)
    return [_case(name, csvid, declaration, shape, variant, rows, oracle) for variant in selected]
