"""Real regeneration costs, destruction responses, and graveyard riders."""

from collections import Counter
from functools import lru_cache
import json
from pathlib import Path

from death_counter_trigger_recipes import exact
from recipes import cost_from_oracle

FAMILY = 'regeneration_costs_and_zones'
PROFILE = Path(__file__).with_name('data') / f'{FAMILY}_wave8_profiles.json'
LABEL = 'RegenerationCostsAndZonesRecipe'
SEED = 2897
HARVESTER, GNOMES, SLASHER, WARLORD, KATHARI, WURM, VAROLZ, SCAVENGERS = (
    11231, 3224, 11517, 13355, 10067, 6023, 13241, 3071,
)
BLACK_SAC = (HARVESTER, KATHARI)
DISCARD = (GNOMES, WURM)
COMMON = ('paid', 'denied_cost', 'declined', 'cannot_regenerate', 'proactive', 'damage', 'combat')
MODES = {
    HARVESTER: (*COMMON, 'no_mana', 'wrong_color'),
    GNOMES: COMMON,
    SLASHER: COMMON,
    WARLORD: COMMON,
    KATHARI: (*COMMON, 'no_mana', 'wrong_color', 'flight'),
    WURM: COMMON,
    VAROLZ: (*COMMON, 'scavenge_paid', 'scavenge_zero', 'scavenge_other_target',
             'scavenge_other_grave', 'scavenge_offturn', 'scavenge_wrong_color'),
    SCAVENGERS: (*COMMON, 'grave_second'),
}
ZONE = ('hand', 'battlefield', 'library', 'graveyard', 'exile')
ZERO = (0, 0, 0, 0, 0, 0)
BLACK = (0, 1, 0, 0, 0, 0)
RED = (0, 0, 0, 0, 1, 0)


@lru_cache(maxsize=1)
def pins():
    return json.loads(PROFILE.read_bytes())


def classify(record, csvid):
    pin = pins()['sources'].get(record.get('name'))
    if (csvid not in MODES or not pin or pin['registry']['csvid'] != csvid
            or not exact(record, pin['oracle']) or record.get('layout') != 'normal'):
        raise ValueError('complete normal regeneration rules required')
    return dict(csvid=csvid, cost=cost_from_oracle(record))


def variants(shape):
    return [f'{mode}_{seat}' for mode in MODES[shape['csvid']] for seat in (0, 1)]


def spec(declaration, record):
    pin = pins()['sources'].get(record.get('name'))
    if not pin:
        raise ValueError('pinned regeneration source required')
    shape = classify(record, pin['registry']['csvid'])
    chosen = declaration.get('variants')
    if (declaration.get('family') != FAMILY or type(declaration.get('version')) is not int
            or declaration['version'] != 2 or declaration.get('oracle_text') != record['oracle_text']
            or not exact(declaration.get('params'), shape)):
        raise ValueError('whole versioned regeneration declaration required')
    if (type(chosen) is not list or not chosen or any(type(v) is not str for v in chosen)
            or len(chosen) != len(set(chosen)) or set(chosen) - set(variants(shape))):
        raise ValueError('explicit distinct supported regeneration variants required')
    return dict(shape, family=FAMILY)


def validate_witnesses(oracle, rows):
    for group in ('sources', 'witnesses'):
        for name, pin in pins()[group].items():
            row = rows.get(name, {})
            if (not exact(oracle.get(name), pin['oracle']) or not exact(row, pin['registry'])
                    or row.get('kind') != 'card' or row.get('handler') in (None, 'NULL', '0')
                    or group == 'sources' and row['handler'] == 'card_dummy'):
                raise ValueError(f'changed regeneration fixture identity: {name}')


class Scene:
    def __init__(self, row, owner, mode, rows, oracle):
        self.name, self.id = row['name'], row['csvid']
        self.owner, self.foe, self.mode = owner, 1 - owner, mode
        self.rows, self.oracle = rows, oracle
        self.setup, self.steps, self.cards = [], [], {}
        self.zones = {p: {z: [] for z in ZONE} for p in (0, 1)}
        self.pools = [ZERO, ZERO]
        self.add('source', self.name, owner, 'hand')
        self.cost_zone = 'hand' if self.id in DISCARD else 'graveyard' if self.id == SCAVENGERS else 'battlefield'
        self.cost_name = 'Forest' if self.id in DISCARD else 'Darksteel Relic' if self.id == SLASHER else 'Grizzly Bears'
        if mode.startswith('scavenge_'):
            self.stage_scavenge()
            return
        if mode == 'flight':
            self.add('ground', 'Hill Giant', self.foe, 'battlefield')
            self.add('reach', 'Giant Spider', self.foe, 'battlefield')
        if self.id == SCAVENGERS and mode != 'denied_cost':
            self.add('older_creature', 'Bog Imp', owner, 'graveyard')
        if mode == 'denied_cost':
            # The right card in the wrong seat/zone must never be paid.
            self.add('invalid_owner', self.cost_name, self.foe, self.cost_zone)
            wrong_zone = 'graveyard' if self.cost_zone == 'hand' else 'hand'
            self.add('invalid_zone', self.cost_name, self.owner, wrong_zone)
        else:
            self.add('cost', self.cost_name, self.owner, self.cost_zone)
        if self.id == SCAVENGERS:
            self.add('newer_noncreature', 'Unsummon', owner, 'graveyard')
        if self.id in (*BLACK_SAC, WARLORD, VAROLZ):
            self.add('wrong_type', 'Darksteel Relic', self.owner, 'battlefield')
        if self.id == SLASHER:
            self.add('wrong_type', 'Grizzly Bears', self.owner, 'battlefield')
        if self.owner == 0 and mode == 'paid' and self.id != SCAVENGERS:
            # The real Human chooser can distinguish two same-type legal costs.
            reserve = 'Island' if self.id in DISCARD else "Traveler's Amulet" if self.id == SLASHER else 'Hill Giant'
            self.add('reserve_cost', reserve, self.owner, self.cost_zone)
        damage_spell = 'Lightning Bolt' if self.id == WURM else 'Shock'
        if mode != 'flight':
            removal = {'damage': damage_spell, 'cannot_regenerate': 'Terminate'}.get(mode, 'Murder')
            self.add('removal', removal, self.foe, 'hand')
        if mode == 'damage' and self.id in (HARVESTER, WURM):
            self.add('first_shock', damage_spell, self.foe, 'hand')
        if mode == 'grave_second':
            self.add('second_removal', 'Murder', self.foe, 'hand')

    def emit(self, op, ref='', *, prop='', **values):
        event = dict(op=op, **values)
        if ref:
            event['ref'] = ref
        if prop:
            event['property'] = prop
        self.steps.append(event)

    def add(self, ref, name, player, zone):
        assert ref not in self.cards
        self.setup.append(dict(ref=ref, name=name, player=player, zone=zone))
        self.cards[ref] = name, player, zone
        self.zones[player][zone].append(name)

    def move(self, ref, zone):
        name, player, old = self.cards[ref]
        self.zones[player][old].remove(name)
        self.zones[player][zone].append(name)
        self.cards[ref] = name, player, zone

    def prop(self, ref, field, value):
        self.emit('assert_card', ref, prop=field, value=value)

    def cast(self, ref, target=None):
        name, player, zone = self.cards[ref]
        assert zone == 'hand'
        self.mana(player, cost_from_oracle(self.oracle[name]))
        self.emit('refresh')
        if target:
            self.emit('choose_target', target, player=player)
        self.emit('cast', ref, value=1)
        self.emit('assert_stack', value=1)
        self.prop(ref, 'in_hand', 0)
        self.pools[player] = ZERO
        self.check_mana(player)
        self.emit('assert_unused_choices', value=0)

    def resolve_spell(self, ref, destination='graveyard', *, unused=0):
        self.emit('resolve')
        self.emit('assert_unused_choices', value=unused)
        self.emit('assert_stack', value=0)
        self.emit('refresh')
        self.move(ref, destination)

    def mana(self, player, amounts):
        self.pools[player] = tuple(amounts)
        for color, amount in enumerate(amounts):
            self.emit('set_mana', player=player, value=amount, color=color)

    def check_mana(self, player):
        for color, amount in enumerate(self.pools[player]):
            self.emit('assert_mana', player=player, value=amount, color=color)

    def stats(self, *, tapped, attacking=0, damage=0):
        self.prop('source', 'in_play', 1)
        self.prop('source', 'csvid', self.id)
        for field in ('power', 'toughness'):
            self.prop('source', field, int(self.oracle[self.name][field]))
        self.prop('source', 'tapped', tapped)
        self.prop('source', 'attacking', attacking)
        self.prop('source', 'damage', damage)

    def queue_cost(self):
        if self.id in DISCARD:
            if self.owner == 0:
                self.emit('choose_zone_card', player=self.owner, value=self.rows[self.cost_name]['csvid'])
            else:
                # Production auto-discards the only AI card, then shows its notice.
                self.emit('choose_dialog', player=self.owner, value=0)
        elif self.owner == 0 and self.id != SCAVENGERS:
            self.emit('choose_target', 'cost', player=self.owner)

    def check_cost_paid(self):
        destination = 'exile' if self.id == SCAVENGERS else 'graveyard'
        self.move('cost', destination)
        self.pools[self.owner] = ZERO
        self.check_mana(self.owner)
        self.emit('assert_zone_multiplicity', player=self.owner, zone=destination,
                  index=self.rows[self.cost_name]['csvid'], value=1)

    def receipts(self):
        self.emit('assert_stack', value=0)
        names = sorted({name for name, _, _ in self.cards.values()})
        for player, zones in self.zones.items():
            self.check_mana(player)
            self.emit('assert_life', player=player, value=20)
            for zone, cards in zones.items():
                self.emit('assert_zone', player=player, zone=zone, value=len(cards))
                counts = Counter(cards)
                for name in names:
                    self.emit('assert_zone_multiplicity', player=player, zone=zone,
                              index=self.rows[name]['csvid'], value=counts[name])

    def attack_before_destruction(self):
        self.emit('advance_turn')
        self.emit('untap_step', player=self.foe)
        self.emit('advance_turn')
        self.emit('untap_step', player=self.owner)
        self.emit('turn', player=self.owner)
        self.prop('source', 'sick', 0)
        self.emit('begin_attackers')
        self.emit('attack', 'source', value=1)
        self.emit('finish_attackers')
        self.stats(tapped=1, attacking=1)

    def preemptive(self):
        # Outside destruction the printed activated ability is still legal.
        # Keep the paid activation and following no-response destruction distinct.
        self.emit('refresh')
        self.emit('can_activate', 'source', value=1)
        self.queue_cost()
        self.emit('activate', 'source', value=1)
        self.emit('assert_unused_choices', value=0)
        self.emit('assert_stack', value=1)
        self.check_cost_paid()
        self.emit('resolve')
        self.emit('assert_stack', value=0)
        self.emit('assert_unused_choices', value=0)
        self.emit('refresh')
        self.stats(tapped=0)
        self.cast('removal', 'source')
        self.resolve_spell('removal')
        self.stats(tapped=1)

    def destruction(self):
        accepted = self.mode in ('paid', 'damage', 'combat', 'grave_second')
        plan = self.mode != 'declined'
        if self.mode == 'damage' and self.id in (HARVESTER, WURM):
            self.cast('first_shock', 'source')
            self.resolve_spell('first_shock')
            self.stats(tapped=0, damage=3 if self.id == WURM else 2)
            self.check_mana(self.owner)
        self.cast('removal', 'source')
        if plan:
            self.emit('choose_regeneration', 'source')
        if accepted:
            self.queue_cost()
        self.resolve_spell('removal', unused=int(plan and not accepted))
        if accepted:
            self.check_cost_paid()
            self.stats(tapped=1)
        else:
            self.move('source', 'graveyard')

    def stage_scavenge(self):
        self.scavenged = 'Memnite' if self.mode == 'scavenge_zero' else 'Hill Giant'
        self.add('grave_foil', 'Giant Growth', self.owner, 'graveyard')
        if self.mode != 'scavenge_other_grave':
            self.add('scavenge_cost', self.scavenged, self.owner, 'graveyard')
        self.add('opponent_grave', 'Walking Corpse', self.foe, 'graveyard')
        recipient = self.foe if self.mode == 'scavenge_other_target' else self.owner
        self.add('recipient', 'Grizzly Bears', recipient, 'battlefield')

    def scavenge(self):
        cost = self.oracle[self.scavenged]['mana_cost']
        full = ZERO if cost == '{0}' else tuple(cost_from_oracle(self.oracle[self.scavenged]))
        budget = full
        if self.mode == 'scavenge_wrong_color':
            assert self.scavenged == 'Hill Giant'
            budget = (4, 0, 0, 0, 0, 0)
        if self.mode == 'scavenge_offturn':
            self.emit('turn', player=self.foe)
        self.mana(self.owner, budget)
        self.emit('refresh')
        permitted = self.mode in ('scavenge_paid', 'scavenge_zero', 'scavenge_other_target')
        self.emit('can_activate', 'source', value=int(permitted))
        if permitted:
            if self.owner == 0:
                self.emit('choose_zone_card', player=self.owner, value=self.rows[self.scavenged]['csvid'])
            self.emit('choose_target', 'recipient', player=self.owner)
        self.emit('activate', 'source', value=int(permitted))
        self.emit('assert_unused_choices', value=0)
        self.emit('assert_stack', value=int(permitted))
        counters = 0
        if permitted:
            self.move('scavenge_cost', 'exile')
            self.pools[self.owner] = ZERO
            self.check_mana(self.owner)
            self.emit('assert_zone_multiplicity', player=self.owner, zone='exile',
                      index=self.rows[self.scavenged]['csvid'], value=1)
            self.prop('recipient', 'plus1_counters', 0)
            self.emit('resolve')
            self.emit('refresh')
            counters = int(self.oracle[self.scavenged]['power'])
        self.prop('recipient', 'in_play', 1)
        self.prop('recipient', 'plus1_counters', counters)
        self.prop('recipient', 'power', 2 + counters)
        self.prop('recipient', 'toughness', 2 + counters)
        self.stats(tapped=0)

    def flight(self):
        self.emit('begin_blockers')
        self.emit('block', 'ground', other='source', value=0)
        self.emit('block', 'reach', other='source', value=1)
        self.emit('advance_combat')
        self.emit('choose_regeneration', 'source')
        self.queue_cost()
        self.emit('combat_damage')
        self.emit('assert_unused_choices', value=0)
        self.emit('refresh')
        self.check_cost_paid()
        self.stats(tapped=1)
        self.prop('reach', 'damage', 3)
        self.prop('ground', 'damage', 0)
        self.emit('end_combat')

    def second_grave_cost(self):
        self.cast('second_removal', 'source')
        self.emit('choose_regeneration', 'source')
        self.resolve_spell('second_removal')
        self.move('older_creature', 'exile')
        self.stats(tapped=1)

    def play(self):
        self.emit('turn', player=self.owner)
        self.emit('present_ai_dialogs', value=1)
        for player in (0, 1):
            self.mana(player, ZERO)
        self.cast('source')
        self.resolve_spell('source', 'battlefield')
        self.stats(tapped=0)
        if self.mode in ('combat', 'flight'):
            self.attack_before_destruction()
        if self.id in BLACK_SAC:
            self.mana(self.owner, {'no_mana': ZERO, 'wrong_color': RED}.get(self.mode, BLACK))
        if self.mode == 'proactive':
            self.preemptive()
        elif self.mode == 'flight':
            self.flight()
        elif self.mode.startswith('scavenge_'):
            self.scavenge()
        else:
            self.destruction()
            if self.mode == 'grave_second':
                self.second_grave_cost()
        self.receipts()
        self.emit('present_ai_dialogs', value=0)


def expand(name, csvid, declaration, shape, selected, rows, oracle):
    validate_witnesses(oracle, rows)
    if shape['csvid'] != csvid:
        raise ValueError('regeneration source CSVID changed')
    cases = []
    for variant in selected:
        mode, seat = variant.rsplit('_', 1)
        scene = Scene(rows[name], int(seat), mode, rows, oracle)
        scene.play()
        cases.append(dict(name=f'{LABEL}_{csvid}_{variant}', card=name, fixture='empty_main', seed=SEED,
                          setup=scene.setup, steps=scene.steps,
                          _recipe=dict(family=FAMILY, version=2, variant=variant, params=declaration['params'],
                                       oracle_text=declaration['oracle_text'])))
    return cases
