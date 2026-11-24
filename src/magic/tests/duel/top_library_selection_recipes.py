"""Bounded top-library choices and exact hand and graveyard destinations."""

import json
from pathlib import Path

from cast_untap_trigger_recipes import exact
from play_own_exile_effects_recipes import ExileBoard


FAMILY = 'top_library_selection'
PROFILE = Path(__file__).parent / 'data/top_library_selection_wave8_profiles.json'
ONE = 'Look at the top three cards of your library. Put one of them into your hand and the rest into your graveyard.'
TWO = 'Look at the top four cards of your library. Put two of them into your hand and the rest into your graveyard. You lose 2 life.'
ALL = 'Reveal the top three cards of your library. Put all creature cards revealed this way into your hand and the rest into your graveyard.'
ENCHANTMENT = 'Reveal the top five cards of your library. You may put a creature or enchantment card from among them into your hand. Put the rest into your graveyard.'
LAND = 'Reveal the top five cards of your library. You may put a creature or land card from among them into your hand. Put the rest into your graveyard.'
SPELLS = 'Reveal the top five cards of your library. Put up to two instant and/or sorcery cards from among them into your hand and the rest into your graveyard.'
MASTERY = ('Reveal the top five cards of your library. You may put a creature card from among them into your hand. Put the rest into your graveyard.\n'
           'Spell mastery — If there are two or more instant and/or sorcery cards in your graveyard, put up to two creature cards from among the revealed cards into your hand instead of one.')
SPLIT = 'Reveal the top five cards of your library. You may put a creature card and/or an enchantment card from among them into your hand. Put the rest into your graveyard.'
DUAL = 'Nyxborn Shieldmate'
CRYPT = "Tormod's Crypt"
PATTERNS = {
    ONE: ('one', 3, 1, True, (), 0, ('{2}{U}', '{1}{U}'), ('Sorcery',)),
    TWO: ('two', 4, 2, True, (), 2, ('{3}{B}',), ('Sorcery',)),
    ALL: ('all', 3, 3, True, ('Creature',), 0, ('{3}{G}',), ('Sorcery',)),
    ENCHANTMENT: ('typed', 5, 1, False, ('Creature', 'Enchantment'), 0, ('{1}{G}',), ('Sorcery',)),
    LAND: ('typed', 5, 1, False, ('Creature', 'Land'), 0, ('{B}{G}', '{2}{G}'), ('Instant', 'Sorcery')),
    SPELLS: ('spells', 5, 2, False, ('Instant', 'Sorcery'), 0, ('{2}{U}',), ('Sorcery',)),
    MASTERY: ('mastery', 5, 2, False, ('Creature',), 0, ('{1}{G}',), ('Sorcery',)),
    SPLIT: ('split', 5, 2, False, ('Creature', 'Enchantment'), 0, ('{2}{G}',), ('Sorcery',)),
}
LANDS = ('Forest', 'Island', 'Plains', 'Swamp')
SUFFIX = ('Shock', 'Hill Giant', 'Mountain', 'Fervor')
ENEMY_DECK = ('Island', 'Forest', 'Swamp')
BASE = {
    'one': ('Forest', 'Grizzly Bears', 'Darksteel Relic'),
    'two': ('Forest', 'Grizzly Bears', 'Darksteel Relic', 'Divination'),
    'all': ('Grizzly Bears', 'Forest', 'Walking Corpse'),
    'enchantment': ('Forest', 'Grizzly Bears', 'Fervor', 'Naturalize', 'Darksteel Relic'),
    'land': ('Fervor', 'Grizzly Bears', 'Forest', 'Naturalize', 'Darksteel Relic'),
    'spells': ('Forest', 'Naturalize', 'Fervor', 'Divination', 'Darksteel Relic'),
    'mastery': ('Grizzly Bears', 'Naturalize', 'Walking Corpse', 'Divination', 'Forest'),
    'split': ('Forest', 'Grizzly Bears', 'Fervor', 'Naturalize', 'Darksteel Relic'),
}
WITNESSES = set(LANDS + SUFFIX + ENEMY_DECK) | {name for cards in BASE.values() for name in cards} | {'Counterspell', DUAL, CRYPT}
COMMON = ('effect', 'alternate', 'empty', 'short', 'outside', 'duplicates', 'wrong_mana', 'short_mana', 'countered', 'replay')
OPTIONAL = ('decline', 'no_match', 'ineligible')
EXTRA = {'one': (), 'two': ('low_life',), 'all': ('all_match', 'no_match'),
         'typed': OPTIONAL, 'spells': ('one_pick', 'one_available', *OPTIONAL),
         'mastery': ('mastery_both', 'mastery_live', 'mastery_one', 'mastery_decline', 'mastery_foe',
                     'mastery_nonspell', 'mastery_same_spell', 'mastery_loses', 'mastery_small'),
         'split': ('only_creature', 'only_enchantment', 'decline', 'dual_creature', 'dual_enchantment',
                   'dual_once', 'two_dual', 'no_match')}


def profile():
    return json.loads(PROFILE.read_text(encoding='utf8'))


def classify(record):
    from recipes import cost_from_oracle

    text = record['oracle_text']
    keywords = ['Spell mastery'] if text == MASTERY else []
    if (record.get('layout') != 'normal' or record.get('card_faces') or record.get('selected_face')
            or record['keywords'] != keywords or text not in PATTERNS):
        raise ValueError('supported whole-Oracle top library selection required')
    method, window, cap, required, eligible, life_loss, costs, types = PATTERNS[text]
    if record.get('mana_cost') not in costs or record['type_line'] not in types:
        raise ValueError('exact printed cost and spell type required')
    return dict(method=method, window=window, cap=cap, required=required, eligible=list(eligible),
                life_loss=life_loss, cost=cost_from_oracle(record), instant=record['type_line'] == 'Instant')


def variants(params):
    modes = COMMON + EXTRA[params['method']] + (('off_turn',) if params['instant'] else ())
    return [f'{mode}_{owner}' for owner in (0, 1) for mode in modes]


def spec(declaration, record):
    params = classify(record)
    chosen = declaration.get('variants')
    if (declaration.get('family') != FAMILY or type(declaration.get('version')) is not int or declaration['version'] != 2
            or declaration.get('oracle_text') != record['oracle_text'] or not exact(declaration.get('params'), params)
            or not exact(profile()['sources'].get(record['name']), record)):
        raise ValueError('exact pinned whole Oracle and typed top-selection recipe required')
    if (not isinstance(chosen, list) or not chosen or any(not isinstance(v, str) for v in chosen)
            or len(set(chosen)) != len(chosen) or set(chosen) - set(variants(params))):
        raise ValueError('distinct reviewed top-selection variants required')
    return dict(params, family=FAMILY)


def validate_witnesses(oracle, rows):
    pins = profile()
    records = pins['sources'] | pins['witnesses']
    if set(pins['witnesses']) != WITNESSES or set(records) != set(pins['registry']):
        raise ValueError('top-selection source and witness inventory incomplete')
    implemented = pins['sources'].keys() | {'Counterspell', 'Divination', CRYPT}
    for name, expected in records.items():
        row = rows[name]
        if not exact(oracle[name], expected) or not exact(row, pins['registry'][name]) or row['kind'] != 'card':
            raise ValueError(f'Oracle or registry drift for {name}')
        if name in implemented and row['handler'] in ('card_dummy', '0', 'NULL', None):
            raise ValueError(f'expected live production handler for {name}')


class Selection(ExileBoard):
    def __init__(self, name, csvid, declaration, params, variant, rows, oracle):
        self.mode, seat = variant.rsplit('_', 1)
        self.owner, self.enemy = int(seat), 1 - int(seat)
        self.name, self.csvid, self.variant = name, csvid, variant
        self.declaration, self.params, self.rows, self.oracle = declaration, params, rows, oracle
        self.method = params['method']
        self.active = self.enemy if self.mode == 'off_turn' else self.owner
        self.setup, self.steps, self.entities = [], [], {}
        self.zones = {zone: [[], []] for zone in ('hand', 'battlefield', 'graveyard', 'library', 'exile')}
        self.mana, self.life = [[0] * 6, [0] * 6], [20, 20]
        self.loyalty = self.replay = None
        self.add(name, 'source', self.owner, 'hand')
        if self.mode == 'countered':
            self.add('Counterspell', 'reply', self.enemy, 'hand')
        if self.scope == 'land':
            # Both controllers receive the unmodified land-or-creature menu.
            # Four lands in hand meet the legacy AI preference's neutral band.
            for i, land in enumerate(LANDS):
                self.add(land, f'reserved_land_{i}', self.owner, 'hand')
        if self.method == 'mastery':
            self.setup_mastery()
        deck = self.deck()
        self.library_ids = set(deck) | set(ENEMY_DECK) | set(self.zones['graveyard'][0] + self.zones['graveyard'][1])
        for player, names in ((self.owner, deck), (self.enemy, ENEMY_DECK)):
            for i, card in enumerate(names):
                self.add(card, f'deck_{player}_{i}', player, 'library')

    @property
    def scope(self):
        if self.method != 'typed':
            return self.method
        return 'land' if 'Land' in self.params['eligible'] else 'enchantment'

    def eligible(self, name, eligible_types=None):
        types = self.oracle[name]['type_line'].split(' — ', 1)[0].split()
        selected_types = self.params['eligible'] if eligible_types is None else eligible_types
        return not selected_types or bool(set(types) & set(selected_types))

    @property
    def maximum(self):
        if self.method != 'mastery':
            return self.params['cap']
        grave = self.zones['graveyard'][self.owner]
        return 2 if sum(self.eligible(name, ('Instant', 'Sorcery')) for name in grave) >= 2 else 1

    def setup_mastery(self):
        mode = self.mode
        grave = {
            'mastery_both': ('Naturalize', 'Divination'), 'mastery_live': ('Naturalize',),
            'mastery_one': ('Naturalize', 'Divination'), 'mastery_decline': ('Naturalize', 'Divination'),
            'mastery_foe': ('Naturalize', 'Divination'), 'mastery_nonspell': ('Naturalize', 'Forest', 'Fervor'),
            'mastery_same_spell': ('Naturalize', 'Naturalize'), 'mastery_loses': ('Naturalize', 'Divination'),
            'mastery_small': ('Naturalize', 'Divination'),
        }.get(mode, ())
        who = self.enemy if mode == 'mastery_foe' else self.owner
        for i, card in enumerate(grave):
            self.add(card, f'mastery_{i}', who, 'graveyard')
        if mode == 'mastery_live':
            self.add('Divination', 'prelude', self.owner, 'hand')
        if mode == 'mastery_loses':
            self.add(CRYPT, 'crypt', self.enemy)

    def live_grave_prelude(self):
        self.start_cast('prelude')
        self.emit('resolve')
        self.move('prelude', 'graveyard')
        self.draw(self.owner)
        self.draw(self.owner)
        self.receipt()

    def purge_grave_while_pending(self):
        self.emit('announce_spell', ref='source')
        self.emit('choose_player', player=self.enemy, value=self.owner)
        self.emit('activate', ref='crypt', value=1)
        self.emit('assert_stack', value=2)
        self.move('crypt', 'graveyard')
        grave = self.zones['graveyard'][self.owner]
        self.zones['exile'][self.owner].extend(grave)
        grave.clear()

    def deck(self):
        mode, scope = self.mode, self.scope
        prefix = list(BASE[scope])
        suffix = list(SUFFIX)
        if mode == 'empty':
            return []
        if mode == 'short':
            return [next(name for name in prefix if self.eligible(name))]
        if mode in ('outside', 'no_match'):
            if scope in ('one', 'two'):
                # The first excluded slot is useful, and the remaining suffix
                # has three distinct identities to expose an unintended shuffle.
                suffix = ['Walking Corpse', 'Island', 'Naturalize', 'Grizzly Bears']
            else:
                ineligible = [name for name in WITNESSES if not self.eligible(name)]
                prefix = sorted(ineligible)[:self.params['window']]
                assert len(prefix) == self.params['window']
                first_outside = {'all': 'Grizzly Bears', 'enchantment': 'Fervor', 'land': 'Forest',
                                 'spells': 'Shock', 'mastery': 'Grizzly Bears', 'split': 'Fervor'}[scope]
                suffix = [first_outside] + [name for name in suffix if name != first_outside]
        if mode == 'all_match' or (mode == 'alternate' and scope == 'all'):
            prefix = ['Walking Corpse', 'Grizzly Bears', 'Hill Giant']
            if mode == 'alternate':
                prefix[0] = 'Darksteel Relic'
        if mode == 'one_available':
            prefix = ['Forest', 'Naturalize', 'Fervor', 'Hill Giant', 'Darksteel Relic']
        if mode == 'duplicates':
            middle = next(name for name in prefix if self.eligible(name))
            prefix[1:3] = [middle, middle]
        if mode == 'mastery_live':
            return ['Island', 'Plains', *prefix, *suffix]
        if mode == 'mastery_small':
            return ['Grizzly Bears']
        if scope == 'split':
            prefix = {
                'only_enchantment': ['Forest', 'Grizzly Bears', 'Naturalize', 'Darksteel Relic', 'Fervor'],
                'dual_creature': ['Forest', DUAL, 'Fervor', 'Grizzly Bears', 'Darksteel Relic'],
                'dual_enchantment': ['Forest', DUAL, 'Fervor', 'Grizzly Bears', 'Darksteel Relic'],
                'dual_once': ['Forest', DUAL, 'Divination', 'Naturalize', 'Darksteel Relic'],
                'two_dual': ['Forest', DUAL, DUAL, 'Naturalize', 'Darksteel Relic'],
            }.get(mode, prefix)
        return prefix + suffix

    def selections(self, prefix):
        available = [name for name in prefix if self.eligible(name)]
        if self.method == 'all' or not available:
            return available
        if self.mode in ('ineligible', 'decline', 'mastery_decline'):
            return []
        if self.mode == 'alternate':
            available.reverse()
        if self.mode in ('one_pick', 'mastery_one'):
            return available[:1]
        if self.method in ('one', 'two') and self.mode == 'effect' and len(available) > 1:
            available = available[1:] + available[:1]
        return available[:self.maximum]

    def offer(self, prefix):
        self.visibility(prefix)
        if self.method == 'one':
            return
        self.emit('assert_offered_selection_required', value=int(self.params['required']))
        for name in sorted(set(prefix)):
            self.emit('assert_offered_legal_card', index=self.rows[name]['csvid'], value=int(self.eligible(name)))

    def split_offer(self, prefix, types):
        # The count and legal identities remain readable if the production
        # second selector incorrectly truncates its offer after a decline.
        self.emit('assert_offered_count', value=len(prefix))
        self.emit('assert_offered_selection_required', value=0)
        for name in sorted(set(prefix)):
            self.emit('assert_offered_legal_card', index=self.rows[name]['csvid'], value=int(self.eligible(name, types)))

    def split_plan(self, prefix):
        remaining = list(prefix)
        chosen = []
        final_offer = None
        for allowed_type in ('Creature', 'Enchantment'):
            if not remaining:
                break
            types = (allowed_type,)
            available = [name for name in remaining if self.eligible(name, types)]
            if self.mode in ('dual_enchantment', 'alternate'):
                available.reverse()
            if self.mode == 'dual_enchantment' and allowed_type == 'Enchantment' and DUAL in available:
                available = [DUAL, *[name for name in available if name != DUAL]]
            excluded = ((self.mode == 'only_creature' and allowed_type == 'Enchantment')
                        or (self.mode == 'only_enchantment' and allowed_type == 'Creature')
                        or self.mode == 'decline')
            final_offer = (list(remaining), types)
            if not excluded and available:
                selected = available[0]
                self.emit('choose_zone_card', player=self.owner, value=self.rows[selected]['csvid'])
                chosen.append(selected)
                remaining.remove(selected)
            else:
                fallback = available[0] if available else remaining[0]
                self.emit('choose_optional_zone', player=self.owner, value=self.rows[fallback]['csvid'])
        return chosen, final_offer

    def plan(self, prefix, chosen):
        remaining = list(prefix)
        final_offer = None
        if self.scope == 'land':
            for player in (0, 1):
                self.emit('choose_zone', player=player, value=-1)
        elif self.scope == 'enchantment' and self.owner == 1:
            self.emit('choose_zone', player=0, value=-1)
        if self.method == 'all':
            return None
        for name in chosen:
            final_offer = list(remaining)
            self.emit('choose_zone_card', player=self.owner, value=self.rows[name]['csvid'])
            if self.method == 'mastery':
                self.emit('choose_dialog', player=self.owner, value=0)
            remaining.remove(name)
        if not self.params['required'] and len(chosen) < self.maximum and remaining:
            final_offer = list(remaining)
            fallback = next((name for name in remaining if self.eligible(name)), remaining[0])
            if self.mode == 'ineligible':
                rejected = next(name for name in remaining if not self.eligible(name))
                self.emit('choose_zone_card', player=self.owner, value=self.rows[rejected]['csvid'])
            else:
                self.emit('choose_optional_zone', player=self.owner, value=self.rows[fallback]['csvid'])
            if self.scope == 'land' and self.owner == 1:
                self.emit('choose_optional_zone', player=self.owner, value=self.rows[fallback]['csvid'])
        return final_offer

    def receipt(self):
        super().receipt()
        for player in (0, 1):
            self.emit('assert_shuffle_count', player=player, value=0)

    def play(self):
        self.emit('turn', player=self.active, value=0)
        self.emit('observe_search_filters', value=1)
        self.fund(self.owner, [0] * 6)
        if self.mode == 'low_life':
            self.life[self.owner] = 3
            self.emit('set_life', player=self.owner, value=3)
        self.receipt()
        if self.mode == 'mastery_live':
            self.live_grave_prelude()
        fee = self.params['cost']
        budget = list(fee)
        denied = self.mode in ('wrong_mana', 'short_mana')
        if self.mode == 'wrong_mana':
            budget = [sum(fee), 0, 0, 0, 0, 0]
        elif self.mode == 'short_mana':
            budget[next(color for color, amount in enumerate(fee) if amount)] -= 1
        self.start_cast('source', fee=fee, budget=budget, denied=denied)
        if denied:
            self.receipt()
            return
        if self.mode == 'countered':
            self.emit('announce_spell', ref='source')
            self.start_cast('reply', response=True)
            self.emit('resolve_response')
            self.move('reply', 'graveyard')
            self.move('source', 'graveyard')
            self.receipt()
            return
        if self.mode == 'replay':
            self.replay = len(self.steps)
        if self.mode == 'mastery_loses':
            self.purge_grave_while_pending()
        library = self.zones['library'][self.owner]
        prefix = library[:self.params['window']]
        if self.method == 'split':
            chosen, last_offer = self.split_plan(prefix)
        else:
            chosen = self.selections(prefix)
            last_offer = self.plan(prefix, chosen) if prefix else None
        self.emit('resolve')
        self.move('source', 'graveyard')
        del library[:len(prefix)]
        sent_to_graveyard = list(prefix)
        for name in chosen:
            sent_to_graveyard.remove(name)
        self.zones['hand'][self.owner].extend(chosen)
        self.zones['graveyard'][self.owner].extend(sent_to_graveyard)
        self.life[self.owner] -= self.params['life_loss']
        if self.method == 'split' and last_offer is not None:
            self.split_offer(*last_offer)
        elif last_offer is not None:
            self.offer(last_offer)
        else:
            self.emit('assert_offered_count', value=0)
        self.emit('assert_forced_zone_choices', value=0)
        self.receipt()


def expand(name, csvid, declaration, params, selected, rows, oracle):
    from recipes import step

    validate_witnesses(oracle, rows)
    if profile()['registry'][name]['csvid'] != csvid:
        raise ValueError('unreserved top-selection source')
    cases = []
    for variant in selected:
        board = Selection(name, csvid, declaration, params, variant, rows, oracle)
        board.play()
        actions = board.steps
        if board.replay is not None:
            cut = board.replay
            tail = actions[cut:]
            actions = actions[:cut] + [step('capture')] + tail + [step('record_state'), step('restore')] + tail + [step('assert_recorded_state')]
        gaps = ['Simultaneous graveyard moves are checked by exact multiplicity. The unselected library suffix is observed in order.']
        if params['method'] == 'all':
            gaps.append('No public reveal observer is available when the automatic handler emits no display.')
        cases.append(dict(name=f'TopLibrarySelectionRecipe_{csvid}_{variant}', card=name, fixture='empty_main',
                          seed=123, setup=board.setup, steps=actions, _recipe=dict(family=FAMILY, version=2,
                            variant=variant, params=declaration['params'], oracle_text=declaration['oracle_text'],
                            untested_components=gaps)))
    return cases
