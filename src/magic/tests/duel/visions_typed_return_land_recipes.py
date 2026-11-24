"""Visions land play, owned return destinations, and paired mana production."""

import copy
import hashlib
import json
from pathlib import Path
import re

from block_stat_trigger_recipes import BlockStatBoard
from combat_value_trigger_recipes import _mirror
from grant_recipes import LIBRARY

FAMILY = 'visions_typed_return_land'
PROFILE_PATH = Path(__file__).with_name('data') / 'visions_typed_return_land_wave8_profiles.json'
ENTERS = 'This land enters tapped.'
MANA = dict(zip('CBUGRW', range(6)))
# Subtype, matching nonbasic land, spell that spends the complete two mana.
LANDS = {
    'U': ('Island', 'Underground Sea', 'Merfolk Looter'),
    'R': ('Mountain', 'Badlands', 'Mogg Flunkies'),
    'B': ('Swamp', 'Bayou', 'Child of Night'),
    'G': ('Forest', 'Savannah', 'River Boa'),
    'W': ('Plains', 'Tundra', 'Samite Healer'),
}
DIRECT = ('play_matching', 'play_nonbasic', 'play_decline', 'play_no_land', 'play_after_land',
          'play_sacrifice_uses_allowance', 'play_offturn_denied', 'play_replay')
MANA_MODES = ('produce_pair', 'repeat_denied', 'produce_offturn', 'spend_pair', 'produce_replay')
FAILURES = ('decline', 'no_matching', 'wrong_subtype', 'tapped_matching', 'foreign_controller', 'owned_wrong_control',
            'matching_in_hand', 'tap_during_atlas', 'indestructible_sacrifice')
ATLAS = ('matching', 'matching_nonbasic', 'matching_snow', 'foreign_owner', 'choose_snow', 'match_among_decoys',
         'offturn_atlas', 'entry_tapped_denied', 'atlas_pending_then_play_denied', 'atlas_leaves_play_available',
         'pending_replay', 'reenter') + FAILURES + MANA_MODES


def profiles():
    return json.loads(PROFILE_PATH.read_text(encoding='utf8'))


def parameters(record):
    lines = record.get('oracle_text', '').splitlines()
    if (record.get('layout') != 'normal' or record.get('card_faces') or record.get('selected_face')
            or record.get('type_line') != 'Land' or record.get('keywords') != [] or record.get('mana_cost') != ''
            or len(lines) != 3 or lines[0] != ENTERS):
        raise ValueError('complete normal Visions land clause required')
    effect = re.fullmatch(r"When this land enters, sacrifice it unless you return an untapped (Island|Mountain|Swamp|Forest|Plains) you control to its owner's hand\.", lines[1])
    output = re.fullmatch(r'\{T\}: Add \{C\}\{([BUGRW])\}\.', lines[2])
    if not effect or not output or effect[1] != LANDS[output[1]][0]:
        raise ValueError('exact subtype return and simultaneous two-mana clause required')
    return dict(subtype=effect[1], output=['C', output[1]], entry_tapped=True, return_to='owner', sacrifice_on_decline=True)


def variants(params):
    return [f'{mode}_{player}' for player in (0, 1) for mode in DIRECT + ATLAS]


def validate_variants(params, selected):
    if (not isinstance(selected, list) or not selected or any(type(v) is not str for v in selected)
            or len(set(selected)) != len(selected) or not set(selected) <= set(variants(params))):
        raise ValueError('distinct supported Visions land variants required')


def spec(declaration, record):
    params = parameters(record)
    pin = profiles()['sources'].get(record['name'])
    if (declaration.get('family') != FAMILY or type(declaration.get('version')) is not int or declaration['version'] != 2
            or not pin or pin['oracle'] != record or pin['params'] != params or declaration.get('oracle_text') != record['oracle_text']
            or json.dumps(declaration.get('params'), sort_keys=True) != json.dumps(params, sort_keys=True)):
        raise ValueError('complete pinned Oracle and typed Visions land parameters required')
    validate_variants(params, declaration.get('variants'))
    return dict(params, family=FAMILY)


def validate_witnesses(oracle, rows):
    data = profiles()
    root = Path(__file__).resolve().parents[4]
    unimplemented = json.loads((PROFILE_PATH.parent / 'unimplemented.json').read_text(encoding='utf8'))['cases']
    unimplemented_ids = {item['csvid'] for item in unimplemented.values()}
    static_only = set(data['native_characteristics_only_witnesses'])
    for name, pin in (data['witnesses'] | data['sources']).items():
        row = rows.get(name, {})
        if oracle.get(name) != pin['oracle'] or row != pin['registry'] or row.get('kind') != 'card':
            raise ValueError('Visions land source or witness changed: ' + name)
        if name not in static_only and (row['csvid'] in unimplemented_ids or 'dummy' in row['handler']):
            raise ValueError('Visions land action witness must be implemented: ' + name)
    for name, pin in data['sources'].items():
        source = pin['compiled_source']
        if hashlib.sha256((root / source['path']).read_bytes()).hexdigest() != source['sha256']:
            raise ValueError('Visions land compiled source changed: ' + name)


class VisionsLandBoard(BlockStatBoard):
    def __init__(self, name, csvid, declaration, params, variant, rows, oracle):
        from recipes import card, cost_from_oracle, observe, step

        self.card, self.step, self.observe, self.cost = card, step, observe, cost_from_oracle
        self.name, self.csvid, self.d, self.p, self.rows, self.oracle = name, csvid, declaration, params, rows, oracle
        self.mode, owner = variant.rsplit('_', 1)
        self.owner = int(owner)
        self.steps, self.setup, self.entities, self.base = [], [], {}, {}
        self.tokens, self.bound = [], set()
        self.boundary = self.discard_nondeterministic = False
        self.life = [20, 20]
        self.pools = [[0] * 6, [0] * 6]
        self.mana = [0, 0]
        self.zones = []
        self.cut = None
        for player in (0, 1):
            zones = dict(hand=['Grizzly Bears', 'Lightning Bolt'], graveyard=['Island'], library=list(LIBRARY), exile=['Mountain'])
            self.zones.append(zones)
            for zone, names in zones.items():
                self.setup.extend(card(stock, f'stock_{zone}_{player}_{index}', player, zone) for index, stock in enumerate(names))

        mode = self.mode
        self.subtype, dual, spender = LANDS[params['output'][1]]
        snow = 'Snow-Covered ' + self.subtype
        wrong = next(subtype for subtype, _, _ in LANDS.values() if subtype != self.subtype)
        self.add(name, 'source', 0, 'hand')
        if mode not in DIRECT:
            self.add('Walking Atlas', 'atlas')
        absent = ('no_matching', 'indestructible_sacrifice', 'play_no_land', 'play_sacrifice_uses_allowance')
        if mode not in absent:
            actual = {'matching_nonbasic': dual, 'play_nonbasic': dual, 'matching_snow': snow,
                      'wrong_subtype': wrong, 'foreign_owner': snow}.get(mode, self.subtype)
            controller = int(mode in ('foreign_controller', 'owned_wrong_control'))
            foreign_owner = mode in ('foreign_owner', 'owned_wrong_control')
            state = ['tapped'] if mode == 'tapped_matching' else []
            if foreign_owner:
                state.append('opponent_owned')
            self.add(actual, 'match', controller, 'hand' if mode in ('matching_in_hand', 'play_after_land') else 'battlefield', state)
            if foreign_owner:
                self.entities['match']['owner'] = 1 - controller
        if mode in ('choose_snow', 'reenter'):
            self.add(snow, 'second_match')
        if mode == 'match_among_decoys':
            self.add(snow, 'tapped_match', state=['tapped'])
            self.add(dual, 'foreign_match', 1)
            self.add(wrong, 'wrong_match')
        if mode == 'indestructible_sacrifice':
            self.add('Terra Eternal', 'protection')
        if mode == 'play_sacrifice_uses_allowance':
            self.add(wrong, 'next_land', 0, 'hand')
        if mode == 'spend_pair':
            self.add(spender, 'spender', 0, 'hand')
        if mode == 'reenter':
            self.add('Boomerang', 'bounce', 1, 'hand')
            self.pools[1] = self.cost(oracle['Boomerang'])
        self.emit('turn', player=int(mode in ('play_offturn_denied', 'offturn_atlas')), value=0)
        self.emit('present_ai_dialogs', value=1)
        for player in (0, 1):
            self.set_pool(player, self.pools[player])
        self.emit('refresh')

    def property(self, ref, name, value):
        self.emit('assert_card', ref=ref, property=name, value=value)

    def play(self, ref, allowed=True):
        self.emit('play_land', ref=ref, value=int(allowed))
        self.property(ref, 'in_hand', int(not allowed))
        self.emit('assert_stack', value=0)

    def actions_for_entry(self, returned, decline=False):
        if decline:
            self.emit('choose_cancel_target', player=0)
        elif returned:
            land = self.entities[returned]
            if (land['player'] != 0 or land['zone'] != 'battlefield' or land['tapped']
                    or self.subtype not in self.oracle[land['name']]['type_line'].partition(' — ')[2].split()):
                raise ValueError('return requires a controlled untapped land of the printed subtype')
            self.emit('choose_target', player=0, ref=returned)

    def settle_entry(self, returned):
        if returned:
            self.move(returned, 'hand')
            item = self.entities[returned]
            csvid = self.rows[item['name']]['csvid']
            self.emit('bind_hand', player=item['owner'], value=csvid, ref=returned)
            self.property(returned, 'in_hand', 1)
            self.move('source', 'battlefield')
            self.entities['source']['tapped'] = True
            self.property('source', 'tapped', 1)
        else:
            self.move('source', 'graveyard')
        self.property('source', 'in_play', int(returned is not None))

    def ordinary_entry(self, returned, decline=False):
        self.actions_for_entry(returned, decline)
        self.play('source')
        self.settle_entry(returned)

    def atlas_entry(self, returned, decline=False):
        self.emit('require_can_activate', ref='atlas', value=1)
        self.emit('activate', ref='atlas', value=1)
        self.entities['atlas']['tapped'] = True
        self.property('atlas', 'tapped', 1)
        self.property('source', 'in_hand', 1)
        self.emit('assert_stack', value=1)
        if self.mode == 'pending_replay':
            self.cut = len(self.steps)
        if self.mode == 'atlas_pending_then_play_denied':
            self.emit('play_land', ref='source', value=0)
            self.property('source', 'in_hand', 1)
            self.emit('assert_stack', value=1)
        if self.mode == 'tap_during_atlas':
            self.emit('activate', ref='match', value=1)
            self.entities['match']['tapped'] = True
            self.pools[0][MANA[self.p['output'][1]]] += 1
            self.mana[0] += 1
            self.property('match', 'tapped', 1)
            self.emit('assert_stack', value=1)
        self.emit('choose_zone_card', player=0, value=self.csvid)
        self.actions_for_entry(returned, decline)
        self.emit('resolve')
        self.emit('refresh')
        self.settle_entry(returned)

    def hand_land(self, ref):
        self.play(ref)
        self.move(ref, 'battlefield')
        self.property(ref, 'in_play', 1)
        self.property(ref, 'tapped', 0)

    def advance(self, player):
        if any(map(any, self.pools)):
            raise ValueError('bounded real untap requires empty pools')
        self.emit('advance_turn')
        self.emit('untap_step', player=player)
        self.emit('upkeep_step', player=player)
        self.emit('turn', player=player, value=0)

    def next_own_turn(self):
        self.advance(1)
        if self.entities['source']['zone'] == 'battlefield':
            self.property('source', 'tapped', int(self.entities['source']['tapped']))
        self.advance(0)
        for item in self.entities.values():
            if item['zone'] == 'battlefield' and item['player'] == 0:
                item['tapped'] = False

    def no_mana_activation(self):
        self.emit('require_can_activate', ref='source', value=0)
        self.emit('activate', ref='source', value=0)

    def produce(self):
        self.next_own_turn()
        self.property('source', 'tapped', 0)
        if self.mode == 'produce_offturn':
            self.advance(1)
        if self.mode == 'produce_replay':
            self.cut = len(self.steps)
        self.emit('require_can_activate', ref='source', value=1)
        self.emit('activate', ref='source', value=1)
        self.entities['source']['tapped'] = True
        for output in self.p['output']:
            self.pools[0][MANA[output]] += 1
            self.mana[0] += 1
        self.emit('assert_stack', value=0)
        self.property('source', 'tapped', 1)
        if self.mode == 'repeat_denied':
            self.no_mana_activation()
        if self.mode == 'spend_pair':
            self.receipts()
            self.paid('spender')

    def reenter(self):
        self.emit('choose_target', player=1, ref='source')
        self.emit('cast', ref='bounce', value=1)
        self.spend(self.oracle['Boomerang']['mana_cost'], player=1)
        self.emit('resolve')
        self.move('bounce', 'graveyard')
        self.move('source', 'hand')
        self.emit('bind_hand', player=0, value=self.csvid, ref='source')
        self.receipts()
        self.next_own_turn()
        self.atlas_entry('second_match')

    def run(self):
        mode = self.mode
        if mode == 'play_offturn_denied':
            self.play('source', allowed=False)
        elif mode == 'play_after_land':
            self.hand_land('match')
            self.play('source', allowed=False)
            self.next_own_turn()
            self.ordinary_entry('match')
        elif mode in DIRECT:
            if mode == 'play_replay':
                self.cut = len(self.steps)
            returned = 'match' if mode in ('play_matching', 'play_nonbasic', 'play_replay') else None
            self.ordinary_entry(returned, decline=mode == 'play_decline')
            if mode == 'play_sacrifice_uses_allowance':
                self.play('next_land', allowed=False)
        else:
            returned = None if mode in FAILURES else 'second_match' if mode == 'choose_snow' else 'match'
            self.atlas_entry(returned, decline=mode == 'decline')
            if mode == 'entry_tapped_denied':
                self.no_mana_activation()
            elif mode == 'atlas_leaves_play_available':
                self.hand_land('match')
            elif mode in MANA_MODES:
                self.produce()
            elif mode == 'reenter':
                self.reenter()
        self.receipts()
        if self.cut is not None:
            replay = self.steps[self.cut:]
            self.steps[self.cut:self.cut] = [self.step('capture')]
            self.steps += [self.step('record_state'), self.step('ai_roundtrip'), self.step('restore')]
            self.steps += copy.deepcopy(replay) + [self.step('assert_recorded_state')]
        variant = f'{mode}_{self.owner}'
        case = dict(name=f'VisionsTypedReturnLandRecipe_{self.csvid}_{variant}', card=self.name,
                    fixture='empty_main', seed=123, setup=self.setup, steps=self.steps,
                    _recipe=dict(family=FAMILY, version=2, variant=variant, params=copy.deepcopy(self.d['params']),
                                 oracle_text=self.d['oracle_text'], untested_components=[
                                     'External land-type and mana replacements are outside this cohort.',
                                 ]))
        if self.owner:
            _mirror(case)
        return case


def expand(name, csvid, declaration, params, selected, rows, oracle):
    validate_witnesses(oracle, rows)
    validate_variants(params, selected)
    return [VisionsLandBoard(name, csvid, declaration, params, variant, rows, oracle).run() for variant in selected]
