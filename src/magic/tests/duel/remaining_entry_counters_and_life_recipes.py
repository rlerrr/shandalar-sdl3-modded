"""Paid entry, life, creature counters and printed riders."""
from collections import Counter
from functools import lru_cache
import json
from pathlib import Path

from counter_condition_recurrence_recipes import exact
from recipes import card, cost_from_oracle, step

FAMILY = 'remaining_entry_counters_and_life'
PROFILE = Path(__file__).with_name('data') / 'remaining_entry_counters_and_life_wave8_profiles.json'
HUNTER, EEL, BLOODWITCH, SHAPE, THRAGTUSK, PACK_SHAMAN = 8027, 933, 10603, 8441, 12628, 15216
WALL, CHANNELER, BELCHER, SOULSTINGER, GLORIFIER = 15715, 16867, 16993, 17029, 18280
VAMPIRE, ELF, BEAST = 'Vampire Interloper', 'Elvish Mystic', 'Beast'
SWORDS, UNSUMMON, FOREST = 'Swords to Plowshares', 'Unsummon', 'Forest'
LARGE, ZOMBIE = 'Scaled Wurm', 'Walking Corpse'

ARBITER, WURM, MONEY, ARCHANGEL = 9062, 12565, 1799, 14161
MODES = {
    ARBITER: ('foe_high', 'owner_high', 'both_low', 'equal', 'vigilance', 'countered'),
    WURM: ('repair', 'above', 'floor', 'payment_below', 'foe_unprotected', 'removed', 'trample', 'countered'),
    MONEY: ('first', 'second', 'not_phases', 'prevented', 'replay', 'countered'),
    ARCHANGEL: ('below', 'above', 'equal', 'flying', 'countered'),
}
ZONES = {'hand': 0, 'battlefield': 1, 'library': 2, 'graveyard': 3, 'exile': 4}
ZERO = [0] * 6
COUNTER, BOLT, AXE, MURDER = 'Counterspell', 'Lightning Bolt', 'Lava Axe', 'Murder'
PASSAGE, ARMOR, LEECH, BEAR = "Whitesun's Passage", "Urza's Armor", 'Putrid Leech', 'Grizzly Bears'
ZONE_NAMES = {code: name for name, code in ZONES.items()}
WITNESSES = frozenset(('Beast', 'Counterspell', 'Elvish Mystic', 'Forest', 'Grizzly Bears', 'Lava Axe', 'Lightning Bolt', 'Murder', 'Putrid Leech', 'Scaled Wurm', 'Swords to Plowshares', 'Unsummon', "Urza's Armor", 'Vampire Interloper', 'Walking Corpse', "Whitesun's Passage"))


FOLLOWERS = {
    HUNTER: ('hit_self','flying','haunt_own','haunt_foe','haunt_unrelated','countered'),
    EEL: ('entry','pump','wrong_color','prevented','countered'),
    BLOODWITCH: ('alone','scale','flying','protection','countered'),
    SHAPE: ('idle','attacked','blocked','opposing_host','noncombat','countered'),
    THRAGTUSK: ('entry','destroy','bounce','exile','countered'),
    PACK_SHAMAN: ('alone','scale','countered'),
    WALL: ('foreign_only','decline','fresh','mana','countered'),
    CHANNELER: ('sick','other','red','green','empty','countered'),
    BELCHER: ('own_dies','foe_death','other_death','self_death','menace_one','menace_pair','countered'),
    SOULSTINGER: ('entry','death','zero','decline','bounce','countered'),
    GLORIFIER: ('scope','unmatched','countered'),
}
MODES.update(FOLLOWERS)
HUMAN_ONLY = {(WALL,'decline'), (SOULSTINGER,'decline')}
GAPS = {
    MONEY: ('Credit totals are measured via real upkeep and life gain from the printed sacrifice cost.',),
    WALL: ('Manual refusal is exercised in seat 0; the other seat follows its automatic optional decision.',),
    SOULSTINGER: ('Manual refusal is exercised in seat 0; the other seat follows its automatic optional decision.',),
    PACK_SHAMAN: ('The two-seat target is the opponent. Production uses the opponent legality check without a selector.',),
}


@lru_cache(maxsize=1)
def profiles():
    return json.loads(PROFILE.read_bytes())


def classify(record, csvid):
    if (csvid not in MODES or record.get('layout') != 'normal' or record.get('card_faces')
            or record.get('selected_face') or 'enters' not in record['oracle_text'].lower()):
        raise ValueError('normal whole entry-trigger primary required')
    return dict(csvid=csvid, printed_cost=record['mana_cost'])


def variants(shape):
    csvid = shape['csvid']
    return [f'{mode}_{owner}' for owner in (0, 1) for mode in MODES[csvid]
            if owner == 0 or (csvid,mode) not in HUMAN_ONLY]


def validate_variants(shape, chosen):
    if (type(chosen) is not list or not chosen or any(type(item) is not str for item in chosen)
            or len(set(chosen)) != len(chosen) or set(chosen) - set(variants(shape))):
        raise ValueError('distinct supported entry-trigger variants required')


def spec(declaration, record):
    pin = profiles()['sources'].get(record.get('name'))
    if not pin or not exact(pin['oracle'], record):
        raise ValueError('exact pinned entry-trigger primary required')
    shape = classify(record, pin['registry']['csvid'])
    if (declaration.get('family') != FAMILY or type(declaration.get('version')) is not int
            or declaration['version'] != 2 or declaration.get('oracle_text') != record['oracle_text']
            or not exact(declaration.get('params'), shape)):
        raise ValueError('exact typed entry-trigger declaration required')
    validate_variants(shape, declaration.get('variants'))
    return dict(shape, family=FAMILY)


def validate_witnesses(oracle, rows):
    pins = profiles()
    if (set(pins) != {'sources', 'witnesses'} or set(pins['witnesses']) != WITNESSES
            or {pin['registry']['csvid'] for pin in pins['sources'].values()} != MODES.keys()):
        raise ValueError('exact entry-trigger sources and witnesses required')
    for group, entries in pins.items():
        for name, pin in entries.items():
            row = rows.get(name, {})
            layout = 'token' if name == BEAST else 'normal'
            if (not exact(oracle.get(name), pin['oracle']) or not exact(row, pin['registry'])
                    or row.get('kind') != 'card' or row.get('handler') in (None, '0', 'NULL')
                    or oracle[name].get('layout') != layout):
                raise ValueError('entry-trigger Oracle or registry changed: ' + name)
            if group == 'sources':
                if row['handler'] == 'card_dummy':
                    raise ValueError('implemented entry-trigger primary required')
                classify(oracle[name], row['csvid'])


def expand(name, csvid, declaration, shape, chosen, rows, oracle):
    validate_variants(shape, chosen)
    validate_witnesses(oracle, rows)
    if shape['csvid'] != csvid or profiles()['sources'][name]['registry']['csvid'] != csvid:
        raise ValueError('exact entry-trigger primary CSVID required')
    cases = []
    for variant in chosen:
        mode, seat = variant.rsplit('_', 1)
        board = Board(rows[name], int(seat), mode, rows, oracle)
        board.run()
        cases.append(dict(name=f'RemainingEntryCountersAndLifeRecipe_{csvid}_{variant}', card=name,
                          fixture='empty_main', seed=3500, setup=board.setup, steps=board.steps,
                          _recipe=dict(family=FAMILY, version=2, variant=variant, params=declaration['params'],
                                       oracle_text=declaration['oracle_text'], untested_components=list(GAPS.get(csvid, ())))))
    return cases

class Board:
    def __init__(self, record, owner, mode, rows, oracle):
        self.id, self.name = record['csvid'], record['name']
        self.owner, self.foe, self.mode = owner, 1 - owner, mode
        self.rows, self.oracle = rows, oracle
        self.setup, self.steps, self.cards = [], [], {}
        self.life = [20, 20]
        self.wallet = [ZERO.copy(), ZERO.copy()]
        self.add('source', self.name, owner, 'hand')
        if mode == 'countered':
            self.add('counter', COUNTER, self.foe, 'hand')
        self.setup_life()
        self.setup_followers()

    def add(self, ref, name, player, zone='battlefield'):
        self.setup.append(card(name, ref, player, zone))
        self.cards[ref] = dict(name=name, player=player, zone=zone)

    def emit(self, op, ref='', other='', prop='', *, player=0, value=0, zone=0, index=0, color=0):
        if op in ('untap_step','upkeep_step'):
            self.steps.append(step(op, player=player))
            return
        if op in ('end_combat_step','assert_forced_target_choices'):
            self.steps.append(step(op, value=value))
            return
        if op == 'choose_mana_source':
            self.steps.append(step(op, ref=ref))
            return
        if op == 'choose_optional_target':
            self.steps.append(step(op, ref=ref, player=player))
            return
        if op == 'assert_counter':
            self.steps.append(step(op, ref=ref, counter_kind=prop, value=value))
            return
        if op == 'play_land':
            self.steps.append(step(op, ref=ref, value=value))
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

    def stats(self, ref, addition=0):
        for prop in ('power', 'toughness'):
            value = int(self.oracle[self.cards[ref]['name']][prop])
            self.observe(ref, prop, value + addition)

    def count(self, ref, value, prop='plus1'):
        self.emit('assert_counter', ref, prop=prop, value=value)

    def move(self, ref, zone):
        self.cards[ref]['zone'] = zone

    def choices(self):
        self.emit('assert_unused_choices')
        self.emit('assert_choices_consumed')

    def pool(self, player, amount, *, fund=False):
        for color, value in enumerate(amount):
            self.emit('set_mana' if fund else 'assert_mana', player=player, value=value, color=color)
        if fund:
            self.wallet[player] = amount.copy()
            self.emit('refresh')

    def announce(self, ref, *, target=None, target_player=None, response=False):
        player = self.cards[ref]['player']
        cost = cost_from_oracle(self.oracle[self.cards[ref]['name']])
        self.pool(player,cost,fund=True)
        if target is not None:
            self.emit('choose_target',target,player=player)
        if target_player is not None:
            self.emit('choose_player',player=player,value=target_player)
        self.emit('cast_response' if response else 'cast',ref,value=1)
        self.choices()
        self.emit('assert_stack',value=2 if response else 1)
        self.wallet[player] = ZERO.copy()
        self.pool(player,ZERO)

    def resolve(self, *, response=False):
        self.emit('resolve_response' if response else 'resolve')
        self.emit('refresh')
        self.choices()
        self.emit('assert_stack')

    def spell(self, ref, *, target=None, target_player=None, destination='graveyard'):
        self.announce(ref,target=target,target_player=target_player)
        self.resolve()
        self.move(ref,destination)

    def receipt(self):
        self.emit('assert_stack')
        for player in (0, 1):
            self.emit('assert_life', player=player, value=self.life[player])
            self.pool(player, self.wallet[player])
            for zone, code in ZONES.items():
                counts = Counter(item['name'] for item in self.cards.values()
                                 if item['player'] == player and item['zone'] == zone)
                self.emit('assert_zone', player=player, zone=code, value=counts.total())
                for name, count in sorted(counts.items()):
                    self.emit('assert_zone_multiplicity', player=player, zone=code,
                              index=self.rows[name]['csvid'], value=count)
        for ref, item in self.cards.items():
            if item['zone'] == 'battlefield':
                self.observe(ref, 'in_play', 1)

    def visit(self, player):
        assert self.wallet == [ZERO, ZERO]
        self.emit('advance_turn')
        self.emit('untap_step', player=player)
        self.emit('turn', player=player)

    def countered(self):
        self.announce('source', target='host' if self.id == SHAPE else None)
        self.emit('announce_spell', 'source')
        self.announce('counter', response=True)
        self.resolve(response=True)
        self.move('source', 'graveyard')
        self.move('counter', 'graveyard')
        if self.id == SHAPE:
            self.counters_and_stats('host', 0)
        self.receipt()

    def setup_life(self):
        mode = self.mode
        self.life_setup = []
        bolts = ()
        if self.id == ARBITER:
            if mode in ('foe_high', 'both_low', 'countered'):
                bolts += (self.owner,)
            if mode == 'both_low':
                bolts += (self.foe,self.foe)
            if mode == 'owner_high':
                self.add('gain', PASSAGE, self.owner, 'hand')
        if self.id == ARCHANGEL:
            if mode in ('below','countered'):
                bolts += (self.owner,self.foe)
            if mode == 'above':
                self.add('gain', PASSAGE, self.owner, 'hand')
            if mode == 'flying':
                self.add('blocker', BEAR, self.foe)
        if self.id == WURM:
            if mode in ('repair','payment_below','countered'):
                for i in range(3):
                    ref = f'axe_{i}'
                    self.add(ref, AXE, self.owner, 'hand')
                    self.life_setup.append((ref,self.owner,5))
            if mode in ('floor','removed','foe_unprotected'):
                bolts += (self.foe if mode=='foe_unprotected' else self.owner,)*4
                recipients = (self.foe,) if mode=='foe_unprotected' else (self.owner,self.owner) if mode=='floor' else (self.owner,)
            elif mode == 'payment_below':
                recipients = (self.owner,)
            else:
                recipients = ()
            for idx, recipient in enumerate(recipients):
                actor = self.owner if recipient==self.foe else self.foe
                self.add(f'after_{idx}', BOLT, actor, 'hand')
            self.following_bolts = recipients
            if mode == 'payment_below':
                self.add('leech', LEECH, self.owner)
            if mode == 'removed':
                self.add('removal', MURDER, self.foe, 'hand')
            if mode == 'trample':
                self.add('blocker', BEAR, self.foe)
        if self.id == MONEY and mode == 'prevented':
            self.add('armor', ARMOR, self.owner)
        for i, recipient in enumerate(bolts):
            ref = f'bolt_{i}'
            self.add(ref,BOLT,self.owner,'hand')
            self.life_setup.append((ref,recipient,3))

    def life_receipt(self):
        for player in (0,1):
            self.emit('assert_life',player=player,value=self.life[player])

    def burn(self, ref, recipient, damage, *, protected=False):
        self.spell(ref,target_player=recipient)
        remaining = self.life[recipient] - damage
        self.life[recipient] = max(7,remaining) if protected and self.life[recipient]>=7 else remaining
        self.life_receipt()

    def prepare_life(self):
        for ref,recipient,damage in self.life_setup:
            self.burn(ref,recipient,damage)
        if 'gain' in self.cards:
            self.spell('gain')
            self.life[self.owner] += 5
            self.life_receipt()

    def enter_creature(self):
        self.spell('source',destination='battlefield')
        self.stats('source')

    def age(self):
        self.visit(self.foe)
        self.visit(self.owner)

    def combat_life(self, *, flying=False, vigilance=False, trample=False):
        self.age()
        blocker = flying or trample
        self.emit('begin_attackers')
        self.emit('attack','source',value=1)
        self.emit('finish_attackers')
        self.observe('source','tapped',int(not vigilance))
        self.emit('begin_blockers')
        if blocker:
            self.emit('block','blocker','source',value=int(trample))
        self.emit('advance_combat')
        self.observe('source','blocked',int(trample))
        self.emit('combat_damage')
        self.resolve()
        if trample:
            self.move('blocker','graveyard')
            self.observe('source','damage',2)
        self.life[self.foe] -= int(self.oracle[self.name]['power']) - 2*int(trample)
        self.emit('end_combat_step')
        self.resolve()
        if vigilance:
            self.observe('source','tapped',0)
        self.receipt()

    def arbiter(self):
        highest = max(self.life)
        self.enter_creature()
        self.life = [highest,highest]
        self.receipt()
        if self.mode=='vigilance':
            self.combat_life(vigilance=True)

    def elderscale(self):
        self.enter_creature()
        self.life[self.owner] = max(7,self.life[self.owner])
        self.receipt()
        if self.mode=='payment_below':
            self.emit('can_activate','leech',value=1)
            self.emit('activate','leech',value=1)
            self.choices()
            self.emit('assert_stack',value=1)
            self.life[self.owner] -= 2
            self.life_receipt()
            self.resolve()
            self.stats('leech',2)
        if self.mode=='removed':
            self.spell('removal',target='source')
            self.move('source','graveyard')
            self.receipt()
        for i, recipient in enumerate(self.following_bolts):
            protects = recipient==self.owner and self.cards['source']['zone']=='battlefield'
            self.burn(f'after_{i}',recipient,3,protected=protects)
        if self.mode=='trample':
            self.combat_life(trample=True)
        self.receipt()

    def archangel(self):
        self.enter_creature()
        self.life[self.owner] = max(20,self.life[self.owner])
        self.receipt()
        if self.mode=='flying':
            self.combat_life(flying=True)

    def upkeep(self, player):
        assert self.wallet==[ZERO,ZERO]
        self.emit('advance_turn')
        self.emit('untap_step',player=player)
        self.emit('upkeep_step',player=player)
        self.choices()
        self.emit('assert_stack')

    def no_cash(self, *, attempt=True):
        if attempt:
            self.emit('can_activate','source',value=0)
            self.emit('activate','source',value=0)
        self.choices()
        self.receipt()

    def cash(self, credit):
        self.emit('can_activate','source',value=1)
        self.emit('activate','source',value=1)
        self.choices()
        self.emit('assert_stack',value=1)
        self.move('source','graveyard')
        self.emit('assert_zone_multiplicity',player=self.owner,zone=ZONES['graveyard'],index=self.id,value=1)
        self.life_receipt()
        self.resolve()
        self.life[self.owner] += credit
        self.receipt()

    def money(self):
        self.enter_creature()
        self.life[self.owner] -= 2 if self.mode=='prevented' else 3
        self.no_cash()
        self.upkeep(self.foe)
        self.no_cash(attempt=self.mode=='not_phases')
        if self.mode=='not_phases':
            return
        if self.mode=='replay':
            self.emit('capture')
            life, cards = self.life.copy(), {ref:entry.copy() for ref,entry in self.cards.items()}
            self.upkeep(self.owner)
            self.cash(4)
            self.emit('restore')
            self.life,self.cards = life,cards
            self.receipt()
        self.upkeep(self.owner)
        credit = 4
        if self.mode=='second':
            self.emit('turn',player=self.owner)
            self.no_cash()
            self.upkeep(self.foe)
            self.no_cash(attempt=False)
            self.upkeep(self.owner)
            credit += 1
        self.cash(credit)

    def setup_followers(self):
        mode = self.mode
        if self.id == SHAPE:
            self.add('host', BEAR, self.foe if mode == 'opposing_host' else self.owner)
        if mode == 'countered' or self.id not in FOLLOWERS:
            return
        if self.id == HUNTER:
            if mode == 'flying':
                self.add('blocker', BEAR, self.foe)
            if mode in ('haunt_own', 'haunt_foe', 'haunt_unrelated'):
                self.add('haunted', BEAR, self.owner if mode == 'haunt_own' else self.foe)
                self.add('source_removal', MURDER, self.foe, 'hand')
                self.add('victim_removal', MURDER, self.foe, 'hand')
                if mode == 'haunt_unrelated':
                    self.add('other_creature', BEAR, self.owner)
        elif self.id == EEL:
            if mode == 'prevented':
                self.add('armor', ARMOR, self.owner)
        elif self.id in (BLOODWITCH, PACK_SHAMAN):
            member = VAMPIRE if self.id == BLOODWITCH else ELF
            self.add('opposing_member', member, self.foe)
            self.add('blocker', BEAR, self.foe)
            self.add('nonmember', BEAR, self.owner)
            if mode == 'scale':
                self.add('first_member', member, self.owner)
                self.add('second_member', member, self.owner)
                self.add('armor', ARMOR, self.foe)
            if mode == 'protection':
                self.add('white_spell', SWORDS, self.foe, 'hand')
                self.add('black_spell', MURDER, self.foe, 'hand')
        elif self.id == SHAPE:
            if mode == 'blocked':
                self.add('attacker', BEAR, self.foe)
            if mode == 'noncombat':
                self.add('ping', BOLT, self.foe, 'hand')
        elif self.id == THRAGTUSK and mode != 'entry':
            spells = {'destroy': MURDER, 'bounce': UNSUMMON, 'exile': SWORDS}
            self.add('removal', spells[mode], self.foe, 'hand')
        elif self.id == WALL:
            self.add('opposing_land', FOREST, self.foe)
            if mode != 'foreign_only':
                self.add('land', FOREST, self.owner, 'hand' if mode == 'fresh' else 'battlefield')
        elif self.id == CHANNELER:
            self.add('opposing', LARGE, self.foe)
            if mode == 'other':
                self.add('ally', LARGE, self.owner)
        elif self.id == BELCHER:
            self.add('opposing', ZOMBIE, self.foe)
            if mode in ('own_dies', 'other_death'):
                self.add('ally', ZOMBIE, self.owner)
            if mode in ('foe_death', 'other_death', 'self_death'):
                self.add('removal', MURDER, self.foe, 'hand')
            if mode == 'menace_pair':
                self.add('second_blocker', BEAR, self.foe)
        elif self.id == SOULSTINGER:
            self.add('victim', LARGE, self.foe)
            self.add('ally', LARGE, self.owner)
            if mode in ('death', 'zero', 'decline', 'bounce'):
                self.add('removal', UNSUMMON if mode == 'bounce' else MURDER, self.foe, 'hand')
        elif self.id == GLORIFIER:
            self.add('opposing', VAMPIRE, self.foe)
            self.add('nonmember', BEAR, self.owner)
            if mode == 'scope':
                self.add('ally', VAMPIRE, self.owner)

    def entry(self, target=None, *, target_player=None, optional=False):
        self.announce('source', target=target if self.id == SHAPE else None)
        if optional and self.owner == 0:
            self.emit('choose_trigger', 'source', player=self.owner)
        if target and self.id != SHAPE:
            self.emit('choose_target', target, player=self.owner)
        if target_player is not None:
            self.emit('choose_player', player=self.owner, value=target_player)
        self.resolve()
        self.move('source', 'battlefield')

    def remove_creature(self, spell, target, *, destination='graveyard', after=None):
        self.announce(spell, target=target)
        if after is not None:
            after()
        self.resolve()
        self.move(spell, 'graveyard')
        self.move(target, destination)

    def counters_and_stats(self, ref, count, *, negative=False):
        self.count(ref, count, 'minus1' if negative else 'plus1')
        self.stats(ref, -count if negative else count)

    def hunter(self):
        victim = self.owner if self.mode == 'hit_self' else self.foe
        self.entry(target_player=victim)
        self.life[victim] -= 2
        self.life[self.owner] += 2
        self.stats('source')
        self.receipt()
        if self.mode == 'flying':
            self.combat_life(flying=True)
        if 'haunted' not in self.cards:
            return
        self.remove_creature('source_removal', 'source', destination='exile',
            after=lambda: self.emit('choose_target', 'haunted', player=self.owner))
        self.receipt()
        if self.mode == 'haunt_unrelated':
            self.remove_creature('victim_removal', 'other_creature')
        else:
            self.remove_creature('victim_removal', 'haunted',
                after=lambda: self.emit('choose_player', player=self.owner, value=self.foe))
            self.life[self.owner] += 2
            self.life[self.foe] -= 2
        self.receipt()

    def eel(self):
        self.entry()
        damage = int(self.mode != 'prevented')
        self.life[self.owner] -= damage
        self.stats('source')
        self.receipt()
        if self.mode == 'entry':
            return
        pay = self.mode != 'wrong_color'
        fee = [0, 0, 0, 0, 2, 0] if pay else [0, 0, 2, 0, 0, 0]
        self.pool(self.owner, fee, fund=True)
        self.emit('can_activate', 'source', value=int(pay))
        self.emit('activate', 'source', value=int(pay))
        self.choices()
        if pay:
            self.wallet[self.owner] = ZERO.copy()
            self.emit('assert_stack', value=1)
            self.life_receipt()
            self.stats('source')
            self.resolve()
            self.life[self.owner] -= damage
            self.observe('source', 'power', 3)
            self.observe('source', 'toughness', 1)
        self.receipt()
        if self.mode == 'pump':
            self.visit(self.foe)
            self.stats('source')
            self.receipt()

    def tribal_life(self):
        self.entry()
        count = 3 if self.mode == 'scale' else 1
        self.life[self.foe] -= count
        if self.id == BLOODWITCH:
            self.life[self.owner] += count
        self.stats('source')
        self.receipt()
        if self.mode == 'flying':
            self.combat_life(flying=True)
        if self.mode == 'protection':
            self.emit('can_target_creature', 'white_spell', 'source', value=0)
            self.emit('can_target_creature', 'black_spell', 'source', value=1)
            self.receipt()

    def shape_of_the_wiitigo(self):
        self.entry('host')
        self.counters_and_stats('host', 6)
        self.receipt()
        if self.mode == 'attacked':
            self.emit('begin_attackers')
            self.emit('attack', 'host', value=1)
            self.emit('finish_attackers')
            self.emit('begin_blockers')
            self.emit('advance_combat')
            self.emit('combat_damage')
            self.resolve()
            self.life[self.foe] -= 8
            self.emit('end_combat_step')
            self.resolve()
        if self.mode == 'noncombat':
            self.spell('ping', target='host')
            self.observe('host', 'damage', 3)
        self.upkeep(self.foe)
        self.counters_and_stats('host', 6)
        if self.mode == 'blocked':
            self.emit('turn', player=self.foe)
            self.emit('begin_attackers')
            self.emit('attack', 'attacker', value=1)
            self.emit('finish_attackers')
            self.emit('begin_blockers')
            self.emit('block', 'host', 'attacker', value=1)
            self.emit('advance_combat')
            self.observe('attacker', 'blocked', 1)
            self.emit('combat_damage')
            self.resolve()
            self.move('attacker', 'graveyard')
            self.emit('end_combat_step')
            self.resolve()
        self.upkeep(self.owner)
        count = 7 if self.mode in ('attacked', 'blocked') else 5
        self.counters_and_stats('host', count)
        self.receipt()
        if self.mode == 'attacked':
            self.upkeep(self.foe)
            self.upkeep(self.owner)
            self.counters_and_stats('host', 6)
            self.receipt()

    def thragtusk(self):
        self.entry()
        self.life[self.owner] += 5
        self.stats('source')
        self.emit('assert_zone_multiplicity', player=self.owner, zone=ZONES['battlefield'],
                  index=self.rows[BEAST]['csvid'], value=0)
        self.receipt()
        if self.mode == 'entry':
            return
        destination = dict(destroy='graveyard', bounce='hand', exile='exile')[self.mode]
        self.remove_creature('removal', 'source', destination=destination)
        if self.mode == 'exile':
            self.life[self.owner] += 5
        self.emit('bind_permanent_copy', 'beast', player=self.owner, value=self.rows[BEAST]['csvid'])
        self.cards['beast'] = dict(name=BEAST, player=self.owner, zone='battlefield')
        self.observe('beast', 'power', 3)
        self.observe('beast', 'toughness', 3)
        for prop, value in (('token', 1), ('color', 8), ('subtype_Beast', 1)):
            self.emit('assert_matching_permanents', prop=prop, player=self.owner,
                      index=self.rows[BEAST]['csvid'], value=value)
        self.receipt()

    def wall(self):
        if self.mode == 'fresh':
            self.emit('play_land', 'land', value=1)
            self.move('land', 'battlefield')
        selected = self.mode in ('fresh', 'mana')
        self.entry('land' if selected else None, optional=selected)
        self.stats('source')
        self.count('opposing_land', 0)
        if 'land' in self.cards:
            self.count('land', 3 if selected else 0)
        if selected:
            self.observe('land', 'power', 3)
            self.observe('land', 'toughness', 3)
            self.emit('assert_matching_permanents', prop='subtype_Elemental', player=self.owner,
                      index=self.rows[FOREST]['csvid'], value=1)
        self.receipt()
        if self.mode == 'mana':
            self.age()
            self.emit('can_activate', 'land', value=1)
            self.emit('activate', 'land', value=1)
            self.choices()
            self.wallet[self.owner][3] += 1
            self.observe('land', 'tapped', 1)
            self.receipt()
        if self.mode == 'fresh':
            self.emit('begin_attackers')
            self.emit('attack', 'land', value=1)
            self.emit('finish_attackers')
            self.emit('begin_blockers')
            self.emit('advance_combat')
            self.emit('combat_damage')
            self.resolve()
            self.life[self.foe] -= 3
            self.emit('end_combat_step')
            self.resolve()
            self.receipt()
        if self.mode == 'foreign_only':
            self.age()
            self.emit('begin_attackers')
            self.emit('attack', 'source', value=0)
            self.emit('finish_attackers')
            self.emit('begin_blockers')
            self.emit('advance_combat')
            self.emit('combat_damage')
            self.resolve()
            self.emit('end_combat_step')
            self.resolve()
            self.receipt()

    def channeler(self):
        target = 'ally' if self.mode == 'other' else 'source'
        self.entry(target)
        self.counters_and_stats(target, 3, negative=True)
        self.counters_and_stats('opposing', 0, negative=True)
        self.emit('can_activate', 'source', value=0)
        self.receipt()
        if self.mode == 'sick':
            return
        self.age()
        if self.mode == 'other':
            self.counters_and_stats('source', 0, negative=True)
            self.emit('can_activate', 'source', value=0)
            self.receipt()
            return
        color = 4 if self.mode == 'red' else 3
        cycles = 3 if self.mode == 'empty' else 1
        for count in range(3, 3 - cycles, -1):
            self.emit('can_activate', 'source', value=1)
            self.emit('choose_color', player=self.owner, value=color)
            self.emit('activate', 'source', value=1)
            self.choices()
            self.emit('assert_stack')
            self.observe('source', 'tapped', 1)
            self.counters_and_stats('source', count - 1, negative=True)
            self.wallet[self.owner][color] += 1
            self.receipt()
            self.emit('can_activate', 'source', value=0)
            if cycles > 1:
                self.pool(self.owner, ZERO, fund=True)
                self.age()
        if cycles > 1:
            self.emit('can_activate', 'source', value=0)
            self.receipt()

    def belcher(self):
        target = 'ally' if self.mode == 'own_dies' else 'source'
        self.entry(target)
        if self.mode == 'own_dies':
            self.move('ally', 'graveyard')
            self.life[self.foe] -= 1
            self.counters_and_stats('source', 0, negative=True)
        else:
            self.counters_and_stats('source', 2, negative=True)
        self.counters_and_stats('opposing', 0, negative=True)
        self.receipt()
        victim = {'foe_death':'opposing', 'other_death':'ally', 'self_death':'source'}.get(self.mode)
        if victim:
            self.remove_creature('removal', victim)
            self.life[self.foe] -= int(self.mode == 'other_death')
            self.receipt()
        if self.mode in ('menace_one', 'menace_pair'):
            self.age()
            self.emit('begin_attackers')
            self.emit('attack', 'source', value=1)
            self.emit('finish_attackers')
            self.emit('begin_blockers')
            pair = self.mode == 'menace_pair'
            self.emit('block', 'opposing', 'source', value=int(pair))
            if pair:
                self.emit('block', 'second_blocker', 'source', value=1)
            self.emit('advance_combat')
            self.observe('source', 'blocked', int(pair))
            if not pair:
                self.emit('combat_damage')
                self.resolve()
                self.life[self.foe] -= 3
                self.emit('end_combat_step')
                self.resolve()
            self.receipt()

    def soulstinger(self):
        target = 'ally' if self.mode == 'zero' else 'source'
        self.entry(target)
        self.counters_and_stats('source', 2 if target == 'source' else 0, negative=True)
        self.counters_and_stats('ally', 2 if target == 'ally' else 0, negative=True)
        self.counters_and_stats('victim', 0, negative=True)
        self.receipt()
        if self.mode == 'entry':
            return
        def death_choice():
            if self.mode == 'decline':
                self.emit('choose_cancel_target', player=self.owner)
            elif self.mode != 'bounce':
                self.emit('choose_target', 'victim', player=self.owner)
        self.remove_creature('removal', 'source', destination='hand' if self.mode == 'bounce' else 'graveyard',
                             after=death_choice)
        self.counters_and_stats('victim', 2 if self.mode == 'death' else 0, negative=True)
        self.receipt()

    def glorifier(self):
        self.entry('ally' if self.mode == 'scope' else None)
        for ref in ('source', 'nonmember', 'opposing', 'ally'):
            if ref in self.cards:
                self.counters_and_stats(ref, int(ref == 'ally'))
        self.receipt()

    def run(self):
        self.emit('turn',player=self.owner)
        for player in (0,1):
            self.pool(player,ZERO,fund=True)
        self.receipt()
        self.prepare_life()
        if self.mode=='countered':
            self.countered()
            return
        methods = {ARBITER:self.arbiter,WURM:self.elderscale,MONEY:self.money,ARCHANGEL:self.archangel}
        methods.update({HUNTER:self.hunter, EEL:self.eel, BLOODWITCH:self.tribal_life,
                        SHAPE:self.shape_of_the_wiitigo, THRAGTUSK:self.thragtusk, PACK_SHAMAN:self.tribal_life,
                        WALL:self.wall, CHANNELER:self.channeler, BELCHER:self.belcher,
                        SOULSTINGER:self.soulstinger, GLORIFIER:self.glorifier})
        methods[self.id]()
