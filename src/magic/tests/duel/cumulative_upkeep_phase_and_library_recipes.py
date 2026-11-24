"""Cumulative upkeep with actual draw, search, token and end-step riders."""

from collections import Counter
from functools import lru_cache
import json
from pathlib import Path

from counter_condition_recurrence_recipes import exact
from cumulative_upkeep_payment_recipes import REMINDER
from recipes import card, cost_from_oracle, step

FAMILY = 'cumulative_upkeep_phase_and_library'
PROFILE = Path(__file__).with_name('data') / 'cumulative_upkeep_phase_and_library_wave8_profiles.json'
COLD, SEARCH, ETCH, TERROR = (1893, 8384, 8422, 3067)
GROWTH, CORE, STAIR, REMORA = (2532, 8408, 2637, 2046)
RULES = {
    COLD: ('{2}', (
        "At the beginning of each player's upkeep, this enchantment deals damage to that player equal to the number of snow lands they control.",
    )),
    SEARCH: ('{1}', (
        'Whenever you pay this enchantment\'s cumulative upkeep, you may search your library for a creature card with mana value equal to the number of age counters on this enchantment, put it onto the battlefield, then shuffle.',
    )),
    ETCH: ('{B}', (
        'At the beginning of your end step, draw a card for each age counter on this enchantment.',
        'When this enchantment is put into a graveyard from the battlefield, you lose 2 life for each age counter on it.',
    )),
    TERROR: ('{1}', (
        "At the beginning of your draw step, destroy each creature with mana value equal to the number of age counters on this enchantment. They can't be regenerated.",
    )),
}
RULES.update({
    GROWTH: ('{1}', (
        'At the beginning of your upkeep, put a growth counter on this enchantment.',
        "At the beginning of each opponent's draw step, that player draws an additional card for each growth counter on this enchantment, then this enchantment deals damage to the player equal to the number of cards they drew this way.",
    )),
    CORE: ('{1}', (
        'At the beginning of your end step, this enchantment deals X damage divided as you choose among any number of target creatures, where X is the number of age counters on it.',
    )),
    STAIR: ('{1}{B}', (
        'At the beginning of each upkeep, if this enchantment is on the battlefield, each player creates a 2/2 black Zombie creature token with haste named Tombspawn for each creature card in their graveyard.',
        "At the beginning of each end step and when this enchantment leaves the battlefield, destroy all tokens created with this enchantment. They can't be regenerated.",
    )),
    REMORA: ('{1}', (
        'Whenever an opponent casts a noncreature spell, you may draw a card unless that player pays {4}.',
    )),
})
PRESERVED_MODES = {
    COLD: ('paid_twice', 'declined_first', 'no_snow'),
    SEARCH: ('search_twice', 'search_optional', 'declined_first'),
    ETCH: ('draw_two', 'decline_second', 'disenchant_age2'),
    TERROR: ('paid_twice', 'decline_second'),
}
NEW_MODES = {
    TERROR: ('draw_values', 'declined_draw'),
    GROWTH: ('draw_ages', 'declined_draw'),
    CORE: ('split_age2', 'focus_age2', 'decline_age2'),
    STAIR: ('tokens_lifecycle', 'empty_graves', 'destroy_stairwell', 'haste_attack'),
    REMORA: ('short_draw', 'exact_payment', 'decline_draw', 'creature_and_own', 'decline_upkeep'),
}
ZONES = {'hand': 0, 'battlefield': 1, 'library': 2, 'graveyard': 3, 'exile': 4}
ZERO = (0,) * 6
MV1, MV2, WRONG = 'Savannah Lions', 'Grizzly Bears', 'Swords to Plowshares'
SEARCH_DECK = ('Forest', MV1, WRONG, MV2, 'Hill Giant')
DRAW_DECK = ('Forest', 'Island', 'Mountain', 'Plains', 'Swamp')
TOMBSPAWN = 'Tombspawn'
WITNESSES = {
    *SEARCH_DECK, *DRAW_DECK, 'Disenchant', 'Phyrexian Ironfoot', 'Snow-Covered Island',
    'Snow-Covered Plains', 'Wastes', 'Eager Cadet', 'Drudge Skeletons', 'Darksteel Relic',
    TOMBSPAWN, 'Zombie',
}
GAPS = {
    TERROR: 'The preserved upkeep cases stop before draw. Expanded draw cases exercise the actual matching-MV sweep and regeneration control.',
    GROWTH: 'The added streams use enough library cards to observe each extra draw. Draw replacement interactions are outside these fixtures.',
    CORE: 'The current helper selects damage chunks during end-step resolution. Lethal zone transitions independently observe allocation.',
    STAIR: 'Actual token identity and P/T are observed separately from combat. The haste branch requires an actual Tombspawn to bind. Regeneration availability for generated tokens needs an independent external grant and remains outside these cases.',
    REMORA: 'The optional owner input and opponent payment are deliberately separate. Printed mode absence is recorded by explicit unused-input receipts and independent card/mana changes.',
}


@lru_cache(maxsize=1)
def profiles():
    return json.loads(PROFILE.read_bytes())


def classify(record, csvid):
    if (csvid not in RULES or record.get('layout') != 'normal' or record.get('selected_face')
            or record.get('card_faces') or record.get('type_line') not in ('Enchantment', 'World Enchantment')):
        raise ValueError('exact normal cumulative upkeep primary required')
    fee, riders = RULES[csvid]
    rendered = '\n'.join(('Cumulative upkeep ' + fee + REMINDER, *riders))
    if record.get('oracle_text') != rendered or record.get('keywords') != ['Cumulative upkeep']:
        raise ValueError('complete printed cumulative upkeep and riders required')
    return dict(csvid=csvid, cast_cost=cost_from_oracle(record), upkeep=cost_from_oracle({'mana_cost': fee}))


def variants(shape):
    csvid = shape['csvid']
    choices = (*PRESERVED_MODES.get(csvid, ()), *NEW_MODES.get(csvid, ()))
    return [f'{mode}_{actor}' for mode in choices for actor in (0, 1)]


def validate_variants(shape, selected):
    if (type(selected) is not list or not selected or any(type(v) is not str for v in selected)
            or len(set(selected)) != len(selected) or set(selected) - set(variants(shape))):
        raise ValueError('distinct supported phase and library variants required')


def spec(declaration, record):
    pin = profiles()['sources'].get(record.get('name'))
    if not pin or not exact(pin['oracle'], record):
        raise ValueError('exact whole cumulative upkeep record required')
    shape = classify(record, pin['registry']['csvid'])
    if (declaration.get('family') != FAMILY or type(declaration.get('version')) is not int
            or declaration['version'] != 2 or declaration.get('oracle_text') != record['oracle_text']
            or not exact(declaration.get('params'), shape)):
        raise ValueError('complete versioned phase and library declaration required')
    validate_variants(shape, declaration.get('variants'))
    return dict(shape, family=FAMILY)


def validate_witnesses(oracle, rows):
    pins = profiles()
    if set(pins) != {'sources', 'witnesses'} or set(pins['witnesses']) != WITNESSES:
        raise ValueError('complete independently pinned witnesses required')
    for group, entries in pins.items():
        for name, pin in entries.items():
            row = rows.get(name, {})
            if (not exact(oracle.get(name), pin['oracle']) or not exact(row, pin['registry'])
                    or row.get('kind') != 'card' or row.get('handler') in (None, '0', 'NULL')):
                raise ValueError('exact source or witness changed: ' + name)
            if group == 'sources':
                if row['handler'] == 'card_dummy':
                    raise ValueError('implemented cumulative upkeep primary required')
                classify(oracle[name], row['csvid'])
    if (oracle[MV1]['mana_cost'] != '{W}' or oracle[MV1]['oracle_text']
            or oracle[WRONG]['mana_cost'] != '{W}' or oracle[WRONG]['type_line'] != 'Instant'
            or oracle[MV2]['mana_cost'] != '{1}{G}' or rows[TOMBSPAWN]['csvid'] != 18839
            or rows['Zombie']['csvid'] != 18860):
        raise ValueError('exact cost, type, payment and token witnesses required')


class Case:
    def __init__(self, row, actor, mode, oracle, registry_by_name):
        self.card, self.csvid = row['name'], row['csvid']
        self.actor, self.enemy, self.active = actor, 1 - actor, actor
        self.mode, self.oracle, self.registry = mode, oracle, registry_by_name
        self.setup, self.steps, self.cards = [], [], {}
        self.life, self.age, self.searches = [20, 20], 0, 0
        self.tapped, self.snow_lands = {}, [0, 0]
        self.libraries = {actor: list(SEARCH_DECK if self.csvid == SEARCH else DRAW_DECK),
                          self.enemy: list(DRAW_DECK)}
        self.payment_lands = []
        self.add('source', self.card, actor, 'hand')
        for p in (0, 1):
            for i, name in enumerate(self.libraries[p]):
                self.add(f'lib_{p}_{i}', name, p, 'library')
        if self.csvid == COLD:
            snow = mode != 'no_snow'
            for i in range(2 if not snow or mode == 'declined_first' else 4):
                name = 'Snow-Covered Plains' if snow and i < 2 else 'Wastes'
                self.pay_land(f'fee_{i}', name)
            if snow:
                self.add('enemy_snow', 'Snow-Covered Island', self.enemy)
                self.snow_lands[self.enemy] = 1
            self.add('enemy_nonsnow', 'Island', self.enemy)
            for p in (0, 1):
                self.add(f'snow_nonland_{p}', 'Phyrexian Ironfoot', p)
        elif self.csvid == STAIR:
            for i, name in enumerate(('Wastes', 'Swamp', 'Wastes', 'Swamp')):
                self.pay_land(f'fee_{i}', name)
        else:
            self.pay_land('fee_0', 'Swamp' if self.csvid == ETCH else 'Wastes')
            if mode not in ('search_optional', 'declined_first'):
                self.pay_land('fee_1', 'Swamp' if self.csvid == ETCH else 'Wastes')
        if self.csvid == ETCH and mode == 'disenchant_age2':
            self.add('disenchant', 'Disenchant', self.enemy, 'hand')
        if self.csvid == TERROR:
            for p in (0, 1):
                self.add(f'mv1_{p}', MV1, p)
                self.add(f'mv2_{p}', MV2, p)

    def add(self, ref, name, player, zone='battlefield'):
        self.setup.append(card(name, ref, player, zone))
        self.cards[ref] = dict(name=name, player=player, zone=zone)

    def pay_land(self, ref, name):
        self.add(ref, name, self.actor)
        self.payment_lands.append(ref)
        self.tapped[ref] = 0
        if self.oracle[name]['type_line'].startswith('Basic Snow Land'):
            self.snow_lands[self.actor] += 1

    def emit(self, op, ref='', other='', prop='', *, player=0, value=0, zone=0, index=0, color=0):
        if op in ('untap_step', 'upkeep_step', 'draw_step'):
            self.steps.append(step(op, player=player))
            return
        if op == 'assert_counter':
            self.steps.append(step(op, ref=ref, counter_kind=prop, value=value))
            return
        if op == 'choose_mana_source':
            self.steps.append(step(op, ref=ref))
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
        if op in ('assert_zone', 'assert_zone_card', 'assert_zone_multiplicity'):
            fields['zone'] = next(name for name, number in ZONES.items() if number == zone)
        self.steps.append(step(op, **fields))

    def pool(self, op, player, mana):
        for color, count in enumerate(mana):
            self.emit(op, player=player, value=count, color=color)

    def receipts(self):
        self.emit('refresh')
        self.emit('assert_stack', value=0)
        if self.cards['source']['zone'] == 'battlefield':
            self.emit('assert_counter', 'source', prop='age', value=self.age)
        names = {item['name'] for item in self.cards.values()}
        for player in (0, 1):
            self.emit('assert_life', player=player, value=self.life[player])
            self.pool('assert_mana', player, ZERO)
            for zone, z in ZONES.items():
                contents = self.libraries[player] if zone == 'library' else [
                    item['name'] for item in self.cards.values()
                    if (item['player'], item['zone']) == (player, zone)
                ]
                self.emit('assert_zone', player=player, value=len(contents), zone=z)
                counts = Counter(contents)
                for name in sorted(names):
                    self.emit('assert_zone_multiplicity', player=player, zone=z,
                              index=self.registry[name]['csvid'], value=counts[name])
                if zone == 'library' and (player != self.actor or not self.searches):
                    for idx, name in enumerate(contents):
                        self.emit('assert_zone_card', player=player, zone=z, index=idx,
                                  value=self.registry[name]['csvid'])
        for ref, tapped in self.tapped.items():
            if tapped is not None:
                self.emit('assert_card', ref, prop='tapped', value=tapped)
        if self.csvid == SEARCH:
            self.emit('assert_shuffle_count', player=self.actor, value=self.searches)
            self.emit('assert_shuffle_count', player=self.enemy, value=0)

    def cast(self, ref, *, target=None):
        item = self.cards[ref]
        payer = item['player']
        record = self.oracle[item['name']]
        fee = ZERO if record['mana_cost'] == '{0}' else cost_from_oracle(record)
        self.pool('set_mana', payer, fee)
        self.emit('refresh')
        if target:
            self.emit('choose_target', target, player=payer)
        self.emit('cast', ref, value=1)
        self.emit('assert_stack', value=1)
        self.pool('assert_mana', payer, ZERO)
        self.emit('resolve')
        self.emit('assert_choices_consumed')
        item['zone'] = 'graveyard' if target else 'battlefield'

    def end_step(self, op):
        self.emit(op)
        if self.csvid == ETCH and self.active == self.actor and self.cards['source']['zone'] == 'battlefield':
            for _ in range(self.age):
                name = self.libraries[self.actor].pop(0)
                self.draw_to_hand(name)

    def draw_to_hand(self, name):
        card, = [entry for entry in self.cards.values() if entry['player'] == self.actor
                 and entry['name'] == name and entry['zone'] == 'library']
        card['zone'] = 'hand'

    def prepare_payment(self, pay):
        self.age += 1
        self.emit('choose_dialog', player=self.actor, value=int(not pay))
        if pay:
            count = self.age * (2 if self.csvid == COLD else 1)
            assert len(self.payment_lands) >= count
            if self.actor == 0:
                for ref in self.payment_lands[:count]:
                    self.emit('choose_mana_source', ref)
                    self.tapped[ref] = 1
            else:
                self.tapped = dict.fromkeys(self.payment_lands, 1 if count == len(self.payment_lands) else None)
        else:
            self.cards['source']['zone'] = 'graveyard'
            if self.csvid == ETCH:
                self.life[self.actor] -= 2 * self.age

    def prepare_search(self, option):
        if self.actor == 0:
            self.emit('choose_dialog', player=0, value=int(option == 'decline'))
        if option == 'decline':
            assert self.actor == 0
            return None
        self.searches += 1
        if option == 'fail_find':
            self.emit('choose_zone', player=self.actor, value=-1)
            return None
        choice = {1: MV1, 2: MV2}[self.age]
        self.emit('observe_search_filters', value=1)
        self.emit('choose_zone_card', player=self.actor, value=self.registry[choice]['csvid'])
        selected, = [entry for entry in self.cards.values()
                     if entry['player'] == self.actor and entry['name'] == choice and entry['zone'] == 'library']
        selected['zone'] = 'battlefield'
        self.libraries[self.actor].remove(choice)
        return choice

    def visit(self, *, pay=True, search='choose'):
        self.end_step('advance_turn')
        self.active = 1 - self.active
        self.emit('untap_step', player=self.active)
        if self.active == self.actor:
            self.tapped = dict.fromkeys(self.payment_lands, 0)
        live = self.cards['source']['zone'] == 'battlefield'
        choice = None
        if live and self.active == self.actor:
            self.prepare_payment(pay)
            if self.csvid == SEARCH and pay:
                choice = self.prepare_search(search)
        if live and self.csvid == COLD:
            self.life[self.active] -= self.snow_lands[self.active]
        self.emit('upkeep_step', player=self.active)
        self.emit('assert_unused_choices', value=0)
        if choice:
            for name in (choice, WRONG, 'Hill Giant'):
                self.emit('assert_offered_legal_card', index=self.registry[name]['csvid'],
                          value=int(name == choice))
            if self.age == 1:
                self.emit('assert_offered_legal_card', index=self.registry[MV2]['csvid'], value=0)
            self.emit('observe_search_filters', value=0)
        self.receipts()

    def build(self):
        self.emit('turn', player=self.actor)
        for player in (0, 1):
            self.pool('set_mana', player, ZERO)
        self.cast('source')
        self.receipts()
        self.visit()
        optional = 'decline' if self.actor == 0 else 'fail_find'
        self.visit(pay=self.mode != 'declined_first',
                   search=optional if self.mode == 'search_optional' else 'choose')
        if self.mode in ('declined_first', 'no_snow', 'search_optional'):
            return
        self.visit()
        self.visit(pay=self.mode != 'decline_second')
        if self.csvid == ETCH:
            if self.mode == 'disenchant_age2':
                self.emit('turn', player=self.actor)
                self.cast('disenchant', target='source')
                self.cards['source']['zone'] = 'graveyard'
                self.life[self.actor] -= 2 * self.age
                self.receipts()
            self.end_step('cleanup')
            self.receipts()



class ExtendedCase(Case):
    def __init__(self, row, actor, mode, oracle, registry_by_name):
        super().__init__(row, actor, mode, oracle, registry_by_name)
        self.growth = 0
        self.tokens = [0, 0]
        self.token_counts = [0, 0]
        if self.csvid in (TERROR, GROWTH):
            for p in (0, 1):
                for name in ('Wastes', 'Snow-Covered Plains'):
                    self.add(f'lib_{p}_{len(self.libraries[p])}', name, p, 'library')
                    self.libraries[p].append(name)
        if self.csvid == TERROR:
            self.add('skeleton', 'Drudge Skeletons', self.enemy)
            self.add('regen_swamp', 'Swamp', self.enemy)
        elif self.csvid == CORE:
            self.add('first_victim', 'Eager Cadet', self.enemy)
            self.add('own_victim', MV1, self.actor)
            self.add('foe_victim', MV1, self.enemy)
            self.add('focus_victim', MV2, self.enemy)
        elif self.csvid == STAIR:
            if mode != 'empty_graves':
                self.add('own_dead', MV1, self.actor, 'graveyard')
                self.add('foe_dead', MV2, self.enemy, 'graveyard')
                if mode != 'haste_attack':
                    self.add('foe_dead_second', 'Hill Giant', self.enemy, 'graveyard')
            for p in (0, 1):
                self.add(f'ineligible_{p}', 'Disenchant', p, 'graveyard')
            self.token_counts = [
                sum(entry['player'] == p and entry['zone'] == 'graveyard'
                    and 'Creature' in oracle[entry['name']]['type_line']
                    for entry in self.cards.values())
                for p in (0, 1)
            ]
            if mode == 'destroy_stairwell':
                self.add('disenchant', 'Disenchant', self.enemy, 'hand')
        elif self.csvid == REMORA:
            if mode == 'creature_and_own':
                self.add('own_spell', 'Darksteel Relic', self.actor, 'hand')
                self.add('foe_spell', MV2, self.enemy, 'hand')
            else:
                self.add('foe_spell', 'Darksteel Relic', self.enemy, 'hand')

    def receipts(self):
        super().receipts()
        if self.csvid == GROWTH and self.cards['source']['zone'] == 'battlefield':
            self.emit('assert_counter', 'source', prop='growth', value=self.growth)
        for p, quantity in enumerate(self.tokens):
            if self.csvid != STAIR:
                continue
            other = sum(entry['name'] != TOMBSPAWN and entry['zone'] == 'battlefield'
                        and entry['player'] == p for entry in self.cards.values())
            for index in range(other, other + quantity):
                for prop in ('power', 'toughness'):
                    self.emit('assert_zone_state', prop=prop, player=p, index=index, value=2)

    def prepare_payment(self, pay):
        if self.csvid != STAIR:
            super().prepare_payment(pay)
            if self.csvid == GROWTH and pay:
                self.growth += 1
            return
        self.age += 1
        assert pay
        self.emit('choose_dialog', player=self.actor, value=0)
        count = 2 * self.age
        refs = self.payment_lands[:count]
        if self.actor == 0:
            for ref in refs:
                self.emit('choose_mana_source', ref)
                self.tapped[ref] = 1
        else:
            self.tapped = dict.fromkeys(self.payment_lands, 1 if count == len(self.payment_lands) else None)

    def remove_tokens(self):
        for entry in self.cards.values():
            if entry['name'] == TOMBSPAWN:
                entry['zone'] = 'gone'
        self.tokens = [0, 0]

    def add_tokens(self):
        self.tokens = self.token_counts.copy()
        for p, quantity in enumerate(self.tokens):
            for i in range(quantity):
                self.cards[f'tomb_{p}_{i}'] = dict(name=TOMBSPAWN, player=p, zone='battlefield')

    def end_step(self, op):
        targets = ()
        live = self.cards['source']['zone'] == 'battlefield'
        if self.csvid == CORE and live and self.active == self.actor:
            if self.age == 1:
                targets = ('first_victim',)
            elif self.age == 2:
                targets = (('focus_victim',) * 2 if self.mode == 'focus_age2'
                           else ('own_victim', 'foe_victim'))
            for ref in targets:
                self.emit('choose_target', ref, player=self.actor)
        super().end_step(op)
        if targets:
            self.emit('assert_unused_choices', value=0)
        for ref in set(targets):
            self.cards[ref]['zone'] = 'graveyard'
        if self.csvid == STAIR:
            self.remove_tokens()

    def visit(self, *, pay=True, search='choose'):
        if self.csvid != STAIR:
            super().visit(pay=pay, search=search)
            return
        self.end_step('advance_turn')
        self.active = 1 - self.active
        self.emit('untap_step', player=self.active)
        if self.active == self.actor:
            self.tapped = dict.fromkeys(self.payment_lands, 0)
            self.prepare_payment(pay)
        self.emit('upkeep_step', player=self.active)
        self.emit('assert_unused_choices', value=0)
        self.add_tokens()
        self.receipts()

    def draw_cards(self, player, quantity):
        for _ in range(quantity):
            name = self.libraries[player].pop(0)
            card, = [entry for entry in self.cards.values() if entry['player'] == player
                     and entry['name'] == name and entry['zone'] == 'library']
            card['zone'] = 'hand'

    def actual_draw(self):
        live = self.cards['source']['zone'] == 'battlefield'
        active_owner = self.active == self.actor
        skeleton = self.cards.get('skeleton', {})
        regeneration = (self.csvid == TERROR and live and active_owner and self.age == 2
                        and skeleton['zone'] == 'battlefield')
        if regeneration:
            # Ready Swamp and a real Drudge Skeletons. A Bury path never offers this response.
            self.emit('choose_regeneration', 'skeleton')
        self.emit('draw_step', player=self.active)
        self.emit('assert_unused_choices', value=int(regeneration))
        draws = 1
        if self.csvid == GROWTH and live and not active_owner:
            draws += self.growth
            self.life[self.active] -= self.growth
        self.draw_cards(self.active, draws)
        if self.csvid == TERROR and live and active_owner:
            for ref, entry in self.cards.items():
                if not ref.startswith(('mv1_', 'mv2_')) and ref != 'skeleton':
                    continue
                fee = cost_from_oracle(self.oracle[entry['name']])
                if entry['zone'] == 'battlefield' and sum(fee) == self.age:
                    entry['zone'] = 'graveyard'
        self.receipts()

    def draw_plan(self):
        self.visit()
        self.actual_draw()
        self.visit()
        self.actual_draw()
        self.visit()
        self.actual_draw()
        self.visit(pay=self.mode != 'declined_draw')
        self.actual_draw()
        if self.csvid == GROWTH:
            self.visit()
            self.actual_draw()

    def core_plan(self):
        self.visit()
        self.visit()
        self.visit()
        self.visit(pay=self.mode != 'decline_age2')
        self.end_step('cleanup')
        self.receipts()

    def stair_plan(self):
        self.visit()
        if self.mode == 'destroy_stairwell':
            self.emit('turn', player=self.enemy)
            self.cast('disenchant', target='source')
            self.cards['source']['zone'] = 'graveyard'
            self.remove_tokens()
            self.receipts()
            return
        if self.mode == 'haste_attack':
            self.emit('bind_permanent', 'new_tombspawn', player=self.enemy,
                      index=-1, value=self.registry[TOMBSPAWN]['csvid'])
            for prop, value in (('color', 2), ('subtype_Zombie', 1), ('token', 1)):
                self.emit('assert_matching_permanents', prop=prop, player=self.enemy,
                          index=self.registry[TOMBSPAWN]['csvid'], value=value)
            self.emit('turn', player=self.enemy)
            self.emit('begin_attackers')
            self.emit('attack', 'new_tombspawn', value=1)
            self.emit('finish_attackers')
            self.emit('begin_blockers')
            self.emit('advance_combat')
            self.emit('combat_damage')
            self.emit('end_combat')
            self.life[self.actor] -= 2
            self.receipts()
            self.end_step('cleanup')
            self.receipts()
            return
        self.visit()
        self.end_step('cleanup')
        self.receipts()

    def remora_plan(self):
        if self.mode == 'creature_and_own':
            self.cast('own_spell')
            self.receipts()
            self.visit()
            self.emit('turn', player=self.enemy)
            self.cast('foe_spell')
            self.receipts()
            return
        self.visit()
        self.visit(pay=self.mode != 'decline_upkeep')
        self.visit()
        self.emit('turn', player=self.enemy)
        reserve = 4 if self.mode == 'exact_payment' else 0
        self.pool('set_mana', self.enemy, (reserve, 0, 0, 0, 0, 0))
        self.emit('refresh')
        self.emit('cast', 'foe_spell', value=1)
        self.emit('assert_stack', value=1)
        if self.mode == 'exact_payment':
            self.emit('choose_dialog', player=self.enemy, value=1)
        if self.mode == 'decline_draw':
            self.emit('choose_dialog', player=self.actor, value=1)
        self.emit('announce_spell', 'foe_spell')
        self.emit('assert_unused_choices', value=0)
        self.emit('resolve')
        self.emit('assert_choices_consumed')
        self.cards['foe_spell']['zone'] = 'battlefield'
        if self.mode == 'short_draw':
            self.draw_cards(self.actor, 1)
        self.receipts()

    def build(self):
        self.emit('turn', player=self.actor)
        for player in (0, 1):
            self.pool('set_mana', player, ZERO)
        self.cast('source')
        self.receipts()
        if self.csvid in (TERROR, GROWTH):
            self.draw_plan()
        elif self.csvid == CORE:
            self.core_plan()
        elif self.csvid == STAIR:
            self.stair_plan()
        else:
            self.remora_plan()


def expand(name, csvid, declaration, shape, selected, rows, oracle):
    validate_variants(shape, selected)
    validate_witnesses(oracle, rows)
    if profiles()['sources'][name]['registry']['csvid'] != csvid:
        raise ValueError('exact registered cumulative upkeep primary CSVID required')
    cases = []
    for variant in selected:
        mode, actor = variant.rsplit('_', 1)
        world = Case if mode in PRESERVED_MODES.get(csvid, ()) else ExtendedCase
        game = world(rows[name], int(actor), mode, oracle, rows)
        game.build()
        gaps = [GAPS[csvid]] if csvid in GAPS else []
        cases.append(dict(
            name=f'CumulativeUpkeepPhaseAndLibraryRecipe_{csvid}_{variant}',
            card=name, fixture='empty_main', seed=2503, setup=game.setup, steps=game.steps,
            _recipe=dict(family=FAMILY, version=2, variant=variant, params=declaration['params'],
                         oracle_text=declaration['oracle_text'], untested_components=gaps),
        ))
    return cases
