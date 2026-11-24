"""Paid named-exile spells over three actual searchable zones."""

from collections import Counter
import json
from pathlib import Path

from cast_untap_trigger_recipes import exact
from play_own_exile_effects_recipes import ExileBoard


FAMILY = 'spell_named_exile'
PROFILE = Path(__file__).parent / 'data/spell_named_exile_wave8_profiles.json'
DEVOID = 'Devoid (This card has no color.)\n'
TARGETS = {
    'nonblack creature': ('creature', 'creature', '{2}{B}{B}', 'Sorcery'),
    'enchantment': ('enchantment', 'enchantment', '{2}{W}{W}', 'Instant'),
    'nonbasic land': ('land', 'land', '{2}{R}{R}', 'Sorcery'),
    'artifact': ('artifact', 'artifact', '{2}{G}{G}', 'Sorcery'),
}
LEGAL = {'creature': 'Grizzly Bears', 'enchantment': 'Fervor', 'land': 'Darksteel Citadel', 'artifact': 'Darksteel Relic'}
ALTERNATE = {'creature': 'Wojek Halberdiers', 'enchantment': 'Nyxborn Shieldmate', 'land': 'Taiga', 'artifact': 'Darksteel Citadel'}
WRONG = {'creature': 'Darksteel Relic', 'enchantment': 'Grizzly Bears', 'land': 'Forest', 'artifact': 'Fervor'}
LIBRARY = ('Island', 'Plains', 'Hill Giant')
WITNESSES = set(LEGAL.values()) | set(ALTERNATE.values()) | set(WRONG.values()) | set(LIBRARY) | {
    'Naturalize', 'Counterspell', 'Ceremonious Rejection', 'Boomerang', 'Walking Corpse', 'Golgari Guildmage',
}
HANDLED = {'Counterspell', 'Ceremonious Rejection', 'Boomerang'}
ZONES = ('hand', 'graveyard', 'library')
COMMON = ('whole', 'single', 'only_hand', 'only_graveyard', 'only_library', 'no_matches', 'empty_search', 'battlefield_copy',
          'own_controller', 'cross_owned', 'alternate_type', 'wrong_target', 'countered', 'target_leaves', 'short_mana', 'wrong_mana', 'replay')


def profile():
    return json.loads(PROFILE.read_text(encoding='utf8'))


def classify(record):
    from recipes import COLORS, cost_from_oracle

    if record.get('layout') != 'normal' or record.get('card_faces') or record.get('selected_face'):
        raise ValueError('normal whole Oracle record required')
    for qualifier, (scope, noun, fee, kind) in TARGETS.items():
        for any_number in (False, True) if scope == 'land' else (False,):
            amount = 'any number of' if any_number else 'all'
            clause = (f"Exile target {qualifier}. Search its controller's graveyard, hand, and library for {amount} cards "
                      f"with the same name as that {noun} and exile them. Then that player shuffles.")
            text = DEVOID + clause if any_number else clause
            if record['oracle_text'] == text:
                cost = '{3}{R}' if any_number else fee
                colors = [] if any_number else [next(key for key in COLORS if f'{{{key}}}' in cost)]
                if (record['mana_cost'] != cost or record['type_line'] != kind or record['colors'] != colors
                        or record['keywords'] != (['Devoid'] if any_number else [])):
                    raise ValueError('complete printed cost, color and type required')
                return dict(scope=scope, any_number=any_number, devoid=any_number, instant=kind == 'Instant',
                            cost=cost_from_oracle(record))
    raise ValueError('supported whole-Oracle targeted same-name exile required')


def variants(params):
    scope = params['scope']
    modes = COMMON
    if scope == 'creature':
        modes += ('black_target', 'black_multicolor')
    if params['instant']:
        modes += ('off_turn',)
    if scope == 'land':
        modes += ('color_gate',)
    if params['any_number']:
        modes += ('partial', 'choose_none')
    return [f'{mode}_{owner}' for owner in (0, 1) for mode in modes]


def spec(declaration, record):
    params = classify(record)
    chosen = declaration.get('variants')
    if (declaration.get('family') != FAMILY or type(declaration.get('version')) is not int or declaration['version'] != 2
            or declaration.get('oracle_text') != record['oracle_text'] or not exact(declaration.get('params'), params)
            or not exact(profile()['sources'].get(record['name']), record)):
        raise ValueError('exact whole Oracle and typed named-exile fixture required')
    if (not isinstance(chosen, list) or not chosen or any(not isinstance(v, str) for v in chosen)
            or len(set(chosen)) != len(chosen) or set(chosen) - set(variants(params))):
        raise ValueError('distinct reviewed named-exile variants required')
    return dict(params, family=FAMILY)


def validate_witnesses(oracle, rows):
    pins = profile()
    records = pins['sources'] | pins['witnesses']
    if set(pins['witnesses']) != WITNESSES or set(records) != set(pins['registry']):
        raise ValueError('complete named-exile source and witness inventory required')
    for name, expected in records.items():
        row = rows[name]
        if not exact(oracle[name], expected) or not exact(row, pins['registry'][name]) or row['kind'] != 'card':
            raise ValueError(f'Oracle or registry drift for {name}')
        if name in pins['sources'].keys() | HANDLED and row['handler'] in ('card_dummy', '0', 'NULL', None):
            raise ValueError(f'expected live production handler for {name}')
    for name, text in {'Counterspell': 'Counter target spell.', 'Ceremonious Rejection': 'Counter target colorless spell.',
                       'Boomerang': "Return target permanent to its owner's hand."}.items():
        if oracle[name]['oracle_text'] != text:
            raise ValueError(f'exact nonpermanent witness clause required for {name}')


class NamedExile(ExileBoard):
    def __init__(self, name, csvid, declaration, params, variant, rows, oracle):
        self.mode, seat = variant.rsplit('_', 1)
        self.owner, self.enemy = int(seat), 1 - int(seat)
        self.name, self.csvid, self.variant = name, csvid, variant
        self.declaration, self.params, self.rows, self.oracle = declaration, params, rows, oracle
        self.scope = params['scope']
        self.active = self.enemy if self.mode == 'off_turn' else self.owner
        self.controller = self.owner if self.mode == 'own_controller' else self.enemy
        self.original_owner = self.owner if self.mode == 'cross_owned' else self.controller
        self.replay = self.loyalty = None
        self.mana, self.life = [[0] * 6, [0] * 6], [20, 20]
        self.shuffle = [0, 0]
        self.setup, self.steps, self.entities = [], [], {}
        self.zones = {zone: [[], []] for zone in ('hand', 'battlefield', 'graveyard', 'library', 'exile')}
        self.library_ids = set()
        self.target = LEGAL[self.scope]
        mode = self.mode
        if mode in ('wrong_target', 'black_target', 'black_multicolor', 'alternate_type'):
            self.target = {'wrong_target': WRONG[self.scope], 'black_target': 'Walking Corpse',
                           'black_multicolor': 'Golgari Guildmage', 'alternate_type': ALTERNATE[self.scope]}[mode]
        self.add(name, 'source', self.owner, 'hand')
        self.add(self.target, 'target', self.controller)
        if self.original_owner != self.controller:
            self.setup[-1]['state'].append('opponent_owned')
        if mode == 'battlefield_copy':
            self.add(self.target, 'other_same', self.controller)
        for player in (0, 1):
            self.seed_zones(player)
        self.reply = {'countered': 'Counterspell', 'target_leaves': 'Boomerang', 'color_gate': 'Ceremonious Rejection'}.get(mode)
        if self.reply:
            self.add(self.reply, 'reply', self.enemy, 'hand')
        self.library_ids.update(name for zone in ZONES for cards in self.zones[zone] for name in cards)

    def seed_zones(self, player):
        mode = self.mode
        main = player == self.controller or mode == 'own_controller'
        count = 2 if main and mode != 'single' else 1
        for zone in ZONES:
            if main and mode == 'empty_search':
                continue
            fillers = {'hand': ('Naturalize',), 'graveyard': ('Forest',), 'library': LIBRARY}[zone]
            matches = count
            if main and mode == 'no_matches':
                matches = 0
            if main and mode.startswith('only_') and mode != 'only_' + zone:
                matches = 0
            for i, name in enumerate([*fillers[:1], *([self.target] * matches), *fillers[1:]]):
                self.add(name, f'{zone}_{player}_{i}', player, zone)

    def receipt(self):
        cut = len(self.steps)
        super().receipt()
        self.steps[cut:] = [step for step in self.steps[cut:] if not (
            step['op'] == 'assert_zone_card' and step['zone'] == 'library' and self.shuffle[step['player']])]
        for who, library in enumerate(self.zones['library']):
            counts = Counter(library)
            for name in sorted(self.library_ids):
                self.emit('assert_zone_multiplicity', player=who, zone='library', index=self.rows[name]['csvid'], value=counts[name])
            self.emit('assert_shuffle_count', player=who, value=self.shuffle[who])

    def move_target(self, destination):
        self.move('target', destination)
        if self.original_owner != self.controller:
            self.zones[destination][self.controller].remove(self.target)
            self.zones[destination][self.original_owner].append(self.target)

    def search_input(self):
        # Headless selects through the actual named tutor. A nonempty hand or
        # library displays even without a match. Graveyard does so if matched.
        last = None
        identity = self.rows[self.target]['csvid']
        for zone in ZONES:
            cards = list(self.zones[zone][self.controller])
            if zone == 'hand' and self.controller == self.owner:
                cards.remove(self.name)
            matches = cards.count(self.target)
            if not cards or (zone == 'graveyard' and not matches):
                continue
            chosen = {'partial': min(1, matches), 'choose_none': 0}.get(self.mode, matches)
            required = zone == 'graveyard' and not self.params['any_number']
            for _ in range(chosen):
                last = (list(cards), required)
                self.emit('choose_zone_card', player=self.owner, value=identity)
                cards.remove(self.target)
            if not matches or chosen < matches:
                last = (list(cards), required)
                fallback = next((name for name in cards if name == self.target), cards[0])
                self.emit('choose_optional_zone', player=self.owner, value=self.rows[fallback]['csvid'])
        return last

    def resolve_search(self):
        if self.mode == 'replay':
            self.replay = len(self.steps)
        last = self.search_input()
        self.emit('resolve')
        self.move('source', 'graveyard')
        self.move_target('exile')
        for zone in ZONES:
            cards = self.zones[zone][self.controller]
            matches = cards.count(self.target)
            chosen = {'partial': min(1, matches), 'choose_none': 0}.get(self.mode, matches)
            for _ in range(chosen):
                cards.remove(self.target)
                self.zones['exile'][self.controller].append(self.target)
        self.shuffle[self.controller] = 1
        if last:
            cards, required = last
            self.emit('assert_offered_count', value=len(cards))
            self.emit('assert_offered_selection_required', value=int(required))
            for name in sorted(set(cards)):
                self.emit('assert_offered_legal_card', index=self.rows[name]['csvid'], value=int(name == self.target))
        else:
            self.emit('assert_offered_count', value=0)
        self.emit('assert_unused_choices', value=0)
        self.emit('assert_forced_zone_choices', value=0)
        self.receipt()

    def response(self):
        from recipes import cost_from_oracle

        self.emit('announce_spell', ref='source')
        if self.mode == 'color_gate' and not self.params['devoid']:
            self.fund(self.enemy, cost_from_oracle(self.oracle[self.reply]))
            self.emit('cast_response', ref='reply', value=0)
            self.observe('reply', 'in_hand', 1)
            self.emit('assert_stack', value=1)
            self.pools()
            self.resolve_search()
            return
        self.start_cast('reply', target='target' if self.mode == 'target_leaves' else None, response=True)
        self.emit('resolve_response')
        self.move('reply', 'graveyard')
        self.move('source', 'graveyard')
        if self.mode == 'target_leaves':
            self.move_target('hand')
        self.emit('assert_offered_count', value=0)
        self.receipt()

    def play(self):
        self.emit('turn', player=self.active, value=0)
        self.emit('observe_search_filters', value=1)
        self.fund(self.owner, [0] * 6)
        self.receipt()
        denied = self.mode in ('wrong_target', 'black_target', 'black_multicolor', 'short_mana', 'wrong_mana')
        fee, budget = self.params['cost'], list(self.params['cost'])
        if self.mode == 'wrong_mana':
            budget = [sum(fee), 0, 0, 0, 0, 0]
        elif self.mode == 'short_mana':
            budget[0] -= 1
        self.start_cast('source', fee=fee, budget=budget, target=None if denied else 'target', denied=denied)
        if denied:
            self.receipt()
        elif self.reply:
            self.response()
        else:
            self.resolve_search()


def expand(name, csvid, declaration, params, selected, rows, oracle):
    from recipes import step

    validate_witnesses(oracle, rows)
    if profile()['registry'][name]['csvid'] != csvid:
        raise ValueError('unreserved same-name exile source')
    cases = []
    for variant in selected:
        board = NamedExile(name, csvid, declaration, params, variant, rows, oracle)
        board.play()
        actions = board.steps
        if board.replay is not None:
            cut = board.replay
            tail = actions[cut:]
            actions = actions[:cut] + [step('capture')] + tail + [step('record_state'), step('restore')] + tail + [step('assert_recorded_state')]
        gaps = ['Shuffled library contents are asserted by exact multiplicity and real shuffle count. Graveyard search prevention and separately issued cards sharing one name are outside these fixtures.']
        if not params['any_number']:
            gaps.append('All-copy positives find every same-name card. Declining eligible hidden-zone matches is held because the legacy mandatory helper retries the same prompt.')
        cases.append(dict(name=f'SpellNamedExileRecipe_{csvid}_{variant}', card=name, fixture='empty_main',
                          seed=123, setup=board.setup, steps=actions, _recipe=dict(family=FAMILY, version=2,
                            variant=variant, params=declaration['params'], oracle_text=declaration['oracle_text'],
                            untested_components=gaps)))
    return cases
