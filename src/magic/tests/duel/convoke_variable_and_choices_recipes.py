"""Actual variable Convoke payment, choices and riders through the duel engine."""

from functools import lru_cache
import json
from pathlib import Path

from death_counter_trigger_recipes import exact
from recipes import cost_from_oracle

FAMILY = 'convoke_variable_and_choices'
PROFILE = Path(__file__).with_name('data') / f'{FAMILY}_wave8_profiles.json'
RANKS, SPROUT, STAIN, MARCH = (14162, 9030, 14184, 19428)
SOLDIER = 18822
SEED = 2688
LABEL = 'ConvokeVariableAndChoicesRecipe'
MODES = {
    RANKS: ('two_taps', 'mixed_x', 'zero', 'insufficient_white'),
    SPROUT: ('base', 'mixed', 'buyback', 'decline'),
    STAIN: ('countered', 'exile', 'decline', 'self_target'),
    MARCH: ('two_taps', 'mixed_x', 'lifelink', 'zero'),
}
ZA = ('hand', 'battlefield', 'library', 'graveyard', 'exile')
ZERO = (0, 0, 0, 0, 0, 0)
GREEN, WHITE = (0x08, 0x20)
FIXED_WHITE = ('Savannah Lions', 'Benalish Hero')
NEUTRAL = ('Memnite', 'Ornithopter', 'Phyrexian Walker')


@lru_cache(maxsize=1)
def pins():
    return json.loads(PROFILE.read_bytes())


def classify(record, csvid):
    pin = pins()['sources'].get(record.get('name'))
    if (csvid not in MODES or not pin or pin['registry']['csvid'] != csvid
            or not exact(record, pin['oracle']) or record.get('layout') != 'normal'
            or 'Convoke' not in record.get('keywords', ()) or not record['oracle_text'].startswith('Convoke (')):
        raise ValueError('complete normal variable Convoke source required')
    variable = csvid in (RANKS, MARCH)
    mana = record['mana_cost']
    if variable:
        assert mana.startswith('{X}')
        mana = mana.removeprefix('{X}')
    cost = cost_from_oracle({'mana_cost': mana})
    return dict(csvid=csvid, fixed_cost=cost, variable=variable)


def variants(shape):
    cid = shape['csvid']
    return [f'{mode}_{seat}' for mode in MODES[cid]
            for seat in ((1,) if cid == STAIN and mode != 'countered' else (0, 1))]


def validate_variants(shape, selected):
    if (type(selected) is not list or not selected or any(type(v) is not str for v in selected)
            or len(set(selected)) != len(selected) or set(selected) - set(variants(shape))):
        raise ValueError('explicit distinct supported variable Convoke variants required')


def spec(declaration, record):
    pin = pins()['sources'].get(record.get('name'))
    if not pin:
        raise ValueError('pinned variable Convoke source required')
    shape = classify(record, pin['registry']['csvid'])
    if (declaration.get('family') != FAMILY or type(declaration.get('version')) is not int
            or declaration['version'] != 2 or declaration.get('oracle_text') != record['oracle_text']
            or not exact(declaration.get('params'), shape)):
        raise ValueError('whole versioned variable Convoke declaration required')
    validate_variants(shape, declaration.get('variants'))
    return dict(shape, family=FAMILY)


def validate_witnesses(oracle, rows):
    for group in ('sources', 'witnesses'):
        for name, pin in pins()[group].items():
            row = rows.get(name, {})
            if (not exact(oracle.get(name), pin['oracle']) or not exact(row, pin['registry'])
                    or row.get('kind') != 'card' or row.get('handler') in (None, 'NULL', '0')
                    or group == 'sources' and row['handler'] == 'card_dummy'):
                raise ValueError(f'changed variable Convoke fixture identity: {name}')


class Scene:
    def __init__(self, row, owner, mode, rows, oracle):
        self.name, self.id = row['name'], row['csvid']
        self.owner, self.foe, self.mode = owner, 1 - owner, mode
        self.rows, self.oracle = rows, oracle
        self.setup, self.steps, self.objects = [], [], {}
        self.zones = {p: {z: [] for z in ZA} for p in (0, 1)}
        self.tappers = []
        self.tapped = set()
        self.shuffle = [0, 0]
        self.life = [20, 20]
        self.add('source', self.name, owner, 'hand')
        self.add('foe_untapped', 'Darksteel Myr', self.foe)
        self.add('tapped', 'Grizzly Bears', owner, state=1)

    def emit(self, op, ref=None, *, prop=None, **values):
        if ref is not None:
            values['ref'] = ref
        if prop is not None:
            values['property'] = prop
        self.steps.append(dict(op=op, **values))

    def add(self, ref, name, player, zone='battlefield', state=0):
        row = dict(ref=ref, player=player, zone=zone, name=name)
        if state:
            row['state'] = [{1: 'tapped', 2: 'sick'}[state]]
        self.setup.append(row)
        self.objects[ref] = (name, player, zone)
        self.zones[player][zone].append(name)

    def move(self, name, player, former, destination):
        self.zones[player][former].remove(name)
        self.zones[player][destination].append(name)

    def prop(self, ref, field, value):
        self.emit('assert_card', ref, prop=field, value=value)

    def seed_tappers(self, names, *, sick_ref=None):
        for i, name in enumerate(names):
            ref = f'helper_{i}'
            self.add(ref, name, self.owner, state=2 if ref == sick_ref else 0)
            self.tappers.append(ref)
        if sick_ref is not None:
            self.prop(sick_ref, 'sick', 1)

    def pay_with(self, offset, count):
        used = self.tappers[offset:offset + count]
        assert len(used) == count and self.tapped.isdisjoint(used)
        if self.owner == 0:
            for ref in used:
                self.emit('choose_target', ref, player=0)
        self.tapped.update(used)

    def book_cast(self, budget=ZERO, *, variable=False):
        self.mana(self.owner, budget, write=True)
        self.emit('refresh')
        if variable:
            # The outer cast bridge asks X from the mana pool before the
            # spell handles deferred Convoke. Preserve leftover mana for the
            # distinct real X transaction in the March callback when used.
            self.emit('choose_x', player=self.owner, value=0)

    def observe_tappers(self):
        for ref in self.tappers:
            self.prop(ref, 'in_play', 1)
            self.prop(ref, 'tapped', int(ref in self.tapped))
        self.prop('foe_untapped', 'tapped', 0)
        self.prop('tapped', 'tapped', 1)

    def mana(self, who, pool=ZERO, write=False):
        for color, value in enumerate(pool):
            self.emit('set_mana' if write else 'assert_mana', player=who, color=color, value=value)

    def cast(self, *, allowed=True):
        self.emit('cast', 'source', value=int(allowed))
        self.emit('assert_stack', value=int(allowed))
        self.emit('assert_unused_choices', value=0)
        self.prop('source', 'in_hand', int(not allowed))
        if allowed:
            self.mana(self.owner)
            self.observe_tappers()

    def end(self, destination='graveyard'):
        self.emit('resolve')
        self.emit('assert_stack', value=0)
        self.emit('assert_unused_choices', value=0)
        if destination != 'hand':
            self.move(self.name, self.owner, 'hand', destination)

    def observe_tokens(self, name, count, color):
        self.zones[self.owner]['battlefield'].extend([name] * count)
        if count:
            cid = self.rows[name]['csvid']
            for field, value in (('token', 1), ('power', 1), ('toughness', 1), ('color', color),
                                 ('subtype_' + name, 1)):
                self.emit('assert_matching_permanents', player=self.owner, prop=field, value=value, index=cid)

    def receipts(self, *extra):
        self.emit('refresh')
        self.emit('assert_stack', value=0)
        names = set(extra)
        for zones in self.zones.values():
            for cards in zones.values():
                names.update(cards)
        for who, zones in self.zones.items():
            self.emit('assert_life', player=who, value=self.life[who])
            self.emit('assert_shuffle_count', player=who, value=self.shuffle[who])
            for zone, cards in zones.items():
                self.emit('assert_zone', player=who, zone=zone, value=len(cards))
                for name in sorted(names):
                    self.emit('assert_zone_multiplicity', player=who, zone=zone, value=cards.count(name),
                              index=self.rows[name]['csvid'])
                if zone == 'library' and not self.shuffle[who]:
                    for i, name in enumerate(cards):
                        self.emit('assert_zone_card', player=who, zone=zone, index=i, value=self.rows[name]['csvid'])

    def ranks(self):
        for index, name in enumerate(('Hill Giant', 'Swamp', 'Grizzly Bears', 'Ornithopter')):
            self.add(f'grave_{index}', name, self.owner, 'graveyard')
        self.add('foe_grave', 'Savannah Lions', self.foe, 'graveyard')
        allowed = self.mode != 'insufficient_white'
        if not allowed:
            self.seed_tappers((FIXED_WHITE[0], *NEUTRAL))
            budget = (2, 0, 0, 0, 0, 0)
            self.book_cast(budget)
            self.cast(allowed=False)
            self.mana(self.owner, budget)
            self.observe_tappers()
            self.receipts()
            return
        target_names = () if self.mode == 'zero' else ('Grizzly Bears', 'Ornithopter')
        extra_bodies = {'zero': 0, 'mixed_x': 1, 'two_taps': 2}[self.mode]
        pool = (int(self.mode == 'mixed_x'), 0, 0, 0, 0, 0)
        self.seed_tappers((*FIXED_WHITE, *NEUTRAL[:extra_bodies]))
        # Return to the Ranks sets its own X from the chosen grave targets.
        # Its engine cc[1] is 0, so there is no outer X choice to queue.
        self.book_cast(pool)
        self.pay_with(0, 2)
        if target_names:
            self.emit('observe_search_filters', value=1)
            for name in target_names:
                self.emit('choose_zone_card', player=self.owner, value=self.rows[name]['csvid'])
            self.pay_with(2, extra_bodies)
        self.cast()
        if target_names:
            for name in ('Grizzly Bears', 'Ornithopter', 'Hill Giant', 'Swamp'):
                # The first chosen identity is replaced by a sentinel in
                # the second offered list. Observe the remaining eligible card.
                if name == 'Grizzly Bears':
                    continue
                self.emit('assert_offered_legal_card', player=self.owner, index=self.rows[name]['csvid'],
                          value=int(name == 'Ornithopter'))
        self.end()
        for name in target_names:
            self.move(name, self.owner, 'graveyard', 'battlefield')
        self.receipts()

    def sprout(self):
        mixed = self.mode == 'mixed'
        offered = self.mode in ('buyback', 'decline')
        bought = self.mode == 'buyback'
        contributors = ('Grizzly Bears',) if mixed else ('Grizzly Bears', 'Memnite')
        if offered:
            contributors += ('Ornithopter', 'Phyrexian Walker', 'Hill Giant')
        self.seed_tappers(contributors)
        pool = (1, 0, 0, 0, 0, 0) if mixed else ZERO
        self.book_cast(pool)
        self.pay_with(0, 1 if mixed else 2)
        if offered:
            self.emit('choose_dialog', player=self.owner, value=int(bought))
        if bought:
            self.pay_with(2, 3)
        self.cast()
        self.end('hand' if bought else 'graveyard')
        self.observe_tokens('Saproling', 1, GREEN)
        self.receipts()

    def stain(self):
        countered = self.mode == 'countered'
        victim = self.owner if self.mode == 'self_target' else self.foe
        for i, zone in enumerate(('hand', 'library', 'graveyard')):
            self.add(f'match_{i}', 'Hill Giant', victim, zone)
        self.add('library_tail', 'Plains', victim, 'library')
        self.add('wrong_hand', 'Mountain', victim, 'hand')
        self.add('wrong_grave', 'Swamp', victim, 'graveyard')
        if countered:
            self.add('reply', 'Counterspell', self.foe, 'hand')
        self.seed_tappers(('Walking Corpse', *NEUTRAL, 'Savannah Lions'))
        self.book_cast()
        self.pay_with(0, 5)
        self.emit('choose_player', player=self.owner, value=victim)
        self.cast()
        if countered:
            self.emit('announce_spell', 'source')
            self.mana(self.foe, cost_from_oracle(self.oracle['Counterspell']), write=True)
            self.emit('refresh')
            self.emit('cast_response', 'reply', value=1)
            self.emit('assert_stack', value=2)
            self.mana(self.foe)
            self.emit('resolve_response')
            self.emit('assert_stack', value=0)
            self.emit('assert_unused_choices', value=0)
            self.move('Counterspell', self.foe, 'hand', 'graveyard')
            self.move(self.name, self.owner, 'hand', 'graveyard')
        else:
            # Only the actual AI route selects an automatic name. Its one
            # nonland choice exists in the target player's real library.
            assert self.owner == 1
            self.emit('choose_dialog', player=0, value=0)
            self.emit('observe_search_filters', value=1)
            decline = self.mode == 'decline'
            for zone in ('hand', 'graveyard', 'library'):
                self.emit('choose_optional_zone' if decline else 'choose_zone_card', player=self.owner,
                          value=self.rows['Hill Giant']['csvid'])
                if not decline:
                    self.move('Hill Giant', victim, zone, 'exile')
            self.end()
            self.emit('assert_forced_zone_choices', value=0)
            self.shuffle[victim] += 1
        self.receipts()

    def march(self):
        x = {'two_taps': 2, 'mixed_x': 2, 'lifelink': 1, 'zero': 0}[self.mode]
        convoke = self.mode != 'zero'
        if convoke:
            more_bodies = 2 if self.mode == 'two_taps' else 1
            self.seed_tappers(('Grizzly Bears', *FIXED_WHITE, *NEUTRAL[:more_bodies]), sick_ref='helper_3')
            pool = (int(self.mode == 'mixed_x'), 0, 0, 0, 0, 0)
            self.book_cast(pool, variable=True)
            self.pay_with(0, 3)
            self.pay_with(3, more_bodies)
            if self.mode == 'mixed_x':
                self.emit('choose_x', player=self.owner, value=1)
        else:
            self.book_cast((0, 0, 0, 1, 0, 2), variable=True)
        self.cast()
        self.end()
        self.observe_tokens('Soldier', x, WHITE)
        self.receipts('Soldier')
        if self.mode == 'lifelink':
            self.lifelink()

    def lifelink(self):
        self.emit('bind_permanent_copy', 'soldier', player=self.owner, index=0, value=SOLDIER)
        self.prop('soldier', 'sick', 1)
        for who in (self.foe, self.owner):
            self.emit('advance_turn')
            self.emit('untap_step', player=who)
            self.emit('turn', player=who)
        self.prop('soldier', 'sick', 0)
        self.emit('begin_attackers')
        self.emit('attack', 'soldier', value=1)
        self.emit('finish_attackers')
        self.emit('begin_blockers')
        self.emit('advance_combat')
        self.emit('combat_damage')
        self.emit('resolve')
        self.emit('assert_unused_choices', value=0)
        self.emit('end_combat')
        self.life[self.owner] += 1
        self.life[self.foe] -= 1
        self.receipts()

    def play(self):
        self.emit('turn', player=self.owner)
        self.emit('present_ai_dialogs', value=1)
        for who in (0, 1):
            self.mana(who, write=True)
        {RANKS: self.ranks, SPROUT: self.sprout, STAIN: self.stain, MARCH: self.march}[self.id]()
        self.emit('present_ai_dialogs', value=0)


def expand(name, csvid, declaration, shape, selected, rows, oracle):
    validate_witnesses(oracle, rows)
    validate_variants(shape, selected)
    if pins()['sources'][name]['registry']['csvid'] != csvid:
        raise ValueError('exact registered variable Convoke CSVID required')
    cases = []
    for variant in selected:
        mode, seat = variant.rsplit('_', 1)
        scene = Scene(rows[name], int(seat), mode, rows, oracle)
        scene.play()
        gaps = []
        if csvid == STAIN and seat == '0':
            gaps.append('Human resolving card-name dialog has no headless input bridge. Seat one resolves actual search.')
        cases.append(dict(name=f'{LABEL}_{csvid}_{variant}', card=name, fixture='empty_main', seed=SEED,
                          setup=scene.setup, steps=scene.steps,
                          _recipe=dict(family=FAMILY, version=2, variant=variant, params=declaration['params'],
                                       oracle_text=declaration['oracle_text'], untested_components=gaps)))
    return cases
