"""Real regeneration, timed shields, and independent spell or Aura riders."""

from functools import lru_cache
import json
from pathlib import Path

from library_search_recipes import wave6_exact as exact
from recipes import card, cost_from_oracle, step


FAMILY = 'regeneration_spells_and_auras'
PROFILE = Path(__file__).parent / 'data' / f'{FAMILY}_wave8_profiles.json'

MENDING, WRAP, DEBT, AURA = 13179, 9054, 2958, 201
MODELS = {
    MENDING: ('{G}', 'Instant', 'Regenerate target creature.',
              ('proactive', 'wounded', 'foreign')),
    WRAP: ('{1}{G}', 'Instant', 'Regenerate each creature you control.',
           ('mass', 'single_use', 'expires', 'cannot_regenerate', 'zero_toughness', 'combat')),
    DEBT: ('{1}{W}{W}', 'Instant',
           'Regenerate target creature. You gain control of that creature if it regenerates this way.',
           ('proactive', 'friendly', 'foreign')),
    AURA: ('{1}{G}', 'Enchantment — Aura',
           'Enchant creature (Target a creature as you cast this. This card enters attached to that creature.)\n'
           '{G}: Regenerate enchanted creature. (The next time that creature would be destroyed this turn, '
           'instead tap it, remove it from combat, and heal all damage on it.)',
           ('proactive', 'wounded', 'foreign', 'short', 'detach')),
}
WITNESSES = {
    'Serra Angel': ('{3}{W}{W}', "Flying\nVigilance (Attacking doesn't cause this creature to tap.)", '4', '4'),
    'Goblin Tinkerer': ('{1}{R}', '{R}, {T}: Destroy target artifact. That artifact deals damage equal to its mana value to this creature.', '1', '2'),
    'Prodigal Sorcerer': ('{2}{U}', '{T}: This creature deals 1 damage to any target.', '1', '1'),
    'Shock': ('{R}', 'Shock deals 2 damage to any target.'),
    'Day of Judgment': ('{2}{W}{W}', 'Destroy all creatures.'),
    'Terminate': ('{B}{R}', "Destroy target creature. It can't be regenerated."),
    'Last Gasp': ('{1}{B}', 'Target creature gets -3/-3 until end of turn.'),
    'Unsummon': ('{U}', "Return target creature to its owner's hand."),
}
ZONES = {'hand': 0, 'battlefield': 1, 'library': 2, 'graveyard': 3, 'exile': 4}
ZERO, GREEN = (0,) * 6, (0, 0, 0, 1, 0, 0)
SEED = 2897
UNTAPPED = 8


def label(csvid, mode, actor):
    return f'RegenerationSpellsAndAurasRecipe_{csvid}_{mode}_{actor}'


class Board:
    def __init__(self, record, csvid, mode, actor, rows, oracle):
        self.record, self.csvid, self.mode, self.actor, self.foe = record, csvid, mode, actor, 1-actor
        self.rows, self.oracle = rows, oracle
        self.setup, self.steps, self.cards = [], [], {}
        self.wallet = [ZERO, ZERO]
        self.life = [20, 20]
        self.add('source', record['name'], actor, 'hand')
        self.host = self.foe if mode == 'foreign' else actor
        host_name = 'Goblin Tinkerer' if mode == 'zero_toughness' else 'Serra Angel'
        self.add('host', host_name, self.host, 'battlefield')
        if csvid == WRAP and mode == 'mass':
            self.add('second', 'Goblin Tinkerer', actor, 'battlefield')
        self.add('contrast', 'Prodigal Sorcerer', 1-self.host, 'battlefield')
        if mode == 'wounded' or (csvid == DEBT and mode == 'foreign'):
            self.add('shock', 'Shock', self.foe, 'hand')
        answer = {'mass': 'Day of Judgment', 'cannot_regenerate': 'Terminate',
                  'zero_toughness': 'Last Gasp', 'detach': 'Unsummon'}.get(mode)
        if answer:
            self.add('answer', answer, actor, 'hand')

    def add(self, ref, name, player, zone):
        assert ref not in self.cards and player in (0, 1)
        self.cards[ref] = dict(name=name, owner=player, controller=player, zone=zone)
        state = UNTAPPED if zone == 'battlefield' else 0
        entry = card(name, ref, player, zone)
        if state:
            entry['state'] = ['untapped']
        self.setup.append(entry)

    def emit(self, op, ref='', other='', prop='', *, player=0, value=0, zone=0, index=0, color=0):
        fields = dict(ref=ref, other=other, property=prop, player=player, value=value,
                      zone=next(name for name, code in ZONES.items() if code == zone), index=index, color=color)
        self.steps.append(step(op, **{key: item for key, item in fields.items() if item != ""}))

    def prop(self, ref, prop, value):
        self.emit('assert_card', ref, prop=prop, value=value)

    def fund(self, player, pool):
        self.wallet[player] = tuple(pool)
        for color, amount in enumerate(pool):
            self.emit('set_mana', player=player, color=color, value=amount)
        self.emit('refresh')

    def money(self):
        for who, pool in enumerate(self.wallet):
            self.emit('assert_life', player=who, value=self.life[who])
            for color, amount in enumerate(pool):
                self.emit('assert_mana', player=who, color=color, value=amount)

    def move(self, ref, zone, *, controller=None):
        card = self.cards[ref]
        card['zone'] = zone
        card['controller'] = card['owner'] if controller is None else controller

    def receipt(self):
        self.emit('assert_choices_consumed')
        self.emit('assert_stack', value=0)
        self.money()
        for who in (0, 1):
            for zone, code in ZONES.items():
                cards = [card for card in self.cards.values() if card['zone'] == zone and
                         card['controller' if zone == 'battlefield' else 'owner'] == who]
                self.emit('assert_zone', player=who, zone=code, value=len(cards))
                for name in sorted({card['name'] for card in cards}):
                    self.emit('assert_zone_multiplicity', player=who, zone=code,
                              index=self.rows[name]['csvid'], value=sum(card['name'] == name for card in cards))

    def fee(self, ref):
        return tuple(cost_from_oracle(self.oracle[self.cards[ref]['name']]))

    def cast(self, ref='source', target=None, destination='graveyard'):
        who = self.cards[ref]['owner']
        self.fund(who, self.fee(ref))
        if target is not None:
            self.emit('choose_target', target, player=who)
        self.emit('cast', ref, value=1)
        self.emit('assert_stack', value=1)
        self.wallet[who] = ZERO
        self.money()
        self.emit('resolve')
        self.move(ref, destination)
        self.emit('assert_choices_consumed')
        self.emit('assert_stack', value=0)

    def intact(self, ref='host', *, damage=0, tapped=0):
        card = self.oracle[self.cards[ref]['name']]
        self.prop(ref, 'in_play', 1)
        for axis in ('power', 'toughness'):
            self.prop(ref, axis, int(card[axis]))
        self.prop(ref, 'damage', damage)
        self.prop(ref, 'tapped', tapped)

    def destroy(self, ref='host', destination=None):
        self.emit('destroy', ref)
        if destination is not None:
            self.move(ref, destination)

    def wounded(self):
        self.cast('shock', 'host')
        self.intact(damage=2)

    def legacy_instant(self):
        self.fund(self.actor, self.fee('source'))
        if self.mode == 'proactive':
            self.emit('can_cast', 'source', value=1)
            self.intact()
            return
        if 'shock' in self.cards:
            self.wounded()
        self.emit('choose_priority', 'source', player=self.actor)
        self.emit('choose_target', 'host', player=self.actor)
        self.destroy()
        self.wallet[self.actor] = ZERO
        self.move('source', 'graveyard')
        if self.csvid == DEBT and self.host != self.actor:
            self.move('host', 'battlefield', controller=self.actor)
            self.receipt()
            self.emit('bind_permanent', 'host', player=self.actor, index=-1,
                      value=self.rows[self.cards['host']['name']]['csvid'])
            self.intact(tapped=1)
            self.emit('cleanup')
            self.intact(tapped=1)
            self.destroy(destination='graveyard')
        else:
            self.intact(tapped=1)
        self.intact('contrast')

    def wrap(self):
        self.cast()
        self.intact()
        self.receipt()
        if self.mode == 'mass':
            self.cast('answer')
            self.move('contrast', 'graveyard')
            self.intact(tapped=1)
            self.intact('second', tapped=1)
        elif self.mode == 'expires':
            self.emit('cleanup')
            self.destroy(destination='graveyard')
        elif self.mode in ('cannot_regenerate', 'zero_toughness'):
            self.cast('answer', 'host')
            self.move('host', 'graveyard')
        elif self.mode == 'single_use':
            self.destroy()
            self.intact(tapped=1)
            self.receipt()
            self.destroy(destination='graveyard')
        elif self.mode == 'combat':
            self.emit('begin_attackers')
            self.emit('attack', 'host', value=1)
            self.emit('finish_attackers')
            self.prop('host', 'attacking', 1)
            self.destroy()
            self.intact(tapped=1)
            self.prop('host', 'attacking', 0)
        else:
            raise ValueError(self.mode)
        if self.mode != 'mass':
            self.intact('contrast')

    def aura(self):
        self.cast(target='host', destination='battlefield')
        self.prop('source', 'in_play', 1)
        self.intact()
        self.receipt()
        if self.mode == 'detach':
            self.cast('answer', 'host')
            self.move('host', 'hand')
            self.move('source', 'graveyard')
            self.intact('contrast')
            return
        if 'shock' in self.cards:
            self.wounded()
        if self.mode != 'short':
            self.fund(self.actor, GREEN)
        if self.mode == 'proactive':
            self.emit('can_activate', 'source', value=1)
        elif self.mode == 'short':
            self.destroy(destination='graveyard')
            self.move('source', 'graveyard')
        else:
            self.emit('choose_regeneration', 'source')
            self.destroy()
            self.wallet[self.actor] = ZERO
            self.intact(tapped=1)
            self.prop('source', 'in_play', 1)
        self.intact('contrast')

    def play(self):
        self.emit('turn', player=self.actor)
        self.emit('present_ai_dialogs', value=1)
        for who in (0, 1):
            self.fund(who, ZERO)
        if self.csvid == WRAP:
            self.wrap()
        elif self.csvid == AURA:
            self.aura()
        else:
            self.legacy_instant()
        self.receipt()



STRANDS, CHARM, WAIL, FALL = 7970, 12731, 6832, 3094
FOLLOWERS = {
    STRANDS: ('{3}{B}', 'Enchantment — Aura',
              'Enchant creature\nWhen this Aura enters, target player discards two cards.\n{B}: Regenerate enchanted creature.',
              ('opponent_discards', 'self_discards', 'one_card', 'foreign', 'proactive')),
    CHARM: ('{B}{G}', 'Instant',
            'Choose one —\n• All creatures get -1/-1 until end of turn.\n• Destroy target enchantment.\n'
            '• Regenerate each creature you control.', ('weaken', 'enchantment', 'regenerate')),
    WAIL: ('{2}{B}', 'Instant',
           'Choose one —\n• Regenerate each creature you control.\n'
           '• Wail of the Nim deals 1 damage to each creature and each player.\n'
           'Entwine {B} (Choose both if you pay the entwine cost.)',
           ('shields', 'damage', 'entwine')),
    FALL: ('{2}{G}', 'Enchantment', "Return this enchantment to its owner's hand: Regenerate target creature.",
           ('friendly', 'foreign', 'recast', 'proactive')),
}
KEYWORDS = {201: {'Heal', 'Regenerate', 'Enchant'}, STRANDS: {'Regenerate', 'Enchant'},
            WAIL: {'Regenerate', 'Entwine'}}
BLACK = (0, 1, 0, 0, 0, 0)
NEW_WITNESSES = {
    'Island': ('', '({T}: Add {U}.)'),
    'Mountain': ('', '({T}: Add {R}.)'),
    'Elvish Mystic': ('{G}', '{T}: Add {G}.', '1', '1'),
    'Rites of Flourishing': ('{2}{G}', "At the beginning of each player's draw step, that player draws an additional card.\nEach player may play an additional land on each of their turns."),
}


class FollowOn(Board):
    def __init__(self, record, csvid, mode, actor, rows, oracle):
        super().__init__(record, csvid, mode, actor, rows, oracle)
        self.recipient = actor if mode == 'self_discards' else self.foe
        if csvid == STRANDS and mode in ('opponent_discards', 'self_discards', 'one_card'):
            names = ('Island',) if mode == 'one_card' else ('Island', 'Mountain')
            for n, name in enumerate(names):
                self.add(f'discard_{n}', name, self.recipient, 'hand')
        if csvid == CHARM and mode in ('weaken', 'regenerate'):
            self.add('second', 'Goblin Tinkerer', actor, 'battlefield')
        if csvid == CHARM and mode == 'enchantment':
            self.add('enchantment', 'Rites of Flourishing', self.foe, 'battlefield')
        if csvid == CHARM and mode == 'regenerate':
            self.add('answer', 'Day of Judgment', actor, 'hand')
        if csvid == WAIL:
            self.add('second', 'Elvish Mystic', actor, 'battlefield')

    def begin(self, ref='source', *, target=None, payment=None, dialog=None, reserve=ZERO):
        who = self.cards[ref]['owner']
        cost = payment or self.fee(ref)
        self.fund(who, tuple(a+b for a, b in zip(cost, reserve)))
        if dialog is not None:
            self.emit('choose_dialog', player=who, value=dialog)
        if target is not None:
            self.emit('choose_target', target, player=who)
        self.emit('cast', ref, value=1)
        self.emit('assert_stack', value=1)
        self.wallet[who] = reserve
        self.money()

    def finish(self, ref='source', destination='graveyard'):
        self.emit('resolve')
        self.move(ref, destination)
        self.emit('assert_choices_consumed')
        self.emit('assert_stack', value=0)

    def strands(self):
        self.begin(target='host')
        self.emit('choose_player', player=self.actor, value=self.recipient)
        discard = sorted(ref for ref in self.cards if ref.startswith('discard_'))
        for ref in discard:
            if self.recipient == 0:
                name = self.cards[ref]['name']
                self.emit('choose_zone_card', player=self.recipient, value=self.rows[name]['csvid'])
            self.move(ref, 'graveyard')
        self.finish(destination='battlefield')
        self.prop('source', 'in_play', 1)
        self.intact()
        self.receipt()
        if self.mode == 'proactive':
            self.fund(self.actor, BLACK)
            self.emit('can_activate', 'source', value=1)
        else:
            self.fund(self.actor, BLACK)
            self.emit('choose_regeneration', 'source')
            self.destroy()
            self.wallet[self.actor] = ZERO
            self.intact(tapped=1)
            self.prop('source', 'in_play', 1)
        self.intact('contrast')

    def charm(self):
        if self.mode == 'regenerate':
            self.begin('answer', reserve=self.fee('source'))
            if self.actor == 0:
                # Pass Day of Judgment's ordinary priority. The legacy
                # regeneration mode is offered only after destruction starts.
                self.emit('choose_priority', player=self.actor, value=-2)
            self.emit('choose_priority', 'source', player=self.actor)
            self.emit('choose_dialog', player=self.actor, value=2)
            self.finish('answer')
            self.move('source', 'graveyard')
            self.move('contrast', 'graveyard')
            self.wallet[self.actor] = ZERO
            self.intact(tapped=1)
            self.intact('second', tapped=1)
            return
        if self.mode == 'enchantment':
            self.begin(dialog=1, target='enchantment')
            self.finish()
            self.move('enchantment', 'graveyard')
            self.intact()
            self.intact('contrast')
            return
        self.begin(dialog=0)
        self.finish()
        self.move('contrast', 'graveyard')
        self.prop('host', 'power', 3)
        self.prop('host', 'toughness', 3)
        self.prop('second', 'power', 0)
        self.prop('second', 'toughness', 1)
        self.receipt()
        self.emit('cleanup')
        self.intact()
        self.intact('second')

    def wail(self):
        mode = {'shields': 0, 'damage': 1, 'entwine': 2}[self.mode]
        payment = self.fee('source')
        if self.mode == 'entwine':
            payment = tuple(a+b for a, b in zip(payment, BLACK))
        self.begin(dialog=mode, payment=payment)
        self.finish()
        if mode == 0:
            self.destroy()
            self.intact(tapped=1)
            self.destroy('second')
            self.intact('second', tapped=1)
            self.intact('contrast')
        else:
            self.life[:] = [19, 19]
            self.move('contrast', 'graveyard')
            self.intact(damage=1)
            if mode == 2:
                self.intact('second', tapped=1)
                self.destroy('second', 'graveyard')
            else:
                self.move('second', 'graveyard')

    def fall(self):
        self.cast(destination='battlefield')
        if self.mode == 'proactive':
            self.emit('can_activate', 'source', value=1)
            self.intact()
            return
        for repetition in range(2 if self.mode == 'recast' else 1):
            if repetition:
                self.emit('bind_hand', 'source', player=self.actor, index=-1, value=FALL)
                self.cast(destination='battlefield')
            self.emit('choose_regeneration', 'source')
            self.emit('choose_target', 'host', player=self.actor)
            self.destroy()
            self.move('source', 'hand')
            self.intact(tapped=1)
            self.intact('contrast')
            self.receipt()

    def play(self):
        self.emit('turn', player=self.actor)
        self.emit('present_ai_dialogs', value=1)
        for who in (0, 1):
            self.fund(who, ZERO)
        effect = {STRANDS: self.strands, CHARM: self.charm, WAIL: self.wail, FALL: self.fall}
        effect[self.csvid]()
        self.receipt()


@lru_cache
def profiles():
    return json.loads(PROFILE.read_bytes())


def classify(record, csvid):
    cost, kind, rules, modes = (MODELS | FOLLOWERS)[csvid]
    if (record.get('mana_cost'), record.get('type_line'), record.get('oracle_text')) != (cost, kind, rules):
        raise ValueError('Exact regeneration mana, type and printed clauses required')
    if (record.get('layout') != 'normal' or record.get('selected_face') or record.get('card_faces')
            or set(record.get('keywords', ())) != KEYWORDS.get(csvid, {'Regenerate'})):
        raise ValueError('Only exact single-face regeneration records are supported')
    return dict(csvid=csvid, printed_cost=cost, choices=[f'{mode}_{seat}' for mode in modes for seat in (0, 1)])


def variants(model):
    return list(model['choices'])


def validate_witnesses(oracle, registry):
    pins = profiles()
    expects = WITNESSES | NEW_WITNESSES
    if (set(pins) != {'sources', 'witnesses'} or set(pins['witnesses']) != expects.keys()
            or {pin['registry']['csvid'] for pin in pins['sources'].values()} != MODELS.keys() | FOLLOWERS.keys()):
        raise ValueError('Exact regeneration source and witness scope required')
    for group in pins.values():
        for name, pin in group.items():
            record, row = oracle.get(name, {}), registry.get(name, {})
            if (not exact(pin, dict(oracle=record, registry=row)) or record.get('layout') != 'normal'
                    or row.get('kind') != 'card' or row.get('handler') in (None, '0', 'NULL', 'card_dummy')):
                raise ValueError(f'Changed implemented regeneration primary or witness: {name}')
            if name in expects:
                fields = ('mana_cost', 'oracle_text', 'power', 'toughness')[:len(expects[name])]
                if tuple(record[k] for k in fields) != expects[name]:
                    raise ValueError(f'Unexpected regeneration witness clause: {name}')


def spec(declaration, record):
    pin = profiles()['sources'].get((record or {}).get('name'))
    if not pin or not exact(record, pin['oracle']):
        raise ValueError('Complete pinned regeneration primary required')
    model = classify(record, pin['registry']['csvid'])
    selected = declaration.get('variants')
    if (declaration.get('family') != FAMILY or type(declaration.get('version')) is not int
            or declaration['version'] != 2 or declaration.get('oracle_text') != record['oracle_text']
            or not exact(declaration.get('params'), model) or not isinstance(selected, list) or not selected
            or any(type(v) is not str for v in selected) or len(selected) != len(set(selected))
            or set(selected) - set(variants(model))):
        raise ValueError('Exact regeneration recipe and distinct scoped variants required')
    return dict(model, family=FAMILY)


def expand(name, csvid, declaration, model, selected, registry, oracle):
    validate_witnesses(oracle, registry)
    if registry[name]['csvid'] != csvid or model['csvid'] != csvid or set(selected) - set(variants(model)):
        raise ValueError('Exact regeneration primary and variants required')
    legacy = {
        MENDING: 'Modern proactive shield resolution is unobserved where the engine denies the legal cast.',
        DEBT: 'Modern proactive shield resolution is unobserved where the engine denies the legal cast.',
        AURA: 'Modern proactive shield resolution is unobserved where the engine denies the legal activation.',
        STRANDS: 'Modern proactive shield resolution is unobserved where the engine denies the legal activation.',
        CHARM: 'The engine disables the printed proactive regeneration mode. The legacy destruction response exercises the same printed recipients.',
        FALL: 'Modern proactive shield resolution is unobserved where the engine denies the legal activation.',
    }
    cases = []
    factory = FollowOn if csvid in FOLLOWERS else Board
    for variant in selected:
        mode, seat = variant.rsplit('_', 1)
        board = factory(oracle[name], csvid, mode, int(seat), registry, oracle)
        board.play()
        metadata = dict(family=FAMILY, version=2, variant=variant, params=declaration['params'],
                        oracle_text=declaration['oracle_text'], covered_clauses=declaration['oracle_text'].splitlines(),
                        untested_components=[legacy[csvid]] if csvid in legacy else [])
        cases.append(dict(name=label(csvid, mode, int(seat)), card=name, fixture='empty_main', seed=SEED,
                          setup=board.setup, steps=board.steps, _recipe=metadata))
    return cases

