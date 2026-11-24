"""Fixed required attackers through the production declaration check."""

import json
import re
from collections import Counter
from pathlib import Path

from cast_untap_trigger_recipes import exact, pin


FAMILY = 'required_attack_creature'
DATA = Path(__file__).parent / 'data/required_attack_creature_wave8_profiles.json'
CLAUSE = 'This creature attacks each combat if able.'
MODES = ('omitted', 'already_selected', 'optional_ally', 'competing_selected', 'two_required', 'tapped', 'sick',
         'pacified', 'opponent_turn', 'blocked', 'paid_fresh', 'paid_next_turn', 'unfunded', 'wrong_color',
         'later_combat', 'proposed_replay', 'other_registration')
CAST = {'paid_fresh', 'paid_next_turn', 'unfunded', 'wrong_color', 'green_payment'}
NEXT_TURN = {'paid_next_turn', 'green_payment'}
INELIGIBLE = {'tapped', 'sick', 'pacified', 'paid_fresh'}
LIBRARY = ('Forest', 'Grizzly Bears', 'Island', 'Mountain', 'Hill Giant', 'Plains')
WITNESSES = set(LIBRARY) | {'Pacifism', 'Crazed Goblin', 'Suntail Hawk', 'Giant Spider'}
PASSIVES = {'Haste': ('Haste',),
            'Haste (This creature can attack and {T} as soon as it comes under your control.)': ('Haste',),
            'Flying': ('Flying',), 'Flying, haste': ('Flying', 'Haste'), 'First strike': ('First strike',)}


def pins():
    return json.loads(DATA.read_text(encoding='utf8'))


def classify(record):
    from recipes import cost_from_oracle

    cost = record.get('mana_cost', '')
    lines = record.get('oracle_text', '').splitlines()
    passive = PASSIVES.get(lines[0]) if len(lines) == 2 else ()
    known_passive = passive is not None and len(lines) in (1, 2) and list(passive) == record.get('keywords')
    if (record.get('layout') != 'normal' or record.get('card_faces') or record.get('selected_face')
            or not record.get('type_line', '').startswith('Creature — ') or not known_passive
            or lines[-1:] != [CLAUSE] or not re.fullmatch(r'(?:(?:\{[1-5]\})?(?:\{R\}){1,2}|\{R/G\}|\{1\}\{U\}\{B\})', cost)
            or any(not re.fullmatch(r'[1-9]', record.get(prop, '')) for prop in ('power', 'toughness'))):
        raise ValueError('whole normal fixed-cost creature with only the requirement to attack if able required')
    if passive:
        return dict(costs={'printed': cost_from_oracle(record)}, power=int(record['power']),
                    toughness=int(record['toughness']), keywords=list(passive))
    costs = {'red': cost_from_oracle({'mana_cost': '{R}' if cost == '{R/G}' else cost})}
    if cost == '{R/G}':
        costs['green'] = cost_from_oracle({'mana_cost': '{G}'})
    return dict(costs=costs, power=int(record['power']), toughness=int(record['toughness']))


def variants(params):
    modes = MODES + (('green_payment',) if 'green' in params['costs'] else ())
    keywords = params.get('keywords', [])
    if 'Flying' in keywords:
        modes += ('air_block', 'reach_block')
    if 'First strike' in keywords:
        modes += ('first_nonlethal',)
    return [f'{mode}_{seat}' for seat in (0, 1) for mode in modes]


def spec(declaration, record):
    if (declaration.get('family') != FAMILY or type(declaration.get('version')) is not int
            or declaration['version'] != 2 or declaration.get('oracle_text') != record.get('oracle_text')
            or not exact(pins()['sources'].get(record['name']), pin(record))):
        raise ValueError('exact whole required-attack source and reviewed recipe version required')
    params = classify(record)
    if not exact(params, declaration.get('params')):
        raise ValueError('source parameters must derive from complete Oracle')
    selected = declaration.get('variants')
    if (not isinstance(selected, list) or not selected or any(not isinstance(v, str) for v in selected)
            or len(set(selected)) != len(selected) or set(selected) - set(variants(params))):
        raise ValueError('distinct reviewed required-attack variants required')
    return dict(params, family=FAMILY)


def validate_witnesses(oracle, rows):
    data = pins()
    records = data['sources'] | data['witnesses']
    if set(data['witnesses']) != WITNESSES or set(data['handlers']) != set(records):
        raise ValueError('complete ability and fixture identity pins required')
    implemented = set(data['sources']) | {'Pacifism', 'Crazed Goblin'}
    for name, expected in records.items():
        row = rows[name]
        if not exact(pin(oracle[name]), expected) or row['handler'] != data['handlers'][name]:
            raise ValueError(f'Oracle or handler changed for {name}')
        if name in implemented and (row['kind'] != 'card' or row['handler'] in ('card_dummy', 'NULL', '0')):
            raise ValueError(f'implemented normal source or ability witness required for {name}')


class RequiredAttackCase:
    def __init__(self, name, csvid, declaration, params, variant, rows, oracle):
        from recipes import cost_from_oracle, step

        mode, seat = variant.rsplit('_', 1)
        self.mode, self.owner, self.variant = mode, int(seat), variant
        self.other = 1 - self.owner
        self.active = self.other if mode == 'opponent_turn' else self.owner
        self.name, self.csvid, self.declaration = name, csvid, declaration
        self.rows, self.oracle = rows, oracle
        self.setup, self.steps, self.entities = [], [], {}
        self.zones = {zone: [[], []] for zone in ('hand', 'battlefield', 'graveyard', 'library', 'exile')}
        self.life, self.mana = [20, 20], [[0] * 6, [0] * 6]
        self.snapshot = 0
        self.add(name, 'source', self.owner, 'hand' if mode in CAST else 'battlefield',
                 state=[mode] if mode in ('tapped', 'sick') else None)
        self.keywords = params.get('keywords', [])
        self.fee = params['costs'].get('printed') or params['costs']['green' if mode == 'green_payment' else 'red']
        if mode in CAST and mode != 'unfunded':
            self.mana[self.owner] = list(self.fee)
            if mode == 'wrong_color':
                color = next(c for c in range(1, 6) if self.fee[c])
                wrong = 2 if not self.fee[2] else 3
                self.mana[self.owner][wrong], self.mana[self.owner][color] = self.mana[self.owner][color], 0
        if mode == 'pacified':
            self.add('Pacifism', 'aura', self.owner, 'hand')
            self.aura_fee = cost_from_oracle(oracle['Pacifism'])
            self.mana[self.owner] = list(self.aura_fee)
        if mode in ('optional_ally', 'competing_selected', 'opponent_turn'):
            self.add('Hill Giant', 'ally', self.active)
        if mode == 'two_required':
            self.add(name, 'second', self.owner)
        blockers = {'blocked': 'Grizzly Bears', 'air_block': 'Suntail Hawk',
                    'reach_block': 'Giant Spider', 'first_nonlethal': 'Hill Giant'}
        if mode in blockers:
            self.add(blockers[mode], 'blocker', self.other)
        if mode == 'other_registration':
            self.add('Crazed Goblin', 'other_required', self.other, state=['tapped'])
        for who in (0, 1):
            self.add('Mountain', f'grave_{who}', who, 'graveyard')
            self.add('Plains', f'exile_{who}', who, 'exile')
            for index, witness in enumerate(LIBRARY):
                self.add(witness, f'lib_{who}_{index}', who, 'library')
        self.names = sorted({entry['name'] for entry in self.setup})
        self.before = [step('turn', player=self.active, value=0)]
        if mode not in NEXT_TURN | {'unfunded', 'wrong_color'}:
            # The fixture reset skips play_duel's default event flags. Run
            # real end-turn processing before any cast or eligibility check.
            self.before.extend(step('set_mana', player=who, color=color, value=0)
                               for who in (0, 1) for color in range(6))
            self.before.extend([step('cleanup'), step('turn', player=self.active, value=0)])
        self.before.extend(step('set_mana', player=who, color=color, value=amount)
                           for who in (0, 1) for color, amount in enumerate(self.mana[who]))
        self.before.append(step('refresh'))

    def add(self, name, ref, who, zone='battlefield', state=None):
        from recipes import card

        entry = card(name, ref, who, zone)
        if state:
            entry['state'] = state
        if zone in ('battlefield', 'hand'):
            record = self.oracle[name]
            creature = record['type_line'].startswith('Creature')
            stats = tuple(int(record[k]) for k in ('power', 'toughness')) if creature else None
            self.entities[ref] = dict(name=name, who=who, zone=zone, stats=stats,
                                      tapped=bool(state and 'tapped' in state), damage=0)
        self.setup.append(entry)
        self.zones[zone][who].append(name)

    def emit(self, op, **fields):
        from recipes import step

        self.steps.append(step(op, **fields))

    def move(self, ref, zone):
        card = self.entities[ref]
        if card['zone'] != 'stack':
            self.zones[card['zone']][card['who']].remove(card['name'])
        if zone != 'stack':
            self.zones[zone][card['who']].append(card['name'])
        card['zone'] = zone

    def observe(self, ref, prop, value):
        self.emit('assert_card', ref=ref, property=prop, value=int(value))

    def receipt(self, stack=0):
        self.emit('assert_stack', value=stack)
        self.emit('assert_choices_consumed')
        for ref, card in self.entities.items():
            if card['zone'] == 'battlefield':
                self.observe(ref, 'in_play', 1)
                self.observe(ref, 'tapped', card['tapped'])
                if card['stats'] is not None:
                    for prop, value in zip(('power', 'toughness', 'damage'), (*card['stats'], card['damage'])):
                        self.observe(ref, prop, value)
        for who in (0, 1):
            self.emit('assert_life', player=who, value=self.life[who])
            for color, amount in enumerate(self.mana[who]):
                self.emit('assert_mana', player=who, color=color, value=amount)
            for zone, seats in self.zones.items():
                names = seats[who]
                if zone == 'hand':
                    # A live pending spell still has a raw hand-slot receipt.
                    names = names + [c['name'] for c in self.entities.values() if c['zone'] == 'stack' and c['who'] == who]
                self.emit('assert_zone', player=who, zone=zone, value=len(names))
                if zone == 'library':
                    for index, name in enumerate(names):
                        self.emit('assert_zone_card', player=who, zone=zone, index=index, value=self.rows[name]['csvid'])
                else:
                    counts = Counter(names)
                    for name in self.names:
                        self.emit('assert_zone_multiplicity', player=who, zone=zone, index=self.rows[name]['csvid'], value=counts[name])

    def cast(self, ref, fee, *, denied=False):
        self.emit('cast', ref=ref, value=int(not denied))
        if denied:
            self.observe(ref, 'in_hand', 1)
        else:
            self.emit('require_card', ref=ref, property='in_hand', value=0)
            self.mana[self.owner] = [have - cost for have, cost in zip(self.mana[self.owner], fee)]
            self.move(ref, 'stack')
            self.receipt(1)
            self.emit('resolve')
            self.emit('refresh')
            self.move(ref, 'battlefield')
        self.receipt()

    def next_own_turn(self):
        self.emit('advance_turn')
        self.emit('advance_turn')
        self.emit('untap_step', player=self.owner)
        self.emit('turn', player=self.owner, value=0)
        self.entities['source']['tapped'] = False
        self.receipt()

    def attack(self, ref, *, allowed=True):
        self.emit('attack', ref=ref, value=int(allowed))
        self.observe(ref, 'attacking', allowed)
        if allowed:
            self.entities[ref]['tapped'] = True

    def declare(self):
        self.emit('begin_required_attackers')
        self.observe('source', 'attacking', 0)
        haste_overrides_sickness = 'Haste' in self.keywords and self.mode in ('sick', 'paid_fresh')
        if self.mode in INELIGIBLE and not haste_overrides_sickness:
            self.attack('source', allowed=False)
            attackers = []
        elif self.mode == 'opponent_turn':
            self.emit('can_finish_attackers', value=1)
            self.attack('ally')
            attackers = ['ally']
        else:
            attackers = []
            if self.mode == 'competing_selected':
                self.attack('ally')
                attackers.append('ally')
            if self.mode != 'already_selected':
                self.emit('can_finish_attackers', value=0)
            if self.mode == 'proposed_replay':
                self.snapshot = len(self.steps)
            self.attack('source')
            attackers.append('source')
            if self.mode == 'two_required':
                self.emit('can_finish_attackers', value=0)
                self.attack('second')
                attackers.append('second')
            if self.mode == 'optional_ally':
                self.observe('ally', 'attacking', 0)
        self.emit('can_finish_attackers', value=1)
        self.emit('finish_attackers')
        self.receipt()
        return attackers

    def combat_damage(self, attackers):
        if not attackers:
            return
        self.emit('begin_blockers')
        blocked = 'blocker' in self.entities and (self.mode != 'blocked' or 'Flying' not in self.keywords)
        if 'blocker' in self.entities:
            self.emit('block', ref='blocker', other='source', value=int(blocked))
        self.emit('advance_combat')
        for attacker in attackers:
            self.observe(attacker, 'blocked', blocked)
        self.emit('combat_damage')
        self.emit('resolve')
        if blocked:
            lethal_first_strike = ('First strike' in self.keywords
                                   and self.entities['source']['stats'][0] >= self.entities['blocker']['stats'][1])
            exchanges = [('blocker', 'source')] if lethal_first_strike else [('blocker', 'source'), ('source', 'blocker')]
            for ref, other in exchanges:
                card = self.entities[ref]
                card['damage'] = self.entities[other]['stats'][0]
                if card['damage'] >= card['stats'][1]:
                    self.move(ref, 'graveyard')
        else:
            self.life[1-self.active] -= sum(self.entities[ref]['stats'][0] for ref in attackers)
        self.emit('refresh')
        self.receipt()
        self.emit('end_combat')

    def case(self):
        from recipes import step

        denied = self.mode in ('unfunded', 'wrong_color')
        self.receipt()
        if self.mode in CAST:
            self.cast('source', self.fee, denied=denied)
        if not denied:
            if self.mode == 'pacified':
                self.emit('choose_target', ref='source', player=self.owner)
                self.cast('aura', self.aura_fee)
            if self.mode in NEXT_TURN:
                self.next_own_turn()
            self.combat_damage(self.declare())
            if self.mode == 'later_combat':
                self.next_own_turn()
                self.combat_damage(self.declare())
        suffix = self.steps[self.snapshot:]
        run = self.before + self.steps[:self.snapshot] + [step('capture')] + suffix
        run += [step('record_state'), step('restore')] + suffix + [step('assert_recorded_state')]
        return dict(name=f'RequiredAttackCreatureRecipe_{self.csvid}_{self.variant}', card=self.name, fixture='empty_main',
                    seed=123, setup=self.setup, steps=run, _recipe=dict(family=FAMILY, version=2, variant=self.variant,
                        params=self.declaration['params'], oracle_text=self.declaration['oracle_text'], untested_components=[
                            'Bounded declaration checks the production Done predicate independently before actual completion.',
                            'Attack taxes and additional combat creation are outside this fixture.',
                        ]))


def expand(name, csvid, declaration, params, selected, rows, oracle):
    validate_witnesses(oracle, rows)
    return [RequiredAttackCase(name, csvid, declaration, params, variant, rows, oracle).case() for variant in selected]
