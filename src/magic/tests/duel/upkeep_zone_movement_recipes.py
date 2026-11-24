"""Pinned upkeep effects with actual zone, timing, and lifecycle receipts."""

from collections import Counter
from functools import lru_cache
import json
from pathlib import Path

from death_counter_trigger_recipes import exact
from generate_cases import OPS
from recipes import cost_from_oracle


FAMILY = 'upkeep_zone_movement'
LANE = 'zone-movement'
LABEL = 'UpkeepZoneMovementRecipe'
SEED = 3686
AUTOMATON, CRUSHER, STINGS, EQUIPOISE = 4678, 9338, 7620, 2699
MODES = {
    AUTOMATON: ('threshold', 'above', 'below', 'foe_upkeep', 'opponent_many', 'borrowed'),
    CRUSHER: ('repeat_prefix', 'only_lands', 'empty', 'nonland_first', 'own_land_destroyed',
              'foe_land_destroyed', 'nonland_destroyed'),
    STINGS: ('cast_return', 'self_cast', 'repeat', 'decline', 'tie', 'less', 'foe_upkeep'),
    EQUIPOISE: ('three_types', 'phase_in', 'equal', 'indirect_aura', 'foe_shroud', 'foe_upkeep'),
}
ZONE = dict(hand=0, battlefield=1, library=2, graveyard=3, exile=4)
ZERO = (0,) * 6

PROFILE = Path(__file__).with_name('data') / f'{FAMILY}_wave8_profiles.json'
ORIGINAL = frozenset(MODES)
ICHORID, IMAGINARY, COILS, BLOOM, MARK, MIST, MORTAL = 5824, 3811, 6705, 12770, 7898, 5977, 5840
SERVITOR, NETHER, SCALES, DRUIDS, GHOULS, MARROW, SQUEE = 7093, 4543, 3833, 3522, 3523, 9014, 4619
MODES.update({
    ICHORID: ('cast_haste_end', 'return_haste_end', 'decline', 'no_payment'),
    IMAGINARY: ('held', 'empty', 'wrong_upkeep'),
    COILS: ('threshold', 'above', 'below', 'other_death', 'token_death'),
    BLOOM: ('zero', 'one_held', 'one_used', 'two_uses'),
    MARK: ('own_host', 'foreign_host', 'foreign_auras'),
    MIST: ('both_choose', 'other_target', 'no_grave_cards'),
    MORTAL: ('wins', 'below', 'foe_threshold', 'foe_upkeep'),
    SERVITOR: ('both_return', 'copies', 'source_removed', 'foe_upkeep'),
    NETHER: ('only_creature', 'creature_foil', 'second_copy', 'decline', 'foe_upkeep', 'actual_death'),
    SCALES: ('both', 'empty_hand', 'foe_upkeep'),
    DRUIDS: ('own', 'enemy', 'decline', 'tie', 'shroud', 'no_creature'),
    GHOULS: ('own', 'enemy', 'decline', 'tie', 'shroud'),
    MARROW: ('copy', 'no_copy', 'empty', 'owner_upkeep'),
    SQUEE: ('returns', 'decline', 'foe_upkeep', 'actual_death'),
})
@lru_cache(maxsize=1)
def pins():
    return json.loads(PROFILE.read_bytes())


def mode_seats(cid):
    for mode in MODES[cid]:
        seats = (0,) if mode == 'decline' and cid in (NETHER, SQUEE) else (0, 1)
        for seat in seats:
            yield mode, seat


def classify(record, csvid):
    pin = pins()['sources'].get(record.get('name'))
    if (csvid not in MODES or not pin or pin['registry']['csvid'] != csvid
            or not exact(record, pin['oracle']) or record.get('layout') != 'normal'):
        raise ValueError('complete pinned upkeep zone rules required')
    mana = record['mana_cost']
    return dict(csvid=csvid, cost=cost_from_oracle(record) if '{X}' not in mana else [0, 0, 0, 1, 0, 0])


def variants(shape):
    return [f'{mode}_{seat}' for mode, seat in mode_seats(shape['csvid'])]


def spec(declaration, record):
    pin = pins()['sources'].get(record.get('name'))
    if not pin:
        raise ValueError('pinned upkeep zone source required')
    shape = classify(record, pin['registry']['csvid'])
    chosen = declaration.get('variants')
    if (declaration.get('family') != FAMILY or type(declaration.get('version')) is not int
            or declaration['version'] != 2 or declaration.get('oracle_text') != record['oracle_text']
            or not exact(declaration.get('params'), shape)):
        raise ValueError('whole versioned upkeep zone declaration required')
    if (type(chosen) is not list or not chosen or any(type(v) is not str for v in chosen)
            or len(chosen) != len(set(chosen)) or set(chosen) - set(variants(shape))):
        raise ValueError('explicit distinct upkeep zone variants required')
    return dict(shape, family=FAMILY)


def validate_witnesses(oracle, rows):
    for group in ('sources', 'witnesses'):
        for name, pin in pins()[group].items():
            row = rows.get(name, {})
            if (not exact(oracle.get(name), pin['oracle']) or not exact(row, pin['registry'])
                    or row.get('kind') not in ('card', 'token') or row.get('handler') in (None, 'NULL', '0')
                    or group == 'sources' and (row['kind'] != 'card' or row['handler'] == 'card_dummy')):
                raise ValueError(f'changed upkeep zone identity: {name}')


class Scene:
    def __init__(self, row, actor, mode, rows, oracle):
        self.cid, self.name = row['csvid'], row['name']
        self.actor, self.foe, self.mode = actor, 1 - actor, mode
        self.rows, self.oracle = rows, oracle
        self.setup, self.steps, self.cards = [], [], {}
        self.zones = {player: {zone: [] for zone in ZONE} for player in (0, 1)}
        self.life = [20, 20]
        self.shuffles = [0, 0]
        self.add('source', self.name, actor, 'hand')
        self.add('own_top', 'Plains', actor, 'library')
        self.add('foe_top', 'Mountain', self.foe, 'library')

    def emit(self, op, ref='', *, other='', prop='', player=0, value=0, zone='hand', index=0, color=0):
        assert op in OPS and player in (0, 1)
        self.steps.append((op, ref, other, prop, player, value, ZONE[zone], index, color))

    def add(self, ref, name, seat, zone, *, state=0):
        assert ref not in self.cards
        self.setup.append((ref, seat, ZONE[zone], self.rows[name]['csvid'], state))
        self.cards[ref] = name, seat, zone
        self.zones[seat][zone].append(ref)

    def move(self, ref, zone):
        name, seat, old_zone = self.cards[ref]
        self.zones[seat][old_zone].remove(ref)
        self.zones[seat][zone].append(ref)
        self.cards[ref] = name, seat, zone

    def id(self, ref):
        return self.rows[self.cards[ref][0]]['csvid']

    def prop(self, ref, key, value):
        self.emit('assert_card', ref, prop=key, value=value)

    def mana(self, player, amounts=ZERO, *, write=False):
        for color, amount in enumerate(amounts):
            self.emit('set_mana' if write else 'assert_mana', player=player, value=amount, color=color)

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
                current = [self.cards[ref][0] for ref in refs]
                counts = Counter(current)
                self.emit('assert_zone', player=player, zone=zone, value=len(current))
                for name in names:
                    self.emit('assert_zone_multiplicity', player=player, zone=zone,
                              index=self.rows[name]['csvid'], value=counts[name])
                if zone == 'library' and (not self.shuffles[player] or len(refs) == 1):
                    for index, ref in enumerate(refs):
                        self.emit('assert_zone_card', player=player, zone=zone, index=index, value=self.id(ref))

    def select_library(self, payload):
        self.setup = [row for row in self.setup if row[0] != 'own_top']
        self.zones[self.actor]['library'].remove('own_top')
        del self.cards['own_top']
        for ref, name in payload:
            self.add(ref, name, self.actor, 'library')

    def cast(self, ref, target=None, *, replace_cost=None, mode=None, setup_choice=None):
        name, player, zone = self.cards[ref]
        assert zone == 'hand'
        nominal = cost_from_oracle(self.oracle[name])
        paid = nominal if replace_cost is None else replace_cost
        available = paid if mode is None else (5, 0, 0, 0, 0, 1)
        self.mana(player, available, write=True)
        self.emit('refresh')
        if mode is not None:
            self.emit('choose_dialog', player=player, value=mode)
        if target:
            self.emit('choose_target', target, player=player)
        if setup_choice:
            setup_choice()
        self.emit('cast', ref, value=1)
        self.emit('assert_unused_choices', value=0)
        self.emit('assert_stack', value=1)
        self.prop(ref, 'in_hand', 0)
        remaining = tuple(a - b for a, b in zip(available, paid))
        self.mana(player, remaining)
        if any(remaining):
            self.mana(player, write=True)

    def owner_turn_upkeep(self, choices=None, *, unused=0):
        self.upkeep(self.foe)
        self.receipts()
        self.upkeep(self.actor, choices, unused=unused)

    def upkeep(self, player, choices=None, *, unused=0):
        self.emit('advance_turn')
        self.emit('untap_step', player=player)
        if choices:
            choices()
        self.emit('upkeep_step', player=player)
        self.resolve(unused=unused)

    def cast_permanent(self, ref='source', *, target=None):
        self.cast(ref, target=target)
        self.resolve()
        self.move(ref, 'battlefield')
        self.prop(ref, 'in_play', 1)

    def cast_spell(self, ref, *, target=None, target_player=None):
        name, caster, _ = self.cards[ref]

        def choose():
            self.emit('choose_player', player=caster, value=target_player)

        self.cast(ref, target=target, setup_choice=choose if target_player is not None else None)
        self.resolve()
        self.move(ref, 'graveyard')

    def automaton(self):
        mode = self.mode
        count = 6 if mode in ('threshold', 'foe_upkeep') else 7 if mode == 'above' else 5
        controller = self.foe if mode == 'borrowed' else self.actor
        supplies = ('Plains', 'Mountain', 'Island', 'Forest', 'Darksteel Relic', 'Grizzly Bears', 'Sol Ring')
        for i, name in enumerate(supplies[:count]):
            self.add(f'supply_{i}', name, controller, 'battlefield')
        if mode == 'opponent_many':
            for i, name in enumerate(supplies):
                self.add(f'enemy_{i}', name, self.foe, 'battlefield')
        if mode == 'borrowed':
            self.add('theft', 'Control Magic', self.foe, 'hand')
        self.cast_permanent()
        self.stats('source')
        if mode == 'borrowed':
            self.emit('turn', player=self.foe)
            self.cast_permanent('theft', target='source')
            self.zones[self.actor]['battlefield'].remove('source')
            self.zones[self.foe]['battlefield'].append('source')
            self.emit('bind_permanent', 'borrowed', player=self.foe, value=self.cid, index=-1)
            self.prop('borrowed', 'in_play', 1)
            # The owner's upkeep occurs while the other seat still controls the source.
            self.upkeep(self.actor)
            self.prop('borrowed', 'in_play', 1)
            self.receipts()
            self.upkeep(self.foe)
            self.zones[self.foe]['battlefield'].remove('source')
            self.zones[self.actor]['hand'].append('source')
            self.cards['source'] = self.name, self.actor, 'hand'
            self.move('theft', 'graveyard')
            return
        if mode == 'foe_upkeep':
            self.upkeep(self.foe)
            self.prop('source', 'in_play', 1)
            return
        self.owner_turn_upkeep()
        if mode in ('threshold', 'above'):
            self.move('source', 'hand')
            self.emit('bind_hand', 'source', player=self.actor, value=self.cid, index=-1)
            self.prop('source', 'in_hand', 1)
        else:
            self.prop('source', 'in_play', 1)

    def reveal_dialogs(self, amount):
        for _ in range(amount):
            self.emit('choose_dialog', player=self.actor, value=0)

    def crusher(self):
        mode = self.mode
        if mode in ('repeat_prefix', 'only_lands'):
            lib = [('land_a', 'Mountain'), ('land_b', 'Island')]
        else:
            lib = []
        if mode not in ('only_lands', 'empty'):
            lib.extend((('stop', 'Lightning Bolt'), ('behind', 'Forest')))
        self.select_library(lib)
        if mode in ('own_land_destroyed', 'foe_land_destroyed'):
            land_owner = self.actor if mode == 'own_land_destroyed' else self.foe
            self.add('land_target', 'Swamp', land_owner, 'battlefield')
            self.add('removal', 'Stone Rain', self.foe, 'hand')
        elif mode == 'nonland_destroyed':
            self.add('nonland', 'Hill Giant', self.actor, 'battlefield')
            self.add('removal', 'Murder', self.foe, 'hand')
        self.cast_permanent()
        self.stats('source')
        self.prop('source', 'plus1_counters', 0)
        amount = 0
        if mode in ('own_land_destroyed', 'foe_land_destroyed', 'nonland_destroyed'):
            self.emit('turn', player=self.foe)
            target = 'nonland' if mode == 'nonland_destroyed' else 'land_target'
            self.cast_spell('removal', target=target)
            self.move(target, 'graveyard')
            amount = int(mode == 'own_land_destroyed')
            self.crusher_stats(amount)
            self.upkeep(self.actor, lambda: self.reveal_dialogs(1))
        else:
            reveals = {'empty': 0, 'only_lands': 2, 'repeat_prefix': 3, 'nonland_first': 1}[mode]
            self.upkeep(self.foe)
            self.crusher_stats(0)
            self.receipts()
            self.upkeep(self.actor, lambda: self.reveal_dialogs(reveals))
            if mode in ('only_lands', 'repeat_prefix'):
                self.move('land_a', 'graveyard')
                self.move('land_b', 'graveyard')
                amount += 2
        self.crusher_stats(amount)
        if mode == 'repeat_prefix':
            self.receipts()
            self.upkeep(self.foe)
            self.upkeep(self.actor, lambda: self.reveal_dialogs(1))
            self.crusher_stats(amount)

    def crusher_stats(self, amount):
        self.prop('source', 'in_play', 1)
        self.prop('source', 'plus1_counters', amount)
        self.prop('source', 'power', 3 + amount)
        self.prop('source', 'toughness', 3 + amount)

    def stings(self):
        mode = self.mode
        paid_cast = mode in ('cast_return', 'self_cast', 'repeat')
        if not paid_cast:
            setup_source = self.setup[0]
            self.setup[0] = setup_source[:2] + (ZONE['graveyard'],) + setup_source[3:]
            self.move('source', 'graveyard')
        hand_card_count = {'tie': (1, 1), 'less': (1, 2)}.get(mode, (2, 1))
        for player, count in zip((self.actor, self.foe), hand_card_count):
            for i, card in enumerate(('Forest', 'Island')[:count]):
                self.add(f'held_{player}_{i}', card, player, 'hand')
        if paid_cast:
            target = self.actor if mode == 'self_cast' else self.foe
            self.cast_spell('source', target_player=target)
            self.life[target] -= 1
            self.life[self.actor] += 1
        self.emit('prepare_graveyard_abilities')
        self.upkeep(self.foe)
        self.receipts()
        if mode == 'foe_upkeep':
            return
        eligible = mode not in ('tie', 'less')
        accept = eligible and mode != 'decline'

        def choose():
            self.emit('choose_dialog', player=self.actor, value=int(not accept))

        self.upkeep(self.actor, choose if eligible else None)
        if accept:
            self.move('source', 'hand')
        if mode != 'repeat':
            return
        self.receipts()
        self.emit('turn', player=self.actor)
        self.emit('bind_hand', 'source', player=self.actor, value=self.cid, index=-1)
        self.cast_spell('source', target_player=self.foe)
        self.life[self.foe] -= 1
        self.life[self.actor] += 1
        self.owner_turn_upkeep(choose)
        self.move('source', 'hand')

    def equipoise(self):
        mode = self.mode
        targets = []
        if mode in ('three_types', 'phase_in', 'equal', 'foe_shroud', 'foe_upkeep'):
            self.add('land', 'Island', self.foe, 'battlefield')
            targets.append('land')
        if mode in ('three_types', 'phase_in', 'equal', 'foe_upkeep'):
            self.add('artifact', 'Darksteel Relic', self.foe, 'battlefield')
            targets.append('artifact')
        if mode in ('three_types', 'phase_in', 'equal', 'indirect_aura', 'foe_upkeep'):
            self.add('creature', 'Hill Giant', self.foe, 'battlefield')
            targets.append('creature')
        if mode == 'equal':
            self.add('own_land', 'Plains', self.actor, 'battlefield')
            self.add('own_artifact', 'Darksteel Relic', self.actor, 'battlefield')
            self.add('own_creature', 'Grizzly Bears', self.actor, 'battlefield')
        if mode == 'foe_shroud':
            self.add('shroud', 'Ivory Mask', self.foe, 'battlefield')
        if mode == 'indirect_aura':
            self.add('aura', 'Holy Strength', self.foe, 'hand')
            self.emit('turn', player=self.foe)
            self.cast_permanent('aura', target='creature')
            self.emit('turn', player=self.actor)
            self.prop('creature', 'power', 4)
            self.prop('creature', 'toughness', 5)
        self.cast_permanent()
        self.upkeep(self.foe)
        for ref in targets:
            self.prop(ref, 'in_play', 1)
        self.receipts()
        if mode == 'foe_upkeep':
            return

        def choose():
            for ref in targets:
                self.emit('choose_target', ref, player=self.actor)

        phase = mode not in ('equal', 'foe_shroud')
        # If the engine bypasses a shrouded player gate, drain its real legal
        # land selection. The Oracle-correct path leaves the plan unused.
        self.upkeep(self.actor, choose if phase or mode == 'foe_shroud' else None,
                    unused=int(mode == 'foe_shroud'))
        for ref in targets:
            self.prop(ref, 'in_play', int(not phase))
        if mode == 'indirect_aura':
            self.prop('aura', 'in_play', 0)
        if mode not in ('phase_in', 'indirect_aura'):
            return
        self.receipts()
        self.emit('advance_turn')
        for ref in targets:
            self.prop(ref, 'in_play', 0)
        self.emit('untap_step', player=self.foe)
        for ref in targets:
            self.prop(ref, 'in_play', 1)
            self.prop(ref, 'tapped', 0)
        if mode == 'indirect_aura':
            self.prop('aura', 'in_play', 1)
            self.prop('creature', 'power', 4)
            self.prop('creature', 'toughness', 5)


    def setup_in_grave(self, ref='source'):
        i, = [i for i, setup in enumerate(self.setup) if setup[0] == ref]
        row = self.setup[i]
        self.setup[i] = row[:2] + (ZONE['graveyard'],) + row[3:]
        self.move(ref, 'graveyard')

    def bind_live(self, ref='source', *, seat=None, copy=None):
        player = self.cards[ref][1] if seat is None else seat
        self.emit('bind_permanent' if copy is None else 'bind_permanent_copy', ref,
                  player=player, value=self.id(ref), index=-1 if copy is None else copy)
        self.prop(ref, 'in_play', 1)

    def pick(self, ref, *, player=None, decline=False):
        self.emit('choose_optional_zone' if decline else 'choose_zone_card',
                  player=self.actor if player is None else player, value=self.id(ref))

    def swing(self, ref, damage):
        self.emit('turn', player=self.actor)
        self.emit('begin_attackers')
        self.emit('attack', ref, value=1)
        self.emit('finish_attackers')
        self.emit('begin_blockers')
        self.emit('advance_combat')
        self.prop(ref, 'blocked', 0)
        self.emit('combat_damage')
        self.resolve()
        self.emit('end_combat')
        self.life[self.foe] -= damage

    def cleanup_now(self):
        self.emit('cleanup')
        self.resolve()

    def destroy_real(self, ref, *, vanish=False):
        self.emit('destroy', ref)
        self.resolve()
        if vanish:
            name, player, zone = self.cards[ref]
            self.zones[player][zone].remove(ref)
            self.cards[ref] = name, player, 'vanished'
        else:
            self.move(ref, 'graveyard')

    def ichorid(self):
        mode = self.mode
        if mode == 'cast_haste_end':
            self.cast_permanent()
            self.stats('source')
            self.swing('source', 3)
            self.cleanup_now()
            self.move('source', 'graveyard')
            return
        self.setup_in_grave()
        self.add('own_green', 'Grizzly Bears', self.actor, 'graveyard')
        self.add('black_spell', 'Duress', self.actor, 'graveyard')
        self.add('foe_black', 'Scathe Zombies', self.foe, 'graveyard')
        supplied = mode in ('return_haste_end', 'decline')
        if supplied:
            self.add('pay', 'Walking Corpse', self.actor, 'graveyard')
        self.emit('prepare_graveyard_abilities')

        def choice():
            self.emit('choose_dialog', player=self.actor, value=int(mode == 'decline'))
            if mode == 'return_haste_end':
                self.emit('observe_search_filters', value=1)
                self.pick('pay')

        self.owner_turn_upkeep(choice if supplied else None)
        if mode != 'return_haste_end':
            return
        self.emit('observe_search_filters', value=0)
        for ref, legal in (('pay', 1), ('black_spell', 0), ('own_green', 0)):
            self.emit('assert_offered_legal_card', index=self.id(ref), value=legal)
        self.move('pay', 'exile')
        self.move('source', 'battlefield')
        self.bind_live()
        self.stats('source')
        self.swing('source', 3)
        self.cleanup_now()
        self.move('source', 'graveyard')

    def imaginary(self):
        mode = self.mode
        self.add('enemy_hand', 'Island', self.foe, 'hand')
        if mode != 'empty':
            self.add('held', 'Forest', self.actor, 'hand')
        self.cast_permanent()
        self.stats('source')
        self.upkeep(self.foe)
        self.prop('source', 'in_play', 1)
        if mode == 'wrong_upkeep':
            return
        self.upkeep(self.actor)
        if mode != 'empty':
            self.move('source', 'hand')
        else:
            self.prop('source', 'in_play', 1)

    def coils(self):
        mode = self.mode
        n = {'below': 4, 'threshold': 5, 'above': 6, 'other_death': 4, 'token_death': 4}[mode]
        for i in range(n):
            self.add(f'own_{i}', 'Hill Giant', self.actor, 'battlefield')
        if mode in ('token_death', 'other_death'):
            token = mode == 'token_death'
            self.add('foil', 'Grizzly Bears', self.actor if token else self.foe, 'battlefield', state=32 if token else 0)
        self.cast_permanent()
        self.emit('assert_counter', 'source', prop='charge', value=0)
        for i in range(n):
            self.destroy_real(f'own_{i}')
        if mode in ('token_death', 'other_death'):
            self.destroy_real('foil', vanish=mode == 'token_death')
        self.emit('assert_counter', 'source', prop='charge', value=n)
        self.owner_turn_upkeep()
        if n < 5:
            self.emit('assert_counter', 'source', prop='charge', value=n)
            return
        self.emit('assert_counter', 'source', prop='charge', value=0)
        for i in range(n):
            ref = f'token_{i}'
            self.cards[ref] = 'Elemental', self.actor, 'battlefield'
            self.zones[self.actor]['battlefield'].append(ref)
            self.bind_live(ref, copy=i)
            for key, value in (('power', 3), ('toughness', 1), ('color', 1 << 4)):
                self.prop(ref, key, value)
        self.receipts()
        self.swing('token_0', 3)
        self.cleanup_now()
        for i in range(n):
            self.zones[self.actor]['battlefield'].remove(f'token_{i}')
        self.emit('assert_counter', 'source', prop='charge', value=0)

    def bloom(self):
        mode = self.mode
        x = 0 if mode == 'zero' else 2 if mode == 'two_uses' else 1
        fee = (x, 0, 0, 1, 0, 0)
        self.mana(self.actor, fee, write=True)
        self.emit('refresh')
        self.emit('choose_x', player=self.actor, value=x)
        self.emit('cast', 'source', value=1)
        self.emit('assert_unused_choices', value=0)
        self.emit('assert_stack', value=1)
        self.mana(self.actor)
        self.resolve()
        self.move('source', 'battlefield')
        self.prop('source', 'in_play', 1)
        self.emit('assert_counter', 'source', prop='charge', value=x)
        if mode in ('one_held', 'zero'):
            self.owner_turn_upkeep()
            if x == 0:
                self.move('source', 'hand')
            else:
                self.emit('assert_counter', 'source', prop='charge', value=x)
            return
        self.bloom_use(color=2, replacement=x-1)
        self.emit('can_activate', 'source', value=0)
        if x == 2:
            self.owner_turn_upkeep()
            self.emit('turn', player=self.actor)
            self.bloom_use(color=4, replacement=0)
        self.owner_turn_upkeep()
        self.move('source', 'hand')

    def bloom_use(self, *, color, replacement):
        self.emit('can_activate', 'source', value=1)
        self.emit('choose_color', player=self.actor, value=color)
        self.emit('activate', 'source', value=1)
        self.emit('assert_stack', value=0)
        self.emit('assert_unused_choices', value=0)
        self.mana(self.actor, tuple(int(i == color) for i in range(6)))
        self.prop('source', 'tapped', 0)
        self.emit('assert_counter', 'source', prop='charge', value=replacement)
        self.mana(self.actor, write=True)

    def mark(self):
        mode = self.mode
        host = self.actor if mode == 'own_host' else self.foe
        self.add('host', 'Hill Giant', host, 'battlefield')
        self.add('other', 'Grizzly Bears', self.foe, 'battlefield')
        self.add('separate', 'Unholy Strength', self.foe, 'hand')
        layered = mode == 'foreign_auras'
        if layered:
            self.add('aura_a', 'Holy Strength', self.actor, 'hand')
            self.add('aura_b', 'Holy Strength', self.foe, 'hand')
            self.cast_permanent('aura_a', target='host')
            self.emit('turn', player=self.foe)
            self.cast_permanent('aura_b', target='host')
        else:
            self.emit('turn', player=self.foe)
        self.cast_permanent('separate', target='other')
        self.emit('turn', player=self.actor)
        self.cast_permanent(target='host')
        self.upkeep(self.foe)
        self.prop('host', 'in_play', 1)
        self.prop('source', 'in_play', 1)
        self.upkeep(self.actor)
        for ref in ('source', 'host', 'aura_a', 'aura_b') if layered else ('source', 'host'):
            self.move(ref, 'hand')
        self.prop('separate', 'in_play', 1)
        self.prop('other', 'in_play', 1)

    def mist(self):
        mode = self.mode
        for player in (self.actor, self.foe):
            self.add(f'land_{player}', 'Island', player, 'battlefield', state=1)
            self.add(f'artifact_{player}', 'Sol Ring', player, 'battlefield', state=1)
        recipients = {self.actor: [], self.foe: []}
        if mode == 'both_choose':
            self.add('actor_gy', 'Forest', self.actor, 'graveyard')
            self.add('foe_gy', 'Lightning Bolt', self.foe, 'graveyard')
            recipients[self.actor] = [f'artifact_{self.actor}']
            recipients[self.foe] = [f'land_{self.foe}']
        elif mode == 'other_target':
            self.add('actor_gy', 'Forest', self.actor, 'graveyard')
            self.add('another_gy', 'Lightning Bolt', self.actor, 'graveyard')
            recipients[self.actor] = [f'artifact_{self.foe}', f'land_{self.actor}']
        self.cast_permanent()
        untapped = set()
        for player in (self.foe, self.actor):
            self.emit('advance_turn')
            self.emit('untap_step', player=player)
            for ref in (f'land_{player}', f'artifact_{player}'):
                self.prop(ref, 'tapped', int(ref not in untapped))
            for ref in recipients[player]:
                self.emit('choose_target', ref, player=player)
            self.emit('upkeep_step', player=player)
            self.resolve()
            untapped.update(recipients[player])
            for seat in (self.actor, self.foe):
                for prefix in ('land', 'artifact'):
                    ref = f'{prefix}_{seat}'
                    self.prop(ref, 'tapped', int(ref not in untapped))

    def mortal(self):
        mode = self.mode
        own_count = 20 if mode in ('wins', 'foe_upkeep') else 19
        for i in range(own_count):
            self.add(f'g{i}', 'Hill Giant', self.actor, 'graveyard')
        self.add('noncreature', 'Lightning Bolt', self.actor, 'graveyard')
        if mode == 'foe_threshold':
            for i in range(20):
                self.add(f'foe_g{i}', 'Grizzly Bears', self.foe, 'graveyard')
        self.cast_permanent()
        self.receipts()
        self.upkeep(self.foe)
        if mode != 'foe_upkeep':
            self.receipts()
            self.upkeep(self.actor)
            if mode == 'wins':
                self.life[self.foe] = -99

    def servitor(self):
        mode = self.mode
        self.add('own_one', self.name, self.actor, 'graveyard')
        self.add('foe_one', self.name, self.foe, 'graveyard')
        if mode == 'copies':
            self.add('own_two', self.name, self.actor, 'graveyard')
            self.add('foe_two', self.name, self.foe, 'graveyard')
        self.add('own_artifact', 'Ornithopter', self.actor, 'graveyard')
        self.add('other_myr', 'Gold Myr', self.foe, 'graveyard')
        if mode == 'source_removed':
            self.add('remover', 'Disenchant', self.actor, 'hand')
        self.cast_permanent()
        self.stats('source')
        if mode == 'source_removed':
            self.cast_spell('remover', target='source')
            self.move('source', 'graveyard')
        self.upkeep(self.foe)
        self.receipts()
        if mode == 'foe_upkeep':
            return
        self.upkeep(self.actor)
        if mode != 'source_removed':
            for ref in ('own_one', 'foe_one') + (('own_two', 'foe_two') if mode == 'copies' else ()):
                self.move(ref, 'battlefield')

    def recursive(self):
        mode = self.mode
        spirit = self.cid == NETHER
        death = mode == 'actual_death'
        if not death:
            self.setup_in_grave()
        else:
            self.add('remover', 'Murder', self.actor, 'hand')
        self.add('foil_spell', 'Lightning Bolt', self.actor, 'graveyard')
        self.add('foe_foil', 'Grizzly Bears', self.foe, 'graveyard')
        if mode == 'creature_foil':
            self.add('own_foil', 'Hill Giant', self.actor, 'graveyard')
        if mode == 'second_copy':
            self.add('other_spirit', self.name, self.actor, 'graveyard')
        if death:
            self.cast_permanent()
            self.cast_spell('remover', target='source')
            self.move('source', 'graveyard')
        self.emit('prepare_graveyard_abilities')
        self.upkeep(self.foe)
        self.receipts()
        if mode == 'foe_upkeep':
            return
        eligible = mode not in ('creature_foil', 'second_copy')
        accept = eligible and mode != 'decline'

        def choose():
            self.emit('choose_dialog', player=0, value=int(not accept))

        self.upkeep(self.actor, choose if eligible and self.actor == 0 else None)
        if accept:
            self.move('source', 'battlefield' if spirit else 'hand')
            if spirit:
                self.bind_live()
                self.stats('source')

    def scales(self):
        mode = self.mode
        for player in (self.actor, self.foe):
            self.add(f'high_{player}', 'Hill Giant', player, 'battlefield')
            self.add(f'equal_{player}', 'Grizzly Bears', player, 'battlefield')
            self.add(f'low_{player}', 'Savannah Lions', player, 'battlefield')
            self.add(f'artifact_{player}', 'Sol Ring', player, 'battlefield')
            if mode != 'empty_hand':
                self.add(f'hand_a_{player}', 'Forest', player, 'hand')
                self.add(f'hand_b_{player}', 'Island', player, 'hand')
        self.cast_permanent()
        for player in (self.foe, self.actor):
            self.upkeep(player)
            self.move(f'high_{player}', 'hand')
            if mode == 'empty_hand':
                for prefix in ('equal', 'low'):
                    self.move(f'{prefix}_{player}', 'hand')
            self.prop(f'artifact_{player}', 'in_play', 1)
            self.receipts()
            if mode == 'foe_upkeep':
                break

    def set_deck(self, seat, payload):
        prior = set(self.zones[seat]['library'])
        self.setup = [row for row in self.setup if row[0] not in prior]
        for ref in prior:
            del self.cards[ref]
        self.zones[seat]['library'] = []
        for ref, name in payload:
            self.add(ref, name, seat, 'library')

    def oath(self):
        mode = self.mode
        active = self.actor if mode not in ('enemy', 'decline') else self.foe
        other = 1-active
        compare = mode != 'tie'
        shroud = mode == 'shroud'
        if shroud:
            self.add('shrouded', 'Ivory Mask', other, 'battlefield')
        druids = self.cid == DRUIDS
        if druids:
            self.add('extra_creature', 'Hill Giant', other, 'battlefield')
            if not compare:
                self.add('equal_creature', 'Grizzly Bears', active, 'battlefield')
            deck = [('first', 'Mountain'), ('second', 'Lightning Bolt')]
            if mode != 'no_creature':
                deck += [('reward', 'Cathedral Sanctifier'), ('remaining', 'Island')]
            self.set_deck(active, deck)
        else:
            self.add('choice_a', 'Hill Giant', active, 'graveyard')
            if compare:
                self.add('choice_b', 'Grizzly Bears', active, 'graveyard')
            self.add('active_spell', 'Lightning Bolt', active, 'graveyard')
            self.add('enemy_grave', 'Savannah Lions', other, 'graveyard')
        self.cast_permanent()
        if active == self.actor:
            self.upkeep(self.foe)
            self.receipts()
        accepts = mode != 'decline'
        actionable = compare and not shroud

        def choice():
            if druids:
                self.emit('choose_dialog', player=active, value=int(not accepts))
                if accepts:
                    self.emit('choose_zone', player=active, value=-1)
                    self.emit('choose_zone', player=other, value=-1)
            else:
                self.pick('choice_a', player=active, decline=not accepts)

        self.upkeep(active, choice if actionable else None)
        if not (accepts and actionable):
            return
        if druids:
            for ref in ('first', 'second'):
                self.move(ref, 'graveyard')
            if mode != 'no_creature':
                self.move('reward', 'battlefield')
                self.life[active] += 3
        else:
            self.move('choice_a', 'hand')

    def marrow(self):
        mode = self.mode
        if mode == 'empty':
            payload = ()
        elif mode in ('no_copy', 'owner_upkeep'):
            payload = (('foil', 'Forest'), ('below', 'Hill Giant'))
        else:
            payload = (('copied', 'Hill Giant'), ('below', 'Grizzly Bears'))
        self.set_deck(self.foe, payload)
        self.set_deck(self.actor, (('owner_top', 'Savannah Lions'),))
        self.cast_permanent()
        self.upkeep(self.foe, (lambda: self.reveal_dialogs(1)) if payload else None)
        if mode == 'copy':
            self.move('copied', 'graveyard')
            self.cards['source'] = 'Hill Giant', self.actor, 'battlefield'
            self.prop('source', 'csvid', self.rows['Hill Giant']['csvid'])
            self.stats('source')
        else:
            self.prop('source', 'csvid', self.cid)
        self.receipts()
        self.upkeep(self.actor)
        if mode == 'owner_upkeep':
            return
        self.upkeep(self.foe, (lambda: self.reveal_dialogs(1)) if mode == 'no_copy' else None)
        if mode == 'copy':
            self.stats('source')


    def play(self):
        self.emit('turn', player=self.actor)
        self.emit('present_ai_dialogs', value=1)
        for seat in (0, 1):
            self.mana(seat, write=True)
        handlers = {
            AUTOMATON: self.automaton, CRUSHER: self.crusher, STINGS: self.stings, EQUIPOISE: self.equipoise,
            ICHORID: self.ichorid, IMAGINARY: self.imaginary, COILS: self.coils, BLOOM: self.bloom,
            MARK: self.mark, MIST: self.mist, MORTAL: self.mortal, SERVITOR: self.servitor,
            NETHER: self.recursive, SCALES: self.scales, DRUIDS: self.oath, GHOULS: self.oath,
            MARROW: self.marrow, SQUEE: self.recursive,
        }
        handlers[self.cid]()
        self.receipts()
        self.emit('present_ai_dialogs', value=0)

SEATED = {
    'assert_life', 'assert_mana', 'assert_matching_permanents', 'assert_poison', 'choose_x', 'bind_hand', 'bind_permanent', 'bind_permanent_copy', 'choose_color', 'choose_dialog',
    'choose_number', 'choose_optional_target', 'choose_optional_zone', 'choose_player', 'choose_target',
    'choose_trigger', 'choose_zone', 'choose_zone_card', 'set_mana', 'turn', 'untap_step', 'upkeep_step',
    'assert_shuffle_count',
}
VALUES = {
    'activate', 'assert_card', 'assert_poison', 'choose_x', 'require_activation_mode', 'assert_forced_zone_choices', 'assert_forced_target_choices', 'assert_life',
    'assert_mana', 'assert_matching_permanents', 'assert_offered_legal_card', 'assert_offered_selection_required', 'assert_shuffle_count',
    'assert_stack', 'assert_unused_choices', 'assert_zone', 'assert_zone_card', 'assert_zone_multiplicity',
    'attack', 'bind_hand', 'bind_permanent', 'bind_permanent_copy', 'block', 'can_activate', 'can_target_creature', 'cast',
    'choose_color', 'choose_dialog', 'choose_number', 'choose_optional_zone', 'choose_player', 'choose_zone',
    'choose_zone_card', 'observe_search_filters', 'play_land', 'present_ai_dialogs', 'set_mana',
}
BY_INDEX = {'assert_matching_permanents', 'assert_offered_legal_card', 'assert_zone_card', 'assert_zone_multiplicity', 'bind_hand', 'bind_permanent', 'bind_permanent_copy'}
ZONE_OPS = {'assert_zone', 'assert_zone_card', 'assert_zone_multiplicity'}
GAPS = {
    EQUIPOISE: ['Production offers no player chooser for the elective self-target. Opposing shroud tests the missing target gate.'],
    NETHER: ['The automatic seat accepts the optional trigger. Decline uses the actual offered human input.'],
    SQUEE: ['The automatic seat accepts the optional trigger. Decline uses the actual offered human input.'],
    MORTAL: ['The host encodes an actual game-loss call as life -99. The fixture ends at that receipt.'],
}


def shaped_steps(rows):
    zones = tuple(ZONE)
    for op, ref, other, prop, player, value, zone, index, color in rows:
        if op == 'assert_counter':
            yield dict(op=op, ref=ref, counter_kind=prop, value=value)
            continue
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
        raise ValueError('upkeep zone source CSVID changed')
    names = {row['csvid']: name for name, row in rows.items() if row['kind'] in ('card', 'token')}
    zones = tuple(ZONE)
    cases = []
    for variant in selected:
        mode, seat = variant.rsplit('_', 1)
        scene = Scene(rows[name], int(seat), mode, rows, oracle)
        scene.play()
        setup = []
        for ref, player, zone, cid, state in scene.setup:
            card = dict(ref=ref, player=player, zone=zones[zone], name=names[cid])
            if state:
                card['state'] = [{1: 'tapped', 32: 'token'}[state]]
            setup.append(card)
        cases.append(dict(name=f'{LABEL}_{csvid}_{variant}', card=name, fixture='empty_main', seed=SEED,
                          setup=setup, steps=list(shaped_steps(scene.steps)),
                          _recipe=dict(family=FAMILY, version=2, variant=variant, params=declaration['params'],
                                       oracle_text=declaration['oracle_text'], untested_components=GAPS.get(csvid, []))))
    return cases
