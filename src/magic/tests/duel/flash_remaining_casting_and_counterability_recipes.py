"""Paid Flash permissions and uncounterable creature compositions."""

from functools import lru_cache
import json
from pathlib import Path

from death_counter_trigger_recipes import exact
from fight_entry_fighters_recipes import Board
from recipes import cost_from_oracle


FAMILY = 'flash_remaining_casting_and_counterability'
PROFILE = Path(__file__).with_name('data') / 'flash_remaining_casting_and_counterability_wave8_profiles.json'
PREFIX = 'FlashRemainingCastingRecipe_'

SHIMMER, TEFERI, YEVA, RAFF = 11522, 8693, 12643, 18487
PEARL, CHROMIUM = 14340, 18894
PEARL_NAME, CHROMIUM_NAME = 'Pearl Lake Ancient', 'Chromium, the Mutable'
PRINTED = {
    SHIMMER: ('mirrodin_besieged.c', '{3}', 'Artifact Creature — Myr', (2, 2), ('Flash',),
              'Flash\nYou may cast artifact spells as though they had flash.'),
    TEFERI: ('time_spiral.c', '{2}{U}{U}{U}', 'Legendary Creature — Human Wizard', (3, 4), ('Flash',),
             "Flash\nCreature cards you own that aren't on the battlefield have flash.\n"
             'Each opponent can cast spells only any time they could cast a sorcery.'),
    YEVA: ('m13.c', '{2}{G}{G}', 'Legendary Creature — Elf Shaman', (4, 4), ('Flash',),
           'Flash (You may cast this spell any time you could cast an instant.)\n'
           'You may cast green creature spells as though they had flash.'),
    RAFF: ('dominaria.c', '{2}{W}{U}', 'Legendary Creature — Human Wizard', (3, 3), ('Flying', 'Flash'),
           'Flash\nFlying\nYou may cast historic spells as though they had flash. '
           '(Artifacts, legendaries, and Sagas are historic.)'),
    PEARL: ('khans_of_tarkir.c', '{5}{U}{U}', 'Creature — Leviathan', (6, 7), ('Prowess', 'Flash'),
            "Flash\nThis spell can't be countered.\nProwess (Whenever you cast a noncreature spell, "
            'this creature gets +1/+1 until end of turn.)\nReturn three lands you control to their '
            "owner's hand: Return this creature to its owner's hand."),
    CHROMIUM: ('m19.c', '{4}{W}{U}{B}', 'Legendary Creature — Elder Dragon', (7, 7), ('Flying', 'Flash'),
               "Flash\nThis spell can't be countered.\nFlying\nDiscard a card: Until end of turn, "
               'Chromium becomes a Human with base power and toughness 1/1, loses all abilities, '
               "and gains hexproof. It can't be blocked this turn."),
}
ARTIFACT, ARTIFACT_CREATURE = 'Millstone', 'Darksteel Myr'
GREEN, MULTICOLOR, BLUE, GREEN_SPELL = 'River Boa', 'Putrid Leech', 'Prodigal Sorcerer', 'Giant Growth'
LEGEND, SAGA = 'Vorel of the Hull Clade', 'The First Eruption'
REACH, SECOND_PARENT, BOUNCE = 'Reach Through Mists', 'Weave Fate', 'Unsummon'
GROUND, AIR = 'Dazzling Ramparts', 'Drift of Phantasms'
COUNTER, AIR_ATTACKER = 'Counterspell', 'Warden of Evos Isle'
CARD_FOR = {
    SHIMMER: dict(artifact=ARTIFACT, artifact_creature=ARTIFACT_CREATURE, wrong_type=BLUE,
                  removed=ARTIFACT, live_parent=ARTIFACT_CREATURE),
    TEFERI: dict(friendly_creature=BLUE, foe_main=REACH, foe_offturn=REACH,
                 foe_pending=REACH, foe_after_bounce=REACH),
    YEVA: dict(green=GREEN, multicolor=MULTICOLOR, wrong_color=BLUE,
               wrong_type=GREEN_SPELL, removed=GREEN),
    RAFF: dict(artifact=ARTIFACT, legendary=LEGEND, saga=SAGA, wrong_type=GREEN,
               foreign_hand=LEGEND, removed=ARTIFACT),
}
CREATURE_MODES = {
    PEARL: ('counter', 'prowess', 'bounce_three', 'short_lands'),
    CHROMIUM: ('counter', 'shift', 'shift_defends', 'flight', 'empty_hand'),
}
WITNESSES = {name for variants in CARD_FOR.values() for name in variants.values()} | {
    GROUND, AIR, REACH, SECOND_PARENT, BOUNCE, COUNTER, AIR_ATTACKER, 'Forest', 'Island', 'Mountain', 'Plains',
}
GAPS = {TEFERI: ['Non-hand casting permission awaits a creature graveyard casting action. '
                 'The existing graveyard cast action handles flashback spells.']}
ZERO = [0] * 6


def stats(record):
    return tuple(int(record[key]) for key in ('power', 'toughness'))


class Duel(Board):
    def __init__(self, csvid, record, actor, mode, rows, oracle):
        self.csvid, self.record, self.actor, self.foe, self.mode = csvid, record, actor, 1 - actor, mode
        self.rows, self.oracle = rows, oracle
        self.setup, self.steps, self.cards = [], [], {}
        self.life = [20, 20]
        self.library = {actor: ['Mountain', 'Plains'], self.foe: ['Island', 'Forest', 'Plains', 'Mountain']}
        self.pools = [ZERO.copy(), ZERO.copy()]
        self.drawn = 0
        self.subject = CARD_FOR[csvid][mode]
        self.subject_owner = self.foe if mode.startswith('foe_') or mode == 'foreign_hand' else actor
        self.add('source', actor, record['name'], 'hand')
        self.add('intro', self.foe, REACH, 'hand')
        self.add('subject', self.subject_owner, self.subject, 'hand')
        if mode in ('foe_pending', 'live_parent'):
            self.add('parent', self.foe, SECOND_PARENT, 'hand')
        if mode in ('removed', 'foe_after_bounce'):
            self.add('bounce', self.foe, BOUNCE, 'hand')
        for seat, names in self.library.items():
            for index, name in enumerate(names):
                self.add(f'library_{seat}_{index}', seat, name, 'library')
        if csvid == RAFF and mode in ('artifact', 'saga'):
            self.add('ground', self.foe, GROUND, 'battlefield')
        if csvid == RAFF and mode == 'artifact':
            self.add('air', self.foe, AIR, 'battlefield')

    def fee(self, ref):
        return cost_from_oracle(self.oracle[self.cards[ref]['name']])

    def fund(self, owner, pool):
        self.pools[owner] = pool.copy()
        self.mana('set_mana', owner, pool)
        self.emit('refresh')

    def payment_receipts(self):
        for seat in (0, 1):
            self.mana('assert_mana', seat, self.pools[seat])

    def cast(self, ref, *, response=False, target=None, legal=True):
        seat = self.cards[ref]['player']
        self.fund(seat, self.fee(ref))
        if target:
            self.emit('choose_target', target, player=seat)
        self.emit('cast_response' if response else 'cast', ref, value=int(legal))
        self.emit('assert_unused_choices', value=0)
        self.emit('assert_choices_consumed')
        self.emit('assert_stack', value=int(legal) + int(response))
        self.observe(ref, 'in_hand', int(not legal))
        if legal:
            self.pools[seat] = ZERO.copy()
        self.payment_receipts()

    def draw(self, count):
        for index in range(self.drawn, self.drawn + count):
            self.cards[f'library_{self.foe}_{index}']['zone'] = 'hand'
        self.drawn += count
        del self.library[self.foe][:count]

    def start(self):
        self.emit('turn', player=self.foe)
        for player in (0, 1):
            self.fund(player, ZERO)
        self.cast('intro')
        self.emit('announce_spell', 'intro')
        self.cast('source', response=True)
        self.emit('resolve_response')
        self.emit('refresh')
        self.cards['source']['zone'] = 'battlefield'
        self.cards['intro']['zone'] = 'graveyard'
        self.draw(1)
        self.inspect_source(present=True)
        self.receipts()

    def inspect_source(self, *, present):
        self.observe('source', 'in_play', int(present))
        if present:
            power, toughness = stats(self.record)
            self.observe('source', 'power', power)
            self.observe('source', 'toughness', toughness)

    def next_actor_turn(self):
        self.emit('advance_turn')
        self.emit('untap_step', player=self.actor)
        self.emit('upkeep_step', player=self.actor)
        self.emit('turn', player=self.actor)

    def remove_source(self):
        self.cast('bounce', target='source')
        self.emit('resolve')
        self.emit('refresh')
        self.cards['bounce']['zone'] = 'graveyard'
        self.cards['source']['zone'] = 'hand'
        self.inspect_source(present=False)
        self.receipts()

    def hostile_instant(self):
        legal = self.mode in ('foe_main', 'foe_after_bounce')
        response = self.mode == 'foe_pending'
        if response:
            self.start_parent()
        if self.mode == 'foe_after_bounce':
            self.remove_source()
        if self.mode in ('foe_offturn', 'foe_after_bounce'):
            self.next_actor_turn()
        self.cast('subject', response=response, legal=legal)
        self.emit('resolve')
        if response:
            self.cards['parent']['zone'] = 'graveyard'
            self.draw(2)
        if legal:
            self.cards['subject']['zone'] = 'graveyard'
            self.draw(1)
        self.inspect_source(present=self.mode != 'foe_after_bounce')
        self.payment_receipts()
        self.receipts()

    def start_parent(self):
        self.cast('parent')
        self.emit('announce_spell', 'parent')

    def allowed_grant(self):
        response = self.mode == 'live_parent'
        if response:
            self.start_parent()
        self.fund(self.actor, self.fee('subject'))
        # These cards offer the actual paid cast via a source callback.
        # Shimmer and Teferi select from a zone list; Yeva and Raff select
        # the actual eligible hand card as a target.
        if not response:
            self.emit('can_activate', 'source', value=1)
        if self.csvid in (SHIMMER, TEFERI):
            self.emit('choose_zone_card', player=self.actor, value=self.rows[self.subject]['csvid'])
        else:
            self.emit('choose_target', 'subject', player=self.actor)
        self.emit('activate_response' if response else 'activate', 'source', value=1)
        self.emit('assert_unused_choices', value=0)
        self.emit('assert_choices_consumed')
        self.pools[self.actor] = ZERO.copy()
        self.payment_receipts()
        self.observe('subject', 'in_hand', 0)
        self.emit('resolve')
        self.emit('refresh')
        self.cards['subject']['zone'] = 'battlefield'
        self.observe('subject', 'in_play', 1)
        if response:
            self.cards['parent']['zone'] = 'graveyard'
            self.draw(2)
        if self.mode == 'saga':
            self.emit('assert_counter', 'subject', counter_kind='lore', value=1)
            self.observe('ground', 'damage', 1)
            self.observe('source', 'damage', 0)
        self.inspect_source(present=True)
        self.receipts()

    def denied_grant(self):
        self.fund(self.actor, self.fee('subject'))
        if self.mode == 'removed':
            self.emit('can_activate', 'source', value=1)
            self.remove_source()
        elif self.mode == 'foreign_hand':
            self.fund(self.foe, self.fee('subject'))
        self.emit('can_activate', 'source', value=0)
        self.observe('subject', 'in_hand', 1)
        self.inspect_source(present=self.mode != 'removed')
        self.payment_receipts()
        self.receipts()

    def raff_flight(self):
        self.next_actor_turn()
        self.emit('begin_attackers')
        self.emit('attack', 'source', value=1)
        self.emit('finish_attackers')
        self.emit('begin_blockers')
        self.emit('block', 'ground', 'source', value=0)
        self.emit('block', 'air', 'source', value=1)
        self.emit('advance_combat')
        self.observe('source', 'blocked', 1)
        self.observe('ground', 'blocking_count', 0)
        self.observe('air', 'blocking_count', 1)
        self.emit('combat_damage')
        self.emit('resolve')
        self.emit('end_combat')
        self.observe('air', 'damage', 3)
        self.receipts()

    def build(self):
        self.start()
        if self.mode.startswith('foe_'):
            self.hostile_instant()
        elif self.mode in ('wrong_type', 'wrong_color', 'foreign_hand', 'removed'):
            self.denied_grant()
        else:
            self.allowed_grant()
        if self.csvid == RAFF and self.mode == 'artifact':
            self.raff_flight()


class CreatureResponse(Duel):
    def __init__(self, csvid, record, actor, mode, rows, oracle):
        self.csvid, self.record, self.actor, self.foe, self.mode = csvid, record, actor, 1 - actor, mode
        self.rows, self.oracle = rows, oracle
        self.setup, self.steps, self.cards = [], [], {}
        self.life = [20, 20]
        self.library = {actor: ['Mountain', 'Plains'], self.foe: ['Island', 'Forest', 'Plains', 'Mountain']}
        self.pools = [ZERO.copy(), ZERO.copy()]
        self.drawn = 0
        self.add('source', actor, record['name'], 'hand')
        self.add('counter' if mode == 'counter' else 'intro', self.foe, COUNTER if mode == 'counter' else REACH, 'hand')
        for seat, names in self.library.items():
            for index, name in enumerate(names):
                self.add(f'library_{seat}_{index}', seat, name, 'library')
        if mode == 'prowess':
            self.add('boost', actor, REACH, 'hand')
            self.add('foe_spell', self.foe, REACH, 'hand')
            self.add('friendly_creature', actor, GREEN, 'hand')
        if mode in ('bounce_three', 'short_lands'):
            self.add_land('land_a', actor, 'Island')
            self.add_land('land_b', actor, 'Mountain')
            self.add_land('land_c', actor if mode == 'bounce_three' else self.foe, 'Forest',
                          foreign=mode == 'bounce_three')
            if mode == 'bounce_three':
                self.add_land('retained_land', actor, 'Plains')
        if mode in ('shift', 'shift_defends'):
            # With identical cards the automatic AI discard has a single observable result.
            self.add('discard', actor, 'Mountain', 'hand')
            self.add('reserve', actor, 'Mountain', 'hand')
            self.add('own_pinger', actor, BLUE, 'battlefield')
            self.add('foe_pinger', self.foe, BLUE, 'battlefield')
        if mode in ('shift', 'flight'):
            self.add('ground', self.foe, GROUND, 'battlefield')
            self.add('air', self.foe, AIR, 'battlefield')
        if mode == 'shift_defends':
            self.add('air_attacker', self.foe, AIR_ATTACKER, 'battlefield')

    def add_land(self, ref, seat, name, *, foreign=False):
        self.add(ref, seat, name, 'battlefield')
        self.setup[-1]['state'] = ['tapped', 'opponent_owned'] if foreign else ['tapped']

    def powered(self, power, toughness):
        self.observe('source', 'in_play', 1)
        self.observe('source', 'power', power)
        self.observe('source', 'toughness', toughness)

    def start_uncounterable(self):
        if self.mode != 'counter':
            self.start()
            return
        self.emit('turn', player=self.foe)
        for seat in (0, 1):
            self.fund(seat, ZERO)
        self.cast('source')
        self.emit('announce_spell', 'source')
        self.cast('counter', response=True)
        self.emit('resolve_response')
        self.emit('refresh')
        self.cards['source']['zone'] = 'battlefield'
        self.cards['counter']['zone'] = 'graveyard'
        self.inspect_source(present=True)
        self.receipts()

    def prowess(self):
        self.cast('foe_spell')
        self.emit('resolve')
        self.emit('refresh')
        self.cards['foe_spell']['zone'] = 'graveyard'
        self.draw(1)
        self.powered(6, 7)
        self.receipts()
        self.cast('boost')
        self.emit('announce_spell', 'boost')
        self.emit('resolve')
        self.emit('refresh')
        self.cards['boost']['zone'] = 'graveyard'
        self.cards[f'library_{self.actor}_0']['zone'] = 'hand'
        del self.library[self.actor][0]
        self.powered(7, 8)
        self.receipts()
        self.next_actor_turn()
        self.powered(6, 7)
        self.cast('friendly_creature')
        self.emit('resolve')
        self.emit('refresh')
        self.cards['friendly_creature']['zone'] = 'battlefield'
        self.powered(6, 7)
        self.receipts()

    def bounce_lands(self):
        paid = self.mode == 'bounce_three'
        self.emit('can_activate', 'source', value=int(paid))
        if paid:
            for land in ('land_a', 'land_b', 'land_c'):
                self.emit('choose_target', land, player=self.actor)
        self.emit('activate', 'source', value=int(paid))
        self.emit('assert_unused_choices', value=0)
        self.emit('assert_choices_consumed')
        self.emit('assert_stack', value=int(paid))
        self.powered(6, 7)
        self.payment_receipts()
        if not paid:
            self.receipts()
            return
        self.cards['land_c']['player'] = self.foe
        for land in ('land_a', 'land_b', 'land_c'):
            item = self.cards[land]
            item['zone'] = 'hand'
            card_id = self.rows[item['name']]['csvid']
            self.emit('assert_zone_multiplicity', player=item['player'], zone='hand', index=card_id, value=1)
        self.observe('retained_land', 'in_play', 1)
        self.emit('resolve')
        self.emit('refresh')
        self.cards['source']['zone'] = 'hand'
        self.emit('bind_hand', 'returned_source', player=self.actor, value=self.csvid)
        self.receipts()

    def chromium_type(self, *, shifted):
        power = 1 if shifted else 7
        self.powered(power, power)
        for creature_type, value in (('Human', int(shifted)), ('Dragon', int(not shifted))):
            self.emit('assert_matching_permanents', prop='subtype_' + creature_type,
                      player=self.actor, index=self.csvid, value=value)

    def chromium_targets(self, *, shifted):
        for ref, legal in (('own_pinger', 1), ('foe_pinger', int(not shifted))):
            self.emit('probe_activation_target', ref, 'source', index=-1, value=legal)

    def chromium_shift(self):
        self.chromium_type(shifted=False)
        self.chromium_targets(shifted=False)
        self.emit('can_activate', 'source', value=1)
        if self.actor == 0:
            self.emit('choose_zone_card', player=self.actor, value=self.rows['Mountain']['csvid'])
        else:
            self.emit('choose_dialog', player=self.actor, value=0)
        self.emit('activate', 'source', value=1)
        self.emit('assert_unused_choices', value=0)
        self.emit('assert_choices_consumed')
        self.emit('assert_stack', value=1)
        self.cards['discard']['zone'] = 'graveyard'
        self.emit('assert_zone_multiplicity', player=self.actor, zone='hand', index=self.rows['Mountain']['csvid'], value=1)
        self.emit('assert_zone_multiplicity', player=self.actor, zone='graveyard',
                  index=self.rows['Mountain']['csvid'], value=1)
        self.payment_receipts()
        self.chromium_type(shifted=False)
        self.emit('resolve')
        self.emit('refresh')
        self.chromium_type(shifted=True)
        self.chromium_targets(shifted=True)
        self.emit('can_activate', 'source', value=0)
        self.receipts()

    def chromium_attack(self, *, shifted):
        self.emit('begin_attackers')
        self.emit('attack', 'source', value=1)
        self.emit('finish_attackers')
        self.emit('begin_blockers')
        self.emit('block', 'ground', 'source', value=0)
        self.emit('block', 'air', 'source', value=int(not shifted))
        self.emit('advance_combat')
        self.observe('source', 'blocked', int(not shifted))
        self.emit('combat_damage')
        self.emit('resolve')
        self.emit('end_combat')
        if shifted:
            self.life[self.foe] -= 1
        else:
            self.cards['air']['zone'] = 'graveyard'
        self.receipts()

    def chromium_defense(self):
        self.emit('begin_attackers')
        self.emit('attack', 'air_attacker', value=1)
        self.emit('finish_attackers')
        self.emit('begin_blockers')
        self.emit('block', 'source', 'air_attacker', value=0)
        self.emit('advance_combat')
        self.observe('air_attacker', 'blocked', 0)
        self.emit('combat_damage')
        self.emit('resolve')
        self.emit('end_combat')
        self.life[self.actor] -= stats(self.oracle[AIR_ATTACKER])[0]
        self.receipts()

    def chromium(self):
        if self.mode == 'empty_hand':
            self.emit('can_activate', 'source', value=0)
            self.chromium_type(shifted=False)
            self.receipts()
            return
        if self.mode != 'shift_defends':
            self.next_actor_turn()
        shifted = self.mode != 'flight'
        if shifted:
            self.chromium_shift()
        if self.mode == 'shift_defends':
            self.chromium_defense()
        else:
            self.chromium_attack(shifted=shifted)
        if shifted:
            self.emit('advance_turn')
            self.emit('refresh')
            self.chromium_type(shifted=False)
            self.chromium_targets(shifted=False)
            self.emit('can_activate', 'source', value=1)
            self.receipts()

    def build(self):
        self.start_uncounterable()
        if self.mode == 'counter':
            return
        if self.csvid == CHROMIUM:
            self.chromium()
        elif self.mode == 'prowess':
            self.prowess()
        else:
            self.bounce_lands()


@lru_cache(maxsize=1)
def profiles():
    return json.loads(PROFILE.read_bytes())


def classify(record, csvid):
    if (csvid not in PRINTED or not record or record.get('layout') != 'normal'
            or record.get('selected_face') or record.get('card_faces')):
        raise ValueError('whole normal Flash casting record required')
    _, cost, type_line, printed_stats, keywords, rules = PRINTED[csvid]
    if (record.get('type_line') != type_line or record.get('mana_cost') != cost
            or record.get('keywords') != list(keywords) or record.get('oracle_text') != rules
            or (record.get('power'), record.get('toughness')) != tuple(map(str, printed_stats))):
        raise ValueError('complete Flash, casting and creature composition required')
    return dict(csvid=csvid, mana=cost_from_oracle(record), stats=list(printed_stats))


def variants(shape):
    csvid = shape['csvid']
    modes = CREATURE_MODES[csvid] if csvid in CREATURE_MODES else CARD_FOR[csvid]
    return [f'{mode}_{seat}' for mode in modes for seat in (0, 1)]


def require_variants(shape, selected):
    if (type(selected) is not list or not selected or any(type(mode) is not str for mode in selected)
            or len(selected) != len(set(selected)) or set(selected) - set(variants(shape))):
        raise ValueError('distinct reviewed Flash casting variants required')


def spec(declaration, record):
    primary = profiles()['sources'].get(record.get('name', '')) if record else None
    if not primary or not exact(primary['oracle'], record):
        raise ValueError('exact pinned Flash casting record required')
    shape = classify(record, primary['registry']['csvid'])
    if (declaration.get('family') != FAMILY or type(declaration.get('version')) is not int
            or declaration['version'] != 2 or declaration.get('oracle_text') != record['oracle_text']
            or not exact(declaration.get('params'), shape)):
        raise ValueError('complete version 2 Flash casting declaration required')
    require_variants(shape, declaration.get('variants'))
    return dict(shape, family=FAMILY)


def validate_witnesses(oracle, rows):
    pins = profiles()
    if (set(pins) != {'sources', 'witnesses'} or set(pins['witnesses']) != WITNESSES
            or {pin['registry']['csvid'] for pin in pins['sources'].values()} != set(PRINTED)):
        raise ValueError('complete Flash source and witness pins required')
    for name, pin in (pins['sources'] | pins['witnesses']).items():
        row = rows.get(name, {})
        layout = 'saga' if name == SAGA else 'normal'
        if (not exact(oracle.get(name), pin['oracle']) or not exact(row, pin['registry'])
                or row.get('kind') != 'card' or row.get('handler') in (None, '0', 'NULL', 'card_dummy')
                or oracle[name].get('layout') != layout or oracle[name].get('card_faces')):
            raise ValueError('pinned Flash casting card changed: ' + name)
        if name in pins['sources']:
            classify(oracle[name], row['csvid'])
    if (oracle[REACH]['oracle_text'] != 'Draw a card.'
            or oracle[SECOND_PARENT]['oracle_text'] != 'Draw two cards.'
            or oracle[BOUNCE]['oracle_text'] != "Return target creature to its owner's hand."
            or oracle[COUNTER]['oracle_text'] != 'Counter target spell.'
            or oracle[BLUE]['oracle_text'] != '{T}: This creature deals 1 damage to any target.'
            or stats(oracle[AIR_ATTACKER]) != (2, 2) or 'Flying' not in oracle[AIR_ATTACKER]['keywords']
            or stats(oracle[GROUND]) != (0, 7) or 'Flying' in oracle[GROUND]['keywords']
            or stats(oracle[AIR]) != (0, 5) or 'Flying' not in oracle[AIR]['keywords']):
        raise ValueError('reviewed Flash casting and combat witnesses required')


def expand(name, csvid, declaration, shape, selected, rows, oracle):
    require_variants(shape, selected)
    validate_witnesses(oracle, rows)
    if profiles()['sources'][name]['registry']['csvid'] != csvid or csvid != shape['csvid']:
        raise ValueError('exact registered Flash casting primary required')
    cases = []
    for variant in selected:
        mode, seat = variant.rsplit('_', 1)
        board_type = CreatureResponse if csvid in CREATURE_MODES else Duel
        board = board_type(csvid, oracle[name], int(seat), mode, rows, oracle)
        board.build()
        cases.append(dict(name=f'{PREFIX}{csvid}_{variant}', card=name, fixture='empty_main', seed=2739,
                          setup=board.setup, steps=board.steps, _recipe=dict(
                              family=FAMILY, version=2, variant=variant, params=declaration['params'],
                              oracle_text=declaration['oracle_text'], untested_components=GAPS.get(csvid, []))))
    return cases

