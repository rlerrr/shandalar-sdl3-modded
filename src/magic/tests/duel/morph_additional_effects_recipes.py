"""Paid Morph with damage, modes, temporary control and owner-hand effects."""

import copy
import json
from pathlib import Path

from morph_creature_recipes import FACE_DOWN, MORPH
from morph_with_keywords_recipes import _World as _MorphWorld


FAMILY = 'morph_additional_effects'
PROFILE = Path(__file__).parent / 'data/morph_additional_effects_wave8_profiles.json'
PATTERNS = {
    ('Whenever this creature is dealt damage, you may create that many 1/1 green Insect creature tokens.',):
        ('damage_tokens', []),
    ('When this creature is turned face up, target creature gets +2/-2 or -2/+2 until end of turn.',):
        ('modal_stats', []),
    ("When this creature is turned face up, if it's your turn, gain control of another target creature until end of turn. "
     'Untap that creature. It gains haste until end of turn.',): ('temporary_control', []),
    ('Flying', "When this creature is turned face up, you may return another target creature to its owner's hand."):
        ('bounce', ['Flying']),
}
COMMON = ('printed', 'face_down_choice', 'face_down_short', 'reveal_short', 'reveal_wrong_color',
          'effect', 'offturn', 'replay')
EXTRA = {
    'damage_tokens': ('damage_two', 'damage_lethal', 'damage_prevented', 'face_down_damage', 'damage_other',
                      'combat_damage', 'two_packets'),
    'modal_stats': ('negative_power', 'lethal_toughness', 'self_target', 'expiry', 'target_protection'),
    'temporary_control': ('own_target', 'return_after_attack', 'source_removed'),
    'bounce': ('own_target', 'decline', 'no_other', 'to_owner', 'flying_ground', 'flying_reach', 'hidden_ground'),
}
INSECT = 'Insect'
SMALL = 'Grizzly Bears'
TIM = 'Prodigal Sorcerer'
LARGE = 'Hill Giant'
REACH = 'Giant Spider'
HEXPROOF = 'Gladecover Scout'
WITNESSES = {SMALL, TIM, LARGE, REACH, HEXPROOF, 'Shock', 'Giant Growth', 'Safe Passage', 'Twiddle',
             'Act of Treason'}
DUMMIES = {SMALL, LARGE, REACH}


def profiles():
    return json.loads(PROFILE.read_bytes())


def classify(record):
    from recipes import COLORS, cost_from_oracle

    lines = record['oracle_text'].splitlines()
    fees = [match[1] for line in lines if (match := MORPH.fullmatch(line))]
    other = tuple(line for line in lines if not MORPH.fullmatch(line))
    stats = (record.get('power'), record.get('toughness'))
    if (record.get('layout') != 'normal' or record.get('card_faces') or record.get('selected_face')
            or not record['type_line'].startswith('Creature — ') or len(fees) != 1 or other not in PATTERNS
            or any(type(value) is not str or not value.isascii() or not value.isdecimal() for value in stats)):
        raise ValueError('unsupported full Oracle Morph effect, fee, or statistics')
    mode, keywords = PATTERNS[other]
    if sorted(record['keywords']) != sorted(['Morph', *keywords]):
        raise ValueError('printed keywords must match the full Oracle clauses')
    return dict(mode=mode, mana=cost_from_oracle(record), morph=cost_from_oracle(dict(record, mana_cost=fees[0])),
                power=int(stats[0]), toughness=int(stats[1]), keywords=list(keywords),
                color=sum(1 << COLORS[color] for color in record['colors']))


def variants(params):
    mode = params['mode']
    planned = [f'{scenario}_{owner}' for scenario in COMMON + EXTRA[mode] for owner in (0, 1)]
    return planned + (['damage_decline_0'] if mode == 'damage_tokens' else [])


def spec(declaration, record):
    params = classify(record)
    pin = profiles()['sources'].get(record['name'])
    proposed = declaration.get('variants')
    if (declaration.get('family') != FAMILY or type(declaration.get('version')) is not int
            or declaration['version'] != 2 or not pin or pin['oracle'] != record
            or declaration.get('oracle_text') != record['oracle_text']
            or json.dumps(declaration.get('params'), sort_keys=True) != json.dumps(params, sort_keys=True)):
        raise ValueError('declaration must exactly match pinned Oracle and typed Morph parameters')
    if (type(proposed) is not list or not proposed or any(type(value) is not str for value in proposed)
            or len(set(proposed)) != len(proposed) or not set(proposed) <= set(variants(params))):
        raise ValueError('approved distinct Morph scenario names required')
    return dict(params, family=FAMILY, effect=FAMILY)


def validate_witnesses(oracle, registry):
    pins = profiles()
    if pins.get('version') != 1 or set(pins['witnesses']) != WITNESSES:
        raise ValueError('complete independent Morph witness set required')
    for scope in ('sources', 'witnesses'):
        for name, pin in pins[scope].items():
            row = registry.get(name, {})
            allowed_dummy = scope == 'witnesses' and name in DUMMIES and pin.get('reviewed_native_characteristics')
            if (pin['oracle'] != oracle.get(name) or pin['registry'] != row or row.get('kind') != 'card'
                    or row.get('handler') in ('NULL', None) or row['handler'] == 'card_dummy' and not allowed_dummy):
                raise ValueError(f'changed or unsupported Morph binding: {name}')
            if scope == 'sources':
                classify(oracle[name])
    if pins['face_down']['csvid'] != FACE_DOWN or registry.get(pins['face_down']['name']) != pins['face_down']:
        raise ValueError('changed face-down object representation')
    insect = pins['insect_engine_identity']
    if (insect != registry.get(INSECT) or insect['csvid'] != 18770 or insect['handler'] != 'manalinkeh_card_insect'):
        raise ValueError('changed binding of generated Insect tokens')


class _World(_MorphWorld):
    def __init__(self, name, params, owner, registry, oracle):
        super().__init__(name, params, owner, registry, oracle)
        self.insects = 0
        self.modifiers = {}
        self.borrowed = {}
        self.origins = {}

    def add(self, name, ref, player=None, zone='hand', source=False):
        super().add(name, ref, player, zone, source)
        self.origins[ref] = self.cards[ref][1]

    def receipts(self):
        start = len(self.steps)
        super().receipts()
        for action in self.steps[start:]:
            if action['op'] == 'assert_card' and action['property'] == 'sick' and self.has(action['ref'], 'Haste'):
                action['value'] = 0
        if self.p['mode'] == 'damage_tokens':
            from recipes import COLORS

            token_id = self.rows[INSECT]['csvid']
            for seat in (0, 1):
                self.emit('assert_zone_multiplicity', player=seat, zone='battlefield', index=token_id,
                          value=self.insects if seat == self.owner else 0)
            if self.insects:
                for prop, expectation in (('power', 1), ('toughness', 1), ('token', 1), ('color', 1 << COLORS['G']),
                                          ('artifact', 0), ('subtype_Insect', 1)):
                    self.emit('assert_matching_permanents', player=self.owner, index=token_id, property=prop,
                              value=expectation)

    def source_cast(self, *, hidden, competing=False, short=False):
        printed = self.p['mana']
        facedown = [3, 0, 0, 0, 0, 0]
        combined = [max(3, printed[0]), *printed[1:]]
        budget = [2, 0, 0, 0, 0, 0] if short else combined if competing else facedown if hidden else printed
        menu = 0 if competing else 1 if not hidden and sum(printed) >= 3 else None
        self.pending('source', allowed=not short, price=budget, spend=facedown if hidden else printed, dialog=menu)
        if not short:
            self.hidden = hidden
            if hidden:
                self.entities['source'].update(power=2, toughness=2, keys=set())
            self.identity()
            self.finish_spell('source', 'battlefield')

    def next_turn(self):
        if any(any(wallet) for wallet in self.mana):
            raise ValueError('Morph turn requires both wallets clear')
        self.emit('advance_turn')
        self.expire()
        for ref, mods in self.modifiers.items():
            self.entities[ref]['power'] -= mods[0]
            self.entities[ref]['toughness'] -= mods[1]
        self.modifiers.clear()
        for ref, seat in self.borrowed.items():
            if self.cards[ref][2] == 'battlefield':
                self.change_controller(ref, seat)
        self.borrowed.clear()
        self.current = 1 - self.current
        self.emit('untap_step', player=self.current)
        for entity in self.entities.values():
            if entity['zone'] == 'battlefield' and entity['player'] == self.current:
                entity.update(sick=False, tapped=False)
        self.emit('turn', player=self.current, value=0)
        self.receipts()

    def change_controller(self, ref, controller):
        name, previous, zone = self.cards[ref]
        self.zones[zone][previous].remove(name)
        self.zones[zone][controller].append(name)
        self.cards[ref] = (name, controller, zone)
        self.entities[ref].update(player=controller, sick=True)
        self.emit('bind_permanent_copy', ref=ref, player=controller, value=self.rows[name]['csvid'], index=0)

    def take(self, ref):
        previous = self.cards[ref][1]
        if previous != self.owner:
            self.borrowed[ref] = previous
            self.change_controller(ref, self.owner)
        self.entities[ref]['tapped'] = False
        self.entities[ref]['grants'].add('Haste')

    def move_owned(self, ref, destination):
        name, controller, _ = self.cards[ref]
        self.move(ref, destination)
        owner = self.origins[ref]
        if controller != owner:
            self.zones[destination][controller].remove(name)
            self.zones[destination][owner].append(name)
            self.cards[ref] = (name, owner, destination)
            self.entities[ref]['player'] = owner

    def modify(self, ref, power, toughness):
        self.modifiers[ref] = (power, toughness)
        self.entities[ref]['power'] += power
        self.entities[ref]['toughness'] += toughness

    def reveal(self, *, target=None, choice=0, declined=False, absent=False, probe=False, denied=None):
        fee = list(self.p['morph'])
        color = next(i for i in range(1, 6) if fee[i])
        if denied == 'reveal_short':
            fee[0 if fee[0] else color] -= 1
        elif denied:
            fee[color] -= 1
            fee[next(i for i in range(1, 6) if not self.p['morph'][i])] += 1
        self.pool(self.owner, fee)
        self.emit('refresh')
        self.emit('can_activate', ref='source', value=int(denied is None))
        mode = self.p['mode']
        if denied is None:
            if probe:
                if mode in ('bounce', 'temporary_control'):
                    self.emit('probe_activation_target', ref='source', other='source', index=-1, value=0)
                if 'protected' in self.cards:
                    self.emit('probe_activation_target', ref='source', other='protected', index=-1, value=0)
                if target:
                    self.emit('probe_activation_target', ref='source', other=target, index=-1, value=1)
            if absent:
                # Ordinarily there is no selection. A cancel safely exposes a
                # selector that wrongly offers Aven itself, without bouncing it.
                self.emit('choose_cancel_target', player=self.owner)
            elif target:
                self.emit('choose_optional_target' if declined else 'choose_target', ref=target, player=self.owner)
            if mode == 'modal_stats':
                self.emit('present_ai_dialogs', value=1)
                self.emit('choose_dialog', player=self.owner, value=choice)
        self.emit('activate', ref='source', value=int(denied is None))
        if denied is None:
            self.hidden = False
            self.mana[self.owner] = [0] * 6
            self.entities['source'].update(power=self.p['power'], toughness=self.p['toughness'], keys=set(self.p['keywords']))
            if mode == 'modal_stats':
                self.emit('present_ai_dialogs', value=0)
                delta = -2 if choice else 2
                self.modify(target, delta, -delta)
            self.emit('refresh')
            self.identity()
            self.wallets()
            self.emit('resolve')
            if target and not declined:
                if mode == 'temporary_control':
                    self.take(target)
                elif mode == 'bounce':
                    self.move_owned(target, 'hand')
                elif mode == 'modal_stats' and self.entities[target]['toughness'] <= 0:
                    self.move_owned(target, 'graveyard')
        self.emit('assert_unused_choices', value=int(absent and denied is None))
        if declined:
            self.emit('assert_forced_target_choices', value=0)
        self.receipts()

    def tap_victim(self):
        self.pending('twiddle', target='victim')
        self.emit('choose_dialog', player=self.owner, value=0)
        self.entities['victim']['tapped'] = True
        self.finish_spell('twiddle', 'graveyard')

    def grow_source(self):
        self.pending('growth', target='source')
        self.modify('source', 3, 3)
        self.finish_spell('growth', 'graveyard')

    def plan_insects(self, quantity, *, accept=True, lethal=False):
        if accept and quantity:
            if self.owner == 0:
                self.emit('choose_trigger' if lethal else 'choose_trigger_source', ref='source', player=self.owner)
            self.insects += quantity
            self.zones['battlefield'][self.owner].extend([INSECT] * quantity)

    def damage_packet(self, source, target, amount, *, accept=True, prevented=False):
        if source == 'pinger':
            self.emit('choose_target', ref=target, player=self.cards[source][1])
            self.emit('activate', ref=source, value=1)
            self.emit('assert_stack', value=1)
            self.entities[source]['tapped'] = True
        else:
            self.pending(source, target=target)
        dealt = 0 if prevented else amount
        victim = self.entities[target]
        victim['damage'] += dealt
        lethal = victim['damage'] >= victim['toughness']
        if target == 'source' and not self.hidden and self.p['mode'] == 'damage_tokens':
            self.plan_insects(dealt, accept=accept, lethal=lethal)
        self.emit('resolve')
        self.emit('assert_unused_choices', value=0)
        if source != 'pinger':
            self.move(source, 'graveyard')
        if lethal:
            self.move_owned(target, 'graveyard')
        self.receipts()

    def insect_combat(self):
        self.emit('begin_attackers')
        self.emit('attack', ref='victim', value=1)
        self.entities['victim']['tapped'] = True
        self.emit('finish_attackers')
        self.emit('begin_blockers')
        self.emit('block', ref='source', other='victim', value=1)
        self.observe('source', 'blocking_count', 1)
        self.emit('advance_combat')
        dealt = self.entities['victim']['power']
        self.plan_insects(dealt)
        self.emit('combat_damage')
        self.emit('resolve')
        self.emit('assert_unused_choices', value=0)
        self.emit('end_combat')
        self.entities['source']['damage'] += dealt
        self.move_owned('victim', 'graveyard')
        self.receipts()


def fixture(w, scenario):
    mode = w.p['mode']
    target = None
    if mode == 'damage_tokens':
        w.add(TIM, 'pinger', w.foe, 'battlefield')
        w.add('Shock', 'burn', w.foe)
        w.add('Giant Growth', 'growth', w.owner)
        w.add('Safe Passage', 'shield', w.owner)
        w.add(SMALL, 'victim', w.foe, 'battlefield')
    elif mode == 'modal_stats':
        w.add(SMALL if scenario == 'lethal_toughness' else REACH, 'victim', w.foe, 'battlefield')
        if scenario == 'target_protection':
            w.add(HEXPROOF, 'protected', w.foe, 'battlefield')
        target = 'source' if scenario == 'self_target' else 'victim'
    elif mode == 'temporary_control':
        own = scenario == 'own_target'
        w.add(LARGE, 'victim', w.owner if own else w.foe, 'hand' if own else 'battlefield')
        w.add('Twiddle', 'twiddle')
        if scenario == 'source_removed':
            w.add('Shock', 'burn', w.foe)
        target = None if scenario == 'offturn' else 'victim'
    else:
        if scenario == 'no_other':
            w.add(HEXPROOF, 'protected', w.foe, 'battlefield')
        else:
            w.add(SMALL, 'victim', w.owner if scenario == 'own_target' else w.foe, 'battlefield')
            target = 'victim'
        if scenario in ('flying_ground', 'flying_reach', 'hidden_ground'):
            w.add(REACH if scenario == 'flying_reach' else LARGE, 'blocker', w.foe, 'battlefield')
        if scenario == 'to_owner':
            w.add('Act of Treason', 'treason')
    return target


def build_variant(name, params, scenario, owner, registry, oracle):
    from recipes import step

    w = _World(name, params, owner, registry, oracle)
    mode = params['mode']
    w.add(name, 'source', source=True)
    target = fixture(w, scenario)
    w.emit('turn', player=owner, value=0)
    for seat in (0, 1):
        w.pool(seat, [0] * 6)
    w.source_cast(hidden=scenario != 'printed', competing=scenario == 'face_down_choice',
                  short=scenario == 'face_down_short')
    if scenario in ('printed', 'face_down_choice', 'face_down_short'):
        return w.setup, w.steps
    if scenario == 'offturn':
        w.next_turn()
    elif mode == 'bounce' and scenario in ('flying_ground', 'flying_reach', 'hidden_ground'):
        w.next_turn()
        w.next_turn()
    if scenario == 'own_target' and mode == 'temporary_control':
        w.pending('victim')
        w.finish_spell('victim', 'battlefield')
    if mode == 'temporary_control' and scenario not in ('reveal_short', 'reveal_wrong_color'):
        w.tap_victim()
    if scenario == 'to_owner':
        w.pending('treason', target='victim')
        w.emit('resolve')
        w.move('treason', 'graveyard')
        w.take('victim')
        w.receipts()
    if scenario == 'replay':
        w.emit('capture')
    cut = len(w.steps)
    denied = scenario if scenario in ('reveal_short', 'reveal_wrong_color') else None
    if scenario not in ('face_down_damage', 'hidden_ground'):
        w.reveal(target=target, choice=int(scenario == 'negative_power'), declined=scenario == 'decline',
                 absent=scenario == 'no_other', probe=scenario in ('effect', 'no_other', 'target_protection'), denied=denied)
    if not denied and mode == 'damage_tokens':
        if scenario not in ('damage_lethal', 'damage_prevented', 'face_down_damage', 'damage_other', 'combat_damage'):
            w.grow_source()
        if scenario == 'damage_prevented':
            w.pending('shield')
            w.finish_spell('shield', 'graveyard')
        if scenario == 'combat_damage':
            w.next_turn()
            # Cast on the defending player's window, after the cleanup that
            # would otherwise clear the toughness needed for the combat packet.
            w.grow_source()
            w.insect_combat()
        elif scenario in ('damage_two', 'damage_lethal', 'damage_prevented', 'two_packets'):
            w.damage_packet('burn', 'source', 2, prevented=scenario == 'damage_prevented')
            if scenario == 'two_packets':
                w.damage_packet('pinger', 'source', 1)
        else:
            w.damage_packet('pinger', 'victim' if scenario == 'damage_other' else 'source', 1,
                            accept=scenario != 'damage_decline')
    if scenario == 'expiry':
        w.next_turn()
    if mode == 'temporary_control' and scenario in ('own_target', 'return_after_attack', 'source_removed', 'replay'):
        if scenario == 'source_removed':
            w.damage_packet('burn', 'source', 2)
        w.fight('victim')
        w.next_turn()
    if mode == 'bounce' and scenario in ('flying_ground', 'flying_reach', 'hidden_ground'):
        w.fight('source', 'blocker', allowed_block=scenario != 'flying_ground')
    if scenario == 'replay':
        tail = copy.deepcopy(w.steps[cut:])
        w.steps.extend([step('record_state'), step('ai_roundtrip'), step('restore'), *tail, step('assert_recorded_state')])
    return w.setup, w.steps


def expand(name, csvid, declaration, params, selected, registry, oracle):
    pin = profiles()['sources'].get(name)
    row = registry[name]
    if not pin or pin['registry'] != row or row['csvid'] != csvid:
        raise ValueError(f'unpinned Morph source: {name}')
    cases = []
    for variant in selected:
        scenario, owner = variant.rsplit('_', 1)
        setup, steps = build_variant(name, params, scenario, int(owner), registry, oracle)
        gaps = ['multiplayer']
        if params['mode'] == 'damage_tokens':
            gaps.append('independent AI acceptance and decline of the Insect trigger')
        else:
            gaps.append('independent response or removal before inline turn-up resolution')
        cases.append(dict(name=f'MorphAdditionalEffectsRecipe_{csvid}_{variant}', card=name, fixture='empty_main',
                          seed=123, setup=setup, steps=steps,
                          _recipe=dict(family=FAMILY, version=2, variant=variant, params=declaration['params'],
                                       oracle_text=declaration['oracle_text'], untested_components=gaps)))
    return cases
