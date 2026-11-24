"""Borderpost alternate payment, returned-land ownership, and real mana entry."""

import copy
import hashlib
import json
from pathlib import Path
import re

from block_stat_trigger_recipes import BlockStatBoard
from combat_value_trigger_recipes import _mirror
from grant_recipes import LIBRARY

FAMILY = 'borderpost_alternate_cast'
PROFILE_PATH = Path(__file__).with_name('data') / 'borderpost_alternate_cast_wave8_profiles.json'
ALTERNATE = "You may pay {1} and return a basic land you control to its owner's hand rather than pay this spell's mana cost."
ENTRY = 'This artifact enters tapped.'
MANA = dict(zip('CBUGRW', range(6)))
SPENDERS = {'B': 'Typhoid Rats', 'U': 'Cursecatcher', 'G': 'Elvish Mystic', 'R': 'Raging Goblin', 'W': 'Alaborn Zealot'}
NORMAL = ('normal_empty', 'normal_with_basic', 'normal_wrong_colors', 'normal_no_generic', 'normal_countered')
REJECT = ('normal_wrong_colors', 'normal_no_generic', 'cancel_menu', 'foreign_controller_only', 'nonbasic_only',
          'no_payment', 'land_in_hand', 'opponent_basic_only')
MANA_MODES = ('mana_first', 'mana_second', 'mana_twice', 'mana_offturn', 'spend_first', 'spend_second')
MODES = NORMAL + ('alternate_only', 'alternate_menu', 'alternate_spent_land', 'alternate_snow', 'alternate_foreign_owner',
                  'alternate_mixed_types', 'alternate_color_float', 'cancel_menu', 'foreign_controller_only', 'nonbasic_only',
                  'no_payment', 'land_in_hand', 'opponent_basic_only', 'alternate_countered', 'entry_tapped_denied',
                  'prior_cast_snapshot', 'pending_cost_snapshot') + MANA_MODES


def profiles():
    return json.loads(PROFILE_PATH.read_text(encoding='utf8'))


def parameters(record):
    lines = record.get('oracle_text', '').splitlines()
    if (record.get('layout') != 'normal' or record.get('card_faces') or record.get('selected_face')
            or record.get('type_line') != 'Artifact' or record.get('keywords') != []
            or len(lines) != 3 or lines[:2] != [ALTERNATE, ENTRY]):
        raise ValueError('complete Borderpost alternate cost, tapped entry, and mana clauses required')
    mana = re.fullmatch(r'\{T\}: Add \{([BUGRW])\} or \{([BUGRW])\}\.', lines[2])
    printed = re.fullmatch(r'\{1\}\{([BUGRW])\}\{([BUGRW])\}', record.get('mana_cost', ''))
    if (not mana or not printed or mana.groups() != printed.groups() or mana[1] == mana[2]
            or set(record.get('colors', [])) != set(mana.groups())):
        raise ValueError('printed Borderpost colors and mana options must agree')
    return dict(printed_mana=record['mana_cost'], colors=list(mana.groups()), alternate_generic=1,
                return_type='basic land', return_to='owner', entry_tapped=True)


def variants(params):
    if len(params['colors']) != 2:
        raise ValueError('Borderpost requires two printed mana colors')
    return [f'{mode}_{owner}' for owner in (0, 1) for mode in MODES] + ['choose_second_basic_0']


def validate_variants(params, selected):
    if (not isinstance(selected, list) or not selected or any(type(v) is not str for v in selected)
            or len(set(selected)) != len(selected) or not set(selected) <= set(variants(params))):
        raise ValueError('distinct supported Borderpost variants required')


def spec(declaration, record):
    params = parameters(record)
    pin = profiles()['sources'].get(record['name'])
    if (declaration.get('family') != FAMILY or type(declaration.get('version')) is not int or declaration['version'] != 2
            or not pin or pin['oracle'] != record or pin['params'] != params or declaration.get('oracle_text') != record['oracle_text']
            or json.dumps(declaration.get('params'), sort_keys=True) != json.dumps(params, sort_keys=True)):
        raise ValueError('complete pinned Oracle and typed Borderpost parameters required')
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
            raise ValueError('Borderpost source or witness changed: ' + name)
        if name not in static_only and (row['csvid'] in unimplemented_ids or 'dummy' in row['handler']):
            raise ValueError('Borderpost action witness must be implemented: ' + name)
    for name, pin in data['sources'].items():
        source = pin['compiled_source']
        if hashlib.sha256((root / source['path']).read_bytes()).hexdigest() != source['sha256']:
            raise ValueError('Borderpost compiled source changed: ' + name)


class BorderpostBoard(BlockStatBoard):
    def __init__(self, name, csvid, declaration, params, variant, rows, oracle):
        from recipes import card, cost_from_oracle, observe, step

        self.card, self.step, self.observe, self.cost = card, step, observe, cost_from_oracle
        self.name, self.csvid, self.d, self.p, self.rows, self.oracle = name, csvid, declaration, params, rows, oracle
        mode, owner = variant.rsplit('_', 1)
        self.mode, self.owner = mode, int(owner)
        self.steps, self.setup, self.entities, self.base = [], [], {}, {}
        self.tokens, self.bound = [], set()
        self.boundary = self.discard_nondeterministic = False
        self.life = [20, 20]
        self.pools = [[0] * 6, [0] * 6]
        self.mana = [0, 0]
        self.cut = None
        self.return_ref = 'land'
        self.zones = []
        for player in (0, 1):
            zones = dict(hand=['Grizzly Bears', 'Lightning Bolt'], graveyard=['Island'], library=list(LIBRARY), exile=['Mountain'])
            self.zones.append(zones)
            for zone, names in zones.items():
                self.setup.extend(card(stock, f'stock_{zone}_{player}_{index}', player, zone) for index, stock in enumerate(names))

        self.add(name, 'source', 0, 'hand')
        if mode not in ('normal_empty', 'normal_wrong_colors', 'normal_no_generic', 'normal_countered'):
            land = {'alternate_snow': 'Snow-Covered Island', 'alternate_foreign_owner': 'Snow-Covered Island',
                    'nonbasic_only': 'Tundra'}.get(mode, 'Forest')
            controller = int(mode in ('opponent_basic_only', 'foreign_controller_only'))
            foreign_owner = mode in ('alternate_foreign_owner', 'foreign_controller_only')
            flags = [] if mode in ('alternate_spent_land', 'land_in_hand') else ['tapped']
            if foreign_owner:
                flags.append('opponent_owned')
            self.add(land, 'land', controller, 'hand' if mode == 'land_in_hand' else 'battlefield', flags)
            if foreign_owner:
                self.entities['land']['owner'] = 1 - controller
        if mode in ('choose_second_basic', 'alternate_mixed_types'):
            extra = 'Island' if mode == 'choose_second_basic' else 'Tundra'
            self.add(extra, 'second_land', state=['tapped'])
            if mode == 'choose_second_basic':
                self.return_ref = 'second_land'
        if mode in ('normal_countered', 'alternate_countered'):
            self.add('Counterspell', 'counter', 1, 'hand')
            self.pools[1] = self.cost(oracle['Counterspell'])
        if mode == 'mana_twice':
            self.add('Twiddle', 'untap', 1, 'hand')
        if mode in ('spend_first', 'spend_second'):
            self.add(SPENDERS[params['colors'][int(mode == 'spend_second')]], 'spender', 0, 'hand')

        self.paid_color = MANA['C']
        wallet = [1, 0, 0, 0, 0, 0]
        if mode in ('normal_empty', 'normal_with_basic', 'normal_countered', 'alternate_menu', 'cancel_menu', 'prior_cast_snapshot'):
            wallet = self.cost(oracle[name])
        elif mode == 'normal_wrong_colors':
            wallet[0] = 3
        elif mode == 'normal_no_generic':
            wallet = self.cost(oracle[name])
            wallet[0] = 0
        elif mode in ('no_payment', 'alternate_spent_land'):
            wallet[0] = 0
        elif mode == 'alternate_color_float':
            self.paid_color = next(MANA[color] for color in 'BUGRW' if color not in params['colors'])
            wallet[0] = 0
            wallet[self.paid_color] = 1
        self.emit('turn', player=0, value=0)
        self.emit('present_ai_dialogs', value=1)
        self.set_pool(0, wallet)
        self.set_pool(1, self.pools[1])
        self.emit('refresh')

    def mana_receipts(self):
        for player in (0, 1):
            self.emit('assert_mana_total', player=player, value=self.mana[player])
            for color, value in enumerate(self.pools[player]):
                self.emit('assert_mana', player=player, color=color, value=value)

    def add_mana(self, color, player=0):
        self.pools[player][color] += 1
        self.mana[player] += 1

    def return_land(self):
        ref = self.return_ref
        land = self.entities[ref]
        record = self.oracle[land['name']]
        if (land['zone'] != 'battlefield' or land['player'] != 0
                or not {'Basic', 'Land'} <= set(record['type_line'].split(' — ')[0].split())):
            raise ValueError('alternate payment requires a controlled basic land')
        self.move(ref, 'hand')
        owner = land['owner']
        csvid = self.rows[land['name']]['csvid']
        self.emit('assert_card', ref=ref, property='in_play', value=0)
        self.emit('bind_hand', player=owner, value=csvid, ref='returned_land')
        self.emit('assert_card', ref='returned_land', property='in_hand', value=1)
        for player in (0, 1):
            self.emit('assert_zone_multiplicity', player=player, zone='hand', index=csvid, value=int(player == owner))

    def cast_source(self):
        mode = self.mode
        allowed = mode not in REJECT
        alternate = allowed and mode not in NORMAL
        if mode == 'alternate_spent_land':
            self.emit('activate', ref='land', value=1)
            self.entities['land']['tapped'] = True
            self.paid_color = MANA['G']
            self.add_mana(self.paid_color)
            self.emit('assert_card', ref='land', property='tapped', value=1)
            self.mana_receipts()
        if mode in ('normal_with_basic', 'alternate_menu', 'cancel_menu', 'prior_cast_snapshot'):
            value = {'normal_with_basic': 0, 'cancel_menu': 2}.get(mode, 1)
            self.emit('choose_dialog', player=0, value=value)
        if alternate and self.owner == 0 and self.paid_color != MANA['C']:
            # The manual human payment path asks which floating colored mana
            # should apply to the generic artifact cost. AI applies it itself.
            self.emit('choose_mana', player=0, color=self.paid_color)
        # The human cost UI selects a controlled basic; the legacy AI chooses
        # its sole eligible basic itself. The choice pays a cost on either path.
        if alternate and self.owner == 0:
            self.emit('choose_target', player=0, ref=self.return_ref)
        self.emit('cast', ref='source', value=int(allowed))
        self.emit('assert_stack', value=int(allowed))
        self.emit('assert_card', ref='source', property='in_hand', value=int(not allowed))
        self.emit('assert_card', ref='source', property='in_play', value=0)
        if alternate:
            self.pools[0][self.paid_color] -= self.p['alternate_generic']
            self.mana[0] -= self.p['alternate_generic']
            if self.pools[0][self.paid_color] < 0:
                raise ValueError('alternate generic payment was not funded')
            self.return_land()
        elif allowed:
            self.spend(self.p['printed_mana'])
        self.mana_receipts()
        return allowed

    def resolve_source(self):
        self.emit('resolve')
        self.emit('refresh')
        self.move('source', 'battlefield')
        self.entities['source']['tapped'] = self.p['entry_tapped']
        self.emit('assert_card', ref='source', property='in_play', value=1)
        self.emit('assert_card', ref='source', property='tapped', value=1)

    def counter_source(self):
        self.emit('announce_spell', ref='source')
        self.emit('cast_response', ref='counter', value=1)
        self.spend(self.oracle['Counterspell']['mana_cost'], player=1)
        self.emit('assert_stack', value=2)
        self.emit('resolve_response')
        self.move('counter', 'graveyard')
        self.move('source', 'graveyard')

    def advance(self, player):
        if any(map(any, self.pools)):
            raise ValueError('bounded real untap requires empty mana pools')
        self.emit('advance_turn')
        self.emit('untap_step', player=player)
        self.emit('upkeep_step', player=player)
        self.emit('turn', player=player, value=0)

    def activate_source(self, output):
        self.emit('require_can_activate', ref='source', value=1)
        self.emit('choose_color', player=0, value=MANA[output])
        self.emit('activate', ref='source', value=1)
        self.entities['source']['tapped'] = True
        self.add_mana(MANA[output])
        self.emit('assert_stack', value=0)
        self.emit('assert_card', ref='source', property='tapped', value=1)
        self.mana_receipts()

    def produce(self):
        self.advance(1)
        self.emit('assert_card', ref='source', property='tapped', value=1)
        self.advance(0)
        self.entities['source']['tapped'] = False
        self.emit('assert_card', ref='source', property='tapped', value=0)
        if self.mode == 'mana_offturn':
            self.advance(1)
        first, second = self.p['colors']
        self.activate_source(second if self.mode in ('mana_second', 'spend_second') else first)
        if self.mode == 'mana_twice':
            self.set_pool(1, self.cost(self.oracle['Twiddle']))
            self.twiddle()
            self.activate_source(second)
        if self.mode.startswith('spend_'):
            self.paid('spender')

    def twiddle(self):
        self.emit('choose_target', player=1, ref='source')
        self.emit('cast', ref='untap', value=1)
        self.spend(self.oracle['Twiddle']['mana_cost'], player=1)
        self.emit('choose_dialog', player=1, value=1)
        self.emit('resolve')
        self.emit('refresh')
        self.move('untap', 'graveyard')
        self.entities['source']['tapped'] = False
        self.emit('assert_card', ref='source', property='tapped', value=0)

    def run(self):
        if self.mode == 'prior_cast_snapshot':
            self.cut = len(self.steps)
        if self.cast_source():
            if self.mode == 'pending_cost_snapshot':
                self.cut = len(self.steps)
            if self.mode in ('normal_countered', 'alternate_countered'):
                self.counter_source()
            else:
                self.resolve_source()
                if self.mode in MANA_MODES:
                    self.produce()
                if self.mode == 'entry_tapped_denied':
                    self.emit('require_can_activate', ref='source', value=0)
                    self.emit('activate', ref='source', value=0)
        self.receipts()
        if self.cut is not None:
            replay = self.steps[self.cut:]
            self.steps[self.cut:self.cut] = [self.step('capture')]
            self.steps += [self.step('record_state'), self.step('ai_roundtrip'), self.step('restore')]
            self.steps += copy.deepcopy(replay) + [self.step('assert_recorded_state')]
        variant = f'{self.mode}_{self.owner}'
        case = dict(name=f'BorderpostAlternateCastRecipe_{self.csvid}_{variant}', card=self.name,
                    fixture='empty_main', seed=123, setup=self.setup, steps=self.steps,
                    _recipe=dict(family=FAMILY, version=2, variant=variant, params=copy.deepcopy(self.d['params']),
                                 oracle_text=self.d['oracle_text'], untested_components=[
                                     'AI costs use one eligible basic. Selecting among different eligible basics is exercised by the human selector.',
                                     'Fixtures with foreign ownership use the supported owner bit to model already-controlled basic lands.',
                                 ]))
        if self.owner:
            _mirror(case)
        return case


def expand(name, csvid, declaration, params, selected, rows, oracle):
    validate_witnesses(oracle, rows)
    validate_variants(params, selected)
    return [BorderpostBoard(name, csvid, declaration, params, variant, rows, oracle).run() for variant in selected]
