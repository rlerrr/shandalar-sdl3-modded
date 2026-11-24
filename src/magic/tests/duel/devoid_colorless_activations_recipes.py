"""Devoid permanents with a real colorless payment or colorless mana output."""

from collections import Counter
from functools import lru_cache
import json
from pathlib import Path
import re

from devoid_case_support import DevoidBoard, devoid_rules
from library_search_recipes import wave6_exact as exact


FAMILY = 'devoid_colorless_activations'
PROFILE = Path(__file__).parent / 'data' / f'{FAMILY}_wave8_profiles.json'
COST = re.compile(r'^(?:\{(\d+)\})?\{C\}: (.+) \(\{C\} represents colorless mana\.\)$')
PUMP = re.compile(r'^This creature gets ([+-]\d+)/([+-]\d+) until end of turn\.( Activate only once each turn\.)?$')
TRANSLATOR = 'Pay 1 life: Add {C}. Activate only once each turn. ({C} represents colorless mana.)'
ELSEWHERE = {'Target opponent loses 1 life and you gain 1 life.': 'drain', 'Draw a card.': 'draw'}
FOLLOW_RULES = {
    ("{C}: This creature gets +1/+0 and gains menace until end of turn. (It can't be blocked except by two or more creatures. {C} represents colorless mana.)",): ('menace', 0),
    ('{C}: This creature gains deathtouch until end of turn. (Any amount of damage it deals to a creature is enough to destroy it. {C} represents colorless mana.)',): ('deathtouch', 0),
    ('{C}, {T}: Tap target creature. ({C} represents colorless mana.)',): ('tap', 0),
    ('Flash', 'Flying', "{1}{C}: Target opponent exiles the top card of their library. If it's a land card, you may return this creature to its owner's hand. ({C} represents colorless mana.)"): ('exile_top', 1),
    ("{3}{C}, Exile a creature card from your graveyard: Target opponent loses life equal to the exiled card's power. ({C} represents colorless mana.)",): ('grave_drain', 3),
    ("This creature can't be blocked.", '{3}{C}: Target opponent exiles a card from their hand. Activate only as a sorcery. ({C} represents colorless mana.)'): ('exile_hand', 3),
}
FOLLOW_MODES = {
    'menace': ('lone_blocker', 'two_blockers', 'menace_expiry'),
    'deathtouch': ('fatal_block', 'deathtouch_expiry'),
    'tap': ('tap_own', 'sick_denied', 'tapped_denied'),
    'exile_top': ('flash', 'flying', 'return', 'decline', 'empty_library'),
    'grave_drain': ('smaller_cost', 'opponent_grave_only'),
    'exile_hand': ('unblockable', 'empty_hand', 'offturn_denied'),
}
WITNESSES = {'Bonesplitter', 'Forest', 'Island', 'Fog Bank', 'Goblin Tinkerer', 'Krosan Tusker',
             'Terra Stomper', 'Welkin Tern'}


@lru_cache(maxsize=1)
def profiles():
    return json.loads(PROFILE.read_bytes())


def classify(record):
    from recipes import cost_from_oracle

    lines = devoid_rules(record)
    if not record['type_line'].startswith('Creature — '):
        raise ValueError('Whole Devoid activated creature Oracle required')
    base = [int(record[k]) for k in ('power', 'toughness')]
    shape = dict(price=cost_from_oracle(record), base=base)
    follower = FOLLOW_RULES.get(tuple(lines))
    if follower:
        effect, generic = follower
        keywords = {'Devoid', 'Flying', 'Flash'} if effect == 'exile_top' else {'Devoid'}
        if set(record['keywords']) != keywords:
            raise ValueError('Exact complete Devoid follow-on keyword set required')
        shape.update(effect=effect, generic=generic, colorless=1, once=False)
        if effect == 'menace':
            shape['delta'] = [1, 0]
        return shape
    if len(lines) != 1 or set(record['keywords']) != {'Devoid'}:
        raise ValueError('Whole Devoid activated creature Oracle required')
    if lines[0] == TRANSLATOR:
        return dict(shape, effect='mana', generic=0, colorless=0, once=True, life_cost=1, output=1)
    cost = COST.fullmatch(lines[0])
    if not cost:
        raise ValueError('Exact explicit colorless activation fee required')
    shape.update(generic=int(cost[1] or 0), colorless=1)
    if cost[2] in ELSEWHERE:
        return dict(shape, effect=ELSEWHERE[cost[2]], once=False)
    boost = PUMP.fullmatch(cost[2])
    if boost:
        return dict(shape, effect='pump', delta=[int(boost[1]), int(boost[2])], once=bool(boost[3]))
    raise ValueError('Unsupported full Devoid activation rider')


def modes(model):
    effect, once = model['effect'], model['once']
    if effect == 'mana':
        return ('immediate', 'once', 'reset', 'replay')
    common = ['exact', 'colored_only', 'short', 'replay']
    if model['generic']:
        common.append('mixed')
    if effect in FOLLOW_MODES:
        return (*common, *FOLLOW_MODES[effect])
    if effect in ('drain', 'draw'):
        return (*common, 'repeat' if effect == 'draw' else 'offturn')
    if once:
        return (*common, 'once', 'reset')
    return (*common, 'cleanup', 'lethal' if model['delta'][1] < 0 else 'repeat')


def variants(model):
    return [f'{mode}_{seat}' for mode in modes(model) for seat in (0, 1)]


def spec(declaration, record):
    name = (record or {}).get('name')
    pin = profiles()['sources'].get(name)
    if not pin or not exact(record, pin['oracle']):
        raise ValueError('Complete pinned Devoid Oracle record required')
    model = classify(record)
    picked = declaration.get('variants')
    if (declaration.get('family') != FAMILY or type(declaration.get('version')) is not int
            or declaration['version'] != 2 or declaration.get('oracle_text') != record['oracle_text']
            or not exact(declaration.get('params'), model) or not isinstance(picked, list) or not picked
            or any(type(v) is not str for v in picked) or len(set(picked)) != len(picked)
            or set(picked) - set(variants(model))):
        raise ValueError('Exact Devoid activation model and distinct scoped variants required')
    return dict(model, family=FAMILY)


def validate_witnesses(oracle, rows):
    pins = profiles()
    if set(pins) != {'sources', 'witnesses'} or set(pins['witnesses']) != WITNESSES:
        raise ValueError('Complete independent Devoid witness pins required')
    for group in pins.values():
        for name, pin in group.items():
            record, row = oracle.get(name, {}), rows.get(name, {})
            if (not exact(pin, {'oracle': record, 'registry': row}) or record.get('layout') != 'normal'
                    or row.get('kind') != 'card' or row.get('handler') in (None, 'NULL', '0', 'card_dummy')):
                raise ValueError(f'Pinned implemented Devoid source or witness changed: {name}')


class ActivationBoard(DevoidBoard):
    def __init__(self, name, shape, mode, owner, rows, oracle):
        self.life = [20, 20]
        self.pump_count = 0
        self.library = []
        self.snapshot = None
        super().__init__(name, shape, mode, owner, rows, oracle)

    def build(self):
        if self.shape['effect'] == 'draw':
            for index, name in enumerate(('Forest', 'Island')):
                ref = f'draw_{index}'
                self.add(name, ref, self.owner, 'library')
                self.library.append(ref)
        if self.shape['effect'] == 'mana' and self.mode == 'reset':
            self.add('Bonesplitter', 'mana_sink', self.owner, 'hand')

    def stat(self, index):
        return self.shape['base'][index] + self.pump_count * self.shape.get('delta', (0, 0))[index]

    def checks(self, *, pending=False):
        quantities = Counter((card['player'], card['zone'], self.rows[card['name']]['csvid']) for card in self.cards.values())
        for who in (0, 1):
            self.emit('assert_life', player=who, value=self.life[who])
            for zone in ('battlefield', 'hand', 'library', 'graveyard', 'exile'):
                total = sum(n for (seat, place, _), n in quantities.items() if seat == who and place == zone)
                self.emit('assert_zone', player=who, zone=zone, value=total)
        for (who, zone, csvid), amount in sorted(quantities.items()):
            self.emit('assert_zone_multiplicity', player=who, zone=zone, index=csvid, value=amount)
        for index, ref in enumerate(self.library):
            self.emit('assert_zone_card', player=self.owner, zone='library', index=index,
                      value=self.rows[self.cards[ref]['name']]['csvid'])
        if self.cards['source']['zone'] == 'battlefield':
            for index, attr in enumerate(('power', 'toughness')):
                self.emit('assert_card', ref='source', property=attr, value=self.stat(index))
            self.emit('assert_card', ref='source', property='tapped', value=int(self.cards['source']['tapped']))
        self.emit('assert_stack', value=int(pending))
        self.observe_mana()
        self.emit('assert_choices_consumed')

    def denied(self):
        self.emit('can_activate', ref='source', value=0)
        self.emit('activate', ref='source', value=0)
        self.checks()

    def plan_activation(self):
        """Queue an activation choice after any mana payment clicks."""

    def apply_additional_costs(self):
        """Track nonmana costs only after paid activation."""

    def plan_resolution(self):
        """Queue choices offered only by the resolver."""

    def effect_resolved(self):
        effect = self.shape['effect']
        if effect == 'drain':
            self.life[self.owner] += 1
            self.life[self.foe] -= 1
        elif effect == 'draw':
            self.cards[self.library.pop(0)]['zone'] = 'hand'
        else:
            self.pump_count += 1
            if self.stat(1) <= 0:
                self.cards['source']['zone'] = 'graveyard'

    def activate(self, *, generic_color=0):
        effect = self.shape['effect']
        generic, colorless = self.shape['generic'], self.shape['colorless']
        self.emit('can_activate', ref='source', value=1)
        if generic_color and self.owner == 0:
            for _ in range(generic):
                self.emit('choose_mana', player=self.owner, color=generic_color)
        self.plan_activation()
        self.emit('activate', ref='source', value=1)
        if effect == 'mana':
            self.life[self.owner] -= self.shape['life_cost']
            self.pools[self.owner][0] += self.shape['output']
            self.emit('refresh')
            self.checks()
            return
        self.pools[self.owner][0] -= colorless
        self.pools[self.owner][generic_color] -= generic
        self.apply_additional_costs()
        self.checks(pending=True)
        if self.mode == 'replay':
            self.snapshot = len(self.steps)
        self.plan_resolution()
        self.emit('resolve')
        self.emit('refresh')
        self.effect_resolved()
        self.checks()

    def turn_boundary(self):
        if any(map(any, self.pools)):
            raise ValueError('Consume floating mana with a real action before advancing')
        self.emit('advance_turn')
        self.pump_count = 0
        self.checks()

    def mana_rider(self):
        if self.mode == 'replay':
            self.snapshot = len(self.steps)
        self.activate()
        if self.mode == 'once':
            self.denied()
        if self.mode == 'reset':
            # The actual mana produced by Translator pays for Bonesplitter.
            self.emit('cast', ref='mana_sink', value=1)
            self.emit('assert_stack', value=1)
            self.pools[self.owner][0] -= 1
            self.observe_mana()
            self.emit('resolve')
            self.cards['mana_sink']['zone'] = 'battlefield'
            self.emit('refresh')
            self.turn_boundary()
            self.activate()

    def run(self):
        self.play_source()
        self.checks()
        mode = self.mode
        generic, colorless = self.shape['generic'], self.shape['colorless']
        if self.shape['effect'] == 'mana':
            self.mana_rider()
            return
        if mode in ('colored_only', 'short'):
            if mode == 'colored_only':
                self.fund_colorless(0, colorless + generic, generic_color=1)
            else:
                self.fund_colorless(colorless + generic - 1)
            self.denied()
            return
        if mode == 'offturn':
            self.turn_boundary()
        generic_color = int(mode == 'mixed')
        activations = 2 if mode == 'repeat' else 1
        if mode == 'lethal':
            loss = -self.shape['delta'][1]
            activations = (self.stat(1) + loss - 1) // loss
        for _ in range(activations):
            self.fund_colorless(colorless, generic, generic_color=generic_color)
            self.activate(generic_color=generic_color)
        if mode in ('cleanup', 'reset'):
            self.turn_boundary()
        if mode in ('once', 'reset'):
            self.fund_colorless(colorless, generic)
            self.denied() if mode == 'once' else self.activate()


class FollowerBoard(ActivationBoard):
    """Add real combat and zone effects to the established colorless fee path."""

    def __init__(self, name, shape, mode, owner, rows, oracle):
        self.foe_library = []
        super().__init__(name, shape, mode, owner, rows, oracle)

    @property
    def effect(self):
        return self.shape['effect']

    def build(self):
        mode, effect = self.mode, self.effect
        if effect == 'menace' and mode in FOLLOW_MODES[effect]:
            if mode == 'two_blockers':
                self.add('Fog Bank', 'blocker', self.foe)
                self.add('Fog Bank', 'blocker_two', self.foe)
            else:
                self.add('Goblin Tinkerer', 'blocker', self.foe)
        elif effect == 'deathtouch' and mode in FOLLOW_MODES[effect]:
            self.add('Terra Stomper', 'blocker', self.foe)
        elif effect == 'tap':
            self.add('Goblin Tinkerer', 'target', self.owner if mode == 'tap_own' else self.foe)
        elif effect == 'exile_top':
            if mode == 'flying':
                self.add('Goblin Tinkerer', 'blocker', self.foe)
                self.add('Welkin Tern', 'flier', self.foe)
            if mode not in ('flash', 'flying', 'empty_library'):
                self.add('Forest' if mode in ('return', 'decline') else 'Goblin Tinkerer', 'top', self.foe, 'library')
                self.add('Island', 'next_top', self.foe, 'library')
                self.foe_library.extend(('top', 'next_top'))
        elif effect == 'grave_drain':
            if mode != 'opponent_grave_only':
                self.add('Krosan Tusker', 'large_cost', self.owner, 'graveyard')
                self.add('Goblin Tinkerer', 'small_cost', self.owner, 'graveyard')
            self.add('Forest', 'own_land', self.owner, 'graveyard')
            self.add('Terra Stomper', 'foreign_creature', self.foe, 'graveyard')
        elif effect == 'exile_hand':
            if mode != 'empty_hand':
                self.add('Forest', 'hand_cost', self.foe, 'hand')
                self.add('Island', 'other_hand', self.foe, 'hand')
            if mode == 'unblockable':
                self.add('Goblin Tinkerer', 'blocker', self.foe)

    def checks(self, *, pending=False):
        super().checks(pending=pending)
        for index, ref in enumerate(self.foe_library):
            self.emit('assert_zone_card', player=self.foe, zone='library', index=index,
                      value=self.rows[self.cards[ref]['name']]['csvid'])
        for ref, card in self.cards.items():
            if card['zone'] != 'battlefield':
                continue
            if ref != 'source':
                self.emit('assert_card', ref=ref, property='tapped', value=int(card['tapped']))
            self.emit('assert_card', ref=ref, property='damage', value=card['damage'])

    def grave_cost(self):
        return 'small_cost' if self.mode == 'smaller_cost' else 'large_cost'

    def plan_activation(self):
        if self.effect == 'tap':
            self.emit('choose_target', ref='target', player=self.owner)
        elif self.effect == 'grave_drain':
            self.emit('choose_zone_card', player=self.owner, value=self.rows[self.cards[self.grave_cost()]['name']]['csvid'])

    def apply_additional_costs(self):
        if self.effect == 'tap':
            self.cards['source']['tapped'] = True
        elif self.effect == 'grave_drain':
            self.cards[self.grave_cost()]['zone'] = 'exile'

    def plan_resolution(self):
        if self.effect == 'exile_top' and self.foe_library:
            self.emit('choose_zone', player=self.owner, value=-1)
            if self.mode in ('return', 'decline'):
                self.emit('choose_dialog', player=self.owner, value=int(self.mode == 'decline'))
        elif self.effect == 'exile_hand' and 'hand_cost' in self.cards:
            self.emit('choose_zone_card', player=self.foe, value=self.rows['Forest']['csvid'])

    def effect_resolved(self):
        if self.effect in ('menace', 'deathtouch'):
            self.pump_count += 1
        elif self.effect == 'tap':
            self.cards['target']['tapped'] = True
        elif self.effect == 'exile_top' and self.foe_library:
            self.cards[self.foe_library.pop(0)]['zone'] = 'exile'
            if self.mode == 'return':
                self.cards['source']['zone'] = 'hand'
        elif self.effect == 'grave_drain':
            creature = self.cards[self.grave_cost()]['name']
            self.life[self.foe] -= int(self.oracle[creature]['power'])
        elif self.effect == 'exile_hand' and 'hand_cost' in self.cards:
            self.cards['hand_cost']['zone'] = 'exile'

    def ready(self):
        self.turn_boundary()
        self.turn_boundary()
        self.emit('assert_card', ref='source', property='sick', value=0)

    def flash_source(self):
        # This is the first main phase of the opponent's actual fixture turn.
        self.emit('turn', player=self.foe)
        self.fund(self.foe, [0] * 6)
        self.cast('source')
        self.emit('resolve')
        self.cards['source']['zone'] = 'battlefield'
        self.emit('refresh')
        self.emit('assert_matching_permanents', player=self.owner,
                  index=self.rows[self.cards['source']['name']]['csvid'], property='color', value=0)

    def combat(self):
        mode = self.mode
        self.emit('begin_attackers')
        self.emit('attack', ref='source', value=1)
        self.cards['source']['tapped'] = True
        self.emit('finish_attackers')
        self.emit('begin_blockers')
        first = mode not in ('lone_blocker', 'flying', 'unblockable')
        self.emit('block', ref='blocker', other='source', value=int(first))
        other = 'blocker_two' if mode == 'two_blockers' else 'flier' if mode == 'flying' else None
        if other:
            self.emit('block', ref=other, other='source', value=1)
        self.emit('advance_combat')
        self.emit('assert_card', ref='source', property='blocked', value=int(first or other is not None))
        self.emit('assert_card', ref='blocker', property='blocking_count', value=int(first))
        if other:
            self.emit('assert_card', ref=other, property='blocking_count', value=1)
        if mode == 'two_blockers' and self.owner == 0:
            # The human combat picker assigns one point per click. Split four damage across the two Fog Banks.
            for ref in ('blocker', 'blocker', 'blocker_two', 'blocker_two'):
                self.emit('choose_target', ref=ref, player=self.owner)
        self.emit('combat_damage')
        self.emit('resolve')
        self.emit('end_combat')

        if mode in ('lone_blocker', 'unblockable'):
            self.life[self.foe] -= self.stat(0)
        elif mode in ('fatal_block', 'flying'):
            self.cards['source']['zone'] = 'graveyard'
            self.cards['blocker' if mode == 'fatal_block' else 'flier']['zone'] = 'graveyard'
        elif mode == 'deathtouch_expiry':
            self.cards['source']['zone'] = 'graveyard'
            self.cards['blocker']['damage'] = self.stat(0)
        elif mode == 'menace_expiry':
            self.cards['source']['damage'] = 1
            self.cards['blocker']['zone'] = 'graveyard'
        # In the two-blocker variant Fog Banks prevent all combat damage to them.
        self.checks()

    def run(self):
        mode = self.mode
        if mode == 'flash':
            self.flash_source()
            self.checks()
            return
        self.play_source()
        if mode in ('return', 'decline'):
            self.emit('present_ai_dialogs', value=1)
        self.checks()
        if self.effect == 'tap' and mode != 'sick_denied':
            self.ready()
        if mode == 'offturn_denied':
            self.turn_boundary()
        if mode in ('flying', 'unblockable'):
            self.ready()
            self.combat()
            return
        combat_modes = ('lone_blocker', 'two_blockers', 'menace_expiry', 'fatal_block', 'deathtouch_expiry')
        if mode in combat_modes:
            self.ready()
        generic, colorless = self.shape['generic'], self.shape['colorless']
        if mode == 'colored_only':
            self.fund_colorless(0, colorless + generic, generic_color=1)
        elif mode == 'short':
            self.fund_colorless(colorless + generic - 1)
        else:
            self.fund_colorless(colorless, generic, generic_color=int(mode == 'mixed'))
        if mode in ('colored_only', 'short', 'sick_denied', 'opponent_grave_only', 'offturn_denied'):
            self.denied()
            return
        self.activate(generic_color=int(mode == 'mixed'))
        if mode in ('menace_expiry', 'deathtouch_expiry'):
            self.ready()
        if mode in combat_modes:
            self.combat()
        elif mode == 'tapped_denied':
            self.fund_colorless(colorless, generic)
            self.denied()


def expand(name, csvid, declaration, shape, selected, rows, oracle):
    from recipes import step

    validate_witnesses(oracle, rows)
    if name not in profiles()['sources'] or rows[name]['csvid'] != csvid or set(selected) - set(variants(shape)):
        raise ValueError('Reserved full Devoid activation variant required')
    result = []
    for variant in selected:
        mode, owner = variant.rsplit('_', 1)
        board_type = FollowerBoard if shape['effect'] in FOLLOW_MODES else ActivationBoard
        board = board_type(name, shape, mode, int(owner), rows, oracle)
        board.run()
        steps = board.steps
        if board.snapshot is not None:
            cut = board.snapshot
            suffix = steps[cut:]
            steps = steps[:cut] + [step('capture')] + suffix + [step('record_state'), step('ai_roundtrip'),
                    step('restore')] + suffix + [step('assert_recorded_state')]
        gaps = ['Spell-color characteristics while on the stack or in non-battlefield zones']
        if shape['effect'] == 'exile_top':
            gaps.append('Control transfer before optional return to the owner’s hand')
        metadata = dict(family=FAMILY, version=2, variant=variant, params=declaration['params'],
                        oracle_text=declaration['oracle_text'], covered_clauses=declaration['oracle_text'].splitlines(),
                        untested_components=gaps)
        result.append(dict(name=f'DevoidColorlessActivationsRecipe_{csvid}_{variant}', card=name, fixture='empty_main',
                           seed=2172, setup=board.setup, steps=steps, _recipe=metadata))
    return result
