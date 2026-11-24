"""Printed, targeted fight, paired creature ownership and settlement."""

from functools import lru_cache
import json
from pathlib import Path

from library_search_recipes import wave6_exact as exact
from recipes import card, cost_from_oracle, step


FAMILY = 'fight_targeted_spells'
PROFILE = Path(__file__).parent / 'data' / f'{FAMILY}_wave8_profiles.json'
MODELS = {
    13464: ('{G}', 'Sorcery', (2, 2), (
        'Target green creature you control gets +2/+2 until end of turn. '
        'It fights target green creature an opponent controls.',),
        ('wrong_own_color', 'through_fog')),
    14368: ('{1}{G}', 'Sorcery', (2, 2), (
        "Target creature you control fights target creature you don't control.",
        'Ferocious — The creature you control gets +2/+2 until end of turn before it fights if you control '
        'a creature with power 4 or greater.'),
        ('ferocious_late', 'ferocious_removed', 'ferocious_opponent_only')),
    14398: ('{3}{R}', 'Instant', (1, 0), (
        'Target creature you control gets +1/+0 until end of turn. '
        "It fights target creature you don't control.",),
        ('offturn_response', 'through_fog')),
    14913: ('{1}{G}', 'Sorcery', (1, 2), (
        'Target creature you control gets +1/+2 until end of turn. '
        "It fights target creature you don't control. (Each deals damage equal to its power to the other.)",),
        ('lethal_toughness', 'through_fog')),
    17480: ('{1}{G}', 'Instant', (0, 0), (
        "Target creature you control fights target creature you don't control. (Each deals damage equal to its power to the other.)",),
        ('offturn_response', 'mutual_death')),
    15253: ('{3}{G}', 'Sorcery', (2, 2), (
        'Target creature you control gets +2/+2 until end of turn. '
        'It fights target creature an opponent controls. (Each deals damage equal to its power to the other.)',),
        ('non_green_targets', 'countered')),
}
COMMON = ('mutual', 'enemy_bounced', 'own_bounced', 'no_opposing_target')
GREEN = 'Folk of the Pines'
NON_GREEN = 'Scion of Glaciers'
HIGH = 'Sengir Vampire'
SMALL = 'Elvish Mystic'
ARTIFACT = "Traveler's Amulet"
ZONES = ('hand', 'battlefield', 'library', 'graveyard', 'exile')
ZERO = (0,) * 6
SEED = 2633

WITNESSES = {GREEN, NON_GREEN, HIGH, SMALL, ARTIFACT, 'Unsummon', 'Giant Growth', 'Fog', 'Counterspell'}


class FightCase:
    def __init__(self, record, csvid, owner, mode, rows, oracle):
        self.record, self.csvid, self.owner, self.other, self.mode = record, csvid, owner, 1 - owner, mode
        self.rows, self.oracle = rows, oracle
        self.setup, self.steps, self.cards = [], [], {}
        self.funded = [ZERO, ZERO]
        self.effects = []
        self.add('source', owner, record['name'], 'hand')

        own = NON_GREEN if mode in ('wrong_own_color', 'non_green_targets') else GREEN
        if mode in ('lethal_toughness', 'mutual_death'):
            own = SMALL
        foe = NON_GREEN if mode in ('lethal_toughness', 'non_green_targets') else GREEN
        if mode == 'no_opposing_target' and csvid == 13464:
            foe = NON_GREEN
        elif mode == 'mutual_death':
            foe = SMALL
        self.add('own', owner, own)
        if mode == 'no_opposing_target' and csvid != 13464:
            self.add('own_extra', owner, SMALL)
            self.add('foe_artifact', self.other, ARTIFACT)
        else:
            self.add('foe', self.other, foe)
        if mode in ('enemy_bounced', 'own_bounced', 'ferocious_removed'):
            self.add('reply', owner if mode == 'enemy_bounced' else self.other, 'Unsummon', 'hand')
        if mode == 'ferocious_removed' or (mode == 'enemy_bounced' and csvid == 14368):
            self.add('threshold', owner, HIGH)
        elif mode == 'ferocious_late':
            self.add('threshold', owner, SMALL)
            self.add('reply', owner, 'Giant Growth', 'hand')
        elif mode == 'ferocious_opponent_only':
            self.add('threshold', self.other, HIGH)
        if mode == 'through_fog':
            self.add('fog', owner, 'Fog', 'hand')
        if mode == 'offturn_response':
            self.add('parent', self.other, ARTIFACT, 'hand')
        elif mode == 'countered':
            self.add('counter', self.other, 'Counterspell', 'hand')

    def add(self, ref, player, name, zone='battlefield'):
        record = self.oracle[name]
        self.setup.append(card(name, ref, player, zone))
        self.cards[ref] = dict(
            name=name, player=player, zone=zone, damage=0, bonus=(0, 0),
            stats=tuple(int(record[p]) for p in ('power', 'toughness')) if 'power' in record else None)

    def emit(self, op, ref=None, prop=None, *, player=None, value=None, zone=None, index=None, color=None):
        values = dict(ref=ref, property=prop, player=player, value=value, zone=zone, index=index, color=color)
        self.steps.append(step(op, **{name: value for name, value in values.items() if value is not None}))

    def card(self, ref, prop, value):
        self.emit('assert_card', ref, prop, value=value)

    def stats(self, ref):
        card = self.cards[ref]
        return tuple(base + boost for base, boost in zip(card['stats'], card['bonus']))

    def fund(self, player, pool):
        assert not any(self.funded[player])
        self.funded[player] = pool
        for color, amount in enumerate(pool):
            self.emit('set_mana', player=player, color=color, value=amount)

    def mana_receipts(self):
        for player, wallet in enumerate(self.funded):
            for color, value in enumerate(wallet):
                self.emit('assert_mana', player=player, color=color, value=value)

    def board(self):
        for ref, card in self.cards.items():
            if card['zone'] != 'battlefield':
                continue
            self.card(ref, 'in_play', 1)
            if card['stats'] is not None:
                power, toughness = self.stats(ref)
                self.card(ref, 'power', power)
                self.card(ref, 'toughness', toughness)
                self.card(ref, 'damage', card['damage'])
                self.card(ref, 'tapped', 0)
                self.card(ref, 'attacking', 0)

    def receipt(self):
        self.board()
        self.emit('assert_stack', value=0)
        self.emit('assert_choices_consumed')
        self.mana_receipts()
        for player in (0, 1):
            for zone in ZONES:
                cards = [c for c in self.cards.values() if c['player'] == player and c['zone'] == zone]
                self.emit('assert_zone', player=player, zone=zone, value=len(cards))
                for name in sorted({c['name'] for c in self.cards.values()}):
                    self.emit('assert_zone_multiplicity', player=player, zone=zone, index=self.rows[name]['csvid'],
                              value=sum(c['name'] == name for c in cards))
            self.emit('assert_life', player=player, value=20)

    def cast(self, ref, *, targets=(), response=False, legal=True):
        current = self.cards[ref]
        name, player = current['name'], current['player']
        self.fund(player, cost_from_oracle(self.oracle[name]))
        self.emit('refresh')
        if not legal:
            self.emit('can_cast', ref, value=0)
            return
        if ref == 'source':
            self.emit('require_can_cast', ref, value=1)
        for target in targets:
            self.emit('choose_target', target, player=player)
        self.emit('cast_response' if response else 'cast', ref, value=1)
        self.emit('assert_choices_consumed')
        self.emit('assert_stack', value=2 if response else 1)
        self.card(ref, 'in_hand', 0)
        self.funded[player] = ZERO
        self.mana_receipts()
        current['zone'] = 'stack'

    def resolve(self, *, response=False):
        self.emit('resolve_response' if response else 'resolve')
        self.emit('refresh')
        self.emit('assert_choices_consumed')
        self.emit('assert_stack', value=0)

    def apply_fight(self):
        own = self.cards['own']
        foe = self.cards['foe']
        own_valid = own['zone'] == 'battlefield'
        foe_valid = foe['zone'] == 'battlefield'
        ferocious = (self.csvid == 14368 and any(
            card['zone'] == 'battlefield' and card['player'] == self.owner and card['stats'] is not None
            and self.stats(ref)[0] >= 4 for ref, card in self.cards.items()))
        boost = MODELS[self.csvid][2] if own_valid and (self.csvid != 14368 or ferocious) else (0, 0)
        own['bonus'] = boost
        hit = (0, 0)
        if own_valid and foe_valid:
            power, toughness = self.stats('own')
            opponent_power, opponent_toughness = self.stats('foe')
            own['damage'] += opponent_power
            foe['damage'] += power
            hit = (opponent_power, power)
            if own['damage'] >= toughness:
                own['zone'] = 'graveyard'
            if foe['damage'] >= opponent_toughness:
                foe['zone'] = 'graveyard'
        self.cards['source']['zone'] = 'graveyard'
        self.effects.append(dict(own_target_valid=own_valid, opposing_target_valid=foe_valid, ferocious=ferocious,
                                 paid_pump=list(boost), dealt_to_own_and_opposing=list(hit)))

    def cleanup(self):
        self.emit('cleanup')
        self.emit('refresh')
        for card in self.cards.values():
            card.update(damage=0, bonus=(0, 0))
        self.receipt()

    def play(self):
        turn = self.other if self.mode == 'offturn_response' else self.owner
        self.emit('turn', player=turn)
        self.emit('present_ai_dialogs', value=1)
        self.fund(0, ZERO)
        self.fund(1, ZERO)
        self.emit('refresh')
        self.receipt()
        if self.mode in ('no_opposing_target', 'wrong_own_color'):
            self.cast('source', legal=False)
            self.receipt()
            return
        if self.mode == 'through_fog':
            self.cast('fog')
            self.resolve()
            self.cards['fog']['zone'] = 'graveyard'
            self.receipt()
        if self.mode == 'offturn_response':
            self.cast('parent')
            self.emit('announce_spell', 'parent')
        self.cast('source', targets=('own', 'foe'), response=self.mode == 'offturn_response')
        self.board()
        if self.mode == 'countered':
            self.countered()
            return
        if self.mode in ('enemy_bounced', 'own_bounced', 'ferocious_late', 'ferocious_removed'):
            self.emit('announce_spell', 'source')
            target = {'enemy_bounced': 'foe', 'own_bounced': 'own', 'ferocious_late': 'threshold',
                      'ferocious_removed': 'threshold'}[self.mode]
            self.cast('reply', targets=(target,), response=True)
            self.board()
            self.resolve(response=True)
            self.cards['reply']['zone'] = 'graveyard'
            if self.mode == 'ferocious_late':
                self.cards[target]['bonus'] = (3, 3)
            else:
                self.cards[target]['zone'] = 'hand'
        else:
            offturn = self.mode == 'offturn_response'
            self.resolve(response=offturn)
            if offturn:
                self.cards['parent']['zone'] = 'battlefield'
        self.apply_fight()
        self.receipt()
        self.cleanup()


    def countered(self):
        self.emit('announce_spell', 'source')
        self.cast('counter', response=True)
        self.resolve(response=True)
        for ref in ('counter', 'source'):
            self.cards[ref]['zone'] = 'graveyard'
        self.receipt()
        self.cleanup()



@lru_cache(maxsize=1)
def profiles():
    return json.loads(PROFILE.read_bytes())


def classify(record, csvid):
    cost, timing, boost, clauses, extra = MODELS[csvid]
    keywords = {'Fight', 'Ferocious'} if csvid == 14368 else {'Fight'}
    if (record.get('oracle_text') != '\n'.join(clauses) or record.get('mana_cost') != cost
            or record.get('type_line') != timing or record.get('layout') != 'normal'
            or record.get('card_faces') or record.get('selected_face')
            or set(record.get('keywords', ())) != keywords):
        raise ValueError('Exact whole targeted fight spell and printed timing required')
    return dict(csvid=csvid, mana=cost_from_oracle(record), timing=timing, boost=list(boost),
                clauses=list(clauses), modes=[*COMMON, *extra])


def variants(model):
    return [f'{mode}_{owner}' for mode in model['modes'] for owner in (0, 1)]


def validate_witnesses(oracle, rows):
    pins = profiles()
    if (set(pins) != {'sources', 'witnesses'} or set(pins['witnesses']) != WITNESSES
            or {pin['registry']['csvid'] for pin in pins['sources'].values()} != set(MODELS)):
        raise ValueError('Exact targeted fight source and witness scope required')
    for group in ('sources', 'witnesses'):
        for name, pin in pins[group].items():
            record, row = oracle.get(name, {}), rows.get(name, {})
            if (not exact(pin, dict(oracle=record, registry=row)) or record.get('layout') != 'normal'
                    or row.get('kind') != 'card' or row.get('handler') in (None, '0', 'NULL', 'card_dummy')):
                raise ValueError(f'Changed implemented targeted fight source or witness: {name}')


def spec(declaration, record):
    pin = profiles()['sources'].get((record or {}).get('name'))
    if not pin or not exact(record, pin['oracle']):
        raise ValueError('Complete pinned targeted fight primary required')
    model = classify(record, pin['registry']['csvid'])
    picked = declaration.get('variants')
    if (declaration.get('family') != FAMILY or type(declaration.get('version')) is not int
            or declaration['version'] != 2 or declaration.get('oracle_text') != record['oracle_text']
            or not exact(declaration.get('params'), model) or not isinstance(picked, list) or not picked
            or any(type(v) is not str for v in picked) or len(picked) != len(set(picked))
            or set(picked) - set(variants(model))):
        raise ValueError('Exact fight clauses, paid costs, and scoped variants required')
    return dict(model, family=FAMILY)


def expand(name, csvid, declaration, model, selected, rows, oracle):
    validate_witnesses(oracle, rows)
    if rows[name]['csvid'] != csvid or model['csvid'] != csvid or set(selected) - set(variants(model)):
        raise ValueError('Exact targeted fight primary and variants required')
    cases = []
    for variant in selected:
        mode, owner = variant.rsplit('_', 1)
        game = FightCase(oracle[name], csvid, int(owner), mode, rows, oracle)
        game.play()
        metadata = dict(family=FAMILY, version=2, variant=variant, params=declaration['params'],
                        oracle_text=declaration['oracle_text'], covered_clauses=model['clauses'],
                        untested_components=[])
        cases.append(dict(name=f'FightTargetedSpellRecipe_{csvid}_{variant}', card=name,
                          fixture='empty_main', seed=SEED, setup=game.setup, steps=game.steps, _recipe=metadata))
    return cases
