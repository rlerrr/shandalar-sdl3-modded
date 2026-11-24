"""Whole Oracle Threshold modes through actual costs and target selection."""

from collections import Counter
from functools import lru_cache
import json
from pathlib import Path
import re

from counter_condition_recurrence_recipes import exact
from recipes import COLORS, cost_from_oracle

FAMILY = 'threshold_modal_activations'
PROFILE = Path(__file__).parent / 'data/threshold_modal_activations_wave8_profiles.json'
ZONES = ('hand', 'battlefield', 'library', 'graveyard', 'exile')
EXPANSION = {
    5910: ('cancel_cost',), 5582: ('short_fee',), 5830: ('choose_one',),
    5979: ('short_fee',), 5638: ('short_fee',),
}
WITNESSES = {
    'Coat of Arms', 'Forest', 'Giant Spider',
    'Grizzly Bears', 'Island', 'Kraul Warrior',
    'Ley Druid', 'Monastery Swiftspear', 'Mountain',
    'Plains', 'Rumbling Baloth', 'Runeclaw Bear',
    'Spellbook', 'Stone-Seeder Hierophant', 'Swamp',
}
STATIC_WITNESSES = {'Grizzly Bears', 'Runeclaw Bear', 'Rumbling Baloth', 'Giant Spider'}

COMMON = ('six_duplicates', 'seven_duplicates', 'opponent_only')
EXTRAS = {5910: ('once_per_turn',), 5582: ('seven_base',), 5830: ('choose_two', 'zero_eligible'),
          5979: (), 5638: ('seven_base',)}
ZERO = [0] * 6
MOUNTAIN = 'Mountain'
LIBRARY = ('Island', 'Plains')
ACTIVATION_MANA = re.compile(r'((?:\{[1-9][0-9]*\}|\{[BGW]\})+)')


class ModalActivationCase:
    def __init__(self, record, csvid, actor, mode, rows, oracle):
        self.source, self.name = record, record['name']
        self.csvid, self.actor, self.enemy, self.mode = csvid, actor, 1-actor, mode
        self.rows, self.oracle = rows, oracle
        self.setup, self.steps = [], []
        self.own_count = 10 if mode == 'once_per_turn' else 6 if mode in ('six_duplicates', 'opponent_only') else 7
        self.enemy_count = 7 if mode == 'opponent_only' else 0
        self.threshold = self.own_count >= 7
        self.bottomed = 0
        self.add('source', actor, self.name, 0)
        for seat, count in ((actor, self.own_count), (self.enemy, self.enemy_count)):
            for index in range(count):
                self.add(f'grave_{seat}_{index}', seat, MOUNTAIN, 3)
            for index, name in enumerate(LIBRARY):
                self.add(f'deck_{seat}_{index}', seat, name, 2)
        if csvid == 5582:
            self.add('ally', actor, 'Grizzly Bears')
            self.add('enemy', self.enemy, 'Runeclaw Bear')
            self.add('large', self.enemy, 'Giant Spider')
            self.add('guard', self.enemy, 'Spellbook')
        if csvid == 5830 and mode != 'zero_eligible':
            self.lands = [('Forest', actor, 'G'), ('Plains', self.enemy, 'W'),
                          ('Island', actor, 'U'), ('Swamp', self.enemy, 'B')]
            for index, (name, seat, _) in enumerate(self.lands):
                self.add(f'land_{index}', seat, name)
        if csvid == 5979:
            self.add('land_probe', actor, 'Ley Druid' if self.threshold else 'Stone-Seeder Hierophant')
            self.add('monk', actor, 'Monastery Swiftspear')
            self.add('insect', actor, 'Kraul Warrior')
            self.add('coat', actor, 'Coat of Arms', 0)
            self.add('blocker', self.enemy, 'Rumbling Baloth')
        if csvid == 5638:
            self.add('target_one', self.enemy, 'Grizzly Bears')
            self.add('target_two', actor, 'Runeclaw Bear')
            self.add('guard', self.enemy, 'Giant Spider')

    def add(self, ref, player, name, zone=1):
        from recipes import card

        self.setup.append(card(name, ref, player, ZONES[zone]))

    def emit(self, op, ref='', other='', prop='', **fields):
        from recipes import step

        if ref:
            fields['ref'] = ref
        if other:
            fields['other'] = other
        if prop:
            fields['property'] = prop
        if 'zone' in fields:
            fields['zone'] = ZONES[fields['zone']]
        self.steps.append(step(op, **fields))

    def observe(self, ref, prop, value):
        self.emit('assert_card', ref, prop=prop, value=value)

    def mana(self, op, player, pool):
        for color, value in enumerate(pool):
            self.emit(op, player=player, value=value, color=color)

    def zone(self, name, player, zone, count):
        self.emit('assert_zone_multiplicity', player=player, zone=zone, index=self.rows[name]['csvid'], value=count)

    def combat(self, attacker, blocker, *, blocked):
        self.emit('begin_attackers')
        self.emit('attack', attacker, value=1)
        self.emit('finish_attackers')
        self.emit('begin_blockers')
        self.emit('block', blocker, attacker, value=int(blocked))
        self.emit('advance_combat')
        self.observe(attacker, 'blocked', int(blocked))
        self.emit('combat_damage')
        self.emit('resolve')
        self.emit('end_combat')
        self.emit('assert_choices_consumed')

    def graves(self, added_own=(), added_enemy=()):
        for seat, mountain_count, added in ((self.actor, self.own_count, added_own),
                                            (self.enemy, self.enemy_count, added_enemy)):
            self.emit('assert_zone', player=seat, zone=3, value=mountain_count + len(added))
            self.zone(MOUNTAIN, seat, 3, mountain_count)
            for name, count in sorted(Counter(added).items()):
                self.zone(name, seat, 3, count)

    def libraries(self):
        for seat, expected in ((self.actor, LIBRARY + (MOUNTAIN,)*self.bottomed), (self.enemy, LIBRARY)):
            self.emit('assert_zone', player=seat, zone=2, value=len(expected))
            for index, name in enumerate(expected):
                self.emit('assert_zone_card', player=seat, zone=2, index=index, value=self.rows[name]['csvid'])

    def pay_cast(self, ref, name):
        fee = cost_from_oracle(self.oracle[name])
        self.mana('set_mana', self.actor, fee)
        self.emit('refresh')
        self.emit('cast', ref, value=1)
        self.emit('assert_stack', value=1)
        self.emit('assert_choices_consumed')
        self.mana('assert_mana', self.actor, ZERO)
        self.resolve()
        self.observe(ref, 'in_play', 1)

    def resolve(self):
        self.emit('resolve')
        self.emit('refresh')
        self.emit('assert_stack', value=0)
        self.emit('assert_choices_consumed')

    def next_turn(self):
        for seat in (self.enemy, self.actor):
            self.emit('advance_turn')
            self.emit('untap_step', player=seat)
            self.emit('turn', player=seat)

    def pt(self, power, toughness):
        self.observe('source', 'power', power)
        self.observe('source', 'toughness', toughness)

    def activate(self, ref='source', *, allowed=True, pending=True):
        self.emit('activate', ref, value=int(allowed))
        self.emit('assert_choices_consumed')
        self.emit('assert_stack', value=int(allowed and pending))

    def mana_budget(self, budget):
        self.mana('set_mana', self.actor, budget)
        self.emit('refresh')

    def scrounger_payment(self):
        for _ in range(3):
            self.emit('choose_zone_card', player=self.actor, value=self.rows[MOUNTAIN]['csvid'])
        self.activate()
        self.own_count -= 3
        self.bottomed += 3
        self.graves()
        self.libraries()
        self.pt(3, 3)
        self.resolve()
        self.pt(6, 6)
        self.emit('can_activate', 'source', value=0)

    def scrounger(self):
        self.emit('can_activate', 'source', value=int(self.threshold))
        if not self.threshold:
            self.activate(allowed=False)
            self.graves()
            self.libraries()
            self.pt(3, 3)
            return
        if self.mode == 'cancel_cost':
            for _ in range(2):
                self.emit('choose_zone_card', player=self.actor, value=self.rows[MOUNTAIN]['csvid'])
            self.emit('choose_zone', player=self.actor, value=-1)
            self.activate(allowed=False)
            self.graves()
            self.libraries()
            self.pt(3, 3)
            self.emit('can_activate', 'source', value=1)
            return
        self.scrounger_payment()
        self.next_turn()
        self.pt(3, 3)
        self.graves()
        self.libraries()
        if self.mode == 'once_per_turn':
            self.emit('can_activate', 'source', value=1)
            if self.actor == 0:
                # Seat one's next-turn eligibility is independently observable.
                # Do not queue dependent inputs after the legacy reset failure.
                self.scrounger_payment()

    def fees(self):
        result = []
        for line in self.source['oracle_text'].splitlines():
            m = ACTIVATION_MANA.match(line.removeprefix('Threshold — '))
            if m:
                result.append(cost_from_oracle({'mana_cost': m[1]}))
        return result

    def offer_mode(self, high, *, high_available):
        if self.actor == 1:
            self.emit('present_ai_dialogs', value=1)
        if high_available or self.actor == 1:
            self.emit('choose_dialog', player=self.actor, value=int(high))

    def vermin(self):
        base, boosted = self.fees()
        high_available = self.threshold and self.mode != 'short_fee'
        high = high_available and self.mode != 'seven_base'
        paid = boosted if high else base
        budget = base if self.mode == 'short_fee' else boosted
        self.mana_budget(budget)
        self.emit('can_activate', 'source', value=1)
        self.offer_mode(high, high_available=high_available)
        self.activate()
        self.mana('assert_mana', self.actor, [have-due for have, due in zip(budget, paid)])
        for seat in (0, 1):
            self.emit('assert_life', player=seat, value=20)
        self.observe('source', 'in_play', 1)
        self.resolve()
        damage = 3 if high else 1
        for seat in (0, 1):
            self.emit('assert_life', player=seat, value=20-damage)
        self.zone(self.name, self.actor, 1, 0)
        own, enemy = (self.name,), ()
        for seat, name, ref in ((self.actor, 'Grizzly Bears', 'ally'), (self.enemy, 'Runeclaw Bear', 'enemy')):
            self.zone(name, seat, 1, int(not high))
            self.zone(name, seat, 3, int(high))
            if not high:
                self.observe(ref, 'damage', damage)
        if high:
            own += ('Grizzly Bears',)
            enemy += ('Runeclaw Bear',)
        self.observe('large', 'damage', damage)
        self.observe('guard', 'in_play', 1)
        self.graves(own, enemy)

    def restorer(self):
        self.next_turn()
        self.observe('source', 'sick', 0)
        if self.mode == 'zero_eligible':
            # Threshold's "up to" permits activating with zero targets.
            self.emit('can_activate', 'source', value=1)
            self.graves()
            return
        for index in range(len(self.lands)):
            self.activate(f'land_{index}', pending=False)
            self.observe(f'land_{index}', 'tapped', 1)
        for seat in (self.actor, self.enemy):
            pool = ZERO.copy()
            for _, controller, color in self.lands:
                if controller == seat:
                    pool[COLORS[color]] += 1
            self.mana('assert_mana', seat, pool)
            self.mana('set_mana', seat, ZERO)
        self.emit('refresh')
        self.emit('can_activate', 'source', value=1)
        count = {'choose_one': 1, 'choose_two': 2}.get(self.mode, 3 if self.threshold else 1)
        for index in range(count):
            self.emit('choose_target', f'land_{index}', player=self.actor)
        if self.mode in ('choose_one', 'choose_two'):
            for _ in range(3-count):
                self.emit('choose_cancel_target', player=self.actor)
        self.activate()
        self.observe('source', 'tapped', 1)
        for index in range(len(self.lands)):
            self.observe(f'land_{index}', 'tapped', 1)
        self.resolve()
        self.observe('source', 'tapped', 1)
        for index in range(len(self.lands)):
            self.observe(f'land_{index}', 'tapped', int(index >= count))
        self.emit('can_activate', 'source', value=0)
        self.graves()

    def monastery(self):
        self.next_turn()
        self.emit('can_target_creature', 'land_probe', 'source', value=0)
        self.observe('source', 'tapped', 0)
        self.activate(pending=False)
        self.observe('source', 'tapped', 1)
        self.mana('assert_mana', self.actor, [1, 0, 0, 0, 0, 0])
        fee, = self.fees()
        if self.mode == 'short_fee':
            fee[COLORS['W']] -= 1
        enabled = self.threshold and self.mode != 'short_fee'
        self.mana_budget(fee)
        self.emit('can_activate', 'source', value=int(enabled))
        self.activate(allowed=enabled)
        self.observe('source', 'tapped', 1)
        self.mana('assert_mana', self.actor, ZERO if enabled else fee)
        if not enabled:
            self.emit('can_target_creature', 'land_probe', 'source', value=0)
            self.observe('source', 'csvid', self.csvid)
            self.graves()
            return
        self.resolve()
        self.emit('can_target_creature', 'land_probe', 'source', value=1)
        self.pt(4, 4)
        self.observe('source', 'color', (1 << COLORS['G']) | (1 << COLORS['W']))
        # Two different subtype witnesses make Coat of Arms add exactly two.
        assert ' Monk' in self.oracle['Monastery Swiftspear']['type_line']
        assert ' Insect' in self.oracle['Kraul Warrior']['type_line']
        self.pay_cast('coat', 'Coat of Arms')
        self.pt(6, 6)
        # This activation can name the animated permanent because it remains a land.
        self.emit('choose_target', 'source', player=self.actor)
        self.activate('land_probe')
        self.resolve()
        self.observe('source', 'tapped', 0)
        self.combat('source', 'blocker', blocked=True)
        self.zone('Rumbling Baloth', self.enemy, 3, 1)
        self.observe('source', 'damage', 0)
        self.emit('assert_life', player=self.actor, value=20)
        self.emit('assert_life', player=self.enemy, value=20)
        self.emit('cleanup')
        self.emit('refresh')
        self.observe('source', 'csvid', self.csvid)
        self.observe('source', 'color', 0)
        self.emit('can_target_creature', 'land_probe', 'source', value=0)
        self.graves(added_enemy=('Rumbling Baloth',))

    def decoy(self):
        self.next_turn()
        self.observe('source', 'sick', 0)
        base, double = self.fees()
        high_available = self.threshold and self.mode != 'short_fee'
        high = high_available and self.mode != 'seven_base'
        paid = double if high else base
        budget = base if self.mode == 'short_fee' else double
        self.mana_budget(budget)
        self.emit('can_activate', 'source', value=1)
        self.offer_mode(high, high_available=high_available)
        self.emit('choose_target', 'target_one', player=self.actor)
        if high:
            self.emit('choose_target', 'target_two', player=self.actor)
        self.activate()
        self.mana('assert_mana', self.actor, [have-due for have,due in zip(budget, paid)])
        self.observe('source', 'tapped', 1)
        self.observe('target_one', 'tapped', 0)
        self.observe('target_two', 'tapped', 0)
        self.resolve()
        self.observe('target_one', 'tapped', 1)
        self.observe('target_two', 'tapped', int(high))
        self.observe('guard', 'tapped', 0)
        self.emit('can_activate', 'source', value=0)
        self.graves()

    def assemble(self):
        self.emit('turn', player=self.actor)
        for seat in (0, 1):
            self.mana('set_mana', seat, ZERO)
        self.emit('refresh')
        self.graves()
        self.libraries()
        if self.csvid == 5979:
            self.emit('play_land', 'source', value=1)
            self.resolve()
            self.observe('source', 'in_play', 1)
        else:
            self.pay_cast('source', self.name)
        effects = {5910: self.scrounger, 5582: self.vermin, 5830: self.restorer, 5979: self.monastery, 5638: self.decoy}
        effects[self.csvid]()



@lru_cache(maxsize=1)
def profile():
    return json.loads(PROFILE.read_text(encoding='utf8'))


def shape(record):
    source = profile()['sources'].get(record['name'], {})
    csvid = source.get('registry', {}).get('csvid')
    if (record.get('layout') != 'normal' or record.get('selected_face') or record.get('card_faces')
            or 'Threshold' not in record.get('keywords', ()) or not exact(source.get('oracle'), record)):
        raise ValueError('whole implemented Oracle Threshold modal record required')
    return dict(source=csvid)


def variants(model):
    csvid = model['source']
    return [f'{mode}_{seat}' for mode in (*COMMON, *EXTRAS[csvid], *EXPANSION[csvid], 'replay')
            for seat in (0,1)]


def spec(declaration, record):
    model = shape(record)
    selected = declaration.get('variants')
    if (declaration.get('family') != FAMILY or type(declaration.get('version')) is not int
            or declaration['version'] != 2 or declaration.get('oracle_text') != record['oracle_text']
            or not exact(declaration.get('params'), model)):
        raise ValueError('whole Oracle and typed Threshold declaration required')
    if (not isinstance(selected, list) or not selected or any(type(v) is not str for v in selected)
            or len(selected) != len(set(selected)) or set(selected) - set(variants(model))):
        raise ValueError('distinct calibrated Threshold mode variants required')
    return dict(model, family=FAMILY)


def validate_witnesses(oracle, registry):
    pins = profile()
    if (pins['witnesses'].keys() != WITNESSES
            or {pin['registry']['csvid'] for pin in pins['sources'].values()} != EXTRAS.keys()):
        raise ValueError('private Threshold witness or source scope changed')
    for name, pin in (pins['sources'] | pins['witnesses']).items():
        row = registry.get(name, {})
        if (not exact(oracle.get(name), pin['oracle']) or not exact(row, pin['registry'])
                or row.get('kind') != 'card' or row.get('handler') in (None, '0','NULL')
                or row.get('handler') == 'card_dummy' and name not in STATIC_WITNESSES):
            raise ValueError('implementation or pinned Oracle changed for ' + name)


def expand(name, csvid, declaration, model, selected, registry, oracle):
    from recipes import step

    validate_witnesses(oracle, registry)
    if model['source'] != csvid or registry[name]['csvid'] != csvid or not selected or set(selected) - set(variants(model)):
        raise ValueError('only exact registered Threshold variants may expand')
    cases = []
    for variant in selected:
        mode, seat = variant.rsplit('_',1)
        board = ModalActivationCase(oracle[name], csvid, int(seat), mode, registry, oracle)
        board.assemble()
        steps = board.steps
        if mode == 'replay':
            steps = [step('capture'), *steps, step('record_state'), step('restore'), *steps, step('assert_recorded_state')]
        cases.append(dict(name=f'ThresholdModalActivationsRecipe_{csvid}_{variant}', card=name,
                          fixture='empty_main', seed=2370, setup=board.setup, steps=steps,
                          _recipe=dict(family=FAMILY, version=2, variant=variant,
                                       params=declaration['params'], oracle_text=declaration['oracle_text'],
                                       untested_components=[])))
    return cases
