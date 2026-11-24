"""Two printed Retreat modes exercised through real land entry."""

from collections import Counter
from copy import deepcopy
from functools import lru_cache
import json
from pathlib import Path

from counted_battlefield_damage_recipes import exact_pin
from landfall_case_support import LandfallBoard, landfall_lines


FAMILY = 'landfall_modal_retreats'
PROFILE = Path(__file__).with_name('data') / 'landfall_modal_retreats_wave8_profiles.json'
TRIGGER = 'Landfall — Whenever a land you control enters, choose one —'
MODES = {
    'coralhelm': ('You may tap or untap target creature.',
                  'Scry 1. (Look at the top card of your library. You may put that card on the bottom.)'),
    'emeria': ('Create a 1/1 white Kor Ally creature token.',
               'Creatures you control get +1/+1 until end of turn.'),
    'hagra': ('Target creature gets +1/+0 and gains deathtouch until end of turn.',
              'Each opponent loses 1 life and you gain 1 life.'),
    'kazandu': ('Put a +1/+1 counter on target creature.', 'You gain 2 life.'),
    'valakut': ('Target creature gets +2/+0 until end of turn.', "Target creature can't block this turn."),
}
COMMON = ('opponent_land', 'before_source', 'second_denied', 'replay')
EXTRAS = {
    'coralhelm': ('tap_foe', 'untap_own', 'pass', 'scry_bottom', 'scry_keep', 'scry_empty'),
    'emeria': ('token', 'team', 'token_then_team'),
    'hagra': ('deathtouch', 'expiry', 'enemy_pump', 'drain'),
    'kazandu': ('own_counter', 'foe_counter', 'counter_then_life', 'life'),
    'valakut': ('own_pump', 'foe_pump', 'no_block', 'block_expiry', 'other_blocks', 'no_targets'),
}
DEFAULT = {
    'coralhelm': 'scry_bottom', 'emeria': 'token', 'hagra': 'drain',
    'kazandu': 'own_counter', 'valakut': 'own_pump',
}
TOKEN = 'Kor Ally'
TOKEN_ID = 18778
BEAR = 'Grizzly Bears'
SPIDER = 'Giant Spider'
GIANT = 'Hill Giant'
PASSIVE = {TOKEN, BEAR, SPIDER, GIANT}
WITNESSES = PASSIVE | {'Forest', 'Mountain', 'Island', 'Plains'}
GAPS = {
    'coralhelm': ('Mode and target selection are processed in the same land-entry callback as the effect; '
                  'the driver has no independent response boundary between them.',),
    'emeria': ('The generated token is bound by exact Kor Ally identity and checked as an Ally; '
               'the separate Kor subtype predicate is unavailable.',),
    'hagra': ('Mode and target selection are processed in the same land-entry callback as the effect; '
              'the driver has no independent response boundary between them.',),
    'kazandu': ('Mode and target selection are processed in the same land-entry callback as the effect; '
                'the driver has no independent response boundary between them.',),
    'valakut': ('Mode and target selection are processed in the same land-entry callback as the effect; '
                'the driver has no independent response boundary between them.',),
}


@lru_cache(maxsize=1)
def pins():
    return json.loads(PROFILE.read_bytes())


def classify(record):
    from recipes import cost_from_oracle

    lines = landfall_lines(record)
    if (record.get('type_line') != 'Enchantment' or len(lines) != 3 or lines[0] != TRIGGER or not all(line.startswith('• ') for line in lines[1:])):
        raise ValueError('exact enchantment with two whole printed Retreat modes required')
    clauses = tuple(line.removeprefix('• ') for line in lines[1:])
    role = next((role for role, options in MODES.items() if options == clauses), None)
    keywords = ['Landfall', 'Scry'] if role == 'coralhelm' else ['Landfall']
    if role is None or record.get('keywords') != keywords:
        raise ValueError('unsupported Retreat mode pair')
    return dict(role=role, mana=cost_from_oracle(record))


def variants(shape):
    return [f'{mode}_{seat}' for mode in COMMON + EXTRAS[shape['role']] for seat in (0, 1)]


def spec(declaration, record):
    shape = classify(record)
    source = pins()['sources'].get(record['name'])
    if (not source or not exact_pin(source['oracle'], record) or declaration.get('family') != FAMILY
            or type(declaration.get('version')) is not int or declaration['version'] != 2
            or declaration.get('oracle_text') != record['oracle_text']
            or not exact_pin(declaration.get('params'), shape)):
        raise ValueError('complete pinned Retreat and exact payment parameters required')
    selected = declaration.get('variants')
    if (not isinstance(selected, list) or not selected or any(not isinstance(v, str) for v in selected)
            or len(set(selected)) != len(selected) or set(selected) - set(variants(shape))):
        raise ValueError('distinct supported Retreat variants required')
    return dict(shape, family=FAMILY)


def validate_witnesses(oracle, rows):
    data = pins()
    if set(data) != {'sources', 'witnesses'} or set(data['witnesses']) != WITNESSES:
        raise ValueError('complete independent Retreat witnesses required')
    for group, entries in data.items():
        for name, pin in entries.items():
            row = rows.get(name, {})
            expected_layout = 'token' if name == TOKEN else 'normal'
            allowed_kinds = {'card', 'token'} if name == TOKEN else {'card'}
            if (not exact_pin(oracle.get(name), pin['oracle']) or not exact_pin(row, pin['registry'])
                    or row.get('kind') not in allowed_kinds or row.get('handler') in (None, 'NULL', '0')
                    or row.get('handler') == 'card_dummy' and name not in PASSIVE
                    or oracle[name]['layout'] != expected_layout):
                raise ValueError('implemented Retreat source or witness changed: ' + name)
            if group == 'sources':
                classify(oracle[name])
    if rows[TOKEN]['csvid'] != TOKEN_ID or oracle[TOKEN]['type_line'] != 'Token Creature — Kor Ally':
        raise ValueError('exact Kor Ally token identity required')


class RetreatBoard(LandfallBoard):
    def build(self):
        self.role = self.shape['role']
        self.name = self.cards['source']['name']
        self.life = [20, 20]
        self.library = {
            self.owner: [] if self.mode == 'scry_empty' else [BEAR, 'Island', 'Plains'],
            self.foe: ['Island', 'Plains'],
        }
        for player, names in self.library.items():
            for position, name in enumerate(names):
                self.add(name, f'deck_{player}_{position}', player, 'library')
        actor = self.foe if self.mode == 'opponent_land' else self.owner
        self.add('Forest', 'land', actor, 'hand')
        if self.mode in ('second_denied', 'token_then_team', 'counter_then_life'):
            self.add('Mountain', 'land2', self.owner, 'hand')
        if self.mode != 'no_targets':
            self.add(BEAR, 'ally', self.owner, tapped=self.mode in ('untap_own', 'pass'))
            self.add(GIANT, 'other_ally', self.owner)
            self.add(SPIDER, 'foe_creature', self.foe)
        if self.mode == 'other_blocks':
            self.add(BEAR, 'other_foe', self.foe)
        self.counters = Counter()
        self.temporary = {}
        self.tokens = []

    def observe(self, ref, prop, value):
        self.emit('assert_card', ref=ref, property=prop, value=value)

    def stats(self, ref):
        card = self.cards[ref]
        record = self.oracle[card['name']]
        power, toughness = self.temporary.get(ref, (0, 0))
        count = self.counters[ref]
        return int(record['power']) + power + count, int(record['toughness']) + toughness + count

    def mod(self, ref, power, toughness):
        before_power, before_toughness = self.temporary.get(ref, (0, 0))
        self.temporary[ref] = before_power + power, before_toughness + toughness

    def receipts(self):
        self.emit('refresh')
        self.observe_mana()
        self.emit('assert_stack', value=0)
        for ref, card in self.cards.items():
            if card['zone'] != 'battlefield':
                continue
            self.observe(ref, 'in_play', 1)
            self.observe(ref, 'tapped', int(card['tapped']))
            if 'Creature' in self.oracle[card['name']]['type_line']:
                power, toughness = self.stats(ref)
                for prop, value in (('power', power), ('toughness', toughness),
                                    ('damage', card['damage']), ('plus1_counters', self.counters[ref])):
                    self.observe(ref, prop, value)
        for player in (0, 1):
            self.emit('assert_life', player=player, value=self.life[player])
            for zone in ('hand', 'battlefield', 'graveyard', 'exile', 'library'):
                names = self.library[player] if zone == 'library' else [
                    card['name'] for card in self.cards.values() if card['player'] == player and card['zone'] == zone
                ]
                self.emit('assert_zone', player=player, zone=zone, value=len(names))
                if zone == 'library':
                    for position, name in enumerate(names):
                        self.emit('assert_zone_card', player=player, zone=zone, index=position,
                                  value=self.rows[name]['csvid'])
                else:
                    counts = Counter(names)
                    for name in sorted(set(names) | {self.name} | ({TOKEN} if self.role == 'emeria' else set())):
                        self.emit('assert_zone_multiplicity', player=player, zone=zone,
                                  index=self.rows[name]['csvid'], value=counts[name])

    def turn_cycle(self):
        for _ in range(2):
            self.advance_turn()
        self.temporary.clear()

    def create_kor(self):
        index = len(self.tokens)
        ref = f'kor_{index}'
        self.tokens.append(ref)
        self.cards[ref] = dict(name=TOKEN, player=self.owner, zone='battlefield', tapped=False, damage=0)
        self.emit('assert_zone_multiplicity', player=self.owner, zone='battlefield', index=TOKEN_ID, value=index + 1)
        self.emit('bind_permanent_copy', ref=ref, player=self.owner, index=index, value=TOKEN_ID)
        for prop, value in (('token', 1), ('subtype_Ally', 1), ('color', 32), ('artifact', 0)):
            self.emit('assert_matching_permanents', player=self.owner, index=TOKEN_ID, property=prop, value=value)

    def choice_plan(self, effect):
        player = self.owner
        second = effect in ('scry_bottom', 'scry_keep', 'scry_empty', 'team', 'drain', 'life',
                            'no_block', 'block_expiry', 'other_blocks')
        self.emit('choose_dialog', player=player, value=int(second))
        if effect in ('tap_foe', 'foe_counter', 'foe_pump', 'enemy_pump', 'no_block', 'block_expiry'):
            self.emit('choose_target', player=player, ref='foe_creature')
        elif effect in ('untap_own', 'pass', 'own_counter', 'own_pump', 'deathtouch', 'expiry'):
            self.emit('choose_target', player=player, ref='ally')
        elif effect == 'other_blocks':
            self.emit('choose_target', player=player, ref='other_foe')
        if effect in ('tap_foe', 'untap_own', 'pass'):
            # The ordinary two-action Twiddle menu includes the optional Cancel row.
            self.emit('choose_dialog', player=player, value={'tap_foe': 0, 'untap_own': 1, 'pass': 2}[effect])
        if effect in ('scry_bottom', 'scry_keep'):
            self.emit('choose_zone', player=player, value=0 if effect == 'scry_bottom' else -1)

    def enter(self, effect, ref='land'):
        is_scry = effect in ('scry_bottom', 'scry_keep', 'scry_empty')
        if is_scry:
            self.emit('record_library', player=self.owner)
        chooser = None if effect in (None, 'no_targets') else lambda _: self.choice_plan(effect)
        self.play_land(ref, choices=chooser)
        if is_scry:
            if effect == 'scry_bottom':
                cards = self.library[self.owner]
                cards.append(cards.pop(0))
            self.emit('assert_scry_result', player=self.owner, value=1)
        elif effect == 'tap_foe':
            self.cards['foe_creature']['tapped'] = True
        elif effect == 'untap_own':
            self.cards['ally']['tapped'] = False
        elif effect == 'token':
            self.create_kor()
        elif effect == 'team':
            for key in ('ally', 'other_ally', *self.tokens):
                self.mod(key, 1, 1)
        elif effect in ('deathtouch', 'expiry', 'enemy_pump'):
            self.mod('foe_creature' if effect == 'enemy_pump' else 'ally', 1, 0)
        elif effect == 'drain':
            self.life[self.owner] += 1
            self.life[self.foe] -= 1
        elif effect in ('own_counter', 'foe_counter'):
            self.counters['ally' if effect == 'own_counter' else 'foe_creature'] += 1
        elif effect == 'life':
            self.life[self.owner] += 2
        elif effect in ('own_pump', 'foe_pump'):
            self.mod('ally' if effect == 'own_pump' else 'foe_creature', 2, 0)
        self.receipts()

    def combat(self, *, block_legal, deathtouch=False):
        self.emit('begin_attackers')
        self.emit('attack', ref='ally', value=1)
        self.cards['ally']['tapped'] = True
        self.emit('finish_attackers')
        self.emit('begin_blockers')
        self.emit('block', ref='foe_creature', other='ally', value=int(block_legal))
        self.emit('advance_combat')
        self.observe('ally', 'blocked', int(block_legal))
        self.emit('combat_damage')
        self.emit('resolve')
        self.emit('end_combat')
        self.emit('assert_choices_consumed')
        power, _ = self.stats('ally')
        if block_legal:
            self.cards['ally']['zone'] = 'graveyard'
            if deathtouch:
                self.cards['foe_creature']['zone'] = 'graveyard'
            else:
                self.cards['foe_creature']['damage'] = power
        else:
            self.life[self.foe] -= power
        self.receipts()

    def play(self):
        mode = self.mode
        if mode == 'before_source':
            self.emit('turn', player=self.owner)
            for player in (0, 1):
                self.fund(player, [0] * 6)
            self.enter(None)
        self.play_source()
        self.receipts()
        if mode == 'before_source':
            return None
        if mode == 'opponent_land':
            self.advance_turn()
            self.enter(None)
            return None
        if mode in ('deathtouch', 'expiry', 'no_block', 'block_expiry', 'other_blocks'):
            self.turn_cycle()
        cut = len(self.steps) if mode == 'replay' else None
        effect = DEFAULT[self.role] if mode in ('replay', 'second_denied') else {
            'token_then_team': 'token', 'counter_then_life': 'own_counter',
        }.get(mode, mode)
        self.enter(effect)
        if mode == 'second_denied':
            self.play_land('land2', allowed=False)
            self.receipts()
        elif mode in ('token_then_team', 'counter_then_life'):
            self.turn_cycle()
            self.enter('team' if mode == 'token_then_team' else 'life', 'land2')
        if effect in ('team', 'enemy_pump', 'own_pump', 'foe_pump') or mode in (
                'token_then_team', 'own_counter', 'foe_counter', 'counter_then_life', 'expiry', 'block_expiry'):
            self.turn_cycle()
            self.receipts()
        if mode in ('deathtouch', 'expiry', 'no_block', 'block_expiry', 'other_blocks'):
            self.combat(block_legal=mode != 'no_block', deathtouch=mode == 'deathtouch')
        return cut


def expand(name, csvid, declaration, shape, selected, rows, oracle):
    from recipes import step

    validate_witnesses(oracle, rows)
    if pins()['sources'][name]['registry']['csvid'] != csvid:
        raise ValueError('exact Retreat source identity required')
    cases = []
    for variant in selected:
        mode, seat = variant.rsplit('_', 1)
        board = RetreatBoard(name, shape, mode, int(seat), rows, oracle)
        cut = board.play()
        actions = board.steps
        if cut is not None:
            suffix = actions[cut:]
            actions = (actions[:cut] + [step('capture')] + suffix
                       + [step('record_state'), step('ai_roundtrip'), step('restore')]
                       + deepcopy(suffix) + [step('assert_recorded_state')])
        cases.append(dict(
            name=f'LandfallModalRetreatsRecipe_{csvid}_{variant}', card=name, fixture='empty_main', seed=2298,
            setup=board.setup, steps=actions, _recipe=dict(
                family=FAMILY, version=2, variant=variant, params=declaration['params'],
                oracle_text=declaration['oracle_text'], untested_components=list(GAPS[shape['role']]),
            ),
        ))
    return cases
