"""Own end-step triggers with stable battlefield predicates and real end-turn dispatch."""

import copy
import hashlib
import json
from pathlib import Path

from block_stat_trigger_recipes import BlockStatBoard
from combat_value_trigger_recipes import _mirror
from grant_recipes import LIBRARY

FAMILY = 'own_end_step_state_trigger'
PROFILE_PATH = Path(__file__).with_name('data') / 'own_end_step_state_trigger_wave8_profiles.json'
PREFIX = 'At the beginning of your end step, '
CLAUSES = {
    'if you control no untapped lands, draw a card.': ('well_draw', 'Artifact'),
    'if you control no untapped lands, you gain 2 life.': ('well_life', 'Artifact'),
    'if this creature is untapped, you lose 3 life.': ('nim', 'Creature — Zombie'),
    'untap all Merfolk you control.': ('merfolk', 'Kindred Enchantment — Merfolk'),
    'each opponent loses life equal to the number of tapped creatures you control.': ('throne', 'Legendary Artifact'),
    'untap all lands you control.': ('lands', 'Enchantment'),
}
WELL_MODES = ['no_lands', 'tapped_pair', 'mixed_lands', 'enemy_untapped', 'enemy_tapped', 'tap_last_land', 'untap_last_land', 'turn_sequence']
EXTRAS = {
    'well_draw': WELL_MODES,
    'well_life': WELL_MODES,
    'nim': ['source_tapped', 'attack_tap', 'tap_with_twiddle', 'untap_with_twiddle', 'turn_sequence'],
    'merfolk': ['two_merfolk', 'kindred_source_tapped', 'nonmerfolk_only', 'ready_merfolk'],
    'throne': ['zero', 'mixed_creatures', 'three_creatures', 'tap_mana_creature', 'noncreatures_only'],
    'lands': ['tapped_pair', 'mixed_lands', 'tap_last_land', 'no_lands'],
}


def profiles():
    return json.loads(PROFILE_PATH.read_text(encoding='utf8'))


def parameters(record):
    text = record.get('oracle_text', '')
    clause = CLAUSES.get(text.removeprefix(PREFIX)) if text.startswith(PREFIX) else None
    if (not clause or record.get('layout') != 'normal' or record.get('card_faces') or record.get('selected_face')
            or record.get('keywords') != [] or record.get('type_line') != clause[1]):
        raise ValueError('one complete reviewed own-end-step clause required')
    role = clause[0]
    if role == 'nim' and any(not record.get(key, '').isascii() or not record[key].isdecimal() for key in ('power', 'toughness')):
        raise ValueError('own-end-step creature requires fixed stats')
    from recipes import cost_from_oracle

    vector = cost_from_oracle(record)
    return dict(role=role, printed_mana=record['mana_cost'], has_colored_cost=any(vector[1:]))


def variants(params):
    modes = ['cast_payment', 'cast_unaffordable', 'cast_countered', 'own_end_step', 'opponent_end_step',
             'no_early_trigger', 'source_removed', 'end_step_snapshot', 'cast_then_end_step']
    if params['has_colored_cost']:
        modes += ['cast_wrong_color']
    modes += EXTRAS[params['role']]
    return [f'{mode}_{owner}' for owner in (0, 1) for mode in modes]


def validate_variants(params, selected):
    if (not isinstance(selected, list) or not selected or any(type(v) is not str for v in selected)
            or len(set(selected)) != len(selected) or not set(selected) <= set(variants(params))):
        raise ValueError('distinct supported own-end-step variants required')


def spec(declaration, record):
    params = parameters(record)
    pin = profiles()['sources'].get(record['name'])
    if (declaration.get('family') != FAMILY or type(declaration.get('version')) is not int or declaration['version'] != 2
            or not pin or pin['oracle'] != record or pin['params'] != params or declaration.get('oracle_text') != record['oracle_text']
            or json.dumps(declaration.get('params'), sort_keys=True) != json.dumps(params, sort_keys=True)):
        raise ValueError('complete pinned Oracle and typed own-end-step parameters required')
    validate_variants(params, declaration.get('variants'))
    return dict(params, family=FAMILY)


def validate_witnesses(oracle, rows):
    data = profiles()
    root = Path(__file__).resolve().parents[4]
    catalog = json.loads((PROFILE_PATH.parent / 'unimplemented.json').read_text(encoding='utf8'))
    unimplemented = {row['csvid'] for row in catalog['cases'].values()}
    static_only = set(data['native_characteristics_only_witnesses'])
    for name, pin in data['witnesses'].items():
        row = rows.get(name, {})
        if (oracle.get(name) != pin['oracle'] or row != pin['registry'] or row.get('kind') != 'card'
                or name not in static_only and (row['csvid'] in unimplemented or 'dummy' in row['handler'])):
            raise ValueError('own-end-step witness changed: ' + name)
    for name, pin in data['sources'].items():
        row = rows.get(name, {})
        source = pin['compiled_source']
        if (oracle.get(name) != pin['oracle'] or row != pin['registry'] or row.get('kind') != 'card' or row.get('csvid') in unimplemented
                or 'dummy' in row['handler'] or hashlib.sha256((root / source['path']).read_bytes()).hexdigest() != source['sha256']):
            raise ValueError('implemented own-end-step source changed: ' + name)


class EndStepBoard(BlockStatBoard):
    def __init__(self, name, csvid, declaration, params, variant, rows, oracle):
        from recipes import card, cost_from_oracle, observe, step

        self.card, self.step, self.observe, self.cost = card, step, observe, cost_from_oracle
        self.name, self.csvid, self.d, self.p, self.rows, self.oracle = name, csvid, declaration, params, rows, oracle
        self.mode, owner = variant.rsplit('_', 1)
        self.owner = int(owner)
        self.player = int(self.mode == 'opponent_end_step')
        self.steps, self.setup, self.entities, self.base = [], [], {}, {}
        self.tokens, self.bound = [], set()
        self.aura_target = None
        self.boundary = self.discard_nondeterministic = False
        self.life = [20, 20]
        self.pools = [[10] * 6, [10] * 6]
        self.mana = [60, 60]
        self.zones = []
        for player in (0, 1):
            zones = dict(hand=['Forest', 'Plains'], graveyard=['Island'], library=list(LIBRARY), exile=['Mountain'])
            self.zones.append(zones)
            for zone, names in zones.items():
                for index, stock in enumerate(names):
                    self.setup.append(card(stock, f'stock_{zone}_{player}_{index}', player, zone))

        mode = self.mode
        role = params['role']
        source_tapped = mode in ('source_tapped', 'untap_with_twiddle', 'kindred_source_tapped')
        cast_source = mode.startswith('cast_')
        self.add(name, 'source', 0, 'hand' if cast_source else 'battlefield', ['tapped'] if source_tapped else None)
        if mode == 'cast_countered':
            self.add('Counterspell', 'counter', 1, 'hand')
        if mode == 'source_removed':
            spell = 'Unsummon' if role == 'nim' else 'Disenchant'
            self.add(spell, 'remove_source', 0, 'hand')
        if mode in ('untap_last_land', 'tap_with_twiddle', 'untap_with_twiddle'):
            self.add('Twiddle', 'twiddle', 0, 'hand')
        if not cast_source or mode == 'cast_then_end_step':
            self.feature_board(role)

        self.emit('turn', player=self.player, value=0)
        self.emit('present_ai_dialogs', value=1)
        if mode == 'cast_payment':
            self.pools[0] = self.cost(oracle[name])
        elif mode in ('cast_unaffordable', 'tap_last_land', 'tap_mana_creature'):
            self.pools[0] = [0] * 6
        elif mode == 'cast_wrong_color':
            self.pools[0] = [60, 0, 0, 0, 0, 0]
        for player in (0, 1):
            self.set_pool(player, self.pools[player])
        self.emit('refresh')

    def add_tapped(self, name, ref, player=0):
        self.add(name, ref, player, state=['tapped'])

    def feature_board(self, role):
        mode = self.mode
        if role in ('well_draw', 'well_life', 'lands'):
            if mode in ('no_lands', 'turn_sequence'):
                return
            if mode in ('tap_last_land', 'enemy_tapped'):
                self.add('Forest', 'land')
            else:
                self.add_tapped('Forest', 'land')
            if mode == 'tapped_pair':
                self.add_tapped('Island', 'land_two')
            elif mode == 'mixed_lands':
                self.add('Island', 'land_two')
            if mode == 'enemy_untapped':
                self.add('Island', 'enemy_land', 1)
            else:
                self.add_tapped('Island', 'enemy_land', 1)
            if role == 'lands':
                self.add_tapped('Grizzly Bears', 'nonland')
            return
        if role == 'merfolk':
            if mode != 'nonmerfolk_only':
                if mode == 'ready_merfolk':
                    self.add('Merfolk of the Pearl Trident', 'merfolk')
                else:
                    self.add_tapped('Merfolk of the Pearl Trident', 'merfolk')
            if mode == 'two_merfolk':
                self.add_tapped('Merfolk of the Pearl Trident', 'merfolk_two')
            self.add_tapped('Merfolk of the Pearl Trident', 'enemy_merfolk', 1)
            self.add_tapped('Grizzly Bears', 'nonmerfolk')
            return
        if role == 'throne':
            if mode == 'tap_mana_creature':
                self.add('Elvish Mystic', 'mana_creature')
            elif mode == 'zero':
                self.add('Grizzly Bears', 'ready_creature')
            elif mode not in ('zero', 'noncreatures_only'):
                self.add_tapped('Grizzly Bears', 'creature')
            if mode == 'mixed_creatures':
                self.add('Grizzly Bears', 'ready_creature')
                self.add_tapped('Merfolk of the Pearl Trident', 'second_creature')
            if mode == 'three_creatures':
                self.add_tapped('Grizzly Bears', 'second_creature')
                self.add_tapped('Merfolk of the Pearl Trident', 'third_creature')
            self.add_tapped('Grizzly Bears', 'enemy_creature', 1)
            self.add_tapped('Forest', 'noncreature_land')
            self.add_tapped('Glasses of Urza', 'noncreature_artifact')

    def land(self, entity):
        return 'Land' in self.oracle[entity['name']]['type_line'].split(' — ')[0].split()

    def subtype(self, entity, name):
        return name in self.oracle[entity['name']]['type_line'].partition(' — ')[2].split()

    def expect_end_step(self):
        if self.player != 0 or self.entities['source']['zone'] != 'battlefield':
            return
        controls = [entity for entity in self.entities.values() if entity['player'] == 0 and entity['zone'] == 'battlefield']
        role = self.p['role']
        if role in ('well_draw', 'well_life') and not any(self.land(entity) and not entity['tapped'] for entity in controls):
            if role == 'well_draw':
                self.draw(0, 1)
            else:
                self.life[0] += 2
        if role == 'nim' and not self.entities['source']['tapped']:
            self.life[0] -= 3
        if role == 'throne':
            self.life[1] -= sum(entity['creature'] and entity['tapped'] for entity in controls)
        if role in ('merfolk', 'lands'):
            for entity in controls:
                qualifies = self.subtype(entity, 'Merfolk') if role == 'merfolk' else self.land(entity)
                if qualifies:
                    entity['tapped'] = False

    def finish_step(self):
        self.expect_end_step()
        # Fixtures use board state that survives cleanup. Start with no floated
        # mana so the legacy mana-burn path cannot obscure the end-step effect.
        for player in (0, 1):
            self.set_pool(player, [0] * 6)
        self.emit('cleanup')
        self.emit('refresh')
        self.receipts()

    def advance_and_untap(self):
        self.expect_end_step()
        for player in (0, 1):
            self.set_pool(player, [0] * 6)
        self.emit('advance_turn')
        self.receipts()
        self.player = 1 - self.player
        self.emit('untap_step', player=self.player)
        for entity in self.entities.values():
            if entity['zone'] == 'battlefield' and entity['player'] == self.player:
                entity['tapped'] = False
        self.receipts()

    def tap_for_mana(self, ref):
        self.emit('can_activate', ref=ref, value=1)
        self.emit('activate', ref=ref, value=1)
        self.entities[ref]['tapped'] = True
        self.pools[0][3] += 1
        self.mana[0] += 1
        self.emit('assert_stack', value=0)
        self.emit('assert_mana', player=0, color=3, value=1)
        self.emit('assert_mana_total', player=0, value=1)
        self.emit('assert_card', ref=ref, property='tapped', value=1)

    def twiddle(self, ref, tapped):
        self.emit('choose_target', player=0, ref=ref)
        self.emit('cast', ref='twiddle', value=1)
        self.spend(self.oracle['Twiddle']['mana_cost'])
        self.emit('assert_stack', value=1)
        self.emit('choose_dialog', player=0, value=int(not tapped))
        self.emit('resolve')
        self.move('twiddle', 'graveyard')
        self.entities[ref]['tapped'] = tapped
        self.receipts()

    def attack_tap(self):
        self.emit('begin_attackers')
        self.emit('attack', ref='source', value=1)
        self.entities['source']['tapped'] = True
        self.emit('finish_attackers')
        self.emit('begin_blockers')
        self.emit('advance_combat')
        self.emit('combat_damage')
        self.emit('resolve')
        self.life[1] -= self.power('source')
        self.emit('end_combat')
        self.receipts()

    def cast_countered(self):
        self.emit('cast', ref='source', value=1)
        self.spend(self.p['printed_mana'])
        self.emit('assert_stack', value=1)
        self.emit('assert_card', ref='source', property='in_hand', value=0)
        self.emit('assert_mana_total', player=0, value=self.mana[0])
        self.emit('announce_spell', ref='source')
        self.emit('cast_response', ref='counter', value=1)
        self.spend(self.oracle['Counterspell']['mana_cost'], 1)
        self.emit('assert_stack', value=2)
        self.emit('resolve_response')
        self.move('source', 'graveyard')
        self.move('counter', 'graveyard')

    def run(self):
        mode = self.mode
        if mode in ('cast_wrong_color', 'cast_unaffordable'):
            self.emit('cast', ref='source', value=0)
            self.emit('assert_card', ref='source', property='in_hand', value=1)
        elif mode == 'cast_countered':
            self.cast_countered()
        elif mode in ('cast_payment', 'cast_then_end_step'):
            self.paid('source')
            if self.p['role'] == 'nim':
                self.emit('assert_card', ref='source', property='sick', value=1)
        elif mode == 'source_removed':
            self.paid('remove_source', 'source')
            self.move('source', 'hand' if self.p['role'] == 'nim' else 'graveyard')
        elif mode == 'tap_last_land':
            self.tap_for_mana('land')
        elif mode == 'tap_mana_creature':
            self.tap_for_mana('mana_creature')
        elif mode == 'untap_last_land':
            self.twiddle('land', False)
        elif mode in ('tap_with_twiddle', 'untap_with_twiddle'):
            self.twiddle('source', mode == 'tap_with_twiddle')
        elif mode == 'attack_tap':
            self.attack_tap()

        self.receipts()
        cut = len(self.steps) if mode == 'end_step_snapshot' else None
        if mode == 'turn_sequence':
            self.advance_and_untap()
            self.advance_and_untap()
        if mode not in ('cast_payment', 'cast_unaffordable', 'cast_wrong_color', 'cast_countered', 'no_early_trigger'):
            self.finish_step()
        steps = self.steps
        if cut is not None:
            replay = steps[cut:]
            steps = steps[:cut] + [self.step('capture')] + replay
            steps += [self.step('record_state'), self.step('ai_roundtrip'), self.step('restore')]
            steps += copy.deepcopy(replay) + [self.step('assert_recorded_state')]
        case = dict(name=f'OwnEndStepStateTriggerRecipe_{self.csvid}_{mode}_{self.owner}', card=self.name,
                    fixture='empty_main', seed=123, setup=self.setup, steps=steps,
                    _recipe=dict(family=FAMILY, version=2, variant=f'{mode}_{self.owner}',
                                 oracle_text=self.d['oracle_text'], params=copy.deepcopy(self.d['params']), untested_components=[
                                     'The bounded end-turn operation completes the trigger and cleanup; separate priority during that trigger is outside these fixtures.',
                                     'Every predicate uses battlefield state that survives cleanup. Multiplayer scope and overlapping end-step trigger ordering are outside the two-player cohort.',
                                 ]))
        if self.owner:
            _mirror(case)
        return case


def expand(name, csvid, declaration, params, selected, rows, oracle):
    validate_witnesses(oracle, rows)
    validate_variants(params, selected)
    return [EndStepBoard(name, csvid, declaration, params, variant, rows, oracle).run() for variant in selected]
