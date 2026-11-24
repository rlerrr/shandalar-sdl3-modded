"""Exercise seven complete Heroic creatures through paid spell announcements."""
from collections import Counter
import json
from pathlib import Path

from heroic_case_support import HeroicBoard, heroic_rules
from library_search_recipes import wave6_exact
from recipes import cost_from_oracle


FAMILY = 'heroic_basic_riders'
PROFILE = Path(__file__).parent / 'data/heroic_basic_riders_wave8_profiles.json'
TRIGGER = 'Heroic — Whenever you cast a spell that targets '
EFFECTS = {
    'Akroan Crusader': ('soldier', (TRIGGER + 'this creature, create a 1/1 red Soldier creature token with haste. '
                                  '(It can attack and {T} as soon as it comes under your control.)',),
                        ('token_attack',), ()),
    'Akroan Line Breaker': ('intimidate', (TRIGGER + 'this creature, this creature gets +2/+0 and gains intimidate until end of turn. '
                                        "(It can't be blocked except by artifact creatures and/or creatures that share a color with it.)",),
                           ('different_color', 'same_color', 'artifact', 'expires'), ()),
    'Anax and Cymede': ('team_pump', ('First strike, vigilance',
                        TRIGGER + 'Anax and Cymede, creatures you control get +1/+1 and gain trample until end of turn.'),
                        ('team_trample', 'base_combat', 'expires'), ('First strike', 'Vigilance')),
    'Labyrinth Champion': ('damage', (TRIGGER + 'this creature, this creature deals 2 damage to any target.',),
                           ('damage_creature', 'damage_walker', 'damage_self'), ()),
    'Phalanx Leader': ('counters', (TRIGGER + 'this creature, put a +1/+1 counter on each creature you control.',),
                       ('persistent',), ()),
    'Tormented Hero': ('drain', ('This creature enters tapped.',
                                TRIGGER + 'this creature, each opponent loses 1 life. You gain life equal to the life lost this way.'),
                       ('entry_untap',), ()),
    'Vanguard of Brimaz': ('cat', ('Vigilance',
                                  TRIGGER + 'this creature, create a 1/1 white Cat Soldier creature token with vigilance.'),
                           ('token_fresh', 'token_vigilance'), ('Vigilance',)),
}
COMMON = ('own', 'opponent', 'other_target', 'repeat')
GOBLIN = "Mons's Goblin Raiders"
TOKENS = {'soldier': ('Soldier', 16, ('Soldier',)), 'cat': ('Cat Soldier', 32, ('Cat', 'Soldier'))}
DUMMIES = {'Grizzly Bears', GOBLIN}
WITNESSES = {*DUMMIES, 'Defiant Strike', 'Forest', 'Scathe Zombies', 'Wall of Tanglecord',
             'Jace Beleren', 'Soldier', 'Cat Soldier'}


def profiles():
    return json.loads(PROFILE.read_bytes())


def classify(record):
    if not record or record.get('name') not in EFFECTS:
        raise ValueError('unreviewed Heroic creature')
    effect, lines, _, keywords = EFFECTS[record['name']]
    if tuple(heroic_rules(record)) != lines or set(record['keywords']) != {'Heroic', *keywords}:
        raise ValueError('complete Heroic Oracle clauses changed')
    power, toughness = (record[key] for key in ('power', 'toughness'))
    if not power.isdecimal() or not toughness.isdecimal():
        raise ValueError('fixed printed creature characteristics required')
    return dict(name=record['name'], cost=cost_from_oracle(record), effect=effect,
                power=int(power), toughness=int(toughness), keywords=list(keywords))


def variants(shape):
    _, _, extra, _ = EFFECTS[shape['name']]
    return [f'{mode}_{seat}' for mode in (*COMMON, *extra) for seat in (0, 1)]


def spec(declaration, record):
    pinned = profiles()['sources'].get(record['name'])
    if not pinned or not wave6_exact(pinned['oracle'], record):
        raise ValueError('exact whole pinned Oracle Heroic record required')
    shape = classify(record)
    chosen = declaration.get('variants')
    if (declaration.get('family') != FAMILY or type(declaration.get('version')) is not int
            or declaration['version'] != 2 or declaration.get('oracle_text') != record['oracle_text']
            or not wave6_exact(declaration.get('params'), shape) or not isinstance(chosen, list)
            or not chosen or len(set(chosen)) != len(chosen) or set(chosen) - set(variants(shape))):
        raise ValueError('exact whole Heroic declaration and unique reviewed variants required')
    return dict(shape, family=FAMILY)


def validate_witnesses(oracle, rows):
    data = profiles()
    if set(data) != {'sources', 'witnesses'} or set(data['sources']) != set(EFFECTS) or set(data['witnesses']) != WITNESSES:
        raise ValueError('incomplete Heroic card and witness set')
    for group, pins in data.items():
        for name, pin in pins.items():
            row, record = rows[name], oracle[name]
            if (not wave6_exact(pin['oracle'], record) or pin['registry'] != row or row['kind'] != 'card'
                    or row['handler'] in (None, '0', 'NULL') or row['handler'] == 'card_dummy' and name not in DUMMIES):
                raise ValueError('unreviewed Oracle or native registry witness: ' + name)
            if group == 'sources':
                classify(record)


class Board(HeroicBoard):
    def build(self):
        self.effect = self.shape['effect']
        self.life = [20, 20]
        self.counters = Counter()
        self.power_bonus = Counter()
        self.tough_bonus = Counter()
        self.token_count = 0
        self.add('Grizzly Bears', 'ally', self.owner)
        foreign = {'different_color':'Scathe Zombies', 'same_color':GOBLIN,
                   'artifact':'Wall of Tanglecord'}.get(self.mode, 'Scathe Zombies' if self.effect == 'intimidate' else 'Grizzly Bears')
        self.add(foreign, 'foreign', self.foe)
        if self.mode == 'team_trample':
            self.add('Grizzly Bears', 'second', self.foe)
        if self.mode == 'damage_walker':
            self.add('Jace Beleren', 'walker', self.foe)
            self.loyalty = int(self.oracle['Jace Beleren']['loyalty'])
        for seat in (0, 1):
            for index in range(3):
                self.add('Forest', f'library_{seat}_{index}', seat, 'library')
        if self.mode != 'base_combat':
            actor = self.foe if self.mode == 'opponent' else self.owner
            self.add('Defiant Strike', 'trigger', actor, 'hand')
            if self.mode == 'repeat':
                self.add('Defiant Strike', 'again', actor, 'hand')

    def stats(self, ref):
        if ref.startswith('created_'):
            return 1, 1
        card = self.oracle[self.cards[ref]['name']]
        plus = self.counters[ref]
        return int(card['power']) + plus + self.power_bonus[ref], int(card['toughness']) + plus + self.tough_bonus[ref]

    def creature(self, ref, prop, value):
        self.emit('assert_card', ref=ref, property=prop, value=int(value))

    def receipts(self, *, bind_tokens=False):
        self.emit('refresh')
        self.emit('assert_stack', value=0)
        self.emit('assert_choices_consumed')
        self.observe_mana()
        for side in (0, 1):
            self.emit('assert_life', player=side, value=self.life[side])
            for zone in ('hand', 'battlefield', 'graveyard', 'library', 'exile'):
                cards = Counter(self.rows[card['name']]['csvid'] for card in self.cards.values()
                                if card['player'] == side and card['zone'] == zone)
                self.emit('assert_zone', player=side, zone=zone, value=sum(cards.values()))
                if zone != 'library':
                    for csvid, count in sorted(cards.items()):
                        self.emit('assert_zone_multiplicity', player=side, zone=zone, index=csvid, value=count)
        if 'walker' in self.cards:
            self.emit('assert_counter', ref='walker', counter_kind='loyalty', value=self.loyalty)
        if self.effect in TOKENS:
            token, color, subtypes = TOKENS[self.effect]
            csvid = self.rows[token]['csvid']
            for seat, count in ((self.owner, self.token_count), (self.foe, 0)):
                self.emit('assert_zone_multiplicity', player=seat, zone='battlefield', index=csvid, value=count)
            if self.token_count:
                expected = [('token', 1), ('color', color), *[('subtype_' + subtype, 1) for subtype in subtypes]]
                for prop, value in expected:
                    self.emit('assert_matching_permanents', player=self.owner, index=csvid, property=prop, value=value)
                if bind_tokens:
                    for index in range(self.token_count):
                        self.bind(f'created_{index}', index)
        for ref, card in self.cards.items():
            if card['zone'] == 'battlefield' and 'Creature' in self.oracle[card['name']]['type_line']:
                power, toughness = self.stats(ref)
                for prop, value in [('power', power), ('toughness', toughness), ('tapped', card['tapped']),
                                    ('damage', card['damage']), ('plus1_counters', self.counters[ref])]:
                    self.creature(ref, prop, value)

    def choose_damage(self, _):
        if self.mode in ('damage_creature', 'damage_walker'):
            self.emit('choose_target', ref='foreign' if self.mode == 'damage_creature' else 'walker', player=self.owner)
        else:
            self.emit('choose_player', player=self.owner, value=self.owner if self.mode == 'damage_self' else self.foe)

    def reward(self):
        if self.effect in TOKENS:
            token, _, _ = TOKENS[self.effect]
            self.cards[f'created_{self.token_count}'] = dict(name=token, player=self.owner, zone='battlefield',
                                                            tapped=False, damage=0)
            self.token_count += 1
        elif self.effect == 'intimidate':
            self.power_bonus['source'] += 2
        elif self.effect in ('team_pump', 'counters'):
            for ref, card in self.cards.items():
                if card['player'] == self.owner and card['zone'] == 'battlefield':
                    if self.effect == 'team_pump':
                        self.power_bonus[ref] += 1
                        self.tough_bonus[ref] += 1
                    else:
                        self.counters[ref] += 1
        elif self.effect == 'damage':
            if self.mode == 'damage_creature':
                self.cards['foreign']['zone'] = 'graveyard'
            elif self.mode == 'damage_walker':
                self.loyalty -= 2
            else:
                self.life[self.owner if self.mode == 'damage_self' else self.foe] -= 2
        elif self.effect == 'drain':
            self.life[self.owner] += 1
            self.life[self.foe] -= 1

    def trigger(self, spell='trigger'):
        own = self.mode != 'opponent'
        target = 'ally' if self.mode == 'other_target' else 'source'
        eligible_cast = own and target == 'source'
        choices = self.choose_damage if self.effect == 'damage' and eligible_cast else None
        self.heroic_spell(own, spell=spell, target=target, choices=choices)
        if eligible_cast:
            self.reward()
        self.power_bonus[target] += 1
        self.draw(self.owner if own else self.foe)
        self.receipts(bind_tokens=self.effect in TOKENS and bool(self.token_count))

    def own_turn(self):
        self.power_bonus.clear()
        self.tough_bonus.clear()
        for seat in (self.foe, self.owner):
            self.emit('advance_turn')
            self.emit('untap_step', player=seat)
            for card in self.cards.values():
                card['damage'] = 0
                if card['player'] == seat and card['zone'] == 'battlefield':
                    card['tapped'] = False
            if self.effect == 'drain':
                self.creature('source', 'tapped', self.cards['source']['tapped'])
        self.emit('turn', player=self.owner, value=0)

    def fight(self, attackers, blocks=(), *, trample=False, deny=()):
        self.emit('begin_attackers')
        for ref in deny:
            self.emit('attack', ref=ref, value=0)
            self.creature(ref, 'attacking', 0)
        for ref in attackers:
            self.emit('attack', ref=ref, value=1)
            vigilant = ref == 'source' and 'Vigilance' in self.shape['keywords'] or (
                self.effect == 'cat' and ref.startswith('created_'))
            self.cards[ref]['tapped'] = not vigilant
            self.creature(ref, 'tapped', not vigilant)
        self.emit('finish_attackers')
        self.emit('begin_blockers')
        for blocker, attacker, allowed in blocks:
            self.emit('block', ref=blocker, other=attacker, value=int(allowed))
        self.emit('advance_combat')
        actual = {attacker: blocker for blocker, attacker, allowed in blocks if allowed}
        for attacker in attackers:
            self.creature(attacker, 'blocked', attacker in actual)
        self.emit('combat_damage')
        for attacker in attackers:
            blocker = actual.get(attacker)
            power, toughness = self.stats(attacker)
            if blocker is None:
                self.life[self.foe] -= power
                continue
            block_power, block_toughness = self.stats(blocker)
            spill = max(0, power-block_toughness) if trample else 0
            self.life[self.foe] -= spill
            damage = power - spill
            self.cards[blocker]['damage'] = damage
            first_strike = attacker == 'source' and 'First strike' in self.shape['keywords']
            self.cards[attacker]['damage'] = 0 if first_strike and damage >= block_toughness else block_power
            if damage >= block_toughness:
                self.cards[blocker]['zone'] = 'graveyard'
            if self.cards[attacker]['damage'] >= toughness:
                self.cards[attacker]['zone'] = 'graveyard'
        self.emit('resolve')
        self.emit('end_combat')
        self.receipts()

    def run(self):
        self.play_source()
        self.receipts()
        if self.mode in ('different_color','same_color','artifact','team_trample','base_combat'):
            self.own_turn()
        if self.mode != 'base_combat':
            self.trigger()
        if self.mode == 'repeat':
            self.trigger('again')
        elif self.mode in ('expires','persistent','entry_untap','token_vigilance'):
            self.own_turn()
            self.receipts()
        if self.mode in ('different_color','same_color','artifact') or self.mode == 'expires' and self.effect == 'intimidate':
            self.fight(('source',), (('foreign','source',self.mode != 'different_color'),))
        elif self.mode == 'team_trample':
            self.fight(('source','ally'), (('foreign','source',True),('second','ally',True)), trample=True)
        elif self.mode in ('base_combat','expires') and self.effect == 'team_pump':
            self.fight(('source','ally'), (('foreign','source',True),))
        elif self.mode == 'token_attack':
            self.fight(('created_0',))
        elif self.mode == 'token_vigilance':
            self.fight(('source','created_0'))
        elif self.mode == 'token_fresh':
            self.fight((), deny=('created_0',))


def build(name, csvid, declaration, shape, variant, rows, oracle):
    mode, seat = variant.rsplit('_', 1)
    board = Board(name, shape, mode, int(seat), rows, oracle)
    board.run()
    return dict(name=f'HeroicBasicRidersRecipe_{csvid}_{variant}', card=name, fixture='empty_main', seed=2119,
                setup=board.setup, steps=board.steps,
                _recipe=dict(family=FAMILY, version=2, variant=variant, params=declaration['params'],
                             oracle_text=declaration['oracle_text'],
                             untested_components=['Changes to spell targets and multiplayer.']))


def expand(name, csvid, declaration, shape, selected, rows, oracle):
    if rows[name]['csvid'] != csvid or not selected or len(set(selected)) != len(selected) or set(selected) - set(variants(shape)):
        raise ValueError('only distinct reviewed cases for the selected Heroic card')
    validate_witnesses(oracle, rows)
    return [build(name, csvid, declaration, shape, variant, rows, oracle) for variant in selected]
