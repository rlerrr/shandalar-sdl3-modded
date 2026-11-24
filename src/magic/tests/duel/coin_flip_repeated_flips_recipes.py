"""Repeated coin games with explicit, production-driven choices and payoffs."""

from collections import Counter
from functools import lru_cache
import json
from pathlib import Path

from counter_condition_recurrence_recipes import exact
from next_upkeep_cantrip_additional_riders_recipes import COIN_SEED, xorshift

FAMILY = 'coin_flip_repeated_flips'
PROFILE = Path(__file__).with_name('data') / 'coin_flip_repeated_flips_wave8_profiles.json'
GAMBIT = 'Fiery Gambit'
CLASH = 'Mana Clash'
REVENGE = "Squee's Revenge"
THUMB = "Krark's Thumb"
GAMBIT_PATHS = {
    'lose_first': (False,),
    'stop_one': (True,),
    'stop_two': (True, True),
    'stop_three': (True, True, True),
    'lose_after_two': (True, True, False),
    'lose_after_three': (True, True, True, False),
}
CARDS = {
    6664: (GAMBIT, 'Flip a coin until you lose a flip or choose to stop flipping. If you lose a flip, '
           'Fiery Gambit has no effect. If you win one or more flips, Fiery Gambit deals 3 damage to target '
           'creature. If you win two or more flips, Fiery Gambit deals 6 damage to each opponent. If you '
           'win three or more flips, draw nine cards and untap all lands you control.', (*GAMBIT_PATHS, 'target_fizzle')),
    343: (CLASH, "You and target opponent each flip a coin. Mana Clash deals 1 damage to each player whose "
          "coin comes up tails. Repeat this process until both players' coins come up heads on the same flip.",
          ('immediate', 'repeat', 'asymmetric', 'no_target', 'armor')),
    5437: (REVENGE, 'Choose a number. Flip a coin that many times or until you lose a flip, whichever comes '
           'first. If you win all the flips, draw two cards for each flip.', ('win_two', 'later_loss', 'zero', 'win_three')),
    6693: (THUMB, 'If you would flip a coin, instead flip two coins and ignore one.',
           ('keep_second_win', 'keep_first_loss', 'foe_not_replaced', 'target_owns_thumb', 'keep_twice')),
}
CLASH_SEEDS = {'immediate': 2, 'repeat': COIN_SEED, 'asymmetric': 24, 'no_target': 24, 'armor': 24}
ZONES = ('hand', 'battlefield', 'library', 'graveyard')
START = [0] * 6
LIBRARY = ('Island', 'Swamp', 'Forest', 'Plains', 'Mountain', 'Forest', 'Swamp', 'Island', 'Plains',
           'Forest', 'Mountain', 'Swamp')
BEAR = 'Grizzly Bears'
ORB = 'Witchbane Orb'
ARMOR = "Urza's Armor"
BOUNCE = 'Unsummon'
WITNESSES = {BEAR, ORB, ARMOR, BOUNCE, CLASH, REVENGE, *LIBRARY}


@lru_cache(maxsize=1)
def profiles():
    return json.loads(PROFILE.read_bytes())


def classify(record, csvid):
    if (csvid not in CARDS or record.get('layout') != 'normal' or record.get('card_faces')
            or record.get('selected_face')):
        raise ValueError('whole normal repeated-flip card required')
    name, text, _ = CARDS[csvid]
    if record.get('name') != name or record.get('oracle_text') != text or record.get('keywords') != []:
        raise ValueError('exact complete coin sequence and riders required')
    from recipes import cost_from_oracle

    return dict(csvid=csvid, casting_cost=cost_from_oracle(record), rules=text)


def variants(shape):
    return [f'{mode}_{actor}' for mode in CARDS[shape['csvid']][2] for actor in (0, 1)]


def validate_variants(shape, selected):
    if (type(selected) is not list or not selected or any(type(mode) is not str for mode in selected)
            or len(set(selected)) != len(selected) or set(selected) - set(variants(shape))):
        raise ValueError('distinct supported repeated-coin scenarios required')


def spec(declaration, record):
    pin = profiles()['sources'].get(record.get('name'))
    if not pin or not exact(pin['oracle'], record):
        raise ValueError('exact pinned repeated-flip primary required')
    shape = classify(record, pin['registry']['csvid'])
    if (declaration.get('family') != FAMILY or type(declaration.get('version')) is not int
            or declaration['version'] != 2 or declaration.get('oracle_text') != record['oracle_text']
            or not exact(declaration.get('params'), shape)):
        raise ValueError('versioned whole repeated-flip declaration required')
    validate_variants(shape, declaration.get('variants'))
    return dict(shape, family=FAMILY)


def validate_witnesses(oracle, rows):
    pins = profiles()
    if set(pins) != {'sources', 'witnesses'} or set(pins['witnesses']) != WITNESSES:
        raise ValueError('exact independent repeated-flip witnesses required')
    if {pin['registry']['csvid'] for pin in pins['sources'].values()} != set(CARDS):
        raise ValueError('whole repeated-flip cohort required')
    for group, entries in pins.items():
        for name, pin in entries.items():
            row = rows.get(name, {})
            if (not exact(oracle.get(name), pin['oracle']) or not exact(row, pin['registry'])
                    or row.get('kind') != 'card' or oracle[name].get('layout') != 'normal'):
                raise ValueError('repeated-flip source or witness changed: ' + name)
            if group == 'sources':
                if not str(row.get('handler', '')).startswith('manalinkeh_card_'):
                    raise ValueError('implemented repeated-flip primary required')
                classify(oracle[name], row['csvid'])
    if (oracle[ARMOR]['oracle_text'] != 'If a source would deal damage to you, prevent 1 of that damage.'
            or oracle[BOUNCE]['oracle_text'] != "Return target creature to its owner's hand."
            or not oracle[ORB]['oracle_text'].splitlines()[-1].startswith('You have hexproof.')):
        raise ValueError('exact prevention, target movement and hexproof witnesses required')


class Coins:
    """Predict the real native/Wasm host samples without setting their outcomes."""

    def __init__(self, seed):
        self.state = seed
        self.calls = []

    def bit(self):
        self.state = xorshift(self.state)
        return self.state % 2

    def called(self, win, keep=None):
        self.bit()  # The actual default Heads/Tails option consumes one sample.
        bits = [self.bit() for _ in range(1 if keep is None else 2)]
        actual = bits[0 if keep is None else keep]
        call = actual if win else 1 - actual
        self.calls.append(dict(results=bits, call=call, kept=keep))
        return call


class Duel:
    def __init__(self, csvid, player, mode, rows, oracle):
        self.csvid, self.name = csvid, CARDS[csvid][0]
        self.player, self.enemy, self.mode, self.rows, self.oracle = player, 1 - player, mode, rows, oracle
        self.active = player
        self.seed = CLASH_SEEDS.get(mode, COIN_SEED)
        self.coins = Coins(self.seed)
        self.setup, self.steps, self.cards = [], [], {}
        self.life = [20, 20]
        self.draws = [0, 0]
        self.rounds = []
        self.add('source', player, self.name, 'hand')
        for seat in (0, 1):
            for i, name in enumerate(LIBRARY):
                self.add(f'library{seat}_{i}', seat, name, 'library')
        if csvid == 6664:
            for i in range(3):
                self.add(f'mana{i}', player, 'Mountain', 'battlefield')
            self.add('spare', player, 'Plains', 'battlefield', state=1)
            self.add('foe_land', self.enemy, 'Forest', 'battlefield', state=1)
            self.add('my_creature', player, BEAR, 'battlefield', state=1)
            self.add('target', self.enemy, BEAR, 'battlefield')
            if mode == 'target_fizzle':
                self.add('bounce', self.enemy, BOUNCE, 'hand')
        if csvid == 343 and mode == 'no_target':
            self.add('orb', self.enemy, ORB, 'battlefield')
        if csvid == 343 and mode == 'armor':
            self.add('armor', player, ARMOR, 'battlefield')
        if csvid == 6693:
            foe_casts = mode in ('foe_not_replaced', 'target_owns_thumb')
            self.add('witness', self.enemy if foe_casts else player,
                     CLASH if mode == 'target_owns_thumb' else REVENGE, 'hand')

    def add(self, ref, player, name, zone, state=0):
        from recipes import card

        self.cards[ref] = [player, name, zone]
        entity = card(name, ref, player, zone)
        if state:
            if state != 1:
                raise ValueError('unsupported repeated-flip setup state')
            entity['state'] = ['tapped']
        self.setup.append(entity)

    def emit(self, op, ref='', other='', prop='', *, player=0, value=0, zone=0, index=0, color=0):
        from recipes import step

        if op == 'untap_step':
            self.steps.append(step(op, player=player))
            return
        fields = dict(player=player, value=value, index=index, color=color)
        if ref:
            fields['ref'] = ref
        if other:
            fields['other'] = other
        if prop:
            fields['property'] = prop
        if op.startswith('assert_zone'):
            fields['zone'] = ZONES[zone]
        self.steps.append(step(op, **fields))

    def move(self, ref, zone):
        self.cards[ref][2] = zone

    def draw(self, player, count):
        for index in range(self.draws[player], self.draws[player] + count):
            self.move(f'library{player}_{index}', 'hand')
        self.draws[player] += count

    def pool(self, op, player, mana):
        for color, amount in enumerate(mana):
            self.emit(op, player=player, color=color, value=amount)

    def empty_pools(self):
        for seat in (0, 1):
            self.pool('assert_mana', seat, START)

    def receipt(self):
        self.emit('assert_stack', value=0)
        for player in (0, 1):
            self.emit('assert_life', player=player, value=self.life[player])
            names = sorted({name for owner, name, _ in self.cards.values() if owner == player})
            for zone_number, zone in enumerate(ZONES):
                contents = [name for owner, name, where in self.cards.values()
                            if (owner, where) == (player, zone)]
                self.emit('assert_zone', player=player, zone=zone_number, value=len(contents))
                count = Counter(contents)
                for name in names:
                    self.emit('assert_zone_multiplicity', player=player, zone=zone_number,
                              index=self.rows[name]['csvid'], value=count[name])
                if zone == 'library':
                    for i, name in enumerate(contents):
                        self.emit('assert_zone_card', player=player, zone=zone_number, index=i,
                                  value=self.rows[name]['csvid'])

    def card(self, ref, prop, value):
        self.emit('assert_card', ref, prop=prop, value=value)

    def cast(self, ref, *, funded=False, target=None):
        from recipes import cost_from_oracle

        caster, name, zone = self.cards[ref]
        assert caster == self.active and zone == 'hand'
        if not funded:
            self.pool('set_mana', caster, cost_from_oracle(self.oracle[name]))
        self.emit('refresh')
        # The legend-only Thumb handler omits the raw EVENT_CAN_CAST query.
        # The actual artifact cast and its exact cost/stack/entry stay checked.
        if name != THUMB:
            self.emit('can_cast', ref, value=1)
        if name == GAMBIT and caster == 0:
            # The colored requirement is paid automatically before the real
            # prompt for {2}. Use two single clicks on the produced red mana.
            for _ in range(2):
                self.emit('choose_mana', player=caster, color=4)
        if target is not None:
            self.emit('choose_target', target, player=caster)
        self.emit('cast', ref, value=1)
        self.emit('assert_choices_consumed')
        self.emit('assert_stack', value=1)
        self.empty_pools()

    def finish(self, ref, zone='graveyard', observe_all_flips=False):
        self.emit('resolve')
        self.emit('refresh')
        self.emit('assert_unused_choices' if observe_all_flips else 'assert_choices_consumed', value=0)
        self.move(ref, zone)
        self.empty_pools()

    def call(self, player, wins, keep=None):
        call = self.coins.called(wins, keep)
        self.emit('choose_dialog', player=player, value=call)
        if keep is not None:
            self.emit('choose_dialog', player=player, value=keep)

    def fund_gambit(self):
        for i in range(3):
            ref = f'mana{i}'
            self.card(ref, 'tapped', 0)
            self.emit('can_activate', ref, value=1)
            self.emit('activate', ref, value=1)
            self.emit('assert_choices_consumed')
            self.emit('assert_stack', value=0)
            self.card(ref, 'tapped', 1)
            self.pool('assert_mana', self.player, [0, 0, 0, 0, i + 1, 0])

    def gambit(self):
        self.fund_gambit()
        self.cast('source', funded=True, target='target')
        if self.mode == 'target_fizzle':
            self.gambit_fizzle()
            return
        results = GAMBIT_PATHS[self.mode]
        for i, win in enumerate(results):
            self.call(self.player, win)
            if win:
                self.emit('choose_dialog', player=self.player, value=int(i + 1 == len(results)))
        self.finish('source', observe_all_flips=self.mode == 'lose_after_three')
        wins = len(results) if all(results) else 0
        if wins:
            self.move('target', 'graveyard')
        else:
            self.card('target', 'damage', 0)
        if wins >= 2:
            self.life[self.enemy] -= 6
        if wins >= 3:
            self.draw(self.player, 9)
        for ref in ('mana0', 'mana1', 'mana2', 'spare'):
            self.card(ref, 'tapped', int(wins < 3))
        self.card('foe_land', 'tapped', 1)
        self.card('my_creature', 'tapped', 1)
        self.card('my_creature', 'damage', 0)
        self.receipt()

    def gambit_fizzle(self):
        from recipes import cost_from_oracle

        self.emit('announce_spell', 'source')
        self.pool('set_mana', self.enemy, cost_from_oracle(self.oracle[BOUNCE]))
        self.emit('refresh')
        self.emit('choose_target', 'target', player=self.enemy)
        self.emit('cast_response', 'bounce', value=1)
        self.emit('assert_stack', value=2)
        self.emit('assert_choices_consumed')
        self.emit('announce_spell', 'bounce')
        self.emit('resolve_response')
        self.emit('refresh')
        self.emit('assert_choices_consumed')
        self.move('bounce', 'graveyard')
        self.move('target', 'hand')
        self.move('source', 'graveyard')
        self.empty_pools()
        for ref in ('mana0', 'mana1', 'mana2', 'spare', 'foe_land', 'my_creature'):
            self.card(ref, 'tapped', 1)
        self.receipt()

    def normal_clash(self):
        for _ in range(12):
            owner, target = self.coins.bit(), self.coins.bit()
            self.rounds.append((owner, target))
            if self.mode != 'armor':
                self.life[self.player] -= owner
            self.life[self.enemy] -= target
            if (owner, target) == (0, 0):
                break
        assert self.rounds[-1] == (0, 0) and min(self.life) >= 15
        self.finish('source')
        self.receipt()

    def clash(self):
        from recipes import cost_from_oracle

        if self.mode == 'no_target':
            cost = cost_from_oracle(self.oracle[CLASH])
            self.pool('set_mana', self.player, cost)
            self.emit('refresh')
            self.emit('can_cast', 'source', value=0)
            self.emit('cast', 'source', value=0)
            self.emit('assert_choices_consumed')
            self.pool('assert_mana', self.player, cost)
            self.receipt()
            return
        # The generic opponent-only cast path stores its actual sole target.
        self.cast('source')
        self.normal_clash()

    def revenge(self, ref, amount, results, *, keep=None):
        caster = self.cards[ref][0]
        self.cast(ref)
        self.emit('choose_number', player=caster, value=amount)
        for win in results:
            self.call(caster, win, keep)
        self.finish(ref)
        if all(results):
            assert len(results) == amount
            self.draw(caster, amount * 2)
        self.receipt()

    def thumb(self):
        self.cast('source')
        self.finish('source', zone='battlefield')
        self.card('source', 'in_play', 1)
        self.receipt()
        if self.mode in ('foe_not_replaced', 'target_owns_thumb'):
            self.emit('advance_turn')
            self.active = self.enemy
            self.emit('untap_step', player=self.active)
            self.emit('turn', player=self.active, value=0)
        if self.mode == 'foe_not_replaced':
            self.revenge('witness', 1, (True,))
        elif self.mode == 'target_owns_thumb':
            self.cast('witness')
            # The actual flipper owns Thumb, even when the opponent cast Mana
            # Clash. Pick heads each time they are available for the owner.
            for _ in range(6):
                caster = self.coins.bit()
                alternatives = [self.coins.bit(), self.coins.bit()]
                keep = alternatives.index(min(alternatives))
                target = alternatives[keep]
                self.coins.calls.append(dict(results=alternatives, call=None, kept=keep))
                self.emit('choose_dialog', player=self.player, value=keep)
                self.rounds.append((caster, target))
                self.life[self.enemy] -= caster
                self.life[self.player] -= target
                if (caster, target) == (0, 0):
                    break
            assert self.rounds == [(1, 0), (0, 0)]
            self.finish('witness', observe_all_flips=True)
            self.receipt()
        else:
            second = self.mode != 'keep_first_loss'
            amount = 2 if self.mode == 'keep_twice' else 1
            self.revenge('witness', amount, (second,) * amount, keep=int(second))
            assert self.coins.calls == [dict(results=[1, 0], call=0, kept=int(second))] * amount

    def build(self):
        self.emit('turn', player=self.player, value=0)
        for player in (0, 1):
            self.pool('set_mana', player, START)
        if self.csvid == 6664:
            self.gambit()
        elif self.csvid == 343:
            self.clash()
        elif self.csvid == 5437:
            amount, results = {'win_two': (2, (True, True)), 'later_loss': (3, (True, False)),
                               'zero': (0, ()), 'win_three': (3, (True, True, True))}[self.mode]
            self.revenge('source', amount, results)
        else:
            self.thumb()


def expand(name, csvid, declaration, shape, selected, rows, oracle):
    validate_variants(shape, selected)
    validate_witnesses(oracle, rows)
    if profiles()['sources'][name]['registry']['csvid'] != csvid:
        raise ValueError('exact registered repeated-flip primary CSVID required')
    output = []
    for variant in selected:
        mode, actor = variant.rsplit('_', 1)
        game = Duel(csvid, int(actor), mode, rows, oracle)
        game.build()
        output.append(dict(
            name=f'CoinFlipRepeatedFlipsRecipe_{csvid}_{variant}', card=name, fixture='empty_main',
            seed=game.seed, setup=game.setup, steps=game.steps,
            _recipe=dict(family=FAMILY, version=2, variant=variant, params=declaration['params'],
                         oracle_text=declaration['oracle_text'], untested_components=[]),
        ))
    return output



