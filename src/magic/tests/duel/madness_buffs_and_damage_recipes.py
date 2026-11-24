"""Targeted Madness buffs and damage through real discard, spells, and combat."""
from collections import Counter
from copy import deepcopy
from functools import lru_cache
import json
from pathlib import Path

from counter_condition_recurrence_recipes import exact
from recipes import card, cost_from_oracle, step


FAMILY = 'madness_buffs_and_damage'
PROFILE = Path(__file__).with_name('data') / 'madness_buffs_and_damage_wave8_profiles.json'
MADNESS = ' (If you discard this card, discard it into exile. When you do, cast it for its madness cost or put it into your graveyard.)'
GOBLIN = 'Rummaging Goblin'
ZONES = ('hand', 'battlefield', 'library', 'graveyard', 'exile')
NOVEL = ('normal_countered', 'offturn_paid', 'replay')
NEW_WITNESSES = {'Counterspell'}
GAP = ('Native Madness pays and resolves synchronously inside the discard callback. Interposition during '
       'that callback, public visibility of the face-up exile intermediate, and multiplayer are outside these cases.')
SEED = 2471
PREFIX = 'MadnessBuffsAndDamageRecipe'
ISOLATION, LUNACY, ABANDON, PRIEST = 5884, 5885, 16028, 16038
AURAS = {ISOLATION: ('black', 1, 2, 32, 'Muck Rats'), LUNACY: ('white', 2, 1, 2, 'Suntail Hawk')}
CARDS = {
    ISOLATION: ('{1}{W}', '{W}', 'Enchantment — Aura', ('Enchant creature',
                 'Enchanted creature gets +1/+2 and has protection from black.')),
    LUNACY: ('{1}{B}', '{B}', 'Enchantment — Aura', ('Enchant creature',
              'Enchanted creature gets +2/+1 and has protection from white.')),
    ABANDON: ('{2}{R}', '{1}{R}', 'Instant', ('Up to two target creatures each get +1/+0 and gain first strike until end of turn. (They deal combat damage before creatures without first strike.)',)),
    PRIEST: ('{2}{B}{R}', '{1}{B}{R}', 'Creature — Vampire Cleric',
             ('Whenever this creature enters or attacks, if you have no cards in hand, this creature deals 2 damage to any target.',)),
}
COMMON = ('paid', 'declined', 'short', 'normal')
EXTRA = {ISOLATION: ('paid_foreign',), LUNACY: ('paid_foreign',), ABANDON: ('one_target', 'zero_targets'),
         PRIEST: ('retain_draw', 'normal_nonempty')}
PAID = {'paid', 'paid_foreign', 'one_target', 'zero_targets', 'retain_draw', 'replay', 'offturn_paid'}
NORMAL = {'normal', 'normal_nonempty'}
BEAR = 'Grizzly Bears'
RED_BLOCKER = "Mons's Goblin Raiders"
SENTINEL = 'Squire'
ZERO = [0] * 6

WITNESSES = {GOBLIN, BEAR, RED_BLOCKER, SENTINEL, 'Mountain', 'Island', 'Plains', 'Muck Rats',
             'Suntail Hawk', 'Disfigure', 'Healing Salve', 'Hill Giant', *NEW_WITNESSES}


@lru_cache(maxsize=1)
def profiles():
    return json.loads(PROFILE.read_bytes())


def classify(record, csvid):
    if (csvid not in CARDS or record.get('layout') != 'normal' or record.get('selected_face')
            or record.get('card_faces')):
        raise ValueError('complete normal Madness record required')
    normal, madness, type_line, riders = CARDS[csvid]
    wanted = '\n'.join((*riders, 'Madness ' + madness + MADNESS))
    keys = {'Madness', 'Enchant'} if csvid in AURAS else {'Madness'}
    if (record.get('oracle_text') != wanted or record.get('mana_cost') != normal
            or record.get('type_line') != type_line or set(record.get('keywords', ())) != keys
            or csvid == PRIEST and (record.get('power'), record.get('toughness')) != ('4', '4')):
        raise ValueError('complete printed Madness cost and rider grammar required')
    return dict(csvid=csvid, normal=cost_from_oracle(record), madness=cost_from_oracle({'mana_cost': madness}))


def variants(shape):
    return [f'{mode}_{actor}' for mode in (*COMMON, *EXTRA[shape['csvid']], *NOVEL) for actor in (0, 1)]


def validate_variants(shape, selected):
    if (type(selected) is not list or not selected or any(type(mode) is not str for mode in selected)
            or len(set(selected)) != len(selected) or set(selected) - set(variants(shape))):
        raise ValueError('distinct supported Madness variants required')


def spec(declaration, record):
    pinned = profiles()['sources'].get(record.get('name'))
    if not pinned or not exact(pinned['oracle'], record):
        raise ValueError('complete pinned Madness primary required')
    shape = classify(record, pinned['registry']['csvid'])
    if (declaration.get('family') != FAMILY or type(declaration.get('version')) is not int
            or declaration['version'] != 2 or declaration.get('oracle_text') != record['oracle_text']
            or not exact(declaration.get('params'), shape)):
        raise ValueError('exact declared Madness branch required')
    validate_variants(shape, declaration.get('variants'))
    return dict(shape, family=FAMILY)


def validate_witnesses(oracle, rows):
    pins = profiles()
    if set(pins) != {'sources', 'witnesses'} or set(pins['witnesses']) != WITNESSES:
        raise ValueError('complete independent Madness witnesses required')
    for group, entries in pins.items():
        for name, pin in entries.items():
            row = rows.get(name, {})
            if (not exact(oracle.get(name), pin['oracle']) or not exact(row, pin['registry'])
                    or row.get('kind') != 'card' or row.get('handler') in (None, '0', 'NULL')
                    or oracle[name].get('layout') != 'normal'):
                raise ValueError('exact Madness primary or witness changed: ' + name)
            if group == 'sources':
                if row['handler'] == 'card_dummy':
                    raise ValueError('implemented Madness primary required')
                classify(oracle[name], row['csvid'])
    if (oracle[GOBLIN]['oracle_text'] != '{T}, Discard a card: Draw a card.'
            or oracle['Disfigure']['colors'] != ['B'] or oracle['Healing Salve']['colors'] != ['W']):
        raise ValueError('exact discard and opposed protection probes required')


class Duel:
    def __init__(self, csvid, record, actor, mode, rows, oracle):
        self.csvid, self.record, self.actor, self.foe, self.mode = csvid, record, actor, 1 - actor, mode
        self.rows, self.oracle = rows, oracle
        shape = classify(record, csvid)
        self.normal_cost, self.madness_cost = shape['normal'], shape['madness']
        self.setup, self.steps, self.cards = [], [], {}
        self.life = [20, 20]
        self.library = {actor: ['Mountain', 'Plains', 'Island'], self.foe: ['Plains', 'Island']}
        self.add('source', actor, record['name'], 'hand')
        if mode == 'normal_countered':
            self.add('counter', self.foe, 'Counterspell', 'hand')
        elif mode not in NORMAL:
            self.add('looter', actor, GOBLIN, 'hand')
        for player, names in self.library.items():
            for index, name in enumerate(names):
                self.add(f'd_{player}_{index}', player, name, 'library')
        self.destination = 'battlefield' if csvid != ABANDON else 'graveyard'
        self.number_of_targets = {'one_target': 1, 'zero_targets': 0}.get(mode, 2)
        if csvid in AURAS:
            self.host_seat = self.foe if mode == 'paid_foreign' else self.actor
            self.add('host', self.host_seat, BEAR, 'battlefield')
            self.add('other', 1 - self.host_seat, BEAR, 'battlefield')
            protected_attacker = SENTINEL if csvid == LUNACY and mode == 'offturn_paid' else AURAS[csvid][-1]
            self.add('protected_blocker', 1 - self.host_seat, protected_attacker, 'battlefield')
            self.add('red_blocker', 1 - self.host_seat, RED_BLOCKER, 'battlefield')
            self.add('black_probe', self.foe, 'Disfigure', 'hand')
            self.add('white_probe', self.foe, 'Healing Salve', 'hand')
        if csvid == ABANDON:
            self.add('first', self.actor, BEAR, 'battlefield')
            self.add('second', self.actor, BEAR, 'battlefield')
            self.add('other', self.actor, SENTINEL, 'battlefield')
            self.add('first_blocker', self.foe, 'Hill Giant', 'battlefield')
            self.add('second_blocker', self.foe, 'Hill Giant', 'battlefield')
        if csvid == PRIEST:
            self.add('victim', self.foe, BEAR, 'battlefield')
            if mode == 'normal_nonempty':
                self.add('remaining', self.actor, SENTINEL, 'hand')

    def add(self, ref, player, name, zone):
        self.setup.append(card(name, ref, player, zone))
        self.cards[ref] = dict(name=name, player=player, zone=zone)

    def emit(self, op, ref='', other='', **values):
        if ref:
            values['ref'] = ref
        if other:
            values['other'] = other
        self.steps.append(step(op, **values))

    def observe(self, ref, prop, value):
        self.emit('assert_card', ref, property=prop, value=value)

    def mana(self, op, player, pool):
        for color, value in enumerate(pool):
            self.emit(op, player=player, color=color, value=value)

    def receipts(self, stack=0):
        self.emit('assert_stack', value=stack)
        names = sorted({c['name'] for c in self.cards.values()})
        for player in (0, 1):
            self.emit('assert_life', player=player, value=self.life[player])
            for zone in ZONES:
                contents = self.library[player] if zone == 'library' else [
                    c['name'] for c in self.cards.values() if c['player'] == player and c['zone'] == zone
                ]
                self.emit('assert_zone', player=player, zone=zone, value=len(contents))
                occurrences = Counter(contents)
                for name in names:
                    self.emit('assert_zone_multiplicity', player=player, zone=zone,
                              index=self.rows[name]['csvid'], value=occurrences[name])
                if zone == 'library':
                    for i, name in enumerate(contents):
                        self.emit('assert_zone_card', player=player, zone=zone, index=i,
                                  value=self.rows[name]['csvid'])

    def cast_paid(self, ref, cost):
        self.mana('set_mana', self.actor, cost)
        self.emit('refresh')
        self.emit('cast', ref, value=1)
        self.emit('assert_choices_consumed')
        self.emit('assert_stack', value=1)
        self.mana('assert_mana', self.actor, [0] * 6)
        self.emit('resolve')
        self.emit('assert_choices_consumed')
        self.emit('refresh')
        self.cards[ref]['zone'] = 'battlefield'

    def age(self):
        for player in (self.foe, self.actor):
            self.emit('advance_turn')
            self.emit('untap_step', player=player)
            self.emit('turn', player=player)

    def zone(self, name, seat, zone, count):
        self.emit('assert_zone_multiplicity', player=seat, zone=zone, index=self.rows[name]['csvid'], value=count)

    def size(self, seat, zone, count):
        self.emit('assert_zone', player=seat, zone=zone, value=count)

    def stats(self, ref, power, toughness):
        self.observe(ref, 'in_play', 1)
        self.observe(ref, 'power', power)
        self.observe(ref, 'toughness', toughness)

    def cast_choices(self):
        if self.csvid in AURAS:
            self.emit('choose_target', 'host', player=self.actor)
        if self.csvid == ABANDON:
            for ref in ('first', 'second')[:self.number_of_targets]:
                self.emit('choose_target', ref, player=self.actor)
            if self.number_of_targets < 2:
                self.emit('choose_cancel_target', player=self.actor)

    def plan_entry_trigger(self):
        if self.csvid == PRIEST and self.mode != 'normal_nonempty':
            self.emit('choose_target', 'victim', player=self.actor)

    def normal_cast(self):
        self.mana('set_mana', self.actor, self.normal_cost)
        self.emit('refresh')
        self.cast_choices()
        self.emit('cast', 'source', value=1)
        self.emit('assert_choices_consumed')
        self.emit('assert_stack', value=1)
        self.mana('assert_mana', self.actor, ZERO)
        if self.mode == 'normal_countered':
            self.counter_normal()
            return
        self.plan_entry_trigger()
        self.emit('resolve')
        self.emit('assert_choices_consumed')
        self.emit('refresh')
        self.cards['source']['zone'] = self.destination
        self.effect_received()

    def counter_normal(self):
        self.emit('announce_spell', 'source')
        self.mana('set_mana', self.foe, cost_from_oracle(self.oracle['Counterspell']))
        self.emit('refresh')
        self.emit('cast_response', 'counter', value=1)
        self.emit('assert_choices_consumed')
        self.emit('assert_stack', value=2)
        self.mana('assert_mana', self.foe, ZERO)
        self.emit('resolve_response')
        self.emit('assert_choices_consumed')
        self.emit('refresh')
        self.cards['source']['zone'] = self.cards['counter']['zone'] = 'graveyard'
        self.unapplied()

    def discard(self):
        self.cast_paid('looter', cost_from_oracle(self.oracle[GOBLIN]))
        self.age()
        if self.mode == 'offturn_paid':
            self.advance(self.foe)
        self.observe('looter', 'in_play', 1)
        self.observe('looter', 'tapped', 0)
        self.observe('looter', 'sick', 0)
        self.receipts()
        self.emit('discard_policy', value=1)
        self.emit('present_ai_dialogs', value=1)
        affordable = self.mode != 'short'
        pool = self.madness_cost if affordable else [sum(self.madness_cost), 0, 0, 0, 0, 0]
        self.mana('set_mana', self.actor, pool)
        self.emit('refresh')
        cut = len(self.steps) if self.mode == 'replay' else None
        self.emit('can_activate', 'looter', value=1)
        if self.actor == 1:
            self.emit('choose_dialog', player=self.actor, value=0)
        paid = self.mode in PAID
        self.emit('choose_dialog', player=self.actor, value=int(affordable and not paid))
        if paid:
            self.cast_choices()
            self.plan_entry_trigger()
        self.emit('activate', 'looter', value=1)
        self.emit('assert_unused_choices', value=0)
        self.emit('assert_choices_consumed')
        if self.actor == 0:
            self.emit('assert_discard_choices', value=1)
        self.cards['source']['zone'] = self.destination if paid else 'graveyard'
        self.observe('looter', 'tapped', 1)
        self.mana('assert_mana', self.actor, ZERO if paid else pool)
        self.mana('assert_mana', self.foe, ZERO)
        self.emit('assert_stack', value=1)
        self.size(self.actor, 'hand', 0)
        self.zone(self.record['name'], self.actor, self.cards['source']['zone'], 1)
        self.zone(self.record['name'], self.actor, 'exile', 0)
        self.emit('resolve')
        self.emit('assert_choices_consumed')
        self.emit('refresh')
        self.cards[f'd_{self.actor}_0']['zone'] = 'hand'
        assert self.library[self.actor].pop(0) == 'Mountain'
        if paid:
            self.effect_received()
        else:
            self.unapplied()
        return cut

    def effect_received(self):
        if self.csvid == PRIEST and self.mode != 'normal_nonempty':
            self.cards['victim']['zone'] = 'graveyard'
        self.receipts()

    def unapplied(self):
        self.receipts()
        if self.csvid in AURAS:
            self.stats('host', 2, 2)
            self.stats('other', 2, 2)
            self.aura_targets(active=False)
        elif self.csvid == ABANDON:
            self.stats('first', 2, 2)
            self.stats('second', 2, 2)
        elif self.csvid == PRIEST:
            self.stats('victim', 2, 2)

    def aura_targets(self, *, active):
        blocked_color = AURAS[self.csvid][0]
        for color in ('black', 'white'):
            self.emit('can_target_creature', f'{color}_probe', 'host', value=int(not active or color != blocked_color))
            self.emit('can_target_creature', f'{color}_probe', 'other', value=1)

    def combat_end(self):
        self.emit('combat_damage')
        self.emit('resolve')
        self.emit('end_combat')
        self.emit('assert_choices_consumed')

    def protect_blocker(self):
        self.emit('begin_attackers')
        self.emit('attack', 'protected_blocker', value=1)
        self.emit('finish_attackers')
        self.emit('begin_blockers')
        self.emit('block', 'host', 'protected_blocker', value=1)
        self.emit('advance_combat')
        self.observe('protected_blocker', 'blocked', 1)
        self.combat_end()
        self.cards['protected_blocker']['zone'] = 'graveyard'
        self.observe('host', 'damage', 0)
        self.receipts()

    def aura(self):
        _, dp, dt, mask, _ = AURAS[self.csvid]
        self.emit('bind_permanent_copy', 'aura', player=self.actor, value=self.csvid, index=0)
        self.observe('aura', 'color', mask)
        self.stats('host', 2 + dp, 2 + dt)
        self.observe('host', 'color', 8)
        self.stats('other', 2, 2)
        self.aura_targets(active=True)
        if self.mode == 'paid_foreign':
            return
        if self.mode == 'offturn_paid':
            self.protect_blocker()
            return
        self.emit('begin_attackers')
        self.emit('attack', 'host', value=1)
        self.emit('finish_attackers')
        self.emit('begin_blockers')
        self.emit('block', 'protected_blocker', 'host', value=0)
        self.emit('block', 'red_blocker', 'host', value=1)
        self.emit('advance_combat')
        self.observe('host', 'blocked', 1)
        self.combat_end()
        self.cards['red_blocker']['zone'] = 'graveyard'
        self.observe('host', 'damage', 1)
        self.receipts()

    def abandon(self):
        attackers = ('first', 'second')
        chosen = attackers[:self.number_of_targets]
        for attacker in attackers:
            self.stats(attacker, 2 + int(attacker in chosen), 2)
        self.stats('other', 1, 2)
        pairs = [(ref, f'{ref}_blocker') for ref in attackers]
        if self.mode == 'offturn_paid':
            pairs = [(blocker, attacker) for attacker, blocker in pairs]
        self.emit('begin_attackers')
        for attacker, _ in pairs:
            self.emit('attack', attacker, value=1)
        self.emit('finish_attackers')
        self.emit('begin_blockers')
        for attacker, blocker in pairs:
            self.emit('block', blocker, attacker, value=1)
        self.emit('advance_combat')
        for attacker, _ in pairs:
            self.observe(attacker, 'blocked', 1)
        self.combat_end()
        for attacker in attackers:
            if attacker in chosen:
                self.cards[f'{attacker}_blocker']['zone'] = 'graveyard'
                self.observe(attacker, 'damage', 0)
            else:
                self.cards[attacker]['zone'] = 'graveyard'
                self.observe(f'{attacker}_blocker', 'damage', 2)
        self.receipts()
        self.emit('advance_turn')
        for attacker in chosen:
            self.stats(attacker, 2, 2)

    def priest(self):
        self.emit('bind_permanent_copy', 'played', player=self.actor, value=self.csvid, index=0)
        self.stats('played', 4, 4)
        self.observe('played', 'sick', 1)
        if self.mode == 'offturn_paid':
            self.advance(self.actor)
        else:
            self.age()
        if self.mode in ('paid', 'offturn_paid', 'replay'):
            self.emit('bind_hand_copy', 'drawn_land', player=self.actor, value=self.rows['Mountain']['csvid'], index=0)
            self.emit('play_land', 'drawn_land', value=1)
            self.cards[f'd_{self.actor}_0']['zone'] = 'battlefield'
            self.zone('Mountain', self.actor, 'battlefield', 1)
        empty = self.mode not in ('retain_draw', 'normal_nonempty')
        self.size(self.actor, 'hand', int(not empty))
        self.emit('begin_attackers')
        self.emit('attack', 'played', value=1)
        if empty:
            self.emit('choose_player', player=self.actor, value=self.foe)
        self.emit('finish_attackers')
        self.emit('begin_blockers')
        self.emit('advance_combat')
        self.observe('played', 'blocked', 0)
        self.combat_end()
        self.life[self.foe] -= 4 + (2 if empty else 0)
        self.receipts()

    def advance(self, player):
        self.emit('advance_turn')
        self.emit('untap_step', player=player)
        self.emit('turn', player=player)

    def build(self):
        self.emit('turn', player=self.actor)
        self.mana('set_mana', self.foe, ZERO)
        cut = None
        if self.mode in NORMAL or self.mode == 'normal_countered':
            self.normal_cast()
        else:
            cut = self.discard()
        if self.mode in (PAID | NORMAL):
            if self.csvid in AURAS:
                self.aura()
            elif self.csvid == ABANDON:
                self.abandon()
            else:
                self.priest()
        if cut is not None:
            suffix = self.steps[cut:]
            self.steps[cut:] = ([step('capture')] + suffix
                                + [step('record_state'), step('ai_roundtrip'), step('restore')]
                                + deepcopy(suffix) + [step('assert_recorded_state')])



def build(name, csvid, declaration, shape, variant, rows, oracle):
    mode, actor = variant.rsplit('_', 1)
    duel = Duel(csvid, oracle[name], int(actor), mode, rows, oracle)
    duel.build()
    return dict(name=f'{PREFIX}_{csvid}_{variant}', card=name, fixture='empty_main', seed=SEED,
                setup=duel.setup, steps=duel.steps,
                _recipe=dict(family=FAMILY, version=2, variant=variant, params=declaration['params'],
                             oracle_text=declaration['oracle_text'],
                             untested_components=[] if mode in (*NORMAL, 'normal_countered') else [GAP]))


def expand(name, csvid, declaration, shape, selected, rows, oracle):
    if rows[name]['csvid'] != csvid or shape['csvid'] != csvid:
        raise ValueError('exact native Madness source required')
    validate_variants(shape, selected)
    validate_witnesses(oracle, rows)
    return [build(name, csvid, declaration, shape, variant, rows, oracle) for variant in selected]
