"""Coin-flip Partner payoffs, seeded Ral turns and Frenetic entry controls."""

from collections import Counter
from functools import lru_cache
import json
from pathlib import Path

from counter_condition_recurrence_recipes import exact
from recipes import card, cost_from_oracle, step
from seeded_coin_riders_recipes import COIN_SEED, actual_call

FAMILY = 'coin_flip_partner_and_recurring'
PROFILE = Path(__file__).with_name('data') / 'coin_flip_partner_and_recurring_wave8_profiles.json'
OKAUN, ZNDRSPLT, RAL, FRENETIC = (18618, 18656, 13201, 8782)
PARTNERS = {OKAUN: 'Zndrsplt, Eye of Wisdom', ZNDRSPLT: 'Okaun, Eye of Chaos'}
RULES = {
    OKAUN: "Partner with Zndrsplt, Eye of Wisdom (When this creature enters, target player may put Zndrsplt into their hand from their library, then shuffle.)\nAt the beginning of combat on your turn, flip a coin until you lose a flip.\nWhenever a player wins a coin flip, double Okaun's power and toughness until end of turn.",
    ZNDRSPLT: "Partner with Okaun, Eye of Chaos (When this creature enters, target player may put Okaun into their hand from their library, then shuffle.)\nAt the beginning of combat on your turn, flip a coin until you lose a flip.\nWhenever a player wins a coin flip, draw a card.",
    RAL: '+1: Tap target permanent, then untap another target permanent.\n−2: Ral Zarek deals 3 damage to any target.\n−7: Flip five coins. Take an extra turn after this one for each coin that comes up heads.',
    FRENETIC: 'All Slivers have "{0}: If this permanent is on the battlefield, flip a coin. If you win the flip, exile this permanent and return it to the battlefield under its owner\'s control at the beginning of the next end step. If you lose the flip, sacrifice it."',
}
PAYOFFS = ('external_win', 'external_loss', 'opponent_library', 'own_flip_win', 'fail_to_find')
MODES = {
    OKAUN: PAYOFFS,
    ZNDRSPLT: PAYOFFS,
    RAL: ('plus', 'bolt_player', 'bolt_creature', 'bolt_walker', 'two_heads', 'zero_heads', 'first_target_lost'),
    FRENETIC: ('bounce_recast', 'wrong_color', 'countered'),
}
RAL_SEEDS = {
    'two_heads': (7212, (0, 1, 0, 1, 1, 0, 1, 0, 1, 1, 0, 1, 0, 1), 2),
    'zero_heads': (10928, (1,) * 14, 0),
}
ZONE = dict(hand=0, battlefield=1, library=2, graveyard=3, exile=4)
ZERO = (0,) * 6
WITNESSES = {
    *PARTNERS.values(), 'Forest', 'Scoria Wurm', 'Wastes', 'Grizzly Bears',
    'Ajani Goldmane', 'Unsummon', 'Metallic Sliver', 'Counterspell',
}
PARTNER_GAP = ('The counterpart search and external actual flip payoffs are observed. The own '
               'beginning-of-combat loop listens for EVENT_BEGIN_COMBAT, whereas this bounded '
               'declaration path and the production main dispatcher do not expose that event.')
FRENETIC_GAP = ('The cast, real zone changes, and countered entry are observable. The shared '
                'zero-cost Sliver action is provided by a hidden TYPE_EFFECT activation source. '
                'The normal binders exclude that source, leaving its win/owner-return and loss '
                'sacrifice clauses outside these fixtures.')




@lru_cache(maxsize=1)
def profiles():
    return json.loads(PROFILE.read_bytes())


def classify(record, csvid):
    if (csvid not in RULES or record.get('layout') != 'normal' or record.get('selected_face')
            or record.get('card_faces') or record.get('oracle_text') != RULES[csvid]):
        raise ValueError('complete normal coin and Partner Oracle record required')
    return dict(csvid=csvid, mana=cost_from_oracle(record))


def variants(shape):
    return [f'{mode}_{owner}' for mode in MODES[shape['csvid']] for owner in (0, 1)]


def validate_variants(shape, selected):
    if (type(selected) is not list or not selected or any(type(v) is not str for v in selected)
            or len(set(selected)) != len(selected) or set(selected) - set(variants(shape))):
        raise ValueError('explicit distinct supported Partner and coin variants required')


def spec(declaration, record):
    pin = profiles()['sources'].get(record.get('name'))
    if not pin or not exact(pin['oracle'], record):
        raise ValueError('exact complete Oracle record required')
    shape = classify(record, pin['registry']['csvid'])
    if (declaration.get('family') != FAMILY or type(declaration.get('version')) is not int
            or declaration['version'] != 2 or declaration.get('oracle_text') != record['oracle_text']
            or not exact(declaration.get('params'), shape)):
        raise ValueError('whole versioned Partner and coin declaration required')
    validate_variants(shape, declaration.get('variants'))
    return dict(shape, family=FAMILY)


def validate_witnesses(oracle, rows):
    pins = profiles()
    if set(pins) != {'sources', 'witnesses'} or set(pins['witnesses']) != WITNESSES:
        raise ValueError('complete independent Partner and coin witnesses required')
    for group, entries in pins.items():
        for name, pin in entries.items():
            row = rows.get(name, {})
            if (not exact(oracle.get(name), pin['oracle']) or not exact(row, pin['registry'])
                    or row.get('kind') != 'card' or row.get('handler') in (None, 'NULL', '0')):
                raise ValueError('exact source or witness changed: ' + name)
            if group == 'sources':
                if row['handler'] == 'card_dummy':
                    raise ValueError('implemented Partner and coin primary required')
                classify(oracle[name], row['csvid'])
    if {name: rows[name]['csvid'] for name in PARTNERS.values()} != {
            'Okaun, Eye of Chaos': 18618, 'Zndrsplt, Eye of Wisdom': 18656}:
        raise ValueError('exact paired registry identities required')
    if oracle['Scoria Wurm']['oracle_text'] != ("At the beginning of your upkeep, flip a coin. "
                                                "If you lose the flip, return this creature to its owner's hand."):
        raise ValueError('one real independently observed upkeep coin flip required')


class Game:
    def __init__(self, row, owner, mode, rows, oracle):
        self.row, self.owner, self.foe = row, owner, 1 - owner
        self.source, self.mode, self.rows, self.oracle = row['csvid'], mode, rows, oracle
        self.active = owner
        self.seed = RAL_SEEDS.get(mode, (COIN_SEED,))[0]
        self.setup, self.steps, self.cards = [], [], {}
        self.life, self.shuffles = [20, 20], [0, 0]
        self.library = {p: [] for p in (0, 1)}
        self.loyalty, self.tapped = {}, {}
        self.add('source', row['name'], owner, 'hand')
        if self.source in PARTNERS:
            partner = PARTNERS[self.source]
            person = self.foe if mode == 'opponent_library' else self.owner
            self.lib(person, (partner, 'Forest'))
            if mode != 'opponent_library':
                coin_player = self.owner if mode == 'own_flip_win' else self.foe
                self.add('coin_witness', 'Scoria Wurm', coin_player, 'battlefield')
        elif self.source == RAL:
            self.add('owner_land', 'Wastes', owner, 'battlefield')
            self.add('victim', 'Grizzly Bears', self.foe, 'battlefield')
            self.tapped = {'owner_land': 0, 'victim': 0}
            if mode == 'bolt_walker':
                self.add('walker', 'Ajani Goldmane', self.foe, 'hand')
            if mode == 'first_target_lost':
                self.add('response', 'Unsummon', self.foe, 'hand')
        elif mode == 'countered':
            self.add('counter', 'Counterspell', self.foe, 'hand')
        elif mode == 'bounce_recast':
            self.add('bounce', 'Unsummon', self.foe, 'hand')
            self.add('own_sliver', 'Metallic Sliver', self.owner, 'battlefield')
            self.add('foe_sliver', 'Metallic Sliver', self.foe, 'battlefield')

    def add(self, ref, name, player, zone):
        self.setup.append(card(name, ref, player, zone))
        self.cards[ref] = dict(name=name, player=player, zone=zone)

    def lib(self, player, names):
        for i, name in enumerate(names):
            self.add(f'lib_{player}_{i}', name, player, 'library')
            self.library[player].append(name)

    def emit(self, op, ref='', *, player=0, value=0, zone=0, index=0, color=0, prop=''):
        if op == 'assert_counter':
            self.steps.append(step(op, ref=ref, value=value, counter_kind=prop))
            return
        if op in ('untap_step', 'upkeep_step'):
            self.steps.append(step(op, player=player))
            return
        if op == 'advance_scheduled_turn':
            self.steps.append(step(op, value=value))
            return
        if op == 'assert_shuffle_count':
            self.steps.append(step(op, player=player, value=value))
            return
        if op == 'cast_response':
            self.steps.append(step(op, ref=ref, value=value))
            return
        fields = dict(player=player, value=value, index=index, color=color)
        if ref:
            fields['ref'] = ref
        if prop:
            fields['property'] = prop
        if op in ('assert_zone', 'assert_zone_multiplicity', 'assert_zone_card'):
            fields['zone'] = next(n for n, number in ZONE.items() if number == zone)
        self.steps.append(step(op, **fields))

    def pool(self, action, player, counts):
        for color, count in enumerate(counts):
            self.emit(action, player=player, color=color, value=count)

    def observe(self):
        self.emit('refresh')
        self.emit('assert_stack', value=0)
        names = sorted({c['name'] for c in self.cards.values()})
        for player in (0, 1):
            self.emit('assert_life', player=player, value=self.life[player])
            self.emit('assert_shuffle_count', player=player, value=self.shuffles[player])
            self.pool('assert_mana', player, ZERO)
            for zone, number in ZONE.items():
                contents = self.library[player] if zone == 'library' else [
                    c['name'] for c in self.cards.values() if (c['player'], c['zone']) == (player, zone)]
                self.emit('assert_zone', player=player, value=len(contents), zone=number)
                for name, count in Counter({n: contents.count(n) for n in names}).items():
                    self.emit('assert_zone_multiplicity', player=player, zone=number,
                              index=self.rows[name]['csvid'], value=count)
                if zone == 'library' and (not self.shuffles[player] or len(contents) == 1):
                    for position, name in enumerate(contents):
                        self.emit('assert_zone_card', player=player, value=self.rows[name]['csvid'],
                                  zone=number, index=position)
        for ref, counters in self.loyalty.items():
            if self.cards[ref]['zone'] == 'battlefield':
                self.emit('assert_counter', ref, value=counters, prop='loyalty')
        for ref, tapped in self.tapped.items():
            if self.cards[ref]['zone'] == 'battlefield':
                self.emit('assert_card', ref, value=tapped, prop='tapped')

    def cast_pending(self, ref, *, target=None, stack=1):
        c = self.cards[ref]
        fee = cost_from_oracle(self.oracle[c['name']])
        self.pool('set_mana', c['player'], fee)
        self.emit('refresh')
        if target:
            self.emit('choose_target', target, player=c['player'])
        self.emit('cast', ref, value=1)
        self.emit('assert_stack', value=stack)
        self.pool('assert_mana', c['player'], ZERO)

    def cast(self, ref, dest='battlefield', *, target=None):
        self.cast_pending(ref, target=target)
        self.emit('resolve')
        self.emit('assert_choices_consumed')
        self.cards[ref]['zone'] = dest

    def take(self, player, name, zone):
        card, = [c for c in self.cards.values() if (c['player'], c['name'], c['zone']) == (player, name, 'library')]
        card['zone'] = zone
        self.library[player].remove(name)

    def advance(self):
        self.emit('advance_turn')
        self.active = 1 - self.active
        self.emit('untap_step', player=self.active)
        for ref in self.tapped:
            if self.cards[ref]['player'] == self.active:
                self.tapped[ref] = 0

    def partner(self):
        partner = PARTNERS[self.source]
        other = self.mode == 'opponent_library'
        recipient = self.foe if other else self.owner
        self.cast_pending('source')
        if self.owner == 0:
            self.emit('choose_trigger', 'source', player=self.owner)
        if other:
            # The current wrong self-search has an empty library, so the real target
            # and opponent’s named tutor intents stay unused without a hard error.
            self.emit('choose_player', player=self.owner, value=recipient)
        no_pick = self.mode == 'fail_to_find'
        if no_pick:
            self.emit('choose_zone', player=recipient, value=-1)
        else:
            self.emit('choose_zone_card', player=recipient, value=self.rows[partner]['csvid'])
        self.emit('resolve')
        self.emit('assert_unused_choices', value=0)
        self.emit('assert_choices_consumed')
        self.cards['source']['zone'] = 'battlefield'
        if not no_pick:
            self.take(recipient, partner, 'hand')
        self.shuffles[recipient] += 1
        self.observe()
        if other or no_pick:
            return
        # One card remains after tutoring. Fisher–Yates records the shuffle
        # without using RNG, leaving the same proved first coin sequence.
        win = self.mode != 'external_loss'
        coin_player = self.cards['coin_witness']['player']
        self.advance()
        if coin_player == self.owner:
            self.emit('upkeep_step', player=self.foe)
            self.advance()
        self.emit('choose_dialog', player=coin_player, value=actual_call(win))
        self.emit('upkeep_step', player=coin_player)
        self.emit('assert_unused_choices', value=0)
        if not win:
            self.cards['coin_witness']['zone'] = 'hand'
        elif self.source == ZNDRSPLT:
            self.take(self.owner, 'Forest', 'hand')
        if self.source == OKAUN:
            for prop in ('power', 'toughness'):
                self.emit('assert_card', 'source', value=6 if win else 3, prop=prop)
        self.observe()
        self.emit('cleanup')
        if self.source == OKAUN:
            for prop in ('power', 'toughness'):
                self.emit('assert_card', 'source', value=3, prop=prop)
        self.observe()

    def ral_cast(self):
        fee = list(cost_from_oracle(self.oracle[self.row['name']]))
        fee[0] -= 1
        self.pool('set_mana', self.owner, fee)
        self.emit('activate', 'owner_land', value=1)
        self.tapped['owner_land'] = 1
        self.emit('assert_stack', value=0)
        self.emit('assert_card', 'owner_land', value=1, prop='tapped')
        self.pool('assert_mana', self.owner, cost_from_oracle(self.oracle[self.row['name']]))
        self.emit('refresh')
        self.emit('cast', 'source', value=1)
        self.emit('assert_stack', value=1)
        self.pool('assert_mana', self.owner, ZERO)
        self.emit('resolve')
        self.emit('assert_choices_consumed')
        self.cards['source']['zone'] = 'battlefield'
        self.loyalty['source'] = 4
        self.observe()

    def walker_mode(self, mode, targets=(), player_target=None):
        self.emit('can_activate', 'source', value=1)
        self.emit('choose_dialog', player=self.owner, value=mode)
        for target in targets:
            self.emit('choose_target', target, player=self.owner)
        if player_target is not None:
            self.emit('choose_player', player=self.owner, value=player_target)
        self.emit('activate', 'source', value=1)
        self.emit('assert_stack', value=1)
        self.loyalty['source'] += (1, -2, -7)[mode]
        if mode != 2:
            self.emit('assert_counter', 'source', prop='loyalty', value=self.loyalty['source'])
            self.emit('can_activate', 'source', value=0)
        if self.mode == 'first_target_lost':
            self.cast_pending('response', target='victim', stack=2)
        if mode == 2:
            # The legacy sees the retired parent as non-Ral and asks five
            # Heads/Tails questions. Heads preserves the real numerical result.
            for _ in range(5):
                self.emit('choose_dialog', player=self.owner, value=0)
        self.emit('resolve')
        self.emit('assert_unused_choices', value=5 if mode == 2 else 0)

    def ral(self):
        if self.mode == 'bolt_walker':
            self.emit('turn', player=self.foe)
            self.cast('walker')
            self.loyalty['walker'] = 4
            self.observe()
            self.emit('turn', player=self.owner)
        self.ral_cast()
        if self.mode in ('plus', 'two_heads', 'zero_heads'):
            turns = 3 if self.mode in RAL_SEEDS else 1
            for i in range(turns):
                self.walker_mode(0, ('victim', 'owner_land'))
                self.tapped.update(victim=1, owner_land=0)
                self.observe()
                if self.mode in RAL_SEEDS:
                    self.advance()
                    self.advance()
                    self.emit('turn', player=self.owner)
            if self.mode in RAL_SEEDS:
                self.walker_mode(2)
                self.cards['source']['zone'] = 'graveyard'
                self.observe()
                _, _, heads = RAL_SEEDS[self.mode]
                for next_player in (self.owner,) * heads + (self.foe, self.owner):
                    self.emit('advance_scheduled_turn', value=next_player)
                    self.emit('untap_step', player=next_player)
                    for ref in self.tapped:
                        if self.cards[ref]['player'] == next_player:
                            self.tapped[ref] = 0
                    self.observe()
        elif self.mode == 'first_target_lost':
            self.walker_mode(0, ('victim', 'owner_land'))
            self.cards['response']['zone'] = 'graveyard'
            self.cards['victim']['zone'] = 'hand'
            self.tapped['owner_land'] = 0
            self.observe()
        elif self.mode == 'bolt_player':
            self.walker_mode(1, player_target=self.foe)
            self.life[self.foe] -= 3
            self.observe()
        else:
            ref = 'victim' if self.mode == 'bolt_creature' else 'walker'
            self.walker_mode(1, (ref,))
            if ref == 'victim':
                self.cards[ref]['zone'] = 'graveyard'
            else:
                self.loyalty['walker'] -= 3
            self.observe()

    def frenetic(self):
        if self.mode == 'countered':
            self.cast_pending('source')
            self.emit('announce_spell', 'source')
            self.pool('set_mana', self.foe, cost_from_oracle(self.oracle['Counterspell']))
            self.emit('refresh')
            self.emit('cast_response', 'counter', value=1)
            self.emit('assert_stack', value=2)
            self.emit('resolve_response')
            self.emit('assert_unused_choices', value=0)
            self.cards['source']['zone'] = self.cards['counter']['zone'] = 'graveyard'
            self.observe()
            return
        if self.mode == 'wrong_color':
            funds = (3, 0, 0, 0, 0, 0)
            self.pool('set_mana', self.owner, funds)
            self.emit('refresh')
            self.emit('can_cast', 'source', value=0)
            self.pool('assert_mana', self.owner, funds)
            self.pool('set_mana', self.owner, ZERO)
            self.observe()
            return
        self.cast('source')
        for prop in ('power', 'toughness'):
            self.emit('assert_card', 'source', prop=prop, value=2)
        self.observe()
        self.cast('bounce', 'graveyard', target='source')
        self.cards['source']['zone'] = 'hand'
        self.emit('bind_hand', 'source', player=self.owner, index=-1, value=self.source)
        self.observe()
        self.cast('source')
        self.observe()

    def build(self):
        self.emit('turn', player=self.owner)
        for player in (0, 1):
            self.pool('set_mana', player, ZERO)
        if self.source in PARTNERS:
            self.partner()
        elif self.source == RAL:
            self.ral()
        else:
            self.frenetic()



def expand(name, csvid, declaration, shape, selected, rows, oracle):
    validate_variants(shape, selected)
    validate_witnesses(oracle, rows)
    if profiles()['sources'][name]['registry']['csvid'] != csvid:
        raise ValueError('exact registered Partner and coin CSVID required')
    cases = []
    for variant in selected:
        mode, owner = variant.rsplit('_', 1)
        game = Game(rows[name], int(owner), mode, rows, oracle)
        game.build()
        gaps = [PARTNER_GAP] if csvid in PARTNERS else [FRENETIC_GAP] if csvid == FRENETIC else []
        cases.append(dict(
            name=f'CoinFlipPartnerAndRecurringRecipe_{csvid}_{variant}',
            card=name, fixture='empty_main', seed=game.seed, setup=game.setup, steps=game.steps,
            _recipe=dict(family=FAMILY, version=2, variant=variant, params=declaration['params'],
                         oracle_text=declaration['oracle_text'], untested_components=gaps),
        ))
    return cases
