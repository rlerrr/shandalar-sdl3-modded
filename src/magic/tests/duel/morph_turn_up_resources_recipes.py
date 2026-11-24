"""Paid Morph with actual damage, mana, untap, upkeep and Scry consequences."""

import copy
import json
from pathlib import Path

from morph_creature_recipes import FACE_DOWN, MORPH
from morph_with_keywords_recipes import _World as _MorphWorld


FAMILY = 'morph_turn_up_resources'
PROFILE = Path(__file__).parent / 'data/morph_turn_up_resources_wave8_profiles.json'
TURN_UP = 'When this creature is turned face up, '
PHOENIX = 'When this creature dies, return it to the battlefield face down under your control.'
SCRY = ('Whenever this creature or another permanent is turned face up, you may scry 2. (Look at the top two '
        'cards of your library, then put any number of them on the bottom and the rest on top in any order.)')
PATTERNS = {
    (('Flying', PHOENIX), TURN_UP + 'it deals 2 damage to each player.'): ('phoenix', ['Flying'], 'ashcloud_phoenix'),
    ((), TURN_UP + 'each opponent skips their next untap step.'): ('brine', [], 'brine_elemental'),
    (('{T}: Add {G}, {U}, or {R}.',), TURN_UP + 'add {G}{U}{R}.'): ('rattleclaw', [], 'rattleclaw_mystic'),
    (('Flying',), TURN_UP + 'put two time counters on it and it gains vanishing. (At the beginning of your upkeep, '
                             'remove a time counter from it. When the last is removed, sacrifice it.)'):
        ('djinn', ['Flying'], 'maelstrom_djinn'),
    ((), SCRY): ('bleb', ['Scry'], 'unblinking_bleb'),
}
ADEPT = 'Flamespeaker Adept'
SPENDER = 'Savage Knuckleblade'
OTHER = 'Scornful Egotist'
LIBRARY = ('Plains', 'Island', 'Mountain', 'Forest')
WITNESSES = {*LIBRARY, ADEPT, SPENDER, OTHER, 'Twiddle', 'Murder', 'Shock', 'Safe Passage',
             'Counterspell', 'Aether Snap', 'Hill Giant', 'Wall of Vines'}
DUMMIES = {'Hill Giant', 'Wall of Vines'}
COMMON = ('printed_cast', 'hidden_cast', 'hidden_choice', 'short_cast', 'paid_reveal', 'reveal_short',
          'reveal_offturn', 'reveal_replay', 'hidden_countered', 'hidden_hit')
EXTRA = {
    'phoenix': ('prevent_damage', 'dies_returns', 'hidden_death', 'returns_reveals', 'flying_ground', 'flying_reach'),
    'brine': ('skip_once', 'normal_untap', 'skip_after_removal', 'skip_offturn'),
    'rattleclaw': ('mana_g', 'mana_u', 'mana_r', 'mana_sick', 'reveal_spend', 'no_hidden_mana'),
    'djinn': ('vanishing_cycle', 'normal_no_vanishing', 'time_other_upkeep', 'snap_vanishing',
              'flying_ground', 'flying_reach'),
    'bleb': ('scry_one', 'scry_empty', 'hidden_other_turns_up'),
}
HUMAN_BLEB = ('scry_reorder', 'scry_decline', 'friendly_other_turns_up', 'opposing_other_turns_up')


def profiles():
    return json.loads(PROFILE.read_bytes())


def classify(record):
    from recipes import COLORS, cost_from_oracle

    if (record.get('layout') != 'normal' or record.get('card_faces') or record.get('selected_face')
            or not record['type_line'].startswith('Creature — ')):
        raise ValueError('normal whole Morph resource creature required')
    lines = record['oracle_text'].splitlines()
    if len(lines) < 2:
        raise ValueError('entire printed resource ability required')
    fee = MORPH.fullmatch(lines[-2])
    clauses = (tuple(lines[:-2]), lines[-1])
    match = PATTERNS.get(clauses)
    stats = [record.get('power'), record.get('toughness')]
    if (not fee or not match or sorted(record['keywords']) != sorted(['Morph', *match[1]])
            or any(not isinstance(value, str) or not value.isascii() or not value.isdecimal() for value in stats)):
        raise ValueError('unreviewed full Oracle resource clause, fee or statistics')
    mode, keyword_tags, _ = match
    return dict(mode=mode, mana=cost_from_oracle(record), morph=cost_from_oracle(dict(record, mana_cost=fee[1])),
                power=int(stats[0]), toughness=int(stats[1]), keywords=[key for key in keyword_tags if key != 'Scry'],
                color=sum(1 << COLORS[c] for c in record['colors']))


def handler(record):
    lines = record['oracle_text'].splitlines()
    return 'manalinkeh_card_' + PATTERNS[(tuple(lines[:-2]), lines[-1])][2]


def variants(params):
    mode = params['mode']
    ordinary = COMMON + EXTRA[mode] + (('reveal_wrong_color',) if any(params['morph'][1:]) else ())
    return ([f'{scenario}_{player}' for scenario in ordinary for player in (0, 1)]
            + ([f'{scenario}_0' for scenario in HUMAN_BLEB] if mode == 'bleb' else []))


def spec(declaration, record):
    params = classify(record)
    pin = profiles()['sources'].get(record['name'])
    if (declaration.get('family') != FAMILY or type(declaration.get('version')) is not int
            or declaration['version'] != 2 or not pin or pin['oracle'] != record
            or declaration.get('oracle_text') != record['oracle_text']
            or json.dumps(declaration.get('params'), sort_keys=True) != json.dumps(params, sort_keys=True)):
        raise ValueError('exact typed complete resource Morph declaration required')
    selected = declaration.get('variants')
    if (type(selected) is not list or not selected or any(type(s) is not str for s in selected)
            or len(set(selected)) != len(selected) or not set(selected) <= set(variants(params))):
        raise ValueError('approved distinct resource Morph cases required')
    return dict(params, family=FAMILY, effect=FAMILY)


def validate_witnesses(oracle, registry):
    pins = profiles()
    if pins.get('version') != 1 or set(pins['witnesses']) != WITNESSES:
        raise ValueError('complete independent resource Morph witnesses required')
    for group in ('witnesses', 'sources'):
        for name, pin in pins[group].items():
            row = registry.get(name, {})
            if (pin['oracle'] != oracle.get(name) or pin['registry'] != row or row['kind'] != 'card'
                    or row['handler'] in ('NULL', None) or row['handler'] == 'card_dummy' and
                    (name not in DUMMIES or not pin.get('reviewed_native_characteristics'))):
                raise ValueError(f'changed Morph resource Oracle or binding: {name}')
            if group == 'sources' and row['handler'] != handler(oracle[name]):
                raise ValueError(f'changed source handler: {name}')
    if pins['face_down']['csvid'] != FACE_DOWN or registry.get(pins['face_down']['name']) != pins['face_down']:
        raise ValueError('changed real face-down representation')


class _World(_MorphWorld):
    def __init__(self, name, params, actor, rows, oracle):
        super().__init__(name, params, actor, rows, oracle)
        self.land_taps = {}
        self.time = 0
        self.vanish = False
        self.recreated = False
        self.legal_scry_order = False
        self.safe = set()

    def source_cast(self, *, hidden, choice=False, short=False):
        hidden_mana = [3, 0, 0, 0, 0, 0]
        printed = self.p['mana']
        combined = [max(3, printed[0]), *printed[1:]]
        budget = [2, 0, 0, 0, 0, 0] if short else combined if choice else hidden_mana if hidden else printed
        dialog = 0 if choice else 1 if not hidden and sum(printed) >= 3 else None
        self.pending('source', allowed=not short, price=budget, spend=hidden_mana if hidden else printed, dialog=dialog)
        if not short:
            self.hidden = hidden
            if hidden:
                self.entities['source'].update(power=2, toughness=2, keys=set())
            self.identity()

    def receipts(self):
        start = len(self.steps)
        super().receipts()
        if self.legal_scry_order or self.recreated:
            self.steps[start:] = [step for step in self.steps[start:] if not (
                self.legal_scry_order and step['op'] == 'assert_zone_card'
                and step['zone'] == 'library' and step['player'] == self.owner
                or self.recreated and step.get('ref') == 'source')]
        for ref, tapped in self.land_taps.items():
            self.observe(ref, 'tapped', tapped)
        if self.p['mode'] == 'djinn' and self.cards['source'][2] == 'battlefield':
            self.emit('assert_counter', ref='source', counter_kind='time', value=self.time)
        if self.recreated:
            for prop, value in (('power', 2), ('toughness', 2)):
                self.emit('assert_matching_permanents', player=self.owner, index=FACE_DOWN, property=prop, value=value)

    def visit(self, *, skip=False, upkeep=False):
        if any(any(wallet) for wallet in self.mana):
            raise ValueError('actual turn/untap requires empty pools')
        self.emit('advance_turn')
        self.expire()
        self.current = 1 - self.current
        self.safe.clear()
        self.emit('untap_step', player=self.current)
        for obj in self.entities.values():
            if obj['zone'] == 'battlefield' and obj['player'] == self.current:
                obj['sick'] = False
                if not skip:
                    obj['tapped'] = False
        for ref in self.land_taps:
            if self.cards[ref][1] == self.current and not skip:
                self.land_taps[ref] = False
        for ref in ('adept', 'opposing_adept'):
            if ref in self.entities:
                self.entities[ref]['power'] = int(self.o[ADEPT]['power'])
        if upkeep:
            self.emit('upkeep_step', player=self.current)
            if self.p['mode'] == 'djinn' and self.current == self.owner and self.vanish and self.time:
                self.time -= 1
                if not self.time:
                    self.move('source', 'graveyard')
        self.emit('turn', player=self.current, value=0)
        self.receipts()

    def tap_land(self, spell, land):
        self.pending(spell, target=land)
        self.emit('choose_dialog', player=self.owner, value=0)
        self.land_taps[land] = True
        self.finish_spell(spell, 'graveyard')

    def scry_plan(self, scenario):
        library = self.zones['library'][self.owner]
        decline = scenario == 'scry_decline'
        if self.owner == 1:
            if library:
                self.emit('record_library', player=self.owner)
                # The AI Scry helper calls an optional selector with fewer
                # than four available lands, even when all examined cards are lands.
                self.emit('choose_zone', player=self.owner, value=-1)
                self.legal_scry_order = True
        else:
            # The current self handler enters an unconditional Scry menu.
            # Decline leaves these fallback inputs queued and observes no Scry.
            if scenario == 'scry_reorder':
                self.emit('choose_zone', player=0, value=-1)
                self.emit('choose_zone', player=0, value=1)
                self.emit('choose_zone', player=0, value=-1)
                library[:2] = library[:2][::-1]
            else:
                examined = min(2, len(library))
                for _ in range(examined):
                    self.emit('choose_zone', player=0, value=0)
                if not decline:
                    library[:] = library[examined:] + library[:examined]
                elif examined:
                    return examined
        if 'adept' in self.entities:
            self.entities['adept']['power'] += 2
        return 0

    def reveal_resource(self, scenario, ref='source', denied=None):
        from recipes import COLORS, cost_from_oracle

        name, player, _ = self.cards[ref]
        record = self.o[name]
        fee = (list(self.p['morph']) if ref == 'source' else
               cost_from_oracle(dict(record, mana_cost=MORPH.fullmatch(record['oracle_text'])[1])))
        if denied == 'reveal_short':
            fee[next(i for i, count in enumerate(fee) if count)] -= 1
        elif denied:
            fee[next(i for i in range(1, 6) if fee[i])] -= 1
            fee[next(i for i in range(1, 6) if not self.p['morph'][i])] += 1
        self.pool(player, fee)
        self.emit('refresh')
        self.emit('can_activate', ref=ref, value=int(denied is None))
        scry = (self.p['mode'] == 'bleb' and denied is None
                and (ref == 'source' or not self.hidden))
        unused = self.scry_plan(scenario) if scry else 0
        self.emit('activate', ref=ref, value=int(denied is None))
        if denied is None:
            self.mana[player] = [0] * 6
            model = self.p if ref == 'source' else self.o[name]
            self.entities[ref].update(power=int(model['power']), toughness=int(model['toughness']),
                                      keys=set(model['keywords']))
            if ref == 'source':
                self.hidden = False
                self.recreated = False
                mode = self.p['mode']
                if mode == 'phoenix':
                    for seat in (0, 1):
                        self.life[seat] -= 2 if seat not in self.safe else 0
                elif mode == 'rattleclaw':
                    for color in 'GUR':
                        self.mana[player][COLORS[color]] += 1
                elif mode == 'djinn':
                    self.time, self.vanish = 2, True
            self.emit('refresh')
            self.observe(ref, 'csvid', self.rows[name]['csvid'])
            self.observe(ref, 'power', int(model['power']))
            self.observe(ref, 'toughness', int(model['toughness']))
            self.wallets()
            self.emit('resolve')
        self.emit('assert_unused_choices', value=unused)
        if scry and self.legal_scry_order:
            self.emit('assert_scry_result', player=self.owner, value=2)
        self.receipts()

    def dies(self, returns):
        self.pending('burn', target='source')
        self.emit('resolve')
        self.move('burn', 'graveyard')
        if returns:
            self.hidden = True
            self.recreated = True
            self.entities['source'].update(power=2, toughness=2, keys=set(), sick=True, damage=0, tapped=False)
        else:
            self.move('source', 'graveyard')
        self.receipts()

    def activate_mana(self, color=None):
        from recipes import COLORS

        allowed = color is not None
        self.emit('refresh')
        self.emit('can_activate', ref='source', value=int(allowed))
        if allowed:
            self.emit('choose_color', player=self.owner, value=COLORS[color])
        self.emit('activate', ref='source', value=int(allowed))
        if allowed:
            self.mana[self.owner][COLORS[color]] += 1
            self.entities['source']['tapped'] = True
        self.receipts()

    def spend_produced(self):
        self.emit('refresh')
        self.emit('cast', ref='spender', value=1)
        self.mana[self.owner] = [0] * 6
        self.emit('assert_stack', value=1)
        self.wallets()
        self.finish_spell('spender', 'battlefield')


BRINE = {'skip_once', 'normal_untap', 'skip_after_removal', 'skip_offturn'}
VANISHING = {'vanishing_cycle', 'normal_no_vanishing', 'time_other_upkeep', 'snap_vanishing'}
REBIRTH = {'dies_returns', 'hidden_death', 'returns_reveals'}
OTHER_TURNS_UP = {'hidden_other_turns_up', 'friendly_other_turns_up', 'opposing_other_turns_up'}
STAY_HIDDEN = {'hidden_cast', 'hidden_choice', 'hidden_hit', 'no_hidden_mana', 'hidden_death'}


def brine_sequence(w, scenario):
    if scenario == 'skip_offturn':
        w.visit()
    w.tap_land('tap_own', 'own_land')
    w.tap_land('tap_foe', 'opposing_land')
    if scenario != 'normal_untap':
        w.reveal_resource(scenario)
    if scenario == 'skip_after_removal':
        w.pending('removal', target='source')
        w.emit('resolve')
        w.move('removal', 'graveyard')
        w.move('source', 'graveyard')
        w.receipts()
    if w.current == w.foe:
        w.visit()
    w.visit(skip=scenario != 'normal_untap')
    w.visit()
    w.visit()


def djinn_sequence(w, scenario):
    if scenario == 'snap_vanishing':
        w.pending('snap')
        w.emit('resolve')
        w.move('snap', 'graveyard')
        w.move('source', 'graveyard')
        w.receipts()
        return
    w.visit(upkeep=True)
    if scenario == 'time_other_upkeep':
        return
    for _ in range(3):
        w.visit(upkeep=True)


def build_variant(name, params, scenario, owner, registry, oracle):
    from recipes import step

    w = _World(name, params, owner, registry, oracle)
    mode, foe = params['mode'], w.foe
    w.add(name, 'source', source=True)
    for player in (0, 1):
        stock = LIBRARY
        if player == owner and scenario in ('scry_one', 'scry_empty'):
            stock = LIBRARY[:1] if scenario == 'scry_one' else ()
        if scenario == 'opposing_other_turns_up' and player == foe:
            stock = LIBRARY[:1]
            # Keep any wrong-seat Scry observable on its Adept without a second
            # player's picker. Four available lands skip the AI tutor and a
            # one-card library needs no reorder. The owner keeps a full Scry 2.
            for i in range(4):
                w.add('Forest', f'opposing_hand_land_{i}', foe)
        for i, card in enumerate(stock):
            w.add(card, f'lib_{player}_{i}', player, 'library')
    if mode == 'bleb' and scenario != 'scry_empty':
        w.add(ADEPT, 'adept', zone='battlefield')
    if scenario == 'opposing_other_turns_up':
        w.add(ADEPT, 'opposing_adept', foe, 'battlefield')
    if scenario in ('flying_ground', 'flying_reach'):
        w.add('Hill Giant' if scenario == 'flying_ground' else 'Wall of Vines', 'blocker', foe, 'battlefield')
    if scenario in REBIRTH:
        w.add('Shock', 'burn', foe)
    elif scenario == 'prevent_damage':
        w.add('Safe Passage', 'shield', foe)
    elif scenario == 'hidden_countered':
        w.add('Counterspell', 'counter', foe)
    elif scenario in BRINE:
        w.add('Forest', 'own_land', owner, 'battlefield')
        w.add('Forest', 'opposing_land', foe, 'battlefield')
        w.land_taps.update(own_land=False, opposing_land=False)
        w.add('Twiddle', 'tap_own')
        w.add('Twiddle', 'tap_foe')
        if scenario == 'skip_after_removal':
            w.add('Murder', 'removal', foe)
    elif scenario == 'snap_vanishing':
        w.add('Aether Snap', 'snap')
    elif scenario == 'reveal_spend':
        w.add(SPENDER, 'spender')
    elif scenario in OTHER_TURNS_UP:
        w.add(OTHER, 'other', owner if scenario == 'friendly_other_turns_up' else foe)
    ordinary_cast = {
        'printed_cast', 'dies_returns', 'returns_reveals', 'normal_untap', 'normal_no_vanishing',
        'mana_sick', 'mana_g', 'mana_u', 'mana_r', 'friendly_other_turns_up', 'opposing_other_turns_up',
    }
    w.emit('turn', player=owner, value=0)
    for player in (0, 1):
        w.pool(player, [0] * 6)
    w.source_cast(hidden=scenario not in ordinary_cast, choice=scenario == 'hidden_choice', short=scenario == 'short_cast')
    if scenario == 'short_cast':
        w.receipts()
        return w.setup, w.steps
    if scenario == 'hidden_countered':
        w.emit('announce_spell', ref='source')
        w.pending('counter', response=True)
        w.emit('resolve_response')
        w.move('counter', 'graveyard')
        w.move('source', 'graveyard')
        w.receipts()
        return w.setup, w.steps
    w.finish_spell('source', 'battlefield')
    if scenario in BRINE:
        brine_sequence(w, scenario)
        return w.setup, w.steps
    if scenario in REBIRTH:
        w.dies(returns=scenario != 'hidden_death')
        if scenario == 'returns_reveals':
            w.emit('bind_permanent_copy', ref='source', player=owner, index=0, value=FACE_DOWN)
            w.reveal_resource(scenario)
        return w.setup, w.steps
    if scenario in OTHER_TURNS_UP:
        if scenario != 'friendly_other_turns_up':
            w.visit()
        w.pending('other', price=[3, 0, 0, 0, 0, 0])
        w.emit('resolve')
        w.move('other', 'battlefield')
        w.observe('other', 'csvid', FACE_DOWN)
        w.observe('other', 'power', 2)
        w.observe('other', 'toughness', 2)
        w.emit('assert_stack', value=0)
        w.reveal_resource(scenario, ref='other')
        return w.setup, w.steps
    if scenario in ('hidden_hit', 'flying_ground', 'flying_reach', 'mana_g', 'mana_u', 'mana_r', 'no_hidden_mana'):
        w.visit()
        w.visit()
    elif scenario == 'reveal_offturn':
        w.visit()
    if scenario == 'prevent_damage':
        w.pending('shield')
        w.safe.add(foe)
        w.finish_spell('shield', 'graveyard')
    if scenario == 'reveal_replay':
        w.emit('capture')
    cut = len(w.steps)
    if scenario not in ordinary_cast | STAY_HIDDEN:
        w.reveal_resource(scenario, denied=scenario if scenario in ('reveal_short', 'reveal_wrong_color') else None)
    if scenario == 'reveal_replay':
        tail = copy.deepcopy(w.steps[cut:])
        w.steps.extend([step('record_state'), step('ai_roundtrip'), step('restore'), *tail,
                        step('assert_recorded_state')])
    if scenario in VANISHING:
        djinn_sequence(w, scenario)
    elif scenario in ('mana_g', 'mana_u', 'mana_r'):
        w.activate_mana(scenario[-1].upper())
    elif scenario in ('mana_sick', 'no_hidden_mana'):
        w.activate_mana()
    elif scenario == 'reveal_spend':
        w.spend_produced()
    elif scenario in ('hidden_hit', 'flying_ground', 'flying_reach'):
        w.fight('source', 'blocker' if scenario.startswith('flying_') else None,
                allowed_block=scenario != 'flying_ground')
    return w.setup, w.steps


def expand(name, csvid, declaration, params, selected, registry, oracle):
    pin = profiles()['sources'].get(name)
    row = registry[name]
    if not pin or pin['registry'] != row or row['csvid'] != csvid or row['handler'] != handler(oracle[name]):
        raise ValueError(f'unpinned resource Morph source: {name}')
    result = []
    for variant in selected:
        scenario, owner = variant.rsplit('_', 1)
        setup, steps = build_variant(name, params, scenario, int(owner), registry, oracle)
        gaps = ['separate priority after the immediate turn-up callback', 'multiplayer']
        if params['mode'] == 'bleb':
            gaps.append('independent AI acceptance and decline of optional Scry')
        result.append(dict(name=f'MorphTurnUpResourcesRecipe_{csvid}_{variant}', card=name, fixture='empty_main',
                           seed=123, setup=setup, steps=steps,
                           _recipe=dict(family=FAMILY, version=2, variant=variant, params=declaration['params'],
                                        oracle_text=declaration['oracle_text'], untested_components=gaps)))
    return result
