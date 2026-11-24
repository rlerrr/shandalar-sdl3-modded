"""Tempest Medallion reductions observed through real paid casts."""

import copy
import hashlib
import json
from pathlib import Path
import re

from block_stat_trigger_recipes import BlockStatBoard
from combat_value_trigger_recipes import _mirror
from grant_recipes import LIBRARY

FAMILY = 'medallion_cast_cost_reduction'
PROFILE_PATH = Path(__file__).with_name('data') / 'medallion_cast_cost_reduction_wave8_profiles.json'
# Each multicolor witness has at least two generic mana to spend. A single
# reduction, a double reduction, and a zero-generic floor are distinguishable.
COLORS = {
    'G': ('green', 'Emerald Medallion', 'Fangren Marauder', 'Ghor-Clan Rampager', 'Elvish Mystic', "Moment's Peace", 'R'),
    'B': ('black', 'Jet Medallion', 'Sengir Vampire', 'Moroii', 'Typhoid Rats', 'Cabal Ritual', 'U'),
    'W': ('white', 'Pearl Medallion', 'Alaborn Cavalier', 'Archon of the Triumvirate', 'Alaborn Zealot', "Whitesun's Passage", 'U'),
    'R': ('red', 'Ruby Medallion', 'Furnace Whelp', 'Ghor-Clan Rampager', 'Raging Goblin', 'Seething Song', 'G'),
    'U': ('blue', 'Sapphire Medallion', 'Scroll Thief', 'Moroii', 'Cursecatcher', 'Divination', 'B'),
}
MODES = ('cast_source', 'unfunded_source', 'source_colorless', 'matching_exact', 'matching_surplus', 'matching_short',
         'multicolored_one', 'multicolored_both', 'wrong_color_short', 'wrong_color_full', 'after_source_cast',
         'opponent_short', 'opponent_full', 'opposing_sources', 'source_in_hand', 'source_in_hand_short', 'source_tapped', 'two_sources',
         'generic_floor', 'no_printed_generic', 'missing_colored_mana', 'colorless_spell', 'noncreature_spell',
         'remove_source', 'two_casts', 'countered_cast', 'pending_snapshot')


def profiles():
    return json.loads(PROFILE_PATH.read_text(encoding='utf8'))


def parameters(record):
    match = re.fullmatch(r'(Black|Blue|Green|Red|White) spells you cast cost \{1\} less to cast\.', record.get('oracle_text', ''))
    if (not match or record.get('layout') != 'normal' or record.get('selected_face') or record.get('card_faces')
            or record.get('keywords') != [] or record.get('type_line') != 'Artifact' or record.get('mana_cost') != '{2}'):
        raise ValueError('one complete reviewed Tempest Medallion clause required')
    color, definition = next((color, definition) for color, definition in COLORS.items() if definition[0] == match[1].lower())
    if definition[1] != record['name']:
        raise ValueError('Tempest Medallion color and card name disagree')
    return dict(color=color, reduction=1, printed_mana='{2}')


def variants(params):
    if params['color'] not in COLORS:
        raise ValueError('unsupported Tempest Medallion color')
    return [f'{mode}_{owner}' for owner in (0, 1) for mode in MODES]


def validate_variants(params, selected):
    if (not isinstance(selected, list) or not selected or any(type(v) is not str for v in selected)
            or len(set(selected)) != len(selected) or not set(selected) <= set(variants(params))):
        raise ValueError('distinct supported Tempest Medallion variants required')


def spec(declaration, record):
    params = parameters(record)
    pin = profiles()['sources'].get(record['name'])
    if (declaration.get('family') != FAMILY or type(declaration.get('version')) is not int or declaration['version'] != 2
            or not pin or pin['oracle'] != record or pin['params'] != params or declaration.get('oracle_text') != record['oracle_text']
            or json.dumps(declaration.get('params'), sort_keys=True) != json.dumps(params, sort_keys=True)):
        raise ValueError('complete pinned Oracle and typed Tempest Medallion parameters required')
    validate_variants(params, declaration.get('variants'))
    return dict(params, family=FAMILY)


def validate_witnesses(oracle, rows):
    from recipes import cost_from_oracle

    data = profiles()
    root = Path(__file__).resolve().parents[4]
    unimplemented = json.loads((PROFILE_PATH.parent / 'unimplemented.json').read_text(encoding='utf8'))['cases']
    unimplemented_ids = {entry['csvid'] for entry in unimplemented.values()}
    static_only = set(data['native_characteristics_only_witnesses'])
    for name, pin in (data['witnesses'] | data['sources']).items():
        row = rows.get(name, {})
        if oracle.get(name) != pin['oracle'] or row != pin['registry'] or row.get('kind') != 'card':
            raise ValueError('Tempest Medallion source or witness changed: ' + name)
        if name not in static_only and (row['csvid'] in unimplemented_ids or 'dummy' in row['handler']):
            raise ValueError('Tempest Medallion action witness must be implemented: ' + name)
    for name, pin in data['sources'].items():
        source = pin['compiled_source']
        if hashlib.sha256((root / source['path']).read_bytes()).hexdigest() != source['sha256']:
            raise ValueError('Tempest Medallion compiled source changed: ' + name)
    for color, (_, _, mono, multi, floor, noncreature, allied) in COLORS.items():
        if (oracle[mono]['colors'] != [color] or cost_from_oracle(oracle[mono])[0] < 2
                or set(oracle[multi]['colors']) != {color, allied} or cost_from_oracle(oracle[multi])[0] < 2
                or oracle[floor]['colors'] != [color] or cost_from_oracle(oracle[floor])[0] != 0
                or oracle[noncreature]['colors'] != [color] or cost_from_oracle(oracle[noncreature])[0] < 1):
            raise ValueError('Tempest Medallion witnesses must distinguish actual payment boundaries')


class MedallionBoard(BlockStatBoard):
    def __init__(self, name, csvid, declaration, params, variant, rows, oracle):
        from recipes import card, cost_from_oracle, observe, step

        self.card, self.step, self.observe, self.cost = card, step, observe, cost_from_oracle
        self.name, self.csvid, self.d, self.p, self.rows, self.oracle = name, csvid, declaration, params, rows, oracle
        mode, owner = variant.rsplit('_', 1)
        self.mode, self.owner = mode, int(owner)
        self.actor = int(mode in ('opponent_short', 'opponent_full'))
        self.steps, self.setup, self.entities, self.base = [], [], {}, {}
        self.tokens, self.bound = [], set()
        self.boundary = self.discard_nondeterministic = False
        self.life = [20, 20]
        self.pools = [[0] * 6, [0] * 6]
        self.mana = [0, 0]
        self.cut = None
        self.zones = []
        for player in (0, 1):
            zones = dict(hand=['Forest', 'Plains'], graveyard=['Island'], library=list(LIBRARY), exile=['Mountain'])
            self.zones.append(zones)
            for zone, names in zones.items():
                self.setup.extend(card(stock, f'stock_{zone}_{player}_{index}', player, zone) for index, stock in enumerate(names))

        _, _, mono, multi, floor, noncreature, allied = COLORS[params['color']]
        wrong = COLORS[allied][2]
        source_spell = mode in ('cast_source', 'unfunded_source', 'source_colorless')
        in_hand = source_spell or mode in ('source_in_hand', 'source_in_hand_short', 'after_source_cast')
        self.add(name, 'source', 0, 'hand' if in_hand else 'battlefield', ['tapped'] if mode == 'source_tapped' else None)
        self.medallions = {'source': params['color']}
        if mode in ('two_sources', 'multicolored_both', 'source_colorless', 'opposing_sources', 'generic_floor', 'no_printed_generic'):
            number = self.cost(oracle[mono])[0] if mode == 'generic_floor' else 2 if mode == 'no_printed_generic' else 1
            color = allied if mode == 'multicolored_both' else params['color']
            for index in range(number):
                ref = f'medallion_{index}'
                self.add(COLORS[color][1], ref, int(mode == 'opposing_sources'))
                self.medallions[ref] = color
        self.spell_ref = 'source' if source_spell else 'spell'
        if not source_spell:
            spell = {'multicolored_one': multi, 'multicolored_both': multi,
                     'wrong_color_short': wrong, 'wrong_color_full': wrong,
                     'no_printed_generic': floor, 'colorless_spell': 'Sol Ring', 'noncreature_spell': noncreature}.get(mode, mono)
            self.add(spell, self.spell_ref, self.actor, 'hand')
        if mode == 'two_casts':
            self.add(mono, 'second_spell', 0, 'hand')
        if mode == 'remove_source':
            self.add('Shatter', 'remove', 1, 'hand')
        if mode == 'countered_cast':
            self.add('Counterspell', 'counter', 1, 'hand')

        paid = self.cast_vector(self.spell_ref)
        wallet = paid.copy()
        self.allowed = mode not in ('unfunded_source', 'matching_short', 'wrong_color_short', 'opponent_short',
                                    'source_in_hand_short', 'missing_colored_mana')
        if mode in ('unfunded_source', 'matching_short', 'wrong_color_short', 'opponent_short', 'source_in_hand_short'):
            wallet[0] -= 1
        elif mode == 'matching_surplus':
            wallet[0] += 2
        elif mode == 'missing_colored_mana':
            wallet = [sum(paid) + 5, 0, 0, 0, 0, 0]
        elif mode == 'two_casts':
            wallet = [2 * amount for amount in paid]
        elif mode == 'after_source_cast':
            wallet[0] += self.cost(oracle[name])[0] - params['reduction']
        elif mode == 'remove_source':
            # The full cost is already available when an opposing Shatter
            # resolves. No fixture mana change hides the discount loss.
            wallet = self.cost(oracle[mono])
        self.pools[self.actor] = wallet
        if mode == 'remove_source':
            self.pools[1] = self.cost(oracle['Shatter'])
        elif mode == 'countered_cast':
            self.pools[1] = self.cost(oracle['Counterspell'])
        self.emit('turn', player=self.actor, value=0)
        self.emit('present_ai_dialogs', value=1)
        for player in (0, 1):
            self.set_pool(player, self.pools[player])
        self.emit('refresh')

    def cast_vector(self, ref):
        caster = self.entities[ref]['player']
        record = self.oracle[self.entities[ref]['name']]
        cost = self.cost(record)
        matching = sum(color in record['colors'] and self.entities[medallion]['zone'] == 'battlefield'
                       and self.entities[medallion]['player'] == caster for medallion, color in self.medallions.items())
        cost[0] = max(0, cost[0] - self.p['reduction'] * matching)
        return cost

    def pending(self, ref, *, allowed=True, target=None, response=False):
        player = self.entities[ref]['player']
        if target is not None:
            self.emit('choose_target', player=player, ref=target)
        self.emit('cast_response' if response else 'cast', ref=ref, value=int(allowed))
        if allowed:
            self.pools[player] = [have - cost for have, cost in zip(self.pools[player], self.cast_vector(ref))]
            if min(self.pools[player]) < 0:
                raise ValueError('fixture cannot pay the independently derived cast cost')
            self.mana[player] = sum(self.pools[player])
        self.emit('assert_stack', value=2 if response else int(allowed))
        self.emit('assert_card', ref=ref, property='in_hand', value=int(not allowed))
        self.emit('assert_mana_total', player=player, value=self.mana[player])
        for color, value in enumerate(self.pools[player]):
            self.emit('assert_mana', player=player, color=color, value=value)

    def resolve_permanent(self, ref):
        self.emit('resolve')
        self.move(ref, 'battlefield')

    def resolve_noncreature(self, ref):
        self.emit('resolve')
        self.move(ref, 'graveyard')
        name = self.entities[ref]['name']
        if name == 'Cabal Ritual':
            self.pools[0][1] += 3
            self.mana[0] += 3
        elif name == 'Seething Song':
            self.pools[0][4] += 5
            self.mana[0] += 5
        elif name == 'Divination':
            self.draw(0, 2)
        elif name == "Whitesun's Passage":
            self.life[0] += 5

    def run(self):
        ref = self.spell_ref
        if self.mode == 'after_source_cast':
            self.pending('source')
            self.resolve_permanent('source')
            self.receipts()
        if self.mode == 'remove_source':
            self.pending('remove', target='source')
            self.emit('resolve')
            self.move('remove', 'graveyard')
            self.move('source', 'graveyard')
            self.receipts()
        self.pending(ref, allowed=self.allowed)
        if self.mode == 'pending_snapshot':
            self.cut = len(self.steps)
        if self.mode == 'countered_cast':
            self.emit('announce_spell', ref=ref)
            self.pending('counter', response=True)
            self.emit('resolve_response')
            self.move(ref, 'graveyard')
            self.move('counter', 'graveyard')
        elif self.mode == 'noncreature_spell':
            self.resolve_noncreature(ref)
        elif self.allowed:
            self.resolve_permanent(ref)
        if self.mode == 'two_casts':
            self.receipts()
            self.pending('second_spell')
            self.resolve_permanent('second_spell')
        self.receipts()
        if self.cut is not None:
            replay = self.steps[self.cut:]
            self.steps[self.cut:self.cut] = [self.step('capture')]
            self.steps += [self.step('record_state'), self.step('ai_roundtrip'), self.step('restore')]
            self.steps += copy.deepcopy(replay) + [self.step('assert_recorded_state')]
        mode = f'{self.mode}_{self.owner}'
        case = dict(name=f'MedallionCastCostReductionRecipe_{self.csvid}_{mode}', card=self.name,
                    fixture='empty_main', seed=123, setup=self.setup, steps=self.steps,
                    _recipe=dict(family=FAMILY, version=2, variant=mode, oracle_text=self.d['oracle_text'],
                                 params=copy.deepcopy(self.d['params']), untested_components=[
                                     'Payment fixtures use fixed printed mana costs. Alternate payments and X costs are outside this cohort.',
                                 ]))
        if self.owner:
            _mirror(case)
        return case


def expand(name, csvid, declaration, params, selected, rows, oracle):
    validate_witnesses(oracle, rows)
    validate_variants(params, selected)
    return [MedallionBoard(name, csvid, declaration, params, variant, rows, oracle).run() for variant in selected]
