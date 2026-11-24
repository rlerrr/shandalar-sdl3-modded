"""Draw-step replacements and their independently printed zone/life riders."""

from collections import Counter
from functools import lru_cache
import json
from pathlib import Path

from counter_condition_recurrence_recipes import exact
from recipes import card, cost_from_oracle, step

FAMILY = 'draw_step_replacement_and_search'
PROFILE = Path(__file__).with_name('data') / 'draw_step_replacement_and_search_wave8_profiles.json'
SANCTUARY, MARALEN, OMEN, SYLVAN = (4991, 9382, 11650, 803)
RULES = {
    SANCTUARY: 'At the beginning of your upkeep, you may search your library for a basic land card, reveal that card, put it into your hand, then shuffle. If you do, you skip your draw step this turn.',
    MARALEN: "Players can't draw cards.\nAt the beginning of each player's draw step, that player loses 3 life, searches their library for a card, puts it into their hand, then shuffles.",
    OMEN: "Players can't draw cards.\nAt the beginning of each player's draw step, that player exiles the top card of their library. If it's a land card, the player puts it onto the battlefield. Otherwise, the player casts it without paying its mana cost if able.",
    SYLVAN: 'At the beginning of your draw step, you may draw two additional cards. If you do, choose two cards in your hand drawn this turn. For each of those cards, pay 4 life or put the card on top of your library.',
}
NECRO, POSSESSION, TAIGAM = (2050, 8262, 17330)
RULES.update({
    NECRO: 'Skip your draw step.\nWhenever you discard a card, exile that card from your graveyard.\nPay 1 life: Exile the top card of your library face down. Put that card into your hand at the beginning of your next end step.',
    POSSESSION: 'Enchant opponent\nSkip your draw step.\nWhenever enchanted opponent draws a card, you may draw a card.',
    TAIGAM: "Skip your draw step.\nAt the beginning of your upkeep, look at the top three cards of your library. Put one of them into your hand and the rest into your graveyard.\n{B}, {T}, Exile X cards from your graveyard: Target creature gets -X/-X until end of turn.",
})
MODES = {
    SANCTUARY: ('search', 'fail_find', 'skip_trigger'),
    MARALEN: ('both_search', 'spells_blocked', 'empty_library'),
    OMEN: ('land', 'free_spell', 'uncastable', 'permanent', 'spells_blocked'),
    SYLVAN: ('extra_draws', 'pass_draws'),
    NECRO: ('life_own_end', 'life_other_turn', 'discard', 'skip_with_spell'),
    POSSESSION: ('rider_draw', 'spell_draws', 'hexproof', 'remove'),
    TAIGAM: ('pick_three', 'short_library', 'shrink_one', 'shrink_two', 'zero_eligible'),
}
HUMAN_ONLY = {SANCTUARY: ('pass_search', 'next_turn'), POSSESSION: ('decline_optional',)}
ORIGINAL_IDS = {SANCTUARY, MARALEN, OMEN, SYLVAN}
ZONE = dict(hand=0, battlefield=1, library=2, graveyard=3, exile=4)
ZERO = (0,) * 6
DECK = ('Island', 'Forest', 'Plains', 'Swamp', 'Grizzly Bears')
WITNESSES = {
    *DECK, 'City of Brass', 'Maralen of the Mornsong', 'Mountain', 'Divination',
    'Lava Spike', 'Swords to Plowshares', 'Rummaging Goblin', 'Hill Giant',
    'Leyline of Sanctity', 'Disenchant',
}
GAPS = {
    SANCTUARY: 'The real search, named move, shuffle and skip are observed. Public reveal visibility has no separate receipt. Seat one uses the implemented no-land-hand acceptance policy.',
    MARALEN: 'Real search, ordinary-draw suppression and life are observed. The final phase receipts do not distinguish life-loss/search order inside the trigger.',
    OMEN: 'Land, free targeted sorcery and uncastable exile are exercised. The permanent fixture has no separate cast-only trigger to distinguish casting from direct entry.',
    SYLVAN: 'The raw partner branch ends at the actual draw-phase boundary. The two card selections and pay-or-top settlement are outside these cases; the legacy callback delays them to a first-main phase event.',
    NECRO: 'The bounded cases observe an exile record and delayed receipt of the known top card. No direct face-down visibility observer exists.',
    POSSESSION: 'The actual opponent target is automatic in two-seat duels. Seat zero makes a direct optional choice; the legacy seat-one autopolicy accepts.',
    TAIGAM: 'The zero-cost branch observes legal activation availability with an empty graveyard. The positive branches really exile chosen cards and apply temporary shrink.',
}


@lru_cache(maxsize=1)
def profiles():
    return json.loads(PROFILE.read_bytes())


def classify(record, csvid):
    if (csvid not in RULES or record.get('layout') != 'normal' or record.get('selected_face')
            or record.get('card_faces') or record.get('oracle_text') != RULES[csvid]):
        raise ValueError('whole normal Oracle draw replacement and rider text required')
    if record.get('keywords') != (['Enchant'] if csvid == POSSESSION else []):
        raise ValueError('exact draw replacement keyword pins required')
    return dict(csvid=csvid, mana=cost_from_oracle(record))


def variants(shape):
    csvid = shape['csvid']
    return [f'{mode}_{seat}' for mode in MODES[csvid] for seat in (0, 1)] + [
        f'{mode}_0' for mode in HUMAN_ONLY.get(csvid, ())
    ]


def validate_variants(shape, selected):
    if (type(selected) is not list or not selected or any(type(v) is not str for v in selected)
            or len(set(selected)) != len(selected) or set(selected) - set(variants(shape))):
        raise ValueError('explicit distinct supported draw scope required')


def spec(declaration, record):
    pin = profiles()['sources'].get(record.get('name'))
    if not pin or not exact(pin['oracle'], record):
        raise ValueError('exact whole Oracle draw replacement required')
    shape = classify(record, pin['registry']['csvid'])
    if (declaration.get('family') != FAMILY or type(declaration.get('version')) is not int
            or declaration['version'] != 2 or declaration.get('oracle_text') != record['oracle_text']
            or not exact(declaration.get('params'), shape)):
        raise ValueError('whole versioned draw replacement declaration required')
    validate_variants(shape, declaration.get('variants'))
    return dict(shape, family=FAMILY)


def validate_witnesses(oracle, rows):
    pins = profiles()
    if set(pins) != {'sources', 'witnesses'} or set(pins['witnesses']) != WITNESSES:
        raise ValueError('complete independent draw witnesses required')
    for group, entries in pins.items():
        for name, pin in entries.items():
            row = rows.get(name, {})
            if (not exact(oracle.get(name), pin['oracle']) or not exact(row, pin['registry'])
                    or row.get('kind') != 'card' or row.get('handler') in (None, '0', 'NULL')):
                raise ValueError('exact source or witness changed: ' + name)
            if group == 'sources':
                if row['handler'] == 'card_dummy':
                    raise ValueError('implemented draw replacement primary required')
                classify(oracle[name], row['csvid'])
    if (oracle['Rummaging Goblin']['oracle_text'] != '{T}, Discard a card: Draw a card.'
            or oracle['Divination']['oracle_text'] != 'Draw two cards.'
            or oracle['Hill Giant']['toughness'] != '3'):
        raise ValueError('real discard, off-step draw and shrink witnesses required')


class Game:
    def __init__(self, row, owner, mode, oracle, rows):
        self.row, self.owner, self.foe = row, owner, 1 - owner
        self.active, self.mode, self.oracle, self.rows = owner, mode, oracle, rows
        self.source = row['csvid']
        self.setup, self.steps, self.cards = [], [], {}
        self.life, self.shuffles = [20, 20], [0, 0]
        self.libraries = {0: list(DECK), 1: list(DECK)}
        self.add('source', row['name'], owner, 'hand')
        if self.source == SANCTUARY:
            self.libraries[owner] = ['Grizzly Bears', 'Forest'] if mode == 'next_turn' else [
                'Grizzly Bears', 'City of Brass', 'Forest', 'Island', 'Swamp',
            ]
            if mode == 'skip_trigger':
                self.add('skip_witness', 'Maralen of the Mornsong', self.foe, 'battlefield')
        elif self.source == MARALEN and mode == 'empty_library':
            self.libraries[self.foe] = []
        elif self.source == OMEN:
            lead = dict(land='Forest', free_spell='Lava Spike', uncastable='Swords to Plowshares',
                        permanent='Grizzly Bears', spells_blocked='Forest')[mode]
            self.libraries[self.foe] = [lead, 'Island', 'Plains', 'Swamp']
            if mode == 'land':
                self.add('normal_land', 'Mountain', self.foe, 'hand')
        elif self.source == SYLVAN:
            self.add('older_hand', 'City of Brass', owner, 'hand')
        if mode == 'spells_blocked':
            for player in (0, 1):
                self.add(f'div_{player}', 'Divination', player, 'hand')
        for player, names in self.libraries.items():
            for i, name in enumerate(names):
                self.add(f'lib_{player}_{i}', name, player, 'library')

    def add(self, ref, name, player, zone):
        self.setup.append(card(name, ref, player, zone))
        self.cards[ref] = dict(player=player, name=name, zone=zone)

    def emit(self, op, ref='', *, player=0, value=0, zone=0, index=0, color=0, prop=''):
        if op in ('untap_step', 'upkeep_step', 'draw_step'):
            self.steps.append(step(op, player=player))
            return
        if op == 'play_land':
            self.steps.append(step(op, ref=ref, value=value))
            return
        if op in ('assert_shuffle_count', 'choose_trigger_source'):
            fields = dict(player=player, **({'ref': ref} if ref else {'value': value}))
            self.steps.append(step(op, **fields))
            return
        fields = dict(player=player, value=value, index=index, color=color)
        if ref:
            fields['ref'] = ref
        if prop:
            fields['property'] = prop
        if op in ('assert_zone', 'assert_zone_card', 'assert_zone_multiplicity'):
            fields['zone'] = next(name for name, number in ZONE.items() if number == zone)
        self.steps.append(step(op, **fields))

    def pool(self, action, player, mana):
        for color, amount in enumerate(mana):
            self.emit(action, player=player, value=amount, color=color)

    def observe(self):
        self.emit('refresh')
        self.emit('assert_stack', value=0)
        names = sorted({card['name'] for card in self.cards.values()})
        for player in (0, 1):
            self.emit('assert_life', player=player, value=self.life[player])
            self.pool('assert_mana', player, ZERO)
            if self.shuffles[player] is not None:
                self.emit('assert_shuffle_count', player=player, value=self.shuffles[player])
            for zone, zid in ZONE.items():
                contents = self.libraries[player] if zone == 'library' else [
                    c['name'] for c in self.cards.values() if (c['player'], c['zone']) == (player, zone)
                ]
                self.emit('assert_zone', player=player, zone=zid, value=len(contents))
                counts = Counter(contents)
                for name in names:
                    self.emit('assert_zone_multiplicity', player=player, zone=zid,
                              index=self.rows[name]['csvid'], value=counts[name])
                if zone == 'library' and (self.shuffles[player] == 0 or len(contents) == 1):
                    for i, name in enumerate(contents):
                        self.emit('assert_zone_card', player=player, zone=zid, index=i,
                                  value=self.rows[name]['csvid'])

    def take(self, player, name, zone):
        entry, = [c for c in self.cards.values() if (c['player'], c['name'], c['zone']) == (player, name, 'library')]
        self.libraries[player].remove(name)
        entry['zone'] = zone

    def draw(self, player, count=1):
        for _ in range(count):
            self.take(player, self.libraries[player][0], 'hand')

    def cast(self, ref, destination, *, target=None, before_resolve=None):
        entry = self.cards[ref]
        player = entry['player']
        self.pool('set_mana', player, cost_from_oracle(self.oracle[entry['name']]))
        self.emit('refresh')
        if target:
            self.emit('choose_target', target, player=player)
        self.emit('cast', ref, value=1)
        self.emit('assert_stack', value=1)
        self.pool('assert_mana', player, ZERO)
        if before_resolve:
            before_resolve()
        self.emit('resolve')
        self.emit('assert_choices_consumed')
        entry['zone'] = destination

    def advance(self):
        self.emit('advance_turn')
        self.active = 1 - self.active
        self.emit('untap_step', player=self.active)

    def upkeep(self):
        self.emit('upkeep_step', player=self.active)

    def draw_phase(self, *, unused=0):
        self.emit('draw_step', player=self.active)
        self.emit('assert_unused_choices', value=unused)

    def tutor(self, player, name):
        self.emit('choose_zone_card', player=player, value=self.rows[name]['csvid'])
        self.take(player, name, 'hand')
        self.shuffles[player] += 1

    def sanctuary(self):
        self.advance()
        self.upkeep()
        if self.mode != 'skip_trigger':
            self.draw_phase()
            self.draw(self.foe)
            self.observe()
        self.advance()
        searching = self.mode != 'pass_search'
        if searching:
            if self.owner == 0:
                self.emit('choose_trigger_source', 'source', player=self.owner)
            if self.mode == 'fail_find':
                self.emit('choose_zone', player=self.owner, value=-1)
                self.shuffles[self.owner] += 1
            else:
                if self.mode != 'next_turn':
                    self.emit('observe_search_filters', value=1)
                self.tutor(self.owner, 'Forest')
        self.upkeep()
        self.emit('assert_unused_choices', value=0)
        if self.mode in ('search', 'skip_trigger'):
            for name, legal in (('Forest', 1), ('Island', 1), ('City of Brass', 0), ('Grizzly Bears', 0)):
                self.emit('assert_offered_legal_card', index=self.rows[name]['csvid'], value=legal)
            self.emit('observe_search_filters', value=0)
        self.observe()
        if self.mode == 'skip_trigger':
            # Maralen must not obtain a draw-step search in a skipped draw step.
            self.emit('choose_zone_card', player=self.owner, value=self.rows['Grizzly Bears']['csvid'])
        self.draw_phase(unused=int(self.mode == 'skip_trigger'))
        if not searching:
            self.draw(self.owner)
        self.observe()
        if self.mode == 'next_turn':
            # In a two-card library, the remaining card is known despite the earlier shuffle.
            self.advance()
            self.upkeep()
            self.advance()
            self.upkeep()
            self.draw_phase()
            self.draw(self.owner)
            self.observe()

    def cast_draw_driver(self, player):
        self.emit('turn', player=player)
        self.cast(f'div_{player}', 'graveyard')
        self.observe()

    def maralen(self):
        if self.mode == 'spells_blocked':
            self.cast_draw_driver(self.owner)
        for player in (self.foe, self.owner):
            self.advance()
            self.upkeep()
            self.observe()
            if self.libraries[player]:
                self.tutor(player, 'Grizzly Bears' if player == self.foe else 'Swamp')
            else:
                self.shuffles[player] = None
            self.life[player] -= 3
            self.draw_phase()
            self.observe()
            if self.mode == 'spells_blocked' and player == self.foe:
                self.cast_draw_driver(player)

    def omen(self):
        if self.mode == 'spells_blocked':
            self.cast_draw_driver(self.owner)
        self.advance()
        if self.mode == 'free_spell':
            # The only legal target intent can be consumed by either real phase. A premature
            # cast still fails independently on life and zone observations after upkeep.
            self.emit('choose_player', player=self.foe, value=self.owner)
        self.upkeep()
        self.observe()
        destinations = dict(land='battlefield', free_spell='graveyard', uncastable='exile',
                            permanent='battlefield', spells_blocked='battlefield')
        self.take(self.foe, self.libraries[self.foe][0], destinations[self.mode])
        if self.mode == 'free_spell':
            self.life[self.owner] -= 3
        self.draw_phase()
        self.observe()
        if self.mode == 'land':
            self.emit('turn', player=self.foe)
            self.emit('play_land', 'normal_land', value=1)
            self.cards['normal_land']['zone'] = 'battlefield'
            self.observe()
        elif self.mode == 'spells_blocked':
            self.cast_draw_driver(self.foe)

    def sylvan(self):
        self.advance()
        self.upkeep()
        self.draw_phase()
        self.draw(self.foe)
        self.observe()
        self.advance()
        self.upkeep()
        self.observe()
        accept = self.mode == 'extra_draws'
        self.emit('choose_dialog', player=self.owner, value=int(not accept))
        self.draw_phase()
        self.draw(self.owner, 3 if accept else 1)
        self.observe()

    def build(self):
        self.emit('turn', player=self.owner)
        for player in (0, 1):
            self.pool('set_mana', player, ZERO)
        self.cast('source', 'battlefield')
        self.observe()
        {SANCTUARY: self.sanctuary, MARALEN: self.maralen,
         OMEN: self.omen, SYLVAN: self.sylvan}[self.source]()



class AddedGame(Game):
    def __init__(self, row, owner, mode, oracle, rows):
        super().__init__(row, owner, mode, oracle, rows)
        if self.source == NECRO and mode == 'skip_with_spell':
            self.add(f'div_{owner}', 'Divination', owner, 'hand')
        elif self.source == NECRO and mode == 'discard':
            self.add('looter', 'Rummaging Goblin', owner, 'hand')
            self.add('fodder', 'Mountain', owner, 'hand')
        elif self.source == POSSESSION:
            if mode == 'hexproof':
                self.add('protected_player', 'Leyline of Sanctity', self.foe, 'battlefield')
            elif mode == 'spell_draws':
                self.add('foe_spell', 'Divination', self.foe, 'hand')
            elif mode == 'remove':
                self.add('remove_spell', 'Disenchant', self.foe, 'hand')
        elif self.source == TAIGAM:
            if mode in ('short_library', 'zero_eligible'):
                keep = {'Island', 'Forest'} if mode == 'short_library' else {'Forest'}
                removed = {ref for ref, entry in self.cards.items() if entry['player'] == owner
                           and entry['zone'] == 'library' and entry['name'] not in keep}
                self.setup[:] = [entry for entry in self.setup if entry['ref'] not in removed]
                for ref in removed:
                    self.cards.pop(ref)
                self.libraries[owner] = [name for name in self.libraries[owner] if name in keep]
            if mode.startswith('shrink') or mode == 'zero_eligible':
                self.add('victim', 'Hill Giant', self.foe, 'battlefield')

    def travel_draw(self, *, draws):
        self.advance()
        self.upkeep()
        self.draw_phase()
        self.draw(self.active, draws)
        self.observe()

    def necro_paid(self):
        self.emit('can_activate', 'source', value=1)
        self.emit('activate', 'source', value=1)
        self.emit('assert_stack', value=1)
        self.life[self.owner] -= 1
        self.emit('assert_life', player=self.owner, value=self.life[self.owner])
        self.emit('resolve')
        self.emit('assert_choices_consumed')
        name = self.libraries[self.owner][0]
        # The public rfg vector enumerates face-up cards only. Keep this
        # face-down card private until the genuine owner-end-step receipt.
        self.take(self.owner, name, 'held_face_down')
        self.observe()
        return name

    def necro_end(self, name):
        self.emit('cleanup')
        held, = [entry for entry in self.cards.values()
                 if entry['player'] == self.owner and entry['name'] == name and entry['zone'] == 'held_face_down']
        held['zone'] = 'hand'
        self.observe()

    def necro(self):
        if self.mode == 'life_own_end':
            self.necro_end(self.necro_paid())
            return
        if self.mode == 'discard':
            self.cast('looter', 'battlefield')
        self.travel_draw(draws=1)
        if self.mode == 'life_other_turn':
            self.emit('turn', player=self.foe)
            held = self.necro_paid()
        self.travel_draw(draws=0)
        if self.mode == 'life_other_turn':
            self.necro_end(held)
        if self.mode == 'skip_with_spell':
            self.emit('turn', player=self.owner)
            self.cast(f'div_{self.owner}', 'graveyard')
            self.draw(self.owner, 2)
            self.observe()
        if self.mode == 'discard':
            self.emit('discard_policy', value=1)
            if self.owner == 1:
                self.emit('choose_dialog', player=1, value=0)
            self.emit('can_activate', 'looter', value=1)
            self.emit('activate', 'looter', value=1)
            self.emit('assert_unused_choices', value=0)
            self.emit('assert_stack', value=1)
            if self.owner == 0:
                self.emit('assert_discard_choices', value=1)
            self.emit('assert_card', 'looter', prop='tapped', value=1)
            self.cards['fodder']['zone'] = 'exile'
            self.emit('resolve')
            self.emit('assert_choices_consumed')
            self.draw(self.owner)
            self.observe()

    def possession_triggers(self, count):
        if self.owner == 0:
            for _ in range(count):
                self.emit('choose_trigger_source', 'source', player=self.owner)

    def possessed_turns(self):
        self.advance()
        self.upkeep()
        accept = self.mode != 'decline_optional'
        if accept:
            self.possession_triggers(1)
        self.draw_phase()
        self.draw(self.foe)
        if accept:
            self.draw(self.owner)
        self.observe()
        if self.mode == 'spell_draws':
            self.emit('turn', player=self.foe)
            self.cast('foe_spell', 'graveyard', before_resolve=lambda: self.possession_triggers(2))
            self.draw(self.foe, 2)
            self.draw(self.owner, 2)
            self.observe()
        if self.mode == 'remove':
            self.emit('turn', player=self.foe)
            self.cast('remove_spell', 'graveyard', target='source')
            self.cards['source']['zone'] = 'graveyard'
            self.observe()
        self.travel_draw(draws=int(self.mode == 'remove'))

    def taigam_upkeep(self):
        self.travel_draw(draws=1)
        self.advance()
        prefix = self.libraries[self.owner][:3]
        chosen = 'Forest' if self.mode != 'short_library' else 'Island'
        self.emit('choose_zone', player=self.owner, value=prefix.index(chosen))
        self.upkeep()
        self.emit('assert_choices_consumed')
        for name in prefix:
            self.take(self.owner, name, 'hand' if name == chosen else 'graveyard')
        self.observe()
        self.draw_phase()
        self.observe()

    def taigam_shrink(self):
        self.emit('turn', player=self.owner)
        self.pool('set_mana', self.owner, (0, 1, 0, 0, 0, 0))
        self.emit('refresh')
        self.emit('assert_card', 'source', prop='sick', value=0)
        self.emit('can_activate', 'source', value=1)
        if self.mode == 'zero_eligible':
            self.pool('assert_mana', self.owner, (0, 1, 0, 0, 0, 0))
            self.pool('set_mana', self.owner, ZERO)
            self.observe()
            return
        count = 1 if self.mode == 'shrink_one' else 2
        self.emit('choose_target', 'victim', player=self.owner)
        for name in ('Island', 'Plains')[:count]:
            self.emit('choose_zone_card', player=self.owner, value=self.rows[name]['csvid'])
            entry, = [c for c in self.cards.values() if (c['player'], c['name'], c['zone'])
                      == (self.owner, name, 'graveyard')]
            entry['zone'] = 'exile'
        if count == 1:
            self.emit('choose_zone', player=self.owner, value=-1)
        self.emit('activate', 'source', value=1)
        self.emit('assert_unused_choices', value=0)
        self.emit('assert_stack', value=1)
        self.emit('assert_card', 'source', prop='tapped', value=1)
        self.pool('assert_mana', self.owner, ZERO)
        self.emit('resolve')
        self.emit('assert_choices_consumed')
        self.observe()
        for prop in ('power', 'toughness'):
            self.emit('assert_card', 'victim', prop=prop, value=3 - count)
        self.emit('cleanup')
        self.observe()
        for prop in ('power', 'toughness'):
            self.emit('assert_card', 'victim', prop=prop, value=3)

    def build(self):
        self.emit('turn', player=self.owner)
        for player in (0, 1):
            self.pool('set_mana', player, ZERO)
        if self.source == POSSESSION and self.mode == 'hexproof':
            cost = cost_from_oracle(self.oracle[self.row['name']])
            self.pool('set_mana', self.owner, cost)
            self.emit('refresh')
            self.emit('can_cast', 'source', value=0)
            self.pool('assert_mana', self.owner, cost)
            self.pool('set_mana', self.owner, ZERO)
            self.observe()
            return
        self.cast('source', 'battlefield')
        self.observe()
        if self.source == NECRO:
            self.necro()
        elif self.source == POSSESSION:
            self.possessed_turns()
        else:
            self.taigam_upkeep()
            if self.mode.startswith('shrink') or self.mode == 'zero_eligible':
                self.taigam_shrink()


FACE_DOWN_GAP = ('The bounded activation observes library removal, no public face-up exile, and delayed hand receipt. '
                 'The existing public exile observer excludes face-down exile records.')


def expand(name, csvid, declaration, shape, selected, rows, oracle):
    validate_variants(shape, selected)
    validate_witnesses(oracle, rows)
    if profiles()['sources'][name]['registry']['csvid'] != csvid:
        raise ValueError('exact registered draw replacement CSVID required')
    cases = []
    for variant in selected:
        mode, owner = variant.rsplit('_', 1)
        world = Game if csvid in ORIGINAL_IDS else AddedGame
        game = world(rows[name], int(owner), mode, oracle, rows)
        game.build()
        gap = FACE_DOWN_GAP if csvid == NECRO and mode.startswith('life_') else GAPS[csvid]
        cases.append(dict(
            name=f'DrawStepReplacementAndSearchRecipe_{csvid}_{variant}',
            card=name, fixture='empty_main', seed=2528, setup=game.setup, steps=game.steps,
            _recipe=dict(family=FAMILY, version=2, variant=variant, params=declaration['params'],
                         oracle_text=declaration['oracle_text'], untested_components=[gap]),
        ))
    return cases
