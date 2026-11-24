"""Pinned entry interaction, linked exile and paid follow-on effects."""

from collections import Counter
from functools import lru_cache
import json
from pathlib import Path

from death_counter_trigger_recipes import exact
from recipes import cost_from_oracle

FAMILY = 'entry_interaction_zone_movement'
PROFILE = Path(__file__).with_name('data') / f'{FAMILY}_wave8_profiles.json'
LABEL = 'EntryInteractionZoneMovementRecipe'
SEED = 3404
PRISON, RAZER, GRASP, FAITH = 1977, 10040, 15517, 16095
MODES = {
    PRISON: ('owner_pays', 'opponent_pays', 'both_decline', 'short_mana', 'returned_owner', 'returned_etb'),
    RAZER: ('all_lands', 'no_lands', 'borrowed_land', 'bounce_source', 'land_token'),
    GRASP: ('artifact', 'enchantment', 'decline', 'no_enemy_nonland', 'returned_owner'),
    FAITH: ('double_target', 'host_dies', 'no_own_target', 'no_enemy_target', 'lost_host', 'returned_owner'),
}
ZONE = dict(hand=0, battlefield=1, library=2, graveyard=3, exile=4)
ZERO = (0,) * 6



ORIGINAL = frozenset(MODES)
DANCE, DAY, DRAKE, HOARDING, HYPNOX, OUBLIETTE = 309, 6500, 4144, 11130, 5822, 447
QUARRY, QUARANTINE, HARBINGER, PETRADON, AVEN, FLEETS = 13955, 15406, 15145, 5857, 14519, 13970
MODES.update({
    DANCE: ('source_leaves', 'token_leaves', 'paid_upkeep', 'unpaid_upkeep', 'token_only'),
    DAY: ('both_controllers', 'borrowed_owner', 'sacrifice_unrelated_dragon', 'empty', 'tokens_fly'),
    DRAKE: ('own_aura', 'foreign_aura', 'two_graves', 'flying', 'no_aura'),
    HOARDING: ('death_return', 'death_decline', 'bounce', 'flying', 'fail_to_find'),
    HYPNOX: ('paid_hand', 'from_grave', 'empty_hand', 'flying'),
    OUBLIETTE: ('ordinary', 'attachments', 'through_untap', 'no_creature'),
    QUARRY: ('top', 'beneath_one', 'short_library', 'borrowed_owner'),
    QUARANTINE: ('zero', 'one', 'two', 'up_to_one', 'opponent_only'),
    HARBINGER: ('normal', 'flash', 'flash_short', 'decline', 'untapped_only'),
    PETRADON: ('two_lands', 'only_one_land', 'no_lands', 'pump'),
    AVEN: ('grow', 'bounce', 'borrowed_owner', 'self_bounce', 'flying'),
    FLEETS: ('two_islands', 'no_islands', 'borrowed_owner', 'one_island'),
})


def source_cost(record, csvid):
    if csvid == HYPNOX and record['mana_cost'] == '{8}{B}{B}{B}':
        return [8, 3, 0, 0, 0, 0]
    if csvid == QUARANTINE and record['mana_cost'] == '{X}{X}{W}{W}':
        return [0, 0, 0, 0, 0, 2]
    return cost_from_oracle(record)


@lru_cache(maxsize=1)
def pins():
    return json.loads(PROFILE.read_bytes())


def classify(record, csvid):
    pin = pins()['sources'].get(record.get('name'))
    if (csvid not in MODES or not pin or pin['registry']['csvid'] != csvid
            or not exact(record, pin['oracle']) or record.get('layout') != 'normal'):
        raise ValueError('complete pinned entry zone-movement rules required')
    return dict(csvid=csvid, cost=source_cost(record, csvid))


def variants(shape):
    return [f'{mode}_{seat}' for mode in MODES[shape['csvid']] for seat in (0, 1)]


def spec(declaration, record):
    pin = pins()['sources'].get(record.get('name'))
    if not pin:
        raise ValueError('pinned entry zone-movement source required')
    shape = classify(record, pin['registry']['csvid'])
    chosen = declaration.get('variants')
    if (declaration.get('family') != FAMILY or type(declaration.get('version')) is not int
            or declaration['version'] != 2 or declaration.get('oracle_text') != record['oracle_text']
            or not exact(declaration.get('params'), shape)):
        raise ValueError('whole versioned entry zone-movement declaration required')
    if (type(chosen) is not list or not chosen or any(type(v) is not str for v in chosen)
            or len(chosen) != len(set(chosen)) or set(chosen) - set(variants(shape))):
        raise ValueError('explicit distinct entry zone-movement variants required')
    return dict(shape, family=FAMILY)


def validate_witnesses(oracle, rows):
    for group in ('sources', 'witnesses'):
        for name, pin in pins()[group].items():
            row = rows.get(name, {})
            if (not exact(oracle.get(name), pin['oracle']) or not exact(row, pin['registry'])
                    or row.get('kind') not in ('card', 'token') or row.get('handler') in (None, 'NULL', '0')
                    or group == 'sources' and (row['kind'] != 'card' or row['handler'] == 'card_dummy')):
                raise ValueError(f'changed entry zone-movement identity: {name}')


class Scene:
    def __init__(self, row, actor, mode, rows, oracle):
        self.cid, self.name = row['csvid'], row['name']
        self.actor, self.foe, self.mode = actor, 1 - actor, mode
        self.rows, self.oracle = rows, oracle
        self.setup, self.steps, self.cards = [], [], {}
        self.zones = {p: {zone: [] for zone in ZONE} for p in (0, 1)}
        self.pools = [ZERO, ZERO]
        self.life = [20, 20]
        self.shuffles = [0, 0]
        self.add('source', self.name, actor, 'hand')
        self.add('own_top', 'Plains', actor, 'library')
        self.add('foe_top', 'Mountain', self.foe, 'library')

    def emit(self, op, ref='', *, other='', prop='', player=0, value=0, zone='hand', index=0, color=0):
        assert player in (0, 1)
        self.steps.append((op, ref, other, prop, player, value, ZONE[zone], index, color))

    def add(self, ref, name, seat, zone, state=0):
        assert ref not in self.cards
        self.setup.append((ref, seat, ZONE[zone], self.rows[name]['csvid'], state))
        self.cards[ref] = name, seat, zone
        self.zones[seat][zone].append(ref)

    def move(self, ref, zone, *, seat=None):
        name, old_seat, old_zone = self.cards[ref]
        self.zones[old_seat][old_zone].remove(ref)
        new_seat = old_seat if seat is None else seat
        if zone != 'vanished':
            self.zones[new_seat][zone].append(ref)
        self.cards[ref] = name, new_seat, zone

    def prop(self, ref, key, value):
        self.emit('assert_card', ref, prop=key, value=value)

    def mana(self, player, amounts=ZERO, *, write=False):
        for color, amount in enumerate(amounts):
            self.emit('set_mana' if write else 'assert_mana', player=player, value=amount, color=color)

    def choose_target(self, ref):
        self.emit('choose_target', ref, player=self.actor)

    def begin_cast(self, ref='source', *, target=None, choices=None, payment=None):
        name, player, zone = self.cards[ref]
        assert zone == 'hand'
        fee = source_cost(self.oracle[name], self.cid) if ref == 'source' else cost_from_oracle(self.oracle[name])
        self.mana(player, fee if payment is None else payment, write=True)
        self.emit('refresh')
        if target:
            self.emit('choose_target', target, player=player)
        if choices:
            choices()
        self.emit('cast', ref, value=1)
        self.emit('assert_unused_choices', value=0)
        self.emit('assert_stack', value=1)
        self.prop(ref, 'in_hand', 0)
        self.mana(player)

    def resolve(self, *, unused=0):
        self.emit('resolve')
        self.emit('assert_unused_choices', value=unused)
        self.emit('assert_stack', value=0)
        self.emit('refresh')

    def stats(self, ref, *, damage=None, power_delta=0, toughness_delta=0):
        name, _, zone = self.cards[ref]
        assert zone == 'battlefield'
        self.prop(ref, 'in_play', 1)
        self.prop(ref, 'csvid', self.rows[name]['csvid'])
        self.prop(ref, 'power', int(self.oracle[name]['power']) + power_delta)
        self.prop(ref, 'toughness', int(self.oracle[name]['toughness']) + toughness_delta)
        if damage is not None:
            self.prop(ref, 'damage', damage)

    def borrow(self, ref):
        name, seat, zone = self.cards[ref]
        assert zone == 'battlefield' and seat == self.foe
        self.add('theft', 'Act of Treason', self.actor, 'hand')
        self.begin_cast('theft', target=ref)
        self.resolve()
        self.move('theft', 'graveyard')
        self.move(ref, 'battlefield', seat=self.actor)
        self.emit('bind_permanent_copy', 'borrowed', player=self.actor, index=0, value=self.rows[name]['csvid'])
        self.prop('borrowed', 'in_play', 1)
        self.prop('borrowed', 'csvid', self.rows[name]['csvid'])
        return 'borrowed'

    def next_own_main(self):
        for seat in (self.foe, self.actor):
            self.emit('advance_turn')
            self.emit('untap_step', player=seat)
        self.emit('turn', player=self.actor)

    def receipts(self):
        self.emit('assert_stack', value=0)
        for p, zones in self.zones.items():
            self.mana(p, self.pools[p])
            self.emit('assert_life', player=p, value=self.life[p])
            self.emit('assert_shuffle_count', player=p, value=self.shuffles[p])
            for zone, slots in zones.items():
                names = [self.cards[ref][0] for ref in slots]
                counts = Counter(names)
                self.emit('assert_zone', player=p, zone=zone, value=len(names))
                for name in sorted({name for name, _, _ in self.cards.values()}):
                    self.emit('assert_zone_multiplicity', player=p, zone=zone, index=self.rows[name]['csvid'],
                              value=counts[name])
                if zone == 'library' and not self.shuffles[p]:
                    for index, name in enumerate(names):
                        self.emit('assert_zone_card', player=p, zone=zone, index=index, value=self.rows[name]['csvid'])

    def id(self, ref):
        return self.rows[self.cards[ref][0]]['csvid']

    def pick_zone(self, ref, *, chooser=None, optional=False):
        self.emit('choose_optional_zone' if optional else 'choose_zone_card',
                  player=self.actor if chooser is None else chooser, value=self.id(ref))

    def bind(self, ref, controller):
        self.emit('bind_permanent', ref, player=controller, value=self.id(ref), index=-1)
        self.prop(ref, 'in_play', 1)
        self.prop(ref, 'csvid', self.id(ref))

    def cast_witness(self, ref, destination, *, target=None):
        self.begin_cast(ref, target=target)
        self.resolve()
        self.move(ref, destination)

    def enter(self, choice=None, *, unused=0):
        self.begin_cast()
        if choice:
            choice()
        self.resolve(unused=unused)
        self.move('source', 'battlefield')
        self.prop('source', 'in_play', 1)

    def optional_etb(self):
        if self.actor == 0:
            self.emit('choose_trigger', 'source', player=0)

    def take_under_opponent_control(self, ref):
        name, owner, zone = self.cards[ref]
        assert owner == self.actor and zone == 'battlefield'
        self.add('control', 'Control Magic', self.foe, 'hand')
        self.emit('turn', player=self.foe)
        self.cast_witness('control', 'battlefield', target=ref)
        self.move(ref, 'battlefield', seat=self.foe)
        self.emit('bind_permanent', 'stolen', player=self.foe, value=self.rows[name]['csvid'], index=-1)
        self.prop('stolen', 'in_play', 1)
        self.prop('stolen', 'csvid', self.rows[name]['csvid'])
        self.emit('turn', player=self.actor)
        return 'stolen'

    def upkeep(self, player):
        self.emit('upkeep_step', player=player)
        self.emit('assert_stack', value=0)
        self.emit('assert_unused_choices', value=0)
        self.emit('refresh')

    def prison(self):
        mode = self.mode
        own_pay = mode in ('owner_pays', 'both_decline')
        foe_pay = mode in ('opponent_pays', 'both_decline')
        funded = {
            self.actor: 3 if own_pay else 2 if mode == 'short_mana' else 0,
            self.foe: 3 if foe_pay else 0,
        }
        producers = {}
        for player, count in funded.items():
            producers[player] = [f'producer_{player}_{i}' for i in range(count)]
            for ref in producers[player]:
                self.add(ref, 'Forest', player, 'battlefield')
        owner = self.actor if mode == 'returned_etb' else self.foe
        if mode == 'returned_etb':
            self.add('target', 'Cathedral Sanctifier', owner, 'hand')
        else:
            self.add('target', 'Hill Giant', owner, 'battlefield')
        self.add('foe_foil', 'Grizzly Bears', self.foe, 'battlefield')
        self.add('relic', 'Darksteel Relic', self.actor, 'battlefield')
        persists = mode in ('owner_pays', 'opponent_pays')
        if persists:
            self.add('remover', 'Disenchant', self.actor, 'hand')
        if mode == 'returned_etb':
            self.cast_witness('target', 'battlefield')
            self.life[owner] += 3
        target = self.borrow('target') if mode == 'returned_owner' else 'target'
        self.enter(lambda: self.choose_target(target))
        self.move('target', 'exile', seat=owner)
        self.receipts()
        self.emit('advance_turn')
        self.emit('untap_step', player=self.foe)
        self.upkeep(self.foe)
        self.receipts()
        self.emit('advance_turn')
        self.emit('untap_step', player=self.actor)
        for player in (self.actor, self.foe):
            if funded[player] == 3:
                pay = mode != 'both_decline'
                self.emit('choose_dialog', player=player, value=int(not pay))
                if pay and player == 0:
                    for ref in producers[player]:
                        self.emit('choose_mana_source', ref)
        self.upkeep(self.actor)
        for player, refs in producers.items():
            paid = mode == ('owner_pays' if player == self.actor else 'opponent_pays')
            for ref in refs:
                self.prop(ref, 'tapped', int(paid))
        if persists:
            self.receipts()
            self.emit('turn', player=self.actor)
            self.cast_witness('remover', 'graveyard', target='source')
        self.move('source', 'graveyard')
        self.move('target', 'battlefield')
        if mode == 'returned_etb':
            self.life[owner] += 3
        self.receipts()
        self.bind('target', owner)
        self.stats('target')

    def razer(self):
        mode = self.mode
        land = []
        self.add('own_creature', 'Grizzly Bears', self.actor, 'battlefield')
        self.add('other_artifact', 'Darksteel Relic', self.foe, 'battlefield')
        self.add('prior_exile', 'Swamp', self.foe, 'exile')
        if mode != 'no_lands':
            land = [('island', 'Island', self.actor, 0), ('mountain', 'Mountain', self.foe, 0)]
            if mode == 'all_lands':
                land.extend((('forest', 'Forest', self.actor, 1), ('plains', 'Plains', self.foe, 0)))
            if mode == 'borrowed_land':
                land.append(('dryad', 'Dryad Arbor', self.foe, 0))
            if mode == 'land_token':
                land.append(('land_copy', 'Forest', self.foe, 32))
        for ref, name, owner, state in land:
            self.add(ref, name, owner, 'battlefield', state)
        bounce = mode == 'bounce_source'
        self.add('remover', 'Unsummon' if bounce else 'Murder', self.actor, 'hand')
        if mode == 'borrowed_land':
            self.borrow('dryad')
        self.enter()
        self.stats('source')
        for ref, _, owner, state in land:
            self.move(ref, 'vanished' if state == 32 else 'exile', seat=owner)
        self.receipts()
        self.cast_witness('remover', 'graveyard', target='source')
        self.move('source', 'hand' if bounce else 'graveyard')
        for ref, _, owner, state in land:
            if state != 32:
                self.move(ref, 'battlefield', seat=owner)
        self.receipts()
        for ref, _, owner, state in land:
            if state != 32:
                self.bind(ref, owner)
                self.prop(ref, 'tapped', 1)
        if mode == 'all_lands':
            self.next_own_main()
            self.prop('island', 'tapped', 0)
            self.emit('can_activate', 'island', value=1)
            self.emit('activate', 'island', value=1)
            self.prop('island', 'tapped', 1)
            self.pools[self.actor] = (0, 0, 1, 0, 0, 0)

    def grasp(self):
        mode = self.mode
        self.add('own_artifact', 'Sol Ring', self.actor, 'battlefield')
        self.add('foe_land', 'Island', self.foe, 'battlefield')
        valid = mode != 'no_enemy_nonland'
        if valid:
            owner = self.actor if mode == 'returned_owner' else self.foe
            witness = {'artifact': 'Darksteel Relic', 'enchantment': 'Fervor',
                       'decline': 'Darksteel Relic', 'returned_owner': 'Hill Giant'}[mode]
            self.add('target', witness, owner, 'battlefield')
            self.add('foe_foil', 'Grizzly Bears', self.foe, 'battlefield')
        target = self.take_under_opponent_control('target') if mode == 'returned_owner' else 'target'
        selected = valid and mode != 'decline'
        if selected:
            self.add('remover', 'Disenchant', self.actor, 'hand')

        def choose():
            self.optional_etb()
            if mode == 'decline':
                self.emit('choose_optional_target', target, player=self.actor)
            elif selected:
                self.choose_target(target)

        self.enter(choose)
        if mode == 'decline':
            self.emit('assert_forced_target_choices', value=0)
        if selected:
            self.move('target', 'exile', seat=owner)
            if mode == 'returned_owner':
                self.move('control', 'graveyard')
            self.receipts()
            self.cast_witness('remover', 'graveyard', target='source')
            self.move('source', 'graveyard')
            self.move('target', 'battlefield', seat=owner)
            self.receipts()
            self.bind('target', owner)

    def faith(self):
        mode = self.mode
        self.add('own_artifact', 'Darksteel Relic', self.actor, 'battlefield')
        if mode != 'no_enemy_target':
            self.add('enemy_foil', 'Walking Corpse', self.foe, 'battlefield')
        self.add('foe_land', 'Island', self.foe, 'battlefield')
        if mode == 'no_own_target':
            self.pools[self.actor] = cost_from_oracle(self.oracle[self.name])
            self.mana(self.actor, self.pools[self.actor], write=True)
            self.emit('refresh')
            self.emit('can_cast', 'source', value=0)
            self.emit('cast', 'source', value=0)
            self.emit('assert_unused_choices', value=0)
            return
        self.add('host', 'Grizzly Bears', self.actor, 'battlefield')
        self.add('friendly_foil', 'Rumbling Baloth', self.actor, 'battlefield')
        selected = mode not in ('no_enemy_target', 'lost_host')
        owner = self.actor if mode == 'returned_owner' else self.foe
        if selected:
            self.add('target', 'Hill Giant', owner, 'battlefield')
        ref = self.take_under_opponent_control('target') if mode == 'returned_owner' else 'target'
        if mode == 'lost_host':
            self.add('response', 'Unsummon', self.foe, 'hand')
        else:
            self.add('remover', 'Murder' if mode == 'host_dies' else 'Disenchant', self.actor, 'hand')
        self.begin_cast(target='host')
        if mode == 'lost_host':
            self.emit('announce_spell', 'source')
            self.mana(self.foe, cost_from_oracle(self.oracle['Unsummon']), write=True)
            self.emit('refresh')
            self.emit('choose_target', 'host', player=self.foe)
            self.emit('cast_response', 'response', value=1)
            self.emit('assert_unused_choices', value=0)
            self.emit('assert_stack', value=2)
            self.mana(self.foe)
            self.emit('resolve_response')
            self.emit('assert_stack', value=0)
            self.emit('assert_unused_choices', value=0)
            self.emit('refresh')
            self.move('host', 'hand')
            self.move('source', 'graveyard')
            self.move('response', 'graveyard')
            self.stats('friendly_foil')
            return
        if selected:
            self.choose_target(ref)
        self.resolve()
        self.move('source', 'battlefield')
        self.prop('source', 'in_play', 1)
        self.stats('host', power_delta=2, toughness_delta=2)
        self.stats('friendly_foil')
        if selected:
            self.move('target', 'exile', seat=owner)
            if mode == 'returned_owner':
                self.move('control', 'graveyard')
        self.receipts()
        dies = mode == 'host_dies'
        self.cast_witness('remover', 'graveyard', target='host' if dies else 'source')
        self.move('source', 'graveyard')
        if dies:
            self.move('host', 'graveyard')
        else:
            self.stats('host')
        if selected:
            self.move('target', 'battlefield')
            self.receipts()
            self.bind('target', owner)
            self.stats('target')

    def play(self):
        self.emit('turn', player=self.actor)
        self.emit('present_ai_dialogs', value=1)
        for seat in (0, 1):
            self.mana(seat, write=True)
        cases = {PRISON: self.prison, RAZER: self.razer, GRASP: self.grasp, FAITH: self.faith}
        if self.cid in cases:
            cases[self.cid]()
        else:
            self.follow_on()
        self.receipts()
        self.emit('present_ai_dialogs', value=0)


    def create_token(self, ref, name, player):
        self.cards[ref] = name, player, 'battlefield'
        self.zones[player]['battlefield'].append(ref)

    def flight(self, weak='ground', strong=None, *, attacker='source'):
        self.next_own_main()
        self.prop(attacker, 'sick', 0)
        self.emit('begin_attackers')
        self.emit('attack', attacker, value=1)
        self.emit('finish_attackers')
        self.emit('begin_blockers')
        self.emit('block', weak, other=attacker, value=0)
        if strong:
            self.emit('block', strong, other=attacker, value=1)
        self.emit('advance_combat')
        self.prop(attacker, 'blocked', int(bool(strong)))
        self.emit('combat_damage')
        self.resolve()
        self.emit('end_combat')

    def ground_witness(self, reach=False):
        self.add('ground', 'Grizzly Bears', self.foe, 'battlefield')
        if reach:
            self.add('reach', 'Giant Spider', self.foe, 'battlefield')

    def dance(self):
        mode = self.mode
        initial_token = mode == 'token_only'
        self.add('model', 'Hill Giant', self.foe, 'battlefield', state=32 if initial_token else 0)
        if mode in ('source_leaves', 'token_leaves', 'paid_upkeep'):
            remover = 'Murder' if mode == 'token_leaves' else 'Disenchant'
            self.add('remove', remover, self.actor, 'hand')
        if mode == 'paid_upkeep':
            for ref in ('island_a', 'island_b'):
                self.add(ref, 'Island', self.actor, 'battlefield')
        if initial_token:
            self.enter(lambda: self.emit('choose_optional_target', 'model', player=self.actor), unused=1)
            self.emit('assert_forced_target_choices', value=0)
            return
        self.enter(lambda: self.choose_target('model'))
        self.create_token('copy', 'Hill Giant', self.actor)
        self.bind('copy', self.actor)
        self.stats('copy')
        self.emit('assert_matching_permanents', prop='token', player=self.actor, index=self.id('copy'), value=1)
        self.receipts()
        if mode in ('paid_upkeep', 'unpaid_upkeep'):
            self.emit('advance_turn')
            self.emit('untap_step', player=self.foe)
            self.upkeep(self.foe)
            self.emit('advance_turn')
            self.emit('untap_step', player=self.actor)
            if mode == 'paid_upkeep':
                self.emit('choose_dialog', player=self.actor, value=0)
                if self.actor == 0:
                    for ref in ('island_a', 'island_b'):
                        self.emit('choose_mana_source', ref)
            self.upkeep(self.actor)
            if mode == 'paid_upkeep':
                self.prop('island_a', 'tapped', 1)
                self.prop('island_b', 'tapped', 1)
                self.receipts()
                self.emit('turn', player=self.actor)
        if mode != 'unpaid_upkeep':
            self.cast_witness('remove', 'graveyard', target='copy' if mode == 'token_leaves' else 'source')
        self.move('source', 'graveyard')
        self.move('copy', 'vanished')

    def day(self):
        mode = self.mode
        self.add('foe_creature', 'Grizzly Bears', self.foe, 'battlefield')
        self.add('actor_relic', 'Darksteel Relic', self.actor, 'battlefield')
        self.add('remover', 'Disenchant', self.actor, 'hand')
        targets = []
        if mode == 'borrowed_owner':
            self.add('first', 'Hill Giant', self.foe, 'battlefield')
            self.add('owner_probe', 'Unsummon', self.actor, 'hand')
            self.borrow('first')
            targets.append(('first', self.foe))
        elif mode != 'empty':
            self.add('first', 'Hill Giant', self.actor, 'battlefield')
            self.add('second', 'Walking Corpse', self.actor, 'battlefield')
            targets.extend((('first', self.actor), ('second', self.actor)))
        if mode == 'sacrifice_unrelated_dragon':
            self.add('ordinary_dragon', 'Shivan Dragon', self.actor, 'hand')
            self.add('foe_dragon', 'Shivan Dragon', self.foe, 'battlefield')
        self.enter()
        for index, (ref, owner) in enumerate(targets):
            self.move(ref, 'exile', seat=owner)
            self.create_token(f'dragon_{index}', 'Dragon Spirit', self.actor)
        if targets:
            self.emit('assert_matching_permanents', prop='subtype_Dragon', player=self.actor,
                      index=self.rows['Dragon Spirit']['csvid'], value=1)
        for index in range(len(targets)):
            ref = f'dragon_{index}'
            self.emit('bind_permanent_copy', ref, player=self.actor, value=self.id(ref), index=index)
            self.prop(ref, 'power', 5)
            self.prop(ref, 'toughness', 5)
            self.prop(ref, 'color', 1 << 4)
        self.receipts()
        if mode == 'sacrifice_unrelated_dragon':
            self.cast_witness('ordinary_dragon', 'battlefield')
            self.stats('ordinary_dragon')
        if mode == 'tokens_fly':
            self.flight('foe_creature', attacker='dragon_0')
            self.life[self.foe] -= 5
        self.cast_witness('remover', 'graveyard', target='source')
        self.move('source', 'graveyard')
        for index, (ref, _) in enumerate(targets):
            self.move(f'dragon_{index}', 'vanished')
            self.move(ref, 'battlefield', seat=self.actor)
        if mode == 'borrowed_owner':
            self.receipts()
            self.bind('first', self.actor)
            self.cast_witness('owner_probe', 'graveyard', target='first')
            self.move('first', 'hand', seat=self.foe)
        if mode == 'sacrifice_unrelated_dragon':
            self.move('ordinary_dragon', 'graveyard')

    def drake(self):
        mode = self.mode
        foreign = mode in ('foreign_aura', 'two_graves')
        owner = self.foe if foreign else self.actor
        selected = mode != 'no_aura'
        if selected:
            self.add('aura', 'Unholy Strength' if foreign else 'Holy Strength', owner, 'graveyard')
        self.add('wrong_enchantment', 'Fervor', owner, 'graveyard')
        if mode == 'two_graves':
            self.add('other_aura', 'Holy Strength', self.actor, 'graveyard')
        if mode == 'flying':
            self.ground_witness()

        def choose():
            if selected:
                if mode == 'two_graves':
                    self.emit('choose_player', player=self.actor, value=owner)
                self.pick_zone('aura')

        self.enter(choose)
        if selected:
            self.move('aura', 'battlefield', seat=self.actor)
            self.bind('aura', self.actor)
        self.stats('source', power_delta=(2 if foreign else 1) if selected else 0,
                   toughness_delta=(1 if foreign else 2) if selected else 0)
        if mode == 'flying':
            self.flight()
            self.life[self.foe] -= 3

    def hoarding(self):
        mode = self.mode
        self.add('artifact', 'Darksteel Relic', self.actor, 'library')
        self.add('non_artifact', 'Grizzly Bears', self.actor, 'library')
        self.add('unrelated_exile', 'Sol Ring', self.foe, 'exile')
        if mode in ('death_return', 'death_decline', 'bounce'):
            self.add('remover', 'Unsummon' if mode == 'bounce' else 'Murder', self.actor, 'hand')
        if mode == 'flying':
            self.ground_witness(reach=True)

        def choose():
            self.optional_etb()
            self.pick_zone('artifact', optional=mode == 'fail_to_find')

        self.enter(choose)
        if mode != 'fail_to_find':
            self.move('artifact', 'exile')
        self.shuffles[self.actor] += 1
        self.stats('source')
        if mode == 'fail_to_find':
            self.emit('assert_forced_zone_choices', value=0)
            return
        self.receipts()
        if mode != 'flying':
            self.begin_cast('remover', target='source')
            if mode != 'bounce':
                self.emit('choose_dialog', player=self.actor, value=int(mode == 'death_decline'))
            self.resolve()
            self.move('remover', 'graveyard')
            self.move('source', 'hand' if mode == 'bounce' else 'graveyard')
            if mode == 'death_return':
                self.move('artifact', 'hand')
        else:
            self.flight('ground', 'reach')
            self.move('reach', 'graveyard')
            self.stats('source', damage=2)

    def hypnox(self):
        mode = self.mode
        victims = ()
        if mode not in ('flying', 'empty_hand'):
            victims = ('foe_spell', 'foe_land')
            self.add('foe_spell', 'Lightning Bolt', self.foe, 'hand')
            self.add('foe_land', 'Forest', self.foe, 'hand')
        self.add('own_foil', 'Unsummon', self.actor, 'hand')
        if mode == 'from_grave':
            index, = (i for i, item in enumerate(self.setup) if item[0] == 'source')
            previous = self.setup[index]
            self.setup[index] = previous[:2] + (3,) + previous[3:]
            self.move('source', 'graveyard')
            self.add('reanimate', 'Reanimate', self.actor, 'hand')
            self.begin_cast('reanimate', choices=lambda: self.pick_zone('source'))
            self.resolve()
            self.move('reanimate', 'graveyard')
            self.move('source', 'battlefield')
            self.life[self.actor] -= 11
            self.bind('source', self.actor)
            self.stats('source')
            return
        self.add('remover', 'Murder', self.actor, 'hand')
        if mode == 'flying':
            self.ground_witness(reach=True)
        self.enter()
        for ref in victims:
            self.move(ref, 'exile')
        self.stats('source')
        self.receipts()
        if mode == 'flying':
            self.flight('ground', 'reach')
            self.move('reach', 'graveyard')
            self.stats('source', damage=2)
        self.cast_witness('remover', 'graveyard', target='source')
        self.move('source', 'graveyard')
        for ref in victims:
            self.move(ref, 'hand')

    def oubliette(self):
        mode = self.mode
        empty = mode == 'no_creature'
        self.add('remover', 'Disenchant', self.actor, 'hand')
        if not empty:
            self.add('target', 'Hill Giant', self.foe, 'battlefield')
        if mode == 'attachments':
            self.add('aura', 'Holy Strength', self.actor, 'hand')
            self.add('equipment', 'Bonesplitter', self.foe, 'battlefield')
            self.cast_witness('aura', 'battlefield', target='target')
            self.emit('turn', player=self.foe)
            self.mana(self.foe, (1, 0, 0, 0, 0, 0), write=True)
            self.emit('refresh')
            self.emit('choose_target', 'target', player=self.foe)
            self.emit('activate', 'equipment', value=1)
            self.emit('assert_stack', value=1)
            self.mana(self.foe)
            self.resolve()
            self.emit('turn', player=self.actor)
            self.stats('target', power_delta=3, toughness_delta=2)
        self.enter((lambda: self.choose_target('target')) if not empty else None)
        if not empty:
            # Phasing keeps the actual card in the same physical slot.
            self.prop('target', 'in_play', 0)
        if mode == 'attachments':
            self.prop('aura', 'in_play', 0)
            self.prop('equipment', 'in_play', 0)
        if mode == 'through_untap':
            self.next_own_main()
            self.prop('target', 'in_play', 0)
        self.receipts()
        self.cast_witness('remover', 'graveyard', target='source')
        self.move('source', 'graveyard')
        if not empty:
            self.stats('target', power_delta=3*int(mode == 'attachments'), toughness_delta=2*int(mode == 'attachments'))
            self.prop('target', 'tapped', 1)
        if mode == 'attachments':
            self.prop('aura', 'in_play', 1)
            self.prop('equipment', 'in_play', 1)

    def quarry(self):
        mode = self.mode
        number = {'top': 0, 'beneath_one': 1, 'short_library': 3, 'borrowed_owner': 1}[mode]
        owner = self.foe if mode == 'borrowed_owner' else self.actor if mode == 'top' else self.foe
        self.add('target', 'Hill Giant', owner, 'battlefield')
        self.add('enemy_plains', 'Plains', self.foe, 'battlefield')
        self.add('own_wrong_land', 'Island', self.actor, 'battlefield')
        for i in range(number):
            self.add(f'plains_{i}', 'Plains', self.actor, 'battlefield')
        if mode != 'short_library':
            self.add('next_card', 'Forest', owner, 'library')
        ref = self.borrow('target') if mode == 'borrowed_owner' else 'target'
        self.enter(lambda: self.choose_target(ref))
        self.stats('source')
        self.move('target', 'library', seat=owner)
        library = self.zones[owner]['library']
        assert library.pop() == 'target'
        library.insert(number, 'target')

    def quarantine(self):
        mode = self.mode
        amount = {'zero': 0, 'one': 1, 'two': 2, 'up_to_one': 2, 'opponent_only': 2}[mode]
        self.add('own_foil', 'Grizzly Bears', self.actor, 'battlefield')
        self.add('land_foil', 'Island', self.foe, 'battlefield')
        targets = []
        if mode != 'opponent_only':
            for ref, name in (('first', 'Hill Giant'), ('second', 'Darksteel Relic'), ('third', 'Sol Ring')):
                self.add(ref, name, self.foe, 'battlefield')
                targets.append(ref)
        selected = targets[:min(amount, 1) if mode == 'up_to_one' else amount]
        if amount:
            self.add('remover', 'Disenchant', self.actor, 'hand')
        payment = (amount*2, 0, 0, 0, 0, 2)
        self.begin_cast(payment=payment,
                        choices=lambda: self.emit('choose_x', player=self.actor, value=amount*2))
        for ref in selected:
            self.choose_target(ref)
        if mode == 'up_to_one':
            self.emit('choose_done_target', player=self.actor)
        self.resolve()
        self.move('source', 'battlefield')
        self.prop('source', 'in_play', 1)
        for ref in selected:
            self.move(ref, 'exile')
        self.receipts()
        if amount:
            self.cast_witness('remover', 'graveyard', target='source')
            self.move('source', 'graveyard')
            for ref in selected:
                self.move(ref, 'battlefield')

    def harbinger(self):
        mode = self.mode
        flash = mode in ('flash', 'flash_short')
        if flash:
            self.emit('turn', player=self.foe)
        self.add('target', 'Hill Giant', self.foe, 'battlefield', state=int(mode != 'untapped_only'))
        self.add('own_tapped', 'Scathe Zombies', self.actor, 'battlefield', state=1)
        self.add('untapped_foe', 'Grizzly Bears', self.foe, 'battlefield')
        if mode == 'flash_short':
            self.pools[self.actor] = (0, 0, 2, 0, 0, 0)
            self.mana(self.actor, self.pools[self.actor], write=True)
            self.emit('refresh')
            # The raw EVENT_CAN_CAST query omits payment. Attempt the actual cast.
            self.emit('cast', 'source', value=0)
            self.prop('source', 'in_hand', 1)
            return
        selected = mode not in ('decline', 'untapped_only')
        self.begin_cast(payment=(2*int(flash), 0, 2, 0, 0, 0))
        self.optional_etb()
        if mode == 'decline':
            self.emit('choose_optional_target', 'target', player=self.actor)
        elif selected:
            self.choose_target('target')
        self.resolve()
        self.move('source', 'battlefield')
        self.stats('source')
        if selected:
            self.move('target', 'hand')

    def petradon(self):
        mode = self.mode
        selected = ('first', 'second') if mode in ('two_lands', 'pump') else ()
        if mode != 'no_lands':
            self.add('first', 'Forest', self.actor, 'battlefield')
        if selected:
            self.add('second', 'Island', self.foe, 'battlefield')
            self.add('third', 'Plains', self.foe, 'battlefield')
        self.add('relic', 'Darksteel Relic', self.foe, 'battlefield')
        self.add('remover', 'Murder', self.actor, 'hand')
        self.begin_cast()
        for ref in selected:
            self.choose_target(ref)
        if mode == 'only_one_land':
            self.emit('choose_optional_target', 'first', player=self.actor)
        self.resolve(unused=int(mode == 'only_one_land'))
        self.move('source', 'battlefield')
        self.stats('source')
        for ref in selected:
            self.move(ref, 'exile')
        self.receipts()
        if mode == 'pump':
            self.mana(self.actor, (0, 0, 0, 0, 1, 0), write=True)
            self.emit('refresh')
            self.emit('activate', 'source', value=1)
            self.emit('assert_stack', value=1)
            self.mana(self.actor)
            self.resolve()
            self.stats('source', power_delta=1)
            self.emit('cleanup')
            self.resolve()
            self.stats('source')
            self.emit('turn', player=self.actor)
        self.cast_witness('remover', 'graveyard', target='source')
        self.move('source', 'graveyard')
        for ref in selected:
            self.move(ref, 'battlefield')

    def aven(self):
        mode = self.mode
        grow = mode in ('grow', 'flying')
        owner = self.foe
        target, ref = 'foe_target', 'foe_target'
        if mode == 'self_bounce':
            owner, target, ref = self.actor, 'source', 'source'
        elif not grow:
            self.add(target, 'Hill Giant', owner, 'battlefield')
            if mode == 'borrowed_owner':
                ref = self.borrow(target)
        if mode == 'flying':
            self.ground_witness()

        def choose():
            self.emit('choose_dialog', player=self.actor, value=int(not grow))
            if not grow:
                self.choose_target(ref)

        # The source can legally bounce itself before any in-play receipt.
        self.begin_cast()
        choose()
        self.resolve()
        self.move('source', 'battlefield')
        if grow:
            self.stats('source', power_delta=1, toughness_delta=1)
            self.prop('source', 'plus1_counters', 1)
        else:
            self.move(target, 'hand', seat=owner)
        if mode == 'flying':
            self.flight()
            self.life[self.foe] -= 3

    def fleets(self):
        mode = self.mode
        amount = {'two_islands': 2, 'no_islands': 0, 'borrowed_owner': 2, 'one_island': 1}[mode]
        for i in range(amount):
            self.add(f'island_{i}', 'Island', self.actor, 'battlefield')
        self.add('foe_island', 'Island', self.foe, 'battlefield')
        self.add('own_creature', 'Savannah Lions', self.actor, 'battlefield')
        self.add('low', 'Savannah Lions', self.foe, 'battlefield')
        self.add('match', 'Grizzly Bears', self.foe, 'battlefield')
        self.add('high', 'Hill Giant', self.foe, 'battlefield')
        if mode == 'borrowed_owner':
            self.add('owned', 'Goblin Piker', self.actor, 'battlefield')
            self.take_under_opponent_control('owned')
        self.enter()
        self.stats('source')
        if amount:
            self.move('low', 'hand')
        if amount >= 2:
            self.move('match', 'hand')
        if mode == 'borrowed_owner':
            self.move('owned', 'hand', seat=self.actor)
            self.move('control', 'graveyard')

    def follow_on(self):
        actions = {
            DANCE: self.dance, DAY: self.day, DRAKE: self.drake, HOARDING: self.hoarding,
            HYPNOX: self.hypnox, OUBLIETTE: self.oubliette, QUARRY: self.quarry,
            QUARANTINE: self.quarantine, HARBINGER: self.harbinger, PETRADON: self.petradon,
            AVEN: self.aven, FLEETS: self.fleets,
        }
        actions[self.cid]()


SEATED = {
    'assert_life', 'assert_mana', 'assert_matching_permanents', 'choose_dialog', 'choose_x', 'upkeep_step', 'assert_shuffle_count', 'bind_permanent', 'bind_permanent_copy',
    'choose_cancel_target', 'choose_done_target', 'choose_optional_target', 'choose_optional_zone', 'choose_player',
    'choose_target', 'choose_trigger', 'choose_zone', 'choose_zone_card', 'set_mana', 'turn', 'untap_step',
}
VALUES = {
    'activate', 'assert_card', 'assert_forced_target_choices', 'assert_forced_zone_choices', 'assert_life',
    'assert_mana', 'assert_offered_count', 'assert_offered_legal_card', 'assert_shuffle_count', 'assert_stack',
    'assert_unused_choices', 'assert_zone', 'assert_zone_card', 'assert_zone_multiplicity', 'attack', 'bind_permanent',
    'bind_permanent_copy', 'block', 'can_activate', 'can_cast', 'cast', 'cast_response', 'choose_optional_zone',
    'choose_player', 'choose_zone', 'choose_zone_card', 'choose_dialog', 'choose_x', 'assert_matching_permanents', 'observe_search_filters', 'present_ai_dialogs', 'set_mana',
}
BY_INDEX = {'assert_matching_permanents', 'assert_offered_legal_card', 'assert_zone_card', 'assert_zone_multiplicity',
            'bind_permanent', 'bind_permanent_copy'}
ZONE_OPS = {'assert_zone', 'assert_zone_card', 'assert_zone_multiplicity'}
GAPS = {
    GRASP: ['In a duel the other seat is the sole opponent.'],
    QUARANTINE: ['The existing counter observer does not expose isolation counters. Actual offers and capacity at X=0, 1 and 2 observe their effect.'],
    OUBLIETTE: ['Raw battlefield slots include phased cards. Actual in-play state, exact no-zone-movement receipts and attachment effects distinguish phasing.'],
}


def shaped_steps(rows):
    zones = tuple(ZONE)
    for op, ref, other, prop, player, value, zone, index, color in rows:
        step = dict(op=op)
        if ref:
            step['ref'] = ref
        if other:
            step['other'] = other
        if prop:
            step['property'] = prop
        if op in SEATED or op in ZONE_OPS:
            step['player'] = player
        if op in VALUES:
            step['value'] = value
        if op in ZONE_OPS:
            step['zone'] = zones[zone]
        if op in BY_INDEX:
            step['index'] = index
        if op in ('assert_mana', 'set_mana'):
            step['color'] = color
        yield step


def expand(name, csvid, declaration, shape, selected, rows, oracle):
    validate_witnesses(oracle, rows)
    if shape['csvid'] != csvid:
        raise ValueError('entry zone-movement source CSVID changed')
    names = {row['csvid']: name for name, row in rows.items() if row['kind'] in ('card', 'token')}
    zones = tuple(ZONE)
    cases = []
    for variant in selected:
        mode, seat = variant.rsplit('_', 1)
        scene = Scene(rows[name], int(seat), mode, rows, oracle)
        scene.play()
        setup = []
        for ref, player, zone, cid, state in scene.setup:
            entry = dict(ref=ref, player=player, zone=zones[zone], name=names[cid])
            if state:
                state_name = {1: 'tapped', 32: 'token'}.get(state)
                if not state_name:
                    raise ValueError('unsupported entry movement fixture state')
                entry['state'] = [state_name]
            setup.append(entry)
        cases.append(dict(name=f'{LABEL}_{csvid}_{variant}', card=name, fixture='empty_main', seed=SEED,
                          setup=setup, steps=list(shaped_steps(scene.steps)),
                          _recipe=dict(family=FAMILY, version=2, variant=variant, params=declaration['params'],
                                       oracle_text=declaration['oracle_text'], untested_components=GAPS.get(csvid, []))))
    return cases


