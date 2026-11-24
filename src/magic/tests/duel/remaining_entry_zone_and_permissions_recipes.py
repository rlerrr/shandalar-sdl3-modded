"""Pinned remaining entry zone movement, permissions, manifest and payment."""

from collections import Counter
from functools import lru_cache
import json
from pathlib import Path

from death_counter_trigger_recipes import exact
from generate_cases import OPS
from morph_creature_recipes import FACE_DOWN
from recipes import cost_from_oracle


FAMILY = 'remaining_entry_zone_and_permissions'
LANE = 'zone-and-permissions'
LABEL = 'RemainingEntryZoneAndPermissionsRecipe'
SEED = 3500
DILUVIAN, ARCANIST, MINDCLAW, SYLVAN = 12924, 13283, 12593, 13077
MODES = {
    DILUVIAN: ('draw_and_exile', 'damage_and_exile', 'decline', 'own_grave_only', 'flying_combat'),
    ARCANIST: ('copy_one', 'copy_two', 'repeat_copy', 'decline_exile', 'no_instant', 'reentry_unlinked'),
    MINDCLAW: ('draw_from_hand', 'damage_from_hand', 'decline', 'no_enemy_spell', 'empty_enemy_hand'),
    SYLVAN: ('destroy_and_search', 'typed_forest', 'decline_search', 'no_enemy_noncreature',
             'indestructible', 'reach_combat'),
}
ZONE = dict(hand=0, battlefield=1, library=2, graveyard=3, exile=4)
ZERO = (0,) * 6

PROFILE = Path(__file__).with_name('data') / f'{FAMILY}_wave8_profiles.json'
ORIGINAL = frozenset(MODES)
CONFESSOR, CLOUDFORM, LIGHTFORM, RAGEFORM = 13095, 14529, 14578, 14605
PIONEER, WURM, TWILIGHT, VESSEL, HEADER, JAIL, PLAZA = 1026, 6292, 9707, 12502, 17736, 17969, 19689
MODES.update({
    CONFESSOR: ('life_two', 'life_zero', 'extort_paid', 'extort_short', 'enemy_cast'),
    CLOUDFORM: ('combat', 'face_up', 'hexproof', 'empty'),
    LIGHTFORM: ('combat', 'face_up', 'empty'),
    RAGEFORM: ('combat', 'face_up', 'empty'),
    PIONEER: ('basic_tapped', 'decline', 'no_basic'),
    WURM: ('all', 'some', 'decline', 'no_permanent'),
    TWILIGHT: ('same_turn', 'older_turn', 'persist_twice', 'flying_vigilance'),
    VESSEL: ('own_bottom', 'foe_bottom', 'only_foe', 'empty'),
    HEADER: ('own_two_words', 'enemy_two_words', 'borrowed_owner', 'decline', 'flying'),
    JAIL: ('manual_leave', 'wrong_upkeep', 'no_enemy_creature', 'doubles', 'not_doubles'),
    PLAZA: ('no_own_gates', 'one_gate', 'two_gates', 'second_gate_color'),
})

@lru_cache(maxsize=1)
def pins():
    return json.loads(PROFILE.read_bytes())


def mode_seats(cid):
    for mode in MODES[cid]:
        seats = (0,) if mode == 'life_zero' else (1,) if mode in ('doubles', 'not_doubles') else (0, 1)
        for seat in seats:
            yield mode, seat


def classify(record, csvid):
    pin = pins()['sources'].get(record.get('name'))
    if (csvid not in MODES or not pin or pin['registry']['csvid'] != csvid
            or not exact(record, pin['oracle']) or record.get('layout') != 'normal'):
        raise ValueError('complete pinned entry and zone rules required')
    mana = record['mana_cost']
    return dict(csvid=csvid, cost=cost_from_oracle(record) if mana else None)


def variants(shape):
    return [f'{mode}_{seat}' for mode, seat in mode_seats(shape['csvid'])]


def spec(declaration, record):
    pin = pins()['sources'].get(record.get('name'))
    if not pin:
        raise ValueError('pinned entry and zone source required')
    shape = classify(record, pin['registry']['csvid'])
    chosen = declaration.get('variants')
    if (declaration.get('family') != FAMILY or type(declaration.get('version')) is not int
            or declaration['version'] != 2 or declaration.get('oracle_text') != record['oracle_text']
            or not exact(declaration.get('params'), shape)):
        raise ValueError('whole versioned entry and zone declaration required')
    if (type(chosen) is not list or not chosen or any(type(v) is not str for v in chosen)
            or len(chosen) != len(set(chosen)) or set(chosen) - set(variants(shape))):
        raise ValueError('explicit distinct entry and zone variants required')
    return dict(shape, family=FAMILY)


def validate_witnesses(oracle, rows):
    for group in ('sources', 'witnesses'):
        for name, pin in pins()[group].items():
            row = rows.get(name, {})
            if (not exact(oracle.get(name), pin['oracle']) or not exact(row, pin['registry'])
                    or row.get('kind') not in ('card', 'token') or row.get('handler') in (None, 'NULL', '0')
                    or group == 'sources' and (row['kind'] != 'card' or row['handler'] == 'card_dummy')):
                raise ValueError(f'changed entry and zone identity: {name}')
    if pins()['native_face_down_observer']['csvid'] != FACE_DOWN:
        raise ValueError('changed engine identity for the manifest observer')



class Scene:
    def __init__(self, row, actor, mode, rows, oracle):
        self.cid, self.name = row['csvid'], row['name']
        self.actor, self.foe, self.mode = actor, 1 - actor, mode
        self.rows, self.oracle = rows, oracle
        self.setup, self.steps, self.cards = [], [], {}
        self.zones = {player: {zone: [] for zone in ZONE} for player in (0, 1)}
        self.hidden = set()
        self.seed = SEED
        self.life = [20, 20]
        self.shuffles = [0, 0]
        self.add('source', self.name, actor, 'hand')
        self.add('own_top', 'Plains', actor, 'library')
        self.add('foe_top', 'Mountain', self.foe, 'library')

    def emit(self, op, ref='', *, other='', prop='', player=0, value=0, zone='hand', index=0, color=0):
        assert op in OPS and player in (0, 1)
        self.steps.append((op, ref, other, prop, player, value, ZONE[zone], index, color))

    def add(self, ref, name, seat, zone):
        assert ref not in self.cards
        self.setup.append((ref, seat, ZONE[zone], self.rows[name]['csvid'], 0))
        self.cards[ref] = name, seat, zone
        self.zones[seat][zone].append(ref)

    def move(self, ref, zone):
        name, seat, old_zone = self.cards[ref]
        self.zones[seat][old_zone].remove(ref)
        self.zones[seat][zone].append(ref)
        self.cards[ref] = name, seat, zone

    def id(self, ref):
        return FACE_DOWN if ref in self.hidden else self.rows[self.cards[ref][0]]['csvid']

    def prop(self, ref, key, value):
        self.emit('assert_card', ref, prop=key, value=value)

    def mana(self, player, amounts=ZERO, *, write=False):
        for color, amount in enumerate(amounts):
            self.emit('set_mana' if write else 'assert_mana', player=player, value=amount, color=color)

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
        self.prop(ref, 'csvid', self.id(ref))
        self.prop(ref, 'power', int(self.oracle[name]['power']))
        self.prop(ref, 'toughness', int(self.oracle[name]['toughness']))
        if damage is not None:
            self.prop(ref, 'damage', damage)

    def next_main(self, player):
        self.emit('advance_turn')
        self.emit('untap_step', player=player)
        self.emit('turn', player=player)

    def next_own_main(self):
        self.next_main(self.foe)
        self.next_main(self.actor)

    def receipts(self):
        self.emit('assert_stack', value=0)
        names = sorted({name for name, _, _ in self.cards.values()})
        for player, zones in self.zones.items():
            self.mana(player)
            self.emit('assert_life', player=player, value=self.life[player])
            self.emit('assert_shuffle_count', player=player, value=self.shuffles[player])
            for zone, refs in zones.items():
                counts = Counter(self.id(ref) for ref in refs)
                self.emit('assert_zone', player=player, zone=zone, value=len(refs))
                for cid in [self.rows[name]['csvid'] for name in names] + ([FACE_DOWN] if self.hidden else []):
                    self.emit('assert_zone_multiplicity', player=player, zone=zone,
                              index=cid, value=counts[cid])
                if zone == 'library' and (not self.shuffles[player] or len(refs) == 1):
                    for index, ref in enumerate(refs):
                        self.emit('assert_zone_card', player=player, zone=zone, index=index, value=self.id(ref))

    def pick(self, ref, *, decline=False):
        self.emit('choose_optional_zone' if decline else 'choose_zone_card', player=self.actor, value=self.id(ref))

    def offered(self, eligible, ineligible):
        self.emit('assert_offered_selection_required', value=0)
        for value, refs in ((1, eligible), (0, ineligible)):
            for ref in refs:
                self.emit('assert_offered_legal_card', index=self.id(ref), value=value)

    def start_entry(self, *, optional=False, observe=False):
        self.begin_cast()
        if observe:
            self.emit('observe_search_filters', value=1)
        if optional and self.actor == 0:
            self.emit('choose_trigger', 'source', player=0)

    def finish_entry(self, *, unused=0, observe=False):
        self.resolve(unused=unused)
        if observe:
            self.emit('observe_search_filters', value=0)
        self.move('source', 'battlefield')
        self.stats('source')

    def combat(self, attacker, blocker, *, illegal=None):
        self.prop(attacker, 'sick', 0)
        self.emit('begin_attackers')
        self.emit('attack', attacker, value=1)
        self.emit('finish_attackers')
        self.emit('begin_blockers')
        if illegal:
            self.emit('block', illegal, other=attacker, value=0)
        self.emit('block', blocker, other=attacker, value=1)
        self.emit('advance_combat')
        self.prop(attacker, 'blocked', 1)
        self.emit('combat_damage')
        self.resolve()
        self.emit('end_combat')

    def diluvian(self):
        mode = self.mode
        selected = mode in ('draw_and_exile', 'damage_and_exile', 'decline')
        if selected:
            effect = 'Lightning Bolt' if mode == 'damage_and_exile' else 'Divination'
            self.add('borrowed_spell', effect, self.foe, 'graveyard')
        self.add('own_spell', 'Opt', self.actor, 'graveyard')
        self.add('grave_foil', 'Hill Giant', self.foe, 'graveyard')
        if mode == 'draw_and_exile':
            self.add('second_draw', 'Island', self.actor, 'library')
        if mode == 'flying_combat':
            self.add('ground', 'Grizzly Bears', self.foe, 'battlefield')
            self.add('reach', 'Giant Spider', self.foe, 'battlefield')
        self.start_entry(optional=True, observe=selected)
        if selected:
            self.pick('borrowed_spell', decline=mode == 'decline')
        if mode == 'damage_and_exile':
            self.emit('choose_player', player=self.actor, value=self.foe)
        self.finish_entry(observe=selected)
        if selected:
            self.offered(('borrowed_spell',), ('grave_foil',))
        if mode in ('draw_and_exile', 'damage_and_exile'):
            self.move('borrowed_spell', 'exile')
        if mode == 'draw_and_exile':
            self.move('own_top', 'hand')
            self.move('second_draw', 'hand')
        elif mode == 'damage_and_exile':
            self.life[self.foe] -= 3
        elif mode == 'decline':
            self.emit('assert_forced_zone_choices', value=0)
        elif mode == 'flying_combat':
            self.next_own_main()
            self.combat('source', 'reach', illegal='ground')
            self.move('reach', 'graveyard')
            self.stats('source', damage=2)

    def arcanist(self):
        mode = self.mode
        linked = mode in ('copy_one', 'copy_two', 'repeat_copy', 'reentry_unlinked')
        with_instant = linked or mode == 'decline_exile'
        if with_instant:
            self.add('instant', 'Think Twice' if mode == 'copy_two' else 'Reach Through Mists', self.actor, 'hand')
        self.add('sorcery_foil', 'Divination', self.actor, 'hand')
        self.add('foe_instant', 'Lightning Bolt', self.foe, 'hand')
        if mode == 'repeat_copy':
            self.add('repeat_draw', 'Island', self.actor, 'library')
        if mode == 'reentry_unlinked':
            self.add('bounce', 'Unsummon', self.actor, 'hand')
        self.start_entry(optional=True, observe=with_instant)
        if with_instant:
            self.pick('instant', decline=mode == 'decline_exile')
        self.finish_entry(observe=with_instant)
        if with_instant:
            self.offered(('instant',), ('sorcery_foil',))
        if not linked:
            self.emit('can_activate', 'source', value=0)
            if with_instant:
                self.emit('assert_forced_zone_choices', value=0)
            return
        self.move('instant', 'exile')
        self.prop('source', 'tapped', 0)
        self.prop('source', 'sick', 1)
        self.receipts()
        if mode == 'reentry_unlinked':
            self.begin_cast('bounce', target='source')
            self.resolve()
            self.move('bounce', 'graveyard')
            self.move('source', 'hand')
            self.emit('bind_hand', 'source', player=self.actor, value=self.cid, index=-1)
            self.prop('source', 'in_hand', 1)
            self.start_entry(optional=True)
            self.finish_entry()
            self.next_own_main()
            self.emit('can_activate', 'source', value=0)
            return
        self.activate_copy('own_top')
        if mode == 'repeat_copy':
            self.receipts()
            self.activate_copy('repeat_draw')

    def activate_copy(self, draw):
        self.next_own_main()
        self.prop('source', 'sick', 0)
        self.prop('source', 'tapped', 0)
        amount = self.oracle[self.cards['instant'][0]]['cmc']
        assert amount in (1, 2)
        self.mana(self.actor, (int(amount), 0, 0, 0, 0, 0), write=True)
        self.emit('refresh')
        self.emit('can_activate', 'source', value=1)
        if self.actor == 0:
            self.pick('instant')
            self.emit('choose_dialog', player=0, value=0)
            self.pick('instant')
        self.emit('activate', 'source', value=1)
        self.emit('assert_unused_choices', value=0)
        self.prop('source', 'tapped', 1)
        self.emit('assert_stack', value=1)
        self.mana(self.actor)
        self.resolve()
        self.move(draw, 'hand')

    def mindclaw(self):
        mode = self.mode
        selected = mode in ('draw_from_hand', 'damage_from_hand', 'decline')
        if selected:
            effect = 'Lightning Bolt' if mode == 'damage_from_hand' else 'Divination'
            self.add('borrowed_spell', effect, self.foe, 'hand')
        self.add('own_spell', 'Opt', self.actor, 'hand')
        if mode != 'empty_enemy_hand':
            self.add('hand_foil', 'Hill Giant', self.foe, 'hand')
            self.add('land_foil', 'Forest', self.foe, 'hand')
        if mode == 'draw_from_hand':
            self.add('second_draw', 'Island', self.actor, 'library')
        observe = mode != 'empty_enemy_hand'
        self.start_entry(observe=observe)
        if selected:
            self.pick('borrowed_spell', decline=mode == 'decline')
        elif mode == 'no_enemy_spell':
            self.emit('choose_zone', player=self.actor, value=-1)
        if mode == 'damage_from_hand':
            self.emit('choose_player', player=self.actor, value=self.foe)
        self.finish_entry(observe=observe)
        if observe:
            self.offered(('borrowed_spell',) if selected else (), ('hand_foil', 'land_foil'))
        if mode in ('draw_from_hand', 'damage_from_hand'):
            self.move('borrowed_spell', 'graveyard')
        if mode == 'draw_from_hand':
            self.move('own_top', 'hand')
            self.move('second_draw', 'hand')
        elif mode == 'damage_from_hand':
            self.life[self.foe] -= 3
        elif mode == 'decline':
            self.emit('assert_forced_zone_choices', value=0)

    def sylvan(self):
        mode = self.mode
        has_target = mode not in ('no_enemy_noncreature', 'reach_combat')
        selected = mode in ('destroy_and_search', 'typed_forest')
        land = 'Dryad Arbor' if mode == 'typed_forest' else 'Forest'
        if has_target:
            target = 'Darksteel Relic' if mode == 'indestructible' else 'Mountain'
            self.add('enemy_permanent', target, self.foe, 'battlefield')
        self.add('own_permanent', 'Island', self.actor, 'battlefield')
        flyer = 'Suntail Hawk' if mode == 'reach_combat' else 'Grizzly Bears'
        self.add('enemy_creature', flyer, self.foe, 'battlefield')
        self.add('forest', land, self.actor, 'library')
        self.start_entry(observe=has_target)
        if has_target:
            self.emit('choose_target', 'enemy_permanent', player=self.actor)
            # If an engine incorrectly searches for a Forest after failing to destroy
            # the indestructible target, legally choose zero cards. The final state
            # and this explicit unused-intent receipt still require the correct limit.
            self.pick('forest', decline=not selected)
        self.finish_entry(unused=int(mode == 'indestructible'), observe=has_target)
        if has_target:
            self.shuffles[self.actor] = 1
        if has_target and mode != 'indestructible':
            self.move('enemy_permanent', 'graveyard')
            self.offered(('forest',), ('own_top',))
            if selected:
                self.move('forest', 'battlefield')
                self.emit('bind_permanent', 'forest', player=self.actor, value=self.id('forest'), index=-1)
                self.prop('forest', 'in_play', 1)
                self.prop('forest', 'csvid', self.id('forest'))
                self.prop('forest', 'tapped', 1)
        if has_target:
            self.emit('assert_forced_zone_choices', value=0)
        self.prop('own_permanent', 'in_play', 1)
        if mode == 'reach_combat':
            self.next_main(self.foe)
            self.combat('enemy_creature', 'source')
            self.move('enemy_creature', 'graveyard')
            self.stats('source', damage=1)
        else:
            self.stats('enemy_creature')

    def finish_noncreature(self):
        self.resolve()
        self.move('source', 'battlefield')
        self.prop('source', 'in_play', 1)
        self.prop('source', 'csvid', self.cid)

    def phase_upkeep(self, player):
        self.emit('advance_turn')
        self.emit('untap_step', player=player)
        self.emit('upkeep_step', player=player)
        self.emit('assert_unused_choices', value=0)
        self.emit('assert_stack', value=0)

    def flight_to_player(self, creature, *, ground=None, damage, lifelink=False, vigilance=False):
        self.next_own_main()
        self.prop(creature, 'sick', 0)
        self.emit('begin_attackers')
        self.emit('attack', creature, value=1)
        self.prop(creature, 'tapped', int(not vigilance))
        self.emit('finish_attackers')
        self.emit('begin_blockers')
        if ground:
            self.emit('block', ground, other=creature, value=0)
        self.emit('advance_combat')
        self.prop(creature, 'blocked', 0)
        self.emit('combat_damage')
        self.resolve()
        self.emit('end_combat')
        self.life[self.foe] -= damage
        if lifelink:
            self.life[self.actor] += damage

    def vizkopa(self):
        mode = self.mode
        self.add('secret', 'Lightning Bolt', self.foe, 'hand')
        if mode == 'life_two':
            self.add('second', 'Island', self.foe, 'hand')
        if mode in ('extort_paid', 'extort_short', 'enemy_cast'):
            witness_owner = self.foe if mode == 'enemy_cast' else self.actor
            self.add('witness', 'Darksteel Relic', witness_owner, 'hand')
        self.start_entry(optional=True, observe=mode == 'life_two')
        amount = 0 if mode == 'life_zero' else 2 if mode == 'life_two' else (2 if mode == 'enemy_cast' else 1)
        if self.actor == 0:
            self.emit('choose_number', player=0, value=amount)
        if amount:
            self.emit('choose_zone_card', player=self.foe, value=self.id('secret'))
            if amount == 2:
                second = 'witness' if mode == 'enemy_cast' else 'second'
                self.emit('choose_zone_card', player=self.foe, value=self.id(second))
            self.pick('secret')
        else:
            self.pick('secret', decline=True)
        self.finish_entry(unused=int(amount == 0), observe=mode == 'life_two')
        if amount:
            self.life[self.actor] -= amount
            self.move('secret', 'exile')
        if mode not in ('extort_paid', 'extort_short', 'enemy_cast'):
            return
        self.receipts()
        enemy = mode == 'enemy_cast'
        pay = mode == 'extort_paid'
        caster = self.foe if enemy else self.actor
        fee = (0, 0, 0, 0, 0, 1) if pay or enemy else ZERO
        self.emit('turn', player=caster)
        self.mana(caster, fee, write=True)
        self.emit('refresh')
        self.emit('cast', 'witness', value=1)
        self.emit('assert_stack', value=1)
        if self.actor == 0 and not enemy:
            self.emit('choose_trigger', 'source', player=0)
        self.emit('announce_spell', 'witness')
        self.resolve()
        self.move('witness', 'battlefield')
        self.prop('witness', 'in_play', 1)
        if pay:
            self.life[self.actor] += 1
            self.life[self.foe] -= 1
        if enemy:
            self.mana(caster, fee)
        self.mana(caster, write=True)

    def forms(self):
        mode = self.mode
        # The existing top slot remains first; replace its fixture identity
        # before adding any further library witness.
        if mode == 'empty':
            self.setup.pop(1)
            self.zones[self.actor]['library'].remove('own_top')
            del self.cards['own_top']
        else:
            self.setup[1] = ('own_top', self.actor, ZONE['library'], self.rows['Hill Giant']['csvid'], 0)
            self.cards['own_top'] = 'Hill Giant', self.actor, 'library'
            self.add('next_card', 'Island', self.actor, 'library')
        if mode not in ('empty', 'face_up') and self.cid != RAGEFORM:
            self.add('ground', 'Grizzly Bears', self.foe, 'battlefield')
        if self.cid == CLOUDFORM and mode == 'hexproof':
            self.add('enemy_spell', 'Murder', self.foe, 'hand')
            self.add('own_spell', 'Murder', self.actor, 'hand')
        self.start_entry()
        self.resolve()
        if mode == 'empty':
            self.move('source', 'graveyard')
            return
        self.move('source', 'battlefield')
        self.prop('source', 'in_play', 1)
        self.move('own_top', 'battlefield')
        self.hidden.add('own_top')
        self.emit('bind_permanent', 'own_top', player=self.actor, value=FACE_DOWN, index=-1)
        for key, value in (('in_play', 1), ('csvid', FACE_DOWN), ('power', 2), ('toughness', 2)):
            self.prop('own_top', key, value)
        self.receipts()
        if mode == 'face_up':
            self.mana(self.actor, cost_from_oracle(self.oracle['Hill Giant']), write=True)
            self.emit('refresh')
            self.emit('can_activate_granted_legacy', 'own_top', index=self.cid, value=1)
            self.emit('activate_granted_legacy', 'own_top', index=self.cid, value=1)
            self.resolve()
            self.mana(self.actor)
            # The real untap after the flip needs empty pools. Assert the
            # entire payment first, then ensure the next phase can still run.
            self.mana(self.actor, write=True)
            self.hidden.remove('own_top')
            self.stats('own_top')
            self.prop('source', 'in_play', 1)
        if mode == 'hexproof':
            self.emit('can_target_creature', 'enemy_spell', other='own_top', value=0)
            self.emit('can_target_creature', 'own_spell', other='own_top', value=1)
            return
        flying = self.cid in (CLOUDFORM, LIGHTFORM)
        power = 3 if mode == 'face_up' else 2
        self.flight_to_player('own_top', ground='ground' if flying and mode != 'face_up' else None,
                              damage=2*power if self.cid == RAGEFORM else power,
                              lifelink=self.cid == LIGHTFORM)

    def pioneer(self):
        selected = self.mode in ('basic_tapped', 'decline')
        if selected:
            self.add('land', 'Island', self.actor, 'hand')
        self.add('nonbasic', 'Dryad Arbor', self.actor, 'hand')
        self.add('foe_basic', 'Forest', self.foe, 'hand')
        self.start_entry(optional=True, observe=selected)
        if selected:
            self.pick('land', decline=self.mode == 'decline')
        self.finish_entry(observe=selected)
        if selected:
            self.offered(('land',), ('nonbasic',))
        if self.mode == 'basic_tapped':
            self.move('land', 'battlefield')
            self.emit('bind_permanent', 'land', player=self.actor, value=self.id('land'), index=-1)
            self.prop('land', 'tapped', 1)
        elif selected:
            self.emit('assert_forced_zone_choices', value=0)

    def tempting(self):
        mode = self.mode
        payload = [('artifact', 'Darksteel Relic'), ('creature', 'Hill Giant'),
                   ('enchantment', 'Fervor'), ('land', 'Island')]
        self.add('foil', 'Lightning Bolt', self.foe, 'hand')
        self.add('own_land', 'Forest', self.actor, 'hand')
        if mode != 'no_permanent':
            for ref, name in payload:
                self.add(ref, name, self.foe, 'hand')
        self.start_entry(observe=mode in ('some', 'decline'))
        selected = payload if mode == 'all' else payload[:2] if mode == 'some' else []
        for ref, _ in selected:
            self.emit('choose_zone_card', player=self.foe, value=self.id(ref))
        if mode in ('some', 'decline'):
            self.emit('choose_optional_zone', player=self.foe,
                      value=self.id('land' if mode == 'some' else 'artifact'))
        self.finish_entry(observe=mode in ('some', 'decline'))
        if mode in ('some', 'decline'):
            self.emit('assert_forced_zone_choices', value=0)
            self.offered(('land',), ('foil',))
        for ref, _ in selected:
            self.move(ref, 'battlefield')

    def twilight(self):
        mode = self.mode
        self.add('old_grave', 'Grizzly Bears', self.actor, 'graveyard')
        self.add('foe_ground', 'Scathe Zombies', self.foe, 'battlefield')
        if mode in ('same_turn', 'older_turn', 'persist_twice'):
            self.add('own_fallen', 'Hill Giant', self.actor, 'battlefield')
        if mode == 'same_turn':
            self.add('own_relic', 'Sol Ring', self.actor, 'battlefield')
            self.add('spell', 'Unsummon', self.actor, 'hand')
            self.add('bounce_target', 'Walking Corpse', self.foe, 'battlefield')
            self.begin_cast('spell', target='bounce_target')
            self.resolve()
            self.move('spell', 'graveyard')
            self.move('bounce_target', 'hand')
            self.destroy_real('own_relic')
            self.destroy_real('foe_ground')
        if mode in ('same_turn', 'older_turn'):
            self.destroy_real('own_fallen')
        if mode == 'older_turn':
            self.next_own_main()
        self.start_entry()
        self.finish_entry()
        if mode == 'same_turn':
            for ref in ('own_fallen', 'own_relic'):
                self.move(ref, 'hand')
        if mode == 'flying_vigilance':
            self.flight_to_player('source', ground='foe_ground', damage=5, vigilance=True)
        if mode != 'persist_twice':
            return
        self.destroy_real('own_fallen')
        self.destroy_real('source')
        self.move('source', 'battlefield')
        self.move('own_fallen', 'hand')
        self.emit('bind_permanent', 'source', player=self.actor, value=self.cid, index=-1)
        self.prop('source', 'minus1_counters', 1)
        self.prop('source', 'power', 4)
        self.prop('source', 'toughness', 4)
        self.receipts()
        self.destroy_real('source')

    def destroy_real(self, ref):
        self.emit('destroy', ref)
        self.resolve()
        self.move(ref, 'graveyard')

    def produce(self, color, *, choose=False):
        if choose:
            self.emit('choose_color', player=self.actor, value=color)
        self.emit('activate', 'source', value=1)
        self.emit('assert_unused_choices', value=0)
        self.emit('assert_stack', value=0)
        self.prop('source', 'tapped', 1)
        payment = tuple(int(pos == color) for pos in range(6))
        self.mana(self.actor, payment)
        self.emit('can_activate', 'source', value=0)
        self.mana(self.actor, write=True)

    def vessel(self):
        mode = self.mode
        selected = mode in ('own_bottom', 'foe_bottom', 'only_foe')
        owner = self.actor if mode == 'own_bottom' else self.foe
        if selected:
            self.add('target', 'Hill Giant', owner, 'graveyard')
            self.add('top_next', 'Island', owner, 'library')
        if mode in ('own_bottom', 'foe_bottom'):
            self.add('other_grave', 'Lightning Bolt', 1-owner, 'graveyard')
        self.start_entry(observe=selected)
        if mode in ('own_bottom', 'foe_bottom'):
            self.emit('choose_player', player=self.actor, value=owner)
        if selected:
            self.pick('target')
        self.finish_noncreature()
        if selected:
            self.emit('assert_offered_selection_required', value=1)
            self.emit('observe_search_filters', value=0)
            self.move('target', 'library')
        if mode in ('own_bottom', 'foe_bottom'):
            self.produce(2 if mode == 'own_bottom' else 4, choose=True)

    def double_header(self):
        mode = self.mode
        owner = self.actor if mode == 'own_two_words' else self.foe
        self.add('one_word', 'Fervor', self.foe, 'battlefield')
        self.add('three_words', 'Swords to Plowshares', self.foe, 'graveyard')
        self.add('three_word_permanent', 'Wall of Swords', self.foe, 'battlefield')
        self.add('two_words', 'Hill Giant', owner, 'battlefield')
        if mode == 'borrowed_owner':
            self.add('theft', 'Act of Treason', self.actor, 'hand')
            self.begin_cast('theft', target='two_words')
            self.resolve()
            self.move('theft', 'graveyard')
            self.zones[self.foe]['battlefield'].remove('two_words')
            self.zones[self.actor]['battlefield'].append('two_words')
            name, _, zone = self.cards['two_words']
            self.cards['two_words'] = name, self.actor, zone
            self.emit('bind_permanent', 'two_words', player=self.actor, value=self.id('two_words'), index=-1)
        self.start_entry(optional=True)
        if mode in ('decline', 'flying'):
            self.emit('choose_optional_target', 'two_words', player=self.actor)
        else:
            self.emit('choose_target', 'two_words', player=self.actor)
        self.finish_entry()
        if mode not in ('decline', 'flying'):
            if mode == 'borrowed_owner':
                self.zones[self.actor]['battlefield'].remove('two_words')
                self.zones[self.foe]['hand'].append('two_words')
                self.cards['two_words'] = 'Hill Giant', self.foe, 'hand'
            else:
                self.move('two_words', 'hand')
        else:
            self.emit('assert_forced_target_choices', value=0)
        if mode == 'flying':
            self.flight_to_player('source', ground='two_words', damage=2)

    def jail(self):
        mode = self.mode
        self.add('decoy', 'Grizzly Bears', self.actor, 'battlefield')
        exists = mode != 'no_enemy_creature'
        if exists:
            self.add('target', 'Hill Giant', self.foe, 'battlefield')
        self.start_entry()
        if exists:
            self.emit('choose_target', 'target', player=self.actor)
        self.finish_noncreature()
        if not exists:
            return
        self.move('target', 'exile')
        self.receipts()
        if mode in ('doubles', 'not_doubles'):
            assert self.actor == 1
            self.seed = 2 if mode == 'doubles' else 1
            self.phase_upkeep(self.foe)
        if mode == 'doubles':
            self.move('source', 'graveyard')
            self.move('target', 'battlefield')
            return
        if mode == 'wrong_upkeep':
            self.next_main(self.foe)
            self.phase_upkeep(self.actor)
            self.receipts()
        if mode in ('manual_leave', 'wrong_upkeep', 'not_doubles'):
            self.destroy_real('source')
            self.move('target', 'battlefield')

    def plaza(self):
        mode = self.mode
        self.add('ordinary_land', 'Mountain', self.actor, 'battlefield')
        self.add('enemy_gate', 'Rakdos Guildgate', self.foe, 'battlefield')
        if mode != 'no_own_gates':
            self.add('first_gate', 'Azorius Guildgate', self.actor, 'battlefield')
        if mode in ('two_gates', 'second_gate_color'):
            self.add('second_gate', 'Golgari Guildgate', self.actor, 'battlefield')
        self.emit('play_land', 'source', value=1)
        self.move('source', 'battlefield')
        self.prop('source', 'in_play', 1)
        self.prop('source', 'tapped', 0)
        if mode in ('two_gates', 'second_gate_color'):
            self.life[self.actor] += 3
        # Exercise the two-Gate life threshold with a legal Azorius color.
        # The no-Gates cases separately require Plaza's printed colorless mana.
        color = {'no_own_gates': 0, 'one_gate': 2, 'two_gates': 2, 'second_gate_color': 3}[mode]
        self.produce(color, choose=mode != 'no_own_gates')


    def play(self):
        self.emit('turn', player=self.actor)
        self.emit('present_ai_dialogs', value=1)
        for seat in (0, 1):
            self.mana(seat, write=True)
        handlers = {
            DILUVIAN: self.diluvian, ARCANIST: self.arcanist, MINDCLAW: self.mindclaw, SYLVAN: self.sylvan,
            CONFESSOR: self.vizkopa, CLOUDFORM: self.forms, LIGHTFORM: self.forms, RAGEFORM: self.forms,
            PIONEER: self.pioneer, WURM: self.tempting, TWILIGHT: self.twilight, VESSEL: self.vessel,
            HEADER: self.double_header, JAIL: self.jail, PLAZA: self.plaza,
        }
        handlers[self.cid]()
        self.receipts()
        self.emit('present_ai_dialogs', value=0)


SEATED = {
    'assert_life', 'assert_mana', 'bind_hand', 'bind_permanent', 'choose_color', 'choose_dialog',
    'choose_number', 'choose_optional_target', 'choose_optional_zone', 'choose_player', 'choose_target',
    'choose_trigger', 'choose_zone', 'choose_zone_card', 'set_mana', 'turn', 'untap_step', 'upkeep_step',
    'assert_shuffle_count',
}
VALUES = {
    'activate', 'activate_granted_legacy', 'can_activate_granted_legacy', 'assert_card', 'assert_forced_zone_choices', 'assert_forced_target_choices', 'assert_life',
    'assert_mana', 'assert_offered_legal_card', 'assert_offered_selection_required', 'assert_shuffle_count',
    'assert_stack', 'assert_unused_choices', 'assert_zone', 'assert_zone_card', 'assert_zone_multiplicity',
    'attack', 'bind_hand', 'bind_permanent', 'block', 'can_activate', 'can_target_creature', 'cast',
    'choose_color', 'choose_dialog', 'choose_number', 'choose_optional_zone', 'choose_player', 'choose_zone',
    'choose_zone_card', 'observe_search_filters', 'play_land', 'present_ai_dialogs', 'set_mana',
}
BY_INDEX = {'activate_granted_legacy', 'can_activate_granted_legacy', 'assert_offered_legal_card', 'assert_zone_card', 'assert_zone_multiplicity', 'bind_hand', 'bind_permanent'}
ZONE_OPS = {'assert_zone', 'assert_zone_card', 'assert_zone_multiplicity'}
GAPS = {
    DILUVIAN: ['A duel has one opposing graveyard.'],
    ARCANIST: ['The current callback does not offer a distinct decline for an untargeted copy after payment.'],
    SYLVAN: ['A duel has one opposing target and a maximum search quantity of one.'],
    CONFESSOR: ['The AI chooses its life amount automatically. Only the manual seat can choose zero.'],
    JAIL: ['The headless network RNG is local to seat 0. Dice cases use a seat-0 exiled owner.'],
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
        raise ValueError('entry and zone source CSVID changed')
    names = {row['csvid']: name for name, row in rows.items() if row['kind'] in ('card', 'token')}
    zones = tuple(ZONE)
    cases = []
    for variant in selected:
        mode, seat = variant.rsplit('_', 1)
        scene = Scene(rows[name], int(seat), mode, rows, oracle)
        scene.play()
        setup = []
        for ref, player, zone, cid, state in scene.setup:
            assert state == 0
            setup.append(dict(ref=ref, player=player, zone=zones[zone], name=names[cid]))
        cases.append(dict(name=f'{LABEL}_{csvid}_{variant}', card=name, fixture='empty_main', seed=scene.seed,
                          setup=setup, steps=list(shaped_steps(scene.steps)),
                          _recipe=dict(family=FAMILY, version=2, variant=variant, params=declaration['params'],
                                       oracle_text=declaration['oracle_text'], untested_components=GAPS.get(csvid, []))))
    return cases
