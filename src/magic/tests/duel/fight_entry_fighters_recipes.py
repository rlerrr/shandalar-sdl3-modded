"""Paid creature and Aura entries that use real fight effects."""

from collections import Counter
from functools import lru_cache
import json
from pathlib import Path

from death_counter_trigger_recipes import exact
from recipes import cost_from_oracle, step


FAMILY = 'fight_entry_fighters'
PROFILE = Path(__file__).with_name('data') / 'fight_entry_fighters_wave8_profiles.json'
PREFIX = 'FightEntryFightersRecipe_'
ZONES = ('hand', 'battlefield', 'library', 'graveyard', 'exile')

GIANT, STAG, INDRIK, HARPOONER = 13216, 16189, 19278, 19414
CARTOUCHE = 16861
CARTOUCHE_NAME = 'Cartouche of Strength'
CARTOUCHE_RULE = (
    'Enchant creature you control\n'
    'When this Aura enters, you may have enchanted creature fight target creature an opponent controls. '
    '(Each deals damage equal to its power to the other.)\nEnchanted creature gets +1/+1 and has trample.'
)
MAY_FIGHT = "When this creature enters, you may have it fight target creature you don't control."
PRINTED = {
    GIANT: ('dragons_maze.c', 'Giant Warrior', '{4}{R}{G}', 4, 5, ('Fight',),
            'When this creature enters, it fights target creature an opponent controls chosen at random.'),
    STAG: ('eldritch_moon.c', 'Elk', '{3}{G}{G}', 4, 3, ('Fight',), MAY_FIGHT),
    INDRIK: ('eldritch_moon.c', 'Beast', '{5}{G}', 4, 4, ('Fight',),
             MAY_FIGHT + ' (Each deals damage equal to its power to the other.)'),
    HARPOONER: ('guilds_of_ravnica.c', 'Insect Warrior', '{1}{G}', 3, 2, ('Reach', 'Fight', 'Undergrowth'),
                "Reach\nUndergrowth — When this creature enters, choose up to one target creature you don't "
                'control with flying. This creature gets +X/+0 until end of turn, where X is the number of '
                'creature cards in your graveyard, then you may have this creature fight that creature.'),
}
OPTIONAL = ('reciprocal', 'backlash', 'decline', 'own_only')
MODES = {
    GIANT: ('single_durable', 'displaced_slot', 'backlash', 'no_eligible'),
    STAG: OPTIONAL,
    INDRIK: OPTIONAL,
    HARPOONER: ('undergrowth_lethal', 'normal_fight', 'zero_targets', 'no_eligible', 'foe_grave_only', 'reach'),
}
GROUNDED = 'Dazzling Ramparts'
FLYER = 'Thieving Magpie'
WITNESS_STATS = {
    GROUNDED: (0, 7, False), FLYER: (1, 3, True), 'Drift of Phantasms': (0, 5, True),
    'Shivan Dragon': (5, 5, True), 'Orcish Artillery': (1, 3, False),
    'Prodigal Sorcerer': (1, 1, False), 'Slippery Bogle': (1, 1, False),
}
ZERO = [0] * 6
HOST, AURA_VICTIM, BOUNCE, TRAMPLE_BLOCKER = 'Merfolk Mesmerist', 'Warden of Evos Isle', 'Unsummon', 'Slippery Bogle'
WITNESSES = set(WITNESS_STATS) | {HOST, AURA_VICTIM, BOUNCE, 'Millstone', 'Mountain', 'Plains', 'Island'}
CARTOUCHE_MODES = ('reciprocal', 'trample', 'lost_host', 'foreign_only', 'lethal_fight')
ALL_IDS = set(PRINTED) | {CARTOUCHE}
GAPS = {
    GIANT: ['Multiple legal targets and the random selection distribution are outside this scope.'],
    HARPOONER: ['The current callback exposes no separate decline after selecting a legal flyer.'],
    CARTOUCHE: ['The AI trigger path resolves automatically. Explicit optional decline is observed in the human seat.'],
}


def stats(record):
    return int(record['power']), int(record['toughness'])


class Board:
    def __init__(self, csvid, record, actor, mode, rows, oracle):
        self.csvid, self.record, self.actor, self.foe, self.mode = csvid, record, actor, 1 - actor, mode
        self.rows, self.oracle = rows, oracle
        self.setup, self.steps, self.cards = [], [], {}
        self.life = [20, 20]
        self.damage = {}
        self.library = {actor: ['Mountain', 'Plains', 'Island'], self.foe: ['Plains', 'Island']}
        self.add('source', actor, record['name'], 'hand')
        for player, names in self.library.items():
            for index, name in enumerate(names):
                self.add(f'library_{player}_{index}', player, name, 'library')

    def add(self, ref, player, name, zone):
        self.setup.append(dict(ref=ref, player=player, name=name, zone=zone))
        self.cards[ref] = dict(name=name, player=player, zone=zone)

    def emit(self, op, ref='', other='', prop='', **fields):
        if ref:
            fields['ref'] = ref
        if other:
            fields['other'] = other
        if prop:
            fields['property'] = prop
        self.steps.append(step(op, **fields))

    def observe(self, ref, prop, value):
        self.emit('assert_card', ref, prop=prop, value=value)

    def mana(self, op, player, pool):
        for color, value in enumerate(pool):
            self.emit(op, player=player, color=color, value=value)

    def receipts(self):
        self.emit('assert_stack', value=0)
        names = sorted({card['name'] for card in self.cards.values()})
        for player in (0, 1):
            self.emit('assert_life', player=player, value=self.life[player])
            for zone in ZONES:
                contents = self.library[player] if zone == 'library' else [
                    card['name'] for card in self.cards.values() if card['player'] == player and card['zone'] == zone
                ]
                self.emit('assert_zone', player=player, zone=zone, value=len(contents))
                occurrences = Counter(contents)
                for name in names:
                    self.emit('assert_zone_multiplicity', player=player, zone=zone,
                              index=self.rows[name]['csvid'], value=occurrences[name])
                if zone == 'library':
                    for index, name in enumerate(contents):
                        self.emit('assert_zone_card', player=player, zone=zone, index=index,
                                  value=self.rows[name]['csvid'])

    def result_of_damage(self, ref, amount, bonus_toughness=0):
        toughness = stats(self.oracle[self.cards[ref]['name']])[1] + bonus_toughness
        if amount >= toughness:
            self.cards[ref]['zone'] = 'graveyard'
            self.damage.pop(ref, None)
        else:
            self.damage[ref] = amount


class EntryFight(Board):
    def __init__(self, csvid, record, actor, mode, rows, oracle):
        super().__init__(csvid, record, actor, mode, rows, oracle)
        self.pump = 0
        self.target_name = None
        self.add('ally', actor, FLYER if csvid == HARPOONER else GROUNDED, 'battlefield')
        self.add('foil', self.foe, 'Millstone', 'battlefield')
        if csvid == GIANT:
            if mode == 'displaced_slot':
                self.add('foil_extra', self.foe, 'Millstone', 'battlefield')
            if mode != 'no_eligible':
                self.target_name = 'Shivan Dragon' if mode == 'backlash' else GROUNDED
            self.add_giant_foe()
        elif csvid in (STAG, INDRIK):
            if mode != 'own_only':
                self.target_name = 'Shivan Dragon' if mode == 'backlash' else 'Orcish Artillery'
        else:
            self.add_graves()
            if mode in ('undergrowth_lethal', 'foe_grave_only'):
                self.target_name = 'Drift of Phantasms'
            elif mode != 'no_eligible':
                self.target_name = FLYER
            self.add('grounded_foe', self.foe, GROUNDED, 'battlefield')
            if mode == 'reach':
                self.add('grounded_ally', self.actor, GROUNDED, 'battlefield')
        if self.target_name and csvid != GIANT:
            self.add('victim', self.foe, self.target_name, 'battlefield')

    def add_giant_foe(self):
        if self.target_name:
            self.add('victim', self.foe, self.target_name, 'battlefield')
        self.add('hexproof_foe', self.foe, 'Slippery Bogle', 'battlefield')

    def add_graves(self):
        pump_with_creatures = self.mode in ('undergrowth_lethal', 'zero_targets', 'no_eligible', 'reach')
        mine = ('Orcish Artillery', 'Prodigal Sorcerer', 'Mountain') if pump_with_creatures else ('Mountain', 'Plains', 'Island')
        theirs = ('Orcish Artillery', 'Prodigal Sorcerer', 'Plains')
        for seat, names in ((self.actor, mine), (self.foe, theirs)):
            for index, name in enumerate(names):
                self.add(f'grave_{seat}_{index}', seat, name, 'graveyard')
        self.pump = sum(self.oracle[name]['type_line'].startswith('Creature') for name in mine)

    def queue_entry(self):
        if self.csvid == GIANT:
            return
        if self.csvid != HARPOONER and self.actor == 0:
            self.emit('choose_trigger', 'source', player=self.actor)
        if not self.target_name:
            return
        if self.mode == 'decline':
            self.emit('choose_optional_target', 'victim', player=self.actor)
        elif self.mode in ('zero_targets', 'reach'):
            self.emit('choose_done_target', player=self.actor)
        else:
            self.emit('choose_target', 'victim', player=self.actor)

    def expected_fight(self):
        if not self.target_name or self.mode in ('decline', 'zero_targets', 'reach'):
            return
        self.result_of_damage('victim', stats(self.record)[0] + self.pump)
        self.result_of_damage('source', stats(self.oracle[self.target_name])[0])

    def battlefield_receipts(self):
        for ref, card in self.cards.items():
            if not self.oracle[card['name']]['type_line'].startswith('Creature'):
                continue
            if card['zone'] == 'battlefield':
                power, toughness = stats(self.oracle[card['name']])
                self.observe(ref, 'in_play', 1)
                self.observe(ref, 'power', power + (self.pump if ref == 'source' else 0))
                self.observe(ref, 'toughness', toughness)
                self.observe(ref, 'damage', self.damage.get(ref, 0))
            elif ref in ('source', 'victim'):
                self.observe(ref, 'in_play', 0)
        self.receipts()

    def reach_combat(self):
        self.emit('advance_turn')
        self.emit('untap_step', player=self.foe)
        self.emit('upkeep_step', player=self.foe)
        self.emit('turn', player=self.foe)
        self.pump = 0
        self.battlefield_receipts()
        self.emit('begin_attackers')
        self.emit('attack', 'victim', value=1)
        self.emit('finish_attackers')
        self.emit('begin_blockers')
        self.emit('block', 'grounded_ally', 'victim', value=0)
        self.emit('block', 'source', 'victim', value=1)
        self.emit('advance_combat')
        self.observe('source', 'blocking_count', 1)
        self.observe('victim', 'blocked', 1)
        self.emit('combat_damage')
        self.result_of_damage('victim', stats(self.record)[0])
        self.result_of_damage('source', stats(self.oracle[self.target_name])[0])
        self.battlefield_receipts()
        self.emit('end_combat')

    def build(self):
        self.emit('turn', player=self.actor)
        for player in (0, 1):
            self.mana('set_mana', player, ZERO)
        cost = cost_from_oracle(self.record)
        self.mana('set_mana', self.actor, cost)
        self.emit('refresh')
        self.emit('cast', 'source', value=1)
        self.emit('assert_choices_consumed')
        self.emit('assert_stack', value=1)
        self.observe('source', 'in_hand', 0)
        for player in (0, 1):
            self.mana('assert_mana', player, ZERO)
        self.queue_entry()
        self.emit('resolve')
        self.emit('assert_unused_choices', value=0)
        self.emit('assert_choices_consumed')
        if self.mode == 'decline':
            self.emit('assert_forced_target_choices', value=0)
        self.emit('refresh')
        self.cards['source']['zone'] = 'battlefield'
        self.expected_fight()
        self.battlefield_receipts()
        if self.mode == 'reach':
            self.reach_combat()


class AuraFight(Board):
    def __init__(self, csvid, record, actor, mode, rows, oracle):
        super().__init__(csvid, record, actor, mode, rows, oracle)
        if mode != 'foreign_only':
            self.add('host', self.actor, HOST, 'battlefield')
        target = {'trample': TRAMPLE_BLOCKER, 'lethal_fight': 'Shivan Dragon'}.get(mode, AURA_VICTIM)
        self.add('victim', self.foe, target, 'battlefield')
        if mode == 'lost_host':
            self.add('bounce', self.foe, BOUNCE, 'hand')

    def payment_receipts(self, actor, pool=ZERO):
        self.mana('assert_mana', actor, pool)
        self.mana('assert_mana', 1 - actor, ZERO)

    def observe_creatures(self):
        attached = self.cards['source']['zone'] == 'battlefield'
        self.observe('source', 'in_play', int(attached))
        for ref in ('host', 'victim'):
            if ref not in self.cards:
                continue
            card = self.cards[ref]
            in_play = card['zone'] == 'battlefield'
            self.observe(ref, 'in_play', int(in_play))
            if in_play:
                power, toughness = stats(self.oracle[card['name']])
                bonus = int(ref == 'host' and attached)
                self.observe(ref, 'power', power + bonus)
                self.observe(ref, 'toughness', toughness + bonus)
                self.observe(ref, 'damage', self.damage.get(ref, 0))

    def respond_to_aura(self):
        self.emit('announce_spell', 'source')
        self.mana('set_mana', self.foe, cost_from_oracle(self.oracle[BOUNCE]))
        self.emit('refresh')
        self.emit('choose_target', 'host', player=self.foe)
        self.emit('cast_response', 'bounce', value=1)
        self.emit('assert_choices_consumed')
        self.emit('assert_stack', value=2)
        self.payment_receipts(self.foe)
        self.emit('resolve_response')
        self.cards['source']['zone'] = self.cards['bounce']['zone'] = 'graveyard'
        self.cards['host']['zone'] = 'hand'
        self.emit('bind_hand', 'returned_host', player=self.actor, value=self.rows[HOST]['csvid'])

    def combat_with_trample(self):
        self.emit('begin_attackers')
        self.emit('attack', 'host', value=1)
        self.emit('finish_attackers')
        self.emit('begin_blockers')
        self.emit('block', 'victim', 'host', value=1)
        self.emit('advance_combat')
        self.observe('host', 'blocked', 1)
        self.observe('victim', 'blocking_count', 1)
        self.emit('combat_damage')
        self.emit('resolve')
        self.emit('end_combat')
        power, _ = stats(self.oracle[HOST])
        blocker_power, blocker_toughness = stats(self.oracle[TRAMPLE_BLOCKER])
        self.result_of_damage('victim', blocker_toughness)
        self.result_of_damage('host', blocker_power, bonus_toughness=1)
        self.life[self.foe] -= power + 1 - blocker_toughness
        self.observe_creatures()
        self.receipts()

    def build(self):
        self.emit('turn', player=self.actor)
        for player in (0, 1):
            self.mana('set_mana', player, ZERO)
        cost = cost_from_oracle(self.record)
        self.mana('set_mana', self.actor, cost)
        self.emit('refresh')
        if self.mode == 'foreign_only':
            self.emit('can_cast', 'source', value=0)
            self.observe('source', 'in_hand', 1)
            self.payment_receipts(self.actor, cost)
            self.observe_creatures()
            self.receipts()
            return

        self.emit('choose_target', 'host', player=self.actor)
        self.emit('cast', 'source', value=1)
        self.emit('assert_choices_consumed')
        self.emit('assert_stack', value=1)
        self.observe('source', 'in_hand', 0)
        self.payment_receipts(self.actor)
        if self.mode == 'lost_host':
            self.respond_to_aura()
        else:
            fights = self.mode in ('reciprocal', 'lethal_fight')
            if fights:
                if self.actor == 0:
                    self.emit('choose_trigger', 'source', player=0)
                self.emit('choose_target', 'victim', player=self.actor)
            # The human priority default declines the optional trigger.
            # The trample blocker has hexproof, so neither seat can fight it.
            self.emit('resolve')
            self.cards['source']['zone'] = 'battlefield'
            if fights:
                host_power, _ = stats(self.oracle[HOST])
                victim_power, _ = stats(self.oracle[self.cards['victim']['name']])
                self.result_of_damage('victim', host_power + 1)
                self.result_of_damage('host', victim_power, bonus_toughness=1)
                if self.cards['host']['zone'] == 'graveyard':
                    self.cards['source']['zone'] = 'graveyard'

        self.emit('assert_unused_choices', value=0)
        self.emit('assert_choices_consumed')
        self.emit('refresh')
        self.observe_creatures()
        self.receipts()
        if self.mode == 'trample':
            self.combat_with_trample()


@lru_cache(maxsize=1)
def profiles():
    return json.loads(PROFILE.read_bytes())


def classify(record, csvid):
    if not record or record.get('layout') != 'normal' or record.get('card_faces') or record.get('selected_face'):
        raise ValueError('whole normal entry-fight printing required')
    if csvid == CARTOUCHE:
        if (record.get('type_line') != 'Enchantment — Aura Cartouche' or record.get('mana_cost') != '{2}{G}'
                or record.get('oracle_text') != CARTOUCHE_RULE or record.get('keywords') != ['Fight', 'Enchant']):
            raise ValueError('complete printed Cartouche costs, choice and riders required')
        return dict(csvid=csvid, mana=cost_from_oracle(record))
    if csvid not in PRINTED:
        raise ValueError('unsupported entry-fight primary')
    _, subtype, cost, power, toughness, keywords, rule = PRINTED[csvid]
    if (record.get('type_line') != 'Creature — ' + subtype or record.get('mana_cost') != cost
            or record.get('oracle_text') != rule or record.get('keywords') != list(keywords)
            or (record.get('power'), record.get('toughness')) != (str(power), str(toughness))):
        raise ValueError('complete printed entry-fight cost, stats and riders required')
    return dict(csvid=csvid, mana=cost_from_oracle(record), stats=[power, toughness])


def variants(shape):
    csvid = shape['csvid']
    if csvid == CARTOUCHE:
        return [f'{mode}_{seat}' for mode in CARTOUCHE_MODES for seat in (0, 1)] + ['decline_0']
    return [f'{mode}_{seat}' for mode in MODES[csvid] for seat in (0, 1)]


def require_variants(shape, selected):
    if (type(selected) is not list or not selected or any(type(mode) is not str for mode in selected)
            or len(selected) != len(set(selected)) or set(selected) - set(variants(shape))):
        raise ValueError('distinct reviewed entry-fight variants required')


def spec(declaration, record):
    primary = profiles()['sources'].get(record.get('name', '')) if record else None
    if not primary or not exact(primary['oracle'], record):
        raise ValueError('exact complete pinned entry-fight Oracle record required')
    shape = classify(record, primary['registry']['csvid'])
    if (declaration.get('family') != FAMILY or type(declaration.get('version')) is not int
            or declaration['version'] != 2 or declaration.get('oracle_text') != record['oracle_text']
            or not exact(declaration.get('params'), shape)):
        raise ValueError('complete version 2 entry-fight declaration required')
    require_variants(shape, declaration.get('variants'))
    return dict(shape, family=FAMILY)


def validate_witnesses(oracle, rows):
    pins = profiles()
    if (set(pins) != {'sources', 'witnesses'} or set(pins['witnesses']) != WITNESSES
            or {pin['registry']['csvid'] for pin in pins['sources'].values()} != ALL_IDS):
        raise ValueError('complete entry-fight source and witness pins required')
    for name, pin in (pins['sources'] | pins['witnesses']).items():
        row = rows.get(name, {})
        if (not exact(oracle.get(name), pin['oracle']) or not exact(row, pin['registry'])
                or row.get('kind') != 'card' or row.get('handler') in (None, '0', 'NULL', 'card_dummy')
                or oracle[name].get('layout') != 'normal'):
            raise ValueError('pinned entry-fight card changed: ' + name)
        if name in pins['sources']:
            classify(oracle[name], row['csvid'])
    for name, (power, toughness, flying) in WITNESS_STATS.items():
        if stats(oracle[name]) != (power, toughness) or ('Flying' in oracle[name]['keywords']) != flying:
            raise ValueError('entry-fight witness changed power, toughness or flying: ' + name)
    if (stats(oracle[HOST]) != (1, 2) or 'Trample' in oracle[HOST]['keywords']
            or stats(oracle[AURA_VICTIM]) != (2, 2) or oracle[TRAMPLE_BLOCKER]['keywords'] != ['Hexproof']
            or oracle['Millstone']['type_line'] != 'Artifact'
            or oracle[BOUNCE]['oracle_text'] != "Return target creature to its owner's hand."):
        raise ValueError('Cartouche fight, attachment and trample witnesses changed')


def expand(name, csvid, declaration, shape, selected, rows, oracle):
    require_variants(shape, selected)
    validate_witnesses(oracle, rows)
    if profiles()['sources'][name]['registry']['csvid'] != csvid or csvid != shape['csvid']:
        raise ValueError('exact registered entry-fight primary required')
    cases = []
    for variant in selected:
        mode, seat = variant.rsplit('_', 1)
        board_type = AuraFight if csvid == CARTOUCHE else EntryFight
        board = board_type(csvid, oracle[name], int(seat), mode, rows, oracle)
        board.build()
        cases.append(dict(name=f'{PREFIX}{csvid}_{variant}', card=name, fixture='empty_main', seed=2633,
                          setup=board.setup, steps=board.steps, _recipe=dict(
                              family=FAMILY, version=2, variant=variant, params=declaration['params'],
                              oracle_text=declaration['oracle_text'], untested_components=GAPS.get(csvid, []))))
    return cases

