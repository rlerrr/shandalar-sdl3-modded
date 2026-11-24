"""Detain through actual attack, block, activation, and start-turn rules."""

import copy
import json
import re
from pathlib import Path

from block_restriction_spells_recipes import _World as _SpellWorld


FAMILY = 'detain_effects'
PROFILE = Path(__file__).parent / 'data/detain_effects_wave8_profiles.json'
CLAUSE = re.compile(r'(When this creature enters, detain |Detain )(target|up to two target) '
                    r'(creature an opponent controls|creatures your opponents control|nonland permanent an opponent controls)\. '
                    r'\(Until your next turn, (that creature|those creatures|that permanent) can\x27t attack or block and '
                    r'(?:its|their) activated abilities can\x27t be activated\.\)')
COMMON = ('effect', 'attack_lock', 'activation_lock', 'mana_lock', 'expiry', 'no_target', 'short', 'wrong_color',
          'countered', 'replay')
PINGER = 'Prodigal Sorcerer'
MANA = 'Elvish Mystic'
BEAR = 'Grizzly Bears'
REACH = 'Giant Spider'
WITNESSES = {PINGER, MANA, BEAR, REACH, 'Forest', 'Mountain', 'Island', 'Sol Ring', 'Fountain of Youth',
             'Gladecover Scout', 'Counterspell', 'Boomerang', 'Icy Manipulator', 'Mawcor',
             'Shock', 'Pyroclasm', 'Hill Giant'}
DUMMIES = {BEAR, REACH, 'Hill Giant'}
DURATION = "(Until your next turn, those permanents can't attack or block and their activated abilities can't be activated.)"
CREATURE_DURATION = "(Until your next turn, that creature can't attack or block and its activated abilities can't be activated.)"
FOLLOW = {
    f"This creature can't be blocked.\nWhen this creature enters, detain target creature an opponent controls. {CREATURE_DURATION}":
        ('spirit', 'soulsworn_spirit', 'Creature — Spirit', []),
    f'At the beginning of your upkeep, detain target creature an opponent controls. {CREATURE_DURATION}':
        ('martial', 'martial_law', 'Enchantment', []),
    f'Protection from red\nWhen Lavinia enters, detain each nonland permanent your opponents control with mana value 4 or less. {DURATION}':
        ('lavinia', 'lavinia_of_the_tenth', 'Legendary Creature — Human Soldier', ['Protection']),
    f'Flying\nWhenever this creature attacks, detain up to two target nonland permanents your opponents control. {DURATION}':
        ('archon', 'archon_of_the_triumvirate', 'Creature — Archon', ['Flying']),
}
FOLLOW_VARIANTS = {
    'spirit': COMMON + ('source_removed', 'unblockable'),
    'martial': ('upkeep', 'alternate_targets', 'expiry', 'no_target', 'before_upkeep', 'source_removed', 'short', 'replay'),
    'lavinia': ('mass', 'artifact_cutoff', 'mana', 'no_targets', 'protection_target', 'protection_block',
                'protection_damage', 'expiry', 'short', 'replay'),
    'archon': ('attack_two', 'attack_one', 'zero', 'fresh', 'restraint', 'flying_ground', 'flying_reach',
               'expiry', 'short', 'replay'),
}


def profiles():
    return json.loads(PROFILE.read_bytes())


def classify(record):
    from recipes import cost_from_oracle

    lines = record['oracle_text'].splitlines()
    if record.get('layout') != 'normal' or record.get('card_faces') or record.get('selected_face') or not lines:
        raise ValueError('normal whole Detain permanent or sorcery required')
    follow = FOLLOW.get(record['oracle_text'])
    if follow:
        mode, callback, kind, keywords = follow
        if record['type_line'] != kind or sorted(record['keywords']) != sorted(['Detain', *keywords]):
            raise ValueError('changed complete Detain follow-on text or type')
        return dict(follow=mode, callback=callback, etb=mode in ('spirit', 'lavinia'), flying=mode == 'archon',
                    draw=False, maximum=2 if mode == 'archon' else 1, nonland=mode in ('lavinia', 'archon'),
                    mana=cost_from_oracle(record), keywords=keywords)
    flying = lines[0] == 'Flying'
    draw = lines[-1] == 'Draw a card.'
    remaining = lines[int(flying):len(lines)-int(draw)]
    clause = CLAUSE.fullmatch(remaining[0]) if len(remaining) == 1 else None
    if not clause:
        raise ValueError('full supported Detain clause and duration required')
    etb = clause[1].startswith('When')
    count = 2 if clause[2].startswith('up to') else 1
    nonland = clause[3].startswith('nonland')
    if (etb != record['type_line'].startswith('Creature — ') or not etb and record['type_line'] != 'Sorcery'
            or flying and not etb or draw and (etb or count != 1) or nonland and (not etb or count != 1)
            or (count == 2) != clause[3].startswith('creatures')
            or clause[4] != ('those creatures' if count == 2 else 'that permanent' if nonland else 'that creature')
            or sorted(record['keywords']) != sorted(['Detain', *(['Flying'] if flying else [])])):
        raise ValueError('unmodeled Detain cardinality, timing, targeting, or passive rules')
    return dict(etb=etb, flying=flying, draw=draw, maximum=count, nonland=nonland, mana=cost_from_oracle(record),
                keywords=['Flying'] if flying else [])


def handler(model):
    if 'follow' in model:
        name = model['callback']
    elif model['etb']:
        name = 'lyev_skyknight' if model['nonland'] else 'azorius_justiciar' if model['maximum'] == 2 else 'azorius_arrester'
    else:
        name = 'inaction_injunction' if model['draw'] else 'lyev_decree'
    return 'manalinkeh_card_' + name


def variants(model):
    if 'follow' in model:
        return [f'{scenario}_{seat}' for scenario in FOLLOW_VARIANTS[model['follow']] for seat in (0, 1)]
    scenarios = COMMON + (('source_removed',) if model['etb'] else ('target_lost',))
    if model['maximum'] == 2:
        scenarios += ('two', 'stop_after_one', 'zero')
    if model['maximum'] == 2 and not model['etb']:
        scenarios += ('partial_lost',)
    if model['flying']:
        scenarios += ('flying_ground', 'flying_reach')
    if model['nonland']:
        scenarios += ('artifact_mana', 'artifact_nonmana')
    if model['draw']:
        scenarios += ('one_card',)
    return [f'{scenario}_{seat}' for scenario in scenarios for seat in (0, 1)]


def spec(declaration, record):
    model = classify(record)
    pin = profiles()['sources'].get(record['name'])
    selected = declaration.get('variants')
    if (declaration.get('family') != FAMILY or type(declaration.get('version')) is not int
            or declaration['version'] != 2 or not pin or pin['oracle'] != record
            or declaration.get('oracle_text') != record['oracle_text']
            or json.dumps(declaration.get('params'), sort_keys=True) != json.dumps(model, sort_keys=True)):
        raise ValueError('exact pinned Detain spell and typed model required')
    if (type(selected) is not list or not selected or any(type(value) is not str for value in selected)
            or len(set(selected)) != len(selected) or not set(selected) <= set(variants(model))):
        raise ValueError('approved distinct Detain scenarios required')
    return dict(model, family=FAMILY, effect=FAMILY)


def validate_witnesses(oracle, registry):
    pins = profiles()
    if pins.get('version') != 1 or set(pins['witnesses']) != WITNESSES:
        raise ValueError('complete independent Detain witnesses required')
    for group in ('sources', 'witnesses'):
        for name, pin in pins[group].items():
            row = registry.get(name, {})
            reviewed_dummy = group == 'witnesses' and name in DUMMIES and pin.get('reviewed_native_characteristics')
            if (pin['oracle'] != oracle.get(name) or pin['registry'] != row or row.get('kind') != 'card'
                    or row.get('handler') in ('NULL', None) or row['handler'] == 'card_dummy' and not reviewed_dummy):
                raise ValueError(f'changed or unsupported Detain binding: {name}')
            if group == 'sources' and row['handler'] != handler(classify(oracle[name])):
                raise ValueError(f'changed Detain source handler: {name}')


class _World(_SpellWorld):
    def __init__(self, name, params, owner, registry, oracle):
        super().__init__(name, params, owner, registry, oracle)
        self.detained = set()
        self.artifact_taps = {}

    def receipts(self):
        super().receipts()
        for ref, tapped in self.artifact_taps.items():
            self.observe(ref, 'tapped', tapped)

    def next_turn(self):
        if 1 - self.current == self.owner:
            self.detained.clear()
        super().next_turn()

    def target_choices(self, targets, decline):
        for ref in targets:
            self.emit('choose_target', ref=ref, player=self.owner)
        if decline is not None:
            self.emit('choose_optional_target', ref=decline, player=self.owner)

    def enter_effect(self, scenario, chosen, decline):
        fee = list(self.p['mana'])
        color = next(index for index in range(1, 6) if fee[index])
        if scenario == 'short':
            fee[0 if fee[0] else color] -= 1
        elif scenario == 'wrong_color':
            fee[color] -= 1
            fee[next(index for index in range(1, 6) if not self.p['mana'][index])] += 1
        denied = (scenario in ('short', 'wrong_color')
                  or scenario == 'no_target' and not self.p['etb'] and self.p['maximum'] == 1)
        if not denied and not self.p['etb']:
            self.target_choices(chosen, decline)
        self.cast('source', budget=fee, denied=denied)
        if denied:
            self.receipts()
            return
        if scenario in ('countered', 'target_lost', 'partial_lost'):
            self.emit('announce_spell', ref='source')
            self.cast('reply', target=None if scenario == 'countered' else chosen[0], parent='source')
            self.finish('reply', 'graveyard', response=True)
            if scenario == 'countered':
                self.move('source', 'graveyard')
                self.receipts()
                return
            self.move(chosen[0], 'hand')
            chosen = chosen[1:]
        if self.p['etb']:
            self.target_choices(chosen, decline)
        self.finish('source', 'battlefield' if self.p['etb'] else 'graveyard')
        self.emit('assert_unused_choices', value=0)
        if decline is not None:
            self.emit('assert_forced_target_choices', value=0)
        self.detained.update(chosen)
        if self.p['draw'] and chosen:
            self.zones['hand'][self.owner].append(self.zones['library'][self.owner].pop(0))
        self.receipts()

    def ping(self, ref):
        actor = self.cards[ref][1]
        allowed = ref not in self.detained
        self.emit('refresh')
        self.emit('can_activate', ref=ref, value=int(allowed))
        self.emit('choose_player', player=actor, value=1-actor)
        self.emit('activate', ref=ref, value=int(allowed))
        self.emit('assert_unused_choices', value=int(not allowed))
        self.emit('assert_stack', value=int(allowed))
        self.emit('resolve')
        if allowed:
            self.entities[ref]['tapped'] = True
            self.life[1-actor] -= 1
        self.receipts()

    def mana_ability(self, ref, color, amount):
        actor = self.cards[ref][1]
        allowed = ref not in self.detained
        self.emit('refresh')
        self.emit('can_activate', ref=ref, value=int(allowed))
        self.emit('activate', ref=ref, value=int(allowed))
        if allowed:
            self.mana[actor][color] += amount
            if ref in self.entities:
                self.entities[ref]['tapped'] = True
            else:
                self.artifact_taps[ref] = True
        self.receipts()

    def artifact_ability(self, ref):
        actor = self.cards[ref][1]
        self.pool(actor, [2, 0, 0, 0, 0, 0])
        self.emit('refresh')
        self.emit('can_activate', ref=ref, value=0)
        self.emit('activate', ref=ref, value=0)
        self.emit('resolve')
        self.receipts()

    def fight(self, attacker, defender=None):
        self.emit('begin_attackers')
        can_attack = attacker not in self.detained
        self.emit('attack', ref=attacker, value=int(can_attack))
        if can_attack:
            self.entities[attacker].update(tapped=True, attacking=True)
        self.emit('finish_attackers')
        self.emit('begin_blockers')
        accepted = {}
        if defender is not None and can_attack:
            legal = (defender not in self.detained and (not self.has(attacker, 'Flying')
                     or self.has(defender, 'Flying') or self.has(defender, 'Reach')))
            self.emit('block', ref=defender, other=attacker, value=int(legal))
            self.observe(defender, 'blocking_count', legal)
            if legal:
                accepted[attacker] = defender
        self.emit('advance_combat')
        if can_attack:
            self.observe(attacker, 'blocked', bool(accepted))
        self.damage(((attacker, defender),) if can_attack else (), accepted)


def fixture(w, scenario):
    owner, foe = w.owner, w.foe
    w.add(w.name, 'source', owner, 'hand')
    w.add('Forest', 'land', foe)
    if w.p['draw']:
        w.add('Mountain', 'top', owner, 'library')
        if scenario != 'one_card':
            w.add('Island', 'next', owner, 'library')
    w.add(BEAR, 'attacker', owner)
    if scenario == 'no_target':
        if not w.p['nonland']:
            w.add('Gladecover Scout', 'protected', foe)
        return ()
    if scenario in ('artifact_mana', 'artifact_nonmana'):
        w.add('Sol Ring' if scenario == 'artifact_mana' else 'Fountain of Youth', 'target', foe)
        w.artifact_taps['target'] = False
        w.artifact_taps['land'] = False
    else:
        w.add(MANA if scenario == 'mana_lock' else PINGER, 'target', foe)
    if scenario in ('two', 'stop_after_one', 'partial_lost'):
        w.add(PINGER, 'second', foe)
    if scenario in ('activation_lock', 'mana_lock'):
        w.add(MANA if scenario == 'mana_lock' else PINGER, 'friendly', owner)
    if scenario in ('flying_ground', 'flying_reach'):
        w.add(REACH if scenario == 'flying_reach' else BEAR, 'blocker', foe)
    if scenario in ('countered', 'target_lost', 'partial_lost', 'source_removed'):
        w.add('Counterspell' if scenario == 'countered' else 'Boomerang', 'reply', foe, 'hand')
    return ('target', 'second') if scenario in ('two', 'partial_lost') else () if scenario == 'zero' else ('target',)


def build_variant(name, params, scenario, owner, registry, oracle):
    from recipes import COLORS, step

    w = _World(name, params, owner, registry, oracle)
    targets = fixture(w, scenario)
    w.emit('turn', player=owner, value=0)
    for seat in (0, 1):
        w.pool(seat, [0] * 6)
    w.receipts()
    if scenario == 'replay':
        w.emit('capture')
    cut = len(w.steps)
    declined = {'zero': 'target', 'stop_after_one': 'second'}.get(scenario)
    w.enter_effect(scenario, targets, declined)
    if scenario in ('effect', 'expiry', 'replay', 'one_card'):
        w.fight('attacker', 'target')
    if scenario in ('attack_lock', 'expiry'):
        w.next_turn()
        w.fight('target')
    if scenario == 'expiry':
        w.next_turn()
        w.emit('can_activate', ref='target', value=1)
        w.fight('attacker', 'target')
    if scenario in ('source_removed', 'activation_lock', 'attack_lock', 'zero', 'two', 'stop_after_one', 'partial_lost'):
        if scenario == 'source_removed':
            w.cast('reply', target='source')
            w.finish('reply', 'graveyard')
            w.move('source', 'hand')
            w.receipts()
        if scenario != 'partial_lost':
            w.ping('target')
        if scenario in ('two', 'stop_after_one', 'partial_lost'):
            w.ping('second')
        elif scenario == 'activation_lock':
            w.ping('friendly')
    if scenario == 'mana_lock':
        w.mana_ability('target', COLORS['G'], 1)
        w.mana_ability('friendly', COLORS['G'], 1)
    elif scenario == 'artifact_mana':
        w.mana_ability('target', 0, 2)
        w.mana_ability('land', COLORS['G'], 1)
    elif scenario == 'artifact_nonmana':
        w.artifact_ability('target')
        w.mana_ability('land', COLORS['G'], 1)
    elif scenario in ('flying_ground', 'flying_reach'):
        w.next_turn()
        w.next_turn()
        w.fight('source', 'blocker')
    if scenario == 'replay':
        tail = copy.deepcopy(w.steps[cut:])
        w.steps.extend([step('record_state'), step('ai_roundtrip'), step('restore'), *tail, step('assert_recorded_state')])
    return w.setup, w.steps


class _FollowWorld(_World):
    def cast_follow(self, scenario):
        denied = scenario == 'short'
        fee = list(self.p['mana'])
        if denied:
            fee[0] -= 1
        self.cast('source', budget=fee, denied=denied)
        if not denied:
            self.finish('source', 'battlefield')
            if self.p['follow'] == 'lavinia':
                self.detained.update(ref for ref, (name, seat, zone) in self.cards.items()
                                     if seat == self.foe and zone == 'battlefield'
                                     and 'Land' not in self.o[name]['type_line'].split(' — ')[0].split()
                                     and self.o[name]['cmc'] <= 4)
        self.receipts()

    def upkeep_visit(self, selected=None, *, check=None):
        if any(any(wallet) for wallet in self.mana):
            raise ValueError('real upkeep requires spent wallets')
        self.emit('advance_turn')
        self.expire()
        self.current = 1 - self.current
        if self.current == self.owner:
            self.detained.clear()
        self.emit('untap_step', player=self.current)
        for entity in self.entities.values():
            if entity['zone'] == 'battlefield' and entity['player'] == self.current:
                entity.update(tapped=False, sick=False)
        if check:
            self.emit('can_activate', ref=check, value=int(check not in self.detained))
        if selected:
            self.target_choices((selected,), None)
        self.emit('upkeep_step', player=self.current)
        self.emit('assert_unused_choices', value=0)
        if selected:
            self.detained.add(selected)
        self.emit('turn', player=self.current, value=0)
        self.receipts()

    def special_combat(self, attacker, *, blocker=None, allowed=True, blocked=False, targets=(), declined=None):
        self.emit('begin_attackers')
        self.emit('attack', ref=attacker, value=int(allowed))
        if allowed:
            self.entities[attacker].update(tapped=True, attacking=True)
        self.target_choices(targets, declined)
        self.emit('finish_attackers')
        self.emit('assert_unused_choices', value=0)
        if declined:
            self.emit('assert_forced_target_choices', value=0)
        self.detained.update(targets)
        self.emit('begin_blockers')
        if blocker:
            self.emit('block', ref=blocker, other=attacker, value=int(blocked))
            self.observe(blocker, 'blocking_count', blocked)
        self.emit('advance_combat')
        if allowed:
            self.observe(attacker, 'blocked', blocked)
        self.damage(((attacker, blocker),) if allowed else (), {attacker: blocker} if blocked else {})


def follow_fixture(w, scenario):
    owner, foe, mode = w.owner, w.foe, w.p['follow']
    w.add(w.name, 'source', owner, 'hand')
    w.add('Forest', 'land', foe)
    w.add(BEAR, 'attacker', owner)
    if mode == 'spirit':
        w.add(PINGER, 'target', foe)
        w.add(REACH, 'blocker', foe)
    elif mode == 'martial':
        if scenario == 'no_target':
            w.add('Gladecover Scout', 'protected', foe)
        else:
            w.add(PINGER, 'target', foe)
        if scenario == 'alternate_targets':
            w.add(PINGER, 'second', foe)
        if scenario == 'source_removed':
            w.add('Boomerang', 'reply', foe, 'hand')
    elif mode == 'archon':
        w.add(PINGER, 'target', foe)
        w.add('Sol Ring', 'artifact', foe)
        w.artifact_taps['artifact'] = False
        if scenario in ('flying_ground', 'flying_reach'):
            w.add(REACH if scenario == 'flying_reach' else BEAR, 'blocker', foe)
    elif scenario in ('mass', 'expiry', 'replay'):
        w.add(PINGER, 'target', foe)
        w.add('Gladecover Scout', 'protected', foe)
        w.add('Mawcor', 'over', foe)
        w.add(PINGER, 'friendly', owner)
    elif scenario == 'artifact_cutoff':
        w.add('Icy Manipulator', 'cutoff', foe)
        w.artifact_taps['cutoff'] = False
    elif scenario == 'mana':
        w.add(MANA, 'target', foe)
        w.artifact_taps['land'] = False
    elif scenario == 'no_targets':
        w.add('Mawcor', 'over', foe)
    elif scenario == 'protection_target':
        w.add('Shock', 'red_probe', foe, 'hand')
        w.add('Boomerang', 'blue_probe', foe, 'hand')
    elif scenario == 'protection_block':
        w.add('Hill Giant', 'blocker', foe)
    elif scenario == 'protection_damage':
        w.add('Pyroclasm', 'burn', foe, 'hand')


def martial_actions(w, scenario):
    w.upkeep_visit()
    if scenario == 'before_upkeep':
        w.ping('target')
        return
    selected = None if scenario == 'no_target' else 'target'
    w.upkeep_visit(selected)
    if selected is None:
        return
    if scenario == 'source_removed':
        w.cast('reply', target='source')
        w.finish('reply', 'graveyard')
        w.move('source', 'hand')
        w.receipts()
    w.fight('attacker', 'target')
    w.upkeep_visit()
    w.fight('target')
    if scenario in ('expiry', 'alternate_targets', 'source_removed'):
        if scenario == 'expiry':
            w.upkeep_visit('target', check='target')
            w.ping('target')
        else:
            w.upkeep_visit('second' if scenario == 'alternate_targets' else None, check='target')
            w.ping('target')
            if scenario == 'alternate_targets':
                w.ping('second')


def lavinia_actions(w, scenario):
    from recipes import COLORS

    if scenario in ('mass', 'replay'):
        w.fight('attacker', 'protected')
        for ref in ('target', 'over', 'friendly'):
            w.ping(ref)
    elif scenario == 'artifact_cutoff':
        w.pool(w.foe, [1, 0, 0, 0, 0, 0])
        w.emit('refresh')
        w.emit('can_activate', ref='cutoff', value=0)
        w.emit('choose_target', ref='land', player=w.foe)
        w.emit('activate', ref='cutoff', value=0)
        w.emit('assert_unused_choices', value=1)
        w.emit('resolve')
        w.receipts()
    elif scenario == 'mana':
        w.mana_ability('target', COLORS['G'], 1)
        w.mana_ability('land', COLORS['G'], 1)
    elif scenario == 'no_targets':
        w.ping('over')
    elif scenario == 'protection_target':
        for ref, eligible in (('red_probe', 0), ('blue_probe', 1)):
            w.emit('can_target_creature', ref=ref, other='source', value=eligible)
    elif scenario == 'protection_block':
        w.next_turn()
        w.next_turn()
        w.special_combat('source', blocker='blocker')
    elif scenario == 'protection_damage':
        w.next_turn()
        w.cast('burn')
        w.finish('burn', 'graveyard')
        w.move('attacker', 'graveyard')
        w.receipts()
    elif scenario == 'expiry':
        w.fight('attacker', 'target')
        w.next_turn()
        w.fight('target')
        w.next_turn()
        w.ping('target')


def archon_actions(w, scenario):
    if scenario == 'fresh':
        w.special_combat('source', allowed=False)
        w.ping('target')
        return
    w.next_turn()
    w.next_turn()
    if scenario == 'restraint':
        w.special_combat('attacker')
        w.ping('target')
    elif scenario == 'zero':
        w.special_combat('source', declined='target')
        w.ping('target')
        w.mana_ability('artifact', 0, 2)
    elif scenario in ('attack_two', 'replay'):
        w.special_combat('source', targets=('target', 'artifact'))
        w.ping('target')
        w.mana_ability('artifact', 0, 2)
    elif scenario in ('flying_ground', 'flying_reach'):
        w.special_combat('source', blocker='blocker', blocked=scenario == 'flying_reach',
                         targets=('target',), declined='blocker')
    else:
        w.special_combat('source', targets=('target',), declined='artifact')
        if scenario == 'expiry':
            w.next_turn()
            w.fight('target')
            w.next_turn()
            w.ping('target')
        else:
            w.ping('target')
            w.mana_ability('artifact', 0, 2)


def build_follow(name, params, scenario, owner, registry, oracle):
    from recipes import step

    mode = params['follow']
    if mode == 'spirit' and scenario != 'unblockable':
        return build_variant(name, params, scenario, owner, registry, oracle)
    w = _FollowWorld(name, params, owner, registry, oracle)
    follow_fixture(w, scenario)
    w.emit('turn', player=owner, value=0)
    for seat in (0, 1):
        w.pool(seat, [0] * 6)
    w.receipts()
    if scenario == 'replay':
        w.emit('capture')
    cut = len(w.steps)
    if mode == 'spirit':
        w.enter_effect(scenario, ('target',), None)
        w.next_turn()
        w.next_turn()
        w.special_combat('source', blocker='blocker')
    else:
        w.cast_follow(scenario)
        if scenario != 'short':
            {'martial': martial_actions, 'lavinia': lavinia_actions, 'archon': archon_actions}[mode](w, scenario)
    if scenario == 'replay':
        tail = copy.deepcopy(w.steps[cut:])
        w.steps.extend([step('record_state'), step('ai_roundtrip'), step('restore'), *tail, step('assert_recorded_state')])
    return w.setup, w.steps


def expand(name, csvid, declaration, params, selected, registry, oracle):
    pin = profiles()['sources'].get(name)
    row = registry[name]
    if not pin or pin['registry'] != row or row['csvid'] != csvid or row['handler'] != handler(params):
        raise ValueError(f'unpinned Detain source: {name}')
    cases = []
    for variant in selected:
        scenario, owner = variant.rsplit('_', 1)
        builder = build_follow if 'follow' in params else build_variant
        setup, steps = builder(name, params, scenario, int(owner), registry, oracle)
        gaps = ['no direct Detain marker observer', 'multiplayer']
        if params['etb']:
            gaps.append('independent response after the inline ETB target choice')
        if params['nonland']:
            gaps.append('separate loyalty and enchantment activation witnesses')
        cases.append(dict(name=f'DetainEffectsRecipe_{csvid}_{variant}', card=name, fixture='empty_main', seed=123,
                          setup=setup, steps=steps, _recipe=dict(family=FAMILY, version=2, variant=variant,
                                                               params=declaration['params'],
                                                               oracle_text=declaration['oracle_text'],
                                                               untested_components=gaps)))
    return cases
