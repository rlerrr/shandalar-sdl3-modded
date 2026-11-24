"""Paid attacks, counters, temporary stats, and printed riders."""
from collections import Counter, defaultdict
from functools import lru_cache
import json
from pathlib import Path

from counter_condition_recurrence_recipes import exact
from recipes import card, cost_from_oracle, step

FAMILY = 'attack_follow_on_counters_and_dethrone'
PROFILE = Path(__file__).with_name('data') / 'attack_follow_on_counters_and_dethrone_wave8_profiles.json'
BRAWLER, CUTTHROAT, EMISSARY, INFILTRATOR, OGRE, ROC = 6373, 14037, 14044, 14045, 14059, 16276
ZOMBIE_A, ZOMBIE_B, FOG = 'Scathe Zombies', 'Walking Corpse', 'Fog'

BEETLE, URCHINS, REVOLUTIONARY, MARCHESA = 6633, 9521, 14033, 14043
MODES = {
    BEETLE: ('attack', 'block', 'idle', 'spent'),
    URCHINS: ('attack_then_death', 'block_then_death', 'natural_third', 'no_counter_death'),
    REVOLUTIONARY: ('tie', 'defender_ahead', 'defender_behind', 'repeat', 'block'),
    MARCHESA: ('tie_team', 'behind_team', 'return_ally', 'return_self', 'no_counter', 'exile_before_end',
               'enemy_counter', 'other_counter'),
}
ZONES = {'hand': 0, 'battlefield': 1, 'library': 2, 'graveyard': 3, 'exile': 4}
ZERO = [0] * 6
BEAR, BALOTH, KOBOLD, DRONE = 'Grizzly Bears', 'Rumbling Baloth', 'Kobolds of Kher Keep', 'Spike Drone'
BOLT, MURDER, CRYPT = 'Lightning Bolt', 'Murder', "Tormod's Crypt"
DECK = ('Island', 'Mountain', 'Plains', 'Forest')
ZONE_NAMES = {code: name for name, code in ZONES.items()}
WITNESSES = frozenset(('Fog', 'Forest', 'Grizzly Bears', 'Island', 'Kobolds of Kher Keep', 'Lightning Bolt', 'Marchesa, the Black Rose', 'Mountain', 'Murder', 'Plains', 'Rumbling Baloth', 'Scathe Zombies', 'Spike Drone', "Tormod's Crypt", 'Walking Corpse'))


MODES.update({
    BRAWLER: ('auto_two', 'enemy_only', 'auto_block', 'manual_one', 'manual_none'),
    CUTTHROAT: ('tie', 'defender_ahead', 'defender_behind', 'first_strike'),
    EMISSARY: ('tie', 'defender_ahead', 'defender_behind', 'hexproof'),
    INFILTRATOR: ('tie', 'defender_ahead', 'defender_behind', 'blocked', 'fog'),
    OGRE: ('tie', 'defender_behind', 'life_mana', 'mana_dethrone', 'short_life'),
    ROC: ('flying_unfunded', 'pay_target', 'no_target', 'decline', 'skip_target', 'foe_entry'),
})
HUMAN_CHOICES = {(BRAWLER, 'manual_one'), (BRAWLER, 'manual_none'), (ROC, 'decline'), (ROC, 'skip_target')}
GAPS = {
    BRAWLER: ('Native seat 1 automatically reveals all matching Zombies. Partial and empty choices use the human menu.',),
    ROC: ('Native seat 1 automatically accepts an affordable attack trigger. Human branches cover declining payment and omitting a legal tap target.',),
}


@lru_cache(maxsize=1)
def profiles():
    return json.loads(PROFILE.read_bytes())


def classify(record, csvid):
    if (csvid not in MODES or record.get('layout') != 'normal' or record.get('card_faces')
            or record.get('selected_face') or 'Creature' not in record['type_line']
            or 'attacks' not in record['oracle_text'].lower()):
        raise ValueError('normal whole attack-trigger creature required')
    return dict(csvid=csvid, printed_cost=record['mana_cost'])


def variants(shape):
    csvid = shape['csvid']
    return [f'{mode}_{owner}' for owner in (0, 1) for mode in MODES[csvid]
            if owner == 0 or (csvid, mode) not in HUMAN_CHOICES]


def validate_variants(shape, chosen):
    if (type(chosen) is not list or not chosen or any(type(item) is not str for item in chosen)
            or len(set(chosen)) != len(chosen) or set(chosen) - set(variants(shape))):
        raise ValueError('distinct supported attack-trigger variants required')


def spec(declaration, record):
    pin = profiles()['sources'].get(record.get('name'))
    if not pin or not exact(pin['oracle'], record):
        raise ValueError('exact pinned attack-trigger primary required')
    shape = classify(record, pin['registry']['csvid'])
    if (declaration.get('family') != FAMILY or type(declaration.get('version')) is not int
            or declaration['version'] != 2 or declaration.get('oracle_text') != record['oracle_text']
            or not exact(declaration.get('params'), shape)):
        raise ValueError('exact typed attack-trigger declaration required')
    validate_variants(shape, declaration.get('variants'))
    return dict(shape, family=FAMILY)


def validate_witnesses(oracle, rows):
    pins = profiles()
    if (set(pins) != {'sources', 'witnesses'} or set(pins['witnesses']) != WITNESSES
            or {pin['registry']['csvid'] for pin in pins['sources'].values()} != MODES.keys()):
        raise ValueError('exact attack-trigger sources and witnesses required')
    for group, entries in pins.items():
        for name, pin in entries.items():
            row = rows.get(name, {})
            if (not exact(oracle.get(name), pin['oracle']) or not exact(row, pin['registry'])
                    or row.get('kind') != 'card' or row.get('handler') in (None, '0', 'NULL')
                    or oracle[name].get('layout') != 'normal'):
                raise ValueError('attack-trigger Oracle or registry changed: ' + name)
            if group == 'sources':
                if row['handler'] == 'card_dummy':
                    raise ValueError('implemented attack-trigger primary required')
                classify(oracle[name], row['csvid'])


def expand(name, csvid, declaration, shape, chosen, rows, oracle):
    validate_variants(shape, chosen)
    validate_witnesses(oracle, rows)
    if shape['csvid'] != csvid or profiles()['sources'][name]['registry']['csvid'] != csvid:
        raise ValueError('exact attack-trigger primary CSVID required')
    cases = []
    for variant in chosen:
        mode, seat = variant.rsplit('_', 1)
        board = Board(rows[name], int(seat), mode, rows, oracle)
        board.run()
        cases.append(dict(name=f'AttackFollowOnCountersAndDethroneRecipe_{csvid}_{variant}', card=name,
                          fixture='empty_main', seed=3062, setup=board.setup, steps=board.steps,
                          _recipe=dict(family=FAMILY, version=2, variant=variant, params=declaration['params'],
                                       oracle_text=declaration['oracle_text'], untested_components=list(GAPS.get(csvid, ())))))
    return cases

class Board:
    def __init__(self, row, owner, mode, rows, oracle):
        self.id, self.name, self.owner, self.foe, self.mode = row['csvid'], row['name'], owner, 1 - owner, mode
        self.rows, self.oracle = rows, oracle
        self.setup, self.steps, self.cards = [], [], {}
        self.life, self.mana = [20, 20], [ZERO.copy(), ZERO.copy()]
        self.active = owner
        self.base = tuple(int(oracle[self.name][trait]) for trait in ('power', 'toughness'))
        self.add('source', self.name, owner, 'hand')
        for actor in (0, 1):
            for index, name in enumerate(DECK):
                self.add(f'deck_{actor}_{index}', name, actor, 'library')
        if self.id == BEETLE:
            if mode == 'block':
                self.add('enemy', KOBOLD, self.foe)
            if mode == 'idle':
                self.add('ally', BEAR, owner)
        elif self.id == URCHINS:
            if mode == 'block_then_death':
                self.add('enemy', BEAR, self.foe)
            elif mode in ('attack_then_death', 'no_counter_death'):
                self.add('removal', MURDER, self.foe, 'hand')
        elif self.id == REVOLUTIONARY:
            if mode in ('defender_ahead', 'defender_behind'):
                self.add('life_action', BOLT, owner, 'hand')
            elif mode == 'block':
                self.add('enemy', BEAR, self.foe)
        elif self.id == MARCHESA:
            if mode in ('tie_team', 'behind_team', 'return_ally', 'no_counter', 'exile_before_end'):
                self.add('ally', BEAR, owner)
            if mode in ('tie_team', 'behind_team'):
                self.add('stays_back', BALOTH, owner)
                self.add('enemy', BEAR, self.foe)
            if mode == 'behind_team':
                self.add('life_action', BOLT, owner, 'hand')
            if mode in ('return_ally', 'return_self', 'exile_before_end'):
                self.add('enemy', BALOTH, self.foe)
            if mode == 'no_counter':
                self.add('removal', MURDER, self.foe, 'hand')
            if mode == 'exile_before_end':
                self.add('crypt', CRYPT, self.foe)
            if mode in ('enemy_counter', 'other_counter'):
                enemy = mode == 'enemy_counter'
                self.add('drone', DRONE, self.foe if enemy else owner, 'hand')
                self.add('removal', MURDER, owner if enemy else self.foe, 'hand')

        self.build_follow_on()

    def add(self, ref, name, player, zone='battlefield'):
        self.setup.append(card(name, ref, player, zone))
        self.cards[ref] = dict(name=name, actor=player, zone=zone)

    def emit(self, op, ref='', other='', prop='', *, player=0, value=0, zone=0, index=0, color=0):
        if op in ('untap_step', 'upkeep_step'):
            self.steps.append(step(op, player=player))
            return
        if op == 'end_combat_step':
            self.steps.append(step(op, value=value))
            return
        if op in ('choose_trigger_source', 'choose_optional_target'):
            self.steps.append(step(op, ref=ref, player=player))
            return
        if op == 'assert_counter':
            self.steps.append(step(op, ref=ref, counter_kind=prop, value=value))
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

    def stats(self, size, ref='source'):
        for prop, value in zip(('power', 'toughness'), size):
            self.observe(ref, prop, value)

    def counter(self, count, ref='source', kind='plus1'):
        self.emit('assert_counter', ref, prop=kind, value=count)

    def choices(self):
        self.emit('assert_unused_choices')
        self.emit('assert_choices_consumed')

    def pool(self, player, amount, *, fund=False):
        for color, value in enumerate(amount):
            self.emit('set_mana' if fund else 'assert_mana', player=player, value=value, color=color)
        if fund:
            self.mana[player] = list(amount)
            self.emit('refresh')

    def settle(self):
        self.emit('resolve')
        self.emit('refresh')
        self.choices()
        self.emit('assert_stack')

    def cast(self, ref, *, target=None, target_player=None, destination='graveyard', before_resolve=None):
        actor = self.cards[ref]['actor']
        self.pool(actor, cost_from_oracle(self.oracle[self.cards[ref]['name']]), fund=True)
        if target is not None:
            self.emit('choose_target', target, player=actor)
        if target_player is not None:
            self.emit('choose_player', player=actor, value=target_player)
        self.emit('cast', ref, value=1)
        self.choices()
        self.emit('assert_stack', value=1)
        self.mana[actor] = ZERO.copy()
        self.pool(actor, ZERO)
        if before_resolve is not None:
            before_resolve()
        self.settle()
        self.cards[ref]['zone'] = destination

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

    def declare(self, attackers, *, trigger_input=None):
        self.emit('begin_attackers')
        for ref in attackers:
            self.emit('attack', ref, value=1)
        if trigger_input is not None:
            trigger_input()
        self.emit('finish_attackers')
        self.emit('refresh')
        self.choices()

    def blocks(self, pairs=()):
        self.emit('begin_blockers')
        for blocker, attacker in pairs:
            self.emit('block', blocker, attacker, value=1)
            self.observe(blocker, 'blocking_count', 1)
        self.emit('advance_combat')
        self.emit('refresh')

    def damage(self):
        self.emit('combat_damage')
        self.settle()

    def end_combat(self):
        self.emit('end_combat_step', value=0)
        self.settle()

    def to_owner_main(self):
        self.visit()
        self.visit()

    def beetle(self):
        self.counter(2)
        self.stats((2, 2))
        is_blocker, is_idle = self.mode == 'block', self.mode == 'idle'
        if is_blocker:
            self.visit()
        else:
            self.to_owner_main()
        self.declare(('enemy',) if is_blocker else ('ally',) if is_idle else ('source',))
        self.counter(2)
        self.blocks((('source', 'enemy'),) if is_blocker else ())
        self.counter(2)
        self.damage()
        self.counter(2)
        self.stats((2, 2))
        if is_blocker:
            self.cards['enemy']['zone'] = 'graveyard'
        else:
            self.life[self.foe] -= 2
        self.end_combat()
        count = 2 if is_idle else 1
        self.counter(count)
        self.stats((count, count))
        if self.mode == 'spent':
            self.to_owner_main()
            self.declare(('source',))
            self.counter(1)
            self.blocks()
            self.damage()
            self.counter(1)
            self.life[self.foe] -= 1
            self.end_combat()
            self.cards['source']['zone'] = 'graveyard'
        else:
            self.visit()
            self.counter(count)

    def draw_on_death(self, count):
        self.cards['source']['zone'] = 'graveyard'
        for index in range(count):
            self.cards[f'deck_{self.owner}_{index}']['zone'] = 'hand'
        for index, name in enumerate(DECK[count:]):
            self.emit('assert_zone_card', player=self.owner, zone=2, index=index, value=self.rows[name]['csvid'])

    def urchins(self):
        self.counter(0, kind='minus1')
        self.stats((4, 3))
        if self.mode == 'no_counter_death':
            self.cast('removal', target='source')
            self.draw_on_death(0)
            return
        if self.mode == 'block_then_death':
            self.visit()
            self.declare(('enemy',))
            self.counter(0, kind='minus1')
            self.blocks((('source', 'enemy'),))
            self.counter(1, kind='minus1')
            self.stats((3, 2))
            self.damage()
            self.end_combat()
            self.cards['enemy']['zone'] = 'graveyard'
            self.draw_on_death(1)
            return
        turns = 3 if self.mode == 'natural_third' else 1
        for count in range(1, turns+1):
            self.to_owner_main()
            self.declare(('source',))
            self.blocks()
            if count <= 2:
                self.counter(count, kind='minus1')
                self.stats((4-count, 3-count))
                self.life[self.foe] -= 4-count
            self.damage()
            self.end_combat()
        if self.mode == 'attack_then_death':
            self.cast('removal', target='source')
        self.draw_on_death(turns)

    def lower_life(self):
        target = self.owner if self.mode == 'defender_ahead' else self.foe
        self.cast('life_action', target_player=target)
        self.life[target] -= 3
        self.emit('assert_life', player=target, value=self.life[target])

    def revolutionary(self):
        self.counter(0)
        self.stats(self.base)
        if self.mode == 'block':
            self.visit()
            self.declare(('enemy',))
            self.blocks((('source', 'enemy'),))
            self.counter(0)
            self.stats(self.base)
            self.damage()
            self.end_combat()
            for ref in ('source', 'enemy'):
                self.cards[ref]['zone'] = 'graveyard'
            return
        if 'life_action' in self.cards:
            self.lower_life()
        self.to_owner_main()
        times = 2 if self.mode == 'repeat' else 1
        count = 0
        for turn in range(times):
            self.declare(('source',))
            count += int(self.life[self.foe] >= self.life[self.owner])
            self.counter(count)
            self.stats((2+count, 1+count))
            self.blocks()
            self.damage()
            self.end_combat()
            self.life[self.foe] -= 2 + count
            if turn + 1 < times:
                self.to_owner_main()
        self.visit()
        self.counter(count)

    def return_card(self, ref, entry_counters=0):
        item = self.cards.pop(ref)
        item['zone'] = 'battlefield'
        self.cards['returned'] = item
        self.emit('bind_permanent_copy', 'returned', player=self.owner, value=self.rows[item['name']]['csvid'])
        self.counter(entry_counters, 'returned')
        self.observe('returned', 'tapped', 0)

    def marchesa(self):
        mode = self.mode
        self.counter(0)
        self.stats(self.base)
        if mode in ('no_counter', 'enemy_counter', 'other_counter'):
            self.marchesa_nondeclaration()
            return
        if mode == 'behind_team':
            self.lower_life()
        self.to_owner_main()
        team = mode in ('tie_team', 'behind_team')
        attacks = ('source', 'ally') if team else ('source',) if mode == 'return_self' else ('ally',)
        self.declare(attacks)
        rewarded = int(self.life[self.foe] >= self.life[self.owner])
        for ref in attacks:
            self.counter(rewarded, ref)
        if team:
            self.counter(0, 'stays_back')
            self.counter(0, 'enemy')
            self.blocks()
            self.damage()
            self.end_combat()
            self.life[self.foe] -= 5 + 2*rewarded
            self.visit()
            for ref in attacks:
                self.counter(rewarded, ref)
            return
        lost = attacks[0]
        self.blocks((('enemy', lost),))
        self.damage()
        self.end_combat()
        self.cards[lost]['zone'] = 'graveyard'
        if lost == 'source':
            self.cards['enemy']['zone'] = 'graveyard'
        self.receipt()
        if mode == 'exile_before_end':
            self.emit('choose_player', player=self.foe, value=self.owner)
            self.emit('activate', 'crypt', value=1)
            self.choices()
            self.emit('assert_stack', value=1)
            self.settle()
            self.cards['crypt']['zone'] = 'graveyard'
            self.cards[lost]['zone'] = 'exile'
        self.visit()
        if mode != 'exile_before_end':
            self.return_card(lost)

    def marchesa_nondeclaration(self):
        mode = self.mode
        dead = 'ally' if mode == 'no_counter' else 'drone'
        if mode == 'enemy_counter':
            self.visit()
        if mode != 'no_counter':
            self.cast('drone', destination='battlefield')
        self.counter(int(mode != 'no_counter'), dead)
        self.cast('removal', target=dead)
        self.cards[dead]['zone'] = 'graveyard'
        self.receipt()
        self.visit()
        if mode == 'other_counter':
            self.return_card(dead, entry_counters=1)

    def build_follow_on(self):
        mode = self.mode
        if self.id == BRAWLER:
            if mode != 'enemy_only':
                self.add('zombie_a', ZOMBIE_A, self.owner, 'hand')
                self.add('zombie_b', ZOMBIE_B, self.owner, 'hand')
            self.add('nonmatching', BEAR, self.owner, 'hand')
            self.add('opposing_zombie', ZOMBIE_A, self.foe, 'hand')
            if mode == 'auto_block':
                self.add('enemy', KOBOLD, self.foe)
        if self.id in (CUTTHROAT, EMISSARY, INFILTRATOR, OGRE):
            if mode in ('defender_ahead', 'defender_behind', 'mana_dethrone'):
                self.add('life_action', BOLT, self.owner, 'hand')
            if mode in ('first_strike', 'blocked'):
                self.add('enemy', BEAR, self.foe)
            if mode == 'fog':
                self.add('mist', FOG, self.foe, 'hand')
            if mode == 'hexproof':
                self.add('friendly_probe', MURDER, self.owner, 'hand')
                self.add('enemy_probe', MURDER, self.foe, 'hand')
                self.add('friendly_control', BEAR, self.owner)
            if mode == 'mana_dethrone':
                self.add('victim', BALOTH, self.foe)
            if mode == 'short_life':
                for index in range(6):
                    self.add(f'life_{index}', BOLT, self.owner, 'hand')
        if self.id == ROC:
            if mode != 'no_target':
                self.add('victim', BEAR, self.foe)
            if mode in ('pay_target', 'no_target', 'decline', 'skip_target'):
                self.add('reinforcement', BEAR, self.owner, 'hand')
            if mode == 'foe_entry':
                self.add('foe_spell', BEAR, self.foe, 'hand')

    def brawler_entry(self):
        if self.owner == 0:
            manual = self.mode in ('manual_one', 'manual_none')
            self.emit('choose_dialog', player=0, value=int(manual))
            if self.mode == 'manual_one':
                self.emit('choose_zone_card', player=0, value=self.rows[ZOMBIE_A]['csvid'])
            if manual:
                self.emit('choose_zone', player=0, value=-1)
        # The legacy reveal confirmation calls show_deck for seat 0 even
        # when the choice to reveal all cards belonged to seat 1.
        if self.mode not in ('enemy_only', 'manual_none'):
            self.emit('choose_zone', player=0, value=-1)

    def brawler(self):
        count = {'enemy_only': 0, 'manual_none': 0, 'manual_one': 1}.get(self.mode, 2)
        self.counter(count)
        self.stats(tuple(stat + count for stat in self.base))
        blocking = self.mode == 'auto_block'
        if blocking:
            self.visit()
        else:
            self.to_owner_main()
        self.declare(('enemy',) if blocking else ('source',))
        if not blocking:
            self.life[self.owner] -= count
        self.emit('assert_life', player=self.owner, value=self.life[self.owner])
        self.blocks((('source', 'enemy'),) if blocking else ())
        if blocking:
            self.life[self.owner] -= count
        self.emit('assert_life', player=self.owner, value=self.life[self.owner])
        self.damage()
        if blocking:
            self.cards['enemy']['zone'] = 'graveyard'
        else:
            self.life[self.foe] -= self.base[0] + count
        self.end_combat()
        self.counter(count)

    def dethrone_rider(self):
        mode = self.mode
        self.counter(0)
        self.stats(self.base)
        if mode == 'hexproof':
            for ref, allowed in (('friendly_probe', 1), ('enemy_probe', 0)):
                self.emit('can_target_creature', ref, 'source', value=allowed)
                self.emit('can_target_creature', ref, 'friendly_control', value=1)
            return
        if mode == 'short_life':
            for index in range(6):
                self.cast(f'life_{index}', target_player=self.owner)
                self.life[self.owner] -= 3
            self.emit('can_activate', 'source', value=0)
            self.pool(self.owner, ZERO)
            return
        if mode in ('defender_ahead', 'defender_behind'):
            self.lower_life()
        if mode != 'life_mana':
            self.to_owner_main()
        if mode in ('life_mana', 'mana_dethrone'):
            self.emit('can_activate', 'source', value=1)
            self.emit('activate', 'source', value=1)
            self.choices()
            self.emit('assert_stack', value=0)
            self.life[self.owner] -= 3
            self.emit('assert_life', player=self.owner, value=self.life[self.owner])
            self.mana[self.owner][4] = 1
            self.pool(self.owner, self.mana[self.owner])
            if mode == 'life_mana':
                return
            self.emit('choose_target', 'victim', player=self.owner)
            self.emit('cast', 'life_action', value=1)
            self.choices()
            self.emit('assert_stack', value=1)
            self.mana[self.owner] = ZERO.copy()
            self.pool(self.owner, ZERO)
            self.settle()
            self.cards['life_action']['zone'] = 'graveyard'
            self.observe('victim', 'damage', 3)
        self.declare(('source',))
        count = int(self.life[self.foe] >= self.life[self.owner])
        self.counter(count)
        self.stats(tuple(stat + count for stat in self.base))
        if mode == 'fog':
            self.cast('mist')
        blocked = mode in ('first_strike', 'blocked')
        self.blocks((('enemy', 'source'),) if blocked else ())
        self.damage()
        if blocked:
            self.cards['enemy']['zone'] = 'graveyard'
            if mode == 'blocked':
                self.cards['source']['zone'] = 'graveyard'
            else:
                self.observe('source', 'damage', 0)
        elif mode != 'fog':
            self.life[self.foe] -= self.base[0] + count
        if self.id == INFILTRATOR:
            drawn = int(not blocked and mode != 'fog')
            for index in range(drawn):
                self.cards[f'deck_{self.owner}_{index}']['zone'] = 'hand'
            for index, name in enumerate(DECK[drawn:]):
                self.emit('assert_zone_card', player=self.owner, zone=ZONES['library'],
                          index=index, value=self.rows[name]['csvid'])
        self.end_combat()

    def roc(self):
        self.counter(0)
        self.stats(self.base)
        energy = 1
        self.emit('assert_energy', player=self.owner, value=energy)
        if 'reinforcement' in self.cards:
            self.cast('reinforcement', destination='battlefield')
            energy += 1
        if 'foe_spell' in self.cards:
            self.visit()
            self.cast('foe_spell', destination='battlefield')
            self.emit('assert_energy', player=self.owner, value=energy)
            self.emit('assert_energy', player=self.foe, value=0)
            self.visit()
        else:
            self.to_owner_main()
        self.emit('assert_energy', player=self.owner, value=energy)
        paid = energy == 2 and self.mode != 'decline'
        target = 'victim' in self.cards and self.mode != 'skip_target'

        def pick():
            if paid:
                if self.owner == 0:
                    self.emit('choose_trigger_source', 'source', player=self.owner)
                if 'victim' in self.cards:
                    prompt = 'choose_target' if target else 'choose_optional_target'
                    self.emit(prompt, 'victim', player=self.owner)

        self.declare(('source',), trigger_input=pick)
        energy -= 2 * paid
        self.emit('assert_energy', player=self.owner, value=energy)
        self.emit('assert_energy', player=self.foe, value=0)
        self.counter(int(paid))
        self.stats(tuple(stat + paid for stat in self.base))
        self.emit('begin_blockers')
        if 'victim' in self.cards:
            self.observe('victim', 'tapped', int(paid and target))
            if not (paid and target):
                self.emit('block', 'victim', 'source', value=0)
        self.emit('advance_combat')
        self.emit('refresh')
        self.damage()
        self.life[self.foe] -= self.base[0] + paid
        self.end_combat()

    def run(self):
        self.emit('turn', player=self.active)
        for player in (0, 1):
            self.pool(player, ZERO, fund=True)
        before_resolve = self.brawler_entry if self.id == BRAWLER else None
        self.cast('source', destination='battlefield', before_resolve=before_resolve)
        path = {BEETLE: self.beetle, URCHINS: self.urchins, REVOLUTIONARY: self.revolutionary, MARCHESA: self.marchesa}
        path.update({BRAWLER: self.brawler, CUTTHROAT: self.dethrone_rider, EMISSARY: self.dethrone_rider,
                     INFILTRATOR: self.dethrone_rider, OGRE: self.dethrone_rider, ROC: self.roc})
        path[self.id]()
        self.receipt()
