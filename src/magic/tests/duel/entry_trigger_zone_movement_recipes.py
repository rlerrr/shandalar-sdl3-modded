"""Oracle-pinned entry triggers with actual zone movement and printed riders."""

from collections import Counter
from functools import lru_cache
import json
from pathlib import Path

from death_counter_trigger_recipes import exact
from recipes import cost_from_oracle

FAMILY = 'entry_trigger_zone_movement'
PROFILE = Path(__file__).with_name('data') / f'{FAMILY}_wave8_profiles.json'
LABEL = 'EntryTriggerZoneMovementRecipe'
SEED = 3152
LIGHT, BOG, SWEEP, HORROR = 10529, 10732, 7071, 12928
MODES = {
    LIGHT: ('black_artifact', 'own_black', 'no_black', 'borrowed', 'protection'),
    BOG: ('opponent_grave', 'own_grave', 'empty_target', 'tapped_black_mana'),
    SWEEP: ('two', 'one', 'zero', 'self', 'borrowed', 'flying', 'countered'),
    HORROR: ('land', 'artifact', 'borrowed', 'own_target', 'same_name_hand', 'countered'),
}
ZONE = dict(hand=0, battlefield=1, library=2, graveyard=3, exile=4)
ZERO = (0,) * 6
BLACK = (0, 1, 0, 0, 0, 0)


ORIGINAL = frozenset(MODES)
ASHEN, SIBSIG, RAMPAGER, SEDRAXIS, HEDRON, CORPSE = 13381, 14627, 16713, 10203, 15262, 11230
ANGEL, COLLECTOR, DEADWOOD, CHAINED, TIDECALLER, LIFEBANE = 12303, 13221, 8765, 13406, 15345, 13308
MODES.update({
    ASHEN: ('entry_and_death', 'own_target', 'flying'),
    SIBSIG: ('delve_return', 'ordinary_return', 'foreign_grave'),
    RAMPAGER: ('zero', 'one', 'pay_two', 'opponent_energy'),
    SEDRAXIS: ('own_blue', 'opponent_blue', 'land_foil'),
    HEDRON: ('power_boundary', 'return_owner', 'no_match'),
    CORPSE: ('return_infect', 'foreign_infect', 'poison', 'infect_blocked'),
    ANGEL: ('mixed_tribes', 'return_humans', 'exile_zombies', 'flying'),
    COLLECTOR: ('instant', 'sorcery', 'empty'),
    DEADWOOD: ('bounce', 'vanish', 'empty'),
    CHAINED: ('return_creature', 'host_destroyed', 'no_enemy_creature', 'wrong_mountain'),
    TIDECALLER: ('awaken', 'wrong_grave', 'own_land_flying', 'opponent_land_ground'),
    LIFEBANE: ('green', 'white', 'empty', 'intimidate'),
})


@lru_cache(maxsize=1)
def pins():
    return json.loads(PROFILE.read_bytes())


def classify(record, csvid):
    pin = pins()['sources'].get(record.get('name'))
    if (csvid not in MODES or not pin or pin['registry']['csvid'] != csvid
            or not exact(record, pin['oracle']) or record.get('layout') != 'normal'):
        raise ValueError('complete pinned zone-movement rules required')
    return dict(csvid=csvid, cost=cost_from_oracle(record) if record['mana_cost'] else None)


def variants(shape):
    return [f'{mode}_{seat}' for mode in MODES[shape['csvid']] for seat in (0, 1)]


def spec(declaration, record):
    pin = pins()['sources'].get(record.get('name'))
    if not pin:
        raise ValueError('pinned zone-movement source required')
    shape = classify(record, pin['registry']['csvid'])
    chosen = declaration.get('variants')
    if (declaration.get('family') != FAMILY or type(declaration.get('version')) is not int
            or declaration['version'] != 2 or declaration.get('oracle_text') != record['oracle_text']
            or not exact(declaration.get('params'), shape)):
        raise ValueError('whole versioned zone-movement declaration required')
    if (type(chosen) is not list or not chosen or any(type(v) is not str for v in chosen)
            or len(chosen) != len(set(chosen)) or set(chosen) - set(variants(shape))):
        raise ValueError('explicit distinct zone-movement variants required')
    return dict(shape, family=FAMILY)


def validate_witnesses(oracle, rows):
    for group in ('sources', 'witnesses'):
        for name, pin in pins()[group].items():
            row = rows.get(name, {})
            if (not exact(oracle.get(name), pin['oracle']) or not exact(row, pin['registry'])
                    or row.get('kind') != 'card' or row.get('handler') in (None, 'NULL', '0')
                    or group == 'sources' and row['handler'] == 'card_dummy'):
                raise ValueError(f'changed zone-movement fixture identity: {name}')


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
        if mode == 'countered':
            self.add('counter', 'Counterspell', self.foe, 'hand')

    def emit(self, op, ref='', *, other='', prop='', player=0, value=0, zone='hand', index=0, color=0):
        assert player in (0, 1)
        self.steps.append((op, ref, other, prop, player, value, ZONE[zone], index, color))

    def add(self, ref, name, seat, zone):
        assert ref not in self.cards
        self.setup.append((ref, seat, ZONE[zone], self.rows[name]['csvid'], 0))
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

    def begin_cast(self, ref='source', *, target=None, mana=None, choices=None):
        name, player, zone = self.cards[ref]
        assert zone == 'hand' and player == self.actor
        self.mana(player, cost_from_oracle(self.oracle[name]) if mana is None else mana, write=True)
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

    def resolve(self):
        self.emit('resolve')
        self.emit('assert_unused_choices', value=0)
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

    def combat(self, illegal_blocker, legal_blocker=None, *, attacker='source', advance=True):
        if advance:
            self.next_own_main()
        self.prop(attacker, 'sick', 0)
        self.emit('begin_attackers')
        self.emit('attack', attacker, value=1)
        self.prop(attacker, 'attacking', 1)
        self.emit('finish_attackers')
        self.emit('begin_blockers')
        if illegal_blocker:
            self.emit('block', illegal_blocker, other=attacker, value=0)
        if legal_blocker:
            self.emit('block', legal_blocker, other=attacker, value=1)
        self.emit('advance_combat')
        self.prop(attacker, 'blocked', int(bool(legal_blocker)))
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

    def lightcaster(self):
        mode = self.mode
        black = "Executioner's Capsule" if mode in ('black_artifact', 'protection') else 'Walking Corpse'
        if mode != 'no_black':
            self.add('target', black, self.actor if mode == 'own_black' else self.foe, 'battlefield')
        self.add('green_foil', 'Grizzly Bears', self.foe, 'battlefield')
        self.add('colorless_foil', 'Darksteel Relic', self.actor, 'battlefield')
        if mode == 'protection':
            self.add('black_blocker', 'Walking Corpse', self.foe, 'battlefield')
        target = self.borrow('target') if mode == 'borrowed' else 'target'
        self.begin_cast()
        if mode != 'no_black':
            self.choose_target(target)
        self.resolve()
        self.move('source', 'battlefield')
        self.stats('source')
        if mode != 'no_black':
            self.move('target', 'exile', seat=self.actor if mode == 'own_black' else self.foe)
        if mode == 'protection':
            self.combat('black_blocker')
            self.life[self.foe] -= int(self.oracle[self.name]['power'])
            self.stats('source', damage=0)

    def bog(self):
        target = self.actor if self.mode in ('own_grave', 'tapped_black_mana') else self.foe
        for p in (0, 1):
            self.add(f'prior_exile_{p}', 'Giant Growth', p, 'exile')
            if self.mode == 'empty_target' and p == target:
                continue
            names = ('Forest', 'Lightning Bolt', 'Hill Giant') if p == self.actor else ('Island', 'Unsummon')
            for index, name in enumerate(names):
                self.add(f'grave_{p}_{index}', name, p, 'graveyard')
        self.emit('assert_stack', value=0)
        self.mana(self.actor)
        self.emit('choose_player', player=self.actor, value=target)
        self.emit('play_land', 'source', value=1)
        self.resolve()
        self.move('source', 'battlefield')
        for ref in list(self.zones[target]['graveyard']):
            self.move(ref, 'exile')
        self.prop('source', 'in_play', 1)
        self.prop('source', 'tapped', 1)
        self.emit('can_activate', 'source', value=0)
        self.emit('activate', 'source', value=0)
        self.mana(self.actor)
        self.receipts()
        if self.mode == 'tapped_black_mana':
            self.next_own_main()
            self.prop('source', 'tapped', 0)
            self.emit('can_activate', 'source', value=1)
            self.emit('activate', 'source', value=1)
            self.prop('source', 'tapped', 1)
            self.emit('can_activate', 'source', value=0)
            self.pools[self.actor] = BLACK

    def sweepers(self):
        mode = self.mode
        self.add('own', 'Hill Giant', self.actor, 'battlefield')
        self.add('foe', 'Grizzly Bears', self.foe, 'battlefield')
        self.add('foe_third', 'Giant Spider', self.foe, 'battlefield')
        self.add('noncreature', 'Darksteel Relic', self.foe, 'battlefield')
        if mode == 'countered':
            self.counter_entry()
            return
        first = self.borrow('foe') if mode == 'borrowed' else 'foe'
        choices = {
            'two': (('own', 'own'), (first, 'foe')),
            'one': ((first, 'foe'),),
            'borrowed': ((first, 'foe'),),
            'self': (('source', 'source'),),
            'zero': (),
            'flying': (),
        }[mode]
        self.begin_cast()
        if self.actor == 0:
            self.emit('choose_trigger', 'source', player=0)
        for pick, _ in choices:
            self.choose_target(pick)
        if len(choices) < 2:
            self.emit('choose_cancel_target', player=self.actor)
        self.resolve()
        self.move('source', 'battlefield')
        for _, ref in choices:
            destination_seat = self.foe if ref == 'foe' else self.actor
            self.move(ref, 'hand', seat=destination_seat)
        if mode != 'self':
            self.stats('source')
        if mode == 'flying':
            self.combat('foe', 'foe_third')
            self.move('foe_third', 'graveyard')
            self.stats('source', damage=2)

    def horror(self):
        mode = self.mode
        if mode == 'countered':
            self.add('kept_land', 'Island', self.foe, 'battlefield')
            self.counter_entry()
            return
        target_name = {'land': 'Island', 'artifact': 'Darksteel Relic', 'borrowed': 'Hill Giant',
                       'own_target': 'Grizzly Bears', 'same_name_hand': 'Island'}[mode]
        target_owner = self.actor if mode == 'own_target' else self.foe
        self.add('target', target_name, target_owner, 'battlefield')
        if mode == 'same_name_hand':
            self.add('same', target_name, target_owner, 'hand')
        self.add('other_hand', 'Forest', 1 - target_owner, 'hand')
        self.add('other_permanent', 'Walking Corpse', 1 - target_owner, 'battlefield')
        target_ref = self.borrow('target') if mode == 'borrowed' else 'target'
        self.begin_cast()
        self.choose_target(target_ref)
        if target_owner == 0:
            self.emit('choose_zone_card', player=0, value=self.rows[target_name]['csvid'])
        else:
            self.emit('choose_dialog', player=1, value=0)
        self.resolve()
        self.move('source', 'battlefield')
        self.move('target', 'hand', seat=target_owner)
        self.move('target', 'graveyard')
        self.stats('source')

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
                if zone == 'library' or (zone == 'graveyard' and self.cid in ORIGINAL):
                    for index, name in enumerate(names):
                        self.emit('assert_zone_card', player=p, zone=zone, index=index, value=self.rows[name]['csvid'])

    def play(self):
        self.emit('turn', player=self.actor)
        self.emit('present_ai_dialogs', value=1)
        for seat in (0, 1):
            self.mana(seat, write=True)
        cases = {LIGHT: self.lightcaster, BOG: self.bog, SWEEP: self.sweepers, HORROR: self.horror}
        if self.cid in cases:
            cases[self.cid]()
        else:
            self.additional_entry()
        self.receipts()
        self.emit('present_ai_dialogs', value=0)


    def id(self, ref):
        return self.rows[self.cards[ref][0]]['csvid']

    def pick_zone(self, ref):
        self.emit('choose_zone_card', player=self.actor, value=self.id(ref))

    def enter(self, choice=None, *, spell_target=None, mana=None, cast_choices=None):
        self.begin_cast(target=spell_target, mana=mana, choices=cast_choices)
        if choice:
            choice()
        self.resolve()
        self.move('source', 'battlefield')
        if 'Creature' in self.oracle[self.name]['type_line']:
            self.stats('source')
        else:
            self.prop('source', 'in_play', 1)

    def cast_witness(self, ref, target):
        self.begin_cast(ref, target=target)
        self.resolve()
        self.move(ref, 'graveyard')

    def ground_reach(self):
        self.add('ground', 'Grizzly Bears', self.foe, 'battlefield')
        self.add('reach', 'Giant Spider', self.foe, 'battlefield')

    def strike_through_reach(self):
        self.combat('ground', 'reach')
        self.move('reach', 'graveyard')
        self.stats('source', damage=int(self.oracle['Giant Spider']['power']))

    def ashen(self):
        target = 'entry_target'
        self.add(target, 'Darksteel Relic', self.actor if self.mode == 'own_target' else self.foe, 'battlefield')
        if self.mode == 'flying':
            self.ground_reach()
        if self.mode == 'entry_and_death':
            self.add('death_target', 'Island', self.foe, 'battlefield')
            self.add('murder', 'Murder', self.actor, 'hand')
        self.enter(lambda: self.choose_target(target))
        self.move(target, 'exile')
        self.receipts()
        if self.mode == 'entry_and_death':
            self.begin_cast('murder', target='source')
            self.choose_target('death_target')
            self.resolve()
            self.move('murder', 'graveyard')
            self.move('source', 'graveyard')
            self.move('death_target', 'exile')
        if self.mode == 'flying':
            self.strike_through_reach()

    def sibsig(self):
        self.add('foe_creature', 'Grizzly Bears', self.foe, 'graveyard')
        if self.mode != 'foreign_grave':
            self.add('recover', 'Hill Giant', self.actor, 'graveyard')
        if self.mode == 'delve_return':
            fodder = ('Forest', 'Island', 'Plains', 'Mountain', 'Swamp', 'Unsummon', 'Lightning Bolt', 'Giant Growth')
            refs = [f'fuel_{index}' for index in range(len(fodder))]
            for ref, name in zip(refs, fodder):
                self.add(ref, name, self.actor, 'graveyard')

            def delve_choices():
                for ref in refs:
                    self.pick_zone(ref)

            self.begin_cast(mana=BLACK, choices=delve_choices)
            for ref in refs:
                self.move(ref, 'exile')
                self.emit('assert_zone_multiplicity', player=self.actor, zone='exile', index=self.id(ref), value=1)
                self.emit('assert_zone_multiplicity', player=self.actor, zone='graveyard', index=self.id(ref), value=0)
            self.pick_zone('recover')
            self.resolve()
            self.move('source', 'battlefield')
            self.stats('source')
        elif self.mode == 'ordinary_return':
            self.enter(lambda: self.pick_zone('recover'),
                       cast_choices=lambda: self.emit('choose_dialog', player=self.actor, value=1))
        else:
            self.enter()
        if self.mode != 'foreign_grave':
            self.move('recover', 'hand')

    def rampager(self):
        attempts = {'zero': 1, 'one': 2, 'pay_two': 3, 'opponent_energy': 1}[self.mode]
        self.emit('assert_energy', player=self.actor, value=0)
        foe_energy = 0
        if self.mode == 'opponent_energy':
            self.add('energy', 'Aether Hub', self.foe, 'hand')
            self.emit('turn', player=self.foe)
            self.emit('play_land', 'energy', value=1)
            self.resolve()
            self.move('energy', 'battlefield')
            foe_energy = 1
            self.emit('turn', player=self.actor)
        self.emit('assert_energy', player=self.foe, value=foe_energy)
        for retry in range(attempts):
            if retry:
                self.emit('bind_hand', 'source', player=self.actor, value=self.cid)
            self.begin_cast()
            self.resolve()
            if retry < 2:
                self.emit('assert_energy', player=self.actor, value=retry + 1)
            else:
                self.move('source', 'battlefield')
                self.stats('source')
                self.emit('assert_energy', player=self.actor, value=0)
            self.emit('assert_energy', player=self.foe, value=foe_energy)
            self.receipts()

    def sedraxis(self):
        qualified = self.mode != 'opponent_blue'
        self.add('blue', 'Horned Turtle', self.actor if qualified else self.foe, 'battlefield')
        self.add('land_foil', 'Island', self.foe, 'battlefield')
        self.add('nonland', 'Darksteel Relic' if self.mode != 'land_foil' else 'Walking Corpse',
                 self.foe, 'battlefield')
        self.enter((lambda: self.choose_target('nonland')) if qualified else None)
        if qualified:
            self.move('nonland', 'hand')

    def hedron(self):
        self.add('low_power', 'Wall of Stone', self.foe, 'battlefield')
        self.add('low_creature', 'Hill Giant', self.actor, 'battlefield')
        owners = {}
        if self.mode != 'no_match':
            for ref, name, seat in (('exact', 'Spined Wurm', self.actor), ('high', 'Yavimaya Wurm', self.foe)):
                self.add(ref, name, seat, 'battlefield')
                owners[ref] = seat
        if self.mode == 'return_owner':
            self.borrow('high')
            self.add('remove', 'Naturalize', self.actor, 'hand')
        self.enter()
        for ref, seat in owners.items():
            self.move(ref, 'exile', seat=seat)
        self.receipts()
        if self.mode == 'return_owner':
            self.cast_witness('remove', 'source')
            self.move('source', 'graveyard')
            for ref, seat in owners.items():
                self.move(ref, 'battlefield')
                self.emit('bind_permanent', ref, player=seat, value=self.id(ref))
                self.stats(ref)

    def corpse(self):
        periodic_combat = self.mode in ('poison', 'infect_blocked')
        if self.mode == 'infect_blocked':
            self.add('blocker', 'Giant Spider', self.foe, 'battlefield')
        elif self.mode == 'poison':
            self.add('ground', 'Grizzly Bears', self.foe, 'battlefield')
        self.add('normal_creature', 'Hill Giant', self.actor, 'graveyard')
        self.add('infect_creature', 'Plague Stinger',
                 self.actor if self.mode == 'return_infect' else self.foe, 'graveyard')

        def choose():
            if self.actor == 0 and self.mode == 'return_infect':
                self.emit('choose_trigger', 'source', player=0)
            if self.mode == 'return_infect':
                self.pick_zone('infect_creature')

        self.enter(choose)
        if self.mode == 'return_infect':
            self.move('infect_creature', 'hand')
        if periodic_combat:
            blocked = self.mode == 'infect_blocked'
            self.combat(None, 'blocker' if blocked else None)
            if blocked:
                self.move('source', 'graveyard')
                self.prop('blocker', 'minus1_counters', 2)
                self.prop('blocker', 'power', 0)
                self.prop('blocker', 'toughness', 2)
        self.emit('assert_poison', player=self.actor, value=0)
        self.emit('assert_poison', player=self.foe, value=2 if self.mode == 'poison' else 0)

    def angel(self):
        exiles = self.mode in ('mixed_tribes', 'exile_zombies')
        returns = self.mode in ('mixed_tribes', 'return_humans')
        if exiles:
            self.add('own_zombie', 'Walking Corpse', self.actor, 'battlefield')
            self.add('foe_zombie', 'Scathe Zombies', self.foe, 'battlefield')
        if returns:
            self.add('human', 'Cathedral Sanctifier', self.actor, 'graveyard')
            self.add('human2', 'Devoted Retainer', self.actor, 'graveyard')
        self.add('foe_human', 'Cathedral Sanctifier', self.foe, 'graveyard')
        self.add('own_nonhuman', 'Walking Corpse', self.actor, 'graveyard')
        if self.mode == 'flying':
            self.ground_reach()
        self.enter()
        if exiles:
            for ref in ('own_zombie', 'foe_zombie'):
                self.move(ref, 'exile')
        if returns:
            for ref in ('human', 'human2'):
                self.move(ref, 'battlefield')
                self.emit('bind_permanent', ref, player=self.actor, value=self.id(ref))
                self.stats(ref)
            self.life[self.actor] += 3
        if self.mode == 'flying':
            self.strike_through_reach()

    def coercion(self):
        chosen = {
            COLLECTOR: {'instant': 'Lightning Bolt', 'sorcery': 'Divination'},
            LIFEBANE: {'green': 'Grizzly Bears', 'white': 'Savannah Lions'},
        }[self.cid].get(self.mode)
        if chosen:
            self.add('selection', chosen, self.foe, 'hand')
            self.add('foe_foil', 'Walking Corpse', self.foe, 'hand')
        self.add('own_foil', chosen or ('Unsummon' if self.cid == COLLECTOR else 'Grizzly Bears'),
                 self.actor, 'hand')
        if self.mode == 'intimidate':
            self.add('green', 'Grizzly Bears', self.foe, 'battlefield')
            self.add('artifact', 'Ornithopter', self.foe, 'battlefield')
        self.enter((lambda: self.pick_zone('selection')) if chosen else None)
        if chosen:
            self.move('selection', 'exile')
        if self.mode == 'intimidate':
            self.combat('green', 'artifact')
            self.move('artifact', 'graveyard')
            self.stats('source', damage=0)

    def deadwood(self):
        selected = self.mode != 'empty'
        self.add('grave_foil', 'Lightning Bolt', self.actor, 'graveyard')
        self.add('foe_creature', 'Grizzly Bears', self.foe, 'graveyard')
        if selected:
            self.add('incoming', 'Walking Corpse', self.actor, 'graveyard')
            self.add('outgoing', 'Hill Giant', self.actor, 'graveyard')
        if self.mode == 'bounce':
            self.add('bounce', 'Unsummon', self.actor, 'hand')
        self.enter((lambda: self.pick_zone('incoming')) if selected else None)
        if selected:
            self.move('incoming', 'hand')
        self.emit('assert_counter', 'source', prop='time', value=3)
        self.receipts()
        if self.mode == 'bounce':
            self.begin_cast('bounce', target='source')
            self.pick_zone('outgoing')
            self.resolve()
            self.move('bounce', 'graveyard')
            self.move('source', 'hand')
            self.move('outgoing', 'hand')
        if self.mode == 'vanish':
            for remaining in (2, 1, 0):
                self.emit('advance_turn')
                self.emit('untap_step', player=self.foe)
                self.emit('upkeep_step', player=self.foe)
                self.emit('assert_counter', 'source', prop='time', value=remaining + 1)
                self.emit('advance_turn')
                self.emit('untap_step', player=self.actor)
                if not remaining:
                    self.pick_zone('outgoing')
                self.emit('upkeep_step', player=self.actor)
                self.emit('assert_unused_choices', value=0)
                if remaining:
                    self.emit('assert_counter', 'source', prop='time', value=remaining)
                else:
                    self.move('source', 'graveyard')
                    self.move('outgoing', 'hand')

    def chained(self):
        denied = self.mode == 'wrong_mountain'
        self.add('mountain', 'Mountain', self.foe if denied else self.actor, 'battlefield')
        self.add('island', 'Island', self.actor, 'battlefield')
        self.add('own_creature', 'Grizzly Bears', self.actor, 'battlefield')
        has_target = self.mode in ('return_creature', 'host_destroyed')
        if has_target:
            self.add('enemy', 'Hill Giant', self.foe, 'battlefield')
            self.add('remove', 'Naturalize' if self.mode == 'return_creature' else 'Stone Rain',
                     self.actor, 'hand')
        if denied:
            self.mana(self.actor, cost_from_oracle(self.oracle[self.name]), write=True)
            self.emit('refresh')
            self.emit('can_cast', 'source', value=0)
            self.emit('cast', 'source', value=0)
            self.pools[self.actor] = tuple(cost_from_oracle(self.oracle[self.name]))
            return
        self.enter((lambda: self.choose_target('enemy')) if has_target else None, spell_target='mountain')
        if has_target:
            self.move('enemy', 'exile')
            self.receipts()
            host = self.mode == 'host_destroyed'
            self.cast_witness('remove', 'mountain' if host else 'source')
            if host:
                self.move('mountain', 'graveyard')
            self.move('source', 'graveyard')
            self.move('enemy', 'battlefield')
            self.emit('bind_permanent', 'enemy', player=self.foe, value=self.id('enemy'))
            self.stats('enemy')

    def tidecaller(self):
        awaken = self.mode == 'awaken'
        self.add('awaken', 'Coastal Discovery', self.actor if awaken else self.foe, 'graveyard')
        self.add('no_awaken', 'Divination', self.actor, 'graveyard')
        flying = self.mode == 'own_land_flying'
        land_combat = flying or self.mode == 'opponent_land_ground'
        if land_combat:
            land_controller = self.actor if flying else self.foe
            self.add('land_creature', 'Dryad Arbor', land_controller, 'battlefield')
            self.add('ground', 'Grizzly Bears', 1 - land_controller, 'battlefield')

        def choice():
            if awaken:
                if self.actor == 0:
                    self.emit('choose_trigger', 'source', player=0)
                self.pick_zone('awaken')

        self.enter(choice)
        if awaken:
            self.move('awaken', 'hand')
        if flying:
            self.combat('ground', attacker='land_creature')
            self.life[self.foe] -= 1
        elif land_combat:
            self.emit('advance_turn')
            self.emit('untap_step', player=self.foe)
            self.emit('turn', player=self.foe)
            self.combat(None, 'ground', attacker='land_creature', advance=False)
            self.move('land_creature', 'graveyard')
            self.stats('ground', damage=1)

    def additional_entry(self):
        actions = {
            ASHEN: self.ashen, SIBSIG: self.sibsig, RAMPAGER: self.rampager,
            SEDRAXIS: self.sedraxis, HEDRON: self.hedron, CORPSE: self.corpse,
            ANGEL: self.angel, COLLECTOR: self.coercion, DEADWOOD: self.deadwood,
            CHAINED: self.chained, TIDECALLER: self.tidecaller, LIFEBANE: self.coercion,
        }
        actions[self.cid]()


SEATED = {
    'activate', 'advance_turn', 'assert_energy', 'assert_life', 'assert_mana', 'assert_poison',
    'assert_shuffle_count', 'bind_hand', 'bind_permanent', 'bind_permanent_copy', 'choose_cancel_target',
    'choose_dialog', 'choose_player', 'choose_target', 'choose_trigger', 'choose_zone_card',
    'set_mana', 'turn', 'untap_step', 'upkeep_step',
}
VALUES = {
    'activate', 'assert_card', 'assert_counter', 'assert_energy', 'assert_life', 'assert_mana',
    'assert_poison', 'assert_shuffle_count', 'assert_stack', 'assert_unused_choices', 'assert_zone',
    'assert_zone_card', 'assert_zone_multiplicity', 'attack', 'bind_hand', 'bind_permanent',
    'bind_permanent_copy', 'block', 'can_activate', 'can_cast', 'cast', 'cast_response',
    'choose_dialog', 'choose_player', 'choose_zone_card', 'play_land', 'present_ai_dialogs', 'set_mana',
}
BY_INDEX = {'assert_zone_card', 'assert_zone_multiplicity', 'bind_permanent_copy'}
ZONE_OPS = {'assert_zone', 'assert_zone_card', 'assert_zone_multiplicity'}
GAPS = {
    LIGHT: ['Legacy target choice and exile are synchronous, so no target-removal window is observed.'],
    SWEEP: ['Legacy target selection and bounce are synchronous. The pilot covers final legal target counts. The priority window between selection and resolution remains untested.'],
    HORROR: ['The discard callback follows the bounce synchronously, with deterministic AI card identity where two equivalent copies exist.'],
    HEDRON: ['Self-exile loops involving a type-changing effect are outside the bounded runner.'],
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
            step['counter_kind' if op == 'assert_counter' else 'property'] = prop
        if op in SEATED or op in ZONE_OPS:
            if op not in ('activate', 'advance_turn'):
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
        raise ValueError('zone-movement source CSVID changed')
    names = {row['csvid']: name for name, row in rows.items() if row['kind'] == 'card'}
    zones = tuple(ZONE)
    result = []
    for variant in selected:
        mode, seat = variant.rsplit('_', 1)
        scene = Scene(rows[name], int(seat), mode, rows, oracle)
        scene.play()
        setup = [dict(ref=ref, player=player, zone=zones[zone], name=names[cid])
                 for ref, player, zone, cid, state in scene.setup]
        result.append(dict(name=f'{LABEL}_{csvid}_{variant}', card=name, fixture='empty_main', seed=SEED,
                           setup=setup, steps=list(shaped_steps(scene.steps)),
                           _recipe=dict(family=FAMILY, version=2, variant=variant, params=declaration['params'],
                                        oracle_text=declaration['oracle_text'], untested_components=GAPS.get(csvid, []))))
    return result
