"""Exercise targeted and paid Landfall effects through actual land entry."""

from collections import Counter
from functools import lru_cache
import json
from pathlib import Path

from landfall_case_support import LandfallBoard, landfall_lines
from library_search_recipes import wave6_exact


FAMILY = 'landfall_targets_and_payment'
PROFILE = Path(__file__).parent / 'data' / f'{FAMILY}_wave8_profiles.json'
LAND = 'Landfall — Whenever a land you control enters, '
MODELS = {
    (LAND + 'target player mills three cards. (They put the top three cards of their library into their graveyard.)',):
        ('mill', 'hedron_crab', ('Landfall', 'Mill'), ()),
    (LAND + 'you may have target player lose 3 life. If you do, put three +1/+1 counters on Ob Nixilis.',):
        ('loss', 'ob_nixilis_the_fallen', ('Landfall',), ()),
    (LAND + 'you may pay {2}. If you do, draw a card.',):
        ('draw_paid', 'seer_sundial', ('Landfall',), (2, 0, 0, 0, 0, 0)),
    (LAND + 'untap this creature.', '{T}: Untap target land.'):
        ('land_untap', 'stoneseeder_hierophant', ('Landfall',), ()),
    ('{U}, {T}: You may tap or untap another target creature.', LAND + 'you may untap this creature.'):
        ('creature_twiddle', 'tideforce_elemental', ('Landfall',), ()),
    (LAND + 'you may pay {3}{G}. If you do, put two +1/+1 counters on this creature.',):
        ('growth_paid', 'murasa_ranger', ('Landfall',), (3, 0, 0, 1, 0, 0)),
    (LAND + 'this creature deals 1 damage to each opponent.',):
        ('damage', 'tunneling_geopede', ('Landfall',), ()),
}
COMMON = ('own', 'enemy', 'offturn', 'repeat', 'replay', 'removed')
SCENES = {
    'mill': ('self_target', 'short_library', 'empty_library'),
    'loss': ('self_target', 'prevention', 'decline'),
    'draw_paid': ('fee_short',),
    'land_untap': ('ability_own', 'fresh', 'ability_filter'),
    'creature_twiddle': ('ability_untap', 'ability_own', 'fresh', 'ability_filter', 'ability_short'),
    'growth_paid': ('fee_short', 'wrong_color'),
    'damage': ('prevention',),
}
PAID = {'draw_paid', 'growth_paid'}
UNTAP = {'land_untap', 'creature_twiddle'}
OPTIONAL_TRIGGER = PAID | {'creature_twiddle'}
LIBRARY = ('Mountain', 'Plains', 'Forest', 'Island', 'Swamp', 'Grizzly Bears')
SUPPORT = set(LIBRARY) | {'Barren Moor', 'Growth Spiral', 'Safe Passage', 'Twiddle', 'Hill Giant'}
VANILLA = set(LIBRARY) | {'Hill Giant'}


@lru_cache(maxsize=1)
def profiles():
    return json.loads(PROFILE.read_text(encoding='utf8'))


def classify(record):
    from recipes import cost_from_oracle

    matched = MODELS.get(tuple(landfall_lines(record)))
    if not matched:
        raise ValueError('unsupported whole printed Landfall effect')
    effect, _, keywords, fee = matched
    if set(record['keywords']) != set(keywords):
        raise ValueError('whole Landfall keyword set must be pinned')
    creature = 'Creature' in record['type_line'].split(' — ')[0]
    stats = [int(record[k]) for k in ('power', 'toughness')] if creature else []
    return dict(effect=effect, mana=cost_from_oracle(record), fee=list(fee), stats=stats)


def handler(record):
    return 'manalinkeh_card_' + MODELS[tuple(landfall_lines(record))][1]


def variants(shape):
    effect = shape['effect']
    variants = [f'{mode}_{owner}' for mode in (*COMMON, *SCENES[effect]) for owner in (0, 1)]
    if effect in OPTIONAL_TRIGGER:
        variants.append('decline_0')
    return variants


def spec(declaration, record):
    shape = classify(record)
    source = profiles()['sources'].get(record['name'])
    chosen = declaration.get('variants')
    if (not source or not wave6_exact(source['oracle'], record) or declaration.get('family') != FAMILY
            or type(declaration.get('version')) is not int or declaration['version'] != 2
            or declaration.get('oracle_text') != record['oracle_text']
            or not wave6_exact(declaration.get('params'), shape) or type(chosen) is not list or not chosen
            or any(type(v) is not str for v in chosen) or len(set(chosen)) != len(chosen)
            or set(chosen) - set(variants(shape))):
        raise ValueError('exact pinned Landfall source and typed variants required')
    return dict(shape, family=FAMILY)


def validate_witnesses(oracle, rows):
    pins = profiles()
    if pins['version'] != 1 or set(pins['witnesses']) != SUPPORT:
        raise ValueError('complete Landfall witness profile required')
    for group in ('sources', 'witnesses'):
        for name, pin in pins[group].items():
            record, row = oracle[name], rows[name]
            if (not wave6_exact(pin, dict(oracle=record, registry=row)) or record['layout'] != 'normal'
                    or row['kind'] != 'card' or row['handler'] in (None, 'NULL', '0')
                    or (row['handler'] == 'card_dummy' and name not in VANILLA)):
                raise ValueError(f'changed primary or value witness {name}')
            if group == 'sources' and row['handler'] != handler(record):
                raise ValueError(f'changed source handler {name}')


class TargetPaymentBoard(LandfallBoard):
    def __init__(self, name, shape, mode, owner, rows, oracle):
        self.life = [20, 20]
        self.libraries = [[], []]
        self.counters = 0
        super().__init__(name, shape, mode, owner, rows, oracle)

    @property
    def effect(self):
        return self.shape['effect']

    def build(self):
        effect, owner, foe, mode = self.effect, self.owner, self.foe, self.mode
        if mode == 'fresh' or mode == 'ability_short':
            self.add('Grizzly Bears' if effect == 'creature_twiddle' else 'Island', 'other', foe)
            return
        land = 'Barren Moor' if effect in PAID else 'Forest'
        self.add(land, 'land', foe if mode == 'enemy' else owner, 'hand')
        if mode in ('repeat', 'replay'):
            self.add(land, 'extra_land', owner, 'hand')
        if mode in ('offturn', 'repeat', 'replay'):
            self.add('Growth Spiral', 'spiral', owner, 'hand')
        if mode == 'prevention':
            self.add('Safe Passage', 'shield', foe, 'hand')
        if effect in UNTAP and mode != 'removed':
            creature = effect == 'creature_twiddle'
            target_owner = owner if mode in ('ability_own', 'ability_untap') else foe
            self.add('Hill Giant' if creature else 'Island', 'other', target_owner)
            if not creature or mode == 'ability_untap':
                self.add('Twiddle', 'tap_witness', foe, 'hand')
            if mode == 'ability_filter' and not creature:
                self.add('Grizzly Bears', 'wrong_target', owner)
            if mode in ('repeat', 'replay'):
                self.add('Twiddle', 'tap_again', foe, 'hand')
        for seat in (0, 1):
            sequence = LIBRARY
            if seat == foe and mode == 'short_library':
                sequence = LIBRARY[:2]
            if seat == foe and mode == 'empty_library':
                sequence = ()
            for index, name in enumerate(sequence):
                ref = f'library_{seat}_{index}'
                self.add(name, ref, seat, 'library')
                self.libraries[seat].append(ref)

    def stat(self, ref, prop, value):
        self.emit('assert_card', ref=ref, property=prop, value=int(value))

    def move(self, ref, zone):
        self.cards[ref]['zone'] = zone

    def zone_count(self, seat, zone, name, amount):
        self.emit('assert_zone_multiplicity', player=seat, zone=zone, index=self.rows[name]['csvid'], value=amount)

    def zones(self):
        for seat in (0, 1):
            self.emit('assert_life', player=seat, value=self.life[seat])
            for zone in ('hand', 'battlefield', 'library', 'graveyard', 'exile'):
                counts = Counter(card['name'] for card in self.cards.values()
                                 if card['player'] == seat and card['zone'] == zone)
                self.emit('assert_zone', player=seat, zone=zone, value=counts.total())
                for name, count in sorted(counts.items()):
                    self.zone_count(seat, zone, name, count)
            for index, ref in enumerate(self.libraries[seat]):
                self.emit('assert_zone_card', player=seat, zone='library', index=index, value=self.rows[self.cards[ref]['name']]['csvid'])

    def facts(self):
        self.emit('assert_stack', value=0)
        self.emit('assert_unused_choices', value=0)
        self.observe_mana()
        self.zones()
        if self.cards['source']['zone'] == 'battlefield':
            for key, value in zip(('power', 'toughness'), self.shape['stats']):
                self.stat('source', key, value + self.counters)
            if self.effect in ('loss', 'growth_paid'):
                self.emit('assert_counter', ref='source', counter_kind='plus1', value=self.counters)
            if self.effect in UNTAP:
                self.stat('source', 'tapped', self.cards['source']['tapped'])
        if 'other' in self.cards:
            self.stat('other', 'tapped', self.cards['other']['tapped'])

    def slice_library(self, who, amount, zone):
        cards = self.libraries[who]
        if zone == 'hand' and len(cards) < amount:
            raise ValueError('bounded Landfall fixture cannot draw from an empty library')
        for ref in cards[:amount]:
            self.move(ref, zone)
        del cards[:amount]

    def target_player(self):
        return self.owner if self.mode == 'self_target' else self.foe

    def payment(self):
        if self.effect not in PAID:
            return [0]*6
        cost = list(self.shape['fee'])
        if self.mode == 'fee_short':
            cost[0] -= 1
        if self.mode == 'wrong_color':
            cost[3], cost[5] = 0, cost[3]
        return cost

    def prepare_trigger(self):
        if self.effect in ('mill', 'loss'):
            if self.mode == 'decline':
                self.emit('choose_cancel_target', player=self.owner)
            else:
                self.emit('choose_player', player=self.owner, value=self.target_player())
        elif self.effect in OPTIONAL_TRIGGER and self.owner == 0 and self.mode != 'decline':
            self.emit('choose_trigger_source', player=self.owner, ref='source')

    def apply_trigger(self):
        if self.mode == 'decline':
            return
        effect, who = self.effect, self.owner
        if effect == 'mill':
            self.slice_library(self.target_player(), 3, 'graveyard')
        elif effect == 'loss':
            self.life[self.target_player()] -= 3
            self.counters += 3
        elif effect == 'damage' and self.mode != 'prevention':
            self.life[self.foe] -= 1
        elif effect in UNTAP:
            self.cards['source']['tapped'] = False
        elif effect in PAID and self.mode not in ('fee_short', 'wrong_color'):
            self.pools[who] = [amount - cost for amount, cost in zip(self.pools[who], self.shape['fee'])]
            if min(self.pools[who]) < 0:
                raise ValueError('Oracle-required Landfall fee exceeds prepared mana')
            if effect == 'draw_paid':
                self.slice_library(who, 1, 'hand')
            else:
                self.counters += 2

    def enter(self, ref, *, spiral=False):
        from recipes import cost_from_oracle

        item = self.cards[ref]
        triggers = item['player'] == self.owner and self.cards['source']['zone'] == 'battlefield'
        pool = self.payment() if triggers else [0]*6
        self.fund(self.owner, pool)
        if spiral:
            fee = cost_from_oracle(self.oracle['Growth Spiral'])
            cash = [a+b for a,b in zip(pool, fee)]
            self.cast_trigger_spell('spiral', funding=cash, resolve=False)
            self.emit('choose_zone_card', player=self.owner, value=self.rows[item['name']]['csvid'])
            if triggers:
                self.prepare_trigger()
            self.emit('resolve')
            self.move('spiral', 'graveyard')
            self.slice_library(self.owner, 1, 'hand')
            self.move(ref, 'battlefield')
            self.emit('bind_permanent_copy', ref=ref, player=self.owner, value=self.rows[item['name']]['csvid'],
                      index=int(ref == 'extra_land'))
            self.emit('refresh')
            self.stat(ref, 'in_play', 1)
        else:
            self.play_land(ref, choices=(lambda board: board.prepare_trigger()) if triggers else None)
        self.emit('assert_unused_choices', value=0)
        self.stat(ref, 'tapped', self.effect in PAID)
        if triggers:
            self.apply_trigger()
        self.observe_mana()

    def tap_with_twiddle(self, ref, target):
        self.cast_trigger_spell(ref, target=target, choices=lambda board:
                                board.emit('choose_dialog', player=self.foe, value=0))
        self.cards[target]['tapped'] = True
        self.stat(target, 'tapped', 1)

    def trigger_activation(self):
        effect, mode, owner = self.effect, self.mode, self.owner
        creature = effect == 'creature_twiddle'
        if mode in ('fresh', 'ability_short'):
            if mode == 'ability_short':
                self.advance_turn()
                self.advance_turn()
            self.fund(owner, [0,0,int(creature and mode=='fresh'),0,0,0])
            self.emit('can_activate', ref='source', value=0)
            self.emit('activate', ref='source', value=0)
            return False
        self.advance_turn()
        self.advance_turn()
        untap = not creature or mode == 'ability_untap'
        if untap:
            self.tap_with_twiddle('tap_witness', 'other')
        self.fund(owner, [0,0,int(creature),0,0,0])
        if mode == 'ability_filter':
            target = 'source' if creature else 'wrong_target'
            self.emit('probe_activation_target', ref='source', other=target, index=-1, value=0)
        self.emit('can_activate', ref='source', value=1)
        self.emit('choose_target', player=owner, ref='other')
        if not creature:
            # A second identical target is safe input for the legacy duplicate
            # prompt. The printed single target leaves this intent unused.
            self.emit('choose_target', player=owner, ref='other')
        self.emit('activate', ref='source', value=1)
        if not creature:
            self.emit('assert_unused_choices', value=1)
        self.pools[owner] = [0]*6
        self.cards['source']['tapped'] = True
        self.stat('source', 'tapped', 1)
        self.stat('other', 'tapped', untap)
        self.emit('assert_stack', value=1)
        if creature:
            self.emit('choose_dialog', player=owner, value=int(untap))
        self.emit('resolve')
        self.emit('assert_unused_choices', value=0)
        self.cards['other']['tapped'] = not untap
        self.stat('other', 'tapped', not untap)
        self.emit('can_activate', ref='source', value=0)
        return True

    def run(self):
        self.play_source()
        if self.mode == 'removed':
            self.emit('destroy', ref='source')
            self.move('source', 'graveyard')
        if self.effect in UNTAP and self.mode != 'removed' and not self.trigger_activation():
            self.facts()
            return
        if self.mode in ('enemy', 'offturn'):
            self.advance_turn()
        if self.mode == 'prevention':
            self.cast_trigger_spell('shield')
        self.enter('land', spiral=self.mode == 'offturn')
        self.facts()
        if self.mode in ('repeat', 'replay'):
            if self.effect in UNTAP:
                self.tap_with_twiddle('tap_again', 'source')
            self.enter('extra_land', spiral=True)
            self.facts()


def expand(name, csvid, declaration, shape, selected, rows, oracle):
    from recipes import step

    result = []
    for label in selected:
        mode, seat = label.rsplit('_', 1)
        board = TargetPaymentBoard(name, shape, mode, int(seat), rows, oracle)
        board.emit('turn', player=board.owner)
        board.emit('present_ai_dialogs', value=1)
        for who in (0, 1):
            board.fund(who, [0]*6)
        start = len(board.steps)
        board.run()
        if mode == 'replay':
            initial, effect = board.steps[:start], board.steps[start:]
            board.steps = initial + [step('capture')] + effect + [step('record_state'), step('restore'),
                                                                  step('ai_roundtrip')] + effect + [step('assert_recorded_state')]
        gaps = []
        if shape['effect'] in OPTIONAL_TRIGGER:
            gaps.append('Optional trigger decline for the AI seat is governed by its ordinary policy.')
        if shape['effect'] == 'loss':
            gaps.append('The legacy decline occurs at the target selector. An independent pending target and resolution decision are unavailable.')
        result.append(dict(name=f'LandfallTargetsAndPaymentRecipe_{csvid}_{label}', card=name, fixture='empty_main',
                           seed=123, setup=board.setup, steps=board.steps, _recipe=dict(family=FAMILY, version=2,
                           variant=label, params=declaration['params'], oracle_text=declaration['oracle_text'],
                           untested_components=gaps)))
    return result
