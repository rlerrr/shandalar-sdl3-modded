"""Count-based burn with actual typed recipients and live count changes."""
import json
import re
from collections import Counter
from pathlib import Path

from counted_battlefield_damage_recipes import Game as CountGame, counted, types
from library_search_recipes import wave6_exact as exact_pin


FAMILY = 'counted_damage_composition'
DATA = Path(__file__).parent / 'data/counted_damage_composition_wave8_profiles.json'
SCOPE = {'Mountains': 'mountain', 'creatures': 'creature', 'Goblins': 'Goblin', 'Vampires': 'Vampire', 'snow permanents': 'snow'}
RECIPIENT = {'any target': 'any', 'target creature': 'creature', 'target player or planeswalker': 'player_walker',
             'target opponent or planeswalker': 'opponent_walker'}
TRIBES = {'Goblin', 'Vampire'}
WITNESSES = {'Mountain', 'Taiga', 'Forest', 'Island', 'Plains', 'Snow-Covered Island', 'Snow-Covered Mountain',
             'Phyrexian Ironfoot', 'Dazzling Ramparts', 'Elvish Mystic', 'Myr Sire', 'Darksteel Relic',
             'Goblin Raider', 'Boggart Shenanigans', 'Vampire Interloper', 'Vampire Hexmage', 'Amoeboid Changeling',
             'Jace Beleren', 'Tibalt, the Fiend-Blooded', 'Ivory Mask', 'Boomerang', 'Growth Spiral', 'Raise the Alarm',
             'Soldier', 'Counterspell', "Ranger's Guile", 'Think Twice'}
COMMON = ('zero', 'own_mixed', 'opponent_only', 'other_zones', 'no_legal_target', 'wrong_color', 'unfunded',
          'gain_own', 'gain_opponent', 'lose_own', 'lose_opponent', 'replay_update', 'target_bounced', 'countered')
GAINS = {'gain_own', 'gain_opponent', 'replay_update', 'gain_nonmatching'}
WALKERS = {'walker', 'own_walker', 'walker_lethal'}
FIZZLES = {'countered', 'target_bounced', 'target_hexproof'}
RESPONSES = GAINS | FIZZLES | {'lose_own', 'lose_opponent', 'lose_type'}


def pins():
    return json.loads(DATA.read_text(encoding='utf8'))


def mechanism(record):
    from recipes import cost_from_oracle

    if (not record or record.get('layout') != 'normal' or record.get('selected_face') or record.get('card_faces')
            or record.get('type_line') not in ('Instant', 'Sorcery') or record.get('keywords') != []):
        raise ValueError('whole ordinary fixed-cost instant or sorcery required')
    pattern = (re.escape(record['name']) + r' deals damage to (' + '|'.join(RECIPIENT)
               + r') equal to the number of (' + '|'.join(SCOPE) + r') you control\.(\nDraw a card\.)?')
    match = re.fullmatch(pattern, record['oracle_text'])
    if not match:
        raise ValueError('whole exact count damage, target scope and optional draw required')
    target, count, draw = RECIPIENT[match[1]], SCOPE[match[2]], bool(match[3])
    price = cost_from_oracle(record)
    if price[4] < 1 or sum(price[1:]) != price[4] or draw != (target == 'opponent_walker' and count == 'Vampire'):
        raise ValueError('unsupported color fee or additional draw clause')
    return dict(scope=count, recipient=target, draw=draw, printed=price, instant=record['type_line'] == 'Instant',
                clauses=record['oracle_text'].splitlines())


def variants(shape):
    target, scope = shape['recipient'], shape['scope']
    modes = COMMON
    if target != 'creature':
        modes += ('walker', 'own_walker', 'walker_lethal')
    if target in ('any', 'player_walker'):
        modes += ('self_player',)
    if target in ('any', 'creature'):
        modes += ('own_creature', 'creature_lethal', 'target_hexproof')
    if target == 'any':
        modes += ('enemy_creature',)
    if scope in TRIBES:
        modes += ('lose_type',)
    if scope in ('snow', 'mountain'):
        modes += ('gain_nonmatching',)
    if shape['instant']:
        modes += ('offturn_response',)
    return [f'{mode}_{owner}' for mode in modes for owner in (0, 1)]


def spec(declaration, record):
    shape = mechanism(record)
    source = pins()['sources'].get(record['name'])
    params = declaration.get('params')
    if (not source or not exact_pin(source['oracle'], record) or declaration.get('family') != FAMILY
            or type(declaration.get('version')) is not int or declaration['version'] != 2
            or declaration.get('oracle_text') != record['oracle_text'] or not isinstance(params, dict)
            or set(params) != {'schema', 'mechanism'} or type(params['schema']) is not int or params['schema'] != 1
            or not exact_pin(params['mechanism'], shape)):
        raise ValueError('whole pinned scope and real fixed printed payment required')
    return dict(shape, csvid=source['csvid'], family=FAMILY)


def validate_witnesses(oracle, rows):
    data = pins()
    if set(data) != {'sources', 'witnesses'} or set(data['witnesses']) != WITNESSES:
        raise ValueError('complete independent count and target witnesses required')
    for group in ('sources', 'witnesses'):
        for name, pin in data[group].items():
            row = rows.get(name, {})
            if (not exact_pin(pin['oracle'], oracle.get(name)) or row.get('csvid') != pin['csvid']
                    or row.get('handler') != pin['handler'] or row.get('kind') != 'card'
                    or row.get('handler') in (None, 'NULL', '0', 'card_dummy')
                    or oracle[name].get('layout') != ('token' if name == 'Soldier' else 'normal')):
                raise ValueError('implemented exact count-damage source or witness changed: ' + name)
            if group == 'sources':
                mechanism(oracle[name])


class Game(CountGame):
    def build(self):
        mode, owner, foe = self.mode, self.owner, self.foe
        scope, recipient = self.shape['scope'], self.shape['recipient']
        self.life = [20, 20]
        self.type_effects = {}
        roster = {'mountain': ('Taiga', 'Mountain'), 'creature': ('Elvish Mystic', 'Myr Sire'),
                  'snow': ('Snow-Covered Island', 'Phyrexian Ironfoot'),
                  'Goblin': ('Goblin Raider', 'Boggart Shenanigans', 'Amoeboid Changeling'),
                  'Vampire': ('Vampire Interloper', 'Vampire Hexmage', 'Amoeboid Changeling')}[scope]
        n = {'own_mixed': len(roster), 'walker_lethal': 2, 'creature_lethal': 1, 'self_player': 1, 'walker': 1,
             'own_walker': 1, 'enemy_creature': 1, 'own_creature': int(scope != 'creature'), 'lose_type': 1,
             'gain_opponent': 1, 'lose_own': 1, 'lose_opponent': 1, 'replay_update': 1, 'gain_nonmatching': 1,
             'target_bounced': 1, 'target_hexproof': 1, 'countered': 1, 'offturn_response': 1}.get(mode, 0)
        for index, name in enumerate(roster[:n]):
            self.add(name, f'own_{index}', owner, tapped=True)
        if mode in ('own_mixed', 'opponent_only', 'lose_opponent'):
            for index, name in enumerate(roster):
                self.add(name, f'enemy_{index}', foe, tapped=True)
        if mode in ('zero', 'own_mixed', 'opponent_only', 'other_zones', 'no_legal_target'):
            self.add('Darksteel Relic', 'nonmatching', owner)
        if mode == 'other_zones':
            for zone in ('hand', 'graveyard', 'exile', 'library'):
                self.add(roster[0], f'out_{zone}', owner, zone)

        self.target = owner if mode == 'self_player' else foe
        if mode in WALKERS or (mode == 'target_bounced' and recipient in ('player_walker', 'opponent_walker')):
            self.target = 'walker'
            self.add('Tibalt, the Fiend-Blooded' if mode == 'walker_lethal' else 'Jace Beleren', self.target,
                     owner if mode == 'own_walker' else foe)
        if mode != 'no_legal_target' and (recipient == 'creature' or mode in ('own_creature', 'enemy_creature', 'creature_lethal', 'target_hexproof')
                or mode == 'target_bounced' and recipient == 'any'):
            self.target = 'target'
            self.add('Elvish Mystic' if mode == 'creature_lethal' else 'Dazzling Ramparts', self.target,
                     owner if mode == 'own_creature' else foe, tapped=mode == 'creature_lethal')
        if mode == 'no_legal_target':
            if recipient != 'creature':
                self.add('Ivory Mask', 'enemy_shroud', foe)
                if recipient != 'opponent_walker':
                    self.add('Ivory Mask', 'own_shroud', owner)
                if recipient != 'any':
                    self.add('Dazzling Ramparts', 'ineligible_creature', foe)
            else:
                self.add('Jace Beleren', 'ineligible_walker', foe)
        for who in (0, 1):
            for index, name in enumerate(('Island', 'Plains', 'Forest')):
                self.add(name, f'library_{who}_{index}', who, 'library')

        if mode in GAINS and scope in TRIBES or mode == 'lose_type':
            self.add('Amoeboid Changeling', 'type_ability', foe)
            if mode != 'lose_type':
                self.add('Elvish Mystic', 'type_subject', foe if mode == 'gain_opponent' else owner, tapped=True)
        elif mode in GAINS:
            actor = foe if mode == 'gain_opponent' else owner
            self.add('Raise the Alarm' if scope == 'creature' else 'Growth Spiral', 'support', actor, 'hand')
            if scope != 'creature':
                land = 'Forest' if mode == 'gain_nonmatching' else {'mountain': 'Mountain', 'snow': 'Snow-Covered Mountain'}[scope]
                self.add(land, 'new_land', actor, 'hand')
        elif mode in RESPONSES or mode == 'offturn_response':
            support = {'target_hexproof': "Ranger's Guile", 'countered': 'Counterspell', 'offturn_response': 'Think Twice'}.get(mode, 'Boomerang')
            self.add(support, 'support', owner if mode == 'lose_own' else foe, 'hand')
        for entry in self.cards.values():
            if 'Planeswalker' in types(self.oracle[entry['name']]):
                entry['loyalty'] = int(self.oracle[entry['name']]['loyalty'])

    def source_cast(self, *, funding, allowed, response):
        if not allowed or type(self.target) is str:
            self.cast('source', target=self.target if allowed else None, funding=funding, allowed=allowed, response=response)
            return
        self.fund(self.owner, funding)
        self.emit('choose_player', player=self.owner, value=self.target)
        self.emit('cast_response' if response else 'cast', ref='source', value=1)
        self.emit('assert_choices_consumed')
        self.emit('assert_stack', value=1+int(response))
        self.cards['source']['zone'] = 'stack'
        self.pools[self.owner] = [have-due for have, due in zip(funding, self.shape['printed'])]
        self.observe_mana()

    def count(self):
        scope = self.shape['scope']
        total = 0
        for ref, entry in self.cards.items():
            if entry['player'] != self.owner or entry['zone'] != 'battlefield':
                continue
            record = self.oracle[entry['name']]
            if scope in TRIBES:
                matching = self.type_effects.get(ref, scope in record['type_line'].partition(' — ')[2].split()
                                                 or 'Changeling' in record['keywords'])
            elif scope == 'snow':
                matching = 'Snow' in types(record)
            else:
                matching = counted(record, scope)
            total += matching
        return total

    def apply_damage(self):
        amount = self.count()
        if type(self.target) is int:
            self.life[self.target] -= amount
        else:
            target = self.cards[self.target]
            if 'loyalty' in target:
                target['loyalty'] = max(0, target['loyalty']-amount)
                if target['loyalty'] == 0:
                    target['zone'] = 'graveyard'
            else:
                super().apply_damage()
        if self.shape['draw']:
            self.draw(self.owner)

    def before_parent(self):
        mode = self.mode
        self.emit('announce_spell', ref='source')
        if 'type_ability' in self.cards:
            subject = 'own_0' if mode == 'lose_type' else 'type_subject'
            self.emit('can_activate', ref='type_ability', value=1)
            self.emit('choose_target', player=self.foe, ref=subject)
            self.emit('choose_dialog', player=self.foe, value=int(mode == 'lose_type'))
            self.emit('activate', ref='type_ability', value=1)
            self.emit('assert_stack', value=2)
            self.cards['type_ability']['tapped'] = True
            self.type_effects[subject] = mode != 'lose_type'
        else:
            target = {'lose_own': 'own_0', 'lose_opponent': 'enemy_0', 'target_bounced': self.target,
                      'target_hexproof': self.target}.get(mode)
            self.cast('support', target=target, response=True)
            support = self.cards['support']
            who = support['player']
            if support['name'] == 'Growth Spiral':
                new_land = self.cards['new_land']
                self.emit('choose_zone_card', player=who, value=self.rows[new_land['name']]['csvid'])
                self.draw(who)
                new_land['zone'] = 'battlefield'
            if support['name'] == 'Raise the Alarm':
                for index in range(2):
                    self.cards[f'soldier_{index}'] = dict(name='Soldier', player=who, zone='battlefield', tapped=False, damage=0, boost=0)
            if support['name'] == 'Boomerang':
                self.cards[target]['zone'] = 'hand'
            if mode == 'target_hexproof':
                self.cards[self.target]['boost'] = 1
            support['zone'] = 'graveyard'
        self.emit('resolve_response')
        self.emit('refresh')
        if 'new_land' in self.cards:
            self.bind('new_land')
        for index in range(2):
            if f'soldier_{index}' in self.cards:
                self.bind(f'soldier_{index}', index)

    def receipts(self):
        self.emit('assert_choices_consumed')
        self.emit('assert_stack', value=0)
        self.observe_mana()
        quantities = Counter((entry['player'], entry['zone'], self.rows[entry['name']]['csvid']) for entry in self.cards.values())
        for player, life in enumerate(self.life):
            self.emit('assert_life', player=player, value=life)
            for zone in ('battlefield', 'hand', 'graveyard', 'exile', 'library'):
                self.emit('assert_zone', player=player, zone=zone, value=sum(n for (p, z, _), n in quantities.items() if p == player and z == zone))
            library = [entry for entry in self.cards.values() if entry['player'] == player and entry['zone'] == 'library']
            for index, card in enumerate(library):
                self.emit('assert_zone_card', player=player, zone='library', index=index, value=self.rows[card['name']]['csvid'])
        for (player, zone, index), value in sorted(quantities.items()):
            self.emit('assert_zone_multiplicity', player=player, zone=zone, index=index, value=value)
        for ref, card in self.cards.items():
            if card['zone'] != 'battlefield':
                continue
            self.emit('assert_card', ref=ref, property='tapped', value=int(card['tapped']))
            record = self.oracle[card['name']]
            if 'loyalty' in card:
                self.emit('assert_counter', ref=ref, counter_kind='loyalty', value=card['loyalty'])
            if 'Creature' in types(record):
                for prop, value in [('power', int(record['power'])+card['boost']), ('toughness', int(record['toughness'])+card['boost']), ('damage', card['damage'])]:
                    self.emit('assert_card', ref=ref, property=prop, value=value)
            if card['name'] == 'Soldier':
                for prop, value in [('token', 1), ('subtype_Soldier', 1), ('color', 32)]:
                    self.emit('assert_matching_permanents', player=card['player'], index=self.rows['Soldier']['csvid'], property=prop, value=value)
            if ref in self.type_effects:
                # Goblin and Elf are available both as positive changeling
                # contrasts. The separate Vampire count is checked by damage.
                for trait in ('Goblin', 'Elf'):
                    self.emit('assert_matching_permanents', player=card['player'], index=self.rows[card['name']]['csvid'],
                              property=f'subtype_{trait}', value=int(self.type_effects[ref]))


def _case(name, csvid, declaration, shape, variant, rows, oracle):
    from recipes import COLORS, step

    mode, seat = variant.rsplit('_', 1)
    owner = int(seat)
    game = Game(name, shape, mode, owner, rows, oracle)
    emit = game.emit
    emit('turn', player=owner, value=0)
    emit('reset_death_history')
    for player in (0, 1):
        game.fund(player, [0]*6)
    game.receipts()
    offturn = mode == 'offturn_response'
    if offturn:
        emit('advance_turn')
        game.cast('support')
        emit('announce_spell', ref='support')
    price = list(shape['printed'])
    if mode == 'wrong_color':
        price[COLORS['U']], price[COLORS['R']] = price[COLORS['R']], 0
    if mode == 'unfunded':
        price[next(color for color, amount in enumerate(price) if amount)] -= 1
    denied = mode in ('unfunded', 'wrong_color', 'no_legal_target')
    if not offturn:
        emit('can_cast', ref='source', value=int(mode != 'no_legal_target'))
    game.source_cast(funding=price, allowed=not denied, response=offturn)
    replay = None
    if not denied:
        if mode == 'replay_update':
            replay = len(game.steps)
        if mode in RESPONSES:
            game.before_parent()
        else:
            emit('resolve_response' if offturn else 'resolve')
            emit('refresh')
            if offturn:
                game.draw(game.foe)
                game.cards['support']['zone'] = 'graveyard'
        if mode not in FIZZLES:
            game.apply_damage()
        game.cards['source']['zone'] = 'graveyard'
    game.receipts()
    if replay is not None:
        suffix = game.steps[replay:]
        game.steps = (game.steps[:replay] + [step('capture')] + suffix
                      + [step('record_state'), step('restore'), step('ai_roundtrip')] + suffix + [step('assert_recorded_state')])
    metadata = dict(family=FAMILY, version=2, variant=variant, params=declaration['params'],
                    oracle_text=declaration['oracle_text'], covered_clauses=shape['clauses'], untested_components=[
                        'Battle targeting, where no Battle witness is available in this registered ruleset.',
                        'Sorcery timing rejection at the UI permission layer, changing control during resolution, and multiplayer.'])
    return dict(name=f'CountedDamageCompositionRecipe_{csvid}_{variant}', card=name, fixture='empty_main', seed=123,
                setup=game.setup, steps=game.steps, _recipe=metadata)


def expand(name, csvid, declaration, shape, selected, rows, oracle):
    if csvid != shape['csvid'] or not selected or len(set(selected)) != len(selected) or set(selected) - set(variants(shape)):
        raise ValueError('distinct reviewed count-damage alternatives required')
    validate_witnesses(oracle, rows)
    return [_case(name, csvid, declaration, shape, variant, rows, oracle) for variant in selected]
