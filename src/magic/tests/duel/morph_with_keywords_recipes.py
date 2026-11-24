"""Paid Morph with actual keyword, damage and spell interaction receipts."""

import copy
import json
from pathlib import Path

from attack_value_trigger_recipes import _AttackResourceWorld
from morph_creature_recipes import FACE_DOWN, MORPH


FAMILY = 'morph_with_keywords'
PROFILE = Path(__file__).parent / 'data/morph_with_keywords_wave8_profiles.json'
LIFELINK = 'Lifelink (Damage dealt by this creature also causes you to gain that much life.)'
SPIRIT_LINK = 'Whenever this creature deals damage, you gain that much life.'
AKROMA = (
    "This spell can't be countered.",
    'Flying, trample, protection from white and from blue',
    '{R}: Akroma gets +1/+0 until end of turn.',
)
PATTERNS = {
    (LIFELINK,): ('lifelink', ['Lifelink']),
    ('First strike',): ('first_strike', ['First strike']),
    ('Flying', SPIRIT_LINK): ('flying_life_trigger', ['Flying']),
    ('Flying',): ('flying', ['Flying']),
    ('Trample',): ('trample', ['Trample']),
    ("Defender (This creature can't attack.)",): ('defender', ['Defender']),
    ('Reach (This creature can block creatures with flying.)',): ('reach', ['Reach']),
    ('Defender, flying',): ('defender_flying', ['Defender', 'Flying']),
    ('Trample, hexproof',): ('trample_hexproof', ['Trample', 'Hexproof']),
    ("This creature can't be blocked.",): ('unblockable', []),
    ('This creature can block any number of creatures.',): ('any_blocks', []),
    AKROMA: ('akroma', ['Flying', 'Protection', 'Trample']),
}
HANDLERS = {
    'lifelink': {'manalinkeh_card_abzan_guide', 'manalinkeh_card_sidisis_pet'},
    'first_strike': {'manalinkeh_card_ainok_tracker'},
    'flying_life_trigger': {'manalinkeh_card_exalted_angel'},
    'flying': {'manalinkeh_card_sage_eye_harrier'},
    'trample': {'manalinkeh_card_snowhorn_rider'},
    'defender': {'manalinkeh_card_war_behemoth'},
    'reach': {'manalinkeh_card_pine_walker'},
    'defender_flying': {'manalinkeh_card_monastery_flock'},
    'trample_hexproof': {'manalinkeh_card_sagu_mauler'},
    'unblockable': {'manalinkeh_card_mystic_of_the_hidden_way'},
    'any_blocks': {'manalinkeh_card_ironfist_crusher'},
    'akroma': {'manalinkeh_card_akroma_angel_of_fury'},
}
SMALL = "Mons's Goblin Raiders"
LIBRARY = ('Forest', 'Mountain')
WITNESSES = {
    *LIBRARY, SMALL, 'Hill Giant', 'Wind Drake', 'Serra Angel', 'Giant Spider', 'Fog', 'Shock',
    'Hermetic Study', 'Counterspell', 'Counterbalance', 'Scornful Egotist', 'Swords to Plowshares',
    'Unsummon', 'Murder', 'Pacifism', 'Scryb Sprites',
}
DUMMIES = {'Hill Giant', SMALL, 'Wind Drake', 'Serra Angel', 'Giant Spider', 'Scryb Sprites'}
COMMON = (
    'printed_cast', 'hidden_cast', 'hidden_choice', 'cast_short', 'paid_reveal', 'reveal_short', 'wrong_color',
    'reveal_offturn', 'pending_damage', 'fresh_reveal', 'hidden_hit', 'revealed_hit', 'hidden_block',
    'hidden_countered', 'reveal_replay',
)
FLYING = ('flying_ground', 'flying_reach', 'flying_defends')
DEFENDER = ('defender_late_reveal',)
MULTIBLOCKS = {'multiple_three': 3, 'multiple_nine': 9, 'hidden_multiple': 3, 'flip_after_blocks': 3}
LIFE = ('blocked_life', 'noncombat_life', 'prevented_life')
EXTRA = {
    'lifelink': LIFE,
    'first_strike': ('first_strike_attack', 'first_strike_defense', 'first_strike_hidden'),
    'flying_life_trigger': FLYING + LIFE,
    'flying': FLYING,
    'trample': ('trample_hit', 'trample_prevented'),
    'defender': DEFENDER,
    'reach': ('block_flyer', 'hidden_flyer'),
    'defender_flying': DEFENDER + ('block_flyer', 'hidden_flyer'),
    'trample_hexproof': ('trample_hit', 'trample_prevented', 'hexproof_foe_denied', 'hexproof_own',
                         'hidden_hexproof_target', 'wrong_second_color'),
    'unblockable': ('blocked_face_up', 'flip_after_block'),
    'any_blocks': tuple(MULTIBLOCKS),
    'akroma': FLYING + (
        'trample_hit', 'protection_white_block', 'protection_blue_block', 'white_block_damage',
        'blue_block_damage', 'white_target', 'blue_target', 'black_target', 'white_aura_detaches',
        'red_pump_repeat', 'red_pump_expires', 'red_pump_short', 'red_pump_hidden',
    ),
}


def profiles():
    return json.loads(PROFILE.read_bytes())


def classify(record):
    from recipes import COLORS, cost_from_oracle

    if (record.get('layout') != 'normal' or record.get('card_faces') or record.get('selected_face')
            or record['type_line'].split(' — ')[0] not in ('Creature', 'Legendary Creature')):
        raise ValueError('complete normal Morph creature required')
    lines = record['oracle_text'].splitlines()
    scope = PATTERNS.get(tuple(lines[:-1]))
    if not scope:
        raise ValueError('unreviewed complete Morph companion clause')
    mode, keys = scope
    last = lines[-1]
    if mode == 'akroma':
        tail = 'Turn her face up any time for her morph cost.)'
        if not last.endswith(tail):
            raise ValueError('exact Akroma Morph reminder required')
        last = last.removesuffix(tail) + 'Turn it face up any time for its morph cost.)'
    match = MORPH.fullmatch(last)
    attributes = [record.get('power'), record.get('toughness')]
    if (not match or sorted(record['keywords']) != sorted(['Morph'] + keys)
            or any(type(n) is not str or not n.isascii() or not n.isdecimal() for n in attributes)):
        raise ValueError('printed Morph fee, keyword set and fixed P/T required')
    morph = cost_from_oracle(dict(record, mana_cost=match[1]))
    price = cost_from_oracle(record)
    if (price[0] < 3 and mode != 'defender_flying') or not any(morph[1:]):
        raise ValueError('reviewed curves require ordinary hidden and colored reveal payment')
    return dict(mode=mode, mana=price, morph=morph, power=int(attributes[0]), toughness=int(attributes[1]),
                keywords=keys, color=sum(1 << COLORS[c] for c in record['colors']))


def variants(params):
    mode = params['mode']
    result = [f'{scenario}_{owner}' for scenario in COMMON + EXTRA[mode] for owner in (0, 1)]
    if mode == 'akroma':
        result.append('uncounterable_reveal_1')
    return result


def spec(declaration, record):
    params = classify(record)
    pin = profiles()['sources'].get(record['name'])
    if (declaration.get('family') != FAMILY or type(declaration.get('version')) is not int
            or declaration['version'] != 2 or not pin or pin['oracle'] != record
            or declaration.get('oracle_text') != record['oracle_text']
            or json.dumps(declaration.get('params'), sort_keys=True) != json.dumps(params, sort_keys=True)):
        raise ValueError('exact typed complete Morph and keyword Oracle required')
    selected = declaration.get('variants')
    if (type(selected) is not list or not selected or any(type(s) is not str for s in selected)
            or len(set(selected)) != len(selected) or not set(selected) <= set(variants(params))):
        raise ValueError('approved distinct Morph keyword scenarios required')
    return dict(params, family=FAMILY, effect=FAMILY)


def validate_witnesses(oracle, registry):
    pins = profiles()
    if pins.get('version') != 1 or set(pins['witnesses']) != WITNESSES:
        raise ValueError('independent complete Morph keyword witness set required')
    for group in ('witnesses', 'sources'):
        for name, pin in pins[group].items():
            row = registry.get(name, {})
            if (pin['oracle'] != oracle.get(name) or pin['registry'] != row or row['kind'] != 'card'
                    or row['handler'] in ('NULL', None) or row['handler'] == 'card_dummy' and
                    (name not in DUMMIES or not pin.get('reviewed_native_characteristics'))):
                raise ValueError(f'out-of-scope Morph keyword binding: {name}')
            if group == 'sources' and row['handler'] not in HANDLERS[classify(oracle[name])['mode']]:
                raise ValueError(f'changed printed Morph handler: {name}')
    if pins['face_down']['csvid'] != FACE_DOWN or registry.get(pins['face_down']['name']) != pins['face_down']:
        raise ValueError('changed representation of actual face-down permanent')


class _World(_AttackResourceWorld):
    def __init__(self, name, params, owner, registry, oracle):
        super().__init__(name, params, owner, registry, oracle)
        self.foe = 1 - owner
        self.cards = {}
        self.hidden = False
        self.fog = False
        self.boost = 0

    def add(self, name, ref, player=None, zone='hand', source=False):
        player = self.owner if player is None else player
        super().add(name, ref, player, zone, source)
        self.cards[ref] = (name, player, zone)

    def move(self, ref, zone):
        name, player, old = self.cards[ref]
        if ref in self.entities:
            if zone == 'battlefield':
                self.arrive(ref)
            else:
                self.leave(ref, zone)
        else:
            self.zones[old][player].remove(name)
            self.zones[zone][player].append(name)
        self.cards[ref] = (name, player, zone)

    def observe(self, ref, prop, value):
        self.emit('assert_card', ref=ref, property=prop, value=int(value))

    def wallets(self):
        for player in (0, 1):
            for color, n in enumerate(self.mana[player]):
                self.emit('assert_mana', player=player, color=color, value=n)

    def identity(self):
        entity = self.entities['source']
        self.observe('source', 'csvid', FACE_DOWN if self.hidden else self.rows[self.name]['csvid'])
        self.observe('source', 'power', entity['power'])
        self.observe('source', 'toughness', entity['toughness'])

    def receipts(self):
        from generate_cases import TOKEN_PROPERTIES

        self.emit('refresh')
        start = len(self.steps)
        super().receipts()
        source_live = self.cards['source'][2] == 'battlefield'
        if not source_live:
            return
        printed = self.rows[self.name]['csvid']
        if self.hidden:
            for row in self.steps[start:]:
                if (row['op'] == 'assert_zone_multiplicity' and row['zone'] == 'battlefield'
                        and row['player'] == self.owner and row['index'] == printed):
                    row['index'] = FACE_DOWN
        self.identity()
        visible = FACE_DOWN if self.hidden else printed
        fields = {'color': 0 if self.hidden else self.p['color'], 'token': 0, 'artifact': 0}
        for subtype in self.o[self.name]['type_line'].partition(' — ')[2].split():
            if f'subtype_{subtype}' in TOKEN_PROPERTIES:
                fields[f'subtype_{subtype}'] = int(not self.hidden)
        for prop, value in fields.items():
            self.emit('assert_matching_permanents', player=self.owner, index=visible, property=prop, value=value)

    def expire(self):
        super().expire()
        self.entities['source']['power'] -= self.boost
        self.boost = 0
        self.fog = False

    def pending(self, ref, *, target=None, allowed=True, price=None, spend=None,
                response=False, dialog=None, generic_choices=()):
        from recipes import cost_from_oracle

        name, player, _ = self.cards[ref]
        budget = cost_from_oracle(self.o[name]) if price is None else price
        self.pool(player, budget)
        self.emit('refresh')
        if target is not None:
            self.emit('choose_target', ref=target, player=player)
        if dialog is not None:
            self.emit('choose_dialog', player=player, value=dialog)
        for color in generic_choices:
            self.emit('choose_mana', player=player, color=color)
        self.emit('cast_response' if response else 'cast', ref=ref, value=int(allowed))
        self.emit('assert_unused_choices', value=0)
        self.emit('assert_stack', value=(2 if response else 1) if allowed else 0)
        self.observe(ref, 'in_hand', not allowed)
        self.observe(ref, 'in_play', 0)
        if allowed:
            actual_fee = budget if spend is None else spend
            self.mana[player] = [have - fee for have, fee in zip(budget, actual_fee)]
        self.wallets()

    def source_cast(self, *, hidden, choice=False, short=False):
        print_cost = self.p['mana']
        hidden_cost = [3, 0, 0, 0, 0, 0]
        budget = [2, 0, 0, 0, 0, 0] if short else print_cost if choice or not hidden else hidden_cost
        menu = int(not hidden) if (choice or not hidden) and not short else None
        floating = list(hidden_cost)
        generic_choices = ()
        if hidden and choice and print_cost[0] < hidden_cost[0]:
            missing = hidden_cost[0] - print_cost[0]
            color, = [i for i in range(1, 6) if print_cost[i]]
            floating[0] -= missing
            floating[color] += missing
            if self.owner == 0:
                generic_choices = (color,) * missing
        self.pending('source', allowed=not short, price=budget,
                     spend=floating if hidden else print_cost, dialog=menu, generic_choices=generic_choices)
        if not short:
            self.hidden = hidden
            if hidden:
                self.entities['source'].update(power=2, toughness=2, keys=set())
            self.identity()

    def finish_spell(self, ref, zone):
        self.emit('resolve')
        self.move(ref, zone)
        self.emit('assert_unused_choices', value=0)
        self.receipts()

    def reveal(self, *, denials=None, response=False):
        cost = list(self.p['morph'])
        if denials == 'reveal_short':
            cost[next(i for i, count in enumerate(cost) if count)] -= 1
        if denials in ('wrong_color', 'wrong_second_color'):
            colored = [i for i in range(1, 6) if cost[i]]
            cost[colored[-1 if denials == 'wrong_second_color' else 0]] -= 1
            cost[next(i for i in range(1, 6) if not self.p['morph'][i])] += 1
        self.pool(self.owner, cost)
        self.emit('refresh')
        allowed = denials is None
        self.emit('can_activate', ref='source', value=int(allowed))
        self.emit('activate', ref='source', value=int(allowed))
        if allowed:
            self.hidden = False
            self.entities['source'].update(power=self.p['power'], toughness=self.p['toughness'],
                                           keys=set(self.p['keywords']))
            self.mana[self.owner] = [0] * 6
            # The paid callback changes the existing object immediately.
            # Drain its legacy bookkeeping only after those exact receipts.
            self.emit('refresh')
            self.identity()
            self.wallets()
            self.emit('resolve_response' if response else 'resolve')
        self.emit('assert_unused_choices', value=0)

    def lifegain(self, damage):
        if not self.hidden and self.p['mode'] in ('lifelink', 'flying_life_trigger'):
            self.life[self.owner] += damage

    def protection(self, incoming):
        name = self.cards[incoming][0]
        return (not self.hidden and self.p['mode'] == 'akroma'
                and not set(self.o[name]['colors']).isdisjoint('WU'))

    def fight(self, attacker, blocker=None, *, allowed_block=True, allowed_attack=True, reveal_after_block=False):
        actor = self.entities[attacker]['player']
        a = self.entities[attacker]
        self.emit('begin_attackers')
        self.emit('attack', ref=attacker, value=int(allowed_attack))
        if allowed_attack:
            a.update(attacking=True, tapped=not self.has(attacker, 'Vigilance'))
        self.emit('finish_attackers')
        self.emit('begin_blockers')
        if blocker:
            self.emit('block', ref=blocker, other=attacker, value=int(allowed_block))
            self.observe(blocker, 'blocking_count', allowed_block)
        self.emit('advance_combat')
        if reveal_after_block:
            self.reveal()
        if allowed_attack and not self.fog:
            if blocker and allowed_block:
                b = self.entities[blocker]
                spill = max(0, a['power'] - b['toughness']) if self.has(attacker, 'Trample') else 0
                outgoing = a['power'] - spill
                incoming = b['power']
                if self.has(attacker, 'First strike') and outgoing >= b['toughness']:
                    incoming = 0
                if self.has(blocker, 'First strike') and incoming >= a['toughness']:
                    outgoing = spill = 0
                if attacker == 'source' and self.protection(blocker):
                    incoming = 0
                if blocker == 'source' and self.protection(attacker):
                    outgoing = 0
                b['damage'] += outgoing
                a['damage'] += incoming
                self.life[1 - actor] -= spill
                self.lifegain(outgoing + spill if attacker == 'source' else incoming)
                for ref, obj in ((attacker, a), (blocker, b)):
                    if obj['damage'] >= obj['toughness']:
                        self.move(ref, 'graveyard')
            else:
                self.life[1 - actor] -= a['power']
                if attacker == 'source':
                    self.lifegain(a['power'])
        a['attacking'] = False
        self.emit('combat_damage')
        self.emit('resolve')
        self.emit('end_combat')
        self.receipts()

    def fight_many(self, attackers, *, hidden_blocking, late_flip):
        a = self.entities['source']
        self.emit('begin_attackers')
        for attacker in attackers:
            self.emit('attack', ref=attacker, value=1)
            enemy = self.entities[attacker]
            enemy.update(attacking=True, tapped=True)
        self.emit('finish_attackers')
        self.emit('begin_blockers')
        blocked = []
        for index, attacker in enumerate(attackers):
            legal = not hidden_blocking or index == 0
            self.emit('block', ref='source', other=attacker, value=int(legal))
            if legal:
                blocked.append(attacker)
            self.observe('source', 'blocking_count', len(blocked))
        self.emit('advance_combat')
        for attacker in attackers:
            self.observe(attacker, 'blocked', attacker in blocked)
        if late_flip:
            self.reveal()
            self.observe('source', 'blocking_count', len(blocked))
        if self.owner == 0 and len(blocked) > 1:
            # Actual damage selector spends one unit per choice when a
            # blocker faces multiple unbanded groups.
            for _ in range(a['power']):
                self.emit('choose_target', ref=blocked[0], player=self.owner)
        self.emit('combat_damage')
        a['damage'] = sum(self.entities[ref]['power'] for ref in blocked)
        self.life[self.owner] -= sum(self.entities[ref]['power'] for ref in attackers if ref not in blocked)
        source_survives = a['damage'] < a['toughness']
        if not source_survives:
            self.move('source', 'graveyard')
        self.emit('resolve')
        self.emit('end_combat')
        self.emit('assert_unused_choices', value=0)
        self.emit('assert_stack', value=0)
        self.wallets()
        for player in (0, 1):
            self.emit('assert_life', player=player, value=self.life[player])
        if source_survives:
            self.identity()
            self.observe('source', 'damage', a['damage'])
        else:
            self.emit('assert_zone_multiplicity', player=self.owner, zone='graveyard',
                      index=self.rows[self.name]['csvid'], value=1)
        self.emit('cleanup')
        if source_survives:
            self.observe('source', 'blocking_count', 0)

    def pump(self, allowed=True):
        from recipes import COLORS

        cost = [0] * 6
        cost[COLORS['R']] = int(allowed or self.hidden)
        self.pool(self.owner, cost)
        self.emit('refresh')
        self.emit('can_activate', ref='source', value=int(allowed))
        self.emit('activate', ref='source', value=int(allowed))
        self.emit('assert_stack', value=int(allowed))
        if allowed:
            self.mana[self.owner] = [0] * 6
            self.identity()
            self.wallets()
            self.emit('resolve')
            self.boost += 1
            self.entities['source']['power'] += 1
        self.receipts()


ATTACK_WITNESSES = {
    'hidden_block': SMALL, 'first_strike_attack': 'Hill Giant', 'first_strike_hidden': 'Hill Giant',
    'blocked_life': 'Giant Spider', 'flying_ground': 'Hill Giant', 'flying_reach': 'Giant Spider',
    'trample_hit': SMALL, 'trample_prevented': SMALL, 'protection_white_block': 'Serra Angel',
    'protection_blue_block': 'Wind Drake', 'white_block_damage': 'Serra Angel', 'blue_block_damage': 'Wind Drake',
    'blocked_face_up': 'Hill Giant', 'flip_after_block': SMALL,
}
DEFEND_WITNESSES = {'first_strike_defense': 'Hill Giant', 'flying_defends': 'Wind Drake',
                    'block_flyer': 'Wind Drake', 'hidden_flyer': 'Wind Drake'}
HIDDEN_END = {'hidden_cast', 'hidden_choice', 'hidden_hit', 'hidden_block', 'first_strike_hidden',
              'red_pump_hidden', 'white_block_damage', 'blue_block_damage', 'hidden_flyer',
              'defender_late_reveal', 'flip_after_block', 'hidden_multiple', 'flip_after_blocks',
              'hidden_hexproof_target'}
COMBAT = (set(ATTACK_WITNESSES) | set(DEFEND_WITNESSES)
          | {'hidden_hit', 'revealed_hit', 'prevented_life', 'fresh_reveal', 'red_pump_repeat', *DEFENDER, *MULTIBLOCKS})


def build_variant(name, params, scenario, owner, registry, oracle):
    from recipes import cost_from_oracle, step

    w = _World(name, params, owner, registry, oracle)
    foe = w.foe
    w.add(name, 'source', source=True)
    if scenario in ATTACK_WITNESSES:
        blocker = ('Scryb Sprites' if scenario == 'trample_hit' and params['mode'] == 'akroma'
                   else ATTACK_WITNESSES[scenario])
        w.add(blocker, 'blocker', foe, 'battlefield')
    if scenario in DEFEND_WITNESSES:
        w.add(DEFEND_WITNESSES[scenario], 'opposing', foe, 'battlefield')
    attackers = [f'attacker_{index}' for index in range(MULTIBLOCKS.get(scenario, 0))]
    for ref in attackers:
        w.add(SMALL, ref, foe, 'battlefield')
    for player in (0, 1):
        lib = ('Scornful Egotist', 'Mountain') if scenario == 'uncounterable_reveal' and player == foe else LIBRARY
        for i, card in enumerate(lib):
            w.add(card, f'lib_{player}_{i}', player, 'library')
    if scenario == 'pending_damage':
        w.add('Shock', 'shock', foe)
    elif scenario in ('prevented_life', 'trample_prevented'):
        w.add('Fog', 'fog', foe)
    elif scenario == 'noncombat_life':
        w.add('Hermetic Study', 'grant')
        w.add('Hill Giant', 'other_target', foe, 'battlefield')
    elif scenario == 'hidden_countered':
        w.add('Counterspell', 'counter', foe)
    elif scenario == 'uncounterable_reveal':
        w.add('Counterbalance', 'counterbalance', foe, 'battlefield')
    elif scenario in ('white_target', 'blue_target', 'black_target'):
        w.add({'white_target': 'Swords to Plowshares', 'blue_target': 'Unsummon', 'black_target': 'Murder'}[scenario],
              'targeting', foe)
    elif scenario == 'white_aura_detaches':
        w.add('Pacifism', 'aura', foe)
    elif scenario in ('hidden_hexproof_target', 'hexproof_foe_denied'):
        w.add('Murder', 'targeting', foe)
    elif scenario == 'hexproof_own':
        w.add('Shock', 'targeting', owner)
    w.emit('turn', player=owner, value=0)
    for player in (0, 1):
        w.pool(player, [0] * 6)
    if scenario == 'cast_short':
        w.source_cast(hidden=True, short=True)
        w.receipts()
        return w.setup, w.steps
    w.source_cast(hidden=scenario not in ('printed_cast', 'uncounterable_reveal'), choice=scenario == 'hidden_choice')
    if scenario == 'hidden_countered':
        w.emit('announce_spell', ref='source')
        w.pending('counter', response=True)
        w.emit('resolve_response')
        w.move('counter', 'graveyard')
        w.move('source', 'graveyard')
        w.receipts()
        return w.setup, w.steps
    if scenario == 'uncounterable_reveal':
        w.emit('choose_trigger_source', player=foe, ref='counterbalance')
        w.emit('choose_dialog', player=foe, value=0)
    w.finish_spell('source', 'battlefield')
    if scenario in ('printed_cast', 'hidden_cast', 'hidden_choice', 'uncounterable_reveal'):
        return w.setup, w.steps
    if (scenario in COMBAT and scenario != 'fresh_reveal') or scenario == 'noncombat_life':
        if scenario in DEFEND_WITNESSES or scenario in MULTIBLOCKS:
            w.next_turn()
        else:
            w.age()
    elif scenario in ('reveal_offturn', 'pending_damage', 'white_aura_detaches',
                       'hidden_hexproof_target', 'hexproof_foe_denied'):
        w.next_turn()
    if scenario == 'pending_damage':
        w.pending('shock', target='source')
    elif scenario == 'white_aura_detaches':
        w.pending('aura', target='source')
        w.finish_spell('aura', 'battlefield')
    if scenario == 'reveal_replay':
        w.emit('capture')
    start = len(w.steps)
    if scenario not in HIDDEN_END:
        w.reveal(denials=scenario if scenario in ('reveal_short', 'wrong_color', 'wrong_second_color') else None,
                 response=scenario == 'pending_damage')
        if scenario == 'pending_damage':
            w.move('shock', 'graveyard')
            w.entities['source']['damage'] = 0 if 'Hexproof' in params['keywords'] else 2
            if w.entities['source']['damage'] >= w.entities['source']['toughness']:
                w.move('source', 'graveyard')
        elif scenario == 'white_aura_detaches':
            w.move('aura', 'graveyard')
        w.receipts()
    if scenario == 'reveal_replay':
        tail = copy.deepcopy(w.steps[start:])
        w.steps.extend([step('record_state'), step('ai_roundtrip'), step('restore'), *tail,
                        step('assert_recorded_state')])
    if scenario in ('prevented_life', 'trample_prevented'):
        w.pending('fog')
        w.fog = True
        w.finish_spell('fog', 'graveyard')
    if scenario == 'noncombat_life':
        w.pending('grant', target='source')
        w.finish_spell('grant', 'battlefield')
        w.emit('require_can_activate', ref='grant', value=1)
        w.emit('choose_player', player=owner, value=foe)
        w.emit('activate', ref='grant', value=1)
        w.observe('source', 'tapped', 1)
        w.entities['source']['tapped'] = True
        w.emit('assert_stack', value=1)
        w.emit('resolve')
        w.life[foe] -= 1
        w.lifegain(1)
        w.receipts()
    if scenario in ('white_target', 'blue_target'):
        w.pool(foe, cost_from_oracle(oracle[w.cards['targeting'][0]]))
        w.emit('refresh')
        w.emit('can_cast', ref='targeting', value=0)
        w.observe('targeting', 'in_hand', 1)
        w.receipts()
    elif scenario == 'black_target':
        w.pending('targeting', target='source')
        w.emit('resolve')
        w.move('targeting', 'graveyard')
        w.move('source', 'graveyard')
        w.receipts()
    if scenario in ('red_pump_repeat', 'red_pump_expires', 'red_pump_short', 'red_pump_hidden'):
        for _ in range(2 if scenario == 'red_pump_repeat' else 1):
            w.pump(allowed=scenario not in ('red_pump_short', 'red_pump_hidden'))
        if scenario == 'red_pump_expires':
            w.age()
    if scenario == 'hexproof_foe_denied':
        price = cost_from_oracle(oracle['Murder'])
        w.pool(foe, price)
        w.emit('refresh')
        w.emit('can_cast', ref='targeting', value=0)
        w.pending('targeting', allowed=False, price=price)
        w.receipts()
    elif scenario == 'hidden_hexproof_target':
        w.pending('targeting', target='source')
        w.emit('resolve')
        w.move('targeting', 'graveyard')
        w.move('source', 'graveyard')
        w.receipts()
    elif scenario == 'hexproof_own':
        w.pending('targeting', target='source')
        w.entities['source']['damage'] = 2
        w.finish_spell('targeting', 'graveyard')
    if scenario in DEFEND_WITNESSES:
        w.fight('opposing', 'source', allowed_block=scenario != 'hidden_flyer')
    elif scenario in MULTIBLOCKS:
        w.fight_many(attackers, hidden_blocking=scenario in ('hidden_multiple', 'flip_after_blocks'),
                     late_flip=scenario == 'flip_after_blocks')
    elif scenario in COMBAT:
        blocker = 'blocker' if scenario in ATTACK_WITNESSES else None
        w.fight('source', blocker, allowed_block=scenario not in (
            'flying_ground', 'protection_white_block', 'protection_blue_block', 'blocked_face_up'),
            allowed_attack=scenario != 'fresh_reveal' and not (
                scenario == 'revealed_hit' and 'Defender' in params['keywords']),
            reveal_after_block=scenario in ('white_block_damage', 'blue_block_damage',
                                            'defender_late_reveal', 'flip_after_block'))
    return w.setup, w.steps


def expand(name, csvid, declaration, params, selected, registry, oracle):
    pin = profiles()['sources'].get(name)
    row = registry[name]
    if not pin or pin['registry'] != row or row['csvid'] != csvid or row['handler'] not in HANDLERS[params['mode']]:
        raise ValueError(f'unpinned Morph keyword source: {name}')
    result = []
    for variant in selected:
        scenario, player = variant.rsplit('_', 1)
        setup, steps = build_variant(name, params, scenario, int(player), registry, oracle)
        gaps = ['rules priority versus legacy activation bookkeeping', 'multiplayer and general hidden-information UI']
        if params['mode'] == 'akroma':
            gaps.append('uncounterability uses a nontargeted counter effect; targeted attempts against a face-up spell require a faithful legal-target bridge')
        if params['mode'] == 'any_blocks':
            gaps.append('Multiple blocks observe legal assignments, incoming damage and defending life; the AI outgoing damage distribution may vary.')
        result.append(dict(name=f'MorphWithKeywordsRecipe_{csvid}_{variant}', card=name, fixture='empty_main',
                           seed=123, setup=setup, steps=steps,
                           _recipe=dict(family=FAMILY, version=2, variant=variant, params=declaration['params'],
                                        oracle_text=declaration['oracle_text'], untested_components=gaps)))
    return result
