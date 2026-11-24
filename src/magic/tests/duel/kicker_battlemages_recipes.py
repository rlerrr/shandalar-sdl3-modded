"""Independent dual-kicker payments and printed Battlemage entry effects."""

from collections import Counter
from copy import deepcopy
from functools import lru_cache
import json
from pathlib import Path
import re

from counter_condition_recurrence_recipes import exact


FAMILY = 'kicker_battlemages'
PROFILE = Path(__file__).with_name('data') / 'kicker_battlemages_wave8_profiles.json'
KICKER = re.compile(r'Kicker (\S+) and/or (\S+) \(You may pay an additional \1 and/or \2 as you cast this spell\.\)')
CAPTURED_MODES = {'normal': (), 'first': (0,), 'second': (1,), 'both': (0, 1)}
NEW_MODES = ('base_only', 'first_funded', 'second_funded', 'scope', 'countered', 'replay')
MODES = (*CAPTURED_MODES, *NEW_MODES)
SPECIAL_SCOPE_PAYMENTS = {8739: (1,), 5268: (0,)}
SCOPE_WITNESSES = {
    8739: ('Hill Giant', 'tap_alt', False),
    5310: ('Grizzly Bears', 'bury_alt', False),
    5320: ('Hill Giant', 'damage_alt', True),
    5322: ('Sanguine Bond', 'enchantment_alt', False),
}
GAP = ('These handlers process their entry effects inside one callback. '
       'The available driver cannot place a response between target selection and the effect itself.')
EFFECTS = {
    8739: ('discard3', 'tap_damage'),
    5268: ('bounce2', 'destroy_land'),
    5310: ('gain3', 'bury_creature'),
    5315: ('destroy_flyer', 'draw2'),
    5320: ('damage2', 'destroy_artifact'),
    5322: ('discard2', 'destroy_enchantment'),
}
# The real ETB callback order only affects the scripted prompts; printed payment quantities and effects are independent.
CALLBACK_ORDER = {5310: (1, 0), 5315: (1, 0)}
GUARDS = {
    8739: (('Hill Giant', 'tap_target', True, False), ('Grizzly Bears', 'tapped_guard', True, True)),
    5268: (('Grizzly Bears', 'bounce1', True, False), ('Hill Giant', 'bounce2', False, False),
           ('Black Knight', 'black_guard', True, False), ('Taiga', 'land_target', True, False)),
    5310: (('Clay Statue', 'bury_target', True, False), ('Black Knight', 'black_guard', True, False)),
    5315: (('Suntail Hawk', 'flyer_target', True, False), ('Grizzly Bears', 'ground_guard', True, False)),
    5320: (("Traveler's Amulet", 'artifact_target', True, False), ('Sanguine Bond', 'type_guard', True, False)),
    5322: (('Sanguine Bond', 'enchantment_target', True, False), ("Traveler's Amulet", 'type_guard', True, False)),
}
DESTROY = {
    'destroy_land': 'land_target', 'bury_creature': 'bury_target', 'destroy_flyer': 'flyer_target',
    'destroy_artifact': 'artifact_target', 'destroy_enchantment': 'enchantment_target',
}
DECLARATIONS = {
    'discard3': 'target player discards three cards.',
    'tap_damage': 'tap target untapped creature and that creature deals damage equal to its power to its controller.',
    'bounce2': "return up to two target nonblack creatures to their owners' hands.",
    'destroy_land': 'destroy target land.',
    'gain3': 'you gain 3 life.',
    'bury_creature': "destroy target nonblack creature. That creature can't be regenerated.",
    'destroy_flyer': 'destroy target creature with flying.',
    'draw2': 'draw two cards.',
    'damage2': 'it deals 2 damage to any target.',
    'destroy_artifact': 'destroy target artifact.',
    'discard2': 'target player discards two cards.',
    'destroy_enchantment': 'destroy target enchantment.',
}
DISCARD = {8739: 3, 5322: 2}
ZONE = {'hand': 0, 'battlefield': 1, 'library': 2, 'graveyard': 3, 'exile': 4}


WITNESSES = {name for entries in GUARDS.values() for name, _, _, _ in entries}
WITNESSES.update(name for name, _, _ in SCOPE_WITNESSES.values())
WITNESSES.update({'Island', 'Mountain', 'Plains', 'Forest', 'Counterspell'})


@lru_cache(maxsize=1)
def profiles():
    return json.loads(PROFILE.read_bytes())


def classify(record, csvid):
    if (csvid not in EFFECTS or record.get('layout') != 'normal' or record.get('selected_face')
            or record.get('card_faces') or not record.get('type_line', '').startswith('Creature — ')
            or any(not re.fullmatch(r'[1-9][0-9]*', record.get(key, '')) for key in ('power', 'toughness'))):
        raise ValueError('reviewed normal dual-kicker creature required')
    effects = EFFECTS[csvid]
    base, extra = amounts(record, effects)
    return dict(base=base, kicker=extra, effects=list(effects))


def variants(_shape=None):
    return [f'{mode}_{player}' for mode in MODES for player in (0, 1)]


def validate_variants(selected):
    if (type(selected) is not list or not selected or any(type(mode) is not str for mode in selected)
            or len(set(selected)) != len(selected) or set(selected) - set(variants())):
        raise ValueError('distinct supported Battlemage variants required')


def spec(declaration, record):
    pinned = profiles()['sources'].get(record.get('name'))
    if not pinned or not exact(pinned['oracle'], record):
        raise ValueError('complete pinned Battlemage Oracle record required')
    shape = classify(record, pinned['registry']['csvid'])
    if (declaration.get('family') != FAMILY or type(declaration.get('version')) is not int
            or declaration['version'] != 2 or declaration.get('oracle_text') != record['oracle_text']
            or not exact(declaration.get('params'), shape)):
        raise ValueError('whole typed dual-kicker declaration required')
    validate_variants(declaration.get('variants'))
    return dict(shape, family=FAMILY)


def validate_witnesses(oracle, registry):
    data = profiles()
    if set(data) != {'sources', 'witnesses'} or set(data['witnesses']) != WITNESSES:
        raise ValueError('complete independent Battlemage witnesses required')
    for group, entries in data.items():
        for name, pin in entries.items():
            row = registry.get(name, {})
            if (not exact(oracle.get(name), pin['oracle']) or not exact(row, pin['registry'])
                    or oracle[name].get('layout') != 'normal' or row.get('kind') != 'card'
                    or row.get('handler') in (None, '0', 'NULL')):
                raise ValueError('pinned Battlemage card or witness changed: ' + name)
            if group == 'sources':
                if row['handler'] == 'card_dummy':
                    raise ValueError('implemented Battlemage source required')
                classify(oracle[name], row['csvid'])


def amounts(record, effects):
    from recipes import cost_from_oracle

    lines = record.get('oracle_text', '').splitlines()
    kicker = KICKER.fullmatch(lines[0]) if lines else None
    if kicker is None or record.get('keywords') != ['Kicker']:
        raise ValueError('both printed kicker costs required')
    taxes = kicker[1], kicker[2]
    printed = [
        f'When this creature enters, if it was kicked with its {tax} kicker, {DECLARATIONS[effect]}'
        for tax, effect in zip(taxes, effects)
    ]
    if lines[1:] != printed:
        raise ValueError('complete printed dual-kicker clauses, in printed order, required')
    return cost_from_oracle(record), [cost_from_oracle({'mana_cost': marker}) for marker in taxes]


def total(*costs):
    return [sum(bucket) for bucket in zip(*costs)]


def displayed_rows(extra, affordable):
    # Omitted illegal rows compress the real menu in color order,
    # regardless of which kicker the card lists first.
    colors = [next(i for i, n in enumerate(mana) if i and n) for mana in extra]
    assert colors[0] != colors[1] and all(sum(mana[1:]) == 1 for mana in extra)
    rows = {(index,): position for position, index in enumerate(sorted(affordable, key=colors.__getitem__))}
    if len(affordable) == 2:
        rows[0, 1] = 2
    rows[()] = len(rows)
    return rows


class Game:
    def __init__(self, csvid, name, actor, mode, rows, oracle):
        self.csvid, self.name, self.actor, self.foe, self.mode = csvid, name, actor, 1 - actor, mode
        self.rows, self.oracle = rows, oracle
        self.entities, self.setup, self.steps = {}, [], []
        self.life = [20, 20]
        self.deck = {actor: ['Island', 'Mountain', 'Plains'], self.foe: ['Forest', 'Plains']}
        self.add('source', actor, name, 'hand')
        for witness, ref, other, tapped in GUARDS[csvid]:
            if mode != 'scope' or ref != 'flyer_target':
                self.add(ref, self.foe if other else actor, witness, tapped=tapped)
        if mode == 'scope' and csvid in SCOPE_WITNESSES:
            witness, ref, other = SCOPE_WITNESSES[csvid]
            self.add(ref, self.foe if other else actor, witness)
        if mode == 'countered':
            self.add('counter', self.foe, 'Counterspell', 'hand')
        for player, deck in self.deck.items():
            for index, witness in enumerate(deck):
                self.add(f'deck_{player}_{index}', player, witness, 'library')
        if csvid in DISCARD:
            self.add('own_hand_guard', actor, 'Plains', 'hand')
            for i in range(DISCARD[csvid] + 1):
                self.add(f'discard_{i}', self.foe, 'Island', 'hand')
        self.base, self.extra = amounts(oracle[name], EFFECTS[csvid])
        self.paid = CAPTURED_MODES.get(mode, {
            'base_only': (), 'first_funded': (0,), 'second_funded': (1,),
            'scope': SPECIAL_SCOPE_PAYMENTS.get(csvid, (0, 1)), 'countered': (0, 1), 'replay': (0, 1),
        }.get(mode))
        self.affordable = self.paid if mode in ('base_only', 'first_funded', 'second_funded') else (0, 1)
        self.funded = total(self.base, *(self.extra[i] for i in self.affordable))
        self.pool_from_payment = total([0] * 6, *(self.extra[i] for i in self.affordable if i not in self.paid))
        self.foe_pool = [2, 0, 0, 0, 0, 0] if csvid == 5310 and mode != 'countered' else [0] * 6
        self.test_regeneration = False

    def add(self, ref, player, name, zone='battlefield', *, tapped=False):
        from recipes import card

        created = card(name, ref, player, zone)
        if tapped:
            created['state'] = ['tapped']
        self.setup.append(created)
        self.entities[ref] = dict(name=name, player=player, zone=zone, tapped=tapped)

    def emit(self, op, ref='', prop='', *, player=0, value=0, zone=0, index=0, color=0):
        from recipes import step

        fields = dict(player=player, value=value, index=index, color=color)
        if ref:
            fields['ref'] = ref
        if prop:
            fields['property'] = prop
        if op in ('assert_zone', 'assert_zone_card', 'assert_zone_multiplicity'):
            fields['zone'] = next(name for name, number in ZONE.items() if number == zone)
        self.steps.append(step(op, **fields))

    def observe(self, ref, prop, value):
        self.emit('assert_card', ref, prop, value=value)

    def mana(self, op, player, pool):
        for color, amount in enumerate(pool):
            self.emit(op, player=player, value=amount, color=color)

    def pay_and_cast(self):
        self.emit('turn', player=self.actor)
        if self.csvid in DISCARD:
            self.emit('discard_policy', value=1)
        self.mana('set_mana', self.actor, self.funded)
        self.mana('set_mana', self.foe, self.foe_pool)
        self.emit('refresh')
        self.emit('choose_dialog', player=self.actor, value=displayed_rows(self.extra, self.affordable)[self.paid])
        self.emit('cast', 'source', value=1)
        self.emit('assert_choices_consumed')
        self.emit('assert_stack', value=1)
        self.observe('source', 'in_play', 0)
        self.mana('assert_mana', self.actor, self.pool_from_payment)
        self.mana('assert_mana', self.foe, self.foe_pool)
        for ref, card in self.entities.items():
            if card['zone'] == 'battlefield':
                self.observe(ref, 'in_play', 1)
                self.observe(ref, 'tapped', int(card['tapped']))
        for player in (0, 1):
            self.emit('assert_life', player=player, value=20)

    def plan_and_apply(self, effect):
        if effect in ('discard3', 'discard2'):
            self.emit('choose_player', player=self.actor, value=self.foe)
            for i in range(DISCARD[self.csvid]):
                if self.foe == 1:
                    # Native random seat chooses internally, then acknowledges each actual discarded card.
                    self.emit('choose_dialog', player=self.foe)
                self.entities[f'discard_{i}']['zone'] = 'graveyard'
        elif effect == 'tap_damage':
            ref = 'tap_alt' if self.mode == 'scope' else 'tap_target'
            self.emit('choose_target', ref, player=self.actor)
            card = self.entities[ref]
            card['tapped'] = True
            self.life[card['player']] -= int(self.oracle[card['name']]['power'])
        elif effect == 'bounce2':
            self.emit('choose_target', 'bounce1', player=self.actor)
            self.entities['bounce1']['zone'] = 'hand'
            if self.mode == 'scope':
                self.emit('choose_cancel_target', player=self.actor)
            else:
                self.emit('choose_target', 'bounce2', player=self.actor)
                self.entities['bounce2']['zone'] = 'hand'
        elif effect in DESTROY:
            ref = DESTROY[effect]
            if self.mode == 'scope':
                ref = {'bury_creature': 'bury_alt', 'destroy_enchantment': 'enchantment_alt'}.get(effect, ref)
            if ref not in self.entities:
                return
            self.emit('choose_target', ref, player=self.actor)
            if ref == 'bury_target':
                self.emit('choose_regeneration', ref)
                self.test_regeneration = True
            self.entities[ref]['zone'] = 'graveyard'
        elif effect == 'gain3':
            self.life[self.actor] += 3
        elif effect == 'draw2':
            for index in range(2):
                self.entities[f'deck_{self.actor}_{index}']['zone'] = 'hand'
            del self.deck[self.actor][:2]
        elif effect == 'damage2':
            if self.mode == 'scope':
                self.emit('choose_target', 'damage_alt', player=self.actor)
            else:
                self.emit('choose_player', player=self.actor, value=self.foe)
                self.life[self.foe] -= 2
        else:
            raise ValueError(f'Unhandled printed effect: {effect}')

    def receipts(self):
        self.emit('refresh')
        self.emit('assert_stack')
        self.mana('assert_mana', self.actor, self.pool_from_payment)
        self.mana('assert_mana', self.foe, self.foe_pool)
        for ref, card in self.entities.items():
            if card['zone'] == 'battlefield':
                self.observe(ref, 'in_play', 1)
                self.observe(ref, 'tapped', int(card['tapped']))
        if self.mode != 'countered':
            source = self.oracle[self.name]
            self.observe('source', 'power', int(source['power']))
            self.observe('source', 'toughness', int(source['toughness']))
        if 'damage_alt' in self.entities:
            self.observe('damage_alt', 'damage', 2)
        names = {c['name'] for c in self.entities.values()}
        for player in (0, 1):
            self.emit('assert_life', player=player, value=self.life[player])
            for zone, index in ZONE.items():
                contents = self.deck[player] if zone == 'library' else [
                    c['name'] for c in self.entities.values() if c['player'] == player and c['zone'] == zone
                ]
                self.emit('assert_zone', player=player, zone=index, value=len(contents))
                if zone == 'library':
                    for pos, name in enumerate(contents):
                        self.emit('assert_zone_card', player=player, zone=index, index=pos, value=self.rows[name]['csvid'])
                else:
                    counts = Counter(contents)
                    for name in sorted(names):
                        self.emit('assert_zone_multiplicity', player=player, zone=index,
                                  index=self.rows[name]['csvid'], value=counts[name])

    def finish_entry(self):
        order = CALLBACK_ORDER.get(self.csvid, (0, 1))
        active = [EFFECTS[self.csvid][i] for i in order if i in self.paid]
        for effect in active:
            self.plan_and_apply(effect)
        self.emit('resolve')
        if self.test_regeneration:
            self.emit('assert_unused_choices', value=1)
        self.emit('assert_choices_consumed')
        self.entities['source']['zone'] = 'battlefield'
        if self.csvid in DISCARD and self.foe == 0:
            amount = DISCARD[self.csvid] if 'discard3' in active or 'discard2' in active else 0
            self.emit('assert_discard_choices', value=amount)
        self.receipts()

    def counter_before_entry(self):
        from recipes import cost_from_oracle

        self.emit('announce_spell', 'source')
        self.mana('set_mana', self.foe, cost_from_oracle(self.oracle['Counterspell']))
        self.emit('refresh')
        self.emit('cast_response', 'counter', value=1)
        self.emit('assert_stack', value=2)
        self.emit('resolve_response')
        self.emit('assert_choices_consumed')
        self.entities['counter']['zone'] = self.entities['source']['zone'] = 'graveyard'
        self.receipts()

    def build(self):
        from recipes import step

        self.pay_and_cast()
        if self.mode == 'countered':
            self.counter_before_entry()
            return
        cut = len(self.steps)
        self.finish_entry()
        if self.mode == 'replay':
            suffix = self.steps[cut:]
            self.steps[cut:] = ([step('capture')] + suffix
                                + [step('record_state'), step('ai_roundtrip'), step('restore')]
                                + deepcopy(suffix) + [step('assert_recorded_state')])


def expand(name, csvid, declaration, shape, selected, rows, oracle):
    validate_variants(selected)
    validate_witnesses(oracle, rows)
    if profiles()['sources'][name]['registry']['csvid'] != csvid:
        raise ValueError('exact Battlemage primary required')
    cases = []
    for variant in selected:
        mode, player = variant.rsplit('_', 1)
        game = Game(csvid, name, int(player), mode, rows, oracle)
        game.build()
        cases.append(dict(
            name=f'KickerBattlemagesRecipe_{csvid}_{variant}', card=name, fixture='empty_main',
            seed=2334, setup=game.setup, steps=game.steps,
            _recipe=dict(family=FAMILY, version=2, variant=variant, params=declaration['params'],
                         oracle_text=declaration['oracle_text'], untested_components=[GAP]),
        ))
    return cases

