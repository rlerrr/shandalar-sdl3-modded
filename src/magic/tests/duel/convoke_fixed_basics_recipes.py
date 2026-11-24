"""Fixed printed Convoke costs, actual payment, and creature combat."""

from functools import lru_cache
import json
from pathlib import Path

from library_search_recipes import wave6_exact as exact
from recipes import card, cost_from_oracle, step


FAMILY = 'convoke_fixed_basics'
PROFILE = Path(__file__).parent / 'data' / f'{FAMILY}_wave8_profiles.json'
CONVOKE = "Convoke (Your creatures can help cast this spell. Each creature you tap while casting this spell pays for {1} or one mana of that creature's color.)"
MODELS = {
    7801: (4, 2, (3, 3), True, 'Creature — Human Soldier'),
    19365: (7, 1, (4, 5), True, 'Creature — Human Knight'),
    19422: (3, 1, (2, 4), False, 'Creature — Human Knight'),
    14200: (6, 0, (4, 4), False, 'Artifact Creature — Golem'),
}
COMMON = ('normal', 'generic_fresh', 'tapped_denied', 'opposing_denied', 'noncreature_denied')
RED = 'Goblin Balloon Brigade'
WHITE = 'Topan Freeblade'
GROUND = 'Elvish Mystic'
NONCREATURE = "Traveler's Amulet"
WITNESSES = {RED, WHITE, GROUND, NONCREATURE, 'Counterspell'}
ZONES = ('hand', 'battlefield', 'library', 'graveyard', 'exile')
TAPPED, FRESH, FOREIGN = 1, 2, 16
ZERO = (0,) * 6
SEED = 2688

EXTRA = {7801: 'foreign_owned_mixed', 19365: 'countered',
         19422: 'last_needed_contributor', 14200: 'pending_replay'}


def modes(csvid):
    normal = ('colored_mixed', 'wrong_color') if MODELS[csvid][1] else ('convoke_only',)
    return (*COMMON, *normal, EXTRA[csvid])


class ConvokeCase:
    def __init__(self, record, csvid, owner, mode, rows, oracle):
        self.record, self.csvid, self.owner, self.other, self.mode = record, csvid, owner, 1 - owner, mode
        self.rows, self.oracle = rows, oracle
        self.setup, self.steps, self.cards = [], [], {}
        self.mana, self.life = [ZERO, ZERO], [20, 20]
        self.generic, self.white, self.stats, self.flight, _ = MODELS[csvid]
        self.add('source', owner, record['name'], 'hand')
        self.contributors = []
        self.paid_helpers = []
        self.snapshot = None
        budget = list(cost_from_oracle(record))
        if mode in ('generic_fresh', 'colored_mixed', 'countered'):
            self.add('red', owner, RED, 'hand')
            self.contributors.append('red')
            self.paid_helpers.append('red')
            budget[0] -= 1
            if mode == 'colored_mixed':
                self.add('white', owner, WHITE, 'hand')
                self.contributors.append('white')
                self.paid_helpers.append('white')
                budget[5] -= 1
        elif mode == 'normal':
            self.add('candidate', owner, RED, state=FRESH)
        elif mode == 'foreign_owned_mixed':
            self.add('red', owner, RED, state=FRESH)
            self.add('white', owner, WHITE, state=FRESH | FOREIGN)
            self.contributors.extend(('red', 'white'))
            budget[0] -= 1
            budget[5] -= 1
        elif mode in ('convoke_only', 'pending_replay', 'last_needed_contributor'):
            for index in range(self.generic):
                ref = f'red_{index}'
                self.add(ref, owner, RED, state=FRESH)
                self.contributors.append(ref)
            budget[0] = 0
            if mode == 'last_needed_contributor':
                self.add('white', owner, WHITE, state=FRESH)
                self.contributors.append('white')
                self.add('spare', owner, RED, state=FRESH)
                budget[5] = 0
        elif mode == 'wrong_color':
            self.add('candidate', owner, RED, state=FRESH)
            budget[5] -= 1
        else:
            if mode == 'tapped_denied':
                self.add('candidate', owner, RED, state=TAPPED)
            elif mode == 'opposing_denied':
                self.add('candidate', self.other, RED)
            elif mode == 'noncreature_denied':
                self.add('candidate', owner, NONCREATURE)
            else:
                raise ValueError(mode)
            budget[0] -= 1
        self.budget = tuple(budget)
        if mode == 'generic_fresh':
            self.add('blocker', self.other, GROUND)
        elif mode == 'countered':
            self.add('counter', self.other, 'Counterspell', 'hand')
        self.success = not (mode.endswith('_denied') or mode == 'wrong_color')

    def add(self, ref, player, name, zone='battlefield', state=0):
        entry = card(name, ref, player, zone)
        if state:
            entry['state'] = [label for mask, label in ((TAPPED, 'tapped'), (FRESH, 'sick'),
                                                       (FOREIGN, 'opponent_owned')) if state & mask]
        self.setup.append(entry)
        record = self.oracle[name]
        stats = tuple(int(record[prop]) for prop in ('power', 'toughness')) if 'power' in record else None
        self.cards[ref] = dict(name=name, player=player, zone=zone, stats=stats,
                               tapped=bool(state & TAPPED), fresh=bool(state & FRESH), damage=0)

    def emit(self, op, ref=None, other=None, prop=None, *, player=None, value=None, zone=None,
             index=None, color=None):
        values = dict(ref=ref, other=other, property=prop, player=player, value=value, zone=zone,
                      index=index, color=color)
        self.steps.append(step(op, **{name: value for name, value in values.items() if value is not None}))

    def card(self, ref, prop, value):
        self.emit('assert_card', ref, prop=prop, value=int(value))

    def fund(self, player, amounts):
        assert not any(self.mana[player])
        self.mana[player] = amounts
        for color, amount in enumerate(amounts):
            self.emit('set_mana', player=player, color=color, value=amount)

    def observe_mana(self):
        for player, amounts in enumerate(self.mana):
            for color, amount in enumerate(amounts):
                self.emit('assert_mana', player=player, color=color, value=amount)

    def observe_board(self):
        for ref, entry in self.cards.items():
            if entry['zone'] != 'battlefield':
                continue
            self.card(ref, 'in_play', 1)
            self.card(ref, 'tapped', entry['tapped'])
            if entry['stats'] is not None:
                self.card(ref, 'sick', entry['fresh'])
                for prop, value in zip(('power', 'toughness'), entry['stats']):
                    self.card(ref, prop, value)
                self.card(ref, 'damage', entry['damage'])

    def receipt(self):
        self.emit('assert_choices_consumed')
        self.emit('assert_stack', value=0)
        self.observe_mana()
        self.observe_board()
        universe = sorted({entry['name'] for entry in self.cards.values()})
        for player in (0, 1):
            self.emit('assert_life', player=player, value=self.life[player])
            for zone in ZONES:
                located = [c['name'] for c in self.cards.values() if c['player'] == player and c['zone'] == zone]
                self.emit('assert_zone', player=player, zone=zone, value=len(located))
                for name in universe:
                    self.emit('assert_zone_multiplicity', player=player, zone=zone,
                              index=self.rows[name]['csvid'], value=located.count(name))

    def drain(self):
        self.emit('resolve')
        self.emit('refresh')
        self.emit('assert_choices_consumed')
        self.emit('assert_stack', value=0)

    def helper(self, ref):
        self.fund(self.owner, cost_from_oracle(self.oracle[self.cards[ref]['name']]))
        self.emit('refresh')
        self.emit('cast', ref, value=1)
        self.emit('assert_choices_consumed')
        self.emit('assert_stack', value=1)
        self.mana[self.owner] = ZERO
        self.observe_mana()
        self.drain()
        self.cards[ref].update(zone='battlefield', fresh=True)
        self.receipt()

    def cast_source(self):
        self.fund(self.owner, self.budget)
        self.emit('refresh')
        if self.mode == 'normal':
            # The full wallet and an eligible helper produce the real
            # Use Convoke / Play normally / Cancel menu for either seat.
            self.emit('choose_dialog', player=self.owner, value=1)
        if self.owner == 0:
            # Human chooses distinct contributors. Seat 1 uses the production
            # automatic branch over exactly the same eligible population.
            for ref in self.contributors:
                self.emit('choose_target', ref, player=self.owner)
        self.emit('cast', 'source', value=int(self.success))
        self.emit('assert_choices_consumed')
        self.emit('assert_stack', value=int(self.success))
        if not self.success:
            self.card('source', 'in_hand', 1)
            self.receipt()
            return
        self.mana[self.owner] = ZERO
        for ref in self.contributors:
            self.cards[ref]['tapped'] = True
        self.card('source', 'in_hand', 0)
        self.card('source', 'in_play', 0)
        self.observe_mana()
        self.observe_board()
        if self.mode == 'countered':
            self.counter_pending()
            return
        if self.mode == 'pending_replay':
            self.snapshot = len(self.steps)
        self.drain()
        self.cards['source'].update(zone='battlefield', fresh=True)
        self.emit('assert_matching_permanents', prop='artifact', player=self.owner,
                  index=self.csvid, value=int(self.csvid == 14200))
        self.receipt()

    def counter_pending(self):
        self.emit('announce_spell', 'source')
        self.fund(self.other, cost_from_oracle(self.oracle['Counterspell']))
        self.emit('refresh')
        self.emit('cast_response', 'counter', value=1)
        self.emit('assert_choices_consumed')
        self.emit('assert_stack', value=2)
        self.mana[self.other] = ZERO
        self.observe_mana()
        self.emit('resolve_response')
        self.emit('refresh')
        self.cards['source']['zone'] = 'graveyard'
        self.cards['counter']['zone'] = 'graveyard'
        self.receipt()

    def combat(self):
        for player in (self.other, self.owner):
            self.emit('advance_turn')
            self.emit('untap_step', player=player)
            self.emit('upkeep_step', player=player)
            self.emit('turn', player=player)
            for entry in self.cards.values():
                if entry['zone'] == 'battlefield' and entry['player'] == player:
                    entry.update(tapped=False, fresh=False)
        self.receipt()
        self.emit('begin_attackers')
        self.emit('attack', 'source', value=1)
        self.emit('finish_attackers')
        self.emit('begin_blockers')
        self.emit('block', 'blocker', 'source', value=int(not self.flight))
        self.emit('advance_combat')
        self.card('source', 'blocked', not self.flight)
        self.emit('combat_damage')
        self.drain()
        self.emit('end_combat')
        self.cards['source']['tapped'] = True
        if self.flight:
            self.life[self.other] -= self.stats[0]
        else:
            self.cards['blocker']['zone'] = 'graveyard'
            self.cards['source']['damage'] = 1
        self.receipt()

    def play(self):
        self.emit('turn', player=self.owner)
        self.emit('present_ai_dialogs', value=1)
        self.fund(0, ZERO)
        self.fund(1, ZERO)
        self.receipt()
        for ref in self.paid_helpers:
            self.helper(ref)
        self.cast_source()
        if self.mode == 'generic_fresh':
            self.combat()



@lru_cache(maxsize=1)
def profiles():
    return json.loads(PROFILE.read_bytes())


def classify(record, csvid):
    generic, white, stats, flight, type_line = MODELS[csvid]
    clauses = [CONVOKE] + (['Flying'] if flight else [])
    if (record.get('oracle_text') != '\n'.join(clauses) or record.get('mana_cost') != f'{{{generic}}}' + '{W}' * white
            or record.get('type_line') != type_line or record.get('layout') != 'normal'
            or record.get('card_faces') or record.get('selected_face')
            or set(record.get('keywords', ())) != ({'Convoke', 'Flying'} if flight else {'Convoke'})
            or tuple(record.get(prop) for prop in ('power', 'toughness')) != tuple(map(str, stats))):
        raise ValueError('Exact whole fixed-cost Convoke primary, printed stats and keyword required')
    return dict(csvid=csvid, cost=cost_from_oracle(record), stats=list(stats), flying=flight,
                clauses=clauses, modes=list(modes(csvid)))


def variants(model):
    return [f'{mode}_{owner}' for mode in model['modes'] for owner in (0, 1)]


def validate_witnesses(oracle, rows):
    pins = profiles()
    if (set(pins) != {'sources', 'witnesses'} or set(pins['witnesses']) != WITNESSES
            or {pin['registry']['csvid'] for pin in pins['sources'].values()} != set(MODELS)):
        raise ValueError('Exact fixed-cost Convoke source and witness scope required')
    for group in ('sources', 'witnesses'):
        for name, pin in pins[group].items():
            record, row = oracle.get(name, {}), rows.get(name, {})
            if (not exact(pin, dict(oracle=record, registry=row)) or record.get('layout') != 'normal'
                    or row.get('kind') != 'card' or row.get('handler') in (None, '0', 'NULL', 'card_dummy')):
                raise ValueError(f'Changed implemented fixed-cost Convoke source or witness: {name}')


def spec(declaration, record):
    pin = profiles()['sources'].get((record or {}).get('name'))
    if not pin or not exact(record, pin['oracle']):
        raise ValueError('Complete pinned fixed-cost Convoke primary required')
    model = classify(record, pin['registry']['csvid'])
    picked = declaration.get('variants')
    if (declaration.get('family') != FAMILY or type(declaration.get('version')) is not int
            or declaration['version'] != 2 or declaration.get('oracle_text') != record['oracle_text']
            or not exact(declaration.get('params'), model) or not isinstance(picked, list) or not picked
            or any(type(v) is not str for v in picked) or len(picked) != len(set(picked))
            or set(picked) - set(variants(model))):
        raise ValueError('Exact printed Convoke clauses, payment, and scoped variants required')
    return dict(model, family=FAMILY)


def expand(name, csvid, declaration, model, selected, rows, oracle):
    validate_witnesses(oracle, rows)
    if rows[name]['csvid'] != csvid or model['csvid'] != csvid or set(selected) - set(variants(model)):
        raise ValueError('Exact fixed-cost Convoke primary and variants required')
    cases = []
    for variant in selected:
        mode, owner = variant.rsplit('_', 1)
        game = ConvokeCase(oracle[name], csvid, int(owner), mode, rows, oracle)
        game.play()
        actions = game.steps
        if game.snapshot is not None:
            cut = game.snapshot
            tail = actions[cut:]
            actions = [*actions[:cut], step('capture'), *tail, step('record_state'),
                       step('restore'), step('ai_roundtrip'), *tail, step('assert_recorded_state')]
        metadata = dict(family=FAMILY, version=2, variant=variant, params=declaration['params'],
                        oracle_text=declaration['oracle_text'], covered_clauses=model['clauses'],
                        untested_components=[])
        cases.append(dict(name=f'ConvokeFixedBasicsRecipe_{csvid}_{variant}', card=name,
                          fixture='empty_main', seed=SEED, setup=game.setup, steps=actions, _recipe=metadata))
    return cases
