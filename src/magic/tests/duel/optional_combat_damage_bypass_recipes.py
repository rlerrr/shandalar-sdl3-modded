"""Optional as-unblocked combat damage with live declarations and assignments."""

import copy
import json
from pathlib import Path

from attack_value_trigger_recipes import _AttackResourceWorld


FAMILY = 'optional_combat_damage_bypass'
PROFILE_PATH = Path(__file__).with_name('data') / 'optional_combat_damage_bypass_wave8_profiles.json'
RULE = "You may have this creature assign its combat damage as though it weren't blocked."
HANDLER = 'manalinkeh_card_thorn_elemental'
MODES = ('cast_then_attack', 'insufficient_mana', 'wrong_color', 'bypass', 'normal', 'unblocked',
         'defending', 'source_idle', 'mixed_attackers', 'first_strike', 'bypass_removed_blocker',
         'normal_removed_blocker', 'choice_snapshot', 'rechoose', 'noncombat_when_chosen')
WITNESSES = ('Craw Wurm', 'Giant Spider', 'Grizzly Bears', 'Pit Fight', 'Unsummon', 'Wall of Stone',
             'Wall of Wood', 'White Knight')


def profiles():
    return json.loads(PROFILE_PATH.read_text(encoding='utf8'))


def classify(record):
    from recipes import cost_from_oracle

    if (not record or record.get('layout') != 'normal' or record.get('card_faces') or record.get('selected_face')
            or not record.get('type_line', '').startswith('Creature — ') or record.get('keywords') != []
            or record.get('oracle_text') != RULE
            or any(not record.get(key, '').isascii() or not record[key].isdecimal() for key in ('power', 'toughness'))):
        raise ValueError('complete fixed-stat, single-clause combat bypass creature required')
    return dict(mana=cost_from_oracle(record), power=int(record['power']), toughness=int(record['toughness']))


def variants(params):
    return [f'{mode}_{owner}' for owner in (0, 1) for mode in MODES]


def spec(declaration, record):
    params = classify(record)
    pin = profiles()['sources'].get(record['name'])
    if (declaration.get('family') != FAMILY or type(declaration.get('version')) is not int or declaration['version'] != 2
            or declaration.get('oracle_text') != RULE or not pin or pin['oracle'] != record
            or json.dumps(declaration.get('params'), sort_keys=True) != json.dumps(params, sort_keys=True)):
        raise ValueError('pinned full Oracle and exact combat bypass parameters required')
    validate_variants(params, declaration.get('variants'))
    return dict(params, family=FAMILY, effect=FAMILY, keywords=[], entry_energy=0)


def validate_variants(params, selected):
    if (type(selected) is not list or not selected or any(type(v) is not str for v in selected)
            or len(set(selected)) != len(selected) or not set(selected) <= set(variants(params))):
        raise ValueError('distinct reviewed combat bypass variants required')


def validate_witnesses(oracle, registry):
    data = profiles()
    catalog = json.loads((PROFILE_PATH.parent / 'unimplemented.json').read_text(encoding='utf8'))
    unimplemented = {entry['csvid'] for entry in catalog['cases'].values()}
    if data.get('version') != 1 or set(data['witnesses']) != set(WITNESSES):
        raise ValueError('combat bypass witness catalog differs from the reviewed matrix')
    for group in ('sources', 'witnesses'):
        for name, pin in data[group].items():
            row = registry.get(name, {})
            if (oracle.get(name) != pin['oracle'] or row != pin['registry'] or row.get('kind') != 'card'
                    or row['csvid'] in unimplemented or row['handler'] == 'NULL'
                    or group == 'sources' and row['handler'] != HANDLER
                    or row['handler'] == 'card_dummy' and not pin.get('reviewed_native_characteristics')):
                raise ValueError(f'combat bypass implementation or pinned Oracle changed for {name}')


class BypassWorld(_AttackResourceWorld):
    def observe(self, ref, prop, value):
        self.emit('assert_card', ref=ref, property=prop, value=int(value))

    def declare(self, attackers, pairs, bypass=None):
        self.emit('begin_attackers')
        for ref in attackers:
            self.emit('attack', ref=ref, value=1)
            self.entities[ref].update(attacking=True, tapped=True)
            self.observe(ref, 'attacking', 1)
            self.observe(ref, 'tapped', 1)
        self.emit('finish_attackers')
        self.emit('begin_blockers')
        for blocker, attacker in pairs:
            self.emit('block', ref=blocker, other=attacker, value=1)
            self.observe(blocker, 'blocking_count', 1)
        if bypass is not None:
            self.emit('choose_dialog', player=self.owner, value=int(bypass))
        self.emit('advance_combat')
        self.emit('assert_unused_choices', value=0)
        for blocker, _ in pairs:
            self.observe(blocker, 'blocking_count', 1)
        for player in (0, 1):
            self.emit('assert_life', player=player, value=self.life[player])

    def hit(self, recipient, power):
        creature = self.entities[recipient]
        creature['damage'] += power
        if creature['damage'] >= creature['toughness']:
            self.leave(recipient, 'graveyard')

    def damage(self, attackers, pairs, bypass=False):
        # These fixtures use one ordinary blocker per attacker. For the removed
        # blocker cases keep the declaration even after its permanent leaves.
        blocked = {attacker: blocker for blocker, attacker in pairs}
        for attacker in attackers:
            creature = self.entities[attacker]
            ref = blocked.get(attacker)
            blocker = self.entities[ref] if ref else None
            alive = blocker is not None and blocker['zone'] == 'battlefield'
            first_strike = alive and self.has(ref, 'First strike')
            if first_strike:
                self.hit(attacker, blocker['power'])
            if creature['zone'] != 'battlefield':
                continue
            if ref is None or bypass and attacker == 'source':
                self.life[1 - self.current] -= creature['power']
            elif alive:
                self.hit(ref, creature['power'])
            if alive and not first_strike:
                self.hit(attacker, blocker['power'])
        self.emit('combat_damage')
        self.emit('resolve')
        self.emit('end_combat')
        for creature in self.entities.values():
            creature['attacking'] = False
        self.receipts()

    def pit_fight(self):
        from recipes import COLORS

        green = [0] * 6
        green[COLORS['G']] = 2  # Pay the generic and green hybrid portions with green mana.
        self.pool(self.owner, green)
        if self.owner == 0:
            # The human payment window spends the remaining green on the generic portion.
            self.emit('choose_mana', player=self.owner, color=COLORS['G'])
        self.emit('choose_target', player=self.owner, ref='source')
        self.emit('choose_target', player=self.owner, ref='blocker')
        self.emit('cast', ref='instant', value=1)
        self.emit('assert_unused_choices', value=0)
        self.emit('assert_stack', value=1)
        self.emit('resolve')
        self.mana[self.owner] = [0] * 6
        self.zones['hand'][self.owner].remove('Pit Fight')
        self.zones['graveyard'][self.owner].append('Pit Fight')
        self.hit('blocker', self.entities['source']['power'])
        self.hit('source', self.entities['blocker']['power'])
        self.receipts()


def scenario(world, mode):
    owner, opponent = world.owner, 1 - world.owner
    casting = mode in ('cast_then_attack', 'insufficient_mana', 'wrong_color')
    actor = opponent if mode == 'defending' else owner
    world.current = actor
    world.add(world.name, 'source', owner, 'hand' if casting else 'battlefield', source=True)
    attackers, pairs = ['source'], [('blocker', 'source')]
    if mode in ('insufficient_mana', 'wrong_color', 'unblocked', 'source_idle'):
        pairs = []
    elif mode == 'defending':
        pairs = [('source', 'ally')]
    else:
        blocker = {'bypass': 'Giant Spider', 'first_strike': 'White Knight', 'normal': 'Craw Wurm'}.get(mode, 'Wall of Stone')
        world.add(blocker, 'blocker', opponent)
    if mode in ('defending', 'source_idle', 'mixed_attackers'):
        world.add('Grizzly Bears', 'ally', actor)
        attackers = attackers + ['ally'] if mode == 'mixed_attackers' else ['ally']
    if mode == 'mixed_attackers':
        world.add('Wall of Wood', 'second_blocker', opponent)
        pairs.append(('second_blocker', 'ally'))
    if mode in ('bypass_removed_blocker', 'normal_removed_blocker', 'noncombat_when_chosen'):
        spell = 'Pit Fight' if mode == 'noncombat_when_chosen' else 'Unsummon'
        world.add(spell, 'instant', owner, 'hand')

    world.emit('turn', player=actor, value=0)
    world.emit('present_ai_dialogs', value=1)
    for player in (0, 1):
        world.pool(player, [0] * 6)
    world.emit('refresh')
    world.receipts()

    if casting:
        denial = mode if mode != 'cast_then_attack' else None
        world.cast_source('source', denial)
        if denial:
            return None
        world.emit('begin_attackers')
        world.emit('attack', ref='source', value=0)
        world.emit('finish_attackers')
        world.emit('begin_blockers')
        world.emit('advance_combat')
        world.damage([], [])
        world.age()

    choice = None if mode in ('defending', 'source_idle', 'unblocked') else mode not in ('normal', 'normal_removed_blocker')
    world.declare(attackers, pairs, choice)
    cut = len(world.steps) if mode == 'choice_snapshot' else None
    if mode in ('bypass_removed_blocker', 'normal_removed_blocker'):
        world.spell('Unsummon', 'instant', owner, 'blocker')
        world.leave('blocker', 'hand')
        world.receipts()
    if mode == 'noncombat_when_chosen':
        world.pit_fight()
    world.damage(attackers, pairs, choice)
    if mode == 'rechoose':
        world.age()
        world.declare(attackers, pairs, False)
        world.damage(attackers, pairs, False)
    return cut


def expand(name, csvid, declaration, params, selected, registry, oracle):
    from recipes import step

    validate_variants(params, selected)
    validate_witnesses(oracle, registry)
    if profiles()['sources'][name]['registry']['csvid'] != csvid:
        raise ValueError('combat bypass source CSVID changed')
    cases = []
    for variant in selected:
        mode, owner = variant.rsplit('_', 1)
        world = BypassWorld(name, params, int(owner), registry, oracle)
        cut = scenario(world, mode)
        steps = world.steps
        if cut is not None:
            tail = steps[cut:]
            steps = (steps[:cut] + [step('capture')] + tail + [step('record_state'), step('ai_roundtrip'), step('restore')]
                     + copy.deepcopy(tail) + [step('assert_recorded_state')])
        cases.append(dict(name=f'OptionalCombatDamageBypassRecipe_{csvid}_{variant}', card=name, fixture='empty_main',
                          seed=123, setup=world.setup, steps=steps, _recipe=dict(
                              family=FAMILY, version=2, variant=variant, params=declaration['params'], oracle_text=RULE,
                              untested_components=['multiple blockers and non-player defending objects',
                                                   'choice retiming between blocker declaration and combat damage'])))
    return cases
