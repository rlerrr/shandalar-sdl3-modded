"""Pinned entry exile, linked return, immediate blink and targeting restrictions."""

from collections import Counter
from functools import lru_cache
import json
from pathlib import Path

from death_counter_trigger_recipes import exact
from recipes import cost_from_oracle

FAMILY = 'entry_follow_on_zone_movement'
PROFILE = Path(__file__).with_name('data') / f'{FAMILY}_wave8_profiles.json'
LABEL = 'EntryFollowOnZoneMovementRecipe'
SEED = 3212
JOURNEY, HUNTER, WISP, RIFT = 10574, 11948, 9788, 9003
MODES = {
    JOURNEY: ('return', 'own_target', 'borrowed', 'empty', 'countered', 'returned_etb'),
    HUNTER: ('return', 'own_target', 'borrowed', 'decline', 'only_self', 'bounce_source'),
    WISP: ('land', 'borrowed', 'returned_etb', 'only_self', 'source_leaves', 'flying'),
    RIFT: ('own_only', 'foe_only', 'choose_own', 'choose_foe', 'none', 'hidden_only', 'hidden_plus_face_up'),
}
ZONE = dict(hand=0, battlefield=1, library=2, graveyard=3, exile=4)
ZERO = (0,) * 6
RED = (0, 0, 0, 0, 1, 0)



ORIGINAL = frozenset(MODES)
BUTCHER, WARDER, STAG, MAGGOT, SCULLER, FIEND = 5805, 11472, 11263, 13853, 10088, 5837
SPHERE, BINDING, FREEBOOTER, BAFFLING, DEPUTY, GUARDIAN = 12704, 17441, 17451, 18163, 19600, 16700
MODES.update({
    BUTCHER: ('return', 'own_target', 'borrowed', 'only_self'),
    WARDER: ('artifact', 'enchantment', 'decline', 'empty'),
    STAG: ('land', 'borrowed', 'only_self', 'source_leaves', 'vigilance'),
    MAGGOT: ('creature', 'spell', 'land_only', 'empty'),
    SCULLER: ('creature', 'spell', 'land_only', 'empty'),
    FIEND: ('creature', 'spell', 'land_only', 'empty'),
    SPHERE: ('same_name', 'decline', 'land_only', 'excluded_name'),
    BINDING: ('cast_lock', 'no_enemy_nonland', 'own_named_spell'),
    FREEBOOTER: ('spell', 'creature_only', 'empty', 'flight'),
    BAFFLING: ('boundary', 'above', 'own_only', 'token_trample'),
    DEPUTY: ('same_name', 'distinct', 'empty_opponent'),
    GUARDIAN: ('entry_again', 'land', 'borrowed', 'decline', 'only_self'),
})


@lru_cache(maxsize=1)
def pins():
    return json.loads(PROFILE.read_bytes())


def classify(record, csvid):
    pin = pins()['sources'].get(record.get('name'))
    if (csvid not in MODES or not pin or pin['registry']['csvid'] != csvid
            or not exact(record, pin['oracle']) or record.get('layout') != 'normal'):
        raise ValueError('complete pinned entry zone-movement rules required')
    return dict(csvid=csvid, cost=cost_from_oracle(record))


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
        if mode == 'countered':
            self.add('counter', 'Counterspell', self.foe, 'hand')

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

    def begin_cast(self, ref='source', *, target=None):
        name, player, zone = self.cards[ref]
        assert zone == 'hand'
        self.mana(player, cost_from_oracle(self.oracle[name]), write=True)
        self.emit('refresh')
        if target:
            self.emit('choose_target', target, player=player)
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

    def stats(self, ref, *, damage=None):
        name, _, zone = self.cards[ref]
        assert zone == 'battlefield'
        self.prop(ref, 'in_play', 1)
        self.prop(ref, 'csvid', self.rows[name]['csvid'])
        for key in ('power', 'toughness'):
            self.prop(ref, key, int(self.oracle[name][key]))
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
        self.emit('finish_attackers')
        self.emit('begin_blockers')
        self.emit('block', illegal_blocker, other='source', value=0)
        if legal_blocker:
            self.emit('block', legal_blocker, other='source', value=1)
        self.emit('advance_combat')
        self.prop('source', 'blocked', int(bool(legal_blocker)))
        self.emit('combat_damage')
        self.resolve()
        self.emit('end_combat')

    def counter_entry(self):
        self.begin_cast()
        self.emit('announce_spell', 'source')
        self.mana(self.foe, cost_from_oracle(self.oracle['Counterspell']), write=True)
        self.emit('refresh')
        self.emit('cast_response', 'counter', value=1)
        self.emit('assert_stack', value=2)
        self.mana(self.foe)
        self.emit('resolve_response')
        self.emit('assert_unused_choices', value=0)
        self.emit('assert_stack', value=0)
        self.emit('refresh')
        self.move('source', 'graveyard')
        self.move('counter', 'graveyard')

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

    def target_receipts(self, ref, seat):
        # Retain the verified slot-zero path on singleton battlefields.
        index = -1 if len(self.zones[seat]['battlefield']) > 1 else 0
        self.emit('bind_permanent', ref, player=seat, value=self.id(ref), index=index)
        self.prop(ref, 'in_play', 1)
        self.prop(ref, 'csvid', self.id(ref))
        self.prop(ref, 'tapped', 0)
        if 'Creature' in self.oracle[self.cards[ref][0]]['type_line']:
            self.stats(ref)

    def start_link(self, target=None, *, decline=False):
        self.begin_cast()
        if self.cid in (HUNTER, GUARDIAN) and self.actor == 0:
            self.emit('choose_trigger', 'source', player=0)
        if decline:
            self.emit('choose_cancel_target', player=self.actor)
        elif target:
            self.choose_target(target)
        self.resolve()
        self.move('source', 'battlefield')
        if 'Creature' in self.oracle[self.name]['type_line']:
            self.stats('source')
        else:
            self.prop('source', 'in_play', 1)

    def prepare_target(self, name, owner):
        if self.mode == 'returned_etb':
            self.add('target', name, owner, 'hand')
            if self.cid == JOURNEY:
                self.add('remover', 'Disenchant', self.actor, 'hand')
            self.begin_cast('target')
            self.resolve()
            self.move('target', 'battlefield')
            self.life[owner] += 3
            self.receipts()
        else:
            self.add('target', name, owner, 'battlefield')

    def remove_source(self, remover, destination):
        self.begin_cast(remover, target='source')
        self.resolve()
        self.move(remover, 'graveyard')
        self.move('source', destination)

    def linked(self):
        mode = self.mode
        self.add('noncreature_foil', 'Darksteel Relic', self.foe, 'battlefield')
        if mode in ('empty', 'only_self'):
            self.start_link()
            return
        owner = self.actor if mode in ('own_target', 'returned_etb') else self.foe
        self.prepare_target('Cathedral Sanctifier' if mode == 'returned_etb' else 'Hill Giant', owner)
        if mode == 'countered':
            self.counter_entry()
            return
        ref = self.borrow('target') if mode == 'borrowed' else 'target'
        if mode == 'decline':
            self.start_link(decline=True)
            self.emit('assert_forced_target_choices', value=0)
            return
        disposition = 'hand' if mode == 'bounce_source' else 'graveyard'
        name = 'Boomerang' if disposition == 'hand' else 'Disenchant' if self.cid == JOURNEY else 'Murder'
        if mode != 'returned_etb':
            self.add('remover', name, self.actor, 'hand')
        self.start_link(ref)
        self.move('target', 'exile', seat=owner)
        self.receipts()
        self.remove_source('remover', disposition)
        self.move('target', 'battlefield')
        if mode == 'returned_etb':
            self.life[owner] += 3
        self.target_receipts('target', owner)

    def wisp(self):
        mode = self.mode
        if mode == 'only_self':
            self.start_link()
            return
        owner = self.actor if mode == 'returned_etb' else self.foe
        target = {'land': 'Island', 'borrowed': 'Hill Giant', 'returned_etb': 'Cathedral Sanctifier',
                  'source_leaves': 'Darksteel Relic', 'flying': 'Swamp', 'vigilance': 'Island'}[mode]
        self.prepare_target(target, owner)
        if mode == 'source_leaves':
            self.add('remover', 'Murder', self.actor, 'hand')
        if mode == 'flying':
            self.add('ground_blocker', 'Grizzly Bears', self.foe, 'battlefield')
        ref = self.borrow('target') if mode == 'borrowed' else 'target'
        self.start_link(ref)
        self.move('target', 'exile', seat=owner)
        self.receipts()
        if mode == 'source_leaves':
            self.remove_source('remover', 'graveyard')
            self.receipts()
        # The existing driver runs the production end-of-turn trigger after its
        # normal cleanup work. The target must still be absent before that call.
        self.emit('cleanup')
        self.resolve()
        self.move('target', 'battlefield')
        if mode == 'returned_etb':
            self.life[owner] += 3
        self.target_receipts('target', owner)
        if mode == 'flying':
            self.combat('ground_blocker')
            self.life[self.foe] -= 3
        if mode == 'vigilance':
            self.next_own_main()
            self.swing('source', tapped=0)
            self.life[self.foe] -= 3

    def courier_attack(self):
        self.begin_cast('courier')
        self.resolve()
        self.move('courier', 'battlefield')
        self.prop('courier', 'sick', 0)
        self.emit('begin_attackers')
        self.emit('attack', 'courier', value=1)
        self.emit('finish_attackers')
        self.emit('assert_unused_choices', value=0)
        self.move('own_top', 'exile')
        self.emit('begin_blockers')
        self.emit('advance_combat')
        self.emit('combat_damage')
        self.resolve()
        self.emit('end_combat')
        self.life[self.foe] -= 1
        self.receipts()

    def courier_recover(self):
        self.mana(self.actor, RED, write=True)
        self.emit('refresh')
        self.emit('can_activate', 'courier', value=1)
        self.emit('activate', 'courier', value=1)
        self.emit('assert_unused_choices', value=0)
        self.emit('assert_stack', value=1)
        self.mana(self.actor)
        self.move('courier', 'graveyard')
        self.resolve()
        self.move('own_top', 'hand')

    def rift(self):
        mode = self.mode
        hidden = mode in ('hidden_only', 'hidden_plus_face_up')
        self.add('grave_foil', 'Giant Growth', self.actor, 'graveyard')
        self.add('grave_foe', 'Lightning Bolt', self.foe, 'graveyard')
        if hidden:
            self.add('courier', 'Bomat Courier', self.actor, 'hand')
            self.add('own_second', 'Island', self.actor, 'library')
        both = mode in ('choose_own', 'choose_foe')
        owner = self.foe if mode in ('foe_only', 'choose_foe') else self.actor
        public = mode not in ('none', 'hidden_only')
        if public:
            self.add('chosen', 'Unsummon', owner, 'exile')
            if mode != 'hidden_plus_face_up':
                self.add('same_zone_foil', 'Grizzly Bears', owner, 'exile')
            if both:
                self.add('other_zone_foil', 'Hill Giant', 1-owner, 'exile')
        if hidden:
            self.courier_attack()
        self.begin_cast()
        if both:
            self.emit('choose_player', player=self.actor, value=owner)
        if public:
            if hidden:
                self.emit('observe_search_filters', value=1)
            self.emit('choose_zone_card', player=self.actor, value=self.id('chosen'))
        if mode == 'hidden_only':
            # A correct engine has no legal target. If the legacy mandatory
            # raw-exile menu appears, the existing optional wrapper records the
            # forced selection and supplies a real selector result.
            self.emit('choose_optional_zone', player=self.actor, value=self.id('own_top'))
        self.resolve(unused=int(mode == 'hidden_only'))
        self.move('source', 'battlefield')
        self.stats('source')
        if mode == 'hidden_only':
            self.emit('assert_forced_zone_choices', value=0)
        if public:
            self.move('chosen', 'library')
            self.shuffles[owner] += 1
        if mode == 'hidden_plus_face_up':
            self.emit('assert_offered_count', value=1)
            self.emit('observe_search_filters', value=0)
            self.receipts()
            self.courier_recover()

    def play(self):
        self.emit('turn', player=self.actor)
        self.emit('present_ai_dialogs', value=1)
        for seat in (0, 1):
            self.mana(seat, write=True)
        cases = {
            JOURNEY: self.linked, HUNTER: self.linked, WISP: self.wisp, RIFT: self.rift,
            BUTCHER: self.linked, WARDER: self.warder, STAG: self.wisp,
            MAGGOT: self.hand_link, SCULLER: self.hand_link, FIEND: self.hand_link, FREEBOOTER: self.hand_link,
            SPHERE: self.named_group, DEPUTY: self.named_group, BINDING: self.binding,
            BAFFLING: self.baffling, GUARDIAN: self.guardian,
        }
        cases[self.cid]()
        self.receipts()
        self.emit('present_ai_dialogs', value=0)


    def swing(self, attacker, *, tapped=1, blocker=None):
        self.emit('begin_attackers')
        self.emit('attack', attacker, value=1)
        self.prop(attacker, 'attacking', 1)
        self.prop(attacker, 'tapped', tapped)
        self.emit('finish_attackers')
        self.emit('begin_blockers')
        if blocker:
            self.emit('block', blocker, other=attacker, value=1)
        self.emit('advance_combat')
        self.emit('combat_damage')
        self.resolve()
        self.emit('end_combat')

    def warder(self):
        mode = self.mode
        self.add('creature_foil', 'Hill Giant', self.foe, 'battlefield')
        self.add('land_foil', 'Forest', self.actor, 'battlefield')
        if mode == 'empty':
            self.start_link()
            return
        self.add('target', 'Fervor' if mode == 'enchantment' else 'Darksteel Relic', self.foe, 'battlefield')
        if mode == 'decline':
            self.start_link(decline=True)
            self.emit('assert_forced_target_choices', value=0)
            return
        self.add('remover', 'Murder', self.actor, 'hand')
        self.start_link('target')
        self.move('target', 'exile')
        self.receipts()
        self.remove_source('remover', 'graveyard')
        self.move('target', 'battlefield')
        self.target_receipts('target', self.foe)

    def hand_link(self):
        mode = self.mode
        name = 'Grizzly Bears' if mode == 'creature' else 'Lightning Bolt'
        selected = mode in ('spell', 'creature', 'flight')
        if selected:
            self.add('chosen', name, self.foe, 'hand')
        if mode != 'empty':
            self.add('land_card', 'Island', self.foe, 'hand')
        if self.cid == FREEBOOTER and mode in ('spell', 'creature_only', 'flight'):
            self.add('creature_card', 'Hill Giant', self.foe, 'hand')
        self.add('own_foil', name, self.actor, 'hand')
        self.add('unrelated_exile', 'Unsummon', self.foe, 'exile')
        self.add('remover', 'Murder', self.actor, 'hand')
        if mode == 'flight':
            self.add('ground', 'Grizzly Bears', self.foe, 'battlefield')
        self.begin_cast()
        if mode != 'empty':
            self.emit('observe_search_filters', value=1)
            if selected:
                self.emit('choose_zone_card', player=self.actor, value=self.id('chosen'))
            else:
                self.emit('choose_zone', player=self.actor, value=-1)
        self.resolve()
        self.move('source', 'battlefield')
        self.stats('source')
        if mode != 'empty':
            if selected:
                self.emit('assert_offered_legal_card', index=self.id('chosen'), value=1)
            self.emit('assert_offered_legal_card', index=self.id('land_card'), value=0)
            if 'creature_card' in self.cards:
                self.emit('assert_offered_legal_card', index=self.id('creature_card'), value=0)
            self.emit('observe_search_filters', value=0)
        if selected:
            self.move('chosen', 'exile')
        self.receipts()
        if mode == 'flight':
            self.combat('ground')
            self.life[self.foe] -= 1
        self.remove_source('remover', 'graveyard')
        if selected:
            self.move('chosen', 'hand')

    def named_group(self):
        mode = self.mode
        sphere = self.cid == SPHERE
        if mode in ('land_only', 'empty_opponent'):
            self.add('land', 'Forest', self.foe, 'battlefield')
            if mode == 'empty_opponent':
                self.add('friendly', 'Grizzly Bears', self.actor, 'battlefield')
            self.start_link()
            return
        if mode == 'excluded_name':
            # A legal token copy tests the same name and ceases to exist
            # if the incorrect mandatory callback exiles it.
            self.add('sphere_foil', 'Detention Sphere', self.actor, 'battlefield', state=32)
            self.add('fallback', 'Darksteel Relic', self.foe, 'battlefield')
            self.begin_cast()
            self.emit('choose_optional_target', 'sphere_foil', player=self.actor)
            # A rules-correct optional choice can be declined on its first offer.
            # If the legacy path exiles itself and re-enters, its second forced
            # selector can actually target a differently named nonland.
            self.emit('choose_optional_target', 'fallback', player=self.actor)
            self.resolve(unused=1)
            self.move('source', 'battlefield')
            self.emit('assert_forced_target_choices', value=0)
            return
        self.add('land', 'Forest', self.foe, 'battlefield')
        self.add('first', 'Grizzly Bears', self.foe, 'battlefield')
        self.add('same_foe', 'Grizzly Bears' if mode == 'same_name' else 'Hill Giant', self.foe, 'battlefield')
        self.add('same_own', 'Grizzly Bears', self.actor, 'battlefield')
        self.add('remover', 'Disenchant' if sphere else 'Murder', self.actor, 'hand')
        if mode == 'decline':
            self.begin_cast()
            self.emit('choose_optional_target', 'first', player=self.actor)
            self.resolve()
            self.move('source', 'battlefield')
            self.emit('assert_forced_target_choices', value=0)
            return
        self.start_link('first')
        subjects = ['first']
        if mode == 'same_name':
            subjects.append('same_foe')
        if sphere:
            subjects.append('same_own')
        for ref in subjects:
            self.move(ref, 'exile')
        self.receipts()
        self.remove_source('remover', 'graveyard')
        for ref in subjects:
            self.move(ref, 'battlefield')

    def next_main(self, player):
        self.emit('advance_turn')
        self.emit('untap_step', player=player)
        self.emit('turn', player=player)

    def move_cast_creature(self, ref):
        self.begin_cast(ref)
        self.resolve()
        self.move(ref, 'battlefield')
        self.stats(ref)

    def binding(self):
        mode = self.mode
        self.add('foe_land', 'Forest', self.foe, 'battlefield')
        self.add('own_nonland', 'Darksteel Relic', self.actor, 'battlefield')
        if mode == 'no_enemy_nonland':
            self.start_link()
            return
        self.add('target', 'Grizzly Bears', self.foe, 'battlefield')
        if mode == 'own_named_spell':
            self.add('own_match', 'Grizzly Bears', self.actor, 'hand')
        else:
            self.add('foe_match', 'Grizzly Bears', self.foe, 'hand')
            self.add('foe_other', 'Hill Giant', self.foe, 'hand')
            self.add('remover', 'Disenchant', self.actor, 'hand')
        self.start_link('target')
        self.move('target', 'exile')
        self.receipts()
        if mode == 'own_named_spell':
            self.move_cast_creature('own_match')
            return
        self.next_main(self.foe)
        pool = cost_from_oracle(self.oracle['Grizzly Bears'])
        self.mana(self.foe, pool, write=True)
        self.emit('refresh')
        self.emit('can_cast', 'foe_match', value=0)
        self.mana(self.foe, pool)
        self.move_cast_creature('foe_other')
        self.next_main(self.actor)
        self.remove_source('remover', 'graveyard')
        self.move('target', 'battlefield')
        self.receipts()
        self.next_main(self.foe)
        self.move_cast_creature('foe_match')

    def baffling(self):
        mode = self.mode
        eligible = mode in ('boundary', 'token_trample')
        self.add('own_foil', 'Grizzly Bears', self.actor, 'battlefield')
        if mode != 'own_only':
            self.add('too_large', 'Hill Giant', self.foe, 'battlefield')
        if eligible:
            self.add('target', 'Goblin Chariot', self.foe, 'battlefield')
        self.add('remover', 'Disenchant', self.actor, 'hand')
        self.start_link('target' if eligible else None)
        if eligible:
            self.move('target', 'exile')
        self.receipts()
        self.remove_source('remover', 'graveyard')
        self.cards['dinosaur'] = 'Dinosaur', self.foe, 'battlefield'
        self.zones[self.foe]['battlefield'].append('dinosaur')
        self.emit('bind_permanent', 'dinosaur', player=self.foe, value=self.id('dinosaur'), index=-1)
        self.prop('dinosaur', 'in_play', 1)
        for key in ('power', 'toughness'):
            self.prop('dinosaur', key, 3)
        self.prop('dinosaur', 'color', 1 << 3)
        if mode == 'token_trample':
            self.next_main(self.foe)
            self.swing('dinosaur', blocker='own_foil')
            self.move('own_foil', 'graveyard')
            self.life[self.actor] -= 1
            self.prop('dinosaur', 'damage', 2)

    def guardian(self):
        mode = self.mode
        self.add('opponent_foil', 'Darksteel Relic', self.foe, 'battlefield')
        if mode == 'only_self':
            self.start_link()
            return
        owner = self.foe if mode == 'borrowed' else self.actor
        if mode == 'entry_again':
            self.add('target', 'Cathedral Sanctifier', owner, 'hand')
            self.move_cast_creature('target')
            self.life[owner] += 3
            self.receipts()
        else:
            self.add('target', 'Hill Giant' if mode == 'borrowed' else 'Forest', owner, 'battlefield',
                     state=int(mode in ('land', 'decline')))
        target = self.borrow('target') if mode == 'borrowed' else 'target'
        if mode == 'decline':
            self.start_link(decline=True)
            self.prop('target', 'tapped', 1)
            self.emit('assert_forced_target_choices', value=0)
            return
        if mode == 'land':
            self.prop('target', 'tapped', 1)
        self.start_link(target)
        self.move('target', 'battlefield', seat=owner)
        if mode == 'entry_again':
            self.life[owner] += 3
        if mode != 'borrowed':
            self.target_receipts('target', owner)


SEATED = {
    'assert_life', 'assert_mana', 'assert_shuffle_count', 'bind_permanent', 'bind_permanent_copy',
    'choose_cancel_target', 'choose_optional_target', 'choose_optional_zone', 'choose_player',
    'choose_target', 'choose_trigger', 'choose_zone', 'choose_zone_card', 'set_mana', 'turn', 'untap_step',
}
VALUES = {
    'activate', 'assert_card', 'assert_forced_target_choices', 'assert_forced_zone_choices', 'assert_life',
    'assert_mana', 'assert_offered_count', 'assert_offered_legal_card', 'assert_shuffle_count', 'assert_stack',
    'assert_unused_choices', 'assert_zone', 'assert_zone_card', 'assert_zone_multiplicity', 'attack', 'bind_permanent',
    'bind_permanent_copy', 'block', 'can_activate', 'can_cast', 'cast', 'cast_response', 'choose_optional_zone',
    'choose_player', 'choose_zone', 'choose_zone_card', 'observe_search_filters', 'present_ai_dialogs', 'set_mana',
}
BY_INDEX = {'assert_offered_legal_card', 'assert_zone_card', 'assert_zone_multiplicity',
            'bind_permanent', 'bind_permanent_copy'}
ZONE_OPS = {'assert_zone', 'assert_zone_card', 'assert_zone_multiplicity'}
GAPS = {
    RIFT: ['The raw exile observer is visibility-blind. A real Bomat attack and the actual menu establish the face-down contrast.'],
    SPHERE: ['The synchronous legacy ETB driver does not expose source departure between target selection and resolution.'],
    GUARDIAN: ['The existing callback models the exile and return synchronously. A real returned entry and a tapped-reset witness observe its effects.'],
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
