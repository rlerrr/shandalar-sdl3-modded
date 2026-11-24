"""Land-type affinity measured through real cast payments and combat."""

import copy
import hashlib
import json
from pathlib import Path
import re

from block_stat_trigger_recipes import BlockStatBoard
from combat_value_trigger_recipes import _mirror

FAMILY = 'land_type_affinity'
PROFILE_PATH = Path(__file__).with_name('data') / 'land_type_affinity_wave8_profiles.json'
NONBASIC = {'Swamp': 'Leechridden Swamp', 'Mountain': 'Madblind Mountain', 'Plains': 'Mistveil Plains',
            'Island': 'Moonring Island', 'Forest': 'Sapseep Forest'}
KEYWORDS = {'': '', 'Fear (This creature can\'t be blocked except by artifact creatures and/or black creatures.)': 'Fear',
            'Haste': 'Haste', 'Vigilance': 'Vigilance', 'Flying': 'Flying'}
MODES = ('full_price', 'short_full', 'own_basics', 'short_discount', 'foe_only', 'unrelated', 'mixed',
         'typed_nonbasic', 'hand_grave_library_only', 'zero_exact', 'floor_surplus', 'land_play', 'remove_land',
         'type_grant', 'repeat_spell', 'pending_replay', 'fresh_attack')
EXTRA = {'': (), 'Fear': ('fear_ground', 'fear_black', 'fear_artifact'), 'Haste': (),
         'Vigilance': ('vigilance',), 'Flying': ('flying_ground', 'flying_reach')}
BLOCKERS = {'fear_ground': 'Hill Giant', 'fear_black': 'Walking Corpse', 'fear_artifact': 'Steel Wall',
            'flying_ground': 'Hill Giant', 'flying_reach': 'Giant Spider'}
SHORT = {'short_full', 'short_discount'}
LAND_LEDGER = ('Forest', 'Island', 'Plains', 'Mountain', 'Swamp')


def profile():
    return json.loads(PROFILE_PATH.read_text(encoding='utf8'))


def land_types(record):
    category, _, subtypes = record['type_line'].partition(' — ')
    if 'Land' not in category.split():
        return frozenset()
    return frozenset(subtypes.split())


def parameters(record):
    if record.get('oracle_text') == GATE_CLAUSE:
        return gate_parameters(record)
    clause, _, secondary = record.get('oracle_text', '').partition('\n')
    pattern = r'Affinity for (Swamp|Mountain|Plains|Island|Forest)s? \(This spell costs \{1\} less to cast for each \1 you control\.\)'
    match = re.fullmatch(pattern, clause)
    mana = re.fullmatch(r'\{([5-7])\}', record.get('mana_cost', ''))
    if (not match or not mana or record.get('type_line') != 'Artifact Creature — Golem' or record.get('layout') != 'normal'
            or record.get('card_faces') or record.get('selected_face') or record.get('colors') != [] or secondary not in KEYWORDS):
        raise ValueError('whole printed Darksteel land-affinity creature required')
    subtype = match[1]
    plural = subtype if subtype == 'Plains' else subtype + 's'
    if not clause.startswith(f'Affinity for {plural} '):
        raise ValueError('printed affinity land-type plural must match the actual reduction')
    keyword = KEYWORDS[secondary]
    if set(record.get('keywords', [])) != ({'Affinity', keyword} if keyword else {'Affinity'}):
        raise ValueError('printed land-affinity keywords changed')
    return dict(mana=int(mana[1]), land_type=subtype, keyword=keyword,
                power=int(record['power']), toughness=int(record['toughness']))


def variants(params):
    if params.get('gate_colossus'):
        return [f'{mode}_{owner}' for owner in (0, 1) for mode in GATE_MODES if not (owner and mode == 'grave_decline')]
    return [f'{mode}_{owner}' for owner in (0, 1) for mode in MODES + EXTRA[params['keyword']]]


def spec(declaration, record):
    params = parameters(record)
    pinned = profile()['sources'].get(record['name'])
    if (declaration.get('family') != FAMILY or type(declaration.get('version')) is not int or declaration['version'] != 2
            or not pinned or pinned['oracle'] != record or pinned['params'] != params
            or declaration.get('oracle_text') != record['oracle_text'] or declaration.get('params') != params):
        raise ValueError('whole pinned Oracle and typed land affinity parameters required')
    selected = declaration.get('variants')
    if not isinstance(selected, list) or not selected or any(type(variant) is not str for variant in selected):
        raise ValueError('explicit land affinity variants required')
    if len(set(selected)) != len(selected) or not set(selected) <= set(variants(params)):
        raise ValueError('distinct reviewed land affinity variants required')
    return dict(params, family=FAMILY)


def validate_witnesses(oracle, rows):
    data = profile()
    pending = {item['csvid'] for item in json.loads((PROFILE_PATH.parent / 'unimplemented.json').read_text(encoding='utf8'))['cases'].values()}
    static = set(data['static_characteristics_only_witnesses'])
    for name, pin in (data['witnesses'] | data['sources']).items():
        row = rows.get(name, {})
        if oracle.get(name) != pin['oracle'] or row != pin['registry'] or row.get('kind') != 'card':
            raise ValueError('land affinity source or witness changed: ' + name)
        if name not in static and (row['csvid'] in pending or 'dummy' in row['handler']):
            raise ValueError('land affinity action witness must be implemented: ' + name)
    for subtype, name in NONBASIC.items():
        land = oracle[name]
        if land_types(land) != {subtype} or land['type_line'].startswith('Basic ') or 'This land enters tapped.' not in land['oracle_text']:
            raise ValueError('pinned native typed nonbasic does not enter tapped: ' + name)
    root = Path(__file__).resolve().parents[4]
    for name, pin in data['sources'].items():
        source = pin['compiled_source']
        if hashlib.sha256((root / source['path']).read_bytes()).hexdigest() != source['sha256']:
            raise ValueError('land affinity compiled source changed: ' + name)


class LandAffinityBoard(BlockStatBoard):
    def __init__(self, name, csvid, declaration, params, variant, rows, oracle):
        from recipes import card, cost_from_oracle, observe, step

        self.card, self.step, self.observe, self.cost = card, step, observe, cost_from_oracle
        self.name, self.csvid, self.d, self.p, self.rows, self.oracle = name, csvid, declaration, params, rows, oracle
        self.mode, owner = variant.rsplit('_', 1)
        self.owner = int(owner)
        self.steps, self.setup, self.entities, self.base = [], [], {}, {}
        self.tokens, self.bound = [], set()
        self.boundary = self.discard_nondeterministic = False
        self.life, self.mana, self.pools = [20, 20], [0, 0], [[0] * 6, [0] * 6]
        self.zones = []
        self.type_granted = False
        self.subtype = params['land_type']
        self.stock()
        self.add(name, 'source', 0, 'hand')
        self.construct()

    def stock(self):
        for player in (0, 1):
            zones = dict(hand=['Forest', 'Plains'], library=list(LAND_LEDGER), graveyard=['Island'], exile=['Mountain'])
            if self.mode == 'hand_grave_library_only' and not player:
                for zone in zones.values():
                    zone.append(self.subtype)
            self.zones.append(zones)
            for zone, names in zones.items():
                self.setup.extend(self.card(name, f'stock_{zone}_{player}_{index}', player, zone) for index, name in enumerate(names))

    def lands(self, name, qty, player, prefix):
        for index in range(qty):
            self.add(name, f'{prefix}_{index}', player, state=['tapped'])

    def construct(self):
        mode, matching = self.mode, self.subtype
        typed = NONBASIC[matching]
        other = next(land for land in LAND_LEDGER if land != matching)
        if mode in ('own_basics', 'short_discount'):
            self.lands(matching, 2, 0, 'own')
            if mode == 'own_basics':
                self.setup[-1].pop('state')
                self.entities['own_1']['tapped'] = False
        if mode in ('foe_only', 'mixed'):
            self.lands(matching, 2, 1, 'foe_land')
        if mode in ('unrelated', 'mixed', 'type_grant'):
            self.lands(other, 1, 0, 'unrelated')
            self.lands('Darksteel Citadel', 1, 0, 'artifact_land')
        if mode in ('typed_nonbasic', 'mixed'):
            self.lands(typed, 1, 0, 'typed')
        if mode in ('mixed', 'remove_land', 'repeat_spell', 'pending_replay'):
            self.lands(matching, 1, 0, 'own')
        if mode in ('zero_exact', 'floor_surplus'):
            self.lands(matching, self.p['mana'] + (2 if mode == 'floor_surplus' else 0), 0, 'own')
        if mode == 'land_play':
            self.add(typed, 'entering', 0, 'hand')
        if mode == 'remove_land':
            self.add('Stone Rain', 'removal', 1, 'hand')
        if mode == 'type_grant':
            self.add('Prismatic Omen', 'omen', 0, 'hand')
        if mode == 'repeat_spell':
            self.add(self.name, 'second', 0, 'hand')
        if mode in BLOCKERS:
            self.add(BLOCKERS[mode], 'blocker', 1)
        if mode == 'vigilance':
            self.add('Hill Giant', 'other_attacker', 1)

    def price(self):
        active = [self.oracle[entity['name']] for entity in self.entities.values()
                  if entity['player'] == 0 and entity['zone'] == 'battlefield']
        count = sum(self.subtype in land_types(record)
                    or self.type_granted and 'Land' in record['type_line'].partition(' — ')[0].split() for record in active)
        return max(0, self.p['mana'] - count)

    def pools_receipt(self):
        for player, pool in enumerate(self.pools):
            for color, amount in enumerate(pool):
                self.emit('assert_mana', player=player, color=color, value=amount)

    def cast_payment(self, ref, allowed=True):
        due = self.price()
        self.emit('cast', ref=ref, value=int(allowed))
        self.emit('assert_unused_choices', value=0)
        self.emit('assert_card', ref=ref, property='in_hand', value=int(not allowed))
        self.emit('assert_stack', value=int(allowed))
        if allowed:
            self.pools[0][0] -= due
            if self.pools[0][0] < 0:
                raise ValueError('typed affinity fixture did not provide the real price')
            self.mana[0] = sum(self.pools[0])
        self.pools_receipt()

    def finish_cast(self, ref):
        self.emit('resolve')
        self.emit('refresh')
        self.move(ref, 'battlefield')
        self.emit('assert_stack', value=0)
        self.emit('assert_card', ref=ref, property='in_play', value=1)

    def next_turn(self):
        if any(self.mana):
            raise ValueError('turn advance fixture still has floating mana')
        self.emit('advance_turn')
        self.emit('refresh')

    def prelude(self):
        mode = self.mode
        self.emit('turn', player=int(mode == 'remove_land'), value=0)
        for player in (0, 1):
            self.set_pool(player, [0] * 6)
        self.emit('refresh')
        if mode == 'land_play':
            self.emit('play_land', ref='entering', value=1)
            self.emit('assert_stack', value=0)
            self.move('entering', 'battlefield')
            self.entities['entering']['tapped'] = True
            self.emit('assert_card', ref='entering', property='in_play', value=1)
            self.emit('assert_card', ref='entering', property='tapped', value=1)
        elif mode == 'remove_land':
            self.set_pool(1, self.cost(self.oracle['Stone Rain']))
            self.emit('refresh')
            self.paid('removal', target='own_0')
            self.move('own_0', 'graveyard')
            self.emit('assert_card', ref='own_0', property='in_play', value=0)
            self.next_turn()
        elif mode == 'type_grant':
            self.set_pool(0, self.cost(self.oracle['Prismatic Omen']))
            self.emit('refresh')
            self.paid('omen')
            self.emit('assert_card', ref='omen', property='in_play', value=1)
            self.type_granted = True

    def attack(self, attacker, player, allowed=True, blocker=None, legal=False):
        self.emit('begin_attackers')
        self.emit('attack', ref=attacker, value=int(allowed))
        entity = self.entities[attacker]
        if allowed:
            entity['tapped'] = not (attacker == 'source' and self.p['keyword'] == 'Vigilance')
        self.emit('assert_card', ref=attacker, property='tapped', value=int(entity['tapped']))
        self.emit('finish_attackers')
        if not allowed:
            return
        self.emit('begin_blockers')
        if blocker:
            self.emit('block', ref=blocker, other=attacker, value=int(legal))
        self.emit('advance_combat')
        self.emit('combat_damage')
        self.emit('resolve')
        self.emit('end_combat')
        if not legal:
            self.life[1-player] -= entity['power']
            return
        blocking = self.entities[blocker]
        entity['damage'] += blocking['power']
        blocking['damage'] += entity['power']
        for ref, card in ((attacker, entity), (blocker, blocking)):
            if card['damage'] >= card['toughness']:
                self.move(ref, 'graveyard')

    def printed_keyword(self):
        mode = self.mode
        if mode == 'fresh_attack':
            self.attack('source', 0, self.p['keyword'] == 'Haste')
        elif mode in BLOCKERS or mode == 'vigilance':
            self.next_turn()
            self.next_turn()
            if mode == 'vigilance':
                self.attack('source', 0)
                self.next_turn()
                self.attack('other_attacker', 1, blocker='source', legal=True)
            else:
                self.attack('source', 0, blocker='blocker', legal=mode not in ('fear_ground', 'flying_ground'))

    def run(self):
        self.prelude()
        price = self.price()
        repeat = self.mode == 'repeat_spell'
        allowed = self.mode not in SHORT
        pool = (2 if repeat else 1) * price + (2 if self.mode == 'floor_surplus' else 0) - int(not allowed)
        self.set_pool(0, [pool, 0, 0, 0, 0, 0])
        self.emit('refresh')
        self.cast_payment('source', allowed)
        cut = len(self.steps) if self.mode == 'pending_replay' else None
        if allowed:
            self.finish_cast('source')
            if repeat:
                self.cast_payment('second')
                self.finish_cast('second')
            self.printed_keyword()
        self.receipts()
        if cut is not None:
            tail = self.steps[cut:]
            self.steps[cut:cut] = [self.step('capture')]
            self.steps += [self.step('record_state'), self.step('ai_roundtrip'), self.step('restore')]
            self.steps += copy.deepcopy(tail) + [self.step('assert_recorded_state')]
        variant = f'{self.mode}_{self.owner}'
        result = dict(name=f'LandTypeAffinityRecipe_{self.csvid}_{variant}', card=self.name, fixture='empty_main', seed=123,
                      setup=self.setup, steps=self.steps, _recipe=dict(family=FAMILY, version=2, variant=variant,
                        params=copy.deepcopy(self.d['params']), oracle_text=self.d['oracle_text'], untested_components=[
                            'Simultaneous independent cost increases and multiplayer are outside the typed-affinity fixture.']))
        if self.owner:
            _mirror(result)
        return result


def expand(name, csvid, declaration, params, selected, rows, oracle):
    validate_witnesses(oracle, rows)
    if not selected or len(set(selected)) != len(selected) or not set(selected) <= set(variants(params)):
        raise ValueError('unique approved land affinity variants required')
    if rows[name]['csvid'] != csvid:
        raise ValueError('land affinity source identity changed')
    board = GateColossusBoard if params.get('gate_colossus') else LandAffinityBoard
    return [board(name, csvid, declaration, params, variant, rows, oracle).run() for variant in selected]


GATE_CLAUSE = ('Affinity for Gates (This spell costs {1} less to cast for each Gate you control.)\n'
               "This creature can't be blocked by creatures with power 2 or less.\n"
               'Whenever a Gate you control enters, you may put this card from your graveyard on top of your library.')
GATE_NAME = 'Azorius Guildgate'
GATE_MODES = ('full_price', 'short_full', 'own_gates', 'short_discount', 'foe_only', 'unrelated',
              'zero_exact', 'floor_surplus', 'gate_play', 'power_two', 'power_three', 'grave_accept',
              'grave_decline', 'grave_wrong_land', 'grave_foe_gate', 'grave_recast', 'grave_snapshot', 'pending_replay')


def gate_parameters(record):
    if (record.get('name') != 'Gate Colossus' or record.get('mana_cost') != '{8}'
            or record.get('layout') != 'normal' or record.get('selected_face') or record.get('card_faces')
            or record.get('type_line') != 'Artifact Creature — Construct' or record.get('colors') != []
            or record.get('keywords') != ['Affinity'] or (record.get('power'), record.get('toughness')) != ('8', '8')):
        raise ValueError('whole pinned Gate Colossus record required')
    return dict(mana=8, land_type='Gate', keyword='', power=8, toughness=8, gate_colossus=True)


class GateColossusBoard(LandAffinityBoard):
    def construct(self):
        mode = self.mode
        if mode in ('own_gates', 'short_discount'):
            self.lands(GATE_NAME, 2, 0, 'own')
        elif mode == 'foe_only':
            self.lands(GATE_NAME, 2, 1, 'foe')
        elif mode == 'unrelated':
            self.lands('Darksteel Citadel', 1, 0, 'artifact')
            self.lands('Forest', 1, 0, 'non_gate')
        elif mode in ('zero_exact', 'floor_surplus'):
            self.lands(GATE_NAME, 8 + 2 * (mode == 'floor_surplus'), 0, 'own')
        elif mode == 'pending_replay':
            self.lands(GATE_NAME, 1, 0, 'own')
        if mode == 'gate_play' or mode.startswith('grave_'):
            land_owner = int(mode == 'grave_foe_gate')
            land = 'Forest' if mode == 'grave_wrong_land' else GATE_NAME
            self.add(land, 'entry_land', land_owner, 'hand')
        if mode.startswith('grave_'):
            self.add('Shatter', 'shatter', 1, 'hand')
        if mode == 'grave_recast':
            self.add('Reach Through Mists', 'draw_spell', 0, 'hand')
        if mode in ('power_two', 'power_three'):
            self.add('Grizzly Bears' if mode == 'power_two' else 'Hill Giant', 'blocker', 1)

    def cast_at_current_price(self, permitted=True):
        due = self.price()
        reserve = 2 if self.mode == 'floor_surplus' else 0
        self.set_pool(0, [due + reserve - int(not permitted), 0, 0, 0, 0, 0])
        self.emit('refresh')
        self.cast_payment('source', permitted)

    def entry(self, trigger=False):
        land = self.entities['entry_land']
        if trigger and not self.owner:
            # The Rules Engine's duh_mode accepts automatically for the
            # actual AI seat. Only the human seat is offered a decline.
            self.emit('choose_dialog', player=0, value=int(self.mode == 'grave_decline'))
        self.emit('play_land', ref='entry_land', value=1)
        self.emit('resolve')
        self.emit('assert_unused_choices', value=0)
        self.emit('assert_stack', value=0)
        self.move('entry_land', 'battlefield')
        land['tapped'] = 'Gate' in land_types(self.oracle[land['name']])
        self.emit('assert_card', ref='entry_land', property='in_play', value=1)
        self.emit('assert_card', ref='entry_land', property='tapped', value=int(land['tapped']))
        self.emit('refresh')

    def grave_round(self):
        self.next_turn()
        self.set_pool(1, self.cost(self.oracle['Shatter']))
        self.emit('refresh')
        self.paid('shatter', target='source')
        self.move('source', 'graveyard')
        self.emit('assert_card', ref='source', property='in_play', value=0)
        self.emit('assert_zone_multiplicity', player=0, zone='graveyard', index=self.csvid, value=1)
        if self.mode != 'grave_foe_gate':
            self.next_turn()
        cut = len(self.steps) if self.mode == 'grave_snapshot' else None
        trigger = self.mode not in ('grave_wrong_land', 'grave_foe_gate')
        self.entry(trigger)
        returns = trigger and self.mode != 'grave_decline'
        if returns:
            self.move('source', 'library')
            library = self.zones[0]['library']
            library.insert(0, library.pop())
            self.emit('assert_zone_card', player=0, zone='library', index=0, value=self.csvid)
        self.emit('assert_zone_multiplicity', player=0, zone='graveyard', index=self.csvid, value=int(not returns))
        if self.mode == 'grave_recast':
            self.set_pool(0, self.cost(self.oracle['Reach Through Mists']))
            self.emit('refresh')
            self.paid('draw_spell')
            self.draw(0, 1)
            self.entities['source']['zone'] = 'hand'
            self.emit('bind_hand', player=0, ref='source', value=self.csvid)
            self.emit('assert_card', ref='source', property='in_hand', value=1)
            self.cast_at_current_price()
            self.finish_cast('source')
        return cut

    def run(self):
        self.emit('turn', player=0, value=0)
        for player in (0, 1):
            self.set_pool(player, [0]*6)
        self.emit('refresh')
        if self.mode == 'gate_play':
            self.entry()
        permitted = self.mode not in SHORT
        self.cast_at_current_price(permitted)
        cut = len(self.steps) if self.mode == 'pending_replay' else None
        if permitted:
            self.finish_cast('source')
        if self.mode in ('power_two', 'power_three'):
            self.next_turn()
            self.next_turn()
            self.attack('source', 0, blocker='blocker', legal=self.mode == 'power_three')
        if self.mode.startswith('grave_'):
            cut = self.grave_round()
        self.receipts()
        if cut is not None:
            tail = self.steps[cut:]
            self.steps[cut:cut] = [self.step('capture')]
            self.steps += [self.step('record_state'), self.step('ai_roundtrip'), self.step('restore')]
            self.steps += copy.deepcopy(tail) + [self.step('assert_recorded_state')]
        variant = f'{self.mode}_{self.owner}'
        case = dict(name=f'LandTypeAffinityRecipe_{self.csvid}_{variant}', card=self.name, fixture='empty_main', seed=123,
                    setup=self.setup, steps=self.steps, _recipe=dict(family=FAMILY, version=2, variant=variant,
                    params=copy.deepcopy(self.d['params']), oracle_text=self.d['oracle_text'],
                    untested_components=['The native AI auto-accepts the optional return in duh_mode. The human seat exercises both choices.']))
        if self.owner:
            _mirror(case)
        return case
