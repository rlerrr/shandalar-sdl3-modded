"""Real X payments, temporary power, and complete adjacent spell clauses."""

import copy
import json
from functools import lru_cache
from pathlib import Path

from attack_value_trigger_recipes import _AttackResourceWorld

FAMILY = 'temporary_x_power_instants'
PROFILE = Path(__file__).with_name('data') / 'temporary_x_power_instants_wave8_profiles.json'
PUMP = 'Target creature gets +X/+0 until end of turn.'
RAGE = ("Target attacking creature gets +X/+0 until end of turn.\n"
        "Draw a card at the beginning of the next turn's upkeep.")
LIFE = 'As an additional cost to cast this spell, pay X life.\n' + PUMP
POWER = 'Target creature gets +X/+0 until end of turn, where X is its power.'
RULES = {PUMP: ('mana', 'manalinkeh_card_howl_from_beyond'), RAGE: ('rage', 'manalinkeh_card_balduvian_rage'),
         LIFE: ('life', 'manalinkeh_card_hatred'), POWER: ('power', 'manalinkeh_card_rush_of_blood')}
GIANT, WALL = 'Hill Giant', 'Fortified Rampart'
WITNESSES = {GIANT, WALL, 'Island', 'Mountain', 'Plains', 'Counterspell', 'Unsummon', 'Giant Growth'}
COMMON = ('exact', 'zero', 'wrong_color', 'short', 'no_legal_target', 'opponent_target', 'countered',
          'lost_target', 'combat', 'blocked', 'expiry', 'snapshot')
GAPS = ('Ordinary sorcery casting is exercised in main phase. The driver enters the cast callback directly, without the separate phase UI.',
        'No malformed or out-of-range X inputs.')


@lru_cache(maxsize=1)
def profiles():
    return json.loads(PROFILE.read_text(encoding='utf8'))


def classify(record):
    from recipes import cost_from_oracle

    if not record or record.get('layout') != 'normal' or record.get('card_faces') or record.get('selected_face'):
        raise ValueError('complete normal temporary power spell required')
    rule = RULES.get(record['oracle_text'])
    timing = record['type_line']
    if rule is None or record['keywords'] or timing not in ('Instant', 'Sorcery'):
        raise ValueError('reviewed whole printed temporary power spell required')
    model, _ = rule
    raw = record['mana_cost']
    mana_x = model in ('mana', 'rage')
    if raw.count('{X}') != int(mana_x) or timing == 'Sorcery' and model != 'mana':
        raise ValueError('complete printed X pricing and cast timing required')
    price = cost_from_oracle(dict(record, mana_cost=raw.replace('{X}', '')))
    return dict(model=model, mana=price, instant=timing == 'Instant', on_cast_x=mana_x)


def variants(params):
    mode = params['model']
    # Every Rage path already goes through combat and observes expiration.
    tests = tuple(test for test in COMMON if test not in ('combat', 'expiry')) if mode == 'rage' else COMMON
    if params['instant'] and mode != 'rage':
        tests += ('opponent_turn',)
    if mode == 'mana':
        tests += ('reserve', 'repeated')
    if mode == 'power':
        tests += ('resolution_power',)
    return [f'{test}_{actor}' for actor in (0, 1) for test in tests]


def check_variants(params, selected):
    if (type(selected) is not list or not selected or any(type(variant) is not str for variant in selected)
            or len(set(selected)) != len(selected) or set(selected) - set(variants(params))):
        raise ValueError('explicit distinct reviewed temporary power variants required')


def spec(declaration, record):
    params = classify(record)
    if (declaration.get('family') != FAMILY or type(declaration.get('version')) is not int or declaration['version'] != 2
            or profiles()['sources'].get(record['name'], {}).get('oracle') != record
            or declaration.get('oracle_text') != record['oracle_text']
            or json.dumps(declaration.get('params'), sort_keys=True) != json.dumps(params, sort_keys=True)):
        raise ValueError('pinned whole temporary power Oracle and exact typed parameters required')
    check_variants(params, declaration.get('variants'))
    return dict(params, family=FAMILY)


def validate_witnesses(oracle, rows):
    profiles_data = profiles()
    skipped = {entry['csvid'] for entry in json.loads((PROFILE.parent / 'unimplemented.json').read_text(
        encoding='utf8'))['cases'].values()}
    if profiles_data.get('version') != 1 or set(profiles_data['witnesses']) != WITNESSES:
        raise ValueError('reviewed exact temporary power witness set required')
    for group in ('sources', 'witnesses'):
        for name, pin in profiles_data[group].items():
            row, record = rows.get(name, {}), oracle.get(name, {})
            if (pin != dict(oracle=record, registry=row) or row.get('kind') != 'card' or record.get('layout') != 'normal'
                    or row['csvid'] in skipped or row['handler'] in (None, 'NULL', '0')
                    or (row['handler'] == 'card_dummy' and name not in (GIANT, WALL))):
                raise ValueError(f'unreviewed temporary power binding: {name}')
            if group == 'sources':
                classify(record)
                if row['handler'] != RULES[record['oracle_text']][1]:
                    raise ValueError(f'unreviewed temporary power source callback: {name}')


class PumpWorld(_AttackResourceWorld):
    def __init__(self, name, params, test, actor, rows, oracle):
        super().__init__(name, dict(params, keywords=[], effect=FAMILY), actor, rows, oracle)
        self.test = test
        self.foe = 1 - actor
        self.mode = params['model']
        self.delayed_draw = False
        self.x = 0 if test == 'zero' else 4

    def observe(self, ref, prop, value):
        self.emit('assert_card', ref=ref, property=prop, value=int(value))

    def receipts(self):
        self.emit('refresh')
        super().receipts()

    def configure(self):
        test, owner, foe = self.test, self.owner, self.foe
        self.add(self.name, 'source', owner, 'hand')
        if test == 'repeated':
            self.add(self.name, 'repeat', owner, 'hand')
        if test != 'no_legal_target' or self.mode == 'rage':
            recipient = foe if test == 'opponent_target' else owner
            self.add(WALL if self.mode == 'power' and test == 'zero' else GIANT, 'recipient', recipient)
            self.add(GIANT, 'unaffected', 1 - recipient)
        if test == 'blocked':
            self.add(WALL, 'blocker', foe)
        if test in ('countered', 'lost_target', 'resolution_power'):
            response = {'countered': 'Counterspell', 'lost_target': 'Unsummon', 'resolution_power': 'Giant Growth'}[test]
            self.add(response, 'response', foe, 'hand')
        for player in (0, 1):
            for i, name in enumerate(('Island', 'Mountain', 'Plains')):
                self.add(name, f'library_{player}_{i}', player, 'library')
        if self.mode == 'life' and owner == 1 and 'recipient' in self.entities:
            # The native AI actually computes this life-payment value itself.
            # A controlled setup supplies the intended legal nonterminal X.
            life = self.entities['recipient']['power'] + self.x
            self.emit('set_life', player=foe, value=life)
            self.life[foe] = life

    def cast(self, ref, x, denied=None):
        cost = list(self.p['mana'])
        cost[0] += x if self.p['on_cast_x'] else 0
        pool = cost.copy()
        if denied == 'wrong_color':
            pool = [0, 0, sum(cost), 0, 0, 0]
        elif denied == 'short':
            pool = [0] * 6
        elif self.test == 'reserve':
            pool[0] += 2
        self.pool(self.owner, pool)
        self.emit('refresh')
        if not denied:
            if self.p['on_cast_x']:
                self.emit('choose_x', player=self.owner, value=x)
            self.emit('choose_target', player=self.owner, ref='recipient')
            if self.mode == 'life' and self.owner == 0:
                self.emit('choose_number', player=self.owner, value=x)
        self.emit('cast', ref=ref, value=int(not denied))
        self.emit('assert_stack', value=int(not denied))
        self.emit('assert_unused_choices', value=0)
        self.observe(ref, 'in_hand', bool(denied))
        self.observe(ref, 'in_play', False)
        if not denied:
            self.mana[self.owner] = [n - fee for n, fee in zip(pool, cost)]
            if self.mode == 'life':
                self.life[self.owner] -= x
        for color, value in enumerate(self.mana[self.owner]):
            self.emit('assert_mana', player=self.owner, color=color, value=value)
        self.emit('assert_life', player=self.owner, value=self.life[self.owner])
        if 'recipient' in self.entities:
            self.observe('recipient', 'power', self.entities['recipient']['power'])
            self.observe('recipient', 'toughness', self.entities['recipient']['toughness'])

    def move_spell(self, name, player):
        self.zones['hand'][player].remove(name)
        self.zones['graveyard'][player].append(name)

    def settle(self, ref, x, response=False):
        test = self.test if response else None
        if test:
            from recipes import cost_from_oracle

            name = self.zones['hand'][self.foe][0]
            self.emit('announce_spell', ref=ref)
            self.pool(self.foe, cost_from_oracle(self.o[name]))
            if test != 'countered':
                self.emit('choose_target', ref='recipient', player=self.foe)
            self.emit('cast_response', ref='response', value=1)
            self.emit('assert_stack', value=2)
            self.emit('resolve_response')
            self.move_spell(name, self.foe)
            self.mana[self.foe] = [0] * 6
            if test == 'lost_target':
                self.leave('recipient', 'hand')
            elif test == 'resolution_power':
                creature = self.entities['recipient']
                creature['power'] += 3
                creature['toughness'] += 3
        else:
            self.emit('resolve')
        self.move_spell(self.name, self.owner)
        if test not in ('countered', 'lost_target'):
            recipient = self.entities['recipient']
            recipient['power'] += recipient['power'] if self.mode == 'power' else x
            self.delayed_draw = self.mode == 'rage'
        self.receipts()

    def declare(self):
        self.emit('begin_attackers')
        self.emit('attack', ref='recipient', value=1)
        self.emit('finish_attackers')
        self.entities['recipient'].update(attacking=True, tapped=True)
        self.receipts()

    def fight(self):
        self.emit('begin_blockers')
        attack = self.entities['recipient']
        blocked = self.test == 'blocked'
        if blocked:
            self.emit('block', ref='blocker', other='recipient', value=1)
            self.observe('blocker', 'blocking_count', 1)
        self.emit('advance_combat')
        self.emit('combat_damage')
        self.emit('resolve')
        self.emit('end_combat')
        if attack['zone'] == 'battlefield':
            if blocked:
                defense = self.entities['blocker']
                defense['damage'] += attack['power']
                attack['damage'] += defense['power']
                if defense['damage'] >= defense['toughness']:
                    self.leave('blocker', 'graveyard')
            else:
                self.life[1-self.current] -= attack['power']
        attack['attacking'] = False
        self.receipts()

    def expire(self):
        super().expire()
        for creature in self.entities.values():
            oracle = self.o[creature['name']]
            creature.update(power=int(oracle['power']), toughness=int(oracle['toughness']))

    def next_upkeep(self):
        # Deliberately perform the real untap-to-upkeep transition before any
        # draw phase or main-phase fixture selection.
        self.emit('advance_turn')
        self.expire()
        self.current = 1 - self.current
        self.emit('untap_step', player=self.current)
        for creature in self.entities.values():
            if creature['player'] == self.current and creature['zone'] == 'battlefield':
                creature.update(tapped=False, sick=False)
        self.receipts()
        self.emit('upkeep_step', player=self.current)
        if self.delayed_draw:
            library = self.zones['library'][self.owner]
            self.zones['hand'][self.owner].append(library.pop(0))
            self.delayed_draw = False
        self.receipts()


def scenario(world):
    test = world.test
    world.emit('turn', player=world.owner, value=0)
    world.configure()
    world.emit('present_ai_dialogs', value=1)
    for player in (0, 1):
        world.pool(player, [0] * 6)
    world.receipts()
    if test in ('wrong_color', 'short', 'no_legal_target'):
        world.cast('source', world.x, denied=test)
        world.receipts()
        return None
    rage = world.mode == 'rage'
    if test == 'opponent_turn' or (rage and test == 'opponent_target'):
        world.next_turn()
    if rage:
        world.declare()  # The printed attacking target restriction is real.
    x = 1 if test == 'repeated' else world.x
    world.cast('source', x)
    cut = len(world.steps) if test == 'snapshot' else None
    world.settle('source', x, response=test in ('countered', 'lost_target', 'resolution_power'))
    if test == 'repeated':
        world.cast('repeat', 4)
        world.settle('repeat', 4)
    if not rage and test in ('combat', 'blocked'):
        world.declare()
    if rage or test in ('combat', 'blocked'):
        world.fight()
    if rage:
        world.next_upkeep()
        world.next_upkeep()
    elif test == 'expiry':
        world.next_turn()
    return cut


def expand(name, csvid, declaration, params, selected, rows, oracle):
    from recipes import step

    check_variants(params, selected)
    validate_witnesses(oracle, rows)
    if profiles()['sources'][name]['registry']['csvid'] != csvid:
        raise ValueError('exact temporary power source CSVID required')
    cases = []
    for variant in selected:
        test, actor = variant.rsplit('_', 1)
        world = PumpWorld(name, params, test, int(actor), rows, oracle)
        cut = scenario(world)
        steps = world.steps
        if cut is not None:
            tail = steps[cut:]
            steps = (steps[:cut] + [step('capture')] + tail + [step('record_state'), step('ai_roundtrip'), step('restore')]
                     + copy.deepcopy(tail) + [step('assert_recorded_state')])
        cases.append(dict(name=f'TemporaryXPowerRecipe_{csvid}_{variant}', card=name, fixture='empty_main', seed=287,
                          setup=world.setup, steps=steps, _recipe=dict(family=FAMILY, version=2, variant=variant,
                              params=declaration['params'], oracle_text=declaration['oracle_text'],
                              untested_components=list(GAPS))))
    return cases
