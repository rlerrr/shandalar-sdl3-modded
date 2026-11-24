"""Whole-Oracle constellation cases driven by actual battlefield entry."""

import copy
import json
from pathlib import Path

from attack_value_trigger_recipes import _AttackResourceWorld


FAMILY = 'constellation_entry_spell'
PROFILE = Path(__file__).parent / 'data/constellation_entry_spell_wave8_profiles.json'
ENTRY = 'Constellation — Whenever this creature or another enchantment you control enters, '
COINSMITH = '{W}{B}, Pay 1 life: Each opponent loses 1 life.'
EFFECTS = {
    'enemy_shrink': ('creatures your opponents control get -1/-1 until end of turn.', 'doomwake_giant'),
    'damage_one': ('this creature deals 1 damage to any target.', 'forgeborn_oreads'),
    'opponent_loses': ('each opponent loses 1 life.', 'grim_guardian'),
    'target_pump': ('target creature gets +1/+1 until end of turn.', 'oakheart_dryads'),
    'target_mill': ('target player mills two cards.', 'thassas_devourer'),
    'controller_gains': ('you gain 1 life.\n' + COINSMITH, 'underworld_coinsmith'),
    'cannot_block': ("target creature can't be blocked this turn.", 'whitewater_naiads'),
    'target_intimidate': ("target creature gains intimidate until end of turn. (It can't be blocked except by "
                          'artifact creatures and/or creatures that share a color with it.)', 'dreadbringer_lampads'),
    'target_prevents': ('prevent all damage that would be dealt to target creature this turn.', 'harvestguard_alseids'),
    'team_trample': ('creatures you control gain trample until end of turn.', 'humbler_of_mortals'),
    'graveyard_pump': ('target creature gets +X/+X until end of turn, where X is the number of creature cards in your '
                      'graveyard.', 'strength_from_the_fallen'),
    'opponent_discards': ('each opponent discards a card.', 'thoughtrender_lamia'),
}
ENCHANTMENT = 'Favorable Winds'
ENCHANTMENT_CREATURE = 'Nyxborn Triton'
FRAGILE = "Mons's Goblin Raiders"
LIBRARY = ('Forest', 'Island', 'Mountain', 'Plains', 'Swamp', 'Grizzly Bears', 'Hill Giant',
           'Forest', 'Mountain', 'Sol Ring', 'Island', 'Grizzly Bears')
WITNESSES = set(LIBRARY) | {ENCHANTMENT, ENCHANTMENT_CREATURE, FRAGILE, 'Safe Passage',
                           'Counterspell', 'Unsummon', 'Murder', 'Elvish Piper', 'Jace Beleren', 'Hussar Patrol',
                           'Wall of Tanglecord', 'Lightning Bolt', 'Disenchant'}
DUMMIES = {'Grizzly Bears', 'Hill Giant', FRAGILE}
COMMON = ('self_entry', 'own_enchantment', 'own_enchantment_creature', 'opponent_enchantment',
          'opponent_enchantment_creature', 'own_noncreature', 'own_creature', 'double_watchers', 'alternating_turns',
          'piper_entry', 'countered', 'source_removed', 'bounce_recast', 'wrong_color', 'unfunded', 'pending_replay')
EXTRA = {
    'enemy_shrink': ('shrink_lethal', 'shrink_expires', 'new_opponent_creature'),
    'damage_one': ('damage_own_player', 'damage_creature', 'damage_lethal', 'damage_walker', 'safe_passage'),
    'opponent_loses': ('safe_passage',),
    'target_pump': ('pump_opponent', 'pump_stacks', 'pump_expires'),
    'target_mill': ('mill_self', 'empty_library', 'short_library', 'repeat_mill'),
    'controller_gains': ('activate_paid', 'activate_repeat', 'activate_wrong_black', 'activate_wrong_white',
                         'activate_short'),
    'cannot_block': ('unblockable_combat', 'unblockable_expires'),
    'target_intimidate': ('intimidate_wrong_color', 'intimidate_matching_color', 'intimidate_artifact',
                          'intimidate_expires'),
    'target_prevents': ('protect_damage', 'protect_other', 'protect_enemy', 'protect_expires', 'protect_combat'),
    'team_trample': ('trample_team', 'trample_expires'),
    'graveyard_pump': ('zero_creatures', 'one_creature', 'opponent_graveyard', 'grave_pump_expires',
                      'graveyard_increases'),
    'opponent_discards': ('empty_hand', 'partial_hand', 'discard_sequence'),
}
TARGET_CREATURE = {'target_pump', 'cannot_block', 'target_intimidate', 'target_prevents', 'graveyard_pump'}


def profiles():
    return json.loads(PROFILE.read_bytes())


def classify(record):
    from recipes import cost_from_oracle

    if record.get('layout') != 'normal' or record.get('card_faces') or record.get('selected_face'):
        raise ValueError('whole normal constellation permanent required')
    enchantment = record['type_line'] == 'Enchantment'
    if not enchantment and not record['type_line'].startswith('Enchantment Creature — '):
        raise ValueError('standalone enchantment or enchantment creature required')
    prefix = ENTRY.replace('this creature', 'this enchantment') if enchantment else ENTRY
    mode = next((mode for mode, (tail, _) in EFFECTS.items() if record['oracle_text'] == prefix + tail), None)
    expected = ['Constellation', 'Mill'] if mode == 'target_mill' else ['Constellation']
    values = [record.get('power'), record.get('toughness')]
    if (mode is None or enchantment != (mode == 'graveyard_pump') or sorted(record.get('keywords', [])) != expected
            or not enchantment and any(not isinstance(value, str) or not value.isascii() or not value.isdecimal()
                                       for value in values)):
        raise ValueError('unsupported complete constellation text or statistics')
    return dict(mode=mode, power=None if enchantment else int(values[0]), toughness=None if enchantment else int(values[1]),
                mana=cost_from_oracle(record), keywords=expected)


def handler(params):
    return 'manalinkeh_card_' + EFFECTS[params['mode']][1]


def variants(params):
    mode = params['mode']
    excluded = {
        'graveyard_pump': {'bounce_recast'},
        'opponent_discards': {'countered', 'source_removed', 'bounce_recast', 'alternating_turns'},
    }.get(mode, ())
    return [f'{scenario}_{owner}' for scenario in COMMON + EXTRA[mode]
            if scenario not in excluded for owner in (0, 1)]


def spec(declaration, record):
    model = classify(record)
    pin = profiles()['sources'].get(record['name'])
    if (declaration.get('family') != FAMILY or type(declaration.get('version')) is not int
            or declaration['version'] != 2 or not pin or pin['oracle'] != record
            or declaration.get('oracle_text') != record['oracle_text']
            or json.dumps(declaration.get('params'), sort_keys=True) != json.dumps(model, sort_keys=True)):
        raise ValueError('complete pinned Oracle and typed constellation parameters required')
    selected = declaration.get('variants')
    if (type(selected) is not list or not selected or any(type(v) is not str for v in selected)
            or len(set(selected)) != len(selected) or not set(selected) <= set(variants(model))):
        raise ValueError('unreviewed constellation variant')
    return dict(model, family=FAMILY, effect=FAMILY)


def validate_witnesses(oracle, registry):
    profile = profiles()
    if profile.get('version') != 1 or set(profile['witnesses']) != WITNESSES:
        raise ValueError('exact independent constellation witnesses required')
    for group in ('sources', 'witnesses'):
        for name, pin in profile[group].items():
            row = registry.get(name, {})
            if (pin['oracle'] != oracle.get(name) or pin['registry'] != row or row.get('kind') != 'card'
                    or row.get('handler') in (None, 'NULL')
                    or row['handler'] == 'card_dummy' and
                    (name not in DUMMIES or not pin.get('reviewed_native_characteristics'))):
                raise ValueError(f'changed constellation Oracle or binding: {name}')
            if group == 'sources' and row['handler'] != handler(classify(oracle[name])):
                raise ValueError(f'changed source handler: {name}')


class _World(_AttackResourceWorld):
    def __init__(self, name, params, owner, registry, oracle):
        super().__init__(name, params, owner, registry, oracle)
        self.foe = 1 - owner
        self.mode = params['mode']
        self.cards = {}
        self.source_refs = set()
        self.watchers = []
        self.shield = set()
        self.protected = set()
        self.loyalty = {}

    def add(self, name, ref, player=None, zone='hand', source=False):
        player = self.owner if player is None else player
        super().add(name, ref, player, zone, source)
        self.cards[ref] = (name, player, zone)
        if source:
            self.source_refs.add(ref)
        if self.o[name].get('loyalty'):
            self.loyalty[ref] = int(self.o[name]['loyalty'])

    def move(self, ref, new):
        name, player, zone = self.cards[ref]
        if ref in self.entities:
            if new == 'battlefield':
                self.arrive(ref)
            else:
                self.leave(ref, new)
        else:
            self.zones[zone][player].remove(name)
            self.zones[new][player].append(name)
        self.cards[ref] = (name, player, new)

    def observe(self, ref, field, value):
        self.emit('assert_card', ref=ref, property=field, value=int(value))

    def receipts(self):
        self.emit('refresh')
        super().receipts()
        for ref, loyalty in self.loyalty.items():
            self.emit('assert_counter', ref=ref, counter_kind='loyalty', value=loyalty)

    def expire(self):
        super().expire()
        self.shield.clear()
        self.protected.clear()
        for obj in self.entities.values():
            record = self.o[obj['name']]
            obj.update(power=int(record['power']), toughness=int(record['toughness']))

    def pending(self, ref, *, allowed=True, response=False, denied=None):
        from recipes import cost_from_oracle

        name, player, _ = self.cards[ref]
        price = cost_from_oracle(self.o[name])
        if denied:
            price = [sum(price) if denied == 'wrong_color' else 0, 0, 0, 0, 0, 0]
        self.pool(player, price)
        self.emit('refresh')
        self.emit('cast_response' if response else 'cast', ref=ref, value=int(allowed))
        self.emit('assert_stack', value=(2 if response else 1) if allowed else 0)
        self.observe(ref, 'in_hand', not allowed)
        self.observe(ref, 'in_play', 0)
        if allowed:
            self.mana[player] = [0] * 6
        for color, amount in enumerate(self.mana[player]):
            self.emit('assert_mana', player=player, color=color, value=amount)

    def entry_watchers(self, ref):
        name, player, _ = self.cards[ref]
        if player != self.owner or 'Enchantment' not in self.o[name]['type_line'].partition(' — ')[0].split():
            return []
        active = [r for r in self.watchers if self.cards[r][2] == 'battlefield']
        if ref in self.source_refs and ref not in active:
            active.append(ref)
        return active

    def trigger_plan(self, ref, target):
        watchers = self.entry_watchers(ref)
        for index, watcher in enumerate(watchers):
            if self.owner == 0 and index < len(watchers) - 1:
                op = 'choose_trigger' if watcher == ref else 'choose_trigger_source'
                self.emit(op, ref=watcher, player=self.owner)
            if self.mode == 'target_mill' or self.mode == 'damage_one' and type(target) is int:
                self.emit('choose_player', player=self.owner, value=target)
            elif self.mode == 'damage_one' or self.mode in TARGET_CREATURE:
                self.emit('choose_target', player=self.owner, ref=target)
        return watchers

    def apply_trigger(self, target):
        if self.mode == 'opponent_loses':
            self.life[self.foe] -= 1
        elif self.mode == 'controller_gains':
            self.life[self.owner] += 1
        elif self.mode == 'target_mill':
            library = self.zones['library'][target]
            self.zones['graveyard'][target].extend(library[:2])
            del library[:2]
        elif self.mode == 'enemy_shrink':
            for ref, obj in self.entities.items():
                if obj['player'] == self.foe and obj['zone'] == 'battlefield':
                    obj['power'] = max(0, obj['power'] - 1)
                    obj['toughness'] -= 1
                    if obj['toughness'] <= 0:
                        self.move(ref, 'graveyard')
        elif self.mode == 'target_pump':
            obj = self.entities[target]
            obj['power'] += 1
            obj['toughness'] += 1
        elif self.mode == 'graveyard_pump':
            amount = sum('Creature' in self.o[name]['type_line'].partition(' — ')[0].split()
                         for name in self.zones['graveyard'][self.owner])
            obj = self.entities[target]
            obj['power'] += amount
            obj['toughness'] += amount
        elif self.mode == 'opponent_discards' and self.zones['hand'][self.foe]:
            hand = self.zones['hand'][self.foe]
            if len(set(hand)) != 1:
                raise ValueError('discard fixture requires an independently identifiable actual outcome')
            self.zones['graveyard'][self.foe].append(hand.pop())
        elif self.mode == 'target_prevents':
            self.protected.add(target)
        elif self.mode == 'target_intimidate':
            self.entities[target]['grants'].add('Intimidate')
        elif self.mode == 'team_trample':
            for obj in self.entities.values():
                if obj['player'] == self.owner and obj['zone'] == 'battlefield':
                    obj['grants'].add('Trample')
        elif self.mode == 'cannot_block':
            self.entities[target]['grants'].add('Cannot be blocked')
        elif self.mode == 'damage_one':
            if type(target) is int:
                if target not in self.shield:
                    self.life[target] -= 1
            elif target in self.loyalty:
                self.loyalty[target] -= 1
            else:
                creature = self.entities[target]
                if creature['player'] not in self.shield:
                    creature['damage'] += 1
                    if creature['damage'] >= creature['toughness']:
                        self.move(target, 'graveyard')

    def finish_entry(self, ref, watchers, target):
        self.move(ref, 'battlefield')
        if ref in self.source_refs and ref not in self.watchers:
            self.watchers.append(ref)
        if ref not in self.entities:
            self.observe(ref, 'in_play', 1)
        for _ in watchers:
            self.apply_trigger(target)
        self.emit('assert_unused_choices', value=0)
        self.receipts()

    def resolve_entry(self, ref, target):
        watchers = self.trigger_plan(ref, target)
        self.emit('resolve')
        self.finish_entry(ref, watchers, target)

    def enter(self, ref, target):
        self.pending(ref)
        self.resolve_entry(ref, target)

    def countered(self, ref):
        self.pending(ref)
        self.emit('announce_spell', ref=ref)
        self.pending('counter', response=True)
        self.emit('resolve_response')
        self.move(ref, 'graveyard')
        self.move('counter', 'graveyard')
        self.receipts()

    def new_opponent_creature(self):
        self.pending('first')
        self.emit('announce_spell', ref='first')
        self.pending('opposing', response=True)
        self.emit('announce_spell', ref='opposing')
        self.emit('resolve_response')
        self.move('opposing', 'battlefield')
        self.move('first', 'battlefield')
        self.receipts()

    def remove_source(self, bounce=False):
        name = 'Unsummon' if bounce else 'Disenchant' if self.mode == 'graveyard_pump' else 'Murder'
        self.spell(name, 'remove', self.foe, 'source')
        self.cards['remove'] = (name, self.foe, 'graveyard')
        self.move('source', 'hand' if bounce else 'graveyard')
        if bounce:
            self.emit('bind_hand_copy', ref='source', player=self.owner, index=0, value=self.rows[self.name]['csvid'])
        self.receipts()

    def piper_entry(self, target):
        from recipes import COLORS

        price = [0] * 6
        price[COLORS['G']] = 1
        self.pool(self.owner, price)
        self.emit('refresh')
        self.emit('require_can_activate', ref='piper', value=1)
        self.emit('activate', ref='piper', value=1)
        self.emit('assert_stack', value=1)
        self.mana[self.owner] = [0] * 6
        self.entities['piper']['tapped'] = True
        self.emit('choose_zone_card', player=self.owner, value=self.rows[ENCHANTMENT_CREATURE]['csvid'])
        watchers = self.trigger_plan('first', target)
        self.emit('resolve')
        self.emit('bind_permanent_copy', ref='first', player=self.owner, index=0,
                  value=self.rows[ENCHANTMENT_CREATURE]['csvid'])
        self.finish_entry('first', watchers, target)

    def coinsmith(self, scenario):
        from recipes import COLORS

        counts = [0] * 6
        for c in ('B', 'W') if scenario in ('activate_paid', 'activate_repeat') else {
                'activate_wrong_black': ('R', 'W'), 'activate_wrong_white': ('B', 'R'), 'activate_short': ('B',)
        }[scenario]:
            counts[COLORS[c]] = 1
        allowed = scenario in ('activate_paid', 'activate_repeat')
        self.pool(self.owner, counts)
        self.emit('refresh')
        self.emit('can_activate', ref='source', value=int(allowed))
        self.emit('activate', ref='source', value=int(allowed))
        if allowed:
            self.life[self.owner] -= 1
            self.mana[self.owner] = [0] * 6
        self.emit('assert_stack', value=int(allowed))
        for player in (0, 1):
            self.emit('assert_life', player=player, value=self.life[player])
            for color, amount in enumerate(self.mana[player]):
                self.emit('assert_mana', player=player, color=color, value=amount)
        if allowed:
            self.emit('resolve')
            self.life[self.foe] -= 1
        self.receipts()

    def blocker_combat(self):
        a, b = self.entities['own'], self.entities['enemy']
        legal = 'Cannot be blocked' not in a['grants']
        self.emit('begin_attackers')
        self.emit('attack', ref='own', value=1)
        a.update(tapped=True, attacking=True)
        self.emit('finish_attackers')
        self.emit('begin_blockers')
        self.emit('block', ref='enemy', other='own', value=int(legal))
        self.observe('enemy', 'blocking_count', legal)
        self.emit('advance_combat')
        self.emit('combat_damage')
        self.emit('resolve')
        self.emit('end_combat')
        if legal:
            b['damage'] += a['power']
            self.move('own', 'graveyard')
        else:
            self.life[self.foe] -= a['power']
            a['attacking'] = False
        self.receipts()

    def delta_combat(self, pairs):
        self.emit('begin_attackers')
        for attacker, _, _ in pairs:
            self.emit('attack', ref=attacker, value=1)
            self.entities[attacker].update(tapped=True, attacking=True)
        self.emit('finish_attackers')
        self.emit('begin_blockers')
        for attacker, blocker, blocked in pairs:
            self.emit('block', ref=blocker, other=attacker, value=int(blocked))
            self.observe(blocker, 'blocking_count', blocked)
        self.emit('advance_combat')
        self.emit('combat_damage')
        self.emit('resolve')
        self.emit('end_combat')
        for attacker, blocker, blocked in pairs:
            a, b = self.entities[attacker], self.entities[blocker]
            a['attacking'] = False
            if blocked:
                trample = 'Trample' in a['grants']
                dealt = min(a['power'], b['toughness'] - b['damage']) if trample else a['power']
                if trample:
                    self.life[self.foe] -= a['power'] - dealt
                if attacker not in self.protected:
                    a['damage'] += b['power']
                if blocker not in self.protected:
                    b['damage'] += dealt
                for ref, entity in ((attacker, a), (blocker, b)):
                    if entity['damage'] >= entity['toughness']:
                        self.move(ref, 'graveyard')
            else:
                self.life[self.foe] -= a['power']
        self.receipts()

    def bolt(self, target):
        player = 1 - self.cards[target][1]
        self.spell('Lightning Bolt', 'bolt', player, target)
        self.cards['bolt'] = ('Lightning Bolt', player, 'graveyard')
        creature = self.entities[target]
        if target not in self.protected:
            creature['damage'] += 3
            if creature['damage'] >= creature['toughness']:
                self.move(target, 'graveyard')
        self.receipts()

    def add_graveyard_fixture(self, scenario):
        count = {'zero_creatures': 0, 'one_creature': 1, 'opponent_graveyard': 0}.get(scenario, 2)
        self.add('Sol Ring', 'grave_noncreature', zone='graveyard')
        if scenario != 'zero_creatures':
            self.add('Hill Giant', 'opposing_grave_creature', self.foe, 'graveyard')
        if scenario == 'opponent_graveyard':
            self.add('Grizzly Bears', 'second_opposing_grave_creature', self.foe, 'graveyard')
        for i, creature in enumerate(('Grizzly Bears', 'Hill Giant')[:count]):
            self.add(creature, f'grave_creature_{i}', zone='graveyard')
        if scenario == 'graveyard_increases':
            self.add(ENCHANTMENT, 'first')
            self.add('Murder', 'grave_murder', self.foe)
            self.add(FRAGILE, 'reserve', zone='battlefield')

    def add_discard_fixture(self, scenario):
        if scenario in ('opponent_enchantment', 'opponent_enchantment_creature'):
            name = self.cards['opposing'][0]
            copies = 2
        else:
            name = 'Sol Ring'
            copies = {'empty_hand': 0, 'partial_hand': 1, 'double_watchers': 6}.get(scenario, 4)
        for i in range(copies):
            self.add(name, f'discard_{i}', self.foe)
        if scenario in ('partial_hand', 'discard_sequence'):
            self.add(ENCHANTMENT, 'first')
            if scenario == 'discard_sequence':
                self.add(ENCHANTMENT, 'second')


def build_variant(name, params, scenario, owner, registry, oracle):
    w = _World(name, params, owner, registry, oracle)
    w.add(name, 'source', source=True)
    w.add('Grizzly Bears', 'own', zone='battlefield')
    blocker = {'intimidate_matching_color': 'Grizzly Bears', 'intimidate_artifact': 'Wall of Tanglecord',
               'trample_team': FRAGILE, 'trample_expires': FRAGILE}.get(scenario, 'Hill Giant')
    w.add(blocker, 'enemy', player=w.foe, zone='battlefield')
    if scenario in ('trample_team', 'trample_expires'):
        w.add('Hill Giant', 'own_second', zone='battlefield')
        w.add(FRAGILE, 'enemy_second', w.foe, 'battlefield')
    if scenario in ('damage_lethal', 'shrink_lethal'):
        w.add(FRAGILE, 'fragile', player=w.foe, zone='battlefield')
    if scenario == 'damage_walker':
        w.add('Jace Beleren', 'walker', player=w.foe, zone='battlefield')
    if params['mode'] == 'target_mill':
        for player in (owner, w.foe):
            names = LIBRARY if player == owner or scenario not in ('empty_library', 'short_library') else (
                LIBRARY[:1] if scenario == 'short_library' else ())
            for index, card in enumerate(names):
                w.add(card, f'lib_{player}_{index}', player=player, zone='library')
    if scenario in ('own_enchantment', 'alternating_turns', 'double_watchers', 'countered',
                    'source_removed', 'bounce_recast', 'pump_stacks', 'repeat_mill'):
        w.add(ENCHANTMENT, 'first')
    elif scenario in ('own_enchantment_creature', 'piper_entry'):
        w.add(ENCHANTMENT_CREATURE, 'first')
    elif scenario == 'own_noncreature':
        w.add('Sol Ring', 'first')
    elif scenario == 'own_creature':
        w.add('Grizzly Bears', 'first')
    elif scenario == 'new_opponent_creature':
        w.add('Sol Ring', 'first')
        w.add('Hussar Patrol', 'opposing', w.foe)
    if scenario in ('opponent_enchantment', 'alternating_turns'):
        w.add(ENCHANTMENT, 'opposing', w.foe)
    elif scenario == 'opponent_enchantment_creature':
        w.add(ENCHANTMENT_CREATURE, 'opposing', w.foe)
    if scenario == 'double_watchers':
        w.add(name, 'copy', source=True)
    elif scenario in ('pump_stacks', 'repeat_mill'):
        w.add(ENCHANTMENT, 'second')
    elif scenario == 'safe_passage':
        w.add('Safe Passage', 'shield', w.foe)
    elif scenario in ('source_removed', 'bounce_recast'):
        removal = 'Unsummon' if scenario == 'bounce_recast' else 'Disenchant' if params['mode'] == 'graveyard_pump' else 'Murder'
        w.add(removal, 'remove', w.foe)
    elif scenario == 'countered':
        w.add('Counterspell', 'counter', w.foe)
    elif scenario == 'piper_entry':
        w.add('Elvish Piper', 'piper', zone='battlefield')
    if scenario in ('protect_damage', 'protect_other', 'protect_enemy', 'protect_expires'):
        w.add('Lightning Bolt', 'bolt', owner if scenario in ('protect_other', 'protect_enemy') else w.foe)
    if params['mode'] == 'graveyard_pump':
        w.add_graveyard_fixture(scenario)
    if params['mode'] == 'opponent_discards':
        w.add_discard_fixture(scenario)

    w.emit('turn', player=owner, value=0)
    w.pool(w.foe, [0] * 6)
    if params['mode'] == 'opponent_discards' and w.foe == 0:
        w.emit('discard_policy', value=1)
    default_target = 'own' if params['mode'] in TARGET_CREATURE else w.foe
    target = {
        'damage_own_player': owner, 'damage_creature': 'enemy', 'damage_lethal': 'fragile',
        'damage_walker': 'walker', 'pump_opponent': 'enemy', 'mill_self': owner, 'protect_enemy': 'enemy',
    }.get(scenario, default_target)
    if scenario in ('wrong_color', 'unfunded'):
        w.pending('source', allowed=False, denied=scenario)
        w.receipts()
        return w.setup, w.steps
    if scenario == 'safe_passage':
        w.pool(owner, [0] * 6)
        w.pending('shield')
        w.emit('resolve')
        w.move('shield', 'graveyard')
        w.shield.add(w.foe)
    w.pending('source')
    if scenario == 'pending_replay':
        w.emit('capture')
        cut = len(w.steps)
        w.resolve_entry('source', target)
        tail = copy.deepcopy(w.steps[cut:])
        w.emit('record_state')
        w.emit('ai_roundtrip')
        w.emit('restore')
        w.steps.extend(tail)
        w.emit('assert_recorded_state')
        return w.setup, w.steps
    w.resolve_entry('source', target)
    if scenario in ('own_enchantment', 'own_enchantment_creature', 'own_noncreature', 'own_creature'):
        w.enter('first', target)
    elif scenario == 'double_watchers':
        w.enter('copy', target)
        w.enter('first', target)
    elif scenario == 'alternating_turns':
        w.enter('first', target)
        w.next_turn()
        w.enter('opposing', target)
    elif scenario in ('opponent_enchantment', 'opponent_enchantment_creature'):
        w.next_turn()
        if params['mode'] == 'opponent_discards':
            w.emit('bind_hand_copy', ref='opposing', player=w.foe, index=0,
                   value=w.rows[w.cards['opposing'][0]]['csvid'])
        w.enter('opposing', target)
    elif scenario == 'piper_entry':
        w.piper_entry(target)
    elif scenario == 'countered':
        w.countered('first')
    elif scenario in ('source_removed', 'bounce_recast'):
        bounced = scenario == 'bounce_recast'
        w.remove_source(bounce=bounced)
        w.enter('first', target)
        if bounced:
            w.enter('source', target)
    elif scenario == 'new_opponent_creature':
        w.new_opponent_creature()
    elif scenario in ('pump_stacks', 'repeat_mill'):
        for ref in ('first', 'second'):
            w.enter(ref, target)
    elif scenario in ('shrink_expires', 'pump_expires', 'unblockable_expires'):
        w.age()
        if scenario == 'unblockable_expires':
            w.blocker_combat()
    elif scenario == 'unblockable_combat':
        w.blocker_combat()
    elif scenario.startswith('intimidate_'):
        if scenario == 'intimidate_expires':
            w.age()
        w.delta_combat([('own', 'enemy', scenario != 'intimidate_wrong_color')])
    elif scenario in ('trample_team', 'trample_expires'):
        if scenario == 'trample_expires':
            w.age()
        w.delta_combat([('own', 'enemy', True), ('own_second', 'enemy_second', True)])
    elif scenario == 'protect_combat':
        w.delta_combat([('own', 'enemy', True)])
    elif scenario in ('protect_damage', 'protect_other', 'protect_enemy', 'protect_expires'):
        if scenario == 'protect_expires':
            w.age()
        w.bolt('enemy' if scenario in ('protect_other', 'protect_enemy') else 'own')
    elif scenario == 'grave_pump_expires':
        w.age()
    elif scenario == 'graveyard_increases':
        w.spell('Murder', 'grave_murder', w.foe, 'reserve')
        w.cards['grave_murder'] = ('Murder', w.foe, 'graveyard')
        w.move('reserve', 'graveyard')
        w.receipts()
        w.enter('first', target)
    elif scenario in ('partial_hand', 'discard_sequence'):
        w.enter('first', target)
        if scenario == 'discard_sequence':
            w.enter('second', target)
    elif scenario.startswith('activate_'):
        for _ in range(2 if scenario == 'activate_repeat' else 1):
            w.coinsmith(scenario)
    return w.setup, w.steps


def expand(name, csvid, declaration, params, selected, registry, oracle):
    pin = profiles()['sources'].get(name)
    actual = registry[name]
    if not pin or pin['registry'] != actual or actual['csvid'] != csvid or actual['handler'] != handler(params):
        raise ValueError(f'unpinned constellation source: {name}')
    result = []
    for variant in selected:
        scenario, owner = variant.rsplit('_', 1)
        setup, steps = build_variant(name, params, scenario, int(owner), registry, oracle)
        gaps = ['post-target priority to a constellation trigger inside the real ETB drain', 'multiplayer targets']
        if params['mode'] == 'cannot_block':
            gaps.append('grant to an opposing attacker via an off-turn friendly enchantment')
        result.append(dict(name=f'ConstellationEntrySpellRecipe_{csvid}_{variant}', card=name,
                           fixture='empty_main', seed=123, setup=setup, steps=steps,
                           _recipe=dict(family=FAMILY, version=2, variant=variant, params=declaration['params'],
                                        oracle_text=declaration['oracle_text'], untested_components=gaps)))
    return result
