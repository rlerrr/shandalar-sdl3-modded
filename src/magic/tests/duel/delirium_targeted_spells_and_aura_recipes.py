"""Targeted Delirium spells and riders at actual resolution and damage."""
from copy import deepcopy
from functools import lru_cache
import json
from pathlib import Path

from counter_condition_recurrence_recipes import exact
from recipes import card, cost_from_oracle, step


FAMILY = 'delirium_targeted_spells_and_aura'
PROFILE = Path(__file__).with_name('data') / 'delirium_targeted_spells_and_aura_wave8_profiles.json'
SEED = 2370
PREFIX = 'DeliriumTargetedSpellsAndAuraRecipe'
ANGEL, PICK, AURA, WHISPERS = 15734, 15916, 16202, 16239
COMMON = ('three_duplicates', 'four_types', 'opponent_only')
EXTRA = {ANGEL: ('no_enemy_creature',), PICK: ('leave_matches',),
         AURA: ('crossing', 'countered'), WHISPERS: ('one_card',)}
NOVEL = {ANGEL: 'fly_block', PICK: 'leave_partial', AURA: 'target_leaves', WHISPERS: 'mixed_hand'}
BEAR = 'Grizzly Bears'
AMULET = "Traveler's Amulet"
NONLAND = 'Shock'
RECLAIM = 'Reclaim'
ZERO = [0] * 6
THREE = ('Mountain', BEAR, 'Giant Growth', 'Mountain')
FOUR = ('Mountain', BEAR, 'Giant Growth', AMULET)
WITNESSES = set(THREE + FOUR) | {'Island', 'Plains', NONLAND, 'Hill Giant', 'Disenchant', 'Counterspell',
                                RECLAIM, 'Unsummon', 'Suntail Hawk'}
ACTIVE = {'four_types', 'no_enemy_creature', 'leave_matches', 'countered', 'one_card',
          'lose_fourth', 'replay', 'short_funding', *NOVEL.values()}
GAP = 'Angel covers combat damage to players or creatures, including flight legality. Noncombat Angel damage, planeswalker damage, and multiplayer are outside these cases.'
THRESHOLD = 'there are four or more card types among cards in your graveyard'
TEXTS = {
    ANGEL: f'Flying\nDelirium — Whenever this creature deals damage, if {THRESHOLD}, exile target creature an opponent controls.',
    PICK: ("Target opponent reveals their hand. You choose a nonland card from it and exile that card.\n"
           f"Delirium — If {THRESHOLD}, search that player's graveyard, hand, and library for any number of cards with the same name as the exiled card, exile those cards, then that player shuffles."),
    AURA: ("Enchant creature\nEnchanted creature gets +1/+1.\n"
           f"Delirium — Enchanted creature gets an additional +2/+2 as long as {THRESHOLD}."),
    WHISPERS: ("Target opponent discards a card at random.\n"
               f"Delirium — If {THRESHOLD}, that player discards two cards at random instead."),
}
TYPES = {ANGEL: 'Creature — Angel', PICK: 'Sorcery', AURA: 'Enchantment — Aura', WHISPERS: 'Sorcery'}
KEYWORDS = {ANGEL: {'Delirium', 'Flying'}, AURA: {'Delirium', 'Enchant'}}


@lru_cache(maxsize=1)
def profiles():
    return json.loads(PROFILE.read_bytes())


def classify(record, csvid):
    if (csvid not in TEXTS or record.get('layout') != 'normal' or record.get('selected_face')
            or record.get('card_faces') or record.get('type_line') != TYPES[csvid]
            or record.get('oracle_text') != TEXTS[csvid]
            or set(record.get('keywords', ())) != KEYWORDS.get(csvid, {'Delirium'})):
        raise ValueError('complete reviewed normal Delirium grammar required')
    return dict(csvid=csvid, mana=cost_from_oracle(record))


def variants(shape):
    csvid = shape['csvid']
    extra = ('short_funding', 'lose_fourth', 'replay', NOVEL[csvid])
    if csvid != AURA:
        extra += ('countered',)
    return [f'{mode}_{seat}' for mode in (*COMMON, *EXTRA[csvid], *extra) for seat in (0, 1)]


def validate_variants(shape, selected):
    if (type(selected) is not list or not selected or any(type(mode) is not str for mode in selected)
            or len(set(selected)) != len(selected) or set(selected) - set(variants(shape))):
        raise ValueError('distinct supported targeted Delirium variants required')


def spec(declaration, record):
    pinned = profiles()['sources'].get(record.get('name'))
    if not pinned or not exact(pinned['oracle'], record):
        raise ValueError('complete pinned Delirium primary required')
    shape = classify(record, pinned['registry']['csvid'])
    if (declaration.get('family') != FAMILY or type(declaration.get('version')) is not int
            or declaration['version'] != 2 or declaration.get('oracle_text') != record['oracle_text']
            or not exact(declaration.get('params'), shape)):
        raise ValueError('exact declared targeted Delirium branch required')
    validate_variants(shape, declaration.get('variants'))
    return dict(shape, family=FAMILY)


def types_for(cards, oracle):
    return {kind for name in cards for kind in oracle[name]['type_line'].split(' — ', 1)[0].split()
            if kind in {'Artifact', 'Battle', 'Creature', 'Enchantment', 'Instant',
                        'Kindred', 'Land', 'Planeswalker', 'Sorcery'}}


def validate_witnesses(oracle, rows):
    pins = profiles()
    if set(pins) != {'sources', 'witnesses'} or set(pins['witnesses']) != WITNESSES:
        raise ValueError('complete independent Delirium witnesses required')
    for group, entries in pins.items():
        for name, pin in entries.items():
            row = rows.get(name, {})
            if (not exact(oracle.get(name), pin['oracle']) or not exact(row, pin['registry'])
                    or row.get('kind') != 'card' or row.get('handler') in (None, '0', 'NULL')
                    or oracle[name].get('layout') != 'normal'):
                raise ValueError('exact Delirium primary or witness changed: ' + name)
            if group == 'sources':
                if row['handler'] == 'card_dummy':
                    raise ValueError('implemented Delirium primary required')
                classify(oracle[name], row['csvid'])
    assert len(THREE) == len(FOUR) == 4 and len(types_for(THREE, oracle)) == 3
    assert types_for(FOUR, oracle) == {'Artifact', 'Creature', 'Instant', 'Land'}
    assert types_for(THREE + ('Disenchant',), oracle) == types_for(THREE, oracle)
    assert types_for(FOUR[:-1] + (RECLAIM,), oracle) == types_for(THREE, oracle)


class Duel:
    def __init__(self, allocation, actor, mode, rows, oracle):
        self.name, self.csvid = allocation['name'], allocation['csvid']
        self.actor, self.foe, self.mode = actor, 1 - actor, mode
        self.registry, self.oracle = rows, oracle
        self.setup, self.steps, self.cards = [], [], {}
        self.life = [20, 20]
        self.active = mode in ACTIVE
        self.add('source', actor, self.name, 'hand')
        for seat, grave in enumerate(self.graves()):
            for index, name in enumerate(grave):
                self.add(f'grave_{seat}_{index}', seat, name, 'graveyard')
        for seat in (0, 1):
            for index, name in enumerate(('Island', 'Plains')):
                self.add(f'library_{seat}_{index}', seat, name, 'library')

        if self.csvid in (ANGEL, AURA):
            self.add('ally', self.actor, BEAR, 'battlefield')
            if self.mode != 'no_enemy_creature':
                self.add('target', self.foe, BEAR, 'battlefield')
            if self.csvid == ANGEL and self.mode != 'no_enemy_creature':
                self.add('other', self.foe, 'Hill Giant', 'battlefield')
        if self.csvid in (PICK, WHISPERS):
            self.add('own_hand', self.actor, NONLAND, 'hand')
            self.add('their_hand_0', self.foe, NONLAND, 'hand')
            if self.mode != 'one_card':
                self.add('their_hand_1', self.foe, 'Plains' if mode == 'mixed_hand' else NONLAND, 'hand')
        if self.csvid == PICK:
            self.add('land_in_hand', self.foe, 'Island', 'hand')
            self.add('same_name_grave', self.foe, NONLAND, 'graveyard')
            self.add('same_name_library', self.foe, NONLAND, 'library')
        if self.mode == 'crossing':
            self.add('artifact', self.actor, AMULET, 'battlefield')
            self.add('disenchant', self.actor, 'Disenchant', 'hand')
        if mode in {'countered', 'target_leaves'}:
            self.add('reply', self.foe, 'Counterspell' if mode == 'countered' else 'Unsummon', 'hand')
        if mode == 'lose_fourth':
            self.add('reclaim', self.actor, RECLAIM, 'hand')
        if mode == 'fly_block':
            self.add('flyer', self.foe, 'Suntail Hawk', 'battlefield')

    def graves(self):
        result = [(), ()]
        result[self.actor] = FOUR if self.active else THREE
        if self.mode == 'opponent_only':
            result[self.foe] = FOUR
        return result

    def add(self, ref, player, name, zone):
        self.setup.append(card(name, ref, player, zone))
        self.cards[ref] = dict(player=player, name=name, zone=zone)

    def emit(self, op, ref='', other='', **values):
        if ref:
            values['ref'] = ref
        if other:
            values['other'] = other
        self.steps.append(step(op, **values))

    def observe(self, ref, field, value):
        self.emit('assert_card', ref, property=field, value=value)

    def mana(self, op, player, pool):
        for color, amount in enumerate(pool):
            self.emit(op, player=player, color=color, value=amount)

    def cast(self, ref, destination, target=None):
        who = self.cards[ref]['player']
        payment = cost_from_oracle(self.oracle[self.cards[ref]['name']])
        self.mana('set_mana', who, payment)
        self.emit('refresh')
        if target:
            self.emit('choose_target', target, player=who)
        self.emit('cast', ref, value=1)
        self.emit('assert_choices_consumed')
        self.emit('assert_stack', value=1)
        self.mana('assert_mana', who, ZERO)
        self.emit('resolve')
        self.emit('assert_choices_consumed')
        self.emit('assert_stack')
        self.cards[ref]['zone'] = destination
        self.emit('refresh')

    def turn_ready(self):
        self.emit('turn', player=self.actor)
        self.mana('set_mana', self.foe, ZERO)
        self.cast('source', 'battlefield')
        self.observe('source', 'in_play', 1)
        self.observe('source', 'sick', 1)
        for player in (self.foe, self.actor):
            self.emit('advance_turn')
            self.emit('untap_step', player=player)
            self.emit('turn', player=player)
        self.observe('source', 'sick', 0)

    def replay_from_here(self, lifecycle):
        self.emit('capture')
        start = len(self.steps)
        lifecycle()
        tail = deepcopy(self.steps[start:])
        self.emit('record_state')
        self.emit('restore')
        self.emit('ai_roundtrip')
        self.steps.extend(tail)
        self.emit('assert_recorded_state')

    def cast_response(self, ref, name, player, *, target=None, grave_card=None):
        self.emit('announce_spell', 'source')
        self.mana('set_mana', player, cost_from_oracle(self.oracle[name]))
        self.emit('refresh')
        if target:
            self.emit('choose_target', target, player=player)
        if grave_card:
            self.emit('choose_zone_card', player=player, value=self.registry[grave_card]['csvid'])
        self.emit('cast_response', ref, value=1)
        self.emit('assert_choices_consumed')
        self.emit('assert_stack', value=2)
        self.mana('assert_mana', player, ZERO)

    def return_fourth(self, *, pending=False):
        if pending:
            self.cast_response('reclaim', RECLAIM, self.actor, grave_card=AMULET)
        else:
            self.mana('set_mana', self.actor, cost_from_oracle(self.oracle[RECLAIM]))
            self.emit('refresh')
            self.emit('choose_zone_card', player=self.actor, value=self.registry[AMULET]['csvid'])
            self.emit('cast', 'reclaim', value=1)
            self.emit('assert_choices_consumed')
            self.emit('assert_stack', value=1)
            self.emit('resolve')
            self.emit('assert_choices_consumed')
            self.emit('assert_stack')
            self.emit('refresh')
        self.active = False

    def returned_fourth(self):
        self.zone(AMULET, self.actor, 'graveyard', 0)
        self.emit('assert_zone_card', player=self.actor, zone='library', index=0,
                  value=self.registry[AMULET]['csvid'])
        self.zone(RECLAIM, self.actor, 'graveyard', 1)

    def short_funding(self):
        printed = cost_from_oracle(self.oracle[self.name])
        short = list(printed)
        color = next(index for index in range(1, len(printed)) if printed[index])
        short[color] -= 1
        self.mana('set_mana', self.actor, short)
        self.emit('refresh')
        self.emit('cast', 'source', value=0)
        self.emit('assert_choices_consumed')
        self.emit('assert_stack')
        self.mana('assert_mana', self.actor, short)
        self.mana('assert_mana', self.foe, ZERO)
        self.zone(self.name, self.actor, 'hand', 1)
        self.zone(self.name, self.actor, 'battlefield', 0)
        self.zone(self.name, self.actor, 'graveyard', 0)
        self.grave_receipts()
        self.unaffected()

    def unaffected(self):
        if self.csvid == ANGEL:
            self.stats('ally', 2, 2)
            self.stats('target', 2, 2)
            self.stats('other', 3, 3)
        elif self.csvid == AURA:
            self.stats('ally', 2, 2)
            self.stats('target', 2, 2)
        else:
            self.zone(NONLAND, self.actor, 'hand', 1)
            self.zone(NONLAND, self.foe, 'hand', 2)
            self.zone(NONLAND, self.foe, 'exile', 0)
            self.zone(NONLAND, self.foe, 'graveyard', int(self.csvid == PICK))
            self.emit('assert_shuffle_count', player=self.foe, value=0)

    def counter(self):
        self.begin_spell('target' if self.csvid == AURA else None)
        self.cast_response('reply', 'Counterspell', self.foe)
        self.finish_spell(response=True)
        self.zone('Counterspell', self.foe, 'graveyard', 1)
        if self.csvid == AURA:
            self.stats('target', 2, 2)
            self.stats('ally', 2, 2)
        else:
            self.unaffected()

    def zone(self, name, player, zone, count):
        self.emit('assert_zone_multiplicity', player=player, zone=zone,
                  index=self.registry[name]['csvid'], value=count)

    def size(self, player, zone, count):
        self.emit('assert_zone', player=player, zone=zone, value=count)

    def stats(self, ref, power, toughness):
        self.observe(ref, 'in_play', 1)
        self.observe(ref, 'power', power)
        self.observe(ref, 'toughness', toughness)

    def grave_receipts(self):
        for seat, grave in enumerate(self.graves()):
            self.size(seat, 'graveyard', len(grave) + int(seat == self.foe and self.csvid == PICK))
            for name in sorted(set((*THREE, *FOUR))):
                self.zone(name, seat, 'graveyard', grave.count(name))

    def begin_spell(self, target=None):
        self.mana('set_mana', self.actor, cost_from_oracle(self.oracle[self.name]))
        self.emit('refresh')
        if target:
            self.emit('choose_target', target, player=self.actor)
        # Both opponent spell handlers set their only target through GS_CAN_ONLY_TARGET_OPPONENT.
        self.emit('cast', 'source', value=1)
        self.emit('assert_choices_consumed')
        self.emit('assert_stack', value=1)
        self.mana('assert_mana', self.actor, ZERO)
        self.mana('assert_mana', self.foe, ZERO)

    def finish_spell(self, battlefield=False, response=False):
        self.emit('resolve_response' if response else 'resolve')
        self.emit('assert_choices_consumed')
        self.emit('assert_stack', value=0)
        self.emit('refresh')
        self.zone(self.name, self.actor, 'hand', 0)
        self.zone(self.name, self.actor, 'battlefield', int(battlefield))
        self.zone(self.name, self.actor, 'graveyard', int(not battlefield))

    def angel(self):
        self.turn_ready()
        self.stats('source', 6, 6)
        self.zone(self.name, self.actor, 'battlefield', 1)
        if self.mode == 'lose_fourth':
            self.return_fourth()
            self.returned_fourth()
        if self.mode == 'replay':
            self.replay_from_here(self.angel_combat)
        else:
            self.angel_combat()

    def angel_combat(self):
        self.emit('begin_attackers')
        self.emit('attack', 'source', value=1)
        self.emit('finish_attackers')
        self.emit('begin_blockers')
        if self.mode != 'no_enemy_creature':
            self.emit('block', 'target', 'source', value=0)
        flying_block = self.mode == 'fly_block'
        if flying_block:
            self.emit('block', 'flyer', 'source', value=1)
        self.emit('advance_combat')
        self.observe('source', 'blocked', int(flying_block))
        exiled = self.active and self.mode != 'no_enemy_creature'
        if exiled:
            self.emit('choose_target', 'target', player=self.actor)
        self.emit('combat_damage')
        self.emit('resolve')
        self.emit('end_combat')
        self.emit('assert_choices_consumed')
        self.stats('source', 6, 6)
        self.stats('ally', 2, 2)
        self.zone(BEAR, self.actor, 'exile', 0)
        self.zone(BEAR, self.foe, 'battlefield', int(not exiled and self.mode != 'no_enemy_creature'))
        self.zone(BEAR, self.foe, 'exile', int(exiled))
        if self.mode != 'no_enemy_creature':
            self.stats('other', 3, 3)
        if flying_block:
            self.zone('Suntail Hawk', self.foe, 'graveyard', 1)
            self.observe('source', 'damage', 1)
        self.life[self.foe] = 20 if flying_block else 14

    def pick(self):
        self.begin_spell()
        if self.mode == 'lose_fourth':
            self.return_fourth(pending=True)
        if self.mode == 'replay':
            self.replay_from_here(self.pick_resolve)
        else:
            self.pick_resolve()

    def pick_resolve(self):
        chosen_id = self.registry[NONLAND]['csvid']
        self.emit('observe_search_filters', value=1)
        self.emit('choose_zone_card', player=self.actor, value=chosen_id)
        zones = ('hand', 'graveyard', 'library')
        take = {zone: self.active and self.mode != 'leave_matches'
                and (zone != 'graveyard' or self.mode != 'leave_partial') for zone in zones}
        if self.active:
            for zone in zones:
                if take[zone]:
                    self.emit('choose_zone_card', player=self.actor, value=chosen_id)
                else:
                    self.emit('choose_zone', player=self.actor, value=-1)
        self.finish_spell(response=self.mode == 'lose_fourth')
        # Report legality from the final library menu with Delirium and the initial
        # mandatory opponent-hand menu below threshold.
        self.emit('assert_offered_count', value=3)
        self.emit('assert_offered_legal_card', index=chosen_id, value=1)
        self.emit('assert_offered_legal_card', index=self.registry['Island']['csvid'], value=0)
        self.emit('observe_search_filters', value=0)
        self.zone(NONLAND, self.actor, 'hand', 1)
        self.zone('Island', self.foe, 'hand', 1)
        for zone in zones:
            self.zone(NONLAND, self.foe, zone, int(not take[zone]))
        self.zone(NONLAND, self.foe, 'exile', 1 + sum(take.values()))
        self.size(self.foe, 'hand', 2 - take['hand'])
        self.size(self.foe, 'library', 3 - take['library'])
        self.zone('Island', self.foe, 'library', 1)
        self.zone('Plains', self.foe, 'library', 1)
        self.emit('assert_shuffle_count', player=self.foe, value=int(self.active))
        self.emit('assert_shuffle_count', player=self.actor, value=0)
        if self.mode == 'lose_fourth':
            self.returned_fourth()

    def aura(self):
        self.begin_spell('target')
        if self.mode == 'target_leaves':
            self.cast_response('reply', 'Unsummon', self.foe, target='target')
            self.finish_spell(response=True)
            self.zone('Unsummon', self.foe, 'graveyard', 1)
            self.zone(BEAR, self.foe, 'hand', 1)
            self.zone(BEAR, self.foe, 'battlefield', 0)
            self.stats('ally', 2, 2)
        elif self.mode == 'replay':
            self.replay_from_here(self.aura_attaches)
        else:
            self.aura_attaches()

    def aura_attaches(self):
        self.finish_spell(battlefield=True)
        self.observe('source', 'in_play', 1)
        power = 5 if self.active else 3
        self.stats('target', power, power)
        if self.mode == 'crossing':
            self.cast('disenchant', 'graveyard', target='artifact')
            self.zone(AMULET, self.actor, 'graveyard', 1)
            self.zone('Disenchant', self.actor, 'graveyard', 1)
            self.zone(AMULET, self.actor, 'battlefield', 0)
            self.stats('target', 5, 5)
        if self.mode == 'lose_fourth':
            self.return_fourth()
            self.returned_fourth()
            self.stats('target', 3, 3)
        self.stats('ally', 2, 2)

    def whispers(self):
        self.begin_spell()
        if self.mode == 'lose_fourth':
            self.return_fourth(pending=True)
        if self.mode == 'replay':
            self.replay_from_here(self.whispers_resolve)
        else:
            self.whispers_resolve()

    def whispers_resolve(self):
        self.finish_spell(response=self.mode == 'lose_fourth')
        hand_count = 1 if self.mode == 'one_card' else 2
        discarded = min(hand_count, 2 if self.active else 1)
        mixed = self.mode == 'mixed_hand'
        self.zone(NONLAND, self.foe, 'hand', hand_count - discarded)
        self.zone(NONLAND, self.foe, 'graveyard', discarded - mixed)
        self.zone(NONLAND, self.foe, 'exile', 0)
        if mixed:
            self.zone('Plains', self.foe, 'hand', 0)
            self.zone('Plains', self.foe, 'graveyard', 1)
        self.size(self.foe, 'hand', hand_count - discarded)
        self.zone(NONLAND, self.actor, 'hand', 1)
        self.zone(NONLAND, self.actor, 'graveyard', 0)
        if self.mode == 'lose_fourth':
            self.returned_fourth()

    def build(self):
        self.emit('turn', player=self.actor)
        self.mana('set_mana', self.foe, ZERO)
        self.grave_receipts()
        if self.mode == 'short_funding':
            self.short_funding()
            return
        if self.mode == 'countered':
            self.counter()
        else:
            {ANGEL: self.angel, PICK: self.pick, AURA: self.aura, WHISPERS: self.whispers}[self.csvid]()
        for seat, value in enumerate(self.life):
            self.emit('assert_life', player=seat, value=value)
        self.mana('assert_mana', self.actor, ZERO)
        self.mana('assert_mana', self.foe, ZERO)



def build(name, csvid, declaration, shape, variant, rows, oracle):
    mode, seat = variant.rsplit('_', 1)
    duel = Duel(dict(name=name, csvid=csvid), int(seat), mode, rows, oracle)
    duel.build()
    return dict(name=f'{PREFIX}_{csvid}_{variant}', card=name, fixture='empty_main', seed=SEED,
                setup=duel.setup, steps=duel.steps,
                _recipe=dict(family=FAMILY, version=2, variant=variant, params=declaration['params'],
                             oracle_text=declaration['oracle_text'], untested_components=[GAP]))


def expand(name, csvid, declaration, shape, selected, rows, oracle):
    if rows[name]['csvid'] != csvid or shape['csvid'] != csvid:
        raise ValueError('exact native Delirium source required')
    validate_variants(shape, selected)
    validate_witnesses(oracle, rows)
    return [build(name, csvid, declaration, shape, variant, rows, oracle) for variant in selected]
