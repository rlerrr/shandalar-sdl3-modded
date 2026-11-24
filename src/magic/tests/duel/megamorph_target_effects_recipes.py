"""Paid Megamorph with persistent counters and independently checked recipients."""

import copy
import json
import re
from pathlib import Path

from morph_creature_recipes import FACE_DOWN
from morph_with_keywords_recipes import _World as _MorphWorld


FAMILY = 'megamorph_target_effects'
PROFILE = Path(__file__).parent / 'data/megamorph_target_effects_wave8_profiles.json'
MEGAMORPH = re.compile(r'Megamorph ((?:\{[0-9WUBRG]+\})+) \(You may cast this card face down as a '
                       r'2/2 creature for \{3\}\. Turn it face up any time for its megamorph cost and '
                       r'put a \+1/\+1 counter on it\.\)')
TURN_UP = 'When this creature is turned face up, '
BLOCK_RULE = "Creatures with power greater than this creature's power can't block it."
EFFECTS = {
    ((), 'destroy target artifact or enchantment an opponent controls.'): ('destroy_artifact_enchantment', None),
    (('Lifelink',), 'destroy target creature with power 4 or greater an opponent controls.'): ('destroy_high', 'Lifelink'),
    ((BLOCK_RULE,), 'destroy target creature with power 3 or less an opponent controls.'): ('destroy_low', None),
    ((), 'it deals 1 damage to any target.'): ('damage', None),
    ((), 'put a +1/+1 counter on another target creature you control.'): ('counter', None),
    (('Haste',), 'target creature gains haste until end of turn.'): ('haste', 'Haste'),
}
COMMON = ('normal_cast', 'hidden_choice', 'short_cast', 'reveal_short', 'reveal_wrong_color',
          'paid_reveal', 'persist_replay', 'offturn')
EXTRA = {
    'destroy_artifact_enchantment': ('enemy_enchantment', 'no_eligible', 'indestructible'),
    'destroy_high': ('no_eligible', 'combat_lifelink', 'hidden_no_lifelink'),
    'destroy_low': ('no_eligible', 'block_over', 'block_equal', 'block_dynamic', 'hidden_block'),
    'damage': ('own_player', 'damage_creature', 'damage_self', 'damage_planeswalker', 'prevented_player', 'prevented_creature'),
    'counter': ('no_eligible', 'recipient_persists'),
    'haste': ('grant_own_haste', 'grant_opposing_haste', 'grant_expires', 'printed_haste', 'hidden_no_haste'),
}
ARTIFACT = 'Fountain of Youth'
ENCHANTMENT = "Enchantress's Presence"
INDESTRUCTIBLE = 'Darksteel Citadel'
SMALL = 'Grizzly Bears'
THREE = 'Hill Giant'
FOUR = 'Rumbling Baloth'
HEXPROOF = 'Gladecover Scout'
WALKER = 'Jace Beleren'
TIM = 'Prodigal Sorcerer'
WITNESSES = {ARTIFACT, ENCHANTMENT, INDESTRUCTIBLE, SMALL, THREE, FOUR, HEXPROOF, WALKER, TIM,
             'Safe Passage', 'Giant Growth'}
DUMMIES = {SMALL, THREE, FOUR}


def profiles():
    return json.loads(PROFILE.read_bytes())


def classify(record):
    from recipes import COLORS, cost_from_oracle

    lines = record['oracle_text'].splitlines()
    if (record.get('layout') != 'normal' or record.get('card_faces') or record.get('selected_face')
            or not record['type_line'].startswith('Creature — ') or len(lines) < 2
            or not lines[-1].startswith(TURN_UP)):
        raise ValueError('normal Megamorph creature with a complete turn-up ability required')
    morph = MEGAMORPH.fullmatch(lines[-2])
    model = EFFECTS.get((tuple(lines[:-2]), lines[-1][len(TURN_UP):]))
    stats = (record.get('power'), record.get('toughness'))
    if (not morph or not model
            or any(type(value) is not str or not value.isascii() or not value.isdecimal() for value in stats)):
        raise ValueError('unsupported complete Megamorph trigger, fee, or creature statistics')
    mode, keyword = model
    keywords = [keyword] if keyword else []
    if sorted(record['keywords']) != sorted(['Megamorph', *keywords]):
        raise ValueError('printed keywords do not match the complete Megamorph clauses')
    return dict(mode=mode, mana=cost_from_oracle(record), morph=cost_from_oracle(dict(record, mana_cost=morph[1])),
                power=int(stats[0]), toughness=int(stats[1]), keywords=keywords,
                color=sum(1 << COLORS[color] for color in record['colors']))


def variants(params):
    return [f'{scenario}_{seat}' for scenario in COMMON + EXTRA[params['mode']] for seat in (0, 1)]


def spec(declaration, record):
    params = classify(record)
    pin = profiles()['sources'].get(record['name'])
    chosen = declaration.get('variants')
    if (declaration.get('family') != FAMILY or type(declaration.get('version')) is not int
            or declaration['version'] != 2 or not pin or pin['oracle'] != record
            or declaration.get('oracle_text') != record['oracle_text']
            or json.dumps(declaration.get('params'), sort_keys=True) != json.dumps(params, sort_keys=True)):
        raise ValueError('Megamorph parameters must match the independently pinned complete record')
    if (type(chosen) is not list or not chosen or any(type(value) is not str for value in chosen)
            or len(set(chosen)) != len(chosen) or not set(chosen) <= set(variants(params))):
        raise ValueError('approved distinct Megamorph variants required')
    return dict(params, family=FAMILY, effect=FAMILY)


def validate_witnesses(oracle, registry):
    pins = profiles()
    if pins.get('version') != 1 or set(pins['witnesses']) != WITNESSES:
        raise ValueError('complete independent Megamorph witness set required')
    for group, items in (('sources', pins['sources']), ('witnesses', pins['witnesses'])):
        for name, pin in items.items():
            row = registry.get(name, {})
            allowed_dummy = group == 'witnesses' and name in DUMMIES and pin.get('reviewed_native_characteristics')
            if (pin['oracle'] != oracle.get(name) or pin['registry'] != row or row.get('kind') != 'card'
                    or row.get('handler') in ('NULL', None) or row['handler'] == 'card_dummy' and not allowed_dummy):
                raise ValueError(f'changed or unsupported independent Megamorph binding: {name}')
            if group == 'sources':
                classify(oracle[name])
    identity = pins['face_down']
    if identity['csvid'] != FACE_DOWN or registry.get(identity['name']) != identity:
        raise ValueError('changed actual face-down object representation')


class _World(_MorphWorld):
    def __init__(self, name, params, owner, registry, oracle):
        super().__init__(name, params, owner, registry, oracle)
        self.counters = {}
        self.loyalty = None
        self.shield = None

    def receipts(self):
        start = len(self.steps)
        super().receipts()
        for action in self.steps[start:]:
            ref = action.get('ref')
            if action['op'] == 'assert_card' and ref in self.entities:
                if action['property'] == 'plus1_counters':
                    action['value'] = self.counters.get(ref, 0)
                elif action['property'] == 'sick' and self.has(ref, 'Haste'):
                    action['value'] = 0
        if self.loyalty is not None:
            self.emit('assert_counter', ref='walker', counter_kind='loyalty', value=self.loyalty)

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
            raise ValueError('Megamorph turn requires both wallets clear')
        self.emit('advance_turn')
        self.expire()
        self.current = 1 - self.current
        self.shield = None
        self.emit('untap_step', player=self.current)
        for entity in self.entities.values():
            if entity['zone'] == 'battlefield' and entity['player'] == self.current:
                entity.update(sick=False, tapped=False)
        self.emit('turn', player=self.current, value=0)
        self.receipts()

    def lifegain(self, damage):
        if self.has('source', 'Lifelink'):
            self.life[self.owner] += damage

    def add_counter(self, ref):
        self.counters[ref] = self.counters.get(ref, 0) + 1
        self.entities[ref]['power'] += 1
        self.entities[ref]['toughness'] += 1

    def legal(self, ref):
        name, controller, zone = self.cards[ref]
        types = self.o[name]['type_line'].split(' — ')[0].split()
        keywords = self.o[name]['keywords']
        mode = self.p['mode']
        if zone != 'battlefield' or 'Shroud' in keywords or controller != self.owner and 'Hexproof' in keywords:
            return False
        if mode == 'destroy_artifact_enchantment':
            return controller == self.foe and bool({'Artifact', 'Enchantment'}.intersection(types))
        if mode in ('destroy_high', 'destroy_low'):
            if controller != self.foe or ref not in self.entities:
                return False
            power = self.entities[ref]['power']
            return power >= 4 if mode == 'destroy_high' else power <= 3
        if mode == 'counter':
            return ref in self.entities and ref != 'source' and controller == self.owner
        if mode == 'haste':
            return ref in self.entities
        return ref in self.entities or 'Planeswalker' in types

    def effect(self, target, player):
        mode = self.p['mode']
        if player is not None:
            if player != self.shield:
                self.life[player] -= 1
        elif target is None:
            return
        elif mode.startswith('destroy_'):
            if 'Indestructible' not in self.o[self.cards[target][0]]['keywords']:
                self.move(target, 'graveyard')
        elif mode == 'counter':
            self.add_counter(target)
        elif mode == 'haste':
            self.entities[target]['grants'].add('Haste')
        elif target == 'walker':
            self.loyalty -= 1
        elif self.cards[target][1] != self.shield:
            creature = self.entities[target]
            creature['damage'] += 1
            if creature['damage'] >= creature['toughness']:
                self.move(target, 'graveyard')

    def reveal(self, *, target=None, player=None, probes=(), denied=None):
        fee = list(self.p['morph'])
        color = next(index for index in range(1, 6) if fee[index])
        if denied == 'reveal_short':
            fee[0 if fee[0] else color] -= 1
        elif denied == 'reveal_wrong_color':
            fee[color] -= 1
            fee[next(index for index in range(1, 6) if not self.p['morph'][index])] += 1
        self.pool(self.owner, fee)
        self.emit('refresh')
        self.emit('can_activate', ref='source', value=int(denied is None))
        if denied is None:
            for ref in probes:
                self.emit('probe_activation_target', ref='source', other=ref, index=-1, value=int(self.legal(ref)))
            if player is not None:
                self.emit('choose_player', player=self.owner, value=player)
            elif target is not None:
                if not self.legal(target):
                    raise ValueError(f'invalid Oracle Megamorph target: {target}')
                self.emit('choose_target', ref=target, player=self.owner)
        self.emit('activate', ref='source', value=int(denied is None))
        if denied is None:
            self.hidden = False
            self.mana[self.owner] = [0] * 6
            self.entities['source'].update(power=self.p['power'], toughness=self.p['toughness'], keys=set(self.p['keywords']))
            self.add_counter('source')
            self.emit('refresh')
            self.identity()
            self.emit('assert_counter', ref='source', counter_kind='plus1', value=1)
            self.wallets()
            self.emit('resolve')
            self.effect(target, player)
        self.emit('assert_unused_choices', value=0)
        self.receipts()


def target_fixture(w, scenario):
    mode, owner, foe = w.p['mode'], w.owner, w.foe
    target, player, probes = None, None, ()
    empty = scenario == 'no_eligible'
    if mode == 'destroy_artifact_enchantment':
        w.add(ARTIFACT, 'friendly_artifact', owner, 'battlefield')
        w.add(ENCHANTMENT, 'friendly_enchantment', owner, 'battlefield')
        w.add(SMALL, 'enemy_creature', foe, 'battlefield')
        if not empty:
            w.add(INDESTRUCTIBLE if scenario == 'indestructible' else ARTIFACT, 'enemy_artifact', foe, 'battlefield')
            w.add(ENCHANTMENT, 'enemy_enchantment', foe, 'battlefield')
            target = 'enemy_enchantment' if scenario == 'enemy_enchantment' else 'enemy_artifact'
        if scenario in ('paid_reveal', 'no_eligible'):
            probes = ('friendly_artifact', 'friendly_enchantment', 'enemy_creature')
    elif mode in ('destroy_high', 'destroy_low'):
        high = mode == 'destroy_high'
        w.add(FOUR if high else THREE, 'friendly_threshold', owner, 'battlefield')
        w.add(THREE if high else FOUR, 'enemy_wrong_power', foe, 'battlefield')
        if not empty and not scenario.startswith('block_') and scenario != 'hidden_block':
            w.add(FOUR if high else THREE, 'enemy_threshold', foe, 'battlefield')
            target = 'enemy_threshold'
        if scenario in ('paid_reveal', 'no_eligible'):
            probes = ('friendly_threshold', 'enemy_wrong_power')
    elif mode == 'counter':
        w.add(SMALL, 'foe_creature', foe, 'battlefield')
        if not empty:
            w.add(SMALL, 'friendly_creature', owner, 'battlefield')
            target = 'friendly_creature'
        if scenario in ('paid_reveal', 'no_eligible'):
            probes = ('source', 'foe_creature')
    elif mode == 'haste':
        if scenario in ('grant_own_haste', 'grant_opposing_haste', 'grant_expires'):
            w.add(TIM, 'recipient', owner if scenario == 'grant_own_haste' else foe)
            target = 'recipient'
        else:
            target = 'source'
    else:
        if scenario in ('damage_creature', 'prevented_creature'):
            w.add(HEXPROOF, 'own_scout', owner, 'battlefield')
            w.add(SMALL, 'victim', foe, 'battlefield')
            w.add(HEXPROOF, 'enemy_scout', foe, 'battlefield')
            target = 'victim'
            probes = ('own_scout', 'enemy_scout')
        elif scenario == 'damage_self':
            target = 'source'
        elif scenario == 'damage_planeswalker':
            w.add(WALKER, 'walker', foe)
            target = 'walker'
        else:
            player = owner if scenario == 'own_player' else foe
        if scenario.startswith('prevented_'):
            w.add('Safe Passage', 'shield', foe)
    return target, player, (*probes, target) if probes and target else probes


def build_variant(name, params, scenario, owner, registry, oracle):
    from recipes import step

    w = _World(name, params, owner, registry, oracle)
    w.add(name, 'source', source=True)
    target, player, probes = target_fixture(w, scenario)
    if scenario in ('block_over', 'block_dynamic'):
        blocker = 'enemy_wrong_power'
        if scenario == 'block_dynamic':
            w.add('Giant Growth', 'growth')
    elif scenario in ('block_equal', 'hidden_block'):
        blocker = 'blocker'
        w.add(THREE, blocker, w.foe, 'battlefield')
        if scenario == 'block_equal':
            w.add(SMALL, 'enemy_threshold', w.foe, 'battlefield')
            target = 'enemy_threshold'
    start = w.foe if scenario == 'damage_planeswalker' else owner
    w.current = start
    w.emit('turn', player=start, value=0)
    for seat in (0, 1):
        w.pool(seat, [0] * 6)
    if scenario == 'damage_planeswalker':
        w.pending('walker')
        w.loyalty = int(oracle[WALKER]['loyalty'])
        w.finish_spell('walker', 'battlefield')
        w.next_turn()
    w.source_cast(hidden=scenario not in ('normal_cast', 'printed_haste'),
                  competing=scenario == 'hidden_choice', short=scenario == 'short_cast')
    if scenario == 'short_cast':
        return w.setup, w.steps
    if scenario in ('offturn', 'grant_opposing_haste', 'grant_expires'):
        w.next_turn()
    elif scenario in ('combat_lifelink', 'hidden_no_lifelink', 'block_over', 'block_equal', 'block_dynamic', 'hidden_block'):
        w.next_turn()
        w.next_turn()
    if scenario in ('grant_own_haste', 'grant_opposing_haste', 'grant_expires'):
        w.pending('recipient')
        w.finish_spell('recipient', 'battlefield')
        w.emit('can_activate', ref='recipient', value=0)
    if scenario.startswith('prevented_'):
        w.pending('shield')
        w.shield = w.foe
        w.finish_spell('shield', 'graveyard')
    if scenario == 'persist_replay':
        w.emit('capture')
    cut = len(w.steps)
    stay = {'normal_cast', 'hidden_choice', 'printed_haste', 'hidden_no_haste', 'hidden_no_lifelink', 'hidden_block'}
    if scenario not in stay:
        denied = scenario if scenario in ('reveal_short', 'reveal_wrong_color') else None
        w.reveal(target=target, player=player, probes=probes, denied=denied)
    if scenario in ('persist_replay', 'recipient_persists'):
        w.next_turn()
        w.next_turn()
    if scenario == 'persist_replay':
        replay = copy.deepcopy(w.steps[cut:])
        w.steps.extend([step('record_state'), step('ai_roundtrip'), step('restore'), *replay, step('assert_recorded_state')])
    if scenario in ('combat_lifelink', 'hidden_no_lifelink', 'printed_haste', 'hidden_no_haste'):
        w.fight('source', allowed_attack=scenario != 'hidden_no_haste')
    elif scenario in ('block_over', 'block_equal', 'block_dynamic', 'hidden_block'):
        if scenario == 'block_dynamic':
            w.pending('growth', target='source')
            w.entities['source']['power'] += 3
            w.entities['source']['toughness'] += 3
            w.finish_spell('growth', 'graveyard')
        w.fight('source', blocker, allowed_block=scenario != 'block_over')
    elif scenario in ('grant_own_haste', 'grant_opposing_haste'):
        w.emit('can_activate', ref='recipient', value=1)
        w.fight('recipient')
    elif scenario == 'grant_expires':
        w.emit('can_activate', ref='recipient', value=1)
        w.next_turn()
        w.emit('can_activate', ref='recipient', value=0)
    return w.setup, w.steps


def expand(name, csvid, declaration, params, selected, registry, oracle):
    pin = profiles()['sources'].get(name)
    row = registry[name]
    if not pin or pin['registry'] != row or row['csvid'] != csvid:
        raise ValueError(f'unpinned Megamorph source: {name}')
    cases = []
    for variant in selected:
        scenario, owner = variant.rsplit('_', 1)
        setup, steps = build_variant(name, params, scenario, int(owner), registry, oracle)
        gaps = ['independent response or removal after the inline turn-up trigger', 'multiplayer']
        if params['mode'] == 'damage':
            gaps.append('battle targets, which have no supported battlefield target type')
        cases.append(dict(name=f'MegamorphTargetEffectsRecipe_{csvid}_{variant}', card=name, fixture='empty_main',
                          seed=123, setup=setup, steps=steps,
                          _recipe=dict(family=FAMILY, version=2, variant=variant, params=declaration['params'],
                                       oracle_text=declaration['oracle_text'], untested_components=gaps)))
    return cases
