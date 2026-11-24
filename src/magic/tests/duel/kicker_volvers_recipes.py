"""Paid dual-kicker Volvers with independent counter and trait receipts."""

from functools import lru_cache
import json
from pathlib import Path
import re

from library_search_recipes import wave6_exact as exact
from recipes import card, cost_from_oracle, step


FAMILY = 'kicker_volvers'
PROFILE = Path(__file__).parent / 'data' / f'{FAMILY}_wave8_profiles.json'
ZONES = {0: 'hand', 1: 'battlefield', 3: 'graveyard'}
WITNESSES = {'Wall of Roots', 'Elvish Mystic', 'Murder', 'Unsummon', 'Counterspell'}


DOUBLE_KICKER = re.compile(
    r'Kicker (\{1\}\{[BUGRW]\}) and/or (\{[BUGRW]\}) '
    r'\(You may pay an additional \1 and/or \2 as you cast this spell\.\)'
)
GRANT = re.compile(
    r'If this creature was kicked with its (\{1\}\{[BUGRW]\}|\{[BUGRW]\}) kicker, '
    r'it enters with (two \+1/\+1 counters|a \+1/\+1 counter) on it and with (.+)'
)
TRAITS = {
    'flying.': 'flying', 'first strike.': 'first_strike', 'trample.': 'trample',
    '"Pay 3 life: Regenerate this creature."': 'regenerate',
    '"Whenever this creature deals damage, you gain that much life."': 'damage_life',
}
EXPECTED_IDS = {5331, 5344, 5354, 5408, 5427}
CHOICES = {'decline': (False, False), 'first': (True, False),
           'second': (False, True), 'both': (True, True)}
NEW_OPTIONS = {'short_generic': 'decline', 'wrong_color': 'second', 'countered': 'both',
               'replay': 'both', 'bounce_recast': 'both'}


@lru_cache(maxsize=1)
def profiles():
    return json.loads(PROFILE.read_bytes())


def parse_grants(record):
    first_line, *rules = record['oracle_text'].splitlines()
    fee_match = DOUBLE_KICKER.fullmatch(first_line)
    if not fee_match or len(rules) != 2 or record['type_line'] != 'Creature — Volver':
        raise ValueError('Exact named dual-kicker creature with both grants required')
    result = []
    for ordinal, line in enumerate(rules):
        grant = GRANT.fullmatch(line)
        if not grant or grant[1] != fee_match[ordinal + 1] or grant[3] not in TRAITS:
            raise ValueError(f'Unsupported or mismatched Volver grant: {line}')
        expected_counts = ('two +1/+1 counters', 'a +1/+1 counter')
        if grant[2] != expected_counts[ordinal]:
            raise ValueError('The printed counter grants must be two and one')
        result.append(dict(cost=cost_from_oracle({'mana_cost': grant[1]}),
                           counters=2 - ordinal, trait=TRAITS[grant[3]]))
    expected_keywords = {'Kicker'} | ({'Regenerate'} if any(v['trait'] == 'regenerate' for v in result) else set())
    if (set(record['keywords']) != expected_keywords or record['layout'] != 'normal'
            or record.get('card_faces') or record.get('selected_face')):
        raise ValueError('Whole ordinary Volver record required')
    return result


def classify(record):
    return dict(price=cost_from_oracle(record), kickers=parse_grants(record),
                pt=[int(record[k]) for k in ('power', 'toughness')],
                clauses=record['oracle_text'].splitlines())


def variants(model):
    return [f'{mode}_{actor}' for mode in (*CHOICES, *NEW_OPTIONS) for actor in (0, 1)]


def validate_witnesses(oracle, rows):
    pins = profiles()
    if set(pins) != {'sources', 'witnesses'} or set(pins['witnesses']) != WITNESSES:
        raise ValueError('Complete independent Volver source and witness pins required')
    if {entry['registry']['csvid'] for entry in pins['sources'].values()} != EXPECTED_IDS:
        raise ValueError('Exact reserved Volver sources required')
    for group in pins.values():
        for name, pin in group.items():
            record, row = oracle.get(name, {}), rows.get(name, {})
            if (not exact(pin, {'oracle': record, 'registry': row}) or record.get('layout') != 'normal'
                    or row.get('kind') != 'card' or row.get('handler') in (None, 'NULL', '0', 'card_dummy')):
                raise ValueError(f'Exact Volver source or witness pin changed: {name}')


def spec(declaration, record):
    pin = profiles()['sources'].get((record or {}).get('name'))
    if not pin or not exact(pin['oracle'], record):
        raise ValueError('Complete pinned Volver record required')
    model = classify(record)
    picked = declaration.get('variants')
    if (declaration.get('family') != FAMILY or type(declaration.get('version')) is not int
            or declaration['version'] != 2 or declaration.get('oracle_text') != record['oracle_text']
            or not exact(declaration.get('params'), model) or not isinstance(picked, list) or not picked
            or any(type(v) is not str for v in picked) or len(picked) != len(set(picked))
            or set(picked) - set(variants(model))):
        raise ValueError('Exact Volver fees, traits and distinct scoped variants required')
    return dict(model, family=FAMILY)


class VolverBoard:
    def __init__(self, record, csvid, actor, mode, rows, oracle):
        self.source = record
        self.csvid, self.actor, self.enemy, self.mode = csvid, actor, 1 - actor, mode
        self.rows, self.oracle = rows, oracle
        self.setup, self.steps = [], []
        self.grants = parse_grants(record)
        self.pending = None
        self.apply_option(NEW_OPTIONS.get(mode, mode))
        self.defender_life = 20
        self.actor_life = 20
        self.alive = True
        self.blocker = 'Wall of Roots' if 'flying' in (g['trait'] for g in self.grants) else 'Elvish Mystic'
        self.reply = {'countered': 'Counterspell', 'bounce_recast': 'Unsummon'}.get(mode)
        # Degavolver's black-only case isolates the life cost. Its other modes
        # expose first strike in real combat while the source can attack.
        self.do_combat = mode != 'countered' and (csvid != 5354 or mode != 'first')
        self.do_regen = mode != 'countered' and (
            record['name'] == 'Anavolver' or csvid == 5354 and mode != 'bounce_recast' and 'regenerate' in self.traits
        )
        self.add('source', actor, record['name'], 0)
        if self.do_combat:
            self.add('blocker', self.enemy, self.blocker)
        if self.do_regen:
            self.add('murder', self.enemy, 'Murder', 0)
        if self.reply:
            self.add('reply', self.enemy, self.reply, 0)

    def apply_option(self, option):
        self.option, self.paid = option, CHOICES[option]
        self.traits = {g['trait'] for g, enabled in zip(self.grants, self.paid) if enabled}
        self.counters = sum(g['counters'] for g, enabled in zip(self.grants, self.paid) if enabled)
        self.power = int(self.source['power']) + self.counters
        self.toughness = int(self.source['toughness']) + self.counters

    def add(self, ref, actor, name, zone=1):
        self.setup.append(card(name, ref, actor, ZONES[zone]))

    def emit(self, op, ref='', other='', property_name='', **fields):
        if ref:
            fields['ref'] = ref
        if other:
            fields['other'] = other
        if property_name:
            fields['property'] = property_name
        self.steps.append(step(op, **fields))

    def observe(self, ref, property_name, value):
        self.emit('assert_card', ref, property_name=property_name, value=value)

    def mana(self, op, actor, pool):
        for color, value in enumerate(pool):
            self.emit(op, player=actor, value=value, color=color)

    def zone(self, name, actor, zone, count):
        self.emit('assert_zone_multiplicity', player=actor, zone=ZONES[zone], value=count,
                  index=self.rows[name]['csvid'])

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

    def payment_plan(self, *, recast):
        fees = [g['cost'] for g in self.grants]
        colors = [next(i for i, amount in enumerate(fee) if i and amount) for fee in fees]
        menu = [x for x, _ in sorted(zip(('first', 'second'), colors), key=lambda item: item[1])] + ['both', 'decline']
        offered = [sum(parts) for parts in zip(*fees)]
        if recast:
            offered, menu = [0] * 6, ['decline']
        elif self.mode == 'short_generic':
            # The first color is present, but it cannot also pay the generic mana.
            offered, menu = fees[0].copy(), ['decline']
            offered[0] = 0
        elif self.mode == 'wrong_color':
            # One generic plus one wrong color remains after the valid second fee.
            substitute = next(i for i in range(1, 6) if i not in colors)
            offered[colors[0]], offered[substitute], menu = 0, 1, ['second', 'decline']
        paid = [sum(value for value, enabled in zip(parts, self.paid) if enabled) for parts in zip(*fees)]
        return offered, [have - due for have, due in zip(offered, paid)], menu.index(self.option)

    def cast_volver(self, *, recast=False):
        base = cost_from_oracle(self.source)
        offered, reserve, menu_index = self.payment_plan(recast=recast)
        funding = [price + extra for price, extra in zip(base, offered)]
        self.emit('turn', player=self.actor)
        self.mana('set_mana', self.actor, funding)
        self.mana('set_mana', self.enemy, [0] * 6)
        self.emit('refresh')
        self.emit('present_ai_dialogs', value=1)
        self.emit('choose_dialog', player=self.actor, value=menu_index)
        self.emit('cast', 'source', value=1)
        self.emit('assert_choices_consumed')
        self.emit('present_ai_dialogs', value=0)
        self.emit('assert_stack', value=1)
        self.observe('source', 'in_play', 0)
        self.zone(self.source['name'], self.actor, 1, 0)
        self.mana('assert_mana', self.actor, reserve)
        self.mana('assert_mana', self.enemy, [0] * 6)
        self.pending = len(self.steps)
        return reserve

    def resolve_entry(self, reserve):
        self.emit('resolve')
        self.emit('refresh')
        self.emit('assert_choices_consumed')
        self.emit('assert_stack', value=0)
        self.zone(self.source['name'], self.actor, 0, 0)
        self.zone(self.source['name'], self.actor, 1, 1)
        self.zone(self.source['name'], self.actor, 3, 0)
        self.observe('source', 'in_play', 1)
        self.observe('source', 'power', self.power)
        self.observe('source', 'toughness', self.toughness)
        self.observe('source', 'plus1_counters', self.counters)
        self.mana('assert_mana', self.actor, reserve)
        self.mana('assert_mana', self.enemy, [0] * 6)

    def cast_reply(self, *, response):
        if response:
            self.emit('announce_spell', 'source')
        self.mana('set_mana', self.enemy, cost_from_oracle(self.oracle[self.reply]))
        self.emit('refresh')
        if not response:
            self.emit('choose_target', 'source', player=self.enemy)
        # Counterspell selects the sole announced pending spell automatically.
        self.emit('cast_response' if response else 'cast', 'reply', value=1)
        self.emit('assert_choices_consumed')
        self.emit('assert_stack', value=2 if response else 1)
        self.mana('assert_mana', self.enemy, [0] * 6)
        self.emit('resolve_response' if response else 'resolve')
        self.emit('refresh')
        self.emit('assert_choices_consumed')
        self.emit('assert_stack', value=0)
        self.zone(self.reply, self.enemy, 3, 1)
        self.zone(self.source['name'], self.actor, 1, 0)
        self.zone(self.source['name'], self.actor, 0, int(not response))
        self.zone(self.source['name'], self.actor, 3, int(response))

    def bounce_and_recast(self):
        self.cast_reply(response=False)
        self.emit('bind_hand', 'source', player=self.actor, value=self.csvid)
        self.apply_option('decline')
        reserve = self.cast_volver(recast=True)
        self.resolve_entry(reserve)
        return reserve

    def check_life(self):
        self.emit('assert_life', player=self.actor, value=self.actor_life)
        self.emit('assert_life', player=self.enemy, value=self.defender_life)

    def check_combat(self, reserve):
        if any(reserve):
            # The exact receipts have established that declined mana survived
            # payment. Clear it before the guarded untap driver.
            self.mana('set_mana', self.actor, [0] * 6)
            self.emit('refresh')
        for seat in (self.enemy, self.actor):
            self.emit('advance_turn')
            self.emit('untap_step', player=seat)
            self.emit('turn', player=seat)

        blocked = 'flying' not in self.traits
        self.combat('source', 'blocker', blocked=blocked)
        other = self.oracle[self.blocker]
        output = self.power if not blocked else 0
        if blocked and 'trample' in self.traits:
            output = max(0, self.power - int(other['toughness']))
        if blocked:
            dies = self.power >= int(other['toughness'])
            incoming = 0 if dies and 'first_strike' in self.traits else int(other['power'])
            self.alive = incoming < self.toughness
            self.zone(self.blocker, self.enemy, 3, int(dies))
            self.zone(self.blocker, self.enemy, 1, int(not dies))
            if not dies:
                self.observe('blocker', 'damage', self.power - output)
            if self.alive:
                self.observe('source', 'damage', incoming)
        self.zone(self.source['name'], self.actor, 1, int(self.alive))
        self.zone(self.source['name'], self.actor, 3, int(not self.alive))
        self.defender_life -= output
        if 'damage_life' in self.traits:
            self.actor_life += self.power
        self.check_life()

    def check_regeneration(self):
        assert self.alive
        regenerates = 'regenerate' in self.traits
        self.mana('set_mana', self.enemy, cost_from_oracle(self.oracle['Murder']))
        self.emit('refresh')
        self.emit('choose_target', 'source', player=self.enemy)
        self.emit('cast', 'murder', value=1)
        self.emit('assert_choices_consumed')
        self.emit('assert_stack', value=1)
        self.mana('assert_mana', self.enemy, [0] * 6)
        self.emit('choose_regeneration', 'source')
        self.emit('resolve')
        self.emit('assert_unused_choices', value=int(not regenerates))
        self.emit('refresh')
        self.emit('assert_choices_consumed')
        self.emit('assert_stack', value=0)
        self.zone('Murder', self.enemy, 3, 1)
        self.zone(self.source['name'], self.actor, 1, int(regenerates))
        self.zone(self.source['name'], self.actor, 3, int(not regenerates))
        if regenerates:
            self.actor_life -= 3
            self.observe('source', 'tapped', 1)
            self.observe('source', 'damage', 0)
            self.observe('source', 'plus1_counters', self.counters)
        self.check_life()

    def assemble(self):
        reserve = self.cast_volver()
        if self.mode == 'countered':
            self.cast_reply(response=True)
            self.mana('assert_mana', self.actor, reserve)
            self.check_life()
            return
        self.resolve_entry(reserve)
        if self.mode == 'bounce_recast':
            reserve = self.bounce_and_recast()
        if self.do_combat:
            self.check_combat(reserve)
        if self.do_regen:
            self.check_regeneration()
        self.check_life()


def expand(name, csvid, declaration, model, selected, rows, oracle):
    validate_witnesses(oracle, rows)
    if rows[name]['csvid'] != csvid or csvid not in EXPECTED_IDS or set(selected) - set(variants(model)):
        raise ValueError('Exact reserved Volver source and variants required')
    cases = []
    for variant in selected:
        mode, actor = variant.rsplit('_', 1)
        board = VolverBoard(oracle[name], csvid, int(actor), mode, rows, oracle)
        board.assemble()
        steps = board.steps
        if mode == 'replay':
            start = board.pending
            suffix = steps[start:]
            steps = steps[:start] + [step('capture')] + suffix + [
                step('record_state'), step('restore'), step('ai_roundtrip')
            ] + suffix + [step('assert_recorded_state')]
        metadata = dict(family=FAMILY, version=2, variant=variant, params=declaration['params'],
                        oracle_text=declaration['oracle_text'], covered_clauses=model['clauses'],
                        untested_components=[])
        cases.append(dict(name=f'KickerVolversRecipe_{csvid}_{variant}', card=name, fixture='empty_main',
                          seed=2334, setup=board.setup, steps=steps, _recipe=metadata))
    return cases

