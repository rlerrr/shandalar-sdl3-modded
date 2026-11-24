"""Paid same-name spells with explicit target, controller, and population receipts."""

import copy
import json
import re
from pathlib import Path

from block_restriction_spells_recipes import _World as _SpellWorld


FAMILY = 'same_name_sweep'
PROFILE = Path(__file__).parent / 'data/same_name_sweep_wave8_profiles.json'
PUMP = re.compile(r'Target creature and all other creatures with the same name as that creature '
                  r'get ([+-][0-9]+)/([+-][0-9]+) until end of turn\.')
DESTROY_NONLAND = 'Destroy target nonland permanent and all other permanents with the same name as that permanent.'
DESTROY_LAND = 'Destroy target land and all other lands with the same name as that land.'
DECLARE = ('Exile target creature and all other creatures its controller controls with the same name as that creature. '
           'That player investigates for each nontoken creature exiled this way.')
HANDLERS = {'destroy_nonland': 'maelstrom_pulse', 'destroy_land': 'wake_of_destruction', 'declaration': 'declaration_in_stone'}
COMMON = ('effect', 'own_target', 'single', 'target_lost', 'countered', 'wrong_color', 'short', 'no_target', 'replay')
EXTRA = {
    'pump': ('survivors', 'expiry', 'protected_peer', 'token_population', 'offturn', 'combat'),
    'destroy_nonland': ('artifact', 'enchantment', 'indestructible', 'token_population'),
    'destroy_land': ('artifact_land', 'indestructible', 'nonbasic'),
    'declaration': ('token_population', 'mixed_nontoken_anchor', 'mixed_token_anchor', 'borrowed_target', 'clue_use'),
}
BEAR = 'Grizzly Bears'
DECOY = 'Runeclaw Bear'
DURABLE = 'Spined Wurm'
SOLDIER = 'Soldier'
CLUE = 'Clue'
WITNESSES = {BEAR, DECOY, DURABLE, 'Elvish Mystic', 'Fountain of Youth', 'Sol Ring', 'Darksteel Relic', 'Waste Not',
             'Forest', 'Island', 'Mountain', 'Tree of Tales', 'Darksteel Citadel', 'Gladecover Scout',
             'Counterspell', 'Boomerang', "Ranger's Guile", 'Raise the Alarm', 'Cackling Counterpart', 'Act of Treason'}
DUMMIES = {BEAR, DECOY, DURABLE}
TOKEN_BINDINGS = {SOLDIER: (18822, 'manalinkeh_card_rhino_token'), CLUE: (18711, 'manalinkeh_card_clue')}


def profiles():
    return json.loads(PROFILE.read_bytes())


def classify(record):
    from recipes import cost_from_oracle

    text = record['oracle_text']
    if record.get('layout') != 'normal' or record.get('card_faces') or record.get('selected_face'):
        raise ValueError('a normal whole same-name spell is required')
    if match := PUMP.fullmatch(text):
        mode, stats = 'pump', [int(match[1]), int(match[2])]
    else:
        mode = {DESTROY_NONLAND: 'destroy_nonland', DESTROY_LAND: 'destroy_land', DECLARE: 'declaration'}.get(text)
        stats = [0, 0]
    if not mode or record['type_line'] != ('Instant' if mode == 'pump' else 'Sorcery'):
        raise ValueError('unsupported complete same-name effect or timing')
    if record['keywords'] != (['Investigate'] if mode == 'declaration' else []):
        raise ValueError('unmodeled same-name keywords')
    if mode == 'pump' and stats not in ([-3, -3], [-2, -2], [2, 2]):
        raise ValueError('unmodeled stat magnitudes for the calibrated creature witnesses')
    return dict(mode=mode, mana=cost_from_oracle(record), power=stats[0], toughness=stats[1], keywords=[])


def handler(params):
    mode = params['mode']
    if mode == 'pump':
        name = {-3: 'bile_blight', -2: 'echoing_decay', 2: 'echoing_courage'}[params['power']]
    else:
        name = HANDLERS[mode]
    return 'manalinkeh_card_' + name


def variants(params):
    return [f'{scenario}_{owner}' for scenario in COMMON + EXTRA[params['mode']] for owner in (0, 1)]


def spec(declaration, record):
    params = classify(record)
    pin = profiles()['sources'].get(record['name'])
    selected = declaration.get('variants')
    if (declaration.get('family') != FAMILY or type(declaration.get('version')) is not int
            or declaration['version'] != 2 or not pin or pin['oracle'] != record
            or declaration.get('oracle_text') != record['oracle_text']
            or json.dumps(declaration.get('params'), sort_keys=True) != json.dumps(params, sort_keys=True)):
        raise ValueError('exact typed same-name spell and Oracle text required')
    if (type(selected) is not list or not selected or any(type(value) is not str for value in selected)
            or len(set(selected)) != len(selected) or not set(selected) <= set(variants(params))):
        raise ValueError('approved distinct sweep variants required')
    return dict(params, family=FAMILY, effect=FAMILY)


def validate_witnesses(oracle, registry):
    pins = profiles()
    if pins.get('version') != 1 or set(pins['witnesses']) != WITNESSES or set(pins['tokens']) != set(TOKEN_BINDINGS):
        raise ValueError('complete independent same-name witness data required')
    for group in ('sources', 'witnesses'):
        for name, pin in pins[group].items():
            row = registry.get(name, {})
            allowed_dummy = group == 'witnesses' and name in DUMMIES and pin.get('reviewed_native_characteristics')
            if (pin['oracle'] != oracle.get(name) or pin['registry'] != row or row.get('kind') != 'card'
                    or row.get('handler') in ('NULL', None) or row['handler'] == 'card_dummy' and not allowed_dummy):
                raise ValueError(f'changed or unsupported same-name binding: {name}')
            if group == 'sources' and row['handler'] != handler(classify(oracle[name])):
                raise ValueError(f'changed same-name source handler: {name}')
    for name, (csvid, callback) in TOKEN_BINDINGS.items():
        pin = pins['tokens'][name]
        if (pin['oracle'] != oracle.get(name) or pin['registry'] != registry.get(name)
                or pin['registry']['csvid'] != csvid or pin['registry']['handler'] != callback):
            raise ValueError(f'changed native generated-token identity: {name}')


class _World(_SpellWorld):
    def __init__(self, name, params, owner, rows, oracle):
        super().__init__(name, params, owner, rows, oracle)
        self.origins = {}
        self.modifiers = {}
        self.produced_soldiers = False

    def add(self, name, ref, player, zone='battlefield'):
        super().add(name, ref, player, zone)
        self.origins[ref] = player

    def receipts(self):
        from recipes import COLORS

        super().receipts()
        roles = []
        if self.produced_soldiers:
            roles.append((SOLDIER, {'power': 1, 'toughness': 1, 'token': 1, 'artifact': 0,
                                    'color': 1 << COLORS['W'], 'subtype_Soldier': 1}))
        if self.p['mode'] == 'declaration':
            roles.append((CLUE, {'token': 1, 'artifact': 1, 'color': 0, 'subtype_Clue': 1}))
        for name, properties in roles:
            csvid = self.rows[name]['csvid']
            for player in (0, 1):
                for zone in ('battlefield', 'graveyard', 'exile'):
                    self.emit('assert_zone_multiplicity', player=player, zone=zone, index=csvid,
                              value=self.zones[zone][player].count(name))
                if self.zones['battlefield'][player].count(name):
                    fields = dict(properties)
                    if name == SOLDIER and self.p['mode'] == 'pump':
                        fields.pop('power')
                        fields.pop('toughness')
                    for prop, value in fields.items():
                        self.emit('assert_matching_permanents', player=player, index=csvid, property=prop, value=value)

    def modify(self, ref, power, toughness):
        prior_power, prior_toughness = self.modifiers.get(ref, (0, 0))
        self.modifiers[ref] = (prior_power + power, prior_toughness + toughness)
        self.entities[ref]['power'] += power
        self.entities[ref]['toughness'] += toughness

    def next_turn(self):
        for ref, (power, toughness) in self.modifiers.items():
            self.entities[ref]['power'] -= power
            self.entities[ref]['toughness'] -= toughness
        self.modifiers.clear()
        super().next_turn()

    def named_token(self, name, ref, player, *, power, toughness):
        index = self.zones['battlefield'][player].count(name)
        self.cards[ref] = (name, player, 'battlefield')
        self.origins[ref] = player
        self.entity(name, ref, player, 'battlefield', token=True)
        self.entities[ref].update(power=power, toughness=toughness, keys=set())
        self.zones['battlefield'][player].append(name)
        self.emit('bind_permanent_copy', ref=ref, player=player, index=index, value=self.rows[name]['csvid'])

    def raise_population(self, ref, actors):
        self.cast(ref)
        self.finish(ref, 'graveyard')
        player = self.cards[ref][1]
        for soldier in actors:
            self.named_token(SOLDIER, soldier, player, power=1, toughness=1)
        self.produced_soldiers = True
        self.receipts()

    def copy_creature(self):
        self.cast('copy_spell', target='target')
        self.finish('copy_spell', 'graveyard')
        name, player, _ = self.cards['target']
        creature = self.entities['target']
        self.named_token(name, 'token_copy', player, power=creature['power'], toughness=creature['toughness'])
        self.receipts()

    def borrow(self):
        self.cast('borrow', target='target')
        self.finish('borrow', 'graveyard')
        name, player, zone = self.cards['target']
        self.zones[zone][player].remove(name)
        self.zones[zone][self.owner].append(name)
        self.cards['target'] = (name, self.owner, zone)
        self.entities['target'].update(player=self.owner, sick=False)
        self.emit('bind_permanent_copy', ref='target', player=self.owner, value=self.rows[name]['csvid'], index=0)
        self.receipts()

    def move_owned(self, ref, zone):
        name, player, _ = self.cards[ref]
        is_token = self.entities.get(ref, {}).get('token', False)
        self.move(ref, zone)
        if is_token:
            self.cards[ref] = (name, player, 'ceased')
            return
        owner = self.origins[ref]
        if player != owner:
            self.zones[zone][player].remove(name)
            self.zones[zone][owner].append(name)
            self.cards[ref] = (name, owner, zone)
            if ref in self.entities:
                self.entities[ref]['player'] = owner

    def affect(self, anchor):
        name, controller, _ = self.cards[anchor]
        mode = self.p['mode']
        group = [ref for ref, (candidate, seat, zone) in self.cards.items()
                 if candidate == name and zone == 'battlefield' and (mode != 'declaration' or seat == controller)]
        for ref in group:
            if mode == 'pump':
                self.modify(ref, self.p['power'], self.p['toughness'])
                if self.entities[ref]['toughness'] <= 0:
                    self.move_owned(ref, 'graveyard')
            elif mode == 'declaration':
                if not self.entities[ref]['token']:
                    self.zones['battlefield'][controller].append(CLUE)
                self.move_owned(ref, 'exile')
            elif 'Indestructible' not in self.o[name]['keywords']:
                self.move_owned(ref, 'graveyard')

    def settle(self, scenario, anchor):
        fee = list(self.p['mana'])
        color = next(index for index in range(1, 6) if fee[index])
        if scenario == 'short':
            fee[0 if fee[0] else color] -= 1
        elif scenario == 'wrong_color':
            fee[color] -= 1
            fee[next(index for index in range(1, 6) if not self.p['mana'][index])] += 1
        denied = scenario in ('short', 'wrong_color', 'no_target')
        self.cast('source', target=None if denied else anchor, denied=denied, budget=fee)
        if not denied:
            if scenario in ('target_lost', 'countered'):
                self.emit('announce_spell', ref='source')
                self.cast('reply', target=anchor if scenario == 'target_lost' else None, parent='source')
                self.finish('reply', 'graveyard', response=True)
                if scenario == 'target_lost':
                    self.move_owned(anchor, 'hand')
                    self.finish('source', 'graveyard')
                else:
                    self.move('source', 'graveyard')
            else:
                self.finish('source', 'graveyard')
                self.affect(anchor)
        self.receipts()

    def use_clue(self, player):
        self.emit('bind_permanent_copy', ref='clue', player=player, index=0, value=self.rows[CLUE]['csvid'])
        self.pool(player, [2, 0, 0, 0, 0, 0])
        self.emit('refresh')
        self.emit('can_activate', ref='clue', value=1)
        self.emit('activate', ref='clue', value=1)
        self.emit('assert_stack', value=1)
        self.mana[player] = [0] * 6
        self.zones['battlefield'][player].remove(CLUE)
        self.emit('assert_zone_multiplicity', player=player, zone='battlefield', index=self.rows[CLUE]['csvid'],
                  value=self.zones['battlefield'][player].count(CLUE))
        self.emit('assert_zone', player=player, zone='library', value=len(self.zones['library'][player]))
        self.emit('resolve')
        self.zones['hand'][player].append(self.zones['library'][player].pop(0))
        self.receipts()


def population(w, scenario):
    mode = w.p['mode']
    if mode == 'destroy_land':
        return {'artifact_land': ('Tree of Tales', 'Sol Ring'), 'indestructible': ('Darksteel Citadel', 'Island'),
                'nonbasic': ('Tree of Tales', 'Forest')}.get(scenario, ('Forest', 'Island'))
    if mode == 'destroy_nonland':
        return {'artifact': ('Fountain of Youth', 'Sol Ring'), 'enchantment': ('Waste Not', 'Tree of Tales'),
                'indestructible': ('Darksteel Relic', 'Fountain of Youth')}.get(scenario, (BEAR, DECOY))
    if scenario in ('survivors', 'expiry', 'protected_peer', 'offturn', 'combat'):
        return DURABLE, 'Elvish Mystic'
    return BEAR, DECOY


def fixture(w, scenario):
    mode, owner, foe = w.p['mode'], w.owner, w.foe
    anchor_seat = owner if scenario == 'own_target' else foe
    other = 1 - anchor_seat
    w.add(w.name, 'source', owner, 'hand')
    if scenario == 'no_target':
        if mode in ('pump', 'declaration'):
            w.add('Gladecover Scout', 'protected', foe)
            w.add('Fountain of Youth', 'wrong_type', owner)
        else:
            name = 'Fountain of Youth' if mode == 'destroy_land' else 'Forest'
            for seat in (0, 1):
                w.add(name, f'wrong_type_{seat}', seat)
        return
    if scenario == 'token_population':
        w.add('Raise the Alarm', 'spawn_target', foe, 'hand')
        w.add('Raise the Alarm', 'spawn_other', owner, 'hand')
        w.add(DECOY, 'unrelated', foe)
        return
    subject, decoy = population(w, scenario)
    w.add(subject, 'target', anchor_seat)
    w.add(decoy, 'unrelated', anchor_seat)
    if scenario != 'single' and not scenario.startswith('mixed_'):
        w.add(subject, 'peer', anchor_seat)
    if scenario not in ('single', 'borrowed_target'):
        w.add(subject, 'other', other)
        if scenario == 'short' and mode == 'destroy_land':
            # The caster's only land must not supply the missing generic mana.
            w.setup[-1]['state'] = ['tapped']
    if scenario.startswith('mixed_'):
        w.add('Cackling Counterpart', 'copy_spell', anchor_seat, 'hand')
    elif scenario == 'borrowed_target':
        w.add('Act of Treason', 'borrow', owner, 'hand')
    elif scenario == 'protected_peer':
        w.add("Ranger's Guile", 'guard', anchor_seat, 'hand')
    elif scenario in ('target_lost', 'countered'):
        w.add('Boomerang' if scenario == 'target_lost' else 'Counterspell', 'reply', foe, 'hand')
    elif scenario == 'clue_use':
        w.add('Mountain', 'top', anchor_seat, 'library')
        w.add('Island', 'next', anchor_seat, 'library')


def build_variant(name, params, scenario, owner, registry, oracle):
    from recipes import step

    w = _World(name, params, owner, registry, oracle)
    w.current = w.foe if scenario == 'offturn' else owner
    fixture(w, scenario)
    w.emit('turn', player=w.current, value=0)
    for seat in (0, 1):
        w.pool(seat, [0] * 6)
    w.receipts()
    if scenario == 'token_population':
        w.raise_population('spawn_target', ('target', 'peer'))
        w.raise_population('spawn_other', ('other', 'other_peer'))
    elif scenario.startswith('mixed_'):
        w.copy_creature()
    elif scenario == 'borrowed_target':
        w.borrow()
    elif scenario == 'protected_peer':
        w.cast('guard', target='peer')
        w.finish('guard', 'graveyard')
        w.modify('peer', 1, 1)
        w.receipts()
    if scenario == 'replay':
        w.emit('capture')
    cut = len(w.steps)
    w.settle(scenario, 'token_copy' if scenario == 'mixed_token_anchor' else 'target')
    if scenario == 'short' and params['mode'] == 'destroy_land':
        w.observe('other', 'tapped', 1)
    if scenario == 'expiry':
        w.next_turn()
    elif scenario == 'clue_use':
        w.use_clue(w.foe)
    elif scenario == 'combat':
        pairs = (('other', 'target'),)
        w.begin_combat(pairs)
        w.emit('begin_blockers')
        w.emit('advance_combat')
        w.damage(pairs, {})
    if scenario == 'replay':
        tail = copy.deepcopy(w.steps[cut:])
        w.steps.extend([step('record_state'), step('ai_roundtrip'), step('restore'), *tail, step('assert_recorded_state')])
    return w.setup, w.steps


def expand(name, csvid, declaration, params, selected, registry, oracle):
    pin = profiles()['sources'].get(name)
    row = registry[name]
    if not pin or pin['registry'] != row or row['csvid'] != csvid or row['handler'] != handler(params):
        raise ValueError(f'unpinned same-name spell: {name}')
    cases = []
    for variant in selected:
        scenario, owner = variant.rsplit('_', 1)
        setup, steps = build_variant(name, params, scenario, int(owner), registry, oracle)
        gaps = ['multiplayer and multiple printed names on one permanent']
        if scenario.startswith('mixed_'):
            gaps.append('an individual token flag within the mixed same-name cohort')
        cases.append(dict(name=f'SameNameSweepRecipe_{csvid}_{variant}', card=name, fixture='empty_main',
                          seed=123, setup=setup, steps=steps,
                          _recipe=dict(family=FAMILY, version=2, variant=variant, params=declaration['params'],
                                       oracle_text=declaration['oracle_text'], untested_components=gaps)))
    return cases
