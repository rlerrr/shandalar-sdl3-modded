"""Paid casts and their independently observed noncreature rewards."""

import copy
import json
from collections import Counter
from functools import lru_cache
from pathlib import Path

from cast_trigger_case_support import CastTriggerBoard
from counted_battlefield_damage_recipes import types

FAMILY = 'cast_noncreature_riders'
PROFILE = Path(__file__).with_name('data') / 'cast_noncreature_riders_wave8_profiles.json'
CAST = 'Whenever you cast a noncreature spell, '
PROWESS = 'Prowess (' + CAST + 'this creature gets +1/+1 until end of turn.)'
SHAPES = {
    'ascendancy': ('jeskai_ascendancy', 'Enchantment', (), CAST + 'creatures you control get +1/+1 until end of turn. '
                   'Untap those creatures.\n' + CAST + 'you may draw a card. If you do, discard a card.'),
    'bounty': ('primeval_bounty', 'Enchantment', ('Landfall',),
               'Whenever you cast a creature spell, create a 3/3 green Beast creature token.\n'
               + CAST + 'put three +1/+1 counters on target creature you control.\n'
               'Landfall — Whenever a land you control enters, you gain 3 life.'),
    'quiet': ('quiet_contemplation', 'Enchantment', (), CAST + 'you may pay {1}. If you do, tap target creature an '
              "opponent controls and it doesn't untap during its controller's next untap step."),
    'mentor': ('monastery_mentor', 'Creature', ('Prowess',), PROWESS + '\n' + CAST +
               'create a 1/1 white Monk creature token with prowess.'),
    'cathar': ('cathars_companion', 'Creature', (), CAST + 'this creature gains indestructible until end of turn. '
               '(Damage and effects that say "destroy" don\'t destroy it.)'),
    'sage': ('sage_of_the_inward_eye', 'Creature', ('Flying',), 'Flying\n' + CAST +
             'creatures you control gain lifelink until end of turn.'),
    'mistfire': ('mistfire_adept', 'Creature', ('Prowess',), PROWESS + '\n' + CAST +
                 'target creature gains flying until end of turn.'),
    'shu': ('shun_yun_the_silent_tempest', 'Legendary Creature', ('Prowess',), PROWESS + '\n' + CAST +
            'you may pay {R/W}{R/W}. If you do, target creature gains double strike until end of turn.'),
    'fist': ('elusive_spellfist', 'Creature', (), CAST + "this creature gets +1/+0 until end of turn and can't be blocked this turn."),
    'myth': ('myth_realized', 'Enchantment', (), CAST + 'put a lore counter on this enchantment.\n'
             '{2}{W}: Put a lore counter on this enchantment.\n{W}: Until end of turn, this enchantment becomes a '
             'Monk Avatar creature in addition to its other types and gains "This creature\'s power and toughness '
             'are each equal to the number of lore counters on it."'),
    'exemplars': ('ojutai_exemplars', 'Creature', (), CAST + 'choose one —\n• Tap target creature.\n'
                  '• This creature gains first strike and lifelink until end of turn.\n'
                  "• Exile this creature, then return it to the battlefield tapped under its owner's control."),
    'skywise': ('pristine_skywise', 'Creature', ('Flying',), 'Flying\n' + CAST + 'untap this creature. It gains '
                'protection from the color of your choice until end of turn.'),
    'teachings': ('skywise_teachings', 'Enchantment', (), CAST +
                  'you may pay {1}{U}. If you do, create a 2/2 blue Djinn Monk creature token with flying.'),
}
COMMON = ('own', 'creature', 'opponent', 'countered')
EXTRAS = {
    'ascendancy': ('loot', 'expiry', 'replay'), 'bounty': ('landfall', 'foe_land', 'no_recipient', 'replay'),
    'quiet': ('unfunded', 'two_untaps', 'replay'), 'mentor': ('second', 'expiry', 'replay'),
    'cathar': ('survives', 'expired', 'no_reward', 'replay'),
    'sage': ('combat', 'ally_lifelink', 'expiry'), 'mistfire': ('combat', 'enemy_lift', 'expiry'),
    'shu': ('red', 'white', 'decline', 'unfunded'),
    'fist': ('combat', 'expiry'), 'myth': ('activate_both', 'expiry'), 'exemplars': ('tap', 'strike_link', 'blink'),
    'skywise': ('protect', 'expiry'), 'teachings': ('unfunded', 'evasion', 'replay'),
}
AMULET = "Traveler's Amulet"
BEAR = 'Grizzly Bears'
GIANT = 'Hill Giant'
HAWK = 'Suntail Hawk'
KILL = 'Murder'
COUNTER = 'Counterspell'
TAP = 'Pressure Point'
GROW = 'Giant Growth'
DECK = ('Island', 'Forest', 'Mountain', 'Plains')
TOKENS = {'mentor': ('Monk', 18789, 1, 1, 32), 'bounty': ('Beast', 18695, 3, 3, 8),
          'teachings': ('Djinn Monk', 18718, 2, 2, 4)}
WITNESSES = {AMULET, BEAR, GIANT, HAWK, KILL, COUNTER, TAP, GROW, *DECK, *(t[0] for t in TOKENS.values())}
VANILLA = {BEAR, GIANT, HAWK, 'Djinn Monk'}
GAPS = {
    'ascendancy': ('The engine executes the team buff and optional loot through a single callback. '
                   'The fixture exercises both effects and the decline, with no independent trigger-order UI.',),
    'quiet': ('Real payable and unfunded trigger gates and both untap boundaries are observed. '
              'The legacy invocation exposes no independent decline-payment menu with an exact pool.',),
    'exemplars': ('The blink checks a new unmodified tapped card under its owner. Temporary control transfer is outside the fixture.',),
    'teachings': ('The funded and unfunded legacy payment branches are observed. A real decline menu is not exposed with this pool.',),
}


@lru_cache(maxsize=1)
def profiles():
    return json.loads(PROFILE.read_text(encoding='utf8'))


def classify(record):
    from recipes import cost_from_oracle

    if not record or record.get('layout') != 'normal' or record.get('card_faces') or record.get('selected_face'):
        raise ValueError('whole normal noncreature-trigger permanent required')
    card_type = record['type_line'].split(' — ')[0]
    shape = next((key for key, (_, typ, labels, text) in SHAPES.items()
                  if typ == card_type and list(labels) == record['keywords'] and text == record['oracle_text']), None)
    if shape is None:
        raise ValueError('whole supported noncreature trigger and complete printed riders required')
    return dict(role=shape, mana=cost_from_oracle(record))


def variants(params):
    return [f'{mode}_{seat}' for seat in (0, 1) for mode in COMMON + EXTRAS[params['role']]]


def check_variants(params, selected):
    if (type(selected) is not list or not selected or any(type(v) is not str for v in selected)
            or len(set(selected)) != len(selected) or set(selected) - set(variants(params))):
        raise ValueError('distinct reviewed noncreature trigger variants required')


def spec(declaration, record):
    params = classify(record)
    if (declaration.get('family') != FAMILY or type(declaration.get('version')) is not int or declaration['version'] != 2
            or profiles()['sources'].get(record['name'], {}).get('oracle') != record
            or declaration.get('oracle_text') != record['oracle_text']
            or json.dumps(declaration.get('params'), sort_keys=True) != json.dumps(params, sort_keys=True)):
        raise ValueError('declaration must equal whole pinned noncreature trigger Oracle')
    check_variants(params, declaration.get('variants'))
    return dict(params, family=FAMILY)


def validate_witnesses(oracle, rows):
    data = profiles()
    excluded = {r['csvid'] for r in json.loads((PROFILE.parent / 'unimplemented.json').read_text(
        encoding='utf8'))['cases'].values()}
    if data.get('version') != 1 or not data.get('sources') or set(data.get('witnesses', ())) != WITNESSES:
        raise ValueError('complete pinned noncreature trigger witnesses required')
    tokens = {row[0] for row in TOKENS.values()}
    for group in ('sources', 'witnesses'):
        for name, pin in data[group].items():
            row, record = rows.get(name, {}), oracle.get(name, {})
            if (pin != dict(oracle=record, registry=row) or row.get('kind') != 'card' or row['csvid'] in excluded
                    or record.get('layout') != ('token' if name in tokens else 'normal')
                    or (row['handler'] in (None, 'NULL', '0', 'card_dummy') and name not in VANILLA)):
                raise ValueError(f'changed pinned noncreature source or witness: {name}')
            if group == 'sources' and row['handler'] != 'manalinkeh_card_' + SHAPES[classify(record)['role']][0]:
                raise ValueError(f'changed noncreature trigger production callback: {name}')
    if any(rows[name]['csvid'] != csvid for name, csvid, *_ in TOKENS.values()):
        raise ValueError('exact printed token creation identities required')


class NoncreatureBoard(CastTriggerBoard):
    def __init__(self, name, params, scene, owner, rows, oracle):
        super().__init__(name, params, scene, owner, rows, oracle)
        self.name = name
        self.role = params['role']
        self.active = owner
        self.life = [20, 20]
        self.library = [list(DECK), list(reversed(DECK))]
        self.temp = {}
        self.counters = {}
        self.token_refs = []
        self.lore = 0
        self.animated = False

    def observe(self, ref, prop, val):
        self.emit('assert_card', ref=ref, property=prop, value=val)

    def zone_count(self, player, zone, name, val):
        self.emit('assert_zone_multiplicity', player=player, zone=zone, index=self.rows[name]['csvid'], value=val)

    def stats(self, ref):
        card = self.cards[ref]
        oracle = self.oracle[card['name']]
        orig = TOKENS[self.role][2:4] if ref in self.token_refs else (int(oracle['power']), int(oracle['toughness']))
        bonus = self.temp.get(ref, (0, 0))
        return tuple(base + delta + self.counters.get(ref, 0) for base, delta in zip(orig, bonus))

    def mod(self, ref, dp, dt):
        p, t = self.temp.get(ref, (0, 0))
        self.temp[ref] = (p + dp, t + dt)

    def draw(self, player):
        top = self.library[player].pop(0)
        self.cards[next(ref for ref, card in self.cards.items() if card['zone'] == 'library'
                        and card['player'] == player and card['name'] == top)]['zone'] = 'hand'
        return top

    def token(self):
        name, csvid, _, _, _ = TOKENS[self.role]
        index = len(self.token_refs)
        ref = f'generated_{index}'
        self.cards[ref] = dict(name=name, player=self.owner, zone='battlefield', tapped=False, damage=0)
        self.token_refs.append(ref)
        self.emit('assert_zone_multiplicity', player=self.owner, zone='battlefield', index=csvid, value=index+1)
        self.emit('bind_permanent_copy', ref=ref, player=self.owner, index=index, value=csvid)

    def shapes(self):
        for ref, card in self.cards.items():
            if card['zone'] != 'battlefield':
                continue
            self.observe(ref, 'in_play', 1)
            self.observe(ref, 'tapped', int(card['tapped']))
            if 'Creature' in types(self.oracle[card['name']]):
                p, t = self.stats(ref)
                self.observe(ref, 'power', p)
                self.observe(ref, 'toughness', t)
                self.observe(ref, 'damage', card['damage'])
                self.observe(ref, 'plus1_counters', self.counters.get(ref, 0))
            if ref in self.token_refs:
                self.observe(ref, 'color', TOKENS[self.role][4])
        if self.role == 'myth':
            self.emit('assert_counter', ref='source', counter_kind='lore', value=self.lore)
            if self.mode in ('activate_both', 'expiry'):
                self.emit('can_target_creature', ref='probe', other='source', value=int(self.animated))
            if self.animated:
                self.observe('source', 'power', self.lore)
                self.observe('source', 'toughness', self.lore)

    def receipts(self):
        self.emit('assert_unused_choices', value=0)
        self.emit('assert_stack', value=0)
        self.emit('refresh')
        self.observe_mana()
        self.shapes()
        if self.role in TOKENS:
            name, csvid, _, _, color = TOKENS[self.role]
            if self.token_refs:
                for key, value in (('color', color), ('token', 1), ('artifact', 0)):
                    self.emit('assert_matching_permanents', player=self.owner, index=csvid, property=key, value=value)
        for player in (0, 1):
            self.emit('assert_life', player=player, value=self.life[player])
            for zone in ('battlefield', 'hand', 'graveyard', 'exile', 'library'):
                cards = self.library[player] if zone == 'library' else [card['name'] for card in self.cards.values()
                    if card['player'] == player and card['zone'] == zone]
                self.emit('assert_zone', player=player, zone=zone, value=len(cards))
                if zone == 'library':
                    for pos, name in enumerate(cards):
                        self.emit('assert_zone_card', player=player, zone=zone, index=pos, value=self.rows[name]['csvid'])
                else:
                    for name, qty in Counter(cards).items():
                        self.zone_count(player, zone, name, qty)
                    if self.name not in cards:
                        self.zone_count(player, zone, self.name, 0)
                    if self.role in TOKENS and TOKENS[self.role][0] not in cards:
                        self.zone_count(player, zone, TOKENS[self.role][0], 0)

    def cleanup(self):
        self.emit('cleanup')
        self.emit('turn', player=self.active, value=0)
        self.temp.clear()
        self.animated = False
        for card in self.cards.values():
            card['damage'] = 0
        self.receipts()

    def advance(self, prevented=()):
        if any(any(pool) for pool in self.pools):
            raise ValueError('true untap requires empty pools')
        self.emit('advance_turn')
        self.active = 1 - self.active
        self.emit('untap_step', player=self.active)
        self.emit('turn', player=self.active, value=0)
        self.temp.clear()
        self.animated = False
        for ref, card in self.cards.items():
            card['damage'] = 0
            if card['zone'] == 'battlefield' and card['player'] == self.active and ref not in prevented:
                card['tapped'] = False

    def combat(self, attacker, *, legal_block=False, repeats=1, first_strike=False, lifelink=False, extra_ground=False, declare_blocker=True):
        self.emit('begin_attackers')
        self.emit('attack', ref=attacker, value=1)
        self.cards[attacker]['tapped'] = True
        self.emit('finish_attackers')
        self.emit('begin_blockers')
        if extra_ground:
            self.emit('block', ref='ground', other=attacker, value=0)
        if declare_blocker:
            self.emit('block', ref='blocker', other=attacker, value=int(legal_block))
        self.emit('advance_combat')
        self.observe(attacker, 'blocked', int(legal_block))
        self.emit('combat_damage')
        self.emit('resolve')
        self.emit('end_combat')
        power, toughness = self.stats(attacker)
        dealt = power * repeats
        if legal_block:
            bp, bt = self.stats('blocker')
            if first_strike and power >= bt:
                dealt = power
            else:
                self.cards[attacker]['damage'] += bp
                if bp >= toughness:
                    self.cards[attacker]['zone'] = 'graveyard'
            self.cards['blocker']['damage'] += dealt
            if dealt >= bt:
                self.cards['blocker']['zone'] = 'graveyard'
        else:
            self.life[1 - self.active] -= dealt
        if lifelink:
            self.life[self.active] += dealt
        self.receipts()

    def grant_fee(self):
        if self.mode == 'unfunded':
            return [0] * 6
        return {'quiet': [1, 0, 0, 0, 0, 0], 'shu': [0, 0, 0, 0, 0, 2] if self.mode == 'white' else [0, 0, 0, 0, 2, 0],
                'teachings': [1, 0, 1, 0, 0, 0]}.get(self.role, [0] * 6)

    def choices(self, _board):
        seat, role, mode = self.owner, self.role, self.mode
        if role == 'ascendancy':
            self.emit('choose_dialog', player=seat, value=int(mode != 'loot'))
        elif role == 'bounty' and mode != 'no_recipient':
            self.emit('choose_target', ref='ally', player=seat)
        elif role == 'quiet' and mode != 'unfunded':
            # The sole cast trigger resolves without a trigger-source menu.
            self.emit('choose_target', ref='foe_creature', player=seat)
        elif role == 'mistfire':
            self.emit('choose_target', ref='blocker' if mode == 'enemy_lift' else 'ally', player=seat)
        elif role == 'shu' and mode != 'unfunded':
            self.emit('choose_dialog', player=seat, value=int(mode == 'decline'))
            if mode != 'decline':
                self.emit('choose_target', ref='ally', player=seat)
        elif role == 'exemplars':
            choice = 1 if mode == 'strike_link' else 2 if mode == 'blink' else 0
            self.emit('choose_dialog', player=seat, value=choice)
            if choice == 0:
                self.emit('choose_target', ref='foe_creature', player=seat)
        elif role == 'skywise':
            self.emit('choose_color', player=seat, value=5)

    def cast_reward(self):
        role, mode = self.role, self.mode
        if role == 'ascendancy':
            for ref, card in self.cards.items():
                if card['player'] == self.owner and card['zone'] == 'battlefield' and 'Creature' in types(self.oracle[card['name']]):
                    self.mod(ref, 1, 1)
                    card['tapped'] = False
            if mode == 'loot':
                discarded = self.draw(self.owner)
                self.cards[next(ref for ref, card in self.cards.items() if card['player'] == self.owner
                                and card['zone'] == 'hand' and card['name'] == discarded)]['zone'] = 'graveyard'
        elif role == 'bounty' and mode != 'no_recipient':
            self.counters['ally'] = self.counters.get('ally', 0) + 3
        elif role in ('quiet', 'shu', 'teachings'):
            if mode not in ('unfunded', 'decline'):
                self.pools[self.owner] = [have - price for have, price in zip(self.pools[self.owner], self.grant_fee())]
                if role == 'quiet':
                    self.cards['foe_creature']['tapped'] = True
                elif role == 'teachings':
                    self.token()
        elif role == 'mentor':
            for ref in ('source', *self.token_refs):
                self.mod(ref, 1, 1)
            self.token()
        elif role == 'fist':
            self.mod('source', 1, 0)
        elif role == 'myth':
            self.lore += 1
        elif role == 'exemplars' and mode != 'strike_link':
            if mode == 'blink':
                self.emit('bind_permanent', ref='source', player=self.owner, value=self.rows[self.name]['csvid'])
                self.temp.pop('source', None)
                self.cards['source']['tapped'] = True
            else:
                self.cards['foe_creature']['tapped'] = True
        elif role == 'skywise':
            self.cards['source']['tapped'] = False
        if role in ('mistfire', 'shu'):
            self.mod('source', 1, 1)

    def witness(self, ref='witness', *, countered=False, trigger=True):
        beneficiary = self.cards[ref]['player'] == self.owner
        creature = 'Creature' in types(self.oracle[self.cards[ref]['name']])
        earns = trigger and beneficiary and not creature
        if earns:
            extra = self.grant_fee()
            funding = list(extra)
            funding[0] += 1
        else:
            funding = None
        self.cast_trigger_spell(ref, funding=funding, choices=self.choices if earns else None, resolve=False)
        if earns:
            self.cast_reward()
        elif beneficiary and creature and self.role == 'bounty':
            self.token()
        self.emit('assert_unused_choices', value=0)
        self.emit('assert_stack', value=1)
        if earns and self.role in ('bounty', 'mentor'):
            self.shapes()
        if countered:
            self.cast('counter', response=True)
            self.emit('resolve_response')
            self.cards['counter']['zone'] = 'graveyard'
        else:
            self.emit('resolve')
        self.cards[ref]['zone'] = 'graveyard' if countered else 'battlefield'
        self.receipts()

    def removal(self, survives):
        self.cast('kill', target='source')
        self.emit('resolve')
        self.cards['kill']['zone'] = 'graveyard'
        if not survives:
            self.cards['source']['zone'] = 'graveyard'
        self.receipts()

    def myth_mode(self, label, cost):
        self.fund(self.owner, cost)
        self.emit('choose_activation_mode', ref='source', player=self.owner, mode_label=label,
                  fallback_color=0, allow_auto_nonmana=False)
        self.emit('activate', ref='source', value=1)
        self.emit('require_activation_mode', value=1)
        self.emit('assert_stack', value=1)
        self.pools[self.owner] = [0] * 6
        self.observe_mana()
        self.emit('resolve')
        if label == 'Lore counter':
            self.lore += 1
        else:
            self.animated = True
        self.receipts()


FIGHTING = {'combat', 'ally_lifelink', 'enemy_lift', 'red', 'white', 'decline', 'strike_link', 'protect', 'evasion'}


def fixtures(w):
    role, mode, owner, foe = w.role, w.mode, w.owner, w.foe
    for player in (0, 1):
        for pos, name in enumerate(w.library[player]):
            w.add(name, f'deck_{player}_{pos}', player, 'library')
    if role in ('ascendancy', 'bounty', 'sage', 'mistfire', 'shu') and mode != 'no_recipient':
        w.add(HAWK if mode == 'enemy_lift' else BEAR, 'ally', owner, tapped=role == 'ascendancy')
    if role == 'ascendancy':
        w.add(GIANT, 'other_ally', owner, tapped=True)
    if role in ('ascendancy', 'quiet', 'bounty', 'exemplars'):
        w.add(BEAR, 'foe_creature', foe, tapped=role == 'ascendancy')
    if mode in ('landfall', 'foe_land'):
        w.add('Forest', 'land', owner if mode == 'landfall' else foe, 'hand')
    else:
        w.add(BEAR if mode in ('creature', 'no_reward') else AMULET, 'witness', foe if mode == 'opponent' else owner, 'hand')
    if mode == 'countered':
        w.add(COUNTER, 'counter', foe, 'hand')
    if mode == 'second':
        w.add(AMULET, 'second', owner, 'hand')
    if role == 'cathar' and mode in ('survives', 'expired', 'no_reward', 'replay'):
        w.add(KILL, 'kill', foe, 'hand')
    if role == 'myth' and mode in ('activate_both', 'expiry'):
        w.add(KILL, 'probe', foe, 'hand')
    if role == 'skywise' and mode in ('protect', 'expiry'):
        w.add(TAP, 'tap_spell', foe, 'hand')
        w.add(BEAR, 'ground', foe)
    if role == 'exemplars' and mode == 'blink':
        w.add(GROW, 'growth', foe, 'hand')
    if mode in FIGHTING or mode == 'expiry' and role in ('sage', 'mistfire', 'fist', 'skywise'):
        w.add(HAWK if role == 'skywise' else BEAR, 'blocker', foe)


def play(w):
    role, mode = w.role, w.mode
    w.play_source()
    w.receipts()
    if mode in ('opponent', 'foe_land'):
        w.advance()
        w.receipts()
    if mode in FIGHTING or mode == 'expiry' and role in ('sage', 'mistfire', 'fist', 'skywise'):
        w.advance()
        w.advance()
    if role == 'skywise' and mode in ('protect', 'expiry'):
        w.cast_trigger_spell('tap_spell', target='source')
        w.draw(w.foe)
        w.cards['source']['tapped'] = True
        w.receipts()
    if role == 'exemplars' and mode == 'blink':
        w.cast_trigger_spell('growth', target='source')
        w.mod('source', 3, 3)
        w.receipts()
    if mode in ('landfall', 'foe_land'):
        w.emit('play_land', ref='land', value=1)
        w.emit('resolve')
        w.cards['land']['zone'] = 'battlefield'
        w.life[w.owner] += 3 if mode == 'landfall' else 0
        w.receipts()
        return None
    if role == 'ascendancy' and mode == 'loot':
        w.emit('discard_policy', value=1)
    cut = len(w.steps) if mode == 'replay' else None
    w.witness(countered=mode == 'countered')
    if mode == 'second':
        w.witness('second')
    if role == 'quiet' and mode == 'two_untaps':
        w.advance(prevented=('foe_creature',))
        w.receipts()
        w.advance()
        w.advance()
        w.receipts()
    if mode in ('expiry', 'expired') and role != 'myth':
        w.cleanup()
    if role == 'cathar' and mode in ('survives', 'expired', 'no_reward', 'replay'):
        w.removal(survives=mode in ('survives', 'replay'))
    if role == 'sage' and mode in ('combat', 'expiry'):
        w.combat('source', legal_block=False, lifelink=mode == 'combat')
    if role == 'sage' and mode == 'ally_lifelink':
        w.combat('ally', legal_block=True, lifelink=True)
    if role == 'mistfire' and mode in ('combat', 'expiry'):
        w.combat('ally', legal_block=mode == 'expiry')
    if role == 'mistfire' and mode == 'enemy_lift':
        w.combat('ally', legal_block=True)
    if role == 'shu' and mode in ('red', 'white', 'decline'):
        # A defender may leave a legal attacker unblocked.
        w.combat('ally', repeats=1 if mode == 'decline' else 2, declare_blocker=False)
    if role == 'fist' and mode in ('combat', 'expiry'):
        w.combat('source', legal_block=mode == 'expiry')
    if role == 'myth' and mode in ('activate_both', 'expiry'):
        w.myth_mode('Lore counter', [2, 0, 0, 0, 0, 1])
        w.myth_mode('Animate', [0, 0, 0, 0, 0, 1])
        if mode == 'expiry':
            w.cleanup()
    if role == 'exemplars' and mode == 'strike_link':
        w.combat('source', legal_block=True, first_strike=True, lifelink=True)
    if role == 'skywise' and mode in ('protect', 'expiry'):
        w.combat('source', legal_block=mode == 'expiry', extra_ground=True)
    if role == 'teachings' and mode == 'evasion':
        w.advance()
        w.advance()
        w.combat('generated_0', legal_block=False)
    return cut


def expand(name, csvid, declaration, params, selected, rows, oracle):
    from recipes import step

    check_variants(params, selected)
    validate_witnesses(oracle, rows)
    if profiles()['sources'][name]['registry']['csvid'] != csvid:
        raise ValueError('exact noncreature trigger source CSVID required')
    cases = []
    for variant in selected:
        scene, seat = variant.rsplit('_', 1)
        world = NoncreatureBoard(name, params, scene, int(seat), rows, oracle)
        fixtures(world)
        cut = play(world)
        steps = world.steps
        if cut is not None:
            suffix = steps[cut:]
            steps = (steps[:cut] + [step('capture')] + suffix + [step('record_state'), step('ai_roundtrip'), step('restore')]
                     + copy.deepcopy(suffix) + [step('assert_recorded_state')])
        cases.append(dict(name=f'CastNoncreatureRidersRecipe_{csvid}_{variant}', card=name, fixture='empty_main', seed=2226,
                          setup=world.setup, steps=steps, _recipe=dict(family=FAMILY, version=2, variant=variant,
                              params=declaration['params'], oracle_text=declaration['oracle_text'],
                              untested_components=list(GAPS.get(params['role'], ())))))
    return cases
