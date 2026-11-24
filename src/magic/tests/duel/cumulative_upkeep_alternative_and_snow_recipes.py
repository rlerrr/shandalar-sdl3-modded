"""Alternative and snow cumulative upkeep via real age events and payments."""

from collections import Counter
from functools import lru_cache
import json
from pathlib import Path

from library_search_recipes import wave6_exact as exact
from recipes import card, cost_from_oracle, step


FAMILY = 'cumulative_upkeep_alternative_and_snow'
PROFILE = Path(__file__).parent / 'data' / f'{FAMILY}_wave8_profiles.json'

REMINDER = ('At the beginning of your upkeep, put an age counter on this permanent, '
            'then sacrifice it unless you pay its upkeep cost for each age counter on it.')
CARDS = {
    8332: ('{G} or {W}', ('Trample',), ('When this creature dies, you gain 2 life for each age counter on it.',),
           ('Forest', 'Plains', 'Mountain')),
    8362: ('{R} or {G}', ('Trample',), ('This creature gets +1/+1 for each age counter on it.',),
           ('Mountain', 'Forest', 'Plains')),
    8403: ('{U} or {B}', ('Enchant creature',), ('You control enchanted creature.',
           'When this Aura is put into a graveyard from the battlefield, you lose 2 life for each age counter on it.'),
           ('Island', 'Swamp', 'Mountain')),
    8374: ('{S}', ('Enchant creature',), ('Enchanted creature gets +3/+3 for each age counter on this Aura.',),
           ('Snow-Covered Plains', 'Snow-Covered Island', 'Plains')),
}
MODES = ('first_color', 'second_color', 'mixed', 'unpayable', 'decline_after_one')
EXTRA = {8332: 'destroy_age_two', 8403: 'destroy_age_two'}
NEW = {
    8353: ('zero_age', 'age_one_player', 'age_two_creature', 'activated_prevent', 'nonsnow', 'noncombat'),
    8383: ('escalate', 'short_after_one', 'decline_second', 'external_destroy', 'separate_theft', 'no_lands'),
}
SNOW_REMINDER = REMINDER + ' {S} can be paid with one mana from a snow source.'
NEW_ORACLE = {
    8353: (f'Cumulative upkeep {{S}} ({SNOW_REMINDER})',
           'If a creature would deal combat damage to you and/or one or more creatures you control, '
           'prevent X of that damage, where X is the number of age counters on this enchantment.',
           '{S}: Put an age counter on this enchantment.'),
    8383: ('Flying', f"Cumulative upkeep—Gain control of a land you don't control. ({REMINDER})",
           "This creature gets +1/+1 for each land you control but don't own.",
           'When this creature leaves the battlefield, each player gains control of each land they own that you control.'),
}
WITNESSES = {name for _, _, _, lands in CARDS.values() for name in lands} | {
    'Wall of Roots', 'Goblin Tinkerer', 'Elvish Mystic', 'Ornithopter', 'Disenchant', 'Murder',
    'Hill Giant', 'Lightning Bolt', 'Political Trickery',
}
STATIC_DUMMY_WITNESSES = {'Hill Giant', 'Ornithopter'}
ZONE = {'hand': 0, 'battlefield': 1, 'library': 2, 'graveyard': 3, 'exile': 4}
ZONE_NAMES = {number: name for name, number in ZONE.items()}
ZERO = [0] * 6


@lru_cache(maxsize=1)
def profiles():
    return json.loads(PROFILE.read_bytes())


def classify(record, csvid):
    if csvid in NEW:
        clauses = list(NEW_ORACLE[csvid])
        keywords = {'Cumulative upkeep'} | ({'Flying'} if csvid == 8383 else set())
        fee = '{S}' if csvid == 8353 else "Gain control of a land you don't control."
    else:
        fee, before, after, _ = CARDS[csvid]
        reminder = SNOW_REMINDER if fee == '{S}' else REMINDER
        clauses = [*before, f'Cumulative upkeep {fee} ({reminder})', *after]
        keywords = {'Cumulative upkeep', 'Enchant' if before[0] == 'Enchant creature' else 'Trample'}
    if (record['oracle_text'] != '\n'.join(clauses) or record.get('layout') != 'normal'
            or set(record['keywords']) != keywords or record.get('card_faces') or record.get('selected_face')):
        raise ValueError('Exact whole cumulative upkeep source required')
    return dict(csvid=csvid, fee=fee, price=cost_from_oracle(record), clauses=clauses)


def variants(model):
    csvid = model['csvid']
    modes = NEW[csvid] if csvid in NEW else MODES + ((EXTRA[csvid],) if csvid in EXTRA else ())
    return [f'{mode}_{seat}' for mode in modes for seat in (0, 1)]


def validate_witnesses(oracle, rows):
    pins = profiles()
    wanted = CARDS.keys() | NEW.keys()
    if (set(pins) != {'sources', 'witnesses'} or set(pins['witnesses']) != WITNESSES
            or {p['registry']['csvid'] for p in pins['sources'].values()} != wanted):
        raise ValueError('Exact independent cumulative upkeep source and witnesses required')
    for group in pins.values():
        for name, pin in group.items():
            record, row = oracle.get(name, {}), rows.get(name, {})
            handler = row.get('handler')
            if (not exact(pin, {'oracle': record, 'registry': row}) or record.get('layout') != 'normal'
                    or row.get('kind') != 'card' or handler in (None, '0', 'NULL')
                    or handler == 'card_dummy' and name not in STATIC_DUMMY_WITNESSES):
                raise ValueError(f'Pinned cumulative upkeep source or witness changed: {name}')


def spec(declaration, record):
    pin = profiles()['sources'].get((record or {}).get('name'))
    if not pin or not exact(record, pin['oracle']):
        raise ValueError('Complete pinned cumulative upkeep primary required')
    model = classify(record, pin['registry']['csvid'])
    picked = declaration.get('variants')
    if (declaration.get('family') != FAMILY or type(declaration.get('version')) is not int
            or declaration['version'] != 2 or declaration.get('oracle_text') != record['oracle_text']
            or not exact(declaration.get('params'), model) or not isinstance(picked, list) or not picked
            or any(type(v) is not str for v in picked) or len(picked) != len(set(picked))
            or set(picked) - set(variants(model))):
        raise ValueError('Exact cumulative upkeep clause, cost and scoped variants required')
    return dict(model, family=FAMILY)


class Duel:
    def __init__(self, csvid, record, actor, mode, rows, oracle):
        self.initialize(csvid, record, actor, mode, rows, oracle)
        first, second, illegal = CARDS[csvid][3]
        self.snow = self.fee == '{S}'
        self.aura = 'Aura' in record['type_line']
        land_pair = {'first_color': (first, first), 'second_color': (second, second),
                     'unpayable': (illegal, None)}.get(mode, (first, second))
        self.add('land', actor, land_pair[0])
        if land_pair[1] is not None:
            self.add('later', actor, land_pair[1], 'hand')
        if self.aura:
            host_owner = self.foe if csvid == 8403 else self.actor
            host = 'Wall of Roots' if csvid == 8403 else 'Goblin Tinkerer'
            self.add('host', host_owner, host)
            self.add('guard', 1 - host_owner, 'Elvish Mystic')
        else:
            self.add('blocker', self.foe, 'Ornithopter')
        if mode == 'destroy_age_two':
            self.add('removal', self.foe, 'Disenchant' if self.aura else 'Murder', 'hand')

    def initialize(self, csvid, record, actor, mode, rows, oracle):
        self.csvid, self.record, self.actor, self.foe, self.mode = csvid, record, actor, 1 - actor, mode
        self.rows, self.oracle = rows, oracle
        self.current = actor
        self.setup, self.steps, self.cards = [], [], {}
        self.life = [20, 20]
        self.age = 0
        self.paid_upkeeps = []
        self.fee = classify(record, csvid)['fee']
        self.add('source', actor, record['name'], 'hand')

    def add(self, ref, player, name, zone='battlefield'):
        self.setup.append(card(name, ref, player, zone))
        self.cards[ref] = dict(name=name, player=player, owner=player, zone=zone, tapped=False)

    def emit(self, op, ref='', other='', prop='', *, zone=None, **fields):
        if op == 'assert_counter':
            self.steps.append(step(op, ref=ref, counter_kind=prop, **fields))
            return
        row = step(op, **fields)
        for key, field in [('ref', ref), ('other', other), ('property', prop)]:
            if field:
                row[key] = field
        if zone is not None:
            row['zone'] = ZONE_NAMES[zone]
        self.steps.append(row)

    def observe(self, ref, prop, value):
        self.emit('assert_card', ref, prop=prop, value=value)

    def mana(self, op, seat, amounts):
        for color, amount in enumerate(amounts):
            self.emit(op, player=seat, color=color, value=amount)

    def source_live(self):
        return self.cards['source']['zone'] == 'battlefield'

    def stats(self, ref):
        row = self.oracle[self.cards[ref]['name']]
        bonus = self.age if ref == 'source' and self.csvid == 8362 else 0
        if ref == 'host' and self.csvid == 8374 and self.source_live():
            bonus = 3 * self.age
        if 'power' in row:
            return tuple(int(row[key]) + bonus for key in ('power', 'toughness'))
        return ()

    def receipts(self):
        self.emit('assert_stack', value=0)
        identities = sorted({c['name'] for c in self.cards.values()})
        for seat in (0, 1):
            self.emit('assert_life', player=seat, value=self.life[seat])
            self.mana('assert_mana', seat, ZERO)
            for zone, number in ZONE.items():
                counts = Counter(c['name'] for c in self.cards.values() if c['player'] == seat and c['zone'] == zone)
                self.emit('assert_zone', player=seat, zone=number, value=sum(counts.values()))
                for name in identities:
                    self.emit('assert_zone_multiplicity', player=seat, zone=number,
                              index=self.rows[name]['csvid'], value=counts[name])
        for ref, card in self.cards.items():
            if card['zone'] == 'battlefield':
                self.observe(ref, 'in_play', 1)
                self.observe(ref, 'tapped', int(card['tapped']))
                for key, value in zip(('power', 'toughness'), self.stats(ref)):
                    self.observe(ref, key, value)
        if self.source_live():
            self.emit('assert_counter', 'source', prop='age', value=self.age)

    def bind_host(self, player):
        self.bind_controller('host', player)

    def bind_controller(self, ref, player):
        actual = self.cards[ref]
        actual['player'] = player
        self.emit('bind_permanent_copy', ref, player=player, index=0, value=self.rows[actual['name']]['csvid'])

    def cast(self, ref, *, target=None, second_target=None, recipient=None):
        spell = self.cards[ref]
        seat = spell['player']
        self.mana('set_mana', seat, cost_from_oracle(self.oracle[spell['name']]))
        self.emit('refresh')
        if target:
            self.emit('choose_target', target, player=seat)
        if second_target:
            self.emit('choose_target', second_target, player=seat)
        if recipient is not None:
            self.emit('choose_player', player=seat, value=recipient)
        self.emit('cast', ref, value=1)
        self.emit('assert_choices_consumed')
        self.emit('assert_stack', value=1)
        self.mana('assert_mana', seat, ZERO)
        self.emit('resolve')
        self.emit('refresh')
        self.emit('assert_choices_consumed')
        spell['zone'] = 'battlefield' if ref == 'source' else 'graveyard'

    def leave_source(self):
        self.cards['source']['zone'] = 'graveyard'
        if self.csvid == 8332:
            self.life[self.actor] += 2 * self.age
        if self.csvid == 8403:
            self.life[self.actor] -= 2 * self.age
            self.bind_host(self.foe)

    def pay(self, refs):
        if self.snow:
            for ref in refs:
                # The snow helper presents eligible actual snow permanents as targets, not mana-pool rows.
                self.emit('choose_target', ref, player=self.actor)
        elif self.actor == 0:
            for ref in refs:
                self.emit('choose_mana_source', ref)
        for ref in refs:
            self.cards[ref]['tapped'] = True
        self.paid_upkeeps.append([self.cards[ref]['name'] for ref in refs])

    def start_upkeep(self, player):
        assert player == 1 - self.current
        self.current = player
        self.emit('advance_turn')
        self.emit('untap_step', player=player)
        for card in self.cards.values():
            if card['player'] == player and card['zone'] == 'battlefield':
                card['tapped'] = False
        self.receipts()

    def resolve_upkeep(self, player):
        self.emit('upkeep_step', player=player)
        self.emit('resolve')
        self.emit('refresh')
        self.emit('assert_unused_choices', value=0)
        self.emit('assert_choices_consumed')

    def finish_upkeep(self, player):
        self.receipts()
        self.emit('turn', player=player)

    def upkeep(self, player, *, payment=(), decline=False):
        self.start_upkeep(player)
        owner_trigger = player == self.actor and self.source_live()
        if owner_trigger:
            self.age += 1
            payable = bool(payment)
            if payable:
                assert len(payment) == self.age and len(set(payment)) == self.age
                assert all(self.cards[r]['zone'] == 'battlefield' and not self.cards[r]['tapped'] for r in payment)
            if payable or player == 1:
                self.emit('choose_dialog', player=player, value=int(decline or not payable))
            if payable and not decline:
                self.pay(payment)
        self.resolve_upkeep(player)
        if owner_trigger and (not payment or decline):
            self.leave_source()
        self.finish_upkeep(player)

    def play_later_land(self):
        self.emit('play_land', 'later', value=1)
        self.emit('resolve')
        self.emit('refresh')
        self.emit('assert_choices_consumed')
        self.cards['later']['zone'] = 'battlefield'
        self.receipts()

    def trample(self):
        self.emit('begin_attackers')
        self.emit('attack', 'source', value=1)
        self.observe('source', 'attacking', 1)
        self.emit('finish_attackers')
        self.emit('begin_blockers')
        self.emit('block', 'blocker', 'source', value=1)
        self.emit('advance_combat')
        self.observe('source', 'blocked', 1)
        self.emit('combat_damage')
        self.emit('resolve')
        self.emit('end_combat')
        self.emit('assert_choices_consumed')
        self.cards['source']['tapped'] = True
        self.cards['blocker']['zone'] = 'graveyard'
        self.life[self.foe] -= self.stats('source')[0] - int(self.oracle['Ornithopter']['toughness'])
        self.receipts()

    def build(self):
        self.emit('turn', player=self.actor)
        self.mana('set_mana', self.foe, ZERO)
        self.cast('source', target='host' if self.aura else None)
        if self.csvid == 8403:
            self.bind_host(self.actor)
        self.receipts()
        self.upkeep(self.foe)
        self.upkeep(self.actor, payment=() if self.mode == 'unpayable' else ('land',))
        if not self.source_live():
            return
        self.play_later_land()
        self.upkeep(self.foe)
        self.upkeep(self.actor, payment=('land', 'later'), decline=self.mode == 'decline_after_one')
        if not self.source_live():
            return
        if not self.aura:
            self.trample()
        if self.mode == 'destroy_age_two':
            self.cast('removal', target='source')
            self.leave_source()
            self.receipts()
            return
        self.upkeep(self.foe)
        self.upkeep(self.actor)


class CoverOfWinter(Duel):
    def __init__(self, csvid, record, actor, mode, rows, oracle):
        self.initialize(csvid, record, actor, mode, rows, oracle)
        self.snow = True
        self.add('land', actor, 'Plains' if mode == 'nonsnow' else 'Snow-Covered Plains')
        if mode in ('age_two_creature', 'activated_prevent'):
            self.add('later', actor, 'Snow-Covered Island', 'hand')
        if mode == 'age_two_creature':
            self.add('blocker', actor, 'Wall of Roots')
        if mode == 'noncombat':
            self.add('bolt', self.foe, 'Lightning Bolt', 'hand')
        else:
            foe = 'Goblin Tinkerer' if mode == 'activated_prevent' else 'Hill Giant'
            self.add('attacker', self.foe, foe)

    def activate_snow(self, ref):
        self.emit('can_activate', 'source', value=1)
        self.emit('choose_target', ref, player=self.actor)
        self.emit('activate', 'source', value=1)
        self.emit('assert_choices_consumed')
        self.emit('assert_stack', value=1)
        self.observe(ref, 'tapped', 1)
        self.emit('assert_counter', 'source', prop='age', value=self.age)
        self.mana('assert_mana', self.actor, ZERO)
        self.emit('resolve')
        self.emit('refresh')
        self.emit('assert_choices_consumed')
        self.cards[ref]['tapped'] = True
        self.age += 1
        self.receipts()

    def prevent_battle(self):
        blocked = self.mode == 'age_two_creature'
        self.emit('begin_attackers')
        self.emit('attack', 'attacker', value=1)
        self.observe('attacker', 'attacking', 1)
        self.emit('finish_attackers')
        self.emit('begin_blockers')
        if blocked:
            self.emit('block', 'blocker', 'attacker', value=1)
        self.emit('advance_combat')
        self.observe('attacker', 'blocked', int(blocked))
        self.emit('combat_damage')
        self.emit('resolve')
        self.emit('end_combat')
        self.emit('assert_choices_consumed')
        self.cards['attacker']['tapped'] = True
        damage = max(0, self.stats('attacker')[0] - (self.age if self.source_live() else 0))
        if blocked:
            self.observe('blocker', 'damage', damage)
            self.observe('attacker', 'damage', 0)
        else:
            self.life[self.actor] -= damage
        self.receipts()

    def build(self):
        self.emit('turn', player=self.actor)
        self.mana('set_mana', self.foe, ZERO)
        self.cast('source')
        self.receipts()
        if self.mode == 'activated_prevent':
            self.play_later_land()
            self.activate_snow('land')
            self.activate_snow('later')
        elif self.mode != 'zero_age':
            if self.mode == 'nonsnow':
                self.emit('can_activate', 'source', value=0)
                self.emit('activate', 'source', value=0)
                self.emit('assert_choices_consumed')
            self.upkeep(self.foe)
            self.upkeep(self.actor, payment=() if self.mode == 'nonsnow' else ('land',))
            if self.mode == 'age_two_creature':
                self.play_later_land()
                self.upkeep(self.foe)
                self.upkeep(self.actor, payment=('land', 'later'))
        self.upkeep(self.foe)
        if self.mode == 'noncombat':
            self.cast('bolt', recipient=self.actor)
            self.life[self.actor] -= 3
            self.receipts()
        else:
            self.prevent_battle()


class HeraldOfLeshrac(Duel):
    def __init__(self, csvid, record, actor, mode, rows, oracle):
        self.initialize(csvid, record, actor, mode, rows, oracle)
        self.add('owned', actor, 'Forest')
        total = {'short_after_one': 2, 'separate_theft': 1, 'no_lands': 0}.get(mode, 3)
        for index, name in enumerate(('Island', 'Mountain', 'Swamp')[:total]):
            self.add(f'foreign_{index}', self.foe, name)
        self.add('ground', self.foe, 'Elvish Mystic')
        if mode in ('external_destroy', 'separate_theft'):
            self.add('murder', self.foe, 'Murder', 'hand')
        if mode == 'separate_theft':
            self.add('exchange', actor, 'Political Trickery', 'hand')

    def stats(self, ref):
        ordinary = super().stats(ref)
        if ref != 'source':
            return ordinary
        bonus = sum(c['zone'] == 'battlefield' and c['player'] == self.actor != c['owner']
                    and 'Land' in self.oracle[c['name']]['type_line'].split(' — ')[0].split()
                    for c in self.cards.values())
        return tuple(n + bonus for n in ordinary)

    def leave_source(self):
        super().leave_source()
        for ref, card in self.cards.items():
            if (card['zone'] == 'battlefield' and card['player'] == self.actor != card['owner']
                    and 'Land' in self.oracle[card['name']]['type_line'].split(' — ')[0].split()):
                self.bind_controller(ref, card['owner'])

    def demand(self, player, *, decline=False):
        self.start_upkeep(player)
        live_own = player == self.actor and self.source_live()
        selected = []
        if live_own:
            self.age += 1
            available = [r for r, c in self.cards.items() if c['zone'] == 'battlefield'
                         and c['player'] == self.foe and 'Land' in self.oracle[c['name']]['type_line'].split(' — ')[0].split()]
            allowed = len(available) >= self.age
            if allowed or player == 1:
                self.emit('choose_dialog', player=player, value=int(decline or not allowed))
            if allowed and not decline:
                selected = available[:self.age]
                for ref in selected:
                    self.emit('choose_target', ref, player=self.actor)
        self.resolve_upkeep(player)
        if live_own:
            if selected:
                for ref in selected:
                    self.bind_controller(ref, self.actor)
            else:
                self.leave_source()
        self.finish_upkeep(player)

    def fly(self):
        self.emit('begin_attackers')
        self.emit('attack', 'source', value=1)
        self.observe('source', 'attacking', 1)
        self.emit('finish_attackers')
        self.emit('begin_blockers')
        self.emit('block', 'ground', 'source', value=0)
        self.emit('advance_combat')
        self.observe('source', 'blocked', 0)
        self.emit('combat_damage')
        self.emit('resolve')
        self.emit('end_combat')
        self.emit('assert_choices_consumed')
        self.cards['source']['tapped'] = True
        self.life[self.foe] -= self.stats('source')[0]
        self.receipts()

    def outside_upkeep(self):
        self.cast('exchange', target='owned', second_target='foreign_0')
        self.bind_controller('owned', self.foe)
        self.bind_controller('foreign_0', self.actor)
        self.receipts()
        self.destroy()

    def destroy(self):
        self.cast('murder', target='source')
        self.leave_source()
        self.receipts()

    def build(self):
        self.emit('turn', player=self.actor)
        self.mana('set_mana', self.foe, ZERO)
        self.cast('source')
        self.receipts()
        if self.mode == 'separate_theft':
            self.outside_upkeep()
            return
        self.demand(self.foe)
        self.demand(self.actor)
        if not self.source_live():
            return
        self.demand(self.foe)
        self.demand(self.actor, decline=self.mode == 'decline_second')
        if not self.source_live():
            return
        self.fly()
        if self.mode == 'external_destroy':
            self.destroy()
        else:
            self.demand(self.foe)
            self.demand(self.actor)


def expand(name, csvid, declaration, model, selected, rows, oracle):
    validate_witnesses(oracle, rows)
    if rows[name]['csvid'] != csvid or model['csvid'] != csvid or set(selected) - set(variants(model)):
        raise ValueError('Exact cumulative upkeep source and scoped variants required')
    world = {8353: CoverOfWinter, 8383: HeraldOfLeshrac}.get(csvid, Duel)
    cases = []
    for variant in selected:
        mode, seat = variant.rsplit('_', 1)
        game = world(csvid, oracle[name], int(seat), mode, rows, oracle)
        game.build()
        gaps = ['Cover has separate player and creature prevention controls; simultaneous split combat packets are untested.'] if csvid == 8353 else []
        metadata = dict(family=FAMILY, version=2, variant=variant, params=declaration['params'],
                        oracle_text=declaration['oracle_text'], covered_clauses=model['clauses'],
                        untested_components=gaps)
        cases.append(dict(name=f'CumulativeUpkeepAlternativeSnowRecipe_{csvid}_{variant}', card=name,
                          fixture='empty_main', seed=2503, setup=game.setup, steps=game.steps, _recipe=metadata))
    return cases

