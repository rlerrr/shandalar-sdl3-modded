"""Whole all-able-to-block creatures through actual declaration and combat."""

import json
import re
from collections import Counter
from pathlib import Path

from cast_untap_trigger_recipes import exact, pin


FAMILY = 'required_block_creature'
DATA = Path(__file__).parent / 'data/required_block_creature_wave8_profiles.json'
CLAUSE = 'All creatures able to block this creature do so.'
COMMON = ('single', 'refused', 'two_eligible', 'two_reverse', 'legal_and_tapped', 'all_tapped', 'cannot_block', 'noncreature',
          'second_attacker', 'reassign', 'idle_lure', 'idle_lure_block_other', 'shrouded_blocker', 'flying_unblocked',
          'flying_reach', 'fresh', 'paid_next_turn', 'unfunded', 'after_block_replay')
CAST = {'fresh', 'paid_next_turn', 'unfunded', 'wrong_color'}
TWO = {'two_eligible', 'two_reverse'}
ALLY = {'second_attacker', 'reassign', 'idle_lure', 'idle_lure_block_other', 'fresh'}
NO_ASSIGN = {'all_tapped', 'cannot_block', 'noncreature', 'flying_unblocked', 'idle_lure', 'idle_lure_block_other', 'fresh'}
LIBRARY = ('Mountain', 'Hill Giant', 'Forest', 'Grizzly Bears', 'Plains', 'Island')
WITNESSES = set(LIBRARY) | {'Flight', 'Giant Spider', 'Wall of Wood', 'Darksteel Relic', 'Carrion Feeder', 'Deadly Insect'}


def pins():
    return json.loads(DATA.read_text(encoding='utf8'))


def classify(record):
    from recipes import cost_from_oracle

    if (record.get('layout') != 'normal' or record.get('card_faces') or record.get('selected_face')
            or not record.get('type_line', '').startswith('Creature — ') or record.get('keywords') != []
            or record.get('oracle_text') != CLAUSE
            or not re.fullmatch(r'(?:\{[1-9]\}(?:\{G\}){0,2}|\{G\})', record.get('mana_cost', ''))
            or not re.fullmatch(r'(?:[0-9]|10)', record.get('power', ''))
            or not re.fullmatch(r'[1-9]', record.get('toughness', ''))):
        raise ValueError('whole normal fixed-cost creature with only the required-block clause required')
    return dict(mana=cost_from_oracle(record), power=int(record['power']), toughness=int(record['toughness']))


def variants(params):
    modes = COMMON + (('wrong_color',) if any(params['mana'][1:]) else ())
    return [f'{mode}_{seat}' for seat in (0, 1) for mode in modes]


def spec(declaration, record):
    if (declaration.get('family') != FAMILY or type(declaration.get('version')) is not int
            or declaration['version'] != 2 or declaration.get('oracle_text') != record.get('oracle_text')
            or not exact(pins()['sources'].get(record['name']), pin(record))):
        raise ValueError('exact whole required-block source and reviewed recipe version required')
    params = classify(record)
    if not exact(params, declaration.get('params')):
        raise ValueError('source parameters must derive from complete Oracle')
    selected = declaration.get('variants')
    if (not isinstance(selected, list) or not selected or any(not isinstance(v, str) for v in selected)
            or len(set(selected)) != len(selected) or set(selected) - set(variants(params))):
        raise ValueError('distinct reviewed required-block variants required')
    return dict(params, family=FAMILY)


def validate_witnesses(oracle, rows):
    data = pins()
    records = data['sources'] | data['witnesses']
    if set(data['witnesses']) != WITNESSES or set(data['handlers']) != set(records):
        raise ValueError('complete fixture witness identity and handler pins required')
    # Reach and Shroud witnesses use registered static card data.
    implemented = set(data['sources']) | {'Flight', 'Carrion Feeder'}
    for name, expected in records.items():
        row = rows[name]
        if not exact(pin(oracle[name]), expected) or row['handler'] != data['handlers'][name]:
            raise ValueError(f'Oracle or handler changed for {name}')
        if name in implemented and (row['kind'] != 'card' or row['handler'] in ('card_dummy', 'NULL', '0')):
            raise ValueError(f'implemented normal source or ability witness required for {name}')


class RequiredBlockCase:
    def __init__(self, name, csvid, declaration, params, variant, rows, oracle):
        from recipes import cost_from_oracle, step

        mode, seat = variant.rsplit('_', 1)
        self.mode, self.owner, self.variant = mode, int(seat), variant
        self.other = 1 - self.owner
        self.name, self.csvid, self.declaration, self.params = name, csvid, declaration, params
        self.rows, self.oracle = rows, oracle
        self.zones = {zone: [[], []] for zone in ('hand', 'battlefield', 'graveyard', 'library', 'exile')}
        self.setup, self.steps, self.entities = [], [], {}
        self.mana, self.life = [[0] * 6, [0] * 6], [20, 20]
        self.add(name, 'source', self.owner, 'hand' if mode in CAST else 'battlefield')
        if mode in CAST and mode != 'unfunded':
            self.mana[self.owner] = list(params['mana'])
            if mode == 'wrong_color':
                self.mana[self.owner][2], self.mana[self.owner][3] = self.mana[self.owner][3], 0
        if mode in ('flying_unblocked', 'flying_reach'):
            self.add('Flight', 'flight', self.owner, 'hand')
            self.mana[self.owner] = cost_from_oracle(oracle['Flight'])
        if mode in ALLY:
            self.add('Hill Giant', 'ally', self.owner)
        if mode not in ('unfunded', 'wrong_color'):
            blocker = {'cannot_block': 'Carrion Feeder', 'noncreature': 'Darksteel Relic', 'shrouded_blocker': 'Deadly Insect'}.get(mode, 'Grizzly Bears')
            self.add(blocker, 'blocker', self.other, tapped=mode == 'all_tapped')
        if mode in TWO:
            self.add('Wall of Wood', 'blocker_two', self.other)
        if mode == 'legal_and_tapped':
            self.add('Hill Giant', 'blocker_two', self.other, tapped=True)
        if mode == 'flying_reach':
            self.add('Giant Spider', 'blocker_two', self.other)
        for who in (0, 1):
            self.add('Forest', f'grave_{who}', who, 'graveyard')
            self.add('Plains', f'exile_{who}', who, 'exile')
            for index, witness in enumerate(LIBRARY):
                self.add(witness, f'library_{who}_{index}', who, 'library')
        self.names = sorted({item['name'] for item in self.setup})
        self.before = [step('turn', player=self.owner, value=0)]
        self.before += [step('set_mana', player=who, color=color, value=amount)
                        for who in (0, 1) for color, amount in enumerate(self.mana[who])]
        self.before.append(step('refresh'))
        self.snapshot = 0

    def add(self, name, ref, who, zone='battlefield', tapped=False):
        from recipes import card

        entry = card(name, ref, who, zone)
        if zone == 'battlefield':
            entry['state'] = ['tapped' if tapped else 'untapped']
        if zone in ('battlefield', 'hand'):
            record = self.oracle[name]
            stats = tuple(int(record[p]) for p in ('power', 'toughness')) if record['type_line'].startswith('Creature') else None
            self.entities[ref] = dict(name=name, who=who, zone=zone, stats=stats, tapped=tapped, damage=0)
        self.setup.append(entry)
        self.zones[zone][who].append(name)

    def emit(self, op, **fields):
        from recipes import step

        self.steps.append(step(op, **fields))

    def move(self, ref, zone):
        entity = self.entities[ref]
        if entity['zone'] != 'stack':
            self.zones[entity['zone']][entity['who']].remove(entity['name'])
        if zone != 'stack':
            self.zones[zone][entity['who']].append(entity['name'])
        entity['zone'] = zone

    def observe(self, ref, prop, value):
        self.emit('assert_card', ref=ref, property=prop, value=int(value))

    def receipt(self, stack=0):
        self.emit('assert_stack', value=stack)
        self.emit('assert_choices_consumed')
        for ref, entity in self.entities.items():
            if entity['zone'] == 'battlefield':
                self.observe(ref, 'in_play', 1)
                self.observe(ref, 'tapped', entity['tapped'])
                if entity['stats'] is not None:
                    for prop, amount in zip(('power', 'toughness', 'damage'), (*entity['stats'], entity['damage'])):
                        self.observe(ref, prop, amount)
        for who in (0, 1):
            self.emit('assert_life', player=who, value=self.life[who])
            for color, amount in enumerate(self.mana[who]):
                self.emit('assert_mana', player=who, color=color, value=amount)
            for zone, seats in self.zones.items():
                cards = seats[who]
                if zone == 'hand':
                    # A pending cast retains a physical slot in the legacy
                    # zone observer, checked independently as not in hand.
                    cards = cards + [e['name'] for e in self.entities.values() if e['zone'] == 'stack' and e['who'] == who]
                self.emit('assert_zone', player=who, zone=zone, value=len(cards))
                if zone == 'library':
                    for index, witness in enumerate(cards):
                        self.emit('assert_zone_card', player=who, zone=zone, index=index, value=self.rows[witness]['csvid'])
                else:
                    counts = Counter(cards)
                    for witness in self.names:
                        self.emit('assert_zone_multiplicity', player=who, zone=zone, index=self.rows[witness]['csvid'], value=counts[witness])

    def cast(self, ref, *, denied=False):
        from recipes import cost_from_oracle

        self.emit('cast', ref=ref, value=int(not denied))
        if denied:
            self.observe(ref, 'in_hand', 1)
            self.receipt()
            return
        self.emit('require_card', ref=ref, property='in_hand', value=0)
        fee = cost_from_oracle(self.oracle[self.entities[ref]['name']])
        self.mana[self.owner] = [have - cost for have, cost in zip(self.mana[self.owner], fee)]
        self.move(ref, 'stack')
        self.receipt(1)
        self.emit('resolve')
        self.emit('refresh')
        self.move(ref, 'battlefield')
        self.receipt()

    def declare_attack(self):
        self.emit('begin_attackers')
        if self.mode not in ('idle_lure', 'idle_lure_block_other'):
            legal = self.mode != 'fresh'
            self.emit('attack', ref='source', value=int(legal))
            self.observe('source', 'attacking', legal)
            self.entities['source']['tapped'] = legal
        if 'ally' in self.entities:
            self.emit('attack', ref='ally', value=1)
            self.observe('ally', 'attacking', 1)
            self.entities['ally']['tapped'] = True
        self.emit('finish_attackers')

    def declare_blockers(self):
        mode = self.mode
        if mode in NO_ASSIGN:
            required = []
        elif mode in TWO:
            required = ['blocker', 'blocker_two'] if mode == 'two_eligible' else ['blocker_two', 'blocker']
        else:
            required = ['blocker_two' if mode == 'flying_reach' else 'blocker']
        if mode == 'refused':
            # Attempt to decline the real selector. The reviewed UI permits
            # Cancel as a retry, then requests this same legal blocker again.
            self.emit('choose_optional_target', ref=required[0], player=self.other)
        for ref in required:
            self.emit('choose_target', ref=ref, player=self.other)
        self.emit('begin_blockers')
        if required:
            self.emit('assert_unused_choices', value=0)
        else:
            self.emit('assert_choices_consumed')
        ally_blocked = mode == 'idle_lure_block_other'
        if ally_blocked or mode == 'reassign':
            self.emit('block', ref='blocker', other='ally', value=int(ally_blocked))
        if mode in ('all_tapped', 'noncreature', 'cannot_block', 'flying_unblocked', 'flying_reach', 'legal_and_tapped'):
            illegal = 'blocker_two' if mode == 'legal_and_tapped' else 'blocker'
            self.emit('block', ref=illegal, other='source', value=0)
        for ref in ('blocker', 'blocker_two'):
            if ref in self.entities:
                self.observe(ref, 'blocking_count', ref in required or ref == 'blocker' and ally_blocked)
        self.emit('advance_combat')
        self.observe('source', 'blocked', bool(required))
        if 'ally' in self.entities:
            self.observe('ally', 'blocked', ally_blocked)
        self.receipt()
        return required, ally_blocked

    def combat_damage(self, required, ally_blocked):
        if self.mode == 'after_block_replay':
            self.snapshot = len(self.steps)
        self.emit('combat_damage')
        self.emit('resolve')
        if self.mode not in ('idle_lure', 'idle_lure_block_other', 'fresh'):
            if required:
                self.exchange('source', required[0])
            else:
                self.life[self.other] -= self.params['power']
        if 'ally' in self.entities:
            if ally_blocked:
                self.exchange('ally', 'blocker')
            else:
                self.life[self.other] -= self.entities['ally']['stats'][0]
        if 'flight' in self.entities and self.entities['source']['zone'] == 'graveyard':
            self.move('flight', 'graveyard')
        self.emit('refresh')
        self.receipt()
        self.emit('end_combat')
        self.emit('cleanup')
        for entity in self.entities.values():
            entity['damage'] = 0
        self.emit('refresh')
        self.receipt()

    def exchange(self, attacker, defender):
        for ref, other in ((attacker, defender), (defender, attacker)):
            self.entities[ref]['damage'] += self.entities[other]['stats'][0]
        for ref in (attacker, defender):
            entity = self.entities[ref]
            if entity['damage'] >= entity['stats'][1]:
                self.move(ref, 'graveyard')

    def case(self):
        from recipes import step

        self.receipt()
        if self.mode in CAST:
            self.cast('source', denied=self.mode in ('unfunded', 'wrong_color'))
        if self.mode not in ('unfunded', 'wrong_color'):
            if self.mode == 'paid_next_turn':
                self.emit('advance_turn')
                self.emit('advance_turn')
                self.emit('untap_step', player=self.owner)
                self.emit('turn', player=self.owner, value=0)
            if 'flight' in self.entities:
                self.emit('choose_target', ref='source', player=self.owner)
                self.cast('flight')
            self.declare_attack()
            required, ally_blocked = self.declare_blockers()
            if self.mode not in TWO:
                self.combat_damage(required, ally_blocked)
        suffix = self.steps[self.snapshot:]
        run = self.before + self.steps[:self.snapshot] + [step('capture')] + suffix
        run += [step('record_state'), step('restore')] + suffix + [step('assert_recorded_state')]
        return dict(name=f'RequiredBlockCreatureRecipe_{self.csvid}_{self.variant}', card=self.name, fixture='empty_main', seed=123,
                    setup=self.setup, steps=run, _recipe=dict(family=FAMILY, version=2, variant=self.variant,
                       params=self.declaration['params'], oracle_text=self.declaration['oracle_text'], untested_components=[
                           'Multiple legal blockers are checked after actual declaration completion; damage assignment among them is outside that branch.',
                           'One real refusal is followed by a vetted legal repeat selection if reoffered. An unconsumed contingency is a presentation receipt independent of combat.',
                       ]))


def expand(name, csvid, declaration, params, selected, rows, oracle):
    validate_witnesses(oracle, rows)
    return [RequiredBlockCase(name, csvid, declaration, params, variant, rows, oracle).case() for variant in selected]
