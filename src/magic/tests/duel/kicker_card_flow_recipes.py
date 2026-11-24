"""Whole Oracle card-flow effects from actual paid or declined Kicker."""

from functools import lru_cache
import json
from pathlib import Path
import re

from counter_condition_recurrence_recipes import exact
from recipes import cost_from_oracle

FAMILY = 'kicker_card_flow'
PROFILE = Path(__file__).parent / 'data/kicker_card_flow_wave8_profiles.json'
COMMON = ('paid', 'declined', 'kicker_short', 'base_short', 'countered', 'replay')
EXTRAS = {
    'search': ('search_declined',),
    'bounce_damage': ('damage_target_lost',),
    'probe': ('self_target',),
    'reveal': ('no_creature',),
    'mass_return': ('stolen_creature',),
}
KICKER = re.compile(r'Kicker ((?:\{(?:\d+|[BUGRW])\})+)(?: \(You may pay an additional \1 as you cast this spell\.\))?')
EFFECTS = {
    ('When this creature enters, if it was kicked, draw two cards.',): 'draw_two',
    ('Enchant creature', 'Enchanted creature has base power and toughness 8/8 and has trample.',
     'When this Aura enters, if it was kicked, you may search your library for a card named Gigantiform, put it onto the battlefield, then shuffle.'): 'search',
    ("Return target creature to its owner's hand. If this spell was kicked, it deals 2 damage to another target creature.",): 'bounce_damage',
    ('Draw three cards, then discard two cards. If this spell was kicked, target player discards two cards.',): 'probe',
    ('Flying', "When this creature enters, draw three cards. Then if it wasn't kicked, discard three cards."): 'draw_loot',
    ('When this creature enters, if it was kicked, you may reveal cards from the top of your library until you reveal a creature card. If you do, put that card onto the battlefield and shuffle all other cards revealed this way into your library.',): 'reveal',
    ("When Slinn Voda enters, if it was kicked, return all creatures to their owners' hands except for Merfolk, Krakens, Leviathans, Octopuses, and Serpents.",): 'mass_return',
}
WITNESSES = {
    'Forest', 'Plains', 'Island', 'Grizzly Bears', 'Hill Giant', 'Runeclaw Bear',
    'Moldervine Cloak', 'Spellbook', 'Merfolk of the Pearl Trident', 'Kraken Hatchling',
    'Inkwell Leviathan', 'Giant Octopus', 'Frilled Sea Serpent', 'Counterspell', 'Unsummon', 'Control Magic',
}
STATIC_WITNESSES = {
    'Grizzly Bears', 'Hill Giant', 'Runeclaw Bear', 'Merfolk of the Pearl Trident',
    'Kraken Hatchling', 'Inkwell Leviathan', 'Giant Octopus',
}

ZERO = [0] * 6
ZONE_NAMES = ('hand', 'battlefield', 'library', 'graveyard', 'exile')
SUBTYPES = {
    'Merfolk of the Pearl Trident': 'Merfolk',
    'Kraken Hatchling': 'Kraken',
    'Inkwell Leviathan': 'Leviathan',
    'Giant Octopus': 'Octopus',
    'Frilled Sea Serpent': 'Serpent',
}


class CardFlowCase:
    def __init__(self, record, actor, mode, rows, oracle, model):
        self.source, self.name = record, record['name']
        self.csvid = rows[self.name]['csvid']
        self.actor, self.enemy = actor, 1-actor
        self.mode, self.model, self.effect = mode, model, model['effect']
        self.paid = mode not in ('declined', 'kicker_short', 'base_short') and not (
            mode == 'replay' and self.effect == 'draw_loot')
        self.oracle, self.rows = oracle, rows
        self.replay_at = None
        self.setup, self.steps = [], []
        self.library = ['Forest', 'Plains', 'Island']
        self.add('source', actor, self.name, 0)
        if self.effect == 'draw_two':
            self.library.append('Runeclaw Bear')
        if self.effect in ('probe', 'draw_loot'):
            self.library = ['Forest', 'Forest', 'Forest', 'Island']
        if self.effect == 'probe':
            if mode == 'self_target':
                self.add('own_discard', actor, 'Forest', 0)
            self.add('enemy_card0', self.enemy, 'Plains', 0)
            self.add('enemy_card1', self.enemy, 'Plains', 0)
        if self.effect in ('search', 'draw_loot'):
            self.add('blocker', self.enemy, 'Grizzly Bears')
        if self.effect == 'search':
            self.library = ['Moldervine Cloak', self.name, 'Plains']
            self.add('enhance_first', actor, 'Runeclaw Bear')
            self.add('enhance_second', actor, 'Hill Giant')
        if self.effect == 'bounce_damage':
            self.add('return_target', self.enemy, 'Grizzly Bears')
            self.add('damage_target', actor, 'Hill Giant')
            self.add('untouched', self.enemy, 'Runeclaw Bear')
        if self.effect == 'reveal':
            self.library = (['Forest', 'Plains', 'Island'] if mode == 'no_creature'
                            else ['Forest', 'Grizzly Bears', 'Plains', 'Hill Giant'])
        if self.effect == 'mass_return':
            self.add('ally_creature', actor, 'Runeclaw Bear')
            self.add('enemy_creature', self.enemy, 'Hill Giant')
            self.add('other_permanent', actor, 'Spellbook')
            for index, (name, subtype) in enumerate(SUBTYPES.items()):
                assert subtype in oracle[name]['type_line'].partition(' — ')[2].split()
                self.add(f'exception_{index}', actor if index % 2 == 0 else self.enemy, name)
        if mode == 'countered':
            self.add('counter', self.enemy, 'Counterspell', 0)
        if mode == 'damage_target_lost':
            self.add('reply', self.enemy, 'Unsummon', 0)
        if mode == 'stolen_creature':
            self.add('control', actor, 'Control Magic', 0)
        for index, name in enumerate(self.library):
            self.add(f'library_{index}', actor, name, 2)
        for index, name in enumerate(('Island', 'Plains')):
            self.add(f'enemy_library_{index}', self.enemy, name, 2)

    def add(self, ref, player, name, zone=1):
        from recipes import card

        self.setup.append(card(name, ref, player, ZONE_NAMES[zone]))

    def emit(self, op, ref='', other='', property_name='', **fields):
        from recipes import step

        if ref:
            fields['ref'] = ref
        if other:
            fields['other'] = other
        if property_name:
            fields['property'] = property_name
        if 'zone' in fields:
            fields['zone'] = ZONE_NAMES[fields['zone']]
        self.steps.append(step(op, **fields))

    def observe(self, ref, property_name, value):
        self.emit('assert_card', ref, property_name=property_name, value=value)

    def mana(self, op, player, pool):
        for color, value in enumerate(pool):
            self.emit(op, player=player, value=value, color=color)

    def zone(self, name, player, zone, amount):
        self.emit('assert_zone_multiplicity', player=player, zone=zone, index=self.rows[name]['csvid'], value=amount)

    def combat(self, attacker, blocker, *, blocked):
        self.emit('begin_attackers')
        self.emit('attack', attacker, value=1)
        self.emit('finish_attackers')
        self.emit('begin_blockers')
        self.emit('block', blocker, attacker, value=int(blocked))
        self.emit('advance_combat')
        self.observe(attacker, 'blocked', int(blocked))
        self.emit('combat_damage')
        self.emit('resolve')
        self.emit('end_combat')
        self.emit('assert_choices_consumed')

    def library_state(self, expected, *, ordered=True):
        self.emit('assert_zone', player=self.actor, zone=2, value=len(expected))
        if ordered:
            for index, name in enumerate(expected):
                self.emit('assert_zone_card', player=self.actor, zone=2, index=index, value=self.rows[name]['csvid'])
        else:
            for name in sorted(set(self.library)):
                self.zone(name, self.actor, 2, expected.count(name))

    def payment_and_cast(self):
        base = self.model['base_cost']
        additional = self.model['kicker_cost'].copy()
        if self.mode == 'kicker_short':
            color = next(i for i, amount in enumerate(additional) if amount)
            additional[color] -= 1
        funding = [printed + extra for printed, extra in zip(base, additional)]
        self.remaining = ZERO if self.paid or self.mode == 'base_short' else additional
        self.emit('turn', player=self.actor)
        self.mana('set_mana', self.actor, ZERO if self.mode == 'base_short' else funding)
        self.mana('set_mana', self.enemy, ZERO)
        self.emit('refresh')
        self.library_state(self.library)
        if self.mode == 'base_short':
            self.emit('cast', 'source', value=0)
            self.emit('assert_choices_consumed')
            self.emit('assert_stack', value=0)
            self.observe('source', 'in_hand', 1)
            self.zone(self.name, self.actor, 1, 0)
            self.zone(self.name, self.actor, 3, 0)
            self.mana('assert_mana', self.actor, ZERO)
            return
        if self.mode == 'replay':
            self.replay_at = len(self.steps)
        if self.effect == 'bounce_damage':
            self.emit('choose_target', 'return_target', player=self.actor)
        if self.mode != 'kicker_short':
            self.emit('choose_dialog', player=self.actor, value=int(not self.paid))
        if self.effect == 'bounce_damage' and self.paid:
            self.emit('choose_target', 'damage_target', player=self.actor)
        if self.effect == 'probe' and self.paid:
            self.emit('choose_player', player=self.actor,
                      value=self.actor if self.mode == 'self_target' else self.enemy)
        if self.effect == 'search':
            self.emit('choose_target', 'enhance_first', player=self.actor)
        self.emit('cast', 'source', value=1)
        self.emit('assert_choices_consumed')
        self.emit('assert_stack', value=1)
        self.observe('source', 'in_play', 0)
        self.observe('source', 'in_hand', 0)
        self.library_state(self.library)
        self.mana('assert_mana', self.actor, self.remaining)
        self.mana('assert_mana', self.enemy, ZERO)
        if self.effect == 'search':
            self.observe('enhance_first', 'power', int(self.oracle['Runeclaw Bear']['power']))
        if self.effect == 'bounce_damage':
            self.observe('return_target', 'in_play', 1)
            self.observe('damage_target', 'damage', 0)
        if self.effect == 'probe':
            self.zone('Plains', self.enemy, 0, 2)

    def resolve(self):
        if self.effect == 'search' and self.paid:
            if self.mode == 'search_declined':
                self.emit('choose_zone', player=self.actor, value=-1)
            else:
                self.emit('choose_zone_card', player=self.actor, value=self.csvid)
                self.emit('choose_target', 'enhance_second', player=self.actor)
        if self.effect == 'reveal' and self.paid and self.mode != 'decline_trigger':
            if self.actor == 0:
                self.emit('choose_trigger', 'source', player=0)
            for seat in (self.actor, self.enemy):
                self.emit('choose_zone', player=seat, value=-1)
        if self.effect == 'probe' or self.effect == 'draw_loot' and not self.paid:
            self.emit('discard_policy', value=1)
        self.emit('resolve')
        self.emit('refresh')
        self.emit('assert_choices_consumed')
        self.emit('assert_stack', value=0)
        self.mana('assert_mana', self.actor, self.remaining)
        self.mana('assert_mana', self.enemy, ZERO)
        self.emit('assert_life', player=self.actor, value=20)
        self.emit('assert_life', player=self.enemy, value=20)
        permanent = self.effect not in ('bounce_damage', 'probe')
        self.zone(self.name, self.actor, 0, 0)
        self.zone(self.name, self.actor, 1, int(permanent) + int(self.search_selected))
        self.zone(self.name, self.actor, 3, int(not permanent))
        if permanent:
            self.observe('source', 'in_play', 1)
        if 'power' in self.source:
            for prop in ('power', 'toughness'):
                self.observe('source', prop, int(self.source[prop]))

    @property
    def search_selected(self):
        return self.effect == 'search' and self.paid and self.mode != 'search_declined'

    def draw_and_discard(self):
        if self.effect == 'draw_two':
            draw = 2 if self.paid else 0
            self.library_state(self.library[draw:])
            self.zone('Forest', self.actor, 0, int(self.paid))
            self.zone('Plains', self.actor, 0, int(self.paid))
            self.zone('Forest', self.actor, 3, 0)
        else:
            self.library_state(self.library[3:])
            own_target = self.mode == 'self_target'
            discards = 2 if self.effect == 'probe' else 0 if self.paid else 3
            discards += 2 * int(own_target)
            self.zone('Forest', self.actor, 0, 3+int(own_target)-discards)
            self.zone('Forest', self.actor, 3, discards)
            if self.effect == 'probe':
                enemy_discards = int(self.paid and not own_target) * 2
                self.zone('Plains', self.enemy, 0, 2-enemy_discards)
                self.zone('Plains', self.enemy, 3, enemy_discards)

    def aura(self):
        kept = [name for name in self.library if name != self.name] if self.search_selected else self.library
        self.library_state(kept, ordered=not self.paid)
        self.zone('Moldervine Cloak', self.actor, 1, 0)
        self.zone(self.name, self.actor, 2, int(not self.search_selected))
        for prop in ('power', 'toughness'):
            self.observe('enhance_first', prop, 8)
            self.observe('enhance_second', prop, 8 if self.search_selected else int(self.oracle['Hill Giant'][prop]))
        self.ready_attacker()
        self.combat('enhance_first', 'blocker', blocked=True)
        self.emit('assert_life', player=self.enemy, value=20-(8-int(self.oracle['Grizzly Bears']['toughness'])))
        self.zone('Grizzly Bears', self.enemy, 3, 1)
        self.observe('enhance_first', 'damage', int(self.oracle['Grizzly Bears']['power']))

    def ready_attacker(self):
        if any(self.remaining):
            self.mana('set_mana', self.actor, ZERO)
            self.emit('refresh')
        self.emit('advance_turn')
        self.emit('untap_step', player=self.enemy)
        self.emit('advance_turn')
        self.emit('untap_step', player=self.actor)
        self.emit('turn', player=self.actor)

    def jilt(self):
        self.zone('Grizzly Bears', self.enemy, 0, 1)
        self.zone('Grizzly Bears', self.enemy, 1, 0)
        if self.mode == 'damage_target_lost':
            self.zone('Hill Giant', self.actor, 0, 1)
            self.zone('Hill Giant', self.actor, 1, 0)
        else:
            self.zone('Hill Giant', self.actor, 1, 1)
            self.observe('damage_target', 'damage', 2*int(self.paid))
        self.observe('untouched', 'damage', 0)
        self.zone('Runeclaw Bear', self.enemy, 1, 1)

    def thicket(self):
        reveal = self.paid and self.mode != 'decline_trigger'
        found = reveal and 'Grizzly Bears' in self.library
        kept = [name for name in self.library if name != 'Grizzly Bears'] if found else self.library
        self.library_state(kept, ordered=not reveal)
        self.zone('Grizzly Bears', self.actor, 0, 0)
        self.zone('Grizzly Bears', self.actor, 1, int(found))
        self.zone('Hill Giant', self.actor, 1, 0)
        self.zone('Forest', self.actor, 3, 0)
        self.zone('Plains', self.actor, 3, 0)

    def slinn(self):
        for seat, name in ((self.actor, 'Runeclaw Bear'), (self.enemy, 'Hill Giant')):
            self.zone(name, seat, 0, int(self.paid))
            self.zone(name, seat, 1, int(not self.paid))
            self.zone(name, seat, 3, 0)
        for index, name in enumerate(SUBTYPES):
            seat = self.actor if index % 2 == 0 else self.enemy
            self.zone(name, seat, 1, 1)
            self.zone(name, seat, 0, 0)
        self.zone('Spellbook', self.actor, 1, 1)

    def cast_support(self, ref, name, player, *, target=None, response=False):
        self.mana('set_mana', player, cost_from_oracle(self.oracle[name]))
        self.emit('refresh')
        if target:
            self.emit('choose_target', target, player=player)
        self.emit('cast_response' if response else 'cast', ref, value=1)
        self.emit('assert_choices_consumed')
        self.emit('assert_stack', value=1+int(response))
        self.mana('assert_mana', player, ZERO)
        self.emit('resolve_response' if response else 'resolve')
        self.emit('refresh')
        self.emit('assert_choices_consumed')

    def counter(self):
        self.emit('announce_spell', 'source')
        self.cast_support('counter', 'Counterspell', self.enemy, response=True)
        self.emit('assert_stack', value=0)
        for card, player in ((self.name, self.actor), ('Counterspell', self.enemy)):
            self.zone(card, player, 0, 0)
            self.zone(card, player, 1, 0)
            self.zone(card, player, 3, 1)
        self.library_state(self.library)
        if self.effect == 'bounce_damage':
            self.observe('return_target', 'in_play', 1)
            self.observe('damage_target', 'damage', 0)
        elif self.effect == 'search':
            self.observe('enhance_first', 'power', int(self.oracle['Runeclaw Bear']['power']))
        elif self.effect == 'probe':
            self.zone('Plains', self.enemy, 0, 2)
            self.zone('Plains', self.enemy, 3, 0)
        elif self.effect == 'mass_return':
            self.zone('Runeclaw Bear', self.actor, 1, 1)
            self.zone('Hill Giant', self.enemy, 1, 1)

    def run(self):
        if self.mode == 'stolen_creature':
            self.emit('turn', player=self.actor)
            self.mana('set_mana', self.enemy, ZERO)
            self.cast_support('control', 'Control Magic', self.actor, target='enemy_creature')
            self.emit('assert_stack', value=0)
            self.zone('Hill Giant', self.actor, 1, 1)
            self.zone('Hill Giant', self.enemy, 1, 0)
        self.payment_and_cast()
        if self.mode == 'base_short':
            return
        if self.mode == 'countered':
            self.counter()
            return
        if self.mode == 'damage_target_lost':
            self.emit('announce_spell', 'source')
            self.cast_support('reply', 'Unsummon', self.enemy, target='damage_target', response=True)
            self.zone('Unsummon', self.enemy, 3, 1)
            self.zone('Hill Giant', self.actor, 0, 1)
        self.resolve()
        if self.effect in ('draw_two', 'probe', 'draw_loot'):
            self.draw_and_discard()
        if self.effect == 'draw_loot':
            self.ready_attacker()
            self.combat('source', 'blocker', blocked=False)
            self.emit('assert_life', player=self.enemy, value=20-int(self.source['power']))
            self.observe('blocker', 'damage', 0)
        elif self.effect == 'search':
            self.aura()
        elif self.effect == 'bounce_damage':
            self.jilt()
        elif self.effect == 'reveal':
            self.thicket()
        elif self.effect == 'mass_return':
            self.slinn()
            if self.mode == 'stolen_creature':
                self.zone('Hill Giant', self.actor, 0, 0)
                self.zone('Hill Giant', self.actor, 1, 0)
                self.zone('Control Magic', self.actor, 1, 0)
                self.zone('Control Magic', self.actor, 3, 1)


@lru_cache(maxsize=1)
def profile():
    return json.loads(PROFILE.read_text(encoding='utf8'))


def shape(record):
    lines = record['oracle_text'].splitlines()
    kicker = KICKER.fullmatch(lines[0]) if lines else None
    effect = EFFECTS.get(tuple(lines[1:]))
    if (record.get('layout') != 'normal' or record.get('selected_face') or record.get('card_faces')
            or 'Kicker' not in record.get('keywords', ()) or not kicker or not effect):
        raise ValueError('whole normal Oracle Kicker card-flow grammar required')
    return dict(effect=effect, base_cost=cost_from_oracle(record),
                kicker_cost=cost_from_oracle({'mana_cost': kicker[1]}))


def variants(model):
    effect = model['effect']
    selected = [f'{mode}_{seat}' for mode in (*COMMON, *EXTRAS.get(effect, ())) for seat in (0, 1)]
    if effect == 'reveal':
        selected.append('decline_trigger_0')
    return selected


def spec(declaration, record):
    model = shape(record)
    selected = declaration.get('variants')
    pin = profile()['sources'].get(record['name'], {})
    if (declaration.get('family') != FAMILY or type(declaration.get('version')) is not int
            or declaration['version'] != 2 or declaration.get('oracle_text') != record['oracle_text']
            or not exact(pin.get('oracle'), record) or not exact(declaration.get('params'), model)):
        raise ValueError('whole implemented Oracle card-flow record and typed model required')
    if (not isinstance(selected, list) or not selected or any(type(v) is not str for v in selected)
            or len(selected) != len(set(selected)) or set(selected) - set(variants(model))):
        raise ValueError('distinct calibrated card-flow variants required')
    return dict(family=FAMILY, **model)


def validate_witnesses(oracle, registry):
    pins = profile()
    if pins['witnesses'].keys() != WITNESSES:
        raise ValueError('private card-flow witness scope changed')
    for name, pin in (pins['sources'] | pins['witnesses']).items():
        row = registry.get(name, {})
        if (not exact(oracle.get(name), pin['oracle']) or not exact(row, pin['registry'])
                or row.get('kind') != 'card' or row.get('handler') in (None, '0', 'NULL')
                or row.get('handler') == 'card_dummy' and name not in STATIC_WITNESSES):
            raise ValueError('whole Oracle or implemented registry pin changed for ' + name)


def expand(name, csvid, declaration, model, selected, registry, oracle):
    from recipes import step

    validate_witnesses(oracle, registry)
    if (profile()['sources'].get(name, {}).get('registry', {}).get('csvid') != csvid
            or registry[name]['csvid'] != csvid or not selected or set(selected) - set(variants(model))):
        raise ValueError('only exact registered card-flow sources may expand')
    cases = []
    for variant in selected:
        mode, seat = variant.rsplit('_', 1)
        board = CardFlowCase(oracle[name], int(seat), mode, registry, oracle, model)
        board.run()
        steps = board.steps
        if board.replay_at is not None:
            prefix, suffix = steps[:board.replay_at], steps[board.replay_at:]
            steps = [*prefix, step('capture'), *suffix, step('record_state'), step('restore'),
                     *suffix, step('assert_recorded_state')]
        cases.append(dict(name=f'KickerCardFlowRecipe_{csvid}_{variant}', card=name, fixture='empty_main',
                          seed=2334, setup=board.setup, steps=steps,
                          _recipe=dict(family=FAMILY, version=2, variant=variant,
                                       params=declaration['params'], oracle_text=declaration['oracle_text'],
                                       untested_components=[])))
    return cases
