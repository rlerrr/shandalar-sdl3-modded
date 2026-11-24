"""Native creature-type choices with full static, cost, and cast-trigger receipts."""

import copy
import json
from pathlib import Path

from block_restriction_spells_recipes import _World as _SpellWorld


FAMILY = 'chosen_type_effects'
PROFILE = Path(__file__).parent / 'data/chosen_type_effects_wave8_profiles.json'
MODELS = {
    ('creature', 'This creature is the chosen type in addition to its other types.\n'
                 'Other creatures you control of the chosen type get +1/+1.'): ('automaton', 'adaptive_automaton'),
    ('artifact', 'Whenever you cast a spell of the chosen type, put a charge counter on this artifact.\n'
                 'Creatures you control of the chosen type get +1/+1 for each charge counter on this artifact.'):
        ('door', 'door_of_destinies'),
    ('enchantment', 'All creatures of the chosen type get -1/-1.'): ('plague', 'engineered_plague'),
    ('enchantment', 'Creatures of the chosen type get +1/+1.'): ('triumph', 'shared_triumph'),
    ('enchantment', "Creatures of the chosen type have shroud. (They can't be the targets of spells or abilities.)"):
        ('shroud', 'steely_resolve'),
    ('artifact', 'Creature spells of the chosen type cost {2} less to cast.'): ('incubator', 'urzas_incubator'),
    ('enchantment', 'Each creature you control is the chosen type in addition to its other types.'):
        ('xenograft', 'xenograft'),
    ('creature', 'This creature is the chosen type in addition to its other types.\n'
                 'Each other creature you control of the chosen type enters with an additional +1/+1 counter on it.'):
        ('mimic', 'metallic_mimic'),
    ('artifact', 'Creatures you control of the chosen type get +1/+1.\n'
                 'Whenever you cast a creature spell of the chosen type, draw a card.'):
        ('banner', 'vanquishers_banner'),
    ('enchantment', "Creatures of the chosen type don't untap during their controllers' untap steps."):
        ('ruins', 'an_zerrin_ruins'),
}
COMMON = ('goblins', 'angels', 'short', 'source_removed', 'countered', 'replay', 'late_match')
EXTRA = {
    'automaton': (), 'triumph': (), 'plague': ('lethal', 'late_lethal'),
    'shroud': ('ability_scope', 'nontargeted_damage'),
    'door': ('matching_casts', 'kindred', 'countered_match', 'opposing_cast', 'nonmatching_cast', 'failed_cast', 'turn_persist'),
    'incubator': ('opposing_discount', 'short_on_discount', 'colored_floor', 'floor_is_zero',
                  'kindred_not_discounted', 'nonmatching', 'remaining_generic'),
    'xenograft': ('late_nonmatch', 'opposing_entry'),
    'mimic': ('opposing_entry', 'nonmatching_entry', 'countered_match', 'original_type'),
    'banner': ('matching_casts', 'kindred', 'countered_match', 'opposing_cast', 'nonmatching_cast', 'failed_cast'),
    'ruins': ('untapped_entry', 'manual_untap'),
}
FOLLOW_ON_MODES = frozenset(('xenograft', 'mimic', 'banner', 'ruins'))
GOBLIN = 'Goblin Roughrider'
ONE_GOBLIN = 'Mogg Fanatic'
ANGEL = 'Serra Angel'
KINDRED = 'Boggart Shenanigans'
WITNESSES = {GOBLIN, ONE_GOBLIN, ANGEL, KINDRED, 'Goblin Goon', 'Wind Drake', 'Prodigal Sorcerer', 'Shock',
             'Counterspell', 'Boomerang', 'Pyroclasm', 'Shared Triumph', 'Morphling', 'Twiddle',
             'Forest', 'Island', 'Mountain'}
DUMMIES = {GOBLIN, 'Wind Drake'}


def profiles():
    return json.loads(PROFILE.read_bytes())


def classify(record):
    from recipes import cost_from_oracle

    lines = record['oracle_text'].splitlines()
    if record.get('layout') != 'normal' or record.get('card_faces') or record.get('selected_face') or not lines or record['keywords']:
        raise ValueError('normal complete chosen-type permanent required')
    first, remainder = lines[0], '\n'.join(lines[1:])
    kind = next((part for part in ('creature', 'artifact', 'enchantment')
                 if first == f'As this {part} enters, choose a creature type.'), None)
    model = MODELS.get((kind, remainder))
    actual_kind = 'creature' if 'Creature' in record['type_line'].split(' — ')[0].split() else record['type_line'].lower()
    if not model or kind != actual_kind:
        raise ValueError('unmodeled complete chosen-type clauses or permanent type')
    mode, _ = model
    return dict(mode=mode, mana=cost_from_oracle(record), keywords=[])


def handler(record):
    first, _, remainder = record['oracle_text'].partition('\n')
    kind = first.split()[2]
    return 'manalinkeh_card_' + MODELS[(kind, remainder)][1]


def variants(params):
    return [f'{scenario}_{owner}' for scenario in COMMON + EXTRA[params['mode']] for owner in (0, 1)]


def spec(declaration, record):
    params = classify(record)
    pin = profiles()['sources'].get(record['name'])
    chosen = declaration.get('variants')
    if (declaration.get('family') != FAMILY or type(declaration.get('version')) is not int
            or declaration['version'] != 2 or not pin or pin['oracle'] != record
            or declaration.get('oracle_text') != record['oracle_text']
            or json.dumps(declaration.get('params'), sort_keys=True) != json.dumps(params, sort_keys=True)):
        raise ValueError('exact pinned chosen-type spell and typed parameters required')
    if (type(chosen) is not list or not chosen or any(type(value) is not str for value in chosen)
            or len(set(chosen)) != len(chosen) or not set(chosen) <= set(variants(params))):
        raise ValueError('approved distinct chosen-type scenarios required')
    return dict(params, family=FAMILY, effect=FAMILY)


def validate_witnesses(oracle, registry):
    pins = profiles()
    if pins.get('version') != 1 or set(pins['witnesses']) != WITNESSES:
        raise ValueError('complete independent chosen-type witnesses required')
    for scope in ('sources', 'witnesses'):
        for name, pin in pins[scope].items():
            row = registry.get(name, {})
            allowed_dummy = scope == 'witnesses' and name in DUMMIES and pin.get('reviewed_native_characteristics')
            if (pin['oracle'] != oracle.get(name) or pin['registry'] != row or row.get('kind') != 'card'
                    or row.get('handler') in ('NULL', None) or row['handler'] == 'card_dummy' and not allowed_dummy):
                raise ValueError(f'changed or unsupported chosen-type binding: {name}')
            if scope == 'sources':
                classify(oracle[name])
                if row['handler'] != handler(oracle[name]):
                    raise ValueError(f'changed chosen-type source callback: {name}')


class _World(_SpellWorld):
    def __init__(self, name, params, owner, registry, oracle, tribe):
        super().__init__(name, params, owner, registry, oracle)
        self.tribe = tribe
        self.charges = 0

    def active(self):
        return self.cards['source'][2] == 'battlefield'

    def chosen_type(self, ref):
        return self.tribe in self.o[self.cards[ref][0]]['type_line'].partition(' — ')[2].split()

    def derive(self):
        for ref, creature in self.entities.items():
            if creature['zone'] != 'battlefield':
                continue
            printed = self.o[creature['name']]
            bonus = 0
            if self.active() and self.chosen_type(ref):
                mode = self.p['mode']
                if mode in ('plague', 'triumph'):
                    bonus = -1 if mode == 'plague' else 1
                elif creature['player'] == self.owner and ref != 'source':
                    bonus = self.charges if mode == 'door' else int(mode == 'automaton')
            creature['power'] = int(printed['power']) + bonus
            creature['toughness'] = int(printed['toughness']) + bonus
            if creature['toughness'] <= 0:
                self.move(ref, 'graveyard')

    def receipts(self):
        self.derive()
        super().receipts()
        if self.active() and self.p['mode'] == 'door':
            self.emit('assert_counter', ref='source', counter_kind='charge', value=self.charges)
        if self.active() and self.p['mode'] == 'automaton':
            for subtype, value in (('Construct', 1), ('Goblin', int(self.tribe == 'Goblin')),
                                   ('Angel', int(self.tribe == 'Angel'))):
                self.emit('assert_matching_permanents', player=self.owner, index=self.rows[self.name]['csvid'],
                          property=f'subtype_{subtype}', value=value)

    def enter(self, scenario):
        fee = list(self.p['mana'])
        if scenario == 'short':
            fee[0] -= 1
        self.cast('source', budget=fee, denied=scenario == 'short')
        if scenario == 'short':
            self.receipts()
            return
        if scenario == 'countered':
            self.emit('announce_spell', ref='source')
            self.cast('reply', parent='source')
            self.finish('reply', 'graveyard', response=True)
            self.move('source', 'graveyard')
        else:
            self.emit('choose_dialog', player=self.owner, value=2)
            self.emit('choose_target', ref='goblin_model' if self.tribe == 'Goblin' else 'angel_model', player=self.owner)
            self.emit('choose_dialog', player=self.owner, value=0)
            self.finish('source', 'battlefield')
            self.emit('assert_unused_choices', value=0)
        self.receipts()

    def affordable_member(self, ref, *, budget, fee, denied=False):
        actor = self.cards[ref][1]
        self.pool(actor, budget)
        self.emit('refresh')
        self.emit('cast', ref=ref, value=int(not denied))
        self.emit('assert_unused_choices', value=0)
        self.emit('assert_stack', value=int(not denied))
        self.observe(ref, 'in_hand', denied)
        if not denied:
            self.mana[actor] = [have - due for have, due in zip(budget, fee)]
            for color, value in enumerate(self.mana[actor]):
                self.emit('assert_mana', player=actor, color=color, value=value)

    def cast_member(self, ref, *, shortage=None, counter=False):
        from recipes import cost_from_oracle

        name, actor, _ = self.cards[ref]
        record = self.o[name]
        printed = cost_from_oracle(record)
        mode = self.p['mode']
        creature_spell = 'Creature' in record['type_line'].split(' — ')[0].split()
        fee = list(printed)
        if self.active() and mode == 'incubator' and creature_spell and self.chosen_type(ref):
            fee[0] = max(0, fee[0] - 2)
        if shortage is None:
            budget = fee
        elif shortage == 'no_mana':
            budget = [0] * 6
        elif shortage == 'colored':
            budget = [1, 0, 0, 0, 0, 0]
        else:
            budget = list(fee)
            budget[0] -= 1 if shortage == 'generic' else 2
        self.affordable_member(ref, budget=budget, fee=fee, denied=shortage is not None)
        if shortage is not None:
            self.receipts()
            return
        if self.active() and mode == 'door':
            self.emit('announce_spell', ref=ref)
            if actor == self.owner and self.chosen_type(ref):
                self.charges += 1
            self.derive()
            self.emit('refresh')
            self.emit('assert_counter', ref='source', counter_kind='charge', value=self.charges)
            self.emit('assert_stack', value=1)
            self.observe('matching_own', 'power', self.entities['matching_own']['power'])
        if counter:
            self.cast('reply', parent=ref)
            self.finish('reply', 'graveyard', response=True)
            self.move(ref, 'graveyard')
        else:
            self.finish(ref, 'battlefield')
        self.receipts()

    def shroud_probes(self, *, activated=False):
        for seat in (0, 1):
            for target in ('matching_own', 'matching_foe', 'angel_model', 'angel_foe'):
                permitted = not (self.active() and self.chosen_type(target))
                if activated:
                    self.emit('probe_activation_target', ref=f'tim_{seat}', other=target, index=-1, value=int(permitted))
                else:
                    self.emit('can_target_creature', ref=f'probe_{seat}', other=target, value=int(permitted))


def fixture(w, scenario):
    owner, foe, mode = w.owner, w.foe, w.p['mode']
    w.add(w.name, 'source', owner, 'hand')
    w.add(ONE_GOBLIN, 'goblin_model', owner)
    w.add(ANGEL, 'angel_model', owner)
    w.add(GOBLIN, 'matching_own', owner)
    w.add(GOBLIN, 'matching_foe', foe)
    w.add(ANGEL, 'angel_foe', foe)
    if scenario in ('lethal', 'nontargeted_damage'):
        w.add(ONE_GOBLIN, 'small_foe', foe)
    if mode == 'shroud':
        for seat in (0, 1):
            w.add('Shock', f'probe_{seat}', seat, 'hand')
            if scenario == 'ability_scope':
                w.add('Prodigal Sorcerer', f'tim_{seat}', seat)
    if scenario == 'nontargeted_damage':
        w.add('Pyroclasm', 'burn', owner, 'hand')
    if scenario in ('source_removed', 'countered', 'countered_match'):
        w.add('Boomerang' if scenario == 'source_removed' else 'Counterspell', 'reply', foe, 'hand')
    needs_member = scenario in ('late_match', 'late_lethal') or mode in ('door', 'incubator') and scenario in (
        *EXTRA[mode], 'goblins', 'angels', 'source_removed', 'replay')
    if needs_member:
        member = {'angels': ANGEL, 'late_lethal': ONE_GOBLIN, 'kindred': KINDRED, 'kindred_not_discounted': KINDRED,
                  'nonmatching': 'Wind Drake', 'nonmatching_cast': 'Wind Drake', 'colored_floor': ONE_GOBLIN,
                  'floor_is_zero': ONE_GOBLIN, 'short_on_discount': 'Goblin Goon',
                  'remaining_generic': 'Goblin Goon'}.get(scenario, GOBLIN)
        actor = foe if scenario in ('opposing_cast', 'opposing_discount') else owner
        w.add(member, 'member', actor, 'hand')
        if scenario == 'matching_casts':
            w.add(ONE_GOBLIN, 'second', owner, 'hand')


def build_variant(name, params, scenario, owner, registry, oracle):
    from recipes import step

    if params['mode'] in FOLLOW_ON_MODES:
        return build_follow_on(name, params, scenario, owner, registry, oracle)
    w = _World(name, params, owner, registry, oracle, 'Angel' if scenario == 'angels' else 'Goblin')
    mode = params['mode']
    fixture(w, scenario)
    w.emit('turn', player=owner, value=0)
    for seat in (0, 1):
        w.pool(seat, [0] * 6)
    w.receipts()
    if scenario == 'replay':
        w.emit('capture')
    cut = len(w.steps)
    w.enter(scenario)
    if mode == 'shroud':
        w.shroud_probes(activated=scenario == 'ability_scope')
    if scenario == 'source_removed':
        if mode == 'door':
            w.cast_member('member')
        w.cast('reply', target='source')
        w.finish('reply', 'graveyard')
        w.move('source', 'hand')
        w.receipts()
        if mode == 'shroud':
            w.shroud_probes()
        elif mode == 'incubator':
            w.cast_member('member', shortage='no_discount')
    elif scenario == 'nontargeted_damage':
        w.cast('burn')
        w.finish('burn', 'graveyard')
        for ref, permanent in w.entities.items():
            if permanent['zone'] == 'battlefield':
                permanent['damage'] += 2
                if permanent['damage'] >= permanent['toughness']:
                    w.move(ref, 'graveyard')
        w.receipts()
    elif 'member' in w.cards:
        if scenario in ('opposing_cast', 'opposing_discount'):
            w.next_turn()
        shortage = {'short_on_discount': 'generic', 'colored_floor': 'colored', 'failed_cast': 'no_mana',
                    'nonmatching': 'no_discount', 'kindred_not_discounted': 'no_discount'}.get(scenario)
        w.cast_member('member', shortage=shortage, counter=scenario == 'countered_match')
        if scenario == 'matching_casts':
            w.cast_member('second')
        elif scenario == 'turn_persist':
            w.next_turn()
        if mode == 'shroud':
            w.shroud_probes()
            if w.cards['member'][2] == 'battlefield':
                w.emit('can_target_creature', ref='probe_0', other='member', value=0)
    if scenario == 'replay':
        tail = copy.deepcopy(w.steps[cut:])
        w.steps.extend([step('record_state'), step('ai_roundtrip'), step('restore'), *tail, step('assert_recorded_state')])
    return w.setup, w.steps


class _FollowerWorld(_World):
    def __init__(self, name, params, owner, registry, oracle, tribe):
        super().__init__(name, params, owner, registry, oracle, tribe)
        self.counters = {}
        self.shapeshifter_triumph = False

    def emit(self, op, **kwargs):
        if op == 'assert_card' and kwargs.get('property') == 'plus1_counters':
            kwargs['value'] = self.counters.get(kwargs['ref'], 0)
        super().emit(op, **kwargs)

    def derive(self):
        super().derive()
        mode = self.p['mode']
        for ref, creature in self.entities.items():
            if creature['zone'] != 'battlefield':
                continue
            boost = self.counters.get(ref, 0)
            if mode == 'banner' and self.active() and creature['player'] == self.owner and self.chosen_type(ref):
                boost += 1
            if self.shapeshifter_triumph and 'Shapeshifter' in self.o[creature['name']]['type_line'].split():
                boost += 1
            for prop in ('power', 'toughness'):
                creature[prop] += boost

    def type_receipts(self):
        mode = self.p['mode']
        if mode == 'mimic' and self.active():
            self.emit('assert_matching_permanents', player=self.owner, index=self.rows[self.name]['csvid'],
                      property='subtype_' + self.tribe, value=1)
        elif mode == 'xenograft':
            for ref in ('goblin_model', 'angel_model', 'angel_foe', 'matching_foe', 'member'):
                if ref not in self.entities or self.cards[ref][2] != 'battlefield':
                    continue
                name, actor, _ = self.cards[ref]
                types = self.o[name]['type_line'].partition(' — ')[2].split()
                for subtype in dict.fromkeys((self.tribe, *types)):
                    value = subtype in types or self.active() and actor == self.owner
                    self.emit('assert_matching_permanents', player=actor, index=self.rows[name]['csvid'],
                              property='subtype_' + subtype, value=int(value))

    def receipts(self):
        super().receipts()
        self.type_receipts()

    def untap_next(self):
        if any(any(pool) for pool in self.mana):
            raise ValueError('real untap requires empty pools')
        self.emit('advance_turn')
        self.current = 1 - self.current
        self.expire()
        self.emit('untap_step', player=self.current)
        for ref, creature in self.entities.items():
            if creature['zone'] == 'battlefield' and creature['player'] == self.current:
                creature['sick'] = False
                if not (self.active() and self.p['mode'] == 'ruins' and self.chosen_type(ref)):
                    creature['tapped'] = False
        self.receipts()
        self.emit('turn', player=self.current, value=0)

    def play_member(self, ref, *, counter=False, denied=False):
        self.cast(ref, denied=denied, budget=[0] * 6 if denied else None)
        if denied:
            self.receipts()
            return
        _, actor, _ = self.cards[ref]
        creature = ref in self.entities
        eligible = self.active() and actor == self.owner and creature and self.chosen_type(ref)
        mode = self.p['mode']
        if mode == 'banner' or counter:
            self.emit('announce_spell', ref=ref)
            if mode == 'banner' and eligible:
                self.zones['hand'][actor].append(self.zones['library'][actor].pop(0))
            self.emit('assert_stack', value=1)
            if mode == 'banner':
                for seat in (0, 1):
                    if seat == actor:
                        # The aggregate active-slot count includes the pending spell.
                        self.observe(ref, 'in_hand', 0)
                    else:
                        self.emit('assert_zone', player=seat, zone='hand', value=len(self.zones['hand'][seat]))
                    self.emit('assert_zone', player=seat, zone='library', value=len(self.zones['library'][seat]))
                    for index, card in enumerate(self.zones['library'][seat]):
                        self.emit('assert_zone_card', player=seat, zone='library', index=index, value=self.rows[card]['csvid'])
                    self.emit('assert_zone_multiplicity', player=seat, zone='hand', index=self.rows['Island']['csvid'],
                              value=self.zones['hand'][seat].count('Island'))
        if counter:
            self.cast('reply', parent=ref)
            self.finish('reply', 'graveyard', response=True)
            self.move(ref, 'graveyard')
        else:
            self.finish(ref, 'battlefield')
            if mode == 'mimic' and eligible:
                self.counters[ref] = 1
        self.receipts()

    def original_shape(self):
        self.cast('triumph')
        self.emit('choose_dialog', player=self.owner, value=2)
        self.emit('choose_target', ref='shape_model', player=self.owner)
        self.emit('choose_dialog', player=self.owner, value=0)
        self.finish('triumph', 'battlefield')
        self.emit('assert_unused_choices', value=0)
        self.shapeshifter_triumph = True
        self.receipts()

    def manually_untap(self):
        self.cast('twiddle', target='matching_own')
        self.emit('present_ai_dialogs', value=1)
        self.emit('choose_dialog', player=self.owner, value=1)
        self.finish('twiddle', 'graveyard')
        self.emit('present_ai_dialogs', value=0)
        self.emit('assert_unused_choices', value=0)
        self.entities['matching_own']['tapped'] = False
        self.receipts()


def follow_on_fixture(w, scenario):
    mode, owner, foe = w.p['mode'], w.owner, w.foe
    fixture(w, scenario)
    if mode == 'banner':
        for actor in (0, 1):
            for index, witness in enumerate(('Island', 'Forest', 'Mountain')):
                w.add(witness, f'library_{actor}_{index}', actor, 'library')
    if mode == 'ruins' and scenario != 'untapped_entry':
        for setup in w.setup:
            if setup['ref'] in w.entities and setup['zone'] == 'battlefield':
                setup['state'] = ['tapped']
                w.entities[setup['ref']]['tapped'] = True
    if mode == 'ruins' and scenario == 'manual_untap':
        w.add('Twiddle', 'twiddle', owner, 'hand')
    if mode == 'mimic' and scenario == 'original_type':
        w.add('Morphling', 'shape_model', owner)
        w.add('Shared Triumph', 'triumph', owner, 'hand')
    if mode in ('xenograft', 'banner', 'mimic') and scenario not in ('short', 'countered', 'late_match', 'original_type'):
        actor = foe if scenario in ('opposing_entry', 'opposing_cast') else owner
        matching = GOBLIN if mode == 'xenograft' else 'Goblin Goon'
        member = {'kindred': KINDRED, 'angels': ANGEL, 'late_nonmatch': 'Wind Drake', 'nonmatching_entry': 'Wind Drake',
                  'nonmatching_cast': 'Wind Drake'}.get(scenario, matching)
        w.add(member, 'member', actor, 'hand')
        if scenario == 'matching_casts':
            w.add('Goblin Goon', 'second', owner, 'hand')
        elif mode == 'mimic' and scenario == 'source_removed':
            w.add(GOBLIN, 'second', owner, 'hand')


def build_follow_on(name, params, scenario, owner, registry, oracle):
    from recipes import step

    w = _FollowerWorld(name, params, owner, registry, oracle, 'Angel' if scenario == 'angels' else 'Goblin')
    mode = params['mode']
    follow_on_fixture(w, scenario)
    w.emit('turn', player=owner, value=0)
    for seat in (0, 1):
        w.pool(seat, [0] * 6)
    w.receipts()
    if scenario == 'replay':
        w.emit('capture')
    cut = len(w.steps)
    w.enter(scenario)
    if mode == 'mimic' and scenario == 'source_removed':
        w.play_member('member')
    if scenario == 'source_removed':
        w.cast('reply', target='source')
        w.finish('reply', 'graveyard')
        w.move('source', 'hand')
        w.receipts()
        if mode == 'mimic':
            w.play_member('second')
    if mode == 'ruins' and scenario != 'short':
        if scenario == 'manual_untap':
            w.manually_untap()
        w.untap_next()
        w.untap_next()
        if 'member' in w.cards:
            w.play_member('member')
    elif mode == 'mimic' and scenario == 'original_type':
        w.original_shape()
    elif 'member' in w.cards and (mode != 'mimic' or scenario != 'source_removed'):
        if scenario in ('opposing_entry', 'opposing_cast'):
            w.untap_next()
        w.play_member('member', counter=scenario == 'countered_match', denied=scenario == 'failed_cast')
        if scenario == 'matching_casts':
            w.play_member('second')
    if scenario == 'replay':
        tail = copy.deepcopy(w.steps[cut:])
        w.steps.extend([step('record_state'), step('ai_roundtrip'), step('restore'), *tail, step('assert_recorded_state')])
    return w.setup, w.steps


def expand(name, csvid, declaration, params, selected, registry, oracle):
    pin = profiles()['sources'].get(name)
    row = registry[name]
    if not pin or pin['registry'] != row or row['csvid'] != csvid or row['handler'] != handler(oracle[name]):
        raise ValueError(f'unpinned chosen-type source: {name}')
    cases = []
    for variant in selected:
        scenario, owner = variant.rsplit('_', 1)
        setup, steps = build_variant(name, params, scenario, int(owner), registry, oracle)
        cases.append(dict(name=f'ChosenTypeEffectsRecipe_{csvid}_{variant}', card=name, fixture='empty_main', seed=123,
                          setup=setup, steps=steps, _recipe=dict(family=FAMILY, version=2, variant=variant,
                                                               params=declaration['params'], oracle_text=declaration['oracle_text'],
                                                               untested_components=['card-list and token-only subtype menu paths', 'multiplayer'])))
    return cases
