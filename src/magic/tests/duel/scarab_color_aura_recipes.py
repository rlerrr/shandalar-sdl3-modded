"""Scarab Aura attachment, live color predicates, and actual blocking."""

import copy
import json
from pathlib import Path

from attack_value_trigger_recipes import _AttackResourceWorld


FAMILY = 'scarab_color_aura'
PROFILE = Path(__file__).with_name('data') / 'scarab_color_aura_wave8_profiles.json'
PALETTE = {
    'black': ('B', 'Scathe Zombies', 'Seal of Doom', 'Swamp'),
    'blue': ('U', 'Coral Merfolk', 'Seal of Removal', 'Island'),
    'green': ('G', 'Grizzly Bears', 'Seal of Strength', 'Forest'),
    'red': ('R', 'Goblin Piker', 'Seal of Fire', 'Mountain'),
    'white': ('W', 'Eager Cadet', 'Seal of Cleansing', 'Plains'),
}
HOST, ALTERNATE = 'Phyrexian Hulk', 'Bronze Sable'
MODES = ('plain', 'insufficient_mana', 'wrong_color', 'no_creature', 'owner_match', 'matching_land',
         'opponent_match', 'match_arrives_leaves', 'last_match_leaves', 'matched_block_denied',
         'other_block_allowed', 'unenchanted_attacker', 'foreign_host_block_denied', 'foreign_host_boost',
         'host_can_block', 'aura_removed_combat', 'host_returns', 'pending_snapshot')
WITNESSES = (HOST, ALTERNATE, 'Disenchant', 'Unsummon',
             *(name for _, *names in PALETTE.values() for name in names))


def profiles():
    return json.loads(PROFILE.read_text(encoding='utf8'))


def text_for(color):
    return (f"Enchant creature\nEnchanted creature can't be blocked by {color} creatures.\n"
            f'Enchanted creature gets +2/+2 as long as an opponent controls a {color} permanent.')


def classify(record):
    color = next((color for color in PALETTE if record and record.get('oracle_text') == text_for(color)), None)
    if (color is None or record.get('layout') != 'normal' or record.get('card_faces') or record.get('selected_face')
            or record.get('type_line') != 'Enchantment — Aura' or record.get('keywords') != ['Enchant']
            or record.get('colors') != ['W'] or record.get('mana_cost') != '{W}'):
        raise ValueError('complete normal Scarab Aura with an identical typed color in both clauses required')
    return dict(color=color, power_bonus=2, toughness_bonus=2)


def variants(params):
    return [f'{mode}_{owner}' for owner in (0, 1) for mode in MODES]


def check_variants(params, selected):
    if (type(selected) is not list or not selected or any(type(variant) is not str for variant in selected)
            or len(selected) != len(set(selected)) or not set(selected) <= set(variants(params))):
        raise ValueError('distinct reviewed Scarab variants required')


def spec(declaration, record):
    from recipes import cost_from_oracle

    expected = classify(record)
    pin = profiles()['sources'].get(record['name'])
    if (declaration.get('family') != FAMILY or type(declaration.get('version')) is not int or declaration['version'] != 2
            or declaration.get('oracle_text') != record['oracle_text'] or not pin or pin['oracle'] != record
            or json.dumps(declaration.get('params'), sort_keys=True) != json.dumps(expected, sort_keys=True)):
        raise ValueError('whole pinned Oracle and exact typed Scarab parameters required')
    check_variants(expected, declaration.get('variants'))
    return dict(expected, family=FAMILY, effect=FAMILY, mana=cost_from_oracle(record))


def validate_witnesses(oracle, registry):
    data = profiles()
    catalog = json.loads((PROFILE.parent / 'unimplemented.json').read_text(encoding='utf8'))
    unimplemented = {row['csvid'] for row in catalog['cases'].values()}
    if data.get('version') != 1 or set(data['witnesses']) != set(WITNESSES):
        raise ValueError('reviewed Scarab witness matrix changed')
    for group in ('sources', 'witnesses'):
        for name, pin in data[group].items():
            binding = registry.get(name, {})
            if (oracle.get(name) != pin['oracle'] or binding != pin['registry'] or binding.get('kind') != 'card'
                    or binding['csvid'] in unimplemented or binding['handler'] == 'NULL'
                    or binding['handler'] == 'card_dummy' and not pin.get('reviewed_native_characteristics')):
                raise ValueError(f'whole Oracle or binding changed for {name}')
            if group == 'sources':
                color = classify(oracle[name])['color']
                if binding['handler'] != f'manalinkeh_card_{color}_scarab':
                    raise ValueError(f'Scarab source handler changed for {name}')
    for color, creature, enchantment, land in PALETTE.values():
        if (oracle[creature]['colors'] != [color] or not oracle[creature]['type_line'].startswith('Creature — ')
                or oracle[creature]['oracle_text'] or oracle[enchantment]['colors'] != [color]
                or oracle[enchantment]['type_line'] != 'Enchantment'
                or oracle[land]['colors'] or not oracle[land]['type_line'].startswith('Basic Land — ')):
            raise ValueError(f'Scarab color contrast witnesses changed for {color}')


class ScarabWorld(_AttackResourceWorld):
    def __init__(self, name, params, owner, registry, oracle):
        super().__init__(name, params, owner, registry, oracle)
        self.color, self.matching_creature, self.enchantment, self.land = PALETTE[params['color']]

    def observe(self, ref, property_name, value):
        self.emit('assert_card', ref=ref, property=property_name, value=int(value))

    def observe_color(self, player, name):
        from recipes import COLORS

        self.emit('assert_matching_permanents', player=player, index=self.rows[name]['csvid'],
                  property='color', value=sum(1 << COLORS[color] for color in self.o[name]['colors']))

    def aura_present(self):
        return self.name in self.zones['battlefield'][self.owner]

    def refresh_host(self):
        # The opponent is relative to the Aura controller even when the Aura
        # enchants the other player's creature.
        enemy_permanents = self.zones['battlefield'][1 - self.owner]
        boost = self.aura_present() and any(self.color in self.o[name]['colors'] for name in enemy_permanents)
        if 'host' in self.entities:
            host = self.entities['host']
            host['power'] = int(self.o[HOST]['power']) + boost * self.p['power_bonus']
            host['toughness'] = int(self.o[HOST]['toughness']) + boost * self.p['toughness_bonus']

    def receipts(self):
        self.refresh_host()
        self.emit('refresh')
        super().receipts()
        if self.aura_present():
            self.observe('source', 'in_play', 1)
            self.observe_color(self.owner, self.name)

    def move(self, name, player, old, new):
        self.zones[old][player].remove(name)
        self.zones[new][player].append(name)

    def cast_aura(self, denial=None):
        from recipes import COLORS

        cost = self.p['mana']
        if denial == 'insufficient_mana':
            cost = [0] * 6
        elif denial == 'wrong_color':
            cost = [0] * 6
            cost[COLORS['B']] = 1
        self.pool(self.owner, cost)
        self.emit('refresh')
        if not denial:
            self.emit('choose_target', player=self.owner, ref='host')
        self.emit('cast', ref='source', value=int(not denial))
        self.emit('assert_unused_choices', value=0)
        self.emit('assert_stack', value=int(not denial))
        if denial:
            self.receipts()
            return None
        self.mana[self.owner] = [0] * 6
        for color in range(6):
            self.emit('assert_mana', player=self.owner, color=color, value=0)
        cut = len(self.steps)
        self.emit('resolve')
        self.move(self.name, self.owner, 'hand', 'battlefield')
        self.receipts()
        return cut

    def enter_colored_enchantment(self):
        from recipes import cost_from_oracle

        player = self.current
        self.pool(player, cost_from_oracle(self.o[self.enchantment]))
        self.emit('refresh')
        self.emit('cast', ref='match', value=1)
        self.emit('assert_stack', value=1)
        self.emit('assert_unused_choices', value=0)
        self.emit('resolve')
        self.mana[player] = [0] * 6
        self.move(self.enchantment, player, 'hand', 'battlefield')
        self.observe_color(player, self.enchantment)
        self.receipts()

    def combat(self, attackers, attempts):
        self.emit('begin_attackers')
        for attacker in attackers:
            self.emit('attack', ref=attacker, value=1)
            self.entities[attacker].update(tapped=True, attacking=True)
            self.observe(attacker, 'tapped', 1)
        self.emit('finish_attackers')
        self.emit('begin_blockers')
        blocks = {}
        for blocker, attacker in attempts:
            blocking = self.entities[blocker]
            self.observe_color(blocking['player'], blocking['name'])
            legal = not (attacker == 'host' and self.aura_present() and self.color in self.o[blocking['name']]['colors'])
            self.emit('block', ref=blocker, other=attacker, value=int(legal))
            self.observe(blocker, 'blocking_count', int(legal))
            if legal:
                blocks[attacker] = blocker
        self.emit('advance_combat')
        for attacker in attackers:
            self.observe(attacker, 'blocked', attacker in blocks)
        self.emit('combat_damage')
        self.emit('resolve')
        self.emit('end_combat')
        for attacker in attackers:
            attacking = self.entities[attacker]
            if attacker in blocks:
                blocking = self.entities[blocks[attacker]]
                attacking['damage'] += blocking['power']
                blocking['damage'] += attacking['power']
            else:
                self.life[1 - self.current] -= attacking['power']
        for ref, creature in self.entities.items():
            if creature['zone'] == 'battlefield' and creature['damage'] >= creature['toughness']:
                self.leave(ref, 'graveyard')
            creature['attacking'] = False
        self.receipts()


def scenario(world, mode):
    owner, enemy = world.owner, 1 - world.owner
    foreign_host = mode in ('foreign_host_block_denied', 'foreign_host_boost')
    host_owner = enemy if foreign_host else owner
    other_creature = next(creature for color, creature, _, _ in PALETTE.values() if color != world.color)
    world.add(world.name, 'source', owner, 'hand')
    if mode != 'no_creature':
        world.add(HOST, 'host', host_owner)
    world.add(ALTERNATE if mode != 'no_creature' else world.land, 'alternate', owner)

    match_modes = ('owner_match', 'opponent_match', 'match_arrives_leaves', 'last_match_leaves',
                   'other_block_allowed', 'foreign_host_boost', 'host_returns', 'pending_snapshot')
    if mode in match_modes:
        match_player = owner if mode == 'owner_match' else enemy
        world.add(world.enchantment, 'match', match_player, 'hand' if mode == 'match_arrives_leaves' else 'battlefield')
    if mode == 'matching_land':
        world.add(world.land, 'match_land', enemy)
    matched_combat = ('matched_block_denied', 'unenchanted_attacker', 'foreign_host_block_denied',
                      'host_can_block', 'aura_removed_combat')
    if mode in matched_combat or mode == 'last_match_leaves':
        world.add(world.matching_creature, 'other', 1-host_owner)
    elif mode in ('other_block_allowed', 'foreign_host_boost'):
        world.add(other_creature, 'other', 1-host_owner)

    if mode in ('match_arrives_leaves', 'last_match_leaves', 'aura_removed_combat'):
        world.add('Disenchant', 'disenchant', owner if mode != 'aura_removed_combat' else enemy, 'hand')
    if mode in ('last_match_leaves', 'host_returns'):
        world.add('Unsummon', 'unsummon', owner, 'hand')

    world.emit('turn', player=owner, value=0)
    for player in (0, 1):
        world.pool(player, [0] * 6)
    world.receipts()
    if mode == 'matching_land':
        world.observe_color(enemy, world.land)
    if mode in match_modes and mode != 'match_arrives_leaves':
        world.observe_color(match_player, world.enchantment)
    denial = mode if mode in ('insufficient_mana', 'wrong_color', 'no_creature') else None
    pending = world.cast_aura(denial)
    if denial:
        return None

    if mode in ('match_arrives_leaves', 'last_match_leaves'):
        if mode == 'match_arrives_leaves':
            world.next_turn()
            world.enter_colored_enchantment()
        world.spell('Disenchant', 'disenchant', owner, 'match')
        world.move(world.enchantment, enemy, 'battlefield', 'graveyard')
        world.receipts()
        if mode == 'last_match_leaves':
            world.spell('Unsummon', 'unsummon', owner, 'other')
            world.leave('other', 'hand')
            world.receipts()
    if mode == 'host_returns':
        world.spell('Unsummon', 'unsummon', owner, 'host')
        world.leave('host', 'hand')
        world.move(world.name, owner, 'battlefield', 'graveyard')
        world.receipts()
    combat_modes = matched_combat + ('other_block_allowed', 'foreign_host_boost')
    if mode in combat_modes:
        if mode == 'aura_removed_combat':
            world.spell('Disenchant', 'disenchant', enemy, 'source')
            world.move(world.name, owner, 'battlefield', 'graveyard')
            world.receipts()
        if foreign_host or mode == 'host_can_block':
            world.next_turn()
        if mode == 'host_can_block':
            world.combat(['other'], [('host', 'other')])
        elif mode == 'unenchanted_attacker':
            world.combat(['host', 'alternate'], [('other', 'host'), ('other', 'alternate')])
        else:
            world.combat(['host'], [('other', 'host')])
    return pending if mode == 'pending_snapshot' else None


def expand(name, csvid, declaration, params, selected, registry, oracle):
    from recipes import step

    check_variants(params, selected)
    validate_witnesses(oracle, registry)
    if profiles()['sources'][name]['registry']['csvid'] != csvid:
        raise ValueError('Scarab source CSVID changed')
    cases = []
    for variant in selected:
        mode, owner = variant.rsplit('_', 1)
        world = ScarabWorld(name, params, int(owner), registry, oracle)
        cut = scenario(world, mode)
        actions = world.steps
        if cut is not None:
            tail = actions[cut:]
            actions = (actions[:cut] + [step('capture')] + tail + [step('record_state'), step('ai_roundtrip'), step('restore')]
                       + copy.deepcopy(tail) + [step('assert_recorded_state')])
        cases.append(dict(name=f'ScarabColorAuraRecipe_{csvid}_{variant}', card=name, fixture='empty_main', seed=123,
                          setup=world.setup, steps=actions, _recipe=dict(
                              family=FAMILY, version=2, variant=variant, params=declaration['params'],
                              oracle_text=declaration['oracle_text'],
                              untested_components=['attachment pointer observed through target, exact stats and departure receipts',
                                                   'text changes, Aura control transfers, and multiplayer'])))
    return cases
