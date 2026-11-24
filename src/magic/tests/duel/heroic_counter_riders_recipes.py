"""Heroic counters and riders driven by genuinely paid targeting spells."""

from collections import Counter
from functools import lru_cache
import json
from pathlib import Path

from heroic_case_support import HeroicBoard, heroic_rules
from library_search_recipes import wave6_exact as exact


FAMILY = 'heroic_counter_riders'
PROFILE = Path(__file__).parent / 'data' / f'{FAMILY}_wave8_profiles.json'
HEROIC = 'Heroic — Whenever you cast a spell that targets this creature, '
SCRY = 'scry 1. (To scry 1, look at the top card of your library, then you may put that card on the bottom.)'
COUNTER = HEROIC + 'put a +1/+1 counter on it.'
PATTERNS = {
    (HEROIC + 'put a +1/+1 counter on this creature, then ' + SCRY,): 'counter_scry',
    (COUNTER, 'Whenever a +1/+1 counter is put on this creature, remove a +1/+1 counter from target creature an opponent controls.'): 'drain_counter',
    ('Flying', HEROIC + SCRY): 'flying_scry',
    (HEROIC + 'put a +1/+1 counter on this creature and prevent all damage that would be dealt to it this turn.',): 'prevent',
    (HEROIC + 'you may pay {X}. If you do, put X +1/+1 counters on this creature.',): 'variable',
    (COUNTER, 'Remove all +1/+1 counters from this creature: For each five counters removed this way, take an extra turn after this one.'): 'turns',
    ('Each instant and sorcery spell you cast costs {1} less to cast for each creature it targets.',
     HEROIC + 'this creature gains hexproof until end of turn.'): 'hexproof_reducer',
}
MODES = {
    'counter_scry': ('scry_bottom',),
    'drain_counter': ('counter_from_other_spell',),
    'flying_scry': ('scry_bottom', 'flying'),
    'prevent': ('shield', 'no_shield'),
    'variable': ('zero_payment',),
    'turns': ('turn_four', 'turn_five', 'turn_six', 'turn_ten'),
    'hexproof_reducer': ('hexproof', 'discount_one', 'discount_two', 'enemy_discount_control'),
}
WITNESSES = {'Defiant Strike', 'Elvish Mystic', 'Forest', 'Giant Growth', 'Goblin Tinkerer',
             'Hunger of the Howlpack', 'Island', 'Mighty Leap', 'Rites of Reaping', 'Shock', 'Welkin Tern'}
TURN_AMOUNTS = {'turn_four': 4, 'turn_five': 5, 'turn_six': 6, 'turn_ten': 10}


@lru_cache(maxsize=1)
def profiles():
    return json.loads(PROFILE.read_bytes())


def classify(record):
    from recipes import cost_from_oracle

    lines = heroic_rules(record)
    effect = PATTERNS.get(tuple(lines))
    if effect is None:
        raise ValueError('Exact complete Heroic counter rider is required')
    return dict(effect=effect, clauses=lines, price=cost_from_oracle(record),
                base=[int(record['power']), int(record['toughness'])])


def variants(model):
    return [f'{mode}_{seat}' for mode in ('own', 'opponent', 'elsewhere', *MODES[model['effect']])
            for seat in (0, 1) if mode != 'scry_bottom' or seat == 0]


def spec(declaration, record):
    name = (record or {}).get('name')
    pin = profiles()['sources'].get(name)
    if not pin or not exact(record, pin['oracle']):
        raise ValueError('Complete pinned Heroic creature record required')
    model = classify(record)
    picked = declaration.get('variants')
    if (declaration.get('family') != FAMILY or type(declaration.get('version')) is not int
            or declaration['version'] != 2 or declaration.get('oracle_text') != record['oracle_text']
            or not exact(declaration.get('params'), model) or not isinstance(picked, list) or not picked
            or any(type(v) is not str for v in picked) or len(set(picked)) != len(picked)
            or set(picked) - set(variants(model))):
        raise ValueError('Exact model and distinct scoped Heroic variants required')
    return dict(model, family=FAMILY)


def validate_witnesses(oracle, rows):
    pins = profiles()
    if set(pins) != {'sources', 'witnesses'} or set(pins['witnesses']) != WITNESSES:
        raise ValueError('Complete scoped Heroic witness pins required')
    for group in pins.values():
        for name, pin in group.items():
            record, row = oracle.get(name, {}), rows.get(name, {})
            if (not exact(pin, {'oracle': record, 'registry': row}) or record.get('layout') != 'normal'
                    or row.get('kind') != 'card' or row.get('handler') in (None, 'NULL', '0', 'card_dummy')):
                raise ValueError(f'Pinned implemented Heroic source or witness changed: {name}')


class CounterHeroicBoard(HeroicBoard):
    def __init__(self, name, shape, mode, owner, rows, oracle):
        self.counters = Counter()
        self.power_bonus = Counter()
        self.toughness_bonus = Counter()
        self.libraries = [[], []]
        self.snapshot = None
        super().__init__(name, shape, mode, owner, rows, oracle)

    def build(self):
        m, effect = self.mode, self.shape['effect']
        if m == 'elsewhere':
            self.add('Goblin Tinkerer', 'ally', self.owner)
        if m in ('own', 'counter_from_other_spell') and effect == 'drain_counter':
            self.add('Goblin Tinkerer', 'victim', self.foe)
            self.add('Elvish Mystic', 'decoy', self.foe)
            self.add('Hunger of the Howlpack', 'seed', self.owner, 'hand')
            if m == 'counter_from_other_spell':
                self.add('Hunger of the Howlpack', 'external', self.foe, 'hand')
        if m in ('flying',):
            self.add('Goblin Tinkerer', 'ground', self.foe)
            self.add('Welkin Tern', 'flier', self.foe)
        if m in ('shield', 'no_shield', 'hexproof'):
            self.add('Shock', 'damage', self.foe, 'hand')
        if m == 'shield':
            self.add('Shock', 'later_damage', self.foe, 'hand')
        if m == 'hexproof':
            self.add('Giant Growth', 'own_probe', self.owner, 'hand')
        if m in TURN_AMOUNTS:
            for index in range(TURN_AMOUNTS[m]):
                self.add('Giant Growth', f'growth_{index}', self.owner, 'hand')
        if m in ('discount_one', 'enemy_discount_control'):
            self.add('Mighty Leap', 'discount', self.foe if m == 'enemy_discount_control' else self.owner, 'hand')
        if m == 'discount_two':
            self.add('Rites of Reaping', 'discount', self.owner, 'hand')
            self.add('Goblin Tinkerer', 'ally', self.owner)
            self.add('Elvish Mystic', 'victim', self.foe)
        defiant = m in ('own', 'opponent', 'elsewhere', 'scry_bottom', 'flying', 'shield', 'no_shield', 'zero_payment', 'hexproof')
        if defiant:
            actor = self.foe if m in ('opponent', 'no_shield') else self.owner
            for name in ('Forest', 'Island'):
                ref = f'library_{actor}_{name.lower()}'
                self.add(name, ref, actor, 'library')
                self.libraries[actor].append(ref)

    def stats(self, ref):
        record = self.oracle[self.cards[ref]['name']]
        return (int(record['power']) + self.counters[ref] + self.power_bonus[ref],
                int(record['toughness']) + self.counters[ref] + self.toughness_bonus[ref])

    def receipt(self, ref):
        for prop, val in zip(('power', 'toughness'), self.stats(ref)):
            self.emit('assert_card', ref=ref, property=prop, value=val)
        self.emit('assert_card', ref=ref, property='plus1_counters', value=self.counters[ref])
        self.emit('assert_card', ref=ref, property='damage', value=self.cards[ref]['damage'])

    def checks(self):
        quantities = Counter((item['player'], item['zone'], self.rows[item['name']]['csvid']) for item in self.cards.values())
        for who in (0, 1):
            self.emit('assert_life', player=who, value=20)
            for zone in ('battlefield', 'hand', 'library', 'graveyard', 'exile'):
                total = sum(n for (seat, place, _), n in quantities.items() if seat == who and place == zone)
                self.emit('assert_zone', player=who, zone=zone, value=total)
            for index, ref in enumerate(self.libraries[who]):
                self.emit('assert_zone_card', player=who, zone='library', index=index,
                          value=self.rows[self.cards[ref]['name']]['csvid'])
        for (who, zone, csvid), value in sorted(quantities.items()):
            self.emit('assert_zone_multiplicity', player=who, zone=zone, index=csvid, value=value)
        for ref, card in self.cards.items():
            if card['zone'] == 'battlefield':
                self.receipt(ref)
        self.emit('assert_stack', value=0)
        self.observe_mana()
        self.emit('assert_choices_consumed')

    def counter_target(self, board):
        if self.owner == 0:
            board.emit('choose_trigger_source', ref='source', player=self.owner)
        board.emit('choose_target', ref='victim', player=self.owner)

    def resolve_heroic_spell(self, spell='trigger'):
        super().resolve_heroic_spell(spell)
        if (self.owner == 0 and self.shape['effect'] == 'drain_counter'
                and (self.mode, spell) in (('own', 'trigger'), ('counter_from_other_spell', 'external'))):
            # A skipped nested target is an ordinary choice receipt. Clear it before later checks or replay.
            choice_check = self.steps[-2]
            if choice_check != {'op': 'assert_choices_consumed'}:
                raise ValueError('Heroic resolution choice boundary changed')
            choice_check.update(op='assert_unused_choices', value=0)

    def seed_victim(self):
        self.heroic_spell(spell='seed', target='victim')
        self.counters['victim'] = 1
        self.receipt('victim')

    def pay_x(self, board):
        if self.owner == 0:
            board.emit('choose_trigger_source', ref='source', player=self.owner)
        board.emit('choose_x', player=self.owner, value=0 if self.mode == 'zero_payment' else 2)

    def defiant(self, *, owned=True, target='source'):
        from recipes import cost_from_oracle

        mode, effect = self.mode, self.shape['effect']
        trigger = owned and target == 'source'
        actor = self.owner if owned else self.foe
        scry = trigger and effect in ('counter_scry', 'flying_scry')
        paid_x = 0 if mode == 'zero_payment' else 2
        funds = None
        choices = None
        if scry:
            choices = lambda b: b.emit('choose_zone', player=self.owner, value=0 if mode == 'scry_bottom' else -1)
        elif trigger and effect == 'variable':
            funds = cost_from_oracle(self.oracle['Defiant Strike'])
            funds[0] += 2
            choices = self.pay_x
        elif trigger and effect == 'drain_counter':
            choices = self.counter_target
        self.heroic_spell(owned, target=target, choices=choices, funding=funds)
        if scry and mode == 'scry_bottom':
            self.libraries[actor].append(self.libraries[actor].pop(0))
        drawn = self.libraries[actor].pop(0)
        self.cards[drawn]['zone'] = 'hand'
        self.power_bonus[target] += 1
        if trigger and effect in ('counter_scry', 'drain_counter', 'prevent', 'turns'):
            self.counters['source'] += 1
        if trigger and effect == 'drain_counter':
            self.counters['victim'] -= 1
        if trigger and effect == 'variable':
            self.counters['source'] += paid_x
            self.pools[self.owner][0] -= paid_x

    def expire(self):
        self.emit('advance_turn')
        self.power_bonus.clear()
        self.toughness_bonus.clear()
        for card in self.cards.values():
            card['damage'] = 0

    def shock(self, ref, prevented=False):
        self.heroic_spell(False, spell=ref)
        if prevented:
            return
        if self.stats('source')[1] <= self.cards['source']['damage'] + 2:
            self.cards['source']['zone'] = 'graveyard'
        else:
            self.cards['source']['damage'] += 2

    def flight(self):
        self.expire()
        self.expire()
        self.emit('assert_card', ref='source', property='sick', value=0)
        self.emit('begin_attackers')
        self.emit('attack', ref='source', value=1)
        self.emit('finish_attackers')
        self.emit('begin_blockers')
        self.emit('block', ref='ground', other='source', value=0)
        self.emit('block', ref='flier', other='source', value=1)
        self.emit('advance_combat')
        self.emit('combat_damage')
        self.emit('resolve')
        self.emit('end_combat')
        for ref in ('source', 'flier'):
            self.cards[ref]['zone'] = 'graveyard'

    def schedule(self, amount):
        for index in range(amount):
            self.heroic_spell(spell=f'growth_{index}')
            self.counters['source'] += 1
            self.power_bonus['source'] += 3
            self.toughness_bonus['source'] += 3
            self.receipt('source')
        self.emit('can_activate', ref='source', value=1)
        self.emit('activate', ref='source', value=1)
        self.emit('assert_stack', value=1)
        self.counters['source'] = 0
        self.receipt('source')
        self.emit('resolve')
        for seat in [self.owner] * (amount // 5) + [self.foe]:
            self.emit('advance_scheduled_turn', value=seat)
            self.power_bonus.clear()
            self.toughness_bonus.clear()
            self.receipt('source')

    def reduction(self):
        from recipes import cost_from_oracle

        mode = self.mode
        item = self.cards['discount']
        price = cost_from_oracle(self.oracle[item['name']])
        payer = item['player']
        if mode == 'discount_two':
            self.fund(payer, price)
            for target in ('ally', 'victim'):
                self.emit('choose_target', ref=target, player=payer)
            self.emit('cast', ref='discount', value=1)
            self.emit('assert_choices_consumed')
            self.emit('assert_stack', value=1)
            self.pools[payer] = [min(price[0], 2), 0, 0, 0, 0, 0]
            self.observe_mana()
            self.emit('announce_spell', ref='discount')
            self.resolve_heroic_spell('discount')
            self.power_bonus['ally'] += 3
            self.toughness_bonus['ally'] += 3
            self.cards['victim']['zone'] = 'graveyard'
        else:
            due = list(price)
            if payer == self.owner:
                due[0] = max(0, due[0] - 1)
            self.heroic_spell(payer == self.owner, spell='discount', funding=price, cost=due)
            self.power_bonus['source'] += 2
            self.toughness_bonus['source'] += 2

    def run(self):
        self.play_source()
        self.receipt('source')
        mode, effect = self.mode, self.shape['effect']
        if mode == 'own':
            self.snapshot = len(self.steps)
        if mode.startswith('discount') or mode == 'enemy_discount_control':
            self.reduction()
        elif mode in TURN_AMOUNTS:
            self.schedule(TURN_AMOUNTS[mode])
        elif mode == 'counter_from_other_spell':
            self.seed_victim()
            self.heroic_spell(False, spell='external', before_resolve=self.counter_target)
            self.counters['source'] += 1
            self.counters['victim'] -= 1
        else:
            if effect == 'drain_counter' and mode == 'own':
                self.seed_victim()
            if mode == 'hexproof':
                self.emit('can_target_creature', ref='damage', other='source', value=1)
            self.defiant(owned=mode not in ('opponent', 'no_shield'), target='ally' if mode == 'elsewhere' else 'source')
            if mode in ('flying', 'shield', 'no_shield', 'hexproof'):
                self.checks()
            if mode == 'flying':
                self.flight()
            elif mode in ('shield', 'no_shield'):
                self.shock('damage', prevented=mode == 'shield')
                if mode == 'shield':
                    self.receipt('source')
                    self.expire()
                    self.shock('later_damage')
            elif mode == 'hexproof':
                self.emit('can_target_creature', ref='damage', other='source', value=0)
                self.emit('can_target_creature', ref='own_probe', other='source', value=1)
                self.expire()
                self.emit('can_target_creature', ref='damage', other='source', value=1)
                self.shock('damage')
        self.checks()


def expand(name, csvid, declaration, shape, selected, rows, oracle):
    from recipes import step

    validate_witnesses(oracle, rows)
    if name not in profiles()['sources'] or rows[name]['csvid'] != csvid or set(selected) - set(variants(shape)):
        raise ValueError('Reserved full Heroic rider case required')
    result = []
    for variant in selected:
        mode, controller = variant.rsplit('_', 1)
        board = CounterHeroicBoard(name, shape, mode, int(controller), rows, oracle)
        board.run()
        steps = board.steps
        if board.snapshot is not None:
            cut = board.snapshot
            suffix = steps[cut:]
            steps = steps[:cut] + [step('capture')] + suffix + [step('record_state'), step('ai_roundtrip'),
                    step('restore')] + suffix + [step('assert_recorded_state')]
        gaps = ['Heroic targeted through copied spells or multiple target designations on one spell']
        if shape['effect'] in ('counter_scry', 'flying_scry') and controller == '1':
            gaps.append('AI discard and reorder strategy beyond keeping the offered top land')
        metadata = dict(family=FAMILY, version=2, variant=variant, params=declaration['params'],
                        oracle_text=declaration['oracle_text'], covered_clauses=shape['clauses'],
                        untested_components=gaps)
        result.append(dict(name=f'HeroicCounterRidersRecipe_{csvid}_{variant}', card=name,
                           fixture='empty_main', seed=2119, setup=board.setup, steps=steps, _recipe=metadata))
    return result
