"""Paid self-regeneration and printed upkeep, typed-land, and combat riders."""
from collections import Counter, defaultdict
from functools import lru_cache
import json
from pathlib import Path

from counter_condition_recurrence_recipes import exact
from recipes import card, cost_from_oracle, step

FAMILY = 'regeneration_follow_on_self_and_riders'
PROFILE = Path(__file__).with_name('data') / 'regeneration_follow_on_self_and_riders_wave8_profiles.json'
CHAMPION, TRESSER, MORTIVORE, PYGMY, RHOX = 5831, 2315, 5621, 3539, 1132
BOUNCE, LEYLINE, DURESS, CRYPT = 'Unsummon', 'Leyline of Sanctity', 'Duress', "Tormod's Crypt"

ERON, KASHI, KATHARI, KORLASH = 1638, 7273, 10298, 8956
COMMON = ('regenerate', 'short', 'terminate', 'proactive')
MODES = {
    ERON: ('haste', 'haste_blocked'),
    KASHI: ('freeze', 'fight', 'fog'),
    KATHARI: ('cascade_accept', 'cascade_decline', 'cascade_countered', 'flight'),
    KORLASH: ('grandeur_two', 'grandeur_one', 'count_change', 'without_copy'),
}
ZONES = {'hand': 0, 'battlefield': 1, 'library': 2, 'graveyard': 3, 'exile': 4}
ZERO = [0] * 6
REGEN = {ERON: [0, 0, 0, 0, 3, 0], KASHI: [1, 0, 0, 1, 0, 0],
         KATHARI: [0, 1, 0, 0, 0, 0], KORLASH: [1, 1, 0, 0, 0, 0]}
BEAR, GIANT, WALL, AIR = 'Grizzly Bears', 'Hill Giant', 'Wall of Stone', 'Air Elemental'
MURDER, TERMINATE = 'Murder', 'Terminate'
PREY, FOG, COUNTER, RAIN = 'Prey Upon', 'Fog', 'Counterspell', 'Stone Rain'
SWAMPS = ('Swamp', 'Snow-Covered Swamp')
DECK = ('Island', 'Mountain', 'Plains', 'Forest')
PREFIX = 'RegenerationFollowOnSelfAndRidersRecipe'
ZONE_NAMES = {code: name for name, code in ZONES.items()}
WITNESSES = frozenset(('Air Elemental', 'Counterspell', 'Duress', 'Fog', 'Forest', 'Grizzly Bears', 'Hill Giant', 'Island', 'Korlash, Heir to Blackblade', 'Leyline of Sanctity', 'Mountain', 'Murder', 'Plains', 'Prey Upon', 'Snow-Covered Swamp', 'Stone Rain', 'Swamp', 'Terminate', "Tormod's Crypt", 'Unsummon', 'Wall of Stone'))


MODES.update({
    CHAMPION: ('linked_self', 'bounce'),
    TRESSER: ('one_fodder', 'zero_fodder', 'protected'),
    MORTIVORE: ('growth', 'clear_foe_grave', 'empty'),
    PYGMY: ('blocked', 'unblocked', 'defending'),
    RHOX: ('normal', 'bypass', 'unblocked'),
})
REGEN.update({CHAMPION: [0, 1, 0, 0, 0, 0], TRESSER: [0, 1, 0, 0, 0, 0],
              MORTIVORE: [0, 1, 0, 0, 0, 0], PYGMY: [0, 0, 0, 1, 0, 0], RHOX: [2, 0, 0, 1, 0, 0]})


@lru_cache(maxsize=1)
def profiles():
    return json.loads(PROFILE.read_bytes())


def classify(record, csvid):
    if (csvid not in MODES or record.get('layout') != 'normal' or record.get('card_faces')
            or record.get('selected_face') or 'Creature' not in record['type_line']
            or 'regenerate' not in record['oracle_text'].lower()):
        raise ValueError('normal whole self-regeneration creature required')
    return dict(csvid=csvid, printed_cost=record['mana_cost'])


def variants(shape):
    return [f'{mode}_{owner}' for owner in (0, 1) for mode in COMMON + MODES[shape['csvid']]]


def validate_variants(shape, chosen):
    if (type(chosen) is not list or not chosen or any(type(item) is not str for item in chosen)
            or len(set(chosen)) != len(chosen) or set(chosen) - set(variants(shape))):
        raise ValueError('distinct supported self-regeneration variants required')


def spec(declaration, record):
    pin = profiles()['sources'].get(record.get('name'))
    if not pin or not exact(pin['oracle'], record):
        raise ValueError('exact pinned self-regeneration primary required')
    shape = classify(record, pin['registry']['csvid'])
    if (declaration.get('family') != FAMILY or type(declaration.get('version')) is not int
            or declaration['version'] != 2 or declaration.get('oracle_text') != record['oracle_text']
            or not exact(declaration.get('params'), shape)):
        raise ValueError('exact typed self-regeneration declaration required')
    validate_variants(shape, declaration.get('variants'))
    return dict(shape, family=FAMILY)


def validate_witnesses(oracle, rows):
    pins = profiles()
    if (set(pins) != {'sources', 'witnesses'} or set(pins['witnesses']) != WITNESSES
            or {pin['registry']['csvid'] for pin in pins['sources'].values()} != MODES.keys()):
        raise ValueError('exact regeneration sources and witnesses required')
    for group, entries in pins.items():
        for name, pin in entries.items():
            row = rows.get(name, {})
            if (not exact(oracle.get(name), pin['oracle']) or not exact(row, pin['registry'])
                    or row.get('kind') != 'card' or row.get('handler') in (None, '0', 'NULL')
                    or oracle[name].get('layout') != 'normal'):
                raise ValueError('regeneration Oracle or registry changed: ' + name)
            if group == 'sources':
                if row['handler'] == 'card_dummy':
                    raise ValueError('implemented regeneration primary required')
                classify(oracle[name], row['csvid'])


def expand(name, csvid, declaration, shape, chosen, rows, oracle):
    validate_variants(shape, chosen)
    validate_witnesses(oracle, rows)
    if shape['csvid'] != csvid or profiles()['sources'][name]['registry']['csvid'] != csvid:
        raise ValueError('exact self-regeneration primary CSVID required')
    cases = []
    for variant in chosen:
        mode, seat = variant.rsplit('_', 1)
        board = Board(rows[name], int(seat), mode, rows, oracle)
        board.run()
        cases.append(dict(name=f'RegenerationFollowOnSelfAndRidersRecipe_{csvid}_{variant}', card=name,
                          fixture='empty_main', seed=2964, setup=board.setup, steps=board.steps,
                          _recipe=dict(family=FAMILY, version=2, variant=variant, params=declaration['params'],
                                       oracle_text=declaration['oracle_text'])))
    return cases

class Board:
    def __init__(self, row, owner, mode, rows, oracle):
        self.id, self.name, self.owner, self.foe, self.mode = row['csvid'], row['name'], owner, 1 - owner, mode
        self.rows, self.oracle = rows, oracle
        self.setup, self.steps, self.cards = [], [], {}
        self.life, self.mana = [20, 20], [ZERO.copy(), ZERO.copy()]
        self.active = owner
        self.add('source', self.name, owner, 'hand')
        self.top_order = {}
        deck = DECK
        if self.id == KATHARI and mode.startswith('cascade_'):
            deck = ('Swamp', GIANT, BEAR, *DECK)
            self.top_order[owner] = DECK
            if mode == 'cascade_countered':
                self.add('counter', COUNTER, self.foe, 'hand')
        if self.id == KORLASH:
            self.add('mine', 'Swamp', self.owner, tapped=True)
            self.add('theirs', 'Swamp', self.foe, tapped=True)
            if mode.startswith('grandeur_'):
                self.add('duplicate', self.name, self.owner, 'hand')
                deck = (*DECK, *SWAMPS)
            elif mode == 'without_copy':
                self.add('different', BEAR, self.owner, 'hand')
            elif mode == 'count_change':
                self.add('my_new_swamp', 'Snow-Covered Swamp', self.owner, 'hand')
                self.add('land_removal', RAIN, self.foe, 'hand')
        for actor in (0, 1):
            for index, name in enumerate(deck if actor == self.owner else DECK):
                self.add(f'deck_{actor}_{index}', name, actor, 'library')
        if mode in ('regenerate', 'short', 'terminate'):
            self.add('removal', TERMINATE if mode == 'terminate' else MURDER, self.foe, 'hand')
        if mode in ('freeze', 'fight', 'fog'):
            self.add('victim', WALL, self.foe)
            self.add('bystander', WALL, self.foe)
            if mode == 'fight':
                self.add('other_spell', PREY, self.owner, 'hand')
            if mode == 'fog':
                self.add('other_spell', FOG, self.owner, 'hand')
        if mode in ('haste_blocked', 'flight'):
            self.add('ground', GIANT, self.foe)
        if mode == 'flight':
            self.add('flyer', AIR, self.foe)

        self.build_follow_on()

    def add(self, ref, name, player, zone='battlefield', *, tapped=False):
        entry = card(name, ref, player, zone)
        if tapped:
            entry['state'] = ['tapped']
        self.setup.append(entry)
        self.cards[ref] = dict(name=name, actor=player, zone=zone)

    def emit(self, op, ref='', other='', prop='', *, player=0, value=0, zone=0, index=0, color=0):
        if op in ('untap_step', 'upkeep_step'):
            self.steps.append(step(op, player=player))
            return
        if op in ('play_land', 'cast_response'):
            self.steps.append(step(op, ref=ref, value=value))
            return
        if op == 'assert_shuffle_count':
            self.steps.append(step(op, player=player, value=value))
            return
        fields = dict(player=player, value=value, index=index, color=color)
        if ref:
            fields['ref'] = ref
        if other:
            fields['other'] = other
        if prop:
            fields['property'] = prop
        if op.startswith('assert_zone'):
            fields['zone'] = ZONE_NAMES[zone]
        self.steps.append(step(op, **fields))

    def observe(self, ref, prop, value):
        self.emit('assert_card', ref, prop=prop, value=value)

    def choices(self, unused=0):
        self.emit('assert_unused_choices', value=unused)
        self.emit('assert_choices_consumed')

    def pool(self, player, amount, *, fund=False):
        for color, value in enumerate(amount):
            self.emit('set_mana' if fund else 'assert_mana', player=player, value=value, color=color)
        if fund:
            self.mana[player] = list(amount)
            self.emit('refresh')

    def cast(self, ref, *, targets=(), response=False):
        item = self.cards[ref]
        actor = item['actor']
        self.pool(actor, cost_from_oracle(self.oracle[item['name']]), fund=True)
        for target in targets:
            self.emit('choose_target', target, player=actor)
        self.emit('cast_response' if response else 'cast', ref, value=1)
        self.choices()
        self.emit('assert_stack', value=2 if response else 1)
        self.mana[actor] = ZERO.copy()
        self.pool(actor, ZERO)

    def settle(self, *, unused=0, response=False, stack=0):
        self.emit('resolve_response' if response else 'resolve')
        self.emit('refresh')
        self.choices(unused)
        self.emit('assert_stack', value=stack)

    def receipt(self):
        self.emit('assert_stack')
        for actor in (0, 1):
            self.emit('assert_life', player=actor, value=self.life[actor])
            self.pool(actor, self.mana[actor])
            counts = defaultdict(Counter)
            for item in self.cards.values():
                if item['actor'] == actor:
                    counts[item['zone']][item['name']] += 1
            for zone, code in ZONES.items():
                self.emit('assert_zone', player=actor, zone=code, value=counts[zone].total())
                for name, count in sorted(counts[zone].items()):
                    self.emit('assert_zone_multiplicity', player=actor, zone=code, value=count, index=self.rows[name]['csvid'])
            for i, name in enumerate(self.top_order.get(actor, ())):
                self.emit('assert_zone_card', player=actor, zone=2, value=self.rows[name]['csvid'], index=i)
        for ref, item in self.cards.items():
            if item['zone'] == 'battlefield':
                self.observe(ref, 'in_play', 1)

    def visit(self):
        assert self.mana == [ZERO, ZERO]
        self.active = 1-self.active
        self.emit('advance_turn')
        self.emit('untap_step', player=self.active)
        self.emit('upkeep_step', player=self.active)
        self.choices()
        self.emit('assert_stack')
        self.emit('turn', player=self.active)

    def regeneration(self):
        fee = REGEN[self.id].copy()
        if self.mode == 'short':
            fee[next(i for i, value in enumerate(fee) if value)] -= 1
        self.pool(self.owner, fee, fund=True)
        if self.mode == 'proactive':
            self.emit('can_activate', 'source', value=1)
            return
        self.cast('removal', targets=('source',))
        self.emit('choose_regeneration', 'source')
        saved = self.mode == 'regenerate'
        self.settle(unused=int(not saved))
        self.cards['removal']['zone'] = 'graveyard'
        if saved:
            self.mana[self.owner] = ZERO.copy()
            self.observe('source', 'tapped', 1)
            self.observe('source', 'damage', 0)
            self.observe('source', 'attacking', 0)
        else:
            self.cards['source']['zone'] = 'graveyard'
            if self.id == CHAMPION:
                self.return_champion_life()

    def declare_combat(self, blocks):
        self.emit('begin_attackers')
        self.emit('attack', 'source', value=1)
        self.emit('finish_attackers')
        self.emit('begin_blockers')
        for blocker, legal in blocks:
            self.emit('block', blocker, 'source', value=legal)
        self.emit('advance_combat')

    def deal_combat_damage(self):
        self.emit('combat_damage')
        self.settle()
        self.emit('end_combat')

    def eron(self):
        blocked = self.mode == 'haste_blocked'
        self.declare_combat((('ground', 1),) if blocked else ())
        self.deal_combat_damage()
        if blocked:
            self.cards['source']['zone'] = 'graveyard'
            self.cards['ground']['zone'] = 'graveyard'
        else:
            self.life[self.foe] -= 5

    def kashi(self):
        if self.mode == 'fight':
            self.cast('other_spell', targets=('source', 'victim'))
            self.settle()
            self.cards['other_spell']['zone'] = 'graveyard'
            self.observe('victim', 'damage', 3)
            self.observe('victim', 'tapped', 0)
            self.visit()
            self.observe('victim', 'tapped', 0)
            return
        self.visit()
        self.visit()
        self.declare_combat((('victim', 1),))
        prevention = self.mode == 'fog'
        if prevention:
            self.cast('other_spell')
            self.settle()
            self.cards['other_spell']['zone'] = 'graveyard'
        self.deal_combat_damage()
        self.observe('victim', 'damage', 0 if prevention else 3)
        self.observe('victim', 'tapped', int(not prevention))
        self.observe('bystander', 'tapped', 0)
        self.visit()
        self.observe('victim', 'damage', 0)
        self.observe('victim', 'tapped', int(not prevention))
        self.visit()
        self.observe('victim', 'tapped', int(not prevention))
        self.visit()
        self.observe('victim', 'tapped', 0)

    def kathari_cast(self):
        cascade = self.mode.startswith('cascade_')
        accepts = cascade and self.mode != 'cascade_decline'
        if cascade:
            self.emit('choose_dialog', player=self.owner, value=int(not accepts))
        self.cast('source')
        if accepts:
            ref = f'deck_{self.owner}_2'
            self.cards[ref]['zone'] = 'battlefield'
            self.emit('bind_permanent', ref, player=self.owner, value=self.rows[BEAR]['csvid'], index=-1)
            self.observe(ref, 'in_play', 1)
            self.observe(ref, 'power', 2)
        if self.mode == 'cascade_countered':
            self.emit('announce_spell', 'source')
            self.cast('counter', response=True)
            self.settle(response=True)
            for ref in ('source', 'counter'):
                self.cards[ref]['zone'] = 'graveyard'
            return False
        self.settle()
        return True

    def kathari_flight(self):
        self.visit()
        self.visit()
        self.declare_combat((('ground', 0), ('flyer', 1)))
        self.deal_combat_damage()
        self.cards['source']['zone'] = 'graveyard'

    def korlash(self):
        if self.mode == 'without_copy':
            self.emit('can_activate', 'source', value=0)
            return
        if self.mode == 'count_change':
            self.emit('play_land', 'my_new_swamp', value=1)
            self.choices()
            self.emit('refresh')
            self.cards['my_new_swamp']['zone'] = 'battlefield'
            self.observe('source', 'power', 2)
            self.observe('source', 'toughness', 2)
            self.visit()
            self.cast('land_removal', targets=('my_new_swamp',))
            self.settle()
            for ref in ('land_removal', 'my_new_swamp'):
                self.cards[ref]['zone'] = 'graveyard'
            self.observe('source', 'power', 1)
            self.observe('source', 'toughness', 1)
            return

        self.emit('can_activate', 'source', value=1)
        self.emit('activate', 'source', value=1)
        self.choices()
        self.emit('assert_stack', value=1)
        self.cards['duplicate']['zone'] = 'graveyard'
        self.emit('assert_zone_multiplicity', player=self.owner, zone=3, index=self.id, value=1)
        self.observe('source', 'in_play', 1)
        seen = SWAMPS[:2 if self.mode == 'grandeur_two' else 1]
        for name in seen:
            self.emit('choose_zone_card', player=self.owner, value=self.rows[name]['csvid'])
        if self.mode == 'grandeur_one':
            self.emit('choose_zone', player=self.owner, value=-1)
        self.settle()
        self.emit('assert_shuffle_count', player=self.owner, value=1)
        for name in seen:
            ref = next(ref for ref, item in self.cards.items()
                       if item == dict(name=name, actor=self.owner, zone='library'))
            self.cards[ref]['zone'] = 'battlefield'
            self.emit('bind_permanent_copy', ref, player=self.owner, value=self.rows[name]['csvid'], index=int(name == 'Swamp'))
            self.observe(ref, 'tapped', 1)
        self.observe('source', 'power', len(seen) + 1)
        self.observe('source', 'toughness', len(seen) + 1)
        self.emit('can_activate', 'source', value=0)

    def build_follow_on(self):
        if self.id == CHAMPION and self.mode in ('linked_self', 'bounce'):
            self.add('bounce', BOUNCE, self.foe, 'hand')
        if self.id == TRESSER:
            amount = {'one_fodder': 1, 'zero_fodder': 0}.get(self.mode, 2)
            for index in range(amount):
                self.add(f'fodder_{index}', BEAR, self.owner)
            if self.mode == 'protected':
                self.add('leyline', LEYLINE, self.foe)
                self.add('duress', DURESS, self.owner, 'hand')
        if self.id == MORTIVORE:
            self.add('grave_land', 'Swamp', self.owner, 'graveyard')
            self.add('grave_noncreature', 'Island', self.foe, 'graveyard')
            if self.mode != 'empty':
                self.add('own_grave_creature', BEAR, self.owner, 'graveyard')
                self.add('foe_grave_creature', GIANT, self.foe, 'graveyard')
            if self.mode == 'growth':
                for label, actor, name in (('own', self.owner, BEAR), ('enemy', self.foe, GIANT)):
                    self.add(label + '_victim', name, actor)
                    self.add(label + '_murder', MURDER, self.owner, 'hand')
            if self.mode == 'clear_foe_grave':
                self.add('crypt', CRYPT, self.owner)
        if self.id == PYGMY:
            if self.mode == 'blocked':
                self.add('blocking_wall', WALL, self.foe)
            if self.mode == 'defending':
                self.add('incoming', BEAR, self.foe)
        if self.id == RHOX and self.mode in ('normal', 'bypass'):
            self.add('blocking_giant', GIANT, self.foe)

    def source_power(self, value):
        self.observe('source', 'power', value)
        self.observe('source', 'toughness', value)

    def load_champion(self):
        self.linked_player = self.owner if self.mode == 'linked_self' else self.foe
        self.cast('source')
        self.emit('choose_player', player=self.owner, value=self.linked_player)
        self.settle()
        self.life[self.linked_player] -= 6
        self.emit('assert_life', player=self.linked_player, value=self.life[self.linked_player])
        self.emit('assert_life', player=1-self.linked_player, value=20)

    def return_champion_life(self):
        self.life[self.linked_player] += 6

    def champion(self):
        self.cast('bounce', targets=('source',))
        self.settle()
        self.cards['bounce']['zone'] = 'graveyard'
        self.cards['source']['zone'] = 'hand'
        self.return_champion_life()

    def load_tresser(self):
        self.cast('source')
        actors = tuple(ref for ref in self.cards if ref.startswith('fodder_'))
        sacrifices = actors if len(actors) == 2 else (*actors, 'source')
        scripted = sacrifices if self.owner == 0 and len(sacrifices) == 2 else ()
        for ref in scripted:
            self.emit('choose_target', ref, player=self.owner)
        if self.mode == 'protected':
            # Correct targeting has no legal opponent. Drain any sentinel input.
            self.settle(unused=len(scripted))
            self.pool(self.owner, [0, 1, 0, 0, 0, 0], fund=True)
            self.emit('can_cast', 'duress', value=0)
            return True
        self.settle()
        self.life[self.owner] -= 2
        for ref in sacrifices:
            self.cards[ref]['zone'] = 'graveyard'
        for index, name in enumerate(DECK[:2]):
            ref = f'deck_{self.foe}_{index}'
            assert self.cards[ref]['name'] == name
            self.cards[ref]['zone'] = 'hand'
        return 'source' not in sacrifices

    def mortivore(self):
        if self.mode == 'growth':
            for count, label in enumerate(('own', 'enemy'), 3):
                self.cast(label + '_murder', targets=(label + '_victim',))
                self.settle()
                for ref in (label + '_murder', label + '_victim'):
                    self.cards[ref]['zone'] = 'graveyard'
                self.source_power(count)
        elif self.mode == 'clear_foe_grave':
            self.emit('choose_player', player=self.owner, value=self.foe)
            self.emit('activate', 'crypt', value=1)
            self.choices()
            self.emit('assert_stack', value=1)
            self.settle()
            self.cards['crypt']['zone'] = 'graveyard'
            for ref in ('foe_grave_creature', 'grave_noncreature'):
                self.cards[ref]['zone'] = 'exile'
            self.source_power(1)

    def pygmy(self):
        self.visit()
        if self.mode == 'defending':
            self.emit('begin_attackers')
            self.emit('attack', 'incoming', value=1)
            self.emit('finish_attackers')
            self.emit('begin_blockers')
            self.emit('block', 'source', 'incoming', value=1)
            self.emit('advance_combat')
            self.source_power(1)
            self.deal_combat_damage()
            self.cards['source']['zone'] = 'graveyard'
            self.observe('incoming', 'damage', 1)
            return
        self.visit()
        blocked = self.mode == 'blocked'
        self.declare_combat((('blocking_wall', 1),) if blocked else ())
        self.source_power(2 if blocked else 1)
        self.deal_combat_damage()
        if blocked:
            self.observe('blocking_wall', 'damage', 2)
        else:
            self.life[self.foe] -= 1
        self.visit()
        self.source_power(1)

    def rhox(self):
        self.visit()
        self.visit()
        mode = self.mode
        blocked = mode != 'unblocked'
        self.emit('present_ai_dialogs', value=1)
        self.emit('begin_attackers')
        self.emit('attack', 'source', value=1)
        self.emit('finish_attackers')
        self.emit('begin_blockers')
        if blocked:
            self.emit('block', 'blocking_giant', 'source', value=1)
            self.emit('choose_dialog', player=self.owner, value=int(mode == 'bypass'))
        self.emit('advance_combat')
        self.choices()
        self.emit('present_ai_dialogs', value=0)
        self.deal_combat_damage()
        if blocked:
            self.observe('source', 'damage', 3)
        if mode == 'normal':
            self.cards['blocking_giant']['zone'] = 'graveyard'
        else:
            if blocked:
                self.observe('blocking_giant', 'damage', 0)
            self.life[self.foe] -= 5

    def run(self):
        self.emit('turn', player=self.active)
        for player in (0, 1):
            self.pool(player, ZERO, fund=True)
        if self.id == KATHARI:
            if not self.kathari_cast():
                self.receipt()
                return
        elif self.id == CHAMPION:
            self.load_champion()
        elif self.id == TRESSER:
            if not self.load_tresser():
                self.receipt()
                return
        else:
            self.cast('source')
            self.settle()
        if self.id == MORTIVORE and self.mode == 'empty':
            self.cards['source']['zone'] = 'graveyard'
            self.observe('source', 'in_play', 0)
            self.receipt()
            return
        self.cards['source']['zone'] = 'battlefield'
        for prop in ('power', 'toughness'):
            if self.id == KORLASH:
                value = 1
            elif self.id == MORTIVORE:
                value = 2
            else:
                value = int(self.oracle[self.name][prop])
            self.observe('source', prop, value)
        if self.mode in COMMON:
            self.regeneration()
        elif self.id == ERON:
            self.eron()
        elif self.id == KASHI:
            self.kashi()
        elif self.id == KATHARI and self.mode == 'flight':
            self.kathari_flight()
        elif self.id == KORLASH:
            self.korlash()
        elif self.id == CHAMPION:
            self.champion()
        elif self.id == MORTIVORE:
            self.mortivore()
        elif self.id == PYGMY:
            self.pygmy()
        elif self.id == RHOX:
            self.rhox()
        self.receipt()
