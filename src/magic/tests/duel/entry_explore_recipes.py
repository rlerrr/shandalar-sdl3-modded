"""Mandatory self-entry explore through actual spell and re-entry paths."""

import json
import re
from collections import Counter
from pathlib import Path

from cast_untap_trigger_recipes import exact, pin


FAMILY = 'entry_explore'
DATA = Path(__file__).parent / 'data/entry_explore_wave8_profiles.json'
CLAUSE = ("When this creature enters, it explores. (Reveal the top card of your library. Put that card into your hand "
          "if it's a land. Otherwise, put a +1/+1 counter on this creature, then put the card back or put it into your graveyard.)")
MODES = ('land', 'nonland_keep', 'nonland_mill', 'artifact_land', 'artifact_nonland', 'instant_nonland',
         'empty', 'only_land', 'only_nonland_mill', 'countered', 'insufficient', 'wrong_color', 'pending_replay',
         'blink_keep_mill', 'blink_mill_land', 'bounce_recast', 'grave_entry', 'two_entries', 'cleanup')
SUPPORT = {'blink_keep_mill': 'Cloudshift', 'blink_mill_land': 'Cloudshift', 'bounce_recast': 'Unsummon',
           'grave_entry': 'Unearth', 'countered': 'Essence Scatter'}
DEFAULT_TOP = 'Grizzly Bears'
LAND = 'Forest'
SUFFIX = ('Island', 'Hill Giant', 'Plains', 'Mountain', 'Island')
WITNESSES = set(SUFFIX) | set(SUPPORT.values()) | {DEFAULT_TOP, LAND, 'Darksteel Citadel', 'Darksteel Relic', 'Giant Growth'}


def pins():
    return json.loads(DATA.read_text(encoding='utf8'))


def classify(record):
    from recipes import cost_from_oracle

    if (record.get('layout') != 'normal' or record.get('card_faces') or record.get('selected_face')
            or not record.get('type_line', '').startswith('Creature — ') or record.get('keywords') != ['Explore']
            or record.get('oracle_text') != CLAUSE or not re.fullmatch(r'\{[12]\}\{[BG]\}', record.get('mana_cost', ''))
            or any(not re.fullmatch(r'[0-9]', record.get(stat, '')) for stat in ('power', 'toughness'))
            or int(record['toughness']) < 1):
        raise ValueError('whole normal fixed-cost mandatory explore creature required')
    return dict(mana=cost_from_oracle(record), power=int(record['power']), toughness=int(record['toughness']))


def variants(params):
    return [f'{mode}_{owner}' for owner in (0, 1) for mode in MODES]


def spec(declaration, record):
    if (declaration.get('family') != FAMILY or type(declaration.get('version')) is not int
            or declaration['version'] != 2 or declaration.get('oracle_text') != record.get('oracle_text')
            or not exact(pins()['sources'].get(record['name']), pin(record))):
        raise ValueError('exact explore source pin and recipe version required')
    params = classify(record)
    if not exact(params, declaration.get('params')):
        raise ValueError('explore parameters must derive from complete Oracle')
    selected = declaration.get('variants')
    if (not isinstance(selected, list) or not selected or any(not isinstance(v, str) for v in selected)
            or len(set(selected)) != len(selected) or set(selected) - set(variants(params))):
        raise ValueError('distinct reviewed explore variants required')
    return dict(params, family=FAMILY)


def validate_witnesses(oracle, rows):
    data = pins()
    records = data['sources'] | data['witnesses']
    if set(data['witnesses']) != WITNESSES or set(data['handlers']) != set(records):
        raise ValueError('complete explore witness identity and handler pins required')
    for name, expected in records.items():
        row = rows[name]
        if not exact(pin(oracle[name]), expected) or row['handler'] != data['handlers'][name]:
            raise ValueError(f'Oracle or handler changed for {name}')
        if name in (set(data['sources']) | set(SUPPORT.values())) and (
                row['kind'] != 'card' or row['handler'] in ('card_dummy', '0', 'NULL')):
            raise ValueError(f'implemented primary required for {name}')


class ExploreCase:
    def __init__(self, name, csvid, declaration, params, variant, rows, oracle):
        from recipes import cost_from_oracle, step

        mode, seat = variant.rsplit('_', 1)
        self.name, self.csvid, self.declaration, self.params = name, csvid, declaration, params
        self.mode, self.owner, self.other, self.variant = mode, int(seat), 1 - int(seat), variant
        self.rows, self.oracle = rows, oracle
        self.zones = {zone: [[], []] for zone in ('hand', 'battlefield', 'graveyard', 'library', 'exile')}
        self.setup, self.steps, self.counters, self.mana = [], [], {}, [[0] * 6, [0] * 6]
        self.costs = {'source': list(params['mana'])}
        self.refs = {'source': (name, self.owner)}
        initial_zone = 'graveyard' if mode == 'grave_entry' else 'hand'
        self.add(name, 'source', self.owner, initial_zone)
        if mode == 'two_entries':
            self.add(name, 'second', self.owner, 'hand')
            self.refs['second'], self.costs['second'] = self.refs['source'], self.costs['source']
        if mode in SUPPORT:
            support = SUPPORT[mode]
            caster = self.other if mode == 'countered' else self.owner
            self.refs['support'], self.costs['support'] = (support, caster), cost_from_oracle(oracle[support])
            self.add(support, 'support', caster, 'hand')
        for ref, (spell, caster) in self.refs.items():
            if mode == 'grave_entry' and ref == 'source':
                if int(oracle[spell]['cmc']) > 3:
                    raise ValueError('Unearth witness requires mana value at most three')
                continue
            self.mana[caster] = [have + cost for have, cost in zip(self.mana[caster], self.costs[ref])]
        if mode == 'bounce_recast':
            # The same hand ref is cast twice, each time at its full cost.
            self.mana[self.owner] = [have + cost for have, cost in zip(self.mana[self.owner], self.costs['source'])]
        if mode == 'insufficient':
            self.mana[self.owner][0] -= 1
        if mode == 'wrong_color':
            color = next(color for color in range(1, 6) if self.mana[self.owner][color])
            self.mana[self.owner][2] += self.mana[self.owner][color]
            self.mana[self.owner][color] = 0
        for who in (0, 1):
            self.add('Mountain', f'grave_{who}', who, 'graveyard')
            self.add('Plains', f'exile_{who}', who, 'exile')
            library = self.library() if who == self.owner else ('Giant Growth', *SUFFIX)
            for index, card in enumerate(library):
                self.add(card, f'library_{who}_{index}', who, 'library')
        self.names = sorted({row['name'] for row in self.setup})
        self.before = [step('turn', player=self.owner, value=0), step('present_ai_dialogs', value=1)]
        self.before += [step('set_mana', player=who, color=color, value=amount)
                        for who in (0, 1) for color, amount in enumerate(self.mana[who])]
        self.before.append(step('refresh'))
        self.snapshot = 0

    def library(self):
        if self.mode == 'empty':
            return ()
        if self.mode == 'only_land':
            return (LAND,)
        if self.mode == 'only_nonland_mill':
            return (DEFAULT_TOP,)
        top = {'land': LAND, 'artifact_land': 'Darksteel Citadel', 'artifact_nonland': 'Darksteel Relic',
               'instant_nonland': 'Giant Growth', 'grave_entry': LAND}.get(self.mode, DEFAULT_TOP)
        tail = (LAND, *SUFFIX) if self.mode == 'blink_mill_land' else SUFFIX
        return (top, *tail)

    def add(self, name, ref, who, zone):
        from recipes import card

        self.setup.append(card(name, ref, who, zone))
        self.zones[zone][who].append(name)

    def move(self, name, who, start, finish):
        self.zones[start][who].remove(name)
        self.zones[finish][who].append(name)

    def emit(self, op, **fields):
        from recipes import step

        self.steps.append(step(op, **fields))

    def receipt(self, stack=0, pending=None):
        self.emit('assert_stack', value=stack)
        self.emit('assert_choices_consumed')
        for ref, count in self.counters.items():
            for prop, value in [('in_play', 1), ('plus1_counters', count), ('power', self.params['power'] + count),
                                ('toughness', self.params['toughness'] + count)]:
                self.emit('assert_card', ref=ref, property=prop, value=value)
        for who in (0, 1):
            self.emit('assert_life', player=who, value=20)
            for color, amount in enumerate(self.mana[who]):
                self.emit('assert_mana', player=who, color=color, value=amount)
            for zone, seats in self.zones.items():
                names = seats[who]
                if pending is not None and zone == 'hand' and self.refs[pending][1] == who:
                    # This observer includes the physical pending slot. The
                    # cast also independently verifies in_hand=0.
                    names = names + [self.refs[pending][0]]
                self.emit('assert_zone', player=who, zone=zone, value=len(names))
                if zone == 'library':
                    for index, name in enumerate(names):
                        self.emit('assert_zone_card', player=who, zone=zone, index=index, value=self.rows[name]['csvid'])
                else:
                    counts = Counter(names)
                    for name in self.names:
                        self.emit('assert_zone_multiplicity', player=who, zone=zone, index=self.rows[name]['csvid'], value=counts[name])

    def cast(self, ref, *, response=False, denied=False):
        name, who = self.refs[ref]
        self.emit('cast_response' if response else 'cast', ref=ref, value=int(not denied))
        self.emit('assert_choices_consumed')
        if not denied:
            self.emit('require_card', ref=ref, property='in_hand', value=0)
            self.mana[who] = [have - cost for have, cost in zip(self.mana[who], self.costs[ref])]
            self.zones['hand'][who].remove(name)

    def explore(self, ref, mill=False):
        # Oracle's otherwise branch includes revealing no card. The choice
        # applies only to an actual nonland card, not to the empty library.
        deck = self.zones['library'][self.owner]
        if not deck:
            self.counters[ref] += 1
            return
        top = deck[0]
        self.emit('choose_dialog', player=self.owner, value=0)  # public reveal acknowledgement
        if 'Land' in self.oracle[top]['type_line'].split(' — ', 1)[0].split():
            self.move(top, self.owner, 'library', 'hand')
            return
        self.counters[ref] += 1
        self.emit('choose_dialog', player=self.owner, value=int(mill))
        if mill:
            self.move(top, self.owner, 'library', 'graveyard')

    def add_entered(self, ref):
        self.zones['battlefield'][self.owner].append(self.name)
        self.counters[ref] = 0

    def finish(self, *, bind=False):
        self.emit('resolve')
        self.emit('refresh')
        if bind:
            self.emit('bind_permanent', ref='source', player=self.owner, value=self.csvid)
        self.receipt()

    def first_entry(self):
        denied = self.mode in ('insufficient', 'wrong_color')
        self.cast('source', denied=denied)
        if denied:
            self.receipt()
            return
        self.receipt(1, pending='source')
        if self.mode == 'pending_replay':
            self.snapshot = len(self.steps)
        if self.mode == 'countered':
            self.emit('announce_spell', ref='source')
            self.cast('support', response=True)
            self.emit('assert_stack', value=2)
            self.emit('resolve_response')
            self.zones['graveyard'][self.owner].append(self.name)
            self.zones['graveyard'][self.other].append(SUPPORT['countered'])
            self.emit('refresh')
            self.receipt()
            return
        self.add_entered('source')
        self.explore('source', self.mode in ('nonland_mill', 'only_nonland_mill', 'artifact_nonland', 'blink_mill_land', 'instant_nonland'))
        self.finish()

    def reenter(self):
        mode = self.mode
        if mode == 'grave_entry':
            self.emit('choose_zone_card', player=self.owner, value=self.csvid)
            self.cast('support')
            self.receipt(1, pending='support')
            self.zones['graveyard'][self.owner].remove(self.name)
            self.add_entered('source')
            self.explore('source')
            self.zones['graveyard'][self.owner].append(SUPPORT[mode])
            self.finish(bind=True)
        elif mode in ('blink_keep_mill', 'blink_mill_land'):
            self.emit('choose_target', ref='source', player=self.owner)
            self.cast('support')
            self.receipt(1, pending='support')
            # Cloudshift returns a new incarnation of the same physical card.
            self.counters['source'] = 0
            self.explore('source', mill=mode == 'blink_keep_mill')
            self.zones['graveyard'][self.owner].append(SUPPORT[mode])
            self.finish(bind=True)
        elif mode == 'bounce_recast':
            self.emit('choose_target', ref='source', player=self.owner)
            self.cast('support')
            self.receipt(1, pending='support')
            self.move(self.name, self.owner, 'battlefield', 'hand')
            self.zones['graveyard'][self.owner].append(SUPPORT[mode])
            del self.counters['source']
            self.finish()
            self.emit('bind_hand', ref='source', player=self.owner, value=self.csvid)
            self.cast('source')
            self.receipt(1, pending='source')
            self.add_entered('source')
            self.explore('source', mill=True)
            self.finish()
        elif mode == 'two_entries':
            self.cast('second')
            self.receipt(1, pending='second')
            self.add_entered('second')
            self.explore('second', mill=True)
            self.finish()

    def case(self):
        from recipes import step

        self.receipt()
        if self.mode != 'grave_entry':
            self.first_entry()
        self.reenter()
        if self.mode == 'cleanup':
            self.emit('cleanup')
            self.emit('refresh')
            self.receipt()
        suffix = self.steps[self.snapshot:]
        sequence = self.before + self.steps[:self.snapshot] + [step('capture')] + suffix
        sequence += [step('record_state'), step('restore')] + suffix + [step('assert_recorded_state')]
        return dict(name=f'EntryExploreRecipe_{self.csvid}_{self.variant}', card=self.name, fixture='empty_main', seed=123,
                    setup=self.setup, steps=sequence, _recipe=dict(family=FAMILY, version=2, variant=self.variant,
                        params=self.declaration['params'], oracle_text=self.declaration['oracle_text'], untested_components=[
                            'Reveal receipt is paired with exact library order and resulting zones. The host does not expose the displayed card identifier independently.',
                            'The real resolve helper drains the entry trigger. No fixture interrupts that synchronous trigger to remove its source.',
                        ]))


def expand(name, csvid, declaration, params, selected, rows, oracle):
    validate_witnesses(oracle, rows)
    return [ExploreCase(name, csvid, declaration, params, variant, rows, oracle).case() for variant in selected]
