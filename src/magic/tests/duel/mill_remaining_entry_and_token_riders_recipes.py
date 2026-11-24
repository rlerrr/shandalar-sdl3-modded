"""Opening, entry, and paid graveyard or token effects with exact mill receipts."""

from functools import lru_cache
import json
from pathlib import Path

from death_counter_trigger_recipes import exact
from mill_death_and_entry_recipes import Duel as MillBoard, stats
from recipes import cost_from_oracle


FAMILY = 'mill_remaining_entry_and_token_riders'
PROFILE = Path(__file__).with_name('data') / 'mill_remaining_entry_and_token_riders_wave8_profiles.json'
PREFIX = 'MillRemainingEntryAndTokenRidersRecipe_'
CHANCELLOR, MINDSEEKER, GISA, GEARHULK = 11581, 13305, 16110, 16320
STITCHER, MEMORIES = 14487, 15816
PRINTED = {
    CHANCELLOR: ('new_phyrexia.c', '{4}{U}{U}{U}', 'Creature — Phyrexian Sphinx', (5, 7), ('Flying', 'Mill'),
                 'You may reveal this card from your opening hand. If you do, at the beginning of the first upkeep, '
                 'each opponent mills seven cards.\nFlying\nWhen this creature enters, you may cast target instant '
                 "or sorcery card from an opponent's graveyard without paying its mana cost."),
    MINDSEEKER: ('m14.c', '{4}{U}{U}', 'Creature — Fish Illusion', (4, 4), ('Flying', 'Mill'),
                 'Flying\nWhen this creature enters, target opponent mills five cards. You may cast an instant '
                 'or sorcery spell from among them without paying its mana cost.'),
    GISA: ('eldritch_moon.c', '{2}{U}{B}', 'Legendary Creature — Human Wizard', (4, 4), ('Mill',),
           'When Gisa and Geralf enters, mill four cards.\nOnce during each of your turns, you may cast a Zombie '
           'creature spell from your graveyard.'),
    GEARHULK: ('kaladesh.c', '{4}{R}{R}', 'Artifact Creature — Construct', (6, 6), ('First strike', 'Mill'),
               'First strike\nWhen this creature enters, target opponent may have you draw three cards. '
               "If the player doesn't, you mill three cards, then this creature deals damage to that player "
               'equal to the total mana value of those cards.'),
    STITCHER: ('commander_2014.c', '{3}{U}{U}', 'Legendary Creature — Human Wizard', (3, 4), ('Mill',),
               '{2}{U}, {T}: Each player mills three cards. Exile up to two creature cards put into graveyards '
               'this way. Create an X/X blue Zombie creature token, where X is the total power of the cards exiled this way.'),
    MEMORIES: ('shadows_over_innistrad.c', '{2}{U}', 'Enchantment', None, ('Investigate', 'Mill'),
               'When this enchantment enters, investigate. (Create a Clue token. It\'s an artifact with '
               '"{2}, Sacrifice this token: Draw a card.")\nWhenever you sacrifice a Clue, target player mills three cards.'),
}
MODES = {
    CHANCELLOR: ('free_cast', 'decline_cast', 'own_only', 'grave_shroud', 'opening_reveal', 'opening_decline', 'flying'),
    MINDSEEKER: ('free_cast', 'decline_cast', 'old_only', 'short', 'hexproof', 'flying'),
    GISA: ('entry', 'short', 'zombie_once', 'zombie_offturn', 'foe_zombie_only', 'zombie_underpaid'),
    GEARHULK: ('draw', 'mill', 'short_mill', 'zero_value', 'hexproof', 'first_strike'),
    STITCHER: ('two', 'one', 'zero', 'foe_only', 'short', 'sick', 'underpaid'),
    MEMORIES: ('entry', 'foe', 'self', 'short_target', 'other_clue', 'underpaid'),
}
DRAW, OLD = 'Reach Through Mists', 'Think Twice'
ZOMBIE, SECOND_ZOMBIE = 'Diregraf Ghoul', 'Festering Goblin'
GRAVESTONE, HEXPROOF = 'Silent Gravestone', 'Leyline of Sanctity'
GROUND, AIR, DRAGON = 'Dazzling Ramparts', 'Drift of Phantasms', 'Shivan Dragon'
ARTIFACT = 'Millstone'
CLUE, ZOMBIE_TOKEN = 'Clue', 'Zombie'
INSPECTOR = 'Thraben Inspector'
STOCK = ['Island', 'Forest', 'Plains', 'Mountain', 'Swamp', 'Wastes', 'Island', 'Mountain', 'Forest']
FOE_STOCK = ['Swamp', 'Mountain', 'Wastes', 'Plains', 'Forest', 'Island', 'Mountain', 'Forest', 'Swamp']
ZERO = [0] * 6
TOKENS = {CLUE: (18711, 'manalinkeh_card_clue'), ZOMBIE_TOKEN: (18860, 'manalinkeh_card_zombie')}
WITNESSES = set(STOCK + FOE_STOCK) | {
    DRAW, OLD, ZOMBIE, SECOND_ZOMBIE, GRAVESTONE, HEXPROOF, GROUND, AIR, DRAGON, ARTIFACT, INSPECTOR,
}


class Duel(MillBoard):
    def __init__(self, csvid, record, actor, mode, rows, oracle):
        self.csvid, self.record, self.actor, self.foe, self.mode = csvid, record, actor, 1 - actor, mode
        self.rows, self.oracle = rows, oracle
        self.cards, self.setup, self.steps, self.mill_events = {}, [], [], []
        self.life = [20, 20]
        self.active = actor
        self.library = {actor: STOCK.copy(), self.foe: FOE_STOCK.copy()}
        self.add('source', actor, record['name'], 'hand')
        if csvid == CHANCELLOR:
            seat = actor if mode == 'own_only' else self.foe
            if mode in ('free_cast', 'decline_cast', 'own_only', 'grave_shroud'):
                self.add('grave_spell', seat, DRAW, 'graveyard')
            if mode == 'grave_shroud':
                self.add('shield', self.foe, GRAVESTONE, 'battlefield')
        elif csvid == MINDSEEKER:
            self.add('old_spell', self.foe, OLD, 'graveyard')
            if mode in ('free_cast', 'decline_cast'):
                self.library[self.foe][2] = DRAW
            if mode == 'short':
                self.library[self.foe] = [DRAW, 'Mountain']
        elif csvid == GISA:
            if mode.startswith('zombie_'):
                self.library[actor][:2] = [ZOMBIE, SECOND_ZOMBIE]
            if mode == 'short':
                self.library[actor] = ['Plains', 'Swamp']
            if mode == 'foe_zombie_only':
                self.add('foe_zombie', self.foe, ZOMBIE, 'graveyard')
        elif csvid == GEARHULK:
            if mode != 'zero_value':
                self.library[actor][:3] = [ARTIFACT, 'Forest', DRAGON]
            if mode == 'short_mill':
                self.library[actor] = [ARTIFACT, 'Forest']
        self.follow_on_setup()
        if mode == 'flying':
            self.add('ground', self.foe, GROUND, 'battlefield')
            self.add('air', self.foe, AIR, 'battlefield')
        if mode == 'hexproof':
            self.add('shield', self.foe, HEXPROOF, 'battlefield')
        if mode == 'first_strike':
            self.add('blocker', self.foe, DRAGON, 'battlefield')
        self.library_refs = {seat: [] for seat in (0, 1)}
        for seat, cards in self.library.items():
            for index, card in enumerate(cards):
                ref = f'library_{seat}_{index}'
                self.add(ref, seat, card, 'library')
                self.library_refs[seat].append(ref)
        self.initial_library = {seat: cards.copy() for seat, cards in self.library.items()}

    def follow_on_setup(self):
        if self.csvid == STITCHER:
            if self.mode in ('two', 'one', 'zero'):
                self.library[self.actor][:2] = [DRAGON, ZOMBIE]
            if self.mode in ('two', 'zero', 'foe_only'):
                self.library[self.foe][0] = SECOND_ZOMBIE
            if self.mode == 'short':
                self.library[self.actor] = [DRAGON]
                self.library[self.foe] = ['Forest']
            self.add('old_creature', self.foe, AIR, 'graveyard')
        elif self.csvid == MEMORIES:
            if self.mode == 'short_target':
                self.library[self.foe] = ['Swamp']
            if self.mode == 'other_clue':
                self.add('inspector', self.foe, INSPECTOR, 'hand')

    def begin(self):
        self.emit('present_ai_dialogs', value=1)
        for seat in (0, 1):
            self.mana('set_mana', seat, ZERO)
        if self.mode.startswith('opening_'):
            # Production's first human upkeep inspects both opening hands.
            self.active = 1
            self.emit('turn', player=1)
            self.observe('source', 'in_hand', 1)
            self.receipts()
            self.emit('advance_turn')
            self.emit('untap_step', player=0)
            reveal = self.mode == 'opening_reveal'
            self.emit('choose_dialog', player=self.actor, value=int(not reveal))
            self.emit('upkeep_step', player=0)
            if reveal:
                self.shift_prefix(self.foe, 7, 'graveyard')
            self.emit('assert_unused_choices', value=0)
            self.emit('assert_choices_consumed')
            self.active = 0
            self.observe('source', 'in_hand', 1)
            self.receipts()
            if self.actor == 1:
                self.go(1)
        self.emit('turn', player=self.actor)

    def enter_source(self):
        self.paid('source')
        casts_draw = self.mode == 'free_cast' or (self.csvid == MINDSEEKER and self.mode == 'short')
        if self.csvid in (CHANCELLOR, MINDSEEKER):
            if casts_draw:
                self.emit('choose_zone_card', player=self.actor, value=self.rows[DRAW]['csvid'])
            elif self.mode not in ('hexproof', 'grave_shroud'):
                self.emit('choose_optional_zone', player=self.actor, value=-1)
        if self.csvid == GEARHULK and self.mode != 'hexproof':
            self.emit('choose_player', player=self.actor, value=self.foe)
            self.emit('choose_dialog', player=self.foe, value=0 if self.mode == 'draw' else 1)
        self.settle()
        self.cards['source']['zone'] = 'battlefield'
        if self.csvid == MINDSEEKER and self.mode != 'hexproof':
            self.shift_prefix(self.foe, 5, 'graveyard')
        if casts_draw:
            # The opposing instant resolves under our control and returns to its owner's graveyard.
            self.shift_prefix(self.actor, 1, 'hand')
        if self.csvid == GISA:
            self.shift_prefix(self.actor, 4, 'graveyard')
        if self.csvid == GEARHULK and self.mode != 'hexproof':
            if self.mode == 'draw':
                self.shift_prefix(self.actor, 3, 'hand')
            else:
                self.shift_prefix(self.actor, 3, 'graveyard')
                self.life[self.foe] -= sum(sum(cost_from_oracle(self.oracle[card]))
                                           for card in self.mill_events[-1]['cards']
                                           if self.oracle[card]['mana_cost'])
        self.observe('source', 'in_play', 1)
        if self.csvid != MEMORIES:
            power, toughness = stats(self.record)
            self.observe('source', 'power', power)
            self.observe('source', 'toughness', toughness)
        else:
            self.produced_token('clue', self.actor, CLUE)
        for seat in (0, 1):
            self.mana('assert_mana', seat, ZERO)
        self.receipts()

    def zombie_permission(self):
        if self.mode == 'zombie_offturn':
            self.go(self.foe)
        paid = cost_from_oracle(self.oracle[ZOMBIE])
        funding = ZERO if self.mode == 'zombie_underpaid' else paid
        self.mana('set_mana', self.actor, funding)
        self.emit('refresh')
        available = self.mode == 'zombie_once'
        self.emit('can_activate', 'source', value=int(available))
        self.mana('assert_mana', self.actor, funding)
        if not available:
            self.receipts()
            return
        selected = 'library_%d_0' % self.actor
        self.emit('choose_zone_card', player=self.actor, value=self.rows[ZOMBIE]['csvid'])
        self.emit('activate', 'source', value=1)
        self.settle()
        self.cards[selected]['zone'] = 'battlefield'
        self.mana('assert_mana', self.actor, ZERO)
        self.receipts()
        # A second affordable Zombie remains, forcing the genuine once-per-turn restriction.
        self.mana('set_mana', self.actor, paid)
        self.emit('can_activate', 'source', value=0)
        self.mana('assert_mana', self.actor, paid)
        self.mana('set_mana', self.actor, ZERO)
        self.go(self.foe)
        self.go(self.actor)
        self.mana('set_mana', self.actor, paid)
        self.emit('can_activate', 'source', value=1)
        self.mana('assert_mana', self.actor, paid)
        self.receipts()

    def first_strike(self):
        self.go(self.foe)
        self.go(self.actor)
        self.emit('begin_attackers')
        self.emit('attack', 'source', value=1)
        self.emit('finish_attackers')
        self.emit('begin_blockers')
        self.emit('block', 'blocker', 'source', value=1)
        self.emit('advance_combat')
        self.observe('source', 'blocked', 1)
        self.emit('combat_damage')
        self.emit('resolve')
        self.emit('end_combat')
        self.cards['blocker']['zone'] = 'graveyard'
        self.observe('source', 'in_play', 1)
        self.observe('source', 'damage', 0)
        self.observe('blocker', 'in_play', 0)
        self.receipts()

    def flight(self):
        if self.csvid != CHANCELLOR:
            super().flight()
            return
        self.go(self.foe)
        self.go(self.actor)
        self.emit('begin_attackers')
        self.emit('attack', 'source', value=1)
        self.emit('finish_attackers')
        self.emit('begin_blockers')
        self.emit('block', 'ground', 'source', value=0)
        self.emit('block', 'air', 'source', value=1)
        self.emit('advance_combat')
        self.observe('source', 'blocked', 1)
        self.observe('air', 'blocking_count', 1)
        self.emit('combat_damage')
        self.emit('resolve')
        self.emit('end_combat')
        self.observe('source', 'in_play', 1)
        self.observe('air', 'in_play', 0)
        self.cards['air']['zone'] = 'graveyard'
        self.receipts()

    def produced_token(self, ref, owner, name, *, power=None):
        self.cards[ref] = dict(name=name, player=owner, zone='battlefield')
        if name == CLUE:
            self.emit('bind_permanent_copy', ref, player=owner, index=0, value=self.rows[name]['csvid'])
            attributes = {'token': 1, 'artifact': 1, 'color': 0, 'subtype_Clue': 1}
        else:
            attributes = {'token': 1, 'artifact': 0, 'color': 4, 'subtype_Zombie': 1,
                          'power': power, 'toughness': power}
        for prop, amount in attributes.items():
            self.emit('assert_matching_permanents', prop=prop, player=owner,
                      index=self.rows[name]['csvid'], value=amount)

    def stitcher(self):
        if self.mode != 'sick':
            self.go(self.foe)
            self.go(self.actor)
        cost = cost_from_oracle({'mana_cost': '{2}{U}'})
        funding = [3, 0, 0, 0, 0, 0] if self.mode == 'underpaid' else cost
        enabled = self.mode not in ('sick', 'underpaid')
        self.mana('set_mana', self.actor, funding)
        self.emit('refresh')
        self.observe('source', 'sick', int(self.mode == 'sick'))
        self.emit('can_activate', 'source', value=int(enabled))
        if not enabled:
            self.mana('assert_mana', self.actor, funding)
            self.receipts()
            return
        self.emit('activate', 'source', value=1)
        self.emit('assert_stack', value=1)
        self.observe('source', 'tapped', 1)
        self.mana('assert_mana', self.actor, ZERO)
        self.emit('can_activate', 'source', value=0)
        selections = {
            'two': ((DRAGON, ZOMBIE), ()),
            'one': ((DRAGON, None), (None,)),
            'zero': ((None,), (None,)),
            'foe_only': ((None,), (SECOND_ZOMBIE, None)),
            'short': ((DRAGON, None), (None,)),
        }[self.mode]
        power = 0
        for seat, decisions in zip((self.actor, self.foe), selections):
            prefix_refs = self.library_refs[seat][:3]
            self.shift_prefix(seat, 3, 'graveyard')
            for name in decisions:
                if name is None:
                    self.emit('choose_optional_zone', player=self.actor, value=-1)
                    continue
                self.emit('choose_zone_card', player=self.actor, value=self.rows[name]['csvid'])
                selected, = [ref for ref in prefix_refs if self.cards[ref]['name'] == name]
                self.cards[selected]['zone'] = 'exile'
                power += stats(self.oracle[name])[0]
        self.settle()
        if power:
            self.produced_token('zombie', self.actor, ZOMBIE_TOKEN, power=power)
        else:
            self.emit('assert_zone_multiplicity', player=self.actor, zone='battlefield',
                      index=self.rows[ZOMBIE_TOKEN]['csvid'], value=0)
        self.observe('source', 'tapped', 1)
        self.receipts()

    def memories(self):
        if self.mode == 'entry':
            return
        seat, ref = self.actor, 'clue'
        if self.mode == 'other_clue':
            self.go(self.foe)
            self.paid('inspector')
            self.settle()
            self.cards['inspector']['zone'] = 'battlefield'
            seat, ref = self.foe, 'foe_clue'
            self.produced_token(ref, seat, CLUE)
            self.receipts()
        enabled = self.mode != 'underpaid'
        funding = [2 if enabled else 1, 0, 0, 0, 0, 0]
        self.mana('set_mana', seat, funding)
        self.emit('refresh')
        self.emit('can_activate', ref, value=int(enabled))
        if not enabled:
            self.mana('assert_mana', seat, funding)
            self.receipts()
            return
        if seat == self.actor:
            target = self.actor if self.mode == 'self' else self.foe
            self.emit('choose_player', player=self.actor, value=target)
            self.shift_prefix(target, 3, 'graveyard')
        self.emit('activate', ref, value=1)
        self.mana('assert_mana', seat, ZERO)
        self.settle()
        self.cards[ref]['zone'] = 'gone'
        self.shift_prefix(seat, 1, 'hand')
        self.receipts()

    def build(self):
        self.begin()
        self.enter_source()
        if self.mode == 'flying':
            self.flight()
        elif self.mode == 'first_strike':
            self.first_strike()
        elif self.csvid == GISA and self.mode not in ('entry', 'short'):
            self.zombie_permission()
        elif self.csvid == STITCHER:
            self.stitcher()
        elif self.csvid == MEMORIES:
            self.memories()


@lru_cache(maxsize=1)
def profiles():
    return json.loads(PROFILE.read_bytes())


def classify(record, csvid):
    if (csvid not in PRINTED or not record or record.get('layout') != 'normal'
            or record.get('selected_face') or record.get('card_faces')):
        raise ValueError('whole normal mill rider record required')
    _, cost, type_line, printed_stats, keywords, rule = PRINTED[csvid]
    if (record.get('type_line') != type_line or record.get('mana_cost') != cost
            or record.get('keywords') != list(keywords) or record.get('oracle_text') != rule
            or (printed_stats is not None and stats(record) != printed_stats)):
        raise ValueError('complete printed mill riders and costs required')
    return dict(csvid=csvid, mana=cost_from_oracle(record), stats=list(printed_stats) if printed_stats is not None else None)


def variants(shape):
    return [f'{mode}_{seat}' for mode in MODES[shape['csvid']] for seat in (0, 1)]


def require_variants(shape, selected):
    if (type(selected) is not list or not selected or any(type(mode) is not str for mode in selected)
            or len(selected) != len(set(selected)) or set(selected) - set(variants(shape))):
        raise ValueError('distinct reviewed mill rider variants required')


def spec(declaration, record):
    primary = profiles()['sources'].get(record.get('name', '')) if record else None
    if not primary or not exact(primary['oracle'], record):
        raise ValueError('exact pinned mill rider record required')
    shape = classify(record, primary['registry']['csvid'])
    if (declaration.get('family') != FAMILY or type(declaration.get('version')) is not int
            or declaration['version'] != 2 or declaration.get('oracle_text') != record['oracle_text']
            or not exact(declaration.get('params'), shape)):
        raise ValueError('complete version 2 mill rider declaration required')
    require_variants(shape, declaration.get('variants'))
    return dict(shape, family=FAMILY)


def validate_witnesses(oracle, rows):
    pins = profiles()
    if (set(pins) != {'sources', 'witnesses', 'tokens'} or set(pins['witnesses']) != WITNESSES
            or set(pins['tokens']) != set(TOKENS)
            or {pin['registry']['csvid'] for pin in pins['sources'].values()} != set(PRINTED)):
        raise ValueError('complete mill rider source, witness and generated token pins required')
    for name, pin in (pins['sources'] | pins['witnesses'] | pins['tokens']).items():
        row, record = rows.get(name, {}), oracle.get(name, {})
        if (not exact(record, pin['oracle']) or not exact(row, pin['registry'])
                or row.get('kind') != 'card' or row.get('handler') in (None, '0', 'NULL', 'card_dummy')):
            raise ValueError('pinned mill rider card changed: ' + name)
        if name in TOKENS:
            if (row['csvid'], row['handler']) != TOKENS[name] or record['layout'] != 'token':
                raise ValueError('native mill rider token identity changed: ' + name)
        elif record.get('layout') != 'normal' or record.get('card_faces'):
            raise ValueError('normal whole mill rider card required: ' + name)
        if name in pins['sources']:
            classify(record, row['csvid'])


def expand(name, csvid, declaration, shape, selected, rows, oracle):
    require_variants(shape, selected)
    validate_witnesses(oracle, rows)
    if profiles()['sources'][name]['registry']['csvid'] != csvid or csvid != shape['csvid']:
        raise ValueError('exact registered mill rider primary required')
    cases = []
    for variant in selected:
        mode, seat = variant.rsplit('_', 1)
        board = Duel(csvid, oracle[name], int(seat), mode, rows, oracle)
        board.build()
        cases.append(dict(name=f'{PREFIX}{csvid}_{variant}', card=name, fixture='empty_main', seed=2809,
                          setup=board.setup, steps=board.steps, _recipe=dict(
                              family=FAMILY, version=2, variant=variant, params=declaration['params'],
                              oracle_text=declaration['oracle_text'])))
    return cases

