"""Fixed divided damage through the native cast and resolution selectors."""

import copy
import hashlib
import json
from pathlib import Path
import re

from block_stat_trigger_recipes import BlockStatBoard
from combat_value_trigger_recipes import _mirror
from grant_recipes import LIBRARY

FAMILY = 'divided_damage_spell'
PROFILE_PATH = Path(__file__).with_name('data') / 'divided_damage_spell_wave8_profiles.json'
WORDING = {'one or two targets': 2, 'one, two, or three targets': 3, 'any number of targets': None}
MODES = ('all_to_enemy', 'all_to_self', 'all_to_creature', 'own_creature', 'split_player_creature',
         'split_creature_player', 'max_distinct', 'lethal_split', 'walker_lethal', 'walker_split',
         'late_bounce_all', 'late_bounce_first', 'late_bounce_last', 'late_hexproof', 'own_hexproof',
         'countered', 'cancel_first', 'unfunded', 'wrong_color', 'single_packet_armor', 'indestructible', 'pending_replay')
DENIALS = ('cancel_first', 'unfunded', 'wrong_color')
BOUNCE = ('late_bounce_all', 'late_bounce_first', 'late_bounce_last')


def profiles():
    return json.loads(PROFILE_PATH.read_text(encoding='utf8'))


def parameters(record):
    if any(record.get('oracle_text', '').endswith(wording + '.') for wording in RESTRICTED):
        return restricted_parameters(record)
    name = record.get('name', '')
    pattern = re.escape(name) + r' deals ([2-5]) damage divided as you choose among (' + '|'.join(map(re.escape, WORDING)) + r')\.'
    match = re.fullmatch(pattern, record.get('oracle_text', ''))
    if (not match or record.get('layout') != 'normal' or record.get('card_faces') or record.get('selected_face')
            or record.get('type_line') not in ('Instant', 'Sorcery') or record.get('keywords') != []
            or not re.fullmatch(r'(?:\{[1-9]\})?(?:\{R\}){1,2}', record.get('mana_cost', ''))
            or record.get('colors') != ['R']):
        raise ValueError('complete fixed divided-damage clause and ordinary printed cost required')
    total, cap = int(match[1]), WORDING[match[2]]
    if cap is not None and total != cap:
        raise ValueError('the fixed target-count wording must match the reviewed damage total')
    return dict(damage=total, target_cap=total if cap is None else cap, printed_mana=record['mana_cost'],
                any_number=cap is None, instant=record['type_line'] == 'Instant')


def variants(params):
    if 'target_scope' in params:
        return restricted_variants(params)
    modes = MODES + (('mixed_three_types',) if params['target_cap'] >= 3 else ()) + (('offturn_instant',) if params['instant'] else ())
    return [f'{mode}_{player}' for player in (0, 1) for mode in modes]


def validate_variants(params, selected):
    if (not isinstance(selected, list) or not selected or any(type(v) is not str for v in selected)
            or len(set(selected)) != len(selected) or not set(selected) <= set(variants(params))):
        raise ValueError('distinct supported divided-damage variants required')


def spec(declaration, record):
    params = parameters(record)
    pin = profiles()['sources'].get(record['name'])
    if (declaration.get('family') != FAMILY or type(declaration.get('version')) is not int or declaration['version'] != 2
            or not pin or pin['oracle'] != record or pin['params'] != params or declaration.get('oracle_text') != record['oracle_text']
            or json.dumps(declaration.get('params'), sort_keys=True) != json.dumps(params, sort_keys=True)):
        raise ValueError('complete pinned Oracle and typed divided-damage parameters required')
    validate_variants(params, declaration.get('variants'))
    return dict(params, family=FAMILY)


def validate_witnesses(oracle, rows):
    data = profiles()
    root = Path(__file__).resolve().parents[4]
    unimplemented = json.loads((PROFILE_PATH.parent / 'unimplemented.json').read_text(encoding='utf8'))['cases']
    unimplemented_ids = {item['csvid'] for item in unimplemented.values()}
    static_only = set(data['native_characteristics_only_witnesses'])
    for name, pin in (data['witnesses'] | data['sources']).items():
        row = rows.get(name, {})
        if oracle.get(name) != pin['oracle'] or row != pin['registry'] or row.get('kind') != 'card':
            raise ValueError('divided-damage source or witness changed: ' + name)
        if name not in static_only and (row['csvid'] in unimplemented_ids or 'dummy' in row['handler']):
            raise ValueError('divided-damage action witness must be implemented: ' + name)
    for name, pin in data['sources'].items():
        source = pin['compiled_source']
        if hashlib.sha256((root / source['path']).read_bytes()).hexdigest() != source['sha256']:
            raise ValueError('divided-damage compiled source changed: ' + name)


def allocation(mode, total):
    if mode in DENIALS:
        return ()
    if mode in ('all_to_creature', 'late_bounce_all', 'indestructible'):
        return (('enemy', total),)
    if mode in ('own_creature', 'own_hexproof'):
        return (('own', total),)
    if mode == 'all_to_self':
        return (('self_player', total),)
    if mode == 'walker_lethal':
        return (('walker', total),)
    if mode == 'walker_split':
        return (('walker', 1), ('enemy_player', total - 1))
    if mode in ('split_player_creature', 'late_bounce_last'):
        return (('enemy_player', 1), ('enemy', total - 1))
    if mode in ('split_creature_player', 'late_bounce_first', 'late_hexproof', 'pending_replay', 'offturn_instant'):
        return (('enemy', total - 1), ('enemy_player', 1))
    if mode == 'lethal_split':
        return (('enemy', 1), ('enemy_player', total - 1))
    if mode == 'max_distinct':
        return tuple((ref, 1) for ref in ('enemy_player', 'self_player', 'enemy', 'walker', 'own')[:total])
    if mode == 'mixed_three_types':
        return (('enemy', 1), ('walker', 1), ('enemy_player', total - 2))
    return (('enemy_player', total),)


class DividedDamageBoard(BlockStatBoard):
    def __init__(self, name, csvid, declaration, params, variant, rows, oracle):
        from recipes import card, cost_from_oracle, observe, step

        self.card, self.step, self.observe, self.cost = card, step, observe, cost_from_oracle
        self.name, self.csvid, self.d, self.p, self.rows, self.oracle = name, csvid, declaration, params, rows, oracle
        mode, owner = variant.rsplit('_', 1)
        self.mode, self.owner = mode, int(owner)
        self.steps, self.setup, self.entities, self.base = [], [], {}, {}
        self.tokens, self.bound = [], set()
        self.boundary = self.discard_nondeterministic = False
        self.life = [20, 20]
        self.pools = [[0] * 6, [0] * 6]
        self.mana = [0, 0]
        self.zones = []
        self.distribution = allocation(mode, params['damage'])
        for player in (0, 1):
            zones = dict(hand=['Forest', 'Plains'], graveyard=['Island'], library=list(LIBRARY), exile=['Mountain'])
            self.zones.append(zones)
            for zone, names in zones.items():
                self.setup.extend(card(stock, f'stock_{zone}_{player}_{index}', player, zone) for index, stock in enumerate(names))

        self.add(name, 'source', 0, 'hand')
        if any(ref == 'enemy' for ref, _ in self.distribution):
            self.add({'lethal_split': 'Merfolk Looter', 'indestructible': 'Darksteel Myr'}.get(mode, 'Crater Elemental'), 'enemy', 1)
        if any(ref == 'own' for ref, _ in self.distribution):
            self.add('Bloodfire Mentor', 'own')
        self.loyalty = None
        if any(ref == 'walker' for ref, _ in self.distribution):
            walker = 'Tibalt, the Fiend-Blooded' if mode == 'walker_lethal' else 'Jace Beleren'
            self.add(walker, 'walker', 1)
            self.loyalty = int(oracle[walker]['loyalty'])
        if mode == 'single_packet_armor':
            self.add("Urza's Armor", 'armor', 1)
        self.response = {'late_hexproof': ("Ranger's Guile", 1, 'enemy'), 'own_hexproof': ("Ranger's Guile", 0, 'own'),
                         'countered': ('Counterspell', 1, None)}.get(mode)
        if mode in BOUNCE:
            self.response = ('Unsummon', 1, 'enemy')

        price = self.cost(oracle[name])
        self.pools[0] = [0] * 6 if mode == 'unfunded' else [sum(price), 0, 0, 0, 0, 0] if mode == 'wrong_color' else price
        if self.response:
            witness, player, _ = self.response
            self.add(witness, 'response', player, 'hand')
            self.pools[player] = [amount + fee for amount, fee in zip(self.pools[player], self.cost(oracle[witness]))]
        self.emit('turn', player=int(mode == 'offturn_instant'), value=0)
        self.emit('present_ai_dialogs', value=1)
        for player in (0, 1):
            self.set_pool(player, self.pools[player])
        self.emit('refresh')

    def choices(self):
        if self.mode == 'cancel_first':
            self.emit('choose_cancel_target', player=0)
            return
        if self.mode in DENIALS:
            return
        refs = [ref for ref, points in self.distribution]
        if (not 1 <= len(refs) <= self.p['target_cap'] or len(set(refs)) != len(refs)
                or any(type(points) is not int or points < 1 for _, points in self.distribution)
                or sum(points for _, points in self.distribution) != self.p['damage']):
            raise ValueError('every legal target receives positive damage and all points must be assigned')
        # These callbacks implement the real allocation UI as one selector
        # per point. Consecutive repeated selections encode one larger share.
        for ref, points in self.distribution:
            for _ in range(points):
                if ref in ('enemy_player', 'self_player'):
                    self.emit('choose_player', player=0, value=int(ref == 'enemy_player'))
                else:
                    self.emit('choose_target', player=0, ref=ref)

    def observations_before_damage(self):
        for player in (0, 1):
            self.emit('assert_life', player=player, value=20)
        for ref in ('enemy', 'own'):
            if ref in self.entities:
                self.emit('assert_card', ref=ref, property='damage', value=0)
        if self.loyalty is not None:
            self.emit('assert_counter', ref='walker', counter_kind='loyalty', value=self.loyalty)

    def source_pending(self):
        allowed = self.mode not in DENIALS
        self.choices()
        self.emit('cast', ref='source', value=int(allowed))
        if allowed:
            self.spend(self.p['printed_mana'])
        self.emit('assert_stack', value=int(allowed))
        self.emit('assert_card', ref='source', property='in_hand', value=int(not allowed))
        self.emit('assert_unused_choices', value=0)
        self.emit('assert_choices_consumed')
        for color, value in enumerate(self.pools[0]):
            self.emit('assert_mana', player=0, color=color, value=value)
        self.observations_before_damage()
        return allowed

    def resolve(self):
        if self.response:
            name, player, target = self.response
            self.emit('announce_spell', ref='source')
            if target is not None:
                self.emit('choose_target', player=player, ref=target)
            self.emit('cast_response', ref='response', value=1)
            self.spend(self.oracle[name]['mana_cost'], player)
            self.emit('assert_stack', value=2)
            self.emit('resolve_response')
            self.move('response', 'graveyard')
            if self.mode in BOUNCE:
                self.move('enemy', 'hand')
                self.emit('bind_hand', player=1, value=self.rows[self.entities['enemy']['name']]['csvid'], ref='enemy')
                self.emit('assert_card', ref='enemy', property='in_hand', value=1)
            elif self.mode in ('late_hexproof', 'own_hexproof'):
                self.entities[target]['boost'] += 1
        else:
            self.emit('resolve')
        self.emit('refresh')
        self.move('source', 'graveyard')
        if self.mode != 'countered':
            self.allocate_damage()

    def allocate_damage(self):
        invalid = {'enemy'} if self.mode in BOUNCE or self.mode == 'late_hexproof' else set()
        for ref, points in self.distribution:
            if ref in invalid:
                continue
            if ref in ('enemy_player', 'self_player'):
                prevention = int(ref == 'enemy_player' and self.mode == 'single_packet_armor')
                self.life[int(ref == 'enemy_player')] -= points - prevention
            elif ref == 'walker':
                self.loyalty = max(0, self.loyalty - points)
                if self.loyalty == 0:
                    self.move(ref, 'graveyard')
            else:
                item = self.entities[ref]
                item['damage'] += points
                if item['damage'] >= item['toughness'] + item['boost'] and 'Indestructible' not in self.oracle[item['name']]['keywords']:
                    self.move(ref, 'graveyard')

    def run(self):
        cut = None
        if self.source_pending():
            if self.mode == 'pending_replay':
                cut = len(self.steps)
            self.resolve()
        self.receipts()
        if self.loyalty:
            self.emit('assert_counter', ref='walker', counter_kind='loyalty', value=self.loyalty)
        if cut is not None:
            replay = self.steps[cut:]
            self.steps[cut:cut] = [self.step('capture')]
            self.steps += [self.step('record_state'), self.step('ai_roundtrip'), self.step('restore')]
            self.steps += copy.deepcopy(replay) + [self.step('assert_recorded_state')]
        variant = f'{self.mode}_{self.owner}'
        case = dict(name=f'DividedDamageSpellRecipe_{self.csvid}_{variant}', card=self.name,
                    fixture='empty_main', seed=123, setup=self.setup, steps=self.steps,
                    _recipe=dict(family=FAMILY, version=2, variant=variant, params=copy.deepcopy(self.d['params']),
                                 oracle_text=self.d['oracle_text'], untested_components=[
                                     'Possible illegal cast-time recipients are left outside the scripted selection interface. Resolution revalidation uses actual response spells.',
                                 ]))
        if self.owner:
            _mirror(case)
        return case


def expand(name, csvid, declaration, params, selected, rows, oracle):
    validate_witnesses(oracle, rows)
    validate_variants(params, selected)
    board = RestrictedDamageBoard if 'target_scope' in params else DividedDamageBoard
    return [board(name, csvid, declaration, params, variant, rows, oracle).run() for variant in selected]


RESTRICTED = {'one, two, or three target creatures with flying': ('flying', 3),
              'one, two, or three target attacking or blocking creatures': ('combat', 3),
              'any number of target attacking or blocking creatures': ('combat', None),
              'any number of target creatures': ('creature', None)}
RESTRICTED_COMMON = ('all_to_creature', 'split', 'max_distinct', 'late_bounce', 'late_hexproof', 'countered',
                     'pending_replay', 'cancel_first', 'unfunded', 'wrong_color', 'none_eligible', 'wrong_types_only')
RESTRICTED_EXTRA = {'combat': ('attacker_only', 'blocker_only', 'combat_idle_negative', 'late_leave_combat'),
                    'flying': ('ground_no_fly', 'flight_grant', 'late_flying_loss'), 'creature': ('own_only',)}
RESTRICTED_NEGATIVES = ('none_eligible', 'wrong_types_only', 'combat_idle_negative', 'ground_no_fly')
HYBRID_MODES = ('hybrid_white', 'hybrid_red_mixed', 'hybrid_white_mixed')


def restricted_parameters(record):
    pattern = re.escape(record['name']) + r' deals ([3-6]) damage divided as you choose among (' + '|'.join(map(re.escape, RESTRICTED)) + r')\.'
    match = re.fullmatch(pattern, record.get('oracle_text', ''))
    fee, colors = record.get('mana_cost', ''), record.get('colors')
    hybrid = fee == '{R/W}{R/W}{R/W}'
    fixed = re.fullmatch(r'(?:\{[1-9]\})?(?P<pip>\{[RWG]\})(?P=pip)?', fee)
    if (not match or record.get('layout') != 'normal' or record.get('card_faces') or record.get('selected_face')
            or record.get('type_line') != 'Instant' or record.get('keywords') != []
            or not (hybrid and colors == ['R', 'W'] or fixed and colors == [fixed['pip'][1]])):
        raise ValueError('complete restricted fixed damage and printed cost required')
    scope, cap = RESTRICTED[match[2]]
    total = int(match[1])
    if cap is not None and total != cap:
        raise ValueError('printed fixed target count disagrees with total damage')
    return dict(damage=total, target_cap=total if cap is None else cap, printed_mana=fee, any_number=cap is None,
                instant=True, target_scope=scope, hybrid=hybrid)


def restricted_variants(params):
    modes = RESTRICTED_COMMON + RESTRICTED_EXTRA[params['target_scope']]
    if params['hybrid']:
        modes += HYBRID_MODES
    return [f'{mode}_{player}' for player in (0, 1) for mode in modes]


def restricted_distribution(mode, total):
    if mode in DENIALS + RESTRICTED_NEGATIVES:
        return ()
    if mode == 'max_distinct':
        return tuple((f'target_{index}', 1) for index in range(total))
    if mode in ('split', 'late_bounce', 'late_hexproof', 'pending_replay', 'late_leave_combat', 'late_flying_loss'):
        return (('target_0', total - 1), ('target_1', 1))
    return (('target_0', total),)


class RestrictedDamageBoard(DividedDamageBoard):
    def __init__(self, name, csvid, declaration, params, variant, rows, oracle):
        from recipes import card, cost_from_oracle, observe, step

        self.card, self.step, self.observe, self.cost = card, step, observe, cost_from_oracle
        self.name, self.csvid, self.d, self.p, self.rows, self.oracle = name, csvid, declaration, params, rows, oracle
        self.mode, owner = variant.rsplit('_', 1)
        self.owner = int(owner)
        self.steps, self.setup, self.entities, self.base = [], [], {}, {}
        self.tokens, self.bound = [], set()
        self.boundary = self.discard_nondeterministic = False
        self.life, self.pools, self.mana = [20, 20], [[0] * 6, [0] * 6], [0, 0]
        self.zones = []
        self.attackers, self.blockers = [], []
        self.distribution = restricted_distribution(self.mode, params['damage'])
        self.invalid = set()
        self.scope = params['target_scope']
        self.in_combat = self.scope == 'combat' and self.mode not in RESTRICTED_NEGATIVES
        for player in (0, 1):
            zones = dict(hand=['Forest', 'Plains'], graveyard=['Island'], library=list(LIBRARY), exile=['Mountain'])
            self.zones.append(zones)
            for zone, names in zones.items():
                self.setup.extend(card(stock, f'stock_{zone}_{player}_{index}', player, zone) for index, stock in enumerate(names))

        self.add(name, 'source', 0, 'hand')
        self.setup_victims()
        if params['hybrid']:
            red = {'hybrid_white': 0, 'hybrid_red_mixed': 2, 'hybrid_white_mixed': 1}.get(self.mode, 3)
            self.fee = [0, 0, 0, 0, red, 3 - red]
        else:
            self.fee = self.cost(oracle[name])
        self.pools[0] = [0] * 6 if self.mode == 'unfunded' else [sum(self.fee), 0, 0, 0, 0, 0] if self.mode == 'wrong_color' else self.fee.copy()
        self.helpers = {'late_bounce': ('Unsummon', 'target_0'), 'late_hexproof': ("Ranger's Guile", 'target_0'),
                        'countered': ('Counterspell', None), 'late_leave_combat': ('Hollowhenge Spirit', 'target_0'),
                        'late_flying_loss': ('Naturalize', 'flight')}
        if self.mode in self.helpers:
            helper, _ = self.helpers[self.mode]
            self.add(helper, 'reply', 1, 'hand')
            self.pools[1] = self.cost(oracle[helper])
        if self.mode in ('flight_grant', 'late_flying_loss'):
            self.add('Flight', 'flight', 0, 'hand')
            self.pools[0] = [amount + fee for amount, fee in zip(self.pools[0], self.cost(oracle['Flight']))]
        self.emit('turn', player=int(self.in_combat), value=0)
        self.emit('present_ai_dialogs', value=1)
        for player in (0, 1):
            self.set_pool(player, self.pools[player])
        self.emit('refresh')

    def setup_victims(self):
        if self.mode in ('wrong_types_only', 'combat_idle_negative'):
            self.add('Jace Beleren' if self.mode == 'wrong_types_only' else 'Crater Elemental', 'excluded', 1)
        elif self.mode == 'ground_no_fly':
            self.add('Thornweald Archer', 'excluded', 1)
        if not self.distribution and self.in_combat:
            # A combat-only affordability or cancellation fixture still needs
            # a real eligible combatant before attempting the paid action.
            self.add('Crater Elemental', 'opener', 1)
            self.attackers.append('opener')
        elif self.mode in DENIALS:
            self.add('Shivan Dragon' if self.scope == 'flying' else 'Crater Elemental', 'opener', 1)
        for index, (ref, _) in enumerate(self.distribution):
            player = 1 if self.mode in ('late_bounce', 'late_hexproof', 'late_leave_combat') else 1 - index % 2
            if self.mode in ('blocker_only', 'own_only'):
                player = 0
            subject = 'Shivan Dragon' if self.scope == 'flying' else 'Crater Elemental'
            if index == 0 and self.mode in ('flight_grant', 'late_flying_loss'):
                subject = 'Crater Elemental'
            self.add(subject, ref, player)
            if self.in_combat:
                (self.attackers if player else self.blockers).append(ref)
        if self.blockers and not self.attackers:
            self.add('Crater Elemental', 'opener', 1)
            self.attackers.append('opener')

    def observations_before_damage(self):
        for player in (0, 1):
            self.emit('assert_life', player=player, value=20)
        for ref, _ in self.distribution:
            self.emit('assert_card', ref=ref, property='damage', value=0)

    def spend(self, text, player=0):
        if not self.p['hybrid'] or text != self.p['printed_mana']:
            return super().spend(text, player)
        self.pools[player] = [amount - fee for amount, fee in zip(self.pools[player], self.fee)]
        if min(self.pools[player]) < 0:
            raise ValueError('hybrid fixture must supply exactly one legal payment')
        self.mana[player] = sum(self.pools[player])

    def begin_combat(self):
        self.emit('begin_attackers')
        for ref in self.attackers:
            self.emit('attack', ref=ref, value=1)
            self.entities[ref]['tapped'] = True
        self.emit('finish_attackers')
        self.emit('begin_blockers')
        for ref, target in zip(self.blockers, self.attackers):
            self.emit('block', ref=ref, other=target, value=1)
        self.emit('advance_combat')
        for ref in self.attackers:
            self.emit('assert_card', ref=ref, property='attacking', value=1)
        for ref in self.blockers:
            self.emit('assert_card', ref=ref, property='blocking_count', value=1)

    def resolve(self):
        mode = self.mode
        if mode in self.helpers:
            name, target = self.helpers[mode]
            self.emit('announce_spell', ref='source')
            if target and mode != 'late_leave_combat':
                self.emit('choose_target', player=1, ref=target)
            self.emit('cast_response', ref='reply', value=1)
            self.spend(self.oracle[name]['mana_cost'], player=1)
            self.emit('assert_stack', value=2)
            if mode == 'late_leave_combat':
                self.emit('choose_target', player=1, ref=target)
            self.emit('resolve_response')
            self.move('reply', 'battlefield' if mode == 'late_leave_combat' else 'graveyard')
            if mode == 'late_bounce':
                self.move(target, 'hand')
                self.emit('bind_hand', player=1, ref=target, value=self.rows[self.entities[target]['name']]['csvid'])
            elif mode == 'late_hexproof':
                self.entities[target]['boost'] += 1
            elif mode == 'late_leave_combat':
                self.emit('assert_card', ref=target, property='attacking', value=0)
            elif mode == 'late_flying_loss':
                self.move('flight', 'graveyard')
            if mode in ('late_bounce', 'late_hexproof', 'late_leave_combat', 'late_flying_loss'):
                self.invalid.add('target_0')
        else:
            self.emit('resolve')
        self.emit('refresh')
        self.move('source', 'graveyard')
        if mode != 'countered':
            for ref, points in self.distribution:
                if ref in self.invalid:
                    continue
                entity = self.entities[ref]
                entity['damage'] += points
                if entity['damage'] >= entity['toughness'] + entity['boost']:
                    self.move(ref, 'graveyard')

    def finish_combat(self):
        self.emit('combat_damage')
        self.emit('resolve')
        self.emit('end_combat')
        if self.mode == 'late_hexproof':
            self.life[0] -= self.entities['target_0']['boost']

    def run(self):
        if self.mode in ('flight_grant', 'late_flying_loss'):
            if self.mode == 'flight_grant':
                self.emit('can_cast', ref='source', value=0)
            self.paid('flight', target='target_0')
            self.emit('refresh')
        if self.in_combat:
            self.begin_combat()
        legal = self.mode not in RESTRICTED_NEGATIVES
        self.emit('can_cast', ref='source', value=int(legal))
        if not legal:
            self.emit('cast', ref='source', value=0)
            self.emit('assert_card', ref='source', property='in_hand', value=1)
        cut = None
        if legal and self.source_pending():
            if self.mode == 'pending_replay':
                cut = len(self.steps)
            self.resolve()
        if self.in_combat:
            self.finish_combat()
        self.receipts()
        if cut is not None:
            tail = self.steps[cut:]
            self.steps[cut:cut] = [self.step('capture')]
            self.steps += [self.step('record_state'), self.step('ai_roundtrip'), self.step('restore')]
            self.steps += copy.deepcopy(tail) + [self.step('assert_recorded_state')]
        variant = f'{self.mode}_{self.owner}'
        case = dict(name=f'DividedDamageSpellRecipe_{self.csvid}_{variant}', card=self.name, fixture='empty_main', seed=123,
                    setup=self.setup, steps=self.steps,
                    _recipe=dict(family=FAMILY, version=2, variant=variant, params=copy.deepcopy(self.d['params']),
                                 oracle_text=self.d['oracle_text'], untested_components=[
                                     'Casting legality negatives use only ineligible targets and never submit a forced illegal choice.',
                                 ]))
        if self.owner:
            _mirror(case)
        return case
