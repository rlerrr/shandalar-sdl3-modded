"""Laccolith Oracle behavior at actual attack, block, and post-block boundaries."""

import copy
import json
from pathlib import Path

from attack_value_trigger_recipes import _AttackResourceWorld

FAMILY = 'laccolith_creature'
PROFILE = Path(__file__).with_name('data') / 'laccolith_creature_wave8_profiles.json'
RULE = ('Whenever this creature becomes blocked, you may have it deal damage equal to its power to target creature. '
        'If you do, this creature assigns no combat damage this turn.')
HANDLER = 'manalinkeh_card_laccolith_grunt'
WALL = 'Wall of Stone'
VICTIM = 'Fortified Rampart'
GOBLIN = "Mons's Goblin Raiders"
MANTLE = "Farrel's Mantle"
SCARAB = 'Rust Scarab'
WITNESSES = {WALL, VICTIM, GOBLIN, 'Giant Growth', 'Ashcoat Bear', 'Unsummon', 'Grizzly Bears', 'Counterspell',
             'Wall of Denial', MANTLE, SCARAB, 'Icy Manipulator', 'Giant Spider'}
COMMON = ('take', 'own_target', 'kill_blocker', 'blocker_returns_damage', 'boost_before_block', 'legal_target',
          'late_flash_target', 'blocker_bounced', 'next_turn_restore', 'pending_block_snapshot', 'unblocked',
          'source_blocks', 'countered', 'wrong_color', 'insufficient_mana')
BRIDGES = ('declare_blockers_bridge', 'damage_option_bridge')
GAPS = ('response to an individually pending becomes-blocked trigger',
        'multiple blockers and combat allocation when the implementation misses suppression')


def profiles():
    return json.loads(PROFILE.read_text(encoding='utf8'))


def classify(record):
    from recipes import cost_from_oracle

    if (not record or record.get('layout') != 'normal' or record.get('selected_face') or record.get('card_faces')
            or not record.get('type_line', '').startswith('Creature — ') or record.get('keywords') != []
            or record.get('oracle_text') != RULE):
        raise ValueError('whole standalone Laccolith printed clause required')
    mana = cost_from_oracle(record)
    fixed = [record.get(prop) for prop in ('power', 'toughness')]
    if any(not isinstance(value, str) or not value.isascii() or not value.isdecimal() for value in fixed):
        raise ValueError('only fixed Laccolith power and toughness are supported')
    power, toughness = map(int, fixed)
    return dict(mana=mana, power=power, toughness=toughness, keywords=[])


def variants(params):
    modes = COMMON + (BRIDGES if params['power'] == 2 else ())
    return [f'{mode}_{owner}' for owner in (0, 1) for mode in modes + (('decline',) if owner == 0 else ())]


def check_variants(params, selected):
    if (type(selected) is not list or not selected or any(type(item) is not str for item in selected)
            or len(set(selected)) != len(selected) or not set(selected) <= set(variants(params))):
        raise ValueError('distinct reviewed Laccolith variants required')


def spec(declaration, record):
    expected = classify(record)
    pin = profiles()['sources'].get(record['name'])
    if (declaration.get('family') != FAMILY or type(declaration.get('version')) is not int or declaration['version'] != 2
            or not pin or pin['oracle'] != record or declaration.get('oracle_text') != RULE
            or json.dumps(expected, sort_keys=True) != json.dumps(declaration.get('params'), sort_keys=True)):
        raise ValueError('whole Laccolith Oracle and exact typed parameters required')
    check_variants(expected, declaration.get('variants'))
    return dict(expected, family=FAMILY, effect=FAMILY)


def validate_witnesses(oracle, registry):
    data = profiles()
    skipped = {row['csvid'] for row in json.loads((PROFILE.parent / 'unimplemented.json').read_text(
        encoding='utf8'))['cases'].values()}
    if data.get('version') != 1 or set(data['witnesses']) != WITNESSES or set(data['sources']) & WITNESSES:
        raise ValueError('reviewed Laccolith profile and witness set changed')
    for group in ('sources', 'witnesses'):
        for name, pin in data[group].items():
            row = registry.get(name, {})
            if (pin['oracle'] != oracle.get(name) or pin['registry'] != row or row.get('kind') != 'card'
                    or row['csvid'] in skipped or row['handler'] == 'NULL'
                    or (row['handler'] == 'card_dummy' and not pin.get('reviewed_native_characteristics'))):
                raise ValueError(f'changed Laccolith Oracle or binding: {name}')
            if group == 'sources':
                classify(oracle[name])
                if row['handler'] != HANDLER:
                    raise ValueError(f'unreviewed Laccolith handler: {name}')


class LaccolithWorld(_AttackResourceWorld):
    def __init__(self, name, params, owner, rows, oracle):
        super().__init__(name, params, owner, rows, oracle)
        self.foe = 1-owner
        self.cards = {}

    def add(self, name, ref, player=None, zone='hand', source=False):
        player = self.owner if player is None else player
        super().add(name, ref, player, zone, source)
        self.cards[ref] = (name, player)

    def observe(self, ref, prop, value):
        self.emit('assert_card', ref=ref, property=prop, value=int(value))

    def receipts(self, *, pending_damage=None):
        self.emit('refresh')
        start = len(self.steps)
        super().receipts()
        if pending_damage is not None:
            index, = [i for i in range(start, len(self.steps))
                      if self.steps[i].get('ref') == pending_damage and self.steps[i].get('property') == 'damage']
            del self.steps[index]

    def paid_cast(self, ref, *, denial=None, response=False, target=None):
        from recipes import cost_from_oracle

        name, player = self.cards[ref]
        cost = cost_from_oracle(self.o[name])
        if denial:
            cost = [sum(cost) if denial == 'wrong_color' else 0] + [0] * 5
        self.pool(player, cost)
        self.emit('refresh')
        if target is not None:
            self.emit('choose_target', ref=target, player=player)
        self.emit('cast_response' if response else 'cast', ref=ref, value=int(denial is None))
        self.emit('assert_choices_consumed')
        self.emit('assert_stack', value=0 if denial else 2 if response else 1)
        if not denial:
            self.mana[player] = [0] * 6
        for color, amount in enumerate(self.mana[player]):
            self.emit('assert_mana', player=player, color=color, value=amount)
        self.observe(ref, 'in_play', 0)
        self.observe(ref, 'in_hand', bool(denial))

    def paid_entry(self, ref):
        self.paid_cast(ref)
        self.emit('resolve')
        self.arrive(ref)
        self.receipts()

    def countered_source(self):
        self.paid_cast('source')
        self.emit('announce_spell', ref='source')
        self.paid_cast('counter', response=True)
        self.emit('resolve_response')
        self.leave('source', 'graveyard')
        self.zones['hand'][self.foe].remove('Counterspell')
        self.zones['graveyard'][self.foe].append('Counterspell')
        self.receipts()

    def declare_attack(self, ref):
        self.emit('begin_attackers')
        self.emit('attack', ref=ref, value=1)
        self.entities[ref].update(attacking=True, tapped=True)
        self.emit('finish_attackers')

    def declare_block(self, blocker=None, attacker='source'):
        self.emit('begin_blockers')
        if blocker is not None:
            self.emit('block', ref=blocker, other=attacker, value=1)
            self.observe(blocker, 'blocking_count', 1)

    def complete_block(self, attacker, blocked):
        self.emit('advance_combat')
        self.emit('resolve')
        self.emit('assert_unused_choices', value=0)
        self.observe(attacker, 'blocked', blocked)

    def damage(self, ref, amount):
        target = self.entities[ref]
        if target['zone'] == 'battlefield':
            target['damage'] += amount
            if target['damage'] >= target['toughness']:
                self.leave(ref, 'graveyard')

    def fight(self, attacker, blocker=None, suppress=False):
        self.emit('combat_damage')
        self.emit('resolve')
        self.emit('end_combat')
        attack = self.entities[attacker]
        if attack['zone'] == 'battlefield':
            if blocker is None:
                if not suppress:
                    self.life[1-self.current] -= attack['power']
            elif self.entities[blocker]['zone'] == 'battlefield':
                opponent = self.entities[blocker]
                outgoing = 0 if suppress else attack['power']
                self.damage(attacker, opponent['power'])
                self.damage(blocker, outgoing)
        attack['attacking'] = False
        self.receipts()

    def no_manual_activation(self):
        self.emit('can_activate', ref='source', value=0)

    def targetable(self, ref, legal):
        self.emit('can_target_creature', ref='source', other=ref, value=int(legal))

    def choose_damage(self, ref, take):
        # The targeted trigger requires its target when put on the stack.
        # The separate optional effect is chosen on resolution (CR 603.5).
        self.emit('choose_target', ref=ref, player=self.owner)
        if self.owner == 0:
            self.emit('choose_dialog', player=self.owner, value=0 if take else 1)

    def mantle_entry(self):
        self.paid_cast('mantle', target='driver')
        self.emit('resolve')
        self.zones['hand'][self.owner].remove(MANTLE)
        self.zones['battlefield'][self.owner].append(MANTLE)
        self.receipts()


def setup(world, mode):
    owner, foe = world.owner, world.foe
    world.add(world.name, 'source', source=True)
    if mode in ('wrong_color','insufficient_mana','countered','unblocked'):
        if mode == 'countered':
            world.add('Counterspell','counter',foe)
        return
    if mode in BRIDGES:
        world.add(SCARAB if mode == 'declare_blockers_bridge' else 'Grizzly Bears','driver',owner,'battlefield')
        if mode == 'declare_blockers_bridge':
            world.add(WALL,'blocker',foe,'battlefield')
            world.add('Icy Manipulator','artifact',foe,'battlefield')
        else:
            world.add(MANTLE,'mantle')
            world.add(VICTIM,'victim',foe,'battlefield')
        return
    if mode == 'source_blocks':
        world.add('Grizzly Bears','opposing',foe,'battlefield')
        return
    block_name = {'kill_blocker':GOBLIN, 'blocker_returns_damage':'Giant Spider'}.get(mode, WALL)
    world.add(block_name,'blocker',foe,'battlefield')
    if mode != 'kill_blocker':
        world.add(VICTIM,'victim',owner if mode == 'own_target' else foe,'battlefield')
    if mode == 'boost_before_block':
        world.add('Giant Growth','growth')
    if mode == 'legal_target':
        world.add('Wall of Denial','protected',foe,'battlefield')
    if mode == 'late_flash_target':
        world.add('Ashcoat Bear','late',foe)
    if mode == 'blocker_bounced':
        world.add('Unsummon','bounce',foe)


def run_bridge(world, mode):
    mantle = mode == 'damage_option_bridge'
    if mantle:
        world.mantle_entry()
    world.declare_attack('driver')
    world.declare_block(None if mantle else 'blocker', 'driver')
    if mantle:
        # This pinned legacy callback exposes the Damage/Pass dialog before
        # target selection. The primary Oracle trigger still targets first.
        world.emit('choose_dialog', player=world.owner, value=0)
    world.emit('choose_target', player=world.owner, ref='victim' if mantle else 'artifact')
    world.complete_block('driver', not mantle)
    if mantle:
        world.damage('victim', world.entities['driver']['power'] + 2)
    else:
        world.zones['battlefield'][world.foe].remove('Icy Manipulator')
        world.zones['graveyard'][world.foe].append('Icy Manipulator')
    # Mantle's real damage packet is still pending at this test boundary. The
    # completed combat step retains the target's exact damage-mark receipt.
    world.receipts(pending_damage='victim' if mantle else None)
    world.fight('driver', None if mantle else 'blocker', suppress=mantle)


def scenario(world, mode):
    setup(world, mode)
    world.emit('turn', player=world.owner, value=0)
    world.emit('present_ai_dialogs', value=1)
    for player in (0, 1):
        world.pool(player, [0] * 6)
    world.receipts()
    if mode in ('wrong_color','insufficient_mana'):
        world.paid_cast('source', denial=mode)
        world.receipts()
        return None
    if mode == 'countered':
        world.countered_source()
        return None
    world.paid_entry('source')
    if mode in BRIDGES:
        run_bridge(world, mode)
        return None
    if mode == 'source_blocks':
        world.next_turn()
        world.declare_attack('opposing')
        world.declare_block('source','opposing')
        world.complete_block('opposing', True)
        world.no_manual_activation()
        world.fight('opposing','source')
        return None
    world.age()
    world.no_manual_activation()
    world.declare_attack('source')
    if mode == 'unblocked':
        world.declare_block()
        world.complete_block('source', False)
        world.no_manual_activation()
        world.fight('source')
        return None
    if mode == 'boost_before_block':
        world.spell('Giant Growth','growth',world.owner,'source')
        world.entities['source']['power'] += 3
        world.entities['source']['toughness'] += 3
        world.receipts()
    world.declare_block('blocker')
    target = 'blocker' if mode == 'kill_blocker' else 'victim'
    if mode == 'legal_target':
        world.targetable('protected', False)
        world.targetable('victim', True)
    if mode == 'late_flash_target':
        world.observe('late','in_hand', True)
        world.targetable('late', False)
    take = mode != 'decline'
    world.choose_damage(target, take)
    cut = len(world.steps) if mode == 'pending_block_snapshot' else None
    world.complete_block('source', True)
    world.no_manual_activation()
    if take:
        world.damage(target, world.entities['source']['power'])
    world.receipts()
    if mode == 'late_flash_target':
        world.paid_entry('late')
        world.targetable('late', True)
        world.no_manual_activation()
    if mode == 'blocker_bounced':
        world.spell('Unsummon','bounce',world.foe,'blocker')
        world.leave('blocker','hand')
        world.observe('source','blocked', True)
        world.no_manual_activation()
        world.receipts()
    world.fight('source','blocker',suppress=take)
    if mode == 'next_turn_restore':
        world.age()
        world.declare_attack('source')
        world.declare_block()
        world.complete_block('source', False)
        world.fight('source')
    return cut


def expand(name, csvid, declaration, params, selected, registry, oracle):
    from recipes import step

    check_variants(params, selected)
    validate_witnesses(oracle, registry)
    if profiles()['sources'][name]['registry']['csvid'] != csvid:
        raise ValueError('Laccolith source CSVID changed')
    cases = []
    for variant in selected:
        mode, owner = variant.rsplit('_', 1)
        world = LaccolithWorld(name, params, int(owner), registry, oracle)
        cut = scenario(world, mode)
        actions = world.steps
        if cut is not None:
            tail = actions[cut:]
            actions = (actions[:cut] + [step('capture')] + tail + [step('record_state'), step('ai_roundtrip'), step('restore')]
                       + copy.deepcopy(tail) + [step('assert_recorded_state')])
        cases.append(dict(name=f'LaccolithCreatureRecipe_{csvid}_{variant}', card=name, fixture='empty_main', seed=331,
                          setup=world.setup, steps=actions, _recipe=dict(family=FAMILY, version=2, variant=variant,
                              params=declaration['params'], oracle_text=RULE, untested_components=list(GAPS))))
    return cases
