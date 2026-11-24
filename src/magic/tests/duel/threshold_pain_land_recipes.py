"""Both abilities of the fixed Odyssey Threshold pain lands."""

import json
import re
from collections import Counter
from pathlib import Path

from cast_untap_trigger_recipes import exact, pin


FAMILY = 'threshold_pain_land'
DATA = Path(__file__).parent / 'data/threshold_pain_land_wave8_profiles.json'
COLORS = {'B': 1, 'U': 2, 'G': 3, 'R': 4, 'W': 5}
PRINTED = re.compile(r'\{T\}: Add \{([BUGRW])\}\. This land deals 1 damage to you\.\n'
                     r'Threshold — \{\1\}, \{T\}, Sacrifice this land: (.+)\. '
                     r'Activate only if there are seven or more cards in your graveyard\.')
EFFECTS = {
    'It deals 2 damage to any target': 'damage',
    'Target creature gets -2/-2 until end of turn': 'shrink',
    'Target creature gets +3/+3 until end of turn': 'pump',
    'Target player draws three cards, then discards three cards': 'loot',
    'You gain 4 life': 'life',
}
# These labels belong to the production UI. They do not drive the rules model.
MENUS = {'damage': 'Sac & damage', 'shrink': 'Sac & weaken creature', 'pump': 'Sac & pump creature',
         'loot': 'Sac & draw', 'life': 'Sac & gain life'}
COMMON = ('mana_empty_grave', 'own_six_opposing_eight', 'at_seven', 'above_seven', 'payment', 'off_turn',
          'tapped', 'unfunded', 'wrong_color', 'seventh_card_dies', 'grave_exiled', 'pending_replay', 'mana_repeat_denied')
EXTRA = {
    'damage': ('own_player', 'creature', 'lethal_creature', 'planeswalker', 'target_destroyed'),
    'shrink': ('own_creature', 'lethal_creature', 'cleanup', 'no_creatures', 'target_destroyed'),
    'pump': ('opponent_creature', 'cleanup', 'no_creatures', 'target_destroyed'),
    'loot': ('own_player', 'existing_hand', 'own_existing_hand', 'other_discard_order'),
    'life': ('recover_from_one',),
}
GRAVE = ('Forest', 'Mountain', 'Swamp', 'Island', 'Plains', 'Grizzly Bears', 'Hill Giant', 'Ornithopter')
LIBRARY = ('Forest', 'Island', 'Grizzly Bears', 'Mountain', 'Plains', 'Hill Giant', 'Swamp', 'Island')
WITNESSES = set(GRAVE + LIBRARY + ('Suntail Hawk', 'Jace Beleren', "Tormod's Crypt"))


def pins():
    return json.loads(DATA.read_text(encoding='utf8'))


def classify(record):
    if (record.get('layout') != 'normal' or record.get('card_faces') or record.get('selected_face')
            or record.get('type_line') != 'Land' or record.get('mana_cost') != ''
            or record.get('keywords') != ['Threshold']):
        raise ValueError('ordinary whole Threshold land required')
    match = PRINTED.fullmatch(record.get('oracle_text', ''))
    if not match or match[2] not in EFFECTS:
        raise ValueError('paired pain mana and exact Threshold effect required')
    return dict(color=COLORS[match[1]], effect=EFFECTS[match[2]])


def variants(params):
    return [f'{mode}_{seat}' for seat in (0, 1) for mode in COMMON + EXTRA[params['effect']]] + ['mana_with_alternative_0']


def spec(declaration, record):
    if (declaration.get('family') != FAMILY or type(declaration.get('version')) is not int
            or declaration['version'] != 2 or declaration.get('oracle_text') != record.get('oracle_text')
            or not exact(pins()['sources'].get(record['name']), pin(record))):
        raise ValueError('exact whole Threshold source and supported version required')
    params = classify(record)
    selected = declaration.get('variants')
    if not exact(params, declaration.get('params')):
        raise ValueError('Threshold parameters must derive from full Oracle')
    if (not isinstance(selected, list) or not selected or any(not isinstance(v, str) for v in selected)
            or len(set(selected)) != len(selected) or set(selected) - set(variants(params))):
        raise ValueError('distinct reviewed Threshold variants required')
    return dict(params, family=FAMILY)


def validate_witnesses(oracle, rows):
    data = pins()
    records = data['sources'] | data['witnesses']
    if set(data['witnesses']) != WITNESSES or set(data['handlers']) != set(records):
        raise ValueError('complete fixture identity and implementation pins required')
    for name, expected in records.items():
        row = rows[name]
        if not exact(pin(oracle[name]), expected) or row['handler'] != data['handlers'][name]:
            raise ValueError(f'Oracle or handler changed for {name}')
        if name in data['sources'] and (row['kind'] != 'card' or row['handler'] in ('card_dummy', 'NULL', '0')):
            raise ValueError(f'implemented normal Threshold source required for {name}')


class ThresholdCase:
    def __init__(self, name, csvid, declaration, params, variant, rows, oracle):
        from recipes import step

        mode, seat = variant.rsplit('_', 1)
        self.mode, self.owner, self.variant = mode, int(seat), variant
        self.other = 1 - self.owner
        self.name, self.csvid, self.declaration, self.rows, self.oracle = name, csvid, declaration, rows, oracle
        self.effect, self.color = params['effect'], params['color']
        self.zones = {zone: [[], []] for zone in ('hand', 'battlefield', 'library', 'graveyard', 'exile')}
        self.setup, self.steps, self.permanents = [], [], {}
        self.pools = [[0] * 6 for _ in (0, 1)]
        self.life = [20, 20]
        self.discards = 0
        self.pools[self.owner][self.color] = 1 if mode == 'payment' else 2
        if mode in ('unfunded', 'wrong_color', 'mana_empty_grave', 'mana_repeat_denied'):
            self.pools[self.owner][self.color] = 0
            if mode == 'wrong_color':
                self.pools[self.owner][1 if self.color != 1 else 2] = 1
        if mode == 'recover_from_one':
            self.life[self.owner] = 1
        self.recipient = self.owner if mode in ('own_player', 'own_existing_hand') else self.other
        self.setup_cards()
        self.names = sorted({entry['name'] for entry in self.setup})
        self.discard_policy = 2 if mode == 'other_discard_order' else 1
        self.before = [step('turn', player=self.other if mode == 'off_turn' else self.owner, value=0),
                       step('discard_policy', value=self.discard_policy)]
        self.before += [step('set_mana', player=who, color=color, value=n)
                        for who in (0, 1) for color, n in enumerate(self.pools[who])]
        if mode == 'recover_from_one':
            self.before.append(step('set_life', player=self.owner, value=1))
        self.before.append(step('refresh'))
        self.snapshot = 0

    def setup_cards(self):
        mode = self.mode
        grave_count = {'mana_empty_grave': 0, 'own_six_opposing_eight': 6, 'above_seven': 8,
                       'seventh_card_dies': 6, 'mana_repeat_denied': 0}.get(mode, 7)
        for who, count in [(self.owner, grave_count), (self.other, 8 if mode == 'own_six_opposing_eight' else 2)]:
            for index, name in enumerate(GRAVE[:count]):
                self.add(name, f'grave_{who}_{index}', who, 'graveyard')
            self.add('Plains', f'exile_{who}', who, 'exile')
            library = LIBRARY
            if self.effect == 'loot' and who == self.recipient and mode in ('existing_hand', 'own_existing_hand', 'other_discard_order'):
                if who == 1:
                    # Production AI discards randomly. This board has a known
                    # multiset independent of which exact instance it chooses.
                    hand = ('Forest', 'Forest')
                    library = ('Forest',) * 3 + LIBRARY[3:]
                else:
                    hand = ('Hill Giant', 'Plains')
                for index, name in enumerate(hand):
                    self.add(name, f'hand_{who}_{index}', who, 'hand')
            for index, name in enumerate(library):
                self.add(name, f'library_{who}_{index}', who, 'library')
        self.add(self.name, 'source', self.owner, tapped=mode == 'tapped')
        if mode == 'seventh_card_dies':
            self.add('Suntail Hawk', 'seventh', self.owner)
        if mode == 'grave_exiled':
            self.add("Tormod's Crypt", 'crypt', self.other)
        if self.effect == 'damage' and mode == 'planeswalker':
            self.add('Jace Beleren', 'target', self.other)
        elif mode != 'no_creatures' and (self.effect in ('shrink', 'pump') or
                self.effect == 'damage' and mode in ('creature', 'lethal_creature', 'target_destroyed')):
            target = 'Grizzly Bears' if mode == 'lethal_creature' else 'Hill Giant'
            friendly = mode == 'own_creature' or self.effect == 'pump' and mode != 'opponent_creature'
            self.add(target, 'target', self.owner if friendly else self.other)
            self.add('Grizzly Bears', 'spare', self.other if friendly else self.owner)

    def add(self, name, ref, who, zone='battlefield', tapped=False):
        from recipes import card

        entry = card(name, ref, who, zone)
        if zone == 'battlefield':
            entry['state'] = ['tapped' if tapped else 'untapped']
            record = self.oracle[name]
            self.permanents[ref] = dict(name=name, who=who, alive=True, tapped=tapped,
                                        pt=tuple(int(record[key]) for key in ('power', 'toughness')) if 'power' in record else None,
                                        damage=0, loyalty=int(record['loyalty']) if 'loyalty' in record else None)
        self.setup.append(entry)
        self.zones[zone][who].append(name)

    def emit(self, op, **fields):
        from recipes import step

        self.steps.append(step(op, **fields))

    def move(self, name, who, start, finish):
        self.zones[start][who].remove(name)
        self.zones[finish][who].append(name)

    def die(self, ref):
        entity = self.permanents[ref]
        entity['alive'] = False
        self.move(entity['name'], entity['who'], 'battlefield', 'graveyard')

    def observe(self, ref, prop, value):
        self.emit('assert_card', ref=ref, property=prop, value=int(value))

    def receipt(self, stack=0):
        self.emit('assert_stack', value=stack)
        self.emit('assert_choices_consumed')
        for ref, entity in self.permanents.items():
            self.observe(ref, 'in_play', entity['alive'])
            if entity['alive']:
                self.observe(ref, 'tapped', entity['tapped'])
                if entity['pt'] is not None:
                    for prop, value in zip(('power', 'toughness', 'damage'), (*entity['pt'], entity['damage'])):
                        self.observe(ref, prop, value)
                if entity['loyalty'] is not None:
                    self.emit('assert_counter', ref=ref, counter_kind='loyalty', value=entity['loyalty'])
        for who in (0, 1):
            self.emit('assert_life', player=who, value=self.life[who])
            for color, value in enumerate(self.pools[who]):
                self.emit('assert_mana', player=who, color=color, value=value)
            for zone, seats in self.zones.items():
                names = seats[who]
                self.emit('assert_zone', player=who, zone=zone, value=len(names))
                if zone == 'library':
                    for index, name in enumerate(names):
                        self.emit('assert_zone_card', player=who, zone=zone, index=index, value=self.rows[name]['csvid'])
                else:
                    counts = Counter(names)
                    for name in self.names:
                        self.emit('assert_zone_multiplicity', player=who, zone=zone, index=self.rows[name]['csvid'], value=counts[name])
        if self.effect == 'loot':
            self.emit('assert_discard_choices', value=self.discards)

    def mode_intent(self, label, *, allow_auto=False):
        self.emit('choose_activation_mode', ref='source', player=self.owner, mode_label=label,
                  fallback_color=self.color, allow_auto_nonmana=allow_auto)

    def mana(self):
        self.mode_intent('Get mana')
        self.emit('activate', ref='source', value=1)
        self.emit('require_activation_mode', value=int(self.mode == 'mana_with_alternative'))
        self.emit('resolve')
        self.emit('refresh')
        self.permanents['source']['tapped'] = True
        self.pools[self.owner][self.color] += 1
        self.life[self.owner] -= 1
        self.receipt()

    def sacrifice(self):
        self.mode_intent(MENUS[self.effect], allow_auto=self.owner == 1)
        if 'target' in self.permanents:
            self.emit('choose_target', ref='target', player=self.owner)
        elif self.effect in ('damage', 'loot'):
            self.emit('choose_player', player=self.owner, value=self.recipient)
        self.emit('activate', ref='source', value=1)
        self.emit('require_activation_mode', value=1)
        self.pools[self.owner][self.color] -= 1
        self.die('source')
        self.receipt(1)
        if self.mode == 'pending_replay':
            self.snapshot = len(self.steps)
        if self.mode == 'target_destroyed':
            self.emit('destroy', ref='target')
            self.emit('refresh')
            self.die('target')
            self.receipt(1)
        self.emit('resolve')
        self.apply_effect()
        self.emit('refresh')
        self.receipt()
        if self.mode == 'cleanup':
            target = self.permanents['target']
            record = self.oracle[target['name']]
            target['pt'] = int(record['power']), int(record['toughness'])
            self.emit('cleanup')
            self.emit('refresh')
            self.receipt()

    def apply_effect(self):
        if self.effect == 'life':
            self.life[self.owner] += 4
        elif self.effect == 'loot':
            who = self.recipient
            deck, hand = self.zones['library'][who], self.zones['hand'][who]
            for name in deck[:3]:
                self.move(name, who, 'library', 'hand')
            select = max if self.discard_policy == 2 else min
            for _ in range(3):
                name = select(hand, key=lambda n: self.rows[n]['csvid'])
                self.move(name, who, 'hand', 'graveyard')
            self.discards += 3 if who == 0 else 0
        elif 'target' not in self.permanents:
            self.life[self.recipient] -= 2
        else:
            target = self.permanents['target']
            if not target['alive']:
                return
            if self.effect == 'damage':
                if target['loyalty'] is not None:
                    target['loyalty'] -= 2
                else:
                    target['damage'] += 2
            else:
                amount = -2 if self.effect == 'shrink' else 3
                target['pt'] = tuple(value + amount for value in target['pt'])
            if target['pt'] is not None and target['damage'] >= target['pt'][1]:
                self.die('target')

    def case(self):
        from recipes import step

        self.receipt()
        if self.mode == 'seventh_card_dies':
            self.emit('destroy', ref='seventh')
            self.emit('refresh')
            self.die('seventh')
            self.receipt()
        if self.mode == 'grave_exiled':
            self.emit('choose_player', player=self.other, value=self.owner)
            self.emit('activate', ref='crypt', value=1)
            self.die('crypt')
            self.emit('resolve')
            self.emit('refresh')
            for name in self.zones['graveyard'][self.owner][:]:
                self.move(name, self.owner, 'graveyard', 'exile')
            self.receipt()
        if self.mode != 'tapped':
            self.emit('require_can_activate', ref='source', value=1)
            if self.mode in ('mana_empty_grave', 'own_six_opposing_eight', 'unfunded', 'wrong_color', 'grave_exiled',
                             'mana_repeat_denied', 'mana_with_alternative', 'no_creatures'):
                self.mana()
            else:
                self.sacrifice()
        if self.mode in ('tapped', 'mana_repeat_denied'):
            self.emit('can_activate', ref='source', value=0)
            self.emit('activate', ref='source', value=0)
            self.receipt()
        suffix = self.steps[self.snapshot:]
        run = self.before + self.steps[:self.snapshot] + [step('capture')] + suffix
        run += [step('record_state'), step('restore')] + suffix + [step('assert_recorded_state')]
        return dict(name=f'ThresholdPainLandRecipe_{self.csvid}_{self.variant}', card=self.name,
                    fixture='empty_main', seed=123, setup=self.setup, steps=run,
                    _recipe=dict(family=FAMILY, version=2, variant=self.variant, params=self.declaration['params'],
                                 oracle_text=self.declaration['oracle_text'], untested_components=[
                                     'The UI exposes a different mode only when its actual engine menu or automatic selection makes it available.',
                                     'AI discard receipts use a known final multiset independent of random instance selection.',
                                     'Damage prevention and library-loss compositions are outside the bounded fixture.',
                                 ]))


def expand(name, csvid, declaration, params, selected, rows, oracle):
    validate_witnesses(oracle, rows)
    return [ThresholdCase(name, csvid, declaration, params, variant, rows, oracle).case() for variant in selected]
