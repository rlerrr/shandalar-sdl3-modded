"""Actual conditional entry, bilateral upkeep and multiple fights."""

from functools import lru_cache
import json
from pathlib import Path

from death_counter_trigger_recipes import exact
from recipes import cost_from_oracle

FAMILY = 'fight_conditional_and_multiple'
PROFILE = Path(__file__).with_name('data') / f'{FAMILY}_wave8_profiles.json'
FEUD, RAVAGER, PREDATION, KIORA = (12742, 13757, 15513, 15356)
DARKSTEEL_MYR, HILL_GIANT = (11238, 110)
OCTOPUS, SOLDIER = (18794, 18822)
GREEN = 0x08
SEED = 2633
LABEL = 'FightConditionalAndMultipleRecipe'
MODES = {
    FEUD: ('both_fight', 'opponent_declines', 'own_declines', 'no_creatures', 'opponent_upkeep', 'opponent_hexproof'),
    RAVAGER: ('tribute_paid', 'enemy_fight', 'own_fight', 'decline', 'no_other_creature', 'reciprocal'),
    PREDATION: ('distinct', 'empty_enemy', 'opponent_tokens', 'shroud', 'reciprocal'),
    KIORA: ('untap', 'reveal_both', 'reveal_neither', 'ultimate', 'reveal_creature_only'),
}
ZA = ('hand', 'battlefield', 'library', 'graveyard', 'exile')
ENV = (0, 0, 0, 0, 0, 0)


@lru_cache(maxsize=1)
def pins():
    return json.loads(PROFILE.read_bytes())


def classify(record, csvid):
    pin = pins()['sources'].get(record.get('name'))
    if (csvid not in MODES or not pin or pin['registry']['csvid'] != csvid
            or pin['oracle'] != record or record.get('layout') != 'normal'):
        raise ValueError('complete normal conditional/multiple Fight source required')
    return dict(csvid=csvid, cost=cost_from_oracle(record))


def variants(shape):
    return [f'{mode}_{owner}' for mode in MODES[shape['csvid']] for owner in (0, 1)]


def validate_variants(shape, selected):
    if (type(selected) is not list or not selected or any(type(v) is not str for v in selected)
            or len(set(selected)) != len(selected) or set(selected) - set(variants(shape))):
        raise ValueError('explicit distinct supported conditional/multiple fights required')


def spec(declaration, record):
    pin = pins()['sources'].get(record.get('name'))
    if not pin:
        raise ValueError('pinned Fight source required')
    shape = classify(record, pin['registry']['csvid'])
    if (declaration.get('family') != FAMILY or type(declaration.get('version')) is not int
            or declaration['version'] != 2 or declaration.get('oracle_text') != record['oracle_text']
            or not exact(declaration.get('params'), shape)):
        raise ValueError('whole versioned Fight declaration required')
    validate_variants(shape, declaration.get('variants'))
    return dict(shape, family=FAMILY)


def validate_witnesses(oracle, rows):
    data = pins()
    for group in ('sources', 'witnesses'):
        for name, pin in data[group].items():
            row = rows.get(name, {})
            if (oracle.get(name) != pin['oracle'] or row != pin['registry'] or row.get('kind') != 'card'
                    or row.get('handler') in (None, 'NULL', '0')
                    or group == 'sources' and row['handler'] == 'card_dummy'):
                raise ValueError(f'changed Fight fixture identity: {name}')


class Game:
    def __init__(self, row, actor, mode, rows, oracle):
        self.id, self.name = row['csvid'], row['name']
        self.owner, self.foe, self.active = actor, 1 - actor, actor
        self.mode, self.rows, self.oracle = mode, rows, oracle
        self.setup, self.steps, self.entities = [], [], {}
        self.zones = {p: {zone: [] for zone in ZA} for p in (0, 1)}
        self.add('source', self.name, actor, 'hand')
        self.facts = {}

    def emit(self, op, ref=None, *, prop=None, **values):
        if ref is not None:
            values['ref'] = ref
        if prop is not None:
            values['counter_kind' if op == 'assert_counter' else 'property'] = prop
        self.steps.append(dict(op=op, **values))

    def add(self, ref, name, who, zone='battlefield', *, tapped=False):
        item = dict(ref=ref, player=who, zone=zone, name=name)
        if tapped:
            item['state'] = ['tapped']
        self.setup.append(item)
        self.zones[who][zone].append(name)
        if zone in ('hand', 'battlefield'):
            self.entities[ref] = (name, who, zone)

    def library(self, who, sequence):
        for index, name in enumerate(sequence):
            self.add(f'l_{who}_{index}', name, who, 'library')

    def transfer(self, name, who, former, dest):
        self.zones[who][former].remove(name)
        self.zones[who][dest].append(name)

    def move(self, ref, dest):
        name, who, former = self.entities[ref]
        self.transfer(name, who, former, dest)
        self.entities[ref] = name, who, dest

    def active_main(self, actor):
        self.active = actor
        self.emit('turn', player=actor)

    def advance(self, *, upkeep=False):
        self.active = 1 - self.active
        self.emit('advance_turn')
        self.emit('untap_step', player=self.active)
        if upkeep:
            self.emit('upkeep_step', player=self.active)
        self.active_main(self.active)

    def mana(self, who, vector=ENV, write=False):
        for color, amount in enumerate(vector):
            self.emit('set_mana' if write else 'assert_mana', player=who, color=color, value=amount)

    def prepare_payment(self, ref):
        name, player, zone = self.entities[ref]
        assert zone == 'hand' and player == self.active
        self.mana(player, cost_from_oracle(self.oracle[name]), True)
        self.emit('refresh')
        # The normal nonflash creature/artifact casting gate does not ask
        # its callback for EVENT_CAN_CAST (functions.c:802).
        if self.rows[name]['csvid'] not in (RAVAGER, DARKSTEEL_MYR, HILL_GIANT):
            self.emit('require_can_cast', ref, value=1)

    def cast(self, ref='source', destination='battlefield', before_resolve=None):
        self.prepare_payment(ref)
        self.emit('cast', ref, value=1)
        self.emit('assert_stack', value=1)
        self.mana(self.active)
        self.emit('require_card', ref, prop='in_hand', value=0)
        if before_resolve is not None:
            before_resolve()
        self.emit('resolve')
        self.move(ref, destination)
        self.emit('assert_unused_choices', value=0)

    def prop(self, ref, name, value):
        self.emit('assert_card', ref, prop=name, value=value)

    def multiplicity(self, who, zone, name, count):
        self.emit('assert_zone_multiplicity', player=who, zone=zone,
                  index=self.rows[name]['csvid'], value=count)

    def receipts(self, *extra_names):
        self.emit('refresh')
        self.emit('assert_stack', value=0)
        names = {name for zones in self.zones.values() for cards in zones.values() for name in cards}
        names.update(extra_names)
        for who, zones in self.zones.items():
            self.mana(who)
            self.emit('assert_life', player=who, value=20)
            self.emit('assert_shuffle_count', player=who, value=0)
            for zone, cards in zones.items():
                self.emit('assert_zone', player=who, zone=zone, value=len(cards))
                for name in sorted(names):
                    self.multiplicity(who, zone, name, cards.count(name))
                if zone == 'library':
                    for index, name in enumerate(cards):
                        self.emit('assert_zone_card', player=who, zone=zone, index=index,
                                  value=self.rows[name]['csvid'])

    def damage(self, ref, amount):
        self.prop(ref, 'in_play', 1)
        self.prop(ref, 'damage', amount)

    def target(self, ref, *, optional=False):
        self.emit('choose_optional_target' if optional else 'choose_target', ref, player=self.owner)

    def declare_appearance(self, who, group, *names):
        self.zones[who]['battlefield'].extend(names)
        for prop, value in group:
            for name in sorted(set(names)):
                self.emit('assert_matching_permanents', player=who, prop=prop, value=value,
                          index=self.rows[name]['csvid'])

    def activate(self, mode, loyalty_after, targets=()):
        self.emit('can_activate', 'source', value=1)
        self.emit('choose_dialog', player=self.owner, value=mode)
        for target in targets:
            self.target(target)
        self.emit('activate', 'source', value=1)
        self.emit('assert_stack', value=1)
        if loyalty_after:
            self.emit('assert_counter', 'source', prop='loyalty', value=loyalty_after)

    def do_feud(self):
        owner, foe = self.owner, self.foe
        own = ['Hill Giant', 'Mountain', 'Lightning Bolt', 'Swamp']
        enemy = ['Wall of Stone', 'Forest', 'Giant Growth', 'Plains']
        if self.mode == 'opponent_hexproof':
            self.add('hexproof', 'Leyline of Sanctity', foe)
        if self.mode == 'no_creatures':
            own[0], enemy[0] = 'Island', 'Mountain'
        self.library(owner, own)
        self.library(foe, enemy)
        self.cast()
        self.advance(upkeep=True)
        self.receipts()
        if self.mode == 'opponent_upkeep':
            return
        self.active = owner
        self.emit('advance_turn')
        self.emit('untap_step', player=owner)
        if self.mode == 'opponent_hexproof':
            self.emit('upkeep_step', player=owner)
            self.emit('assert_unused_choices', value=0)
            self.receipts()
            return
        self.emit('observe_search_filters', value=1)
        choices = {
            owner: self.mode not in ('own_declines', 'no_creatures'),
            foe: self.mode not in ('opponent_declines', 'no_creatures'),
        }
        for chooser, offered in ((foe, enemy[:3]), (owner, own[:3])):
            self.emit('choose_zone_card' if choices[chooser] else 'choose_optional_zone',
                      player=chooser, value=self.rows[offered[0]]['csvid'])
        self.emit('upkeep_step', player=owner)
        self.emit('assert_unused_choices', value=0)
        self.emit('assert_forced_zone_choices', value=0)
        for chooser, prefix in ((foe, enemy[:3]), (owner, own[:3])):
            for index, name in enumerate(prefix):
                dest = 'battlefield' if index == 0 and choices[chooser] else 'graveyard'
                self.transfer(name, chooser, 'library', dest)
        self.receipts()
        for who, ref, name, damage in ((foe, 'enemy_entry', enemy[0], 3),
                                      (owner, 'own_entry', own[0], 0)):
            if choices[who]:
                self.emit('bind_permanent', ref, player=who, index=-1, value=self.rows[name]['csvid'])
                self.damage(ref, damage if all(choices.values()) else 0)
        self.facts['both_actual_entries_fight'] = all(choices.values())

    def do_ravager(self):
        has_target = self.mode != 'no_other_creature'
        if has_target:
            self.add('friendly', 'Wall of Stone', self.owner)
            enemy_kind = 'Trained Orgg' if self.mode == 'reciprocal' else 'Wall of Stone'
            self.add('enemy', enemy_kind, self.foe)
        tribute = self.mode == 'tribute_paid'
        target = 'friendly' if self.mode == 'own_fight' else 'enemy'

        def choices():
            self.emit('choose_dialog', player=self.foe, value=int(not tribute))
            if not tribute and has_target:
                self.target(target, optional=self.mode == 'decline')

        self.cast(before_resolve=choices)
        if self.mode == 'reciprocal':
            self.move('source', 'graveyard')
            self.move('enemy', 'graveyard')
            self.damage('friendly', 0)
            self.receipts()
            return
        self.receipts()
        self.prop('source', 'in_play', 1)
        self.prop('source', 'plus1_counters', 6 if tribute else 0)
        self.prop('source', 'power', 12 if tribute else 6)
        self.prop('source', 'toughness', 12 if tribute else 6)
        if has_target:
            self.emit('assert_forced_target_choices', value=0)
            for ref in ('friendly', 'enemy'):
                fought = not tribute and self.mode != 'decline' and target == ref
                self.damage(ref, 6 if fought else 0)
        self.facts['paid_tribute'] = tribute

    def predation_body(self):
        owner, foe = self.owner, self.foe
        self.add('friendly', 'Hill Giant', owner)
        self.add('unrelated', 'Forest', foe)
        if self.mode == 'distinct':
            self.add('wall', 'Wall of Stone', foe)
            self.add('zero_power', 'Ornithopter', foe)
        elif self.mode == 'reciprocal':
            self.add('foe_creature', 'Grizzly Bears', foe)
        elif self.mode == 'opponent_tokens':
            self.add('foe_token_spell', 'Raise the Alarm', foe, 'hand')
            self.active_main(foe)
            self.cast('foe_token_spell', destination='graveyard')
            self.declare_appearance(foe, (('token', 1), ('power', 1), ('toughness', 1),
                                          ('subtype_Soldier', 1)), 'Soldier', 'Soldier')
            self.multiplicity(foe, 'battlefield', 'Soldier', 2)
            for index in range(2):
                self.emit('bind_permanent_copy', f'soldier_{index}', player=foe,
                          index=index, value=SOLDIER)
            self.active_main(owner)
        elif self.mode == 'shroud':
            self.add('shroud', 'Wall of Denial', foe)
            self.emit('can_target_creature', 'source', other='shroud', value=0)

        targets = {'distinct': ('wall', 'zero_power'), 'reciprocal': ('foe_creature',),
                   'opponent_tokens': ('soldier_0', 'soldier_1')}.get(self.mode, ())

        def choices():
            # Legacy Predation asks for a distinct target where Oracle pairs
            # each token without targeting. These fixed choices merely route
            # two legal one-to-one pairings in those boards.
            for target in targets:
                self.target(target)

        self.cast(destination='graveyard', before_resolve=choices)
        if self.mode == 'distinct':
            self.move('zero_power', 'graveyard')
            self.damage('wall', 4)
        elif self.mode == 'reciprocal':
            self.move('foe_creature', 'graveyard')
        elif self.mode == 'opponent_tokens':
            for _ in range(2):
                self.zones[foe]['battlefield'].remove('Soldier')
        elif self.mode == 'shroud':
            self.damage('shroud', 4)
        count = len(targets) + int(self.mode == 'shroud')
        if count:
            self.declare_appearance(owner, (('token', 1), ('power', 4), ('toughness', 4), ('color', GREEN),
                                            ('subtype_Beast', 1), ('subtype_Phyrexian', 1),
                                            ('damage', {'opponent_tokens': 1, 'reciprocal': 2}.get(self.mode, 0))),
                                    *(['Beast'] * count))
        self.receipts('Beast', *(['Soldier'] if self.mode == 'opponent_tokens' else []))
        self.damage('friendly', 0)
        self.facts['oracle_beasts'] = count

    def do_kiora(self):
        owner, foe = self.owner, self.foe
        if self.mode == 'untap':
            self.add('tap_creature', 'Grizzly Bears', owner, tapped=True)
            self.add('tap_land', 'Forest', owner, tapped=True)
        if self.mode.startswith('reveal_'):
            top = ['Mountain', 'Hill Giant', 'Grizzly Bears', 'Lightning Bolt']
            self.library(owner, top + ['Swamp'])
        if self.mode == 'ultimate':
            self.add('indestructible', 'Darksteel Myr', foe, 'hand')
            self.add('future_entry', 'Hill Giant', owner, 'hand')
        self.cast()
        self.emit('assert_counter', 'source', prop='loyalty', value=4)

        if self.mode == 'untap':
            for ref in ('tap_creature', 'tap_land'):
                self.prop(ref, 'tapped', 1)
            self.activate(0, 5, ('tap_creature', 'tap_land'))
            self.emit('resolve')
            self.emit('assert_unused_choices', value=0)
            for ref in ('tap_creature', 'tap_land'):
                self.prop(ref, 'tapped', 0)
            self.emit('can_activate', 'source', value=0)
        elif self.mode.startswith('reveal_'):
            chosen = {
                'reveal_both': ('Hill Giant', 'Mountain'),
                'reveal_creature_only': ('Hill Giant',),
                'reveal_neither': (),
            }[self.mode]
            self.activate(1, 2)
            self.emit('observe_search_filters', value=1)
            self.emit('choose_zone', player=0, value=-1)  # actual informational reveal
            for name in ('Hill Giant', 'Mountain'):
                self.emit('choose_zone_card' if name in chosen else 'choose_optional_zone',
                          player=owner, value=self.rows[name]['csvid'])
            self.emit('resolve')
            self.emit('assert_unused_choices', value=0)
            self.emit('assert_forced_zone_choices', value=0)
            for name in chosen:
                self.transfer(name, owner, 'library', 'hand')
            for name in top:
                if name not in chosen:
                    self.transfer(name, owner, 'library', 'graveyard')
            self.emit('can_activate', 'source', value=0)
        elif self.mode == 'ultimate':
            for loyalty in range(5, 9):
                self.activate(0, loyalty)
                self.emit('resolve')
                self.emit('assert_unused_choices', value=0)
                self.emit('can_activate', 'source', value=0)
                self.advance()
                if loyalty == 8:
                    self.cast('indestructible')
                self.advance()
            self.activate(2, 0)
            if owner == 1:
                for _ in range(3):
                    self.target('indestructible')
            self.emit('resolve')
            self.emit('assert_unused_choices', value=0)
            self.move('source', 'graveyard')
            self.declare_appearance(owner, (('token', 1), ('power', 8), ('toughness', 8), ('color', 4)),
                                    'Octopus', 'Octopus', 'Octopus')
            self.damage('indestructible', 24 if owner == 1 else 0)
            self.receipts()
            self.cast('future_entry', before_resolve=(
                lambda: self.target('indestructible')) if owner == 1 else None)
            self.damage('indestructible', 27 if owner == 1 else 0)
            self.damage('future_entry', 0)
            self.facts['emblem_damage_witness'] = 'accept native AI option' if owner == 1 else 'pass human option'
        self.receipts()

    def play(self):
        self.active_main(self.owner)
        self.emit('present_ai_dialogs', value=1)
        for who in (0, 1):
            self.mana(who, write=True)
        {FEUD: self.do_feud, RAVAGER: self.do_ravager, PREDATION: self.predation_body,
         KIORA: self.do_kiora}[self.id]()
        self.emit('present_ai_dialogs', value=0)


def expand(name, csvid, declaration, shape, selected, rows, oracle):
    validate_witnesses(oracle, rows)
    validate_variants(shape, selected)
    if pins()['sources'][name]['registry']['csvid'] != csvid:
        raise ValueError('exact registered Fight CSVID required')
    cases = []
    for variant in selected:
        mode, owner = variant.rsplit('_', 1)
        game = Game(rows[name], int(owner), mode, rows, oracle)
        game.play()
        gaps = []
        if csvid == KIORA and mode == 'ultimate' and owner == '0':
            gaps.append('Human optional emblem acceptance needs binding the hidden effect; seat one accepts live fights.')
        cases.append(dict(name=f'{LABEL}_{csvid}_{variant}', card=name, fixture='empty_main', seed=SEED,
                          setup=game.setup, steps=game.steps,
                          _recipe=dict(family=FAMILY, version=2, variant=variant, params=declaration['params'],
                                       oracle_text=declaration['oracle_text'], untested_components=gaps)))
    return cases
