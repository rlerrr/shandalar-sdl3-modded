"""Chosen-color protection observed through real spell and combat actions."""

import copy
import json
from pathlib import Path

from attack_value_trigger_recipes import _AttackResourceWorld


FAMILY = 'chosen_color_protection'
GRANT_OWN = 'Target creature you control gains protection from the color of your choice until end of turn.'
GRANT_ANY = 'Target creature gains protection from the color of your choice until end of turn.'
ENTRY = 'When this creature enters, target creature you control gains protection from the color of your choice until end of turn.'
WHITE_TEAM = 'Choose a color. White creatures you control gain protection from the chosen color until end of turn.'
REMINDER = " (It can't be blocked, targeted, dealt damage, enchanted, or equipped by anything of that color.)"
COLORS = {'black': 1, 'blue': 2, 'green': 3, 'red': 4, 'white': 5}
COLOR_NAMES = dict(zip('BUGRW', COLORS))
BLOCKER = {'black': 'Scathe Zombies', 'blue': 'Air Elemental', 'green': 'Craw Wurm',
           'red': 'Hill Giant', 'white': 'Yoked Ox'}


def profiles():
    path = Path(__file__).parent / 'data/chosen_color_protection_wave8_profiles.json'
    return json.loads(path.read_text(encoding='utf8'))


def classify(record):
    from recipes import cost_from_oracle

    if (not record or record.get('layout') != 'normal' or record.get('card_faces')
            or record.get('selected_face') or record['type_line'].startswith('Token ')):
        raise ValueError('whole normal source required')
    text = record['oracle_text']
    kind = record['type_line']
    if kind == 'Instant':
        scope = {GRANT_ANY: 'any', GRANT_OWN: 'own', WHITE_TEAM: 'white_team',
                 GRANT_OWN + REMINDER + '\nScry 1.': 'own'}.get(text)
        if not scope:
            raise ValueError('all printed instant clauses must fit the protection model')
        return dict(action='instant', scope=scope, scry=int(text.endswith('\nScry 1.')), cost=cost_from_oracle(record))
    if kind.startswith('Creature — ') and record['power'].isdigit() and record['toughness'].isdigit():
        action = {'{T}: ' + GRANT_OWN: 'tap', 'Sacrifice this creature: ' + GRANT_OWN: 'sacrifice',
                  ENTRY: 'entry'}.get(text)
        if action:
            return dict(action=action, scope='own', scry=0, cost=cost_from_oracle(record))
    raise ValueError('all printed protection source clauses must fit the family')


def spec(declaration, record):
    if (not record or declaration.get('family') != FAMILY or type(declaration.get('version')) is not int
            or declaration['version'] != 2 or declaration.get('oracle_text') != record['oracle_text']
            or 'variants' not in declaration):
        raise ValueError('typed version-two whole Oracle declaration required')
    expected = classify(record)
    params = declaration.get('params')
    if (type(params) is not dict or params != expected or type(params['scry']) is not int
            or type(params['cost']) is not list or any(type(n) is not int for n in params['cost'])):
        raise ValueError('exact typed full Oracle parameters required')
    pin = profiles()['sources'].get(record['name'])
    if not pin or pin['oracle'] != record:
        raise ValueError('independent released Oracle source pin required')
    return dict(copy.deepcopy(params), family=FAMILY, effect=FAMILY, keywords=[])


def validate_witnesses(oracle, registry):
    for group, pins in profiles().items():
        if group == 'version':
            continue
        for name, pin in pins.items():
            row = registry.get(name)
            if (not row or row['handler'] != pin['handler'] or row['kind'] != 'card' or oracle.get(name) != pin['oracle']
                    or oracle[name]['layout'] != 'normal' or row['handler'] == 'NULL'
                    or row['handler'] == 'card_dummy' and not (group == 'witnesses' and pin.get('reviewed_native_witness'))):
                raise ValueError('whole normal Oracle and binding witness changed: ' + name)


def variants(params):
    action = params['action']
    modes = ['effect', 'without_effect', 'probe_red', 'probe_blue', 'aura_drop', 'after_turn', 'pending_snapshot',
             'wrong_cast_color', 'insufficient_cast_mana', 'block_other']
    modes.extend(f'block_{color}' for color in COLORS)
    if action != 'entry':
        modes.append('defend_red')
    if action in ('instant', 'tap', 'sacrifice'):
        modes.append('bolt_response')
    if params['scope'] != 'white_team' and action != 'entry':
        modes.append('target_lost_pending')
    if params['scope'] == 'any':
        modes.append('foreign_target')
    if params['scope'] == 'white_team':
        modes.append('no_white_creatures')
    if action == 'instant' and params['scope'] == 'own':
        modes.append('no_own_target')
    if action in ('entry', 'tap', 'sacrifice'):
        modes.append('self_target')
    if action in ('tap', 'sacrifice'):
        modes.extend(['cast_permanent', 'foreign_target_probe', 'fresh_source'])
    if action == 'tap':
        modes.extend(['tapped_source', 'source_lost_pending'])
    items = [f'{mode}_{owner}' for owner in (0, 1) for mode in modes]
    if params['scry']:
        items.append('scry_bottom_0')
    return items


class ProtectionWorld(_AttackResourceWorld):
    def __init__(self, name, params, owner, rows, oracle):
        super().__init__(name, params, owner, rows, oracle)
        self.protections = {}
        self.auras = {}

    def move(self, name, player, old, new):
        self.zones[old][player].remove(name)
        self.zones[new][player].append(name)

    def observe(self, ref, prop, value):
        self.emit('assert_card', ref=ref, property=prop, value=int(value))

    def main(self, player):
        self.current = player
        self.emit('turn', player=player, value=0)

    def pay(self, player, cost):
        balance = [available-due for available, due in zip(self.mana[player], cost)]
        if min(balance) < 0:
            raise ValueError('complete payment required')
        self.mana[player] = balance

    def fund(self, player, cost):
        self.pool(player, cost)
        self.emit('refresh')

    def put_spell(self, name, ref, player, target=None, previous_stack=0):
        from recipes import cost_from_oracle

        cost = cost_from_oracle(self.o[name])
        self.fund(player, cost)
        if target:
            self.emit('choose_target', player=player, ref=target)
        self.emit('cast', ref=ref, value=1)
        self.emit('assert_unused_choices', value=0)
        self.emit('assert_stack', value=1+previous_stack)
        self.pay(player, cost)

    def enter_source(self):
        self.put_spell(self.name, 'source', self.owner)
        self.emit('resolve')
        self.arrive('source')
        self.receipts()

    def cast_aura(self, target):
        self.put_spell('Pacifism', 'aura', self.owner, target)
        self.emit('resolve')
        self.move('Pacifism', self.owner, 'hand', 'battlefield')
        self.auras['aura'] = target
        self.observe('aura', 'in_play', 1)
        self.receipts()

    def colors(self, ref):
        return self.o[self.entities[ref]['name']]['colors']

    def grant(self, target, color):
        if self.p['scope'] == 'white_team':
            refs = [ref for ref, card in self.entities.items()
                    if card['zone'] == 'battlefield' and card['player'] == self.owner and 'W' in self.colors(ref)]
        else:
            refs = [target]
        for ref in refs:
            self.protections[ref] = color
        for aura, ref in list(self.auras.items()):
            if self.protections.get(ref) == 'white':
                self.move('Pacifism', self.owner, 'battlefield', 'graveyard')
                del self.auras[aura]

    def hit(self, target, amount, color):
        if self.protections.get(target) != color:
            self.entities[target]['damage'] += amount

    def deaths(self):
        for ref, card in self.entities.items():
            if card['zone'] == 'battlefield' and card['damage'] >= card['toughness']:
                self.leave(ref, 'graveyard')

    def pyroclasm(self, ref='sweep'):
        self.put_spell('Pyroclasm', ref, self.current)
        self.emit('resolve')
        self.move('Pyroclasm', self.current, 'hand', 'graveyard')
        for ref, card in self.entities.items():
            if card['zone'] == 'battlefield':
                self.hit(ref, 2, 'red')
        self.deaths()
        self.receipts()

    def combat(self, attacker, blocker, block_color):
        self.emit('begin_attackers')
        self.emit('attack', ref=attacker, value=1)
        self.entities[attacker]['tapped'] = True
        self.emit('finish_attackers')
        self.emit('begin_blockers')
        legal = self.protections.get(attacker) != block_color
        self.emit('block', ref=blocker, other=attacker, value=int(legal))
        if legal:
            self.observe(blocker, 'blocking_count', 1)
        self.emit('advance_combat')
        self.emit('combat_damage')
        self.emit('resolve')
        self.emit('end_combat')
        attacking, defending = self.entities[attacker], self.entities[blocker]
        if legal:
            self.hit(attacker, defending['power'], block_color)
            reverse = COLOR_NAMES[self.colors(attacker)[0]]
            self.hit(blocker, attacking['power'], reverse)
        else:
            self.life[1-self.current] -= attacking['power']
        self.deaths()
        self.receipts()

    def pass_turn(self):
        if any(any(pool) for pool in self.mana):
            raise ValueError('real untap requires actually spent pools')
        self.emit('advance_turn')
        self.current = 1 - self.current
        self.protections.clear()
        for card in self.entities.values():
            card['damage'] = 0
            if card['player'] == self.current and card['zone'] == 'battlefield':
                card['tapped'] = card['sick'] = False
        self.emit('untap_step', player=self.current)
        self.main(self.current)


def _apply(world, target, color, mode, previous_stack=0):
    owner, params = world.owner, world.p
    action = params['action']
    if action in ('instant', 'entry'):
        cast_target = target if action == 'instant' and params['scope'] != 'white_team' else None
        world.put_spell(world.name, 'source', owner, cast_target, previous_stack)
    else:
        world.emit('choose_target', player=owner, ref=target)
        world.emit('activate', ref='source', value=1)
        world.emit('assert_unused_choices', value=0)
        world.emit('assert_stack', value=1+previous_stack)
        if action == 'sacrifice':
            world.leave('source', 'graveyard')
            world.emit('assert_zone_multiplicity', player=owner, zone='graveyard',
                       index=world.rows[world.name]['csvid'], value=1)
        else:
            world.entities['source']['tapped'] = True
            world.observe('source', 'tapped', 1)
    cut = len(world.steps)
    if mode in ('target_lost_pending', 'source_lost_pending'):
        removed = target if mode == 'target_lost_pending' else 'source'
        world.emit('destroy', ref=removed)
        world.leave(removed, 'graveyard')
    active = mode != 'target_lost_pending' and not (action == 'sacrifice' and target == 'source')
    if active:
        if action == 'entry':
            world.emit('choose_target', player=owner, ref=target)
        world.emit('choose_color', player=owner, value=COLORS[color])
        if params['scry']:
            world.emit('record_library', player=owner)
            if owner == 0:
                if mode == 'scry_bottom':
                    top = world.zones['library'][owner][0]
                    world.emit('choose_zone_card', player=owner, value=world.rows[top]['csvid'])
                else:
                    world.emit('choose_zone', player=owner, value=-1)
    world.emit('resolve')
    world.emit('assert_unused_choices', value=0)
    if action == 'entry':
        world.arrive('source')
    if action == 'instant':
        world.move(world.name, owner, 'hand', 'graveyard')
    if active:
        world.grant(target, color)
        if params['scry']:
            world.emit('assert_scry_result', player=owner, value=1)
            if mode == 'scry_bottom':
                library = world.zones['library'][owner]
                library.append(library.pop(0))
    if previous_stack:
        world.move('Lightning Bolt', 1-owner, 'hand', 'graveyard')
        world.hit(target, 3, 'red')
        world.deaths()
    world.receipts()
    return cut


def _scenario(world, mode):
    from recipes import card

    owner, params = world.owner, world.p
    enemy = 1-owner
    action = params['action']
    denied_cast = mode in ('wrong_cast_color', 'insufficient_cast_mana', 'no_own_target')
    forced_cast = mode in ('cast_permanent', 'fresh_source')
    source_zone = 'hand' if action in ('instant', 'entry') or forced_cast or denied_cast else 'battlefield'
    world.add(world.name, 'source', owner, source_zone, source=True)
    if mode == 'tapped_source':
        world.setup[-1]['state'] = ['tapped']
        world.entities['source']['tapped'] = True
    if mode not in ('no_own_target', 'self_target'):
        recipient = enemy if mode == 'foreign_target' else owner
        target_name = 'Giant Spider' if mode == 'no_white_creatures' else 'Pillarfield Ox'
        world.add(target_name, 'selected', recipient)
    if params['scope'] == 'white_team':
        if mode != 'no_white_creatures':
            world.add('Yoked Ox', 'other_white', owner)
            world.add('Giant Spider', 'nonwhite', owner)
        world.add('Pillarfield Ox', 'foreign_white', enemy)
    if mode in ('foreign_target_probe', 'no_own_target'):
        world.add('Pillarfield Ox', 'foreign', enemy)
    if mode in ('probe_red', 'probe_blue'):
        world.add('Prodigal Pyromancer', 'red_pinger', enemy)
        world.add('Prodigal Sorcerer', 'blue_pinger', enemy)

    block_case = mode.startswith('block_') or mode == 'defend_red'
    block_color = 'red' if mode in ('defend_red', 'block_other') else mode.removeprefix('block_')
    if block_case:
        world.add(BLOCKER[block_color], 'combatant', enemy)
    red_damage = mode in ('effect', 'without_effect', 'foreign_target', 'after_turn', 'pending_snapshot',
                          'no_white_creatures', 'self_target', 'source_lost_pending', 'fresh_source')
    if red_damage:
        world.add('Pyroclasm', 'sweep', owner, 'hand')
    if mode == 'after_turn':
        world.add('Pyroclasm', 'sweep_later', owner, 'hand')
    if mode == 'bolt_response':
        world.add('Lightning Bolt', 'bolt', enemy, 'hand')
    if mode == 'aura_drop':
        world.add('Pacifism', 'aura', owner, 'hand')
    if params['scry'] and owner == 1:
        # The current engine leaves the top card in place with four or five
        # available lands. They are tapped so they cannot fund failed casts.
        for index in range(4):
            land = card('Swamp', f'scry_board_land_{index}', owner, 'battlefield')
            land['state'] = ['tapped']
            world.setup.append(land)
            world.zones['battlefield'][owner].append('Swamp')
    for player in (0, 1):
        for index, name in enumerate(('Island', 'Plains', 'Mountain')):
            world.add(name, f'library_{player}_{index}', player, 'library')

    world.main(enemy if mode == 'defend_red' else owner)
    for player in (0, 1):
        world.pool(player, [0] * 6)
    world.emit('refresh')
    if denied_cast:
        cost = [0] * 6
        if mode == 'wrong_cast_color':
            cost[0] = sum(params['cost'])
        elif mode == 'no_own_target':
            cost = params['cost']
        world.fund(owner, cost)
        world.emit('cast', ref='source', value=0)
        world.emit('assert_unused_choices', value=0)
        world.receipts()
        return None
    if forced_cast:
        world.enter_source()
        if mode == 'cast_permanent':
            return None
    if mode in ('tapped_source', 'foreign_target_probe') or mode == 'fresh_source' and action == 'tap':
        if mode == 'foreign_target_probe':
            world.emit('probe_activation_target', ref='source', other='foreign', index=-1, value=0)
        else:
            world.emit('activate', ref='source', value=0)
        world.emit('assert_unused_choices', value=0)
        if red_damage:
            world.pyroclasm()
        else:
            world.receipts()
        return None
    if mode == 'aura_drop':
        world.cast_aura('selected')
    previous_stack = int(mode == 'bolt_response')
    if previous_stack:
        world.put_spell('Lightning Bolt', 'bolt', enemy, 'selected')

    target = 'source' if mode == 'self_target' else 'selected'
    color = ('white' if mode == 'aura_drop' else 'blue' if mode in ('probe_blue', 'block_other')
             else block_color if block_case else 'red')
    cut = None if mode == 'without_effect' else _apply(world, target, color, mode, previous_stack)
    if mode in ('probe_red', 'probe_blue'):
        for test_color, ref in (('red', 'red_pinger'), ('blue', 'blue_pinger')):
            world.emit('probe_activation_target', ref=ref, other='selected', index=-1, value=int(color != test_color))
        world.receipts()
    if red_damage:
        world.pyroclasm()
        if mode == 'after_turn':
            world.pass_turn()
            world.pass_turn()
            world.pyroclasm('sweep_later')
    if block_case:
        if mode == 'defend_red':
            world.combat('combatant', 'selected', 'white')
        else:
            world.combat('selected', 'combatant', block_color)
    return cut


def expand(name, csvid, declaration, params, selected, rows, oracle):
    from recipes import step

    if not selected or len(set(selected)) != len(selected) or any(variant not in variants(params) for variant in selected):
        raise ValueError('explicit distinct protection variants required')
    validate_witnesses(oracle, rows)
    cases = []
    for variant in selected:
        mode, owner = variant.rsplit('_', 1)
        world = ProtectionWorld(name, params, int(owner), rows, oracle)
        cut = _scenario(world, mode)
        steps = world.steps
        if mode == 'pending_snapshot':
            assert cut is not None
            tail = steps[cut:]
            steps = (steps[:cut] + [step('capture')] + copy.deepcopy(tail)
                     + [step('record_state'), step('ai_roundtrip'), step('restore')]
                     + copy.deepcopy(tail) + [step('assert_recorded_state')])
        cases.append(dict(
            name=f'ChosenColorProtectionRecipe_{csvid}_{variant}', card=name, fixture='empty_main', seed=123,
            setup=world.setup, steps=steps, _recipe=dict(
                family=FAMILY, version=2, variant=variant, params=declaration['params'], oracle_text=declaration['oracle_text'],
                untested_components=['multiplayer, colored Equipment and multicolor source combinations',
                                     'effects that override prevention, repeated protection colors and damage redirection',
                                     'optional AI Scry strategy outside the controlled board'])))
    return cases
