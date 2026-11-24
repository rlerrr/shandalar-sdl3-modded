"""Paid Suspend and ordinary cast timing for four entry and sacrifice cards."""
from copy import deepcopy
from functools import lru_cache
import json
from pathlib import Path

from counter_condition_recurrence_recipes import exact
from recipes import card, cost_from_oracle, step


FAMILY = 'suspend_entry_and_sacrifice'
PROFILE = Path(__file__).with_name('data') / 'suspend_entry_and_sacrifice_wave8_profiles.json'
SEED = 2439
PREFIX = 'SuspendEntryAndSacrificeRecipe'
GARGADON, IVORY, NANTUKO, PHTHISIS = 8567, 8583, 8615, 8632
COMMON = ('parked', 'unfunded', 'countdown', 'normal')
EXTRA = {NANTUKO: ('normal_tapped', 'foe_tapped'), PHTHISIS: ('own_target', 'indestructible')}
CARDS = {
    GARGADON: (10, '{R}', '{9}{R}', 'Creature — Beast', (9, 7),
               'Sacrifice an artifact, creature, or land: Remove a time counter from this card. Activate only if this card is suspended.'),
    IVORY: (5, '{W}', '{5}{W}{W}', 'Creature — Giant', (3, 4),
            'When this creature enters, tap all nonwhite creatures.'),
    NANTUKO: (1, '{2}{G}{G}', '{2}{G}', 'Creature — Insect Shaman', (3, 2),
              'When this creature enters, if you control no tapped lands, draw a card.'),
    PHTHISIS: (5, '{1}{B}', '{3}{B}{B}{B}{B}', 'Sorcery', None,
               'Destroy target creature. Its controller loses life equal to its power plus its toughness.'),
}

NOVEL = ('wrong_suspend_color', 'countered_normal', 'replay_release')
EXTRA_NOVEL = {GARGADON: ('stolen_haste',), IVORY: ('next_untap',), NANTUKO: ('normal_untapped',),
               PHTHISIS: ('response_growth', 'target_bounced')}
NEW_WITNESSES = {'Counterspell', 'Control Magic', 'Giant Growth', 'Unsummon'}
WITNESSES = {'Island', 'Plains', 'Forest', 'Grizzly Bears', "Feldon's Cane", 'Maze of Ith',
             'Ornithopter', 'Suntail Hawk', 'Squire', 'Hill Giant', 'Darksteel Myr', 'Fog', *NEW_WITNESSES}
ZERO = [0] * 6
GAP = ('The existing active-zone binders hide TYPE_EFFECT, including Gargadon’s real suspended effect. '
       'The suspended sacrifice input and direct time-counter observer remain untested. Optional release refusal '
       'and a tapped-own-land condition at suspended Nantuko release are outside these cases.')


@lru_cache(maxsize=1)
def profiles():
    return json.loads(PROFILE.read_bytes())


def classify(record, csvid):
    if csvid not in CARDS or record.get('selected_face') or record.get('card_faces'):
        raise ValueError('whole normal Suspend source required')
    duration, fee, normal, type_line, stats, effect = CARDS[csvid]
    if (record.get('layout') != 'normal' or record.get('keywords') != ['Suspend']
            or record.get('mana_cost') != normal or record.get('type_line') != type_line
            or stats and (record.get('power'), record.get('toughness')) != tuple(map(str, stats))):
        raise ValueError('exact Suspend cost, body, keyword, and type required')
    head = f'Suspend {duration}—{fee}'
    if csvid == GARGADON:
        wanted = f'{head}\n{effect}'
    else:
        quantity = 'a time counter' if duration == 1 else 'five time counters'
        haste = ' It has haste.' if stats else ''
        wanted = (f'{effect}\n{head} (Rather than cast this card from your hand, you may pay {fee} '
                  f'and exile it with {quantity} on it. At the beginning of your upkeep, remove a time counter. '
                  f'When the last is removed, you may cast it without paying its mana cost.{haste})')
    if wanted != record.get('oracle_text'):
        raise ValueError('complete printed Suspend and rider text required')
    return dict(csvid=csvid, duration=duration, mana=cost_from_oracle(record),
                suspend_fee=cost_from_oracle({'mana_cost': fee}))



def variants(shape):
    csvid = shape['csvid']
    modes = (*COMMON, *EXTRA.get(csvid, ()), *NOVEL, *EXTRA_NOVEL[csvid])
    return [f'{mode}_{seat}' for mode in modes for seat in (0, 1)]


def validate_variants(shape, selected):
    if (type(selected) is not list or not selected or any(type(mode) is not str for mode in selected)
            or len(set(selected)) != len(selected) or set(selected) - set(variants(shape))):
        raise ValueError('distinct supported Suspend variants required')


def spec(declaration, record):
    pinned = profiles()['sources'].get(record.get('name'))
    if not pinned or not exact(pinned['oracle'], record):
        raise ValueError('complete pinned Suspend primary required')
    shape = classify(record, pinned['registry']['csvid'])
    if (declaration.get('family') != FAMILY or type(declaration.get('version')) is not int
            or declaration['version'] != 2 or declaration.get('oracle_text') != record['oracle_text']
            or not exact(declaration.get('params'), shape)):
        raise ValueError('exact declared Suspend branch required')
    validate_variants(shape, declaration.get('variants'))
    return dict(shape, family=FAMILY)


def validate_witnesses(oracle, rows):
    pins = profiles()
    if set(pins) != {'sources', 'witnesses'} or set(pins['witnesses']) != WITNESSES:
        raise ValueError('complete independent Suspend witnesses required')
    for group, entries in pins.items():
        for name, pin in entries.items():
            row = rows.get(name, {})
            if (not exact(oracle.get(name), pin['oracle']) or not exact(row, pin['registry'])
                    or row.get('kind') != 'card' or row.get('handler') in (None, '0', 'NULL')
                    or oracle[name].get('layout') != 'normal'):
                raise ValueError('exact Suspend primary or witness changed: ' + name)
            if group == 'sources':
                if row['handler'] == 'card_dummy':
                    raise ValueError('implemented Suspend primary required')
                classify(oracle[name], row['csvid'])


class Duel:
    def __init__(self, csvid, record, actor, mode, rows, oracle):
        self.csvid, self.record, self.actor, self.foe, self.mode = csvid, record, actor, 1 - actor, mode
        self.rows, self.oracle = rows, oracle
        self.duration, _, _, _, self.stats, _ = CARDS[csvid]
        shape = classify(record, csvid)
        self.payment, self.normal_fee = shape['suspend_fee'], shape['mana']
        self.setup, self.steps, self.cards = [], [], {}
        self.life = [20, 20]
        self.deck = {actor: ['Island', 'Plains'], self.foe: ['Island', 'Plains']}
        self.add('source', actor, record['name'], 'hand')
        for seat, deck in self.deck.items():
            for index, name in enumerate(deck):
                self.add(f'deck_{seat}_{index}', seat, name, 'library')
        if csvid == GARGADON:
            self.add('own_artifact', self.actor, "Feldon's Cane", 'battlefield')
            self.add('own_creature', self.actor, 'Grizzly Bears', 'battlefield')
            self.add('own_land', self.actor, 'Maze of Ith', 'battlefield')
        if csvid == IVORY:
            self.white_refs, self.nonwhite_refs = [], []
            for seat in (0, 1):
                for suffix, name, group in (('colored', 'Grizzly Bears', self.nonwhite_refs),
                                            ('colorless', 'Ornithopter', self.nonwhite_refs),
                                            ('white', 'Suntail Hawk', self.white_refs)):
                    ref = f'{suffix}_{seat}'
                    self.add(ref, seat, name, 'battlefield')
                    group.append(ref)
        if csvid == NANTUKO:
            if mode in ('normal_tapped', 'normal_untapped'):
                self.add('land', self.actor, 'Forest', 'battlefield')
            if mode == 'foe_tapped':
                self.add('land', self.foe, 'Forest', 'battlefield')
                self.add('fog', self.foe, 'Fog', 'hand')
        if csvid == PHTHISIS:
            self.target_player = self.actor if mode == 'own_target' else self.foe
            target = {'own_target': 'Grizzly Bears', 'indestructible': 'Darksteel Myr'}.get(mode, 'Hill Giant')
            self.add('victim', self.target_player, target, 'battlefield')
            self.add('other_creature', 1 - self.target_player, 'Squire', 'battlefield')

        responses = {'countered_normal': 'Counterspell', 'response_growth': 'Giant Growth',
                     'target_bounced': 'Unsummon', 'stolen_haste': 'Control Magic'}
        if mode in responses:
            self.add('response', self.foe, responses[mode], 'hand')

    def add(self, ref, player, name, zone):
        self.setup.append(card(name, ref, player, zone))
        self.cards[ref] = dict(name=name, player=player, zone=zone)

    def emit(self, op, ref='', other='', **values):
        if ref:
            values['ref'] = ref
        if other:
            values['other'] = other
        self.steps.append(step(op, **values))

    def observe(self, ref, prop, value):
        self.emit('assert_card', ref, property=prop, value=value)

    def mana(self, op, player, amounts):
        for color, amount in enumerate(amounts):
            self.emit(op, player=player, color=color, value=amount)

    def suspend_from_hand(self):
        self.emit('turn', player=self.actor)
        self.mana('set_mana', self.foe, ZERO)
        funded = self.mode not in ('unfunded', 'wrong_suspend_color')
        budget = self.payment if funded else ZERO
        if self.mode == 'wrong_suspend_color':
            budget = [0, 0, sum(self.payment), 0, 0, 0]
        self.mana('set_mana', self.actor, budget)
        self.emit('refresh')
        normal_also_affordable = all(have >= need for have, need in zip(self.payment, self.normal_fee))
        if funded and (self.actor == 1 or normal_also_affordable):
            self.emit('choose_dialog', player=self.actor, value=1)
        self.emit('cast', 'source', value=0)
        self.emit('assert_choices_consumed')
        self.mana('assert_mana', self.actor, ZERO if funded else budget)
        self.mana('assert_mana', self.foe, ZERO)
        if funded:
            self.cards['source']['zone'] = 'exile'
        self.receipts()

    def zone(self, name, player, zone, count):
        self.emit('assert_zone_multiplicity', player=player, zone=zone, index=self.rows[name]['csvid'], value=count)

    def receipts(self, *, ordered_library=True):
        self.emit('assert_stack')
        source_zone = self.cards['source']['zone']
        for zone in ('hand', 'battlefield', 'graveyard', 'exile'):
            self.zone(self.record['name'], self.actor, zone, int(source_zone == zone))
        for player, life in enumerate(self.life):
            self.emit('assert_life', player=player, value=life)

    def deck_receipts(self):
        for player, library in self.deck.items():
            self.emit('assert_zone', player=player, zone='library', value=len(library))
            for index, name in enumerate(library):
                self.emit('assert_zone_card', player=player, zone='library', index=index, value=self.rows[name]['csvid'])

    def plan_last(self):
        if self.csvid == PHTHISIS:
            self.emit('choose_target', 'victim', player=self.actor)

    def upkeep(self, player, *, last=False):
        self.emit('advance_turn')
        self.emit('untap_step', player=player)
        if last:
            # The legal acceptance route casts through the actual Suspend legacy.
            self.plan_last()
        self.emit('upkeep_step', player=player)
        self.emit('resolve')
        self.emit('refresh')
        self.emit('assert_unused_choices', value=0)
        self.emit('assert_choices_consumed')
        for seat in (0, 1):
            self.mana('assert_mana', seat, [0] * 6)
        if not last:
            self.receipts()

    def resolve(self):
        self.emit('resolve')
        self.emit('assert_choices_consumed')
        self.emit('assert_stack')
        self.emit('refresh')

    def land_activation(self, controller, starting):
        self.mana('set_mana', controller, starting)
        self.emit('refresh')
        self.emit('can_activate', 'land', value=1)
        self.emit('activate', 'land', value=1)
        self.emit('assert_choices_consumed')
        self.emit('assert_stack')
        self.observe('land', 'tapped', 1)
        green = cost_from_oracle(self.oracle['Fog'])
        self.mana('assert_mana', controller, [a + b for a, b in zip(starting, green)])

    def normal_cast(self):
        self.emit('turn', player=self.actor)
        self.mana('set_mana', self.foe, ZERO)
        self.mana('set_mana', self.actor, ZERO)
        due = self.normal_fee
        if self.mode == 'normal_tapped':
            self.land_activation(self.actor, [2, 0, 0, 0, 0, 0])
        else:
            if self.mode == 'foe_tapped':
                self.land_activation(self.foe, ZERO)
                self.emit('cast', 'fog', value=1)
                self.emit('assert_stack', value=1)
                self.resolve()
                self.zone('Fog', self.foe, 'graveyard', 1)
            self.mana('set_mana', self.actor, due)
        self.emit('refresh')
        # The untapped own Forest can provide the second green pip for Suspend.
        if self.mode == 'normal_untapped' or all(mana >= fee for mana, fee in zip(due, self.payment)):
            self.emit('choose_dialog', player=self.actor, value=0)
        self.plan_last()
        self.emit('cast', 'source', value=1)
        self.emit('assert_choices_consumed')
        self.emit('assert_stack', value=1)
        for seat in (0, 1):
            self.mana('assert_mana', seat, ZERO)
        if self.mode not in ('countered_normal', 'response_growth', 'target_bounced'):
            self.resolve()
            return
        name = self.cards['response']['name']
        self.emit('announce_spell', 'source')
        self.mana('set_mana', self.foe, cost_from_oracle(self.oracle[name]))
        self.emit('refresh')
        if self.mode != 'countered_normal':
            self.emit('choose_target', 'victim', player=self.foe)
        self.emit('cast_response', 'response', value=1)
        self.emit('assert_choices_consumed')
        self.emit('assert_stack', value=2)
        self.mana('assert_mana', self.foe, ZERO)
        self.emit('resolve_response')
        self.emit('assert_choices_consumed')
        self.emit('assert_stack')
        self.emit('refresh')
        self.zone(name, self.foe, 'graveyard', 1)

    def countered(self):
        self.cards['source']['zone'] = 'graveyard'
        self.receipts()
        self.deck_receipts()
        if self.csvid == IVORY:
            for ref in self.white_refs + self.nonwhite_refs:
                self.observe(ref, 'in_play', 1)
                self.observe(ref, 'tapped', 0)
        if self.csvid == PHTHISIS:
            target = self.cards['victim']['name']
            self.zone(target, self.target_player, 'battlefield', 1)
            self.zone(target, self.target_player, 'graveyard', 0)
        if self.csvid == NANTUKO:
            self.zone('Island', self.actor, 'hand', 0)

    def phthisis(self):
        target = self.cards['victim']['name']
        target_record = self.oracle[target]
        if self.mode == 'target_bounced':
            self.zone(target, self.target_player, 'hand', 1)
            self.zone(target, self.target_player, 'battlefield', 0)
            self.zone(target, self.target_player, 'graveyard', 0)
            self.zone('Squire', 1 - self.target_player, 'battlefield', 1)
            return
        lost = int(target_record['power']) + int(target_record['toughness'])
        if self.mode == 'response_growth':
            lost += 6
        self.life[self.target_player] -= lost
        destroyed = self.mode != 'indestructible'
        self.zone(target, self.target_player, 'battlefield', int(not destroyed))
        self.zone(target, self.target_player, 'graveyard', int(destroyed))
        self.zone('Squire', 1 - self.target_player, 'battlefield', 1)

    def entry(self, *, suspended):
        self.cards['source']['zone'] = 'battlefield' if self.stats else 'graveyard'
        if self.csvid == PHTHISIS:
            self.phthisis()
        elif self.csvid == IVORY:
            for ref in self.white_refs + self.nonwhite_refs:
                self.observe(ref, 'in_play', 1)
                self.observe(ref, 'tapped', int(ref in self.nonwhite_refs))
        elif self.csvid == NANTUKO:
            if 'land' in self.cards:
                self.observe('land', 'tapped', int(self.mode in ('normal_tapped', 'foe_tapped')))
            drawn = self.mode != 'normal_tapped'
            if drawn:
                assert self.deck[self.actor].pop(0) == 'Island'
            self.zone('Island', self.actor, 'hand', int(drawn))
            self.zone('Island', self.foe, 'hand', 0)

        self.receipts()
        self.deck_receipts()
        if not self.stats:
            return
        self.emit('bind_permanent_copy', 'played', player=self.actor, value=self.csvid, index=0)
        self.observe('played', 'in_play', 1)
        for prop, value in zip(('power', 'toughness'), self.stats):
            self.observe('played', prop, value)
        self.observe('played', 'tapped', 0)
        if self.csvid == GARGADON:
            self.emit('can_activate', 'played', value=0)
            for ref in ('own_artifact', 'own_creature', 'own_land'):
                self.observe(ref, 'in_play', 1)
        if self.mode == 'foe_tapped':
            # Fog applies this turn; the ordinary casting branch checks only entry.
            return
        if self.mode == 'stolen_haste':
            self.stolen_haste()
            return
        self.emit('turn', player=self.actor)
        self.emit('begin_attackers')
        self.emit('attack', 'played', value=int(suspended))
        self.observe('played', 'attacking', int(suspended))
        self.emit('finish_attackers')
        self.emit('begin_blockers')
        self.emit('advance_combat')
        self.emit('combat_damage')
        self.emit('resolve')
        self.emit('end_combat')
        self.emit('assert_choices_consumed')
        if suspended:
            self.life[self.foe] -= self.stats[0]
        self.receipts()

    def stolen_haste(self):
        self.emit('advance_turn')
        self.emit('untap_step', player=self.foe)
        self.emit('turn', player=self.foe)
        self.mana('set_mana', self.foe, cost_from_oracle(self.oracle['Control Magic']))
        self.emit('refresh')
        self.emit('choose_target', 'played', player=self.foe)
        self.emit('cast', 'response', value=1)
        self.emit('assert_stack', value=1)
        self.mana('assert_mana', self.foe, ZERO)
        self.resolve()
        self.zone('Control Magic', self.foe, 'battlefield', 1)
        self.zone(self.record['name'], self.actor, 'battlefield', 0)
        self.zone(self.record['name'], self.foe, 'battlefield', 1)
        self.emit('bind_permanent_copy', 'stolen', player=self.foe, value=self.csvid, index=0)
        self.observe('stolen', 'in_play', 1)
        self.observe('stolen', 'tapped', 0)
        self.emit('begin_attackers')
        self.emit('attack', 'stolen', value=0)
        self.observe('stolen', 'attacking', 0)
        self.emit('finish_attackers')
        self.emit('begin_blockers')
        self.emit('advance_combat')
        self.emit('combat_damage')
        self.emit('resolve')
        self.emit('end_combat')
        self.emit('assert_choices_consumed')
        self.emit('assert_life', player=self.actor, value=20)
        self.emit('assert_life', player=self.foe, value=20)

    def next_untap(self):
        self.emit('advance_turn')
        self.emit('untap_step', player=self.foe)
        for ref in self.nonwhite_refs:
            self.observe(ref, 'tapped', int(self.cards[ref]['player'] == self.actor))
        for ref in self.white_refs:
            self.observe(ref, 'tapped', 0)
        self.receipts()

    def final_release(self):
        self.upkeep(self.actor, last=True)
        self.entry(suspended=True)

    def replay_release(self):
        self.emit('capture')
        start = len(self.steps)
        self.final_release()
        tail = deepcopy(self.steps[start:])
        self.emit('record_state')
        self.emit('restore')
        self.emit('ai_roundtrip')
        self.steps.extend(tail)
        self.emit('assert_recorded_state')

    def build(self):
        if self.csvid == GARGADON:
            self.emit('can_activate', 'source', value=0)
        if self.mode in ('normal', 'normal_tapped', 'foe_tapped', 'countered_normal',
                         'normal_untapped', 'response_growth', 'target_bounced'):
            self.normal_cast()
            if self.mode == 'countered_normal':
                self.countered()
            else:
                self.entry(suspended=False)
            return
        self.suspend_from_hand()
        if self.mode in ('unfunded', 'wrong_suspend_color'):
            self.deck_receipts()
            return
        if self.mode == 'parked':
            self.upkeep(self.foe)
            self.deck_receipts()
            return
        for _ in range(self.duration - 1):
            self.upkeep(self.foe)
            self.upkeep(self.actor)
        self.upkeep(self.foe)
        if self.mode == 'replay_release':
            self.replay_release()
        else:
            self.final_release()
        if self.mode == 'next_untap':
            self.next_untap()



def build(name, csvid, declaration, shape, variant, rows, oracle):
    mode, seat = variant.rsplit('_', 1)
    duel = Duel(csvid, oracle[name], int(seat), mode, rows, oracle)
    duel.build()
    return dict(name=f'{PREFIX}_{csvid}_{variant}', card=name, fixture='empty_main', seed=SEED,
                setup=duel.setup, steps=duel.steps,
                _recipe=dict(family=FAMILY, version=2, variant=variant, params=declaration['params'],
                             oracle_text=declaration['oracle_text'], untested_components=[GAP]))


def expand(name, csvid, declaration, shape, selected, rows, oracle):
    if rows[name]['csvid'] != csvid or shape['csvid'] != csvid:
        raise ValueError('exact native Suspend source required')
    validate_variants(shape, selected)
    validate_witnesses(oracle, rows)
    return [build(name, csvid, declaration, shape, variant, rows, oracle) for variant in selected]
