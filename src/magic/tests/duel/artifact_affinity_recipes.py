"""Real affinity payments, controlled artifact changes and printed Flying."""

import copy
import json
from pathlib import Path

from attack_value_trigger_recipes import _AttackResourceWorld


FAMILY = 'artifact_affinity'
PROFILE = Path(__file__).with_name('data') / 'artifact_affinity_wave8_profiles.json'
HANDLER = 'manalinkeh_card_frogmite'
RULE = 'Affinity for artifacts (This spell costs {1} less to cast for each artifact you control.)'
ARTIFACT = 'Darksteel Relic'
CREATURE_ARTIFACT = 'Ornithopter'
WITNESSES = (ARTIFACT, CREATURE_ARTIFACT, 'Darksteel Citadel', 'Hill Giant', 'Giant Spider', 'Wind Drake',
             'Shatter', 'Steal Artifact')
MODES = ('full_price', 'own_exact', 'short_own', 'other_permanents', 'opponent_only', 'mixed_controllers',
         'other_zones', 'artifact_land', 'tapped_artifact', 'floor_surplus', 'generic_from_black', 'enter_artifact',
         'remove_artifact', 'stolen_artifact', 'second_cast', 'pending_snapshot')
FLYING_MODES = ('colored_floor_short', 'ground_block_denied', 'reach_block_allowed', 'block_flyer')


def profiles():
    return json.loads(PROFILE.read_text(encoding='utf8'))


def types(record):
    return record['type_line'].partition(' — ')[0].split()


def classify(record):
    compound = compound_model(record)
    if compound is not None:
        return compound

    from recipes import COLORS, cost_from_oracle

    if (not record or record.get('layout') != 'normal' or record.get('card_faces') or record.get('selected_face')
            or types(record) not in (['Creature'], ['Artifact', 'Creature'])
            or record.get('oracle_text') not in (RULE, RULE + '\nFlying')
            or any(not record.get(key, '').isascii() or not record[key].isdecimal() for key in ('power', 'toughness'))):
        raise ValueError('whole fixed-stat artifact affinity creature with optional printed Flying required')
    flying = record['oracle_text'].endswith('\nFlying')
    if set(record['keywords']) != ({'Affinity', 'Flying'} if flying else {'Affinity'}):
        raise ValueError('printed keyword and Oracle affinity clauses must agree')
    printed = cost_from_oracle(record)
    if printed[0] < 2 or sum(printed[1:]) != printed[COLORS['U']]:
        raise ValueError('reviewed affinity witness matrix needs generic and optional blue costs')
    return dict(printed=printed, artifact='Artifact' in types(record), flying=flying,
                power=int(record['power']), toughness=int(record['toughness']))


def variants(params):
    if 'role' in params:
        return [f'{mode}_{owner}' for owner in (0, 1) for mode in COMPOUND_MODES[params['role']]]
    modes = MODES + (FLYING_MODES if params['flying'] else ())
    return [f'{mode}_{owner}' for owner in (0, 1) for mode in modes]


def check_variants(params, selected):
    if (type(selected) is not list or not selected or any(type(variant) is not str for variant in selected)
            or len(set(selected)) != len(selected) or not set(selected) <= set(variants(params))):
        raise ValueError('distinct reviewed affinity variants required')


def spec(declaration, record):
    expected = classify(record)
    group = 'compound_sources' if 'role' in expected else 'sources'
    pin = profiles()[group].get(record['name'])
    if (declaration.get('family') != FAMILY or type(declaration.get('version')) is not int or declaration['version'] != 2
            or declaration.get('oracle_text') != record['oracle_text'] or not pin or pin['oracle'] != record
            or json.dumps(declaration.get('params'), sort_keys=True) != json.dumps(expected, sort_keys=True)):
        raise ValueError('whole pinned Oracle and exact typed affinity data required')
    check_variants(expected, declaration.get('variants'))
    flying = expected.get('flying', expected.get('dynamic_artifact_size', False))
    return dict(expected, family=FAMILY, effect=FAMILY, keywords=['Flying'] if flying else [])


def validate_witnesses(oracle, registry):
    data = profiles()
    catalog = json.loads((PROFILE.parent / 'unimplemented.json').read_text(encoding='utf8'))
    unimplemented = {row['csvid'] for row in catalog['cases'].values()}
    if data.get('version') != 1 or set(data['witnesses']) != set(WITNESSES):
        raise ValueError('reviewed artifact affinity witness matrix changed')
    for group in ('sources', 'witnesses'):
        for name, pin in data[group].items():
            binding = registry.get(name, {})
            if (oracle.get(name) != pin['oracle'] or binding != pin['registry'] or binding.get('kind') != 'card'
                    or binding['csvid'] in unimplemented or binding['handler'] == 'NULL'
                    or binding['handler'] == 'card_dummy' and not pin.get('reviewed_native_characteristics')):
                raise ValueError(f'whole Oracle or binding changed for {name}')
            if group == 'sources' and binding['handler'] != HANDLER:
                raise ValueError(f'artifact affinity source handler changed for {name}')


class AffinityWorld(_AttackResourceWorld):
    def observe(self, ref, property_name, value):
        self.emit('assert_card', ref=ref, property=property_name, value=int(value))

    def receipts(self):
        self.emit('refresh')
        super().receipts()
        for player, cards in enumerate(self.zones['battlefield']):
            for name in sorted(set(cards)):
                self.emit('assert_matching_permanents', player=player, index=self.rows[name]['csvid'],
                          property='artifact', value=int('Artifact' in types(self.o[name])))

    def price(self):
        count = sum('Artifact' in types(self.o[name]) for name in self.zones['battlefield'][self.owner])
        printed = self.p['printed']
        return [max(0, printed[0] - count), *printed[1:]]

    def pending(self, ref, spent, *, allowed=True, colored_generic=0, target=None):
        from recipes import COLORS

        player = self.owner
        if allowed and any(due > available for due, available in zip(spent, self.mana[player])):
            raise ValueError('real cast must be funded by the available wallet')
        if player == 0:
            for _ in range(colored_generic):
                self.emit('choose_mana', player=player, color=COLORS['B'])
        if target:
            self.emit('choose_target', player=player, ref=target)
        self.emit('cast', ref=ref, value=int(allowed))
        self.emit('assert_unused_choices', value=0)
        self.emit('assert_stack', value=int(allowed))
        self.observe(ref, 'in_hand', not allowed)
        self.observe(ref, 'in_play', 0)
        if allowed:
            self.mana[player] = [available - due for available, due in zip(self.mana[player], spent)]
        for color, amount in enumerate(self.mana[player]):
            self.emit('assert_mana', player=player, color=color, value=amount)

    def resolve_creature(self, ref):
        self.emit('resolve')
        self.arrive(ref)
        self.receipts()

    def establish_control(self):
        from recipes import cost_from_oracle

        player = self.owner
        cost = cost_from_oracle(self.o['Steal Artifact'])
        self.pool(player, cost)
        self.emit('refresh')
        self.pending('steal', cost, target='stolen')
        self.emit('resolve')
        self.zones['hand'][player].remove('Steal Artifact')
        self.zones['battlefield'][player].append('Steal Artifact')
        self.zones['battlefield'][1 - player].remove(ARTIFACT)
        self.zones['battlefield'][player].append(ARTIFACT)
        self.emit('bind_permanent_copy', ref='stolen', player=player, value=self.rows[ARTIFACT]['csvid'], index=0)
        self.observe('stolen', 'in_play', 1)
        self.receipts()

    def combat(self, attacker, blocker):
        attacking = self.entities[attacker]
        blocking = self.entities[blocker]
        legal = not self.has(attacker, 'Flying') or self.has(blocker, 'Flying') or self.has(blocker, 'Reach')
        self.emit('begin_attackers')
        self.emit('attack', ref=attacker, value=1)
        attacking.update(tapped=True, attacking=True)
        self.emit('finish_attackers')
        self.emit('begin_blockers')
        self.emit('block', ref=blocker, other=attacker, value=int(legal))
        self.observe(blocker, 'blocking_count', legal)
        self.emit('advance_combat')
        self.observe(attacker, 'blocked', legal)
        self.emit('combat_damage')
        self.emit('resolve')
        self.emit('end_combat')
        if legal:
            blocking['damage'] += attacking['power']
            attacking['damage'] += blocking['power']
            for ref in (attacker, blocker):
                creature = self.entities[ref]
                if creature['damage'] >= creature['toughness']:
                    self.leave(ref, 'graveyard')
        else:
            self.life[1 - self.current] -= attacking['power']
        attacking['attacking'] = False
        self.receipts()


def setup(world, mode):
    owner, enemy = world.owner, 1 - world.owner
    world.add(world.name, 'source', owner, 'hand', source=True)
    if mode in ('own_exact', 'short_own', 'mixed_controllers', 'generic_from_black', 'second_cast', 'pending_snapshot',
               'ground_block_denied', 'reach_block_allowed', 'block_flyer'):
        world.add(ARTIFACT, 'relic', owner)
        if mode == 'own_exact':
            world.add(CREATURE_ARTIFACT, 'artifact_creature', owner)
    if mode in ('opponent_only', 'mixed_controllers'):
        for index in range(3):
            world.add(ARTIFACT, f'enemy_relic_{index}', enemy)
    if mode == 'other_zones':
        for zone in ('hand', 'graveyard', 'exile', 'library'):
            world.add(ARTIFACT, f'artifact_{zone}', owner, zone)
    if mode == 'other_permanents':
        world.add('Hill Giant', 'nonartifact', owner)
        world.add('Hill Giant', 'another_nonartifact', owner)
    if mode == 'artifact_land':
        world.add('Darksteel Citadel', 'artifact_land', owner)
    if mode in ('floor_surplus', 'colored_floor_short'):
        for index in range(world.p['printed'][0] + 2):
            world.add(ARTIFACT, f'excess_relic_{index}', owner)
    if mode in ('tapped_artifact', 'enter_artifact', 'remove_artifact'):
        world.add(CREATURE_ARTIFACT, 'changing', owner, 'hand' if mode == 'enter_artifact' else 'battlefield')
        if mode == 'tapped_artifact':
            world.setup[-1]['state'] = ['tapped']
            world.entities['changing']['tapped'] = True
    if mode == 'remove_artifact':
        world.add('Shatter', 'shatter', enemy, 'hand')
    if mode == 'stolen_artifact':
        world.add(ARTIFACT, 'stolen', enemy)
        world.add('Steal Artifact', 'steal', owner, 'hand')
    if mode == 'second_cast':
        world.add(world.name, 'second', owner, 'hand', source=True)
    if mode in ('ground_block_denied', 'reach_block_allowed', 'block_flyer'):
        foe = {'ground_block_denied': 'Hill Giant', 'reach_block_allowed': 'Giant Spider', 'block_flyer': 'Wind Drake'}[mode]
        world.add(foe, 'foe', enemy)


def scenario(world, mode):
    from recipes import COLORS

    owner, enemy = world.owner, 1 - world.owner
    setup(world, mode)
    if mode in ('remove_artifact', 'reach_block_allowed', 'block_flyer'):
        world.emit('reset_death_history')
    world.emit('turn', player=owner, value=0)
    for player in (0, 1):
        world.pool(player, [0] * 6)
    world.receipts()

    if mode == 'enter_artifact':
        world.pending('changing', [0] * 6)
        world.resolve_creature('changing')
    if mode == 'stolen_artifact':
        world.establish_control()

    price = world.price()
    wallet, spent = price.copy(), price.copy()
    colored_generic = 0
    denied = mode in ('short_own', 'other_permanents', 'opponent_only', 'other_zones', 'remove_artifact', 'colored_floor_short')
    if denied and mode != 'colored_floor_short' and mode != 'remove_artifact':
        wallet[0] -= 1
    if mode == 'floor_surplus':
        wallet[0] += 2
    if mode == 'colored_floor_short':
        wallet[0] = world.p['printed'][0] + 2
        wallet[COLORS['U']] -= 1
    if mode == 'generic_from_black':
        colored_generic = price[0]
        spent[0], spent[COLORS['B']] = 0, colored_generic
        wallet = spent.copy()
    if mode == 'second_cast':
        follow_price = [max(0, price[0] - world.p['artifact']), *price[1:]]
        wallet = [first + second for first, second in zip(price, follow_price)]
    world.pool(owner, wallet)
    world.emit('refresh')
    if mode == 'remove_artifact':
        world.spell('Shatter', 'shatter', enemy, 'changing')
        world.leave('changing', 'graveyard')
        world.receipts()
        spent = world.price()
    world.pending('source', spent, allowed=not denied, colored_generic=colored_generic)
    if denied:
        world.receipts()
        return None
    cut = len(world.steps) if mode == 'pending_snapshot' else None
    world.resolve_creature('source')
    if mode == 'second_cast':
        world.pending('second', world.price())
        world.resolve_creature('second')
    if mode in ('ground_block_denied', 'reach_block_allowed'):
        world.age()
        world.combat('source', 'foe')
    if mode == 'block_flyer':
        world.next_turn()
        world.combat('foe', 'source')
    return cut


def expand(name, csvid, declaration, params, selected, registry, oracle):
    if 'role' in params:
        return expand_compound(name, csvid, declaration, params, selected, registry, oracle)

    from recipes import step

    check_variants(params, selected)
    validate_witnesses(oracle, registry)
    if profiles()['sources'][name]['registry']['csvid'] != csvid:
        raise ValueError('affinity source CSVID changed')
    cases = []
    for variant in selected:
        mode, owner = variant.rsplit('_', 1)
        world = AffinityWorld(name, params, int(owner), registry, oracle)
        cut = scenario(world, mode)
        actions = world.steps
        if cut is not None:
            tail = actions[cut:]
            actions = (actions[:cut] + [step('capture')] + tail + [step('record_state'), step('ai_roundtrip'), step('restore')]
                       + copy.deepcopy(tail) + [step('assert_recorded_state')])
        cases.append(dict(name=f'ArtifactAffinityRecipe_{csvid}_{variant}', card=name, fixture='empty_main', seed=123,
                          setup=world.setup, steps=actions, _recipe=dict(
                              family=FAMILY, version=2, variant=variant, params=declaration['params'],
                              oracle_text=declaration['oracle_text'],
                              untested_components=['compound alternate casting costs and simultaneous cost modifiers',
                                                   'artifact token creation, text changes and multiplayer'])))
    return cases


COMPOUND_MODES = {
    'draw': ('full_draw', 'reduced_draw', 'floor_draw', 'short_blue', 'opponent_only', 'two_draws', 'last_two', 'pending_snapshot'),
    'pump': ('reduced_cast', 'floor_cast', 'flash_response', 'flash_combat_response', 'pump_attack', 'enemy_pump', 'tapped_denied', 'no_target'),
    'size': ('paid_size', 'opponent_only_zero', 'actual_artifact_entry', 'last_artifact_leaves', 'control_changes_size',
             'ground_block_denied', 'reach_block_allowed', 'block_flyer', 'short_blue'),
}
COMPOUND_HANDLERS = {'draw': 'manalinkeh_card_thoughtcast', 'pump': 'manalinkeh_card_scale_of_chiss_goria',
                     'size': 'manalinkeh_card_broodstar'}
DRAW_LIBRARY = ('Plains', 'Island', 'Mountain', 'Forest')
COMPOUND_WITNESSES = (*WITNESSES, 'Giant Growth', *DRAW_LIBRARY)


def compound_model(record):
    from recipes import cost_from_oracle

    if not record or record.get('layout') != 'normal' or record.get('card_faces') or record.get('selected_face'):
        return None
    text = record.get('oracle_text')
    shape = (types(record), set(record.get('keywords', [])))
    if text == RULE + '\nDraw two cards.' and shape == (['Sorcery'], {'Affinity'}):
        role = 'draw'
    elif text == 'Flash\n' + RULE + '\n{T}: Target creature gets +1/+0 until end of turn.' and shape == (['Artifact'], {'Flash', 'Affinity'}):
        role = 'pump'
    elif (text == RULE + f"\nFlying\n{record['name']}'s power and toughness are each equal to the number of artifacts you control."
          and shape == (['Creature'], {'Flying', 'Affinity'}) and record.get('power') == record.get('toughness') == '*'):
        role = 'size'
    else:
        return None
    return dict(role=role, printed=cost_from_oracle(record), draw_count=2 if role == 'draw' else 0, flash=role == 'pump',
                power_pump=1 if role == 'pump' else 0, dynamic_artifact_size=role == 'size')


def validate_compound_witnesses(oracle, registry):
    data = profiles()
    catalog = json.loads((PROFILE.parent / 'unimplemented.json').read_text(encoding='utf8'))
    unimplemented = {row['csvid'] for row in catalog['cases'].values()}
    witnesses = data['witnesses'] | data['compound_witnesses']
    if set(witnesses) != set(COMPOUND_WITNESSES):
        raise ValueError('compound affinity witness matrix differs')
    for group, pins in (('sources', data['compound_sources']), ('witnesses', witnesses)):
        for name, pin in pins.items():
            row = registry.get(name, {})
            if (oracle.get(name) != pin['oracle'] or row != pin['registry'] or row.get('kind') != 'card'
                    or row['csvid'] in unimplemented or row['handler'] == 'NULL'
                    or row['handler'] == 'card_dummy' and not pin.get('reviewed_native_characteristics')):
                raise ValueError('whole Oracle or implementation changed for ' + name)
            if group == 'sources' and row['handler'] != COMPOUND_HANDLERS[compound_model(oracle[name])['role']]:
                raise ValueError('compound affinity source handler changed for ' + name)


class CompoundWorld(AffinityWorld):
    def __init__(self, name, params, owner, registry, oracle):
        super().__init__(name, params, owner, registry, oracle)
        self.tooth_tapped = False
        self.temporary = {}

    def entity(self, name, ref, player, zone, source=False, token=False):
        if source and self.p['dynamic_artifact_size']:
            self.entities[ref] = dict(name=name, player=player, zone=zone, source=True, token=False, power=0, toughness=0,
                                      sick=False, tapped=False, attacking=False, damage=0, keys={'Flying'}, grants=set())
        else:
            super().entity(name, ref, player, zone, source, token)

    def receipts(self):
        if self.p['dynamic_artifact_size'] and self.entities['source']['zone'] == 'battlefield':
            cards = self.zones['battlefield'][self.owner]
            count = sum('Artifact' in types(self.o[name]) for name in cards)
            self.entities['source'].update(power=count, toughness=count)
        super().receipts()
        if self.p['role'] == 'pump' and self.name in self.zones['battlefield'][self.owner]:
            self.observe('source', 'in_play', 1)
            self.observe('source', 'tapped', self.tooth_tapped)

    def boost(self, ref, power, toughness=0):
        previous = self.temporary.get(ref, (0, 0))
        self.temporary[ref] = (previous[0] + power, previous[1] + toughness)
        self.entities[ref]['power'] += power
        self.entities[ref]['toughness'] += toughness

    def expire(self):
        super().expire()
        if self.current != self.owner:
            self.tooth_tapped = False
        for ref, (power, toughness) in self.temporary.items():
            self.entities[ref]['power'] -= power
            self.entities[ref]['toughness'] -= toughness
        self.temporary.clear()

    def resolve_thoughtcast(self):
        self.emit('resolve')
        player = self.owner
        library = self.zones['library'][player]
        draws = self.p['draw_count']
        if len(library) < draws:
            raise ValueError('draw probe requires a nonterminal library')
        self.zones['hand'][player].remove(self.name)
        self.zones['hand'][player].extend(library[:draws])
        del library[:draws]
        self.zones['graveyard'][player].append(self.name)
        self.receipts()

    def tooth_enters(self):
        self.zones['hand'][self.owner].remove(self.name)
        self.zones['battlefield'][self.owner].append(self.name)

    def tooth_pump(self, ref=None, allowed=True):
        if allowed:
            self.emit('choose_target', player=self.owner, ref=ref)
        self.emit('activate', ref='source', value=int(allowed))
        self.emit('assert_unused_choices', value=0)
        self.emit('assert_stack', value=int(allowed))
        if allowed:
            self.tooth_tapped = True
            self.observe('source', 'tapped', 1)
            self.emit('resolve')
            self.boost(ref, self.p['power_pump'])
        self.receipts()

    def flash_response(self):
        from recipes import cost_from_oracle

        enemy = 1 - self.owner
        self.pool(enemy, cost_from_oracle(self.o['Hill Giant']))
        self.pool(self.owner, self.price())
        self.emit('refresh')
        self.emit('cast', ref='parent', value=1)
        self.emit('assert_stack', value=1)
        self.emit('announce_spell', ref='parent')
        self.emit('cast_response', ref='source', value=1)
        self.emit('assert_stack', value=2)
        self.observe('source', 'in_hand', 0)
        self.observe('source', 'in_play', 0)
        self.emit('announce_spell', ref='source')
        self.emit('resolve_response')
        self.mana = [[0] * 6, [0] * 6]
        self.arrive('parent')
        self.tooth_enters()
        self.receipts()

    def flash_combat_response(self):
        from recipes import COLORS, cost_from_oracle

        enemy = 1 - self.owner
        self.emit('begin_attackers')
        self.emit('attack', ref='foe', value=1)
        self.entities['foe'].update(tapped=True, attacking=True)
        self.emit('finish_attackers')
        self.emit('begin_blockers')
        self.emit('advance_combat')
        self.pool(enemy, cost_from_oracle(self.o['Giant Growth']))
        self.pool(self.owner, self.price())
        self.emit('refresh')
        # The fixture has a real after-blocking phase and a real pending opponent spell.
        # Use the explicit response bridge for the legal Flash cast at this combat boundary.
        self.emit('require_can_cast', ref='source', value=1)
        self.emit('choose_target', player=enemy, ref='foe')
        self.emit('cast', ref='parent', value=1)
        self.emit('assert_unused_choices', value=0)
        self.emit('assert_stack', value=1)
        self.emit('announce_spell', ref='parent')
        self.emit('cast_response', ref='source', value=1)
        self.emit('assert_stack', value=2)
        self.observe('source', 'in_hand', 0)
        self.observe('source', 'in_play', 0)
        self.emit('assert_mana', player=self.owner, color=0, value=0)
        self.emit('assert_mana', player=enemy, color=COLORS['G'], value=0)
        self.emit('announce_spell', ref='source')
        self.emit('resolve_response')
        self.emit('assert_unused_choices', value=0)
        self.mana = [[0] * 6, [0] * 6]
        self.zones['hand'][enemy].remove('Giant Growth')
        self.zones['graveyard'][enemy].append('Giant Growth')
        self.tooth_enters()
        self.boost('foe', 3, 3)
        self.receipts()
        self.tooth_pump('foe')
        self.emit('combat_damage')
        self.emit('resolve')
        self.emit('end_combat')
        self.life[self.owner] -= self.entities['foe']['power']
        self.entities['foe']['attacking'] = False
        self.receipts()


def compound_scenario(world, mode):
    from recipes import COLORS

    role, owner = world.p['role'], world.owner
    enemy = 1 - owner
    world.add(world.name, 'source', owner, 'hand', source=True)
    counts = 2
    if mode in ('full_draw', 'opponent_only', 'opponent_only_zero', 'last_artifact_leaves', 'control_changes_size'):
        counts = 0
    elif mode in ('floor_draw', 'floor_cast', 'short_blue'):
        counts = world.p['printed'][0] + 2
    elif mode == 'actual_artifact_entry':
        counts = 1
    elif mode == 'reach_block_allowed':
        counts = 4
    for index in range(counts):
        world.add(ARTIFACT, f'relic_{index}', owner)
    if role == 'size' or mode == 'opponent_only':
        world.add(ARTIFACT, 'enemy_relic', enemy)
    if role == 'draw':
        for index, card in enumerate(DRAW_LIBRARY[:2] if mode == 'last_two' else DRAW_LIBRARY):
            world.add(card, f'draw_{index}', owner, 'library')
        world.add('Island', 'enemy_library', enemy, 'library')
    if mode == 'two_draws':
        world.add(world.name, 'second', owner, 'hand')
    if mode in ('actual_artifact_entry', 'last_artifact_leaves'):
        world.add(CREATURE_ARTIFACT, 'changing', owner, 'hand' if mode == 'actual_artifact_entry' else 'battlefield')
    if mode == 'last_artifact_leaves':
        world.add('Shatter', 'shatter', enemy, 'hand')
    if mode == 'control_changes_size':
        world.add(CREATURE_ARTIFACT, 'existing_artifact', owner)
        world.add('Steal Artifact', 'steal', owner, 'hand')
        world.add(ARTIFACT, 'stolen', enemy)
    foes = {'ground_block_denied': 'Hill Giant', 'reach_block_allowed': 'Giant Spider', 'block_flyer': 'Wind Drake',
            'enemy_pump': 'Hill Giant', 'flash_combat_response': 'Hill Giant'}
    if mode in foes:
        world.add(foes[mode], 'foe', enemy)
    if mode in ('pump_attack', 'tapped_denied'):
        world.add('Hill Giant', 'ally', owner)
    if mode in ('flash_response', 'flash_combat_response'):
        world.current = enemy
        world.add('Hill Giant' if mode == 'flash_response' else 'Giant Growth', 'parent', enemy, 'hand')
    if mode in ('opponent_only_zero', 'last_artifact_leaves', 'reach_block_allowed', 'block_flyer'):
        world.emit('reset_death_history')
    world.emit('turn', player=world.current, value=0)
    for player in (0, 1):
        world.pool(player, [0] * 6)
    world.receipts()

    if mode in ('flash_response', 'flash_combat_response'):
        if mode == 'flash_response':
            world.flash_response()
            world.tooth_pump('parent')
        else:
            world.flash_combat_response()
        world.next_turn()
        return None

    paid = world.price()
    wallet = paid.copy()
    denied = mode in ('short_blue', 'opponent_only')
    if mode == 'short_blue':
        wallet[COLORS['U']] -= 1
        wallet[0] += 3
    if mode == 'opponent_only':
        wallet[0] -= 1
    if mode in ('floor_draw', 'floor_cast'):
        wallet[0] += 2
    if mode == 'two_draws':
        wallet = [2 * amount for amount in paid]
    world.pool(owner, wallet)
    world.emit('refresh')
    world.pending('source', paid, allowed=not denied)
    if denied:
        world.receipts()
        return None
    cut = len(world.steps) if mode == 'pending_snapshot' else None
    if role == 'draw':
        world.resolve_thoughtcast()
        if mode == 'two_draws':
            world.pending('second', world.price())
            world.resolve_thoughtcast()
    elif role == 'pump':
        world.emit('resolve')
        world.tooth_enters()
        world.receipts()
        if mode in ('pump_attack', 'enemy_pump', 'tapped_denied'):
            world.tooth_pump('foe' if mode == 'enemy_pump' else 'ally')
            if mode == 'tapped_denied':
                world.tooth_pump(allowed=False)
            if mode == 'pump_attack':
                world.emit('begin_attackers')
                world.emit('attack', ref='ally', value=1)
                world.entities['ally']['tapped'] = True
                for op in ('finish_attackers', 'begin_blockers', 'advance_combat', 'combat_damage', 'resolve', 'end_combat'):
                    world.emit(op)
                world.life[enemy] -= world.entities['ally']['power']
                world.receipts()
            world.next_turn()
        if mode == 'no_target':
            world.tooth_pump(allowed=False)
    elif mode == 'opponent_only_zero':
        world.emit('resolve')
        world.arrive('source')
        world.leave('source', 'graveyard')
        world.receipts()
    else:
        world.resolve_creature('source')
        if mode == 'actual_artifact_entry':
            world.pending('changing', [0] * 6)
            world.resolve_creature('changing')
        elif mode == 'last_artifact_leaves':
            world.spell('Shatter', 'shatter', enemy, 'changing')
            world.leave('changing', 'graveyard')
            world.leave('source', 'graveyard')
            world.receipts()
        elif mode == 'control_changes_size':
            world.establish_control()
        elif mode in ('ground_block_denied', 'reach_block_allowed'):
            world.age()
            world.combat('source', 'foe')
        elif mode == 'block_flyer':
            world.next_turn()
            world.combat('foe', 'source')
    return cut


def expand_compound(name, csvid, declaration, params, selected, registry, oracle):
    from recipes import step

    check_variants(params, selected)
    validate_compound_witnesses(oracle, registry)
    if profiles()['compound_sources'][name]['registry']['csvid'] != csvid:
        raise ValueError('compound affinity source CSVID differs')
    cases = []
    for variant in selected:
        mode, owner = variant.rsplit('_', 1)
        world = CompoundWorld(name, params, int(owner), registry, oracle)
        cut = compound_scenario(world, mode)
        actions = world.steps
        if cut is not None:
            tail = actions[cut:]
            actions = (actions[:cut] + [step('capture')] + tail + [step('record_state'), step('ai_roundtrip'), step('restore')]
                       + copy.deepcopy(tail) + [step('assert_recorded_state')])
        gaps = ['multiplayer, external costs, and artifact tokens']
        if params['role'] == 'draw':
            gaps.append('terminal missing-card draw processing')
        if mode == 'flash_response':
            gaps.append('explicit pending-spell Flash bridge observes legal casting and payment; menu discovery exercised separately')
        if mode == 'flash_combat_response':
            gaps.append('combat UI response-menu discovery remains outside the bounded declaration fixture')
        cases.append(dict(name=f'ArtifactAffinityCompoundRecipe_{csvid}_{variant}', card=name, fixture='empty_main', seed=123,
                          setup=world.setup, steps=actions, _recipe=dict(family=FAMILY, version=2, variant=variant,
                              params=declaration['params'], oracle_text=declaration['oracle_text'], untested_components=gaps)))
    return cases
