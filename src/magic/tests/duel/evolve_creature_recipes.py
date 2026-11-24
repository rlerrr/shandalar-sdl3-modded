"""Evolve using real creature entries, strict P/T contrasts, and printed combat rules."""

import copy
import json
from collections import Counter
from pathlib import Path

FAMILY = 'evolve_creature'
PROFILE = Path(__file__).with_name('data') / 'evolve_creature_wave8_profiles.json'
RULE = ('Evolve (Whenever a creature you control enters, if that creature has greater power or toughness '
        'than this creature, put a +1/+1 counter on this creature.)')
EXTRAS = {'': None, 'Trample\n': 'Trample', 'Defender\n': 'Defender', 'Flying\n': 'Flying',
          'Reach (This creature can block creatures with flying.)\n': 'Reach',
          "This creature can't be blocked.\n": 'Unblockable',
          'Flash (You may cast this spell any time you could cast an instant.)\n': 'Flash'}
TEAM_LINE = ('\nWhenever this creature evolves, put a +1/+1 counter on each other creature '
             'you control with a +1/+1 counter on it.')
EXTRA_HANDLERS = {'Unblockable': 'manalinkeh_card_elusive_krasis', 'Flash': 'manalinkeh_card_shambleshark',
                  'Team': 'manalinkeh_card_renegade_krasis'}
COMMON = ('power_only', 'toughness_only', 'both_greater', 'equal', 'opponent_entry', 'noncreature_entry',
          'lower_after_growth', 'sequence', 'separate_sources', 'insufficient_mana', 'wrong_color',
          'piper_entry', 'response_prevents', 'countered_entrant', 'bounce_reset', 'pending_replay')
KEYWORD_MODES = {None: (), 'Trample': ('trample_combat',), 'Defender': ('defender_combat',),
                 'Flying': ('flying_combat',), 'Reach': ('reach_combat',),
                 'Unblockable': ('unblockable_combat',), 'Flash': ('flash_response', 'flash_block'),
                 'Team': ('team_counter', 'external_counter_no_team')}
FILLER = "Mons's Goblin Raiders"
FLIER = 'Storm Crow'
UNCOVERED = ('priority response to an already pending evolve trigger, including its entrant leaving',
             'simultaneous multi-creature entry, token and copy-specific entry')


def profiles():
    return json.loads(PROFILE.read_text(encoding='utf8'))


def stats(record):
    values = [record.get(key) for key in ('power', 'toughness')]
    if any(not isinstance(value, str) or not value.isascii() or not value.isdecimal() for value in values):
        raise ValueError('Evolve fixture requires printed creature statistics')
    return tuple(map(int, values))


def classify(record):
    from recipes import cost_from_oracle

    if (not record or record.get('layout') != 'normal' or record.get('selected_face') or record.get('card_faces')
            or not record.get('type_line', '').startswith('Creature — ')):
        raise ValueError('whole normal Evolve creature required')
    prefix, suffix, tail = record.get('oracle_text', '').partition(RULE)
    if not suffix or prefix not in EXTRAS or tail not in ('', TEAM_LINE) or tail and prefix:
        raise ValueError('unsupported Evolve companion ability')
    keyword = 'Team' if tail else EXTRAS[prefix]
    printed = [] if not keyword or keyword in ('Team', 'Unblockable') else [keyword]
    if sorted(record.get('keywords', [])) != sorted(['Evolve'] + printed):
        raise ValueError('Evolve keyword record differs from whole Oracle')
    power, toughness = stats(record)
    return dict(mana=cost_from_oracle(record), power=power, toughness=toughness, keyword=keyword)


def variants(params):
    modes = COMMON + (('weaker',) if params['power'] > 0 else ()) + KEYWORD_MODES[params['keyword']]
    return [f'{mode}_{owner}' for owner in (0, 1) for mode in modes]


def check_variants(params, selected):
    if (type(selected) is not list or not selected or any(type(item) is not str for item in selected)
            or len(set(selected)) != len(selected) or not set(selected) <= set(variants(params))):
        raise ValueError('distinct reviewed Evolve variants required')


def spec(declaration, record):
    expected = classify(record)
    pin = profiles()['sources'].get(record['name'])
    if (declaration.get('family') != FAMILY or type(declaration.get('version')) is not int or declaration['version'] != 2
            or not pin or pin['oracle'] != record or declaration.get('oracle_text') != record['oracle_text']
            or json.dumps(declaration.get('params'), sort_keys=True) != json.dumps(expected, sort_keys=True)):
        raise ValueError('whole Evolve Oracle and typed parameters required')
    check_variants(expected, declaration.get('variants'))
    return dict(expected, family=FAMILY, effect=FAMILY)


def validate_witnesses(oracle, registry):
    data = profiles()
    excluded = {row['csvid'] for row in json.loads((PROFILE.parent / 'unimplemented.json').read_text(
        encoding='utf8'))['cases'].values()}
    if data.get('version') != 1 or set(data['sources']) & set(data['witnesses']):
        raise ValueError('invalid Evolve profile schema')
    for group in ('sources', 'witnesses'):
        for name, pin in data[group].items():
            row = registry.get(name, {})
            if (pin['oracle'] != oracle.get(name) or pin['registry'] != row or row.get('kind') != 'card'
                    or row['csvid'] in excluded or row['handler'] == 'NULL'
                    or (row['handler'] == 'card_dummy' and not pin.get('reviewed_native_characteristics'))):
                raise ValueError(f'changed Evolve witness or binding: {name}')
            if group != 'sources':
                continue
            expected = classify(oracle[name])
            if row['handler'] != EXTRA_HANDLERS.get(expected['keyword'], 'manalinkeh_card_cloudfin_raptor'):
                raise ValueError('unreviewed Evolve source handler')
            own = (expected['power'], expected['toughness'])
            witnesses = pin['entry_witnesses']
            relations = {'power': (True, False), 'toughness': (False, True), 'both': (True, True)}
            if set(witnesses) != {*relations, 'weaker'}:
                raise ValueError('incomplete Evolve witness matrix')
            for kind in relations:
                candidate = witnesses[kind]
                if candidate not in data['witnesses']:
                    raise ValueError('a pinned independent entrant is required')
                actual = stats(oracle[candidate])
                if tuple(a > b for a, b in zip(actual, own)) != relations[kind]:
                    raise ValueError(f'wrong {kind} witness for {name}')
            weak = witnesses['weaker']
            if own[0] > 0:
                if weak not in data['witnesses']:
                    raise ValueError(f'missing weaker witness for {name}')
                power, toughness = stats(oracle[weak])
                if power >= own[0] or not 0 < toughness <= max(1, own[1] - 1):
                    raise ValueError(f'wrong weaker witness for {name}')
            elif weak is not None:
                raise ValueError(f'unsupported weaker witness for {name}')


class EvolveWorld:
    def __init__(self, name, owner, rows, oracle):
        self.name = name
        self.owner = self.current = owner
        self.foe = 1 - owner
        self.rows = rows
        self.oracle = oracle
        self.setup = []
        self.steps = []
        self.objects = {}
        self.life = [20, 20]
        self.watchers = []
        pin = profiles()['sources'][name]
        self.witnesses = pin['entry_witnesses']
        self.team = classify(pin['oracle'])['keyword'] == 'Team'

    def emit(self, op, **fields):
        from recipes import step

        self.steps.append(step(op, **fields))

    def add(self, ref, name, player=None, zone='hand'):
        from recipes import card

        player = self.owner if player is None else player
        self.setup.append(card(name, ref, player, zone))
        creature = 'Creature' in self.oracle[name]['type_line'].split(' — ')[0].split()
        self.objects[ref] = dict(name=name, player=player, zone=zone, creature=creature, counters=0, boost=0, damage=0)
        if name == self.name:
            self.watchers.append(ref)
        return ref

    def observe(self, ref, prop, value):
        self.emit('assert_card', ref=ref, property=prop, value=int(value))

    def price(self, ref):
        from recipes import cost_from_oracle

        record = self.oracle[self.objects[ref]['name']]
        return [0] * 6 if record['mana_cost'] == '{0}' else cost_from_oracle(record)

    def pool(self, player, cost):
        for color, value in enumerate(cost):
            self.emit('set_mana', player=player, color=color, value=value)

    def mana(self, player, cost):
        for color, value in enumerate(cost):
            self.emit('assert_mana', player=player, color=color, value=value)

    def cast(self, ref, *, target=None, response=False, denied=None):
        obj = self.objects[ref]
        player, cost = obj['player'], self.price(ref)
        if denied == 'insufficient_mana':
            cost = [0] * 6
        if denied == 'wrong_color':
            cost = [sum(cost), 0, 0, 0, 0, 0]
        self.pool(player, cost)
        self.emit('refresh')
        if target is not None:
            self.emit('choose_target', player=player, ref=target)
        self.emit('cast_response' if response else 'cast', ref=ref, value=int(denied is None))
        self.emit('assert_choices_consumed')
        self.emit('assert_stack', value=(2 if response else 1) if denied is None else 0)
        self.mana(player, [0] * 6 if denied is None else cost)
        self.observe(ref, 'in_hand', denied is not None)
        self.observe(ref, 'in_play', False)

    def power_toughness(self, obj):
        return tuple(value + obj['counters'] + obj['boost'] for value in stats(self.oracle[obj['name']]))

    def arrive(self, ref):
        obj = self.objects[ref]
        obj.update(zone='battlefield', counters=0, boost=0, damage=0)
        if not obj['creature']:
            return
        incoming = self.power_toughness(obj)
        for watcher in self.watchers:
            target = self.objects[watcher]
            if target['zone'] == 'battlefield' and target['player'] == obj['player']:
                if any(a > b for a, b in zip(incoming, self.power_toughness(target))):
                    self.evolve(watcher)

    def evolve(self, ref):
        obj = self.objects[ref]
        obj['counters'] += 1
        if self.team:
            for other_ref, other in self.objects.items():
                if (other_ref != ref and other['zone'] == 'battlefield' and other['player'] == obj['player']
                        and other['creature'] and other['counters'] > 0):
                    other['counters'] += 1

    def receipts(self):
        self.emit('refresh')
        self.emit('assert_stack', value=0)
        self.emit('assert_choices_consumed')
        for ref, obj in self.objects.items():
            if obj['zone'] != 'battlefield':
                continue
            self.observe(ref, 'in_play', True)
            if obj['creature']:
                power, toughness = self.power_toughness(obj)
                for prop, value in (('power', power), ('toughness', toughness), ('plus1_counters', obj['counters']),
                                    ('damage', obj['damage'])):
                    self.observe(ref, prop, value)
        names = sorted({obj['name'] for obj in self.objects.values()})
        ledger = Counter((obj['player'], obj['zone'], obj['name']) for obj in self.objects.values())
        for player in (0, 1):
            self.emit('assert_life', player=player, value=self.life[player])
            for zone in ('battlefield', 'hand', 'graveyard', 'exile'):
                for name in names:
                    self.emit('assert_zone_multiplicity', player=player, zone=zone,
                              index=self.rows[name]['csvid'], value=ledger[player, zone, name])

    def resolve_entry(self, ref):
        self.emit('resolve')
        self.arrive(ref)
        self.receipts()

    def enter(self, ref):
        self.cast(ref)
        self.resolve_entry(ref)

    def next_turn(self):
        self.current = 1 - self.current
        self.emit('advance_turn')
        self.emit('untap_step', player=self.current)
        self.emit('turn', player=self.current, value=0)
        for obj in self.objects.values():
            obj.update(boost=0, damage=0)

    def piper_entry(self, ref):
        from recipes import COLORS

        cost = [0] * 6
        cost[COLORS['G']] = 1
        self.pool(self.owner, cost)
        self.emit('refresh')
        self.emit('require_can_activate', ref='piper', value=1)
        self.emit('activate', ref='piper', value=1)
        self.emit('assert_stack', value=1)
        self.emit('assert_choices_consumed')
        self.observe(ref, 'in_hand', True)
        csvid = self.rows[self.objects[ref]['name']]['csvid']
        self.emit('choose_zone_card', player=self.owner, value=csvid)
        self.emit('resolve')
        self.mana(self.owner, [0] * 6)
        self.observe('piper', 'tapped', True)
        self.arrive(ref)
        self.emit('bind_permanent_copy', ref=ref, player=self.owner, index=0, value=csvid)
        self.receipts()

    def response(self, ref, boost):
        self.cast(ref)
        self.emit('announce_spell', ref=ref)
        self.cast('response', target='source' if boost else None, response=True)
        self.emit('resolve_response')
        self.objects['response']['zone'] = 'graveyard'
        if boost:
            self.objects['source']['boost'] += 3
            self.arrive(ref)
        else:
            self.objects[ref]['zone'] = 'graveyard'
        self.receipts()
        if boost:
            self.next_turn()
            self.receipts()

    def bounce(self):
        self.cast('response', target='source')
        self.emit('resolve')
        self.objects['response']['zone'] = 'graveyard'
        self.objects['source'].update(zone='hand', counters=0, damage=0)
        self.emit('bind_hand_copy', ref='source', player=self.owner, index=0, value=self.rows[self.name]['csvid'])
        self.receipts()

    def combat(self, mode):
        incoming = mode == 'reach_combat'
        self.next_turn()
        if not incoming:
            self.next_turn()
        self.emit('begin_attackers')
        if mode == 'defender_combat':
            self.observe('source', 'sick', False)
            self.emit('attack', ref='source', value=0)
            self.observe('source', 'attacking', False)
            self.emit('finish_attackers')
            self.emit('begin_blockers')
            self.emit('advance_combat')
            self.emit('combat_damage')
            self.emit('resolve')
            self.emit('end_combat')
            self.receipts()
            return
        attacker, blocker = ('opposing', 'source') if incoming else ('source', 'opposing')
        a, b = self.objects[attacker], self.objects[blocker]
        ap, at = self.power_toughness(a)
        bp, bt = self.power_toughness(b)
        self.emit('attack', ref=attacker, value=1)
        self.emit('finish_attackers')
        self.emit('begin_blockers')
        allowed = mode not in ('flying_combat', 'unblockable_combat')
        self.emit('block', ref=blocker, other=attacker, value=int(allowed))
        self.observe(blocker, 'blocking_count', allowed)
        self.emit('advance_combat')
        self.emit('combat_damage')
        self.emit('resolve')
        self.emit('end_combat')
        if allowed:
            a['damage'], b['damage'] = bp, min(ap, bt) if mode == 'trample_combat' else ap
            if a['damage'] >= at:
                a['zone'] = 'graveyard'
            if b['damage'] >= bt:
                b['zone'] = 'graveyard'
            if mode == 'trample_combat':
                self.life[b['player']] -= max(0, ap - bt)
        else:
            self.life[b['player']] -= ap
        self.receipts()

    def flash_response(self):
        self.next_turn()
        self.cast('parent')
        self.emit('announce_spell', ref='parent')
        self.cast('source', response=True)
        self.emit('resolve_response')
        self.arrive('source')
        self.objects['parent']['zone'] = 'graveyard'
        self.life[self.foe] += 5
        self.receipts()
        self.next_turn()
        self.enter('first')

    def flash_block(self):
        self.next_turn()
        self.emit('begin_attackers')
        self.emit('attack', ref='opposing', value=1)
        self.emit('finish_attackers')
        self.emit('begin_blockers')
        self.enter('source')
        self.emit('block', ref='source', other='opposing', value=1)
        self.observe('source', 'blocking_count', 1)
        self.emit('advance_combat')
        self.emit('combat_damage')
        self.emit('resolve')
        self.emit('end_combat')
        self.objects['source']['zone'] = self.objects['opposing']['zone'] = 'graveyard'
        self.receipts()

    def add_via_burst(self, ref, spell):
        self.cast(spell, target=ref)
        self.emit('resolve')
        self.objects[spell]['zone'] = 'graveyard'
        self.objects[ref]['counters'] += 1
        self.receipts()


def scenario(world, mode):
    owner, foe = world.owner, world.foe
    witnesses = world.witnesses
    world.add('source', world.name)
    if mode in ('power_only', 'toughness_only', 'both_greater'):
        world.add('first', witnesses[mode.split('_')[0]])
    elif mode in ('opponent_entry', 'pending_replay', 'piper_entry', 'countered_entrant'):
        world.add('first', witnesses['both'], foe if mode == 'opponent_entry' else owner)
    elif mode in ('response_prevents', 'bounce_reset', 'lower_after_growth', 'separate_sources', 'sequence'):
        world.add('first', witnesses['power'])
    elif mode in ('equal', 'weaker', 'noncreature_entry'):
        world.add('first', {'equal': world.name, 'weaker': witnesses['weaker'], 'noncreature_entry': 'Sol Ring'}[mode])
    if mode == 'piper_entry':
        world.add('piper', 'Elvish Piper', zone='battlefield')
    if mode in ('response_prevents', 'countered_entrant', 'bounce_reset'):
        world.add('response', {'response_prevents': 'Giant Growth', 'countered_entrant': 'Counterspell',
                               'bounce_reset': 'Unsummon'}[mode], foe)
    if mode in ('separate_sources', 'lower_after_growth', 'bounce_reset', 'sequence'):
        world.add('second', {
            'separate_sources': world.name, 'lower_after_growth': witnesses['power'],
            'bounce_reset': witnesses['power'], 'sequence': witnesses['both'],
        }[mode])
    if mode in ('separate_sources', 'sequence'):
        third = witnesses['power'] if mode == 'separate_sources' else (
            'Scaled Wurm' if world.name == 'Adaptive Snapjaw' else 'Colossapede')
        world.add('third', third)
    if mode in ('trample_combat', 'flying_combat', 'reach_combat', 'defender_combat', 'unblockable_combat',
                'flash_response', 'team_counter'):
        world.add('first', witnesses['power'])
    if mode in ('trample_combat', 'flying_combat', 'reach_combat', 'unblockable_combat', 'flash_block'):
        world.add('opposing', FLIER if mode == 'reach_combat' else FILLER, foe, 'battlefield')
    if mode == 'flash_response':
        world.add('parent', 'Feed the Clan', foe)
    if mode in ('team_counter', 'external_counter_no_team'):
        world.add('ally', FILLER, zone='battlefield')
        world.add('ally_burst', 'Burst of Strength')
        world.add('other_burst', 'Burst of Strength')
    if mode == 'team_counter':
        world.add('counterless', 'Grizzly Bears', zone='battlefield')
        world.add('enemy', FILLER, foe, 'battlefield')

    world.emit('turn', player=owner, value=0)
    world.emit('present_ai_dialogs', value=1)
    for player in (0, 1):
        world.pool(player, [0] * 6)
    world.receipts()
    if mode in ('flash_response', 'flash_block'):
        getattr(world, mode)()
        return None
    if mode in ('insufficient_mana', 'wrong_color'):
        world.cast('source', denied=mode)
        world.receipts()
        return None
    world.enter('source')
    if mode in ('team_counter', 'external_counter_no_team'):
        world.add_via_burst('ally', 'ally_burst')
        world.add_via_burst('enemy' if mode == 'team_counter' else 'source', 'other_burst')
        if mode == 'external_counter_no_team':
            return None
    if mode == 'pending_replay':
        world.cast('first')
        cut = len(world.steps)
        world.resolve_entry('first')
        return cut
    if mode == 'piper_entry':
        world.piper_entry('first')
        return None
    if mode in ('response_prevents', 'countered_entrant'):
        world.response('first', mode == 'response_prevents')
        return None
    if mode == 'opponent_entry':
        world.next_turn()
    world.enter('first')
    if mode == 'bounce_reset':
        world.bounce()
        world.enter('source')
    if 'second' in world.objects:
        world.enter('second')
    if 'third' in world.objects:
        world.enter('third')
    if mode in ('trample_combat', 'flying_combat', 'reach_combat', 'defender_combat', 'unblockable_combat'):
        world.combat(mode)
    return None


def expand(name, csvid, declaration, params, selected, registry, oracle):
    from recipes import step

    check_variants(params, selected)
    validate_witnesses(oracle, registry)
    if profiles()['sources'][name]['registry']['csvid'] != csvid:
        raise ValueError('Evolve source CSVID changed')
    cases = []
    for variant in selected:
        mode, owner = variant.rsplit('_', 1)
        world = EvolveWorld(name, int(owner), registry, oracle)
        cut = scenario(world, mode)
        steps = world.steps
        if cut is not None:
            suffix = steps[cut:]
            steps = (steps[:cut] + [step('capture')] + suffix
                     + [step('record_state'), step('ai_roundtrip'), step('restore')] + copy.deepcopy(suffix)
                     + [step('assert_recorded_state')])
        cases.append(dict(name=f'EvolveCreatureRecipe_{csvid}_{variant}', card=name, fixture='empty_main', seed=113,
                          setup=world.setup, steps=steps, _recipe=dict(family=FAMILY, version=2, variant=variant,
                          params=declaration['params'], oracle_text=declaration['oracle_text'],
                          untested_components=list(UNCOVERED))))
    return cases
