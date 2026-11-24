"""Optional Ally-entry counters from real spell, activation, and combat actions."""

import copy
import json
from pathlib import Path

from attack_value_trigger_recipes import _AttackResourceWorld

FAMILY = 'ally_entry_counter'
PROFILE = Path(__file__).with_name('data') / 'ally_entry_counter_wave8_profiles.json'
RULE = 'Whenever this creature or another Ally you control enters, you may put a +1/+1 counter on this creature.'
ENTRY = 'Whenever this creature or another Ally you control enters, '
COMPOUNDS = {
    'First strike, vigilance\n' + RULE: 'first_vigilance',
    ENTRY + 'you may put a +1/+1 counter on each Ally creature you control.': 'team',
    ENTRY + 'you may create a 2/2 green Wolf creature token. If you do, put a +1/+1 counter on this creature.': 'wolf',
    ENTRY + 'you may have Allies you control gain lifelink until end of turn, and you may put a +1/+1 counter on this creature.': 'talus',
}
COMPOUND_HANDLERS = {'first_vigilance': 'manalinkeh_card_kazandu_blademaster', 'team': 'manalinkeh_card_kazuul_warlord',
                     'wolf': 'manalinkeh_card_turntimber_ranger', 'talus': 'manalinkeh_card_talus_paladin'}
TALUS_OPTIONS = {(True, False): 0, (False, True): 1, (True, True): 2, (False, False): 3}
KEYWORD_MODES = {None: (), 'Defender': ('defender_attack', 'defender_block'), 'Flying': ('flying_ground', 'flying_reach'),
                 'Haste': ('fresh_haste',), 'Trample': ('trample_combat',)}
HANDLERS = {'manalinkeh_card_hada_freeblade', 'manalinkeh_card_makindi_shieldmate',
            'manalinkeh_card_oran_rief_survivalist', 'manalinkeh_card_tuktuk_grunts'}
ALLY = 'Cliffside Lookout'
CHANGELING = 'Avian Changeling'
FILLER = "Mons's Goblin Raiders"
WITNESSES = {ALLY, CHANGELING, FILLER, 'Elvish Piper', 'Unsummon', 'Sol Ring', 'Counterspell', 'Grizzly Bears', 'Giant Spider', 'Wolf'}
COMMON = ('self_take', 'friendly_ally', 'opposing_ally', 'nonally_creature', 'noncreature', 'changeling', 'sequence',
          'two_sources', 'piper_entry', 'countered_entry', 'wrong_color', 'insufficient_mana', 'bounce_reset', 'pending_replay')
HUMAN = ('self_decline', 'friendly_decline', 'take_after_decline')
GAPS = ('post-trigger priority response and simultaneous multi-entrant or copy entry', 'explicit AI refusal')


def profiles():
    return json.loads(PROFILE.read_text(encoding='utf8'))


def is_ally(record):
    return ('Ally' in record.get('type_line', '').partition(' — ')[2].split()
            or 'Changeling' in record.get('keywords', []))


def classify(record):
    from recipes import cost_from_oracle

    if (not record or record.get('layout') != 'normal' or record.get('selected_face') or record.get('card_faces')
            or not record.get('type_line', '').startswith('Creature — ') or not is_ally(record)):
        raise ValueError('normal standalone Ally creature required')
    text = record.get('oracle_text', '')
    compound = COMPOUNDS.get(text)
    if compound:
        keywords = ['Vigilance', 'First strike'] if compound == 'first_vigilance' else []
    else:
        prefix, rule, tail = text.partition(RULE)
        keyword = prefix[:-1] if prefix.endswith('\n') else None
        if not rule or tail or prefix != (keyword + '\n' if keyword else '') or keyword not in KEYWORD_MODES:
            raise ValueError('unsupported Ally companion ability')
        keywords = [keyword] if keyword else []
    if sorted(record.get('keywords', [])) != sorted(keywords):
        raise ValueError('Ally companion keyword record changed')
    stats = [record.get(prop) for prop in ('power', 'toughness')]
    if any(not isinstance(value, str) or not value.isascii() or not value.isdecimal() for value in stats):
        raise ValueError('fixed Ally creature statistics required')
    result = dict(mana=cost_from_oracle(record), power=int(stats[0]), toughness=int(stats[1]), keywords=keywords)
    if compound:
        result['compound'] = compound
    return result


def combat_modes(params):
    return KEYWORD_MODES.get(next(iter(params['keywords']), None), ())


def variants(params):
    compound = params.get('compound')
    base = COMMON + combat_modes(params)
    human = HUMAN + (('fresh_haste_decline',) if 'Haste' in params['keywords'] else ())
    extra = {'first_vigilance': ('first_strike_vigilance',), 'team': ('team_scope',), 'wolf': ('wolf_sequence',),
             'talus': ('talus_both', 'talus_expiry', 'talus_later')}
    base += extra.get(compound, ())
    if compound == 'talus':
        base = tuple(mode for mode in base if mode not in ('two_sources', 'piper_entry'))
        human += ('talus_lifelink', 'talus_counter', 'talus_neither', 'talus_two_choices')
    return [f'{mode}_{owner}' for owner in (0, 1) for mode in base + (human if owner == 0 else ())]


def check_variants(params, selected):
    if (type(selected) is not list or not selected or any(type(item) is not str for item in selected)
            or len(set(selected)) != len(selected) or not set(selected) <= set(variants(params))):
        raise ValueError('distinct reviewed Ally variants required')


def spec(declaration, record):
    expected = classify(record)
    pin = profiles()['sources'].get(record['name'])
    if (declaration.get('family') != FAMILY or type(declaration.get('version')) is not int or declaration['version'] != 2
            or not pin or pin['oracle'] != record or declaration.get('oracle_text') != record['oracle_text']
            or json.dumps(expected, sort_keys=True) != json.dumps(declaration.get('params'), sort_keys=True)):
        raise ValueError('whole Ally Oracle and exact typed parameters required')
    check_variants(expected, declaration.get('variants'))
    return dict(expected, family=FAMILY, effect=FAMILY)


def validate_witnesses(oracle, registry):
    data = profiles()
    excluded = {row['csvid'] for row in json.loads((PROFILE.parent / 'unimplemented.json').read_text(
        encoding='utf8'))['cases'].values()}
    if data['version'] != 1 or set(data['witnesses']) != WITNESSES or set(data['sources']) & WITNESSES:
        raise ValueError('reviewed Ally profile/witness set changed')
    for group in ('sources', 'witnesses'):
        for name, pin in data[group].items():
            row = registry.get(name, {})
            if (pin['oracle'] != oracle.get(name) or pin['registry'] != row or row.get('kind') != 'card'
                    or row['csvid'] in excluded or row['handler'] == 'NULL'
                    or (row['handler'] == 'card_dummy' and not pin.get('reviewed_native_characteristics'))):
                raise ValueError(f'changed Ally Oracle or registry binding: {name}')
            if group == 'sources':
                model = classify(oracle[name])
                allowed = {COMPOUND_HANDLERS[model['compound']]} if 'compound' in model else HANDLERS
                if row['handler'] not in allowed:
                    raise ValueError(f'unreviewed Ally ETB handler: {name}')
    if (not all(is_ally(oracle[name]) for name in (ALLY, CHANGELING))
            or any(is_ally(oracle[name]) for name in ('Grizzly Bears', 'Sol Ring'))):
        raise ValueError('Ally positive and negative witnesses no longer contrast')
    wolf = oracle['Wolf']
    if (wolf['layout'] != 'token' or wolf['type_line'] != 'Token Creature — Wolf'
            or (wolf['power'], wolf['toughness'], wolf['colors']) != ('2', '2', ['G'])
            or wolf['keywords'] or wolf['oracle_text'] or is_ally(wolf) or registry['Wolf']['csvid'] != 18856):
        raise ValueError('reviewed Wolf token record changed')


class AllyWorld(_AttackResourceWorld):
    def __init__(self, name, params, owner, rows, oracle):
        super().__init__(name, params, owner, rows, oracle)
        self.cards = {}
        self.foe = 1 - owner
        self.compound = params.get('compound')

    def add(self, name, ref, player=None, zone='hand', source=False):
        player = self.owner if player is None else player
        super().add(name, ref, player, zone, source)
        self.cards[ref] = (name, player)
        if ref in self.entities:
            self.entities[ref]['counters'] = 0

    def observe(self, ref, field, value):
        self.emit('assert_card', ref=ref, property=field, value=int(value))

    def receipts(self):
        self.emit('refresh')
        start = len(self.steps)
        super().receipts()
        checks = []
        for check in self.steps[start:]:
            if check.get('property') == 'sick' and self.has(check['ref'], 'Haste'):
                continue
            if check.get('property') == 'plus1_counters':
                check['value'] = self.entities[check['ref']]['counters']
            checks.append(check)
        self.steps[start:] = checks
        if self.compound == 'wolf':
            csvid = self.rows['Wolf']['csvid']
            for player in (0, 1):
                count = self.zones['battlefield'][player].count('Wolf')
                self.emit('assert_zone_multiplicity', player=player, zone='battlefield', index=csvid, value=count)
                if count:
                    for field, value in (('token', 1), ('power', 2), ('toughness', 2), ('color', 8),
                                         ('subtype_Wolf', 1), ('subtype_Ally', 0)):
                        self.emit('assert_matching_permanents', property=field, player=player, index=csvid, value=value)

    def paid_cast(self, ref, *, denied=None, response=False):
        from recipes import cost_from_oracle

        name, player = self.cards[ref]
        cost = cost_from_oracle(self.o[name])
        if denied:
            cost = [sum(cost) if denied == 'wrong_color' else 0, 0, 0, 0, 0, 0]
        self.pool(player, cost)
        self.emit('refresh')
        self.emit('cast_response' if response else 'cast', ref=ref, value=int(denied is None))
        self.emit('assert_choices_consumed')
        self.emit('assert_stack', value=0 if denied else 2 if response else 1)
        if not denied:
            self.mana[player] = [0] * 6
        for color, value in enumerate(self.mana[player]):
            self.emit('assert_mana', player=player, color=color, value=value)
        self.observe(ref, 'in_play', 0)
        self.observe(ref, 'in_hand', denied is not None)

    def watchers(self, ref):
        name, player = self.cards[ref]
        if not is_ally(self.o[name]):
            return []
        return [key for key, obj in self.entities.items() if obj['source'] and obj['player'] == player
                and (obj['zone'] == 'battlefield' or key == ref)]

    def queue_growth(self, entrant, accepted):
        eligible = self.watchers(entrant)
        take = eligible if accepted is None else accepted
        if not set(take) <= set(eligible):
            raise ValueError('only actual eligible Ally triggers may be accepted')
        for ref in take:
            player = self.entities[ref]['player']
            if player == 0:
                self.emit('choose_trigger' if ref == entrant else 'choose_trigger_source', ref=ref, player=player)
        return take

    def finish_entry(self, ref, take):
        if ref in self.entities:
            self.arrive(ref)
        else:
            name, player = self.cards[ref]
            self.zones['hand'][player].remove(name)
            self.zones['battlefield'][player].append(name)
            self.observe(ref, 'in_play', 1)
        for watcher in take:
            self.apply_growth(watcher)
        # Diagnose a missing or auto-resolved trigger as an ordinary receipt.
        # Leave no queued choices to affect a later spell or declaration.
        self.emit('assert_unused_choices', value=0)
        self.receipts()

    def apply_growth(self, watcher):
        source = self.entities[watcher]
        player = source['player']
        if self.compound == 'team':
            recipients = [entity for entity in self.entities.values()
                          if entity['player'] == player and entity['zone'] == 'battlefield' and is_ally(self.o[entity['name']])]
        else:
            recipients = [source]
        for entity in recipients:
            entity['counters'] += 1
            entity['power'] += 1
            entity['toughness'] += 1
        if self.compound == 'wolf':
            self.zones['battlefield'][player].append('Wolf')

    def resolve_talus(self, ref, *, lifelink, counter):
        watchers = self.watchers(ref)
        if len(watchers) > 1:
            raise ValueError('independent multi-Talus menus require calibration')
        if watchers and self.owner == 0:
            self.emit('choose_dialog', player=self.owner, value=TALUS_OPTIONS[lifelink, counter])
        if watchers and self.owner == 1 and not (lifelink and counter):
            raise ValueError('the AI takes both actual Talus choices')
        self.emit('resolve')
        if watchers and lifelink:
            for key, obj in self.entities.items():
                if obj['player'] == self.owner and (obj['zone'] == 'battlefield' or key == ref) and is_ally(self.o[obj['name']]):
                    obj['grants'].add('Lifelink')
        self.finish_entry(ref, watchers if counter else [])

    def resolve_entry(self, ref, accepted=None):
        if self.compound == 'talus':
            self.resolve_talus(ref, lifelink=accepted is None, counter=accepted is None)
            return
        take = self.queue_growth(ref, accepted)
        self.emit('resolve')
        self.finish_entry(ref, take)

    def enter(self, ref, accepted=None):
        self.paid_cast(ref)
        self.resolve_entry(ref, accepted)

    def piper_entry(self):
        from recipes import COLORS

        cost = [0] * 6
        cost[COLORS['G']] = 1
        self.pool(self.owner, cost)
        self.emit('refresh')
        self.emit('require_can_activate', ref='piper', value=1)
        self.emit('activate', ref='piper', value=1)
        self.emit('assert_stack', value=1)
        self.emit('assert_choices_consumed')
        self.mana[self.owner] = [0] * 6
        self.entities['piper']['tapped'] = True
        csvid = self.rows[ALLY]['csvid']
        self.emit('choose_zone_card', player=self.owner, value=csvid)
        take = self.queue_growth('first', None)
        self.emit('resolve')
        self.emit('bind_permanent_copy', ref='first', player=self.owner, index=0, value=csvid)
        self.finish_entry('first', take)

    def countered_entry(self):
        self.paid_cast('first')
        self.emit('announce_spell', ref='first')
        self.paid_cast('counter', response=True)
        self.emit('resolve_response')
        self.leave('first', 'graveyard')
        self.zones['hand'][self.foe].remove('Counterspell')
        self.zones['graveyard'][self.foe].append('Counterspell')
        self.observe('first', 'in_play', 0)
        self.receipts()

    def bounce_source(self):
        self.spell('Unsummon', 'bounce', self.foe, 'source')
        self.leave('source', 'hand')
        self.entities['source'].update(counters=0, power=self.p['power'], toughness=self.p['toughness'], damage=0)
        self.emit('bind_hand_copy', ref='source', player=self.owner, index=0, value=self.rows[self.name]['csvid'])
        self.receipts()

    def combat(self, mode):
        fresh = mode.startswith('fresh_haste')
        defending = mode == 'defender_block'
        if not fresh:
            self.next_turn()
            if not defending:
                self.next_turn()
        attacker, blocker = ('opposing', 'source') if defending else ('source', 'opposing')
        attack_allowed = mode != 'defender_attack'
        block_allowed = mode in ('trample_combat', 'defender_block', 'flying_reach', 'first_strike_vigilance')
        self.emit('begin_attackers')
        self.emit('attack', ref=attacker, value=int(attack_allowed))
        self.entities[attacker].update(attacking=attack_allowed, tapped=attack_allowed and not self.has(attacker, 'Vigilance'))
        self.observe(attacker, 'attacking', attack_allowed)
        self.emit('finish_attackers')
        self.emit('begin_blockers')
        if blocker in self.entities:
            self.emit('block', ref=blocker, other=attacker, value=int(block_allowed))
            self.observe(blocker, 'blocking_count', block_allowed)
        self.emit('advance_combat')
        self.emit('combat_damage')
        self.emit('resolve')
        self.emit('end_combat')
        a = self.entities[attacker]
        if block_allowed:
            b = self.entities[blocker]
            dealt = min(a['power'], b['toughness']) if mode == 'trample_combat' else a['power']
            if not (mode == 'first_strike_vigilance' and dealt >= b['toughness']):
                a['damage'] += b['power']
            b['damage'] += dealt
            if mode == 'trample_combat':
                self.life[b['player']] -= a['power'] - dealt
            for ref, obj in ((attacker, a), (blocker, b)):
                if obj['damage'] >= obj['toughness']:
                    self.leave(ref, 'graveyard')
        elif attack_allowed:
            self.life[1-self.current] -= a['power']
        a['attacking'] = False
        self.receipts()

    def talus_attack(self, attackers):
        self.emit('begin_attackers')
        for ref in attackers:
            self.emit('attack', ref=ref, value=1)
            self.entities[ref].update(attacking=True, tapped=True)
        self.emit('finish_attackers')
        self.emit('begin_blockers')
        self.emit('advance_combat')
        self.emit('combat_damage')
        self.emit('resolve')
        self.emit('end_combat')
        for ref in attackers:
            entity = self.entities[ref]
            self.life[self.foe] -= entity['power']
            if self.has(ref, 'Lifelink'):
                self.life[self.owner] += entity['power']
            entity['attacking'] = False
        self.receipts()


def talus_combat(world, mode):
    entry = {'talus_lifelink': (True, False), 'talus_counter': (False, True), 'talus_neither': (False, False),
             'talus_two_choices': (True, False)}
    lifelink, counter = entry.get(mode, (True, True))
    world.paid_cast('source')
    world.resolve_talus('source', lifelink=lifelink, counter=counter)
    if mode in ('talus_expiry', 'talus_later', 'talus_two_choices'):
        world.next_turn()
        world.next_turn()
        if mode == 'talus_later':
            world.enter('first')
        elif mode == 'talus_two_choices':
            world.paid_cast('first')
            world.resolve_talus('first', lifelink=False, counter=True)
        world.talus_attack(('source', 'ally', 'nonally'))
    else:
        world.talus_attack(('ally', 'nonally'))


def scenario(world, mode):
    owner, foe = world.owner, world.foe
    world.add(world.name, 'source', source=True)
    if mode in ('friendly_ally', 'friendly_decline', 'take_after_decline', 'piper_entry', 'countered_entry',
                'pending_replay', 'sequence', 'bounce_reset', 'opposing_ally', 'two_sources'):
        world.add(ALLY, 'first', foe if mode == 'opposing_ally' else owner)
    if mode in ('nonally_creature', 'noncreature', 'changeling'):
        world.add({'nonally_creature': 'Grizzly Bears', 'noncreature': 'Sol Ring', 'changeling': CHANGELING}[mode], 'first')
    if mode == 'sequence':
        world.add('Grizzly Bears', 'second')
        world.add(ALLY, 'third')
    if mode in ('team_scope', 'wolf_sequence'):
        world.add(CHANGELING, 'first')
    if mode == 'wolf_sequence':
        world.add('Grizzly Bears', 'second')
        world.add(ALLY, 'third')
    if mode == 'team_scope':
        world.add(ALLY, 'ally', zone='battlefield')
        world.add(ALLY, 'enemy', foe, 'battlefield')
        world.add('Grizzly Bears', 'nonally', zone='battlefield')
    if mode.startswith('talus_'):
        world.add(CHANGELING, 'ally', zone='battlefield')
        world.add('Grizzly Bears', 'nonally', zone='battlefield')
        if mode in ('talus_later', 'talus_two_choices'):
            world.add(ALLY, 'first')
    if mode == 'two_sources':
        world.add(world.name, 'copy', source=True)
    if mode == 'piper_entry':
        world.add('Elvish Piper', 'piper', zone='battlefield')
    if mode == 'countered_entry':
        world.add('Counterspell', 'counter', foe)
    if mode == 'bounce_reset':
        world.add('Unsummon', 'bounce', foe)
        world.add(ALLY, 'second')
    if mode in ('trample_combat', 'defender_block', 'flying_ground', 'flying_reach'):
        world.add('Giant Spider' if mode == 'flying_reach' else FILLER, 'opposing', foe, 'battlefield')
    if mode == 'first_strike_vigilance':
        world.add('Grizzly Bears', 'opposing', foe, 'battlefield')

    world.emit('turn', player=owner, value=0)
    world.emit('present_ai_dialogs', value=1)
    for player in (0, 1):
        world.pool(player, [0] * 6)
    world.receipts()
    if mode.startswith('talus_'):
        talus_combat(world, mode)
        return None
    if mode in ('insufficient_mana', 'wrong_color'):
        world.paid_cast('source', denied=mode)
        world.receipts()
        return None
    decline_self = mode in ('self_decline', 'take_after_decline', 'fresh_haste_decline')
    world.enter('source', [] if decline_self else None)
    if mode == 'piper_entry':
        world.piper_entry()
    elif mode == 'countered_entry':
        world.countered_entry()
    elif mode == 'pending_replay':
        world.paid_cast('first')
        cut = len(world.steps)
        world.resolve_entry('first')
        return cut
    elif mode in combat_modes(world.p) or mode in ('fresh_haste_decline', 'first_strike_vigilance'):
        world.combat(mode)
    elif mode == 'two_sources':
        world.enter('copy', ['source'] if owner == 0 else None)
        world.enter('first', ['copy'] if owner == 0 else None)
    elif 'first' in world.cards:
        if mode == 'opposing_ally':
            world.next_turn()
        world.enter('first', [] if mode == 'friendly_decline' else None)
        if mode == 'bounce_reset':
            world.bounce_source()
            world.enter('source')
        for ref in ('second', 'third'):
            if ref in world.cards:
                world.enter(ref)
    return None


def expand(name, csvid, declaration, params, selected, registry, oracle):
    from recipes import step

    check_variants(params, selected)
    validate_witnesses(oracle, registry)
    if profiles()['sources'][name]['registry']['csvid'] != csvid:
        raise ValueError('Ally source CSVID changed')
    cases = []
    for variant in selected:
        mode, owner = variant.rsplit('_', 1)
        world = AllyWorld(name, params, int(owner), registry, oracle)
        cut = scenario(world, mode)
        actions = world.steps
        if cut is not None:
            tail = actions[cut:]
            actions = (actions[:cut] + [step('capture')] + tail + [step('record_state'), step('ai_roundtrip'), step('restore')]
                       + copy.deepcopy(tail) + [step('assert_recorded_state')])
        scope = list(GAPS)
        if params.get('compound') == 'talus':
            scope.append('two Talus permanents offering independently ordered inner menus in the same entry window')
        cases.append(dict(name=f'AllyEntryCounterRecipe_{csvid}_{variant}', card=name, fixture='empty_main', seed=227,
                          setup=world.setup, steps=actions, _recipe=dict(family=FAMILY, version=2, variant=variant,
                              params=declaration['params'], oracle_text=declaration['oracle_text'], untested_components=scope)))
    return cases
