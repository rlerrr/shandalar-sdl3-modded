"""Mill spell choices, ordered transfers, Cipher, Chroma, and paid riders."""

from functools import lru_cache
import json
from pathlib import Path
import re

from library_search_recipes import wave6_exact as exact
from recipes import card, cost_from_oracle, step


FAMILY = 'mill_remaining_spells_and_modes'
PROFILE = Path(__file__).parent / 'data' / f'{FAMILY}_wave8_profiles.json'
BOOK, CIPHER, SPIRAL, SANITY = 5913, 13027, 12788, 9861
TRAUMA, INDIGNATION = 11735, 15882
MODELS = {
    TRAUMA: ('{B}', 'Sorcery',
             ('Join forces — Starting with you, each player may pay any amount of mana. '
              'Each player mills X cards, where X is the total amount of mana paid this way.',),
             ('split', 'owner_only', 'foe_only', 'decline', 'short_split', 'colored'), ()),
    INDIGNATION: ('{X}{B}', 'Sorcery',
                  ('Mill X cards. Target player loses 2 life for each creature card put into your graveyard this way.',),
                  ('mixed', 'zero_x', 'short_library', 'no_creatures', 'through_purity'), ('self_target',)),
    BOOK: ('{1}{R}', 'Sorcery',
           ("Any player may have Book Burning deal 6 damage to them. If no one does, target player mills six cards.",),
           ('both_decline', 'seat0_accept', 'seat1_accept', 'short_library'), ('self_target',)),
    CIPHER: ('{U}{B}', 'Sorcery',
             ('Target player mills three cards.',
              "Cipher (Then you may exile this spell card encoded on a creature you control. Whenever that creature deals combat damage to a player, its controller may cast a copy of the encoded card without paying its mana cost.)"),
             ('no_host', 'decline_encode', 'encode_hit', 'encode_blocked'), ('decline_copy',)),
    SPIRAL: ('{4}{U}', 'Instant',
             ('Shuffle all cards from your graveyard into your library. Target player mills that many cards.',),
             ('opposing', 'short_target', 'live_grave'), ('self_target',)),
    SANITY: ('{U}{U}{U}', 'Sorcery',
             ('Chroma — Reveal the top ten cards of your library. For each blue mana symbol in the mana costs of the revealed cards, target opponent mills a card. Then put the cards you revealed this way on the bottom of your library in any order.',),
             ('mixed', 'short_reveal', 'zero_blue', 'empty_reveal', 'short_target'), ()),
}
BODY = 'Goblin Balloon Brigade'
BLOCKER = 'Goblin Tinkerer'
CARRIER = 'Reach Through Mists'
HYBRID = 'Dimir Guildmage'
RELIC = 'Darksteel Relic'
THREE = 'Sphinx of Magosi'
BOOK_COSTS = ('Forest', BODY, 'Mountain', RELIC, 'Plains', 'Island', BLOCKER)
LIBRARY = (BODY, 'Island', 'Counterspell', RELIC, 'Mountain', BLOCKER, 'Forest', 'Brainstorm', 'Plains', CARRIER,
           'Swamp', 'Divination', HYBRID, 'Forest')
PURITY = 'Purity'
SHOCK = 'Shock'
OWN_MIX = (BODY, 'Forest', BLOCKER, RELIC, 'Plains', CARRIER)
CHROMA = ('Forest', 'Counterspell', BODY, HYBRID, 'Brainstorm', 'Island', CARRIER, RELIC,
          'Divination', THREE, 'Cancel', 'Forest')
NO_BLUE = (BODY, 'Forest', 'Island', RELIC, 'Plains', BLOCKER, 'Mountain', 'Swamp', 'Mountain', 'Forest', THREE)
ZONES = {'hand': 0, 'battlefield': 1, 'library': 2, 'graveyard': 3, 'exile': 4}
ZERO = (0,) * 6
UNTAPPED = 8
SEED = 2809
FOLLOWERS = {TRAUMA, INDIGNATION}
WITNESSES = set(BOOK_COSTS) | set(LIBRARY) | set(CHROMA) | set(NO_BLUE) | {'Paranoid Delusions', PURITY, SHOCK}
# Exactly one kind of spendable mana per participant makes the contribution
# and residual wallet unambiguous while still allowing a partial contribution.
CONTRIBUTIONS = {
    'split': ((0, 4, 2), (0, 3, 1)),
    'owner_only': ((0, 3, 3), (0, 1, 0)),
    'foe_only': ((0, 2, 0), (0, 3, 3)),
    'decline': ((0, 1, 0), (0, 2, 0)),
    'short_split': ((0, 3, 3), (0, 2, 2)),
    'colored': ((2, 2, 2), (3, 1, 1)),
}


def label(csvid, mode, owner):
    return f'MillRemainingSpellsAndModesRecipe_{csvid}_{mode}_{owner}'


def blue_symbols(record):
    symbols = re.findall(r'\{([^{}]+)\}', record['mana_cost'])
    return sum('U' in symbol.split('/') for symbol in symbols)


class MillCase:
    def __init__(self, record, csvid, owner, mode, rows, oracle):
        self.record, self.csvid, self.owner, self.foe, self.mode = record, csvid, owner, 1-owner, mode
        self.rows, self.oracle = rows, oracle
        self.setup, self.steps, self.refs = [], [], {}
        self.zones = [{zone: [] for zone in ZONES} for _ in (0, 1)]
        self.pools = [ZERO, ZERO]
        self.shuffles, self.life = [0, 0], [20, 20]
        self.order_unknown = set()
        self.target = owner if mode == 'self_target' else self.foe
        self.milled = []
        self.revealed, self.bottom = [], []
        self.add('source', record['name'], owner, 'hand')
        self.build()

    def build(self):
        mode, owner, foe = self.mode, self.owner, self.foe
        libraries = [None, None]
        libraries[owner], libraries[foe] = BOOK_COSTS, LIBRARY
        if mode in ('short_library', 'short_target'):
            libraries[foe] = LIBRARY[:2]
        if self.csvid == CIPHER:
            self.add('blocker', BLOCKER, foe, 'battlefield', UNTAPPED)
            if mode != 'no_host':
                self.add('host', BODY, owner, 'battlefield', UNTAPPED)
            if mode == 'encode_hit' and owner == 0:
                # Named hand copy supplies only the identity for the real
                # encoded legacy's offered optional-priority action.
                self.add('anchor', self.record['name'], owner, 'hand')
        if self.csvid == SPIRAL:
            graves = (BODY, 'Forest', 'Plains')
            if mode == 'self_target':
                libraries[owner] = ('Forest',) * 6
                graves = ('Forest',) * 3
            for index, name in enumerate(graves):
                self.add(f'own_grave_{index}', name, owner, 'graveyard')
            if mode == 'live_grave':
                self.add('answer', CARRIER, owner, 'hand')
        else:
            self.add('own_grave', BLOCKER, owner, 'graveyard')
        self.add('foe_grave', 'Swamp', foe, 'graveyard')
        if self.csvid == SANITY:
            libraries[owner] = {'zero_blue': NO_BLUE, 'short_reveal': (HYBRID, BODY, CARRIER),
                                'empty_reveal': ()}.get(mode, CHROMA)
        for player in (0, 1):
            for index, name in enumerate(libraries[player]):
                self.add(f'library_{player}_{index}', name, player, 'library')

    def add(self, ref, name, player, zone, state=0):
        assert ref not in self.refs
        assert state == (UNTAPPED if zone == 'battlefield' else 0)
        entry = card(name, ref, player, zone)
        if state:
            entry['state'] = ['untapped']
        self.setup.append(entry)
        self.refs[ref] = (name, player)
        self.zones[player][zone].append(name)

    def emit(self, op, ref=None, other=None, prop=None, *, player=None, value=None, zone=None, index=None, color=None):
        fields = dict(ref=ref, other=other, property=prop, player=player,
                      value=value, zone=zone, index=index, color=color)
        self.steps.append(step(op, **{key: value for key, value in fields.items() if value is not None}))

    def transfer(self, ref, source, destination):
        name, player = self.refs[ref]
        self.zones[player][source].remove(name)
        if destination:
            self.zones[player][destination].append(name)

    def fund(self, player, pool):
        self.pools[player] = tuple(pool)
        for color, value in enumerate(pool):
            self.emit('set_mana', player=player, color=color, value=value)
        self.emit('refresh')

    def receipt(self, expected_stack=0):
        self.emit('assert_choices_consumed')
        self.emit('assert_stack', value=expected_stack)
        for player, pool in enumerate(self.pools):
            for color, value in enumerate(pool):
                self.emit('assert_mana', player=player, color=color, value=value)
            self.emit('assert_life', player=player, value=self.life[player])
            self.emit('assert_shuffle_count', player=player, value=self.shuffles[player])
            for zone in ZONES:
                # Physical hand slots can still include pending spells.
                if expected_stack and zone == 'hand':
                    continue
                names = self.zones[player][zone]
                self.emit('assert_zone', player=player, zone=zone, value=len(names))
                if zone == 'graveyard' or zone == 'library' and player not in self.order_unknown:
                    for index, name in enumerate(names):
                        self.emit('assert_zone_card', player=player, zone=zone, index=index,
                                  value=self.rows[name]['csvid'])
                else:
                    for name in sorted({name for name, _ in self.refs.values()}):
                        self.emit('assert_zone_multiplicity', player=player, zone=zone,
                                  index=self.rows[name]['csvid'], value=names.count(name))

    def cast(self, ref='source', *, target=None, response=False, fee=None, budget=None, x=None):
        name, actor = self.refs[ref]
        cost = tuple(cost_from_oracle(self.oracle[name])) if fee is None else fee
        self.fund(actor, cost if budget is None else budget)
        if x is not None:
            self.emit('choose_x', player=actor, value=x)
        if target is not None:
            self.emit('choose_player', player=actor, value=target)
        self.emit('cast_response' if response else 'cast', ref, value=1)
        self.pools[actor] = tuple(have-due for have, due in zip(self.pools[actor], cost))
        assert min(self.pools[actor]) >= 0
        self.transfer(ref, 'hand', None)
        self.receipt(2 if response else 1)

    def drain(self, response=False):
        self.emit('resolve_response' if response else 'resolve')
        self.emit('refresh')
        self.emit('assert_choices_consumed')
        self.emit('assert_stack', value=0)

    def finish(self, ref='source', zone='graveyard'):
        name, player = self.refs[ref]
        self.zones[player][zone].append(name)

    def mill(self, amount, *, player=None):
        player = self.target if player is None else player
        library = self.zones[player]['library']
        moved = library[:amount]
        del library[:amount]
        self.zones[player]['graveyard'].extend(moved)
        self.milled.extend(moved)
        return moved

    def burn(self):
        self.cast(target=self.target)
        take = {'seat0_accept': 0, 'seat1_accept': 1}.get(self.mode)
        for player in (0, 1):
            self.emit('choose_dialog', player=player, value=int(player != take))
            if player == take:
                self.life[player] -= 6
                break
        self.drain()
        if take is None:
            self.mill(6)
        self.finish()
        self.receipt()

    def cipher(self):
        self.cast(target=self.target)
        encoded = self.mode in ('encode_hit', 'encode_blocked', 'decline_copy')
        if 'host' in self.refs:
            self.emit('choose_dialog', player=self.owner, value=int(not encoded))
            if encoded:
                self.emit('choose_target', 'host', player=self.owner)
        self.drain()
        self.mill(3)
        self.finish(zone='exile' if encoded else 'graveyard')
        self.receipt()
        if encoded:
            self.cipher_combat()

    def cipher_combat(self):
        owner = self.owner
        blocked = self.mode == 'encode_blocked'
        copied = self.mode == 'encode_hit'
        self.emit('begin_attackers')
        self.emit('attack', 'host', value=1)
        self.emit('finish_attackers')
        self.emit('begin_blockers')
        if blocked:
            self.emit('block', 'blocker', 'host', value=1)
        self.emit('advance_combat')
        self.emit('assert_card', 'host', prop='blocked', value=int(blocked))
        if copied:
            if owner == 0:
                self.emit('choose_trigger', 'anchor', player=owner)
            self.emit('choose_player', player=owner, value=self.foe)
        self.emit('combat_damage')
        self.drain()
        self.emit('end_combat')
        if blocked:
            self.transfer('host', 'battlefield', 'graveyard')
            self.emit('assert_card', 'blocker', prop='damage', value=1)
        else:
            self.life[self.foe] -= 1
            self.emit('assert_card', 'host', prop='tapped', value=1)
        if copied:
            self.mill(3)
        self.receipt()

    def spiral(self):
        self.cast(target=self.target)
        response = self.mode == 'live_grave'
        if response:
            self.emit('announce_spell', 'source')
            self.cast('answer', response=True)
        self.drain(response=response)
        owner = self.zones[self.owner]
        if response:
            owner['hand'].append(owner['library'].pop(0))
            self.finish('answer')
        shuffled = list(owner['graveyard'])
        owner['graveyard'].clear()
        owner['library'].extend(shuffled)
        self.shuffles[self.owner] += 1
        if len(set(owner['library'])) > 1:
            assert self.target != self.owner
            self.order_unknown.add(self.owner)
        self.mill(len(shuffled))
        self.finish()
        self.receipt()

    def sanity(self):
        self.cast()  # Forced sole opponent, with no player-choice dialog.
        library = self.zones[self.owner]['library']
        seen = library[:10]
        self.revealed = list(seen)
        bottom = list(reversed(seen))
        self.bottom = list(bottom)
        if seen:
            # Information-only show_deck explicitly addresses HUMAN even
            # when seat 1 cast the spell. Ordering uses the actual caster.
            self.emit('choose_zone', player=0, value=-1)
            for name in bottom[:-1]:
                self.emit('choose_zone_card', player=self.owner, value=self.rows[name]['csvid'])
        self.drain()
        del library[:len(seen)]
        library.extend(bottom)
        self.mill(sum(blue_symbols(self.oracle[name]) for name in seen))
        self.finish()
        self.receipt()

    def play(self):
        active = self.foe if self.csvid == SPIRAL and self.mode in ('opposing', 'live_grave') else self.owner
        self.emit('turn', player=active)
        self.emit('present_ai_dialogs', value=1)
        self.fund(0, ZERO)
        self.fund(1, ZERO)
        self.receipt()
        {BOOK: self.burn, CIPHER: self.cipher, SPIRAL: self.spiral, SANITY: self.sanity}[self.csvid]()


class FollowerCase(MillCase):
    def build(self):
        own, foe = OWN_MIX, LIBRARY
        if self.mode == 'short_split':
            foe = foe[:2]
        if self.csvid == INDIGNATION:
            if self.mode == 'short_library':
                own = own[:2]
            elif self.mode == 'no_creatures':
                own = ('Forest', 'Plains', RELIC, BODY)
            elif self.mode == 'through_purity':
                self.add('purity', PURITY, self.foe, 'battlefield', UNTAPPED)
                self.add('damage_probe', SHOCK, self.owner, 'hand')
        for player, deck in ((self.owner, own), (self.foe, foe)):
            # Existing graveyard creatures never contribute to the printed rider.
            self.add(f'old_grave_{player}', BLOCKER, player, 'graveyard')
            for index, name in enumerate(deck):
                self.add(f'library_{player}_{index}', name, player, 'library')

    def trauma(self):
        own, foe = CONTRIBUTIONS[self.mode]
        owner_budget = list(cost_from_oracle(self.record))
        owner_budget[own[0]] += own[1]
        foe_budget = [0]*6
        foe_budget[foe[0]] = foe[1]
        self.fund(self.foe, foe_budget)
        self.cast(budget=tuple(owner_budget))
        total = 0
        for player, (color, available, contribution) in ((self.owner, own), (self.foe, foe)):
            self.emit('choose_x', player=player, value=contribution)
            have = list(self.pools[player])
            assert have[color] == available and available >= contribution
            have[color] -= contribution
            self.pools[player] = tuple(have)
            total += contribution
        self.drain()
        for player in (self.owner, self.foe):
            self.mill(total, player=player)
        self.finish()
        self.receipt()

    def indignation(self):
        if self.mode == 'through_purity':
            self.cast('damage_probe', target=self.foe)
            self.drain()
            self.finish('damage_probe')
            self.life[self.foe] += 2
            self.receipt()
        x = {'zero_x': 0, 'no_creatures': 3}.get(self.mode, 4)
        fee = (x, 1, 0, 0, 0, 0)
        self.cast(target=self.target, fee=fee, x=x)
        self.drain()
        moved = self.mill(x, player=self.owner)
        count = sum('Creature' in self.oracle[name]['type_line'].partition(' — ')[0].split() for name in moved)
        self.life[self.target] -= 2*count
        self.finish()
        self.receipt()

    def play(self):
        self.emit('turn', player=self.owner)
        self.emit('present_ai_dialogs', value=1)
        self.fund(0, ZERO)
        self.fund(1, ZERO)
        self.receipt()
        {TRAUMA: self.trauma, INDIGNATION: self.indignation}[self.csvid]()


@lru_cache(maxsize=1)
def profiles():
    return json.loads(PROFILE.read_bytes())


def classify(record, csvid):
    cost, type_line, clauses, common, human = MODELS[csvid]
    keywords = {'Mill'}
    optional_keyword = {CIPHER: 'Cipher', SANITY: 'Chroma', TRAUMA: 'Join forces'}.get(csvid)
    if optional_keyword:
        keywords.add(optional_keyword)
    if (record.get('oracle_text') != '\n'.join(clauses) or record.get('mana_cost') != cost
            or record.get('type_line') != type_line or record.get('layout') != 'normal'
            or record.get('card_faces') or record.get('selected_face') or set(record.get('keywords', ())) != keywords):
        raise ValueError('Exact whole mill spell and printed costs and riders required')
    return dict(csvid=csvid, printed_cost=cost, clauses=list(clauses), modes=list(common), human_modes=list(human))


def variants(model):
    return ([f'{mode}_{owner}' for mode in model['modes'] for owner in (0, 1)]
            + [f'{mode}_0' for mode in model['human_modes']])


def validate_witnesses(oracle, rows):
    pins = profiles()
    if (set(pins) != {'sources', 'witnesses'} or set(pins['witnesses']) != WITNESSES
            or {pin['registry']['csvid'] for pin in pins['sources'].values()} != set(MODELS)):
        raise ValueError('Exact library mill source and witness scope required')
    for group in pins.values():
        for name, pin in group.items():
            record, row = oracle.get(name, {}), rows.get(name, {})
            if (not exact(pin, dict(oracle=record, registry=row)) or record.get('layout') != 'normal'
                    or row.get('kind') != 'card' or row.get('handler') in (None, '0', 'NULL', 'card_dummy')):
                raise ValueError(f'Changed implemented mill source or witness: {name}')


def spec(declaration, record):
    pin = profiles()['sources'].get((record or {}).get('name'))
    if not pin or not exact(record, pin['oracle']):
        raise ValueError('Complete pinned mill-spell Oracle required')
    model = classify(record, pin['registry']['csvid'])
    selected = declaration.get('variants')
    if (declaration.get('family') != FAMILY or type(declaration.get('version')) is not int
            or declaration['version'] != 2 or declaration.get('oracle_text') != record['oracle_text']
            or not exact(declaration.get('params'), model) or not isinstance(selected, list) or not selected
            or any(type(v) is not str for v in selected) or len(selected) != len(set(selected))
            or set(selected) - set(variants(model))):
        raise ValueError('Exact mill clauses, costs, and scoped variants required')
    return dict(model, family=FAMILY)


def expand(name, csvid, declaration, model, selected, rows, oracle):
    validate_witnesses(oracle, rows)
    if rows[name]['csvid'] != csvid or model['csvid'] != csvid or set(selected) - set(variants(model)):
        raise ValueError('Exact mill primary and variants required')
    factory = FollowerCase if csvid in FOLLOWERS else MillCase
    gaps = {
        CIPHER: ['Optional encoded-copy decline is exercised on the human seat with the actual offered priority action'],
        SPIRAL: ['Shuffle distribution is unobserved; exact card multiset is asserted'],
    }.get(csvid, [])
    cases = []
    for variant in selected:
        mode, owner = variant.rsplit('_', 1)
        game = factory(oracle[name], csvid, int(owner), mode, rows, oracle)
        game.play()
        metadata = dict(family=FAMILY, version=2, variant=variant, params=declaration['params'],
                        oracle_text=declaration['oracle_text'], covered_clauses=model['clauses'],
                        untested_components=gaps)
        cases.append(dict(name=label(csvid, mode, int(owner)), card=name,
                          fixture='empty_main', seed=SEED, setup=game.setup, steps=game.steps, _recipe=metadata))
    return cases
