"""The five Magus Staves' real cast and controlled-land entry triggers."""

import copy
import hashlib
import json
from pathlib import Path
import re

from block_stat_trigger_recipes import BlockStatBoard
from combat_value_trigger_recipes import _mirror
from grant_recipes import LIBRARY

FAMILY = 'staff_magus_cast_land_life'
PROFILE_PATH = Path(__file__).with_name('data') / 'staff_magus_cast_land_life_wave8_profiles.json'
CASTS = {
    'black': ('B', 'Swamp', 'Badlands', 'Dark Ritual', 'Putrid Leech'),
    'red': ('R', 'Mountain', 'Volcanic Island', 'Seething Song', 'Skyknight Legionnaire'),
    'blue': ('U', 'Island', 'Underground Sea', 'Reach Through Mists', 'Tidehollow Strix'),
    'white': ('W', 'Plains', 'Tundra', "Whitesun's Passage", 'Qasali Pridemage'),
    'green': ('G', 'Forest', 'Savannah', 'Fog', 'Ghor-Clan Rampager'),
}
COMMON = ('cast_source', 'unfunded_source', 'countered_source', 'match_spell', 'opponent_spell', 'wrong_spell',
          'colorless_spell', 'multicolored_spell', 'countered_matching_spell', 'two_matching_spells',
          'announce_replay', 'announced_replay', 'unfunded_spell', 'land_own', 'land_enemy', 'land_wrong_subtype',
          'land_dual', 'land_pending_replay', 'two_events', 'staff_enters_after_match', 'source_removed',
          'two_sources', 'opposing_staff', 'tapped_staff', 'search_basic_land', 'setup_land')


def profiles():
    return json.loads(PROFILE_PATH.read_text(encoding='utf8'))


def parameters(record):
    match = re.fullmatch(r'Whenever you cast a (black|red|blue|white|green) spell or an? ([A-Z][a-z]+) you control enters, you gain 1 life\.',
                         record.get('oracle_text', ''))
    if (not match or record.get('layout') != 'normal' or record.get('selected_face') or record.get('card_faces')
            or record.get('keywords') != [] or record.get('type_line') != 'Artifact' or record.get('mana_cost') != '{3}'):
        raise ValueError('one complete reviewed Magus Staff clause required')
    color_name, land = match.groups()
    color, expected_land, _, _, _ = CASTS[color_name]
    if land != expected_land:
        raise ValueError('the reviewed Staff clause pairs color and land subtype')
    return dict(color_name=color_name, color=color, subtype=land, printed_mana='{3}', gain=1)


def variants(params):
    modes = list(COMMON)
    if params['color'] == 'U':
        modes.append('copy_does_not_trigger')
    if params['color'] == 'G':
        modes.append('colored_land_creature')
    return [f'{mode}_{owner}' for owner in (0, 1) for mode in modes]


def validate_variants(params, selected):
    if (not isinstance(selected, list) or not selected or any(type(v) is not str for v in selected)
            or len(set(selected)) != len(selected) or not set(selected) <= set(variants(params))):
        raise ValueError('distinct supported Magus Staff variants required')


def spec(declaration, record):
    params = parameters(record)
    pin = profiles()['sources'].get(record['name'])
    if (declaration.get('family') != FAMILY or type(declaration.get('version')) is not int or declaration['version'] != 2
            or not pin or pin['oracle'] != record or pin['params'] != params or declaration.get('oracle_text') != record['oracle_text']
            or json.dumps(declaration.get('params'), sort_keys=True) != json.dumps(params, sort_keys=True)):
        raise ValueError('complete pinned Oracle and typed Magus Staff parameters required')
    validate_variants(params, declaration.get('variants'))
    return dict(params, family=FAMILY)


def validate_witnesses(oracle, rows):
    data = profiles()
    root = Path(__file__).resolve().parents[4]
    catalog = json.loads((PROFILE_PATH.parent / 'unimplemented.json').read_text(encoding='utf8'))
    unimplemented = {row['csvid'] for row in catalog['cases'].values()}
    for name, pin in data['witnesses'].items():
        row = rows.get(name, {})
        if oracle.get(name) != pin['oracle'] or row != pin['registry'] or row.get('kind') != 'card':
            raise ValueError('Magus Staff witness changed: ' + name)
        if name not in data['native_characteristics_only_witnesses'] and (row['csvid'] in unimplemented or 'dummy' in row['handler']):
            raise ValueError('Magus Staff action witness must be implemented: ' + name)
    for name, pin in data['sources'].items():
        row = rows.get(name, {})
        source = pin['compiled_source']
        if (oracle.get(name) != pin['oracle'] or row != pin['registry'] or row.get('kind') != 'card' or row.get('csvid') in unimplemented
                or 'dummy' in row['handler'] or hashlib.sha256((root / source['path']).read_bytes()).hexdigest() != source['sha256']):
            raise ValueError('implemented Magus Staff source changed: ' + name)


class StaffBoard(BlockStatBoard):
    def __init__(self, name, csvid, declaration, params, variant, rows, oracle):
        from recipes import card, cost_from_oracle, observe, step

        self.card, self.step, self.observe, self.cost = card, step, observe, cost_from_oracle
        self.name, self.csvid, self.d, self.p, self.rows, self.oracle = name, csvid, declaration, params, rows, oracle
        mode, owner = variant.rsplit('_', 1)
        self.mode, self.owner = mode, int(owner)
        self.actor = int(mode in ('opponent_spell', 'land_enemy'))
        self.steps, self.setup, self.entities, self.base = [], [], {}, {}
        self.tokens, self.bound = [], set()
        self.aura_target = None
        self.boundary = self.discard_nondeterministic = False
        self.life = [20, 20]
        self.pools = [[10] * 6, [10] * 6]
        self.mana = [60, 60]
        self.cut = None
        self.staff_refs = ['source']
        self.zones = []
        for player in (0, 1):
            library = [params['subtype']] if mode == 'search_basic_land' and player == 0 else list(LIBRARY)
            zones = dict(hand=['Grizzly Bears', 'Lightning Bolt'], graveyard=['Island'], library=library, exile=['Mountain'])
            self.zones.append(zones)
            for zone, names in zones.items():
                for index, stock in enumerate(names):
                    ref = 'searched_land' if zone == 'library' and mode == 'search_basic_land' and player == 0 else f'stock_{zone}_{player}_{index}'
                    if ref == 'searched_land':
                        self.add(stock, ref, player, zone)
                    else:
                        self.setup.append(card(stock, ref, player, zone))

        _, land, dual, spell, multicolored = CASTS[params['color_name']]
        wrong = next(v for v in CASTS.values() if v[0] != params['color'])
        casting_source = mode in ('cast_source', 'unfunded_source', 'countered_source', 'staff_enters_after_match')
        if mode == 'setup_land':
            # Fixture battlefield placement runs real entry. Have the land
            # arrive first so a later Staff cannot receive a backdated trigger.
            self.add(land, 'land')
        self.add(name, 'source', 0, 'hand' if casting_source else 'battlefield', ['tapped'] if mode == 'tapped_staff' else None)
        if mode in ('two_sources', 'opposing_staff'):
            self.add(name, 'second_staff', int(mode == 'opposing_staff'))
            self.staff_refs.append('second_staff')
        if mode.startswith('land_') or mode in ('two_events', 'colored_land_creature'):
            entering = {'land_wrong_subtype': wrong[1], 'land_dual': dual, 'colored_land_creature': 'Dryad Arbor'}.get(mode, land)
            self.add('Walking Atlas', 'atlas', self.actor)
            self.add(entering, 'land', self.actor, 'hand')
        if mode == 'search_basic_land':
            self.add('Rampant Growth', 'spell', 0, 'hand')
        elif mode not in ('cast_source', 'unfunded_source', 'countered_source', 'setup_land', 'colored_land_creature') and not mode.startswith('land_'):
            card_name = {'wrong_spell': wrong[3], 'multicolored_spell': multicolored, 'colorless_spell': 'Sol Ring'}.get(mode, spell)
            self.add(card_name, 'spell', self.actor, 'hand')
        if mode == 'two_matching_spells':
            self.add(spell, 'second_spell', 0, 'hand')
        if mode in ('countered_source', 'countered_matching_spell'):
            self.add('Counterspell', 'counter', 1, 'hand')
        if mode == 'copy_does_not_trigger':
            self.add('Twincast', 'copy_spell', 0, 'hand')
        if mode == 'source_removed':
            self.add('Shatter', 'remove_staff', 0, 'hand')

        self.emit('turn', player=self.actor, value=0)
        self.emit('present_ai_dialogs', value=1)
        if mode == 'cast_source':
            self.pools[0] = self.cost(oracle[name])
        elif mode in ('unfunded_source', 'unfunded_spell'):
            self.pools[0] = [0] * 6
        for player in (0, 1):
            self.set_pool(player, self.pools[player])
        self.emit('refresh')

    def qualifying_staves(self, player):
        return sum(self.entities[ref]['zone'] == 'battlefield' and self.entities[ref]['player'] == player for ref in self.staff_refs)

    def life_receipts(self):
        for player, value in enumerate(self.life):
            self.emit('assert_life', player=player, value=value)

    def pending_cast(self, ref, *, response=False, allowed=True, target=None):
        entity = self.entities[ref]
        player = entity['player']
        if target is not None:
            self.emit('choose_target', player=player, ref=target)
        self.emit('cast_response' if response else 'cast', ref=ref, value=int(allowed))
        if allowed:
            self.spend(self.oracle[entity['name']]['mana_cost'], player)
        self.emit('assert_stack', value=2 if response else int(allowed))
        self.emit('assert_card', ref=ref, property='in_hand', value=int(not allowed))
        self.emit('assert_mana_total', player=player, value=self.mana[player])
        self.life_receipts()

    def announce(self, ref, depth=1):
        entity = self.entities[ref]
        self.emit('announce_spell', ref=ref)
        if self.p['color'] in self.oracle[entity['name']]['colors']:
            self.life[entity['player']] += self.qualifying_staves(entity['player']) * self.p['gain']
        self.emit('assert_stack', value=depth)
        self.life_receipts()

    def notify_land(self, ref):
        entity = self.entities[ref]
        subtypes = self.oracle[entity['name']]['type_line'].partition(' — ')[2].split()
        if self.p['subtype'] in subtypes:
            self.life[entity['player']] += self.qualifying_staves(entity['player']) * self.p['gain']

    def witness_result(self, ref, copies=1):
        entity = self.entities[ref]
        name, player = entity['name'], entity['player']
        text = self.oracle[name]['type_line']
        if 'Instant' not in text and 'Sorcery' not in text:
            self.move(ref, 'battlefield')
            return
        if name in ('Dark Ritual', 'Seething Song'):
            color, amount = (1, 3) if name == 'Dark Ritual' else (4, 5)
            self.pools[player][color] += amount * copies
            self.mana[player] += amount * copies
        elif name == 'Reach Through Mists':
            self.draw(player, copies)
        elif name == "Whitesun's Passage":
            self.life[player] += 5 * copies
        self.move(ref, 'graveyard')

    def enter_land(self):
        player = self.actor
        self.emit('can_activate', ref='atlas', value=1)
        self.emit('activate', ref='atlas', value=1)
        self.entities['atlas']['tapped'] = True
        self.emit('assert_card', ref='atlas', property='tapped', value=1)
        self.emit('assert_card', ref='land', property='in_hand', value=1)
        self.emit('assert_stack', value=1)
        self.life_receipts()
        if self.mode == 'land_pending_replay':
            self.cut = len(self.steps)
        self.emit('choose_zone_card', player=player, value=self.rows[self.entities['land']['name']]['csvid'])
        self.emit('resolve')
        self.emit('refresh')
        self.move('land', 'battlefield')
        self.notify_land('land')
        self.emit('assert_card', ref='land', property='in_hand', value=0)

    def search_land(self):
        self.pending_cast('spell')
        self.announce('spell')
        self.emit('choose_zone_card', player=-1, value=self.rows[self.p['subtype']]['csvid'])
        self.emit('resolve')
        self.emit('refresh')
        self.move('spell', 'graveyard')
        self.move('searched_land', 'battlefield')
        self.entities['searched_land']['tapped'] = True
        self.notify_land('searched_land')
        self.emit('bind_permanent', ref='searched_land', player=0, value=self.rows[self.p['subtype']]['csvid'])

    def countered(self, ref):
        self.pending_cast(ref)
        self.announce(ref)
        self.pending_cast('counter', response=True)
        self.emit('resolve_response')
        self.move(ref, 'graveyard')
        self.move('counter', 'graveyard')

    def copied(self):
        self.pending_cast('spell')
        self.announce('spell')
        self.pending_cast('copy_spell', response=True)
        self.announce('copy_spell', depth=2)
        self.emit('resolve_response')
        self.move('copy_spell', 'graveyard')
        # Resolving the copy and original draws two cards in total. Only the
        # two real casts above earn life, and only physical cards go to grave.
        self.witness_result('spell', copies=2)

    def ordinary_spell(self, ref):
        self.pending_cast(ref)
        if self.mode == 'announce_replay':
            self.cut = len(self.steps)
        self.announce(ref)
        if self.mode == 'announced_replay':
            self.cut = len(self.steps)
        self.emit('resolve')
        self.witness_result(ref)

    def run(self):
        mode = self.mode
        self.life_receipts()
        if mode in ('unfunded_source', 'unfunded_spell'):
            self.pending_cast('source' if mode == 'unfunded_source' else 'spell', allowed=False)
        elif mode == 'cast_source':
            self.paid('source')
        elif mode in ('countered_source', 'countered_matching_spell'):
            self.countered('source' if mode == 'countered_source' else 'spell')
        elif mode == 'copy_does_not_trigger':
            self.copied()
        elif mode == 'search_basic_land':
            self.search_land()
        elif mode.startswith('land_') or mode == 'colored_land_creature':
            self.enter_land()
        elif mode != 'setup_land':
            if mode == 'source_removed':
                self.pending_cast('remove_staff', target='source')
                self.announce('remove_staff')
                self.emit('resolve')
                self.move('remove_staff', 'graveyard')
                self.move('source', 'graveyard')
            self.ordinary_spell('spell')
            if mode == 'two_matching_spells':
                self.receipts()
                self.ordinary_spell('second_spell')
            elif mode == 'two_events':
                self.receipts()
                self.enter_land()
            elif mode == 'staff_enters_after_match':
                self.receipts()
                self.paid('source')
        self.receipts()
        steps = self.steps
        if self.cut is not None:
            replay = steps[self.cut:]
            steps = steps[:self.cut] + [self.step('capture')] + replay
            steps += [self.step('record_state'), self.step('ai_roundtrip'), self.step('restore')]
            steps += copy.deepcopy(replay) + [self.step('assert_recorded_state')]
        case = dict(name=f'StaffMagusCastLandLifeRecipe_{self.csvid}_{mode}_{self.owner}', card=self.name,
                    fixture='empty_main', seed=123, setup=self.setup, steps=steps,
                    _recipe=dict(family=FAMILY, version=2, variant=f'{mode}_{self.owner}',
                                 oracle_text=self.d['oracle_text'], params=copy.deepcopy(self.d['params']), untested_components=[
                                     'Actual land entry is exercised through Walking Atlas and Rampant Growth. Independent land-play action is outside this bridge.',
                                     'The resolved-copy contrast uses the blue Staff and actual Twincast. It checks two resolves and only two real cast triggers.',
                                 ]))
        if self.owner:
            _mirror(case)
        return case


def expand(name, csvid, declaration, params, selected, rows, oracle):
    validate_witnesses(oracle, rows)
    validate_variants(params, selected)
    return [StaffBoard(name, csvid, declaration, params, variant, rows, oracle).run() for variant in selected]
