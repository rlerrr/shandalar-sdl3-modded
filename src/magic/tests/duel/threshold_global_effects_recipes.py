"""Threshold effects that change boards or resolve into distinct target receipts."""
from copy import deepcopy
from functools import lru_cache
import json
from pathlib import Path

from counter_condition_recurrence_recipes import exact
from recipes import card, cost_from_oracle, step


FAMILY = 'threshold_global_effects'
PROFILE = Path(__file__).with_name('data') / 'threshold_global_effects_wave8_profiles.json'
CRYPT = "Tormod's Crypt"
DENIED = ('short', 'wrong_color', 'countered')
REPLAY = {5793, 5640, 5696}
CLEANSING, DEMORALIZE, WRATH, NUT, COALS = 5793, 5538, 5591, 5640, 5696
THRESHOLD = 'Threshold — If there are seven or more cards in your graveyard, '
CLAUSES = {
    CLEANSING: ('Destroy all enchantments.\n' + THRESHOLD + 'instead destroy all enchantments, '
                'then return all cards in your graveyard destroyed this way to the battlefield.'),
    DEMORALIZE: ("All creatures gain menace until end of turn. (They can't be blocked except by two or more creatures.)\n"
                 + THRESHOLD + "creatures can't block this turn."),
    WRATH: ("Destroy all creatures. They can't be regenerated.\n" + THRESHOLD
            + 'instead destroy all creatures, then create two 1/1 white Spirit creature tokens with flying. '
              "Creatures destroyed this way can't be regenerated."),
    NUT: ('At the beginning of your upkeep, you may create a 1/1 green Squirrel creature token.\n'
          'Threshold — All Squirrels get +2/+2 as long as there are seven or more cards in your graveyard.'),
    COALS: ('Shower of Coals deals 2 damage to each of up to three targets.\n'
            'Threshold — Shower of Coals deals 4 damage to each of those permanents and/or players instead '
            'if there are seven or more cards in your graveyard.'),
}
COMMON = ('six', 'seven', 'opponent_only')
EXTRA = {
    CLEANSING: ('old_enchantment',),
    DEMORALIZE: ('pair', 'offturn_seven', 'expires'),
    WRATH: ('own_death', 'spirits_fly', 'shield'),
    NUT: ('gain_seventh',),
    COALS: ('one_target', 'zero_targets'),
}
START_AT_SEVEN = {'seven', 'old_enchantment', 'offturn_seven', 'spirits_fly', 'shield',
                  'one_target', 'zero_targets', 'decline'}

ZONE = {'hand': 0, 'battlefield': 1, 'library': 2, 'graveyard': 3, 'exile': 4}
ZONES = {value: name for name, value in ZONE.items()}
TOKENS = {'Spirit', 'Squirrel'}
WITNESSES = {'Mountain', 'Island', 'Plains', 'Bad Moon', 'Glorious Anthem', 'Fervor', 'Grizzly Bears',
             "Traveler's Amulet", 'Hill Giant', 'Enormous Baloth', 'Darksteel Myr', 'Drudge Skeletons',
             'Wind Drake', 'Wrap in Vigor', 'Woodland Changeling', 'Shock', 'Giant Spider',
             'Counterspell', CRYPT}
BRANCHES = {CLEANSING: 'cleansing', DEMORALIZE: 'demoralize', WRATH: 'wrath', NUT: 'nut', COALS: 'coals'}
GAPS = {
    DEMORALIZE: ('With Threshold active the prohibition on all blocks masks menace independently.',
                 'The legal two-blocker comparison ends after real declarations.'),
    NUT: ('The decline branch uses the human upkeep choice. Seat 1 exercises the production AI policy.',),
    COALS: ('Planeswalker targets and target loss during a response are outside this recipe.',),
}


@lru_cache(maxsize=1)
def profiles():
    return json.loads(PROFILE.read_bytes())


def classify(record, csvid):
    if (csvid not in CLAUSES or record.get('oracle_text') != CLAUSES[csvid]
            or record.get('layout') != 'normal' or record.get('selected_face') or record.get('card_faces')
            or record.get('keywords') != ['Threshold']):
        raise ValueError('complete normal Threshold primary and printed clauses required')
    expected_type = 'Instant' if csvid == DEMORALIZE else 'Creature — Human Druid' if csvid == NUT else 'Sorcery'
    if (record.get('type_line') != expected_type
            or csvid == NUT and (record.get('power'), record.get('toughness')) != ('1', '1')):
        raise ValueError('exact Threshold card type and printed stats required')
    return dict(csvid=csvid, branch=BRANCHES[csvid], mana=cost_from_oracle(record))


def variants(shape):
    csvid = shape['csvid']
    fresh = ('lose_seventh',) if csvid == NUT else ('gain_seventh',)
    modes = (*COMMON, *EXTRA[csvid], *DENIED, *fresh, *(('replay',) if csvid in REPLAY else ()))
    return [f'{mode}_{owner}' for mode in modes for owner in (0, 1)] + (['decline_0'] if csvid == NUT else [])


def validate_variants(shape, selected):
    if (type(selected) is not list or not selected or any(type(mode) is not str for mode in selected)
            or len(set(selected)) != len(selected) or set(selected) - set(variants(shape))):
        raise ValueError('distinct supported Threshold board variants required')


def spec(declaration, record):
    pinned = profiles()['sources'].get(record.get('name'))
    if not pinned or not exact(pinned['oracle'], record):
        raise ValueError('exact complete Threshold primary required')
    shape = classify(record, pinned['registry']['csvid'])
    if (declaration.get('family') != FAMILY or type(declaration.get('version')) is not int
            or declaration['version'] != 2 or declaration.get('oracle_text') != record['oracle_text']
            or not exact(declaration.get('params'), shape)):
        raise ValueError('exact typed Threshold board declaration required')
    validate_variants(shape, declaration.get('variants'))
    return dict(shape, family=FAMILY)


def validate_witnesses(oracle, rows):
    pins = profiles()
    if (set(pins) != {'sources', 'witnesses', 'token_registry'} or set(pins['witnesses']) != WITNESSES
            or set(pins['token_registry']) != TOKENS
            or {pin['registry']['csvid'] for pin in pins['sources'].values()} != CLAUSES.keys()):
        raise ValueError('complete independent Threshold primaries, witnesses and token identities required')
    for group in ('sources', 'witnesses'):
        for name, pin in pins[group].items():
            row = rows.get(name, {})
            if (not exact(oracle.get(name), pin['oracle']) or not exact(row, pin['registry'])
                    or row.get('kind') != 'card' or row.get('handler') in (None, '0', 'NULL')
                    or oracle[name].get('layout') != 'normal'):
                raise ValueError('exact Threshold primary or witness changed: ' + name)
            if group == 'sources':
                if row['handler'] == 'card_dummy':
                    raise ValueError('implemented Threshold primary required')
                classify(oracle[name], row['csvid'])
    for name, pin in pins['token_registry'].items():
        if not exact(rows.get(name), pin):
            raise ValueError('exact Threshold token registry identity changed: ' + name)


class Board:
    def __init__(self, record, cid, actor, mode, rows, oracle):
        self.record, self.cid, self.actor, self.foe, self.mode = record, cid, actor, 1 - actor, mode
        self.rows, self.oracle = rows, oracle
        self.setup, self.steps, self.refs = [], [], {}
        self.active = mode in START_AT_SEVEN or mode in (*DENIED, 'lose_seventh', 'replay')
        self.current = self.foe if mode == 'offturn_seven' else actor
        self.add('source', actor, record['name'], 'hand')
        for seat, cards in enumerate(self.graves()):
            for i, name in enumerate(cards):
                self.add(f'g_{seat}_{i}', seat, name, 'graveyard')
            for i, name in enumerate(('Island', 'Plains')):
                self.add(f'd_{seat}_{i}', seat, name, 'library')
        self.witnesses()
        if mode == 'countered':
            self.add('counter', self.foe, 'Counterspell', 'hand')
        if mode == 'gain_seventh':
            self.add('shock', self.actor, 'Shock', 'hand')
        if mode == 'lose_seventh':
            self.add('crypt', self.actor, CRYPT)

    def graves(self):
        cards = [(), ()]
        cards[self.actor] = ('Mountain',) * (7 if self.active else 6)
        if self.mode == 'old_enchantment':
            cards[self.actor] = cards[self.actor][:-1] + ('Bad Moon',)
        if self.mode == 'opponent_only':
            cards[self.foe] = ('Mountain',) * 7
        return cards

    def add(self, ref, player, name, zone='battlefield'):
        self.setup.append(card(name, ref, player, zone))
        self.refs[ref] = name, player

    def emit(self, op, ref='', other='', prop='', *, player=0, value=0, zone=0, index=0, color=0):
        if op in ('untap_step', 'upkeep_step', 'choose_cancel_target'):
            self.steps.append(step(op, player=player))
            return
        if op == 'choose_trigger_source':
            self.steps.append(step(op, ref=ref, player=player))
            return
        fields = dict(player=player, value=value, index=index, color=color)
        if ref:
            fields['ref'] = ref
        if other:
            fields['other'] = other
        if prop:
            fields['property'] = prop
        if op in ('assert_zone', 'assert_zone_multiplicity'):
            fields['zone'] = ZONES[zone]
        self.steps.append(step(op, **fields))

    def witnesses(self):
        if self.cid == CLEANSING:
            self.add('friendly', self.actor, 'Glorious Anthem')
            self.add('opposing', self.foe, 'Fervor')
            self.add('bear', self.actor, 'Grizzly Bears')
            self.add('artifact', self.foe, "Traveler's Amulet")
        elif self.cid == DEMORALIZE:
            self.add('attacker', self.current, 'Grizzly Bears')
            self.add('first', 1 - self.current, 'Hill Giant')
            if self.mode in ('pair', 'offturn_seven'):
                self.add('second', 1 - self.current, 'Enormous Baloth')
        elif self.cid == WRATH:
            self.add('myr', self.actor, 'Darksteel Myr')
            self.add('artifact', self.foe, "Traveler's Amulet")
            self.add('victim', self.foe, 'Drudge Skeletons' if self.mode == 'shield' else 'Wind Drake')
            if self.mode in ('own_death', 'gain_seventh') or self.active:
                self.add('own_victim', self.actor, 'Grizzly Bears')
            if self.mode == 'shield':
                self.add('shield', self.foe, 'Wrap in Vigor', 'hand')
            if self.mode == 'spirits_fly':
                self.add('ground', self.foe, 'Hill Giant', 'hand')
        elif self.cid == NUT:
            self.add('own_changeling', self.actor, 'Woodland Changeling')
            self.add('foe_changeling', self.foe, 'Woodland Changeling')
            self.add('foil', self.foe, 'Grizzly Bears')
        elif self.cid == COALS:
            self.add('durable', self.actor, 'Enormous Baloth')
            self.add('fragile', self.foe, 'Giant Spider')
            self.add('foil', self.foe, 'Hill Giant')

    def mana(self, op, player, pool):
        for color, amount in enumerate(pool):
            self.emit(op, player=player, color=color, value=amount)

    def cast(self, ref, choices=(), *, pending=False):
        name, player = self.refs[ref]
        self.mana('set_mana', player, cost_from_oracle(self.oracle[name]))
        self.emit('refresh')
        for choice in choices:
            op, target, value = choice
            self.emit(op, target, player=player, value=value)
        self.emit('cast', ref, value=1)
        self.emit('assert_choices_consumed')
        self.emit('assert_stack', value=1)
        self.mana('assert_mana', player, [0] * 6)
        if not pending:
            self.resolve()

    def resolve(self):
        self.emit('resolve')
        self.emit('assert_choices_consumed')
        self.emit('assert_stack')
        self.emit('refresh')

    def observe(self, ref, prop, expected):
        self.emit('assert_card', ref, prop=prop, value=expected)

    def stats(self, ref, power, toughness):
        self.observe(ref, 'power', power)
        self.observe(ref, 'toughness', toughness)

    def receipt(self, name, player, zone, count):
        self.emit('assert_zone_multiplicity', player=player, zone=ZONE[zone], index=self.rows[name]['csvid'], value=count)

    def life(self, player, total):
        self.emit('assert_life', player=player, value=total)

    def token(self, name, count, traits=()):
        self.receipt(name, self.actor, 'battlefield', count)
        self.receipt(name, self.foe, 'battlefield', 0)
        if count:
            for prop, value in (('token', 1), *traits):
                self.emit('assert_matching_permanents', player=self.actor, index=self.rows[name]['csvid'],
                          prop=prop, value=value)

    def turn(self, *, upkeep=None):
        self.current = 1 - self.current
        self.emit('advance_turn')
        self.emit('untap_step', player=self.current)
        if upkeep is not None:
            if self.current == self.actor and self.actor == 0:
                if upkeep:
                    self.emit('choose_trigger_source', 'source', player=0)
                else:
                    self.emit('choose_priority', player=0, value=-2)
            self.emit('upkeep_step', player=self.current)
            self.emit('assert_choices_consumed')
        self.emit('turn', player=self.current)
        self.emit('refresh')

    def attack(self, *attackers):
        self.emit('begin_attackers')
        for ref in attackers:
            self.emit('attack', ref, value=1)
            self.observe(ref, 'attacking', 1)
        self.emit('finish_attackers')
        self.emit('begin_blockers')

    def block(self, blocker, attacker, legal):
        self.emit('block', blocker, attacker, value=int(legal))

    def damage(self):
        self.emit('advance_combat')
        self.emit('combat_damage')
        self.emit('resolve')
        self.emit('end_combat')
        self.emit('refresh')
        self.emit('assert_choices_consumed')

    def cleansing(self):
        self.stats('bear', 3, 3)
        self.cast('source')
        self.receipt('Glorious Anthem', self.actor, 'battlefield', int(self.active))
        self.receipt('Glorious Anthem', self.actor, 'graveyard', int(not self.active))
        self.receipt('Fervor', self.foe, 'battlefield', 0)
        self.receipt('Fervor', self.foe, 'graveyard', 1)
        self.observe('artifact', 'in_play', 1)
        size = 3 if self.active else 2
        self.stats('bear', size, size)
        if self.mode == 'old_enchantment':
            self.receipt('Bad Moon', self.actor, 'graveyard', 1)
            self.receipt('Bad Moon', self.actor, 'battlefield', 0)

    def demoralize(self):
        self.cast('source')
        self.attack('attacker')
        defender = 1 - self.current
        if self.mode == 'pair':
            for ref in ('first', 'second'):
                self.block(ref, 'attacker', True)
            self.emit('advance_combat')
            self.observe('first', 'blocking_count', 1)
            self.observe('second', 'blocking_count', 1)
            return
        self.block('first', 'attacker', False)
        if self.mode == 'offturn_seven':
            self.block('second', 'attacker', False)
        self.damage()
        self.life(defender, 16 if self.mode == 'gain_seventh' else 18)
        if self.mode == 'expires':
            self.turn()
            self.turn()
            self.attack('attacker')
            self.block('first', 'attacker', True)
            self.emit('advance_combat')
            self.observe('first', 'blocking_count', 1)

    def wrath(self):
        if self.mode == 'shield':
            self.cast('shield')
            self.receipt('Wrap in Vigor', self.foe, 'graveyard', 1)
            self.observe('victim', 'in_play', 1)
        self.cast('source')
        self.observe('myr', 'in_play', 1)
        self.observe('artifact', 'in_play', 1)
        for ref in ('victim', 'own_victim'):
            if ref in self.refs:
                name, seat = self.refs[ref]
                self.receipt(name, seat, 'graveyard', 1)
                self.receipt(name, seat, 'battlefield', 0)
        self.token('Spirit', 2 if self.active else 0,
                   (('color', 32), ('subtype_Spirit', 1), ('power', 1), ('toughness', 1)))
        if self.mode == 'spirits_fly':
            for i in range(2):
                self.emit('bind_permanent_copy', f'spirit_{i}', player=self.actor, index=i, value=self.rows['Spirit']['csvid'])
            self.turn()
            self.cast('ground')
            self.turn()
            self.attack('spirit_0', 'spirit_1')
            self.block('ground', 'spirit_0', False)
            self.damage()
            self.life(self.foe, 18)

    def squirrels(self, bonus, tokens):
        self.stats('source', 1, 1)
        self.stats('own_changeling', 2 + bonus, 2 + bonus)
        self.stats('foe_changeling', 2 + bonus, 2 + bonus)
        self.stats('foil', 2, 2)
        self.token('Squirrel', tokens,
                   (('color', 8), ('subtype_Squirrel', 1), ('power', 1 + bonus), ('toughness', 1 + bonus)))

    def nut(self):
        self.cast('source')
        self.observe('source', 'in_play', 1)
        bonus = 2 if self.active else 0
        self.squirrels(bonus, 0)
        if self.mode == 'gain_seventh':
            self.gain_seventh()
            bonus = 2
            self.squirrels(bonus, 0)
        elif self.mode == 'lose_seventh':
            self.emit('choose_player', player=self.actor, value=self.actor)
            self.emit('activate', 'crypt', value=1)
            self.emit('assert_choices_consumed')
            self.emit('assert_stack', value=1)
            self.resolve()
            self.emit('assert_zone', player=self.actor, zone=ZONE['graveyard'], value=0)
            self.receipt('Mountain', self.actor, 'exile', 7)
            self.receipt(CRYPT, self.actor, 'exile', 1)
            bonus = 0
            self.squirrels(bonus, 0)
        self.turn(upkeep=True)
        self.squirrels(bonus, 0)
        accepted = self.mode != 'decline'
        self.turn(upkeep=accepted)
        self.squirrels(bonus, int(accepted))

    def coals_choices(self):
        choices = () if self.mode == 'zero_targets' else (('choose_player', '', self.foe),)
        if self.mode in ('one_target', 'zero_targets'):
            return choices + (('choose_cancel_target', '', 0),)
        return choices + (('choose_target', 'durable', 0), ('choose_target', 'fragile', 0))

    def coals(self):
        single = self.mode == 'one_target'
        empty = self.mode == 'zero_targets'
        self.cast('source', self.coals_choices())
        amount = 4 if self.active else 2
        self.life(self.actor, 20)
        before = 18 if self.mode == 'gain_seventh' else 20
        self.life(self.foe, before - (0 if empty else amount))
        self.observe('durable', 'damage', amount if not single and not empty else 0)
        self.observe('foil', 'damage', 0)
        if not single and not empty and amount == 4:
            self.receipt('Giant Spider', self.foe, 'graveyard', 1)
            self.receipt('Giant Spider', self.foe, 'battlefield', 0)
        else:
            self.observe('fragile', 'damage', amount if not single and not empty else 0)



    def deny_entry(self):
        csvid = self.rows[self.record['name']]['csvid']
        if self.mode == 'countered':
            choices = self.coals_choices() if self.cid == COALS else ()
            self.cast('source', choices, pending=True)
            self.emit('announce_spell', 'source')
            self.mana('set_mana', self.foe, cost_from_oracle(self.oracle['Counterspell']))
            self.emit('refresh')
            self.emit('cast_response', 'counter', value=1)
            self.emit('assert_stack', value=2)
            self.emit('resolve_response')
            self.emit('assert_choices_consumed')
            self.emit('assert_stack')
            self.mana('assert_mana', self.foe, [0] * 6)
            self.receipt('Counterspell', self.foe, 'graveyard', 1)
            self.receipt(self.record['name'], self.actor, 'graveyard', 1)
        else:
            payment = cost_from_oracle(self.record)
            color = next(i for i in range(1, 6) if payment[i])
            if self.mode == 'short':
                payment[0 if payment[0] else color] -= 1
            else:
                substitute = next(i for i in range(1, 6) if not payment[i])
                payment[color] -= 1
                payment[substitute] += 1
            self.mana('set_mana', self.actor, payment)
            self.emit('refresh')
            self.emit('cast', 'source')
            self.emit('assert_choices_consumed')
            self.emit('assert_stack')
            self.mana('assert_mana', self.actor, payment)
            self.receipt(self.record['name'], self.actor, 'hand', 1)
            self.receipt(self.record['name'], self.actor, 'graveyard', 0)
        self.emit('assert_zone_multiplicity', player=self.actor, zone=ZONE['battlefield'], index=csvid)
        self.life(self.actor, 20)
        self.life(self.foe, 20)
        if self.cid == WRATH:
            self.token('Spirit', 0)
            self.observe('own_victim', 'in_play', 1)
            self.observe('victim', 'in_play', 1)
        elif self.cid == COALS:
            self.observe('durable', 'damage', 0)
            self.observe('fragile', 'damage', 0)
        elif self.cid == CLEANSING:
            self.observe('friendly', 'in_play', 1)
            self.observe('opposing', 'in_play', 1)
            self.stats('bear', 3, 3)
        elif self.cid == NUT:
            self.token('Squirrel', 0)
            self.stats('own_changeling', 2, 2)
            self.stats('foe_changeling', 2, 2)
        elif self.mode == 'countered':
            self.attack('attacker')
            self.block('first', 'attacker', True)
            self.emit('advance_combat')
            self.observe('first', 'blocking_count', 1)

    def gain_seventh(self):
        self.cast('shock', (('choose_player', '', self.foe),))
        self.emit('assert_zone', player=self.actor, zone=ZONE['graveyard'], value=7)
        self.receipt('Shock', self.actor, 'graveyard', 1)
        self.life(self.foe, 18)
        if self.cid != NUT:
            self.active = True

    def play(self):
        if self.mode == 'gain_seventh' and self.cid != NUT:
            self.gain_seventh()
        handler = {CLEANSING: self.cleansing, DEMORALIZE: self.demoralize, WRATH: self.wrath,
                   NUT: self.nut, COALS: self.coals}
        handler[self.cid]()
        if self.cid != NUT:
            self.receipt(self.record['name'], self.actor, 'graveyard', 1)

    def build(self):
        self.emit('turn', player=self.current)
        for seat, cards in enumerate(self.graves()):
            self.mana('set_mana', seat, [0] * 6)
            self.emit('assert_zone', player=seat, zone=ZONE['graveyard'], value=len(cards))
            for name in sorted(set(cards)):
                self.receipt(name, seat, 'graveyard', cards.count(name))
        if self.mode in DENIED:
            self.deny_entry()
            return
        cut = len(self.steps)
        self.play()
        if self.mode == 'replay':
            suffix = self.steps[cut:]
            self.steps[cut:] = ([step('capture')] + suffix + [step('record_state'), step('ai_roundtrip'), step('restore')]
                                + deepcopy(suffix) + [step('assert_recorded_state')])


def expand(name, csvid, declaration, shape, selected, rows, oracle):
    validate_variants(shape, selected)
    validate_witnesses(oracle, rows)
    if shape['csvid'] != csvid or profiles()['sources'][name]['registry']['csvid'] != csvid:
        raise ValueError('exact Threshold primary CSVID required')
    cases = []
    for variant in selected:
        mode, player = variant.rsplit('_', 1)
        board = Board(oracle[name], csvid, int(player), mode, rows, oracle)
        board.build()
        cases.append(dict(
            name=f'ThresholdGlobalEffectsRecipe_{csvid}_{variant}', card=name, fixture='empty_main', seed=2370,
            setup=board.setup, steps=board.steps,
            _recipe=dict(family=FAMILY, version=2, variant=variant, params=declaration['params'],
                         oracle_text=declaration['oracle_text'], untested_components=list(GAPS.get(csvid, ()))),
        ))
    return cases
