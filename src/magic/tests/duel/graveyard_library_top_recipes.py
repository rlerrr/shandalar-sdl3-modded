"""Ordered graveyard recovery with no draw, an immediate draw, or a slowtrip."""

import json
import re
from collections import Counter
from pathlib import Path

from cast_untap_trigger_recipes import exact, pin

FAMILY = 'graveyard_library_top'
DATA = Path(__file__).parent / 'data/graveyard_library_top_wave8_profiles.json'
TOP = re.compile(r'Put (any number of|up to three) target (creature|artifact) cards from your graveyard on top of your library\.')
DRAW = {'Draw a card.': 'immediate', "Draw a card at the beginning of the next turn's upkeep.": 'next_upkeep'}
TARGETS = {
    'creature': ('Grizzly Bears', 'Silvercoat Lion', 'Goblin Piker'),
    'artifact': ('Sol Ring', 'Darksteel Relic', 'Ornithopter'),
}
LIBRARY = ['Mountain', 'Island', 'Plains', 'Forest', 'Lightning Bolt']
WITNESSES = set(LIBRARY) | {card for group in TARGETS.values() for card in group} | {
    'Giant Growth', 'Reclaim', 'Silent Gravestone', 'Counterspell',
}


def pins():
    return json.loads(DATA.read_text(encoding='utf8'))


def classify(record):
    from recipes import cost_from_oracle

    if (record.get('layout') != 'normal' or record.get('card_faces')
            or record.get('selected_face') or record.get('type_line') not in ('Instant', 'Sorcery')
            or record.get('keywords')):
        raise ValueError('normal whole graveyard recovery spell required')
    lines = record.get('oracle_text', '').splitlines()
    match = TOP.fullmatch(lines[0]) if lines else None
    if not match or len(lines) > 2 or (len(lines) == 2 and lines[1] not in DRAW):
        raise ValueError('unreviewed whole ordered graveyard recovery text')
    return {
        'scope': match[2], 'maximum': 3 if match[1] == 'up to three' else None,
        'draw': DRAW[lines[1]] if len(lines) == 2 else 'none',
        'mana': cost_from_oracle(record), 'instant': record['type_line'] == 'Instant',
    }


def variants(params):
    modes = ['effect', 'alternate', 'zero_targets', 'wrong_type', 'empty_graveyard', 'other_graveyard',
             'graveyard_shroud', 'mixed_filter', 'duplicate_targets', 'artifact_creature', 'target_lost',
             'partially_lost', 'countered', 'insufficient_mana', 'wrong_color', 'pending_replay']
    if params['scope'] == 'creature':
        modes.remove('artifact_creature')
    if params['instant']:
        modes.append('off_turn')
    if params['maximum'] == 3:
        modes.append('three_of_four')
    else:
        modes.append('four_targets')
    return [f'{mode}_{owner}' for owner in (0, 1) for mode in modes] + ['order_forward_0', 'order_reverse_0']


def spec(declaration, record):
    if (declaration.get('family') != FAMILY or type(declaration.get('version')) is not int
            or declaration['version'] != 2 or declaration.get('oracle_text') != record.get('oracle_text')
            or not exact(pins()['sources'].get(record['name']), pin(record))):
        raise ValueError('complete pinned source Oracle and family version required')
    params = classify(record)
    if not exact(declaration.get('params'), params):
        raise ValueError('ordered recovery parameters must match Oracle')
    selected = declaration.get('variants')
    if (not isinstance(selected, list) or not selected or any(not isinstance(v, str) for v in selected)
            or len(set(selected)) != len(selected) or set(selected) - set(variants(params))):
        raise ValueError('distinct reviewed ordered recovery variants required')
    return dict(params, family=FAMILY)


def validate_witnesses(oracle, rows):
    data = pins()
    if set(data['witnesses']) != WITNESSES:
        raise ValueError('complete fixture witness set required')
    records = dict(data['witnesses'], **data['sources'])
    if set(records) != set(data['handlers']):
        raise ValueError('complete presentation handler pins required')
    for name, expected in records.items():
        row = rows.get(name, {})
        if (not exact(pin(oracle.get(name, {})), expected) or row.get('handler') != data['handlers'][name]
                or row.get('handler') in ('0', 'NULL', None)):
            raise ValueError(f'changed Oracle or presentation handler for {name}')
        if name in data['sources'] and (row['kind'] != 'card' or row['handler'] == 'card_dummy'):
            raise ValueError(f'implemented primary required for {name}')


class RecoveryCase:
    def __init__(self, name, csvid, declaration, params, variant, rows, oracle):
        from recipes import card, step

        self.name, self.csvid, self.declaration, self.params = name, csvid, declaration, params
        self.variant, self.rows, self.oracle = variant, rows, oracle
        self.mode, controller = variant.rsplit('_', 1)
        self.owner = int(controller)
        self.active = 1 - self.owner if self.mode == 'off_turn' else self.owner
        self.setup, self.steps = [], []
        self.zones = {zone: [[], []] for zone in ('hand', 'library', 'graveyard', 'exile', 'battlefield')}
        self.pending = [[], []]
        self.mana = [[0] * 6, [0] * 6]
        self.mana[self.owner] = list(params['mana'])
        self.target_count_based_limit = params['scope'] == 'creature' and params['maximum'] is None
        primary, alternate, mixed = TARGETS[params['scope']]
        ineligible = ['Giant Growth', 'Forest']
        own_grave = [primary, alternate, mixed, *ineligible]
        opposing_grave = [primary, alternate, *ineligible]
        self.selected = [primary]
        if self.mode == 'alternate':
            self.selected = [alternate]
        elif self.mode == 'artifact_creature':
            self.selected = [mixed]
        elif self.mode in ('zero_targets', 'wrong_type', 'empty_graveyard', 'other_graveyard', 'graveyard_shroud'):
            self.selected = []
        elif self.mode in ('order_forward', 'order_reverse', 'partially_lost'):
            self.selected = [primary, alternate]
        elif self.mode == 'duplicate_targets':
            own_grave = [primary, primary, alternate, *ineligible]
            self.selected = [primary, primary]
        elif self.mode in ('three_of_four', 'four_targets'):
            own_grave = [primary] * 4 + ineligible
            self.selected = [primary] * (3 if self.mode == 'three_of_four' else 4)
        if self.mode in ('wrong_type', 'other_graveyard'):
            own_grave = ineligible
            if self.mode == 'wrong_type':
                opposing_grave = ineligible[:]
        elif self.mode == 'empty_graveyard':
            own_grave = []
        if self.mode == 'insufficient_mana':
            color = next(color for color, amount in enumerate(self.mana[self.owner]) if amount)
            self.mana[self.owner][color] -= 1
        elif self.mode == 'wrong_color':
            self.mana[self.owner] = [0, 0, 0, 0, sum(params['mana']), 0]
        self.response = 'Counterspell' if self.mode == 'countered' else 'Reclaim' if self.mode in ('target_lost', 'partially_lost') else None
        self.response_owner = 1 - self.owner if self.response == 'Counterspell' else self.owner
        self.response_cost = [0, 0, 2, 0, 0, 0] if self.response == 'Counterspell' else [0, 0, 0, 1, 0, 0]
        if self.response:
            self.mana[self.response_owner] = [a + b for a, b in zip(self.mana[self.response_owner], self.response_cost)]
        for player in (0, 1):
            initial = {
                'hand': ['Lightning Bolt'], 'library': LIBRARY,
                'graveyard': own_grave if player == self.owner else opposing_grave, 'exile': ['Plains'],
            }
            for zone, names in initial.items():
                for index, fixture_name in enumerate(names):
                    self.setup.append(card(fixture_name, f'{zone}_{player}_{index}', player, zone))
                self.zones[zone][player].extend(names)
        self.setup.append(card(name, 'source', self.owner, 'hand'))
        self.zones['hand'][self.owner].append(name)
        if self.response:
            self.setup.append(card(self.response, 'response', self.response_owner, 'hand'))
            self.zones['hand'][self.response_owner].append(self.response)
        if self.mode == 'graveyard_shroud':
            self.setup.append(card('Silent Gravestone', 'seal', 1 - self.owner))
            self.zones['battlefield'][1 - self.owner].append('Silent Gravestone')
        self.live_names = sorted({entry['name'] for entry in self.setup})
        self.before = [step('turn', player=self.active, value=0)]
        self.before.extend(step('set_mana', player=player, color=color, value=amount)
                           for player in (0, 1) for color, amount in enumerate(self.mana[player]))
        self.before.append(step('refresh'))

    def emit(self, op, **kwargs):
        from recipes import step

        self.steps.append(step(op, **kwargs))

    def card_choice(self, name, op='choose_zone_card'):
        self.emit(op, player=self.owner, value=self.rows[name]['csvid'])

    def is_eligible(self, name):
        card_types = self.oracle[name]['type_line'].partition(' — ')[0].lower().split()
        return self.params['scope'] in card_types

    def plan_targets(self):
        grave = self.zones['graveyard'][self.owner]
        if not grave or self.mode == 'graveyard_shroud':
            return
        candidates = [name for name in grave if self.is_eligible(name)]
        max_offers = len(candidates) if self.target_count_based_limit else min(self.params['maximum'] or 10, len(grave))
        for name in self.selected:
            self.card_choice(name)
        if len(self.selected) < max_offers:
            remaining = Counter(grave) - Counter(self.selected)
            fallback = next((name for name in remaining if self.is_eligible(name)), next(iter(remaining)))
            self.card_choice(fallback, 'choose_optional_zone')

    def pay_cast(self, ref, name, owner, cost, response=False):
        self.emit('cast_response' if response else 'cast', ref=ref, value=1)
        self.emit('assert_unused_choices', value=0)
        self.emit('require_card', ref=ref, property='in_hand', value=0)
        self.mana[owner] = [a - b for a, b in zip(self.mana[owner], cost)]
        self.zones['hand'][owner].remove(name)
        self.pending[owner].append(name)
        self.emit('assert_stack', value=2 if response else 1)
        self.emit('announce_spell', ref=ref)

    def receipt(self, stack=0):
        self.emit('assert_stack', value=stack)
        self.emit('assert_choices_consumed')
        self.emit('assert_forced_zone_choices', value=0)
        for player in (0, 1):
            self.emit('assert_life', player=player, value=20)
            for color, amount in enumerate(self.mana[player]):
                self.emit('assert_mana', player=player, color=color, value=amount)
            for zone, by_player in self.zones.items():
                names = by_player[player]
                if zone == 'hand':
                    # The API reports raw slots for live pending spells. The
                    # separate in_hand=0 receipt asserts the logical move.
                    names = names + self.pending[player]
                self.emit('assert_zone', player=player, zone=zone, value=len(names))
                counts = Counter(names)
                for name in self.live_names:
                    self.emit('assert_zone_multiplicity', player=player, zone=zone,
                              index=self.rows[name]['csvid'], value=counts[name])
                if zone == 'library':
                    for index, name in enumerate(names):
                        self.emit('assert_zone_card', player=player, zone=zone, index=index, value=self.rows[name]['csvid'])

    def finish_spell(self, name, owner):
        self.pending[owner].remove(name)
        self.zones['graveyard'][owner].append(name)

    def resolve(self):
        valid = list(self.selected)
        if self.response == 'Reclaim':
            lost = valid.pop(0)
            self.card_choice(lost)
            self.pay_cast('response', self.response, self.response_owner, self.response_cost, response=True)
            self.zones['graveyard'][self.owner].remove(lost)
            self.zones['library'][self.owner].insert(0, lost)
        elif self.response == 'Counterspell':
            self.pay_cast('response', self.response, self.response_owner, self.response_cost, response=True)
            valid = []
        if self.mode == 'order_reverse':
            valid.reverse()
        if len(valid) > 1 and self.owner == 0:
            # Every named click moves that card to the front of the preview.
            # Reverse clicks establish the exact chosen top prefix, then Done.
            for name in dict.fromkeys(reversed(valid)):
                self.card_choice(name)
            self.card_choice(valid[0], 'choose_optional_zone')
        self.emit('resolve_response' if self.response else 'resolve')
        self.emit('assert_unused_choices', value=0)
        if self.response:
            self.finish_spell(self.response, self.response_owner)
        self.finish_spell(self.name, self.owner)
        for name in valid:
            self.zones['graveyard'][self.owner].remove(name)
        self.zones['library'][self.owner][0:0] = valid
        resolves = self.response != 'Counterspell' and (bool(valid) or not self.selected)
        if resolves and self.params['draw'] == 'immediate':
            self.draw()
        self.receipt()
        if self.params['draw'] == 'next_upkeep':
            self.next_upkeep()
            if resolves:
                self.draw()
            self.receipt()
            self.next_upkeep()
            self.receipt()

    def draw(self):
        drawn = self.zones['library'][self.owner].pop(0)
        self.zones['hand'][self.owner].append(drawn)

    def next_upkeep(self):
        self.active = 1 - self.active
        self.emit('advance_turn')
        self.emit('untap_step', player=self.active)
        self.emit('upkeep_step', player=self.active)

    def case(self):
        from recipes import step

        self.emit('observe_search_filters', value=1)
        if self.mode in ('insufficient_mana', 'wrong_color'):
            self.emit('cast', ref='source', value=0)
            self.receipt()
            replay_cut = 0
        else:
            self.plan_targets()
            self.pay_cast('source', self.name, self.owner, self.params['mana'])
            if self.mode == 'mixed_filter':
                alternative = TARGETS[self.params['scope']][1]
                for name, legal in ((alternative, True), ('Giant Growth', False), ('Forest', False)):
                    self.emit('assert_offered_legal_card', index=self.rows[name]['csvid'], value=int(legal))
            self.receipt(stack=1)
            replay_cut = len(self.steps) if self.mode == 'pending_replay' else 0
            self.resolve()
        first = self.steps[:replay_cut]
        replay = self.steps[replay_cut:]
        sequence = self.before + first + [step('capture')] + replay + [step('record_state'), step('restore')]
        sequence += replay + [step('assert_recorded_state')]
        return {
            'name': f'GraveyardLibraryTopRecipe_{self.csvid}_{self.variant}', 'card': self.name,
            'fixture': 'empty_main', 'seed': 123, 'setup': self.setup, 'steps': sequence,
            '_recipe': {
                'family': FAMILY, 'version': 2, 'variant': self.variant,
                'params': self.declaration['params'], 'oracle_text': self.declaration['oracle_text'],
                'untested_components': [
                    'Distinct AI ordering is a legal strategy choice. The AI multi-card case uses identical copies.',
                    'The largest selected set in this pilot is four cards.',
                    'Slowtrip uses real next-turn, untap and upkeep entry points. The normal turn draw is outside this fixture.',
                ],
            },
        }


def expand(name, csvid, declaration, params, selected, rows, oracle):
    validate_witnesses(oracle, rows)
    if not selected or len(set(selected)) != len(selected) or set(selected) - set(variants(params)):
        raise ValueError('unreviewed ordered recovery variants')
    return [RecoveryCase(name, csvid, declaration, params, variant, rows, oracle).case() for variant in selected]
