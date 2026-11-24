"""Paid Morph or Megamorph followed by the actual revealed activated ability."""

import copy
import json
import re
from pathlib import Path

from morph_creature_recipes import FACE_DOWN
from morph_with_keywords_recipes import _World as _MorphWorld


FAMILY = 'morph_faceup_activated'
PROFILE = Path(__file__).parent / 'data/morph_faceup_activated_wave8_profiles.json'
MANA = r'(?:\{[1-9BUGRW]\})+'
MORPH = re.compile(rf'(Morph|Megamorph) ({MANA}) \(You may cast this card face down as a 2/2 creature for \{{3\}}\. '
                   r'Turn it face up any time for its (morph cost|megamorph cost and put a \+1/\+1 counter on it)\.\)')
ABILITY = re.compile(rf'({MANA})(, \{{T\}})?: (.+)')
PUMP = re.compile(r'Target (Goblin|Beast) creature gets \+1/\+(0|1) until end of turn\.')
COMMON = ('printed', 'face_down_choice', 'short_cast', 'short_reveal', 'wrong_reveal_color', 'effect',
          'hidden_ability', 'replay', 'opposing_turn', 'short_ability', 'countered')
EXTRA = {
    'regen_self': ('regen_lethal', 'cannot_regenerate', 'no_response', 'proactive', 'regen_combat', 'spent_reuse'),
    'regen_target': ('regen_lethal', 'cannot_regenerate', 'no_response', 'proactive', 'regen_combat', 'spent_reuse',
                     'other_zombie', 'foe_zombie', 'non_zombie'),
    'tap': ('sick', 'untap_reuse', 'target_protection', 'own_target'),
    'pump': ('pump_enemy', 'pump_self', 'target_protection', 'typed_target', 'expiry', 'combat_power', 'repeated_pump'),
}
WITNESSES = {'Rumbling Baloth', 'Scathe Zombies', 'Goblin Roughrider', 'Grizzly Bears', 'Hill Giant', 'Gladecover Scout', 'Asceticism',
             'Murder', 'Lightning Bolt', 'Terminate', 'Counterspell'}
DUMMIES = {'Rumbling Baloth', 'Goblin Roughrider', 'Grizzly Bears', 'Hill Giant'}
TYPED = {'Goblin': 'Goblin Roughrider', 'Beast': 'Rumbling Baloth', 'Zombie': 'Scathe Zombies'}
SOURCE_HANDLERS = {
    ('regen_self', False): 'kin_tree_warden', ('regen_self', True): 'marang_river_skeleton',
    ('regen_target', False): 'boneknitter', ('tap', False): 'whipcorder',
    ('pump', 'Goblin'): 'goblin_taskmaster', ('pump', 'Beast'): 'snarling_undorak',
}


def profiles():
    return json.loads(PROFILE.read_bytes())


def classify(record):
    from recipes import COLORS, cost_from_oracle

    lines = record['oracle_text'].splitlines()
    stats = (record.get('power'), record.get('toughness'))
    if (record.get('layout') != 'normal' or record.get('card_faces') or record.get('selected_face')
            or not record['type_line'].startswith('Creature — ') or len(lines) != 2
            or any(type(value) is not str or not re.fullmatch('[0-9]+', value) for value in stats)):
        raise ValueError('normal two-clause Morph creature with numeric base stats required')
    morph = MORPH.fullmatch(lines[1])
    activation = ABILITY.fullmatch(lines[0])
    if not morph or not activation:
        raise ValueError('exact Morph and activated ability clauses required')
    keyword, reveal_cost, reminder = morph.groups()
    mega = keyword == 'Megamorph'
    if mega != reminder.startswith('megamorph'):
        raise ValueError('Morph reminder and keyword disagree')
    fee, tap, effect = activation.groups()
    bonuses = []
    subtype = None
    if effect == 'Regenerate this creature.':
        mode = 'regen_self'
    elif effect == 'Regenerate target Zombie.':
        mode, subtype = 'regen_target', 'Zombie'
    elif effect == 'Tap target creature.':
        mode = 'tap'
    elif match := PUMP.fullmatch(effect):
        mode, subtype = 'pump', match[1]
        bonuses = [1, int(match[2])]
        if int(match[2]) != int(subtype == 'Beast'):
            raise ValueError('unmodeled typed pump stats')
    else:
        raise ValueError('unmodeled face-up activated effect')
    expected = [keyword, 'Regenerate'] if mode.startswith('regen') else [keyword]
    if bool(tap) != (mode == 'tap') or sorted(expected) != sorted(record['keywords']):
        raise ValueError('unmodeled printed keywords or tap cost')
    if (mode, subtype if mode == 'pump' else mega) not in SOURCE_HANDLERS:
        raise ValueError('unverified source-effect combination')
    return dict(mode=mode, subtype=subtype, bonus=bonuses, megamorph=mega, mana=cost_from_oracle(record),
                morph=cost_from_oracle(dict(record, mana_cost=reveal_cost)), activation=cost_from_oracle(dict(record, mana_cost=fee)),
                power=int(stats[0]), toughness=int(stats[1]), keywords=[], color=sum(1 << COLORS[color] for color in record['colors']))


def handler(record):
    p = classify(record)
    return 'manalinkeh_card_' + SOURCE_HANDLERS[p['mode'], p['subtype'] if p['mode'] == 'pump' else p['megamorph']]


def variants(p):
    return [f'{scene}_{owner}' for scene in (*COMMON, *EXTRA[p['mode']]) for owner in (0, 1)]


def spec(declaration, record):
    p = classify(record)
    pin = profiles()['sources'].get(record['name'])
    names = declaration.get('variants')
    if (declaration.get('family') != FAMILY or type(declaration.get('version')) is not int
            or declaration['version'] != 2 or not pin or pin['oracle'] != record
            or declaration.get('oracle_text') != record['oracle_text']
            or json.dumps(declaration.get('params'), sort_keys=True) != json.dumps(p, sort_keys=True)):
        raise ValueError('exact pinned Morph/activation costs and clauses required')
    if (type(names) is not list or not names or any(type(name) is not str for name in names)
            or len(names) != len(set(names)) or not set(names) <= set(variants(p))):
        raise ValueError('distinct approved Morph/activation scenarios required')
    return dict(p, family=FAMILY, effect=FAMILY)


def validate_witnesses(oracle, registry):
    data = profiles()
    if data.get('version') != 1 or set(data['witnesses']) != WITNESSES:
        raise ValueError('complete independently pinned witness set required')
    for group in ('sources', 'witnesses'):
        for name, pin in data[group].items():
            row = registry.get(name, {})
            native = group == 'witnesses' and name in DUMMIES and pin.get('reviewed_native_characteristics')
            if (pin['oracle'] != oracle.get(name) or pin['registry'] != row or row.get('kind') != 'card'
                    or row.get('handler') in (None, 'NULL') or row['handler'] == 'card_dummy' and not native):
                raise ValueError(f'changed Morph source or witness: {name}')
            if group == 'sources' and row['handler'] != handler(oracle[name]):
                raise ValueError(f'changed face-up activation callback: {name}')
    proxy = data['face_down']
    if proxy['csvid'] != FACE_DOWN or registry.get(proxy['name']) != proxy:
        raise ValueError('changed native face-down proxy')


class _World(_MorphWorld):
    def __init__(self, name, params, owner, registry, oracle):
        super().__init__(name, params, owner, registry, oracle)
        self.counter = 0
        self.modifiers = {}

    def emit(self, op, **kw):
        if op == 'assert_card' and kw.get('ref') == 'source' and kw.get('property') == 'plus1_counters':
            kw['value'] = self.counter
        super().emit(op, **kw)

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

    def reveal(self, denied=None):
        budget = list(self.p['morph'])
        color = next(i for i in range(1, 6) if budget[i])
        if denied == 'short_reveal':
            budget[0 if budget[0] else color] -= 1
        elif denied:
            budget[color] -= 1
            budget[next(i for i in range(1, 6) if not self.p['morph'][i])] += 1
        self.pool(self.owner, budget)
        self.emit('refresh')
        self.emit('can_activate', ref='source', value=int(denied is None))
        self.emit('activate', ref='source', value=int(denied is None))
        if denied is None:
            self.hidden = False
            self.counter = int(self.p['megamorph'])
            self.entities['source'].update(power=self.p['power'] + self.counter, toughness=self.p['toughness'] + self.counter)
            self.mana[self.owner] = [0] * 6
            self.emit('refresh')
            self.identity()
            self.emit('assert_counter', ref='source', counter_kind='plus1', value=self.counter)
            self.wallets()
            self.emit('resolve')
        self.emit('assert_unused_choices', value=0)
        self.receipts()

    def next_main(self):
        if any(map(any, self.mana)):
            raise ValueError('Morph turn needs genuinely clear pools')
        self.emit('advance_turn')
        self.expire()
        for ref, (power, toughness) in self.modifiers.items():
            self.entities[ref]['power'] -= power
            self.entities[ref]['toughness'] -= toughness
        self.modifiers.clear()
        self.current = 1 - self.current
        self.emit('untap_step', player=self.current)
        for creature in self.entities.values():
            if creature['zone'] == 'battlefield' and creature['player'] == self.current:
                creature.update(tapped=False, sick=False)
        self.emit('turn', player=self.current, value=0)
        self.receipts()

    def use(self, target, *, available=True, budget=None, probes=()):
        cost = self.p['activation']
        budget = cost if budget is None else budget
        self.pool(self.owner, budget)
        self.emit('refresh')
        for ref, legal in probes:
            self.emit('probe_activation_target', ref='source', other=ref, index=-1, value=int(legal))
        self.emit('can_activate', ref='source', value=int(available))
        if available and target:
            self.emit('choose_target', ref=target, player=self.owner)
        self.emit('activate', ref='source', value=int(available))
        self.emit('assert_unused_choices', value=0)
        self.emit('assert_stack', value=int(available))
        if available:
            self.mana[self.owner] = [have - due for have, due in zip(budget, cost)]
            if self.p['mode'] == 'tap':
                self.entities['source']['tapped'] = True
                self.observe('source', 'tapped', 1)
            self.wallets()
            self.emit('resolve')
            if self.p['mode'] == 'tap':
                self.entities[target]['tapped'] = True
            elif self.p['mode'] == 'pump':
                old_power, old_toughness = self.modifiers.get(target, (0, 0))
                p, t = self.p['bonus']
                self.modifiers[target] = old_power + p, old_toughness + t
                self.entities[target]['power'] += p
                self.entities[target]['toughness'] += t
        self.receipts()

    def regeneration_plan(self, target, *, permitted):
        self.emit('choose_regeneration', ref='source')
        if permitted and self.p['mode'] == 'regen_target':
            self.emit('choose_target', ref=target, player=self.owner)

    def regenerate_receipt(self, target):
        self.mana[self.owner] = [have - due for have, due in zip(self.mana[self.owner], self.p['activation'])]
        self.entities[target].update(tapped=True, attacking=False, damage=0)
        self.observe(target, 'attacking', 0)

    def against_removal(self, ref, target, *, request=True, save=True):
        self.pending(ref, target=target)
        if request:
            self.regeneration_plan(target, permitted=save)
        self.emit('resolve')
        self.move(ref, 'graveyard')
        self.emit('assert_unused_choices', value=int(request and not save))
        if save:
            self.regenerate_receipt(target)
        else:
            self.move(target, 'graveyard')
        self.receipts()

    def combat_effect(self, attacker, blocker=None, *, regenerate=False):
        a = self.entities[attacker]
        self.emit('begin_attackers')
        self.emit('attack', ref=attacker, value=1)
        a.update(attacking=True, tapped=True)
        self.emit('finish_attackers')
        self.emit('begin_blockers')
        if blocker:
            self.emit('block', ref=blocker, other=attacker, value=1)
        self.emit('advance_combat')
        if regenerate:
            self.regeneration_plan(attacker, permitted=True)
        self.emit('combat_damage')
        self.emit('resolve')
        self.emit('assert_unused_choices', value=0)
        if blocker:
            b = self.entities[blocker]
            a['damage'] += b['power']
            b['damage'] += a['power']
            if b['damage'] >= b['toughness']:
                self.move(blocker, 'graveyard')
            if regenerate:
                self.regenerate_receipt(attacker)
            elif a['damage'] >= a['toughness']:
                self.move(attacker, 'graveyard')
        else:
            self.life[self.foe] -= a['power']
        self.emit('end_combat')
        a['attacking'] = False
        self.receipts()


def fixture(w, scene):
    p, mode, owner, foe = w.p, w.p['mode'], w.owner, w.foe
    w.add(w.name, 'source', source=True)
    if scene == 'countered':
        w.add('Counterspell', 'counter', foe)
    if mode.startswith('regen'):
        if scene in ('other_zombie','foe_zombie','proactive') and mode == 'regen_target':
            w.add('Scathe Zombies', 'recipient', foe if scene == 'foe_zombie' else owner, 'battlefield')
        elif scene == 'non_zombie':
            w.add('Grizzly Bears', 'recipient', owner, 'battlefield')
        if scene == 'regen_combat':
            w.add('Hill Giant', 'blocker', foe, 'battlefield')
        else:
            spell = {'cannot_regenerate': 'Terminate', 'regen_lethal': 'Lightning Bolt'}.get(scene, 'Murder')
            w.add(spell, 'removal', foe)
            if scene == 'spent_reuse':
                w.add('Murder', 'second_removal', foe)
    else:
        witness = TYPED.get(p['subtype'], 'Hill Giant')
        actor = foe if mode == 'tap' and scene != 'own_target' or scene == 'pump_enemy' else owner
        w.add(witness, 'recipient', actor, 'battlefield')
        if scene == 'target_protection':
            if mode == 'tap':
                w.add('Gladecover Scout', 'protected', foe, 'battlefield')
            else:
                w.add(witness, 'protected', foe, 'battlefield')
                w.add('Asceticism', 'hexproof', foe, 'battlefield')
        if scene == 'typed_target':
            w.add('Grizzly Bears', 'wrong_type', foe, 'battlefield')
        if scene == 'combat_power' and p['subtype'] == 'Beast':
            w.add('Rumbling Baloth', 'blocker', foe, 'battlefield')


def run_regeneration(w, scene):
    fee = list(w.p['activation'])
    target = 'recipient' if 'recipient' in w.cards else 'source'
    if scene == 'proactive':
        w.use(target if w.p['mode'] == 'regen_target' else None)
        w.emit('destroy', ref=target)
        w.entities[target].update(tapped=True, attacking=False, damage=0)
        w.receipts()
        return
    if scene == 'regen_combat':
        w.next_main()
        w.next_main()
        w.pool(w.owner, fee)
        w.combat_effect('source', 'blocker', regenerate=True)
        return
    if scene == 'short_ability':
        fee[next(i for i, amount in enumerate(fee) if amount)] -= 1
    w.pool(w.owner, fee)
    save = scene not in ('short_ability', 'hidden_ability', 'cannot_regenerate', 'no_response', 'non_zombie')
    w.against_removal('removal', target, request=scene != 'no_response', save=save)
    if scene == 'spent_reuse':
        w.against_removal('second_removal', target, save=False)


def run_tap(w, scene):
    fee = list(w.p['activation'])
    if scene == 'sick':
        w.use(None, available=False)
        return
    w.next_main()
    w.next_main()
    if scene == 'opposing_turn':
        w.next_main()
    if scene == 'short_ability':
        fee[5] -= 1
        w.use(None, available=False, budget=fee)
        return
    probes = [('protected', False), ('recipient', True)] if scene == 'target_protection' else []
    w.use('recipient', probes=probes)
    if scene == 'untap_reuse':
        w.use(None, available=False)
        # Explicit test funding only. Both real turn transitions require clear pools.
        w.pool(w.owner, [0] * 6)
        w.next_main()
        w.next_main()
        w.use('recipient')


def run_pump(w, scene):
    if scene == 'opposing_turn':
        w.next_main()
    if scene == 'short_ability':
        fee = list(w.p['activation'])
        fee[0] -= 1
        w.use(None, available=False, budget=fee)
        return
    target = 'source' if scene == 'pump_self' else 'recipient'
    probes = [('wrong_type', False), (target, True)] if scene == 'typed_target' else []
    if scene == 'target_protection':
        probes = [('protected', False), (target, True)]
    w.use(target, probes=probes)
    if scene == 'repeated_pump':
        w.use(target)
    elif scene == 'expiry':
        w.next_main()
    elif scene == 'combat_power':
        w.combat_effect(target, 'blocker' if 'blocker' in w.cards else None)


def build_variant(name, p, scene, owner, registry, oracle):
    from recipes import step

    w = _World(name, p, owner, registry, oracle)
    fixture(w, scene)
    w.emit('turn', player=owner, value=0)
    for seat in (0, 1):
        w.pool(seat, [0] * 6)
    w.receipts()
    w.source_cast(hidden=scene != 'printed', competing=scene == 'face_down_choice', short=scene == 'short_cast')
    if scene == 'short_cast':
        w.receipts()
        return w.setup, w.steps
    if scene == 'countered':
        if w.rows[name]['csvid'] == 6272:
            # The response bridge requires a pending spell. Fail ordinarily if the paid cast never put it there.
            w.emit('require_card', ref='source', property='in_hand', value=0)
        w.emit('announce_spell', ref='source')
        w.pending('counter', response=True)
        w.emit('resolve_response')
        w.move('counter','graveyard')
        w.move('source','graveyard')
        w.receipts()
        return w.setup, w.steps
    w.finish_spell('source', 'battlefield')
    if scene == 'face_down_choice':
        return w.setup, w.steps
    if scene in ('short_reveal', 'wrong_reveal_color'):
        w.reveal(denied=scene)
        return w.setup, w.steps
    if scene == 'replay':
        w.emit('capture')
    cut = len(w.steps)
    mode = p['mode']
    if scene == 'hidden_ability':
        if mode.startswith('regen'):
            run_regeneration(w, scene)
        elif mode == 'tap' or p['subtype'] == 'Goblin':
            # The legal paid face-down action uses the shared colored mana to turn face up.
            w.reveal()
        else:
            # One green can pay the printed pump but cannot pay the face-down object's turn-up cost.
            w.use(None, available=False)
    else:
        if scene != 'printed':
            w.reveal()
        if mode.startswith('regen'):
            if scene == 'opposing_turn':
                w.next_main()
            run_regeneration(w, scene)
        elif mode == 'tap':
            run_tap(w, scene)
        else:
            run_pump(w, scene)
    if scene == 'replay':
        tail = copy.deepcopy(w.steps[cut:])
        w.steps.extend([step('record_state'), step('ai_roundtrip'), step('restore'), *tail, step('assert_recorded_state')])
    return w.setup, w.steps


def expand(name, csvid, declaration, p, selected, registry, oracle):
    record, row = oracle[name], registry[name]
    pin = profiles()['sources'].get(name)
    if not pin or pin['registry'] != row or row['csvid'] != csvid or row['handler'] != handler(record):
        raise ValueError(f'unpinned Morph activation source: {name}')
    result = []
    for variant in selected:
        scene, owner = variant.rsplit('_', 1)
        setup, steps = build_variant(name, p, scene, int(owner), registry, oracle)
        result.append(dict(name=f'MorphFaceupActivatedRecipe_{csvid}_{variant}', card=name, fixture='empty_main', seed=123,
                           setup=setup, steps=steps, _recipe=dict(family=FAMILY, version=2, variant=variant,
                           params=declaration['params'], oracle_text=declaration['oracle_text'],
                           untested_components=['multiplayer','source control changes','selector UI while destruction callback is pending'])))
    return result
