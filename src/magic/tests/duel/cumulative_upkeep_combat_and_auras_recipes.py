"""Whole cumulative upkeep and printed activated, aura and combat effects."""
from functools import lru_cache
import json
from pathlib import Path

from counter_condition_recurrence_recipes import exact
from cumulative_upkeep_payment_recipes import REMINDER
from recipes import card, cost_from_oracle, step


FAMILY = 'cumulative_upkeep_combat_and_auras'
PROFILE = Path(__file__).with_name('data') / 'cumulative_upkeep_combat_and_auras_wave8_profiles.json'
SEED = 2503
PREFIX = 'CumulativeCombatAndAurasRecipe'
BANDING = ('Banding (Any creatures with banding, and up to one without, can attack in a band. Bands are blocked as a group. '
           "If any creatures with banding you control are blocking or being blocked by a creature, you divide that creature's "
           "combat damage, not its controller, among any of the creatures it's being blocked by or is blocking.)")
ZERO = [0] * 6
BEAR = 'Grizzly Bears'
SQUIRE = 'Squire'
GIANT = 'Hill Giant'
GOBLIN = "Mons's Goblin Raiders"
RELIC = "Feldon's Cane"
WITNESSES = {'Island', 'Mountain', 'Swamp', 'Wastes', 'Plains', BEAR, SQUIRE, GIANT, 'Wind Drake',
             'Forest', 'Disenchant', GOBLIN, RELIC}
NEW_WITNESSES = {'Forest', 'Disenchant', GOBLIN, RELIC}
BANDING_GAP = ('The native defending seat 0 makes the damage assignment through the existing combat prompt. '
               'Seat 1 retains automatic allocation. Attacking band formation is outside the declaration wrapper.')
ASCENT, FALLEN, JAV, SIMULACRUM = 1859, 8335, 8397, 2109
BRAND, POLLEN, WIND, RESERVES = 1878, 1942, 2024, 3066
CARDS = {
    ASCENT: ('{U}', '{1}{U}{U}', 'Enchantment', None, '{1}: Target creature gains flying until end of turn.'),
    FALLEN: ('{1}', '{3}{B}', 'Creature — Zombie', (3, 5),
             "Whenever this creature's cumulative upkeep is paid, it gets +1/+0 until end of turn for each {B} or {R} spent this way."),
    JAV: ('{1}', '{W}', 'Creature — Human Soldier', (1, 2),
          '{T}: This creature deals damage equal to the number of age counters on it to target attacking or blocking creature.'),
    SIMULACRUM: ('{1}', '{4}', 'Artifact Creature — Soldier', (2, 4), '{1}: This creature gets +1/+0 until end of turn.'),
}
NEW = {
    BRAND: ('{R}', '{3}{R}', 'Enchantment — Aura', None, "Enchanted creature's controller can't cast creature spells."),
    POLLEN: ('{1}', '{2}{G}', 'Enchantment', None, ('All creatures get -1/-0.', '{1}{G}: All creatures get -1/-0 until end of turn.')),
    WIND: ('{G}', '{2}{G}', 'Enchantment — Aura', None,
           "At the beginning of the upkeep of enchanted creature's controller, this Aura deals 2 damage to that player."),
    RESERVES: ('{1}', '{1}{W}', 'Creature — Human Soldier', (2, 4), None),
}
CARDS.update(NEW)
COMMON = ('paid_one', 'paid_two', 'decline_two')
EXTRA = {ASCENT: ('flight_expires',), FALLEN: ('neutral_payment',), JAV: ('blocking_target',),
         BRAND: ('own_brand',), POLLEN: ('static_only',), WIND: ('own_wind',)}
SECOND_UPKEEP = {'paid_two', 'decline_two', 'neutral_payment', 'blocking_target'}


@lru_cache(maxsize=1)
def profiles():
    return json.loads(PROFILE.read_bytes())


def classify(record, csvid):
    if (csvid not in CARDS or record.get('layout') != 'normal' or record.get('selected_face')
            or record.get('card_faces')):
        raise ValueError('complete normal cumulative upkeep primary required')
    fee, mana, type_line, stats, effect = CARDS[csvid]
    cu = f'Cumulative upkeep {fee}{REMINDER}'
    if csvid in (BRAND, WIND):
        lines, keywords = ('Enchant creature', cu, effect), ['Enchant', 'Cumulative upkeep']
    elif csvid == POLLEN:
        lines, keywords = (cu, *effect), ['Cumulative upkeep']
    elif csvid == RESERVES:
        lines, keywords = (BANDING, cu), ['Banding', 'Cumulative upkeep']
    else:
        lines, keywords = (cu, effect), ['Cumulative upkeep']
    if (record.get('oracle_text') != '\n'.join(lines) or record.get('keywords') != keywords
            or record.get('type_line') != type_line or record.get('mana_cost') != mana
            or stats and (record.get('power'), record.get('toughness')) != tuple(map(str, stats))):
        raise ValueError('complete cumulative upkeep source text, stats, and mana required')
    return dict(csvid=csvid, mana=cost_from_oracle(record), upkeep=cost_from_oracle({'mana_cost': fee}))


def variants(shape):
    return [f'{mode}_{actor}' for mode in (*COMMON, *EXTRA.get(shape['csvid'], ())) for actor in (0, 1)]


def validate_variants(shape, selected):
    if (type(selected) is not list or not selected or any(type(mode) is not str for mode in selected)
            or len(set(selected)) != len(selected) or set(selected) - set(variants(shape))):
        raise ValueError('distinct supported cumulative upkeep variants required')


def spec(declaration, record):
    pinned = profiles()['sources'].get(record.get('name'))
    if not pinned or not exact(pinned['oracle'], record):
        raise ValueError('complete pinned cumulative upkeep source required')
    shape = classify(record, pinned['registry']['csvid'])
    if (declaration.get('family') != FAMILY or type(declaration.get('version')) is not int
            or declaration['version'] != 2 or declaration.get('oracle_text') != record['oracle_text']
            or not exact(declaration.get('params'), shape)):
        raise ValueError('exact declared cumulative upkeep branch required')
    validate_variants(shape, declaration.get('variants'))
    return dict(shape, family=FAMILY)


def validate_witnesses(oracle, rows):
    pins = profiles()
    if set(pins) != {'sources', 'witnesses'} or set(pins['witnesses']) != WITNESSES:
        raise ValueError('complete independent cumulative upkeep witnesses required')
    for group, entries in pins.items():
        for name, pin in entries.items():
            row = rows.get(name, {})
            if (not exact(oracle.get(name), pin['oracle']) or not exact(row, pin['registry'])
                    or row.get('kind') != 'card' or row.get('handler') in (None, '0', 'NULL')
                    or oracle[name].get('layout') != 'normal'):
                raise ValueError('exact cumulative upkeep source or witness changed: ' + name)
            if group == 'sources':
                if row['handler'] == 'card_dummy':
                    raise ValueError('implemented cumulative upkeep primary required')
                classify(oracle[name], row['csvid'])


class Duel:
    def __init__(self, csvid, record, actor, mode, rows, oracle):
        self.csvid, self.record, self.actor, self.foe, self.mode = csvid, record, actor, 1 - actor, mode
        self.rows, self.oracle = rows, oracle
        self.setup, self.steps = [], []
        self.age, self.live, self.power_bonus = 0, True, 0
        self.life = [20, 20]
        self.add('source', actor, record['name'], 'hand')
        if csvid == ASCENT:
            self.lands = ('Island', 'Island')
        elif csvid == FALLEN and mode != 'neutral_payment':
            self.lands = ('Swamp', 'Mountain')
        elif csvid == BRAND:
            self.lands = ('Mountain', 'Mountain')
        elif csvid == WIND:
            self.lands = ('Forest', 'Forest')
        else:
            self.lands = ('Wastes', 'Wastes')
        self.land_refs = ['land_0']
        self.add('land_0', actor, self.lands[0], 'battlefield')
        if mode in SECOND_UPKEEP:
            self.add('land_1', actor, self.lands[1], 'hand')
        for seat in (0, 1):
            self.add(f'deck_{seat}', seat, 'Plains', 'library')
        if csvid in (ASCENT, JAV):
            self.add('ally', actor, BEAR, 'battlefield')
        if csvid == ASCENT:
            self.add('enemy', self.foe, 'Wind Drake' if mode == 'flight_expires' else SQUIRE, 'battlefield')
        if csvid == JAV:
            self.victim = SQUIRE if mode == 'blocking_target' else BEAR
            self.add('victim', self.foe, self.victim, 'battlefield')
            self.add('noncombat', self.foe, 'Hill Giant', 'battlefield')

        if csvid in (BRAND, WIND):
            self.host_seat = self.actor if mode in ('own_brand', 'own_wind') else self.foe
            self.add('host', self.host_seat, BEAR, 'battlefield')
        if csvid == BRAND:
            self.add('own_creature', self.actor, SQUIRE, 'hand')
            self.add('foe_creature', self.foe, SQUIRE, 'hand')
            self.add('other_spell', self.host_seat, 'Disenchant', 'hand')
            self.add('relic', self.actor, RELIC, 'battlefield')
        if csvid == POLLEN:
            self.add('ally', self.actor, GIANT, 'battlefield')
            self.add('enemy', self.foe, BEAR, 'battlefield')
            self.add('relic', self.actor, RELIC, 'battlefield')
        if csvid == RESERVES:
            self.add('ally', self.actor, BEAR, 'battlefield')
            self.add('attacker', self.foe, GOBLIN, 'battlefield')

    def add(self, ref, player, name, zone):
        self.setup.append(card(name, ref, player, zone))

    def emit(self, op, ref='', other='', **values):
        if ref:
            values['ref'] = ref
        if other:
            values['other'] = other
        self.steps.append(step(op, **values))

    def observe(self, ref, prop, value):
        self.emit('assert_card', ref, property=prop, value=value)

    def mana(self, op, seat, pool):
        for color, value in enumerate(pool):
            self.emit(op, player=seat, color=color, value=value)

    def zone(self, name, seat, zone, count):
        self.emit('assert_zone_multiplicity', player=seat, zone=zone, index=self.rows[name]['csvid'], value=count)

    def resolve(self):
        self.emit('resolve')
        self.emit('assert_stack')
        self.emit('assert_choices_consumed')
        self.emit('refresh')

    def mana_empty(self):
        for seat in (0, 1):
            self.mana('assert_mana', seat, ZERO)

    def state(self):
        name = self.record['name']
        self.zone(name, self.actor, 'hand', 0)
        self.zone(name, self.actor, 'battlefield', int(self.live))
        self.zone(name, self.actor, 'graveyard', int(not self.live))
        for seat, life in enumerate(self.life):
            self.emit('assert_life', player=seat, value=life)
        if self.live:
            self.observe('source', 'in_play', 1)
            self.emit('assert_counter', 'source', counter_kind='age', value=self.age)
            stats = CARDS[self.csvid][3]
            if stats:
                self.observe('source', 'power', stats[0] + self.power_bonus)
                self.observe('source', 'toughness', stats[1])

    def cast(self):
        self.emit('turn', player=self.actor)
        self.mana('set_mana', self.foe, ZERO)
        self.mana('set_mana', self.actor, cost_from_oracle(self.record))
        self.emit('refresh')
        if self.csvid in (BRAND, WIND):
            self.emit('choose_target', 'host', player=self.actor)
        self.emit('cast', 'source', value=1)
        self.emit('assert_stack', value=1)
        self.mana_empty()
        self.resolve()
        self.observe('land_0', 'tapped', 0)
        self.state()

    def visit(self, seat, *, decline=False):
        was_live = self.live
        self.emit('advance_turn')
        self.power_bonus = 0
        self.emit('untap_step', player=seat)
        if seat == self.actor:
            self.age += 1
            assert self.age == len(self.land_refs)
            self.emit('choose_dialog', player=seat, value=int(decline))
            if seat == 0 and not decline:
                for ref in self.land_refs:
                    self.emit('choose_mana_source', ref)
            if self.csvid == FALLEN and not decline and self.mode != 'neutral_payment':
                self.power_bonus = self.age
            self.live = not decline
        if was_live and self.csvid == WIND and seat == self.host_seat:
            self.life[seat] -= 2
        # Probe a payment offer that Oracle does not permit.
        wrong_turn_probe = self.csvid == WIND and was_live and self.age == 0 and seat == self.foe == self.host_seat
        repeated_own_probe = self.csvid == WIND and seat == self.actor == self.host_seat == 1
        payment_probe = wrong_turn_probe or repeated_own_probe
        if payment_probe:
            self.emit('choose_dialog', player=self.actor, value=1)
        self.emit('upkeep_step', player=seat)
        self.emit('assert_unused_choices', value=int(payment_probe))
        self.emit('assert_choices_consumed')
        self.emit('assert_stack')
        self.mana_empty()
        self.emit('refresh')
        self.emit('turn', player=seat)
        if seat == self.actor:
            for ref in self.land_refs:
                self.observe(ref, 'tapped', int(not decline))
        self.state()

    def accept_second_land(self):
        self.emit('play_land', 'land_1', value=1)
        self.emit('assert_choices_consumed')
        self.observe('land_1', 'in_play', 1)
        self.observe('land_1', 'tapped', 0)
        self.land_refs.append('land_1')

    def combat_damage(self):
        self.emit('combat_damage')
        self.emit('resolve')
        self.emit('end_combat')
        self.emit('assert_choices_consumed')

    def unblocked_source(self):
        self.emit('begin_attackers')
        self.emit('attack', 'source', value=1)
        self.emit('finish_attackers')
        self.emit('begin_blockers')
        self.emit('advance_combat')
        self.observe('source', 'blocked', 0)
        self.combat_damage()
        self.life[self.foe] -= CARDS[self.csvid][3][0] + self.power_bonus
        self.state()
        self.visit(self.foe)

    def activate_with_mana(self, target=None, remaining=0):
        self.emit('can_activate', 'source', value=1)
        if target:
            self.emit('choose_target', target, player=self.actor)
        self.emit('activate', 'source', value=1)
        self.emit('assert_stack', value=1)
        self.mana('assert_mana', self.actor, [remaining, 0, 0, 0, 0, 0])
        self.resolve()
        self.observe('source', 'tapped', 0)

    def ascent(self):
        self.emit('can_activate', 'source', value=0)
        self.mana('set_mana', self.actor, [1, 0, 0, 0, 0, 0])
        self.emit('refresh')
        self.activate_with_mana('ally')
        if self.mode == 'flight_expires':
            self.visit(self.foe)
            attacker, blocker = 'enemy', 'ally'
            self.life[self.actor] -= 2
        else:
            attacker, blocker = 'ally', 'enemy'
            self.life[self.foe] -= 2
        self.emit('begin_attackers')
        self.emit('attack', attacker, value=1)
        self.emit('finish_attackers')
        self.emit('begin_blockers')
        self.emit('block', blocker, attacker, value=0)
        self.emit('advance_combat')
        self.observe(attacker, 'blocked', 0)
        self.combat_damage()
        self.state()

    def javelineer(self):
        self.emit('can_activate', 'source', value=0)
        self.emit('activate', 'source', value=0)
        blocking = self.mode == 'blocking_target'
        if not blocking:
            self.visit(self.foe)
        attacker = 'ally' if blocking else 'victim'
        self.emit('begin_attackers')
        self.emit('attack', attacker, value=1)
        self.emit('finish_attackers')
        if blocking:
            self.emit('begin_blockers')
            self.emit('block', 'victim', attacker, value=1)
            self.emit('advance_combat')
            self.observe(attacker, 'blocked', 1)
        self.emit('can_activate', 'source', value=1)
        self.emit('probe_activation_target', 'source', 'noncombat', index=-1, value=0)
        self.emit('choose_target', 'victim', player=self.actor)
        self.emit('activate', 'source', value=1)
        self.emit('assert_stack', value=1)
        self.observe('source', 'tapped', 1)
        self.resolve()
        self.mana_empty()
        dead = self.age == 2
        self.zone(self.victim, self.foe, 'battlefield', int(not dead))
        self.zone(self.victim, self.foe, 'graveyard', int(dead))
        if not dead:
            self.observe('victim', 'damage', 1)
            self.life[self.actor] -= 2
        if not blocking:
            self.emit('begin_blockers')
            self.emit('advance_combat')
        elif dead:
            self.observe(attacker, 'blocked', 1)
        self.combat_damage()
        self.zone('Hill Giant', self.foe, 'battlefield', 1)
        self.state()

    def simulacrum(self):
        self.emit('can_activate', 'source', value=0)
        self.mana('set_mana', self.actor, [2, 0, 0, 0, 0, 0])
        self.emit('refresh')
        for result in (1, 2):
            self.activate_with_mana(remaining=2 - result)
            self.power_bonus = result
            self.state()
        self.emit('can_activate', 'source', value=0)
        self.unblocked_source()

    def cast_witness_creature(self, seat, allowed):
        ref = 'own_creature' if seat == self.actor else 'foe_creature'
        fee = cost_from_oracle(self.oracle[SQUIRE])
        self.mana('set_mana', seat, fee)
        self.emit('refresh')
        self.emit('cast', ref, value=int(allowed))
        self.emit('assert_choices_consumed')
        self.emit('assert_stack', value=int(allowed))
        self.mana('assert_mana', seat, ZERO if allowed else fee)
        if allowed:
            self.resolve()
        self.zone(SQUIRE, seat, 'hand', int(not allowed))
        self.zone(SQUIRE, seat, 'battlefield', int(allowed))

    def pay_other_spell(self):
        seat = self.host_seat
        assert cost_from_oracle(self.oracle['Disenchant']) == cost_from_oracle(self.oracle[SQUIRE])
        self.emit('choose_target', 'relic', player=seat)
        self.emit('cast', 'other_spell', value=1)
        self.emit('assert_stack', value=1)
        self.mana('assert_mana', seat, ZERO)
        self.resolve()
        self.zone('Disenchant', seat, 'graveyard', 1)
        self.zone(RELIC, self.actor, 'graveyard', 1)
        self.zone(RELIC, self.actor, 'battlefield', 0)
        self.state()

    def brand(self):
        self.visit(self.foe)
        self.cast_witness_creature(self.foe, allowed=self.mode == 'own_brand' or not self.live)
        self.observe('host', 'in_play', 1)
        self.state()

    def pollen_state(self, temporary=0):
        modifier = int(self.live) + temporary
        self.observe('ally', 'power', 3 - modifier)
        self.observe('ally', 'toughness', 3)
        self.observe('enemy', 'power', 2 - modifier)
        self.observe('enemy', 'toughness', 2)
        self.zone(RELIC, self.actor, 'battlefield', 1)

    def pollen(self):
        self.pollen_state()
        temporary = int(self.mode != 'static_only')
        if temporary:
            self.emit('can_activate', 'source', value=0)
            self.mana('set_mana', self.actor, cost_from_oracle({'mana_cost': '{1}{G}'}))
            self.emit('refresh')
            self.activate_with_mana()
            self.pollen_state(temporary=1)
        self.emit('begin_attackers')
        self.emit('attack', 'ally', value=1)
        self.emit('finish_attackers')
        self.emit('begin_blockers')
        self.emit('advance_combat')
        self.combat_damage()
        self.life[self.foe] -= 2 - temporary
        self.state()
        self.visit(self.foe)
        self.pollen_state()

    def wind(self):
        self.visit(self.foe)
        self.observe('host', 'in_play', 1)
        self.state()

    def reserves(self):
        self.visit(self.foe)
        self.emit('begin_attackers')
        self.emit('attack', 'attacker', value=1)
        self.emit('finish_attackers')
        self.emit('begin_blockers')
        self.emit('block', 'source', 'attacker', value=1)
        self.emit('block', 'ally', 'attacker', value=1)
        self.emit('advance_combat')
        self.observe('attacker', 'blocked', 1)
        recipient = 'ally' if self.age == 2 else 'source'
        if self.actor == 0:
            self.emit('choose_target', recipient, player=0)
        self.combat_damage()
        self.zone(GOBLIN, self.foe, 'graveyard', 1)
        self.zone(BEAR, self.actor, 'battlefield', 1)
        if self.actor == 0:
            self.observe('source', 'damage', int(recipient == 'source'))
            self.observe('ally', 'damage', int(recipient == 'ally'))
        self.state()

    def build(self):
        self.cast()
        self.visit(self.foe)
        if self.csvid == BRAND and self.host_seat == self.foe:
            self.cast_witness_creature(self.foe, allowed=False)
            self.pay_other_spell()
        self.visit(self.actor)
        if self.csvid == BRAND:
            self.cast_witness_creature(self.actor, allowed=self.host_seat != self.actor)
            if self.host_seat == self.actor:
                self.pay_other_spell()
        if self.mode in SECOND_UPKEEP:
            self.accept_second_land()
            self.visit(self.foe)
            self.visit(self.actor, decline=self.mode == 'decline_two')
        if self.csvid in (BRAND, WIND):
            (self.brand if self.csvid == BRAND else self.wind)()
        elif self.csvid == POLLEN and not self.live:
            self.pollen_state()
        elif self.live:
            {ASCENT: self.ascent, FALLEN: self.unblocked_source, JAV: self.javelineer,
             SIMULACRUM: self.simulacrum, POLLEN: self.pollen, RESERVES: self.reserves}[self.csvid]()



def build(name, csvid, declaration, shape, variant, rows, oracle):
    mode, actor = variant.rsplit('_', 1)
    duel = Duel(csvid, oracle[name], int(actor), mode, rows, oracle)
    duel.build()
    return dict(name=f'{PREFIX}_{csvid}_{variant}', card=name, fixture='empty_main', seed=SEED,
                setup=duel.setup, steps=duel.steps,
                _recipe=dict(family=FAMILY, version=2, variant=variant, params=declaration['params'],
                             oracle_text=declaration['oracle_text'],
                             untested_components=[BANDING_GAP] if csvid == RESERVES else []))


def expand(name, csvid, declaration, shape, selected, rows, oracle):
    if rows[name]['csvid'] != csvid or shape['csvid'] != csvid:
        raise ValueError('exact native cumulative upkeep source required')
    validate_variants(shape, selected)
    validate_witnesses(oracle, rows)
    return [build(name, csvid, declaration, shape, variant, rows, oracle) for variant in selected]
