"""Actual land mana production with a symmetric, one-mana bonus per source."""

import copy
import json
from pathlib import Path

from attack_value_trigger_recipes import _AttackResourceWorld


FAMILY = 'global_land_mana_multiplier'
PROFILE = Path(__file__).parent / 'data/global_land_mana_multiplier_wave8_profiles.json'
RULE = 'Whenever a player taps a land for mana, that player adds one mana of any type that land produced.'
DAMAGE_RULE = RULE[:-1] + ', and this enchantment deals 1 damage to the player.'
HANDLERS = {
    'plain': 'manalinkeh_card_mana_flare',
    'flash': 'manalinkeh_card_dictate_of_karametra',
    'damage': 'manalinkeh_card_overabundance',
}
WITNESSES = (
    'Forest', 'Savannah', 'Wastes', 'City of Traitors', 'Mossfire Valley', 'Elvish Mystic',
    'Sol Ring', 'Twiddle', 'Disenchant', 'Murder', 'Safe Passage', 'Kalonian Tusker', 'Hill Giant',
)
PRODUCERS = {
    'Forest': ('G', True),
    'Savannah': ('W', True),
    'Wastes': ('C', True),
    'City of Traitors': ('CC', True),
    'Mossfire Valley': ('RG', True),
    'Elvish Mystic': ('G', False),
    'Sol Ring': ('CC', False),
}
COMMON = (
    'owner_land', 'opponent_land', 'colorless_land', 'two_colorless', 'selected_white',
    'mixed_red', 'mixed_green', 'opponent_mixed', 'nonland_mana', 'nonmana_tap',
    'while_pending', 'source_removed', 'two_multipliers', 'spend_pool', 'pay_during', 'replay',
)


def classify(record):
    from recipes import cost_from_oracle

    if (record.get('layout') != 'normal' or record.get('card_faces') or record.get('selected_face')
            or record.get('type_line') not in ('Enchantment', 'Creature — Beast')):
        raise ValueError('whole normal multiplier permanent required')
    text = record.get('oracle_text')
    mode = {RULE: 'plain', 'Flash\n' + RULE: 'flash', DAMAGE_RULE: 'damage'}.get(text)
    creature = record['type_line'] == 'Creature — Beast'
    if (mode is None or record.get('keywords') != (['Flash'] if mode == 'flash' else [])
            or creature and (mode != 'plain' or (record.get('power'), record.get('toughness')) != ('7', '5'))):
        raise ValueError('unsupported multiplier clause or printed characteristic')
    return dict(mode=mode, creature=creature, mana=cost_from_oracle(record))


def spec(declaration, record):
    params = classify(record)
    supplied = declaration.get('params')
    if (declaration.get('family') != FAMILY or type(declaration.get('version')) is not int
            or declaration['version'] != 2 or declaration.get('oracle_text') != record['oracle_text']
            or not isinstance(supplied, dict) or supplied != params or type(supplied.get('creature')) is not bool
            or any(type(amount) is not int for amount in supplied['mana']) or 'variants' not in declaration):
        raise ValueError('exact Oracle text, typed mana and multiplier profile required')
    return dict(params, family=FAMILY)


def variants(params):
    modes = list(COMMON)
    if params['mode'] == 'damage':
        modes.append('prevent_damage')
    if params['mode'] == 'flash':
        modes.append('flash_response')
    if params['creature']:
        modes.append('creature_combat')
    return [f'{mode}_{owner}' for mode in modes for owner in (0, 1)]


def pins():
    return json.loads(PROFILE.read_bytes())


def validate_witnesses(oracle, registry):
    profile = pins()
    if profile.get('version') != 1 or set(profile['witnesses']) != set(WITNESSES):
        raise ValueError('incomplete independently pinned mana and action witnesses')
    for name, pin in profile['witnesses'].items():
        if pin['oracle'] != oracle.get(name) or pin['registry'] != registry.get(name):
            raise ValueError(f'pinned witness changed: {name}')
        if registry[name]['handler'] == 'card_dummy' and not pin.get('reviewed_native_characteristics'):
            raise ValueError(f'unreviewed native witness: {name}')


def pool_for(symbols):
    return [symbols.count(symbol) for symbol in 'CBUGRW']


class _World(_AttackResourceWorld):
    def __init__(self, name, params, owner, registry, oracle):
        super().__init__(name, dict(params, effect='mana', keywords=[]), owner, registry, oracle)
        self.objects = {}
        self.active_sources = set()
        self.prevented = set()

    def add(self, name, ref, player, zone='battlefield', source=False):
        super().add(name, ref, player, zone, source=source)
        self.objects[ref] = (name, player, zone, False)

    def move(self, ref, zone):
        name, player, old, _ = self.objects[ref]
        if ref in self.entities:
            if zone == 'battlefield':
                self.arrive(ref)
            else:
                self.leave(ref, zone)
        else:
            self.zones[old][player].remove(name)
            self.zones[zone][player].append(name)
        self.objects[ref] = (name, player, zone, False)

    def observe(self, ref, prop, value):
        self.emit('assert_card', ref=ref, property=prop, value=value)

    def resource_receipts(self, stack=0):
        self.emit('assert_stack', value=stack)
        self.emit('assert_choices_consumed')
        for player in (0, 1):
            self.emit('assert_life', player=player, value=self.life[player])
            for color, count in enumerate(self.mana[player]):
                self.emit('assert_mana', player=player, color=color, value=count)

    def receipts(self):
        super().receipts()
        for ref, (name, _, zone, tapped) in self.objects.items():
            if ref not in self.entities and zone == 'battlefield':
                self.observe(ref, 'in_play', 1)
                self.observe(ref, 'tapped', int(tapped))

    def begin_cast(self, ref, from_current_pool=False, response=False):
        from recipes import cost_from_oracle

        name, player, _, _ = self.objects[ref]
        price = cost_from_oracle(self.o[name])
        if not from_current_pool:
            self.pool(player, price)
        self.emit('cast_response' if response else 'cast', ref=ref, value=1)
        self.mana[player] = [a - b for a, b in zip(self.mana[player], price)]
        self.observe(ref, 'in_hand', 0)
        self.observe(ref, 'in_play', 0)

    def cast_source(self, ref='source'):
        self.begin_cast(ref)
        self.resource_receipts(stack=1)
        self.emit('resolve')
        self.move(ref, 'battlefield')
        self.active_sources.add(ref)
        self.receipts()

    def _credit(self, ref, extra=''):
        name, player, zone, _ = self.objects[ref]
        output, land = PRODUCERS[name]
        multiplier_count = len(self.active_sources) if land else 0
        if multiplier_count and len(set(output)) > 1 and extra not in output:
            raise ValueError('distinct mana requires a choice among actual outputs')
        symbols = output + (extra or output[0]) * multiplier_count
        self.mana[player] = [a + b for a, b in zip(self.mana[player], pool_for(symbols))]
        if name == 'Mossfire Valley':
            self.mana[player][0] -= 1
        if self.p['mode'] == 'damage' and player not in self.prevented:
            self.life[player] -= multiplier_count
        self.objects[ref] = (name, player, zone, True)
        if ref in self.entities:
            self.entities[ref]['tapped'] = True

    def tap(self, ref, extra='', stack=0):
        from recipes import COLORS

        name, player, _, _ = self.objects[ref]
        if name == 'Mossfire Valley':
            self.pool(player, pool_for('C'))
        self.emit('refresh')
        self.emit('require_can_activate', ref=ref, value=1)
        if name == 'Savannah':
            self.emit('choose_color', player=player, value=COLORS['W'])
        if extra:
            # The foreign mixed-output case observes the wallet even if the
            # implementation presents its color selector to the other seat.
            chooser = player if player == self.owner else -1
            self.emit('choose_color', player=chooser, value=COLORS[extra])
        self.emit('activate', ref=ref, value=1)
        self._credit(ref, extra)
        self.resource_receipts(stack)
        self.observe(ref, 'tapped', 1)

    def paid_cast(self, during_payment):
        player = self.owner
        if during_payment:
            self.emit('refresh')
            if player == 0:
                self.emit('choose_mana_source', ref='producer')
            self._credit('producer')
        else:
            self.tap('producer')
        self.begin_cast('spender', from_current_pool=True)
        self.resource_receipts(stack=1)
        self.emit('resolve')
        self.move('spender', 'battlefield')

    def flash_response(self):
        self.begin_cast('parent')
        self.resource_receipts(stack=1)
        self.emit('announce_spell', ref='parent')
        self.begin_cast('source', response=True)
        self.resource_receipts(stack=2)
        self.emit('announce_spell', ref='source')
        self.emit('resolve_response')
        self.move('source', 'battlefield')
        self.move('parent', 'battlefield')
        self.active_sources.add('source')
        self.receipts()
        self.tap('producer')

    def creature_combat(self):
        self.emit('begin_attackers')
        self.emit('attack', ref='source', value=0)
        self.emit('finish_attackers')
        self.age()
        self.emit('begin_attackers')
        self.emit('attack', ref='source', value=1)
        self.emit('finish_attackers')
        self.emit('begin_blockers')
        self.emit('advance_combat')
        self.observe('source', 'power', 7)
        self.observe('source', 'toughness', 5)
        self.emit('combat_damage')
        self.emit('resolve')
        self.emit('end_combat')
        self.life[1 - self.owner] -= 7
        self.entities['source']['tapped'] = True
        self.tap('producer')


def build_variant(name, params, scenario, owner, registry, oracle):
    w = _World(name, params, owner, registry, oracle)
    enemy = 1 - owner
    w.add(name, 'source', owner, 'hand')
    producer = {
        'colorless_land': 'Wastes',
        'two_colorless': 'City of Traitors',
        'selected_white': 'Savannah',
        'mixed_red': 'Mossfire Valley',
        'mixed_green': 'Mossfire Valley',
        'opponent_mixed': 'Mossfire Valley',
        'nonland_mana': 'Elvish Mystic',
    }.get(scenario, 'Forest')
    player = enemy if scenario in ('opponent_land', 'opponent_mixed', 'flash_response', 'prevent_damage') else owner
    w.add(producer, 'producer', player)
    if scenario in ('while_pending', 'source_removed'):
        w.add('Forest', 'second_land', owner)
    if scenario == 'nonland_mana':
        w.add('Sol Ring', 'second_source', enemy)
    elif scenario == 'nonmana_tap':
        w.add('Twiddle', 'twiddle', owner, 'hand')
    elif scenario == 'two_multipliers':
        w.add(name, 'second', owner, 'hand', source=True)
    elif scenario == 'source_removed':
        w.add('Murder' if params['creature'] else 'Disenchant', 'removal', enemy, 'hand')
    elif scenario in ('spend_pool', 'pay_during'):
        w.add('Kalonian Tusker', 'spender', owner, 'hand')
    elif scenario == 'prevent_damage':
        w.add('Safe Passage', 'shield', enemy, 'hand')
    elif scenario == 'flash_response':
        w.add('Hill Giant', 'parent', enemy, 'hand')
    first_caster = enemy if scenario == 'flash_response' else owner
    w.emit('turn', player=first_caster, value=0)
    # begin_cast installs the payer's exact pool. Remove the other seat's fixture mana.
    w.pool(1 - first_caster, [0] * 6)

    if scenario == 'flash_response':
        w.flash_response()
    elif scenario == 'while_pending':
        w.begin_cast('source')
        w.resource_receipts(stack=1)
        w.tap('producer', stack=1)
        w.emit('resolve')
        w.move('source', 'battlefield')
        w.active_sources.add('source')
        w.receipts()
        w.tap('second_land')
    else:
        w.cast_source()
        if scenario == 'nonland_mana':
            w.tap('producer')
            w.tap('second_source')
        elif scenario == 'nonmana_tap':
            w.spell('Twiddle', 'twiddle', owner, 'producer', dialog=0)
            w.objects['producer'] = (producer, player, 'battlefield', True)
        elif scenario == 'source_removed':
            w.tap('producer')
            w.spell('Murder' if params['creature'] else 'Disenchant', 'removal', enemy, 'source')
            w.move('source', 'graveyard')
            w.active_sources.clear()
            w.tap('second_land')
        elif scenario == 'two_multipliers':
            w.cast_source('second')
            w.tap('producer')
        elif scenario in ('spend_pool', 'pay_during'):
            w.paid_cast(during_payment=scenario == 'pay_during')
        elif scenario == 'prevent_damage':
            w.pool(enemy, pool_for('CCW'))
            w.begin_cast('shield', from_current_pool=True)
            w.resource_receipts(stack=1)
            w.emit('resolve')
            w.move('shield', 'graveyard')
            w.prevented.add(enemy)
            w.tap('producer')
        elif scenario == 'creature_combat':
            w.creature_combat()
        else:
            extra = {'mixed_red': 'R', 'mixed_green': 'G', 'opponent_mixed': 'R'}.get(scenario, '')
            if scenario == 'replay':
                from recipes import step

                steps_before = len(w.steps)
                w.tap('producer')
                w.receipts()
                tail = copy.deepcopy(w.steps[steps_before:])
                w.steps[steps_before:steps_before] = [step('capture')]
                w.emit('record_state')
                w.emit('ai_roundtrip')
                w.emit('restore')
                w.steps.extend(tail)
                w.emit('assert_recorded_state')
            else:
                w.tap('producer', extra)
    w.receipts()
    return w.setup, w.steps


def expand(name, csvid, declaration, params, selected, registry, oracle):
    pin = pins()['sources'].get(name)
    row = registry[name]
    if (not pin or pin['oracle'] != oracle[name] or pin['registry'] != row or row['csvid'] != csvid
            or row['kind'] != 'card' or row['handler'] != HANDLERS[params['mode']]):
        raise ValueError(f'unsupported or unpinned source: {name}')
    result = []
    for variant in selected:
        scenario, owner = variant.rsplit('_', 1)
        setup, steps = build_variant(name, params, scenario, int(owner), registry, oracle)
        gaps = ['multiplayer and replacement-effect ordering']
        if scenario == 'opponent_mixed':
            gaps.append('choice actor for foreign mixed output; both mana wallets remain explicit')
        if params['mode'] == 'flash':
            gaps.append('automatic response-menu discovery; the off-turn case uses the real explicit response bridge')
        result.append(dict(
            name=f'GlobalLandManaMultiplierRecipe_{csvid}_{variant}', card=name,
            fixture='empty_main', seed=123, setup=setup, steps=steps,
            _recipe=dict(family=FAMILY, version=2, variant=variant, params=declaration['params'],
                         oracle_text=declaration['oracle_text'], untested_components=gaps)))
    return result
