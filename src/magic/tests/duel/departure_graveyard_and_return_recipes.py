"""Pinned departure triggers with real zone, cost, and lifecycle receipts."""

from collections import Counter
from functools import lru_cache
import json
from pathlib import Path

from death_counter_trigger_recipes import exact
from generate_cases import OPS
from recipes import cost_from_oracle


FAMILY = 'departure_graveyard_and_return'
LANE = 'graveyard-and-return'
LABEL = 'DepartureGraveyardAndReturnRecipe'
SEED = 3596
BARISHI, KEEPER, EPOCH, LARK = 2938, 4131, 8921, 9418
MODES = {
    BARISHI: ('mixed', 'no_creature', 'bounce', 'borrowed_dies'),
    KEEPER: ('top_creature', 'skip_noncreatures', 'no_creature', 'empty', 'decline', 'bounce'),
    EPOCH: ('hand_only', 'reanimate', 'bounce', 'suspend_first', 'suspend_three'),
    LARK: ('return_two', 'only_one_eligible', 'only_one_chosen', 'decline', 'bounce',
           'evoke', 'normal_with_evoke_mana', 'foreign_only', 'flying'),
}
ZONE = dict(hand=0, battlefield=1, library=2, graveyard=3, exile=4)
ZERO = (0,) * 6

PROFILE = Path(__file__).with_name('data') / f'{FAMILY}_wave8_profiles.json'
ORIGINAL = frozenset(MODES)
MISTMOON, MOLDGRAF, PHYTOTITAN, HULK, TOTEM, SOVEREIGN = 3011, 12026, 14153, 8261, 8729, 14845
RELENTLESS, TRAWLER, HIEROPHANT, CERBERUS, OIL, FOOLS, PHOENIX, VAMPIRE = (
    15932, 16790, 3046, 13588, 11609, 8550, 2674, 19144
)
MODES.update({
    MISTMOON: ('top_creature', 'no_creature', 'flying'),
    MOLDGRAF: ('random_two', 'only_one', 'no_creature', 'trample'),
    PHYTOTITAN: ('next_upkeep', 'grave_removed'),
    HULK: ('exact_six', 'stop_at_four', 'decline_all', 'zero_after_cap'),
    TOTEM: ('unanimated_dies', 'animated_dies', 'expired_dies', 'trample'),
    SOVEREIGN: ('top', 'bottom', 'decline', 'flying'),
    RELENTLESS: ('return_self', 'return_other', 'both', 'decline', 'menace'),
    TRAWLER: ('self_lower', 'other_lower', 'equal_only', 'enemy_artifact'),
    HIEROPHANT: ('chosen', 'no_creature', 'bounce'),
    CERBERUS: ('both_graves', 'shroud', 'blocks'),
    OIL: ('upkeep', 'aura_destroyed', 'infect'),
    FOOLS: ('own_host', 'opp_owner', 'host_bounced', 'aura_destroyed'),
    PHOENIX: ('two_deaths', 'bounce_resets', 'flying'),
    VAMPIRE: ('target', 'indestructible', 'no_target', 'lifelink'),
})
@lru_cache(maxsize=1)
def pins():
    return json.loads(PROFILE.read_bytes())


def mode_seats(cid):
    for mode in MODES[cid]:
        seats = (0,) if mode == 'decline' and cid in (KEEPER, SOVEREIGN, RELENTLESS) else (0, 1)
        for seat in seats:
            yield mode, seat


def classify(record, csvid):
    pin = pins()['sources'].get(record.get('name'))
    if (csvid not in MODES or not pin or pin['registry']['csvid'] != csvid
            or not exact(record, pin['oracle']) or record.get('layout') != 'normal'):
        raise ValueError('complete pinned departure rules required')
    mana = record['mana_cost']
    return dict(csvid=csvid, cost=cost_from_oracle(record) if mana else None)


def variants(shape):
    return [f'{mode}_{seat}' for mode, seat in mode_seats(shape['csvid'])]


def spec(declaration, record):
    pin = pins()['sources'].get(record.get('name'))
    if not pin:
        raise ValueError('pinned departure source required')
    shape = classify(record, pin['registry']['csvid'])
    chosen = declaration.get('variants')
    if (declaration.get('family') != FAMILY or type(declaration.get('version')) is not int
            or declaration['version'] != 2 or declaration.get('oracle_text') != record['oracle_text']
            or not exact(declaration.get('params'), shape)):
        raise ValueError('whole versioned departure declaration required')
    if (type(chosen) is not list or not chosen or any(type(v) is not str for v in chosen)
            or len(chosen) != len(set(chosen)) or set(chosen) - set(variants(shape))):
        raise ValueError('explicit distinct departure variants required')
    return dict(shape, family=FAMILY)


def validate_witnesses(oracle, rows):
    for group in ('sources', 'witnesses'):
        for name, pin in pins()[group].items():
            row = rows.get(name, {})
            if (not exact(oracle.get(name), pin['oracle']) or not exact(row, pin['registry'])
                    or row.get('kind') not in ('card', 'token') or row.get('handler') in (None, 'NULL', '0')
                    or group == 'sources' and (row['kind'] != 'card' or row['handler'] == 'card_dummy')):
                raise ValueError(f'changed departure identity: {name}')


class Scene:
    def __init__(self, row, actor, mode, rows, oracle):
        self.cid, self.name = row['csvid'], row['name']
        self.actor, self.foe, self.mode = actor, 1 - actor, mode
        self.rows, self.oracle = rows, oracle
        self.setup, self.steps, self.cards = [], [], {}
        self.zones = {player: {zone: [] for zone in ZONE} for player in (0, 1)}
        self.life = [20, 20]
        self.pools = [ZERO, ZERO]
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
            self.mana(player, self.pools[player])
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

    def pick_for(self, ref, player, *, decline=False):
        op = 'choose_optional_zone' if decline else 'choose_zone_card'
        self.emit(op, player=player, value=self.id(ref))

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

    def finish_creature(self):
        self.resolve()
        self.move('source', 'battlefield')
        self.stats('source')

    def murder(self, *, select=None, victim='source'):
        self.cast('removal', target=victim)
        if select:
            select()
        self.resolve()
        self.move('removal', 'graveyard')

    def barishi(self):
        mode = self.mode
        controller = self.foe if mode == 'borrowed_dies' else self.actor
        active_grave = mode != 'no_creature'
        if active_grave:
            self.add('first_grave', 'Hill Giant', controller, 'graveyard')
            self.add('second_grave', 'Wall of Swords', controller, 'graveyard')
        self.add('noncreature', 'Lightning Bolt', controller, 'graveyard')
        self.add('other_creature', 'Grizzly Bears', 1-controller, 'graveyard')
        removal = 'Unsummon' if mode == 'bounce' else 'Murder'
        self.add('removal', removal, controller, 'hand')
        if mode == 'borrowed_dies':
            self.add('borrow', 'Act of Treason', self.foe, 'hand')
        self.cast('source')
        self.finish_creature()
        victim = 'source'
        if mode == 'borrowed_dies':
            self.emit('turn', player=self.foe)
            self.cast('borrow', target='source')
            self.resolve()
            self.move('borrow', 'graveyard')
            self.zones[self.actor]['battlefield'].remove('source')
            self.zones[controller]['battlefield'].append('source')
            self.emit('bind_permanent', 'borrowed', player=controller, value=self.cid, index=-1)
            self.prop('borrowed', 'in_play', 1)
            victim = 'borrowed'
        self.murder(victim=victim)
        if mode == 'bounce':
            self.move('source', 'hand')
            return
        if mode == 'borrowed_dies':
            self.zones[controller]['battlefield'].remove('source')
            self.zones[self.actor]['battlefield'].append('source')
        self.move('source', 'exile')
        self.shuffles[controller] += 1
        if active_grave:
            for ref in ('first_grave', 'second_grave'):
                self.move(ref, 'library')

    def keeper(self):
        mode = self.mode
        top = [('first_land', 'Plains'), ('first_spell', 'Lightning Bolt')]
        release = [('reward', 'Cathedral Sanctifier'), ('next_creature', 'Hill Giant'), ('next_land', 'Island')]
        if mode == 'top_creature':
            library = release
        elif mode == 'empty':
            library = []
        elif mode == 'no_creature':
            library = top
        else:
            library = top + release
        self.select_library(library)
        self.add('grave_foil', 'Grizzly Bears', self.actor, 'graveyard')
        removal = 'Unsummon' if mode == 'bounce' else 'Murder'
        self.add('removal', removal, self.actor, 'hand')
        self.cast('source')
        self.finish_creature()
        accepts = mode not in ('bounce', 'decline')
        callback = (lambda: self.emit('choose_trigger', 'source', player=0)) if accepts and self.actor == 0 else None
        self.murder(select=callback)
        if not accepts:
            self.move('source', 'hand' if mode == 'bounce' else 'graveyard')
            return
        self.move('source', 'exile')
        if mode not in ('top_creature', 'empty'):
            self.move('first_land', 'graveyard')
            self.move('first_spell', 'graveyard')
        if mode in ('top_creature', 'skip_noncreatures'):
            self.move('reward', 'battlefield')
            self.emit('bind_permanent', 'reward', player=self.actor, value=self.id('reward'), index=-1)
            self.stats('reward')
            self.life[self.actor] += 3

    def epoch(self):
        mode = self.mode
        if mode == 'reanimate':
            row = self.setup[0]
            self.setup[0] = row[:2] + (ZONE['graveyard'],) + row[3:]
            self.move('source', 'graveyard')
            self.add('return_spell', 'Reanimate', self.actor, 'hand')
            self.cast('return_spell', setup_choice=lambda: self.pick_for('source', self.actor))
            self.resolve()
            self.move('return_spell', 'graveyard')
            self.move('source', 'battlefield')
            self.life[self.actor] -= 2
            self.emit('bind_permanent', 'source', player=self.actor, value=self.cid, index=-1)
            self.epoch_stats(3)
            return
        if mode != 'hand_only':
            self.add('removal', 'Unsummon' if mode == 'bounce' else 'Murder', self.actor, 'hand')
        self.cast('source')
        self.resolve()
        self.move('source', 'battlefield')
        self.epoch_stats(0)
        if mode == 'hand_only':
            return
        self.murder()
        if mode == 'bounce':
            self.move('source', 'hand')
            self.emit('bind_hand', 'source', player=self.actor, value=self.cid, index=-1)
            self.cast('source')
            self.resolve()
            self.move('source', 'battlefield')
            self.epoch_stats(0)
            return
        self.move('source', 'exile')
        self.receipts()
        turns = 3 if mode == 'suspend_three' else 1
        for visit in range(turns):
            self.upkeep(self.foe)
            self.receipts()
            self.upkeep(self.actor)
            if visit + 1 < 3:
                self.receipts()
        if mode != 'suspend_three':
            return
        self.move('source', 'battlefield')
        self.receipts()
        self.emit('bind_permanent', 'source', player=self.actor, value=self.cid, index=-1)
        self.epoch_stats(3)
        self.emit('turn', player=self.actor)
        self.flight('source', 4, expect_haste=True)

    def upkeep(self, player):
        self.emit('advance_turn')
        self.emit('untap_step', player=player)
        self.emit('upkeep_step', player=player)
        self.resolve()

    def epoch_stats(self, counters):
        self.prop('source', 'in_play', 1)
        self.prop('source', 'csvid', self.cid)
        self.prop('source', 'plus1_counters', counters)
        self.prop('source', 'power', 1 + counters)
        self.prop('source', 'toughness', 1 + counters)

    def flight(self, ref, amount, *, ground=None, expect_haste=False):
        if not expect_haste:
            self.next_own_main()
        self.emit('begin_attackers')
        self.emit('attack', ref, value=1)
        self.prop(ref, 'attacking', 1)
        self.emit('finish_attackers')
        self.emit('begin_blockers')
        if ground:
            self.emit('block', ground, other=ref, value=0)
        self.emit('advance_combat')
        self.prop(ref, 'blocked', 0)
        self.emit('combat_damage')
        self.resolve()
        self.emit('end_combat')
        self.life[self.foe] -= amount

    def lark(self):
        mode = self.mode
        has_first = mode not in ('foreign_only', 'flying')
        has_second = has_first and mode != 'only_one_eligible'
        if has_first:
            self.add('first', 'Savannah Lions', self.actor, 'graveyard')
        if has_second:
            self.add('second', 'Grizzly Bears', self.actor, 'graveyard')
        self.add('too_large', 'Hill Giant', self.actor, 'graveyard')
        self.add('spell_foil', 'Lightning Bolt', self.actor, 'graveyard')
        self.add('foe_low_power', 'Scathe Zombies', self.foe, 'graveyard')
        if mode == 'flying':
            self.add('ground', 'Grizzly Bears', self.foe, 'battlefield')
        if mode not in ('evoke', 'flying'):
            removal = 'Unsummon' if mode == 'bounce' else 'Murder'
            self.add('removal', removal, self.actor, 'hand')

        def choose():
            if has_first:
                self.emit('observe_search_filters', value=1)
                self.pick_for('first', self.actor, decline=mode == 'decline')
            if has_second:
                decline = mode in ('decline', 'only_one_chosen')
                self.pick_for('second', self.actor, decline=decline)

        if mode == 'evoke':
            self.cast('source', replace_cost=(5, 0, 0, 0, 0, 1), mode=1)
            choose()
            self.resolve()
            self.move('source', 'graveyard')
        else:
            self.cast('source', mode=0 if mode == 'normal_with_evoke_mana' else None)
            self.finish_creature()
            if mode == 'flying':
                self.flight('source', 4, ground='ground')
                return
            self.receipts()
            self.murder(select=choose)
            self.move('source', 'hand' if mode == 'bounce' else 'graveyard')
        if has_first:
            self.emit('observe_search_filters', value=0)
            self.emit('assert_offered_selection_required', value=0)
            for ref in ('too_large', 'spell_foil'):
                self.emit('assert_offered_legal_card', index=self.id(ref), value=0)
            last = 'second' if has_second else 'first'
            self.emit('assert_offered_legal_card', index=self.id(last), value=1)
        if mode in ('decline', 'only_one_chosen'):
            self.emit('assert_forced_zone_choices', value=0)
        if has_first and mode != 'decline':
            self.move('first', 'battlefield')
        if has_second and mode not in ('decline', 'only_one_chosen'):
            self.move('second', 'battlefield')


    def bind_live(self, ref, *, seat=None, hand=False):
        player = self.cards[ref][1] if seat is None else seat
        self.emit('bind_hand' if hand else 'bind_permanent', ref, player=player, value=self.id(ref), index=-1)

    def cast_to_board(self, *, host=None):
        self.cast('source', target=host)
        self.resolve()
        self.move('source', 'battlefield')
        self.prop('source', 'in_play', 1)

    def discharge(self, ref, *, victim=None, choice=None, unused=0):
        self.cast(ref, target=victim)
        if choice:
            choice()
        self.resolve(unused=unused)
        self.move(ref, 'graveyard')

    def ordinary_death(self, *, choice=None, unused=0):
        self.discharge('removal', victim='source', choice=choice, unused=unused)
        self.move('source', 'graveyard')

    def creature_default(self):
        self.cast('source')
        self.finish_creature()

    def add_flight_foil(self):
        self.add('ground', 'Grizzly Bears', self.foe, 'battlefield')

    def attack_receipt(self, ref, *, blocker=None, blocked=False, tap=1, starts_ready=False):
        if not starts_ready:
            self.next_own_main()
        self.emit('begin_attackers')
        self.emit('attack', ref, value=1)
        self.prop(ref, 'tapped', tap)
        self.emit('finish_attackers')
        self.emit('begin_blockers')
        if blocker:
            self.emit('block', blocker, other=ref, value=int(blocked))
        self.emit('advance_combat')
        self.prop(ref, 'blocked', int(blocked))
        self.emit('combat_damage')
        self.resolve()
        self.emit('end_combat')

    def grave_creatures(self):
        self.add('own_spell', 'Lightning Bolt', self.actor, 'graveyard')
        self.add('enemy_creature', 'Grizzly Bears', self.foe, 'graveyard')

    def return_from_grave(self, ref, destination='battlefield'):
        self.move(ref, destination)
        if destination == 'battlefield':
            self.bind_live(ref)
            self.stats(ref)

    def mistmoon(self):
        mode = self.mode
        if mode == 'flying':
            self.add_flight_foil()
        else:
            self.add('removal', 'Murder', self.actor, 'hand')
            self.grave_creatures()
            if mode == 'top_creature':
                self.add('old_creature', 'Savannah Lions', self.actor, 'graveyard')
                self.add('top_creature', 'Hill Giant', self.actor, 'graveyard')
                self.add('top_foil', 'Disenchant', self.actor, 'graveyard')
        self.creature_default()
        if mode == 'flying':
            self.flight('source', 2, ground='ground')
            return
        self.ordinary_death()
        self.move('source', 'exile')
        if mode == 'top_creature':
            self.return_from_grave('top_creature')

    def moldgraf(self):
        mode = self.mode
        if mode == 'trample':
            self.add_flight_foil()
        else:
            self.add('removal', 'Murder', self.actor, 'hand')
            self.grave_creatures()
        draws = ('first_grave', 'second_grave') if mode == 'random_two' else ('first_grave',) if mode == 'only_one' else ()
        for ref, name in zip(draws, ('Grizzly Bears', 'Hill Giant')):
            self.add(ref, name, self.actor, 'graveyard')
        self.creature_default()
        if mode == 'trample':
            self.attack_receipt('source', blocker='ground', blocked=True)
            self.life[self.foe] -= 6
            self.move('ground', 'graveyard')
            self.stats('source', damage=2)
            return
        self.ordinary_death()
        self.move('source', 'exile')
        for ref in draws:
            self.return_from_grave(ref)

    def phytotitan(self):
        mode = self.mode
        self.add('removal', 'Murder', self.actor, 'hand')
        if mode == 'grave_removed':
            self.add('reanimate', 'Reanimate', self.actor, 'hand')
        self.creature_default()
        self.ordinary_death()
        self.receipts()
        if mode == 'grave_removed':
            self.cast('reanimate', setup_choice=lambda: self.pick_for('source', self.actor))
            self.resolve()
            self.move('reanimate', 'graveyard')
            self.return_from_grave('source')
            self.life[self.actor] -= 6
        self.upkeep(self.foe)
        self.receipts()
        self.upkeep(self.actor)
        if mode != 'grave_removed':
            self.return_from_grave('source')
        self.bind_live('source')
        self.prop('source', 'tapped', int(mode != 'grave_removed'))

    def hulk(self):
        mode = self.mode
        self.add('removal', 'Murder', self.actor, 'hand')
        self.add('four', 'Hill Giant', self.actor, 'library')
        self.add('two', 'Grizzly Bears', self.actor, 'library')
        self.add('above', 'Verdant Force', self.actor, 'library')
        self.add('spell', 'Lightning Bolt', self.actor, 'library')
        if mode == 'zero_after_cap':
            self.add('zero', 'Ornithopter', self.actor, 'library')
        self.creature_default()

        def choose():
            self.emit('observe_search_filters', value=1)
            self.pick_for('four', self.actor, decline=mode == 'decline_all')
            if mode != 'decline_all':
                self.pick_for('two', self.actor, decline=mode == 'stop_at_four')
            if mode == 'zero_after_cap':
                self.pick_for('zero', self.actor)

        self.ordinary_death(choice=choose)
        self.emit('observe_search_filters', value=0)
        self.shuffles[self.actor] += 1
        self.emit('assert_forced_zone_choices', value=0)
        chosen = {'decline_all': (), 'stop_at_four': ('four',), 'exact_six': ('four', 'two'),
                  'zero_after_cap': ('four', 'two', 'zero')}[mode]
        for ref in chosen:
            self.move(ref, 'battlefield')

    def totem(self):
        mode = self.mode
        self.add('removal', 'Disenchant', self.actor, 'hand')
        if mode == 'trample':
            self.add_flight_foil()
        self.cast_to_board()
        # A sole mana option auto-selects for the human controller.
        if self.actor == 1:
            self.emit('choose_activation_mode', 'source', prop='Get mana', player=1, value=0, color=3)
        self.emit('activate', 'source', value=1)
        if self.actor == 1:
            self.emit('require_activation_mode', value=1)
        self.prop('source', 'tapped', 1)
        self.mana(self.actor, (0, 0, 0, 1, 0, 0))
        self.mana(self.actor, write=True)
        if mode == 'unanimated_dies':
            self.ordinary_death()
            return
        self.next_own_main()
        self.mana(self.actor, (2, 0, 0, 3, 0, 0), write=True)
        self.emit('refresh')
        self.emit('choose_activation_mode', 'source', prop='Animate', player=self.actor, value=1, color=3)
        self.emit('activate', 'source', value=1)
        self.emit('require_activation_mode', value=1)
        self.mana(self.actor)
        self.resolve()
        for key, value in (('power', 5), ('toughness', 3), ('color', 1 << 3)):
            self.prop('source', key, value)
        for kind in ('artifact', 'subtype_Treefolk'):
            self.emit('assert_matching_permanents', prop=kind, player=self.actor, index=self.cid, value=1)
        if mode == 'trample':
            self.attack_receipt('source', blocker='ground', blocked=True, starts_ready=True)
            self.life[self.foe] -= 3
            self.move('ground', 'graveyard')
            self.stats_without_card_type(damage=2)
            return
        if mode == 'expired_dies':
            self.emit('cleanup')
            self.resolve()
            self.prop('source', 'power', 0)
            self.emit('turn', player=self.actor)
        self.ordinary_death()
        if mode == 'animated_dies':
            self.move('source', 'hand')

    def stats_without_card_type(self, *, damage):
        self.prop('source', 'in_play', 1)
        self.prop('source', 'damage', damage)

    def sovereign(self):
        mode = self.mode
        if mode == 'flying':
            self.add_flight_foil()
            self.creature_default()
            self.flight('source', 6, ground='ground')
            return
        self.add('removal', 'Murder', self.actor, 'hand')
        self.add('second_land', 'Island', self.actor, 'library')
        self.creature_default()

        def choose():
            if self.actor == 0:
                self.emit('choose_trigger', 'source', player=0)
            self.emit('choose_dialog', player=self.actor, value=int(mode == 'bottom'))

        self.ordinary_death(choice=choose if mode != 'decline' else None)
        if mode != 'decline':
            self.move('source', 'library')
            if mode == 'top':
                lib = self.zones[self.actor]['library']
                lib.remove('source')
                lib.insert(0, 'source')

    def relentless(self):
        mode = self.mode
        if mode == 'menace':
            self.add('ground', 'Wall of Wood', self.foe, 'battlefield')
            self.creature_default()
            self.attack_receipt('source', blocker='ground')
            self.life[self.foe] -= 2
            return
        self.add('removal', 'Murder', self.foe, 'hand')
        self.add('other_zombie', 'Walking Corpse', self.actor, 'graveyard')
        self.add('wrong_type', 'Grizzly Bears', self.actor, 'graveyard')
        self.creature_default()
        self.emit('turn', player=self.foe)
        wants_self = mode in ('return_self', 'both')
        wants_other = mode in ('return_other', 'both')
        pay = mode != 'decline'
        fee = (2 if wants_other else 0, int(pay), 0, 0, 0, 0)
        self.mana(self.actor, fee, write=True)
        self.pools[self.actor] = fee

        def choose():
            if self.actor == 0:
                self.emit('choose_trigger', 'source', player=0)
            self.emit('choose_dialog', player=self.actor, value={'return_self': 0, 'return_other': 1, 'both': 2}[mode])
            if wants_other and self.actor == 0:
                self.emit('choose_x', player=0, value=2)
                self.pick_for('other_zombie', 0)

        self.ordinary_death(choice=choose if pay else None)
        if wants_self:
            self.move('source', 'hand')
        if wants_other:
            self.move('other_zombie', 'battlefield')
        self.pools[self.actor] = (0, int(mode == 'return_other'), 0, 0, 0, 0)

    def trawler(self):
        mode = self.mode
        artifact_dies = mode in ('other_lower', 'enemy_artifact')
        equal = 'Worn Powerstone'
        self.add('cyclic', equal, self.actor, 'graveyard')
        if mode != 'equal_only':
            self.add('small', 'Sol Ring', self.actor, 'graveyard')
        if artifact_dies:
            owner = self.foe if mode == 'enemy_artifact' else self.actor
            self.add('dying', 'Coalition Relic', owner, 'battlefield')
        self.add('removal', 'Disenchant' if artifact_dies else 'Murder', self.actor, 'hand')
        self.creature_default()
        eligible = mode in ('self_lower', 'other_lower')

        def choose():
            self.emit('observe_search_filters', value=1)
            self.pick_for('small', self.actor)

        victim = 'dying' if artifact_dies else 'source'
        self.discharge('removal', victim=victim, choice=choose if eligible else None)
        self.move(victim, 'graveyard')
        if eligible:
            self.emit('observe_search_filters', value=0)
            self.move('small', 'hand')

    def hierophant(self):
        mode = self.mode
        self.add('removal', 'Unsummon' if mode == 'bounce' else 'Murder', self.actor, 'hand')
        self.grave_creatures()
        if mode != 'no_creature':
            self.add('first_grave', 'Hill Giant', self.actor, 'graveyard')
            self.add('chosen', 'Savannah Lions', self.actor, 'graveyard')
        self.creature_default()
        positive = mode == 'chosen'
        self.discharge('removal', victim='source',
                       choice=(lambda: self.pick_for('chosen', self.actor)) if positive else None)
        self.move('source', 'hand' if mode == 'bounce' else 'exile')
        if positive:
            self.move('chosen', 'hand')

    def cerberus(self):
        mode = self.mode
        self.add('a', 'Hill Giant', self.actor, 'graveyard')
        self.grave_creatures()
        self.add('foe_spell', 'Disenchant', self.foe, 'graveyard')
        if mode == 'blocks':
            self.add('one', 'Wall of Wood', self.foe, 'battlefield')
        else:
            self.add('removal', 'Murder', self.actor, 'hand')
        if mode == 'shroud':
            self.add('probe', 'Reanimate', self.actor, 'hand')
        self.creature_default()
        if mode == 'blocks':
            self.attack_receipt('source', blocker='one')
            self.life[self.foe] -= 6
            return
        if mode == 'shroud':
            fee = cost_from_oracle(self.oracle['Reanimate'])
            self.mana(self.actor, fee, write=True)
            self.emit('refresh')
            # Both graves have cards; only real target prerequisites can deny the cast.
            self.emit('cast', 'probe', value=0)
            self.mana(self.actor, fee)
            self.mana(self.actor, write=True)
        self.ordinary_death()
        self.move('source', 'exile')
        self.move('a', 'hand')
        self.move('enemy_creature', 'hand')

    def oil(self):
        mode = self.mode
        self.add('host', 'Grizzly Bears', self.actor, 'battlefield')
        if mode == 'aura_destroyed':
            self.add('remove_aura', 'Disenchant', self.actor, 'hand')
        self.cast_to_board(host='host')
        if mode == 'infect':
            self.attack_receipt('host')
            self.emit('assert_poison', player=self.foe, value=2)
            return
        if mode == 'aura_destroyed':
            self.discharge('remove_aura', victim='source')
            self.move('source', 'hand')
            self.stats('host')
            return
        self.upkeep(self.foe)
        self.prop('host', 'minus1_counters', 0)
        self.upkeep(self.actor)
        self.prop('host', 'minus1_counters', 1)
        self.prop('host', 'power', 1)
        self.prop('host', 'toughness', 1)
        self.receipts()
        self.upkeep(self.foe)
        self.upkeep(self.actor)
        self.move('host', 'graveyard')
        self.move('source', 'hand')

    def fools(self):
        mode = self.mode
        owner = self.foe if mode == 'opp_owner' else self.actor
        self.add('host', 'Hill Giant', owner, 'battlefield')
        if mode == 'aura_destroyed':
            self.add('remove_aura', 'Disenchant', self.actor, 'hand')
        else:
            self.add('remove_host', 'Unsummon' if mode == 'host_bounced' else 'Murder', self.actor, 'hand')
        if mode == 'opp_owner':
            self.add('owner_probe', 'Unsummon', self.actor, 'hand')
        self.cast_to_board(host='host')
        if mode == 'aura_destroyed':
            self.discharge('remove_aura', victim='source')
            self.move('source', 'hand')
            self.stats('host')
            return
        self.discharge('remove_host', victim='host')
        self.move('source', 'hand')
        if mode == 'host_bounced':
            self.move('host', 'hand')
        elif mode == 'opp_owner':
            self.zones[owner]['battlefield'].remove('host')
            self.zones[self.actor]['battlefield'].append('host')
            self.cards['host'] = 'Hill Giant', self.actor, 'battlefield'
            self.bind_live('host')
            self.stats('host')
            self.receipts()
            self.discharge('owner_probe', victim='host')
            self.zones[self.actor]['battlefield'].remove('host')
            self.zones[owner]['hand'].append('host')
            self.cards['host'] = 'Hill Giant', owner, 'hand'
        else:
            self.bind_live('host')
            self.stats('host')

    def phoenix(self):
        mode = self.mode
        if mode == 'flying':
            self.add_flight_foil()
            self.creature_default()
            self.flight('source', 3, ground='ground')
            return
        self.add('removal', 'Murder', self.actor, 'hand')
        self.add('follow', 'Murder', self.actor, 'hand')
        if mode == 'bounce_resets':
            self.add('bounce', 'Unsummon', self.actor, 'hand')
        self.creature_default()
        self.ordinary_death()
        self.return_from_grave('source')
        self.receipts()
        if mode == 'bounce_resets':
            self.discharge('bounce', victim='source')
            self.move('source', 'hand')
            self.bind_live('source', hand=True)
            self.creature_default()
        self.discharge('follow', victim='source')
        if mode == 'two_deaths':
            self.move('source', 'exile')
        else:
            self.bind_live('source')
            self.stats('source')

    def vampire(self):
        mode = self.mode
        self.add('own_foil', 'Grizzly Bears', self.actor, 'battlefield')
        if mode == 'lifelink':
            self.creature_default()
            self.attack_receipt('source')
            self.life[self.actor] += 4
            self.life[self.foe] -= 4
            return
        target = mode in ('target', 'indestructible')
        self.add('removal', 'Murder', self.actor, 'hand')
        if target:
            self.add('foe_target', 'Darksteel Myr' if mode == 'indestructible' else 'Hill Giant',
                     self.foe, 'battlefield')
        self.creature_default()

        def choose():
            self.emit('choose_target', 'foe_target', player=self.actor)

        self.ordinary_death(choice=choose if target else None)
        if target:
            self.life[self.actor] += 4
        if mode == 'target':
            self.move('foe_target', 'graveyard')


    def play(self):
        self.emit('turn', player=self.actor)
        self.emit('present_ai_dialogs', value=1)
        for seat in (0, 1):
            self.mana(seat, write=True)
        handlers = {
            BARISHI: self.barishi, KEEPER: self.keeper, EPOCH: self.epoch, LARK: self.lark,
            MISTMOON: self.mistmoon, MOLDGRAF: self.moldgraf, PHYTOTITAN: self.phytotitan, HULK: self.hulk,
            TOTEM: self.totem, SOVEREIGN: self.sovereign, RELENTLESS: self.relentless, TRAWLER: self.trawler,
            HIEROPHANT: self.hierophant, CERBERUS: self.cerberus, OIL: self.oil, FOOLS: self.fools,
            PHOENIX: self.phoenix, VAMPIRE: self.vampire,
        }
        handlers[self.cid]()
        self.receipts()
        self.emit('present_ai_dialogs', value=0)

SEATED = {
    'assert_life', 'assert_mana', 'assert_matching_permanents', 'assert_poison', 'choose_x', 'bind_hand', 'bind_permanent', 'choose_color', 'choose_dialog',
    'choose_number', 'choose_optional_target', 'choose_optional_zone', 'choose_player', 'choose_target',
    'choose_trigger', 'choose_zone', 'choose_zone_card', 'set_mana', 'turn', 'untap_step', 'upkeep_step',
    'assert_shuffle_count',
}
VALUES = {
    'activate', 'assert_card', 'assert_poison', 'choose_x', 'require_activation_mode', 'assert_forced_zone_choices', 'assert_forced_target_choices', 'assert_life',
    'assert_mana', 'assert_matching_permanents', 'assert_offered_legal_card', 'assert_offered_selection_required', 'assert_shuffle_count',
    'assert_stack', 'assert_unused_choices', 'assert_zone', 'assert_zone_card', 'assert_zone_multiplicity',
    'attack', 'bind_hand', 'bind_permanent', 'block', 'can_activate', 'can_target_creature', 'cast',
    'choose_color', 'choose_dialog', 'choose_number', 'choose_optional_zone', 'choose_player', 'choose_zone',
    'choose_zone_card', 'observe_search_filters', 'play_land', 'present_ai_dialogs', 'set_mana',
}
BY_INDEX = {'assert_matching_permanents', 'assert_offered_legal_card', 'assert_zone_card', 'assert_zone_multiplicity', 'bind_hand', 'bind_permanent'}
ZONE_OPS = {'assert_zone', 'assert_zone_card', 'assert_zone_multiplicity'}
GAPS = {
    KEEPER: ['Manual decline is covered on the offered human branch.'],
    EPOCH: ['The Suspend legacy does not present an optional decline after the last counter. Owner-only cadence observes countdown indirectly.'],
    MOLDGRAF: ['The random cases use at most two distinct eligible cards so both exact identities are known. Selection distribution is unasserted.'],
    PHOENIX: ['The first return, second exile and bounce reset observe the death counter through its rules effect.'],
    RELENTLESS: ['Exact intended B and X branches use the real death callback. The source checks an uninitialized position on an unfunded automatic branch, which is excluded.'],
}


def shaped_steps(rows):
    zones = tuple(ZONE)
    for op, ref, other, prop, player, value, zone, index, color in rows:
        if op == 'choose_activation_mode':
            yield dict(op=op, ref=ref, player=player, mode_label=prop, fallback_color=color,
                       allow_auto_nonmana=bool(value))
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
        raise ValueError('departure source CSVID changed')
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
        cases.append(dict(name=f'{LABEL}_{csvid}_{variant}', card=name, fixture='empty_main', seed=SEED,
                          setup=setup, steps=list(shaped_steps(scene.steps)),
                          _recipe=dict(family=FAMILY, version=2, variant=variant, params=declaration['params'],
                                       oracle_text=declaration['oracle_text'], untested_components=GAPS.get(csvid, []))))
    return cases
