"""Paid entry and genuine death triggers with exact mill receipts."""

from functools import lru_cache
import json
from pathlib import Path

from death_counter_trigger_recipes import exact
from fight_entry_fighters_recipes import Board
from recipes import cost_from_oracle


FAMILY = 'mill_death_and_entry'
PROFILE = Path(__file__).with_name('data') / 'mill_death_and_entry_wave8_profiles.json'
PREFIX = 'MillDeathEntryRecipe_'

ALTAR, EXCAVATOR, OCCULTIST, CROW = 14215, 10774, 12065, 15775
DENIZEN, SUPPLIER, GLOWSPORE, WALL = 13043, 19017, 19376, 19789
PRINTED = {
    ALTAR: ('khans_of_tarkir.c', '{1}', 'Artifact', None, ('Mill',),
            'Whenever another permanent you control enters, each opponent mills a card.'),
    EXCAVATOR: ('worldwake.c', '{1}{U}', 'Creature — Human Wizard Ally', (1, 3), ('Mill',),
                'Whenever this creature or another Ally you control enters, target player mills X cards, '
                'where X is the number of Allies you control.'),
    OCCULTIST: ('innistrad.c', '{2}{U}', 'Creature — Human Rogue', (2, 3), ('Mill',),
                'Whenever this creature or another creature dies, target player mills a card.'),
    CROW: ('shadows_over_innistrad.c', '{2}{B}', 'Creature — Zombie Bird', (2, 1), ('Flying', 'Mill'),
           'Flying\nWhen this creature enters or dies, mill two cards. '
           '(Put the top two cards of your library into your graveyard.)'),
    DENIZEN: ('gatecrash.c', '{2}{U}', 'Creature — Vedalken Wizard', (2, 3), ('Mill',),
              'Whenever another blue creature you control enters, target player mills two cards.'),
    SUPPLIER: ('m19.c', '{B}', 'Creature — Zombie', (1, 1), ('Mill',),
               'When this creature enters or dies, mill three cards. (Put the top three cards of your library into your graveyard.)'),
    GLOWSPORE: ('guilds_of_ravnica.c', '{B}{G}', 'Creature — Elf Shaman', (3, 1), ('Mill',),
                'When this creature enters, mill three cards. You may put a land card from your graveyard on top of your library. '
                '(To mill a card, put the top card of your library into your graveyard.)'),
    WALL: ('theros.c', '{1}{U}', 'Creature — Wall', (0, 4), ('Mill', 'Defender'),
           "Defender (This creature can't attack.)\nWhen this creature enters, target player mills four cards. "
           '(They put the top four cards of their library into their graveyard.)'),
}
MODES = {
    ALTAR: ('self_only', 'friendly', 'foe', 'short', 'spell_only'),
    EXCAVATOR: ('self_entry', 'own_ally', 'foe_ally', 'nonally', 'short'),
    OCCULTIST: ('self_death', 'own_death', 'foe_death', 'noncreature', 'bounce', 'exile'),
    CROW: ('enter_die', 'short', 'reentry', 'exile', 'flying', 'other_dies'),
    DENIZEN: ('blue', 'multicolor', 'foe_blue', 'nonblue', 'noncreature'),
    SUPPLIER: ('enter_die', 'short', 'reentry', 'exile', 'other_dies'),
    GLOWSPORE: ('recover_milled', 'recover_old', 'decline', 'no_land', 'short', 'grave_shroud'),
    WALL: ('foe', 'self', 'short', 'defender'),
}
SELF_MILL = {CROW: 2, SUPPLIER: 3}
FOLLOW_ONS = {DENIZEN, SUPPLIER, GLOWSPORE, WALL}
ALLY, SECOND_ALLY, OTHER = 'Kor Castigator', 'Joraga Auxiliary', 'Prodigal Sorcerer'
ARTIFACT, DRAW = 'Millstone', 'Reach Through Mists'
KILL, BREAK, BOUNCE, EXILE = 'Murder', 'Naturalize', 'Unsummon', 'Swords to Plowshares'
GROUND, AIR = 'Dazzling Ramparts', 'Drift of Phantasms'
MULTICOLOR, BLUE_ENCHANTMENT, GRAVE_SHROUD = 'Vorel of the Hull Clade', 'Telepathy', 'Silent Gravestone'
ZERO = [0] * 6
MY_LIBRARY = ['Island', 'Forest', 'Plains', 'Mountain', 'Swamp', 'Wastes']
FOE_LIBRARY = ['Swamp', 'Mountain', 'Wastes', 'Plains', 'Forest', 'Island']
WITNESSES = set(MY_LIBRARY) | {
    ALLY, SECOND_ALLY, OTHER, ARTIFACT, DRAW, KILL, BREAK, BOUNCE, EXILE, GROUND, AIR,
    MULTICOLOR, BLUE_ENCHANTMENT, GRAVE_SHROUD,
}


def stats(record):
    return tuple(int(record[key]) for key in ('power', 'toughness'))


class Duel(Board):
    def __init__(self, csvid, record, actor, mode, rows, oracle):
        self.csvid, self.record, self.actor, self.foe, self.mode = csvid, record, actor, 1 - actor, mode
        self.rows, self.oracle = rows, oracle
        self.setup, self.steps, self.cards = [], [], {}
        self.life = [20, 20]
        self.active = actor
        self.library = {actor: MY_LIBRARY.copy(), self.foe: FOE_LIBRARY.copy()}
        self.mill_events = []
        self.victim = None
        self.effect = None
        self.configure_library()
        self.add('source', actor, record['name'], 'hand')
        self.library_refs = {}
        for seat, names in self.library.items():
            self.library_refs[seat] = []
            for index, name in enumerate(names):
                ref = f'library_{seat}_{index}'
                self.add(ref, seat, name, 'library')
                self.library_refs[seat].append(ref)
        self.configure_board()

    def configure_library(self):
        if self.mode == 'short':
            seat, limit = (self.actor, SELF_MILL[self.csvid] + 1) if self.csvid in SELF_MILL else (self.foe, 1)
            self.library[seat] = self.library[seat][:limit]

    def configure_board(self):
        if self.csvid in (ALTAR, EXCAVATOR):
            self.entry_witnesses()
        else:
            self.death_witnesses()

    def entry_witnesses(self):
        if self.csvid == ALTAR:
            if self.mode in ('friendly', 'short', 'foe'):
                who = self.foe if self.mode == 'foe' else self.actor
                self.add('visitor', who, OTHER, 'hand')
                self.add('artifact', who, ARTIFACT, 'hand')
                if self.mode != 'short':
                    self.add('land', who, 'Island', 'hand')
            if self.mode == 'spell_only':
                self.add('visitor', self.actor, DRAW, 'hand')
        else:
            self.add('foe_ally', self.foe, SECOND_ALLY, 'battlefield')
            self.add('not_ally', self.actor, OTHER, 'battlefield')
            if self.mode == 'short':
                self.add('own_ally', self.actor, ALLY, 'battlefield')
            if self.mode in ('own_ally', 'foe_ally', 'nonally'):
                seat = self.foe if self.mode == 'foe_ally' else self.actor
                self.add('visitor', seat, OTHER if self.mode == 'nonally' else ALLY, 'hand')

    def death_witnesses(self):
        if self.mode in ('own_death', 'foe_death', 'other_dies', 'noncreature'):
            who = self.foe if self.mode == 'foe_death' else self.actor
            self.add('victim', who, ARTIFACT if self.mode == 'noncreature' else OTHER, 'hand')
            self.victim = 'victim'
        elif self.mode != 'flying':
            self.victim = 'source'
        if self.mode == 'flying':
            self.add('ground', self.foe, GROUND, 'battlefield')
            self.add('air', self.foe, AIR, 'battlefield')
        if self.victim:
            self.effect = {'bounce': BOUNCE, 'reentry': BOUNCE, 'exile': EXILE, 'noncreature': BREAK}.get(self.mode, KILL)
            self.add('removal', self.foe, self.effect, 'hand')

    def go(self, next_seat):
        assert next_seat == 1 - self.active
        self.emit('advance_turn')
        self.emit('untap_step', player=next_seat)
        self.emit('upkeep_step', player=next_seat)
        self.emit('turn', player=next_seat)
        self.active = next_seat

    def paid(self, ref, *, target=None, rebound_ref=None):
        item = self.cards[ref]
        seat = item['player']
        played_ref = rebound_ref or ref
        assert self.active == seat
        self.mana('set_mana', seat, cost_from_oracle(self.oracle[item['name']]))
        self.emit('refresh')
        if target:
            self.emit('choose_target', target, player=seat)
        self.emit('cast', played_ref, value=1)
        self.emit('assert_unused_choices', value=0)
        self.emit('assert_choices_consumed')
        self.emit('assert_stack', value=1)
        self.observe(played_ref, 'in_hand', 0)
        for player in (0, 1):
            self.mana('assert_mana', player, ZERO)

    def settle(self):
        self.emit('resolve')
        self.emit('assert_unused_choices', value=0)
        self.emit('assert_choices_consumed')
        self.emit('refresh')

    def shift_prefix(self, seat, count, destination):
        taken = self.library_refs[seat][:count]
        cards = self.library[seat][:count]
        del self.library_refs[seat][:count]
        del self.library[seat][:count]
        for ref in taken:
            self.cards[ref]['zone'] = destination
        if destination == 'graveyard':
            self.mill_events.append(dict(player=seat, requested=count, cards=cards))

    def ally_count(self):
        return sum(item['player'] == self.actor and item['zone'] == 'battlefield'
                   and 'Ally' in self.oracle[item['name']]['type_line'].split(' — ')[-1].split()
                   for item in self.cards.values())

    def mill_target(self, target):
        self.emit('choose_player', player=self.actor, value=target)

    def enter(self, ref, *, mill=False, target=None, rebound_ref=None):
        self.paid(ref, rebound_ref=rebound_ref)
        if target is not None:
            self.mill_target(target)
        self.settle()
        self.cards[ref]['zone'] = 'battlefield'
        if mill:
            if self.csvid == EXCAVATOR:
                self.shift_prefix(target, self.ally_count(), 'graveyard')
            elif self.csvid in (DENIZEN, WALL):
                self.shift_prefix(target, 2 if self.csvid == DENIZEN else 4, 'graveyard')
            else:
                seat = self.actor if self.csvid in SELF_MILL else self.foe
                self.shift_prefix(seat, SELF_MILL.get(self.csvid, 1), 'graveyard')
        self.observe_entry(ref, rebound_ref or ref)
        self.receipts()

    def observe_entry(self, ref, live_ref):
        self.observe(live_ref, 'in_play', 1)
        if self.oracle[self.cards[ref]['name']]['type_line'].startswith('Creature'):
            power, toughness = stats(self.oracle[self.cards[ref]['name']])
            self.observe(live_ref, 'power', power)
            self.observe(live_ref, 'toughness', toughness)

    def altar(self):
        if self.mode == 'spell_only':
            self.paid('visitor')
            self.settle()
            self.cards['visitor']['zone'] = 'graveyard'
            self.shift_prefix(self.actor, 1, 'hand')
            self.receipts()
        elif self.mode != 'self_only':
            friendly = self.mode != 'foe'
            if not friendly:
                self.go(self.foe)
            for ref in ('visitor', 'artifact'):
                self.enter(ref, mill=friendly)
            if 'land' in self.cards:
                self.emit('play_land', 'land', value=1)
                self.emit('assert_unused_choices', value=0)
                self.emit('assert_choices_consumed')
                self.cards['land']['zone'] = 'battlefield'
                self.observe('land', 'in_play', 1)
                if friendly:
                    self.shift_prefix(self.foe, 1, 'graveyard')
                self.receipts()

    def excavator(self):
        if self.mode == 'foe_ally':
            self.go(self.foe)
        if 'visitor' in self.cards:
            triggered = self.mode == 'own_ally'
            self.enter('visitor', mill=triggered, target=self.actor if triggered else None)

    def removal(self):
        if self.victim != 'source' and self.cards[self.victim]['player'] == self.actor:
            self.enter(self.victim)
        self.go(self.foe)
        if self.victim != 'source' and self.cards[self.victim]['player'] == self.foe:
            self.enter(self.victim)
        triggers = self.csvid == OCCULTIST and self.effect == KILL
        target = self.actor if self.mode == 'own_death' else self.foe
        self.paid('removal', target=self.victim)
        if triggers:
            self.mill_target(target)
        self.settle()
        item = self.cards[self.victim]
        self.cards['removal']['zone'] = 'graveyard'
        item['zone'] = 'hand' if self.effect == BOUNCE else 'exile' if self.effect == EXILE else 'graveyard'
        if self.effect == EXILE:
            self.life[item['player']] += stats(self.oracle[item['name']])[0]
        if triggers:
            self.shift_prefix(target, 1, 'graveyard')
        if self.csvid in SELF_MILL and self.victim == 'source' and self.effect == KILL:
            self.shift_prefix(self.actor, SELF_MILL[self.csvid], 'graveyard')
        self.observe(self.victim, 'in_play', 0)
        if self.victim != 'source':
            self.observe('source', 'in_play', 1)
        self.receipts()
        if self.mode == 'reentry':
            self.go(self.actor)
            self.emit('bind_hand', 'returned', player=self.actor, value=self.csvid, index=-1)
            self.enter('source', mill=True, rebound_ref='returned')

    def flight(self):
        self.go(self.foe)
        self.go(self.actor)
        self.emit('begin_attackers')
        self.emit('attack', 'source', value=1)
        self.emit('finish_attackers')
        self.emit('begin_blockers')
        self.emit('block', 'ground', 'source', value=0)
        self.emit('block', 'air', 'source', value=1)
        self.emit('advance_combat')
        self.observe('source', 'blocked', 1)
        self.observe('air', 'blocking_count', 1)
        self.emit('combat_damage')
        self.emit('resolve')
        self.emit('end_combat')
        self.observe('source', 'in_play', 1)
        self.observe('air', 'damage', stats(self.record)[0])
        self.receipts()

    def setup_turn(self):
        self.emit('turn', player=self.actor)
        for seat in (0, 1):
            self.mana('set_mana', seat, ZERO)

    def build(self):
        self.setup_turn()
        self.enter('source', mill=self.csvid in SELF_MILL or self.csvid == EXCAVATOR,
                   target=self.foe if self.csvid == EXCAVATOR else None)
        if self.csvid == ALTAR:
            self.altar()
        elif self.csvid == EXCAVATOR:
            self.excavator()
        elif self.mode == 'flying':
            self.flight()
        else:
            self.removal()


class EntryFollowOn(Duel):
    def configure_library(self):
        if self.csvid == GLOWSPORE and self.mode == 'no_land':
            self.library[self.actor] = [OTHER, ARTIFACT, KILL, 'Island', 'Forest', 'Wastes']
        elif self.csvid == GLOWSPORE and self.mode == 'short':
            self.library[self.actor] = ['Island', OTHER]
        else:
            super().configure_library()

    def configure_board(self):
        if self.csvid == DENIZEN:
            visitor = {'multicolor': MULTICOLOR, 'nonblue': ALLY, 'noncreature': BLUE_ENCHANTMENT}.get(self.mode, OTHER)
            self.add('visitor', self.foe if self.mode == 'foe_blue' else self.actor, visitor, 'hand')
        elif self.csvid == GLOWSPORE:
            if self.mode == 'recover_old':
                self.add('old_land', self.actor, 'Wastes', 'graveyard')
            elif self.mode == 'no_land':
                self.add('opposing_land', self.foe, 'Wastes', 'graveyard')
                self.add('nonland_grave', self.actor, DRAW, 'graveyard')
            elif self.mode == 'grave_shroud':
                self.add('seal', self.actor, GRAVE_SHROUD, 'hand')
        elif self.mode == 'defender':
            self.add('attacker', self.actor, OTHER, 'hand')

    def denizen(self):
        self.enter('source')
        if self.mode == 'foe_blue':
            self.go(self.foe)
        triggered = self.mode in ('blue', 'multicolor')
        player = self.actor if self.mode == 'multicolor' else self.foe
        self.enter('visitor', mill=triggered, target=player if triggered else None)

    def glowspore(self):
        if self.mode == 'grave_shroud':
            self.enter('seal')
        land = {
            'recover_old': 'Wastes', 'recover_milled': 'Forest', 'grave_shroud': 'Forest', 'short': 'Island',
        }.get(self.mode)
        self.paid('source')
        if land:
            self.emit('choose_zone_card', player=self.actor, value=self.rows[land]['csvid'])
        elif self.mode == 'decline':
            self.emit('choose_optional_zone', player=self.actor, value=self.rows['Forest']['csvid'])
        self.settle()
        if self.mode == 'decline':
            self.emit('assert_forced_zone_choices', value=0)
        self.cards['source']['zone'] = 'battlefield'
        self.shift_prefix(self.actor, 3, 'graveyard')
        if land:
            ref, = [ref for ref, card in self.cards.items() if card == dict(name=land, player=self.actor, zone='graveyard')]
            self.cards[ref]['zone'] = 'library'
            self.library[self.actor].insert(0, land)
            self.library_refs[self.actor].insert(0, ref)
        self.observe_entry('source', 'source')
        self.receipts()

    def wall(self):
        self.enter('source', mill=True, target=self.actor if self.mode == 'self' else self.foe)
        if self.mode != 'defender':
            return
        self.enter('attacker')
        self.go(self.foe)
        self.go(self.actor)
        self.emit('begin_attackers')
        self.emit('attack', 'source', value=0)
        self.observe('source', 'attacking', 0)
        self.emit('attack', 'attacker', value=1)
        self.emit('finish_attackers')
        self.emit('begin_blockers')
        self.emit('advance_combat')
        self.emit('combat_damage')
        self.emit('resolve')
        self.emit('end_combat')
        self.life[self.foe] -= stats(self.oracle[OTHER])[0]
        self.receipts()

    def build(self):
        self.setup_turn()
        if self.csvid == DENIZEN:
            self.denizen()
        elif self.csvid == GLOWSPORE:
            self.glowspore()
        else:
            self.wall()


@lru_cache(maxsize=1)
def profiles():
    return json.loads(PROFILE.read_bytes())


def classify(record, csvid):
    if (csvid not in PRINTED or not record or record.get('layout') != 'normal'
            or record.get('selected_face') or record.get('card_faces')):
        raise ValueError('whole normal mill entry or death record required')
    _, cost, type_line, printed_stats, keywords, rule = PRINTED[csvid]
    if (record.get('type_line') != type_line or record.get('mana_cost') != cost
            or record.get('keywords') != list(keywords) or record.get('oracle_text') != rule
            or (printed_stats is not None
                and (record.get('power'), record.get('toughness')) != tuple(map(str, printed_stats)))):
        raise ValueError('complete printed mill trigger, filters and riders required')
    return dict(csvid=csvid, mana=cost_from_oracle(record), stats=list(printed_stats) if printed_stats is not None else None)


def variants(shape):
    return [f'{mode}_{seat}' for mode in MODES[shape['csvid']] for seat in (0, 1)]


def require_variants(shape, selected):
    if (type(selected) is not list or not selected or any(type(mode) is not str for mode in selected)
            or len(selected) != len(set(selected)) or set(selected) - set(variants(shape))):
        raise ValueError('distinct reviewed mill entry or death variants required')


def spec(declaration, record):
    primary = profiles()['sources'].get(record.get('name', '')) if record else None
    if not primary or not exact(primary['oracle'], record):
        raise ValueError('exact pinned mill entry or death record required')
    shape = classify(record, primary['registry']['csvid'])
    if (declaration.get('family') != FAMILY or type(declaration.get('version')) is not int
            or declaration['version'] != 2 or declaration.get('oracle_text') != record['oracle_text']
            or not exact(declaration.get('params'), shape)):
        raise ValueError('complete version 2 mill entry or death declaration required')
    require_variants(shape, declaration.get('variants'))
    return dict(shape, family=FAMILY)


def validate_witnesses(oracle, rows):
    pins = profiles()
    if (set(pins) != {'sources', 'witnesses'} or set(pins['witnesses']) != WITNESSES
            or {pin['registry']['csvid'] for pin in pins['sources'].values()} != set(PRINTED)):
        raise ValueError('complete mill entry or death source and witness pins required')
    for name, pin in (pins['sources'] | pins['witnesses']).items():
        row = rows.get(name, {})
        if (not exact(oracle.get(name), pin['oracle']) or not exact(row, pin['registry'])
                or row.get('kind') != 'card' or row.get('handler') in (None, '0', 'NULL', 'card_dummy')
                or oracle[name].get('layout') != 'normal' or oracle[name].get('card_faces')):
            raise ValueError('pinned mill entry or death card changed: ' + name)
        if name in pins['sources']:
            classify(oracle[name], row['csvid'])
    if (not all(oracle[name]['type_line'].endswith(' Ally') for name in (ALLY, SECOND_ALLY))
            or 'Ally' in oracle[OTHER]['type_line'] or oracle[ARTIFACT]['type_line'] != 'Artifact'
            or oracle[OTHER]['colors'] != ['U'] or oracle[MULTICOLOR]['colors'] != ['G', 'U']
            or oracle[BLUE_ENCHANTMENT]['colors'] != ['U'] or oracle[BLUE_ENCHANTMENT]['type_line'] != 'Enchantment'
            or not oracle[GRAVE_SHROUD]['oracle_text'].startswith("Cards in graveyards can't be the targets")
            or oracle[DRAW]['oracle_text'] != 'Draw a card.'
            or oracle[KILL]['oracle_text'] != 'Destroy target creature.'
            or oracle[BREAK]['oracle_text'] != 'Destroy target artifact or enchantment.'
            or oracle[BOUNCE]['oracle_text'] != "Return target creature to its owner's hand."
            or oracle[EXILE]['oracle_text'] != 'Exile target creature. Its controller gains life equal to its power.'
            or stats(oracle[GROUND]) != (0, 7) or 'Flying' in oracle[GROUND]['keywords']
            or stats(oracle[AIR]) != (0, 5) or 'Flying' not in oracle[AIR]['keywords']):
        raise ValueError('reviewed mill entry and death witnesses required')


def expand(name, csvid, declaration, shape, selected, rows, oracle):
    require_variants(shape, selected)
    validate_witnesses(oracle, rows)
    if profiles()['sources'][name]['registry']['csvid'] != csvid or csvid != shape['csvid']:
        raise ValueError('exact registered mill entry or death primary required')
    cases = []
    for variant in selected:
        mode, seat = variant.rsplit('_', 1)
        board_type = EntryFollowOn if csvid in (DENIZEN, GLOWSPORE, WALL) else Duel
        board = board_type(csvid, oracle[name], int(seat), mode, rows, oracle)
        board.build()
        cases.append(dict(name=f'{PREFIX}{csvid}_{variant}', card=name, fixture='empty_main', seed=2771,
                          setup=board.setup, steps=board.steps, _recipe=dict(
                              family=FAMILY, version=2, variant=variant, params=declaration['params'],
                              oracle_text=declaration['oracle_text'])))
    return cases

