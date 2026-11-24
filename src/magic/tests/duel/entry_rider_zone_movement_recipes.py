"""Pinned graveyard entry riders, actual payment, and zone outcomes."""

from collections import Counter
from functools import lru_cache
import json
from pathlib import Path

from death_counter_trigger_recipes import exact
from defensive_creature_recipes import reviewed_payment
from recipes import cost_from_oracle

FAMILY = 'entry_rider_zone_movement'
PROFILE = Path(__file__).with_name('data') / f'{FAMILY}_wave8_profiles.json'
LABEL = 'EntryRiderZoneMovementRecipe'
SEED = 3308
ANIMATE, AURIOK, LUMINATE, WISPWEAVER = 3, 11563, 12996, 16580
MODES = {
    ANIMATE: ('own_grave', 'foreign_grave', 'both_choose_foreign', 'sacrifice_indestructible', 'lost_grave_target', 'no_creature'),
    AURIOK: ('return_attach', 'return_unattached', 'decline_return', 'enemy_only', 'shrouded'),
    LUMINATE: ('positive_power', 'zero_power', 'modified_power', 'decline', 'own_only', 'vigilance'),
    WISPWEAVER: ('returned_etb', 'borrowed_owner', 'untaps', 'decline', 'enemy_only', 'flying'),
}
ZONE = dict(hand=0, battlefield=1, library=2, graveyard=3, exile=4)
ZERO = (0,) * 6



ORIGINAL = frozenset(MODES)
SERENITY, DANCE, SERVITUDE, NECROMANCY, HAG, NUCKLAVEE = 12647, 1897, 3744, 2749, 9763, 9841
WARREN, GREENWARDEN, SKULLWINDER, DUKHARA, DWELLERS, BLOODWATER = 9306, 15337, 15542, 16354, 15605, 17092
HYBRID_COLORS = {HAG: ('B', 'G'), NUCKLAVEE: ('U', 'R')}
MODES.update({
    SERENITY: ('mixed', 'three_battlefield', 'limit_grave', 'decline', 'flying'),
    DANCE: ('upkeep_pay', 'upkeep_decline', 'upkeep_short', 'opponent_owner'),
    SERVITUDE: ('dies', 'enchantment_leaves', 'opponent_only'),
    NECROMANCY: ('ordinary_persists', 'flash_cleanup', 'source_removed', 'foreign_grave'),
    HAG: ('highest', 'tie', 'opponent_only', 'ignore_shroud'),
    NUCKLAVEE: ('both', 'blue_only', 'red_only', 'decline', 'wrong_type'),
    WARREN: ('goblin', 'nongoblin', 'opponent_only'),
    GREENWARDEN: ('entry_and_death', 'death_declined', 'entry_declined', 'opponent_only'),
    SKULLWINDER: ('both_graves', 'own_only', 'opponent_only', 'deathtouch'),
    DUKHARA: ('artifact', 'creature', 'decline', 'wrong_grave'),
    DWELLERS: ('free_three', 'free_targeted', 'decline', 'above_limit', 'menace'),
    BLOODWATER: ('instant', 'sorcery', 'decline', 'wrong_type', 'prowess_flying'),
})


def source_cost(record, csvid, mode=''):
    if csvid in HYBRID_COLORS:
        pair = HYBRID_COLORS[csvid]
        symbols = [pair[1]] * 2 if mode in ('tie', 'red_only') else [pair[0]] * 2
        mana, _ = reviewed_payment(record, dict(payment_colors=symbols))
        return mana
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
        slots = self.zones[new_seat][zone]
        slots.append(ref)
        self.cards[ref] = name, new_seat, zone

    def prop(self, ref, key, value):
        self.emit('assert_card', ref, prop=key, value=value)

    def mana(self, player, amounts=ZERO, *, write=False):
        for color, amount in enumerate(amounts):
            self.emit('set_mana' if write else 'assert_mana', player=player, value=amount, color=color)

    def choose_target(self, ref):
        self.emit('choose_target', ref, player=self.actor)

    def begin_cast(self, ref='source', *, target=None, choices=None):
        name, player, zone = self.cards[ref]
        assert zone == 'hand' and player == self.actor
        self.mana(player, source_cost(self.oracle[name], self.cid, self.mode) if ref == 'source' else cost_from_oracle(self.oracle[name]), write=True)
        self.emit('refresh')
        if target:
            self.choose_target(target)
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

    def combat(self, illegal_blocker, legal_blocker=None):
        self.next_own_main()
        self.prop('source', 'sick', 0)
        self.emit('begin_attackers')
        self.emit('attack', 'source', value=1)
        self.prop('source', 'attacking', 1)
        self.prop('source', 'tapped', int(self.cid != LUMINATE))
        self.emit('finish_attackers')
        self.emit('begin_blockers')
        if illegal_blocker:
            self.emit('block', illegal_blocker, other='source', value=0)
        if legal_blocker:
            self.emit('block', legal_blocker, other='source', value=1)
        self.emit('advance_combat')
        self.prop('source', 'blocked', int(bool(legal_blocker)))
        self.emit('combat_damage')
        self.resolve()
        self.emit('end_combat')

    def receipts(self):
        self.emit('assert_stack', value=0)
        for p, zones in self.zones.items():
            self.mana(p, self.pools[p])
            self.emit('assert_life', player=p, value=self.life[p])
            self.emit('assert_shuffle_count', player=p, value=0)
            for zone, slots in zones.items():
                names = [self.cards[ref][0] for ref in slots]
                counts = Counter(names)
                self.emit('assert_zone', player=p, zone=zone, value=len(names))
                for name in sorted({name for name, _, _ in self.cards.values()}):
                    self.emit('assert_zone_multiplicity', player=p, zone=zone, index=self.rows[name]['csvid'],
                              value=counts[name])
                if zone == 'library':
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

    def enter(self, choice=None):
        self.begin_cast()
        if choice:
            choice()
        self.resolve()
        self.move('source', 'battlefield')
        self.prop('source', 'in_play', 1)

    def optional_etb(self):
        if self.actor == 0:
            self.emit('choose_trigger', 'source', player=0)

    def animate(self):
        mode = self.mode
        self.add('own_grave_spell', 'Giant Growth', self.actor, 'graveyard')
        self.add('foe_grave_land', 'Island', self.foe, 'graveyard')
        if mode == 'no_creature':
            self.pools[self.actor] = cost_from_oracle(self.oracle[self.name])
            self.mana(self.actor, self.pools[self.actor], write=True)
            self.emit('refresh')
            self.emit('can_cast', 'source', value=0)
            self.emit('cast', 'source', value=0)
            self.emit('assert_unused_choices', value=0)
            return
        owner = self.foe if mode in ('foreign_grave', 'both_choose_foreign') else self.actor
        target_name = 'Darksteel Sentinel' if mode == 'sacrifice_indestructible' else 'Hill Giant'
        self.add('target', target_name, owner, 'graveyard')
        self.add('bystander', 'Grizzly Bears', self.actor, 'battlefield')
        if mode == 'both_choose_foreign':
            self.add('other_creature', 'Walking Corpse', self.actor, 'graveyard')
        lost = mode == 'lost_grave_target'
        if lost:
            self.add('response', 'Cremate', self.foe, 'hand')
        else:
            self.add('remove_aura', 'Disenchant', self.actor, 'hand')

        def grave_choice():
            if mode == 'both_choose_foreign':
                self.emit('choose_player', player=self.actor, value=owner)
            self.emit('observe_search_filters', value=1)
            self.pick_zone('target')

        self.begin_cast(choices=grave_choice)
        self.emit('assert_offered_legal_card', index=self.id('target'), value=1)
        foil = 'own_grave_spell' if owner == self.actor else 'foe_grave_land'
        self.emit('assert_offered_legal_card', index=self.id(foil), value=0)
        self.emit('observe_search_filters', value=0)
        if lost:
            self.emit('announce_spell', 'source')
            self.mana(self.foe, cost_from_oracle(self.oracle['Cremate']), write=True)
            self.emit('refresh')
            self.emit('choose_player', player=self.foe, value=owner)
            self.pick_zone('target', chooser=self.foe)
            self.emit('cast_response', 'response', value=1)
            self.emit('assert_unused_choices', value=0)
            self.emit('assert_stack', value=2)
            self.mana(self.foe)
            self.emit('resolve_response')
            self.emit('assert_stack', value=0)
            self.emit('assert_unused_choices', value=0)
            self.emit('refresh')
            self.move('target', 'exile')
            self.move('foe_top', 'hand')
            self.move('response', 'graveyard')
            self.move('source', 'graveyard')
            return
        self.resolve()
        self.move('source', 'battlefield')
        self.move('target', 'battlefield', seat=self.actor)
        self.prop('source', 'in_play', 1)
        self.bind('target', self.actor)
        self.stats('target', power_delta=-1)
        self.stats('bystander')
        self.receipts()
        self.cast_witness('remove_aura', 'graveyard', target='source')
        self.move('source', 'graveyard')
        self.move('target', 'graveyard', seat=owner)
        self.stats('bystander')

    def auriok(self):
        mode = self.mode
        eligible = mode != 'enemy_only'
        self.add('equipment', 'Bonesplitter', self.actor if eligible else self.foe, 'graveyard')
        self.add('non_equipment', 'Darksteel Relic', self.actor, 'graveyard')
        self.add('foe_spell', 'Lightning Bolt', self.foe, 'graveyard')
        if mode == 'shrouded':
            self.add('shroud', 'Silent Gravestone', self.foe, 'battlefield')
        offered = eligible and mode != 'shrouded'
        returned = mode in ('return_attach', 'return_unattached')

        def choose():
            self.optional_etb()
            if offered:
                self.emit('observe_search_filters', value=1)
                self.pick_zone('equipment', optional=not returned)
                if returned:
                    self.emit('choose_dialog', player=self.actor, value=int(mode == 'return_unattached'))

        self.enter(choose)
        if offered:
            self.emit('assert_offered_legal_card', index=self.id('equipment'), value=1)
            self.emit('assert_offered_legal_card', index=self.id('non_equipment'), value=0)
            self.emit('assert_forced_zone_choices', value=0)
            self.emit('observe_search_filters', value=0)
        if returned:
            self.move('equipment', 'battlefield')
            self.bind('equipment', self.actor)
        self.stats('source', power_delta=2 if mode == 'return_attach' else 0)

    def luminate(self):
        mode = self.mode
        self.add('own_foil', 'Rumbling Baloth', self.actor, 'battlefield')
        if mode != 'own_only':
            self.add('target', 'Ornithopter' if mode == 'zero_power' else 'Hill Giant', self.foe, 'battlefield')
            self.add('foe_foil', 'Grizzly Bears', self.foe, 'battlefield')
        if mode == 'modified_power':
            self.add('pump', 'Giant Growth', self.actor, 'hand')
            self.cast_witness('pump', 'graveyard', target='target')
            self.stats('target', power_delta=3, toughness_delta=3)
        selected = mode not in ('own_only', 'decline')
        if selected:
            self.enter(lambda: self.choose_target('target'))
            p = int(self.oracle[self.cards['target'][0]]['power'])
            self.life[self.foe] += p + (3 if mode == 'modified_power' else 0)
            self.move('target', 'exile')
        elif mode == 'decline':
            self.enter(lambda: self.emit('choose_optional_target', 'target', player=self.actor))
            self.emit('assert_forced_target_choices', value=0)
        else:
            self.enter()
        self.stats('source')
        self.receipts()
        if mode == 'vigilance':
            self.combat(None)
            self.life[self.foe] -= 4

    def wispweaver(self):
        mode = self.mode
        self.add('enemy_foil', 'Grizzly Bears', self.foe, 'battlefield')
        self.add('own_noncreature', 'Darksteel Relic', self.actor, 'battlefield')
        owner = self.foe if mode == 'borrowed_owner' else self.actor
        ref = 'target'
        if mode == 'returned_etb':
            self.add('target', 'Cathedral Sanctifier', owner, 'hand')
            self.cast_witness('target', 'battlefield')
            self.life[owner] += 3
            self.receipts()
        elif mode != 'enemy_only':
            self.add('target', 'Hill Giant', owner, 'battlefield', state=int(mode in ('untaps', 'decline')))
            if mode == 'borrowed_owner':
                ref = self.borrow('target')
        if mode == 'flying':
            self.add('reach', 'Giant Spider', self.foe, 'battlefield')

        def choose():
            self.optional_etb()
            if mode == 'decline':
                self.emit('choose_optional_target', ref, player=self.actor)
            elif mode != 'enemy_only':
                self.choose_target(ref)

        self.enter(choose)
        self.stats('source')
        if mode == 'decline':
            self.emit('assert_forced_target_choices', value=0)
            self.prop('target', 'tapped', 1)
        elif mode != 'enemy_only':
            self.move('target', 'battlefield', seat=owner)
            self.bind('target', owner)
            self.stats('target')
            self.prop('target', 'tapped', 0)
            if mode == 'returned_etb':
                self.life[owner] += 3
        if mode == 'flying':
            self.combat('enemy_foil', 'reach')
            self.move('reach', 'graveyard')
            self.stats('source', damage=2)

    def play(self):
        self.emit('turn', player=self.actor)
        self.emit('present_ai_dialogs', value=1)
        for seat in (0, 1):
            self.mana(seat, write=True)
        cases = {ANIMATE: self.animate, AURIOK: self.auriok,
                 LUMINATE: self.luminate, WISPWEAVER: self.wispweaver}
        if self.cid in cases:
            cases[self.cid]()
        else:
            self.follow_on()
        self.receipts()
        self.emit('present_ai_dialogs', value=0)


    def upkeep(self, player):
        self.emit('upkeep_step', player=player)
        self.emit('assert_stack', value=0)
        self.emit('assert_unused_choices', value=0)
        self.emit('refresh')

    def remove_paid(self, ref, target):
        self.cast_witness(ref, 'graveyard', target=target)

    def closed_grave_aura(self, *, necromancy):
        mode = self.mode
        owner = self.foe if mode in ('foreign_grave', 'opponent_owner') else self.actor
        self.add('target', 'Hill Giant', owner, 'graveyard')
        self.add('foe_foil', 'Island', 1-owner, 'graveyard')
        paid_upkeep = mode == 'upkeep_pay'
        short = mode == 'upkeep_short'
        affordable = not (necromancy or short)
        if affordable:
            for ref in ('swamp_a', 'swamp_b'):
                self.add(ref, 'Swamp', self.actor, 'battlefield')
        leaves_on_cleanup = mode == 'flash_cleanup'
        use_remover = not leaves_on_cleanup and mode != 'ordinary_persists'
        if use_remover:
            self.add('remover', 'Disenchant', self.actor, 'hand')
        if leaves_on_cleanup:
            self.emit('turn', player=self.foe)
        self.begin_cast(choices=lambda: self.pick_zone('target'))
        self.resolve()
        self.move('source', 'battlefield')
        self.move('target', 'battlefield', seat=self.actor)
        self.prop('source', 'in_play', 1)
        self.bind('target', self.actor)
        bonus = 0 if necromancy else 1
        self.stats('target', power_delta=bonus, toughness_delta=bonus)
        self.prop('target', 'tapped', bonus)
        self.receipts()
        if necromancy:
            if mode in ('flash_cleanup', 'ordinary_persists'):
                self.emit('cleanup')
                self.resolve()
                if leaves_on_cleanup:
                    self.move('source', 'graveyard')
                    self.move('target', 'graveyard', seat=owner)
            else:
                self.remove_paid('remover', 'source')
                self.move('source', 'graveyard')
                self.move('target', 'graveyard', seat=owner)
            return
        self.emit('advance_turn')
        self.emit('untap_step', player=self.foe)
        self.upkeep(self.foe)
        self.prop('target', 'tapped', 1)
        self.emit('advance_turn')
        self.emit('untap_step', player=self.actor)
        self.prop('target', 'tapped', 1)
        if affordable:
            self.emit('choose_dialog', player=self.actor, value=int(not paid_upkeep))
            if paid_upkeep and self.actor == 0:
                for ref in ('swamp_a', 'swamp_b'):
                    self.emit('choose_mana_source', ref)
        self.upkeep(self.actor)
        self.prop('target', 'tapped', int(not paid_upkeep))
        if affordable:
            for ref in ('swamp_a', 'swamp_b'):
                self.prop(ref, 'tapped', int(paid_upkeep))
        self.receipts()
        self.emit('turn', player=self.actor)
        self.remove_paid('remover', 'source')
        self.move('source', 'graveyard')
        self.move('target', 'graveyard', seat=owner)

    def serenity(self):
        mode = self.mode
        battlefield, graves = [], []
        names = ('Hill Giant', 'Walking Corpse', 'Grizzly Bears', 'Goblin Piker')
        if mode in ('three_battlefield', 'mixed', 'decline'):
            for i in range(3 if mode == 'three_battlefield' else 1):
                ref = f'permanent_{i}'
                self.add(ref, names[i], (self.foe, self.actor, self.foe)[i], 'battlefield')
                battlefield.append(ref)
        if mode == 'mixed':
            for i, seat in enumerate((self.actor, self.foe)):
                ref = f'grave_{i}'
                self.add(ref, names[1+i], seat, 'graveyard')
                graves.append(ref)
        if mode == 'limit_grave':
            for i, name in enumerate(names):
                ref = f'grave_{i}'
                self.add(ref, name, self.actor, 'graveyard')
                graves.append(ref)
        if mode == 'flying':
            self.add('ground', 'Grizzly Bears', self.foe, 'battlefield')
            self.add('reach', 'Giant Spider', self.foe, 'battlefield')
        returns = mode in ('three_battlefield', 'mixed', 'limit_grave')
        if returns:
            self.add('remover', 'Murder', self.actor, 'hand')
        self.begin_cast()
        if mode in ('decline', 'flying'):
            self.emit('choose_cancel_target', player=self.actor)
        for ref in battlefield if returns else ():
            self.choose_target(ref)
        for ref in graves[:3]:
            self.pick_zone(ref)
        if mode == 'limit_grave':
            self.pick_zone(graves[3], optional=True)
        self.resolve(unused=int(mode == 'limit_grave'))
        self.move('source', 'battlefield')
        self.stats('source')
        if returns:
            targets = battlefield + graves[:3]
            assert len(targets) == 3
            for ref in targets:
                self.move(ref, 'exile')
            self.receipts()
            self.remove_paid('remover', 'source')
            self.move('source', 'graveyard')
            for ref in targets:
                self.move(ref, 'hand')
        if mode == 'flying':
            self.combat('ground', 'reach')
            self.move('reach', 'graveyard')
            self.stats('source', damage=2)

    def servitude(self):
        mode = self.mode
        eligible = mode != 'opponent_only'
        owner = self.actor if eligible else self.foe
        self.add('target', 'Hill Giant', owner, 'graveyard')
        self.add('grave_foil', 'Forest', self.actor, 'graveyard')
        if eligible:
            self.add('remover', 'Murder' if mode == 'dies' else 'Disenchant', self.actor, 'hand')
        self.enter((lambda: self.pick_zone('target')) if eligible else None)
        if eligible:
            self.move('target', 'battlefield')
            self.bind('target', self.actor)
            self.stats('target')
            self.receipts()
            self.remove_paid('remover', 'target' if mode == 'dies' else 'source')
            self.move('source', 'hand' if mode == 'dies' else 'graveyard')
            self.move('target', 'exile')

    def hag(self):
        mode = self.mode
        available = mode != 'opponent_only'
        if available:
            self.add('largest', 'Rumbling Baloth', self.actor, 'graveyard')
            self.add('smaller', 'Giant Spider', self.actor, 'graveyard')
        if mode == 'tie':
            self.add('tie', 'Spined Wurm', self.actor, 'graveyard')
            self.add('max_other', 'Shivan Dragon', self.actor, 'graveyard')
        if mode == 'ignore_shroud':
            self.add('shroud', 'Silent Gravestone', self.foe, 'battlefield')
        self.add('opponent_large', 'Force of Nature', self.foe, 'graveyard')
        # The tied case uses two actual 5-power creature names, with distinct
        # lower-power and foreign cards retained by ordered zone receipts.
        chosen = 'tie' if mode == 'tie' else 'largest'
        self.begin_cast()
        if mode == 'tie':
            self.pick_zone(chosen)
        if available:
            self.emit('choose_dialog', player=self.actor, value=0)
        self.resolve()
        self.move('source', 'battlefield')
        self.stats('source')
        if available:
            self.move(chosen, 'hand')

    def nucklavee(self):
        mode = self.mode
        valid = mode != 'wrong_type'
        self.add('not_blue_instant', 'Divination', self.actor, 'graveyard')
        self.add('not_red_sorcery', 'Lightning Bolt', self.actor, 'graveyard')
        self.add('opponent_blue', 'Unsummon', self.foe, 'graveyard')
        if valid:
            self.add('blue', 'Unsummon', self.actor, 'graveyard')
            self.add('red', 'Stone Rain', self.actor, 'graveyard')
        chosen = {'both': ('blue', 'red'), 'blue_only': ('blue',), 'red_only': ('red',),
                  'decline': (), 'wrong_type': ()}[mode]

        def choices():
            if valid:
                for ref in ('blue', 'red'):
                    self.pick_zone(ref, optional=ref not in chosen)

        self.enter(choices)
        self.stats('source')
        for ref in chosen:
            self.move(ref, 'hand')
        self.emit('assert_forced_zone_choices', value=0)

    def warren(self):
        mode = self.mode
        recover = mode != 'opponent_only'
        goblin = mode == 'goblin'
        self.add('card', 'Goblin Piker' if goblin else 'Hill Giant',
                 self.actor if recover else self.foe, 'graveyard')
        self.add('foil', 'Forest', self.actor, 'graveyard')
        self.enter((lambda: self.pick_zone('card')) if recover else None)
        self.stats('source')
        if recover:
            self.move('card', 'hand')
        self.emit('begin_attackers')
        self.emit('attack', 'source', value=int(goblin))
        self.prop('source', 'attacking', int(goblin))
        self.emit('finish_attackers')
        self.emit('begin_blockers')
        self.emit('advance_combat')
        self.emit('combat_damage')
        self.resolve()
        self.emit('end_combat')
        self.life[self.foe] -= 3 * int(goblin)
        self.emit('cleanup')
        self.resolve()
        self.stats('source')

    def greenwarden(self):
        mode = self.mode
        recovers = mode in ('entry_and_death', 'death_declined')
        dying = mode in ('entry_and_death', 'death_declined')
        if mode != 'opponent_only':
            self.add('entry_card', 'Forest', self.actor, 'graveyard')
        self.add('opponent_card', 'Plains', self.foe, 'graveyard')
        if dying:
            self.add('death_card', 'Lightning Bolt', self.actor, 'graveyard')
            self.add('remover', 'Murder', self.actor, 'hand')

        def choose():
            self.optional_etb()
            if mode != 'opponent_only':
                self.pick_zone('entry_card', optional=not recovers)

        self.enter(choose)
        self.stats('source')
        if recovers:
            self.move('entry_card', 'hand')
        if not dying:
            return
        self.receipts()
        self.begin_cast('remover', target='source')
        accept = mode == 'entry_and_death'
        # The automatic seat accepts the optional self-exile in this fixture.
        # Its decline probe addresses the ensuing required return selector.
        decline_return = not accept and self.actor == 1
        if self.actor == 0 and accept:
            self.emit('choose_trigger', 'source', player=0)
        if accept or decline_return:
            self.pick_zone('death_card', optional=decline_return)
        self.resolve()
        if decline_return:
            self.emit('assert_forced_zone_choices', value=1)
        self.move('remover', 'graveyard')
        paid = accept or decline_return
        self.move('source', 'exile' if paid else 'graveyard')
        if paid:
            self.move('death_card', 'hand')

    def skullwinder(self):
        mode = self.mode
        own = mode in ('both_graves', 'own_only')
        foe = mode in ('both_graves', 'opponent_only')
        if own:
            self.add('own_card', 'Lightning Bolt', self.actor, 'graveyard')
        if foe:
            self.add('foe_card', 'Hill Giant', self.foe, 'graveyard')
            self.add('foe_foil', 'Island', self.foe, 'graveyard')
        if mode == 'deathtouch':
            self.add('giant', 'Hill Giant', self.foe, 'battlefield')

        def choose():
            if own:
                self.pick_zone('own_card')
                if foe:
                    self.pick_zone('foe_card', chooser=self.foe)

        self.enter(choose)
        self.stats('source')
        if own:
            self.move('own_card', 'hand')
            if foe:
                self.move('foe_card', 'hand')
        if mode == 'deathtouch':
            self.combat(None, 'giant')
            self.move('source', 'graveyard')
            self.move('giant', 'graveyard')

    def to_library(self):
        mode = self.mode
        bloodwater = self.cid == BLOODWATER
        allowed = mode not in ('wrong_grave', 'wrong_type', 'prowess_flying')
        choose = allowed and mode != 'decline'
        target = ('Divination' if mode == 'sorcery' else 'Unsummon') if bloodwater else (
            'Hill Giant' if mode == 'creature' else 'Darksteel Relic')
        if allowed:
            self.add('target', target, self.actor, 'graveyard')
        self.add('own_land', 'Forest', self.actor, 'graveyard')
        self.add('foe_target', target, self.foe, 'graveyard')
        if mode == 'prowess_flying':
            self.add('ground', 'Grizzly Bears', self.foe, 'battlefield')
            self.add('boost', 'Giant Growth', self.actor, 'hand')
        self.enter((lambda: self.pick_zone('target', optional=not choose)) if allowed else None)
        self.stats('source')
        if choose:
            self.move('target', 'library')
            assert self.zones[self.actor]['library'].pop() == 'target'
            self.zones[self.actor]['library'].insert(0, 'target')
        if mode == 'prowess_flying':
            self.cast_witness('boost', 'graveyard', target='ground')
            self.stats('source', power_delta=1, toughness_delta=1)
            self.emit('cleanup')
            self.resolve()
            self.stats('source')
            self.combat('ground')
            self.life[self.foe] -= 2

    def dwellers(self):
        mode = self.mode
        has_spell = mode in ('free_three', 'free_targeted', 'decline')
        accepted = mode in ('free_three', 'free_targeted')
        if has_spell:
            self.add('target', 'Divination' if mode == 'free_three' else 'Lightning Bolt', self.actor, 'graveyard')
        if mode == 'free_three':
            self.add('next_draw', 'Island', self.actor, 'library')
        self.add('too_large', 'Concentrate', self.actor, 'graveyard')
        self.add('foreign_spell', 'Lightning Bolt', self.foe, 'graveyard')
        if mode == 'menace':
            self.add('sole_blocker', 'Hill Giant', self.foe, 'battlefield')

        def choose():
            self.optional_etb()
            if has_spell:
                self.pick_zone('target', optional=not accepted)
                if mode == 'free_targeted':
                    self.emit('choose_player', player=self.actor, value=self.foe)

        self.enter(choose)
        self.stats('source')
        if accepted:
            self.move('target', 'exile')
            if mode == 'free_three':
                for ref in ('own_top', 'next_draw'):
                    self.move(ref, 'hand')
            else:
                self.life[self.foe] -= 3
        if mode == 'menace':
            self.next_own_main()
            self.emit('begin_attackers')
            self.emit('attack', 'source', value=1)
            self.emit('finish_attackers')
            self.emit('begin_blockers')
            self.emit('block', 'sole_blocker', other='source', value=0)
            self.emit('advance_combat')
            self.prop('source', 'blocked', 0)
            self.emit('combat_damage')
            self.resolve()
            self.emit('end_combat')
            self.life[self.foe] -= 4

    def follow_on(self):
        actions = {
            SERENITY: self.serenity, DANCE: lambda: self.closed_grave_aura(necromancy=False),
            NECROMANCY: lambda: self.closed_grave_aura(necromancy=True), SERVITUDE: self.servitude,
            HAG: self.hag, NUCKLAVEE: self.nucklavee, WARREN: self.warren, GREENWARDEN: self.greenwarden,
            SKULLWINDER: self.skullwinder, DUKHARA: self.to_library, DWELLERS: self.dwellers, BLOODWATER: self.to_library,
        }
        actions[self.cid]()


SEATED = {
    'assert_life', 'assert_mana', 'choose_dialog', 'upkeep_step', 'assert_shuffle_count', 'bind_permanent', 'bind_permanent_copy',
    'choose_cancel_target', 'choose_optional_target', 'choose_optional_zone', 'choose_player',
    'choose_target', 'choose_trigger', 'choose_zone', 'choose_zone_card', 'set_mana', 'turn', 'untap_step',
}
VALUES = {
    'activate', 'assert_card', 'assert_forced_target_choices', 'assert_forced_zone_choices', 'assert_life',
    'assert_mana', 'assert_offered_count', 'assert_offered_legal_card', 'assert_shuffle_count', 'assert_stack',
    'assert_unused_choices', 'assert_zone', 'assert_zone_card', 'assert_zone_multiplicity', 'attack', 'bind_permanent',
    'bind_permanent_copy', 'block', 'can_activate', 'can_cast', 'cast', 'cast_response', 'choose_optional_zone',
    'choose_player', 'choose_zone', 'choose_zone_card', 'choose_dialog', 'observe_search_filters', 'present_ai_dialogs', 'set_mana',
}
BY_INDEX = {'assert_offered_legal_card', 'assert_zone_card', 'assert_zone_multiplicity',
            'bind_permanent', 'bind_permanent_copy'}
ZONE_OPS = {'assert_zone', 'assert_zone_card', 'assert_zone_multiplicity'}
GAPS = {
    SERENITY: ['The legacy ETB resolves target selection synchronously; the independent proof covers battlefield and graveyards, up to three and owner hand return.'],
    NECROMANCY: ['Flash is tested during the opponent’s main phase, followed by that turn’s actual cleanup.'],
    LUMINATE: ['Each opponent is represented by the other seat in the duel.'],
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
                if state != 1:
                    raise ValueError('unsupported entry movement fixture state')
                entry['state'] = ['tapped']
            setup.append(entry)
        cases.append(dict(name=f'{LABEL}_{csvid}_{variant}', card=name, fixture='empty_main', seed=SEED,
                          setup=setup, steps=list(shaped_steps(scene.steps)),
                          _recipe=dict(family=FAMILY, version=2, variant=variant, params=declaration['params'],
                                       oracle_text=declaration['oracle_text'], untested_components=GAPS.get(csvid, []))))
    return cases

