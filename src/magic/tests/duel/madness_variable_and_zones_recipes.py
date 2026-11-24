"""Actual Madness X, permanent control and revealed-card movement."""

from collections import Counter
from functools import lru_cache
import json
from pathlib import Path

from counter_condition_recurrence_recipes import exact

FAMILY = 'madness_variable_and_zones'
PROFILE = Path(__file__).with_name('data') / 'madness_variable_and_zones_wave8_profiles.json'
ZERO = (0,) * 6
GOBLIN = 'Rummaging Goblin'
ZOMBIE = 'Zombie'
DAMAGE, TOKENS, CONTROL, EPISODE = (15746, 15823, 16021, 8642)
MADNESS = ' (If you discard this card, discard it into exile. When you do, cast it for its madness cost or put it into your graveyard.)'
ZONE = {'hand': 0, 'battlefield': 1, 'library': 2, 'graveyard': 3, 'exile': 4}
WITNESSES = {GOBLIN, ZOMBIE, 'Grizzly Bears', 'Hill Giant', 'Forest', 'Swamp', 'Mountain', 'Plains', 'Island', 'Counterspell'}
GAP = ('The discard callback casts the hand instance with an exile marker and resolves it inside discard. '
       'The legacy exposes no temporary exile residency or separate Madness trigger response window.')
AVACYN_GAP = ('Player and creature allocation is observable. Casting-time damage division and a '
              'planeswalker allocation are outside this recipe.')
EPISODE_GAP = ('The headless adapter skips the separate hand display. The actual hand/top selection '
               'and ordered library change are observable.')
RULES = {
    DAMAGE: ('{1}{R}', '{X}{R}', "Avacyn's Judgment deals 2 damage divided as you choose among any number of targets. If this spell's madness cost was paid, it deals X damage divided as you choose among those permanents and/or players instead."),
    TOKENS: ('{3}{B}{B}', '{X}{B}{B}', "Create three tapped 2/2 black Zombie creature tokens and you gain 3 life. If this spell's madness cost was paid, instead create X of those tokens and you gain X life."),
    CONTROL: ('{2}{U}{U}', '{X}{U}{U}', "Gain control of target creature if its toughness is 2 or less. If this spell's madness cost was paid, instead gain control of that creature if its toughness is X or less."),
    EPISODE: ('{1}{B}{B}', '{1}{B}', 'Target player reveals their hand and the top card of their library. You choose a card revealed this way. That player puts the chosen card on the bottom of their library.'),
}
# A fixed integer is the actual declared X. None means no X is part of this cast.
VARIANTS = {
    DAMAGE: (('normal_split', None), ('paid_split', 3), ('paid_zero', 0)),
    TOKENS: (('normal_three', None), ('paid_four', 4), ('paid_one', 1), ('paid_zero', 0)),
    CONTROL: (('normal_two', None), ('normal_three', None), ('paid_three', 3), ('paid_two', 2), ('paid_zero', 0)),
    EPISODE: (('normal_top', None), ('normal_hand', None), ('paid_top', None)),
}


@lru_cache(maxsize=1)
def profiles():
    return json.loads(PROFILE.read_bytes())


def classify(record, csvid):
    from recipes import cost_from_oracle

    if (csvid not in RULES or record.get('layout') != 'normal'
            or record.get('selected_face') or record.get('card_faces')):
        raise ValueError('whole normal Madness spell required')
    normal, alternative, effect = RULES[csvid]
    reminder = 'Madness ' + alternative + MADNESS
    lines = (effect, reminder) if csvid == EPISODE else (reminder, effect)
    if (record.get('oracle_text') != '\n'.join(lines) or record.get('keywords') != ['Madness']
            or record.get('type_line') != 'Sorcery' or record.get('mana_cost') != normal):
        raise ValueError('complete printed Madness, payment and effect required')
    return dict(csvid=csvid, normal=cost_from_oracle(record),
                madness_fixed=cost_from_oracle({'mana_cost': alternative.replace('{X}', '')}))


def modes(csvid):
    optional = () if csvid == EPISODE else (('declined', None),)
    return (*VARIANTS[csvid], *optional, ('short', None), ('normal_countered', None))


def variants(shape):
    return [f'{mode}_{seat}' for mode, _ in modes(shape['csvid']) for seat in (0, 1)]


def validate_variants(shape, selected):
    if (type(selected) is not list or not selected or any(type(v) is not str for v in selected)
            or len(set(selected)) != len(selected) or set(selected) - set(variants(shape))):
        raise ValueError('distinct supported Madness branches required')


def spec(declaration, record):
    pin = profiles()['sources'].get(record.get('name'))
    if not pin or not exact(pin['oracle'], record):
        raise ValueError('exact whole Madness spell required')
    shape = classify(record, pin['registry']['csvid'])
    if (declaration.get('family') != FAMILY or type(declaration.get('version')) is not int
            or declaration['version'] != 2 or declaration.get('oracle_text') != record['oracle_text']
            or not exact(declaration.get('params'), shape)):
        raise ValueError('whole versioned Madness declaration required')
    validate_variants(shape, declaration.get('variants'))
    return dict(shape, family=FAMILY)


def validate_witnesses(oracle, rows):
    pins = profiles()
    if set(pins) != {'sources', 'witnesses'} or set(pins['witnesses']) != WITNESSES:
        raise ValueError('complete independent Madness witnesses required')
    for group, entries in pins.items():
        for name, pin in entries.items():
            row = rows.get(name, {})
            if (not exact(oracle.get(name), pin['oracle']) or not exact(row, pin['registry'])
                    or row.get('kind') != 'card' or row.get('handler') in (None, '0', 'NULL')):
                raise ValueError('exact Madness card or witness changed: ' + name)
            if group == 'sources':
                if row['handler'] == 'card_dummy':
                    raise ValueError('implemented Madness primary required')
                classify(oracle[name], row['csvid'])
    if oracle[GOBLIN]['oracle_text'] != '{T}, Discard a card: Draw a card.':
        raise ValueError('real tap, discard and draw cost witness required')
    if rows[ZOMBIE]['csvid'] != 18860 or rows[ZOMBIE]['handler'] != 'manalinkeh_card_zombie':
        raise ValueError('exact generated Zombie identity required')
    if (oracle['Grizzly Bears']['toughness'], oracle['Hill Giant']['toughness']) != ('2', '3'):
        raise ValueError('independent two and three toughness witnesses required')


class Spell:
    def __init__(self, shape, record, actor, mode, x, rows, oracle):
        self.csvid, self.actor, self.foe = shape['csvid'], actor, 1 - actor
        self.mode, self.x, self.rows, self.oracle = mode, x, rows, oracle
        self.normal = mode.startswith('normal_')
        self.paid = mode.startswith('paid_')
        self.normal_cost, self.madness_fixed = shape['normal'], shape['madness_fixed']
        self.setup, self.steps, self.cards = [], [], {}
        self.life = [20, 20]
        self.library = {actor: ['Mountain', 'Plains', 'Island'], self.foe: ['Island', 'Plains', 'Mountain']}
        self.add('source', actor, record['name'], 'hand')
        if mode == 'normal_countered':
            self.add('counter', self.foe, 'Counterspell', 'hand')
        elif not self.normal:
            self.add('looter', actor, GOBLIN, 'hand')
        for player, names in self.library.items():
            for i, name in enumerate(names):
                self.add(f'd_{player}_{i}', player, name, 'library')
        if self.csvid in (DAMAGE, CONTROL):
            small = self.csvid == DAMAGE or mode == 'normal_two'
            self.add('victim', self.foe, 'Grizzly Bears' if small else 'Hill Giant', 'battlefield')
        if self.csvid == EPISODE:
            self.add('revealed_card', self.foe, 'Forest', 'hand')
            self.add('kept_hand', self.foe, 'Swamp', 'hand')
        self.tokens = 0

    def add(self, ref, player, name, zone):
        from recipes import card

        self.setup.append(card(name, ref, player, zone))
        self.cards[ref] = dict(name=name, player=player, zone=zone)

    def emit(self, op, ref='', other='', prop='', *, player=0, value=0, zone=0, index=0, color=0):
        from recipes import step

        if op == 'untap_step':
            self.steps.append(step(op, player=player))
            return
        fields = dict(player=player, value=value, index=index, color=color)
        if ref:
            fields['ref'] = ref
        if other:
            fields['other'] = other
        if prop:
            fields['property'] = prop
        if op in ('assert_zone', 'assert_zone_card', 'assert_zone_multiplicity'):
            fields['zone'] = next(name for name, number in ZONE.items() if number == zone)
        self.steps.append(step(op, **fields))

    def observe(self, ref, prop, value):
        self.emit('assert_card', ref, prop=prop, value=value)

    def mana(self, op, player, pool):
        for color, value in enumerate(pool):
            self.emit(op, player=player, color=color, value=value)

    def receipts(self, stack=0):
        self.emit('assert_stack', value=stack)
        names = sorted({c['name'] for c in self.cards.values()})
        for player in (0, 1):
            self.emit('assert_life', player=player, value=self.life[player])
            for zone, index in ZONE.items():
                contents = self.library[player] if zone == 'library' else [
                    c['name'] for c in self.cards.values() if c['player'] == player and c['zone'] == zone
                ]
                self.emit('assert_zone', player=player, zone=index, value=len(contents))
                occurrences = Counter(contents)
                for name in names:
                    self.emit('assert_zone_multiplicity', player=player, zone=index,
                              index=self.rows[name]['csvid'], value=occurrences[name])
                if zone == 'library':
                    for i, name in enumerate(contents):
                        self.emit('assert_zone_card', player=player, zone=index, index=i,
                                  value=self.rows[name]['csvid'])

    def cast_pending(self, ref, cost):
        self.mana('set_mana', self.actor, cost)
        self.emit('refresh')
        self.emit('cast', ref, value=1)
        self.emit('assert_choices_consumed')
        self.emit('assert_stack', value=1)
        self.mana('assert_mana', self.actor, [0] * 6)

    def cast_paid(self, ref, cost):
        self.cast_pending(ref, cost)
        self.emit('resolve')
        self.emit('assert_choices_consumed')
        self.emit('refresh')
        self.cards[ref]['zone'] = 'battlefield'

    def age(self):
        for player in (self.foe, self.actor):
            self.emit('advance_turn')
            self.emit('untap_step', player=player)
            self.emit('turn', player=player)

    def announce_choices(self):
        if self.csvid == CONTROL:
            self.emit('choose_target', 'victim', player=self.actor)
        elif self.csvid == EPISODE:
            self.emit('choose_player', player=self.actor, value=self.foe)

    def effect_choices(self):
        if self.csvid == DAMAGE:
            amount = 2 if self.normal else self.x
            # Two actual legal targets let a wrong normal-mode fallback finish.
            # The Oracle-correct X=0 leaves both choices unused and deals no damage.
            self.emit('choose_player', player=self.actor, value=self.foe)
            for _ in range((amount or 2) - 1):
                self.emit('choose_target', 'victim', player=self.actor)
            return 2 if amount == 0 else 0
        if self.csvid == EPISODE:
            # The legacy selector owns one top-card display and a real From hand / From deck menu.
            self.emit('choose_zone', player=self.actor, value=-1)
            hand = self.mode == 'normal_hand'
            self.emit('choose_dialog', player=self.actor, value=int(not hand))
            if hand:
                self.emit('choose_zone_card', player=self.actor, value=self.rows['Forest']['csvid'])
        return 0

    def expected_effect(self):
        if self.csvid == DAMAGE:
            amount = 2 if self.normal else self.x
            if amount:
                self.life[self.foe] -= 1
                if amount - 1 >= 2:
                    self.cards['victim']['zone'] = 'graveyard'
        elif self.csvid == TOKENS:
            self.tokens = 3 if self.normal else self.x
            self.life[self.actor] += self.tokens
            for ordinal in range(self.tokens):
                self.cards[f'z_{ordinal}'] = dict(name=ZOMBIE, zone='battlefield', player=self.actor)
        elif self.csvid == CONTROL:
            toughness = int(self.oracle[self.cards['victim']['name']]['toughness'])
            limit = 2 if self.normal else self.x
            if toughness <= limit:
                self.cards['victim']['player'] = self.actor
        else:
            library = self.library[self.foe]
            if self.mode == 'normal_hand':
                self.cards['revealed_card']['zone'] = 'library'
                library.append(self.cards['revealed_card']['name'])
            else:
                library.append(library.pop(0))

    def effect_receipts(self, stack=0):
        self.receipts(stack)
        if self.csvid == TOKENS:
            csvid = self.rows[ZOMBIE]['csvid']
            for player in (self.actor, self.foe):
                self.emit('assert_zone_multiplicity', player=player, zone=ZONE['battlefield'],
                          index=csvid, value=self.tokens if player == self.actor else 0)
            if self.tokens:
                for prop, value in (('token', 1), ('color', 2), ('subtype_Zombie', 1),
                                    ('power', 2), ('toughness', 2), ('tapped', 1)):
                    self.emit('assert_matching_permanents', prop=prop, player=self.actor,
                              index=csvid, value=value)
        elif self.csvid == DAMAGE and self.cards['victim']['zone'] == 'battlefield':
            amount = (2 if self.normal else self.x) if self.normal or self.paid else 0
            self.observe('victim', 'damage', max(0, amount - 1))

    def cast_spell(self):
        self.mana('set_mana', self.actor, self.normal_cost)
        self.emit('refresh')
        self.announce_choices()
        self.emit('cast', 'source', value=1)
        self.emit('assert_choices_consumed')
        self.emit('assert_stack', value=1)
        self.mana('assert_mana', self.actor, ZERO)
        if self.mode == 'normal_countered':
            self.counter_pending()
            return
        self.effect_choices()
        self.emit('resolve')
        self.emit('assert_unused_choices', value=0)
        self.emit('assert_choices_consumed')
        self.emit('refresh')
        self.cards['source']['zone'] = 'graveyard'
        self.expected_effect()
        self.effect_receipts()

    def counter_pending(self):
        from recipes import cost_from_oracle

        self.emit('announce_spell', 'source')
        self.mana('set_mana', self.foe, cost_from_oracle(self.oracle['Counterspell']))
        self.emit('refresh')
        self.emit('cast_response', 'counter', value=1)
        self.emit('assert_stack', value=2)
        self.emit('resolve_response')
        self.emit('assert_choices_consumed')
        self.mana('assert_mana', self.foe, ZERO)
        self.cards['source']['zone'] = self.cards['counter']['zone'] = 'graveyard'
        self.emit('refresh')
        if self.csvid == TOKENS:
            self.effect_receipts()
        else:
            self.receipts()
            if self.csvid == DAMAGE:
                self.observe('victim', 'damage', 0)

    def discard_spell(self):
        from recipes import cost_from_oracle

        self.cast_paid('looter', cost_from_oracle(self.oracle[GOBLIN]))
        self.age()
        self.observe('looter', 'in_play', 1)
        self.observe('looter', 'sick', 0)
        self.observe('looter', 'tapped', 0)
        self.receipts()
        self.emit('discard_policy', value=1)
        self.emit('present_ai_dialogs', value=1)
        paid_cost, pool = list(self.madness_fixed), list(self.madness_fixed)
        if self.x is not None:
            paid_cost[0] = self.x
            # Fund the legacy extra charge when the payment can otherwise abort.
            # The expected remainder still reflects only the printed X cost.
            needs_reserve = self.actor == 0 or self.x < self.normal_cost[0]
            reserve = max(1, self.normal_cost[0]) if needs_reserve else 1
            pool[0] = self.x + reserve
        if self.mode == 'short':
            pool = [sum(self.madness_fixed) + 1, 0, 0, 0, 0, 0]
        self.mana('set_mana', self.actor, pool)
        self.emit('refresh')
        self.emit('can_activate', 'looter', value=1)
        if self.actor == 1:
            self.emit('choose_dialog', player=self.actor, value=0)
        # If the printed paid option is missing, index 0 is the sole legitimate Decline.
        # The unused downstream choices and untouched mana/library independently expose that absence.
        self.emit('choose_dialog', player=self.actor, value=int(self.mode == 'declined'))
        unused = 0
        if self.paid:
            if self.x is not None:
                self.emit('choose_x', player=self.actor, value=self.x)
            self.announce_choices()
            unused = self.effect_choices()
        self.emit('activate', 'looter', value=1)
        self.emit('assert_unused_choices', value=unused)
        self.emit('assert_choices_consumed')
        if self.actor == 0:
            self.emit('assert_discard_choices', value=1)
        self.observe('looter', 'tapped', 1)
        self.observe('source', 'in_hand', 0)
        self.mana('assert_mana', self.actor, [p - c for p, c in zip(pool, paid_cost)] if self.paid else pool)
        self.mana('assert_mana', self.foe, ZERO)
        self.cards['source']['zone'] = 'graveyard'
        if self.paid:
            self.expected_effect()
        self.emit('refresh')
        self.effect_receipts(stack=1)
        self.emit('resolve')
        self.emit('assert_choices_consumed')
        self.emit('refresh')
        self.cards[f'd_{self.actor}_0']['zone'] = 'hand'
        del self.library[self.actor][0]
        self.effect_receipts()

    def build(self):
        self.emit('turn', player=self.actor)
        self.mana('set_mana', self.foe, ZERO)
        if self.csvid == CONTROL:
            self.observe('victim', 'toughness', int(self.oracle[self.cards['victim']['name']]['toughness']))
        if self.normal:
            self.cast_spell()
        else:
            self.discard_spell()



def expand(name, csvid, declaration, shape, selected, rows, oracle):
    validate_variants(shape, selected)
    validate_witnesses(oracle, rows)
    if profiles()['sources'][name]['registry']['csvid'] != csvid:
        raise ValueError('exact registered Madness primary CSVID required')
    cases = []
    choices = dict(modes(csvid))
    for variant in selected:
        mode, actor = variant.rsplit('_', 1)
        game = Spell(shape, oracle[name], int(actor), mode, choices[mode], rows, oracle)
        game.build()
        gaps = [] if game.normal else [GAP]
        if csvid == DAMAGE:
            gaps.append(AVACYN_GAP)
        elif csvid == EPISODE:
            gaps.append(EPISODE_GAP)
        cases.append(dict(
            name=f'MadnessVariableAndZonesRecipe_{csvid}_{variant}', card=name,
            fixture='empty_main', seed=2471, setup=game.setup, steps=game.steps,
            _recipe=dict(family=FAMILY, version=2, variant=variant, params=declaration['params'],
                         oracle_text=declaration['oracle_text'], untested_components=gaps),
        ))
    return cases
