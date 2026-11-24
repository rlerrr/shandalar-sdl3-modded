"""Incoming damage prevention keyed to enchantment state, Walls, or blocks."""

import copy
import json
from pathlib import Path

from attack_value_trigger_recipes import _AttackResourceWorld


FAMILY = 'conditional_combat_prevention'
DEFENDER = "Defender (This creature can't attack.)"
ENCHANTED = 'Prevent all combat damage that would be dealt to this creature by enchanted creatures.'
WHITE = ('Protection from white\nPrevent all damage that would be dealt to this creature by enchanted creatures.')
BLOCKING = ("Prevent all damage that would be dealt to this creature by creatures it's blocking.\n"
            "This creature can't be the target of spells that can target only Walls or of abilities that can target only Walls.")
WALLS = ('All Walls able to block this creature do so.\n'
         'Prevent all combat damage that would be dealt to this creature by Walls.')


def profiles():
    path = Path(__file__).parent / 'data/conditional_combat_prevention_wave8_profiles.json'
    return json.loads(path.read_text(encoding='utf8'))


def classify(record):
    from recipes import cost_from_oracle

    if (not record or record.get('layout') != 'normal' or record.get('card_faces') or record.get('selected_face')
            or not record['type_line'].startswith(('Creature — ', 'Artifact Creature — '))
            or not record['power'].isdigit() or not record['toughness'].isdigit()):
        raise ValueError('whole normal creature with fixed printed stats required')
    text = record['oracle_text']
    clauses = {
        ENCHANTED: ('enchanted', True, False, False, False, False),
        DEFENDER + '\n' + WHITE: ('enchanted', False, True, True, False, False),
        DEFENDER + '\n' + BLOCKING: ('blocking', False, True, False, True, False),
        WALLS: ('wall', True, False, False, False, True),
    }
    if text not in clauses:
        raise ValueError('complete conditional prevention and companion clauses required')
    keys = ('condition', 'combat_only', 'defender', 'white_protection', 'wall_only_target_shield', 'mandatory_wall_block')
    return dict(zip(keys, clauses[text]), cost=cost_from_oracle(record))


def spec(declaration, record):
    if (not record or type(declaration.get('version')) is not int or declaration['version'] != 2
            or declaration.get('family') != FAMILY or declaration.get('oracle_text') != record['oracle_text']
            or 'variants' not in declaration):
        raise ValueError('typed version-two whole Oracle source required')
    expected = classify(record)
    params = declaration.get('params')
    if (type(params) is not dict or params != expected or any(type(params[k]) is not type(v) for k, v in expected.items())
            or any(type(n) is not int for n in params['cost'])):
        raise ValueError('exact Oracle prevention fields required')
    pin = profiles()['sources'].get(record['name'])
    if not pin or pin['oracle'] != record:
        raise ValueError('independent reserved source pin required')
    return dict(copy.deepcopy(params), family=FAMILY, effect=FAMILY, keywords=[])


def validate_witnesses(oracle, registry):
    data = profiles()
    for group in ('sources', 'witnesses'):
        for name, pin in data[group].items():
            row = registry.get(name)
            if (not row or row['handler'] != pin['handler'] or row['kind'] != 'card' or oracle.get(name) != pin['oracle']
                    or oracle[name]['layout'] != 'normal' or row['handler'] == 'NULL'
                    or row['handler'] == 'card_dummy' and not (group == 'witnesses' and pin.get('reviewed_native_witness'))):
                raise ValueError('whole Oracle and implementation pin changed: ' + name)


def variants(params):
    modes = ['cast', 'insufficient', 'source_attack', 'combat_match', 'combat_other', 'unblocked_side_damage',
             'spell_damage', 'blocked_snapshot']
    if any(params['cost'][1:]):
        modes.append('wrong_color')
    if params['condition'] == 'enchanted':
        modes.extend(['aura_removed', 'enchanted_ping', 'unenchanted_ping'])
    if params['white_protection']:
        modes.extend(['white_combat', 'white_aura_denied', 'black_aura_control'])
    if params['wall_only_target_shield']:
        modes.extend(['blocked_ping', 'unblocked_ping', 'wall_spell_denied', 'wall_spell_control',
                      'wall_ability_denied', 'wall_ability_control'])
    if params['mandatory_wall_block']:
        modes.extend(['multiple_walls', 'tapped_wall'])
    return [f'{mode}_{owner}' for owner in (0, 1) for mode in modes]


class PreventionWorld(_AttackResourceWorld):
    def __init__(self, name, params, owner, rows, oracle):
        super().__init__(name, params, owner, rows, oracle)
        self.enchanted = {}
        self.blocking = {}

    def observe(self, ref, prop, value):
        self.emit('assert_card', ref=ref, property=prop, value=int(value))

    def main(self, actor):
        self.current = actor
        self.emit('turn', player=actor, value=0)

    def fund(self, player, price):
        self.pool(player, price)
        self.emit('refresh')

    def move_spell(self, name, player, zone='graveyard'):
        self.zones['hand'][player].remove(name)
        self.zones[zone][player].append(name)

    def cast(self, name, ref, player, target=None, *, denied=None):
        from recipes import cost_from_oracle

        price = cost_from_oracle(self.o[name])
        if denied == 'insufficient':
            price = [0] * 6
        if denied == 'wrong_color':
            price = [sum(price)] + [0] * 5
        self.fund(player, price)
        if target:
            self.emit('choose_target', player=player, ref=target)
        self.emit('cast', ref=ref, value=int(denied is None))
        self.emit('assert_unused_choices', value=0)
        if denied:
            self.receipts()
            return
        self.emit('assert_stack', value=1)
        self.mana[player] = [0] * 6
        self.emit('resolve')

    def attach(self, name, ref, player, target):
        self.cast(name, ref, player, target)
        self.move_spell(name, player, 'battlefield')
        self.enchanted[target] = (name, ref, player)
        self.entities[target]['power'] += 2
        self.entities[target]['toughness'] += 1
        self.observe(ref, 'in_play', 1)
        self.receipts()

    def detach(self, target):
        name, ref, controller = self.enchanted.pop(target)
        self.cast('Disenchant', 'disenchant', 1-controller, ref)
        self.move_spell('Disenchant', 1-controller)
        self.zones['battlefield'][controller].remove(name)
        self.zones['graveyard'][controller].append(name)
        self.entities[target]['power'] -= 2
        self.entities[target]['toughness'] -= 1
        self.receipts()

    def receive(self, source_ref, recipient, amount, *, combat, spell_color=None):
        if recipient == 'source':
            hitter = self.entities.get(source_ref)
            if self.p['white_protection'] and (spell_color == 'W' or hitter and 'W' in self.o[hitter['name']]['colors']):
                return
            enabled = combat or not self.p['combat_only']
            match = {'enchanted': source_ref in self.enchanted, 'blocking': self.blocking.get('source') == source_ref,
                     'wall': bool(hitter and 'Wall' in self.o[hitter['name']]['type_line'].split(' — ')[-1].split())}
            if enabled and match[self.p['condition']]:
                return
        self.entities[recipient]['damage'] += amount

    def deaths(self):
        for ref, card in self.entities.items():
            if card['zone'] != 'battlefield' or card['damage'] < card['toughness']:
                continue
            self.leave(ref, 'graveyard')
            if ref in self.enchanted:
                name, _, owner = self.enchanted.pop(ref)
                self.zones['battlefield'][owner].remove(name)
                self.zones['graveyard'][owner].append(name)

    def attack(self, attackers):
        self.emit('begin_attackers')
        for ref in attackers:
            legal = ref != 'source' or not self.p['defender']
            self.emit('attack', ref=ref, value=int(legal))
            if legal:
                self.entities[ref]['attacking'] = self.entities[ref]['tapped'] = True
        self.emit('finish_attackers')

    def blockers(self, pairs, forced=()):
        for ref in forced:
            self.emit('choose_target', player=1-self.current, ref=ref)
        self.emit('begin_blockers')
        self.emit('assert_unused_choices', value=0)
        for ref in forced:
            self.blocking[ref] = 'source'
            self.observe(ref, 'blocking_count', 1)
        for blocker, attacker in pairs:
            self.emit('block', ref=blocker, other=attacker, value=1)
            self.blocking[blocker] = attacker
            self.observe(blocker, 'blocking_count', 1)
        self.emit('advance_combat')

    def damage(self, attackers):
        self.emit('combat_damage')
        self.emit('resolve')
        self.emit('end_combat')
        for ref in attackers:
            matches = [blocker for blocker, attacker in self.blocking.items() if attacker == ref]
            strength = self.entities[ref]['power']
            if not matches:
                self.life[1-self.current] -= strength
                continue
            blocker, = matches
            self.receive(ref, blocker, strength, combat=True)
            self.receive(blocker, ref, self.entities[blocker]['power'], combat=True)
        self.deaths()
        for card in self.entities.values():
            card['attacking'] = False
        self.receipts()

    def ping(self, target):
        self.emit('choose_target', player=1-self.owner, ref=target)
        self.emit('activate', ref='other', value=1)
        self.emit('assert_unused_choices', value=0)
        self.emit('assert_stack', value=1)
        self.emit('resolve')
        self.entities['other']['tapped'] = True
        self.receive('other', target, 1, combat=False)
        self.deaths()
        self.receipts()


def _scenario(world, mode):
    from recipes import cost_from_oracle

    owner, params = world.owner, world.p
    enemy = 1-owner
    cast_test = mode in ('cast', 'insufficient', 'wrong_color')
    target_test = mode.startswith('wall_') or mode in ('white_aura_denied', 'black_aura_control')
    marble = params['mandatory_wall_block']
    combat_mode = not (cast_test or target_test or mode in ('spell_damage', 'enchanted_ping', 'unenchanted_ping', 'unblocked_ping'))
    actor = owner if cast_test or marble or mode in ('source_attack', 'black_aura_control') else enemy
    world.add(world.name, 'source', owner, 'hand' if cast_test else 'battlefield', source=True)

    walls = mode in ('combat_match', 'unblocked_side_damage', 'blocked_snapshot', 'tapped_wall', 'multiple_walls') and marble
    enchanted = params['condition'] == 'enchanted' and mode in ('combat_match', 'unblocked_side_damage', 'blocked_snapshot',
                                                                'enchanted_ping', 'aura_removed')
    other_name = 'Wall of Heat' if walls else 'Pillarfield Ox' if mode == 'white_combat' else 'Hill Giant'
    if enchanted:
        other_name = 'Grizzly Bears'
    if mode in ('blocked_ping', 'unblocked_ping', 'enchanted_ping', 'unenchanted_ping'):
        other_name = 'Prodigal Sorcerer'
    if combat_mode and mode != 'source_attack' or mode in ('unblocked_ping', 'enchanted_ping', 'unenchanted_ping'):
        world.add(other_name, 'other', enemy)
        if mode == 'tapped_wall':
            world.setup[-1]['state'] = ['tapped']
            world.entities['other']['tapped'] = True
    if mode == 'multiple_walls':
        world.add('Wall of Wood', 'extra_wall', enemy)
    if mode == 'unblocked_side_damage':
        world.add('Grizzly Bears', 'side', enemy if not marble else owner)
    if mode == 'combat_other' and params['condition'] == 'blocking':
        world.add('Grizzly Bears', 'side_blocker', owner)
    if enchanted or mode == 'black_aura_control':
        world.add('Unholy Strength', 'aura', owner if mode == 'black_aura_control' else enemy, 'hand')
    if mode == 'aura_removed':
        world.add('Disenchant', 'disenchant', owner, 'hand')
    if mode == 'white_aura_denied':
        world.add('Holy Strength', 'white_aura', enemy, 'hand')
    if mode == 'blocked_ping':
        world.add('Vitalize', 'vitalize', enemy, 'hand')
    if mode == 'spell_damage':
        world.add('Lightning Bolt', 'bolt', enemy, 'hand')
    if mode.startswith('wall_spell'):
        world.add('Tunnel', 'tunnel', enemy, 'hand')
    if mode in ('wall_spell_control', 'wall_ability_denied', 'wall_ability_control'):
        world.add('Wall of Wood', 'eligible_wall', owner)
    if mode.startswith('wall_ability'):
        world.add('Dwarven Demolition Team', 'demolition', enemy)
    for player in (0, 1):
        world.add('Island', f'library_{player}', player, 'library')
        world.add('Mountain', f'grave_{player}', player, 'graveyard')

    world.main(actor)
    for player in (0, 1):
        world.pool(player, [0] * 6)
    world.emit('refresh')
    if cast_test:
        denied = None if mode == 'cast' else mode
        world.cast(world.name, 'source', owner, denied=denied)
        if denied is None:
            world.arrive('source')
            world.receipts()
        return None
    if enchanted:
        world.attach('Unholy Strength', 'aura', enemy, 'other')
        if mode == 'aura_removed':
            world.detach('other')
    if target_test:
        if mode in ('wall_spell_denied', 'white_aura_denied'):
            name, ref = ('Tunnel', 'tunnel') if mode == 'wall_spell_denied' else ('Holy Strength', 'white_aura')
            world.fund(enemy, cost_from_oracle(world.o[name]))
            world.emit('can_cast', ref=ref, value=0)
        elif mode == 'wall_spell_control':
            world.cast('Tunnel', 'tunnel', enemy, 'eligible_wall')
            world.move_spell('Tunnel', enemy)
            world.leave('eligible_wall', 'graveyard')
        elif mode == 'wall_ability_denied':
            world.emit('probe_activation_target', ref='demolition', other='source', index=-1, value=0)
            world.emit('probe_activation_target', ref='demolition', other='eligible_wall', index=-1, value=1)
        elif mode == 'wall_ability_control':
            world.emit('choose_target', player=enemy, ref='eligible_wall')
            world.emit('activate', ref='demolition', value=1)
            world.emit('assert_unused_choices', value=0)
            world.emit('assert_stack', value=1)
            world.emit('resolve')
            world.entities['demolition']['tapped'] = True
            world.leave('eligible_wall', 'graveyard')
        else:
            world.attach('Unholy Strength', 'aura', owner, 'source')
        world.receipts()
        return None
    if mode == 'spell_damage':
        world.cast('Lightning Bolt', 'bolt', enemy, 'source')
        world.move_spell('Lightning Bolt', enemy)
        world.receive('bolt', 'source', 3, combat=False, spell_color='R')
        world.deaths()
        world.receipts()
        return None
    if not combat_mode:
        world.ping('source')
        return None

    attackers = ['source' if marble or mode == 'source_attack' else 'other']
    if mode == 'unblocked_side_damage':
        attackers.append('side')
    world.attack(attackers)
    if mode == 'source_attack' and params['defender']:
        world.receipts()
        return None
    cut = len(world.steps)
    forced = ()
    pairs = []
    if mode == 'multiple_walls':
        forced = ('other', 'extra_wall')
    elif mode != 'source_attack' and mode != 'tapped_wall':
        if walls:
            forced = ('other',)
        elif marble:
            pairs = [('other', 'source')]
        elif 'side_blocker' in world.entities:
            pairs = [('side_blocker', 'other')]
        else:
            pairs = [('source', 'other')]
    world.blockers(pairs, forced)
    if mode == 'multiple_walls':
        # Both blocks are already real. Leave damage-order decisions open.
        world.receipts()
        return None
    if mode == 'blocked_ping':
        world.cast('Vitalize', 'vitalize', enemy)
        world.move_spell('Vitalize', enemy)
        world.entities['other']['tapped'] = False
        world.ping('source')
    world.damage(attackers)
    return cut


def expand(name, csvid, declaration, params, selected, rows, oracle):
    from recipes import step

    if not selected or len(set(selected)) != len(selected) or any(variant not in variants(params) for variant in selected):
        raise ValueError('explicit distinct prevention variants required')
    validate_witnesses(oracle, rows)
    cases = []
    for variant in selected:
        mode, owner = variant.rsplit('_', 1)
        world = PreventionWorld(name, params, int(owner), rows, oracle)
        cut = _scenario(world, mode)
        steps = world.steps
        if mode == 'blocked_snapshot':
            assert cut is not None
            tail = steps[cut:]
            steps = (steps[:cut] + [step('capture')] + copy.deepcopy(tail)
                     + [step('record_state'), step('ai_roundtrip'), step('restore')]
                     + copy.deepcopy(tail) + [step('assert_recorded_state')])
        cases.append(dict(name=f'ConditionalCombatPreventionRecipe_{csvid}_{variant}', card=name, fixture='empty_main',
                          seed=123, setup=world.setup, steps=steps, _recipe=dict(
                              family=FAMILY, version=2, variant=variant, params=declaration['params'],
                              oracle_text=declaration['oracle_text'],
                              untested_components=['multiplayer, damage reassignment and power-based allocation across multiple blockers',
                                                   'colored Equipment, bypass prevention effects and planeswalkers'])))
    return cases
