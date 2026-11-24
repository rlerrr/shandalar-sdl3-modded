"""Paid mill effects with real graveyard recovery, timers and ordered zone receipts."""

from collections import Counter
from functools import lru_cache
import json
from pathlib import Path

from death_counter_trigger_recipes import exact
from recipes import cost_from_oracle

FAMILY = 'mill_recursion_and_zones'
PROFILE = Path(__file__).with_name('data') / f'{FAMILY}_wave8_profiles.json'
CHANT, DROWN, EXTRACT, CHURN = (13139, 13145, 14034, 15575)
FALSE_MEMORIES, SUDDEN, GRAPPLE, WINDS = (5806, 14633, 16117, 17071)
SEED = 2771
LABEL = 'MillRecursionAndZonesRecipe'
MODES = {
    CHANT: ('mill_ten', 'creature_upkeep', 'noncreature_upkeep', 'repeat_upkeep'),
    DROWN: ('mixed_lands', 'no_own_land', 'lethal', 'own_target_short', 'target_lost'),
    EXTRACT: ('own_milled', 'foe_milled', 'old', 'short_no_creatures'),
    CHURN: ('milled', 'old', 'decline', 'short_no_creatures', 'discarded'),
    FALSE_MEMORIES: ('fresh_seven', 'old_and_source', 'short_all', 'opposing_end'),
    SUDDEN: ('both_milled', 'old_creature_new_land', 'creature_only', 'land_only'),
    GRAPPLE: ('creature', 'land', 'decline', 'short_neither'),
    WINDS: ('artifact', 'stolen', 'short_offturn', 'target_lost', 'land_only'),
}
ZA = ('hand', 'battlefield', 'library', 'graveyard', 'exile')
ZERO = (0, 0, 0, 0, 0, 0)
LONG_LIBRARY = ('Forest', 'Lightning Bolt', 'Grizzly Bears', 'Plains', 'Goblin Piker', 'Island',
                'Divination', 'Mountain', 'Ornithopter', 'Swamp', 'Hill Giant', 'Unsummon')


@lru_cache(maxsize=1)
def pins():
    return json.loads(PROFILE.read_bytes())


def classify(record, csvid):
    pin = pins()['sources'].get(record.get('name'))
    if (csvid not in MODES or not pin or pin['registry']['csvid'] != csvid
            or not exact(record, pin['oracle']) or record.get('layout') != 'normal'):
        raise ValueError('complete normal mill/recursion record required')
    return dict(csvid=csvid, cost=cost_from_oracle(record))


def variants(shape):
    return [f'{mode}_{seat}' for mode in MODES[shape['csvid']] for seat in (0, 1)]


def validate_variants(shape, selected):
    if (type(selected) is not list or not selected or any(type(v) is not str for v in selected)
            or len(set(selected)) != len(selected) or set(selected) - set(variants(shape))):
        raise ValueError('explicit distinct supported mill/recursion variants required')


def spec(declaration, record):
    pin = pins()['sources'].get(record.get('name'))
    if not pin:
        raise ValueError('pinned mill/recursion source required')
    shape = classify(record, pin['registry']['csvid'])
    if (declaration.get('family') != FAMILY or type(declaration.get('version')) is not int
            or declaration['version'] != 2 or declaration.get('oracle_text') != record['oracle_text']
            or not exact(declaration.get('params'), shape)):
        raise ValueError('whole versioned mill/recursion declaration required')
    validate_variants(shape, declaration.get('variants'))
    return dict(shape, family=FAMILY)


def validate_witnesses(oracle, rows):
    for group in ('sources', 'witnesses'):
        for name, pin in pins()[group].items():
            row = rows.get(name, {})
            if (not exact(oracle.get(name), pin['oracle']) or not exact(row, pin['registry'])
                    or row.get('kind') != 'card' or row.get('handler') in (None, 'NULL', '0')
                    or group == 'sources' and row['handler'] == 'card_dummy'):
                raise ValueError(f'changed mill/recursion fixture identity: {name}')


class MillScene:
    def __init__(self, row, owner, mode, registry, oracle):
        self.name, self.id = row['name'], row['csvid']
        self.owner, self.foe, self.active, self.mode = owner, 1 - owner, owner, mode
        self.rows, self.oracle = registry, oracle
        self.setup, self.steps, self.cards = [], [], {}
        self.zones = {p: {zone: [] for zone in ZA} for p in (0, 1)}
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

    def library(self, player, names):
        for index, name in enumerate(names):
            self.add(f'deck_{player}_{index}', name, player, 'library')

    def move_name(self, name, player, old_zone, new_zone, *, owner=None):
        self.zones[player][old_zone].remove(name)
        self.zones[player if owner is None else owner][new_zone].append(name)

    def move(self, ref, new_zone, *, owner=None):
        name, player, old = self.cards[ref]
        self.move_name(name, player, old, new_zone, owner=owner)
        self.cards[ref] = name, player if owner is None else owner, new_zone

    def mill(self, player, number):
        for name in self.zones[player]['library'][:number]:
            self.move_name(name, player, 'library', 'graveyard')

    def prop(self, ref, field, value):
        self.emit('assert_card', ref, prop=field, value=value)

    def stats(self, ref, name, delta=0):
        self.prop(ref, 'in_play', 1)
        self.prop(ref, 'csvid', self.rows[name]['csvid'])
        for field in ('power', 'toughness'):
            self.prop(ref, field, int(self.oracle[name][field]) + delta)

    def mana(self, player, amounts=ZERO, write=False):
        for color, amount in enumerate(amounts):
            self.emit('set_mana' if write else 'assert_mana', player=player, color=color, value=amount)

    def cast(self, ref='source', target=None, *, response=False):
        name, who, zone = self.cards[ref]
        assert zone == 'hand'
        self.mana(who, cost_from_oracle(self.oracle[name]), True)
        self.emit('refresh')
        if target:
            self.emit('choose_target', target, player=who)
        self.emit('cast_response' if response else 'cast', ref, value=1)
        self.emit('assert_stack', value=2 if response else 1)
        self.mana(who)
        self.prop(ref, 'in_hand', 0)
        self.emit('assert_unused_choices', value=0)

    def finish(self, ref='source', destination='graveyard', *, response=False):
        self.emit('resolve_response' if response else 'resolve')
        self.emit('assert_stack', value=0)
        self.emit('assert_unused_choices', value=0)
        self.move(ref, destination)

    def receipt(self, *extra):
        self.emit('refresh')
        self.emit('assert_stack', value=0)
        names = set(extra)
        for zones in self.zones.values():
            for cards in zones.values():
                names.update(cards)
        for who, zones in self.zones.items():
            self.mana(who)
            self.emit('assert_life', player=who, value=20)
            self.emit('assert_shuffle_count', player=who, value=0)
            for zone, cards in zones.items():
                counts = Counter(cards)
                self.emit('assert_zone', player=who, zone=zone, value=len(cards))
                for name in sorted(names):
                    self.emit('assert_zone_multiplicity', player=who, zone=zone,
                              index=self.rows[name]['csvid'], value=counts[name])
                if zone == 'library':
                    for index, name in enumerate(cards):
                        self.emit('assert_zone_card', player=who, zone=zone, index=index,
                                  value=self.rows[name]['csvid'])

    def upkeep(self, *, acknowledge=False):
        self.active = 1 - self.active
        self.emit('advance_turn')
        self.emit('untap_step', player=self.active)
        if acknowledge:
            self.emit('choose_dialog', player=self.active, value=0)
        self.emit('upkeep_step', player=self.active)
        self.emit('assert_stack', value=0)
        self.emit('assert_unused_choices', value=0)

    def chant(self):
        owner, foe = self.owner, self.foe
        deck = {
            'mill_ten': LONG_LIBRARY,
            'creature_upkeep': ('Hill Giant',),
            'noncreature_upkeep': ('Lightning Bolt',),
            'repeat_upkeep': ('Forest', 'Forest'),
        }[self.mode]
        self.library(owner, deck)
        self.library(foe, ('Plains', 'Island', 'Mountain'))
        self.add('foe_grave', 'Grizzly Bears', foe, 'graveyard')
        if self.mode == 'mill_ten':
            self.add('old_grave', 'Zephyr Sprite', owner, 'graveyard')
        self.cast()
        self.finish(destination='battlefield')
        self.mill(owner, 10)
        self.prop('source', 'in_play', 1)
        self.receipt()
        if self.mode == 'mill_ten':
            return
        count = len(deck) + int(self.mode == 'repeat_upkeep')
        for visit in range(count):
            self.upkeep()
            assert self.active == foe
            self.receipt()
            self.upkeep(acknowledge=owner == 0 and visit < len(deck))
            assert self.active == owner
            if visit < len(deck):
                selected = deck[0]
                creature = self.mode == 'creature_upkeep'
                self.move_name(selected, owner, 'graveyard', 'battlefield' if creature else 'hand')
                if creature:
                    self.emit('bind_permanent_copy', 'recovered', player=owner, index=0, value=self.rows[selected]['csvid'])
                    self.stats('recovered', selected)
            self.receipt()

    def drown(self):
        owner, foe = self.owner, self.foe
        victim = owner if self.mode == 'own_target_short' else foe
        creature = 'Grizzly Bears' if self.mode == 'lethal' else 'Hill Giant' if victim == owner else 'Craw Wurm'
        self.add('target', creature, victim)
        self.add('unrelated', 'Walking Corpse', 1 - victim)
        self.add('opp_land', 'Island', foe, 'graveyard')
        self.add('opp_land_two', 'Swamp', foe, 'graveyard')
        self.add('own_old_spell', 'Unsummon', owner, 'graveyard')
        if self.mode in ('mixed_lands', 'own_target_short', 'target_lost'):
            self.add('own_old_land', 'Plains', owner, 'graveyard')
        decks = {
            'mixed_lands': ('Forest', 'Divination', 'Mountain', 'Grizzly Bears', 'Island', 'Swamp'),
            'no_own_land': ('Divination', 'Grizzly Bears', 'Lightning Bolt', 'Ornithopter', 'Mountain'),
            'lethal': ('Forest', 'Mountain', 'Lightning Bolt', 'Ornithopter', 'Island'),
            'own_target_short': ('Forest', 'Ornithopter'),
            'target_lost': ('Forest', 'Divination', 'Mountain', 'Grizzly Bears', 'Island'),
        }
        self.library(owner, decks[self.mode])
        if self.mode == 'target_lost':
            self.add('bounce', 'Unsummon', foe, 'hand')
        self.cast(target='target')
        if self.mode == 'target_lost':
            self.emit('announce_spell', 'source')
            self.cast('bounce', 'target', response=True)
            self.finish('bounce', response=True)
            self.move('source', 'graveyard')
            self.move('target', 'hand')
            self.receipt()
            return
        self.finish()
        self.mill(owner, 4)
        lands = {'mixed_lands': 3, 'no_own_land': 0, 'lethal': 2, 'own_target_short': 2}[self.mode]
        if self.mode == 'lethal':
            self.move('target', 'graveyard')
        else:
            self.stats('target', creature, -lands)
        self.stats('unrelated', 'Walking Corpse')
        self.receipt()
        if self.mode != 'lethal':
            self.emit('cleanup')
            self.emit('refresh')
            self.stats('target', creature)
            self.receipt()

    def extract(self):
        owner, foe = self.owner, self.foe
        short = self.mode == 'short_no_creatures'
        self.library(owner, ('Forest',) if short else ('Grizzly Bears', 'Forest', 'Island'))
        self.library(foe, ('Plains',) if short else ('Walking Corpse', 'Plains', 'Mountain'))
        self.add('old_foil', 'Lightning Bolt', owner, 'graveyard')
        if not short:
            self.add('old_creature', 'Zephyr Sprite', owner, 'graveyard')
            self.add('old_foreign', 'Ornithopter', foe, 'graveyard')
        if self.mode == 'foe_milled':
            self.add('bounce', 'Unsummon', foe, 'hand')
        self.cast()
        picked = None
        if not short:
            picked = {'own_milled': 'Grizzly Bears', 'foe_milled': 'Walking Corpse', 'old': 'Zephyr Sprite'}[self.mode]
            whose = foe if self.mode == 'foe_milled' else owner
            self.emit('choose_player', player=owner, value=whose)
            self.emit('observe_search_filters', value=1)
            self.emit('choose_zone_card', player=owner, value=self.rows[picked]['csvid'])
        self.finish()
        self.mill(owner, 2)
        self.mill(foe, 2)
        if not short:
            self.move_name(picked, whose, 'graveyard', 'battlefield', owner=owner)
            foil = 'Plains' if whose == foe else 'Forest'
            self.emit('assert_offered_legal_card', player=owner, index=self.rows[picked]['csvid'], value=1)
            self.emit('assert_offered_legal_card', player=owner, index=self.rows[foil]['csvid'], value=0)
            self.emit('bind_permanent_copy', 'recovered', player=owner, index=0, value=self.rows[picked]['csvid'])
            self.cards['recovered'] = picked, owner, 'battlefield'
            self.stats('recovered', picked)
        self.receipt()
        if self.mode == 'foe_milled':
            self.cast('bounce', 'recovered')
            self.finish('bounce')
            self.move('recovered', 'hand', owner=foe)
            self.receipt()

    def discard_fixture(self):
        owner = self.owner
        self.add('discarder', 'Tortured Existence', owner)
        self.add('discard_cost', 'Walking Corpse', owner, 'hand')
        self.add('activation_target', 'Zephyr Sprite', owner, 'graveyard')
        self.mana(owner, (0, 1, 0, 0, 0, 0), True)
        self.emit('refresh')
        self.emit('require_can_activate', 'discarder', value=1)
        for name in ('Walking Corpse', 'Zephyr Sprite'):
            self.emit('choose_zone_card', player=owner, value=self.rows[name]['csvid'])
        self.emit('activate', 'discarder', value=1)
        self.emit('assert_stack', value=1)
        self.mana(owner)
        self.emit('resolve')
        self.emit('assert_unused_choices', value=0)
        self.move('discard_cost', 'graveyard')
        self.move('activation_target', 'hand')
        self.receipt()

    def churn(self):
        owner, foe = self.owner, self.foe
        short = self.mode == 'short_no_creatures'
        self.library(owner, ('Forest',) if short else ('Grizzly Bears', 'Forest', 'Lightning Bolt', 'Island', 'Swamp'))
        self.library(foe, ('Mountain', 'Plains'))
        self.add('foe_grave', 'Ornithopter', foe, 'graveyard')
        self.add('old_spell', 'Divination', owner, 'graveyard')
        if not short:
            self.add('old_creature', 'Hill Giant', owner, 'graveyard')
        if self.mode == 'discarded':
            self.discard_fixture()
        self.cast()
        picked = {'milled': 'Grizzly Bears', 'old': 'Hill Giant', 'discarded': 'Walking Corpse'}.get(self.mode)
        if not short:
            self.emit('observe_search_filters', value=1)
            self.emit('choose_zone_card' if picked else 'choose_optional_zone', player=owner,
                      value=self.rows[picked or 'Hill Giant']['csvid'])
        self.finish()
        self.mill(owner, 3)
        if picked:
            self.move_name(picked, owner, 'graveyard', 'hand')
        if not short:
            self.emit('assert_forced_zone_choices', value=0)
            for name, legal in (('Grizzly Bears', 1), ('Hill Giant', 1), ('Forest', 0), ('Divination', 0)):
                self.emit('assert_offered_legal_card', player=owner, index=self.rows[name]['csvid'], value=legal)
        self.receipt()

    def false_memories(self):
        owner, foe = self.owner, self.foe
        short = self.mode == 'short_all'
        deck = LONG_LIBRARY[:2] if short else LONG_LIBRARY
        self.library(owner, deck)
        self.library(foe, ('Island', 'Plains', 'Mountain'))
        self.add('old_a', 'Craw Wurm', owner, 'graveyard')
        self.add('old_b', 'Zephyr Sprite', owner, 'graveyard')
        self.add('foe_grave', 'Divination', foe, 'graveyard')
        if self.mode == 'opposing_end':
            self.add('parent', 'Goblin Piker', foe, 'hand')
            self.emit('turn', player=foe)
            self.cast('parent')
            self.emit('announce_spell', 'parent')
            self.cast(response=True)
            self.finish(response=True)
            self.move('parent', 'battlefield')
        else:
            self.cast()
            self.finish()
        self.mill(owner, 7)
        self.receipt()
        picks = {
            'short_all': list(self.zones[owner]['graveyard']),
            'old_and_source': ['Craw Wurm', self.name, *deck[:5]],
            'fresh_seven': list(deck[:7]),
            'opposing_end': list(deck[:7]),
        }[self.mode]
        assert len(picks) == min(7, len(self.zones[owner]['graveyard']))
        for name in picks:
            self.emit('choose_zone_card', player=owner, value=self.rows[name]['csvid'])
        # The test bridge dispatches the real end trigger through its
        # production end-turn phase. No synthetic grave-to-exile operation.
        self.emit('cleanup')
        self.emit('assert_unused_choices', value=0)
        for name in picks:
            self.move_name(name, owner, 'graveyard', 'exile')
        self.receipt()
        self.emit('advance_turn')
        self.emit('cleanup')
        self.emit('assert_unused_choices', value=0)
        self.receipt()

    def sudden(self):
        owner, foe = self.owner, self.foe
        creature = self.mode != 'land_only'
        land = self.mode != 'creature_only'
        deck = (
            'Grizzly Bears' if creature else 'Lightning Bolt',
            'Forest' if land else 'Lightning Bolt',
            'Unsummon', 'Giant Growth', 'Island', 'Ornithopter',
        )
        self.library(owner, deck)
        self.library(foe, ('Mountain', 'Plains', 'Swamp'))
        self.add('foe_creature', 'Zephyr Sprite', foe, 'graveyard')
        self.add('foe_land', 'Swamp', foe, 'graveyard')
        self.add('old_foil', 'Divination', owner, 'graveyard')
        if creature:
            self.add('old_creature', 'Hill Giant', owner, 'graveyard')
        if land:
            self.add('old_land', 'Plains', owner, 'graveyard')
        choices = []
        if creature:
            choices.append('Hill Giant' if self.mode == 'old_creature_new_land' else 'Grizzly Bears')
        if land:
            choices.append('Forest')
        self.cast()
        self.emit('observe_search_filters', value=1)
        for name in choices:
            self.emit('choose_zone_card', player=owner, value=self.rows[name]['csvid'])
        self.finish()
        self.mill(owner, 4)
        for name in choices:
            self.move_name(name, owner, 'graveyard', 'hand')
        # The action says return, so a legal candidate on the final menu
        # cannot be declined. Keep the actual successful transfer checks.
        self.emit('assert_offered_selection_required', value=1)
        for name, legal in ((choices[-1], 1), ('Divination', 0)):
            self.emit('assert_offered_legal_card', player=owner, index=self.rows[name]['csvid'], value=legal)
        self.receipt()

    def grapple(self):
        owner, foe = self.owner, self.foe
        short = self.mode == 'short_neither'
        self.library(owner, ('Lightning Bolt',) if short else (
            'Grizzly Bears', 'Forest', 'Lightning Bolt', 'Island', 'Swamp'))
        self.library(foe, ('Mountain', 'Plains'))
        self.add('foe_grave', 'Ornithopter', foe, 'graveyard')
        self.add('old_foil', 'Divination', owner, 'graveyard')
        if not short:
            self.add('old_land', 'Plains', owner, 'graveyard')
            self.add('old_creature', 'Zephyr Sprite', owner, 'graveyard')
        picked = {'creature': 'Grizzly Bears', 'land': 'Plains'}.get(self.mode)
        self.cast()
        if not short:
            self.emit('observe_search_filters', value=1)
            self.emit('choose_zone_card' if picked else 'choose_optional_zone', player=owner,
                      value=self.rows[picked or 'Forest']['csvid'])
        self.finish()
        self.mill(owner, 3)
        if picked:
            self.move_name(picked, owner, 'graveyard', 'hand')
        if not short:
            self.emit('assert_offered_selection_required', value=0)
            self.emit('assert_forced_zone_choices', value=0)
            for name, legal in (('Grizzly Bears', 1), ('Forest', 1), ('Divination', 0)):
                self.emit('assert_offered_legal_card', player=owner, index=self.rows[name]['csvid'], value=legal)
        self.receipt()

    def winds(self):
        owner, foe = self.owner, self.foe
        short = self.mode == 'short_offturn'
        self.library(owner, ('Forest',) if short else ('Forest', 'Lightning Bolt', 'Mountain'))
        self.library(foe, ('Plains',) if short else ('Plains', 'Grizzly Bears', 'Island'))
        self.add('friendly_land', 'Swamp', owner)
        self.add('foreign_land', 'Island', foe)
        if self.mode == 'land_only':
            price = cost_from_oracle(self.oracle[self.name])
            self.mana(owner, price, True)
            self.emit('refresh')
            self.emit('cast', 'source', value=0)
            self.emit('assert_unused_choices', value=0)
            self.prop('source', 'in_hand', 1)
            self.mana(owner, price)
            self.mana(owner, write=True)
            self.receipt()
            return
        artifact = self.mode == 'artifact'
        name = "Traveler's Amulet" if artifact else 'Hill Giant'
        original_owner = owner if short else foe
        self.add('recipient', name, original_owner)
        target = 'recipient'
        if self.mode == 'stolen':
            self.add('theft', 'Act of Treason', owner, 'hand')
            self.cast('theft', target)
            self.finish('theft')
            self.move('recipient', 'battlefield', owner=owner)
            self.cards['borrowed'] = name, owner, 'battlefield'
            self.emit('bind_permanent_copy', 'borrowed', player=owner, index=0, value=self.rows[name]['csvid'])
            self.stats('borrowed', name)
            target = 'borrowed'
        if short:
            self.add('parent', 'Goblin Piker', foe, 'hand')
            self.emit('turn', player=foe)
            self.cast('parent')
            self.emit('announce_spell', 'parent')
            self.cast(target=target, response=True)
            self.finish(response=True)
            self.move('parent', 'battlefield')
        elif self.mode == 'target_lost':
            self.add('bounce', 'Unsummon', foe, 'hand')
            self.cast(target=target)
            self.emit('announce_spell', 'source')
            self.cast('bounce', target, response=True)
            self.finish('bounce', response=True)
            self.move('source', 'graveyard')
        else:
            self.cast(target=target)
            self.finish()
        self.move(target, 'hand', owner=original_owner)
        if self.mode != 'target_lost':
            self.mill(owner, 2)
            self.mill(foe, 2)
        self.receipt()

    def play(self):
        self.emit('turn', player=self.owner)
        self.emit('present_ai_dialogs', value=1)
        for who in (0, 1):
            self.mana(who, write=True)
        {CHANT: self.chant, DROWN: self.drown, EXTRACT: self.extract, CHURN: self.churn,
         FALSE_MEMORIES: self.false_memories, SUDDEN: self.sudden, GRAPPLE: self.grapple, WINDS: self.winds}[self.id]()
        self.emit('present_ai_dialogs', value=0)


def expand(name, csvid, declaration, shape, selected, rows, oracle):
    validate_witnesses(oracle, rows)
    validate_variants(shape, selected)
    if pins()['sources'][name]['registry']['csvid'] != csvid:
        raise ValueError('exact registered mill/recursion CSVID required')
    cases = []
    for variant in selected:
        mode, seat = variant.rsplit('_', 1)
        scene = MillScene(rows[name], int(seat), mode, rows, oracle)
        scene.play()
        gaps = []
        if csvid == CHANT:
            gaps.append('Real upkeep RNG is exercised with equal-identity candidates; distribution among different identities is unmeasured.')
        if csvid == FALSE_MEMORIES:
            gaps.append('The bounded end-turn step dispatches the real legacy trigger; it does not expose independent cleanup priority.')
        cases.append(dict(name=f'{LABEL}_{csvid}_{variant}', card=name, fixture='empty_main', seed=SEED,
                          setup=scene.setup, steps=scene.steps,
                          _recipe=dict(family=FAMILY, version=2, variant=variant, params=declaration['params'],
                                       oracle_text=declaration['oracle_text'], untested_components=gaps)))
    return cases
