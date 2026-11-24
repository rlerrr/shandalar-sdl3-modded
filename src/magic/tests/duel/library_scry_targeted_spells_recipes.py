"""Paid Scry spells, optional targets, card selection and exact zone destinations."""

from collections import Counter
from functools import lru_cache
import json
from pathlib import Path

from death_counter_trigger_recipes import exact
from recipes import cost_from_oracle

FAMILY = 'library_scry_targeted_spells'
PROFILE = Path(__file__).with_name('data') / f'{FAMILY}_wave8_profiles.json'
LABEL = 'LibraryScryTargetedSpellsRecipe'
SEED = 2857
TITAN, VOYAGE, CRUEL, SEA = 13579, 13593, 16682, 13543
SPITE, ANCHOR, SELECT, FILL, HARSH, PEREGRINATION = 13983, 15078, 16508, 7055, 16404, 13777
MODES = {
    TITAN: ('keep', 'bottom', 'foe_target', 'target_lost', 'empty'),
    VOYAGE: ('foe_keep', 'own_bottom', 'borrowed', 'target_lost', 'empty'),
    CRUEL: ('survives', 'lethal', 'target_lost', 'empty'),
    SEA: ('zero', 'one', 'three', 'partial_loss', 'all_lost', 'own_only'),
    SPITE: ('mixed_count', 'lethal_count', 'zero_spells', 'target_lost'),
    ANCHOR: ('own_keep', 'own_bottom', 'opponent', 'borrowed', 'target_lost'),
    SELECT: ('own_tapped', 'foe_tapped', 'untapped_only', 'untap_response'),
    FILL: ('two_both_bottom', 'self_one_bottom', 'one_short', 'empty_hand'),
    HARSH: ('multiple_creatures', 'no_creature', 'empty_hand', 'shroud_response'),
    PEREGRINATION: ('both_basics', 'only_one', 'no_basic', 'decline_all'),
}
ZONE = dict(hand=0, battlefield=1, library=2, graveyard=3, exile=4)
ZERO = (0,) * 6
DECK = ('Lightning Bolt', 'Plains', 'Forest', 'Swamp')
FOE_DECK = ('Island', 'Mountain', 'Darksteel Relic')


@lru_cache(maxsize=1)
def pins():
    return json.loads(PROFILE.read_bytes())


def classify(record, csvid):
    pin = pins()['sources'].get(record.get('name'))
    if (csvid not in MODES or not pin or pin['registry']['csvid'] != csvid
            or not exact(record, pin['oracle']) or record.get('layout') != 'normal'):
        raise ValueError('complete normal Scry rules required')
    return dict(csvid=csvid, cost=cost_from_oracle(record), scry=2 if csvid == FILL else 1)


def variants(shape):
    return [f'{mode}_{seat}' for mode in MODES[shape['csvid']] for seat in (0, 1)]


def spec(declaration, record):
    pin = pins()['sources'].get(record.get('name'))
    if not pin:
        raise ValueError('pinned targeted Scry source required')
    shape = classify(record, pin['registry']['csvid'])
    chosen = declaration.get('variants')
    if (declaration.get('family') != FAMILY or type(declaration.get('version')) is not int
            or declaration['version'] != 2 or declaration.get('oracle_text') != record['oracle_text']
            or not exact(declaration.get('params'), shape)):
        raise ValueError('whole versioned targeted Scry declaration required')
    if (type(chosen) is not list or not chosen or any(type(v) is not str for v in chosen)
            or len(chosen) != len(set(chosen)) or set(chosen) - set(variants(shape))):
        raise ValueError('explicit distinct supported targeted Scry variants required')
    return dict(shape, family=FAMILY)


def validate_witnesses(oracle, rows):
    for group in ('sources', 'witnesses'):
        for name, pin in pins()[group].items():
            row = rows.get(name, {})
            if (not exact(oracle.get(name), pin['oracle']) or not exact(row, pin['registry'])
                    or row.get('kind') != 'card' or row.get('handler') in (None, 'NULL', '0')
                    or group == 'sources' and row['handler'] == 'card_dummy'):
                raise ValueError(f'changed targeted Scry fixture identity: {name}')


class Scene:
    def __init__(self, row, owner, mode, rows, oracle):
        self.name, self.id = row['name'], row['csvid']
        self.owner, self.foe, self.mode = owner, 1 - owner, mode
        self.rows, self.oracle = rows, oracle
        self.setup, self.steps, self.cards = [], [], {}
        self.zones = {p: {z: [] for z in ZONE} for p in (0, 1)}
        self.shuffles = [0, 0]
        self.add('source', self.name, owner, 'hand')
        self.library(owner, self.source_library())
        self.library(self.foe, FOE_DECK)

    def emit(self, op, ref='', *, prop='', **values):
        step = dict(op=op, **values)
        if ref:
            step['ref'] = ref
        if prop:
            step['property'] = prop
        self.steps.append(step)

    def add(self, ref, name, player, zone='battlefield', *, tapped=False):
        entry = dict(ref=ref, name=name, player=player, zone=zone)
        if tapped:
            entry['state'] = ['tapped']
        self.setup.append(entry)
        self.cards[ref] = name, player, zone
        self.zones[player][zone].append(name)

    def library(self, player, names):
        for index, name in enumerate(names):
            self.add(f'deck_{player}_{index}', name, player, 'library')

    def move(self, ref, zone, *, seat=None, top=False):
        name, player, old = self.cards[ref]
        destination = player if seat is None else seat
        self.zones[player][old].remove(name)
        if top:
            self.zones[destination][zone].insert(0, name)
        else:
            self.zones[destination][zone].append(name)
        self.cards[ref] = name, destination, zone

    def prop(self, ref, field, value):
        self.emit('assert_card', ref, prop=field, value=value)

    def stats(self, ref, power_delta=0, toughness_delta=0):
        name, _, _ = self.cards[ref]
        self.prop(ref, 'in_play', 1)
        self.prop(ref, 'csvid', self.rows[name]['csvid'])
        self.prop(ref, 'power', int(self.oracle[name]['power']) + power_delta)
        self.prop(ref, 'toughness', int(self.oracle[name]['toughness']) + toughness_delta)

    def mana(self, player, amounts=ZERO, *, write=False):
        for color, amount in enumerate(amounts):
            self.emit('set_mana' if write else 'assert_mana', player=player, value=amount, color=color)

    def cast(self, ref, targets=(), *, cancel=False, response=False, player_target=None):
        name, player, zone = self.cards[ref]
        assert zone == 'hand'
        self.mana(player, cost_from_oracle(self.oracle[name]), write=True)
        self.emit('refresh')
        for target in targets:
            self.emit('choose_target', target, player=player)
        if cancel:
            self.emit('choose_cancel_target', player=player)
        if player_target is not None:
            self.emit('choose_player', player=player, value=player_target)
        self.emit('cast_response' if response else 'cast', ref, value=1)
        self.emit('assert_stack', value=2 if response else 1)
        self.prop(ref, 'in_hand', 0)
        self.mana(player)
        self.emit('assert_unused_choices', value=0)

    def resolve(self, ref, *, response=False, unused=0):
        self.emit('resolve_response' if response else 'resolve')
        self.emit('assert_stack', value=0)
        self.emit('assert_unused_choices', value=unused)
        self.move(ref, 'graveyard')
        if response:
            self.move('response', 'graveyard')

    def scry_plan(self, *, bottom=True):
        # Both production menus are optional and index the inspected prefix.
        choice = 0 if bottom else -1
        self.emit('choose_zone', player=self.owner, value=choice)

    def scry_result(self, *, bottom=True):
        library = self.zones[self.owner]['library']
        if bottom and library:
            library.append(library.pop(0))

    def respond(self, target):
        self.emit('announce_spell', 'source')
        self.cast('response', (target,), response=True)
        self.move(target, 'hand')

    def receipt(self):
        self.emit('refresh')
        self.emit('assert_stack', value=0)
        names = sorted({name for name, _, _ in self.cards.values()})
        for player, zones in self.zones.items():
            self.mana(player)
            self.emit('assert_life', player=player, value=20)
            self.emit('assert_shuffle_count', player=player, value=self.shuffles[player])
            for zone, cards in zones.items():
                self.emit('assert_zone', player=player, zone=zone, value=len(cards))
                counts = Counter(cards)
                for name in names:
                    self.emit('assert_zone_multiplicity', player=player, zone=zone,
                              index=self.rows[name]['csvid'], value=counts[name])
                if zone == 'library':
                    for index, name in enumerate(cards):
                        self.emit('assert_zone_card', player=player, zone=zone, index=index,
                                  value=self.rows[name]['csvid'])

    def single(self):
        lost = self.mode == 'target_lost'
        player = self.foe if self.mode in ('target_lost', 'foe_target', 'foe_keep', 'borrowed', 'lethal') else self.owner
        name = 'Grizzly Bears' if self.mode == 'lethal' else 'Hill Giant'
        self.add('recipient', name, player)
        self.add('foil', 'Zephyr Sprite', 1 - player)
        self.stats('recipient')
        ref = 'recipient'
        if self.mode == 'borrowed':
            self.add('theft', 'Act of Treason', self.owner, 'hand')
            self.cast('theft', (ref,))
            self.resolve('theft')
            self.move(ref, 'battlefield', seat=self.owner)
            ref = 'borrowed'
            self.cards[ref] = self.cards['recipient']
            self.emit('bind_permanent_copy', ref, player=self.owner, index=0, value=self.rows[name]['csvid'])
            self.stats(ref)
        if lost:
            self.add('response', 'Unsummon', self.foe, 'hand')
        self.cast('source', (ref,))
        if lost:
            self.respond(ref)
        bottom = self.mode not in ('keep', 'foe_keep', 'survives')
        if self.mode != 'empty':
            self.scry_plan(bottom=bottom)
        self.resolve('source', response=lost, unused=int(lost))
        if not lost:
            self.scry_result(bottom=bottom)
            if self.id == VOYAGE:
                self.move(ref, 'hand', seat=player)
            elif self.mode == 'lethal':
                self.move(ref, 'graveyard')
            else:
                delta = (3, 1) if self.id == TITAN else (-2, -2)
                self.stats(ref, *delta)
                self.emit('cleanup')
                self.stats(ref)
        self.stats('foil')
        self.receipt()

    def sea(self):
        self.add('foil', 'Grizzly Bears', self.owner)
        foes = ('first', 'second', 'third')
        if self.mode != 'own_only':
            for ref, name in zip(foes, ('Hill Giant', 'Walking Corpse', 'Zephyr Sprite')):
                self.add(ref, name, self.foe)
        count = dict(zero=0, own_only=0, one=1, three=3, partial_loss=2, all_lost=1)[self.mode]
        selected = foes[:count]
        response = self.mode in ('partial_loss', 'all_lost')
        if response:
            self.add('response', 'Unsummon', self.foe, 'hand')
        self.cast('source', selected, cancel=count < 3 and self.mode != 'own_only')
        if response:
            self.respond('first')
            selected = selected[1:]
        self.scry_plan()
        self.resolve('source', response=response, unused=int(self.mode == 'all_lost'))
        if self.mode != 'all_lost':
            self.scry_result()
        for ref in selected:
            self.move(ref, 'hand')
        self.stats('foil')
        self.receipt()

    def source_library(self):
        if self.id == FILL:
            return {
                'two_both_bottom': ('Lightning Bolt', 'Giant Growth', 'Forest', 'Plains'),
                'self_one_bottom': ('Lightning Bolt', 'Giant Growth', 'Forest', 'Plains'),
                'one_short': ('Lightning Bolt',),
                'empty_hand': ('Lightning Bolt', 'Giant Growth', 'Forest', 'Plains'),
            }[self.mode]
        if self.id == PEREGRINATION:
            return {
                'both_basics': ('Lightning Bolt', 'Forest', 'Island'),
                'only_one': ('Forest', 'Lightning Bolt'),
                'no_basic': ('Yavimaya Coast',),
                'decline_all': ('Forest',),
            }[self.mode]
        return () if self.mode == 'empty' else DECK

    def spite(self):
        self.add('recipient', 'Hill Giant', self.foe)
        self.add('foil', 'Zephyr Sprite', self.owner)
        self.add('nonspell', 'Grizzly Bears', self.owner, 'graveyard')
        self.add('land', 'Forest', self.owner, 'graveyard')
        self.add('foreign_spell', 'Giant Growth', self.foe, 'graveyard')
        count = {'mixed_count': 2, 'lethal_count': 3, 'zero_spells': 0, 'target_lost': 2}[self.mode]
        for index, name in enumerate(('Unsummon', 'Divination', 'Lightning Bolt')[:count]):
            self.add(f'own_spell_{index}', name, self.owner, 'graveyard')
        lost = self.mode == 'target_lost'
        if lost:
            self.add('response', 'Unsummon', self.foe, 'hand')
        self.cast('source', ('recipient',))
        if lost:
            self.respond('recipient')
        self.scry_plan()
        self.resolve('source', response=lost, unused=int(lost))
        if not lost:
            self.scry_result()
            if count >= 3:
                self.move('recipient', 'graveyard')
            else:
                self.stats('recipient')
                self.prop('recipient', 'damage', count)
        self.stats('foil')
        self.receipt()

    def anchor(self):
        original_owner = self.owner if self.mode in ('own_keep', 'own_bottom') else self.foe
        self.add('recipient', 'Hill Giant', original_owner)
        self.add('foil', 'Zephyr Sprite', 1 - original_owner)
        self.stats('recipient')
        ref = 'recipient'
        if self.mode == 'borrowed':
            self.add('theft', 'Act of Treason', self.owner, 'hand')
            self.cast('theft', (ref,))
            self.resolve('theft')
            self.move(ref, 'battlefield', seat=self.owner)
            ref = 'borrowed'
            self.cards[ref] = self.cards['recipient']
            self.emit('bind_permanent_copy', ref, player=self.owner, index=0, value=self.rows['Hill Giant']['csvid'])
            self.stats(ref)
        lost = self.mode == 'target_lost'
        if lost:
            self.add('response', 'Unsummon', self.foe, 'hand')
        self.cast('source', (ref,))
        if lost:
            self.respond(ref)
        else:
            self.move(ref, 'library', seat=original_owner, top=True)
        bottom = self.mode != 'own_keep'
        self.scry_plan(bottom=bottom)
        self.resolve('source', response=lost, unused=int(lost))
        if not lost:
            self.scry_result(bottom=bottom)
        self.stats('foil')
        self.receipt()

    def inspection(self):
        who = self.owner if self.mode == 'own_tapped' else self.foe
        tapped = self.mode != 'untapped_only'
        self.add('recipient', 'Hill Giant', who, tapped=tapped)
        self.add('untapped_foil', 'Zephyr Sprite', 1 - who)
        self.prop('recipient', 'tapped', int(tapped))
        if not tapped:
            self.mana(self.owner, cost_from_oracle(self.oracle[self.name]), write=True)
            self.emit('refresh')
            self.emit('can_cast', 'source', value=0)
            self.emit('assert_unused_choices', value=0)
            self.prop('source', 'in_hand', 1)
            self.mana(self.owner, write=True)
            self.receipt()
            return
        response = self.mode == 'untap_response'
        if response:
            self.add('response', 'Twiddle', self.foe, 'hand')
        self.cast('source', ('recipient',))
        if response:
            self.emit('announce_spell', 'source')
            self.cast('response', ('recipient',), response=True)
            self.emit('choose_dialog', player=self.foe, value=1)
        self.scry_plan()
        self.resolve('source', response=response, unused=int(response))
        if response:
            self.stats('recipient')
            self.prop('recipient', 'tapped', 0)
        else:
            self.move('recipient', 'hand')
            self.scry_result()
        self.stats('untapped_foil')
        self.receipt()

    def fright(self):
        target = self.owner if self.mode == 'self_one_bottom' else self.foe
        discard = {'two_both_bottom': ('Hill Giant', 'Giant Growth'),
                   'self_one_bottom': ('Hill Giant', 'Giant Growth'),
                   'one_short': ('Hill Giant',), 'empty_hand': ()}[self.mode]
        for index, name in enumerate(discard):
            self.add(f'discard_{index}', name, target, 'hand')
        if target != self.owner:
            self.add('own_foil', 'Grizzly Bears', self.owner, 'hand')
        else:
            self.add('foe_foil', 'Grizzly Bears', self.foe, 'hand')
        self.cast('source', player_target=target)
        if target == 0:
            for name in discard:
                self.emit('choose_zone_card', player=target, value=self.rows[name]['csvid'])
        else:
            # The engine selects the AI card, then shows the real discard notice.
            for _ in discard:
                self.emit('choose_dialog', player=target, value=0)
        self.scry_plan()
        two = self.mode != 'one_short'
        keep_second = self.mode == 'self_one_bottom'
        if two:
            self.scry_plan(bottom=not keep_second)
        self.resolve('source')
        for index in range(len(discard)):
            self.move(f'discard_{index}', 'graveyard')
        self.scry_result()
        if two and not keep_second:
            self.scry_result()
        self.receipt()

    def harsh(self):
        candidates = {'multiple_creatures': ('Hill Giant', 'Walking Corpse', 'Forest'),
                      'no_creature': ('Giant Growth', 'Forest'), 'empty_hand': (),
                      'shroud_response': ('Hill Giant', 'Forest')}[self.mode]
        for index, name in enumerate(candidates):
            self.add(f'hand_{index}', name, self.foe, 'hand')
        self.add('own_foil', 'Grizzly Bears', self.owner, 'hand')
        lost = self.mode == 'shroud_response'
        if lost:
            self.add('response', 'Gilded Light', self.foe, 'hand')
        self.cast('source')
        if lost:
            self.emit('announce_spell', 'source')
            self.cast('response', response=True)
        if self.mode == 'multiple_creatures':
            self.emit('choose_zone_card', player=self.owner, value=self.rows['Walking Corpse']['csvid'])
        if self.mode == 'no_creature':
            self.emit('observe_search_filters', value=1)
            self.emit('choose_zone', player=self.owner, value=-1)
        self.scry_plan()
        self.resolve('source', response=lost, unused=int(lost))
        if not lost:
            self.scry_result()
        if self.mode == 'multiple_creatures':
            self.move('hand_1', 'graveyard')
        self.receipt()

    def peregrination(self):
        self.cast('source')
        selected = self.mode in ('both_basics', 'only_one')
        second = self.mode == 'both_basics'
        if selected:
            self.emit('choose_zone_card', player=self.owner, value=self.rows['Forest']['csvid'])
            if second:
                self.emit('choose_zone_card', player=self.owner, value=self.rows['Island']['csvid'])
            else:
                self.emit('choose_zone', player=self.owner, value=-1)
        else:
            self.emit('choose_zone', player=self.owner, value=-1)
        self.scry_plan(bottom=selected)
        self.resolve('source')
        zones = self.zones[self.owner]
        if selected:
            zones['library'].remove('Forest')
            zones['battlefield'].append('Forest')
            self.emit('bind_permanent_copy', 'ramp_land', player=self.owner, index=0, value=self.rows['Forest']['csvid'])
            self.prop('ramp_land', 'tapped', 1)
        if second:
            zones['library'].remove('Island')
            zones['hand'].append('Island')
        assert len(zones['library']) == 1
        self.shuffles[self.owner] = 1
        self.receipt()

    def play(self):
        self.emit('turn', player=self.owner)
        self.emit('present_ai_dialogs', value=1)
        for player in (0, 1):
            self.mana(player, write=True)
        effect = {SEA: self.sea, SPITE: self.spite, ANCHOR: self.anchor, SELECT: self.inspection,
                  FILL: self.fright, HARSH: self.harsh, PEREGRINATION: self.peregrination}.get(self.id, self.single)
        effect()
        self.emit('present_ai_dialogs', value=0)



def expand(name, csvid, declaration, shape, selected, rows, oracle):
    validate_witnesses(oracle, rows)
    if shape['csvid'] != csvid:
        raise ValueError('targeted Scry source CSVID changed')
    cases = []
    for variant in selected:
        mode, seat = variant.rsplit('_', 1)
        scene = Scene(rows[name], int(seat), mode, rows, oracle)
        scene.play()
        cases.append(dict(name=f'{LABEL}_{csvid}_{variant}', card=name, fixture='empty_main', seed=SEED,
                          setup=scene.setup, steps=scene.steps,
                          _recipe=dict(family=FAMILY, version=2, variant=variant, params=declaration['params'],
                                       oracle_text=declaration['oracle_text'])))
    return cases
