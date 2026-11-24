"""Untargeted Rebound resolution with actual upkeep and Oracle-derived value receipts."""

import copy
import json
from functools import lru_cache
from pathlib import Path

from targeted_rebound_recipes import BEAR, COUNTER, GIANT, LANDS, REBOUND, SQUIRE, ZERO, ReboundWorld

FAMILY = 'untargeted_rebound'
PROFILE = Path(__file__).with_name('data') / 'untargeted_rebound_wave8_profiles.json'
COAT, SPIDER = 'Coat of Arms', 'Giant Spider'
TOKEN_MODELS = {
    'Create a 1/1 white Kor Soldier creature token for each creature you control.': (
        'Kor Soldier', 1, 1, 32, ['Kor', 'Soldier'], False, 'creatures_you_control'),
    'Create a 2/2 blue Djinn Monk creature token with flying.': (
        'Djinn Monk', 2, 2, 4, ['Djinn', 'Monk'], True, 'one'),
}
TEAM_PUMP = 'Creatures you control get +2/+1 until end of turn.'
LIFE_DRAW = 'You gain 2 life. Then if you have more life than an opponent, draw a card.'
SIGHT = ('Look at the top two cards of your library. Put one of them into your hand '
         'and the other on the bottom of your library.')
MEMOIR = 'Return an instant card at random from your graveyard to your hand.'
INSTANTS = ('Shock', 'Unsummon')
SORCERY = 'Divination'
LIBRARY = ('Forest', 'Island', 'Mountain', 'Plains', 'Swamp')
HANDLERS = {'tokens': {'manalinkeh_card_nomads_assembly', 'manalinkeh_card_ojutais_summons'},
            'team': {'manalinkeh_card_great_teachers_decree'}, 'life': {'manalinkeh_card_survival_cache'},
            'sight': {'manalinkeh_card_sight_beyond_sight'}, 'memoir': {'manalinkeh_card_surreal_memoir'}}
TYPE_WITNESSES = {'Kor Soldier': ('Kor Cartographer', SQUIRE),
                  'Djinn Monk': ('Mahamoti Djinn', 'Wandering Champion')}
WITNESSES = {BEAR, COUNTER, GIANT, SQUIRE, *LIBRARY, *INSTANTS, SORCERY, COAT, SPIDER,
             *(n for names in TYPE_WITNESSES.values() for n in names)}
TOKENS = {'Kor Soldier': 18779, 'Djinn Monk': 18718}
GAPS = ('The complete real upkeep driver does not expose a separate response window to the recast.',
        'Human may decline through actual default-pass; the AI outer policy elects to take Rebound.')


@lru_cache(maxsize=1)
def profiles():
    return json.loads(PROFILE.read_text(encoding='utf8'))


def classify(record):
    from recipes import cost_from_oracle

    if (not record or record.get('layout') != 'normal' or record.get('card_faces') or record.get('selected_face')
            or record.get('keywords') != ['Rebound'] or record.get('type_line') != 'Sorcery'):
        raise ValueError('whole normal untargeted Rebound Sorcery required')
    clause, *extra = record['oracle_text'].splitlines()
    if extra != [REBOUND]:
        raise ValueError('complete printed Rebound clause required')
    params = dict(mana=cost_from_oracle(record), keywords=[])
    if clause in TOKEN_MODELS:
        name, power, toughness, color, subtypes, flying, amount = TOKEN_MODELS[clause]
        return dict(params, mode='tokens', token=dict(name=name, power=power, toughness=toughness,
                                                     color=color, subtypes=subtypes, flying=flying, amount=amount))
    if clause == TEAM_PUMP:
        return dict(params, mode='team', power=2, toughness=1)
    if clause == LIFE_DRAW:
        return dict(params, mode='life', life=2, draw=1)
    if clause == SIGHT:
        return dict(params, mode='sight', look=2, keep=1)
    if clause == MEMOIR:
        return dict(params, mode='memoir', card_type='Instant', amount=1, random=True)
    raise ValueError('untargeted Rebound effect outside reviewed whole clauses')


def variants(params):
    pairs = ('scheduled', 'countered', 'snapshot', 'wrong_color')
    if params['mode'] == 'tokens':
        pairs += ('subtypes',)
        pairs += ('flying', 'reach') if params['token']['flying'] else ('enemy_only',)
    elif params['mode'] == 'team':
        pairs += ('enemy_only', 'combat')
    elif params['mode'] == 'life':
        pairs += ('equal_then_ahead', 'still_behind')
    elif params['mode'] == 'sight':
        pairs += ('choose_second', 'one_card', 'empty_library')
    else:
        pairs += ('one_instant', 'no_instant')
    return [f'{mode}_{owner}' for owner in (0, 1) for mode in pairs] + ['decline_0']


def check_variants(params, selected):
    if (type(selected) is not list or not selected or any(type(v) is not str for v in selected)
            or len(selected) != len(set(selected)) or set(selected) - set(variants(params))):
        raise ValueError('distinct reviewed untargeted Rebound variants required')


def spec(declaration, record):
    params = classify(record)
    if (declaration.get('family') != FAMILY or type(declaration.get('version')) is not int
            or declaration['version'] != 2 or profiles()['sources'].get(record['name'], {}).get('oracle') != record
            or declaration.get('oracle_text') != record['oracle_text']
            or json.dumps(declaration.get('params'), sort_keys=True) != json.dumps(params, sort_keys=True)):
        raise ValueError('exact typed declaration with full pinned untargeted Rebound Oracle required')
    check_variants(params, declaration.get('variants'))
    return dict(params, family=FAMILY)


def validate_witnesses(oracle, rows):
    data = profiles()
    skipped = {row['csvid'] for row in json.loads((PROFILE.parent / 'unimplemented.json').read_text(
        encoding='utf8'))['cases'].values()}
    if (data.get('version') != 1 or not data.get('sources') or set(data.get('witnesses', ())) != WITNESSES
            or set(data.get('tokens', ())) != set(TOKENS)):
        raise ValueError('exact untargeted Rebound pins required')
    for group in ('sources', 'witnesses', 'tokens'):
        for name, pin in data[group].items():
            row, record = rows.get(name, {}), oracle.get(name, {})
            if pin != dict(oracle=record, registry=row):
                raise ValueError(f'unreviewed untargeted Rebound binding: {name}')
            if group == 'tokens':
                # These reviewed core registry entries default to kind=card. Their
                # creator handlers and pinned Oracle identify the physical tokens.
                if row['csvid'] != TOKENS[name] or record.get('layout') != 'token':
                    raise ValueError(f'physical token identity required: {name}')
                continue
            if (row.get('kind') != 'card' or row['csvid'] in skipped or record.get('layout') != 'normal'
                    or row['handler'] in (None, '0', 'NULL', 'card_dummy')
                    and name not in (BEAR, SQUIRE, GIANT, SPIDER, 'Mahamoti Djinn')):
                raise ValueError(f'whole normal implemented source or fixture required: {name}')
            if group == 'sources':
                params = classify(record)
                if row['handler'] not in HANDLERS[params['mode']]:
                    raise ValueError(f'unreviewed untargeted Rebound handler: {name}')
                if params['mode'] == 'tokens':
                    token = params['token']
                    token_oracle = oracle[token['name']]
                    if (token_oracle['type_line'].split(' — ')[-1].split() != token['subtypes']
                            or (int(token_oracle['power']), int(token_oracle['toughness'])) != (token['power'], token['toughness'])
                            or token_oracle['keywords'] != (['Flying'] if token['flying'] else [])
                            or token_oracle['colors'] != (['U'] if token['flying'] else ['W'])):
                        raise ValueError(f'token descriptor disagrees with intended token pin: {name}')


class ValueWorld(ReboundWorld):
    def __init__(self, name, params, mode, owner, rows, oracle):
        self.result = params['mode']
        self.token = params.get('token')
        super().__init__(name, params, mode, owner, rows, oracle)

    def configure(self):
        self.add(self.name, 'source', self.owner, 'hand')
        if self.result != 'life':
            if self.mode != 'enemy_only':
                self.add(BEAR, 'first', self.owner)
                if self.result == 'team':
                    self.add(SQUIRE, 'other', self.owner)
            if self.mode == 'subtypes':
                self.add(COAT, 'coat', self.foe)
                for index, name in enumerate(TYPE_WITNESSES[self.token['name']]):
                    self.add(name, f'type_witness_{index}', self.foe)
            else:
                self.add(SPIDER if self.mode == 'reach' else GIANT, 'blocker', self.foe)
        if self.mode == 'countered':
            self.add(COUNTER, 'response', self.foe, 'hand')
        for player in (0, 1):
            for index, name in enumerate(LANDS):
                self.add(name, f'library_{player}_{index}', player, 'library')
        self.emit('turn', player=self.owner, value=0)
        self.emit('present_ai_dialogs', value=1)
        for player in (0, 1):
            self.pool(player, ZERO)
        if self.mode in ('equal_then_ahead', 'still_behind'):
            self.life[self.owner] = 18 if self.mode == 'equal_then_ahead' else 15
            self.emit('set_life', player=self.owner, value=self.life[self.owner])
        self.receipts()

    def base_creatures(self):
        return sum(e['zone'] == 'battlefield' and e['player'] == self.owner for e in self.entities.values())

    def tokens_created(self):
        return self.zones['battlefield'][self.owner].count(self.token['name']) if self.token else 0

    def receipts(self):
        token_count = self.tokens_created()
        witnesses = [e for ref, e in self.entities.items() if ref.startswith('type_witness_')]
        # Each independently typed opposing witness shares exactly one of the
        # two expected subtypes. Coat changes their real P/T by the token count.
        for e in witnesses:
            for stat in ('power', 'toughness'):
                e[stat] += token_count
        try:
            super().receipts()
        finally:
            for e in witnesses:
                for stat in ('power', 'toughness'):
                    e[stat] -= token_count
        if not self.token:
            return
        token_id = self.rows[self.token['name']]['csvid']
        for player in (0, 1):
            for zone in ('battlefield', 'graveyard', 'exile'):
                self.emit('assert_zone_multiplicity', player=player, zone=zone, index=token_id,
                          value=token_count if player == self.owner and zone == 'battlefield' else 0)
        if token_count:
            extra = token_count - 1 + len(witnesses) if self.mode == 'subtypes' else 0
            for prop, value in (('token', 1), ('color', self.token['color']), ('artifact', 0),
                                ('power', self.token['power'] + extra), ('toughness', self.token['toughness'] + extra)):
                self.emit('assert_matching_permanents', player=self.owner, index=token_id, property=prop, value=value)

    def apply_effect(self):
        if self.result == 'life':
            self.life[self.owner] += self.p['life']
            if self.life[self.owner] > self.life[self.foe]:
                for _ in range(self.p['draw']):
                    self.zones['hand'][self.owner].append(self.zones['library'][self.owner].pop(0))
        elif self.result == 'team':
            for ref, e in self.entities.items():
                if e['zone'] == 'battlefield' and e['player'] == self.owner:
                    self.boost(ref)
        else:
            amount = 1
            if self.token['amount'] == 'creatures_you_control':
                amount = self.base_creatures() + self.tokens_created()
            self.zones['battlefield'][self.owner] += [self.token['name']] * amount

    def plan_value(self):
        """Queue any resolution choice before the operation that offers it."""

    def paid_value_cast(self):
        if self.mode == 'wrong_color':
            self.paid_cast()  # This inherited branch exercises the exact colored payment denial.
            return
        self.pool(self.owner, self.p['mana'])
        self.emit('cast', ref='source', value=1)
        self.emit('assert_stack', value=1)
        self.emit('assert_unused_choices', value=0)
        self.observe('source', 'in_hand', 0)
        self.mana[self.owner] = ZERO.copy()
        self.funds()
        if self.mode == 'countered':
            self.respond()
            self.move_spell(self.name, self.owner, 'hand', 'graveyard')
        else:
            self.plan_value()
            if self.owner == 0 and self.mode != 'decline':
                self.emit('choose_trigger', player=self.owner, ref='source')
            self.emit('resolve')
            self.move_spell(self.name, self.owner, 'hand', 'exile')
            self.apply_effect()
        self.receipts()
        if self.mode in ('flying', 'reach'):
            self.emit('bind_permanent', player=self.owner, value=self.rows[self.token['name']]['csvid'], ref='djinn')
            self.observe('djinn', 'sick', 1)

    def rebound_value(self, take):
        if take:
            self.plan_value()
        self.emit('upkeep_step', player=self.owner)
        self.emit('assert_unused_choices', value=0)
        if take:
            self.move_spell(self.name, self.owner, 'exile', 'graveyard')
            self.apply_effect()
        self.receipts()
        self.emit('turn', player=self.owner, value=0)

    def token_combat(self):
        token_name = self.token['name']
        self.observe('djinn', 'sick', 0)
        self.emit('begin_attackers')
        self.emit('attack', ref='djinn', value=1)
        self.emit('finish_attackers')
        self.emit('begin_blockers')
        blocked = self.mode == 'reach'
        self.emit('block', ref='blocker', other='djinn', value=int(blocked))
        self.observe('blocker', 'blocking_count', blocked)
        self.emit('advance_combat')
        self.emit('combat_damage')
        self.emit('resolve')
        self.emit('end_combat')
        if blocked:
            self.entities['blocker']['damage'] = self.token['power']
            self.zones['battlefield'][self.owner].remove(token_name)
        else:
            self.life[self.foe] -= self.token['power']
        self.receipts()


class ZoneValueWorld(ValueWorld):
    def configure(self):
        self.returned = 0
        self.add(self.name, 'source', self.owner, 'hand')
        if self.mode == 'countered':
            self.add(COUNTER, 'response', self.foe, 'hand')
        for player in (0, 1):
            library = LANDS
            if player == self.owner and self.result == 'sight':
                library = {'one_card': LIBRARY[:1], 'empty_library': ()}.get(self.mode, LIBRARY)
            for index, name in enumerate(library):
                self.add(name, f'library_{player}_{index}', player, 'library')
        if self.result == 'memoir':
            self.eligible = {'one_instant': INSTANTS[:1], 'no_instant': ()}.get(self.mode, INSTANTS)
            for index, name in enumerate((SORCERY, *self.eligible)):
                self.add(name, f'own_grave_{index}', self.owner, 'graveyard')
            self.add(INSTANTS[0], 'enemy_instant', self.foe, 'graveyard')
        self.emit('turn', player=self.owner, value=0)
        self.emit('present_ai_dialogs', value=1)
        for player in (0, 1):
            self.pool(player, ZERO)
        self.receipts()

    def sight_selection(self):
        prefix = self.zones['library'][self.owner][:self.p['look']]
        return prefix[-1 if self.mode == 'choose_second' else 0] if prefix else None

    def plan_value(self):
        if self.result == 'sight':
            chosen = self.sight_selection()
            if chosen:
                self.emit('choose_zone_card', player=self.owner, value=self.rows[chosen]['csvid'])

    def apply_effect(self):
        if self.result == 'sight':
            library = self.zones['library'][self.owner]
            chosen = self.sight_selection()
            if chosen:
                rest = library[:self.p['look']]
                rest.remove(chosen)
                library[:] = library[self.p['look']:] + rest
                self.zones['hand'][self.owner].append(chosen)
            return
        if self.returned < len(self.eligible):
            # The ledger uses one eligible name to account for zone sizes. An
            # ambiguous individual card is checked across both zones instead.
            name = self.eligible[self.returned]
            self.move_spell(name, self.owner, 'graveyard', 'hand')
            self.returned += 1

    def receipts(self):
        start = len(self.steps)
        super().receipts()
        if self.result != 'memoir' or len(self.eligible) != 2 or self.returned != 1:
            return
        eligible = {self.rows[name]['csvid'] for name in self.eligible}
        self.steps[start:] = [row for row in self.steps[start:]
                             if not (row['op'] == 'assert_zone_multiplicity' and row['player'] == self.owner
                                     and row['zone'] in ('hand', 'graveyard') and row['index'] in eligible)]
        for identity in sorted(eligible):
            self.emit('assert_hand_graveyard_total', player=self.owner, index=identity, value=1)


def scenario(world):
    if world.mode == 'no_instant':
        world.pool(world.owner, world.p['mana'])
        world.emit('can_cast', ref='source', value=1)
        world.receipts()
        return None
    world.paid_value_cast()
    if world.mode == 'wrong_color':
        return None
    pending_choice = world.owner == 0 and world.mode not in ('countered', 'decline')
    world.prepare_upkeep()
    world.perform_upkeep(observe_choices=not pending_choice)
    world.prepare_upkeep()
    cut = len(world.steps) if world.mode == 'snapshot' else None
    world.rebound_value(world.mode not in ('countered', 'decline'))
    if world.mode in ('flying', 'reach'):
        world.token_combat()
    if world.mode == 'combat':
        world.fight(blocked=True, bonus=False)
    if world.mode != 'countered':
        for _ in range(2):
            world.prepare_upkeep()
            world.perform_upkeep()
    return cut


def expand(name, csvid, declaration, params, selected, rows, oracle):
    from recipes import step

    check_variants(params, selected)
    validate_witnesses(oracle, rows)
    if profiles()['sources'][name]['registry']['csvid'] != csvid:
        raise ValueError('exact untargeted Rebound CSVID required')
    cases = []
    for variant in selected:
        mode, owner = variant.rsplit('_', 1)
        world_type = ZoneValueWorld if params['mode'] in ('sight', 'memoir') else ValueWorld
        world = world_type(name, params, mode, int(owner), rows, oracle)
        cut = scenario(world)
        steps = world.steps
        if cut is not None:
            tail = steps[cut:]
            steps = (steps[:cut] + [step('capture')] + tail + [step('record_state'), step('ai_roundtrip'), step('restore')]
                     + copy.deepcopy(tail) + [step('assert_recorded_state')])
        cases.append(dict(name=f'UntargetedReboundRecipe_{csvid}_{variant}', card=name, fixture='empty_main', seed=163,
                          setup=world.setup, steps=steps, _recipe=dict(family=FAMILY, version=2, variant=variant,
                              params=declaration['params'], oracle_text=declaration['oracle_text'],
                              untested_components=list(GAPS))))
    return cases
