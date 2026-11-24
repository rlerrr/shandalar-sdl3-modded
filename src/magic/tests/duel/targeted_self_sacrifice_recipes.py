"""Whole-Oracle self-sacrifice upon becoming a spell or ability target."""

import copy
import hashlib
import json
from pathlib import Path

from block_stat_trigger_recipes import BlockStatBoard
from combat_value_trigger_recipes import _mirror
from grant_recipes import LIBRARY

FAMILY = 'targeted_self_sacrifice'
PROFILE_PATH = Path(__file__).with_name('data') / 'targeted_self_sacrifice_wave8_profiles.json'
TRIGGER = 'When this creature becomes the target of a spell or ability, sacrifice it.'


def profiles():
    return json.loads(PROFILE_PATH.read_text(encoding='utf8'))


def parameters(record):
    text = record.get('oracle_text', '')
    keywords = []
    for keyword in ('Flying', 'Defender'):
        if text == f'{keyword}\n{TRIGGER}':
            keywords = [keyword]
            break
    expected_text = '\n'.join([*keywords, TRIGGER])
    if (text != expected_text or record.get('keywords') != keywords or record.get('layout') != 'normal'
            or record.get('card_faces') or record.get('selected_face') or not record.get('type_line', '').startswith('Creature — ')
            or any(not record.get(key, '').isascii() or not record[key].isdecimal() for key in ('power', 'toughness'))):
        raise ValueError('complete fixed-stat targeted-sacrifice creature required')
    from recipes import cost_from_oracle

    if not any(cost_from_oracle(record)[1:]):
        raise ValueError('the reviewed casting controls require a colored symbol')
    return dict(printed_mana=record['mana_cost'], keywords=keywords, target_events=['spell', 'ability'])


def variants(params):
    modes = ['cast_payment', 'cast_unaffordable', 'cast_wrong_color', 'cast_countered', 'fresh_attack',
             'own_bounce', 'enemy_bounce', 'own_pump', 'other_spell_target', 'countered_targeting_spell',
             'spell_before_announcement_snapshot', 'spell_after_announcement_snapshot', 'untargeted_anthem',
             'indestructible_sacrifice', 'own_ability', 'enemy_ability', 'other_ability_target', 'ability_targets_player',
             'tapping_ability', 'ability_pending_snapshot', 'ground_combat', 'defend_ground']
    if 'Flying' in params['keywords']:
        modes += ['flying_blocker', 'reach_blocker']
    return [f'{mode}_{owner}' for owner in (0, 1) for mode in modes] + ['cancel_target_0']


def validate_variants(params, selected):
    if (not isinstance(selected, list) or not selected or any(type(v) is not str for v in selected)
            or len(selected) != len(set(selected)) or not set(selected) <= set(variants(params))):
        raise ValueError('distinct reviewed targeted-sacrifice variants required')


def spec(declaration, record):
    params = parameters(record)
    pin = profiles()['sources'].get(record['name'])
    if (declaration.get('family') != FAMILY or type(declaration.get('version')) is not int or declaration['version'] != 2
            or not pin or pin['oracle'] != record or pin['params'] != params or declaration.get('oracle_text') != record['oracle_text']
            or json.dumps(declaration.get('params'), sort_keys=True) != json.dumps(params, sort_keys=True)):
        raise ValueError('complete pinned Oracle and typed targeted-sacrifice parameters required')
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
            raise ValueError('targeted-sacrifice witness changed: ' + name)
    for name, pin in data['sources'].items():
        row = rows.get(name, {})
        source = pin['compiled_source']
        if (oracle.get(name) != pin['oracle'] or row != pin['registry'] or row.get('csvid') in unimplemented
                or row.get('kind') != 'card' or row.get('handler') != 'manalinkeh_card_skulking_ghost'
                or hashlib.sha256((root / source['path']).read_bytes()).hexdigest() != source['sha256']):
            raise ValueError('implemented targeted-sacrifice source changed: ' + name)


SPELLS = {
    'own_bounce': 'Unsummon', 'enemy_bounce': 'Unsummon', 'own_pump': 'Giant Growth', 'other_spell_target': 'Giant Growth',
    'countered_targeting_spell': 'Unsummon', 'spell_before_announcement_snapshot': 'Unsummon',
    'spell_after_announcement_snapshot': 'Unsummon', 'untargeted_anthem': "Gaea's Anthem",
    'indestructible_sacrifice': 'Unsummon', 'cancel_target': 'Unsummon',
}
ABILITY_MODES = {'own_ability', 'enemy_ability', 'other_ability_target', 'ability_targets_player', 'tapping_ability', 'ability_pending_snapshot'}


class TargetedBoard(BlockStatBoard):
    def __init__(self, name, csvid, declaration, params, variant, rows, oracle):
        from recipes import card, cost_from_oracle, observe, step

        self.card, self.step, self.observe, self.cost = card, step, observe, cost_from_oracle
        self.name, self.csvid, self.d, self.p, self.rows, self.oracle = name, csvid, declaration, params, rows, oracle
        self.mode, owner = variant.rsplit('_', 1)
        self.owner = int(owner)
        self.steps, self.setup, self.entities, self.base = [], [], {}, {}
        self.tokens, self.bound = [], set()
        self.aura_target = None
        self.boundary = self.discard_nondeterministic = False
        self.life = [20, 20]
        self.pools = [[10] * 6, [10] * 6]
        self.mana = [60, 60]
        self.cut = None
        mode = self.mode
        self.actor = int(mode in ('enemy_bounce', 'enemy_ability', 'defend_ground'))
        self.zones = []
        for player in (0, 1):
            zones = dict(hand=['Forest', 'Plains'], graveyard=['Island'], library=list(LIBRARY), exile=['Mountain'])
            self.zones.append(zones)
            for zone, names in zones.items():
                for index, stock in enumerate(names):
                    self.setup.append(card(stock, f'stock_{zone}_{player}_{index}', player, zone))

        casting = mode.startswith('cast_') or mode == 'fresh_attack'
        self.add(name, 'source', 0, 'hand' if casting else 'battlefield')
        if mode in SPELLS:
            self.add(SPELLS[mode], 'spell', self.actor, 'hand')
        if mode in ('cast_countered', 'countered_targeting_spell'):
            self.add('Counterspell', 'counter', 1, 'hand')
        if mode in ABILITY_MODES:
            provider = 'Icy Manipulator' if mode == 'tapping_ability' else 'Spikeshot Elder'
            self.add(provider, 'provider', self.actor)
        if mode in ('other_spell_target', 'other_ability_target', 'untargeted_anthem', 'indestructible_sacrifice'):
            self.add('Grizzly Bears', 'decoy', int(mode in ('other_ability_target', 'indestructible_sacrifice')))
        if mode in ('ground_combat', 'defend_ground', 'flying_blocker', 'reach_blocker'):
            opponent = {'flying_blocker': 'Air Elemental', 'reach_blocker': 'Giant Spider'}.get(mode, 'Grizzly Bears')
            self.add(opponent, 'opponent', 1)
        if mode == 'indestructible_sacrifice':
            self.add('Avacyn, Angel of Hope', 'shield')
            self.add('Day of Judgment', 'destruction', 0, 'hand')

        self.emit('turn', player=self.actor, value=0)
        self.emit('present_ai_dialogs', value=1)
        if mode == 'cast_payment':
            self.pools[0] = self.cost(oracle[name])
        elif mode == 'cast_unaffordable':
            self.pools[0] = [0] * 6
        elif mode == 'cast_wrong_color':
            self.pools[0] = [60, 0, 0, 0, 0, 0]
        for player in (0, 1):
            self.set_pool(player, self.pools[player])
        self.emit('refresh')

    def pending_spell_receipts(self, player, spell):
        self.emit('assert_stack', value=1)
        self.emit('assert_card', ref=spell, property='in_hand', value=0)
        self.emit('assert_mana_total', player=player, value=self.mana[player])
        for color, value in enumerate(self.pools[player]):
            self.emit('assert_mana', player=player, color=color, value=value)

    def source_receipt(self):
        self.emit('assert_card', ref='source', property='in_play', value=int(self.entities['source']['zone'] == 'battlefield'))
        self.emit('assert_zone_multiplicity', player=0, zone='graveyard', index=self.csvid,
                  value=self.zones[0]['graveyard'].count(self.name))
        self.emit('assert_zone_multiplicity', player=0, zone='hand', index=self.csvid,
                  value=self.zones[0]['hand'].count(self.name))

    def counter_pending(self):
        self.emit('cast_response', ref='counter', value=1)
        self.spend(self.oracle['Counterspell']['mana_cost'], 1)
        self.emit('assert_stack', value=2)
        self.emit('assert_card', ref='counter', property='in_hand', value=0)
        self.emit('assert_mana_total', player=1, value=self.mana[1])
        self.emit('resolve_response')
        self.move('counter', 'graveyard')

    def cast_source(self):
        mode = self.mode
        if mode in ('cast_unaffordable', 'cast_wrong_color'):
            self.emit('cast', ref='source', value=0)
            self.emit('assert_card', ref='source', property='in_hand', value=1)
            return
        self.emit('cast', ref='source', value=1)
        self.spend(self.p['printed_mana'])
        self.pending_spell_receipts(0, 'source')
        if mode == 'cast_countered':
            self.emit('announce_spell', ref='source')
            self.counter_pending()
            self.move('source', 'graveyard')
        else:
            self.emit('resolve')
            self.move('source', 'battlefield')
            self.emit('assert_card', ref='source', property='sick', value=1)
        if mode == 'fresh_attack':
            self.combat()

    def spell_action(self):
        mode = self.mode
        if mode == 'untargeted_anthem':
            self.paid('spell')
            for ref in ('source', 'decoy'):
                self.entities[ref]['boost'] += 1
            return
        if mode == 'indestructible_sacrifice':
            self.paid('destruction')
            self.move('decoy', 'graveyard')
            self.receipts()
        if mode == 'cancel_target':
            self.emit('choose_cancel_target', player=0)
            self.emit('cast', ref='spell', value=0)
            self.emit('assert_card', ref='spell', property='in_hand', value=1)
            return
        target = 'decoy' if mode == 'other_spell_target' else 'source'
        self.emit('choose_target', player=self.actor, ref=target)
        self.emit('cast', ref='spell', value=1)
        self.spend(self.oracle[SPELLS[mode]]['mana_cost'], self.actor)
        self.pending_spell_receipts(self.actor, 'spell')
        # The call above stages and pays the spell. The next real engine call
        # dispatches and completes its cast trigger while the spell is pending.
        self.source_receipt()
        if mode == 'spell_before_announcement_snapshot':
            self.cut = len(self.steps)
        self.emit('announce_spell', ref='spell')
        if target == 'source':
            self.move('source', 'graveyard')
        self.source_receipt()
        self.pending_spell_receipts(self.actor, 'spell')
        if mode == 'spell_after_announcement_snapshot':
            self.cut = len(self.steps)
        if mode == 'countered_targeting_spell':
            self.counter_pending()
        else:
            self.emit('resolve')
            if target == 'decoy':
                self.entities['decoy']['boost'] += 3
        self.move('spell', 'graveyard')

    def ability_action(self):
        mode = self.mode
        provider = self.entities['provider']
        self.emit('can_activate', ref='provider', value=1)
        self.source_receipt()
        if mode == 'ability_targets_player':
            self.emit('choose_player', player=self.actor, value=1 - self.actor)
        else:
            target = 'decoy' if mode == 'other_ability_target' else 'source'
            self.emit('choose_target', player=self.actor, ref=target)
        self.emit('activate', ref='provider', value=1)
        taps = mode == 'tapping_ability'
        self.spend('{1}' if taps else '{1}{R}{R}', self.actor)
        provider['tapped'] = taps
        self.emit('assert_card', ref='provider', property='tapped', value=int(taps))
        self.emit('assert_stack', value=1)
        self.emit('assert_mana_total', player=self.actor, value=self.mana[self.actor])
        for color, value in enumerate(self.pools[self.actor]):
            self.emit('assert_mana', player=self.actor, color=color, value=value)
        if mode not in ('other_ability_target', 'ability_targets_player'):
            self.move('source', 'graveyard')
        self.source_receipt()
        if mode == 'ability_pending_snapshot':
            self.cut = len(self.steps)
        self.emit('resolve')
        if mode == 'other_ability_target':
            self.entities['decoy']['damage'] += 1
        elif mode == 'ability_targets_player':
            self.life[1 - self.actor] -= 1

    def combat(self):
        defender = self.mode == 'defend_ground'
        attacker, blocker = ('opponent', 'source') if defender else ('source', 'opponent')
        flying = 'Flying' in self.p['keywords']
        legal_attack = defender or ('Defender' not in self.p['keywords'] and self.mode != 'fresh_attack')
        self.emit('begin_attackers')
        self.emit('attack', ref=attacker, value=int(legal_attack))
        if legal_attack:
            self.entities[attacker]['tapped'] = True
        self.emit('finish_attackers')
        self.emit('begin_blockers')
        if legal_attack:
            legal_block = defender or not flying or self.mode in ('flying_blocker', 'reach_blocker')
            self.emit('block', ref=blocker, other=attacker, value=int(legal_block))
            self.emit('assert_card', ref=blocker, property='blocking_count', value=int(legal_block))
        self.emit('advance_combat')
        if legal_attack:
            self.emit('assert_card', ref=attacker, property='blocked', value=int(legal_block))
        self.receipts()
        if legal_attack and legal_block:
            attacking_power, blocking_power = self.power(attacker), self.power(blocker)
            for ref, damage in ((attacker, blocking_power), (blocker, attacking_power)):
                entity = self.entities[ref]
                entity['damage'] += damage
                if entity['damage'] >= entity['toughness']:
                    self.move(ref, 'graveyard')
        elif legal_attack:
            self.life[1 - self.actor] -= self.power(attacker)
        self.emit('combat_damage')
        self.emit('resolve')
        self.receipts()
        self.emit('end_combat')

    def run(self):
        if self.mode.startswith('cast_') or self.mode == 'fresh_attack':
            self.cast_source()
        elif self.mode in SPELLS:
            self.spell_action()
        elif self.mode in ABILITY_MODES:
            self.ability_action()
        else:
            self.combat()
        self.receipts()
        steps = self.steps
        if self.cut is not None:
            replay = steps[self.cut:]
            steps = steps[:self.cut] + [self.step('capture')] + replay
            steps += [self.step('record_state'), self.step('ai_roundtrip'), self.step('restore')]
            steps += copy.deepcopy(replay) + [self.step('assert_recorded_state')]
        case = dict(name=f'TargetedSelfSacrificeRecipe_{self.csvid}_{self.mode}_{self.owner}', card=self.name,
                    fixture='empty_main', seed=123, setup=self.setup, steps=steps,
                    _recipe=dict(family=FAMILY, version=2, variant=f'{self.mode}_{self.owner}',
                                 oracle_text=self.d['oracle_text'], params=copy.deepcopy(self.d['params']), untested_components=[
                                     'The engine settles its target trigger at real spell or ability announcement. Separate priority during the pending sacrifice trigger is outside these fixtures.',
                                     'Optional target-selection cancellation is human-only. Multiplayer and compound copy, bestow, and Embalm cards use other recipes.',
                                 ]))
        if self.owner:
            _mirror(case)
        return case


def expand(name, csvid, declaration, params, selected, rows, oracle):
    validate_witnesses(oracle, rows)
    validate_variants(params, selected)
    return [TargetedBoard(name, csvid, declaration, params, variant, rows, oracle).run() for variant in selected]
