"""Single-color Laces with real stack targets and color-dependent gameplay."""

import copy
import hashlib
import json
from pathlib import Path
import re

from block_stat_trigger_recipes import BlockStatBoard
from combat_value_trigger_recipes import _mirror
from grant_recipes import LIBRARY

FAMILY = 'spell_or_permanent_recolor'
PROFILE_PATH = Path(__file__).with_name('data') / 'spell_or_permanent_recolor_wave8_profiles.json'
MASK = {'B': 2, 'U': 4, 'G': 8, 'R': 16, 'W': 32}
COLORS = {
    'black': ('B', 'Deathlace', 'White Knight', 'Sengir Vampire'),
    'green': ('G', 'Lifelace', 'Spectral Lynx', 'Fangren Marauder'),
    'white': ('W', 'Purelace', 'Black Knight', 'Alaborn Cavalier'),
    'blue': ('U', 'Thoughtlace', 'Goblin Piledriver', 'Scroll Thief'),
}
MODES = ('permanent_own', 'permanent_opposing', 'permanent_multicolored', 'permanent_artifact', 'permanent_enchantment',
         'permanent_land_mana', 'permanent_tapped', 'persist_cleanup', 'printed_symbols', 'spell_becomes_permanent',
         'spell_own_becomes_permanent', 'spell_protection', 'spell_protection_control', 'target_leaves', 'target_hexproof',
         'source_countered', 'no_target', 'no_mana', 'same_color_eligible', 'replace_previous_color',
         'recast_after_change', 'pending_permanent_replay', 'pending_spell_replay')
SPELL_COLOR_MODES = ('spell_becomes_permanent', 'spell_own_becomes_permanent', 'spell_protection',
                     'spell_protection_control', 'pending_spell_replay')


def profiles():
    return json.loads(PROFILE_PATH.read_text(encoding='utf8'))


def parameters(record):
    text = record.get('oracle_text', '')
    effect = re.fullmatch(r'Target spell or permanent becomes (black|green|white|blue)\. \(Mana symbols on that permanent remain unchanged\.\)', text)
    if (not effect or record.get('layout') != 'normal' or record.get('card_faces') or record.get('selected_face')
            or record.get('type_line') != 'Instant' or record.get('keywords') != []):
        raise ValueError('whole single-color Lace clause required')
    color, name, _, _ = COLORS[effect[1]]
    if record.get('name') != name or record.get('mana_cost') != '{' + color + '}' or record.get('colors') != [color]:
        raise ValueError('Lace name, printed cost, and output color must agree')
    return dict(color=color, mask=MASK[color], printed_mana=record['mana_cost'], targets=['spell', 'permanent'], changes_mana_symbols=False)


def variants(params):
    return [f'{mode}_{player}' for player in (0, 1) for mode in MODES]


def validate_variants(params, selected):
    if (not isinstance(selected, list) or not selected or any(type(v) is not str for v in selected)
            or len(set(selected)) != len(selected) or not set(selected) <= set(variants(params))):
        raise ValueError('distinct supported Lace variants required')


def spec(declaration, record):
    params = parameters(record)
    pin = profiles()['sources'].get(record['name'])
    if (declaration.get('family') != FAMILY or type(declaration.get('version')) is not int or declaration['version'] != 2
            or not pin or pin['oracle'] != record or pin['params'] != params or declaration.get('oracle_text') != record['oracle_text']
            or json.dumps(declaration.get('params'), sort_keys=True) != json.dumps(params, sort_keys=True)):
        raise ValueError('complete pinned Oracle and typed Lace parameters required')
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
            raise ValueError('Lace source or witness changed: ' + name)
        if name not in static_only and (row['csvid'] in unimplemented_ids or 'dummy' in row['handler']):
            raise ValueError('Lace action witness must be implemented: ' + name)
    for name, pin in data['sources'].items():
        source = pin['compiled_source']
        if hashlib.sha256((root / source['path']).read_bytes()).hexdigest() != source['sha256']:
            raise ValueError('Lace compiled source changed: ' + name)


class LaceBoard(BlockStatBoard):
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
        self.zones = []
        for player in (0, 1):
            zones = dict(hand=['Forest', 'Plains'], graveyard=['Island'], library=list(LIBRARY), exile=['Mountain'])
            self.zones.append(zones)
            for zone, names in zones.items():
                self.setup.extend(card(stock, f'stock_{zone}_{player}_{index}', player, zone) for index, stock in enumerate(names))

        _, _, protected, same_color = next(definition for definition in COLORS.values() if definition[0] == params['color'])
        self.add(name, 'source', 0, 'hand')
        if mode not in ('no_mana', 'spell_protection_control'):
            self.add_budget(0, params['printed_mana'])
        if mode in SPELL_COLOR_MODES:
            spell_controller = int(mode != 'spell_own_becomes_permanent')
            if mode in ('spell_protection', 'spell_protection_control'):
                self.add('Lightning Bolt', 'spell', 1, 'hand')
                self.add(protected, 'receiver')
                self.add_budget(1, oracle['Lightning Bolt']['mana_cost'])
            else:
                self.add('Furnace Whelp', 'spell', spell_controller, 'hand')
                self.add_budget(spell_controller, oracle['Furnace Whelp']['mana_cost'])
        elif mode != 'no_target':
            target = {'permanent_multicolored': 'Skyknight Legionnaire' if params['color'] == 'G' else 'Ghor-Clan Rampager',
                      'permanent_artifact': 'Glasses of Urza', 'permanent_enchantment': 'Fervor',
                      'permanent_land_mana': 'Forest', 'same_color_eligible': same_color}.get(mode, 'Furnace Whelp')
            player = int(mode in ('permanent_opposing', 'target_leaves', 'target_hexproof', 'recast_after_change'))
            self.add(target, 'target', player, state=['tapped'] if mode == 'permanent_tapped' else None)
        helper = {'target_leaves': ('Unsummon', 1), 'target_hexproof': ("Ranger's Guile", 1),
                  'source_countered': ('Counterspell', 1), 'printed_symbols': ('Fanatic of Mogis', 0),
                  'recast_after_change': ('Unsummon', 1)}.get(mode)
        if mode == 'replace_previous_color':
            helper = ('Lifelace' if params['color'] == 'B' else 'Deathlace', 0)
        if helper:
            witness, player = helper
            self.add(witness, 'helper', player, 'hand')
            self.add_budget(player, oracle[witness]['mana_cost'])
        if mode == 'recast_after_change':
            self.add_budget(1, oracle['Furnace Whelp']['mana_cost'])
        turn = int(mode in ('permanent_opposing', 'target_leaves', 'target_hexproof', 'recast_after_change') or
                   (mode in SPELL_COLOR_MODES and mode != 'spell_own_becomes_permanent'))
        self.emit('turn', player=turn, value=0)
        self.emit('present_ai_dialogs', value=1)
        for player in (0, 1):
            self.set_pool(player, self.pools[player])
        self.emit('refresh')

    def add_budget(self, player, mana):
        self.pools[player] = [have + cost for have, cost in zip(self.pools[player], self.cost(dict(mana_cost=mana)))]

    def visible_color(self, ref, value):
        item = self.entities[ref]
        self.emit('assert_matching_permanents', player=item['player'], index=self.rows[item['name']]['csvid'], property='color', value=value)

    def printed_color(self, ref):
        return sum(MASK[color] for color in self.oracle[self.entities[ref]['name']]['colors'])

    def pending(self, ref, *, target=None, response=False):
        item = self.entities[ref]
        if target:
            self.emit('choose_target', player=item['player'], ref=target)
        self.emit('cast_response' if response else 'cast', ref=ref, value=1)
        self.spend(self.oracle[item['name']]['mana_cost'], item['player'])
        self.emit('assert_stack', value=2 if response else 1)
        self.emit('assert_card', ref=ref, property='in_hand', value=0)
        self.emit('assert_mana_total', player=item['player'], value=self.mana[item['player']])
        for color, value in enumerate(self.pools[item['player']]):
            self.emit('assert_mana', player=item['player'], color=color, value=value)

    def resolve_instants(self, *refs, response=False):
        self.emit('resolve_response' if response else 'resolve')
        self.emit('refresh')
        for ref in refs:
            self.move(ref, 'graveyard')

    def returned_to_hand(self, ref):
        self.move(ref, 'hand')
        item = self.entities[ref]
        self.emit('bind_hand', ref=ref, player=item['owner'], value=self.rows[item['name']]['csvid'])
        self.emit('assert_card', ref=ref, property='in_hand', value=1)

    def must_persist(self, ref):
        self.emit('cleanup')
        self.emit('refresh')
        self.visible_color(ref, self.p['mask'])

    def spell_mode(self):
        protection = self.mode in ('spell_protection', 'spell_protection_control')
        self.pending('spell', target='receiver' if protection else None)
        self.emit('announce_spell', ref='spell')
        if self.mode == 'spell_protection_control':
            self.resolve_instants('spell')
            self.move('receiver', 'graveyard')
            return
        # The legacy stack selector binds to its real pending parent. It does
        # not request a battlefield target from the scripted fixture.
        self.pending('source', response=True)
        if self.mode == 'pending_spell_replay':
            self.cut = len(self.steps)
        if protection:
            self.resolve_instants('source', 'spell', response=True)
        else:
            self.resolve_instants('source', response=True)
            self.move('spell', 'battlefield')
            # CR 400.7a preserves a characteristic-changing effect on the
            # resolving permanent spell as it becomes the permanent.
            self.visible_color('spell', self.p['mask'])

    def permanent_mode(self):
        mode = self.mode
        before = self.printed_color('target')
        self.visible_color('target', before)
        self.pending('source', target='target')
        self.visible_color('target', before)
        if mode == 'pending_permanent_replay':
            self.cut = len(self.steps)
        if mode in ('target_leaves', 'target_hexproof', 'source_countered'):
            self.pending('helper', response=True, target='target' if mode != 'source_countered' else None)
            self.resolve_instants('helper', 'source', response=True)
            if mode == 'target_leaves':
                self.returned_to_hand('target')
            elif mode == 'target_hexproof':
                self.entities['target']['boost'] = 1
                self.visible_color('target', before)
            else:
                self.visible_color('target', before)
            return
        self.resolve_instants('source')
        self.visible_color('target', self.p['mask'])
        if mode == 'persist_cleanup':
            self.must_persist('target')
            self.emit('advance_turn')
            self.emit('refresh')
            self.visible_color('target', self.p['mask'])
        elif mode == 'permanent_artifact':
            self.emit('assert_matching_permanents', player=0, index=self.rows['Glasses of Urza']['csvid'], property='artifact', value=1)
        elif mode == 'permanent_land_mana':
            self.emit('activate', ref='target', value=1)
            self.entities['target']['tapped'] = True
            self.pools[0][3] += 1
            self.mana[0] += 1
            self.visible_color('target', self.p['mask'])
        elif mode == 'printed_symbols':
            self.paid('helper')
            self.life[1] -= self.oracle['Furnace Whelp']['mana_cost'].count('{R}') + self.oracle['Fanatic of Mogis']['mana_cost'].count('{R}')
            self.visible_color('target', self.p['mask'])
        elif mode == 'replace_previous_color':
            self.pending('helper', target='target')
            self.resolve_instants('helper')
            self.visible_color('target', self.printed_color('helper'))
        elif mode == 'recast_after_change':
            self.pending('helper', target='target')
            self.resolve_instants('helper')
            self.returned_to_hand('target')
            self.paid('target')
            self.visible_color('target', before)

    def run(self):
        if self.mode in SPELL_COLOR_MODES:
            self.spell_mode()
        elif self.mode == 'same_color_eligible':
            # An already matching object is still a legal target. Read the
            # actual predicate without feeding a target an engine may reject.
            self.emit('can_cast', ref='source', value=1)
        elif self.mode in ('no_target', 'no_mana'):
            if self.mode == 'no_target':
                self.emit('can_cast', ref='source', value=0)
            self.emit('cast', ref='source', value=0)
            self.emit('assert_card', ref='source', property='in_hand', value=1)
        else:
            self.permanent_mode()
        self.receipts()
        if self.cut is not None:
            replay = self.steps[self.cut:]
            self.steps[self.cut:self.cut] = [self.step('capture')]
            self.steps += [self.step('record_state'), self.step('ai_roundtrip'), self.step('restore')]
            self.steps += copy.deepcopy(replay) + [self.step('assert_recorded_state')]
        variant = f'{self.mode}_{self.owner}'
        case = dict(name=f'SpellOrPermanentRecolorRecipe_{self.csvid}_{variant}', card=self.name,
                    fixture='empty_main', seed=123, setup=self.setup, steps=self.steps,
                    _recipe=dict(family=FAMILY, version=2, variant=variant, params=copy.deepcopy(self.d['params']),
                                 oracle_text=self.d['oracle_text'], untested_components=[
                                     'The legacy stack selector automatically chooses its pending parent. Multiple simultaneous spell choices remain outside the cohort.',
                                     'Spell color evidence comes from protection during resolution and the permanent it becomes. Direct mid-stack measurement is outside the bridge.',
                                 ]))
        if self.owner:
            _mirror(case)
        return case


def expand(name, csvid, declaration, params, selected, rows, oracle):
    validate_witnesses(oracle, rows)
    validate_variants(params, selected)
    return [LaceBoard(name, csvid, declaration, params, variant, rows, oracle).run() for variant in selected]
