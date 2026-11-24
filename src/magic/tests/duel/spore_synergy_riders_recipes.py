"""Real spore placement, spending and tribal effects composed with live Fungi."""

import copy
import json
from functools import lru_cache
from pathlib import Path

from attack_value_trigger_recipes import _AttackResourceWorld
from spore_upkeep_riders_recipes import BEAR, COST, GOBLIN, MAKE, TOKEN, UPKEEP, SporeWorld

FAMILY = 'spore_synergy_riders'
PROFILE = Path(__file__).with_name('data') / 'spore_synergy_riders_wave8_profiles.json'
MAKER = 'Thallid'
FUNGUS = 'Fungusaur'
JUDGE = 'Thelon of Havenwood'
WALKER = 'Jace Beleren'
BOUNCE = 'Unsummon'
WITNESSES = {MAKER, FUNGUS, JUDGE, BEAR, GOBLIN, TOKEN, WALKER, BOUNCE}
SHAPES = {
    'placement': ('manalinkeh_card_fungal_bloom', '{G}{G}: Put a spore counter on target Fungus.'),
    'upkeep_team': ('manalinkeh_card_sporesower_thallid',
                    'At the beginning of your upkeep, put a spore counter on each Fungus you control.\n' + MAKE),
    'exile_team': ('manalinkeh_card_thelon_of_havenwood',
                   'Each Fungus creature gets +1/+1 for each spore counter on it.\n'
                   '{B}{G}, Exile a Fungus card from a graveyard: Put a spore counter on each Fungus on the battlefield.'),
    'tribal_lord': ('manalinkeh_card_sporecrown_thallid',
                    "Each other creature you control that's a Fungus or Saproling gets +1/+1."),
    'self_pump': ('manalinkeh_card_thallid_devourer', UPKEEP + '\n' + MAKE + '\n'
                  'Sacrifice a Saproling: This creature gets +1/+2 until end of turn.'),
    'team_pump': ('manalinkeh_card_pallid_mycoderm', UPKEEP + '\n' + MAKE + '\n'
                  "Sacrifice a Saproling: Each creature you control that's a Fungus or a Saproling gets +1/+1 until end of turn."),
    'damage': ('manalinkeh_card_thorn_thallid', UPKEEP + '\n' + COST + 'It deals 1 damage to any target.'),
    'mana': ('manalinkeh_card_utopia_mycon', UPKEEP + '\n' + MAKE + '\n'
             'Sacrifice a Saproling: Add one mana of any color.'),
}
CASES = {
    'placement': ('own_fungus', 'enemy_fungus', 'short_green', 'no_fungus'),
    'upkeep_team': ('team_threshold', 'team_replay'),
    'exile_team': ('own_grave', 'enemy_grave', 'wrong_mana', 'no_fungus_grave', 'exile_replay'),
    'tribal_lord': ('own_saproling', 'enemy_saproling', 'no_lord'),
    'self_pump': ('threshold', 'pump_expiry', 'pump_replay'),
    'team_pump': ('threshold', 'pump_expiry', 'pump_replay'),
    'damage': ('player', 'creature', 'planeswalker', 'missing_target', 'damage_replay'),
    'mana': ('threshold', 'black', 'blue', 'green', 'red', 'white', 'mana_replay'),
}
MODES = {
    'self_pump': ('Generate a Saproling', 'Sac a Saproling and pump'),
    'team_pump': ('Generate a Saproling', 'Pump Fungi & Saprolings'),
    'mana': ('Create a Saproling', 'Sac Saproling and get mana'),
}
COLORS = {'black': 1, 'blue': 2, 'green': 3, 'red': 4, 'white': 5}
SELF_UPKEEP = {'self_pump', 'team_pump', 'damage', 'mana'}
GAPS = ('Exact spore counts use repeated legal spends and Thelon’s actual printed P/T effect.',)


@lru_cache(maxsize=1)
def profiles():
    return json.loads(PROFILE.read_text(encoding='utf8'))


def classify(record):
    from recipes import cost_from_oracle

    if (not record or record.get('layout') != 'normal' or record.get('card_faces')
            or record.get('selected_face') or record.get('keywords')):
        raise ValueError('whole normal spore synergy Oracle required')
    shape = next((key for key, (_, rules) in SHAPES.items() if rules == record['oracle_text']), None)
    type_line = record['type_line']
    if shape == 'placement' and type_line == 'Enchantment':
        extra = {}
    elif shape == 'exile_team' and type_line == 'Legendary Creature — Elf Druid':
        extra = dict(power=int(record['power']), toughness=int(record['toughness']))
    elif shape not in ('placement', 'exile_team', None) and type_line == 'Creature — Fungus':
        extra = dict(power=int(record['power']), toughness=int(record['toughness']))
    else:
        raise ValueError('complete supported spore synergy text and type required')
    return dict(shape=shape, mana=cost_from_oracle(record), keywords=[], **extra)


def variants(params):
    return [f'{mode}_{owner}' for owner in (0, 1) for mode in CASES[params['shape']]]


def check_variants(params, selected):
    if (type(selected) is not list or not selected or any(type(v) is not str for v in selected)
            or len(set(selected)) != len(selected) or set(selected) - set(variants(params))):
        raise ValueError('distinct reviewed spore synergy variants required')


def spec(declaration, record):
    params = classify(record)
    if (declaration.get('family') != FAMILY or type(declaration.get('version')) is not int or declaration['version'] != 2
            or profiles()['sources'].get(record['name'], {}).get('oracle') != record
            or declaration.get('oracle_text') != record['oracle_text']
            or json.dumps(declaration.get('params'), sort_keys=True) != json.dumps(params, sort_keys=True)):
        raise ValueError('typed spore synergy declaration must equal the local Oracle pin')
    check_variants(params, declaration.get('variants'))
    return dict(params, family=FAMILY)


def validate_witnesses(oracle, rows):
    data = profiles()
    skipped = {r['csvid'] for r in json.loads((PROFILE.parent / 'unimplemented.json').read_text(
        encoding='utf8'))['cases'].values()}
    if data.get('version') != 1 or not data.get('sources') or set(data.get('witnesses', ())) != WITNESSES:
        raise ValueError('exact reviewed spore synergy profile required')
    for group in ('sources', 'witnesses'):
        for name, pin in data[group].items():
            row, record = rows.get(name, {}), oracle.get(name, {})
            if (pin != dict(oracle=record, registry=row) or row.get('kind') != 'card' or row['csvid'] in skipped
                    or record.get('layout') != ('token' if name == TOKEN else 'normal')
                    or (row['handler'] in (None, 'NULL', '0', 'card_dummy') and name not in (BEAR, GOBLIN))):
                raise ValueError(f'exact implemented spore synergy fixture required: {name}')
            if group == 'sources' and row['handler'] != SHAPES[classify(record)['shape']][0]:
                raise ValueError(f'changed spore synergy handler: {name}')
    token = oracle[TOKEN]
    if (rows[TOKEN]['csvid'], token['type_line'], token['power'], token['toughness'], token['colors']) != (
            18813, 'Token Creature — Saproling', '1', '1', ['G']):
        raise ValueError('exact green Saproling token required')
    if oracle[MAKER]['oracle_text'] != UPKEEP + '\n' + MAKE or oracle[JUDGE]['oracle_text'] != SHAPES['exile_team'][1]:
        raise ValueError('changed threshold producer or Thelon observation effect')


class SynergyWorld(SporeWorld):
    def __init__(self, name, params, mode, owner, rows, oracle):
        super().__init__(name, params, mode, owner, rows, oracle)
        self.counters = {}
        self.walker = None
        self.temporary = False
        self.lord_in_play = False
        self.pump_targets = set()

    def add_fungus(self, name, ref, player):
        self.add(name, ref, player)
        self.counters[ref] = 0

    def tribal_board(self):
        self.add_fungus(FUNGUS, 'friend', self.owner)
        self.add_fungus(FUNGUS, 'enemy', self.foe)
        self.add(BEAR, 'ordinary', self.owner)

    def cast_permanent(self):
        self.pool(self.owner, self.p['mana'])
        self.emit('cast', ref='source', value=1)
        self.emit('assert_stack', value=1)
        self.emit('resolve')
        self.mana[self.owner] = [0] * 6
        if self.shape == 'placement':
            self.zones['hand'][self.owner].remove(self.name)
            self.zones['battlefield'][self.owner].append(self.name)
            self.observe('source', 'in_play', 1)
        else:
            self.arrive('source')
            if self.shape != 'exile_team':
                self.counters['source'] = 0
        self.lord_in_play = self.shape == 'tribal_lord'
        self.receipts()

    def ally_token_stats(self, player):
        boosted = player == self.owner and (self.lord_in_play or (self.shape == 'team_pump' and self.temporary))
        return 1 + int(boosted)

    def calculate_stats(self):
        judge = self.shape in ('exile_team', 'upkeep_team')
        for ref, entity in self.entities.items():
            if entity['zone'] != 'battlefield':
                continue
            printed = self.o[entity['name']]
            amount = self.counters.get(ref, 0) if judge else 0
            power = toughness = amount
            if self.lord_in_play and ref in self.counters and ref != 'source' and entity['player'] == self.owner:
                power += 1
                toughness += 1
            if self.temporary and ref in self.pump_targets:
                power += 1
                toughness += 2 if self.shape == 'self_pump' else 1
            entity.update(power=int(printed['power']) + power, toughness=int(printed['toughness']) + toughness)

    def receipts(self, token_identity=False):
        self.calculate_stats()
        _AttackResourceWorld.receipts(self)
        if self.walker is not None:
            self.emit('assert_counter', ref='walker', counter_kind='loyalty', value=self.walker)
        for player in (0, 1):
            count = self.zones['battlefield'][player].count(TOKEN)
            for zone, qty in (('battlefield', count), ('graveyard', 0), ('exile', 0)):
                self.zone_count(TOKEN, player, zone, qty)
            if count and token_identity:
                stats = self.ally_token_stats(player)
                for prop, val in (('token', 1), ('power', stats), ('toughness', stats), ('color', 8), ('subtype_Saproling', 1)):
                    self.emit('assert_matching_permanents', player=player, index=self.rows[TOKEN]['csvid'], property=prop, value=val)

    def upkeep_turn(self):
        self.emit('advance_turn')
        self.temporary = False
        self.expire()
        self.current = 1 - self.current
        self.emit('untap_step', player=self.current)
        for entity in self.entities.values():
            if entity['zone'] == 'battlefield' and entity['player'] == self.current:
                entity.update(tapped=False, sick=False)
        self.emit('upkeep_step', player=self.current)
        if 'maker' in self.counters and self.entities['maker']['player'] == self.current:
            self.counters['maker'] += 1
        if self.current == self.owner:
            if self.shape in SELF_UPKEEP:
                self.counters['source'] += 1
            if self.shape == 'upkeep_team':
                for ref in self.counters:
                    if self.entities[ref]['player'] == self.owner:
                        self.counters[ref] += 1
        self.emit('turn', player=self.current, value=0)
        self.emit('assert_stack', value=0)
        self.emit('assert_unused_choices', value=0)

    def available(self, ref, expected=None):
        controller = self.entities[ref]['player']
        extra = ref == 'source' and self.shape in MODES and TOKEN in self.zones['battlefield'][controller]
        can = self.counters[ref] >= 3 or extra if expected is None else expected
        self.emit('can_activate', ref=ref, value=int(can))

    def token_spend(self, ref, *, identity=False):
        self.available(ref)
        if ref == 'source' and self.shape in MODES:
            self.mode_intent(MODES[self.shape][0])
        self.emit('activate', ref=ref, value=1)
        if ref == 'source' and self.shape in MODES:
            self.emit('require_activation_mode', value=1)
        self.counters[ref] -= 3
        self.emit('assert_stack', value=1)
        self.emit('assert_unused_choices', value=0)
        self.zero_pool()
        self.observe(ref, 'tapped', 0)
        controller = self.entities[ref]['player']
        count = self.zones['battlefield'][controller].count(TOKEN)
        self.zone_count(TOKEN, controller, 'battlefield', count)
        self.available(ref)
        self.emit('resolve')
        self.zones['battlefield'][controller].append(TOKEN)
        self.available(ref)
        self.receipts(identity)

    def pay_saproling(self, color=None):
        if self.owner == 0:
            self.emit('bind_permanent_copy', player=0, ref='meal', value=self.rows[TOKEN]['csvid'], index=0)
        self.mode_intent(MODES[self.shape][1])
        if self.owner == 0:
            self.emit('choose_target', player=0, ref='meal')
        if color is not None:
            self.emit('choose_color', player=self.owner, value=color)
        self.emit('activate', ref='source', value=1)
        self.emit('require_activation_mode', value=1)
        self.emit('assert_choices_consumed')
        self.zones['battlefield'][self.owner].remove(TOKEN)
        self.zone_count(TOKEN, self.owner, 'battlefield', self.zones['battlefield'][self.owner].count(TOKEN))
        self.zone_count(TOKEN, self.owner, 'graveyard', 0)
        self.emit('assert_stack', value=int(color is None))
        self.available('source')
        if color is None:
            self.zero_pool()
            self.observe('source', 'power', self.entities['source']['power'])
            self.emit('resolve')
            self.temporary = True
        else:
            self.mana[self.owner][color] = 1
        self.receipts(token_identity=True)

    def set_grave_cost(self, paid, grave):
        self.pool(self.owner, [0, 1, 0, 1, 0, 0] if self.mode != 'wrong_mana' else [1, 0, 0, 1, 0, 0])
        self.emit('can_activate', ref='source', value=int(paid))
        if paid:
            self.emit('choose_zone_card', player=self.owner, value=self.rows[MAKER]['csvid'])
        self.emit('activate', ref='source', value=int(paid))
        self.emit('assert_unused_choices', value=0)
        self.emit('assert_stack', value=int(paid))
        if not paid:
            self.receipts()
            return
        self.zones['graveyard'][grave].remove(MAKER)
        self.zones['exile'][grave].append(MAKER)
        self.mana[self.owner] = [0] * 6
        self.zero_pool()
        self.zone_count(MAKER, grave, 'graveyard', 0)
        self.zone_count(MAKER, grave, 'exile', 1)
        self.zone_count(MAKER, 1 - grave, 'exile', 0)
        for ref in self.counters:
            self.observe(ref, 'power', self.entities[ref]['power'])
        self.emit('resolve')
        for ref in self.counters:
            self.counters[ref] += 1
        self.receipts()


def start(world):
    world.pool(world.foe, [0] * 6)
    world.emit('turn', player=world.owner, value=0)


def placement(world):
    mode = world.mode
    w = world
    if mode != 'no_fungus':
        controller = w.foe if mode == 'enemy_fungus' else w.owner
        w.add_fungus(MAKER, 'maker', controller)
    w.add(BEAR, 'ordinary', w.owner)
    w.cast_permanent()
    valid = mode in ('own_fungus', 'enemy_fungus')
    if valid:
        for _ in range(4):
            w.upkeep_turn()
            w.available('maker')
    w.pool(w.owner, [1, 0, 0, 1, 0, 0] if mode == 'short_green' else [0, 0, 0, 2, 0, 0])
    w.emit('can_activate', ref='source', value=int(valid))
    if valid:
        w.emit('choose_target', player=w.owner, ref='maker')
    w.emit('activate', ref='source', value=int(valid))
    w.emit('assert_unused_choices', value=0)
    w.emit('assert_stack', value=int(valid))
    if valid:
        w.mana[w.owner] = [0] * 6
        w.zero_pool()
        w.available('maker')
        w.emit('resolve')
        w.counters['maker'] += 1
        w.token_spend('maker')
    else:
        w.receipts()


def upkeep_team(w):
    w.tribal_board()
    w.add(JUDGE, 'judge', w.foe)
    w.cast_permanent()
    w.available('source')
    for _ in range(5):
        w.upkeep_turn()
        w.available('source')
        w.receipts()
    cut = len(w.steps) if w.mode == 'team_replay' else None
    w.upkeep_turn()
    w.receipts()
    w.token_spend('source', identity=True)
    return cut


def exile_team(w):
    w.tribal_board()
    grave = w.foe if w.mode in ('enemy_grave', 'exile_replay') else w.owner
    if w.mode != 'no_fungus_grave':
        w.add(MAKER, 'exiled_fungus', grave, 'graveyard')
    w.add(BEAR, 'grave_decoy', grave, 'graveyard')
    w.cast_permanent()
    cut = len(w.steps) if w.mode == 'exile_replay' else None
    w.set_grave_cost(w.mode not in ('no_fungus_grave', 'wrong_mana'), grave)
    return cut


def tribal_lord(w):
    w.tribal_board()
    controller = w.foe if w.mode == 'enemy_saproling' else w.owner
    w.add_fungus(MAKER, 'maker', controller)
    w.add(BOUNCE, 'bounce', w.foe, 'hand')
    # This fixture takes a real untap before the lord's first cast spends mana.
    w.pool(w.owner, [0] * 6)
    if controller == w.owner:
        w.own_upkeeps(3)
    else:
        for _ in range(5):
            w.upkeep_turn()
    w.token_spend('maker', identity=True)
    if w.current != w.owner:
        w.upkeep_turn()
    w.cast_permanent()
    w.receipts(token_identity=True)
    if w.mode == 'no_lord':
        w.spell(BOUNCE, 'bounce', w.foe, 'source')
        w.leave('source', 'hand')
        w.lord_in_play = False
        w.receipts(token_identity=True)


def pump_or_mana(w):
    if w.shape != 'mana':
        w.tribal_board()
    w.cast_permanent()
    w.available('source')
    for _ in range(3):
        w.own_upkeeps(1)
        w.available('source')
    w.token_spend('source', identity=True)
    if w.mode == 'threshold':
        return None
    if w.shape == 'team_pump':
        w.own_upkeeps(3)
        w.token_spend('source', identity=True)
    cut = len(w.steps) if w.mode.endswith('_replay') else None
    color = COLORS.get(w.mode, 3) if w.shape == 'mana' else None
    if color is None:
        w.pump_targets = {'source'} if w.shape == 'self_pump' else {'source', 'friend'}
    w.pay_saproling(color)
    if w.mode == 'pump_expiry':
        w.upkeep_turn()
        w.receipts(token_identity=True)
    return cut


def damage(w):
    if w.mode in ('creature', 'missing_target'):
        w.add(GOBLIN, 'target', w.foe)
    if w.mode == 'planeswalker':
        w.add(WALKER, 'walker', w.foe)
        w.walker = int(w.o[WALKER]['loyalty'])
    if w.mode == 'missing_target':
        w.add(BOUNCE, 'bounce', w.foe, 'hand')
    w.cast_permanent()
    w.available('source')
    for _ in range(3):
        w.own_upkeeps(1)
        w.available('source')
    cut = len(w.steps) if w.mode == 'damage_replay' else None
    if w.mode == 'planeswalker':
        w.emit('choose_target', player=w.owner, ref='walker')
    elif w.mode in ('creature', 'missing_target'):
        w.emit('choose_target', player=w.owner, ref='target')
    else:
        w.emit('choose_player', player=w.owner, value=w.foe)
    w.emit('activate', ref='source', value=1)
    w.counters['source'] -= 3
    w.emit('assert_unused_choices', value=0)
    w.emit('assert_stack', value=1)
    w.zero_pool()
    w.available('source')
    w.emit('assert_life', player=w.foe, value=20)
    if w.mode == 'missing_target':
        from recipes import cost_from_oracle

        w.pool(w.foe, cost_from_oracle(w.o[BOUNCE]))
        w.emit('choose_target', player=w.foe, ref='target')
        # The parent is an activation. The normal Instant cast can sit above it.
        w.emit('cast', ref='bounce', value=1)
        w.emit('assert_stack', value=2)
        w.emit('resolve')
        w.mana[w.foe] = [0] * 6
        w.zones['hand'][w.foe].remove(BOUNCE)
        w.zones['graveyard'][w.foe].append(BOUNCE)
        w.leave('target', 'hand')
    else:
        w.emit('resolve')
        if w.mode == 'creature':
            w.leave('target', 'graveyard')
        elif w.mode == 'planeswalker':
            w.walker -= 1
        else:
            w.life[w.foe] -= 1
    w.receipts()
    return cut


SCENARIOS = {'placement': placement, 'upkeep_team': upkeep_team, 'exile_team': exile_team,
             'tribal_lord': tribal_lord, 'self_pump': pump_or_mana, 'team_pump': pump_or_mana,
             'mana': pump_or_mana, 'damage': damage}


def expand(name, csvid, declaration, params, selected, rows, oracle):
    from recipes import step

    check_variants(params, selected)
    validate_witnesses(oracle, rows)
    if profiles()['sources'][name]['registry']['csvid'] != csvid:
        raise ValueError('exact spore synergy CSVID required')
    result = []
    for variant in selected:
        mode, owner = variant.rsplit('_', 1)
        world = SynergyWorld(name, params, mode, int(owner), rows, oracle)
        start(world)
        cut = SCENARIOS[params['shape']](world)
        steps = world.steps
        if cut is not None:
            tail = steps[cut:]
            steps = (steps[:cut] + [step('capture')] + tail
                     + [step('record_state'), step('ai_roundtrip'), step('restore')]
                     + copy.deepcopy(tail) + [step('assert_recorded_state')])
        result.append(dict(name=f'SporeSynergyRidersRecipe_{csvid}_{variant}', card=name, fixture='empty_main',
                           seed=2008, setup=world.setup, steps=steps,
                           _recipe=dict(family=FAMILY, version=2, variant=variant, params=declaration['params'],
                                        oracle_text=declaration['oracle_text'], untested_components=list(GAPS))))
    return result
