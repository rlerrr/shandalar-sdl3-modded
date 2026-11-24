"""Real Dredge replacement, graveyard riders and combat/end-step boundaries."""

from collections import Counter
from functools import lru_cache
import json
from pathlib import Path

from death_counter_trigger_recipes import exact
from recipes import cost_from_oracle

FAMILY = 'mill_remaining_dredge_and_graveyard'
PROFILE = Path(__file__).with_name('data') / f'{FAMILY}_wave8_profiles.json'
DAKMOR, BROWNSCALE, THUG, MOSSDOG, NECROPLASM, IMP = (8910, 7860, 7865, 7870, 7910, 7967)
SEED = 2809
LABEL = 'MillRemainingDredgeAndGraveyardRecipe'
COMMON = ('exact', 'pass', 'short')
NECROPLASM_MODES = ('zero_end', 'opponent_end', 'growth_self')
IMP_MODES = ('flying', 'attack_destroy', 'defend_destroy', 'combat_prevented', 'noncombat')
MODES = {
    DAKMOR: (*COMMON, 'dredge_land'),
    BROWNSCALE: (*COMMON, 'raise_dead', 'bounce_non_grave'),
    THUG: (*COMMON, 'death_old', 'death_self', 'death_shroud'),
    MOSSDOG: (*COMMON, 'draw_two', 'competing', 'other_grave'),
    NECROPLASM: (*COMMON, *NECROPLASM_MODES),
    IMP: (*COMMON, *IMP_MODES),
}
AMOUNTS = {DAKMOR: 2, BROWNSCALE: 2, THUG: 4, MOSSDOG: 3, NECROPLASM: 2, IMP: 5}
ZA = ('hand', 'battlefield', 'library', 'graveyard', 'exile')
ZERO = (0, 0, 0, 0, 0, 0)
DECK = ('Forest', 'Lightning Bolt', 'Grizzly Bears', 'Island', 'Plains', 'Goblin Piker', 'Mountain')


@lru_cache(maxsize=1)
def pins():
    return json.loads(PROFILE.read_bytes())


def mana_value(cost):
    return sum(cost_from_oracle({'mana_cost': cost})) if cost != '{0}' else 0


def classify(record, csvid):
    pin = pins()['sources'].get(record.get('name'))
    if (csvid not in MODES or not pin or pin['registry']['csvid'] != csvid
            or not exact(record, pin['oracle']) or record.get('layout') != 'normal'
            or f'Dredge {AMOUNTS[csvid]} (' not in record.get('oracle_text', '')):
        raise ValueError('complete normal Dredge record required')
    cost = [0] * 6 if csvid == DAKMOR else cost_from_oracle(record)
    return dict(csvid=csvid, cost=cost, dredge=AMOUNTS[csvid])


def variants(shape):
    return [f'{mode}_{seat}' for mode in MODES[shape['csvid']] for seat in (0, 1)]


def validate_variants(shape, chosen):
    if (type(chosen) is not list or not chosen or any(type(v) is not str for v in chosen)
            or len(set(chosen)) != len(chosen) or set(chosen) - set(variants(shape))):
        raise ValueError('explicit distinct supported Dredge variants required')


def spec(declaration, record):
    pin = pins()['sources'].get(record.get('name'))
    if not pin:
        raise ValueError('pinned Dredge source required')
    shape = classify(record, pin['registry']['csvid'])
    if (declaration.get('family') != FAMILY or type(declaration.get('version')) is not int
            or declaration['version'] != 2 or declaration.get('oracle_text') != record['oracle_text']
            or not exact(declaration.get('params'), shape)):
        raise ValueError('whole versioned Dredge declaration required')
    validate_variants(shape, declaration.get('variants'))
    return dict(shape, family=FAMILY)


def validate_witnesses(oracle, rows):
    for group in ('sources', 'witnesses'):
        for name, pin in pins()[group].items():
            row = rows.get(name, {})
            if (not exact(oracle.get(name), pin['oracle']) or not exact(row, pin['registry'])
                    or row.get('kind') != 'card' or row.get('handler') in (None, 'NULL', '0')
                    or group == 'sources' and row['handler'] == 'card_dummy'):
                raise ValueError(f'changed Dredge fixture identity: {name}')


class Scene:
    def __init__(self, row, owner, mode, rows, oracle):
        self.name, self.id = row['name'], row['csvid']
        self.owner, self.foe, self.mode = owner, 1 - owner, mode
        self.amount, self.rows, self.oracle = AMOUNTS[self.id], rows, oracle
        self.setup, self.steps, self.cards = [], [], {}
        self.zones = {p: {z: [] for z in ZA} for p in (0, 1)}
        self.life = [20, 20]
        self.pools = {0: ZERO, 1: ZERO}
        self.add('old_spell', 'Unsummon', owner, 'graveyard')
        self.add('foe_old', 'Ornithopter', self.foe, 'graveyard')
        if mode == 'competing':
            self.add('alternative', 'Golgari Brownscale', owner, 'graveyard')
        source_zone = 'battlefield' if mode.startswith('death_') or mode == 'bounce_non_grave' else 'graveyard'
        if self.id == NECROPLASM and mode in NECROPLASM_MODES:
            source_zone = 'battlefield' if mode == 'opponent_end' else 'hand'
        if self.id == IMP and mode in IMP_MODES:
            source_zone = 'battlefield'
        self.add('source', self.name, self.foe if mode == 'other_grave' else owner, source_zone)
        # Fixture preparation derives production rule flags from normal hand,
        # battlefield and library cards. It does not scan grave-only seeds.
        if source_zone == 'graveyard':
            self.add('engine_card', 'Stinkweed Imp', self.foe, 'hand')

    def emit(self, op, ref=None, *, prop=None, **values):
        if ref is not None:
            values['ref'] = ref
        if prop is not None:
            values['counter_kind' if op == 'assert_counter' else 'property'] = prop
        self.steps.append(dict(op=op, **values))

    def add(self, ref, name, who, zone='battlefield'):
        self.setup.append(dict(ref=ref, player=who, zone=zone, name=name))
        self.cards[ref] = name, who, zone
        self.zones[who][zone].append(name)

    def library(self, player, names):
        for i, name in enumerate(names):
            self.add(f'deck_{player}_{i}', name, player, 'library')

    def move_name(self, name, player, old, new, *, top=False):
        self.zones[player][old].remove(name)
        dest = self.zones[player][new]
        if top:
            dest.insert(0, name)
        else:
            dest.append(name)

    def move(self, ref, new, *, top=False):
        name, who, old = self.cards[ref]
        self.move_name(name, who, old, new, top=top)
        self.cards[ref] = name, who, new

    def prop(self, ref, field, value):
        self.emit('assert_card', ref, prop=field, value=value)

    def mana(self, player, payment=ZERO, write=False):
        for color, value in enumerate(payment):
            self.emit('set_mana' if write else 'assert_mana', player=player, color=color, value=value)

    def cast(self, ref, *, target=None, grave_target=None):
        name, who, zone = self.cards[ref]
        assert zone == 'hand'
        self.mana(who, cost_from_oracle(self.oracle[name]), True)
        self.emit('refresh')
        if target:
            self.emit('choose_target', target, player=who)
        if grave_target:
            self.emit('observe_search_filters', value=1)
            self.emit('choose_zone_card', player=who, value=self.rows[grave_target]['csvid'])
        self.emit('cast', ref, value=1)
        self.emit('assert_stack', value=1)
        self.mana(who)
        self.prop(ref, 'in_hand', 0)
        self.emit('assert_unused_choices', value=0)

    def resolve(self, ref):
        self.emit('resolve')
        self.emit('assert_stack', value=0)
        self.emit('assert_unused_choices', value=0)
        self.move(ref, 'graveyard')

    def receipt(self):
        self.emit('refresh')
        self.emit('assert_stack', value=0)
        names = {name for zones in self.zones.values() for cards in zones.values() for name in cards}
        for who, zones in self.zones.items():
            self.mana(who, self.pools[who])
            self.emit('assert_life', player=who, value=self.life[who])
            self.emit('assert_shuffle_count', player=who, value=0)
            for zone, cards in zones.items():
                counts = Counter(cards)
                self.emit('assert_zone', player=who, zone=zone, value=len(cards))
                for name in sorted(names):
                    self.emit('assert_zone_multiplicity', player=who, zone=zone, index=self.rows[name]['csvid'],
                              value=counts[name])
                if zone == 'library':
                    for index, name in enumerate(cards):
                        self.emit('assert_zone_card', player=who, zone=zone, index=index,
                                  value=self.rows[name]['csvid'])

    def ordinary_draw(self, who):
        self.move_name(self.zones[who]['library'][0], who, 'library', 'hand')

    def replace_draw(self):
        who = self.owner
        cards = tuple(self.zones[who]['library'][:self.amount])
        assert len(cards) == self.amount
        for name in cards:
            self.move_name(name, who, 'library', 'graveyard')
        self.move('source', 'hand')
        if self.id == BROWNSCALE:
            self.life[who] += 2

    def draw_step(self):
        owner, foe = self.owner, self.foe
        size = {'exact': self.amount, 'short': self.amount - 1}.get(self.mode, self.amount + 2)
        self.library(owner, DECK[:size])
        self.library(foe, ('Mountain', 'Swamp', 'Plains'))
        self.emit('turn', player=foe)
        self.emit('advance_turn')
        self.emit('untap_step', player=owner)
        self.emit('upkeep_step', player=owner)
        blocked = self.mode in ('short', 'other_grave')
        pick = 2 if self.mode == 'competing' else 0 if self.mode == 'pass' or blocked else 1
        # Short and foreign-grave boards must omit the Dredge menu entirely.
        # An explicitly unused pass intent detects any erroneous offer.
        self.emit('choose_dialog', player=owner, value=pick)
        self.emit('draw_step', player=owner)
        self.emit('assert_unused_choices', value=int(blocked))
        if pick:
            self.replace_draw()
        else:
            self.ordinary_draw(owner)
        self.receipt()
        if self.mode == 'dredge_land':
            self.play_land()

    def play_land(self):
        owner, foe = self.owner, self.foe
        self.emit('turn', player=owner)
        self.emit('bind_hand_copy', 'land', player=owner, index=0, value=self.id)
        self.emit('play_land', 'land', value=1)
        self.move('source', 'battlefield')
        self.prop('land', 'tapped', 1)
        self.prop('land', 'in_play', 1)
        self.emit('can_activate', 'land', value=0)
        for who in (foe, owner):
            self.emit('advance_turn')
            self.emit('untap_step', player=who)
        self.emit('turn', player=owner)
        self.prop('land', 'tapped', 0)
        self.emit('require_can_activate', 'land', value=1)
        self.emit('activate', 'land', value=1)
        self.emit('assert_unused_choices', value=0)
        self.prop('land', 'tapped', 1)
        self.pools[owner] = (0, 1, 0, 0, 0, 0)
        self.receipt()

    def draw_two(self):
        owner, foe = self.owner, self.foe
        self.library(owner, DECK[:self.amount + 2])
        self.library(foe, ('Mountain', 'Swamp', 'Plains'))
        self.add('draw_spell', 'Divination', owner, 'hand')
        self.cast('draw_spell')
        self.emit('choose_dialog', player=owner, value=1)
        self.resolve('draw_spell')
        self.replace_draw()
        self.ordinary_draw(owner)
        self.receipt()

    def brownscale_move(self):
        owner, foe = self.owner, self.foe
        self.library(owner, DECK[:3])
        if self.mode == 'raise_dead':
            self.add('recovery', 'Raise Dead', owner, 'hand')
            self.cast('recovery', grave_target=self.name)
            self.resolve('recovery')
            self.move('source', 'hand')
            self.life[owner] += 2
        else:
            self.add('bounce', 'Unsummon', foe, 'hand')
            self.prop('source', 'power', 2)
            self.prop('source', 'toughness', 3)
            self.cast('bounce', target='source')
            self.resolve('bounce')
            self.move('source', 'hand')
        self.receipt()

    def thug_death(self):
        owner, foe = self.owner, self.foe
        self.library(owner, DECK[:4])
        self.library(foe, ('Mountain', 'Plains', 'Swamp'))
        self.add('old_creature', 'Hill Giant', owner, 'graveyard')
        self.add('removal', 'Murder', foe, 'hand')
        if self.mode == 'death_shroud':
            self.add('protection', 'Silent Gravestone', owner)
        self.prop('source', 'power', 1)
        self.prop('source', 'toughness', 1)
        self.cast('removal', target='source')
        picked = {'death_old': 'Hill Giant', 'death_self': self.name}.get(self.mode)
        if picked:
            self.emit('observe_search_filters', value=1)
            self.emit('choose_zone_card', player=owner, value=self.rows[picked]['csvid'])
        self.resolve('removal')
        self.move('source', 'graveyard')
        if picked:
            self.move_name(picked, owner, 'graveyard', 'library', top=True)
            self.emit('assert_offered_selection_required', value=1)
            self.emit('assert_offered_legal_card', player=owner, index=self.rows[picked]['csvid'], value=1)
            self.emit('assert_offered_legal_card', player=owner, index=self.rows['Unsummon']['csvid'], value=0)
        self.receipt()

    def step_into_upkeep(self, who):
        self.emit('untap_step', player=who)
        self.emit('upkeep_step', player=who)
        self.emit('assert_unused_choices', value=0)

    def necroplasm_counters(self, count):
        self.emit('assert_counter', 'source', prop='plus1', value=count)
        self.prop('source', 'power', 1 + count)
        self.prop('source', 'toughness', 1 + count)

    def necroplasm(self):
        owner, foe = self.owner, self.foe
        lines = (
            ('Memnite', 'Ornithopter'),
            ('Savannah Lions', 'Expedition Envoy'),
            ('Grizzly Bears', 'Walking Corpse'),
            ('Standing Troops', 'Wall of Air'),
        )
        for mv, (mine, theirs) in enumerate(lines):
            for who, name in ((owner, mine), (foe, theirs)):
                assert mana_value(self.oracle[name]['mana_cost']) == mv
                self.add(f'mv_{mv}_{who}', name, who)
        self.add('land_foil', 'Forest', owner)
        self.add('artifact_foil', "Tormod's Crypt", foe)
        if self.mode == 'opponent_end':
            self.emit('turn', player=foe)
            self.necroplasm_counters(0)
            self.emit('advance_turn')
            self.necroplasm_counters(0)
            self.receipt()
            return
        self.cast('source')
        self.emit('resolve')
        self.emit('assert_stack', value=0)
        self.emit('assert_unused_choices', value=0)
        self.move('source', 'battlefield')
        self.receipt()
        stages = 4 if self.mode == 'growth_self' else 1
        for count in range(stages):
            self.necroplasm_counters(count)
            # advance_turn performs the current player's real end trigger, then
            # starts the other player's turn without a second same-turn end.
            self.emit('advance_turn')
            for who in (0, 1):
                self.move(f'mv_{count}_{who}', 'graveyard')
            if count == 3:
                self.move('source', 'graveyard')
            self.receipt()
            if count + 1 == stages:
                break
            self.step_into_upkeep(foe)
            self.necroplasm_counters(count)
            self.receipt()
            self.emit('advance_turn')
            self.necroplasm_counters(count)
            self.step_into_upkeep(owner)
            self.necroplasm_counters(count + 1)
            self.receipt()

    def imp_noncombat(self):
        owner, foe = self.owner, self.foe
        self.add('grant', 'Hermetic Study', owner, 'hand')
        self.add('recipient', 'Wall of Air', foe)
        self.cast('grant', target='source')
        self.emit('resolve')
        self.emit('assert_unused_choices', value=0)
        self.move('grant', 'battlefield')
        self.emit('require_can_activate', 'grant', value=1)
        self.emit('choose_target', 'recipient', player=owner)
        self.emit('activate', 'grant', value=1)
        self.emit('assert_stack', value=1)
        self.emit('resolve')
        self.emit('assert_unused_choices', value=0)
        self.prop('source', 'tapped', 1)
        self.prop('recipient', 'in_play', 1)
        self.prop('recipient', 'damage', 1)
        self.receipt()

    def imp(self):
        owner, foe = self.owner, self.foe
        if self.mode == 'noncombat':
            self.imp_noncombat()
            return
        defends = self.mode == 'defend_destroy'
        fog = self.mode == 'combat_prevented'
        blocked = self.mode != 'flying'
        recipient = 'Phantom Monster' if defends else 'Wall of Air' if blocked else 'Grizzly Bears'
        self.add('recipient', recipient, foe)
        if fog:
            self.add('fog', 'Fog', foe, 'hand')
        if defends:
            self.emit('turn', player=foe)
        attacker, blocker = ('recipient', 'source') if defends else ('source', 'recipient')
        self.emit('begin_attackers')
        self.emit('attack', attacker, value=1)
        self.emit('finish_attackers')
        self.emit('begin_blockers')
        self.emit('block', blocker, other=attacker, value=int(blocked))
        self.emit('advance_combat')
        self.prop(attacker, 'blocked', int(blocked))
        if fog:
            self.cast('fog')
            self.resolve('fog')
        self.emit('combat_damage')
        self.emit('resolve')
        self.emit('assert_unused_choices', value=0)
        if not blocked:
            self.life[foe] -= 1
            self.prop('recipient', 'damage', 0)
        elif fog:
            for ref in ('source', 'recipient'):
                self.prop(ref, 'in_play', 1)
                self.prop(ref, 'damage', 0)
        else:
            self.move('recipient', 'graveyard')
            if defends:
                self.move('source', 'graveyard')
            else:
                self.prop('source', 'in_play', 1)
                self.prop('source', 'damage', 1)
        self.emit('end_combat')
        self.receipt()

    def play(self):
        self.emit('turn', player=self.owner)
        self.emit('present_ai_dialogs', value=1)
        for who in (0, 1):
            self.mana(who, write=True)
        if self.id == NECROPLASM and self.mode in NECROPLASM_MODES:
            self.necroplasm()
        elif self.id == IMP and self.mode in IMP_MODES:
            self.imp()
        elif self.mode in ('raise_dead', 'bounce_non_grave'):
            self.brownscale_move()
        elif self.mode.startswith('death_'):
            self.thug_death()
        elif self.mode == 'draw_two':
            self.draw_two()
        else:
            self.draw_step()
        self.emit('present_ai_dialogs', value=0)


def expand(name, csvid, declaration, shape, selected, rows, oracle):
    validate_witnesses(oracle, rows)
    validate_variants(shape, selected)
    if pins()['sources'][name]['registry']['csvid'] != csvid:
        raise ValueError('exact registered Dredge CSVID required')
    cases = []
    for variant in selected:
        mode, seat = variant.rsplit('_', 1)
        scene = Scene(rows[name], int(seat), mode, rows, oracle)
        scene.play()
        cases.append(dict(name=f'{LABEL}_{csvid}_{variant}', card=name, fixture='empty_main', seed=SEED,
                          setup=scene.setup, steps=scene.steps,
                          _recipe=dict(family=FAMILY, version=2, variant=variant, params=declaration['params'],
                                       oracle_text=declaration['oracle_text'], untested_components=[])))
    return cases
