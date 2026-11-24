"""Kicked and ordinary creation from actual paid spells and copied creatures."""

from functools import lru_cache
import json
from pathlib import Path
import re

from counted_battlefield_damage_recipes import Game
from library_search_recipes import wave6_exact as exact


FAMILY = 'kicker_tokens_and_copies'
PROFILE = Path(__file__).parent / 'data' / f'{FAMILY}_wave8_profiles.json'
KICKER = re.compile(r'Kicker ((?:\{(?:\d+|[BUGRW])\})+) \(You may pay an additional \1 as you cast this spell\.\)')
QUANTITY = re.compile(r'Create (two|six) 1/1 (white|green) (Kor Soldier|Saproling) creature tokens\. '
                      r'If this spell was kicked, create (four|twelve) of those tokens instead\.')
WORDS = {'two': 2, 'four': 4, 'six': 6, 'twelve': 12}
COLORS = {'white': 32, 'green': 8}
QUANTITY_TRAITS = {'Kor Soldier': ('white', 'Soldier'), 'Saproling': ('green', 'Saproling')}
ELEMENTAL = ('Create a 7/1 red Elemental creature token with trample and haste. '
             'Exile it at the beginning of the next end step. If this spell was kicked, '
             'that creature gets +7/+0 until end of turn.')
COPY = ("Create a token that's a copy of target creature. If this spell was kicked, create five of those tokens instead.",)
ZOMBIES = ('Menace', 'When Josu Vess enters, if he was kicked, create eight 2/2 black Zombie Knight creature tokens with menace.')
DRAGON = ('Flying', 'When Verix Bladewing enters, if it was kicked, create Karox Bladewing, a legendary '
          '4/4 red Dragon creature token with flying.')
COPY_TARGET = 'Welkin Tern'
PRODUCTS = {'Kor Soldier', 'Saproling', 'Elemental', 'Zombie Knight', 'Karox Bladewing'}
WITNESSES = PRODUCTS | {COPY_TARGET, 'Wall of Roots', 'Counterspell', 'Unsummon'}
PAID_MODES = {'paid', 'countered', 'replay', 'target_bounced'}


@lru_cache(maxsize=1)
def profiles():
    return json.loads(PROFILE.read_bytes())


def product(effect, identity, counts, pt, color, subtypes, **other):
    return dict(effect=effect, identity=identity, counts=list(counts), pt=list(pt),
                color=color, subtypes=list(subtypes), **other)


def classify(record):
    from recipes import cost_from_oracle

    lines = record['oracle_text'].splitlines()
    if not lines or not (fee := KICKER.fullmatch(lines[0])):
        raise ValueError('One fixed printed kicker cost is required')
    body = tuple(lines[1:])
    if len(body) == 1 and (quantity := QUANTITY.fullmatch(body[0])):
        basic, color, identity, increased = quantity.groups()
        expected_color, subtype = QUANTITY_TRAITS[identity]
        if color != expected_color:
            raise ValueError('Quantity product has its own printed color')
        model = product('quantity', identity, (WORDS[basic], WORDS[increased]), (1, 1), COLORS[color], (subtype,))
    elif body == (ELEMENTAL,):
        model = product('elemental', 'Elemental', (1, 1), (7, 1), 16, ('Elemental',), kicked_power=7)
    elif body == COPY:
        model = product('copy', COPY_TARGET, (1, 5), (), None, ())
    elif body == ZOMBIES:
        model = product('menace_entry', 'Zombie Knight', (0, 8), (2, 2), 2, ('Zombie', 'Knight'))
    elif body == DRAGON:
        model = product('flying_entry', 'Karox Bladewing', (0, 1), (4, 4), 16, ('Dragon',))
    else:
        raise ValueError('Outside exact kicked token and copy clauses')

    keyword = {'menace_entry': 'Menace', 'flying_entry': 'Flying'}.get(model['effect'])
    expected_keywords = {'Kicker'} | ({keyword} if keyword else set())
    is_creature = 'Creature' in record['type_line']
    if (record.get('layout') != 'normal' or record.get('card_faces') or record.get('selected_face')
            or set(record['keywords']) != expected_keywords or is_creature != bool(keyword)
            or not is_creature and record['type_line'] != 'Sorcery'):
        raise ValueError('Exact whole creature entry or sorcery required')
    return dict(model, price=cost_from_oracle(record), fee=cost_from_oracle({'mana_cost': fee[1]}),
                clauses=lines, source_pt=[int(record[k]) for k in ('power', 'toughness')] if is_creature else [])


def variants(model):
    modes = ('paid', 'declined', 'short', 'countered', 'replay')
    if model['effect'] == 'copy':
        modes += ('target_bounced',)
    return [f'{mode}_{owner}' for mode in modes for owner in (0, 1)]


def validate_witnesses(oracle, rows):
    pins = profiles()
    if set(pins) != {'sources', 'witnesses'} or set(pins['witnesses']) != WITNESSES:
        raise ValueError('Complete independent kicker source and witness pins required')
    for group in pins.values():
        for name, pin in group.items():
            record, row = oracle.get(name, {}), rows.get(name, {})
            expected_layout = 'token' if name in PRODUCTS else 'normal'
            if (not exact(pin, {'oracle': record, 'registry': row}) or record.get('layout') != expected_layout
                    or row.get('kind') != 'card' or (name != 'Kor Soldier' and not implemented(row))):
                raise ValueError(f'Exact kicker source or witness pin changed: {name}')


def implemented(row):
    return row.get('handler') not in ('NULL', '0', 'card_dummy', None)


def spec(declaration, record):
    pin = profiles()['sources'].get((record or {}).get('name'))
    if not pin or not exact(pin['oracle'], record):
        raise ValueError('Complete pinned kicker source Oracle required')
    model = classify(record)
    picked = declaration.get('variants')
    if (declaration.get('family') != FAMILY or type(declaration.get('version')) is not int
            or declaration['version'] != 2 or declaration.get('oracle_text') != record['oracle_text']
            or not exact(declaration.get('params'), model) or not isinstance(picked, list) or not picked
            or any(type(v) is not str for v in picked) or len(picked) != len(set(picked))
            or set(picked) - set(variants(model))):
        raise ValueError('Exact kicker source and supported distinct variants required')
    return dict(model, family=FAMILY)


class KickerProductBoard(Game):
    def __init__(self, name, shape, mode, owner, rows, oracle):
        self.pending = None
        self.enemy_life = 20
        self.paid = mode in PAID_MODES
        self.effect = shape['effect']
        self.identity = shape['identity']
        self.reply = dict(countered='Counterspell', target_bounced='Unsummon').get(mode)
        super().__init__(name, shape, mode, owner, rows, oracle)

    def build(self):
        if self.effect in ('elemental', 'copy', 'menace_entry', 'flying_entry'):
            self.add('Wall of Roots', 'wall', self.foe)
        if self.effect == 'copy':
            self.add(self.identity, 'copy_target', self.foe)
        if self.reply:
            self.add(self.reply, 'reply', self.foe, 'hand')

    def property(self, ref, prop, value):
        self.emit('assert_card', ref=ref, property=prop, value=value)

    def zone(self, name, seat, zone, count):
        self.emit('assert_zone_multiplicity', player=seat, zone=zone, index=self.rows[name]['csvid'], value=count)

    def pay_source(self):
        base, fee = self.shape['price'], self.shape['fee']
        reserve = fee.copy()
        if self.mode == 'short':
            assert reserve[0] >= 1
            reserve[0] -= 1
        self.emit('turn', player=self.owner)
        self.fund(self.foe, [0] * 6)
        self.fund(self.owner, [value + extra for value, extra in zip(base, reserve)])
        if self.effect == 'copy':
            # Rite's actual target chooser precedes its additional payment dialog.
            self.emit('choose_target', player=self.owner, ref='copy_target')
        if self.mode != 'short':
            self.emit('choose_dialog', player=self.owner, value=int(not self.paid))
        self.emit('cast', ref='source', value=1)
        self.emit('assert_choices_consumed')
        self.emit('assert_stack', value=1)
        self.property('source', 'in_play', 0)
        self.zone(self.identity, self.owner, 'battlefield', 0)
        self.pools[self.owner] = [0] * 6 if self.paid else reserve
        self.observe_mana()
        self.pending = len(self.steps)

    def resolve_source(self):
        if self.reply:
            self.emit('announce_spell', ref='source')
            # Counterspell automatically selects the single pending source. Unsummon accepts a named creature.
            self.cast('reply', target='copy_target' if self.mode == 'target_bounced' else None, response=True)
            self.emit('resolve_response')
            self.zone(self.reply, self.foe, 'graveyard', 1)
            self.zone(self.reply, self.foe, 'hand', 0)
        else:
            self.emit('resolve')
        self.emit('refresh')
        self.emit('assert_choices_consumed')
        self.emit('assert_stack', value=0)
        name = self.cards['source']['name']
        creature = bool(self.shape['source_pt'] and not self.reply)
        self.zone(name, self.owner, 'hand', 0)
        self.zone(name, self.owner, 'battlefield', int(creature))
        self.zone(name, self.owner, 'graveyard', int(not creature))
        self.property('source', 'in_play', int(creature))
        if creature:
            for prop, amount in zip(('power', 'toughness'), self.shape['source_pt']):
                self.property('source', prop, amount)
        self.observe_mana()

    def check_product(self):
        count = self.shape['counts'][int(self.paid)] if not self.reply else 0
        bounced = self.mode == 'target_bounced'
        if self.effect == 'copy':
            self.zone(self.identity, self.foe, 'battlefield', int(not bounced))
            self.zone(self.identity, self.foe, 'hand', int(bounced))
            if not bounced:
                self.property('copy_target', 'in_play', 1)
        else:
            self.zone(self.identity, self.foe, 'battlefield', 0)
        self.zone(self.identity, self.owner, 'battlefield', count)
        for seat in (0, 1):
            self.emit('assert_life', player=seat, value=20)
        pt, color, subtypes = self.shape['pt'], self.shape['color'], self.shape['subtypes']
        if self.effect == 'copy':
            record = self.oracle[self.identity]
            if record['colors'] != ['U'] or record['keywords'] != ['Flying'] or record['type_line'] != 'Creature — Bird':
                raise ValueError('Exact independent flying copy target required')
            pt, color, subtypes = [int(record[k]) for k in ('power', 'toughness')], 4, ('Bird',)
        power, toughness = pt
        if self.paid:
            power += self.shape.get('kicked_power', 0)
        if count:
            csvid = self.rows[self.identity]['csvid']
            props = [('token', 1), ('power', power), ('toughness', toughness), ('color', color), ('artifact', 0)]
            props += [('subtype_' + subtype, 1) for subtype in subtypes]
            for prop, amount in props:
                self.emit('assert_matching_permanents', player=self.owner, index=csvid, property=prop, value=amount)
            if self.effect != 'quantity':
                self.emit('bind_permanent_copy', ref='token', player=self.owner, index=0, value=csvid)
        return count, power

    def attack(self, ref, *, blocked=False):
        self.emit('begin_attackers')
        self.emit('attack', ref=ref, value=1)
        self.emit('finish_attackers')
        self.emit('begin_blockers')
        self.emit('block', ref='wall', other=ref, value=int(blocked))
        self.emit('advance_combat')
        self.property(ref, 'blocked', int(blocked))
        self.emit('combat_damage')
        self.emit('resolve')
        self.emit('end_combat')
        self.emit('assert_choices_consumed')
        self.emit('assert_stack', value=0)

    def next_combat(self):
        # Exact cost receipts have already established the declined/short reserve.
        # Clear that fixture reserve before the guarded real untap boundary.
        if any(self.pools[self.owner]):
            self.fund(self.owner, [0] * 6)
        for seat in (self.foe, self.owner):
            self.emit('advance_turn')
            self.emit('untap_step', player=seat)
            self.emit('turn', player=seat)

    def check_riders(self, count, power):
        if self.reply or self.effect == 'quantity':
            return
        if self.effect == 'elemental':
            self.attack('token', blocked=True)
            self.zone('Wall of Roots', self.foe, 'graveyard', 1)
            self.zone(self.identity, self.owner, 'battlefield', 1)
            self.emit('assert_life', player=self.foe, value=25 - power)
            self.emit('advance_turn')
            for where in ('battlefield', 'exile', 'graveyard'):
                self.zone(self.identity, self.owner, where, 0)
            return
        self.next_combat()
        if self.effect != 'copy':
            self.attack('source')
            self.enemy_life -= self.shape['source_pt'][0]
            self.emit('assert_life', player=self.foe, value=self.enemy_life)
            if count:
                self.next_combat()
        if count:
            self.attack('token')
            self.enemy_life -= power
            self.emit('assert_life', player=self.foe, value=self.enemy_life)
        if self.effect == 'copy':
            self.property('copy_target', 'in_play', 1)
        self.zone('Wall of Roots', self.foe, 'battlefield', 1)

    def run(self):
        self.pay_source()
        self.resolve_source()
        self.check_riders(*self.check_product())
        self.emit('assert_life', player=self.owner, value=20)


def expand(name, csvid, declaration, shape, selected, rows, oracle):
    from recipes import step

    validate_witnesses(oracle, rows)
    if name not in profiles()['sources'] or rows[name]['csvid'] != csvid or set(selected) - set(variants(shape)):
        raise ValueError('Exact source and kicker product variants required')
    gaps = {
        'Kor Soldier': ['Direct Kor subtype observer; the generated Kor Soldier identity and Soldier subtype are asserted.'],
        'Elemental': ['Combating through the next end-step observes token expiry; ceased tokens cannot retain an exile receipt.'],
        'Karox Bladewing': ['Independent legend-rule contest for Karox; its exact token identity is asserted.'],
    }.get(shape['identity'], [])
    cases = []
    for variant in selected:
        mode, owner = variant.rsplit('_', 1)
        board = KickerProductBoard(name, shape, mode, int(owner), rows, oracle)
        board.run()
        steps = board.steps
        if mode == 'replay':
            start = board.pending
            suffix = steps[start:]
            steps = steps[:start] + [step('capture')] + suffix + [step('record_state'), step('restore'),
                    step('ai_roundtrip')] + suffix + [step('assert_recorded_state')]
        metadata = dict(family=FAMILY, version=2, variant=variant, params=declaration['params'],
                        oracle_text=declaration['oracle_text'], covered_clauses=shape['clauses'],
                        untested_components=gaps)
        cases.append(dict(name=f'KickerTokensAndCopiesRecipe_{csvid}_{variant}', card=name, fixture='empty_main',
                          seed=2334, setup=board.setup, steps=steps, _recipe=metadata))
    return cases
