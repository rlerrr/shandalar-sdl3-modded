"""Player hexproof exercises the real spell, ability, and phase paths."""

import copy
import hashlib
import json
from pathlib import Path
import re

from block_stat_trigger_recipes import BlockStatBoard
from combat_value_trigger_recipes import _mirror

FAMILY = 'player_hexproof'
PROFILE_PATH = Path(__file__).with_name('data') / 'player_hexproof_wave8_profiles.json'
HEXPROOF = "You have hexproof. (You can't be the target of spells or abilities your opponents control.)"
HEXPROOF_AURAS = "You have hexproof. (You can't be the target of spells or abilities your opponents control, including Aura spells.)"
OPENING = 'If this card is in your opening hand, you may begin the game with it on the battlefield.'
TEAM = "When this enchantment enters, if it's not a token, each of your teammates creates a token that's a copy of this enchantment."
CURSES = 'When this artifact enters, destroy all Curses attached to you.'
ORACLE_EFFECTS = {
    HEXPROOF: ('static', ()),
    TEAM + '\n' + HEXPROOF: ('team', ()),
    OPENING + '\n' + HEXPROOF: ('opening', ()),
    'Flying\n' + HEXPROOF: ('flying', ('Flying',)),
    CURSES + '\n' + HEXPROOF_AURAS: ('curses', ()),
}
MODES = ('unprotected', 'opponent_blocked', 'own_target', 'enemy_self_target', 'symmetric_damage',
         'ability_blocked', 'ability_unprotected', 'removed', 'late_entry', 'snapshot')
EXTRA = {'static': (), 'team': ('no_teammates',), 'opening': ('opening_place', 'opening_decline'),
         'flying': ('flying_ground', 'flying_blocked'), 'curses': ('curse_scope', 'curse_both_owners', 'own_aura')}
LIBRARY = ('Forest', 'Island', 'Plains', 'Mountain')
CURSE = 'Curse of the Pierced Heart'


def profiles():
    return json.loads(PROFILE_PATH.read_text(encoding='utf8'))


def parameters(record):
    params = ORACLE_EFFECTS.get(record.get('oracle_text'))
    mana = record.get('mana_cost', '')
    line = record.get('type_line', '')
    if (not params or record.get('layout') != 'normal' or record.get('card_faces') or record.get('selected_face')
            or not line.startswith(('Creature ', 'Enchantment', 'Artifact'))
            or not mana or not re.fullmatch(r'(?:\{[1-9]\})?(?:\{W\}){0,2}', mana)):
        raise ValueError('complete player-hexproof permanent and printed mana required')
    branch, keywords = params
    if record.get('keywords') != list(keywords) or record.get('colors') != (['W'] if '{W}' in mana else []):
        raise ValueError('unexpected printed player-hexproof keywords or color')
    return dict(mana=mana, branch=branch, type_line=line, power=int(record['power']) if 'Creature' in line else None,
                toughness=int(record['toughness']) if 'Creature' in line else None)


def variants(params):
    return [f'{mode}_{owner}' for owner in (0, 1) for mode in MODES + EXTRA[params['branch']]]


def spec(declaration, record):
    params = parameters(record)
    pin = profiles()['sources'].get(record['name'])
    if (declaration.get('family') != FAMILY or type(declaration.get('version')) is not int or declaration['version'] != 2
            or not pin or pin['oracle'] != record or pin['params'] != params or declaration.get('oracle_text') != record['oracle_text']
            or declaration.get('params') != params):
        raise ValueError('complete pinned player-hexproof text and parameters required')
    selected = declaration.get('variants')
    if not isinstance(selected, list) or not selected or any(type(variant) is not str for variant in selected):
        raise ValueError('explicit player-hexproof variants required')
    if len(selected) != len(set(selected)) or not set(selected) <= set(variants(params)):
        raise ValueError('unique reviewed player-hexproof variants required')
    return dict(params, family=FAMILY)


def validate_witnesses(oracle, rows):
    data = profiles()
    unimplemented = json.loads((PROFILE_PATH.parent / 'unimplemented.json').read_text(encoding='utf8'))['cases']
    pending = {entry['csvid'] for entry in unimplemented.values()}
    static = set(data['static_characteristics_only_witnesses'])
    for name, pin in (data['witnesses'] | data['sources']).items():
        row = rows.get(name, {})
        if oracle.get(name) != pin['oracle'] or row != pin['registry'] or row.get('kind') != 'card':
            raise ValueError('player-hexproof source or witness changed: ' + name)
        if name not in static and (row['csvid'] in pending or 'dummy' in row['handler']):
            raise ValueError('player-hexproof action witness must be implemented: ' + name)
    root = Path(__file__).resolve().parents[4]
    for name, pin in data['sources'].items():
        c = pin['compiled_source']
        if hashlib.sha256((root / c['path']).read_bytes()).hexdigest() != c['sha256']:
            raise ValueError('player-hexproof compiled source changed: ' + name)


class HexproofBoard(BlockStatBoard):
    def __init__(self, name, csvid, declaration, params, variant, rows, oracle):
        from recipes import card, cost_from_oracle, observe, step

        self.card, self.step, self.observe, self.cost = card, step, observe, cost_from_oracle
        self.name, self.csvid, self.d, self.p, self.rows, self.oracle = name, csvid, declaration, params, rows, oracle
        self.mode, owner = variant.rsplit('_', 1)
        self.owner = int(owner)
        self.steps, self.setup, self.entities, self.base = [], [], {}, {}
        self.tokens, self.bound = [], set()
        self.boundary = self.discard_nondeterministic = False
        self.life, self.mana, self.pools = [20, 20], [0, 0], [[0] * 6, [0] * 6]
        self.zones = []
        for player in (0, 1):
            zones = dict(hand=['Forest', 'Plains'], library=list(LIBRARY), graveyard=['Island'], exile=['Mountain'])
            self.zones.append(zones)
            for zone, names in zones.items():
                self.setup.extend(self.card(stock, f'stock_{zone}_{player}_{index}', player, zone) for index, stock in enumerate(names))
        living = self.mode in ('late_entry', 'flying_ground', 'flying_blocked')
        self.add(name, 'source', 0, 'battlefield' if living else 'hand')
        self.curses = []
        self.build_witnesses()
        self.cut = None

    def build_witnesses(self):
        mode = self.mode
        if mode in ('unprotected', 'opponent_blocked', 'removed', 'late_entry', 'snapshot', 'opening_place', 'opening_decline'):
            self.add('Bump in the Night', 'probe', 1, 'hand')
        if mode in ('removed', 'late_entry', 'snapshot'):
            self.add('Vindicate', 'removal', 1, 'hand')
        if mode == 'late_entry':
            self.add("Faith's Reward", 'returner', 0, 'hand')
        if mode in ('own_target', 'enemy_self_target'):
            self.add('Lightning Bolt', 'bolt', int(mode == 'enemy_self_target'), 'hand')
        if mode == 'symmetric_damage':
            self.add('Flame Rift', 'sweep', 1, 'hand')
        if mode in ('ability_blocked', 'ability_unprotected'):
            self.add('Razortip Whip', 'ability', 1)
        if mode.startswith('flying_'):
            self.add('Hill Giant', 'ground', 1)
            if mode == 'flying_blocked':
                self.add('Welkin Tern', 'flyer', 1)
        if mode == 'curse_scope':
            self.curses = [(1, 0), (1, 1)]
        if mode == 'curse_both_owners':
            self.curses = [(0, 0), (1, 0), (1, 1)]
        if mode == 'own_aura':
            self.curses = [(0, 0)]
        for index, (player, _) in enumerate(self.curses):
            self.add(CURSE, f'curse_{index}', player, 'hand')
        if self.curses:
            self.add('Howling Mine', 'noncurse', 1)

    def stage_cast(self, ref, target=None, player_target=None, response=False):
        entity = self.entities[ref]
        player = entity['player']
        name = entity['name']
        if target is not None:
            self.emit('choose_target', player=player, ref=target)
        if player_target is not None:
            self.emit('choose_player', player=player, value=player_target)
        self.emit('cast_response' if response else 'cast', ref=ref, value=1)
        self.spend(self.oracle[name]['mana_cost'], player)
        self.emit('assert_unused_choices', value=0)
        self.emit('assert_stack', value=2 if response else 1)
        self.emit('assert_card', ref=ref, property='in_hand', value=0)
        self.assert_pools()

    def assert_pools(self):
        for player, pool in enumerate(self.pools):
            for color, amount in enumerate(pool):
                self.emit('assert_mana', player=player, color=color, value=amount)

    def provide(self, player, *refs):
        prices = [self.cost(self.oracle[self.entities[ref]['name']]) for ref in refs]
        self.set_pool(player, [sum(items) for items in zip(*prices)])
        self.emit('refresh')

    def finish(self, ref, zone, response=False):
        self.emit('resolve_response' if response else 'resolve')
        self.emit('refresh')
        self.move(ref, zone)
        self.emit('assert_stack', value=0)

    def cast_permanent(self, ref, recipient=None, funded=False):
        if not funded:
            self.provide(self.entities[ref]['player'], ref)
        self.stage_cast(ref, player_target=recipient)
        self.finish(ref, 'battlefield')
        self.emit('assert_card', ref=ref, property='in_play', value=1)

    def next_turn(self):
        if any(self.mana):
            raise ValueError('turn change requires the authored payments to empty mana pools')
        self.emit('advance_turn')
        self.emit('refresh')

    def check_probe(self, legal, funded=False):
        if not funded:
            self.provide(1, 'probe')
        self.emit('can_cast', ref='probe', value=int(legal))
        if legal:
            # Bump through GS_CAN_ONLY_TARGET_OPPONENT fixes the sole target.
            self.stage_cast('probe')
            self.finish('probe', 'graveyard')
            self.life[0] -= 3
        else:
            self.emit('cast', ref='probe', value=0)
            self.emit('assert_unused_choices', value=0)
            self.emit('assert_card', ref='probe', property='in_hand', value=1)
            self.emit('assert_stack', value=0)
            self.assert_pools()

    def remove_source(self):
        self.stage_cast('removal', target='source')
        self.finish('removal', 'graveyard')
        self.move('source', 'graveyard')
        self.emit('assert_card', ref='source', property='in_play', value=0)

    def late_entry(self):
        self.emit('reset_death_history')
        self.provide(1, 'removal', 'probe')
        self.remove_source()
        self.emit('can_cast', ref='probe', value=1)
        self.stage_cast('probe')
        self.emit('announce_spell', ref='probe')
        self.provide(0, 'returner')
        self.stage_cast('returner', response=True)
        self.finish('returner', 'graveyard', response=True)
        self.move('source', 'battlefield')
        self.emit('bind_permanent', ref='source', player=0, value=self.csvid)
        self.emit('assert_card', ref='source', property='in_play', value=1)
        self.move('probe', 'graveyard')

    def target_ability(self, legal):
        self.set_pool(1, [1, 0, 0, 0, 0, 0])
        self.emit('refresh')
        self.emit('can_activate', ref='ability', value=int(legal))
        if legal:
            self.emit('choose_player', player=1, value=0)
        self.emit('activate', ref='ability', value=int(legal))
        self.emit('assert_unused_choices', value=0)
        self.emit('assert_stack', value=int(legal))
        if legal:
            self.spend('{1}', 1)
            self.entities['ability']['tapped'] = True
            self.emit('resolve')
            self.emit('refresh')
            self.life[0] -= 1
        self.assert_pools()

    def opening(self):
        # The actual legacy Rules Engine runs pregame prompts on the first
        # human upkeep and scans both original opening hands.
        first_human = self.owner
        self.emit('turn', player=1-first_human, value=0)
        self.emit('assert_card', ref='source', property='in_hand', value=1)
        self.next_turn()
        self.emit('untap_step', player=first_human)
        accepting = self.mode == 'opening_place'
        self.emit('choose_dialog', player=0, value=int(not accepting))
        self.emit('upkeep_step', player=first_human)
        self.emit('assert_unused_choices', value=0)
        self.emit('refresh')
        if accepting:
            self.move('source', 'battlefield')
        self.emit('assert_card', ref='source', property='in_play', value=int(accepting))
        self.emit('assert_card', ref='source', property='in_hand', value=int(not accepting))
        self.assert_pools()
        if first_human != 1:
            self.next_turn()
        else:
            self.emit('turn', player=1, value=0)
        self.check_probe(not accepting)

    def combat(self):
        self.emit('begin_attackers')
        self.emit('attack', ref='source', value=1)
        self.entities['source']['tapped'] = True
        self.emit('finish_attackers')
        self.emit('begin_blockers')
        self.emit('block', ref='ground', other='source', value=0)
        if self.mode == 'flying_blocked':
            self.emit('block', ref='flyer', other='source', value=1)
        self.emit('advance_combat')
        self.emit('combat_damage')
        self.emit('resolve')
        self.emit('end_combat')
        if self.mode == 'flying_blocked':
            self.entities['source']['damage'] += int(self.oracle['Welkin Tern']['power'])
            self.move('flyer', 'graveyard')
        else:
            self.life[1] -= self.p['power']

    def cast_curses(self):
        mode = self.mode
        if mode == 'own_aura':
            self.cast_permanent('source')
            self.cast_permanent('curse_0', recipient=0)
            return
        if mode == 'curse_both_owners':
            self.cast_permanent('curse_0', recipient=0)
            self.next_turn()
        for index, (player, recipient) in enumerate(self.curses):
            if player == 1:
                self.cast_permanent(f'curse_{index}', recipient=recipient)
        self.next_turn()
        self.cast_permanent('source')
        for index, (_, recipient) in enumerate(self.curses):
            if recipient == 0:
                self.move(f'curse_{index}', 'graveyard')

    def play(self):
        # empty_main starts both seats with ten of every mana color. Most
        # actions below fund just their actual actor, so clear both once.
        for player in (0, 1):
            self.set_pool(player, [0] * 6)
        mode = self.mode
        if mode.startswith('opening_'):
            self.opening()
            return
        self.emit('turn', player=int(mode in ('unprotected', 'ability_unprotected', 'late_entry', 'curse_scope')), value=0)
        self.emit('refresh')
        if mode == 'late_entry':
            self.late_entry()
        elif mode.startswith('flying_'):
            self.combat()
        elif self.curses:
            self.cast_curses()
        elif mode == 'unprotected':
            self.check_probe(True)
        elif mode == 'ability_unprotected':
            self.target_ability(True)
        else:
            self.cast_permanent('source')
            if mode == 'snapshot':
                self.cut = len(self.steps)
            if mode in ('opponent_blocked', 'enemy_self_target', 'symmetric_damage', 'ability_blocked', 'removed', 'snapshot'):
                self.next_turn()
            if mode == 'opponent_blocked':
                self.check_probe(False)
            elif mode in ('removed', 'snapshot'):
                self.provide(1, 'removal', 'probe')
                if mode == 'snapshot':
                    self.emit('can_cast', ref='probe', value=0)
                self.remove_source()
                self.check_probe(True, funded=True)
            elif mode in ('own_target', 'enemy_self_target'):
                player = int(mode == 'enemy_self_target')
                self.provide(player, 'bolt')
                self.stage_cast('bolt', player_target=player)
                self.finish('bolt', 'graveyard')
                self.life[player] -= 3
            elif mode == 'symmetric_damage':
                self.provide(1, 'sweep')
                self.stage_cast('sweep')
                self.finish('sweep', 'graveyard')
                self.life = [total-4 for total in self.life]
            elif mode == 'ability_blocked':
                self.target_ability(False)
            elif mode == 'no_teammates':
                for player in (0, 1):
                    self.emit('assert_zone_multiplicity', player=player, zone='battlefield', index=self.csvid, value=int(player == 0))

    def run(self):
        self.play()
        self.receipts()
        if self.cut is not None:
            tail = self.steps[self.cut:]
            self.steps[self.cut:self.cut] = [self.step('capture')]
            self.steps += [self.step('record_state'), self.step('ai_roundtrip'), self.step('restore')]
            self.steps += copy.deepcopy(tail) + [self.step('assert_recorded_state')]
        variant = f'{self.mode}_{self.owner}'
        limitations = {
            'team': ['The duel runtime has only two opposing seats; the positive teammate-copy branch needs a team format.'],
            'opening': ['The real Rules Engine presents its opening-hand choice at first human upkeep. A distinct pregame timing observation is unavailable.'],
        }
        result = dict(name=f'PlayerHexproofRecipe_{self.csvid}_{variant}', card=self.name, fixture='empty_main', seed=123, setup=self.setup,
                      steps=self.steps, _recipe=dict(family=FAMILY, version=2, variant=variant, params=copy.deepcopy(self.d['params']),
                                                    oracle_text=self.d['oracle_text'], untested_components=limitations.get(self.p['branch'], [])))
        if self.owner:
            _mirror(result)
        return result


def expand(name, csvid, declaration, params, selected, rows, oracle):
    validate_witnesses(oracle, rows)
    if not selected or len(selected) != len(set(selected)) or not set(selected) <= set(variants(params)):
        raise ValueError('unique approved player-hexproof variants required')
    if rows[name]['csvid'] != csvid:
        raise ValueError('player-hexproof source identity changed')
    return [HexproofBoard(name, csvid, declaration, params, variant, rows, oracle).run() for variant in selected]
