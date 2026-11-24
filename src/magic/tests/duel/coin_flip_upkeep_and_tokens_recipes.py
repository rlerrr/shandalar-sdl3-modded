"""Pinned Oracle upkeep flips, produced tokens, and seeded creature outcomes."""

from functools import lru_cache
import json
from pathlib import Path

from library_search_recipes import wave6_exact as exact
from next_upkeep_cantrip_additional_riders_recipes import xorshift
from recipes import card, cost_from_oracle, step
from seeded_coin_riders_recipes import COIN_SEED, actual_call


FAMILY = 'coin_flip_upkeep_and_tokens'
PROFILE = Path(__file__).parent / 'data' / f'{FAMILY}_wave8_profiles.json'
MINES = 'Land Mine'
BEAR = 'Grizzly Bears'
FLYER = 'Ornithopter'
COPY = 'Elvish Mystic'
PW = 'Garruk Wildspeaker'
EXEMPT = ('Lord of the Pit', 'Torch Fiend', 'Daggerclaw Imp')
MODELS = {
    2983: (
        ('At the beginning of your upkeep, you may flip a coin. If you win the flip, put a fuse counter on '
         'this enchantment. If you lose the flip, remove a fuse counter from this enchantment.',
         'Remove five fuse counters from this enchantment and sacrifice it: It deals 20 damage to target '
         'player or planeswalker.'),
        ('player', 'planeswalker', 'loss_recovers', 'human_declines')),
    14108: (
        ('At the beginning of your upkeep, create a colorless artifact token named Land Mine with '
         '"{R}, Sacrifice this token: This token deals 2 damage to target attacking creature without flying." '
         'Then flip a coin. If you lose the flip, this creature deals 2 damage to itself.',),
        ('win', 'lose', 'mine_ground', 'mine_flyer_only')),
    19678: (
        ("Whenever a nontoken creature you control enters, flip a coin until you lose a flip. For each flip you won, "
         "create a token that's a copy of that creature. Those tokens gain haste. Exile them at the beginning of "
         'the next end step.',),
        ('zero', 'one', 'two', 'opponent_enters', 'token_enters')),
    19703: (
        ('Flying, trample',
         "When Rakdos enters, flip a coin for each creature that isn't a Demon, Devil, or Imp. "
         'Destroy each creature whose coin comes up tails.'),
        ('both_tails', 'split', 'flying', 'trample')),
}

ZERO = (0,) * 6
STATIC_WITNESSES = {BEAR, FLYER}
TOKEN_WITNESSES = {MINES, 'Soldier'}
WITNESSES = {BEAR, FLYER, COPY, PW, *EXEMPT, 'Raise the Alarm', 'Forest', *TOKEN_WITNESSES,
             'Scion of Glaciers', 'Darksteel Myr', 'Disenchant'}
EXTRA = {2983: 'loss_at_zero', 14108: 'mine_nonlethal', 19678: 'source_gone',
         19703: 'indestructible_tails'}
GAPS = {
    2983: ['Fuse counters are exercised through the real five-counter activation threshold. '
           'The observer does not expose the raw fuse total.'],
    19678: ['The completion of the real end-step/cleanup transition is observable. '
            'Responses to the delayed exile trigger are untested.'],
}


def seed_for(*results):
    for seed in range(1, 1000):
        state = seed
        for expected in results:
            state = xorshift(state)
            if state % 2 != expected:
                break
        else:
            return seed
    raise ValueError('No seed for the reviewed coin sequence')


class CoinCase:
    def __init__(self, record, csvid, owner, mode, rows, oracle):
        self.record, self.csvid, self.owner, self.other, self.mode = record, csvid, owner, 1 - owner, mode
        self.seed = self.random_state = COIN_SEED
        self.rows, self.oracle = rows, oracle
        self.refs, self.setup, self.steps = {}, [], []
        self.current = self.owner
        self.coin_calls = []
        self.add('source', owner, record['name'], 'hand')

    def add(self, ref, player, name, zone='battlefield'):
        assert ref not in self.refs
        self.refs[ref] = (player, name)
        self.setup.append(card(name, ref, player, zone))

    def emit(self, op, ref=None, other=None, prop=None, *, player=None, value=None, zone=None,
             index=None, color=None):
        field = 'counter_kind' if op == 'assert_counter' else 'property'
        values = dict(ref=ref, other=other, player=player, value=value, zone=zone, index=index, color=color)
        if prop is not None:
            values[field] = prop
        self.steps.append(step(op, **{name: value for name, value in values.items() if value is not None}))

    def card(self, ref, prop, value):
        self.emit('assert_card', ref, prop=prop, value=value)

    def contains(self, player, zone, name, amount=1):
        self.emit('assert_zone_multiplicity', player=player, zone=zone, index=self.rows[name]['csvid'],
                  value=amount)

    def mana(self, op, player, amounts):
        for color, amount in enumerate(amounts):
            self.emit(op, player=player, color=color, value=amount)

    def stack(self, amount=0):
        self.emit('assert_stack', value=amount)
        self.emit('assert_choices_consumed')

    def begin_cast(self, ref, *, target=None):
        player, name = self.refs[ref]
        self.mana('set_mana', player, cost_from_oracle(self.oracle[name]))
        self.emit('refresh')
        if target is not None:
            self.emit('choose_target', target, player=player)
        self.emit('cast', ref, value=1)
        self.stack(1)
        self.mana('assert_mana', player, ZERO)

    def resolve(self):
        self.emit('resolve')
        self.emit('refresh')
        self.stack()

    def cast_source(self):
        self.begin_cast('source')
        self.resolve()
        self.live('source')

    def live(self, ref):
        player, name = self.refs[ref]
        self.card(ref, 'in_play', 1)
        self.contains(player, 'battlefield', name)
        record = self.oracle[name]
        if 'power' in record and 'toughness' in record:
            self.card(ref, 'power', int(record['power']))
            self.card(ref, 'toughness', int(record['toughness']))

    def gone(self, ref):
        player, name = self.refs[ref]
        self.contains(player, 'battlefield', name, 0)
        self.contains(player, 'graveyard', name)

    def life(self, p, amount):
        self.emit('assert_life', player=p, value=amount)

    def start(self):
        self.emit('turn', player=self.owner)
        for p in (0, 1):
            self.mana('set_mana', p, ZERO)
        self.life(self.owner, 20)
        self.life(self.other, 20)

    def advance(self, player):
        assert 1 - self.current == player
        self.current = player
        self.emit('advance_turn')

    def step_upkeep(self, player, *, before=None):
        self.advance(player)
        self.emit('untap_step', player=player)
        for seat in (0, 1):
            self.mana('assert_mana', seat, ZERO)
        if before is not None:
            before()
        self.emit('upkeep_step', player=player)
        self.stack()

    def call(self, win):
        self.random_state = xorshift(xorshift(self.random_state))
        result = self.random_state % 2
        choice = result if win else 1 - result
        if not self.coin_calls:
            assert choice == actual_call(win)
        self.coin_calls.append(dict(call=choice, observed_face=result, expected_win=win))
        self.emit('choose_dialog', player=self.owner, value=choice)

    def bind(self, ref, seat, name, ordinal=0):
        self.emit('bind_permanent_copy', ref, player=seat, value=self.rows[name]['csvid'], index=ordinal)

    def attackers(self, ref):
        self.emit('begin_attackers')
        self.emit('attack', ref, value=1)
        self.emit('finish_attackers')


class BombCase(CoinCase):
    def build(self):
        if self.mode == 'planeswalker':
            self.add('walker', self.other, PW)
        self.start()
        self.cast_source()
        self.emit('set_life', player=self.other, value=30)
        self.emit('can_activate', 'source', value=0)
        wins = [True] * 5
        if self.mode == 'loss_recovers':
            wins = [True] * 4 + [False, True, True]
        elif self.mode == 'loss_at_zero':
            wins = [False] + wins
        elif self.mode == 'human_declines':
            assert self.owner == 0
            wins = [True] * 4 + [None, True]
        fuse = 0
        for win in wins:
            self.step_upkeep(self.other)
            self.emit('can_activate', 'source', value=int(fuse >= 5))
            self.step_upkeep(self.owner, before=self.queue_flip(win))
            if win is not None:
                fuse = max(0, fuse + (1 if win else -1))
            self.emit('can_activate', 'source', value=int(fuse >= 5))
            self.life(self.other, 30)
        assert fuse == 5
        if self.mode == 'planeswalker':
            self.emit('assert_counter', 'walker', prop='loyalty', value=3)
            self.emit('choose_target', 'walker', player=self.owner)
        else:
            self.emit('choose_player', player=self.owner, value=self.other)
        self.emit('activate', 'source', value=1)
        self.stack(1)
        self.gone('source')
        self.mana('assert_mana', self.owner, ZERO)
        self.life(self.other, 30)
        self.resolve()
        self.gone('source')
        if self.mode == 'planeswalker':
            self.gone('walker')
            self.life(self.other, 30)
        else:
            self.life(self.other, 10)
        self.life(self.owner, 20)

    def queue_flip(self, win):
        def enqueue():
            if win is not None:
                if self.owner == 0:
                    self.emit('choose_trigger_source', 'source', player=self.owner)
                self.call(win)
        return enqueue


class KaboomistCase(CoinCase):
    def mine_receipt(self, exists):
        self.contains(self.owner, 'battlefield', MINES, int(exists))
        self.contains(self.other, 'battlefield', MINES, 0)
        for zone in ('hand', 'graveyard', 'exile'):
            for seat in (0, 1):
                self.contains(seat, zone, MINES, 0)
        if exists:
            for prop, value in (('token', 1), ('artifact', 1), ('color', 0)):
                self.emit('assert_matching_permanents', prop=prop, player=self.owner,
                          index=self.rows[MINES]['csvid'], value=value)

    def build(self):
        attackers = {'mine_ground': BEAR, 'mine_flyer_only': FLYER, 'mine_nonlethal': 'Scion of Glaciers'}
        if self.mode in attackers:
            self.add('attacker', self.other, attackers[self.mode])
        self.start()
        self.cast_source()
        self.step_upkeep(self.other)
        self.mine_receipt(False)
        wins = self.mode not in ('lose', 'mine_ground')
        self.step_upkeep(self.owner, before=lambda: self.call(wins))
        if wins:
            self.live('source')
        else:
            self.gone('source')
        self.mine_receipt(True)
        if self.mode in ('win', 'lose'):
            return
        self.bind('mine', self.owner, MINES)
        self.advance(self.other)
        self.mana('set_mana', self.owner, (0, 0, 0, 0, 1, 0))
        self.emit('can_activate', 'mine', value=0)
        self.attackers('attacker')
        self.card('attacker', 'attacking', 1)
        if self.mode == 'mine_flyer_only':
            self.emit('can_activate', 'mine', value=0)
            self.mana('assert_mana', self.owner, (0, 0, 0, 0, 1, 0))
            self.mine_receipt(True)
            return
        if self.mode == 'mine_nonlethal':
            # The attacker is now independently eligible. Generic/colorless
            # mana alone cannot pay the token's red activation cost.
            wrong_color = (1, 0, 0, 0, 0, 0)
            self.mana('set_mana', self.owner, wrong_color)
            self.emit('can_activate', 'mine', value=0)
            self.mana('assert_mana', self.owner, wrong_color)
            self.mine_receipt(True)
            self.mana('set_mana', self.owner, (0, 0, 0, 0, 1, 0))
        self.emit('can_activate', 'mine', value=1)
        self.emit('choose_target', 'attacker', player=self.owner)
        self.emit('activate', 'mine', value=1)
        self.stack(1)
        self.mana('assert_mana', self.owner, ZERO)
        self.mine_receipt(False)
        self.card('attacker', 'damage', 0)
        self.resolve()
        if self.mode == 'mine_nonlethal':
            self.live('attacker')
            self.card('attacker', 'damage', 2)
        else:
            self.gone('attacker')
        self.mine_receipt(False)
        self.life(self.owner, 20)


class MarchCase(CoinCase):
    def mystics(self, copies):
        self.contains(self.owner, 'battlefield', COPY, 1 + copies)
        for prop, value in (('power', 1), ('toughness', 1), ('color', 8), ('subtype_Elf', 1)):
            self.emit('assert_matching_permanents', prop=prop, player=self.owner,
                      index=self.rows[COPY]['csvid'], value=value)
        self.contains(self.other, 'battlefield', COPY, 0)

    def build(self):
        if self.mode == 'token_enters':
            self.add('entrant', self.owner, 'Raise the Alarm', 'hand')
        else:
            controller = self.other if self.mode == 'opponent_enters' else self.owner
            self.add('entrant', controller, COPY, 'hand')
        if self.mode == 'source_gone':
            self.add('removal', self.owner, 'Disenchant', 'hand')
        self.start()
        self.cast_source()
        if self.mode == 'opponent_enters':
            self.advance(self.other)
        self.begin_cast('entrant')
        if self.mode in ('opponent_enters', 'token_enters'):
            self.resolve()
            if self.mode == 'token_enters':
                self.contains(self.owner, 'battlefield', 'Soldier', 2)
                self.contains(self.other, 'battlefield', 'Soldier', 0)
            else:
                self.live('entrant')
                self.contains(self.owner, 'battlefield', COPY, 0)
            self.emit('cleanup')
            self.stack()
            return
        copies = {'zero': 0, 'one': 1, 'two': 2, 'source_gone': 2}[self.mode]
        for win in [True] * copies + [False]:
            self.call(win)
        self.resolve()
        self.mystics(copies)
        if self.mode == 'source_gone':
            self.begin_cast('removal', target='source')
            self.resolve()
            self.gone('source')
            self.contains(self.owner, 'graveyard', 'Disenchant')
            self.mystics(copies)
        if not copies:
            self.emit('cleanup')
            self.stack()
            self.mystics(0)
            return
        self.emit('begin_attackers')
        self.emit('attack', 'entrant', value=0)
        self.bind('copy', self.owner, COPY, 1)
        self.emit('attack', 'copy', value=1)
        self.emit('finish_attackers')
        self.emit('begin_blockers')
        self.emit('advance_combat')
        self.emit('combat_damage')
        self.resolve()
        self.life(self.other, 19)
        self.emit('end_combat')
        self.mystics(copies)
        self.emit('cleanup')
        self.stack()
        self.mystics(0)
        self.contains(self.owner, 'graveyard', COPY, 0)
        self.contains(self.owner, 'exile', COPY, 0)


class RakdosCase(CoinCase):
    def build(self):
        indestructible = self.mode == 'indestructible_tails'
        sweep = self.mode in ('both_tails', 'split', 'indestructible_tails')
        if sweep:
            self.add('own', self.owner, COPY)
            self.add('foe', self.other, 'Darksteel Myr' if indestructible else BEAR)
            for number, name in enumerate(EXEMPT):
                self.add(f'exempt_{number}', (self.owner + number) % 2, name)
            self.add('land', self.other, 'Forest')
            outcomes = (0, 1) if self.mode == 'split' else (1, 1)
        else:
            self.add('foe', self.other, BEAR if self.mode == 'flying' else FLYER)
            outcomes = (0,)
        self.seed = seed_for(*outcomes)
        for face in outcomes:
            self.coin_calls.append(dict(observed_face=face, expected_destroyed=bool(face)))
        self.start()
        self.cast_source()
        if sweep:
            for ref, tail in zip(('own', 'foe'), outcomes):
                if tail and not (indestructible and ref == 'foe'):
                    self.gone(ref)
                else:
                    self.live(ref)
            for i in range(len(EXEMPT)):
                self.live(f'exempt_{i}')
            self.live('land')
            self.life(self.owner, 20)
            self.life(self.other, 20)
            return
        self.live('foe')
        self.advance(self.other)
        self.advance(self.owner)
        self.attackers('source')
        self.emit('begin_blockers')
        self.emit('block', 'foe', 'source', value=int(self.mode == 'trample'))
        self.emit('advance_combat')
        self.card('source', 'blocked', int(self.mode == 'trample'))
        self.emit('combat_damage')
        self.resolve()
        if self.mode == 'trample':
            self.gone('foe')
            self.life(self.other, 16)
        else:
            self.live('foe')
            self.life(self.other, 14)
        self.emit('end_combat')
        self.life(self.owner, 20)



BOARDS = {2983: BombCase, 14108: KaboomistCase, 19678: MarchCase, 19703: RakdosCase}


@lru_cache(maxsize=1)
def profiles():
    return json.loads(PROFILE.read_bytes())


def classify(record, csvid):
    clauses, modes = MODELS[csvid]
    keywords = ['Flying', 'Trample'] if csvid == 19703 else []
    if (record['oracle_text'] != '\n'.join(clauses) or record.get('layout') != 'normal'
            or record.get('keywords') != keywords or record.get('card_faces') or record.get('selected_face')):
        raise ValueError('Exact whole upkeep and token coin-flip source required')
    return dict(csvid=csvid, price=cost_from_oracle(record), clauses=list(clauses),
                modes=[*modes, EXTRA[csvid]])


def variants(model):
    return [f'{mode}_{seat}' for mode in model['modes']
            for seat in ((0,) if mode == 'human_declines' else (0, 1))]


def validate_witnesses(oracle, rows):
    pins = profiles()
    if (set(pins) != {'sources', 'witnesses'} or set(pins['witnesses']) != WITNESSES
            or {pin['registry']['csvid'] for pin in pins['sources'].values()} != set(MODELS)):
        raise ValueError('Exact coin-flip upkeep and token source and witness scope required')
    for group in pins.values():
        for name, pin in group.items():
            record, row = oracle.get(name, {}), rows.get(name, {})
            if (not exact(pin, {'oracle': record, 'registry': row})
                    or record.get('layout') != ('token' if name in TOKEN_WITNESSES else 'normal')
                    or row.get('kind') != 'card' or row.get('handler') in (None, '0', 'NULL')
                    or (row['handler'] == 'card_dummy') != (name in STATIC_WITNESSES)):
                raise ValueError(f'Pinned coin-flip upkeep and token source or witness changed: {name}')


def spec(declaration, record):
    pin = profiles()['sources'].get((record or {}).get('name'))
    if not pin or not exact(record, pin['oracle']):
        raise ValueError('Complete pinned coin-flip upkeep and token primary required')
    model = classify(record, pin['registry']['csvid'])
    picked = declaration.get('variants')
    if (declaration.get('family') != FAMILY or type(declaration.get('version')) is not int
            or declaration['version'] != 2 or declaration.get('oracle_text') != record['oracle_text']
            or not exact(declaration.get('params'), model) or not isinstance(picked, list) or not picked
            or any(type(v) is not str for v in picked) or len(picked) != len(set(picked))
            or set(picked) - set(variants(model))):
        raise ValueError('Exact coin-flip upkeep and token clauses, costs, and scoped variants required')
    return dict(model, family=FAMILY)


def expand(name, csvid, declaration, model, selected, rows, oracle):
    validate_witnesses(oracle, rows)
    if rows[name]['csvid'] != csvid or model['csvid'] != csvid or set(selected) - set(variants(model)):
        raise ValueError('Exact coin-flip upkeep and token primary and variants required')
    cases = []
    for variant in selected:
        mode, seat = variant.rsplit('_', 1)
        game = BOARDS[csvid](oracle[name], csvid, int(seat), mode, rows, oracle)
        game.build()
        metadata = dict(family=FAMILY, version=2, variant=variant, params=declaration['params'],
                        oracle_text=declaration['oracle_text'], covered_clauses=model['clauses'],
                        untested_components=GAPS.get(csvid, []))
        cases.append(dict(name=f'CoinFlipUpkeepTokensRecipe_{csvid}_{variant}', card=name,
                          fixture='empty_main', seed=game.seed, setup=game.setup,
                          steps=game.steps, _recipe=metadata))
    return cases
