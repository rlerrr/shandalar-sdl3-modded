"""Paid cast-color triggers with independent rewards and observed stack color."""

from collections import Counter
from functools import lru_cache
import json
from pathlib import Path
import re

from cast_trigger_case_support import CastTriggerBoard
from defensive_creature_recipes import reviewed_payment
from library_search_recipes import wave6_exact as exact


FAMILY = 'cast_spell_color_riders'
PROFILE = Path(__file__).parent / 'data' / f'{FAMILY}_wave8_profiles.json'
COLOR = {'white': 'W', 'blue': 'U', 'black': 'B', 'red': 'R', 'green': 'G'}
HYBRID = re.compile(r'\{([WUBRG])/([WUBRG])\}')
CAST_COUNTER = re.compile(r'^Whenever you cast a (white|blue|black|red|green) spell, remove a -1/-1 counter from this creature\.$')
HATCH = 'This creature enters with four -1/-1 counters on it.'
MONO = {'B': 'Dark Ritual', 'U': 'Think Twice', 'G': 'Fog', 'R': 'Lightning Bolt', 'W': 'Holy Day'}
PAIR = {frozenset('RW'): 'Lightning Helix', frozenset('UB'): 'Dimir Infiltrator', frozenset('WB'): 'Vizkopa Guildmage',
        frozenset('BG'): 'Shambling Shell', frozenset('UR'): 'Nivix Guildmage', frozenset('GU'): "Kiora's Follower"}
COLORLESS = "Kozilek's Sentinel"
HATCH_RIDERS = {
    ('First strike', HATCH): ('first_strike', {'First strike'}),
    (HATCH, 'Wither (This deals damage to creatures in the form of -1/-1 counters.)'): ('wither', {'Wither'}),
    (HATCH, "{U/R}: Target creature can't block this creature this turn."): ('restrict_block', set()),
    (HATCH, "{G/U}: This creature gains shroud until end of turn. (It can't be the target of spells or abilities.)"): ('shroud', set()),
    ('Lifelink', HATCH): ('lifelink', {'Lifelink'}),
}
HATCH_RIDER_MODES = {
    'wither': ('wither_combat',),
    'lifelink': ('lifelink_combat',),
    'restrict_block': ('pay_first', 'pay_second', 'activation_expiry', 'activation_denied'),
    'shroud': ('pay_first', 'pay_second', 'activation_expiry', 'activation_denied'),
}
RIDER = {
    ('Other red creatures you control get +1/+1.', 'Other white creatures you control get +1/+1.',
     'Whenever you cast a red spell, this creature deals 3 damage to target player or planeswalker.',
     'Whenever you cast a white spell, you gain 3 life.'): ('balefire', 'RW'),
    ("Whenever you cast a blue spell, exile the top card of target player's library.",
     "Whenever you cast a black spell, exile the top card of target player's library.",
     "Your opponents can't cast spells with the same name as a card exiled with Circu."): ('circu', 'UB'),
    ('Other white creatures you control get +1/+1.', 'Other black creatures you control get +1/+1.',
     'Whenever you cast a white spell, you may tap target creature.',
     "Whenever you cast a black spell, you may destroy target creature if it's tapped."): ('deathbringer', 'WB'),
    ("Whenever you cast a red spell, target creature can't block this turn.",): ('jester', 'R'),
    ('Whenever you cast a blue spell, target creature gains flying until end of turn.',
     'Whenever you cast a red spell, Tibor and Lumia deals 1 damage to each creature without flying.'): ('tibor', 'UR'),
}
RIDER_MODES = {'hatchling': ('first_strike',), 'balefire': ('planeswalker',),
               'circu': ('own_library', 'linked', 'unlinked'),
               'deathbringer': ('untapped_target',), 'jester': ('expires',),
               'tibor': ('flying', 'flying_expiry')}
WITNESSES = {*MONO.values(), *PAIR.values(), COLORLESS, 'Goblin Tinkerer', 'Mother of Runes',
             'Boros Guildmage', 'Viscera Seer', 'Boggart Ram-Gang', 'Forest', 'Island', 'Shock', 'Unsummon',
             'Jace Beleren', 'Elvish Mystic', 'Krosan Tusker', 'Welkin Tern', 'Goblin Chariot', 'Giant Growth'}


@lru_cache(maxsize=1)
def profiles():
    return json.loads(PROFILE.read_bytes())


def classify(record):
    if (record.get('layout') != 'normal' or record.get('selected_face') or record.get('card_faces')
            or not record.get('type_line', '').startswith(('Creature — ', 'Legendary Creature — '))):
        raise ValueError('Whole normal Oracle creature required')
    lines = record['oracle_text'].splitlines()
    rider = RIDER.get(tuple(lines))
    hatch_rider = None
    if rider:
        effect, colors = rider
        if record['keywords']:
            raise ValueError('Unreviewed spell-color creature keywords')
    else:
        triggers = [CAST_COUNTER.fullmatch(line) for line in lines[-2:]]
        hatch = HATCH_RIDERS.get(tuple(lines[:-2]))
        if not hatch or set(record['keywords']) != hatch[1] or len(triggers) != 2 or not all(triggers):
            raise ValueError('Exact whole cast-color reward or counter-ladder creature required')
        hatch_rider = hatch[0] if hatch[0] != 'first_strike' else None
        effect, colors = 'hatchling', ''.join(COLOR[trigger[1]] for trigger in triggers)
    if len(set(colors)) != len(colors):
        raise ValueError('Each rewarded color must have one independent clause')
    hybrid = HYBRID.findall(record['mana_cost'])
    if hybrid and any(set(pair) != set(colors) for pair in hybrid):
        raise ValueError('Hybrid payment alternatives must be printed relevant colors')
    default = {'payment_colors': [pair[0] for pair in hybrid]} if hybrid else {}
    alternative = {'payment_colors': [pair[1] for pair in hybrid]} if hybrid else {}
    price, _ = reviewed_payment(record, default)
    other_price, _ = reviewed_payment(record, alternative)
    shape = dict(effect=effect, colors=list(colors), price=price, other_price=other_price,
                 hybrid=bool(hybrid), base=[int(record['power']), int(record['toughness'])], clauses=lines)
    if hatch_rider:
        shape['hatch_rider'] = hatch_rider
    return shape


def variants(model):
    modes = ['first', 'opponent', 'unmatched', 'colorless', 'replay']
    if len(model['colors']) > 1:
        modes += ['second', 'both']
    riders = HATCH_RIDER_MODES[model['hatch_rider']] if 'hatch_rider' in model else RIDER_MODES[model['effect']]
    return [f'{mode}_{seat}' for mode in (*modes, *riders) for seat in (0, 1)]


def spec(declaration, record):
    pin = profiles()['sources'].get((record or {}).get('name'))
    if not pin or not exact(record, pin['oracle']):
        raise ValueError('Complete pinned cast-color Oracle record required')
    model = classify(record)
    picked = declaration.get('variants')
    if (declaration.get('family') != FAMILY or type(declaration.get('version')) is not int
            or declaration['version'] != 2 or declaration.get('oracle_text') != record['oracle_text']
            or not exact(declaration.get('params'), model) or not isinstance(picked, list) or not picked
            or any(type(v) is not str for v in picked) or len(set(picked)) != len(picked)
            or set(picked) - set(variants(model))):
        raise ValueError('Exact cast-color reward and distinct scoped variants required')
    return dict(model, family=FAMILY)


def validate_witnesses(oracle, rows):
    pins = profiles()
    if set(pins) != {'sources', 'witnesses'} or set(pins['witnesses']) != WITNESSES:
        raise ValueError('Complete independent cast-color witness records required')
    for group in pins.values():
        for name, pin in group.items():
            record, row = oracle.get(name, {}), rows.get(name, {})
            if (not exact(pin, {'oracle': record, 'registry': row}) or record.get('layout') != 'normal'
                    or row.get('kind') != 'card' or row.get('handler') in (None, 'NULL', '0', 'card_dummy')):
                raise ValueError(f'Pinned implemented cast-color source or witness changed: {name}')


class ColorBoard(CastTriggerBoard):
    def __init__(self, name, shape, mode, owner, rows, oracle):
        self.life = [20, 20]
        self.libraries = [[], []]
        self.counters = 4 if shape['effect'] == 'hatchling' else 0
        self.snapshot = None
        super().__init__(name, shape, mode, owner, rows, oracle)

    @property
    def effect(self):
        return self.shape['effect']

    @property
    def actor(self):
        return self.foe if self.mode == 'opponent' else self.owner

    def witness(self):
        colors = self.shape['colors']
        if self.mode == 'both':
            return PAIR[frozenset(colors)]
        if self.mode == 'colorless':
            return COLORLESS
        if self.mode == 'unmatched':
            return MONO[next(color for color in 'GWURB' if color not in colors)]
        if self.mode in ('second', 'untapped_target'):
            return MONO[colors[1]]
        return MONO[colors[0]]

    def build(self):
        effect, mode = self.effect, self.mode
        self.add(self.witness(), 'trigger', self.actor, 'hand')
        if effect in ('jester', 'deathbringer'):
            self.add('Goblin Tinkerer', 'victim', self.foe,
                     tapped=effect == 'deathbringer' and mode == 'second')
        if effect == 'hatchling' and mode == 'first_strike':
            self.add('Boggart Ram-Gang', 'victim', self.foe)
        if effect in ('balefire', 'deathbringer'):
            dual = 'Boros Guildmage' if effect == 'balefire' else 'Vizkopa Guildmage'
            mono = 'Goblin Tinkerer' if effect == 'balefire' else 'Viscera Seer'
            self.add(mono, 'ally_primary', self.owner)
            self.add('Mother of Runes', 'ally_white', self.owner)
            self.add(dual, 'ally_both', self.owner)
            self.add(dual, 'foreign_both', self.foe)
        if mode == 'planeswalker':
            self.add('Jace Beleren', 'walker', self.foe, 'hand')
        if effect == 'circu':
            for seat, names in ((self.owner, ('Island', 'Forest', 'Shock')),
                                (self.foe, ('Holy Day', 'Shock', 'Forest'))):
                for index, name in enumerate(names):
                    ref = f'lib_{seat}_{index}'
                    self.add(name, ref, seat, 'library')
                    self.libraries[seat].append(ref)
            if mode in ('linked', 'unlinked'):
                self.add('Holy Day', 'forbidden', self.foe, 'hand')
            if mode == 'linked':
                self.add('Holy Day', 'own_copy', self.owner, 'hand')
            if mode == 'unlinked':
                self.add('Unsummon', 'bounce', self.foe, 'hand')

    def cast_source(self):
        if not self.shape['hybrid']:
            self.play_source()
            return
        self.emit('turn', player=self.owner)
        self.fund(self.foe, [0] * 6)
        self.fund(self.owner, self.shape['other_price' if self.mode == 'second' else 'price'])
        self.emit('cast', ref='source', value=1)
        self.emit('assert_stack', value=1)
        self.observe_color('source')
        self.pools[self.owner] = [0] * 6
        self.observe_mana()
        self.emit('assert_choices_consumed')
        self.resolve('source', 'battlefield')

    def observe_color(self, ref):
        from recipes import COLORS

        colors = self.oracle[self.cards[ref]['name']]['colors']
        self.emit('assert_card', ref=ref, property='color', value=sum(1 << COLORS[c] for c in colors))

    def resolve(self, ref, zone='graveyard'):
        self.emit('resolve')
        self.emit('refresh')
        self.emit('assert_choices_consumed')
        self.emit('assert_stack', value=0)
        self.cards[ref]['zone'] = zone

    def stats(self, ref):
        card = self.cards[ref]
        record = self.oracle[card['name']]
        count = -self.counters if ref == 'source' else 0
        if (ref != 'source' and self.effect in ('balefire', 'deathbringer') and card['player'] == self.owner
                and self.cards['source']['zone'] == 'battlefield'):
            count += len(set(record['colors']) & set(self.shape['colors']))
        return int(record['power']) + count, int(record['toughness']) + count

    def checks(self):
        quantities = Counter((c['player'], c['zone'], self.rows[c['name']]['csvid']) for c in self.cards.values())
        for who in (0, 1):
            self.emit('assert_life', player=who, value=self.life[who])
            for zone in ('battlefield', 'hand', 'library', 'graveyard', 'exile'):
                count = sum(n for (seat, area, _), n in quantities.items() if seat == who and area == zone)
                self.emit('assert_zone', player=who, zone=zone, value=count)
            for index, ref in enumerate(self.libraries[who]):
                self.emit('assert_zone_card', player=who, zone='library', index=index,
                          value=self.rows[self.cards[ref]['name']]['csvid'])
        for (who, zone, csvid), value in sorted(quantities.items()):
            self.emit('assert_zone_multiplicity', player=who, zone=zone, index=csvid, value=value)
        for ref, card in self.cards.items():
            if card['zone'] != 'battlefield':
                continue
            if 'power' not in self.oracle[card['name']]:
                continue
            for attribute, value in zip(('power', 'toughness'), self.stats(ref)):
                self.emit('assert_card', ref=ref, property=attribute, value=value)
            self.emit('assert_card', ref=ref, property='tapped', value=int(card['tapped']))
            self.emit('assert_card', ref=ref, property='damage', value=card['damage'])
            if self.effect == 'hatchling' and ref == 'source':
                self.emit('assert_card', ref=ref, property='minus1_counters', value=self.counters)
        self.observe_mana()
        self.emit('assert_stack', value=0)
        self.emit('assert_choices_consumed')

    def matching_colors(self):
        colors = self.oracle[self.cards['trigger']['name']]['colors']
        return set(colors) & set(self.shape['colors']) if self.actor == self.owner else set()

    def library_target(self):
        return self.owner if self.mode == 'own_library' else self.foe

    def choose_reward(self, board):
        hit = self.matching_colors()
        if self.effect == 'balefire' and 'R' in hit:
            if self.mode == 'planeswalker':
                board.emit('choose_target', player=self.owner, ref='walker')
            else:
                board.emit('choose_player', player=self.owner, value=self.foe)
        if self.effect == 'circu':
            for _ in hit:
                board.emit('choose_player', player=self.owner, value=self.library_target())
        if self.effect in ('deathbringer', 'jester'):
            for _ in hit:
                board.emit('choose_target', player=self.owner, ref='victim')

    def reward(self):
        hit = self.matching_colors()
        if self.effect == 'balefire':
            self.life[self.owner] += 3 * ('W' in hit)
            if 'R' in hit and self.mode == 'planeswalker':
                self.cards['walker']['zone'] = 'graveyard'
            else:
                self.life[self.foe] -= 3 * ('R' in hit)
        elif self.effect == 'hatchling':
            self.counters = max(0, self.counters - len(hit))
        elif self.effect == 'circu':
            library = self.libraries[self.library_target()]
            for _ in hit:
                self.cards[library.pop(0)]['zone'] = 'exile'
        elif self.effect == 'deathbringer':
            target = self.cards['victim']
            if 'W' in hit:
                target['tapped'] = True
            if 'B' in hit and target['tapped']:
                target['zone'] = 'graveyard'

    def witness_resolves(self):
        name = self.cards['trigger']['name']
        if name in ('Lightning Bolt', 'Lightning Helix'):
            self.life[1-self.actor] -= 3
            if name == 'Lightning Helix':
                self.life[self.actor] += 3
        elif name == 'Dark Ritual':
            self.pools[self.actor][1] += 3
        elif name == 'Think Twice':
            self.cards[self.libraries[self.actor].pop(0)]['zone'] = 'hand'

    def trigger(self):
        name = self.cards['trigger']['name']
        if name in ('Lightning Bolt', 'Lightning Helix'):
            self.emit('choose_player', player=self.actor, value=1-self.actor)
        dual_circu = self.effect == 'circu' and len(self.matching_colors()) == 2
        self.cast_trigger_spell('trigger', choices=self.choose_reward, resolve=not dual_circu)
        if dual_circu:
            # Observe the second oracle choice without turning a missing prompt into a harness failure.
            self.emit('assert_unused_choices', value=0)
            self.resolve('trigger', 'battlefield')
        self.reward()
        self.witness_resolves()
        self.checks()

    def age(self):
        if any(map(any, self.pools)):
            raise ValueError('Real combat aging requires spent pools')
        self.emit('advance_turn')
        self.emit('advance_turn')
        self.emit('assert_card', ref='source', property='sick', value=0)

    def combat(self, *, blocked):
        self.emit('begin_attackers')
        self.emit('attack', ref='source', value=1)
        self.cards['source']['tapped'] = True
        self.emit('finish_attackers')
        self.emit('begin_blockers')
        self.emit('block', ref='victim', other='source', value=int(blocked))
        self.emit('advance_combat')
        self.emit('assert_card', ref='source', property='blocked', value=int(blocked))
        self.emit('combat_damage')
        self.emit('resolve')
        self.emit('end_combat')
        if self.mode == 'unmatched':
            pass  # The paid Fog also prevents the actual combat damage.
        elif not blocked:
            self.life[self.foe] -= self.stats('source')[0]
        else:
            self.cards['victim']['zone'] = 'graveyard'
            if self.effect == 'jester':
                self.cards['source']['damage'] = 1
        self.checks()

    def linked(self):
        self.cast('forbidden', allowed=False)
        if self.mode == 'linked':
            self.cast_trigger_spell('own_copy')
        else:
            self.cast_trigger_spell('bounce', target='source')
            self.cards['source']['zone'] = 'hand'
            self.cast_trigger_spell('forbidden')
        self.checks()

    def run(self):
        self.cast_source()
        self.checks()
        if self.mode == 'planeswalker':
            self.emit('advance_turn')
            self.cast_trigger_spell('walker')
            self.emit('assert_counter', ref='walker', counter_kind='loyalty', value=int(self.oracle['Jace Beleren']['loyalty']))
            self.emit('advance_turn')
        do_combat = self.effect == 'jester' or self.mode == 'first_strike'
        if do_combat:
            self.age()
        if self.mode == 'replay':
            self.snapshot = len(self.steps)
        self.trigger()
        if self.mode in ('linked', 'unlinked'):
            self.linked()
        if do_combat:
            if self.mode == 'expires':
                self.age()
            self.combat(blocked=self.effect == 'hatchling' or self.mode in ('opponent', 'expires', 'colorless', 'unmatched'))


class FollowColorBoard(ColorBoard):
    def __init__(self, name, shape, mode, owner, rows, oracle):
        self.victim_minus = 0
        super().__init__(name, shape, mode, owner, rows, oracle)

    def witness(self):
        if self.mode == 'wither_combat':
            return 'Elvish Mystic'
        if self.mode == 'lifelink_combat':
            return 'Viscera Seer'
        return super().witness()

    def build(self):
        super().build()
        mode, rider = self.mode, self.shape.get('hatch_rider')
        if self.witness() == 'Think Twice':
            self.add('Forest', 'draw_top', self.actor, 'library')
            self.libraries[self.actor].append('draw_top')
        if mode == 'wither_combat':
            self.add('Krosan Tusker', 'victim', self.foe)
        if rider == 'restrict_block' and mode in HATCH_RIDER_MODES[rider]:
            self.add('Goblin Tinkerer', 'victim', self.foe)
            self.add('Goblin Chariot', 'fellow', self.owner)
        if rider == 'shroud' and mode in HATCH_RIDER_MODES[rider]:
            self.add('Shock', 'foe_probe', self.foe, 'hand')
            self.add('Giant Growth', 'own_probe', self.owner, 'hand')
        if self.effect == 'tibor':
            self.add('Goblin Tinkerer', 'victim', self.foe)
            self.add('Elvish Mystic', 'ground_ally', self.owner)
            self.add('Welkin Tern', 'flying_foe', self.foe)

    def stats(self, ref):
        p, t = super().stats(ref)
        if ref == 'victim':
            return p - self.victim_minus, t - self.victim_minus
        return p, t

    def checks(self):
        super().checks()
        if self.mode == 'wither_combat':
            self.emit('assert_card', ref='victim', property='minus1_counters', value=self.victim_minus)

    def choose_reward(self, board):
        super().choose_reward(board)
        if self.effect == 'tibor' and 'U' in self.matching_colors():
            board.emit('choose_target', ref='source', player=self.owner)

    def reward(self):
        super().reward()
        hit = self.matching_colors()
        if self.effect == 'tibor' and 'R' in hit:
            # The resolving trigger sees the board before the creature spell resolves.
            for ref in ('source', 'victim', 'ground_ally'):
                if ref == 'source' and 'U' in hit:
                    continue
                self.cards[ref]['damage'] += 1
                if self.cards[ref]['damage'] >= self.stats(ref)[1]:
                    self.cards[ref]['zone'] = 'graveyard'

    def combat_declarations(self, attackers, blocks):
        self.emit('begin_attackers')
        for attacker in attackers:
            self.emit('attack', ref=attacker, value=1)
            self.cards[attacker]['tapped'] = True
        self.emit('finish_attackers')
        self.emit('begin_blockers')
        for attacker, expected in blocks:
            self.emit('block', ref='victim', other=attacker, value=int(expected))
        self.emit('advance_combat')
        for attacker in attackers:
            assigned = any(target == attacker and legal for target, legal in blocks)
            self.emit('assert_card', ref=attacker, property='blocked', value=int(assigned))
        self.emit('combat_damage')
        self.emit('resolve')
        self.emit('end_combat')

    def hatchling_combat(self, mode):
        if mode == 'wither_combat':
            self.combat_declarations(('source',), (('source', True),))
            self.victim_minus = self.stats('source')[0]
            self.cards['source']['zone'] = 'graveyard'
        elif mode == 'lifelink_combat':
            self.combat_declarations(('source',), ())
            power = self.stats('source')[0]
            self.life[self.owner] += power
            self.life[self.foe] -= power
        else:
            expiry = self.mode == 'activation_expiry'
            blocks = (('source', True),) if expiry else (('source', False), ('fellow', True))
            self.combat_declarations(('source', 'fellow'), blocks)
            self.life[self.foe] -= self.stats('fellow' if expiry else 'source')[0]
            self.cards['source' if expiry else 'fellow']['damage'] = 1
            self.cards['victim']['zone'] = 'graveyard'
        self.checks()

    def shroud_receipts(self, allowed):
        for spell in ('foe_probe', 'own_probe'):
            self.emit('can_target_creature', ref=spell, other='source', value=int(allowed))

    def paid_rider(self):
        from recipes import COLORS

        mode, rider = self.mode, self.shape['hatch_rider']
        selected = 'W' if mode == 'activation_denied' else self.shape['colors'][int(mode == 'pay_second')]
        budget = [0] * 6
        budget[COLORS[selected]] = 1
        self.fund(self.owner, budget)
        if mode == 'activation_denied':
            self.emit('can_activate', ref='source', value=0)
            self.checks()
            return
        if rider == 'shroud':
            self.shroud_receipts(True)
        self.emit('can_activate', ref='source', value=1)
        if rider == 'restrict_block':
            self.emit('choose_target', ref='victim', player=self.owner)
        self.emit('activate', ref='source', value=1)
        self.emit('assert_stack', value=1)
        self.emit('assert_choices_consumed')
        self.pools[self.owner] = [0] * 6
        self.observe_mana()
        self.emit('resolve')
        self.emit('refresh')
        if rider == 'shroud':
            self.shroud_receipts(False)
        if mode == 'activation_expiry':
            self.age()
            if rider == 'shroud':
                self.shroud_receipts(True)
                self.cast_trigger_spell('foe_probe', target='source')
                self.cards['source']['damage'] = 2
        if rider == 'restrict_block':
            self.hatchling_combat(mode)
        else:
            self.checks()

    def run(self):
        self.cast_source()
        self.checks()
        mode, rider = self.mode, self.shape.get('hatch_rider')
        combat_modes = ('wither_combat', 'lifelink_combat', 'flying', 'flying_expiry')
        if mode in combat_modes or (rider == 'restrict_block' and mode in HATCH_RIDER_MODES[rider]):
            self.age()
        if mode == 'replay':
            self.snapshot = len(self.steps)
        self.trigger()
        if rider in ('restrict_block', 'shroud') and mode in HATCH_RIDER_MODES[rider]:
            self.paid_rider()
        elif mode in ('wither_combat', 'lifelink_combat'):
            self.hatchling_combat(mode)
        elif mode in ('flying', 'flying_expiry'):
            expiry = mode == 'flying_expiry'
            if expiry:
                self.age()
            self.combat_declarations(('source',), (('source', expiry),))
            if expiry:
                self.cards['victim']['zone'] = 'graveyard'
                self.cards['source']['damage'] = 1
            else:
                self.life[self.foe] -= self.stats('source')[0]
            self.checks()


def expand(name, csvid, declaration, shape, selected, rows, oracle):
    from recipes import step

    validate_witnesses(oracle, rows)
    if name not in profiles()['sources'] or rows[name]['csvid'] != csvid or set(selected) - set(variants(shape)):
        raise ValueError('Reserved full cast-color variant required')
    result = []
    for variant in selected:
        mode, owner = variant.rsplit('_', 1)
        board_type = FollowColorBoard if 'hatch_rider' in shape or shape['effect'] == 'tibor' else ColorBoard
        board = board_type(name, shape, mode, int(owner), rows, oracle)
        board.run()
        steps = board.steps
        if board.snapshot is not None:
            cut = board.snapshot
            suffix = steps[cut:]
            steps = steps[:cut] + [step('capture')] + suffix + [step('record_state'), step('ai_roundtrip'),
                    step('restore')] + suffix + [step('assert_recorded_state')]
        gaps = ['Spell color changed by another effect after announcement']
        if shape['effect'] == 'deathbringer':
            gaps.append('Independent effect decline after the target has been selected; the engine exposes only a cancellable target picker')
        meta = dict(family=FAMILY, version=2, variant=variant, params=declaration['params'],
                    oracle_text=declaration['oracle_text'], covered_clauses=shape['clauses'], untested_components=gaps)
        result.append(dict(name=f'CastSpellColorRidersRecipe_{csvid}_{variant}', card=name,
                           fixture='empty_main', seed=2226, setup=board.setup, steps=steps, _recipe=meta))
    return result
