"""Kicked creature entry from paid mana, paid life, or actual creature return."""

from functools import lru_cache
import json
from pathlib import Path
import re

from counter_condition_recurrence_recipes import exact


FAMILY = 'kicker_entry_counters'
PROFILE = Path(__file__).parent / 'data/kicker_entry_counters_wave8_profiles.json'
MODES = ('paid', 'declined', 'kicker_short', 'base_short', 'countered', 'blink', 'replay')
WITNESSES = {'Grizzly Bears', 'Hill Giant', 'Island', 'Plains', 'Counterspell', 'Cloudshift'}
KICKER = re.compile(r'Kicker ((?:\{(?:\d+|[BUGRW])\})+) \(You may pay an additional \1 as you cast this spell\.\)')


UNBLOCKABLE = "This creature can't be blocked."
KICKER_RETURN = (
    "Kicker—Return a creature you control to its owner's hand. "
    "(You may return a creature you control to its owner's hand in addition to any other costs as you cast this spell.)"
)
KICKER_LIFE = (
    'Kicker—Pay 3 life. (You may pay 3 life in addition to any other costs as you cast this spell.)'
)
VANISHING = (
    'Vanishing 2 (This creature enters with two time counters on it. At the beginning of your upkeep, '
    'remove a time counter from it. When the last is removed, sacrifice it.)'
)
GRUNN = 'Whenever Grunn attacks alone, double its power and toughness until end of turn.'
COUNTER_CLAUSES = {
    'If this creature was kicked, it enters with a +1/+1 counter on it.': 1,
    'If this creature was kicked, it enters with two +1/+1 counters on it.': 2,
    'If this creature was kicked, it enters with three +1/+1 counters on it and with trample.': 3,
    'If Grunn was kicked, it enters with five +1/+1 counters on it.': 5,
}
ZERO = (0,) * 6


def shape(record):
    from recipes import cost_from_oracle

    card_type = record.get('type_line', '').partition(' — ')[0].split()
    if (record.get('layout') != 'normal' or record.get('selected_face') or record.get('card_faces')
            or 'Creature' not in card_type or 'Kicker' not in record.get('keywords', ())
            or any(not re.fullmatch('[1-9][0-9]*', record.get(stat, '')) for stat in ('power', 'toughness'))):
        raise ValueError('normal creature with a complete Kicker clause and fixed stats required')
    kicker, *lines = record['oracle_text'].splitlines()
    mana = KICKER.fullmatch(kicker)
    if mana:
        cost = dict(mana=cost_from_oracle({'mana_cost': mana[1]}), life=0, returned_card=None)
    elif kicker == KICKER_RETURN:
        cost = dict(mana=list(ZERO), life=0, returned_card='Grizzly Bears')
    elif kicker == KICKER_LIFE:
        cost = dict(mana=list(ZERO), life=3, returned_card=None)
    else:
        raise ValueError('Uncalibrated printed kicker cost')

    if lines == [VANISHING, 'If this creature was kicked, it enters with three additional time counters on it.']:
        return dict(cost, counter='time', base=2, extra=3, rider='vanishing')

    if not lines or lines[0] not in COUNTER_CLAUSES:
        raise ValueError('Uncalibrated entry-counter clause')
    count, *riders = lines
    extra = COUNTER_CLAUSES[count]
    if riders == [UNBLOCKABLE]:
        rider = 'unblockable'
    elif riders == [GRUNN]:
        rider = 'attacks_alone'
    elif count.endswith(' and with trample.') and not riders:
        rider = 'trample_when_kicked'
    elif not riders:
        rider = 'none'
    else:
        raise ValueError('Uncalibrated entry-counter rider')
    return dict(cost, counter='plus1', base=0, extra=extra, rider=rider)


class EntryCounterCase:
    def __init__(self, record, actor, mode, rows, oracle, model):
        self.source = record
        self.actor, self.enemy = actor, 1 - actor
        self.mode = mode
        self.paid = mode in ('paid', 'countered', 'blink', 'replay')
        self.mana_left = list(ZERO)
        self.replay_at = None
        self.oracle, self.rows, self.model = oracle, rows, model
        self.setup, self.steps = [], []
        self.add('source', actor, record['name'], 'hand')
        self.power, self.toughness = int(record['power']), int(record['toughness'])
        self.entry_count = model['base'] + model['extra'] * self.paid
        if model['counter'] == 'plus1':
            self.power += self.entry_count
            self.toughness += self.entry_count
        self.initial_life = 2 if model['life'] and mode == 'kicker_short' else 20
        self.own_life = self.initial_life - model['life'] * self.paid
        self.enemy_life = 50 if model['rider'] == 'attacks_alone' else 20
        self.returned = model['returned_card']
        self.blocker = 'Grizzly Bears' if model['rider'] in ('unblockable', 'trample_when_kicked') else None
        if self.returned:
            if mode != 'kicker_short':
                self.add('return_cost', actor, self.returned)
            self.add('enemy_witness', self.enemy, 'Hill Giant')
        if self.blocker:
            self.add('blocker', self.enemy, self.blocker)
        if model['rider'] == 'attacks_alone':
            self.add('other_attacker', actor, 'Grizzly Bears')
        if mode == 'countered':
            self.add('counter', self.enemy, 'Counterspell', 'hand')
        if mode == 'blink':
            self.add('blink_spell', self.actor, 'Cloudshift', 'hand')
        for seat in (0, 1):
            for index, name in enumerate(('Island', 'Plains')):
                self.add(f'deck_{seat}_{index}', seat, name, 'library')

    def observe_counter(self, count):
        self.emit('assert_counter', 'source', property_name=self.model['counter'], value=count)

    def life(self):
        self.emit('assert_life', player=self.actor, value=self.own_life)
        self.emit('assert_life', player=self.enemy, value=self.enemy_life)

    def observe_pt(self, power, toughness):
        self.observe('source', 'power', power)
        self.observe('source', 'toughness', toughness)

    def add(self, ref, player, name, zone='battlefield'):
        from recipes import card

        self.setup.append(card(name, ref, player, zone))

    def emit(self, op, ref='', other='', property_name='', **fields):
        from recipes import step

        if ref:
            fields['ref'] = ref
        if other:
            fields['other'] = other
        if property_name:
            fields['counter_kind' if op == 'assert_counter' else 'property'] = property_name
        self.steps.append(step(op, **fields))

    def observe(self, ref, property_name, value):
        self.emit('assert_card', ref, property_name=property_name, value=value)

    def mana(self, op, actor, pool):
        for color, value in enumerate(pool):
            self.emit(op, player=actor, value=value, color=color)

    def zone(self, name, actor, zone, count):
        self.emit('assert_zone_multiplicity', player=actor, zone=zone, index=self.rows[name]['csvid'], value=count)

    def receipt(self):
        self.mana('assert_mana', self.actor, self.mana_left)
        self.mana('assert_mana', self.enemy, ZERO)
        self.life()
        if self.returned:
            self.zone(self.returned, self.actor, 'hand', int(self.paid))
            self.zone(self.returned, self.actor, 'battlefield', int(not self.paid and self.mode != 'kicker_short'))
            self.zone('Hill Giant', self.enemy, 'battlefield', 1)
            self.zone('Hill Giant', self.enemy, 'hand', 0)

    def prepare_and_cast(self):
        from recipes import cost_from_oracle

        base = cost_from_oracle(self.source)
        extra = self.model['mana'].copy()
        mode = self.mode
        if mode == 'kicker_short' and any(extra):
            missing_color = next(color for color, amount in enumerate(extra) if amount)
            extra[missing_color] -= 1
        funding = [mana + add for mana, add in zip(base, extra)]
        if mode == 'base_short':
            funding = list(ZERO)
        else:
            self.mana_left = list(ZERO) if self.paid else extra

        self.emit('turn', player=self.actor)
        if self.enemy_life != 20:
            self.emit('set_life', player=self.enemy, value=self.enemy_life)
        if self.initial_life != 20:
            self.emit('set_life', player=self.actor, value=self.initial_life)
        self.mana('set_mana', self.actor, funding)
        self.mana('set_mana', self.enemy, ZERO)
        self.emit('refresh')
        if mode == 'replay':
            self.replay_at = len(self.steps)
        if mode not in ('base_short', 'kicker_short'):
            self.emit('choose_dialog', player=self.actor, value=int(not self.paid))
            if self.paid and self.returned:
                self.emit('choose_target', 'return_cost', player=self.actor)
        self.emit('cast', 'source', value=int(mode != 'base_short'))
        self.emit('assert_choices_consumed')
        self.emit('assert_stack', value=int(mode != 'base_short'))
        self.observe('source', 'in_play', 0)
        self.receipt()
        if mode == 'base_short':
            self.zone(self.source['name'], self.actor, 'hand', 1)
            self.zone(self.source['name'], self.actor, 'battlefield', 0)
            self.zone(self.source['name'], self.actor, 'graveyard', 0)

    def enter(self):
        self.emit('resolve')
        self.emit('refresh')
        self.emit('assert_stack', value=0)
        self.emit('assert_choices_consumed')
        self.zone(self.source['name'], self.actor, 'hand', 0)
        self.zone(self.source['name'], self.actor, 'battlefield', 1)
        self.zone(self.source['name'], self.actor, 'graveyard', 0)
        self.observe('source', 'in_play', 1)
        self.observe_counter(self.entry_count)
        self.observe_pt(self.power, self.toughness)
        self.receipt()

    def counter(self):
        from recipes import cost_from_oracle

        self.emit('announce_spell', 'source')
        self.mana('set_mana', self.enemy, cost_from_oracle(self.oracle['Counterspell']))
        self.emit('refresh')
        self.emit('cast_response', 'counter', value=1)
        self.emit('assert_choices_consumed')
        self.emit('assert_stack', value=2)
        self.mana('assert_mana', self.enemy, ZERO)
        self.emit('resolve_response')
        self.emit('refresh')
        self.emit('assert_stack', value=0)
        self.emit('assert_choices_consumed')
        self.zone(self.source['name'], self.actor, 'hand', 0)
        self.zone(self.source['name'], self.actor, 'battlefield', 0)
        self.zone(self.source['name'], self.actor, 'graveyard', 1)
        self.zone('Counterspell', self.enemy, 'graveyard', 1)
        self.receipt()

    def blink(self):
        from recipes import cost_from_oracle

        self.mana('set_mana', self.actor, cost_from_oracle(self.oracle['Cloudshift']))
        self.emit('refresh')
        self.emit('choose_target', 'source', player=self.actor)
        self.emit('cast', 'blink_spell', value=1)
        self.emit('assert_choices_consumed')
        self.emit('assert_stack', value=1)
        self.mana('assert_mana', self.actor, ZERO)
        self.emit('resolve')
        self.emit('refresh')
        self.emit('assert_choices_consumed')
        self.emit('assert_stack', value=0)
        self.emit('bind_permanent_copy', 'source', player=self.actor, index=0,
                  value=self.rows[self.source['name']]['csvid'])
        self.observe('source', 'in_play', 1)
        self.observe_counter(self.model['base'])
        self.observe_pt(int(self.source['power']), int(self.source['toughness']))
        self.zone(self.source['name'], self.actor, 'battlefield', 1)
        self.zone(self.source['name'], self.actor, 'exile', 0)
        self.zone('Cloudshift', self.actor, 'graveyard', 1)
        self.receipt()

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

    def next_owner_turn(self, *, own_upkeep=False):
        if any(self.mana_left):
            # The prior receipts measured the actual unused kicker pool.
            self.mana('set_mana', self.actor, ZERO)
            self.mana_left = list(ZERO)
            self.emit('refresh')
        self.emit('advance_turn')
        self.emit('untap_step', player=self.enemy)
        if own_upkeep:
            self.emit('upkeep_step', player=self.enemy)
        self.emit('advance_turn')
        self.emit('untap_step', player=self.actor)
        if own_upkeep:
            self.emit('upkeep_step', player=self.actor)
        self.emit('turn', player=self.actor)
        self.emit('assert_stack', value=0)
        self.emit('assert_choices_consumed')

    def vanishing(self):
        for remaining in range(self.entry_count - 1, -1, -1):
            self.next_owner_turn(own_upkeep=True)
            self.zone(self.source['name'], self.actor, 'battlefield', int(remaining > 0))
            self.zone(self.source['name'], self.actor, 'graveyard', int(remaining == 0))
            if remaining:
                self.observe_counter(remaining)
                self.observe_pt(self.power, self.toughness)

    def combat_rider(self):
        self.next_owner_turn()
        blocked = self.model['rider'] == 'trample_when_kicked'
        self.combat('source', 'blocker', blocked=blocked)
        if blocked:
            blocker = self.oracle[self.blocker]
            self.enemy_life -= max(0, self.power - int(blocker['toughness'])) if self.paid else 0
            survives = self.toughness > int(blocker['power'])
            self.zone(self.source['name'], self.actor, 'battlefield', int(survives))
            self.zone(self.source['name'], self.actor, 'graveyard', int(not survives))
            self.zone(self.blocker, self.enemy, 'battlefield', 0)
            self.zone(self.blocker, self.enemy, 'graveyard', 1)
            if survives:
                self.observe('source', 'damage', int(blocker['power']))
                self.observe_counter(self.entry_count)
        else:
            self.enemy_life -= self.power
            self.observe('blocker', 'damage', 0)
            self.zone(self.blocker, self.enemy, 'battlefield', 1)
        self.life()

    def grunn_combat(self):
        for with_company in (False, True):
            self.next_owner_turn()
            self.emit('begin_attackers')
            self.emit('attack', 'source', value=1)
            if with_company:
                self.emit('attack', 'other_attacker', value=1)
            self.emit('finish_attackers')
            self.emit('resolve')
            self.emit('refresh')
            multiplier = 1 if with_company else 2
            self.observe_pt(self.power * multiplier, self.toughness * multiplier)
            self.observe_counter(self.entry_count)
            self.emit('begin_blockers')
            self.emit('advance_combat')
            self.emit('combat_damage')
            self.emit('resolve')
            self.emit('end_combat')
            self.enemy_life -= self.power * multiplier
            if with_company:
                self.enemy_life -= int(self.oracle['Grizzly Bears']['power'])
            self.life()
            self.emit('assert_choices_consumed')
            self.emit('cleanup')
            self.emit('refresh')
            self.observe_pt(self.power, self.toughness)
            self.observe_counter(self.entry_count)

    def run(self):
        self.prepare_and_cast()
        if self.mode == 'base_short':
            return
        if self.mode == 'countered':
            self.counter()
            return
        self.enter()
        if self.mode == 'blink':
            self.blink()
        elif self.mode in ('paid', 'declined'):
            rider = self.model['rider']
            if rider == 'vanishing':
                self.vanishing()
            elif rider in ('unblockable', 'trample_when_kicked'):
                self.combat_rider()
            elif rider == 'attacks_alone':
                self.grunn_combat()


@lru_cache(maxsize=1)
def profile():
    return json.loads(PROFILE.read_text(encoding='utf8'))


def variants(model):
    return [f'{mode}_{seat}' for mode in MODES for seat in (0, 1)]


def spec(declaration, record):
    model = shape(record)
    selected = declaration.get('variants')
    pin = profile()['sources'].get(record['name'], {})
    if (declaration.get('family') != FAMILY or type(declaration.get('version')) is not int
            or declaration['version'] != 2 or declaration.get('oracle_text') != record['oracle_text']
            or not exact(pin.get('oracle'), record) or not exact(declaration.get('params'), model)):
        raise ValueError('whole pinned Oracle record and typed kicker declaration required')
    if (not isinstance(selected, list) or not selected or any(type(v) is not str for v in selected)
            or len(selected) != len(set(selected)) or set(selected) - set(variants(model))):
        raise ValueError('distinct calibrated mirrored kicker variants required')
    return dict(family=FAMILY, **model)


def validate_witnesses(oracle, registry):
    pins = profile()
    if pins['witnesses'].keys() != WITNESSES:
        raise ValueError('kicker witness scope changed')
    for name, pin in (pins['sources'] | pins['witnesses']).items():
        row = registry.get(name, {})
        vanilla = name in {'Grizzly Bears', 'Hill Giant'} and pin['oracle']['oracle_text'] == ''
        if (not exact(oracle.get(name), pin['oracle']) or not exact(row, pin['registry'])
                or row.get('kind') != 'card' or row.get('handler') in (None, '0', 'NULL')
                or row.get('handler') == 'card_dummy' and not vanilla):
            raise ValueError('whole implemented Oracle or registry pin changed for ' + name)


def expand(name, csvid, declaration, model, selected, registry, oracle):
    from recipes import step

    validate_witnesses(oracle, registry)
    if (profile()['sources'].get(name, {}).get('registry', {}).get('csvid') != csvid
            or registry[name]['csvid'] != csvid or not selected or set(selected) - set(variants(model))):
        raise ValueError('only exact registered entry-counter kicker variants may expand')
    cases = []
    for variant in selected:
        mode, seat = variant.rsplit('_', 1)
        board = EntryCounterCase(oracle[name], int(seat), mode, registry, oracle, model)
        board.run()
        steps = board.steps
        if board.replay_at is not None:
            prefix, suffix = steps[:board.replay_at], steps[board.replay_at:]
            steps = [*prefix, step('capture'), *suffix, step('record_state'), step('restore'),
                     *suffix, step('assert_recorded_state')]
        cases.append(dict(name=f'KickerEntryCountersRecipe_{csvid}_{variant}', card=name,
                          fixture='empty_main', seed=2334, setup=board.setup, steps=steps,
                          _recipe=dict(family=FAMILY, version=2, variant=variant,
                                       params=declaration['params'], oracle_text=declaration['oracle_text'],
                                       untested_components=[])))
    return cases
