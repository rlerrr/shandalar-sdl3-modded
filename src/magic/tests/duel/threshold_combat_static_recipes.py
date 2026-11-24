"""Actual Threshold combat, upkeep, and mutable graveyard counts."""

from collections import Counter
from copy import deepcopy
from functools import lru_cache
import json
from pathlib import Path

from counter_condition_recurrence_recipes import exact


FAMILY = 'threshold_combat_static'
PROFILE = Path(__file__).with_name('data') / 'threshold_combat_static_wave8_profiles.json'
THRESHOLD = 'Threshold — As long as there are seven or more cards in your graveyard, '
CARDS = {
    5518: (('Flying',), "this creature gets +2/+2 and can't block.", 2, 2),
    5570: (("Fear (This creature can't be blocked except by artifact creatures and/or black creatures.)",),
           "this creature gets +2/+2 and can't block.", 2, 2),
    5626: (('Protection from black and from red',), 'this creature gets +1/+1 and has flying.', 1, 1),
    5627: (('Protection from black',), 'this creature gets +3/+3 and has flying.', 3, 3),
    5843: (('Flying',), 'this creature gets +1/+1 and has protection from black.', 1, 1),
    5751: (('Flying, vigilance',), 'this creature gets +3/+3, is black, has trample, and has '
                                  '"At the beginning of your upkeep, sacrifice a creature."', 3, 3),
}
COMMON = ('six', 'seven', 'opponent_only')
EXTRA = {
    5518: ('flying_attack',),
    5570: ('fear_black', 'fear_artifact'),
    5626: ('crossing',),
    5751: ('upkeep_six', 'upkeep_seven'),
}
SEVEN = {'seven', 'flying_attack', 'fear_black', 'fear_artifact', 'upkeep_seven',
         'lose_threshold', 'short_funding', 'countered'}
TRANSITIONS = ('gain_threshold', 'lose_threshold', 'replay')
GAINS = ('crossing', 'gain_threshold', 'replay', 'upkeep_crossed')
ADDITIONS = ('short_funding', 'countered', 'lose_threshold', 'replay')
CRYPT = "Tormod's Crypt"
WITNESSES = {'Mountain', 'Island', 'Plains', 'Grizzly Bears', 'Suntail Hawk', 'Muck Rats',
             "Mons's Goblin Raiders", 'Dusk Imp', 'Wind Drake', 'Ornithopter', "Traveler's Amulet",
             'Disenchant', 'Counterspell', CRYPT}
UPKEEP_SCOPE = 'Actual upkeep controls use the sole source as the sacrifice.'
DEFEND = {5518: 'Suntail Hawk', 5570: 'Grizzly Bears'}
ZONE = {'hand': 0, 'battlefield': 1, 'library': 2, 'graveyard': 3, 'exile': 4}


@lru_cache(maxsize=1)
def profiles():
    return json.loads(PROFILE.read_bytes())


def classify(record, csvid):
    from recipes import cost_from_oracle

    if (csvid not in CARDS or record.get('layout') != 'normal' or record.get('selected_face')
            or record.get('card_faces') or not record.get('type_line', '').startswith('Creature — ')):
        raise ValueError('whole pinned Threshold combat creature required')
    passives, clause, power, toughness = CARDS[csvid]
    if (record.get('oracle_text') != '\n'.join((*passives, THRESHOLD + clause))
            or 'Threshold' not in record.get('keywords', ())
            or any(not record.get(stat, '').isdigit() or int(record[stat]) < 1 for stat in ('power', 'toughness'))):
        raise ValueError('complete printed Threshold and combat clauses required')
    return dict(csvid=csvid, mana=cost_from_oracle(record), power_delta=power, toughness_delta=toughness)


def variants(shape):
    csvid = shape['csvid']
    additions = ADDITIONS + (('gain_threshold',) if csvid != 5626 else ())
    if csvid == 5751:
        additions += ('upkeep_crossed',)
    return [f'{mode}_{seat}' for mode in COMMON + EXTRA.get(csvid, ()) + additions for seat in (0, 1)]


def validate_variants(shape, selected):
    if (type(selected) is not list or not selected or any(type(mode) is not str for mode in selected)
            or len(set(selected)) != len(selected) or set(selected) - set(variants(shape))):
        raise ValueError('distinct supported Threshold combat variants required')


def spec(declaration, record):
    pinned = profiles()['sources'].get(record.get('name'))
    if not pinned or not exact(pinned['oracle'], record):
        raise ValueError('complete pinned Threshold combat primary required')
    shape = classify(record, pinned['registry']['csvid'])
    if (declaration.get('family') != FAMILY or type(declaration.get('version')) is not int
            or declaration['version'] != 2 or declaration.get('oracle_text') != record['oracle_text']
            or not exact(declaration.get('params'), shape)):
        raise ValueError('exact typed Threshold combat declaration required')
    validate_variants(shape, declaration.get('variants'))
    return dict(shape, family=FAMILY)


def validate_witnesses(oracle, rows):
    pins = profiles()
    if set(pins) != {'sources', 'witnesses'} or set(pins['witnesses']) != WITNESSES:
        raise ValueError('complete independent Threshold combat witnesses required')
    for group, entries in pins.items():
        for name, pin in entries.items():
            row = rows.get(name, {})
            if (not exact(oracle.get(name), pin['oracle']) or not exact(row, pin['registry'])
                    or row.get('kind') != 'card' or row.get('handler') in (None, '0', 'NULL')
                    or oracle[name].get('layout') != 'normal'):
                raise ValueError('exact Threshold card or witness changed: ' + name)
            if group == 'sources':
                if row['handler'] == 'card_dummy':
                    raise ValueError('implemented Threshold combat primary required')
                classify(oracle[name], row['csvid'])


class Duel:
    def __init__(self, csvid, record, actor, mode, rows, oracle):
        self.csvid, self.record, self.actor, self.foe, self.mode = csvid, record, actor, 1 - actor, mode
        self.rows, self.oracle = rows, oracle
        self.setup, self.steps, self.cards = [], [], {}
        self.life = [20, 20]
        self.active = mode in SEVEN
        self.defending = csvid in DEFEND and mode in COMMON + TRANSITIONS
        self.upkeep = mode.startswith('upkeep_')
        self.blocker = None
        self.denied = []
        self.add('source', actor, record['name'], 'hand')
        for player in (0, 1):
            count = (7 if self.active else 6) if player == actor else 7 if mode == 'opponent_only' else 0
            for i in range(count):
                self.add(f'g_{player}_{i}', player, 'Mountain', 'graveyard')
            for i, name in enumerate(('Island', 'Plains')):
                self.add(f'd_{player}_{i}', player, name, 'library')
        if not self.upkeep:
            self.configure_combat()
        if mode in GAINS:
            self.add('artifact', self.foe, "Traveler's Amulet", 'battlefield')
            self.add('disenchant', self.actor, 'Disenchant', 'hand')
        if mode == 'lose_threshold':
            self.add('crypt', self.actor, CRYPT, 'battlefield')
        if mode == 'countered':
            self.add('counter', self.foe, 'Counterspell', 'hand')

    def configure_combat(self):
        if self.defending:
            self.add('attacker', self.foe, DEFEND[self.csvid], 'battlefield')
            return
        role = {
            5518: ('Grizzly Bears',),
            5570: ('Grizzly Bears',),
            5626: ('Grizzly Bears', 'Muck Rats', "Mons's Goblin Raiders"),
            5627: ('Grizzly Bears', 'Muck Rats'),
            5843: ('Dusk Imp', 'Grizzly Bears'),
            5751: ('Wind Drake', 'Grizzly Bears'),
        }[self.csvid]
        for i, name in enumerate(role):
            self.add(f'blocker_{i}', self.foe, name, 'battlefield')
        if self.mode in ('fear_black', 'fear_artifact'):
            self.add('fear_witness', self.foe, 'Muck Rats' if self.mode == 'fear_black' else 'Ornithopter', 'battlefield')
            self.blocker, self.denied = 'fear_witness', ['blocker_0']
        elif self.csvid in (5626, 5627):
            self.blocker = None if self.active else 'blocker_0'
            self.denied = [f'blocker_{i}' for i in range(1, len(role))]
            if self.active:
                self.denied.append('blocker_0')
        elif self.csvid == 5843:
            self.blocker, self.denied = (None, ['blocker_0', 'blocker_1']) if self.active else ('blocker_0', ['blocker_1'])
        elif self.csvid == 5751:
            self.blocker, self.denied = 'blocker_0', ['blocker_1']
        else:
            self.denied = ['blocker_0']

    def add(self, ref, player, name, zone):
        from recipes import card

        self.setup.append(card(name, ref, player, zone))
        self.cards[ref] = dict(name=name, player=player, zone=zone, damage=0)

    def emit(self, op, ref='', other='', prop='', *, player=0, value=0, zone=0, index=0, color=0):
        from recipes import step

        if op in ('untap_step', 'upkeep_step'):
            self.steps.append(step(op, player=player))
            return
        fields = dict(player=player, value=value, index=index, color=color)
        if ref:
            fields['ref'] = ref
        if other:
            fields['other'] = other
        if prop:
            fields['property'] = prop
        if op in ('assert_zone', 'assert_zone_multiplicity'):
            fields['zone'] = next(name for name, number in ZONE.items() if number == zone)
        self.steps.append(step(op, **fields))

    def observe(self, ref, prop, value):
        self.emit('assert_card', ref, prop=prop, value=value)

    def mana(self, op, player, pool):
        for color, value in enumerate(pool):
            self.emit(op, player=player, value=value, color=color)

    def cast_pending(self, ref, target=None, *, allowed=True):
        from recipes import cost_from_oracle

        cost = cost_from_oracle(self.oracle[self.cards[ref]['name']])
        if not allowed:
            first_owed = next(color for color, amount in enumerate(cost) if amount)
            cost[first_owed] -= 1
        self.mana('set_mana', self.actor, cost)
        self.emit('refresh')
        if target:
            self.emit('choose_target', target, player=self.actor)
        self.emit('cast', ref, value=int(allowed))
        self.emit('assert_choices_consumed')
        self.emit('assert_stack', value=int(allowed))
        self.mana('assert_mana', self.actor, [0] * 6 if allowed else cost)

    def cast(self, ref, destination, target=None):
        self.cast_pending(ref, target)
        self.emit('resolve')
        self.emit('assert_choices_consumed')
        self.emit('assert_stack')
        self.cards[ref]['zone'] = destination
        self.emit('refresh')

    def stats(self, ref):
        record = self.oracle[self.cards[ref]['name']]
        power, toughness = int(record['power']), int(record['toughness'])
        if ref == 'source' and self.active:
            power += CARDS[self.csvid][2]
            toughness += CARDS[self.csvid][3]
        return power, toughness

    def receipts(self):
        self.emit('refresh')
        if self.cards['source']['zone'] == 'battlefield':
            power, toughness = self.stats('source')
            self.observe('source', 'in_play', 1)
            self.observe('source', 'power', power)
            self.observe('source', 'toughness', toughness)
            if self.csvid == 5751:
                self.observe('source', 'color', 2 if self.active else 32)
        for ref, card in self.cards.items():
            if card['zone'] == 'battlefield' and card['damage']:
                self.observe(ref, 'damage', card['damage'])
        identities = sorted({card['name'] for card in self.cards.values() if card['zone'] != 'library'})
        zones = ('hand', 'battlefield', 'graveyard')
        if self.mode == 'lose_threshold':
            zones += ('exile',)
        for player in (0, 1):
            self.emit('assert_life', player=player, value=self.life[player])
            for zone in zones:
                occurrences = Counter(card['name'] for card in self.cards.values()
                                      if card['player'] == player and card['zone'] == zone)
                self.emit('assert_zone', player=player, zone=ZONE[zone], value=sum(occurrences.values()))
                for name in identities:
                    self.emit('assert_zone_multiplicity', player=player, zone=ZONE[zone],
                              index=self.rows[name]['csvid'], value=occurrences[name])

    def advance(self, player, *, upkeep=False):
        self.emit('advance_turn')
        self.emit('untap_step', player=player)
        if upkeep:
            if self.active and player == self.actor == 0:
                # Player 0 selects the sole creature. The native AI path selects its sole legal creature directly.
                self.emit('choose_target', 'source', player=0)
            self.emit('upkeep_step', player=player)
            self.emit('assert_choices_consumed')
            if self.active and player == self.actor:
                self.cards['source']['zone'] = 'graveyard'
        self.emit('turn', player=player)

    def cross(self):
        self.observe('artifact', 'in_play', 1)
        self.cast('disenchant', 'graveyard', target='artifact')
        self.cards['artifact']['zone'] = 'graveyard'
        self.active = True
        if not self.upkeep:
            if self.csvid in (5626, 5627):
                self.blocker = None
                self.denied.append('blocker_0')
            elif self.csvid == 5843:
                self.blocker, self.denied = None, ['blocker_0', 'blocker_1']
        self.receipts()

    def lose_threshold(self):
        self.emit('choose_player', player=self.actor, value=self.actor)
        self.emit('activate', 'crypt', value=1)
        self.emit('assert_choices_consumed')
        self.emit('assert_stack', value=1)
        self.cards['crypt']['zone'] = 'graveyard'
        self.emit('resolve')
        self.emit('assert_choices_consumed')
        self.emit('assert_stack')
        for card in self.cards.values():
            if card['player'] == self.actor and card['zone'] == 'graveyard':
                card['zone'] = 'exile'
        self.active = False
        if self.csvid in (5626, 5627, 5843):
            self.blocker = 'blocker_0'
            self.denied.remove('blocker_0')
        self.receipts()

    def deny_entry(self):
        from recipes import cost_from_oracle

        if self.mode == 'short_funding':
            self.cast_pending('source', allowed=False)
            self.observe('source', 'in_hand', 1)
        else:
            self.cast_pending('source')
            self.emit('announce_spell', 'source')
            self.mana('set_mana', self.foe, cost_from_oracle(self.oracle['Counterspell']))
            self.emit('refresh')
            self.emit('cast_response', 'counter', value=1)
            self.emit('assert_stack', value=2)
            self.emit('resolve_response')
            self.emit('assert_choices_consumed')
            self.emit('assert_stack')
            self.mana('assert_mana', self.foe, [0] * 6)
            self.cards['source']['zone'] = self.cards['counter']['zone'] = 'graveyard'
        self.receipts()

    def combat(self):
        if self.defending:
            attacker = 'attacker'
            blocker = None if self.active else 'source'
            denied = ['source'] if self.active else []
        else:
            attacker, blocker, denied = 'source', self.blocker, self.denied
        self.emit('begin_attackers')
        self.emit('attack', attacker, value=1)
        self.observe(attacker, 'attacking', 1)
        self.observe(attacker, 'tapped', int(self.csvid != 5751))
        self.emit('finish_attackers')
        self.emit('begin_blockers')
        for ref in denied:
            self.emit('block', ref, attacker, value=0)
        if blocker:
            self.emit('block', blocker, attacker, value=1)
        self.emit('advance_combat')
        self.observe(attacker, 'blocked', int(blocker is not None))
        self.emit('combat_damage')
        self.emit('resolve')
        self.emit('end_combat')
        self.emit('assert_choices_consumed')
        power, toughness = self.stats(attacker)
        attacked_player = self.actor if self.defending else self.foe
        if blocker is None:
            self.life[attacked_player] -= power
        else:
            defending_power, defending_toughness = self.stats(blocker)
            self.cards[attacker]['damage'] = defending_power
            self.cards[blocker]['damage'] = power
            if defending_power >= toughness:
                self.cards[attacker]['zone'] = 'graveyard'
            if power >= defending_toughness:
                self.cards[blocker]['zone'] = 'graveyard'
            if self.csvid == 5751 and self.active:
                self.life[attacked_player] -= power - defending_toughness
        if self.csvid == 5751:
            self.observe('source', 'tapped', 0)
        self.receipts()

    def scheduled_steps(self):
        self.advance(self.foe, upkeep=self.upkeep)
        if self.upkeep:
            self.receipts()
        if not self.defending:
            self.advance(self.actor, upkeep=self.upkeep)
        if self.upkeep:
            self.receipts()
            return
        if self.mode == 'crossing':
            self.cross()
        self.combat()


    def build(self):
        from recipes import step

        self.emit('turn', player=self.actor)
        self.mana('set_mana', self.foe, [0] * 6)
        if self.mode in ('short_funding', 'countered'):
            self.deny_entry()
            return
        self.cast('source', 'battlefield')
        self.receipts()
        cut = len(self.steps)
        if self.mode in ('gain_threshold', 'replay', 'upkeep_crossed'):
            self.cross()
        elif self.mode == 'lose_threshold':
            self.lose_threshold()
        self.scheduled_steps()
        if self.mode == 'replay':
            suffix = self.steps[cut:]
            self.steps[cut:] = ([step('capture')] + suffix
                                + [step('record_state'), step('ai_roundtrip'), step('restore')]
                                + deepcopy(suffix) + [step('assert_recorded_state')])


def expand(name, csvid, declaration, shape, selected, rows, oracle):
    validate_variants(shape, selected)
    validate_witnesses(oracle, rows)
    if profiles()['sources'][name]['registry']['csvid'] != csvid:
        raise ValueError('exact Threshold combat primary CSVID required')
    cases = []
    for variant in selected:
        mode, actor = variant.rsplit('_', 1)
        game = Duel(csvid, oracle[name], int(actor), mode, rows, oracle)
        game.build()
        cases.append(dict(
            name=f'ThresholdCombatStaticRecipe_{csvid}_{variant}', card=name, fixture='empty_main', seed=2370,
            setup=game.setup, steps=game.steps,
            _recipe=dict(family=FAMILY, version=2, variant=variant, params=declaration['params'],
                         oracle_text=declaration['oracle_text'],
                         untested_components=[UPKEEP_SCOPE] if mode.startswith('upkeep_') else []),
        ))
    return cases
