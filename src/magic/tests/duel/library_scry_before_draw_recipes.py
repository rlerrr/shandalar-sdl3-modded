"""Real Scry before draw, reveal, entry abilities and exact printed riders."""

from functools import lru_cache
import json
from pathlib import Path

from library_search_recipes import wave6_exact as exact
from recipes import card, cost_from_oracle, step


FAMILY = 'library_scry_before_draw'
PROFILE = Path(__file__).parent / 'data' / f'{FAMILY}_wave8_profiles.json'
PREORDAIN, FORESEE, BONES, INTERPRET = 11160, 8929, 13532, 13914
REMINDER = (' (To scry {count}, look at the top {words} cards of your library, then put any number of them '
            'on the bottom and the rest on top in any order.)')
MODELS = {
    PREORDAIN: ('{U}', 2, 1, 0, 'Scry 2, then draw a card.' + REMINDER.format(count=2, words='two')),
    FORESEE: ('{3}{U}', 4, 2, 0, 'Scry 4, then draw two cards.'),
    BONES: ('{2}{B}', 2, 2, 2, 'Scry 2, then draw two cards. You lose 2 life.' + REMINDER.format(count=2, words='two')),
    INTERPRET: ('{5}{U}', 3, None, 0, "Scry 3, then reveal the top card of your library. Draw cards equal to that card's mana value."
                + REMINDER.format(count=3, words='three')),
}

RIDDLE = 9001
NISSA = 15187
UGIN = 15469
GLIMMER = 16395
PERCEPTION = 19690
ANNELID = 8907
EMPATH = 8960
EAGER = 16357
PUZZLE = 16394
FOLLOWERS = {
    RIDDLE: ('{3}{R}{R}', 'Instant', "Choose any target. Scry 3, then reveal the top card of your library. Riddle of Lightning deals damage equal to that card's mana value to that permanent or player.", ('Scry',), ('player', 'creature', 'planeswalker', 'lost_target')),
    NISSA: ('{5}{G}{G}', 'Sorcery', "Scry 5, then reveal the top card of your library. If it's a creature card, you draw cards equal to its power and you gain life equal to its toughness.", ('Scry',), ('creature', 'noncreature', 'short')),
    UGIN: ('{3}{U}{U}', 'Sorcery', 'Scry X, where X is the greatest mana value among permanents you control, then draw three cards.', ('Scry',), ('greatest', 'opposing_only', 'greatest_leaves')),
    GLIMMER: ('{3}{U}', 'Instant', 'Scry 2, then draw two cards. You get {E}{E} (two energy counters).', ('Scry',), ('offturn', 'short', 'countered')),
    PERCEPTION: ('{3}{U}{U}', 'Instant', 'Draw three cards.\nAddendum — If you cast this spell during your main phase, instead scry 3, then draw three cards.', ('Scry', 'Addendum'), ('own_main', 'offturn', 'own_combat')),
    ANNELID: ('{3}{U}', 'Creature — Worm Beast', 'When this creature enters, scry 1, then scry 2, then scry 3. (To scry X, look at the top X cards of your library, then put any number of them on the bottom and the rest on top in any order.)', ('Scry',), ('sequential', 'short')),
    EMPATH: ('{3}{G}', 'Creature — Elf Shaman', "When this creature enters, scry 2, then reveal the top card of your library. If it's a creature card, put it into your hand. (To scry 2, look at the top two cards of your library, then put any number of them on the bottom and the rest on top in any order.)", ('Scry',), ('creature', 'noncreature', 'short')),
    EAGER: ('{2}', 'Artifact Creature — Construct', 'When this creature enters, each player may scry 1. (To scry 1, look at the top card of your library, then you may put that card on the bottom.)', ('Scry',), ('both_bottom', 'opposing_empty')),
    PUZZLE: ('{2}', 'Artifact', 'When this artifact enters, scry 2, then you get {E}{E}. (You get two energy counters. To scry 2, look at the top two cards of your library, then put any number of them on the bottom and the rest on top in any order.)\n{2}{U}, Sacrifice this artifact: Scry 2, then you get {E}{E}.', ('Scry',), ('paid_sacrifice', 'short', 'short_payment')),
}
COUNTER = 'Counterspell'
LONG = ('Lightning Bolt', 'Divination', COUNTER, 'Reach Through Mists',
        'Elvish Mystic', 'Forest', 'Island', 'Plains', 'Mountain')
FOE_LIBRARY = ('Mountain', 'Island', 'Swamp', 'Forest')
OLD_GRAVE = 'Goblin Balloon Brigade'
ZONES = {'hand': 0, 'battlefield': 1, 'library': 2, 'graveyard': 3, 'exile': 4}
ZERO = (0,) * 6
SEED = 2857
TINKERER = 'Goblin Tinkerer'
SERRA = 'Serra Angel'
SPHINX = 'Sphinx of Magosi'
JACE = 'Jace Beleren'
BOUNCE = 'Unsummon'
CORROSION = 'Psychic Corrosion'
BOARD = {ANNELID: (1, 4), EMPATH: (2, 2), EAGER: (2, 2), PUZZLE: None}
WITNESSES = set(LONG) | set(FOE_LIBRARY) | {OLD_GRAVE, TINKERER, SERRA, SPHINX, JACE, BOUNCE, CORROSION, 'Brainstorm'}


def pilot_variants(csvid):
    shared = [(mode, seat) for mode in ('short', 'countered') for seat in (0, 1)]
    human = [(mode, 0) for mode in ('reverse_top', 'split_bottom', 'all_bottom')]
    ai = [(mode, 1) for mode in ('ai_sweep', 'ai_keep_land')]
    return shared + human + ai + ([('empty', 0), ('empty', 1), ('zero_top', 0)] if csvid == INTERPRET else [])


def label(csvid, mode, owner):
    return f'LibraryScryBeforeDrawRecipe_{csvid}_{mode}_{owner}'


class ScryCase:
    def __init__(self, record, csvid, owner, mode, rows, oracle):
        self.record, self.csvid, self.owner, self.foe, self.mode = record, csvid, owner, 1-owner, mode
        self.rows, self.oracle = rows, oracle
        self.setup, self.steps, self.refs = [], [], {}
        self.zones = [{zone: [] for zone in ZONES} for _ in (0, 1)]
        self.pools, self.life = [ZERO, ZERO], [20, 20]
        _, self.window, self.fixed_draw, self.life_loss, _ = MODELS[csvid]
        self.drawn, self.bottom, self.top = [], [], []
        self.revealed = None
        self.add('source', record['name'], owner, 'hand')
        if mode == 'countered':
            self.add('answer', COUNTER, self.foe, 'hand')
        library = LONG
        if mode == 'short':
            library = LONG[:self.fixed_draw or 1]
        elif mode == 'empty':
            library = ()
        elif mode == 'zero_top':
            library = ('Lightning Bolt', 'Forest', COUNTER, *LONG[3:])
        elif mode == 'ai_keep_land':
            library = LONG[:self.window-1] + ('Forest',) + LONG[self.window:5] + ('Island', 'Plains')
        for who, deck in ((owner, library), (self.foe, FOE_LIBRARY)):
            self.add(f'old_grave_{who}', OLD_GRAVE, who, 'graveyard')
            for index, name in enumerate(deck):
                self.add(f'library_{who}_{index}', name, who, 'library')

    def add(self, ref, name, player, zone):
        assert ref not in self.refs
        entry = card(name, ref, player, zone)
        if zone == 'battlefield':
            entry['state'] = ['untapped']
        self.setup.append(entry)
        self.refs[ref] = (name, player)
        self.zones[player][zone].append(name)

    def emit(self, op, ref=None, other=None, prop=None, *, player=None, value=None, zone=None, index=None, color=None):
        values = dict(ref=ref, other=other, property=prop, player=player, value=value,
                      zone=zone, index=index, color=color)
        self.steps.append(step(op, **{key: value for key, value in values.items() if value is not None}))

    def fund(self, player, pool):
        self.pools[player] = tuple(pool)
        for color, amount in enumerate(pool):
            self.emit('set_mana', player=player, color=color, value=amount)
        self.emit('refresh')

    def receipt(self, depth=0):
        self.emit('assert_choices_consumed')
        self.emit('assert_stack', value=depth)
        names = sorted({name for name, _ in self.refs.values()})
        for who in (0, 1):
            self.emit('assert_life', player=who, value=self.life[who])
            self.emit('assert_shuffle_count', player=who, value=0)
            for color, amount in enumerate(self.pools[who]):
                self.emit('assert_mana', player=who, color=color, value=amount)
            for zone in ZONES:
                if depth and zone == 'hand':
                    continue
                cards = self.zones[who][zone]
                self.emit('assert_zone', player=who, zone=zone, value=len(cards))
                if zone in ('library', 'graveyard'):
                    for index, name in enumerate(cards):
                        self.emit('assert_zone_card', player=who, zone=zone, index=index,
                                  value=self.rows[name]['csvid'])
                elif zone == 'hand':
                    for name in names:
                        self.emit('assert_zone_multiplicity', player=who, zone=zone,
                                  index=self.rows[name]['csvid'], value=cards.count(name))

    def cast(self, ref='source', *, response=False, target=None):
        name, actor = self.refs[ref]
        self.fund(actor, cost_from_oracle(self.oracle[name]))
        if isinstance(target, str):
            self.emit('choose_target', target, player=actor)
        elif type(target) is int:
            self.emit('choose_player', player=actor, value=target)
        self.emit('cast_response' if response else 'cast', ref, value=1)
        self.emit('assert_card', ref, prop='in_hand', value=0)
        self.pools[actor] = ZERO
        self.zones[actor]['hand'].remove(name)
        self.receipt(2 if response else 1)

    def finish(self, ref='source'):
        name, actor = self.refs[ref]
        self.zones[actor]['graveyard'].append(name)

    def pick(self, name):
        self.emit('choose_zone_card', player=self.owner, value=self.rows[name]['csvid'])

    def done(self):
        self.emit('choose_zone', player=self.owner, value=-1)

    def scry(self):
        library = self.zones[self.owner]['library']
        window = library[:self.window]
        assert len(window) == len(set(window))
        if not window:
            return

        if self.mode in ('all_bottom', 'ai_sweep', 'short'):
            bottom, top = window[::-1], []
        elif self.mode == 'ai_keep_land':
            bottom, top = window[:-1], window[-1:]
            assert self.oracle[top[0]]['type_line'].startswith('Basic Land ')
        elif self.mode == 'zero_top':
            bottom, top = [window[0], window[2]], [window[1]]
        elif self.mode == 'split_bottom':
            bottom, top = window[:1], window[:0:-1]
        else:
            bottom, top = [], window[::-1]

        if self.owner == 1:
            # With no lands in hand or in play, the real AI tutor may
            # bottom nonlands. The remaining zero or one card avoids
            # deriving Oracle order from any automatic ordering policy.
            assert len(top) <= 1
            assert all('Land' not in self.oracle[name]['type_line'].split(' — ')[0].split() for name in bottom)
        for name in bottom:
            self.pick(name)
        if top:
            self.done()
        if len(top) > 1:
            for name in reversed(top):
                self.pick(name)
            self.done()

        self.bottom, self.top = bottom, top
        library[:] = top + library[len(window):] + bottom

    def apply(self):
        self.scry()
        library = self.zones[self.owner]['library']
        amount = self.fixed_draw
        if amount is None:
            self.revealed = library[0] if library else None
            if self.revealed is not None and self.owner == 0:
                self.emit('choose_dialog', player=0, value=0)
            amount = int(self.oracle[self.revealed]['cmc']) if self.revealed else 0
        assert len(library) >= amount
        self.drawn = library[:amount]
        del library[:amount]
        self.zones[self.owner]['hand'].extend(self.drawn)
        self.life[self.owner] -= self.life_loss

    def play(self):
        self.emit('turn', player=self.owner)
        self.emit('present_ai_dialogs', value=1)
        self.fund(0, ZERO)
        self.fund(1, ZERO)
        self.receipt()
        self.cast()
        countered = self.mode == 'countered'
        if countered:
            self.emit('announce_spell', 'source')
            self.cast('answer', response=True)
            self.emit('resolve_response')
            self.finish('answer')
        else:
            self.apply()
            self.emit('resolve')
        self.emit('refresh')
        self.finish()
        self.receipt()


class FollowOn(ScryCase):
    def __init__(self, record, csvid, owner, mode, rows, oracle):
        self.record, self.csvid, self.owner, self.foe, self.mode = record, csvid, owner, 1-owner, mode
        self.rows, self.oracle = rows, oracle
        self.setup, self.steps, self.refs = [], [], {}
        self.zones = [{zone: [] for zone in ZONES} for _ in (0, 1)]
        self.pools, self.life, self.energy = [ZERO, ZERO], [20, 20], 0
        self.target, self.victim_damage, self.loyalty = None, 0, None
        self.drawn, self.revealed, self.windows = [], None, []
        self.add('source', record['name'], owner, 'hand')
        own, foe = self.build()
        for player, library in ((owner, own), (self.foe, foe)):
            self.add(f'old_grave_{player}', OLD_GRAVE, player, 'graveyard')
            for index, name in enumerate(library):
                self.add(f'library_{player}_{index}', name, player, 'library')

    def build(self):
        csvid, mode = self.csvid, self.mode
        # All scripted bottoms are nonlands in the real seat 1 Scry route.
        prefix = (LONG[0], LONG[1], LONG[2], LONG[3], TINKERER, OLD_GRAVE)
        own = prefix + LONG[4:]
        foe = (TINKERER,) + FOE_LIBRARY
        if csvid == RIDDLE:
            self.target = self.foe
            if mode in ('creature', 'lost_target'):
                self.add('victim', TINKERER, self.foe, 'battlefield')
                self.target = 'victim'
            if mode == 'lost_target':
                self.add('answer', BOUNCE, self.foe, 'hand')
            if mode == 'planeswalker':
                self.add('walker', JACE, self.foe, 'hand')
                self.target = 'walker'
        elif csvid == NISSA:
            own = prefix[:5] + ((TINKERER,) if mode != 'noncreature' else ('Forest',)) + LONG[4:]
        elif csvid == UGIN:
            self.add('opposing_high', SPHINX, self.foe, 'battlefield')
            if mode != 'opposing_only':
                self.add('low', TINKERER, self.owner, 'battlefield')
                self.add('high', SERRA, self.owner, 'battlefield')
            if mode == 'greatest_leaves':
                self.add('answer', BOUNCE, self.foe, 'hand')
        elif csvid == GLIMMER and mode == 'countered':
            self.add('answer', COUNTER, self.foe, 'hand')
        elif csvid == EMPATH:
            self.add('draw_observer', CORROSION, self.owner, 'battlefield')
            own = prefix[:2] + ((TINKERER,) if mode != 'noncreature' else ('Forest',)) + LONG[4:]
        elif csvid == EAGER and mode == 'opposing_empty':
            foe = ()
        if mode == 'short':
            own = (TINKERER,) if csvid in (NISSA, EMPATH) else prefix[:1] if csvid == PUZZLE else prefix[:2]
        if csvid in (ANNELID, PUZZLE) and mode != 'short':
            # Even after several rotations the next Scry window remains all nonland.
            own = prefix + ('Elvish Mystic', 'Brainstorm')
        return own, foe

    def receipt(self, depth=0):
        super().receipt(depth)
        for player in (0, 1):
            self.emit('assert_energy', player=player, value=self.energy if player == self.owner else 0)
        for ref, (name, player) in self.refs.items():
            if ref.startswith('library_') or name not in self.zones[player]['battlefield']:
                continue
            self.emit('assert_card', ref, prop='in_play', value=1)
            if 'power' in self.oracle[name]:
                self.emit('assert_card', ref, prop='power', value=int(self.oracle[name]['power']))
                self.emit('assert_card', ref, prop='toughness', value=int(self.oracle[name]['toughness']))
        if self.victim_damage:
            self.emit('assert_card', 'victim', prop='damage', value=self.victim_damage)
        if self.loyalty is not None:
            self.steps.append(step('assert_counter', ref='walker', counter_kind='loyalty', value=self.loyalty))

    def bottom_scry(self, player, count):
        library = self.zones[player]['library']
        size = min(len(library), count)
        seen = library[:size]
        self.windows.append((player, list(seen)))
        assert len(set(seen)) == size
        for name in reversed(seen):
            if player == 1:
                assert not self.oracle[name]['type_line'].startswith('Basic Land')
            self.emit('choose_zone_card', player=player, value=self.rows[name]['csvid'])
        library[:] = library[size:] + seen[::-1]

    def reveal(self):
        library = self.zones[self.owner]['library']
        self.revealed = library[0] if library else None
        if self.revealed is not None and self.owner == 0:
            self.emit('choose_dialog', player=0, value=0)
        return self.oracle[self.revealed] if self.revealed else None

    def draw(self, amount):
        library = self.zones[self.owner]['library']
        assert len(library) >= amount
        drawn = library[:amount]
        self.drawn.extend(drawn)
        del library[:amount]
        self.zones[self.owner]['hand'].extend(drawn)

    def resolve(self, response=False):
        self.emit('resolve_response' if response else 'resolve')
        self.emit('refresh')

    def prepare_walker(self):
        self.cast('walker')
        self.resolve()
        self.zones[self.foe]['battlefield'].append(JACE)
        self.loyalty = 3
        self.receipt()

    def finish_entry(self):
        self.zones[self.owner]['battlefield'].append(self.record['name'])

    def riddle(self):
        mode = self.mode
        if mode == 'planeswalker':
            self.prepare_walker()
        self.cast(target=self.target)
        if mode == 'lost_target':
            self.emit('announce_spell', 'source')
            self.cast('answer', response=True, target='victim')
            self.resolve(response=True)
            self.finish('answer')
            self.zones[self.foe]['battlefield'].remove(TINKERER)
            self.zones[self.foe]['hand'].append(TINKERER)
        else:
            self.bottom_scry(self.owner, 3)
            amount = int(self.reveal()['cmc'])
            if mode == 'player':
                self.life[self.foe] -= amount
            elif mode == 'creature':
                self.victim_damage = amount
            else:
                self.loyalty -= amount
            self.resolve()
        self.finish()

    def nissa(self):
        self.cast()
        self.bottom_scry(self.owner, 5)
        top = self.reveal()
        if top and top['type_line'].startswith('Creature'):
            self.draw(int(top['power']))
            self.life[self.owner] += int(top['toughness'])
        self.resolve()
        self.finish()

    def ugin(self):
        self.cast()
        response = self.mode == 'greatest_leaves'
        home = self.zones[self.owner]
        if response:
            self.emit('announce_spell', 'source')
            self.cast('answer', response=True, target='high')
            self.finish('answer')
            home['battlefield'].remove(SERRA)
            home['hand'].append(SERRA)
        greatest = max((int(self.oracle[name]['cmc']) for name in home['battlefield']), default=0)
        self.bottom_scry(self.owner, greatest)
        self.draw(3)
        self.resolve(response=response)
        self.finish()

    def fixed_draw(self):
        self.cast()
        countered = self.mode == 'countered'
        if countered:
            self.emit('announce_spell', 'source')
            self.cast('answer', response=True)
            self.resolve(response=True)
            self.finish('answer')
        else:
            if self.csvid == GLIMMER:
                self.bottom_scry(self.owner, 2)
                self.draw(2)
                self.energy += 2
            else:
                if self.mode == 'own_main':
                    self.bottom_scry(self.owner, 3)
                self.draw(3)
            self.resolve()
        self.finish()

    def enter(self):
        self.cast()
        if self.csvid == ANNELID:
            for size in (1, 2, 3):
                self.bottom_scry(self.owner, size)
        elif self.csvid == EMPATH:
            self.bottom_scry(self.owner, 2)
            top = self.reveal()
            if top and top['type_line'].startswith('Creature'):
                self.zones[self.owner]['hand'].append(self.zones[self.owner]['library'].pop(0))
        elif self.csvid == EAGER:
            for player in (self.owner, self.foe):
                self.bottom_scry(player, 1)
        else:
            self.bottom_scry(self.owner, 2)
            self.energy += 2
        self.resolve()
        self.finish_entry()
        if self.csvid == PUZZLE:
            self.receipt()
            self.puzzle_cost()

    def puzzle_cost(self):
        legal = self.mode != 'short_payment'
        fee = (2, 0, int(legal), 0, 0, 0)
        self.fund(self.owner, fee)
        self.emit('can_activate', 'source', value=int(legal))
        self.emit('activate', 'source', value=int(legal))
        if not legal:
            return
        self.pools[self.owner] = ZERO
        self.zones[self.owner]['battlefield'].remove(self.record['name'])
        self.finish()
        self.receipt(1)
        self.bottom_scry(self.owner, 2)
        self.resolve()
        self.energy += 2

    def play(self):
        offturn = self.mode in ('offturn', 'planeswalker')
        self.emit('turn', player=self.foe if offturn else self.owner)
        self.emit('present_ai_dialogs', value=1)
        self.fund(0, ZERO)
        self.fund(1, ZERO)
        self.receipt()
        if self.mode == 'own_combat':
            self.emit('begin_attackers')
            self.emit('finish_attackers')
        actions = {RIDDLE: self.riddle, NISSA: self.nissa, UGIN: self.ugin,
                   GLIMMER: self.fixed_draw, PERCEPTION: self.fixed_draw}
        actions.get(self.csvid, self.enter)()
        self.receipt()


@lru_cache(maxsize=1)
def profiles():
    return json.loads(PROFILE.read_bytes())


def classify(record, csvid):
    if csvid in MODELS:
        cost, _, _, _, text = MODELS[csvid]
        typ, keywords = 'Sorcery', ('Scry',)
        choices = pilot_variants(csvid)
    else:
        cost, typ, text, keywords, modes = FOLLOWERS[csvid]
        choices = [(mode, owner) for mode in modes for owner in (0, 1)]
    if (record.get('oracle_text') != text or record.get('mana_cost') != cost
            or record.get('type_line') != typ or record.get('layout') != 'normal'
            or record.get('card_faces') or record.get('selected_face') or set(record.get('keywords', ())) != set(keywords)):
        raise ValueError('Exact whole Scry primary, mana and printed effects required')
    if csvid in BOARD and BOARD[csvid] is not None:
        assert tuple(int(record[prop]) for prop in ('power', 'toughness')) == BOARD[csvid]
    return dict(csvid=csvid, printed_cost=cost, choices=[f'{mode}_{owner}' for mode, owner in choices])


def variants(model):
    return list(model['choices'])


def validate_witnesses(oracle, rows):
    pins = profiles()
    if (set(pins) != {'sources', 'witnesses'} or set(pins['witnesses']) != WITNESSES
            or {pin['registry']['csvid'] for pin in pins['sources'].values()} != MODELS.keys() | FOLLOWERS.keys()):
        raise ValueError('Exact Scry source and witness scope required')
    for group in pins.values():
        for name, pin in group.items():
            record, row = oracle.get(name, {}), rows.get(name, {})
            if (not exact(pin, dict(oracle=record, registry=row)) or record.get('layout') != 'normal'
                    or row.get('kind') != 'card' or row.get('handler') in (None, '0', 'NULL', 'card_dummy')):
                raise ValueError(f'Changed implemented Scry source or witness: {name}')


def spec(declaration, record):
    pin = profiles()['sources'].get((record or {}).get('name'))
    if not pin or not exact(record, pin['oracle']):
        raise ValueError('Complete pinned Scry Oracle required')
    model = classify(record, pin['registry']['csvid'])
    selected = declaration.get('variants')
    if (declaration.get('family') != FAMILY or type(declaration.get('version')) is not int
            or declaration['version'] != 2 or declaration.get('oracle_text') != record['oracle_text']
            or not exact(declaration.get('params'), model) or not isinstance(selected, list) or not selected
            or any(type(v) is not str for v in selected) or len(selected) != len(set(selected))
            or set(selected) - set(variants(model))):
        raise ValueError('Exact Scry source, action grammar and scoped variants required')
    return dict(model, family=FAMILY)


def expand(name, csvid, declaration, model, selected, rows, oracle):
    validate_witnesses(oracle, rows)
    if rows[name]['csvid'] != csvid or model['csvid'] != csvid or set(selected) - set(variants(model)):
        raise ValueError('Exact Scry primary and variants required')
    gaps = {
        EAGER: ['Independent optional decline is unobserved because the callback offers Scry for both players directly'],
        PERCEPTION: ['Cast-phase commitment across a later phase boundary is unobserved'],
    }.get(csvid, [])
    cases = []
    factory = FollowOn if csvid in FOLLOWERS else ScryCase
    for variant in selected:
        mode, player = variant.rsplit('_', 1)
        game = factory(oracle[name], csvid, int(player), mode, rows, oracle)
        game.play()
        metadata = dict(family=FAMILY, version=2, variant=variant, params=declaration['params'],
                        oracle_text=declaration['oracle_text'], covered_clauses=declaration['oracle_text'].splitlines(),
                        untested_components=gaps)
        cases.append(dict(name=label(csvid, mode, int(player)), card=name, fixture='empty_main', seed=SEED,
                          setup=game.setup, steps=game.steps, _recipe=metadata))
    return cases
