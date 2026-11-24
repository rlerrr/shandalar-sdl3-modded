"""Pure Unleash through real entry, counter updates and combat declarations."""

import copy
import json
from pathlib import Path

from attack_value_trigger_recipes import _AttackResourceWorld


FAMILY = 'unleash_creature'
PROFILE = Path(__file__).with_name('data') / 'unleash_creature_wave8_profiles.json'
RULE = ("Unleash (You may have this creature enter with a +1/+1 counter on it. "
        "It can't block as long as it has a +1/+1 counter on it.)")
ATTACKER = "Mons's Goblin Raiders"
WITNESSES = (ATTACKER, 'Burst of Strength', 'Elvish Piper', 'Unsummon', 'Vampire Hexmage')
MODES = ('accept', 'decline', 'insufficient_mana', 'wrong_color', 'block_accept', 'block_decline',
         'block_after_clear', 'decline_external_counter', 'grow_other', 'double_counter', 'boosted_attack',
         'bounce_rechoose', 'piper_accept', 'piper_decline', 'pending_snapshot', 'separate_copies')


def profiles():
    return json.loads(PROFILE.read_text(encoding='utf8'))


def classify(record):
    from recipes import COLORS, cost_from_oracle

    if (not record or record.get('layout') != 'normal' or record.get('card_faces') or record.get('selected_face')
            or not record.get('type_line', '').startswith('Creature — ') or record.get('keywords') != ['Unleash']
            or record.get('oracle_text') != RULE
            or any(not record.get(key, '').isascii() or not record[key].isdecimal() for key in ('power', 'toughness'))):
        raise ValueError('whole fixed-stat pure Unleash creature required')
    hybrid = record['mana_cost'] == '{B/R}'
    if hybrid:
        mana = [[int(i == COLORS[color]) for i in range(6)] for color in ('B', 'R')]
    else:
        mana = [cost_from_oracle(record)]
    return dict(mana_options=mana, hybrid=hybrid, power=int(record['power']), toughness=int(record['toughness']))


def variants(params):
    modes = ('pay_black', 'pay_red', *MODES[1:]) if params['hybrid'] else MODES
    return [f'{mode}_{owner}' for owner in (0, 1) for mode in modes]


def check_variants(params, selected):
    if (type(selected) is not list or not selected or any(type(variant) is not str for variant in selected)
            or len(set(selected)) != len(selected) or not set(selected) <= set(variants(params))):
        raise ValueError('distinct reviewed Unleash variants required')


def spec(declaration, record):
    expected = classify(record)
    pin = profiles()['sources'].get(record['name'])
    if (declaration.get('family') != FAMILY or type(declaration.get('version')) is not int or declaration['version'] != 2
            or declaration.get('oracle_text') != RULE or not pin or pin['oracle'] != record
            or json.dumps(declaration.get('params'), sort_keys=True) != json.dumps(expected, sort_keys=True)):
        raise ValueError('whole pinned Oracle and exact typed Unleash parameters required')
    check_variants(expected, declaration.get('variants'))
    return dict(expected, family=FAMILY, effect=FAMILY, keywords=['Unleash'])


def validate_witnesses(oracle, registry):
    data = profiles()
    catalog = json.loads((PROFILE.parent / 'unimplemented.json').read_text(encoding='utf8'))
    unimplemented = {row['csvid'] for row in catalog['cases'].values()}
    if data.get('version') != 1 or set(data['witnesses']) != set(WITNESSES):
        raise ValueError('reviewed Unleash witnesses changed')
    for group in ('sources', 'witnesses'):
        for name, pin in data[group].items():
            binding = registry.get(name, {})
            if (oracle.get(name) != pin['oracle'] or binding != pin['registry'] or binding.get('kind') != 'card'
                    or binding['csvid'] in unimplemented or binding['handler'] == 'NULL'
                    or binding['handler'] == 'card_dummy' and not pin.get('reviewed_native_characteristics')):
                raise ValueError(f'whole Oracle or binding changed for {name}')
            if group == 'sources':
                hybrid = classify(oracle[name])['hybrid']
                handler = 'manalinkeh_card_rakdos_cackler' if hybrid else 'manalinkeh_card_dark_reveler'
                if binding['handler'] != handler:
                    raise ValueError(f'Unleash source handler changed for {name}')


class UnleashWorld(_AttackResourceWorld):
    def entity(self, name, ref, player, zone, source=False, token=False):
        super().entity(name, ref, player, zone, source, token)
        self.entities[ref]['counters'] = 0

    def observe(self, ref, property_name, value):
        self.emit('assert_card', ref=ref, property=property_name, value=int(value))

    def receipts(self):
        self.emit('refresh')
        start = len(self.steps)
        super().receipts()
        for assertion in self.steps[start:]:
            if assertion.get('property') == 'plus1_counters':
                assertion['value'] = self.entities[assertion['ref']]['counters']

    def counters(self, ref, count):
        entity = self.entities[ref]
        printed = self.o[entity['name']]
        entity.update(counters=count, power=int(printed['power']) + count, toughness=int(printed['toughness']) + count)

    def cast_with_choice(self, ref, decline, mode='accept'):
        from recipes import COLORS

        options = self.p['mana_options']
        price = options[1] if mode == 'pay_red' else options[0]
        denied = mode in ('insufficient_mana', 'wrong_color')
        if denied:
            incorrect = [0] * 6
            if mode == 'wrong_color':
                incorrect[COLORS['U']] = sum(price)
            price = incorrect
        self.pool(self.owner, price)
        self.emit('refresh')
        if not denied:
            self.emit('choose_dialog', player=self.owner, value=int(decline))
        self.emit('cast', ref=ref, value=int(not denied))
        self.emit('assert_unused_choices', value=0)
        self.emit('assert_stack', value=int(not denied))
        if denied:
            self.observe(ref, 'in_hand', 1)
            self.receipts()
            return None
        self.mana[self.owner] = [0] * 6
        for color in range(6):
            self.emit('assert_mana', player=self.owner, color=color, value=0)
        cut = len(self.steps)
        self.emit('resolve')
        self.arrive(ref)
        self.counters(ref, int(not decline))
        self.receipts()
        return cut

    def piper_entry(self, decline):
        from recipes import COLORS

        pool = [0] * 6
        pool[COLORS['G']] = 1
        self.pool(self.owner, pool)
        self.emit('refresh')
        self.emit('require_can_activate', ref='piper', value=1)
        self.emit('activate', ref='piper', value=1)
        self.emit('assert_stack', value=1)
        self.emit('assert_unused_choices', value=0)
        self.observe('source', 'in_hand', 1)
        self.emit('choose_zone_card', player=self.owner, value=self.rows[self.name]['csvid'])
        self.emit('choose_dialog', player=self.owner, value=int(decline))
        self.emit('resolve')
        self.emit('assert_unused_choices', value=0)
        self.mana[self.owner] = [0] * 6
        self.entities['piper']['tapped'] = True
        self.arrive('source')
        self.counters('source', int(not decline))
        self.emit('bind_permanent_copy', ref='source', player=self.owner, value=self.rows[self.name]['csvid'], index=0)
        self.receipts()

    def grow(self, ref):
        self.spell('Burst of Strength', 'growth', self.owner, ref)
        self.entities[ref]['tapped'] = False
        self.counters(ref, self.entities[ref]['counters'] + 1)
        self.receipts()

    def clear(self):
        self.emit('choose_target', player=self.owner, ref='source')
        self.emit('activate', ref='remover', value=1)
        self.emit('assert_unused_choices', value=0)
        self.emit('assert_stack', value=1)
        self.leave('remover', 'graveyard')
        self.emit('assert_zone_multiplicity', player=self.owner, zone='graveyard',
                  index=self.rows['Vampire Hexmage']['csvid'], value=1)
        self.emit('resolve')
        self.counters('source', 0)
        self.receipts()

    def bounce(self):
        self.spell('Unsummon', 'bounce', 1 - self.owner, 'source')
        self.leave('source', 'hand')
        self.emit('bind_hand_copy', ref='source', player=self.owner, value=self.rows[self.name]['csvid'], index=0)
        self.receipts()

    def combat(self, attacker, candidates=()):
        self.emit('begin_attackers')
        self.emit('attack', ref=attacker, value=1)
        attack = self.entities[attacker]
        attack.update(tapped=True, attacking=True)
        self.emit('finish_attackers')
        self.emit('begin_blockers')
        blocked_by = None
        for candidate in candidates:
            legal = not self.has(candidate, 'Unleash') or not self.entities[candidate]['counters']
            self.emit('block', ref=candidate, other=attacker, value=int(legal))
            self.observe(candidate, 'blocking_count', legal)
            if legal:
                blocked_by = candidate
        self.emit('advance_combat')
        self.observe(attacker, 'blocked', blocked_by is not None)
        self.emit('combat_damage')
        self.emit('resolve')
        self.emit('end_combat')
        if blocked_by:
            blocker = self.entities[blocked_by]
            blocker['damage'] += attack['power']
            attack['damage'] += blocker['power']
            for ref in (attacker, blocked_by):
                creature = self.entities[ref]
                if creature['damage'] >= creature['toughness']:
                    self.leave(ref, 'graveyard')
        else:
            self.life[1 - self.current] -= attack['power']
        attack['attacking'] = False
        self.receipts()


def scenario(world, mode):
    owner, enemy = world.owner, 1 - world.owner
    blocks = ('block_accept', 'block_decline', 'block_after_clear', 'decline_external_counter',
              'grow_other', 'double_counter', 'bounce_rechoose', 'piper_accept', 'piper_decline', 'separate_copies')
    world.add(world.name, 'source', owner, 'hand', source=True)
    if mode in blocks:
        world.add(ATTACKER, 'attacker', enemy)
    if mode == 'separate_copies':
        world.add(world.name, 'second', owner, 'hand', source=True)
    if mode in ('decline_external_counter', 'grow_other', 'double_counter'):
        world.add('Burst of Strength', 'growth', owner, 'hand')
    if mode == 'grow_other':
        world.add(ATTACKER, 'alternate', owner)
    if mode == 'block_after_clear':
        world.add('Vampire Hexmage', 'remover', owner)
    if mode == 'bounce_rechoose':
        world.add('Unsummon', 'bounce', enemy, 'hand')
    if mode.startswith('piper_'):
        world.add('Elvish Piper', 'piper', owner)

    world.emit('turn', player=owner, value=0)
    world.emit('present_ai_dialogs', value=1)
    for player in (0, 1):
        world.pool(player, [0] * 6)
    world.receipts()

    decline = mode in ('decline', 'block_decline', 'decline_external_counter', 'grow_other', 'piper_decline')
    cut = None
    if mode.startswith('piper_'):
        world.piper_entry(decline)
    else:
        cut = world.cast_with_choice('source', decline, mode)
    if mode in ('decline_external_counter', 'grow_other', 'double_counter'):
        world.grow('alternate' if mode == 'grow_other' else 'source')
    if mode == 'bounce_rechoose':
        world.bounce()
        world.cast_with_choice('source', True)
    if mode == 'separate_copies':
        world.cast_with_choice('second', True)
    if mode in blocks:
        world.next_turn()
        world.combat('attacker', ('source', 'second') if mode == 'separate_copies' else ('source',))
        if mode == 'block_after_clear':
            world.clear()
            world.age()
            world.combat('attacker', ('source',))
    if mode == 'boosted_attack':
        world.age()
        world.combat('source')
    return cut if mode == 'pending_snapshot' else None


def expand(name, csvid, declaration, params, selected, registry, oracle):
    from recipes import step

    check_variants(params, selected)
    validate_witnesses(oracle, registry)
    if profiles()['sources'][name]['registry']['csvid'] != csvid:
        raise ValueError('Unleash source CSVID changed')
    cases = []
    for variant in selected:
        mode, owner = variant.rsplit('_', 1)
        world = UnleashWorld(name, params, int(owner), registry, oracle)
        cut = scenario(world, mode)
        actions = world.steps
        if cut is not None:
            tail = actions[cut:]
            actions = (actions[:cut] + [step('capture')] + tail + [step('record_state'), step('ai_roundtrip'), step('restore')]
                       + copy.deepcopy(tail) + [step('assert_recorded_state')])
        cases.append(dict(name=f'UnleashCreatureRecipe_{csvid}_{variant}', card=name, fixture='empty_main', seed=123,
                          setup=world.setup, steps=actions, _recipe=dict(
                              family=FAMILY, version=2, variant=variant, params=declaration['params'], oracle_text=RULE,
                              untested_components=['mixed-pool hybrid payment menu',
                                                   'other simultaneous entry replacements and multiplayer'])))
    return cases
