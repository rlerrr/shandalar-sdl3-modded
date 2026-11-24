"""Whole Oracle fight activations, printed costs and actual loyalty abilities."""
from collections import Counter
from functools import lru_cache
import json
from pathlib import Path

from counter_condition_recurrence_recipes import exact
from recipes import card, cost_from_oracle, step

FAMILY = 'fight_battlefield_activations'
PROFILE = Path(__file__).with_name('data') / 'fight_battlefield_activations_wave8_profiles.json'
ARENA, CLIFFS, MAGUS, TRACKER = 899, 6092, 1266, 12496
MODES = {
    ARENA: ('paid', 'short', 'target_leaves', 'power_changed'),
    CLIFFS: ('paid', 'no_beast', 'wrong_color', 'target_leaves', 'power_changed'),
    MAGUS: ('paid', 'fresh', 'target_leaves', 'power_changed'),
    TRACKER: ('paid', 'own_pair', 'short', 'target_leaves', 'power_changed', 'alone', 'fresh'),
}
FEES = {ARENA: '{3}', CLIFFS: '{R}{G}', MAGUS: '{3}', TRACKER: '{1}{G}'}
PREFIX = 'FightBattlefieldActivationsRecipe'
SEED = 2633
ZONES = {'hand': 0, 'battlefield': 1, 'graveyard': 3, 'exile': 4}
ZERO = [0] * 6
BEAR, GIANT, WALL = 'Grizzly Bears', 'Hill Giant', 'Wall of Stone'
WURM, CRAW, BEAST = 'Scaled Wurm', 'Craw Wurm', 'Ravenous Baloth'
BOUNCE, GROWTH = 'Unsummon', 'Giant Growth'
WITNESSES = {BEAR, GIANT, WALL, WURM, CRAW, BEAST, BOUNCE, GROWTH}

DOMRI = 12930
EMBLEM_CSVID = 12931
FOREST, SQUIRE = 'Forest', 'Squire'
WITNESSES.update((FOREST, SQUIRE))
MODES[DOMRI] = ('fight', 'target_leaves', 'own_pair', 'low_loyalty', 'reveal_creature', 'top_noncreature',
                'decline_creature', 'emblem')
LIBRARY = 2
ZONE_NAMES = {value: name for name, value in ZONES.items()} | {LIBRARY: 'library'}
SEATED = {'assert_life', 'set_mana', 'assert_mana', 'assert_zone_multiplicity', 'assert_zone_card', 'assert_zone',
          'choose_target', 'choose_dialog', 'choose_zone', 'choose_zone_card', 'turn', 'untap_step'}
VALUES = {'assert_life', 'assert_card', 'assert_stack', 'assert_zone_multiplicity', 'assert_zone_card', 'assert_zone',
          'assert_unused_choices', 'assert_offered_count', 'assert_offered_legal_card', 'assert_counter',
          'assert_offered_selection_required', 'observe_search_filters', 'choose_dialog', 'set_mana', 'assert_mana',
          'attack', 'block', 'can_activate', 'can_target_creature', 'activate', 'present_ai_dialogs'}
ARENA_TEXT = ("{3}, {T}: Tap target creature you control and target creature of an opponent's choice they control. "
              'Those creatures fight each other. (Each deals damage equal to its power to the other.)')
PRINTED = {
    ARENA: ARENA_TEXT,
    MAGUS: ARENA_TEXT,
    CLIFFS: ('{T}: Add {C}.\n{R}{G}, {T}: Target Beast creature you control fights target creature an opponent controls. '
             '(Each deals damage equal to its power to the other.)'),
    TRACKER: '{1}{G}, {T}: Target creature you control fights another target creature.',
    DOMRI: ('+1: Look at the top card of your library. If it\'s a creature card, you may reveal it and put it into your hand.\n'
            '−2: Target creature you control fights another target creature.\n'
            '−7: You get an emblem with "Creatures you control have double strike, trample, hexproof, and haste."'),
}
DOMRI_GAP = ('The public reveal presentation is not a typed observer. The selected top card, hand move, decline and'
             ' remaining library are checked. The emblem is observed through its actual combat and targeting effects.')

@lru_cache(maxsize=1)
def profiles():
    return json.loads(PROFILE.read_bytes())


def classify(record, csvid):
    pin = profiles()['sources'].get(record.get('name'))
    if (csvid not in PRINTED or not pin or pin['registry']['csvid'] != csvid or not exact(pin['oracle'], record)
            or record.get('layout') != 'normal' or record.get('selected_face') or record.get('card_faces')
            or record.get('keywords') != ['Fight'] or record.get('oracle_text') != PRINTED[csvid]):
        raise ValueError('whole pinned normal fight activation source required')
    if csvid == DOMRI and (record.get('loyalty') != '3' or record.get('mana_cost') != '{1}{R}{G}'):
        raise ValueError('exact printed Domri cast and loyalty required')
    return dict(csvid=csvid, fee=FEES.get(csvid), mana=cost_from_oracle(record) if record['mana_cost'] else ZERO)


def variants(shape):
    return [f'{mode}_{seat}' for mode in MODES[shape['csvid']] for seat in (0, 1)]


def validate_variants(shape, selected):
    if (type(selected) is not list or not selected or any(type(mode) is not str for mode in selected)
            or len(set(selected)) != len(selected) or set(selected) - set(variants(shape))):
        raise ValueError('distinct supported fight activation variants required')


def spec(declaration, record):
    pin = profiles()['sources'].get(record.get('name'))
    if not pin:
        raise ValueError('reserved fight activation source required')
    shape = classify(record, pin['registry']['csvid'])
    if (declaration.get('family') != FAMILY or type(declaration.get('version')) is not int
            or declaration['version'] != 2 or declaration.get('oracle_text') != record['oracle_text']
            or not exact(declaration.get('params'), shape)):
        raise ValueError('exact declared fight activation parameters required')
    validate_variants(shape, declaration.get('variants'))
    return dict(shape, family=FAMILY)


def validate_witnesses(oracle, rows):
    pins = profiles()
    if set(pins) != {'sources', 'witnesses', 'runtime_emblem'} or set(pins['witnesses']) != WITNESSES:
        raise ValueError('complete independent fight activation witnesses required')
    if {pin['registry']['csvid'] for pin in pins['sources'].values()} != MODES.keys():
        raise ValueError('exact five-source fight activation profile required')
    emblem = pins['runtime_emblem']
    if (emblem['kind'], emblem['csvid'], emblem['handler']) != ('internal', EMBLEM_CSVID, 'manalinkeh_card_domri_rades_emblem'):
        raise ValueError('Domri’s engine emblem callback identity changed')
    for group in ('sources', 'witnesses'):
        for name, pin in pins[group].items():
            row = rows.get(name, {})
            if (not exact(oracle.get(name), pin['oracle']) or not exact(row, pin['registry'])
                    or row.get('kind') != 'card' or row.get('handler') in (None, '0', 'NULL')
                    or oracle[name].get('layout') != 'normal'):
                raise ValueError('exact fight activation source or witness changed: ' + name)
            if group == 'sources':
                if row['handler'] == 'card_dummy':
                    raise ValueError('implemented fight primary required')
                classify(oracle[name], row['csvid'])


class FightBoard:
    def __init__(self, csvid, name, scene, owner, rows, oracle):
        self.id, self.name, self.scene, self.owner, self.foe = csvid, name, scene, owner, 1-owner
        self.rows, self.oracle = rows, oracle
        self.sources_choose_opponent = csvid in (ARENA, MAGUS)
        self.fee = ZERO if csvid == DOMRI else cost_from_oracle({'mana_cost': FEES[csvid]})
        self.setup, self.steps, self.cards = [], [], {}
        self.mana = [ZERO.copy(), ZERO.copy()]
        self.add('source', name, owner, 'hand')
        self.chosen = 'defender'
        if csvid == DOMRI:
            return
        if self.sources_choose_opponent:
            self.add('fighter', WURM, owner)
            self.add('friendly_other', BEAR, owner)
            self.add('defender', WALL, self.foe)
            self.add('other_defender', GIANT, self.foe)
            if owner == 1 and scene == 'paid':
                self.chosen = 'other_defender'
        elif scene != 'alone':
            if csvid == CLIFFS:
                friend = WURM if scene == 'no_beast' else BEAST
                foe = CRAW if scene == 'power_changed' else BEAST if scene == 'no_beast' else BEAR
            else:
                friend = BEAR if scene == 'power_changed' else BEAST
                foe = GIANT if scene == 'power_changed' else BEAR
            self.add('fighter', friend, owner)
            self.add('defender', foe, owner if scene == 'own_pair' else self.foe)
        if scene == 'target_leaves':
            self.add('response', BOUNCE, self.foe, 'hand')
        if scene == 'power_changed':
            self.add('response', GROWTH, owner, 'hand')

    def add(self, ref, name, seat, zone='battlefield'):
        self.setup.append(card(name, ref, seat, zone))
        self.cards[ref] = dict(name=name, seat=seat, zone=zone, tapped=False, damage=0, bonus=0)

    def emit(self, op, ref='', other='', prop='', *, player=0, value=0, zone=0, index=0, color=0):
        values = {}
        if ref:
            values['ref'] = ref
        if other:
            values['other'] = other
        if op == 'assert_counter':
            values['counter_kind'] = prop
        elif prop:
            values['property'] = prop
        if player or op in SEATED:
            values['player'] = player
        if value or op in VALUES:
            values['value'] = value
        if op in ('assert_zone', 'assert_zone_multiplicity', 'assert_zone_card'):
            values['zone'] = ZONE_NAMES[zone]
        if op in ('assert_zone_multiplicity', 'assert_zone_card', 'assert_offered_legal_card'):
            values['index'] = index
        if op in ('set_mana', 'assert_mana'):
            values['color'] = color
        self.steps.append(step(op, **values))

    def observe(self, ref, prop, value):
        self.emit('assert_card', ref, prop=prop, value=int(value))

    def pools(self):
        for seat, pool in enumerate(self.mana):
            for color, amount in enumerate(pool):
                self.emit('assert_mana', player=seat, color=color, value=amount)

    def fund(self, seat, pool):
        assert not any(self.mana[seat])
        self.mana[seat] = pool.copy()
        for color, amount in enumerate(pool):
            self.emit('set_mana', player=seat, color=color, value=amount)
        self.emit('refresh')

    def paid_spell(self, ref, target=None, *, pending=0):
        card = self.cards[ref]
        seat = card['seat']
        price = cost_from_oracle(self.oracle[card['name']])
        self.fund(seat, price)
        if target is not None:
            self.emit('choose_target', target, player=seat)
        self.emit('cast', ref, value=1)
        self.emit('assert_choices_consumed')
        self.emit('assert_stack', value=pending+1)
        self.mana[seat] = ZERO.copy()
        self.pools()

    def settle(self):
        self.emit('resolve')
        self.emit('refresh')
        self.emit('assert_choices_consumed')
        self.emit('assert_stack')

    def ready(self):
        self.emit('turn', player=self.owner)
        for seat in (0, 1):
            self.fund(seat, ZERO)
        if self.id in (ARENA, CLIFFS):
            self.emit('play_land', 'source', value=1)
            self.emit('assert_choices_consumed')
            self.emit('assert_stack')
        else:
            self.paid_spell('source')
            self.settle()
            if self.scene != 'fresh':
                for seat in (self.foe, self.owner):
                    self.emit('advance_turn')
                    self.emit('untap_step', player=seat)
                    self.emit('turn', player=seat)
                    self.emit('assert_choices_consumed')
            self.observe('source', 'sick', self.scene == 'fresh')
        self.cards['source']['zone'] = 'battlefield'
        self.observe('source', 'tapped', 0)
        self.observe('source', 'in_play', 1)

    def receipts(self):
        self.emit('assert_stack')
        self.pools()
        for seat in (0, 1):
            self.emit('assert_life', player=seat, value=20)
            cards = [card for card in self.cards.values() if card['seat'] == seat]
            counts = Counter((card['name'], card['zone']) for card in cards)
            for name in sorted({card['name'] for card in cards}):
                for zone, number in ZONES.items():
                    self.emit('assert_zone_multiplicity', player=seat, zone=number,
                              index=self.rows[name]['csvid'], value=counts[name, zone])
        for ref, card in self.cards.items():
            self.observe(ref, 'in_play', card['zone'] == 'battlefield')
            if card['zone'] == 'battlefield':
                self.observe(ref, 'tapped', card['tapped'])
                if 'Creature' in self.oracle[card['name']]['type_line']:
                    for continuous in ('power', 'toughness'):
                        self.observe(ref, continuous, self.stats(ref, continuous))
                    self.observe(ref, 'damage', card['damage'])

    def stats(self, ref, prop):
        card = self.cards[ref]
        return int(self.oracle[card['name']][prop])+card['bonus']

    def activation_choices(self):
        if self.sources_choose_opponent:
            if self.owner == 0:
                self.emit('choose_target', 'fighter', player=0)
            self.emit('choose_dialog', player=self.owner, value=0)
            if self.owner == 1:
                self.emit('choose_target', self.chosen, player=0)
        else:
            if self.id == CLIFFS and self.owner == 0:
                self.emit('choose_dialog', player=0, value=1)
            self.emit('choose_target', 'fighter', player=self.owner)
            self.emit('choose_target', self.chosen, player=self.owner)

    def damage_exchange(self):
        a, b = 'fighter', self.chosen
        outgoing = [(a, self.stats(b, 'power')), (b, self.stats(a, 'power'))]
        for ref, damage in outgoing:
            card = self.cards[ref]
            card['damage'] += damage
            if card['damage'] >= self.stats(ref, 'toughness'):
                card['zone'] = 'graveyard'

    def activate_fight(self):
        self.fund(self.owner, self.fee)
        self.emit('require_can_activate', 'source', value=1)
        self.activation_choices()
        self.emit('activate', 'source', value=1)
        self.emit('assert_choices_consumed')
        self.emit('assert_stack', value=1)
        self.cards['source']['tapped'] = True
        self.mana[self.owner] = ZERO.copy()
        self.observe('source', 'tapped', 1)
        self.observe('fighter', 'tapped', 0)
        self.observe(self.chosen, 'tapped', 0)
        self.pools()
        target_left = self.scene == 'target_leaves'
        power_changed = self.scene == 'power_changed'
        # A plain cast accepts an instant over a pending activation. The
        # explicit response wrapper is reserved for an underlying spell.
        if target_left or power_changed:
            self.paid_spell('response', self.chosen if target_left else 'fighter', pending=1)
            self.cards['response']['zone'] = 'graveyard'
        self.settle()
        if target_left:
            self.cards[self.chosen]['zone'] = 'hand'
        if power_changed:
            self.cards['fighter']['bonus'] = 3
        if self.sources_choose_opponent:
            self.cards['fighter']['tapped'] = True
            if not target_left:
                self.cards[self.chosen]['tapped'] = True
        if not target_left:
            self.damage_exchange()
        self.emit('can_activate', 'source', value=0)

    def activate_mana(self):
        budget = self.fee if self.scene == 'no_beast' else cost_from_oracle({'mana_cost': '{1}{G}'})
        self.fund(self.owner, budget)
        self.emit('require_can_activate', 'source', value=1)
        self.emit('activate', 'source', value=1)
        self.emit('assert_choices_consumed')
        self.emit('assert_stack')
        self.cards['source']['tapped'] = True
        self.mana[self.owner][0] += 1

    def run(self):
        self.ready()
        if self.scene in ('no_beast', 'wrong_color'):
            self.activate_mana()
        elif self.scene in ('short', 'fresh', 'alone'):
            budget = self.fee.copy()
            if self.scene == 'short':
                budget[0] -= 1
            self.fund(self.owner, budget)
            self.emit('can_activate', 'source', value=0)
        else:
            self.activate_fight()
        self.receipts()


class DomriBoard(FightBoard):
    def __init__(self, csvid, name, scene, owner, rows, oracle):
        super().__init__(csvid, name, scene, owner, rows, oracle)
        self.loyalty = 0
        self.life = [20, 20]
        self.deck = []
        if scene in ('fight', 'target_leaves', 'own_pair', 'low_loyalty'):
            self.add('fighter', BEAST, owner)
            self.add('defender', BEAR, owner if scene == 'own_pair' else self.foe)
        if scene == 'target_leaves':
            self.add('response', BOUNCE, self.foe, 'hand')
        if scene in ('reveal_creature', 'decline_creature', 'top_noncreature'):
            names = (FOREST, BEAR) if scene == 'top_noncreature' else (BEAR, FOREST)
            for index, name in enumerate(names):
                ref = f'library_{index}'
                self.add(ref, name, owner, 'library')
                self.deck.append(ref)
        if scene == 'emblem':
            self.add('new_bear', BEAR, owner, 'hand')
            self.add('growth_probe', GROWTH, owner, 'hand')
            self.add('bounce_probe', BOUNCE, self.foe, 'hand')
            self.add('blocker', SQUIRE, self.foe)

    def observe_loyalty(self):
        self.emit('assert_counter', 'source', prop='loyalty', value=self.loyalty)

    def start(self):
        self.emit('turn', player=self.owner)
        self.emit('present_ai_dialogs', value=1)
        for seat in (0, 1):
            self.fund(seat, ZERO)
        self.paid_spell('source')
        self.settle()
        self.cards['source']['zone'] = 'battlefield'
        self.loyalty = int(self.oracle[self.name]['loyalty'])
        self.observe('source', 'in_play', 1)
        self.observe_loyalty()

    def next_own_main(self):
        for seat in (self.foe, self.owner):
            self.emit('advance_turn')
            self.emit('untap_step', player=seat)
            self.emit('turn', player=seat)
            self.emit('assert_choices_consumed')
        self.observe_loyalty()

    def loyalty_action(self, index, change, targets=(), *, allowed=True):
        self.emit('require_can_activate', 'source', value=1)
        self.emit('choose_dialog', player=self.owner, value=index)
        for target in targets:
            self.emit('choose_target', target, player=self.owner)
        self.emit('activate', 'source', value=int(allowed))
        # Both creatures are legal under Oracle in the own-pair control. A
        # disabled native mode can leave both planned choices unconsumed.
        self.emit('assert_unused_choices', value=0)
        self.emit('assert_stack', value=int(allowed))
        if allowed:
            self.loyalty += change
        if self.loyalty:
            self.observe_loyalty()
        self.pools()

    def choose_pair(self):
        self.loyalty_action(1, -2, ('fighter', 'defender'))
        if self.scene == 'target_leaves':
            self.paid_spell('response', 'defender', pending=1)
            self.cards['response']['zone'] = 'graveyard'
        self.settle()
        if self.scene == 'target_leaves':
            self.cards['defender']['zone'] = 'hand'
        else:
            self.damage_exchange()
        self.observe_loyalty()
        self.emit('can_activate', 'source', value=0)
        if self.scene == 'low_loyalty':
            self.next_own_main()
            self.cards['fighter']['damage'] = 0
            self.emit('capture')
            self.loyalty_action(1, 0, allowed=False)
            # The main-phase UI clears a rejected action; the raw bridge does not.
            # Try +1 as the alternate choice from the same one-loyalty state.
            self.emit('restore')
            self.plus()

    def plus(self):
        self.loyalty_action(0, 1)
        if self.deck:
            top = self.deck[0]
            name = self.cards[top]['name']
            legal = 'Creature' in self.oracle[name]['type_line']
            self.emit('observe_search_filters', value=1)
            if self.scene == 'reveal_creature':
                self.emit('choose_zone_card', player=self.owner, value=self.rows[name]['csvid'])
                self.cards[top]['zone'] = 'hand'
                del self.deck[0]
            else:
                self.emit('choose_zone', player=self.owner, value=-1)
            self.settle()
            self.emit('assert_offered_count', value=1)
            self.emit('assert_offered_selection_required', value=0)
            self.emit('assert_offered_legal_card', index=self.rows[name]['csvid'], value=int(legal))
            self.emit('observe_search_filters', value=0)
        else:
            self.settle()
        self.observe_loyalty()
        self.emit('can_activate', 'source', value=0)

    def emblem(self):
        for expected in (4, 5, 6, 7):
            self.plus()
            assert self.loyalty == expected
            self.next_own_main()
        self.loyalty_action(2, -7)
        self.settle()
        self.cards['source']['zone'] = 'graveyard'
        self.paid_spell('new_bear')
        self.settle()
        self.cards['new_bear']['zone'] = 'battlefield'
        self.emit('can_target_creature', 'growth_probe', 'new_bear', value=1)
        self.emit('can_target_creature', 'bounce_probe', 'new_bear', value=0)
        self.emit('begin_attackers')
        self.emit('attack', 'new_bear', value=1)
        self.emit('finish_attackers')
        self.emit('begin_blockers')
        self.emit('block', 'blocker', 'new_bear', value=1)
        self.emit('advance_combat')
        self.observe('new_bear', 'blocked', 1)
        self.emit('combat_damage')
        self.settle()
        self.emit('end_combat')
        self.cards['new_bear']['tapped'] = True
        self.cards['blocker']['zone'] = 'graveyard'
        # The blocker absorbs the first two damage. Double strike then
        # tramples two over the historical blocked flag in the second step.
        self.life[self.foe] -= 2

    def receipts(self):
        self.emit('assert_stack')
        self.pools()
        if self.loyalty:
            self.observe_loyalty()
        for seat in (0, 1):
            self.emit('assert_life', player=seat, value=self.life[seat])
            cards = [card for card in self.cards.values() if card['seat'] == seat]
            for zone, number in ZONES.items():
                counts = Counter(card['name'] for card in cards if card['zone'] == zone)
                for name in sorted({card['name'] for card in cards}):
                    self.emit('assert_zone_multiplicity', player=seat, zone=number,
                              index=self.rows[name]['csvid'], value=counts[name])
        self.emit('assert_zone', player=self.owner, zone=LIBRARY, value=len(self.deck))
        for index, ref in enumerate(self.deck):
            self.emit('assert_zone_card', player=self.owner, zone=LIBRARY, index=index,
                      value=self.rows[self.cards[ref]['name']]['csvid'])
        for ref, item in self.cards.items():
            if ref.startswith('library_'):
                continue
            self.observe(ref, 'in_play', item['zone'] == 'battlefield')
            if item['zone'] == 'battlefield' and 'Creature' in self.oracle[item['name']]['type_line']:
                self.observe(ref, 'damage', item['damage'])
                self.observe(ref, 'tapped', item['tapped'])

    def run(self):
        self.start()
        if self.scene in ('fight', 'target_leaves', 'own_pair', 'low_loyalty'):
            self.choose_pair()
        elif self.scene == 'emblem':
            self.emblem()
        else:
            self.plus()
        self.receipts()


def build(name, csvid, declaration, shape, variant, rows, oracle):
    mode, seat = variant.rsplit('_', 1)
    board = (DomriBoard if csvid == DOMRI else FightBoard)(csvid, name, mode, int(seat), rows, oracle)
    board.run()
    return dict(name=f'{PREFIX}_{csvid}_{variant}', card=name, fixture='empty_main', seed=SEED,
                setup=board.setup, steps=board.steps,
                _recipe=dict(family=FAMILY, version=2, variant=variant, params=declaration['params'],
                             oracle_text=declaration['oracle_text'], untested_components=[DOMRI_GAP] if csvid == DOMRI else []))


def expand(name, csvid, declaration, shape, selected, rows, oracle):
    if rows[name]['csvid'] != csvid or shape['csvid'] != csvid:
        raise ValueError('exact registered fight source required')
    validate_variants(shape, selected)
    validate_witnesses(oracle, rows)
    return [build(name, csvid, declaration, shape, variant, rows, oracle) for variant in selected]
