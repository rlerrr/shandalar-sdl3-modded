"""Real equipment activation and damage-bound board effects."""

from collections import Counter
from functools import lru_cache
import json
from pathlib import Path

from death_counter_trigger_recipes import exact
from recipes import cost_from_oracle, step


FAMILY = 'equipment_combat_damage_board_effects'
PROFILE = Path(__file__).with_name('data') / 'equipment_combat_damage_board_effects_wave8_profiles.json'
PREFIX = 'EquipmentDamageBoardRecipe_'
ZONES = ('hand', 'battlefield', 'library', 'graveyard', 'exile')

HAMMER, NEKO, SPIKE, SWORD = 10775, 7522, 10036, 6796
WORLD = 6839
WORLD_NAME = 'Worldslayer'
EQUIP_REMINDER = ' ({4}: Attach to target creature you control. Equip only as a sorcery.)'
PRINTED = {
    HAMMER: ('worldwake.c', 2, 2, 2, 0, 'Artifact — Equipment',
             'Equipped creature gets +2/+0.\nWhenever equipped creature deals combat damage to a player, '
             'you may destroy target Equipment that player controls.\nEquip {2}'),
    NEKO: ('betrayers_of_kamigawa.c', 3, 2, 0, 0, 'Artifact — Equipment',
           "Whenever equipped creature deals damage to a creature, tap that creature. That creature doesn't "
           "untap during its controller's untap step for as long as this Equipment remains on the battlefield.\n"
           'Whenever equipped creature deals damage to a player, that player loses 1 life.\nEquip {2}'),
    SPIKE: ('shards_of_alara.c', 3, 3, 0, 0, 'Artifact — Equipment',
            'Equipped creature has deathtouch.\nWhenever equipped creature deals combat damage to a player, '
            'that player loses half their life, rounded up.\nEquip {3}'),
    SWORD: ('mirrodin.c', 4, 4, 5, 5, 'Legendary Artifact — Equipment',
            'Equipped creature gets +5/+5.\nWhenever equipped creature deals damage to a creature, exile that '
            "creature. (Exile it only if it's still on the battlefield.)\nEquip {4}" + EQUIP_REMINDER),
    WORLD: ('m12.c', 5, 5, 0, 0, 'Artifact — Equipment',
            'Whenever equipped creature deals combat damage to a player, destroy all permanents other '
            'than this Equipment.\nEquip {5} ({5}: Attach to target creature you control. Equip only as a sorcery.)'),
}
MODES = {
    HAMMER: ('destroy', 'decline', 'noncombat', 'blocked', 'unattached'),
    NEKO: ('combat_player', 'noncombat_player', 'combat_lock', 'reequip_lock', 'remove_unlock'),
    SPIKE: ('odd', 'even', 'noncombat', 'deathtouch', 'bare_block'),
    SWORD: ('blocked_exile', 'noncombat_exile', 'lethal_dies', 'face', 'bare_ping'),
    WORLD: ('hit', 'indestructible', 'blocked', 'noncombat', 'unattached'),
}
HOST = 'Prodigal Sorcerer'
WALL = 'Dazzling Ramparts'
FOE_GEAR = 'Bonesplitter'
OWN_GEAR = 'Leonin Scimitar'
REPLACEMENT = 'Merfolk Mesmerist'
REMOVAL = 'Naturalize'
ARTIFACT = 'Millstone'
INDESTRUCTIBLE = 'Darksteel Myr'
WITNESSES = {
    HOST, WALL, FOE_GEAR, OWN_GEAR, REPLACEMENT, REMOVAL, ARTIFACT, INDESTRUCTIBLE,
    'Forest', 'Island', 'Mountain', 'Plains',
}
ZERO = [0] * 6


def stats(record):
    return int(record['power']), int(record['toughness'])


class Board:
    def __init__(self, record, actor, rows, oracle):
        self.actor, self.foe, self.rows, self.oracle = actor, 1 - actor, rows, oracle
        self.setup, self.steps, self.cards = [], [], {}
        self.life = [20, 20]
        self.library = {actor: ['Mountain', 'Plains', 'Island'], self.foe: ['Plains', 'Island']}
        self.add('source', actor, record['name'], 'hand')

    def add_libraries(self):
        for player, names in self.library.items():
            for index, name in enumerate(names):
                self.add(f'library_{player}_{index}', player, name, 'library')

    def add(self, ref, player, name, zone, *, tapped=False):
        fields = dict(ref=ref, player=player, name=name, zone=zone)
        if tapped:
            fields['state'] = ['tapped']
        self.setup.append(fields)
        self.cards[ref] = dict(name=name, player=player, zone=zone)

    def emit(self, op, ref='', other='', prop='', **fields):
        if ref:
            fields['ref'] = ref
        if other:
            fields['other'] = other
        if prop:
            fields['property'] = prop
        self.steps.append(step(op, **fields))

    def observe(self, ref, prop, value):
        self.emit('assert_card', ref, prop=prop, value=value)

    def mana(self, op, player, pool):
        for color, value in enumerate(pool):
            self.emit(op, player=player, color=color, value=value)

    def receipts(self):
        self.emit('assert_stack', value=0)
        names = sorted({card['name'] for card in self.cards.values()})
        for player in (0, 1):
            self.emit('assert_life', player=player, value=self.life[player])
            for zone in ZONES:
                contents = self.library[player] if zone == 'library' else [
                    card['name'] for card in self.cards.values() if card['player'] == player and card['zone'] == zone
                ]
                self.emit('assert_zone', player=player, zone=zone, value=len(contents))
                occurrences = Counter(contents)
                for name in names:
                    self.emit('assert_zone_multiplicity', player=player, zone=zone,
                              index=self.rows[name]['csvid'], value=occurrences[name])
                if zone == 'library':
                    for index, name in enumerate(contents):
                        self.emit('assert_zone_card', player=player, zone=zone, index=index,
                                  value=self.rows[name]['csvid'])

    def cast_paid(self, ref, cost):
        self.mana('set_mana', self.actor, cost)
        self.emit('refresh')
        self.emit('cast', ref, value=1)
        self.emit('assert_choices_consumed')
        self.emit('assert_stack', value=1)
        self.mana('assert_mana', self.actor, ZERO)
        self.emit('resolve')
        self.emit('assert_choices_consumed')
        self.emit('refresh')
        self.cards[ref]['zone'] = 'battlefield'


class Duel(Board):
    def __init__(self, csvid, record, owner, mode, rows, oracle):
        self.csvid, self.record, self.actor, self.foe, self.mode = csvid, record, owner, 1 - owner, mode
        self.rows, self.oracle = rows, oracle
        super().__init__(record, owner, rows, oracle)
        if mode == 'even':
            self.life[self.foe] = 21
        self.bearer = None
        self.victim = None
        self.damage = 0
        self.locked = False
        self.add('host', owner, HOST, 'battlefield')
        self.add_libraries()
        if csvid == HAMMER:
            self.add('gear', self.foe, FOE_GEAR, 'battlefield')
            self.add('own_gear', owner, OWN_GEAR, 'battlefield')
            self.add('artifact', self.foe, ARTIFACT, 'battlefield')
        if mode == 'reequip_lock':
            self.add('replacement', owner, REPLACEMENT, 'battlefield')
        if mode in ('reequip_lock', 'combat_lock', 'remove_unlock'):
            self.add('untap_control', self.foe, ARTIFACT, 'battlefield', tapped=True)
        if mode == 'remove_unlock':
            self.add('removal', self.foe, REMOVAL, 'hand')
        if mode in ('blocked', 'combat_lock', 'reequip_lock', 'remove_unlock', 'deathtouch', 'bare_block',
                    'blocked_exile', 'noncombat_exile', 'lethal_dies', 'bare_ping'):
            self.victim = HOST if mode == 'lethal_dies' else WALL
            self.add('victim', self.foe, self.victim, 'battlefield')
        if csvid == WORLD:
            self.world_board()

    def world_board(self):
        self.add('other_copy', self.foe, WORLD_NAME, 'battlefield')
        self.add('own_artifact', self.actor, ARTIFACT, 'battlefield')
        self.add('foe_gear', self.foe, FOE_GEAR, 'battlefield')
        # Tapped lands are ordinary destructible permanents and cannot offer
        # an extra mana-payment route during the measured cast and equip.
        self.add('own_land', self.actor, 'Forest', 'battlefield', tapped=True)
        self.add('foe_land', self.foe, 'Plains', 'battlefield', tapped=True)
        if self.mode == 'indestructible':
            self.add('own_survivor', self.actor, INDESTRUCTIBLE, 'battlefield')
            self.add('foe_survivor', self.foe, INDESTRUCTIBLE, 'battlefield')

    def host_stats(self, ref):
        power, toughness = stats(self.oracle[self.cards[ref]['name']])
        if self.bearer == ref:
            _, _, _, added_power, added_toughness, _, _ = PRINTED[self.csvid]
            power += added_power
            toughness += added_toughness
        self.observe(ref, 'power', power)
        self.observe(ref, 'toughness', toughness)

    def equip(self, ref):
        price = PRINTED[self.csvid][2]
        self.mana('set_mana', self.actor, [price, 0, 0, 0, 0, 0])
        self.emit('refresh')
        self.emit('can_activate', 'source', value=1)
        self.emit('choose_target', ref, player=self.actor)
        self.emit('activate', 'source', value=1)
        self.emit('assert_stack', value=1)
        self.emit('assert_choices_consumed')
        self.mana('assert_mana', self.actor, ZERO)
        self.host_stats(ref)
        self.emit('resolve')
        self.emit('refresh')
        self.emit('assert_choices_consumed')
        self.emit('assert_stack', value=0)
        self.bearer = ref
        self.host_stats(ref)

    def hammer_choices(self):
        if self.mode == 'destroy':
            if self.actor == 0:
                self.emit('choose_trigger', 'source', player=self.actor)
            self.emit('choose_target', 'gear', player=self.actor)
        elif self.mode == 'decline' and self.actor == 1:
            # AI trigger processing resolves automatically. Cancel at its real
            # Equipment picker, checking that it offers cancellation.
            self.emit('choose_optional_target', 'gear', player=self.actor)

    def combat(self):
        self.emit('begin_attackers')
        self.emit('attack', 'host', value=1)
        self.emit('finish_attackers')
        self.emit('begin_blockers')
        if self.victim:
            self.emit('block', 'victim', 'host', value=1)
        self.emit('advance_combat')
        self.observe('host', 'blocked', int(bool(self.victim)))
        if self.victim:
            self.observe('victim', 'blocking_count', 1)
        if self.csvid == HAMMER:
            self.hammer_choices()
        self.emit('combat_damage')
        self.emit('resolve')
        self.emit('assert_unused_choices', value=0)
        self.emit('assert_choices_consumed')
        if self.csvid == HAMMER and self.mode == 'decline' and self.actor == 1:
            self.emit('assert_forced_target_choices', value=0)
        self.emit('end_combat')
        power = stats(self.oracle[HOST])[0] + (PRINTED[self.csvid][3] if self.bearer else 0)
        self.expect_damage(power, combat=True)

    def ping(self):
        self.emit('can_activate', 'host', value=1)
        if self.victim:
            self.emit('choose_target', 'victim', player=self.actor)
        else:
            self.emit('choose_player', player=self.actor, value=self.foe)
        self.emit('activate', 'host', value=1)
        self.emit('assert_choices_consumed')
        self.emit('assert_stack', value=1)
        self.observe('host', 'tapped', 1)
        self.emit('resolve')
        self.emit('assert_unused_choices', value=0)
        self.emit('assert_choices_consumed')
        self.emit('refresh')
        self.expect_damage(1, combat=False)

    def expect_damage(self, amount, *, combat):
        attached = self.bearer == 'host'
        if not self.victim:
            self.life[self.foe] -= amount
            if attached and self.csvid == NEKO:
                self.life[self.foe] -= 1
            if combat and attached and self.csvid == SPIKE:
                self.life[self.foe] -= (self.life[self.foe] + 1) // 2
            if combat and attached and self.csvid == HAMMER and self.mode == 'destroy':
                self.cards['gear']['zone'] = 'graveyard'
            if combat and attached and self.csvid == WORLD:
                self.world_destruction()
            return
        lethal = amount >= stats(self.oracle[self.victim])[1]
        lethal |= attached and self.csvid == SPIKE and amount > 0
        if lethal:
            self.cards['victim']['zone'] = 'graveyard'
        elif attached and self.csvid == SWORD:
            self.cards['victim']['zone'] = 'exile'
        else:
            self.damage = amount
            self.locked = attached and self.csvid == NEKO

    def world_destruction(self):
        for ref, card in self.cards.items():
            if ref == 'source' or card['zone'] != 'battlefield':
                continue
            if 'Indestructible' not in self.oracle[card['name']]['keywords']:
                card['zone'] = 'graveyard'
        if self.cards['host']['zone'] != 'battlefield':
            self.bearer = None

    def verify_damage_state(self):
        self.observe('source', 'in_play', int(self.cards['source']['zone'] == 'battlefield'))
        host_alive = self.cards['host']['zone'] == 'battlefield'
        self.observe('host', 'in_play', int(host_alive))
        if host_alive:
            self.host_stats('host')
        if 'replacement' in self.cards:
            self.host_stats('replacement')
        if self.victim:
            survived = self.cards['victim']['zone'] == 'battlefield'
            self.observe('victim', 'in_play', int(survived))
            if survived:
                self.observe('victim', 'damage', self.damage)
                self.observe('victim', 'tapped', int(self.locked))
        self.receipts()

    def visit(self, player):
        self.emit('advance_turn')
        self.emit('untap_step', player=player)
        self.emit('upkeep_step', player=player)
        self.emit('turn', player=player)

    def lock_lifetime(self):
        self.observe('untap_control', 'tapped', 1)
        if self.mode == 'reequip_lock':
            self.equip('replacement')
        self.visit(self.foe)
        self.damage = 0
        self.observe('untap_control', 'tapped', 0)
        self.verify_damage_state()
        if self.mode == 'remove_unlock':
            cost = cost_from_oracle(self.oracle[REMOVAL])
            self.mana('set_mana', self.foe, cost)
            self.emit('refresh')
            self.emit('choose_target', 'source', player=self.foe)
            self.emit('cast', 'removal', value=1)
            self.emit('assert_choices_consumed')
            self.emit('assert_stack', value=1)
            self.mana('assert_mana', self.foe, ZERO)
            self.emit('resolve')
            self.emit('refresh')
            self.emit('assert_choices_consumed')
            self.cards['source']['zone'] = self.cards['removal']['zone'] = 'graveyard'
            self.bearer = None
            # The creature is still tapped before its next own untap.
            self.verify_damage_state()
            self.visit(self.actor)
            self.visit(self.foe)
            self.locked = False
            self.verify_damage_state()

    def build(self):
        self.emit('turn', player=self.actor)
        for player in (0, 1):
            self.mana('set_mana', player, ZERO)
            self.emit('set_life', player=player, value=self.life[player])
        self.cast_paid('source', cost_from_oracle(self.record))
        self.observe('source', 'in_play', 1)
        self.host_stats('host')
        if self.mode not in ('unattached', 'bare_block', 'bare_ping'):
            self.equip('host')
        if self.mode in ('noncombat', 'noncombat_player', 'reequip_lock', 'remove_unlock',
                         'noncombat_exile', 'lethal_dies', 'bare_ping'):
            self.ping()
        else:
            self.combat()
        self.verify_damage_state()
        if self.mode in ('combat_lock', 'reequip_lock', 'remove_unlock'):
            self.lock_lifetime()
        for player in (0, 1):
            self.mana('assert_mana', player, ZERO)


@lru_cache(maxsize=1)
def profiles():
    return json.loads(PROFILE.read_bytes())


def classify(record, csvid):
    if (csvid not in PRINTED or not record or record.get('layout') != 'normal'
            or record.get('selected_face') or record.get('card_faces')):
        raise ValueError('whole normal Equipment damage primary required')
    _, cast, equip, power, toughness, type_line, rules = PRINTED[csvid]
    if (record.get('type_line') != type_line or record.get('mana_cost') != f'{{{cast}}}'
            or record.get('keywords') != ['Equip'] or record.get('oracle_text') != rules):
        raise ValueError('complete Equipment damage rules, printed cast and equip fees required')
    return dict(csvid=csvid, mana=cost_from_oracle(record), equip=equip, bonus=[power, toughness])


def variants(shape):
    return [f'{mode}_{seat}' for mode in MODES[shape['csvid']] for seat in (0, 1)]


def require_variants(shape, selected):
    if (type(selected) is not list or not selected or any(type(mode) is not str for mode in selected)
            or len(selected) != len(set(selected)) or set(selected) - set(variants(shape))):
        raise ValueError('distinct reviewed Equipment damage variants required')


def spec(declaration, record):
    primary = profiles()['sources'].get(record.get('name', '')) if record else None
    if not primary or not exact(primary['oracle'], record):
        raise ValueError('exact complete pinned Equipment damage record required')
    shape = classify(record, primary['registry']['csvid'])
    if (declaration.get('family') != FAMILY or type(declaration.get('version')) is not int
            or declaration['version'] != 2 or declaration.get('oracle_text') != record['oracle_text']
            or not exact(declaration.get('params'), shape)):
        raise ValueError('complete version 2 Equipment damage declaration required')
    require_variants(shape, declaration.get('variants'))
    return dict(shape, family=FAMILY)


def validate_witnesses(oracle, rows):
    pins = profiles()
    if (set(pins) != {'sources', 'witnesses'} or set(pins['witnesses']) != WITNESSES
            or {pin['registry']['csvid'] for pin in pins['sources'].values()} != set(PRINTED)):
        raise ValueError('complete Equipment damage source and witness pins required')
    for name, pin in (pins['sources'] | pins['witnesses']).items():
        row = rows.get(name, {})
        if (not exact(oracle.get(name), pin['oracle']) or not exact(row, pin['registry'])
                or row.get('kind') != 'card' or row.get('handler') in (None, '0', 'NULL', 'card_dummy')
                or oracle[name].get('layout') != 'normal'):
            raise ValueError('pinned Equipment damage card changed: ' + name)
        if name in pins['sources']:
            classify(oracle[name], row['csvid'])
    if (stats(oracle[HOST]) != (1, 1)
            or oracle[HOST]['oracle_text'] != '{T}: This creature deals 1 damage to any target.'
            or stats(oracle[WALL]) != (0, 7) or oracle[WALL]['keywords'] != ['Defender']
            or stats(oracle[REPLACEMENT]) != (1, 2)
            or not all(oracle[name]['type_line'] == 'Artifact — Equipment' for name in (FOE_GEAR, OWN_GEAR))
            or oracle[ARTIFACT]['type_line'] != 'Artifact'
            or oracle[REMOVAL]['oracle_text'] != 'Destroy target artifact or enchantment.'
            or oracle[INDESTRUCTIBLE]['keywords'] != ['Indestructible']):
        raise ValueError('reviewed Equipment damage and destruction witnesses required')


def expand(name, csvid, declaration, shape, selected, rows, oracle):
    require_variants(shape, selected)
    validate_witnesses(oracle, rows)
    if profiles()['sources'][name]['registry']['csvid'] != csvid or csvid != shape['csvid']:
        raise ValueError('exact registered Equipment damage primary required')
    cases = []
    for variant in selected:
        mode, seat = variant.rsplit('_', 1)
        board = Duel(csvid, oracle[name], int(seat), mode, rows, oracle)
        board.build()
        cases.append(dict(name=f'{PREFIX}{csvid}_{variant}', card=name, fixture='empty_main', seed=2660,
                          setup=board.setup, steps=board.steps, _recipe=dict(
                              family=FAMILY, version=2, variant=variant, params=declaration['params'],
                              oracle_text=declaration['oracle_text'])))
    return cases


