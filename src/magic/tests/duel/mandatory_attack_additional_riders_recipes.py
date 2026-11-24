"""Required attacks with printed Bestow, cycling, landfall and Madness riders."""

import copy
import json
import re
from functools import lru_cache
from pathlib import Path

from bestow_stat_aura_recipes import REMINDER, Scene
from required_attack_creature_recipes import CLAUSE, LIBRARY, RequiredAttackCase

FAMILY = 'mandatory_attack_additional_riders'
PROFILE = Path(__file__).with_name('data') / 'mandatory_attack_additional_riders_wave8_profiles.json'
CARABID = 'Cycling {B/R} ({B/R}, Discard this card: Draw a card.)'
LANDFALL = ('Landfall — Whenever a land you control enters, you may pay {4}{R}{R}. '
            'If you do, return this card from your graveyard to the battlefield.')
MADNESS = ('Madness {3}{R} (If you discard this card, discard it into exile. '
           'When you do, cast it for its madness cost or put it into your graveyard.)')
CASES = {
    'bestow': ('normal_fresh', 'normal_aged', 'bestow_own', 'bestow_enemy', 'detach', 'pending_target', 'unfunded'),
    'cycling': ('omitted', 'tapped', 'paid_fresh', 'blocked', 'red', 'black', 'wrong_color', 'cycle_replay'),
    'landfall': ('omitted', 'paid_fresh', 'reach_block', 'air_block', 'return_pay', 'return_decline', 'short', 'enemy_land'),
    'madness': ('omitted', 'tapped', 'paid_fresh', 'blocked', 'madness_pay', 'madness_decline', 'madness_short', 'madness_replay'),
}
COMMON = {'omitted', 'tapped', 'paid_fresh', 'blocked', 'reach_block', 'air_block'}
HANDLERS = {'bestow': 'manalinkeh_card_mogiss_warhound', 'cycling': 'manalinkeh_card_monstrous_carabid',
            'landfall': 'manalinkeh_card_akoum_firebird', 'madness': 'manalinkeh_card_insatiable_gorgers'}
HOST = 'Krosan Tusker'
OTHER = 'Ravenous Baloth'
BOUNCE = 'Unsummon'
DISCARD = 'Rummaging Goblin'
WITNESSES = set(LIBRARY) | {HOST, OTHER, BOUNCE, DISCARD, 'Murder', 'Suntail Hawk', 'Giant Spider'}
VANILLA = {'Grizzly Bears', 'Hill Giant', 'Suntail Hawk', 'Giant Spider'}
GAPS = {
    'bestow': (), 'cycling': (), 'landfall': (),
    'madness': ('The legacy payoff routine casts from hand with an exile marker. '
                'Actual exile-residency and a separate Madness trigger priority window are not exposed by this fixture.',),
}


@lru_cache(maxsize=1)
def profiles():
    return json.loads(PROFILE.read_text(encoding='utf8'))


def classify(record):
    from recipes import cost_from_oracle

    if (not record or record.get('layout') != 'normal' or record.get('card_faces') or record.get('selected_face')
            or any(not re.fullmatch(r'[1-9][0-9]*', record.get(key, '')) for key in ('power', 'toughness'))):
        raise ValueError('full normal fixed-stat required attacker needed')
    lines = record['oracle_text'].splitlines()
    card_type = record['type_line'].split(' — ')[0]
    power, toughness = (int(record[k]) for k in ('power', 'toughness'))
    costs = dict(printed=cost_from_oracle(record))
    extra = {}
    if lines == [CLAUSE, CARABID] and card_type == 'Creature':
        flag, keywords = 'cycling', ['Cycling']
        for color, symbol in (('red', 'R'), ('black', 'B')):
            costs[color] = cost_from_oracle({'mana_cost': '{' + symbol + '}'})
    elif lines == ['Flying, haste', CLAUSE, LANDFALL] and card_type == 'Creature':
        flag, keywords = 'landfall', ['Landfall', 'Flying', 'Haste']
        costs['return'] = cost_from_oracle({'mana_cost': '{4}{R}{R}'})
    elif lines == [CLAUSE, MADNESS] and card_type == 'Creature':
        flag, keywords = 'madness', ['Madness']
        costs['madness'] = cost_from_oracle({'mana_cost': '{3}{R}'})
    elif (len(lines) == 3 and card_type == 'Enchantment Creature' and lines[1] == CLAUSE
          and lines[0].endswith(REMINDER)
          and (cost := re.fullmatch(r'Bestow ((?:\{[1-9]\})?\{[BUGRW]\})', lines[0].removesuffix(REMINDER)))
          and (bonus := re.fullmatch(r'Enchanted creature gets \+([1-9])/\+([1-9]) and attacks each combat if able\.', lines[2]))):
        flag, keywords = 'bestow', ['Bestow']
        costs['bestowed'] = cost_from_oracle({'mana_cost': cost[1]})
        extra['bonus'] = list(map(int, bonus.groups()))
    else:
        raise ValueError('complete supported required-attack rider text required')
    if record['keywords'] != keywords:
        raise ValueError('all required-attack rider keywords must agree with Oracle')
    return dict(shape=flag, keywords=keywords, costs=costs, power=power, toughness=toughness, **extra)


def variants(params):
    return [f'{mode}_{seat}' for seat in (0, 1) for mode in CASES[params['shape']]]


def check_variants(params, selected):
    if (type(selected) is not list or not selected or any(type(v) is not str for v in selected)
            or len(set(selected)) != len(selected) or set(selected) - set(variants(params))):
        raise ValueError('distinct reviewed required-attack rider variants needed')


def spec(declaration, record):
    params = classify(record)
    if (declaration.get('family') != FAMILY or type(declaration.get('version')) is not int or declaration['version'] != 2
            or profiles()['sources'].get(record['name'], {}).get('oracle') != record
            or declaration.get('oracle_text') != record['oracle_text']
            or json.dumps(declaration.get('params'), sort_keys=True) != json.dumps(params, sort_keys=True)):
        raise ValueError('declaration must equal whole pinned mandatory-attack rider Oracle')
    check_variants(params, declaration.get('variants'))
    return dict(params, family=FAMILY)


def validate_witnesses(oracle, rows):
    data = profiles()
    skipped = {r['csvid'] for r in json.loads((PROFILE.parent / 'unimplemented.json').read_text(
        encoding='utf8'))['cases'].values()}
    if data.get('version') != 1 or not data.get('sources') or set(data.get('witnesses', ())) != WITNESSES:
        raise ValueError('exact implemented mandatory-attack rider profiles required')
    for group in ('sources', 'witnesses'):
        for name, pin in data[group].items():
            row, record = rows.get(name, {}), oracle.get(name, {})
            if (pin != dict(oracle=record, registry=row) or row.get('kind') != 'card' or record.get('layout') != 'normal'
                    or row['csvid'] in skipped or (row['handler'] in (None, 'NULL', '0', 'card_dummy') and name not in VANILLA)):
                raise ValueError(f'changed required-attack source or witness: {name}')
            if group == 'sources' and row['handler'] != HANDLERS[classify(record)['shape']]:
                raise ValueError(f'changed mandatory-attack rider handler: {name}')
    aura = data['bestowed']
    source = data['sources']["Mogis's Warhound"]['registry']
    if rows.get(aura['name']) != aura or aura['csvid'] != source['csvid'] + 1 or aura['handler'] != source['handler']:
        raise ValueError('registered paired Bestow Aura identity required')


class BestowAttack(Scene):
    def __init__(self, name, owner, mode, params, rows, oracle):
        self.name, self.owner, self.enemy, self.mode = name, owner, 1 - owner, mode
        self.registry, self.oracle = rows, oracle
        self.model = dict(normal=params['costs']['printed'], bestowed=params['costs']['bestowed'],
                          creature=(params['power'], params['toughness']), bonus=params['bonus'])
        self.id, self.aura_id = rows[name]['csvid'], profiles()['bestowed']['csvid']
        self.setup, self.steps, self.objects = [], [], {}
        self.mana, self.life = [[0] * 6 for _ in (0, 1)], [20, 20]
        self.form, self.snapshot = None, None

    def advance_to_enemy(self):
        self.emit('advance_turn')
        self.emit('untap_step', player=self.enemy)
        self.emit('turn', player=self.enemy, value=0)

    def declaration(self, ref, actor, amount, allowed=True):
        self.emit('begin_required_attackers')
        self.prop(ref, 'attacking', 0)
        self.emit('can_finish_attackers', value=int(not allowed))
        self.emit('attack', ref=ref, value=int(allowed))
        self.prop(ref, 'attacking', int(allowed))
        self.emit('can_finish_attackers', value=1)
        self.emit('finish_attackers')
        if allowed:
            self.objects[ref]['tapped'] = True
            self.emit('begin_blockers')
            self.emit('advance_combat')
            self.emit('combat_damage')
            self.resolve()
            self.emit('end_combat')
            self.life[1 - actor] -= amount

    def play(self):
        mode = self.mode
        self.add('source', self.name, self.owner)
        self.add('probe', 'Murder', self.enemy)
        self.add('host', HOST, self.enemy if mode == 'bestow_enemy' else self.owner, 'battlefield')
        self.add('other', OTHER, self.owner, 'battlefield')
        if mode in ('detach', 'pending_target'):
            self.add('helper', BOUNCE, self.enemy)
        self.emit('turn', player=self.owner, value=0)
        self.emit('present_ai_dialogs', value=1)
        for who in (0, 1):
            for color in range(6):
                self.emit('set_mana', player=who, color=color, value=0)
        if mode == 'bestow_own':
            # Run the normal end-turn callback before the fixture's first combat.
            self.emit('cleanup')
            self.emit('turn', player=self.owner, value=0)
        self.emit('refresh')
        if mode == 'unfunded':
            self.cast_source(budget=[0] * 6, success=False)
            self.receipts()
            return None
        normal = mode in ('normal_fresh', 'normal_aged')
        self.cast_source(bestow=not normal, choice=None if normal else 0)
        if mode == 'pending_target':
            self.cast_helper('helper', 'host', response=True)
            self.objects['host']['zone'] = 'hand'
            self.entered('creature')
        else:
            self.resolve()
            self.entered('creature' if normal else 'aura')
        if mode == 'detach':
            self.cast_helper('helper', 'host')
            self.objects['host']['zone'] = 'hand'
            self.entered('creature')
        if mode == 'bestow_enemy':
            self.advance_to_enemy()
        if self.form == 'creature' and mode != 'normal_fresh':
            self.age_turns()
        cut = len(self.steps) if mode == 'bestow_own' else None
        if self.form == 'aura':
            actor = self.objects['host']['player']
            self.declaration('host', actor, int(self.oracle[HOST]['power']) + self.model['bonus'][0])
        else:
            self.declaration('source', self.owner, self.model['creature'][0], mode != 'normal_fresh')
        self.receipts()
        return cut


class RiderCase(RequiredAttackCase):
    def __init__(self, name, csvid, declaration, params, variant, rows, oracle):
        mode, owner = variant.rsplit('_', 1)
        super().__init__(name, csvid, declaration, params, f'paid_fresh_{owner}', rows, oracle)
        self.mode, self.variant = mode, variant
        self.params = params
        self.mana = [[0] * 6 for _ in (0, 1)]
        self.steps = []

    def fund(self, p, pool):
        self.mana[p] = list(pool)
        for color, value in enumerate(pool):
            self.emit('set_mana', player=p, color=color, value=value)
        self.emit('refresh')

    def start(self, active):
        self.emit('turn', player=active, value=0)
        self.fund(self.owner, self.mana[self.owner])
        self.fund(self.other, self.mana[self.other])
        self.emit('cleanup')
        self.emit('turn', player=active, value=0)
        self.emit('refresh')

    def cycle(self):
        self.start(self.owner)
        who = self.owner
        mode = self.mode
        denied = mode == 'wrong_color'
        self.fund(who, [0, 0, 1, 0, 0, 0] if denied else self.params['costs']['black' if mode == 'black' else 'red'])
        self.emit('can_activate_hand', ref='source', value=int(not denied))
        if denied:
            self.emit('activate_hand', ref='source', value=0)
            self.receipt()
            return None
        self.emit('choose_dialog', player=who, value=1)
        self.emit('activate_hand', ref='source', value=1)
        self.move('source', 'graveyard')
        self.mana[who] = [0] * 6
        self.receipt(1)
        cut = len(self.steps) if mode == 'cycle_replay' else None
        self.emit('resolve')
        self.zones['hand'][who].append(self.zones['library'][who].pop(0))
        self.receipt()
        return cut

    def firebird(self):
        self.move('source', 'graveyard')
        next(row for row in self.setup if row['ref'] == 'source')['zone'] = 'graveyard'
        active = self.other if self.mode == 'enemy_land' else self.owner
        self.add('Island', 'land', active, 'hand')
        self.start(active)
        self.emit('prepare_graveyard_abilities')
        paid = self.mode == 'return_pay'
        full = self.mode != 'short'
        budget = self.params['costs']['return'] if full else [5, 0, 0, 0, 1, 0]
        self.fund(self.owner, budget)
        self.emit('present_ai_dialogs', value=1)
        if full and active == self.owner:
            self.emit('choose_dialog', player=self.owner, value=int(not paid))
        self.emit('play_land', ref='land', value=1)
        self.move('land', 'battlefield')
        self.emit('resolve')
        self.emit('refresh')
        if paid:
            self.mana[self.owner] = [0] * 6
            # A graveyard ref identifies the initial zone index, not the new permanent.
            self.zones['graveyard'][self.owner].remove(self.name)
            self.zones['battlefield'][self.owner].append(self.name)
            self.entities['source']['zone'] = 'returned'
        self.receipt()
        return None

    def madness(self):
        who = self.owner
        self.add(DISCARD, 'discarder', who)
        self.names = sorted({row['name'] for row in self.setup})
        self.start(who)
        self.emit('discard_policy', value=1)
        self.emit('present_ai_dialogs', value=1)
        funded = self.mode != 'madness_short'
        paid = self.mode in ('madness_pay', 'madness_replay')
        self.fund(who, self.params['costs']['madness'] if funded else [4, 0, 0, 0, 0, 0])
        cut = len(self.steps) if self.mode == 'madness_replay' else None
        self.emit('can_activate', ref='discarder', value=1)
        if who == 1:
            # The AI exposes its sole discarded card in an OK-only notice first.
            self.emit('choose_dialog', player=who, value=0)
        # Omit-illegal Madness menu leaves Decline at index 0 when unaffordable.
        self.emit('choose_dialog', player=who, value=int(funded and not paid))
        self.emit('activate', ref='discarder', value=1)
        self.emit('assert_unused_choices', value=0)
        self.emit('resolve')
        self.emit('refresh')
        self.entities['discarder']['tapped'] = True
        self.move('source', 'battlefield' if paid else 'graveyard')
        self.zones['hand'][who].append(self.zones['library'][who].pop(0))
        if paid:
            self.mana[who] = [0] * 6
        self.receipt()
        return cut


def replay(step, steps, cut):
    if cut is None:
        return steps
    tail = steps[cut:]
    return (steps[:cut] + [step('capture')] + tail + [step('record_state'), step('ai_roundtrip'), step('restore')]
            + copy.deepcopy(tail) + [step('assert_recorded_state')])


def expand(name, csvid, declaration, params, selected, rows, oracle):
    from recipes import step

    check_variants(params, selected)
    validate_witnesses(oracle, rows)
    if profiles()['sources'][name]['registry']['csvid'] != csvid:
        raise ValueError('exact mandatory-attack rider CSVID required')
    shape = params['shape']
    cases = []
    for variant in selected:
        mode, owner = variant.rsplit('_', 1)
        if mode in COMMON:
            case = RequiredAttackCase(name, csvid, declaration, params, variant, rows, oracle).case()
        elif shape == 'bestow':
            scene = BestowAttack(name, int(owner), mode, params, rows, oracle)
            cut = scene.play()
            case = dict(setup=scene.setup, steps=replay(step, scene.steps, cut))
        else:
            scene = RiderCase(name, csvid, declaration, params, variant, rows, oracle)
            action = {'cycling': scene.cycle, 'landfall': scene.firebird, 'madness': scene.madness}[shape]
            cut = action()
            case = dict(setup=scene.setup, steps=replay(step, scene.steps, cut))
        case.update(name=f'MandatoryAttackAdditionalRidersRecipe_{csvid}_{variant}', card=name,
                    fixture='empty_main', seed=2047,
                    _recipe=dict(family=FAMILY, version=2, variant=variant, params=declaration['params'],
                                 oracle_text=declaration['oracle_text'], untested_components=list(GAPS[shape])))
        cases.append(case)
    return cases
