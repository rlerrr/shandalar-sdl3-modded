"""Actual Delirium activation, cast reductions, and graveyard transitions."""

from functools import lru_cache
import json
from pathlib import Path

from counter_condition_recurrence_recipes import exact
from recipes import cost_from_oracle

FAMILY = 'delirium_activated_and_cast_costs'
PROFILE = Path(__file__).parent / 'data/delirium_activated_and_cast_costs_wave8_profiles.json'
CRYPT = "Tormod's Crypt"
MIXED = ('Mountain', 'Memnite', 'Giant Growth', 'Mountain')
ZONES = ('hand', 'battlefield', 'library', 'graveyard', 'exile')
ACTIVATIONS = {15930, 15967, 16062}
COST_REDUCTIONS = {16084, 16178}
EXTENDED = ('gain_fourth', 'lose_fourth')
STATIC_WITNESSES = {'Grizzly Bears', 'Runeclaw Bear', 'Memnite'}
THREE = ('Mountain', 'Grizzly Bears', 'Giant Growth', 'Mountain')
FOUR = ('Mountain', 'Grizzly Bears', 'Giant Growth', "Traveler's Amulet")
COMMON = ('three_duplicates', 'four_types', 'opponent_only')
EXTRAS = {
    15930: ('no_other',),
    15967: ('unfunded',),
    16062: ('upkeep_crossing',),
    16084: ('full_cost',),
    16178: ('full_cost',),
}
ZERO = [0] * 6
REDUCTION = 'Delirium — This spell costs {2} less to cast if there are four or more card types among cards in your graveyard.'
ACTIVATE = ' Activate only if there are four or more card types among cards in your graveyard.'
ORACLE_TEXT = {
    15930: 'Flying\nDelirium — Sacrifice another creature: This creature gets +2/+1 until end of turn.' + ACTIVATE,
    15967: 'Delirium — {1}{B}: This creature gets +1/+1 until end of turn.' + ACTIVATE,
    16062: ('At the beginning of your upkeep, you may mill a card.\nDelirium — {2}{G}, Sacrifice this enchantment: '
            'Return up to one target creature card and up to one target land card from your graveyard to your hand.' + ACTIVATE),
    16084: REDUCTION + '\nFlying',
    16178: REDUCTION + '\nDraw three cards.',
}


class DeliriumCostCase:
    def __init__(self, record, csvid, actor, mode, rows, oracle):
        self.source, self.name = record, record['name']
        self.csvid, self.actor, self.enemy, self.mode = csvid, actor, 1-actor, mode
        self.rows, self.oracle = rows, oracle
        self.setup, self.steps = [], []
        own = FOUR if mode in ('four_types', 'no_other', 'unfunded', 'lose_fourth', 'countered') else THREE
        if mode == 'mixed_type':
            own = MIXED
        if mode == 'replay' and csvid != 16062:
            own = FOUR
        enemy = FOUR if mode == 'opponent_only' else ()
        self.grave = {actor: list(own), self.enemy: list(enemy)}
        self.library = {actor: ['Forest', 'Plains', 'Island', 'Runeclaw Bear'],
                        self.enemy: ['Plains', 'Island']}
        if mode in ('upkeep_crossing', 'upkeep_decline') or mode == 'replay' and csvid == 16062:
            self.library[actor].insert(0, "Traveler's Amulet")
        self.add('source', actor, self.name, 0)
        for seat in (0, 1):
            for index, name in enumerate(self.grave[seat]):
                self.add(f'grave_{seat}_{index}', seat, name, 3)
            for index, name in enumerate(self.library[seat]):
                self.add(f'deck_{seat}_{index}', seat, name, 2)
        if csvid in (15930, 16084):
            self.add('blocker', self.enemy, 'Grizzly Bears')
        if csvid == 15930 and mode != 'no_other':
            self.add('sacrifice', self.actor, 'Runeclaw Bear')
        if mode == 'gain_fourth':
            self.add('amulet', self.actor, "Traveler's Amulet")
            self.add('disenchant', self.actor, 'Disenchant', 0)
        if mode == 'lose_fourth':
            self.add('crypt', self.actor, CRYPT)
        if mode == 'countered':
            self.add('counter', self.enemy, 'Counterspell', 0)

    def add(self, ref, player, name, zone=1):
        from recipes import card

        self.setup.append(card(name, ref, player, ZONES[zone]))

    def emit(self, op, ref='', other='', property_name='', **fields):
        from recipes import step

        if ref:
            fields['ref'] = ref
        if other:
            fields['other'] = other
        if property_name:
            fields['property'] = property_name
        if 'zone' in fields:
            fields['zone'] = ZONES[fields['zone']]
        self.steps.append(step(op, **fields))

    def observe(self, ref, property_name, value):
        self.emit('assert_card', ref, property_name=property_name, value=value)

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

    def grave_state(self):
        for seat, cards in self.grave.items():
            self.emit('assert_zone', player=seat, zone=3, value=len(cards))
            for card in sorted(set(FOUR) | set(cards)):
                self.zone(card, seat, 3, cards.count(card))

    def library_state(self):
        for seat, cards in self.library.items():
            self.emit('assert_zone', player=seat, zone=2, value=len(cards))
            for index, card in enumerate(cards):
                self.emit('assert_zone_card', player=seat, zone=2, index=index, value=self.rows[card]['csvid'])

    def pt(self, power=0, toughness=0):
        self.observe('source', 'power', int(self.source['power']) + power)
        self.observe('source', 'toughness', int(self.source['toughness']) + toughness)

    def next_own_turn(self, upkeep=False, accept_mill=False):
        for seat in (self.enemy, self.actor):
            self.emit('advance_turn')
            self.emit('untap_step', player=seat)
            if upkeep and seat == self.actor:
                if seat == 0 and accept_mill:
                    self.emit('choose_trigger_source', 'source', player=seat)
                self.emit('upkeep_step', player=seat)
                self.emit('assert_choices_consumed')
                self.emit('assert_stack')
            self.emit('turn', player=seat)

    def begin_threshold(self):
        name = "Traveler's Amulet"
        self.zone(name, self.actor, 1, 1)
        self.mana('set_mana', self.actor, cost_from_oracle(self.oracle['Disenchant']))
        self.emit('refresh')
        self.emit('choose_target', 'amulet', player=self.actor)
        self.emit('cast', 'disenchant', value=1)
        self.emit('assert_choices_consumed')
        self.emit('assert_stack', value=1)
        self.mana('assert_mana', self.actor, ZERO)
        self.emit('resolve')
        self.emit('refresh')
        self.emit('assert_stack')
        self.emit('assert_choices_consumed')
        self.zone(name, self.actor, 1, 0)
        self.grave[self.actor].extend((name, 'Disenchant'))
        self.grave_state()

    def end_threshold(self):
        self.emit('choose_player', player=self.actor, value=self.actor)
        self.emit('activate', 'crypt', value=1)
        self.emit('assert_choices_consumed')
        self.emit('assert_stack', value=1)
        self.zone(CRYPT, self.actor, 1, 0)
        self.grave[self.actor].append(CRYPT)
        self.grave_state()
        self.emit('resolve')
        self.emit('refresh')
        self.emit('assert_stack')
        self.emit('assert_choices_consumed')
        for name in sorted(set(self.grave[self.actor])):
            self.zone(name, self.actor, 4, self.grave[self.actor].count(name))
        self.grave[self.actor].clear()
        self.grave_state()

    def counter(self):
        self.emit('announce_spell', 'source')
        self.mana('set_mana', self.enemy, cost_from_oracle(self.oracle['Counterspell']))
        self.emit('refresh')
        self.emit('cast_response', 'counter', value=1)
        self.emit('assert_choices_consumed')
        self.emit('assert_stack', value=2)
        self.mana('assert_mana', self.enemy, ZERO)
        self.emit('resolve_response')
        self.emit('refresh')
        self.emit('assert_choices_consumed')
        self.emit('assert_stack')
        self.zone(self.name, self.actor, 0, 0)
        self.zone(self.name, self.actor, 1, 0)
        self.zone('Counterspell', self.enemy, 0, 0)
        self.grave[self.actor].append(self.name)
        self.grave[self.enemy].append('Counterspell')
        self.grave_state()
        self.library_state()

    def cast_source(self):
        base = cost_from_oracle(self.source)
        reduction_spell = self.csvid in (16084, 16178)
        budget = base.copy()
        if reduction_spell and self.mode != 'full_cost':
            budget[0] -= 2
        allowed = not reduction_spell or self.mode in (
            'full_cost', 'four_types', 'mixed_type', 'gain_fourth', 'countered', 'replay')
        self.emit('turn', player=self.actor)
        self.mana('set_mana', self.actor, budget)
        self.mana('set_mana', self.enemy, ZERO)
        self.emit('refresh')
        self.grave_state()
        self.library_state()
        if reduction_spell and self.mode == 'gain_fourth':
            self.emit('cast', 'source', value=0)
            self.emit('assert_stack')
            self.observe('source', 'in_hand', 1)
            self.mana('assert_mana', self.actor, budget)
            self.begin_threshold()
            self.mana('set_mana', self.actor, budget)
            self.emit('refresh')
        if reduction_spell and self.mode == 'lose_fourth':
            self.end_threshold()
        self.emit('cast', 'source', value=int(allowed))
        self.emit('assert_choices_consumed')
        self.emit('assert_stack', value=int(allowed))
        self.observe('source', 'in_play', 0)
        self.observe('source', 'in_hand', int(not allowed))
        self.mana('assert_mana', self.actor, ZERO if allowed else budget)
        self.mana('assert_mana', self.enemy, ZERO)
        if not allowed:
            self.zone(self.name, self.actor, 0, 1)
            self.zone(self.name, self.actor, 1, 0)
            self.zone(self.name, self.actor, 3, 0)
            self.grave_state()
            self.library_state()
            return budget, False
        self.library_state()
        if self.mode == 'countered':
            self.counter()
            return budget, False
        self.emit('resolve')
        self.emit('refresh')
        self.emit('assert_choices_consumed')
        self.emit('assert_stack')
        self.zone(self.name, self.actor, 0, 0)
        self.zone(self.name, self.actor, 1, int(self.csvid != 16178))
        if self.csvid == 16178:
            self.grave[self.actor].append(self.name)
            draw = self.library[self.actor][:3]
            del self.library[self.actor][:3]
            for name in draw:
                self.zone(name, self.actor, 0, 1)
        else:
            self.observe('source', 'in_play', 1)
            if 'power' in self.source:
                self.pt()
        self.grave_state()
        self.library_state()
        return budget, True

    def activation_gate(self, allowed, budget):
        self.mana('set_mana', self.actor, budget)
        self.emit('refresh')
        self.emit('can_activate', 'source', value=int(allowed))

    def activation_commit(self, allowed, budget):
        self.emit('activate', 'source', value=int(allowed))
        self.emit('assert_choices_consumed')
        self.emit('assert_stack', value=int(allowed))
        self.mana('assert_mana', self.actor, ZERO if allowed else budget)
        self.mana('assert_mana', self.enemy, ZERO)

    def activate_pump(self):
        reaper = self.csvid == 15930
        if reaper:
            self.next_own_turn()
        budget = ZERO if reaper or self.mode == 'unfunded' else cost_from_oracle(
            {'mana_cost': self.source['oracle_text'].split('Delirium — ')[1].split(':')[0]})
        if self.mode == 'gain_fourth':
            self.activation_gate(False, budget)
            self.begin_threshold()
        if self.mode == 'lose_fourth':
            self.activation_gate(True, budget)
            self.end_threshold()
        allowed = self.mode in ('four_types', 'gain_fourth', 'replay')
        self.activation_gate(allowed, budget)
        if reaper and allowed and self.actor == 0:
            self.emit('choose_target', 'sacrifice', player=self.actor)
        self.activation_commit(allowed, budget)
        self.pt()
        if reaper:
            if allowed:
                self.grave[self.actor].append('Runeclaw Bear')
            self.zone('Runeclaw Bear', self.actor, 1, int(not allowed and self.mode != 'no_other'))
            self.zone('Runeclaw Bear', self.actor, 3, int(allowed))
            self.zone('Grizzly Bears', self.enemy, 1, 1)
        self.grave_state()
        if not allowed:
            return
        self.emit('resolve')
        self.emit('refresh')
        self.emit('assert_stack')
        self.pt(2 if reaper else 1, 1)
        if reaper:
            self.flying_combat(2)
        self.emit('cleanup')
        self.emit('refresh')
        self.pt()
        self.grave_state()

    def activate_sigil(self):
        budget = cost_from_oracle({'mana_cost': self.source['oracle_text'].split('Delirium — ')[1].split(',')[0]})
        if self.mode in ('upkeep_crossing', 'upkeep_decline', 'replay'):
            self.emit('can_activate', 'source', value=0)
            accepts = self.mode in ('upkeep_crossing', 'replay')
            self.next_own_turn(upkeep=True, accept_mill=accepts)
            if accepts:
                self.grave[self.actor].append(self.library[self.actor].pop(0))
            self.grave_state()
            self.library_state()
            if self.mode == 'replay':
                self.activation_gate(True, budget)
                return
            if not accepts:
                self.activation_gate(False, budget)
                self.activation_commit(False, budget)
                return
        allowed = self.mode in ('four_types', 'upkeep_crossing')
        self.activation_gate(allowed, budget)
        if allowed:
            self.emit('observe_search_filters', value=1)
            # Zero targets is legal for both printed selectors. Record the
            # second selector's actual legal set without assigning an illegal
            # scripted creature or land to a mismatched legacy filter.
            for _ in range(2):
                self.emit('choose_zone', player=self.actor, value=-1)
        self.activation_commit(allowed, budget)
        if not allowed:
            self.observe('source', 'in_play', 1)
            self.grave_state()
            return
        self.grave[self.actor].append(self.name)
        self.emit('assert_offered_selection_required', value=0)
        for name, eligible in (('Mountain', 1), ('Grizzly Bears', 0), ('Giant Growth', 0),
                               ("Traveler's Amulet", 0)):
            self.emit('assert_offered_legal_card', index=self.rows[name]['csvid'], value=eligible)
        self.zone(self.name, self.actor, 1, 0)
        self.grave_state()
        self.emit('resolve')
        self.emit('refresh')
        self.emit('assert_stack')
        self.grave_state()
        self.zone('Mountain', self.actor, 0, 0)
        self.zone('Grizzly Bears', self.actor, 0, 0)

    def flying_combat(self, bonus=0):
        self.combat('source', 'blocker', blocked=False)
        self.emit('assert_life', player=self.enemy, value=20-int(self.source['power'])-bonus)
        self.emit('assert_life', player=self.actor, value=20)
        self.observe('blocker', 'damage', 0)

    def assemble(self):
        budget, allowed = self.cast_source()
        if self.csvid in (15930, 15967):
            self.activate_pump()
        elif self.csvid == 16062:
            self.activate_sigil()
        elif self.csvid == 16084 and allowed:
            self.next_own_turn()
            self.flying_combat()
        return budget



WITNESSES = set(THREE + FOUR + MIXED) | {'Forest', 'Plains', 'Island', 'Runeclaw Bear',
                                      'Disenchant', CRYPT, 'Counterspell'}


@lru_cache(maxsize=1)
def profiles():
    return json.loads(PROFILE.read_text(encoding='utf8'))


def shape(record):
    pin = profiles()['sources'].get(record['name'], {})
    csvid = pin.get('registry', {}).get('csvid')
    if (record.get('layout') != 'normal' or record.get('selected_face') or record.get('card_faces')
            or record.get('oracle_text') != ORACLE_TEXT.get(csvid) or not exact(pin.get('oracle'), record)):
        raise ValueError('whole implemented Oracle Delirium cost record required')
    return dict(source=csvid)


def variants(model):
    source = model['source']
    extra = ()
    if source != 16062:
        extra += EXTENDED
    if source in COST_REDUCTIONS:
        extra += ('mixed_type', 'countered')
    labels = [f'{mode}_{seat}' for mode in (*COMMON, *EXTRAS[source], *extra, 'replay') for seat in (0,1)]
    if source == 16062:
        labels.append('upkeep_decline_0')
    return labels


def spec(declaration, record):
    expected = shape(record)
    selected = declaration.get('variants')
    if (declaration.get('family') != FAMILY or type(declaration.get('version')) is not int
            or declaration['version'] != 2 or declaration.get('oracle_text') != record['oracle_text']
            or not exact(declaration.get('params'), expected)):
        raise ValueError('whole pinned Oracle and typed Delirium model required')
    if (not isinstance(selected, list) or not selected or any(type(v) is not str for v in selected)
            or len(selected) != len(set(selected)) or set(selected) - set(variants(expected))):
        raise ValueError('distinct known Delirium variants required')
    return dict(family=FAMILY, **expected)


def validate_witnesses(oracle, registry):
    pins = profiles()
    if (pins['witnesses'].keys() != WITNESSES
            or {record['registry']['csvid'] for record in pins['sources'].values()} != ORACLE_TEXT.keys()):
        raise ValueError('private Delirium source or witness scope changed')
    for name, pin in (pins['sources'] | pins['witnesses']).items():
        row = registry.get(name, {})
        if (not exact(oracle.get(name), pin['oracle']) or not exact(row, pin['registry'])
                or row.get('kind') != 'card' or row.get('handler') in (None, '0', 'NULL')
                or row.get('handler') == 'card_dummy' and name not in STATIC_WITNESSES):
            raise ValueError('implementation or pinned Oracle changed for ' + name)


def expand(name, csvid, declaration, model, selected, registry, oracle):
    from recipes import step

    validate_witnesses(oracle, registry)
    if (model['source'] != csvid or registry[name]['csvid'] != csvid or not selected
            or set(selected) - set(variants(model))):
        raise ValueError('only exact registered Delirium cost variants may expand')
    gaps = []
    if csvid == 16062:
        gaps.append('Crop Sigil has engine type mismatches for both printed graveyard targets. '
                    'The optional land filter is asserted from the typed selector. The observer stores one selector '
                    'and the strict chooser cannot complete a positive selection the engine rejects.')
    cases = []
    for variant in selected:
        mode, seat = variant.rsplit('_', 1)
        board = DeliriumCostCase(oracle[name], csvid, int(seat), mode, registry, oracle)
        board.assemble()
        steps = board.steps
        if mode == 'replay':
            steps = [step('capture'), *steps, step('record_state'), step('restore'), *steps, step('assert_recorded_state')]
        cases.append(dict(name=f'DeliriumActivatedAndCastCostsRecipe_{csvid}_{variant}', card=name,
                          fixture='empty_main', seed=2370, setup=board.setup, steps=steps,
                          _recipe=dict(family=FAMILY, version=2, variant=variant,
                                       params=declaration['params'], oracle_text=declaration['oracle_text'],
                                       untested_components=gaps)))
    return cases
