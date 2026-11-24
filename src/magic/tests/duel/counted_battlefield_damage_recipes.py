"""Creature damage from actual battlefield counts at spell resolution."""
import json
import re
from collections import Counter
from pathlib import Path

from library_search_recipes import wave6_exact as exact_pin


FAMILY = 'counted_battlefield_damage'
DATA = Path(__file__).parent / 'data/counted_battlefield_damage_wave8_profiles.json'
SCOPES = {'Mountains': 'mountain', 'lands': 'land', 'creatures': 'creature'}
WITNESSES = {'Mountain', 'Taiga', 'Forest', 'Island', 'Plains', 'Darksteel Citadel', 'Darksteel Relic',
             'Dazzling Ramparts', 'Elvish Mystic', 'Myr Sire', 'Boomerang', 'Growth Spiral', 'Raise the Alarm',
             'Soldier', 'Counterspell', "Ranger's Guile", 'Think Twice'}
COMMON = ('zero', 'own_mixed', 'opponent_only', 'other_zones', 'no_legal_target', 'wrong_color', 'unfunded',
          'own_target', 'exact_lethal', 'target_bounced', 'target_hexproof', 'countered', 'gain_own', 'gain_opponent',
          'lose_own', 'lose_opponent', 'replay_update', 'cleanup_mark')
GAINS = {'gain_own', 'gain_opponent', 'replay_update', 'gain_nonmountain'}
RESPONSES = GAINS | {'lose_own', 'lose_opponent', 'target_bounced', 'target_hexproof', 'countered'}


def pins():
    return json.loads(DATA.read_text(encoding='utf8'))


def types(record):
    return record['type_line'].partition(' — ')[0].split()


def counted(record, scope):
    printed = types(record)
    if scope == 'mountain':
        return 'Land' in printed and 'Mountain' in record['type_line'].partition(' — ')[2].split()
    return scope.capitalize() in printed


def mechanism(record):
    from recipes import COLORS, cost_from_oracle

    if (not record or record.get('layout') != 'normal' or record.get('selected_face') or record.get('card_faces')
            or record.get('type_line') not in ('Instant', 'Sorcery') or record.get('keywords') != []):
        raise ValueError('whole normal fixed-cost instant or sorcery needed')
    labels = '|'.join(SCOPES)
    pattern = re.escape(record['name']) + rf' deals damage to target creature equal to the number of ({labels}) you control\.'
    match = re.fullmatch(pattern, record['oracle_text'])
    if not match:
        raise ValueError('one creature target and complete controller-specific battlefield count required')
    price = cost_from_oracle(record)
    if price[COLORS['R']] < 1 or price[COLORS['B']] or price[COLORS['U']] or price[COLORS['W']]:
        raise ValueError('reviewed red or red-green payment required')
    return dict(scope=SCOPES[match[1]], printed=price, instant=record['type_line'] == 'Instant', clauses=[record['oracle_text']])


def variants(shape):
    modes = COMMON + (('offturn_response',) if shape['instant'] else ()) + (('gain_nonmountain',) if shape['scope'] == 'mountain' else ())
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
        raise ValueError('whole pinned counting spell and exact printed payment required')
    return dict(shape, csvid=source['csvid'], family=FAMILY)


def validate_witnesses(oracle, rows):
    data = pins()
    if set(data) != {'sources', 'witnesses'} or set(data['witnesses']) != WITNESSES:
        raise ValueError('complete independently pinned update and target witnesses required')
    for group in ('sources', 'witnesses'):
        for name, pin in data[group].items():
            row = rows.get(name, {})
            if (not exact_pin(pin['oracle'], oracle.get(name)) or row.get('csvid') != pin['csvid']
                    or row.get('handler') != pin['handler'] or row.get('kind') != 'card'
                    or row.get('handler') in (None, 'NULL', '0', 'card_dummy')
                    or oracle[name].get('layout') != ('token' if name == 'Soldier' else 'normal')):
                raise ValueError('implemented exact counting source or witness changed: ' + name)
            if group == 'sources':
                mechanism(oracle[name])


class Game:
    def __init__(self, name, shape, mode, owner, rows, oracle):
        self.shape, self.mode, self.owner, self.foe = shape, mode, owner, 1-owner
        self.rows, self.oracle = rows, oracle
        self.cards, self.setup, self.steps = {}, [], []
        self.pools = [[0]*6 for _ in (0, 1)]
        self.add(name, 'source', owner, 'hand')
        self.build()

    def emit(self, op, **fields):
        from recipes import step

        self.steps.append(step(op, **fields))

    def add(self, name, ref, who, zone='battlefield', *, tapped=False):
        from recipes import card

        entry = card(name, ref, who, zone)
        if tapped:
            entry['state'] = ['tapped']
        self.setup.append(entry)
        self.cards[ref] = dict(entry, tapped=tapped, damage=0, boost=0)

    def build(self):
        scope, mode, owner, foe = self.shape['scope'], self.mode, self.owner, self.foe
        own_counted = {'mountain': ('Taiga', 'Mountain'), 'land': ('Darksteel Citadel', 'Mountain', 'Forest'),
                       'creature': ('Elvish Mystic', 'Myr Sire')}[scope]
        matching_count = {'own_mixed': len(own_counted), 'gain_opponent': 1, 'lose_own': 1, 'lose_opponent': 1,
                          'gain_nonmountain': 1, 'replay_update': 1, 'exact_lethal': 1, 'target_bounced': 1,
                          'target_hexproof': 1, 'countered': 1, 'offturn_response': 1, 'cleanup_mark': 1}.get(mode, 0)
        for index, name in enumerate(own_counted[:matching_count]):
            self.add(name, f'own_{index}', owner, tapped=index == 0 or 'Land' in types(self.oracle[name]))
        if mode in ('own_mixed', 'opponent_only', 'lose_opponent'):
            for index, name in enumerate(own_counted):
                self.add(name, f'enemy_{index}', foe, tapped=True)
        if mode in ('own_mixed', 'zero', 'opponent_only', 'no_legal_target', 'other_zones'):
            nonmatching = 'Forest' if scope == 'mountain' else 'Darksteel Relic'
            self.add(nonmatching, 'nonmatching', owner, tapped=nonmatching == 'Forest')
        if mode == 'other_zones':
            for zone in ('hand', 'graveyard', 'exile', 'library'):
                self.add(own_counted[0], f'other_{zone}', owner, zone)
        if mode != 'no_legal_target':
            self.add('Elvish Mystic' if mode == 'exact_lethal' else 'Dazzling Ramparts', 'target',
                     owner if mode == 'own_target' else foe, tapped=mode == 'exact_lethal')
        if mode == 'own_target':
            self.add('Dazzling Ramparts', 'other_target', foe)
            if scope != 'creature':
                self.add(own_counted[0], 'own_0', owner, tapped=True)
        for who in (0, 1):
            for index, name in enumerate(('Island', 'Plains')):
                self.add(name, f'library_{who}_{index}', who, 'library')

        if mode in GAINS:
            actor = foe if mode == 'gain_opponent' else owner
            gain_source = 'Raise the Alarm' if scope == 'creature' else 'Growth Spiral'
            self.add(gain_source, 'support', actor, 'hand')
            if scope != 'creature':
                added = 'Mountain' if scope == 'mountain' and mode != 'gain_nonmountain' else 'Forest'
                self.add(added, 'new_land', actor, 'hand')
        elif mode in RESPONSES or mode == 'offturn_response':
            name = {'target_hexproof': "Ranger's Guile", 'countered': 'Counterspell', 'offturn_response': 'Think Twice'}.get(mode, 'Boomerang')
            actor = owner if mode == 'lose_own' else foe
            self.add(name, 'support', actor, 'hand')

    def fund(self, player, pool):
        self.pools[player] = list(pool)
        for color, amount in enumerate(pool):
            self.emit('set_mana', player=player, color=color, value=amount)
        self.emit('refresh')

    def observe_mana(self):
        for player, pool in enumerate(self.pools):
            for color, amount in enumerate(pool):
                self.emit('assert_mana', player=player, color=color, value=amount)

    def cast(self, ref, *, target=None, funding=None, allowed=True, response=False):
        from recipes import cost_from_oracle

        item = self.cards[ref]
        who = item['player']
        cost = cost_from_oracle(self.oracle[item['name']])
        self.fund(who, cost if funding is None else funding)
        if target:
            self.emit('choose_target', player=who, ref=target)
        self.emit('cast_response' if response else 'cast', ref=ref, value=int(allowed))
        self.emit('assert_choices_consumed')
        self.emit('assert_stack', value=(1+int(response)) if allowed else 0)
        if allowed:
            item['zone'] = 'stack'
            self.pools[who] = [have-due for have, due in zip(self.pools[who], cost)]
            if min(self.pools[who]) < 0:
                raise ValueError('unfunded valid spell')
        self.observe_mana()

    def count(self):
        return sum(item['player'] == self.owner and item['zone'] == 'battlefield'
                   and counted(self.oracle[item['name']], self.shape['scope']) for item in self.cards.values())

    def apply_damage(self):
        target = self.cards['target']
        target['damage'] = self.count()
        if target['damage'] >= int(self.oracle[target['name']]['toughness']) + target['boost']:
            target['zone'] = 'graveyard'

    def bind(self, ref, index=0):
        card = self.cards[ref]
        self.emit('bind_permanent_copy', ref=ref, player=card['player'], index=index, value=self.rows[card['name']]['csvid'])

    def draw(self, who):
        first = next(ref for ref, card in self.cards.items() if card['player'] == who and card['zone'] == 'library')
        self.cards[first]['zone'] = 'hand'

    def before_parent(self):
        mode = self.mode
        support = self.cards['support']
        who = support['player']
        target = {'lose_own': 'own_0', 'lose_opponent': 'enemy_0', 'target_bounced': 'target', 'target_hexproof': 'target'}.get(mode)
        self.emit('announce_spell', ref='source')
        self.cast('support', target=target, response=True)
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
            self.cards['target']['boost'] = 1
        self.emit('resolve_response')
        self.emit('refresh')
        support['zone'] = 'graveyard'
        if support['name'] == 'Growth Spiral':
            self.bind('new_land')
        if support['name'] == 'Raise the Alarm':
            for index in range(2):
                self.bind(f'soldier_{index}', index)

    def receipts(self):
        self.emit('assert_choices_consumed')
        self.emit('assert_stack', value=0)
        self.observe_mana()
        quantities = Counter((card['player'], card['zone'], self.rows[card['name']]['csvid']) for card in self.cards.values())
        for who in (0, 1):
            self.emit('assert_life', player=who, value=20)
            for zone in ('battlefield', 'hand', 'graveyard', 'exile', 'library'):
                self.emit('assert_zone', player=who, zone=zone, value=sum(count for (p, z, _), count in quantities.items() if p == who and z == zone))
            ordered = [card['name'] for card in self.cards.values() if card['player'] == who and card['zone'] == 'library']
            for index, name in enumerate(ordered):
                self.emit('assert_zone_card', player=who, zone='library', index=index, value=self.rows[name]['csvid'])
        for (who, zone, card), value in sorted(quantities.items()):
            self.emit('assert_zone_multiplicity', player=who, zone=zone, index=card, value=value)
        for ref, entry in self.cards.items():
            if entry['zone'] != 'battlefield':
                continue
            self.emit('assert_card', ref=ref, property='tapped', value=int(entry['tapped']))
            if 'Creature' in types(self.oracle[entry['name']]):
                record = self.oracle[entry['name']]
                for prop, value in [('power', int(record['power'])+entry['boost']), ('toughness', int(record['toughness'])+entry['boost']),
                                    ('damage', entry['damage'])]:
                    self.emit('assert_card', ref=ref, property=prop, value=value)
            if entry['name'] == 'Soldier':
                for prop, amount in [('token', 1), ('subtype_Soldier', 1), ('color', 32)]:
                    self.emit('assert_matching_permanents', player=entry['player'], index=self.rows['Soldier']['csvid'], property=prop, value=amount)


def _case(name, csvid, declaration, shape, variant, rows, oracle):
    from recipes import COLORS, step

    mode, suffix = variant.rsplit('_', 1)
    owner = int(suffix)
    game = Game(name, shape, mode, owner, rows, oracle)
    emit = game.emit
    emit('turn', player=owner, value=0)
    emit('reset_death_history')
    for who in (0, 1):
        game.fund(who, [0]*6)
    game.receipts()
    denied = mode in ('wrong_color', 'unfunded', 'no_legal_target')
    if mode == 'offturn_response':
        emit('advance_turn')
        game.cast('support')
        emit('announce_spell', ref='support')
    budget = list(shape['printed'])
    if mode == 'wrong_color':
        budget[COLORS['U']], budget[COLORS['R']] = budget[COLORS['R']], 0
    if mode == 'unfunded':
        due = next(color for color, value in enumerate(budget) if value)
        budget[due] -= 1
    if mode != 'offturn_response':
        emit('can_cast', ref='source', value=int(mode != 'no_legal_target'))
    game.cast('source', target=None if denied else 'target', allowed=not denied, funding=budget, response=mode == 'offturn_response')
    replay_from = None
    if not denied:
        if mode == 'replay_update':
            replay_from = len(game.steps)
        if mode in RESPONSES:
            game.before_parent()
        else:
            emit('resolve_response' if mode == 'offturn_response' else 'resolve')
            emit('refresh')
            if mode == 'offturn_response':
                game.draw(game.foe)
                game.cards['support']['zone'] = 'graveyard'
        if mode not in ('countered', 'target_bounced', 'target_hexproof'):
            game.apply_damage()
        game.cards['source']['zone'] = 'graveyard'
    game.receipts()
    if mode == 'cleanup_mark':
        emit('cleanup')
        game.cards['target']['damage'] = 0
        game.receipts()
    if replay_from is not None:
        suffix = game.steps[replay_from:]
        game.steps = (game.steps[:replay_from] + [step('capture')] + suffix
                       + [step('record_state'), step('restore'), step('ai_roundtrip')] + suffix + [step('assert_recorded_state')])
    metadata = dict(family=FAMILY, version=2, variant=variant, params=declaration['params'],
                    oracle_text=declaration['oracle_text'], covered_clauses=shape['clauses'], untested_components=[
                        'Sorcery timing rejection at the UI permission layer, where the raw cast driver does not enforce it.',
                        'Combined type-changing or change-of-control responses, damage prevention multipliers, and multiplayer.'])
    return dict(name=f'CountedBattlefieldDamageRecipe_{csvid}_{variant}', card=name, fixture='empty_main', seed=123,
                setup=game.setup, steps=game.steps, _recipe=metadata)


def expand(name, csvid, declaration, shape, selected, rows, oracle):
    if csvid != shape['csvid'] or not selected or len(set(selected)) != len(selected) or set(selected) - set(variants(shape)):
        raise ValueError('distinct reviewed count-damage variants required')
    validate_witnesses(oracle, rows)
    return [_case(name, csvid, declaration, shape, variant, rows, oracle) for variant in selected]
