"""Conditional seeded coin activations with real target and resolution windows."""

from collections import Counter
from functools import lru_cache
import json
from pathlib import Path

from counter_condition_recurrence_recipes import exact
from next_upkeep_cantrip_additional_riders_recipes import COIN_SEED, actual_call, xorshift
from recipes import card, cost_from_oracle, step


FAMILY = 'coin_flip_conditional_activation'
PROFILE = Path(__file__).with_name('data') / 'coin_flip_conditional_activation_wave8_profiles.json'
ZONE = {'hand': 0, 'battlefield': 1, 'library': 2, 'graveyard': 3, 'exile': 4}

QUOZ = 1851
ARTISANS = 498
KITES = 1784
BOX = 863
PRINTED = {
    QUOZ: ('{6}', 'Artifact', (
        "Remove this card from your deck before playing if you're not playing for ante.",
        "{T}, Sacrifice this artifact: Target opponent may ante the top card of their library. If they don't, you flip a coin. If you win the flip, that player loses the game. If you lose the flip, you lose the game. Activate only during your upkeep.",
    )),
    ARTISANS: ('{R}', 'Creature — Goblin Artificer', (
        "{T}: Flip a coin. If you win the flip, draw a card. If you lose the flip, counter target artifact spell you control that isn't the target of an ability from another creature named Goblin Artisans.",
    )),
    KITES: ('{1}{R}', 'Enchantment', (
        '{R}: Target creature you control with toughness 2 or less gains flying until end of turn. Flip a coin at the beginning of the next end step. If you lose the flip, sacrifice that creature.',
    )),
    BOX: ('{5}', 'Artifact', (
        "{3}, {T}: Choose a random summon card from all players' decks. For each player, flip a coin. If the flip ends up heads, put a token creature into play and treat it as though an exact copy of the chosen summon card were just played.",
    )),
}
MODES = {
    QUOZ: ('main_denied', 'foe_upkeep_denied', 'own_upkeep_probe'),
    ARTISANS: ('win', 'lose', 'no_parent', 'foe_artifact', 'own_nonartifact'),
    KITES: ('win', 'lose', 'only_illegal'),
    BOX: ('both_heads', 'both_tails', 'own_heads', 'foe_heads'),
}
EXPECTED_BOX_HEADS = {'both_heads': (True, True), 'both_tails': (False, False),
                      'own_heads': (True, False), 'foe_heads': (False, True)}
ZERO = [0] * 6
RED = cost_from_oracle({'mana_cost': '{R}'})
THREE = cost_from_oracle({'mana_cost': '{3}'})
COPY = 'Elvish Visionary'


WITNESSES = {
    'Forest', 'Island', 'Mountain', 'Plains', 'Swamp', 'Elvish Visionary', 'Prodigal Sorcerer',
    "Traveler's Amulet", 'Dazzling Ramparts', 'Merfolk Mesmerist', 'Darksteel Myr',
}
GAPS = {
    QUOZ: ['Diagnostic preplaced-artifact timing only. Ante deck admission, ante transfer and terminal result are outside the bounded fixture.'],
    ARTISANS: ['A second Artisans choosing the same parent while the first activation is pending requires an activation-on-activation response window.'],
    BOX: ['One eligible copied identity calibrates the seeded outcomes and functional copy. Random choice among distinct summon cards and empty candidate pools are untested.'],
}


@lru_cache(maxsize=1)
def profiles():
    return json.loads(PROFILE.read_bytes())


def classify(record, csvid):
    if csvid not in PRINTED or not record or record.get('card_faces') or record.get('selected_face'):
        raise ValueError('whole normal conditional coin primary required')
    fee, type_line, clauses = PRINTED[csvid]
    if (record.get('layout') != 'normal' or record.get('type_line') != type_line or record.get('keywords')
            or record.get('mana_cost') != fee or record.get('oracle_text') != '\n'.join(clauses)):
        raise ValueError('complete printed coin trigger, cost, target, and outcome clauses required')
    if csvid == ARTISANS and (record.get('power'), record.get('toughness')) != ('1', '1'):
        raise ValueError('exact Goblin Artisans stats required')
    return dict(csvid=csvid, cost=cost_from_oracle(record))


def variants(shape):
    return [f'{mode}_{seat}' for mode in MODES[shape['csvid']] for seat in (0, 1)]


def require_variants(shape, selected):
    if (type(selected) is not list or not selected or any(type(m) is not str for m in selected)
            or len(set(selected)) != len(selected) or set(selected) - set(variants(shape))):
        raise ValueError('distinct supported conditional coin variants required')


def spec(declaration, record):
    pin = profiles()['sources'].get(record.get('name', ''))
    if not pin or not exact(pin['oracle'], record):
        raise ValueError('complete pinned conditional coin primary required')
    shape = classify(record, pin['registry']['csvid'])
    if (declaration.get('family') != FAMILY or type(declaration.get('version')) is not int
            or declaration['version'] != 2 or declaration.get('oracle_text') != record['oracle_text']
            or not exact(declaration.get('params'), shape)):
        raise ValueError('version 2 whole-card conditional coin declaration required')
    require_variants(shape, declaration.get('variants'))
    return dict(shape, family=FAMILY)


def validate_witnesses(oracle, rows):
    pins = profiles()
    if (set(pins) != {'sources', 'witnesses'} or set(pins['witnesses']) != WITNESSES
            or {p['registry']['csvid'] for p in pins['sources'].values()} != set(PRINTED)):
        raise ValueError('complete conditional coin source and witness profile required')
    for name, pin in (pins['sources'] | pins['witnesses']).items():
        row = rows.get(name, {})
        if (not exact(oracle.get(name), pin['oracle']) or not exact(row, pin['registry'])
                or row.get('kind') != 'card' or row.get('handler') in (None, '0', 'NULL', 'card_dummy')
                or oracle[name].get('layout') != 'normal'):
            raise ValueError('pinned conditional coin source or witness changed: ' + name)
        if name in pins['sources']:
            classify(oracle[name], row['csvid'])


def box_heads(seed):
    # One eligible identity means internal_rand(1) does not advance the RNG.
    # Pandora bypasses human coin calls and consumes one sample per APNAP seat.
    first = xorshift(seed)
    second = xorshift(first)
    return first % 2 == 0, second % 2 == 0


BOX_SEEDS = {name: next(seed for seed in range(1, 1 << 16)
                        if box_heads(seed) == outcomes) for name, outcomes in EXPECTED_BOX_HEADS.items()}


class Duel:
    def __init__(self, csvid, record, actor, mode, rows, oracle):
        self.csvid, self.record, self.actor, self.foe, self.mode = csvid, record, actor, 1 - actor, mode
        self.rows, self.oracle = rows, oracle
        self.cast_cost = classify(record, csvid)['cost']
        self.setup, self.steps, self.cards = [], [], {}
        self.life = [20, 20]
        self.drawn = [0, 0]
        self.seed = BOX_SEEDS[mode] if csvid == BOX else COIN_SEED
        self.library = {actor: ['Forest', 'Plains', 'Island', 'Swamp'],
                        self.foe: ['Island', 'Mountain', 'Swamp', 'Forest']}
        if csvid == BOX:
            # Keep the candidate from moving if its token's ETB draws the top card.
            self.library[actor if mode == 'own_heads' else self.foe].insert(2, COPY)
        self.add('source', actor, record['name'], 'battlefield' if csvid == QUOZ else 'hand')
        for player, deck in self.library.items():
            for index, name in enumerate(deck):
                self.add(f'd_{player}_{index}', player, name, 'library')
        if csvid == ARTISANS and mode != 'no_parent':
            self.add('parent', self.foe if mode == 'foe_artifact' else actor,
                     'Prodigal Sorcerer' if mode == 'own_nonartifact' else "Traveler's Amulet", 'hand')
        if csvid == KITES:
            if mode == 'only_illegal':
                self.add('large', actor, 'Dazzling Ramparts', 'battlefield')
                self.add('foreign', self.foe, 'Merfolk Mesmerist', 'battlefield')
            else:
                self.add('flier', actor, 'Darksteel Myr' if mode == 'lose' else 'Merfolk Mesmerist', 'battlefield')
                self.add('blocker', self.foe, 'Dazzling Ramparts', 'battlefield')

    def add(self, ref, player, name, zone):
        self.setup.append(card(name, ref, player, zone))
        self.cards[ref] = dict(name=name, player=player, zone=zone)

    def emit(self, op, ref='', other='', prop='', *, player=0, value=0, zone=0, index=0, color=0):
        if op in ('untap_step', 'upkeep_step'):
            self.steps.append(step(op, player=player))
            return
        if op == 'activate_response':
            self.steps.append(step(op, ref=ref, value=value))
            return
        fields = dict(player=player, value=value, index=index, color=color)
        if ref:
            fields['ref'] = ref
        if other:
            fields['other'] = other
        if prop:
            fields['property'] = prop
        if op in ('assert_zone', 'assert_zone_card', 'assert_zone_multiplicity'):
            fields['zone'] = next(name for name, number in ZONE.items() if number == zone)
        self.steps.append(step(op, **fields))

    def observe(self, ref, prop, value):
        self.emit('assert_card', ref, prop=prop, value=value)

    def mana(self, op, player, pool):
        for color, value in enumerate(pool):
            self.emit(op, player=player, color=color, value=value)

    def receipts(self, stack=0):
        self.emit('assert_stack', value=stack)
        names = sorted({c['name'] for c in self.cards.values()})
        for player in (0, 1):
            self.emit('assert_life', player=player, value=self.life[player])
            for zone, index in ZONE.items():
                contents = self.library[player] if zone == 'library' else [
                    c['name'] for c in self.cards.values() if c['player'] == player and c['zone'] == zone
                ]
                self.emit('assert_zone', player=player, zone=index, value=len(contents))
                occurrences = Counter(contents)
                for name in names:
                    self.emit('assert_zone_multiplicity', player=player, zone=index,
                              index=self.rows[name]['csvid'], value=occurrences[name])
                if zone == 'library':
                    for i, name in enumerate(contents):
                        self.emit('assert_zone_card', player=player, zone=index, index=i,
                                  value=self.rows[name]['csvid'])

    def cast_pending(self, ref, cost):
        self.mana('set_mana', self.actor, cost)
        self.emit('refresh')
        self.emit('cast', ref, value=1)
        self.emit('assert_choices_consumed')
        self.emit('assert_stack', value=1)
        self.mana('assert_mana', self.actor, [0] * 6)

    def cast_paid(self, ref, cost):
        self.cast_pending(ref, cost)
        self.emit('resolve')
        self.emit('assert_choices_consumed')
        self.emit('refresh')
        self.cards[ref]['zone'] = 'battlefield'

    def age(self):
        for player in (self.foe, self.actor):
            self.emit('advance_turn')
            self.emit('untap_step', player=player)
            self.emit('turn', player=player)

    def mark_draw(self, player):
        self.cards[f'd_{player}_{self.drawn[player]}']['zone'] = 'hand'
        self.drawn[player] += 1
        del self.library[player][0]

    def state(self, stack=0, *, live_mana=None):
        self.receipts(stack)
        for player in (0, 1):
            self.mana('assert_mana', player, live_mana if live_mana is not None and player == self.actor else ZERO)

    def quoz(self):
        if self.mode != 'main_denied':
            self.emit('advance_turn')
            self.emit('untap_step', player=self.foe)
            self.emit('upkeep_step', player=self.foe)
            if self.mode == 'own_upkeep_probe':
                self.emit('advance_turn')
                self.emit('untap_step', player=self.actor)
                self.emit('upkeep_step', player=self.actor)
        eligible = self.mode == 'own_upkeep_probe'
        self.emit('can_activate', 'source', value=int(eligible))
        if not eligible:
            self.emit('activate', 'source', value=0)
        self.observe('source', 'in_play', 1)
        self.observe('source', 'tapped', 0)
        self.emit('assert_choices_consumed')
        self.state()

    def artisans(self):
        self.age()
        self.observe('source', 'sick', 0)
        self.observe('source', 'tapped', 0)
        if self.mode == 'no_parent':
            self.emit('can_activate', 'source', value=0)
            self.emit('activate', 'source', value=0)
            self.emit('assert_choices_consumed')
            self.state()
            return
        parent = self.cards['parent']
        seat = parent['player']
        if seat != self.actor:
            self.emit('advance_turn')
            self.emit('untap_step', player=seat)
            self.emit('turn', player=seat)
        self.mana('set_mana', seat, cost_from_oracle(self.oracle[parent['name']]))
        self.emit('refresh')
        self.emit('cast', 'parent', value=1)
        self.emit('announce_spell', 'parent')
        self.emit('assert_stack', value=1)
        self.observe('parent', 'in_hand', 0)
        self.mana('assert_mana', seat, ZERO)
        eligible = self.mode in ('win', 'lose')
        self.emit('activate_response', 'source', value=int(eligible))
        self.observe('source', 'tapped', int(eligible))
        self.emit('assert_stack', value=2 if eligible else 1)
        if eligible:
            self.emit('choose_dialog', player=self.actor, value=actual_call(self.mode == 'win'))
            self.emit('resolve_response')
            self.emit('assert_unused_choices', value=0)
        else:
            self.emit('resolve')
        self.emit('assert_choices_consumed')
        self.emit('refresh')
        parent['zone'] = 'graveyard' if self.mode == 'lose' else 'battlefield'
        if self.mode == 'win':
            self.mark_draw(self.actor)
        self.state()

    def kites_combat(self, flying):
        self.emit('begin_attackers')
        self.emit('attack', 'flier', value=1)
        self.emit('finish_attackers')
        self.emit('begin_blockers')
        self.emit('block', 'blocker', 'flier', value=int(not flying))
        self.emit('advance_combat')
        self.observe('flier', 'blocked', int(not flying))
        self.emit('combat_damage')
        self.emit('resolve')
        self.emit('end_combat')
        if flying:
            self.life[self.foe] -= int(self.oracle[self.cards['flier']['name']]['power'])
        self.state()

    def kites(self):
        self.emit('can_activate', 'source', value=0)
        self.emit('activate', 'source', value=0)
        self.mana('set_mana', self.actor, RED)
        self.emit('refresh')
        if self.mode == 'only_illegal':
            self.emit('can_activate', 'source', value=0)
            self.emit('activate', 'source', value=0)
            self.emit('assert_choices_consumed')
            self.state(live_mana=RED)
            return
        self.emit('can_activate', 'source', value=1)
        self.emit('choose_target', 'flier', player=self.actor)
        self.emit('activate', 'source', value=1)
        self.emit('assert_stack', value=1)
        self.mana('assert_mana', self.actor, ZERO)
        self.emit('resolve')
        self.emit('assert_choices_consumed')
        self.emit('refresh')
        self.observe('flier', 'in_play', 1)
        self.kites_combat(True)
        # The winning fixture deals combat damage, whose native prevention
        # search consumes one sample before saving its replay RNG state.
        face = xorshift(xorshift(xorshift(self.seed))) % 2 if self.mode == 'win' else actual_call(False)
        self.emit('choose_dialog', player=self.actor, value=face)
        self.emit('advance_turn')
        self.emit('assert_unused_choices', value=0)
        self.emit('assert_choices_consumed')
        if self.mode == 'lose':
            self.cards['flier']['zone'] = 'graveyard'
            self.state()
            return
        self.state()
        self.emit('untap_step', player=self.foe)
        self.emit('turn', player=self.foe)
        self.emit('advance_turn')
        self.emit('untap_step', player=self.actor)
        self.emit('turn', player=self.actor)
        self.kites_combat(False)

    def box(self):
        self.emit('can_activate', 'source', value=0)
        self.emit('activate', 'source', value=0)
        self.mana('set_mana', self.actor, THREE)
        self.emit('refresh')
        self.emit('can_activate', 'source', value=1)
        self.emit('activate', 'source', value=1)
        self.observe('source', 'tapped', 1)
        self.state(stack=1)
        self.emit('choose_zone', player=self.actor, value=-1)
        self.emit('resolve')
        self.emit('assert_unused_choices', value=0)
        self.emit('assert_choices_consumed')
        self.emit('refresh')
        for player, heads in zip((self.actor, self.foe), box_heads(self.seed)):
            if heads:
                self.cards[f'copy_{player}'] = dict(name=COPY, player=player, zone='battlefield')
                self.mark_draw(player)
        self.state()
        copy_id = self.rows[COPY]['csvid']
        for player, heads in zip((self.actor, self.foe), box_heads(self.seed)):
            if heads:
                for prop, value in (('token', 1), ('power', 1), ('toughness', 1), ('color', 1 << 3),
                                    ('artifact', 0), ('subtype_Elf', 1)):
                    self.emit('assert_matching_permanents', prop=prop, player=player, index=copy_id, value=value)

    def build(self):
        self.emit('turn', player=self.actor)
        for player in (0, 1):
            self.mana('set_mana', player, ZERO)
        self.emit('present_ai_dialogs', value=1)
        if self.csvid == QUOZ:
            self.quoz()
            return
        self.cast_paid('source', self.cast_cost)
        self.state()
        if self.csvid == ARTISANS:
            self.artisans()
        elif self.csvid == KITES:
            if self.mode != 'only_illegal':
                self.age()
            self.kites()
        else:
            self.box()


def expand(name, csvid, declaration, shape, selected, rows, oracle):
    require_variants(shape, selected)
    validate_witnesses(oracle, rows)
    if profiles()['sources'][name]['registry']['csvid'] != csvid:
        raise ValueError('exact registered conditional coin primary required')
    cases = []
    for variant in selected:
        mode, seat = variant.rsplit('_', 1)
        world = Duel(csvid, oracle[name], int(seat), mode, rows, oracle)
        world.build()
        cases.append(dict(
            name=f'CoinConditionalActivationRecipe_{csvid}_{variant}', card=name, fixture='empty_main', seed=world.seed,
            setup=world.setup, steps=world.steps,
            _recipe=dict(family=FAMILY, version=2, variant=variant, params=declaration['params'],
                         oracle_text=declaration['oracle_text'], untested_components=GAPS.get(csvid, [])),
        ))
    return cases
