"""Paid off-turn Flash entry, Champion, copy and graveyard history."""

from functools import lru_cache
import json
from pathlib import Path

from death_counter_trigger_recipes import exact
from recipes import cost_from_oracle

FAMILY = 'flash_remaining_triggers_and_copy'
PROFILE = Path(__file__).with_name('data') / f'{FAMILY}_wave8_profiles.json'
MISTBIND, STUNT, NARU, VENDILION, NEBELGAST, GARNA = (9213, 16537, 18471, 9452, 16161, 18411)
SEED = 2739
LABEL = 'FlashRemainingTriggersAndCopyRecipe'
MODES = {
    MISTBIND: ('foe_lands', 'own_lands', 'release', 'foreign_only', 'flight'),
    STUNT: ('copy_own', 'copy_foe', 'copy_shroud', 'decline'),
    NARU: ('own_copy', 'own_retarget', 'opponent_spell', 'own_creature'),
    VENDILION: ('opponent', 'self', 'decline', 'only_land', 'flight'),
    NEBELGAST: ('own_spirit', 'foreign_spirit', 'own_nonspirit', 'flight'),
    GARNA: ('battlefield_deaths', 'milled', 'discarded', 'haste'),
}
ZA = ('hand', 'battlefield', 'library', 'graveyard', 'exile')
ZERO = (0, 0, 0, 0, 0, 0)
FIXED_LIBRARY = ('Island', 'Plains', 'Mountain')


@lru_cache(maxsize=1)
def pins():
    return json.loads(PROFILE.read_bytes())


def classify(record, csvid):
    pin = pins()['sources'].get(record.get('name'))
    if (csvid not in MODES or not pin or pin['registry']['csvid'] != csvid
            or not exact(record, pin['oracle']) or record.get('layout') != 'normal'
            or 'Creature — ' not in record.get('type_line', '')
            or record['oracle_text'].splitlines()[0] != 'Flash'):
        raise ValueError('complete normal trigger/copy Flash source required')
    return dict(csvid=csvid, cost=cost_from_oracle(record))


def variants(shape):
    return [f'{mode}_{seat}' for mode in MODES[shape['csvid']] for seat in (0, 1)]


def validate_variants(shape, selected):
    if (type(selected) is not list or not selected or any(type(v) is not str for v in selected)
            or len(set(selected)) != len(selected) or set(selected) - set(variants(shape))):
        raise ValueError('explicit distinct supported trigger/copy Flash variants required')


def spec(declaration, record):
    pin = pins()['sources'].get(record.get('name'))
    if not pin:
        raise ValueError('pinned trigger/copy Flash source required')
    shape = classify(record, pin['registry']['csvid'])
    if (declaration.get('family') != FAMILY or type(declaration.get('version')) is not int
            or declaration['version'] != 2 or declaration.get('oracle_text') != record['oracle_text']
            or not exact(declaration.get('params'), shape)):
        raise ValueError('whole versioned trigger/copy Flash declaration required')
    validate_variants(shape, declaration.get('variants'))
    return dict(shape, family=FAMILY)


def validate_witnesses(oracle, rows):
    for group in ('sources', 'witnesses'):
        for name, pin in pins()[group].items():
            row = rows.get(name, {})
            if (not exact(oracle.get(name), pin['oracle']) or not exact(row, pin['registry'])
                    or row.get('kind') != 'card' or row.get('handler') in (None, 'NULL', '0')
                    or group == 'sources' and row['handler'] == 'card_dummy'):
                raise ValueError(f'changed trigger/copy Flash fixture identity: {name}')


class Scene:
    def __init__(self, source, owner, mode, rows, oracle):
        self.name, self.id = source['name'], source['csvid']
        self.owner, self.foe, self.mode = owner, 1 - owner, mode
        self.rows, self.oracle = rows, oracle
        self.setup, self.steps, self.cards = [], [], {}
        self.zones = {p: {z: [] for z in ZA} for p in (0, 1)}
        self.life = [20, 20]
        self.add('source', self.name, owner, 'hand')

    def emit(self, op, ref=None, *, prop=None, **values):
        if ref is not None:
            values['ref'] = ref
        if prop is not None:
            values['property'] = prop
        self.steps.append(dict(op=op, **values))

    def add(self, ref, name, who, zone='battlefield'):
        self.setup.append(dict(ref=ref, player=who, zone=zone, name=name))
        self.cards[ref] = name, who, zone
        self.zones[who][zone].append(name)

    def transfer(self, name, who, former, destination):
        self.zones[who][former].remove(name)
        self.zones[who][destination].append(name)

    def move(self, ref, destination, *, face=None):
        name, who, former = self.cards[ref]
        self.transfer(name, who, former, destination)
        if face:
            self.zones[who][destination].pop()
            self.zones[who][destination].append(face)
        self.cards[ref] = face or name, who, destination

    def prop(self, ref, field, value):
        self.emit('assert_card', ref, prop=field, value=value)

    def stats(self, ref, bonus=0):
        name, _, _ = self.cards[ref]
        self.prop(ref, 'in_play', 1)
        self.prop(ref, 'csvid', self.rows[name]['csvid'])
        for stat in ('power', 'toughness'):
            self.prop(ref, stat, int(self.oracle[name][stat]) + bonus)

    def mana(self, who, pool=ZERO, write=False):
        for color, value in enumerate(pool):
            self.emit('set_mana' if write else 'assert_mana', player=who, color=color, value=value)

    def cast(self, ref, *, response=False, card_target=None, player_target=None):
        name, who, zone = self.cards[ref]
        assert zone == 'hand'
        self.mana(who, cost_from_oracle(self.oracle[name]), write=True)
        self.emit('refresh')
        if card_target:
            self.emit('choose_target', card_target, player=who)
        if player_target is not None:
            self.emit('choose_player', player=who, value=player_target)
        self.emit('cast_response' if response else 'cast', ref, value=1)
        self.emit('assert_stack', value=2 if response else 1)
        self.mana(who)
        self.prop(ref, 'in_hand', 0)
        self.emit('assert_unused_choices', value=0)

    def enter(self, setup_rider=None, *, parent_target=None, parent_zone='battlefield',
              source_zone='battlefield', copied=None):
        self.cast('parent', player_target=parent_target)
        self.emit('announce_spell', 'parent')
        self.cast('source', response=True)
        self.prop('source', 'in_play', 0)
        if setup_rider:
            setup_rider()
        # Stunt Double uses a shared Clone callback with a Flash type flag but
        # no affirmative CAN_CAST. Preserve correct source expectations even
        # if there is only one live stack item on this legacy implementation.
        self.emit('resolve' if self.id == STUNT else 'resolve_response')
        self.emit('assert_stack', value=0)
        self.emit('assert_unused_choices', value=0)
        self.move('source', source_zone, face=copied)
        self.move('parent', parent_zone)
        self.emit('refresh')
        if source_zone == 'battlefield':
            self.stats('source')
        if parent_zone == 'battlefield':
            self.stats('parent')

    def library(self, player):
        for index, name in enumerate(FIXED_LIBRARY):
            self.add(f'l_{player}_{index}', name, player, 'library')

    def draw(self, player, count=1):
        for _ in range(count):
            name = self.zones[player]['library'][0]
            self.transfer(name, player, 'library', 'hand')

    def receipt(self, *extra):
        self.emit('refresh')
        self.emit('assert_stack', value=0)
        names = set(extra)
        for zones in self.zones.values():
            for cards in zones.values():
                names.update(cards)
        for who, zones in self.zones.items():
            self.mana(who)
            self.emit('assert_life', player=who, value=self.life[who])
            self.emit('assert_shuffle_count', player=who, value=0)
            for zone, cards in zones.items():
                self.emit('assert_zone', player=who, zone=zone, value=len(cards))
                for name in sorted(names):
                    self.emit('assert_zone_multiplicity', player=who, zone=zone, index=self.rows[name]['csvid'],
                              value=cards.count(name))
                if zone == 'library':
                    for index, name in enumerate(cards):
                        self.emit('assert_zone_card', player=who, zone=zone, value=self.rows[name]['csvid'], index=index)

    def flight(self, power):
        self.emit('advance_turn')
        self.emit('untap_step', player=self.owner)
        self.emit('turn', player=self.owner)
        self.emit('begin_attackers')
        self.emit('attack', 'source', value=1)
        self.emit('finish_attackers')
        self.emit('begin_blockers')
        self.emit('block', 'ground', other='source', value=0)
        self.emit('advance_combat')
        self.prop('source', 'blocked', 0)
        self.emit('combat_damage')
        self.emit('resolve')
        self.emit('end_combat')
        self.life[self.foe] -= power

    def mistbind(self):
        self.add('parent', 'Goblin Piker', self.foe, 'hand')
        eligible = self.mode != 'foreign_only'
        victim = self.owner if self.mode == 'own_lands' else self.foe
        self.add('foreign_faerie', 'Zephyr Sprite', self.foe)
        for i, name in enumerate(('Island', 'Mountain')):
            self.add(f'foe_land_{i}', name, self.foe)
        if eligible:
            self.add('faerie', 'Zephyr Sprite', self.owner)
            self.add('own_nonfaerie', 'Grizzly Bears', self.owner)
            for i, name in enumerate(('Plains', 'Forest')):
                self.add(f'own_land_{i}', name, self.owner)
        if self.mode == 'release':
            self.add('bounce', 'Unsummon', self.foe, 'hand')
        if self.mode == 'flight':
            self.add('ground', 'Hill Giant', self.foe)

        def choices():
            self.emit('choose_target', 'faerie', player=self.owner)
            self.emit('choose_player', player=self.owner, value=victim)

        self.enter(choices if eligible else None, source_zone='battlefield' if eligible else 'graveyard')
        if eligible:
            self.move('faerie', 'exile')
        for i in (0, 1):
            self.prop(f'foe_land_{i}', 'tapped', int(eligible and victim == self.foe))
            if eligible:
                self.prop(f'own_land_{i}', 'tapped', int(victim == self.owner))
        self.stats('foreign_faerie')
        if eligible:
            self.stats('own_nonfaerie')
        self.receipt()
        if self.mode == 'release':
            self.cast('bounce', card_target='source')
            self.emit('resolve')
            self.emit('assert_unused_choices', value=0)
            self.move('bounce', 'graveyard')
            self.move('source', 'hand')
            self.move('faerie', 'battlefield')
            self.emit('bind_permanent_copy', 'faerie', player=self.owner, index=0, value=self.rows['Zephyr Sprite']['csvid'])
            self.stats('faerie')
            self.receipt()
        elif self.mode == 'flight':
            self.flight(4)
            self.receipt()

    def stunt(self):
        self.add('parent', 'Goblin Piker', self.foe, 'hand')
        self.add('own_candidate', 'Grizzly Bears', self.owner)
        kind = 'Wall of Denial' if self.mode == 'copy_shroud' else 'Hill Giant'
        self.add('foe_candidate', kind, self.foe)
        decline = self.mode == 'decline'
        choose = 'own_candidate' if self.mode == 'copy_own' else 'foe_candidate'
        if self.mode == 'copy_shroud':
            self.add('probe', 'Prodigal Sorcerer', self.owner)
            self.emit('can_target_creature', 'probe', other='foe_candidate', value=0)

        def choices():
            if decline:
                self.emit('choose_optional_target', 'own_candidate', player=self.owner)
            else:
                self.emit('choose_target', choose, player=self.owner)

        self.enter(choices, source_zone='graveyard' if decline else 'battlefield',
                   copied=None if decline else self.cards[choose][0])
        self.emit('assert_forced_target_choices', value=0)
        for ref in ('own_candidate', 'foe_candidate'):
            self.stats(ref)
        self.receipt(self.name)

    def naru(self):
        parent, who = {
            'own_copy': ('Think Twice', self.owner),
            'own_retarget': ('Lightning Bolt', self.owner),
            'opponent_spell': ('Think Twice', self.foe),
            'own_creature': ('Ashcoat Bear', self.owner),
        }[self.mode]
        self.add('parent', parent, who, 'hand')
        self.add('wizard', 'Prodigal Sorcerer', self.owner)
        self.add('foe_wizard', 'Prodigal Sorcerer', self.foe)
        self.add('foil', 'Grizzly Bears', self.owner)
        self.library(self.owner)
        self.library(self.foe)

        def change_target():
            # The real Lightning Bolt copy can select a different legal
            # player from the original. No synthetic target override.
            self.emit('choose_player', player=self.owner, value=self.owner)

        retarget = self.mode == 'own_retarget'
        self.enter(change_target if retarget else None, parent_target=self.foe if retarget else None,
                   parent_zone='battlefield' if self.mode == 'own_creature' else 'graveyard')
        self.stats('wizard', 1)
        self.stats('foe_wizard')
        self.stats('foil')
        if retarget:
            self.life[self.owner] -= 3
            self.life[self.foe] -= 3
        elif self.mode == 'own_copy':
            self.draw(self.owner, 2)
        elif self.mode == 'opponent_spell':
            self.draw(self.foe)
        self.receipt()

    def vendilion(self):
        self.add('parent', 'Goblin Piker', self.foe, 'hand')
        for player in (0, 1):
            self.library(player)
        victim = self.owner if self.mode == 'self' else self.foe
        land_only = self.mode == 'only_land'
        select = self.mode in ('opponent', 'self')
        self.add('land_foil', 'Swamp', victim, 'hand')
        if not land_only:
            self.add('choice', 'Lightning Bolt', victim, 'hand')
            self.add('nonland_foil', 'Giant Growth', victim, 'hand')
        self.add('other_hand', 'Unsummon', 1 - victim, 'hand')
        if self.mode == 'flight':
            self.add('ground', 'Grizzly Bears', self.foe)

        def choices():
            self.emit('choose_player', player=self.owner, value=victim)
            self.emit('observe_search_filters', value=1)
            picked = 'Swamp' if land_only else 'Lightning Bolt'
            self.emit('choose_zone_card' if select else 'choose_optional_zone', player=self.owner,
                      value=self.rows[picked]['csvid'])

        self.enter(choices)
        self.emit('assert_forced_zone_choices', value=0)
        self.emit('assert_offered_legal_card', player=self.owner, index=self.rows['Swamp']['csvid'], value=0)
        if not land_only:
            for card in ('Lightning Bolt', 'Giant Growth'):
                self.emit('assert_offered_legal_card', player=self.owner, index=self.rows[card]['csvid'], value=1)
        if select:
            self.move('choice', 'library')
            self.draw(victim)
        self.receipt()
        if self.mode == 'flight':
            self.flight(3)
            self.receipt()

    def nebelgast(self):
        owner, foe = self.owner, self.foe
        self.add('parent', 'Goblin Piker', foe, 'hand')
        self.add('ground', 'Hill Giant', foe)
        self.add('second', 'Walking Corpse', foe)
        self.add('friend', 'Walking Corpse', owner)
        if self.mode == 'own_spirit':
            self.add('later', 'Spirit of the Hunt', owner, 'hand')
        elif self.mode == 'foreign_spirit':
            self.add('later', 'Zephyr Sprite', foe, 'hand')
        elif self.mode == 'own_nonspirit':
            self.add('later', 'Ashcoat Bear', owner, 'hand')
        self.enter(lambda: self.emit('choose_target', 'second', player=owner))
        self.prop('second', 'tapped', 1)
        self.prop('ground', 'tapped', 0)
        self.prop('friend', 'tapped', 0)
        if self.mode == 'flight':
            self.flight(2)
        else:
            self.cast('later')
            qualifies = self.mode == 'own_spirit'
            # A genuinely incorrect extra trigger still has a legal target.
            # Its observable unused intent is expected for either negative.
            self.emit('choose_target', 'ground', player=owner)
            self.emit('resolve')
            self.emit('assert_unused_choices', value=int(not qualifies))
            self.move('later', 'battlefield')
            self.stats('later')
            self.prop('ground', 'tapped', int(qualifies))
            self.prop('second', 'tapped', 1)
            self.prop('friend', 'tapped', 0)
        self.receipt()

    def garna(self):
        owner, foe = self.owner, self.foe
        self.add('parent', 'Goblin Piker', foe, 'hand')
        self.add('old_foil', 'Ornithopter', owner, 'graveyard')
        self.emit('reset_death_history')
        recovered = ()
        if self.mode == 'battlefield_deaths':
            self.add('sweep', 'Pyroclasm', foe, 'hand')
            self.add('dead_one', 'Suntail Hawk', owner)
            self.add('dead_two', 'Walking Corpse', owner)
            self.add('foreign_dead', 'Grizzly Bears', foe)
            self.cast('sweep')
            self.emit('resolve')
            self.emit('assert_unused_choices', value=0)
            for ref in ('sweep', 'dead_one', 'dead_two', 'foreign_dead'):
                self.move(ref, 'graveyard')
            recovered = ('Suntail Hawk', 'Walking Corpse')
        elif self.mode == 'milled':
            self.add('mill', 'Thought Scour', foe, 'hand')
            for i, name in enumerate(('Hill Giant', 'Lightning Bolt', 'Swamp')):
                self.add(f'top_{i}', name, owner, 'library')
            self.library(foe)
            self.cast('mill', player_target=owner)
            self.emit('resolve')
            self.emit('assert_unused_choices', value=0)
            for ref in ('mill', 'top_0', 'top_1'):
                self.move(ref, 'graveyard')
            self.draw(foe)
            recovered = ('Hill Giant',)
        elif self.mode == 'discarded':
            self.add('discard_engine', 'Tortured Existence', owner)
            self.add('discard_cost', 'Hill Giant', owner, 'hand')
            self.add('prior_target', 'Zephyr Sprite', owner, 'graveyard')
            self.mana(owner, (0, 1, 0, 0, 0, 0), write=True)
            self.emit('refresh')
            self.emit('require_can_activate', 'discard_engine', value=1)
            self.emit('choose_zone_card', player=owner, value=self.rows['Hill Giant']['csvid'])
            self.emit('choose_zone_card', player=owner, value=self.rows['Zephyr Sprite']['csvid'])
            self.emit('activate', 'discard_engine', value=1)
            self.emit('assert_stack', value=1)
            self.mana(owner)
            self.emit('resolve')
            self.emit('assert_unused_choices', value=0)
            self.move('discard_cost', 'graveyard')
            self.move('prior_target', 'hand')
            recovered = ('Hill Giant',)
        else:
            self.add('runner', 'Grizzly Bears', owner, 'hand')
        self.receipt()
        self.enter()
        for name in recovered:
            self.transfer(name, owner, 'graveyard', 'hand')
        self.prop('source', 'sick', 1)
        self.prop('parent', 'sick', 1)
        self.receipt()
        if self.mode != 'haste':
            return
        self.emit('advance_turn')
        self.emit('untap_step', player=owner)
        self.emit('turn', player=owner)
        self.cast('runner')
        self.emit('resolve')
        self.emit('assert_unused_choices', value=0)
        self.move('runner', 'battlefield')
        self.emit('refresh')
        self.prop('runner', 'sick', 0)
        self.emit('begin_attackers')
        self.emit('attack', 'runner', value=1)
        self.emit('finish_attackers')
        self.emit('begin_blockers')
        self.emit('advance_combat')
        self.emit('combat_damage')
        self.emit('resolve')
        self.emit('end_combat')
        self.life[foe] -= 2
        self.receipt()

    def play(self):
        self.emit('turn', player=self.foe)
        self.emit('present_ai_dialogs', value=1)
        for player in (0, 1):
            self.mana(player, write=True)
        {MISTBIND: self.mistbind, STUNT: self.stunt, NARU: self.naru, VENDILION: self.vendilion,
         NEBELGAST: self.nebelgast, GARNA: self.garna}[self.id]()
        self.emit('present_ai_dialogs', value=0)


def expand(name, csvid, declaration, shape, selected, rows, oracle):
    validate_witnesses(oracle, rows)
    validate_variants(shape, selected)
    if pins()['sources'][name]['registry']['csvid'] != csvid:
        raise ValueError('exact registered trigger/copy Flash CSVID required')
    cases = []
    for variant in selected:
        mode, seat = variant.rsplit('_', 1)
        scene = Scene(rows[name], int(seat), mode, rows, oracle)
        scene.play()
        gaps = []
        if csvid == VENDILION:
            gaps.append('No standalone reveal receipt. Real offered card identity and both exact zones are checked.')
        cases.append(dict(name=f'{LABEL}_{csvid}_{variant}', card=name, fixture='empty_main', seed=SEED,
                          setup=scene.setup, steps=scene.steps,
                          _recipe=dict(family=FAMILY, version=2, variant=variant, params=declaration['params'],
                                       oracle_text=declaration['oracle_text'], untested_components=gaps)))
    return cases
