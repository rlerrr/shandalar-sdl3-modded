"""Paid mill spells with exact printed costs, ordered zones, and coupled rewards."""

from functools import lru_cache
import json
from pathlib import Path

from library_search_recipes import wave6_exact as exact
from recipes import card, cost_from_oracle, step


FAMILY = 'mill_library_spells'
PROFILE = Path(__file__).parent / 'data' / f'{FAMILY}_wave8_profiles.json'
TRAP, DAMPEN, CONFESSION, DREADWATERS = 10494, 7190, 12910, 12354
CONFUSION, TRAUMATIZE, FASCINATION, SUMMONS = 12217, 5740, 14546, 15511
MODELS = {
    CONFUSION: ('{X}{U}', 'Sorcery',
                ('Target player mills X cards. If this spell was cast from a graveyard, that player mills twice that many cards instead.',
                 'Flashback {X}{U} (You may cast this card from your graveyard for its flashback cost. Then exile it.)'),
                ('normal_x2', 'zero_x', 'flashback_x2', 'flashback_short', 'flashback_offturn'), ()),
    TRAUMATIZE: ('{3}{U}{U}', 'Sorcery', ('Target player mills half their library, rounded down.',),
                 ('even', 'odd', 'one_card', 'live_draw'), ('self_target',)),
    FASCINATION: ('{X}{U}{U}', 'Sorcery', ('Choose one —', '• Each player draws X cards.', '• Each player mills X cards.'),
                  ('draw_x2', 'mill_x3_short', 'zero_draw', 'zero_mill'), ()),
    SUMMONS: ('{X}{B}{B}', 'Sorcery',
              ('Each player mills X cards. For each creature card put into a graveyard this way, you create a tapped '
               '2/2 black Zombie creature token. (To mill a card, a player puts the top card of their library into their graveyard.)',),
              ('mixed_x4', 'short_x3', 'no_creatures', 'zero_x'), ()),
    TRAP: ('{3}{U}{U}', 'Instant — Trap',
           ("If an opponent searched their library this turn, you may pay {0} rather than pay this spell's mana cost.",
            'Target opponent mills thirteen cards.'),
           ('normal_paid', 'short_paid', 'alternate', 'no_search', 'own_search_only', 'prior_turn'), ('paid_after_search',)),
    DAMPEN: ('{1}{U}', 'Instant — Arcane',
             ('Target player mills four cards.',
              "Splice onto Arcane {1}{U} (As you cast an Arcane spell, you may reveal this card from your hand and pay its splice cost. If you do, add this card's effects to that spell.)"),
             ('ordinary', 'short_library', 'offturn', 'countered'), ('self_target', 'splice', 'decline_splice', 'short_splice')),
    CONFESSION: ('{4}{U/B}', 'Sorcery',
                 ('Target player mills four cards. You draw a card for each creature card put into their graveyard this way.',),
                 ('mixed_blue', 'mixed_black', 'short_black', 'zero_creatures'), ('self_target',)),
    DREADWATERS: ('{3}{U}', 'Sorcery',
                  ('Target player mills X cards, where X is the number of lands you control.',),
                  ('own_lands', 'foe_lands_only', 'land_removed', 'short_library'), ('self_target',)),
}
SEARCH = {'alternate', 'own_search_only', 'prior_turn', 'paid_after_search'}
DENIED = {'no_search', 'own_search_only', 'prior_turn'}
SPLICE = {'splice', 'decline_splice', 'short_splice'}
AMULET = "Traveler's Amulet"
CARRIER = 'Reach Through Mists'
BODY = 'Goblin Balloon Brigade'
SECOND_BODY = 'Prodigal Sorcerer'
THIRD_BODY = 'Goblin Tinkerer'
AMULET_KEEP = 'Lightning Bolt'
CITADEL = 'Darksteel Citadel'
RELIC = 'Darksteel Relic'
BOUNCE = 'Boomerang'
COUNTER = 'Counterspell'
CASTER_DECK = ('Forest', 'Plains', BODY, 'Mountain', SECOND_BODY, 'Island', 'Swamp', CARRIER, RELIC)
FOE_DECK = (BODY, 'Island', RELIC, SECOND_BODY, 'Mountain', THIRD_BODY, 'Plains', AMULET_KEEP, 'Swamp',
            COUNTER, 'Forest', CARRIER, BODY, CITADEL, 'Island', THIRD_BODY)
ZOMBIE = 'Zombie'
FOLLOWERS = {CONFUSION, TRAUMATIZE, FASCINATION, SUMMONS}
WITNESSES = set(CASTER_DECK) | set(FOE_DECK) | {AMULET, BOUNCE, ZOMBIE}
ZONES = {'hand': 0, 'battlefield': 1, 'library': 2, 'graveyard': 3, 'exile': 4}
ZERO = (0,) * 6
UNTAPPED, TAPPED = 8, 1
SEED = 2771


def has_type(record, kind):
    return kind in record['type_line'].split(' — ', 1)[0].lower().split()


class MillCase:
    def __init__(self, record, csvid, owner, mode, rows, oracle):
        self.record, self.csvid, self.owner, self.foe, self.mode = record, csvid, owner, 1-owner, mode
        self.rows, self.oracle = rows, oracle
        self.setup, self.steps, self.refs = [], [], {}
        self.zones = [{zone: [] for zone in ZONES} for _ in (0, 1)]
        self.pools = [ZERO, ZERO]
        self.shuffles = [0, 0]
        self.milled, self.drawn = [], []
        self.target = owner if mode == 'self_target' else self.foe
        self.searcher = None
        source_zone = 'graveyard' if mode.startswith('flashback_') else 'hand'
        self.add('source', record['name'], owner, source_zone)
        self.build()

    def build(self):
        mode, owner, foe = self.mode, self.owner, self.foe
        libraries = [None, None]
        libraries[owner], libraries[foe] = CASTER_DECK, FOE_DECK
        if mode in ('short_library', 'short_black', 'short_paid'):
            libraries[foe] = FOE_DECK[:2 if self.csvid != TRAP else 3]
        if mode == 'zero_creatures':
            libraries[foe] = ('Island', RELIC, AMULET_KEEP, 'Mountain', BODY, SECOND_BODY)
        if mode in SEARCH:
            self.searcher = owner if mode == 'own_search_only' else foe
            # A single legal search hit and an identity-uniform remainder make
            # the shuffled order deterministic without prescribing the RNG.
            libraries[self.searcher] = ('Forest',) + (AMULET_KEEP,) * 14
            self.add('amulet', AMULET, self.searcher, 'battlefield', UNTAPPED)
        for player in (0, 1):
            self.add(f'old_grave_{player}', THIRD_BODY, player, 'graveyard')
            for index, name in enumerate(libraries[player]):
                self.add(f'library_{player}_{index}', name, player, 'library')

        if mode in SPLICE:
            self.add('carrier', CARRIER, owner, 'hand')
        elif mode == 'offturn':
            self.add('parent', AMULET, foe, 'hand')
        elif mode == 'countered':
            self.add('answer', COUNTER, foe, 'hand')
        if self.csvid == DREADWATERS:
            # Both enemy lands are real and tapped, including in the zero-own case.
            self.add('foe_land_0', 'Island', foe, 'battlefield', TAPPED)
            self.add('foe_land_1', 'Mountain', foe, 'battlefield', TAPPED)
            self.add('nonland', RELIC, owner, 'battlefield', UNTAPPED)
            if mode != 'foe_lands_only':
                for index, name in enumerate((CITADEL, 'Mountain', 'Forest')):
                    self.add(f'own_land_{index}', name, owner, 'battlefield', TAPPED)
            if mode == 'land_removed':
                self.add('answer', BOUNCE, foe, 'hand')

    def add(self, ref, name, player, zone, state=0):
        assert ref not in self.refs
        if zone == 'battlefield':
            assert state in (TAPPED, UNTAPPED)
        else:
            assert state == 0
        entry = card(name, ref, player, zone)
        if zone == 'battlefield':
            entry['state'] = ['tapped' if state == TAPPED else 'untapped']
        self.setup.append(entry)
        self.refs[ref] = (name, player)
        self.zones[player][zone].append(name)

    def emit(self, op, ref=None, other=None, prop=None, *, player=None, value=None, zone=None, index=None, color=None):
        fields = dict(ref=ref, other=other, property=prop, player=player,
                      value=value, zone=zone, index=index, color=color)
        self.steps.append(step(op, **{key: value for key, value in fields.items() if value is not None}))

    def transfer(self, ref, source, destination):
        name, player = self.refs[ref]
        self.zones[player][source].remove(name)
        if destination:
            self.zones[player][destination].append(name)

    def fund(self, player, pool):
        self.pools[player] = tuple(pool)
        for color, value in enumerate(pool):
            self.emit('set_mana', player=player, color=color, value=value)
        self.emit('refresh')

    def pay(self, player, fee):
        self.pools[player] = tuple(have-due for have, due in zip(self.pools[player], fee))
        assert min(self.pools[player]) >= 0

    def receipt(self, expected_stack=0):
        self.emit('assert_choices_consumed')
        self.emit('assert_stack', value=expected_stack)
        for player, pool in enumerate(self.pools):
            for color, value in enumerate(pool):
                self.emit('assert_mana', player=player, color=color, value=value)
            self.emit('assert_life', player=player, value=20)
            self.emit('assert_shuffle_count', player=player, value=self.shuffles[player])
            for zone in ZONES:
                # A pending spell retains a physical hand slot in this engine.
                if expected_stack and zone == 'hand':
                    continue
                names = self.zones[player][zone]
                self.emit('assert_zone', player=player, zone=zone, value=len(names))
                if zone in ('library', 'graveyard'):
                    for index, name in enumerate(names):
                        self.emit('assert_zone_card', player=player, zone=zone, index=index,
                                  value=self.rows[name]['csvid'])
                else:
                    for name in sorted({name for name, _ in self.refs.values()}):
                        self.emit('assert_zone_multiplicity', player=player, zone=zone,
                                  index=self.rows[name]['csvid'], value=names.count(name))

    def drain(self, response=False):
        self.emit('resolve_response' if response else 'resolve')
        self.emit('refresh')
        self.emit('assert_choices_consumed')
        self.emit('assert_stack', value=0)

    def search(self):
        actor = self.searcher
        self.fund(actor, (1, 0, 0, 0, 0, 0))
        self.emit('activate', 'amulet', value=1)
        self.pay(actor, (1, 0, 0, 0, 0, 0))
        self.transfer('amulet', 'battlefield', 'graveyard')
        self.receipt(1)
        self.emit('choose_zone_card', player=actor, value=self.rows['Forest']['csvid'])
        self.drain()
        self.zones[actor]['library'].remove('Forest')
        self.zones[actor]['hand'].append('Forest')
        self.shuffles[actor] += 1
        self.receipt()
        if self.mode == 'prior_turn':
            self.emit('advance_turn')
            self.emit('turn', player=self.owner)
            self.receipt()

    def cast(self, ref, fee, *, target=None, response=False, budget=None, legal=True, dialogs=(), addon=ZERO, x=None):
        _, actor = self.refs[ref]
        self.fund(actor, fee if budget is None else budget)
        if x is not None:
            self.emit('choose_x', player=actor, value=x)
        for choice in dialogs:
            self.emit('choose_dialog', player=actor, value=choice)
        if type(target) is int:
            self.emit('choose_player', player=actor, value=target)
        elif target is not None:
            self.emit('choose_target', ref=target, player=actor)
        self.emit('cast_response' if response else 'cast', ref, value=int(legal))
        if legal:
            self.pay(actor, tuple(base+extra for base, extra in zip(fee, addon)))
            self.transfer(ref, 'hand', None)
        self.receipt(int(legal) + int(response))

    def finish_spell(self, ref, zone='graveyard'):
        name, actor = self.refs[ref]
        self.zones[actor][zone].append(name)

    def mill(self, amount, *, player=None):
        player = self.target if player is None else player
        library = self.zones[player]['library']
        moved = library[:amount]
        del library[:amount]
        self.zones[player]['graveyard'].extend(moved)
        self.milled.extend(moved)
        return moved

    def draw(self, amount, *, player=None):
        player = self.owner if player is None else player
        library = self.zones[player]['library']
        assert len(library) >= amount
        moved = library[:amount]
        del library[:amount]
        self.zones[player]['hand'].extend(moved)
        self.drawn.extend(moved)

    def archive_trap(self):
        printed = tuple(cost_from_oracle(self.record))
        budget = ZERO if self.mode in DENIED | {'alternate'} else printed
        due = ZERO if self.mode == 'alternate' else printed
        if self.searcher is not None:
            self.search()
        dialogs = ()
        if self.owner == 0 and self.mode in ('alternate', 'paid_after_search'):
            dialogs = (int(self.mode == 'paid_after_search'),)
        legal = self.mode not in DENIED
        # Target-opponent cards automatically nominate the sole opponent.
        self.cast('source', due, budget=budget, legal=legal, dialogs=dialogs)
        if legal:
            self.drain()
            self.mill(13)
            self.finish_spell('source')
            self.receipt()

    def dampen(self):
        mode = self.mode
        normal_cost = tuple(cost_from_oracle(self.record))
        if mode in SPLICE:
            carrier_cost = tuple(cost_from_oracle(self.oracle[CARRIER]))
            accept = mode == 'splice'
            budget = tuple(base+fee for base, fee in zip(carrier_cost, normal_cost))
            if mode == 'short_splice':
                budget = carrier_cost
            dialogs = () if mode == 'short_splice' else (int(accept),)
            self.cast('carrier', carrier_cost, dialogs=dialogs, target=self.target if accept else None,
                      budget=budget, addon=normal_cost if accept else ZERO)
            self.drain()
            self.draw(1)
            if accept:
                self.mill(4)
            self.finish_spell('carrier')
        else:
            response = mode == 'offturn'
            if response:
                self.cast('parent', tuple(cost_from_oracle(self.oracle[AMULET])))
                self.emit('announce_spell', 'parent')
            self.cast('source', normal_cost, target=self.target, response=response)
            if mode == 'countered':
                self.emit('announce_spell', 'source')
                self.cast('answer', tuple(cost_from_oracle(self.oracle[COUNTER])), response=True)
                self.drain(response=True)
                self.finish_spell('answer')
            else:
                self.drain(response=response)
                if response:
                    self.finish_spell('parent', 'battlefield')
                self.mill(4)
            self.finish_spell('source')
        self.receipt()

    def confession(self):
        black = self.mode in ('mixed_black', 'short_black')
        fee = (4, int(black), int(not black), 0, 0, 0)
        self.cast('source', fee, target=self.target)
        self.drain()
        self.mill(4)
        self.draw(sum(has_type(self.oracle[name], 'creature') for name in self.milled))
        self.finish_spell('source')
        self.receipt()

    def dreadwaters(self):
        self.cast('source', tuple(cost_from_oracle(self.record)), target=self.target)
        response = self.mode == 'land_removed'
        if response:
            self.emit('announce_spell', 'source')
            self.cast('answer', tuple(cost_from_oracle(self.oracle[BOUNCE])), target='own_land_0', response=True)
        self.drain(response=response)
        if response:
            self.finish_spell('answer')
            self.transfer('own_land_0', 'battlefield', 'hand')
        lands = sum(has_type(self.oracle[name], 'land') for name in self.zones[self.owner]['battlefield'])
        self.mill(lands)
        self.finish_spell('source')
        self.receipt()

    def play(self):
        active = self.foe if self.mode in ('offturn', 'alternate', 'prior_turn', 'paid_after_search') else self.owner
        self.emit('turn', player=active)
        self.emit('present_ai_dialogs', value=1)
        self.fund(0, ZERO)
        self.fund(1, ZERO)
        self.receipt()
        action = {TRAP: self.archive_trap, DAMPEN: self.dampen, CONFESSION: self.confession, DREADWATERS: self.dreadwaters}
        action[self.csvid]()


class FollowerCase(MillCase):
    def build(self):
        mode, owner, foe = self.mode, self.owner, self.foe
        libraries = [None, None]
        libraries[owner], libraries[foe] = CASTER_DECK, FOE_DECK
        short = {'flashback_short': 2, 'odd': 7, 'one_card': 1, 'live_draw': 6,
                 'mill_x3_short': 1, 'short_x3': 2}
        if mode in short:
            libraries[foe] = libraries[foe][:short[mode]]
        if mode == 'no_creatures':
            libraries[owner] = ('Forest', 'Plains', RELIC, BODY)
            libraries[foe] = ('Island', AMULET_KEEP, 'Mountain', SECOND_BODY)
        for player in (0, 1):
            self.add(f'old_grave_{player}', THIRD_BODY, player, 'graveyard')
            for index, name in enumerate(libraries[player]):
                self.add(f'library_{player}_{index}', name, player, 'library')
        if mode == 'live_draw':
            self.add('answer', CARRIER, foe, 'hand')
        self.zombies = 0

    def flashback(self, x, fee):
        self.emit('prepare_graveyard_abilities')
        self.fund(self.owner, fee)
        legal = self.mode != 'flashback_offturn'
        self.emit('require_can_cast_graveyard', 'source', value=int(legal))
        if legal:
            self.emit('choose_x', player=self.owner, value=x)
            self.emit('choose_player', player=self.owner, value=self.target)
        self.emit('cast_graveyard', 'source', value=int(legal))
        self.emit('refresh')
        if legal:
            self.pay(self.owner, fee)
            self.transfer('source', 'graveyard', 'exile')
            self.mill(2*x)
        self.receipt()

    def confusion(self):
        x = 0 if self.mode == 'zero_x' else 2
        fee = (x, 0, 1, 0, 0, 0)
        if self.mode.startswith('flashback_'):
            self.flashback(x, fee)
            return
        self.cast('source', fee, target=self.target, x=x)
        self.drain()
        self.mill(x)
        self.finish_spell('source')
        self.receipt()

    def traumatize(self):
        self.cast('source', tuple(cost_from_oracle(self.record)), target=self.target)
        response = self.mode == 'live_draw'
        if response:
            self.emit('announce_spell', 'source')
            self.cast('answer', tuple(cost_from_oracle(self.oracle[CARRIER])), response=True)
        self.drain(response=response)
        if response:
            self.draw(1, player=self.foe)
            self.finish_spell('answer')
        self.mill(len(self.zones[self.target]['library']) // 2)
        self.finish_spell('source')
        self.receipt()

    def fascination(self):
        mode = self.mode
        x = 0 if mode.startswith('zero_') else 2 if mode == 'draw_x2' else 3
        self.cast('source', (x, 0, 2, 0, 0, 0), x=x)
        mill_mode = mode in ('mill_x3_short', 'zero_mill')
        # This callback's mode input is actually presented during resolution.
        self.emit('choose_dialog', player=self.owner, value=int(mill_mode))
        self.drain()
        apply = self.mill if mill_mode else self.draw
        for player in (self.owner, self.foe):
            apply(x, player=player)
        self.finish_spell('source')
        self.receipt()

    def summons(self):
        x = {'mixed_x4': 4, 'short_x3': 3, 'no_creatures': 3, 'zero_x': 0}[self.mode]
        self.cast('source', (x, 2, 0, 0, 0, 0), x=x)
        self.drain()
        for player in (self.owner, self.foe):
            moved = self.mill(x, player=player)
            self.zombies += sum(has_type(self.oracle[name], 'creature') for name in moved)
        self.zones[self.owner]['battlefield'].extend([ZOMBIE]*self.zombies)
        self.finish_spell('source')
        self.receipt()
        for player in (0, 1):
            self.emit('assert_zone_multiplicity', player=player, zone='battlefield',
                      index=self.rows[ZOMBIE]['csvid'], value=self.zombies if player == self.owner else 0)
        if self.zombies:
            for prop, value in (('token', 1), ('subtype_Zombie', 1), ('color', 2),
                                ('power', 2), ('toughness', 2), ('tapped', 1)):
                self.emit('assert_matching_permanents', player=self.owner, index=self.rows[ZOMBIE]['csvid'],
                          prop=prop, value=value)

    def play(self):
        active = self.foe if self.mode == 'flashback_offturn' else self.owner
        self.emit('turn', player=active)
        self.emit('present_ai_dialogs', value=1)
        self.fund(0, ZERO)
        self.fund(1, ZERO)
        self.receipt()
        action = {CONFUSION: self.confusion, TRAUMATIZE: self.traumatize,
                  FASCINATION: self.fascination, SUMMONS: self.summons}
        action[self.csvid]()


@lru_cache(maxsize=1)
def profiles():
    return json.loads(PROFILE.read_bytes())


def classify(record, csvid):
    cost, type_line, clauses, common, human = MODELS[csvid]
    keywords = {'Mill'}
    if csvid == DAMPEN:
        keywords.add('Splice')
    elif csvid == CONFUSION:
        keywords.add('Flashback')
    if (record.get('oracle_text') != '\n'.join(clauses) or record.get('mana_cost') != cost
            or record.get('type_line') != type_line or record.get('layout') != 'normal'
            or record.get('card_faces') or record.get('selected_face') or set(record.get('keywords', ())) != keywords):
        raise ValueError('Exact whole mill spell and printed costs and riders required')
    return dict(csvid=csvid, printed_cost=cost, clauses=list(clauses), modes=list(common), human_modes=list(human))


def variants(model):
    return ([f'{mode}_{owner}' for mode in model['modes'] for owner in (0, 1)]
            + [f'{mode}_0' for mode in model['human_modes']])


def validate_witnesses(oracle, rows):
    pins = profiles()
    if (set(pins) != {'sources', 'witnesses'} or set(pins['witnesses']) != WITNESSES
            or {pin['registry']['csvid'] for pin in pins['sources'].values()} != set(MODELS)):
        raise ValueError('Exact library mill source and witness scope required')
    for group in pins.values():
        for name, pin in group.items():
            record, row = oracle.get(name, {}), rows.get(name, {})
            layout = 'token' if name == ZOMBIE else 'normal'
            if (not exact(pin, dict(oracle=record, registry=row)) or record.get('layout') != layout
                    or row.get('kind') != 'card' or name != ZOMBIE and row.get('handler') in (None, '0', 'NULL', 'card_dummy')):
                raise ValueError(f'Changed implemented mill source or witness: {name}')


def spec(declaration, record):
    pin = profiles()['sources'].get((record or {}).get('name'))
    if not pin or not exact(record, pin['oracle']):
        raise ValueError('Complete pinned mill-spell Oracle required')
    model = classify(record, pin['registry']['csvid'])
    selected = declaration.get('variants')
    if (declaration.get('family') != FAMILY or type(declaration.get('version')) is not int
            or declaration['version'] != 2 or declaration.get('oracle_text') != record['oracle_text']
            or not exact(declaration.get('params'), model) or not isinstance(selected, list) or not selected
            or any(type(v) is not str for v in selected) or len(selected) != len(set(selected))
            or set(selected) - set(variants(model))):
        raise ValueError('Exact mill clauses, costs, and scoped variants required')
    return dict(model, family=FAMILY)


def expand(name, csvid, declaration, model, selected, rows, oracle):
    validate_witnesses(oracle, rows)
    if rows[name]['csvid'] != csvid or model['csvid'] != csvid or set(selected) - set(variants(model)):
        raise ValueError('Exact mill primary and variants required')
    factory = FollowerCase if csvid in FOLLOWERS else MillCase
    gaps = {
        DAMPEN: ['Splice reveal presentation and native AI Splice selection'],
        FASCINATION: ['Modal commitment at casting versus the legacy resolution-time menu'],
    }.get(csvid, [])
    cases = []
    for variant in selected:
        mode, owner = variant.rsplit('_', 1)
        game = factory(oracle[name], csvid, int(owner), mode, rows, oracle)
        game.play()
        metadata = dict(family=FAMILY, version=2, variant=variant, params=declaration['params'],
                        oracle_text=declaration['oracle_text'], covered_clauses=model['clauses'],
                        untested_components=gaps)
        cases.append(dict(name=f'MillLibrarySpellsRecipe_{csvid}_{variant}', card=name,
                          fixture='empty_main', seed=SEED, setup=game.setup, steps=game.steps, _recipe=metadata))
    return cases
