"""Static Delirium traits and useful mana from genuine cast and combat events."""

from collections import Counter
from functools import lru_cache
import json
from pathlib import Path

from library_search_recipes import wave6_exact as exact
from recipes import card, cost_from_oracle, step


FAMILY = 'delirium_static_stats_and_mana'
PROFILE = Path(__file__).parent / 'data' / f'{FAMILY}_wave8_profiles.json'
CLAUSE = ' as long as there are four or more card types among cards in your graveyard.'
THREE = ('Mountain', 'Grizzly Bears', 'Giant Growth', 'Mountain')
FOUR = ('Mountain', 'Grizzly Bears', 'Giant Growth', "Traveler's Amulet")
ZONE = {'hand': 0, 'battlefield': 1, 'library': 2, 'graveyard': 3, 'exile': 4}
ZONE_NAMES = {number: name for name, number in ZONE.items()}

SHAPES = {
    15782: ('deathtouch', 0, 0, '{T}: Add {B} or {G}.\n', 'This creature has deathtouch'),
    15892: ('stats', 3, 0, '', 'This creature gets +3/+0'),
    15894: ('flying', 0, 0, '', 'This creature has flying'),
    16033: ('trample', 1, 1, '', 'This creature gets +1/+1 and has trample'),
    16108: ('defender', 0, 0, 'Defender, flying\n', "This creature can attack as though it didn't have defender"),
    16113: ('deathtouch', 2, 2, 'Deathtouch\n', 'This creature gets +2/+2'),
}
MODES = ('three_duplicates', 'four_types', 'opponent_only')
EXTRA = {
    15782: ('mana_black', 'mana_green'),
    15892: ('crossing',),
    16108: ('defends_flying',),
}
NEW_MODES = ('crossing', 'lose_threshold', 'countered', 'replay')
WITNESSES = set(FOUR) | {'Island', 'Plains', 'Wall of Roots', 'Ornithopter', 'Welkin Tern',
                         'Disenchant', 'Fatal Push', "Tormod's Crypt", 'Counterspell'}
STATIC_DUMMY_WITNESSES = {'Grizzly Bears', 'Ornithopter'}


@lru_cache(maxsize=1)
def profiles():
    return json.loads(PROFILE.read_bytes())


def classify(record, csvid):
    keyword, power, toughness, prelude, clause = SHAPES[csvid]
    keywords = {'Delirium'} | {16108: {'Defender', 'Flying'}, 16113: {'Deathtouch'}}.get(csvid, set())
    if (record['oracle_text'] != f'{prelude}Delirium — {clause}{CLAUSE}'
            or record['layout'] != 'normal' or not record['type_line'].startswith('Creature — ')
            or set(record['keywords']) != keywords or record.get('selected_face') or record.get('card_faces')):
        raise ValueError('Exact whole ordinary Delirium creature required')
    return dict(csvid=csvid, keyword=keyword, delirium_delta=[power, toughness],
                source_pt=[int(record[k]) for k in ('power', 'toughness')],
                price=cost_from_oracle(record), clauses=record['oracle_text'].splitlines())


def variants(model):
    csvid = model['csvid']
    modes = (*MODES, *EXTRA.get(csvid, ()), *NEW_MODES)
    if csvid == 15782:
        modes += ('mana_repeat',)
    return [f'{mode}_{actor}' for mode in dict.fromkeys(modes) for actor in (0, 1)]


def validate_witnesses(oracle, registry):
    pins = profiles()
    if (set(pins) != {'sources', 'witnesses'} or set(pins['witnesses']) != WITNESSES
            or {p['registry']['csvid'] for p in pins['sources'].values()} != SHAPES.keys()):
        raise ValueError('Exact independent source and Delirium witness pins required')
    for group in pins.values():
        for name, pin in group.items():
            record, row = oracle.get(name, {}), registry.get(name, {})
            handler = row.get('handler')
            if (not exact(pin, {'oracle': record, 'registry': row}) or record.get('layout') != 'normal'
                    or row.get('kind') != 'card' or handler in (None, '0', 'NULL')
                    or handler == 'card_dummy' and name not in STATIC_DUMMY_WITNESSES):
                raise ValueError(f'Pinned Delirium source or witness changed: {name}')


def spec(declaration, record):
    pin = profiles()['sources'].get((record or {}).get('name'))
    if not pin or not exact(record, pin['oracle']):
        raise ValueError('Complete pinned Delirium source record required')
    model = classify(record, pin['registry']['csvid'])
    picked = declaration.get('variants')
    if (declaration.get('family') != FAMILY or type(declaration.get('version')) is not int
            or declaration['version'] != 2 or declaration.get('oracle_text') != record['oracle_text']
            or not exact(declaration.get('params'), model) or not isinstance(picked, list) or not picked
            or any(type(v) is not str for v in picked) or len(picked) != len(set(picked))
            or set(picked) - set(variants(model))):
        raise ValueError('Exact Delirium parameters and distinct scoped variants required')
    return dict(model, family=FAMILY)


class DeliriumBoard:
    def graves(self):
        result = [(), ()]
        result[self.actor] = FOUR if self.active else THREE
        if self.mode == 'opponent_only':
            result[self.foe] = FOUR
        return result

    def add(self, ref, player, name, zone):
        self.setup.append(card(name, ref, player, zone))
        self.cards[ref] = dict(player=player, name=name, zone=zone)

    def emit(self, op, ref='', other='', prop='', *, zone=None, **fields):
        row = step(op, **fields)
        for key, field in [('ref', ref), ('other', other), ('property', prop)]:
            if field:
                row[key] = field
        if zone is not None:
            row['zone'] = ZONE_NAMES[zone]
        self.steps.append(row)

    def observe(self, ref, prop, value):
        self.emit('assert_card', ref, prop=prop, value=value)

    def mana(self, op, player, pool):
        for color, amount in enumerate(pool):
            self.emit(op, player=player, color=color, value=amount)

    def cast(self, ref, destination, target=None):
        payment = cost_from_oracle(self.oracle[self.cards[ref]['name']])
        self.mana('set_mana', self.actor, payment)
        self.emit('refresh')
        if target:
            self.emit('choose_target', target, player=self.actor)
        self.emit('cast', ref, value=1)
        self.emit('assert_choices_consumed')
        self.emit('assert_stack', value=1)
        self.mana('assert_mana', self.actor, [0] * 6)
        self.emit('resolve')
        self.emit('assert_choices_consumed')
        self.emit('assert_stack')
        self.cards[ref]['zone'] = destination
        self.emit('refresh')

    def turn_ready(self):
        self.emit('turn', player=self.actor)
        self.mana('set_mana', self.foe, [0] * 6)
        self.cast('source', 'battlefield')
        self.observe('source', 'in_play', 1)
        self.observe('source', 'sick', 1)
        for player in (self.foe, self.actor):
            self.emit('advance_turn')
            self.emit('untap_step', player=player)
            self.emit('turn', player=player)
        self.observe('source', 'sick', 0)

    def stats(self):
        record = self.oracle[self.name]
        return (int(record['power']) + self.power_delta * int(self.active),
                int(record['toughness']) + self.toughness_delta * int(self.active))

    def state(self):
        self.emit('refresh')
        if self.cards['source']['zone'] == 'battlefield':
            self.observe('source', 'in_play', 1)
            for prop, expected in zip(('power', 'toughness'), self.stats()):
                self.observe('source', prop, expected)
        for player in (0, 1):
            self.emit('assert_life', player=player, value=self.life[player])
            for zone in ('graveyard', 'battlefield', 'hand'):
                counts = Counter(c['name'] for c in self.cards.values()
                                 if c['player'] == player and c['zone'] == zone)
                self.emit('assert_zone', player=player, zone=ZONE[zone], value=sum(counts.values()))
                for name in sorted({c['name'] for c in self.cards.values()}):
                    self.emit('assert_zone_multiplicity', player=player, zone=ZONE[zone],
                              index=self.registry[name]['csvid'], value=counts[name])

    def cross_threshold(self):
        self.observe('amulet', 'in_play', 1)
        self.cast('disenchant', 'graveyard', target='amulet')
        self.cards['amulet']['zone'] = 'graveyard'
        self.active = True
        self.state()



class StaticDuel(DeliriumBoard):
    def __init__(self, name, csvid, actor, mode, rows, oracle):
        self.name, self.csvid, self.actor, self.foe, self.mode = name, csvid, actor, 1 - actor, mode
        self.registry, self.oracle = rows, oracle
        self.keyword, self.power_delta, self.toughness_delta, _, _ = SHAPES[csvid]
        self.active = mode in ('four_types', 'mana_green', 'lose_threshold', 'replay')
        self.setup, self.steps, self.cards = [], [], {}
        self.life = [20, 20]
        self.boost = 0
        self.snapshot = None
        self.add('source', actor, name, 'hand')
        for player, cards in enumerate(self.graves()):
            for i, grave_name in enumerate(cards):
                self.add(f'g_{player}_{i}', player, grave_name, 'graveyard')
        for player in (0, 1):
            for i, name in enumerate(('Island', 'Plains')):
                self.add(f'd_{player}_{i}', player, name, 'library')
        if self.keyword in ('deathtouch', 'flying') and mode not in ('mana_green', 'countered'):
            self.add('blocker', self.foe, 'Wall of Roots', 'battlefield')
        if self.keyword == 'trample' and mode != 'countered':
            self.add('blocker', self.foe, 'Ornithopter', 'battlefield')
        if mode == 'defends_flying':
            self.add('flyer', self.foe, 'Welkin Tern', 'battlefield')
        if mode == 'crossing':
            self.add('amulet', actor, "Traveler's Amulet", 'battlefield')
            self.add('disenchant', actor, 'Disenchant', 'hand')
        if mode in ('mana_black', 'mana_green'):
            self.add('spend', actor, 'Fatal Push' if mode == 'mana_black' else 'Giant Growth', 'hand')
        if mode == 'mana_repeat':
            self.add('spend', actor, 'Fatal Push', 'hand')
            self.add('again', actor, 'Giant Growth', 'hand')
        if mode in ('lose_threshold', 'replay'):
            # Human selection can legally target either graveyard, including its own.
            self.add('crypt', 0, "Tormod's Crypt", 'battlefield')
        if mode == 'countered':
            self.add('counter', self.foe, 'Counterspell', 'hand')

    def turn_ready(self):
        if not self.mode.startswith('mana_'):
            super().turn_ready()
            return
        self.emit('turn', player=self.actor)
        self.mana('set_mana', self.foe, [0] * 6)
        self.cast('source', 'battlefield')
        self.observe('source', 'sick', 1)
        self.emit('can_activate', 'source', value=0)
        self.emit('activate', 'source', value=0)
        self.mana('assert_mana', self.actor, [0] * 6)
        self.observe('source', 'tapped', 0)
        for player in (self.foe, self.actor):
            self.advance_to(player)
        self.observe('source', 'sick', 0)

    def advance_to(self, player):
        self.emit('advance_turn')
        self.emit('untap_step', player=player)
        self.emit('turn', player=player)

    def stats(self):
        power, toughness = super().stats()
        return power + self.boost, toughness + self.boost

    def combat(self):
        power, _ = self.stats()
        allowed = self.active or self.keyword != 'defender'
        self.emit('begin_attackers')
        self.emit('attack', 'source', value=int(allowed))
        self.observe('source', 'attacking', int(allowed))
        self.observe('source', 'tapped', int(allowed))
        self.emit('finish_attackers')
        self.emit('begin_blockers')
        blocked = 'blocker' in self.cards and not (self.keyword == 'flying' and self.active)
        if 'blocker' in self.cards:
            self.emit('block', 'blocker', 'source', value=int(blocked))
        self.emit('advance_combat')
        self.observe('source', 'blocked', int(blocked))
        self.emit('combat_damage')
        self.emit('resolve')
        self.emit('end_combat')
        self.emit('assert_choices_consumed')
        if not allowed:
            return
        if blocked:
            tough = int(self.oracle[self.cards['blocker']['name']]['toughness'])
            if self.keyword == 'trample' and self.active:
                self.life[self.foe] -= max(0, power - tough)
            deathtouch = self.keyword == 'deathtouch' and (self.active or self.csvid == 16113)
            if power >= tough or deathtouch and power:
                self.cards['blocker']['zone'] = 'graveyard'
            else:
                self.observe('blocker', 'damage', power)
            self.observe('source', 'damage', 0)
        else:
            self.life[self.foe] -= power

    def defend_flyer(self):
        self.advance_to(self.foe)
        self.emit('begin_attackers')
        self.emit('attack', 'flyer', value=1)
        self.emit('finish_attackers')
        self.emit('begin_blockers')
        self.emit('block', 'source', 'flyer', value=1)
        self.emit('advance_combat')
        self.observe('flyer', 'blocked', 1)
        self.emit('combat_damage')
        self.emit('resolve')
        self.emit('end_combat')
        self.emit('assert_choices_consumed')
        self.cards['flyer']['zone'] = 'graveyard'
        self.observe('source', 'damage', 2)
        self.observe('source', 'tapped', 0)

    def produce_and_spend(self, *, black, spell_ref='spend'):
        color = 1 if black else 3
        resource = [int(i == color) for i in range(6)]
        self.emit('can_activate', 'source', value=1)
        self.emit('choose_color', player=self.actor, value=color)
        self.emit('activate', 'source', value=1)
        self.emit('assert_choices_consumed')
        self.emit('assert_stack', value=0)
        self.mana('assert_mana', self.actor, resource)
        self.mana('assert_mana', self.foe, [0] * 6)
        self.observe('source', 'tapped', 1)
        self.emit('can_activate', 'source', value=0)
        self.emit('activate', 'source', value=0)
        # The real earned mana is the only funding for this matching spell.
        spell = self.oracle[self.cards[spell_ref]['name']]
        assert cost_from_oracle(spell) == resource
        self.emit('choose_target', 'blocker' if black else 'source', player=self.actor)
        self.emit('cast', spell_ref, value=1)
        self.emit('assert_choices_consumed')
        self.emit('assert_stack', value=1)
        self.mana('assert_mana', self.actor, [0] * 6)
        self.emit('resolve')
        self.emit('assert_choices_consumed')
        self.emit('assert_stack', value=0)
        self.cards[spell_ref]['zone'] = 'graveyard'
        if black:
            self.cards['blocker']['zone'] = 'graveyard'
        else:
            self.boost = 3
        self.state()
        if not black:
            self.advance_to(self.foe)
            self.boost = 0
            self.state()

    def lose_threshold(self):
        crypt = self.cards['crypt']
        self.emit('choose_player', player=crypt['player'], value=self.actor)
        self.emit('activate', 'crypt', value=1)
        self.emit('assert_choices_consumed')
        self.emit('assert_stack', value=1)
        crypt['zone'] = 'graveyard'
        counts = Counter(c['name'] for c in self.cards.values()
                         if c['player'] == self.actor and c['zone'] == 'graveyard')
        self.state()
        self.emit('resolve')
        self.emit('assert_choices_consumed')
        self.emit('assert_stack', value=0)
        for obj in self.cards.values():
            if obj['player'] == self.actor and obj['zone'] == 'graveyard':
                obj['zone'] = 'exile'
        self.active = False
        self.emit('assert_zone', player=self.actor, zone=ZONE['exile'], value=sum(counts.values()))
        for name, value in sorted(counts.items()):
            self.emit('assert_zone_multiplicity', player=self.actor, zone=ZONE['exile'],
                      index=self.registry[name]['csvid'], value=value)
        self.state()

    def countered(self):
        self.emit('turn', player=self.actor)
        self.mana('set_mana', self.foe, [0] * 6)
        self.mana('set_mana', self.actor, cost_from_oracle(self.oracle[self.name]))
        self.emit('refresh')
        self.emit('cast', 'source', value=1)
        self.emit('assert_choices_consumed')
        self.emit('assert_stack', value=1)
        self.mana('assert_mana', self.actor, [0] * 6)
        self.observe('source', 'in_play', 0)
        self.emit('announce_spell', 'source')
        self.mana('set_mana', self.foe, cost_from_oracle(self.oracle['Counterspell']))
        self.emit('refresh')
        self.emit('cast_response', 'counter', value=1)
        self.emit('assert_choices_consumed')
        self.emit('assert_stack', value=2)
        self.mana('assert_mana', self.foe, [0] * 6)
        self.emit('resolve_response')
        self.emit('assert_stack', value=0)
        self.emit('assert_choices_consumed')
        for ref in ('source', 'counter'):
            self.cards[ref]['zone'] = 'graveyard'
        self.state()

    def build(self):
        if self.mode == 'countered':
            self.countered()
            return
        self.turn_ready()
        self.state()
        if self.mode in ('mana_black', 'mana_green'):
            self.produce_and_spend(black=self.mode == 'mana_black')
        elif self.mode == 'mana_repeat':
            self.produce_and_spend(black=True)
            self.advance_to(self.foe)
            self.advance_to(self.actor)
            self.produce_and_spend(black=False, spell_ref='again')
        elif self.mode == 'defends_flying':
            self.defend_flyer()
        else:
            if self.mode == 'crossing':
                self.cross_threshold()
            if self.mode in ('lose_threshold', 'replay'):
                if self.mode == 'replay':
                    self.snapshot = len(self.steps)
                self.lose_threshold()
            self.combat()
        self.state()


def expand(name, csvid, declaration, model, selected, registry, oracle):
    validate_witnesses(oracle, registry)
    if registry[name]['csvid'] != csvid or model['csvid'] != csvid or set(selected) - set(variants(model)):
        raise ValueError('Exact Delirium source and scoped variants required')
    cases = []
    for variant in selected:
        mode, seat = variant.rsplit('_', 1)
        game = StaticDuel(name, csvid, int(seat), mode, registry, oracle)
        game.build()
        actions = game.steps
        if game.snapshot is not None:
            cut = game.snapshot
            suffix = actions[cut:]
            actions = actions[:cut] + [step('capture')] + suffix + [
                step('record_state'), step('restore'), step('ai_roundtrip')
            ] + suffix + [step('assert_recorded_state')]
        metadata = dict(family=FAMILY, version=2, variant=variant, params=declaration['params'],
                        oracle_text=declaration['oracle_text'], covered_clauses=model['clauses'],
                        untested_components=[])
        cases.append(dict(name=f'DeliriumStaticStatsAndManaRecipe_{csvid}_{variant}', card=name, fixture='empty_main',
                          seed=2370, setup=game.setup, steps=actions, _recipe=metadata))
    return cases

