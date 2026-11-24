"""Resolve optional attack taps through declaration and actual blocking."""

import copy
import hashlib
import json
from pathlib import Path
import re

from block_stat_trigger_recipes import BlockStatBoard
from combat_value_trigger_recipes import _mirror

FAMILY = 'attack_tap_creatures'
PROFILE_PATH = Path(__file__).with_name('data') / 'attack_tap_creatures_wave8_profiles.json'
CLAUSE = 'Whenever this creature attacks, you may tap target creature.'
MODES = ('foe_tap', 'own_target', 'self_target', 'already_tapped', 'decline_trigger', 'decline_target',
         'one_of_two', 'target_type', 'tapped_denied', 'fresh_cast_denied', 'other_attacks', 'repeat_turns',
         'two_sources', 'snapshot')
FLYING_MODES = ('flying_ground', 'flying_reach', 'block_flyer')
LIBRARY = ('Forest', 'Island', 'Plains', 'Mountain')


def profile():
    return json.loads(PROFILE_PATH.read_text(encoding='utf8'))


def parameters(record):
    if record.get('oracle_text') == BRUISER_CLAUSE:
        return bruiser_parameters(record)
    text = record.get('oracle_text')
    flying = text == 'Flying\n' + CLAUSE
    mana = record.get('mana_cost', '')
    if (text not in (CLAUSE, 'Flying\n' + CLAUSE) or record.get('layout') != 'normal'
            or record.get('selected_face') or record.get('card_faces') or not record.get('type_line', '').startswith('Creature — ')
            or record.get('colors') != ['W'] or not re.fullmatch(r'\{[1-6]\}\{W\}(?:\{W\})?', mana)
            or record.get('keywords') != (['Flying'] if flying else [])):
        raise ValueError('whole pinned optional attack tap and ordinary printed creature required')
    return dict(mana=mana, flying=flying, target_type='creature', target_controller='any', optional=True,
                power=int(record['power']), toughness=int(record['toughness']))


def variants(params):
    modes = BRUISER_MODES if params.get('tap_limit') == 2 else MODES + (FLYING_MODES if params['flying'] else ())
    return [f'{mode}_{owner}' for owner in (0, 1) for mode in modes if not (owner and mode == 'decline_trigger')]


def spec(declaration, record):
    params = parameters(record)
    pin = profile()['sources'].get(record['name'])
    if (declaration.get('family') != FAMILY or type(declaration.get('version')) is not int or declaration['version'] != 2
            or not pin or pin['oracle'] != record or pin['params'] != params or declaration.get('oracle_text') != record['oracle_text']
            or json.dumps(declaration.get('params'), sort_keys=True) != json.dumps(params, sort_keys=True)):
        raise ValueError('exact printed optional tap-trigger data required')
    selected = declaration.get('variants')
    if not isinstance(selected, list) or not selected or any(type(mode) is not str for mode in selected):
        raise ValueError('explicit reviewed optional attack tap variants required')
    if len(set(selected)) != len(selected) or not set(selected) <= set(variants(params)):
        raise ValueError('distinct attack tap variants required')
    return dict(params, family=FAMILY)


def validate_witnesses(oracle, rows):
    data = profile()
    pending = {item['csvid'] for item in json.loads((PROFILE_PATH.parent / 'unimplemented.json').read_text(encoding='utf8'))['cases'].values()}
    static = set(data['static_characteristics_only_witnesses'])
    for name, pin in (data['witnesses'] | data['sources']).items():
        row = rows.get(name, {})
        if oracle.get(name) != pin['oracle'] or row != pin['registry'] or row.get('kind') != 'card':
            raise ValueError('attack tap source or witness changed: ' + name)
        if name not in static and (row['csvid'] in pending or 'dummy' in row['handler']):
            raise ValueError('attack tap action witness must be implemented: ' + name)
    root = Path(__file__).resolve().parents[4]
    for name, pin in data['sources'].items():
        source = pin['compiled_source']
        if hashlib.sha256((root / source['path']).read_bytes()).hexdigest() != source['sha256']:
            raise ValueError('attack tap compiled source changed: ' + name)


class AttackTapBoard(BlockStatBoard):
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
            for zone, cards in zones.items():
                self.setup.extend(self.card(name, f'stock_{zone}_{player}_{index}', player, zone) for index, name in enumerate(cards))
        self.add(name, 'source', 0, 'hand' if self.mode == 'fresh_cast_denied' else 'battlefield',
                 state=['tapped'] if self.mode == 'tapped_denied' else None)
        self.witnesses()

    def witnesses(self):
        mode = self.mode
        if mode == 'block_flyer':
            self.add('Welkin Tern', 'flyer', 1)
            return
        self.add('Hill Giant' if mode == 'flying_ground' else 'Giant Spider', 'foe', 1,
                 state=['tapped'] if mode == 'already_tapped' else None)
        if mode in ('own_target', 'flying_ground', 'flying_reach', 'target_type'):
            self.add('Iron Myr', 'own', 0)
        if mode == 'target_type':
            self.add('Forest', 'land', 0)
            self.add('Darksteel Relic', 'noncreature_artifact', 1)
        if mode == 'other_attacks':
            self.add('Crater Elemental', 'other', 0)
        if mode in ('one_of_two', 'two_sources'):
            self.add('Giant Spider', 'second_foe', 1)
        if mode == 'two_sources':
            self.add(self.name, 'second_source', 0)

    def attack_group(self, sources, permitted, targets):
        self.emit('begin_attackers')
        for ref in sources:
            self.emit('attack', ref=ref, value=int(permitted))
            if permitted:
                self.entities[ref]['tapped'] = True
        cut = len(self.steps) if self.mode == 'snapshot' else None
        for trigger, target in targets:
            self.emit('assert_card', ref=target, property='tapped', value=int(self.entities[target]['tapped']))
            if self.owner == 0:
                self.emit('choose_trigger_source', player=0, ref=trigger)
            self.emit('choose_optional_target' if self.mode == 'decline_target' else 'choose_target', player=0, ref=target)
        self.emit('finish_attackers')
        self.emit('assert_unused_choices', value=0)
        if self.mode == 'decline_target':
            self.emit('assert_forced_target_choices', value=0)
        for _, target in targets:
            if self.mode != 'decline_target':
                self.entities[target]['tapped'] = True
            self.emit('assert_card', ref=target, property='tapped', value=int(self.entities[target]['tapped']))
        return cut

    def damage(self, attackers, blocks, attacker=0):
        self.emit('begin_blockers')
        legal_blocks = {}
        for blocker, target, legal in blocks:
            self.emit('block', ref=blocker, other=target, value=int(legal))
            self.emit('assert_card', ref=blocker, property='blocking_count', value=int(legal))
            if legal:
                legal_blocks[target] = blocker
        self.emit('advance_combat')
        self.emit('combat_damage')
        self.emit('resolve')
        self.emit('end_combat')
        for ref in attackers:
            creature = self.entities[ref]
            if ref not in legal_blocks:
                self.life[1-attacker] -= creature['power']
                continue
            blocker = legal_blocks[ref]
            defender = self.entities[blocker]
            creature['damage'] += defender['power']
            defender['damage'] += creature['power']
            for target, card in ((ref, creature), (blocker, defender)):
                if card['damage'] >= card['toughness']:
                    self.move(target, 'graveyard')

    def main_combat(self):
        mode = self.mode
        if mode == 'block_flyer':
            self.attack_group(['flyer'], True, [])
            self.damage(['flyer'], [('source', 'flyer', True)], attacker=1)
            return None
        if mode == 'other_attacks':
            self.attack_group(['other'], True, [])
            self.emit('assert_card', ref='foe', property='tapped', value=0)
            self.damage(['other'], [])
            return None
        if mode in ('tapped_denied', 'fresh_cast_denied'):
            self.attack_group(['source'], False, [])
            self.emit('assert_card', ref='foe', property='tapped', value=0)
            return None
        if mode == 'target_type':
            for ref in ('land', 'noncreature_artifact', 'own', 'foe'):
                self.emit('can_target_creature', ref='source', other=ref, value=int(ref in ('own', 'foe')))
        sources = ['source'] + (['second_source'] if mode == 'two_sources' else [])
        recipient = 'own' if mode in ('own_target', 'target_type', 'flying_ground', 'flying_reach') else 'source' if mode == 'self_target' else 'foe'
        targets = [] if mode == 'decline_trigger' else [('source', recipient)]
        if mode == 'two_sources':
            targets.append(('second_source', 'second_foe'))
        cut = self.attack_group(sources, True, targets)
        foe_legal = not self.entities['foe']['tapped'] and mode != 'flying_ground'
        blocks = [('foe', 'source', foe_legal)]
        if mode == 'one_of_two':
            blocks += [('second_foe', 'source', True)]
        if mode == 'two_sources':
            blocks += [('second_foe', 'second_source', False)]
        self.damage(sources, blocks)
        return cut

    def untap_for_next_attack(self):
        self.emit('advance_turn')
        self.emit('untap_step', player=1)
        self.entities['foe']['tapped'] = False
        self.emit('assert_card', ref='foe', property='tapped', value=0)
        self.emit('advance_turn')
        self.emit('untap_step', player=0)
        self.entities['source']['tapped'] = False
        self.emit('assert_card', ref='source', property='tapped', value=0)
        self.emit('turn', player=0, value=0)

    def run(self):
        self.emit('turn', player=int(self.mode == 'block_flyer'), value=0)
        for player in (0, 1):
            pool = self.cost(self.oracle[self.name]) if player == 0 and self.mode == 'fresh_cast_denied' else [0]*6
            self.set_pool(player, pool)
        self.emit('refresh')
        if self.mode == 'fresh_cast_denied':
            self.paid('source')
            self.emit('assert_card', ref='source', property='sick', value=1)
        cut = self.main_combat()
        if self.mode == 'repeat_turns':
            self.receipts()
            self.untap_for_next_attack()
            self.main_combat()
        self.receipts()
        if cut is not None:
            tail = self.steps[cut:]
            self.steps[cut:cut] = [self.step('capture')]
            self.steps += [self.step('record_state'), self.step('ai_roundtrip'), self.step('restore')]
            self.steps += copy.deepcopy(tail) + [self.step('assert_recorded_state')]
        variant = f'{self.mode}_{self.owner}'
        result = dict(name=f'AttackTapCreaturesRecipe_{self.csvid}_{variant}', card=self.name, fixture='empty_main', seed=123,
                      setup=self.setup, steps=self.steps, _recipe=dict(family=FAMILY, version=2, variant=variant,
                        params=copy.deepcopy(self.d['params']), oracle_text=self.d['oracle_text'], untested_components=[
                            'The actual AI automatically schedules the outer optional trigger. Human pass and both seats’ real target cancellation exercise independent declines.']))
        if self.owner:
            _mirror(result)
        return result


def expand(name, csvid, declaration, params, selected, rows, oracle):
    validate_witnesses(oracle, rows)
    if not selected or len(set(selected)) != len(selected) or not set(selected) <= set(variants(params)):
        raise ValueError('unique approved attack tap variants required')
    if rows[name]['csvid'] != csvid:
        raise ValueError('attack tap source identity changed')
    board = BruiserBoard if params.get('tap_limit') == 2 else AttackTapBoard
    return [board(name, csvid, declaration, params, variant, rows, oracle).run() for variant in selected]


BRUISER_CLAUSE = 'Whenever this creature attacks, you may tap up to two target creatures.'
BRUISER_MODES = ('zero_targets', 'decline_trigger', 'one_legal', 'two_targets', 'mixed_targets',
                 'tapped_denied', 'fresh_cast_denied', 'snapshot')
CLOAK = "Alexi's Cloak"


def bruiser_parameters(record):
    if (record.get('name') != 'Cho-Arrim Bruiser' or record.get('type_line') != 'Creature — Ogre Rebel'
            or record.get('mana_cost') != '{5}{W}' or record.get('colors') != ['W'] or record.get('keywords') != []
            or record.get('layout') != 'normal' or record.get('card_faces') or record.get('selected_face')
            or (record.get('power'), record.get('toughness')) != ('3', '4')):
        raise ValueError('complete printed Cho-Arrim Bruiser required')
    return dict(mana='{5}{W}', flying=False, target_type='creature', target_controller='any', optional=True,
                power=3, toughness=4, tap_limit=2)


class BruiserBoard(AttackTapBoard):
    def witnesses(self):
        mode = self.mode
        self.add("Jolrael's Centaur" if mode == 'zero_targets' else 'Giant Spider', 'foe', 1)
        if mode in ('zero_targets', 'one_legal'):
            self.add(CLOAK, 'cloak', 0, 'hand')
        if mode in ('decline_trigger', 'two_targets', 'mixed_targets', 'snapshot'):
            self.add('Giant Spider', 'second_foe', 1)
        if mode == 'mixed_targets':
            self.add('Iron Myr', 'own', 0)

    def main_combat(self):
        mode = self.mode
        if mode in ('tapped_denied', 'fresh_cast_denied'):
            return super().main_combat()
        if mode in ('zero_targets', 'one_legal'):
            self.set_pool(0, self.cost(self.oracle[CLOAK]))
            self.emit('refresh')
            self.paid('cloak', target='source')
            self.emit('assert_card', ref='cloak', property='in_play', value=1)
            self.emit('can_target_creature', ref='source', other='source', value=0)
            self.emit('can_target_creature', ref='source', other='foe', value=int(mode == 'one_legal'))
        self.emit('begin_attackers')
        self.emit('attack', ref='source', value=1)
        self.entities['source']['tapped'] = True
        cut = len(self.steps) if mode == 'snapshot' else None
        targets = {
            'one_legal': ('foe',), 'two_targets': ('foe', 'second_foe'),
            'mixed_targets': ('own', 'foe'), 'snapshot': ('foe', 'second_foe'),
        }.get(mode, ())
        if self.owner == 0 and mode != 'decline_trigger':
            self.emit('choose_trigger_source', player=0, ref='source')
        # In one_legal, the real Aura removes the attacking Bruiser from
        # consideration. After selection marks the only other legal creature,
        # the actual picker has no second candidate and ends naturally.
        for target in targets:
            self.emit('choose_target', player=0, ref=target)
        self.emit('finish_attackers')
        self.emit('assert_unused_choices', value=0)
        for target in targets:
            self.entities[target]['tapped'] = True
        for ref in ('foe', 'second_foe', 'own'):
            if ref in self.entities:
                self.emit('assert_card', ref=ref, property='tapped', value=int(self.entities[ref]['tapped']))
        blocks = {
            'zero_targets': [('foe', 'source', True)],
            'decline_trigger': [('foe', 'source', True)],
            'one_legal': [('foe', 'source', False)],
            'two_targets': [('foe', 'source', False), ('second_foe', 'source', False)],
            'mixed_targets': [('foe', 'source', False), ('second_foe', 'source', True)],
            'snapshot': [('foe', 'source', False), ('second_foe', 'source', False)],
        }
        self.damage(['source'], blocks[mode])
        if 'cloak' in self.entities and self.entities['source']['zone'] != 'battlefield':
            self.move('cloak', 'graveyard')
        return cut

    def run(self):
        result = super().run()
        result['_recipe']['untested_components'] = [
            'The scripted target selection conflates Done and Cancel. A real shroud Aura supplies the independent one-eligible-target branch.',
            'Human pass and a shrouded battlefield exercise zero; the AI automatically schedules the trigger.']
        return result
