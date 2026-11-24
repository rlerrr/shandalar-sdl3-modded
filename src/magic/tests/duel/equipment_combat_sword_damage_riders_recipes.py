"""Real Equipment payments, protection checks and printed combat-damage riders."""

from functools import lru_cache
import json
from pathlib import Path

from death_counter_trigger_recipes import exact
from recipes import cost_from_oracle

FAMILY = 'equipment_combat_sword_damage_riders'
PROFILE = Path(__file__).with_name('data') / f'{FAMILY}_wave8_profiles.json'
BODY, FEAST, FIRE, LIGHT, WAR = (11375, 11536, 6970, 6971, 11695)
WOLF = 18856
SEED = 2660
LABEL = 'EquipmentCombatSwordDamageRidersRecipe'
COMMON = ('connect', 'legal_block', 'protected_blocks', 'noncombat')
MODES = {
    BODY: (*COMMON, 'short_mill'), FEAST: (*COMMON, 'empty_hand'),
    FIRE: COMMON, LIGHT: (*COMMON, 'decline', 'foreign_only', 'ground_seal'),
    WAR: (*COMMON, 'owner_empty', 'victim_empty', 'changed_hands'),
}
PROTECTED = {
    BODY: ('Grizzly Bears', 'Merfolk of the Pearl Trident'),
    FEAST: ('Walking Corpse', 'Grizzly Bears'),
    FIRE: ('Goblin Piker', 'Merfolk of the Pearl Trident'),
    LIGHT: ('Savannah Lions', 'Walking Corpse'),
    WAR: ('Goblin Piker', 'Savannah Lions'),
}
ZA = ('hand', 'battlefield', 'library', 'graveyard', 'exile')
GREEN = 0x08
ZERO = (0, 0, 0, 0, 0, 0)
LIBRARY = ('Forest', 'Plains', 'Island', 'Swamp', 'Mountain', 'Lightning Bolt',
           'Unsummon', 'Grizzly Bears', 'Hill Giant', 'Walking Corpse', 'Savannah Lions', 'Goblin Piker')


@lru_cache(maxsize=1)
def pins():
    return json.loads(PROFILE.read_bytes())


def classify(record, csvid):
    pin = pins()['sources'].get(record.get('name'))
    if (csvid not in MODES or not pin or pin['registry']['csvid'] != csvid
            or not exact(record, pin['oracle']) or record.get('layout') != 'normal'
            or record.get('type_line') != 'Artifact — Equipment'
            or not record['oracle_text'].splitlines()[-1].startswith('Equip {2}')):
        raise ValueError('complete normal Equipment Sword record required')
    return dict(csvid=csvid, cost=cost_from_oracle(record), equip=[2, 0, 0, 0, 0, 0])


def variants(shape):
    return [f'{mode}_{seat}' for mode in MODES[shape['csvid']] for seat in (0, 1)]


def validate_variants(shape, selected):
    if (type(selected) is not list or not selected or any(type(v) is not str for v in selected)
            or len(set(selected)) != len(selected) or set(selected) - set(variants(shape))):
        raise ValueError('explicit distinct supported Sword variants required')


def spec(declaration, record):
    pin = pins()['sources'].get(record.get('name'))
    if not pin:
        raise ValueError('pinned Equipment Sword source required')
    shape = classify(record, pin['registry']['csvid'])
    if (declaration.get('family') != FAMILY or type(declaration.get('version')) is not int
            or declaration['version'] != 2 or declaration.get('oracle_text') != record['oracle_text']
            or not exact(declaration.get('params'), shape)):
        raise ValueError('whole versioned Equipment Sword declaration required')
    validate_variants(shape, declaration.get('variants'))
    return dict(shape, family=FAMILY)


def validate_witnesses(oracle, rows):
    data = pins()
    for group in ('sources', 'witnesses'):
        for name, pin in data[group].items():
            row = rows.get(name, {})
            if (not exact(oracle.get(name), pin['oracle']) or not exact(row, pin['registry'])
                    or row.get('kind') != 'card' or row.get('handler') in (None, 'NULL', '0')
                    or group == 'sources' and row['handler'] == 'card_dummy'):
                raise ValueError(f'changed Sword fixture identity: {name}')


class SwordScene:
    def __init__(self, source, mode, actor, rows, oracle):
        self.name, self.id = source['name'], source['csvid']
        self.mode, self.owner, self.foe = mode, actor, 1 - actor
        self.rows, self.oracle = rows, oracle
        self.setup, self.steps, self.entities = [], [], {}
        self.life = [20, 20]
        self.zones = {p: {zone: [] for zone in ZA} for p in (0, 1)}
        self.add('source', self.name, actor, 'hand')
        self.host = 'Prodigal Sorcerer' if mode == 'noncombat' else 'Hill Giant'
        self.add('host', self.host, actor)
        self.library(actor, ('Plains', 'Forest', 'Swamp', 'Island'))
        self.library(self.foe, LIBRARY[:3] if mode == 'short_mill' else LIBRARY)
        if mode == 'legal_block':
            self.add('blocker', 'Darksteel Myr', self.foe)
        if mode == 'protected_blocks':
            for index, name in enumerate(PROTECTED[self.id]):
                self.add(f'protected_{index}', name, self.foe)
        self.initial_riders()

    def emit(self, op, ref=None, *, prop=None, **values):
        if ref is not None:
            values['ref'] = ref
        if prop is not None:
            values['property'] = prop
        self.steps.append(dict(op=op, **values))

    def add(self, ref, name, player, zone='battlefield', *, tapped=False):
        item = dict(ref=ref, player=player, zone=zone, name=name)
        if tapped:
            item['state'] = ['tapped']
        self.setup.append(item)
        self.zones[player][zone].append(name)
        if zone in ('hand', 'battlefield'):
            self.entities[ref] = (name, player, zone)

    def library(self, player, names):
        for index, name in enumerate(names):
            self.add(f'library_{player}_{index}', name, player, 'library')

    def transfer(self, name, player, former, dest):
        self.zones[player][former].remove(name)
        self.zones[player][dest].append(name)

    def move(self, ref, zone):
        name, player, former = self.entities[ref]
        self.transfer(name, player, former, zone)
        self.entities[ref] = name, player, zone

    def initial_riders(self):
        if self.id == FEAST:
            self.add('land_a', 'Forest', self.owner, tapped=True)
            self.add('land_b', 'Mountain', self.owner, tapped=True)
            self.add('foe_land', 'Island', self.foe, tapped=True)
            self.add('keep', 'Swamp', self.owner, 'hand')
            if self.mode != 'empty_hand':
                self.add('discard', 'Plains', self.foe, 'hand')
        elif self.id == FIRE and self.mode == 'protected_blocks':
            self.add('damage_target', 'Wall of Stone', self.foe)
        elif self.id == LIGHT:
            self.add('own_invalid', 'Lightning Bolt', self.owner, 'graveyard')
            self.add('foe_creature', 'Grizzly Bears', self.foe, 'graveyard')
            if self.mode != 'foreign_only':
                self.add('own_creature', 'Hill Giant', self.owner, 'graveyard')
            if self.mode == 'ground_seal':
                self.add('seal', 'Ground Seal', self.owner, 'hand')
        elif self.id == WAR:
            self.war_setup()

    def war_setup(self):
        own = () if self.mode == 'owner_empty' else ('Island', 'Swamp')
        enemy = () if self.mode == 'victim_empty' else ('Plains', 'Mountain', 'Grizzly Bears')
        if self.mode == 'changed_hands':
            own = ('Swamp', 'Divination')
            enemy = ('Mountain', 'Lightning Bolt')
            self.add('bolt_sink', 'Wall of Stone', self.owner)
        for player, prefix, cards in ((self.owner, 'kept', own), (self.foe, 'opposing', enemy)):
            for index, name in enumerate(cards):
                self.add(f'{prefix}_{index}', name, player, 'hand')

    def mana(self, who, amounts=ZERO, write=False):
        for color, amount in enumerate(amounts):
            self.emit('set_mana' if write else 'assert_mana', player=who, color=color, value=amount)

    def prop(self, ref, prop, expected):
        self.emit('assert_card', ref, prop=prop, value=expected)

    def exact_state(self):
        self.emit('refresh')
        self.emit('assert_stack', value=0)
        names = {name for zones in self.zones.values() for cards in zones.values() for name in cards}
        if self.id == BODY:
            names.add('Wolf')
        for player, zones in self.zones.items():
            self.mana(player)
            self.emit('assert_life', player=player, value=self.life[player])
            self.emit('assert_shuffle_count', player=player, value=0)
            for zone, cards in zones.items():
                self.emit('assert_zone', player=player, zone=zone, value=len(cards))
                for name in sorted(names):
                    self.emit('assert_zone_multiplicity', player=player, zone=zone,
                              value=cards.count(name), index=self.rows[name]['csvid'])
                if zone == 'library':
                    for index, name in enumerate(cards):
                        self.emit('assert_zone_card', player=player, zone=zone, index=index,
                                  value=self.rows[name]['csvid'])

    def cast(self, ref):
        name, player, zone = self.entities[ref]
        assert player == self.owner and zone == 'hand'
        self.mana(player, cost_from_oracle(self.oracle[name]), True)
        self.emit('refresh')
        self.emit('cast', ref, value=1)
        self.emit('assert_stack', value=1)
        self.mana(player)
        self.emit('require_card', ref, prop='in_hand', value=0)
        self.emit('resolve')
        self.emit('assert_unused_choices', value=0)
        self.emit('assert_stack', value=0)
        self.move(ref, 'battlefield')
        self.prop(ref, 'in_play', 1)

    def equip(self):
        self.mana(self.owner, (2, 0, 0, 0, 0, 0), True)
        self.emit('refresh')
        self.emit('require_can_activate', 'source', value=1)
        self.emit('choose_target', 'host', player=self.owner)
        self.emit('activate', 'source', value=1)
        self.emit('assert_stack', value=1)
        self.mana(self.owner)
        self.emit('resolve')
        self.emit('assert_stack', value=0)
        self.emit('assert_unused_choices', value=0)
        self.emit('refresh')
        for prop in ('power', 'toughness'):
            self.prop('host', prop, int(self.oracle[self.host][prop]) + 2)

    def draw(self):
        name = self.zones[self.owner]['library'][0]
        self.transfer(name, self.owner, 'library', 'hand')

    def start(self):
        self.emit('turn', player=self.owner)
        self.emit('present_ai_dialogs', value=1)
        for player in (0, 1):
            self.mana(player, write=True)
        self.cast('source')
        if self.mode == 'ground_seal':
            self.cast('seal')
            self.draw()
        self.equip()

    def change_hands(self):
        # Paid Divination consumes a card and draws two. The real opposing
        # Bolt consumes one of the other hand's two cards without killing the
        # unrelated Wall or changing either player's life.
        self.mana(self.owner, cost_from_oracle(self.oracle['Divination']), True)
        self.emit('refresh')
        self.emit('cast', 'kept_1', value=1)
        self.emit('assert_stack', value=1)
        self.mana(self.owner)
        self.emit('announce_spell', 'kept_1')
        self.mana(self.foe, cost_from_oracle(self.oracle['Lightning Bolt']), True)
        self.emit('refresh')
        self.emit('choose_target', 'bolt_sink', player=self.foe)
        self.emit('cast_response', 'opposing_1', value=1)
        self.emit('assert_stack', value=2)
        self.mana(self.foe)
        self.emit('resolve_response')
        self.emit('assert_stack', value=0)
        self.emit('assert_unused_choices', value=0)
        self.move('kept_1', 'graveyard')
        self.move('opposing_1', 'graveyard')
        self.draw()
        self.draw()
        self.prop('bolt_sink', 'damage', 3)

    def prepare_rider(self):
        if self.id == FIRE:
            if self.mode == 'protected_blocks':
                self.emit('choose_target', 'damage_target', player=self.owner)
            else:
                self.emit('choose_player', player=self.owner, value=self.foe)
        elif self.id == FEAST and self.mode != 'empty_hand':
            if self.foe == 0:
                self.emit('discard_policy', value=1)
        elif self.id == LIGHT and self.mode not in ('foreign_only', 'ground_seal'):
            self.emit('observe_search_filters', value=1)
            self.emit('choose_optional_zone' if self.mode == 'decline' else 'choose_zone_card',
                      player=self.owner, value=self.rows['Hill Giant']['csvid'])

    def finish_rider(self):
        if self.id == BODY:
            owner, victim = self.owner, self.foe
            for name in self.zones[victim]['library'][:10]:
                self.transfer(name, victim, 'library', 'graveyard')
            self.zones[owner]['battlefield'].append('Wolf')
            for prop, value in (('token', 1), ('color', GREEN), ('power', 2), ('toughness', 2),
                                ('subtype_Wolf', 1)):
                self.emit('assert_matching_permanents', player=owner, prop=prop, value=value, index=WOLF)
        elif self.id == FEAST and self.mode != 'empty_hand':
            self.move('discard', 'graveyard')
            if self.foe == 0:
                self.emit('assert_discard_choices', value=1)
        elif self.id == FIRE:
            if self.mode == 'protected_blocks':
                self.prop('damage_target', 'damage', 2)
            else:
                self.life[self.foe] -= 2
            self.draw()
        elif self.id == LIGHT:
            self.life[self.owner] += 3
            if self.mode == 'decline':
                self.emit('assert_forced_zone_choices', value=0)
            elif self.mode not in ('foreign_only', 'ground_seal'):
                self.transfer('Hill Giant', self.owner, 'graveyard', 'hand')
        elif self.id == WAR:
            self.life[self.foe] -= len(self.zones[self.foe]['hand'])
            self.life[self.owner] += len(self.zones[self.owner]['hand'])

    def combat(self):
        self.emit('begin_attackers')
        self.emit('attack', 'host', value=1)
        self.emit('finish_attackers')
        self.emit('begin_blockers')
        legal_block = self.mode == 'legal_block'
        if legal_block:
            self.emit('block', 'blocker', other='host', value=1)
        elif self.mode == 'protected_blocks':
            for index in range(2):
                self.emit('block', f'protected_{index}', other='host', value=0)
        self.emit('advance_combat')
        self.prop('host', 'blocked', int(legal_block))
        if not legal_block:
            self.prepare_rider()
        self.emit('combat_damage')
        self.emit('resolve')
        self.emit('assert_unused_choices', value=0)
        if legal_block:
            self.prop('blocker', 'damage', 5)
        else:
            self.life[self.foe] -= 5
            self.finish_rider()
        self.emit('end_combat')
        if self.id == FEAST:
            for ref in ('land_a', 'land_b', 'foe_land'):
                self.prop(ref, 'tapped', int(legal_block or ref == 'foe_land'))
        self.prop('host', 'tapped', 1)

    def noncombat(self):
        self.emit('require_can_activate', 'host', value=1)
        self.emit('choose_player', player=self.owner, value=self.foe)
        self.emit('activate', 'host', value=1)
        self.emit('assert_stack', value=1)
        self.emit('resolve')
        self.emit('assert_unused_choices', value=0)
        self.prop('host', 'tapped', 1)
        self.life[self.foe] -= 1
        if self.id == FEAST:
            for ref in ('land_a', 'land_b', 'foe_land'):
                self.prop(ref, 'tapped', 1)

    def play(self):
        self.start()
        if self.id == WAR:
            if self.mode == 'changed_hands':
                self.change_hands()
            self.exact_state()
        if self.id == FEAST:
            for ref in ('land_a', 'land_b', 'foe_land'):
                self.prop(ref, 'tapped', 1)
        if self.mode == 'noncombat':
            self.noncombat()
        else:
            self.combat()
        self.exact_state()
        self.emit('present_ai_dialogs', value=0)


def expand(name, csvid, declaration, shape, selected, rows, oracle):
    validate_witnesses(oracle, rows)
    validate_variants(shape, selected)
    if pins()['sources'][name]['registry']['csvid'] != csvid:
        raise ValueError('exact registered Equipment Sword CSVID required')
    cases = []
    for variant in selected:
        mode, seat = variant.rsplit('_', 1)
        scene = SwordScene(rows[name], mode, int(seat), rows, oracle)
        scene.play()
        cases.append(dict(name=f'{LABEL}_{csvid}_{variant}', card=name, fixture='empty_main', seed=SEED,
                          setup=scene.setup, steps=scene.steps,
                          _recipe=dict(family=FAMILY, version=2, variant=variant, params=declaration['params'],
                                       oracle_text=declaration['oracle_text'], untested_components=[])))
    return cases
