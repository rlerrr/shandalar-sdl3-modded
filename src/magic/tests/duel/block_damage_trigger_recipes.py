"""Fixed blocking triggers composed with actual combat and Renown."""
import json
import re
from collections import Counter
from pathlib import Path

from counted_battlefield_damage_recipes import Game as PaymentBoard, types
from library_search_recipes import wave6_exact as exact_pin


FAMILY = 'block_damage_trigger'
DATA = Path(__file__).parent / 'data/block_damage_trigger_wave8_profiles.json'
RENOWN = "Renown 1 (When this creature deals combat damage to a player, if it isn't renowned, put a +1/+1 counter on it and it becomes renowned.)"
TRIGGER = re.compile(r"Whenever this creature (blocks or becomes blocked|becomes blocked) by a creature, (?:this creature|it) deals ([123]) damage to that creature(?: and ([123]) damage to that creature's controller)?\.")
WITNESSES = {'Island', 'Plains', 'Elvish Mystic', 'Dazzling Ramparts', 'Nessian Asp',
             'Goblin Raider', 'Thalia, Guardian of Thraben', 'Slippery Bogle', 'Think Twice'}
COMMON = ('cast', 'wrong_color', 'unfunded', 'fresh_attack', 'attack_lethal', 'defend_lethal', 'attack_marked',
          'defend_marked', 'attack_first_strike', 'attack_two', 'hexproof_blocker', 'unblocked', 'unrelated', 'replay')
CAST_MODES = {'cast', 'wrong_color', 'unfunded', 'fresh_attack'}
DEFENSE = {'defend_lethal', 'defend_marked'}
MARKED = {'attack_marked', 'defend_marked'}


def pins():
    return json.loads(DATA.read_text(encoding='utf8'))


def mechanism(record):
    from recipes import cost_from_oracle

    if (not record or record.get('layout') != 'normal' or record.get('selected_face') or record.get('card_faces')
            or not record.get('type_line', '').startswith('Creature — ')
            or any(not re.fullmatch(r'[0-9]+', record.get(stat, '')) for stat in ('power', 'toughness'))):
        raise ValueError('whole fixed-stat normal creature required')
    clauses = record['oracle_text'].splitlines()
    renown = clauses[:1] == [RENOWN]
    body = clauses[1:] if renown else clauses
    match = TRIGGER.fullmatch(body[0]) if len(body) == 1 else None
    if not match or record['keywords'] != (['Renown'] if renown else []):
        raise ValueError('complete printed blocking damage and any Renown clause required')
    mana = cost_from_oracle(record)
    if mana[4] < 1 or sum(mana[1:]) != mana[4]:
        raise ValueError('reviewed printed red creature payment required')
    return dict(defense=match[1].startswith('blocks'), damage=int(match[2]), controller_damage=int(match[3] or 0),
                renown=renown, printed=mana, power=int(record['power']), toughness=int(record['toughness']), clauses=clauses)


def variants(shape):
    modes = COMMON + (('renown_twice',) if shape['renown'] else ())
    return [f'{mode}_{player}' for mode in modes for player in (0, 1)]


def spec(declaration, record):
    shape = mechanism(record)
    source = pins()['sources'].get(record['name'])
    params = declaration.get('params')
    if (not source or not exact_pin(source['oracle'], record) or declaration.get('family') != FAMILY
            or type(declaration.get('version')) is not int or declaration['version'] != 2
            or declaration.get('oracle_text') != record['oracle_text'] or not isinstance(params, dict)
            or set(params) != {'schema', 'mechanism'} or type(params['schema']) is not int or params['schema'] != 1
            or not exact_pin(params['mechanism'], shape)):
        raise ValueError('whole pinned source with exact block peers and fees required')
    return dict(shape, csvid=source['csvid'], family=FAMILY)


def validate_witnesses(oracle, rows):
    data = pins()
    if set(data) != {'sources', 'witnesses'} or set(data['witnesses']) != WITNESSES:
        raise ValueError('complete independent timing and combatant witnesses required')
    for group in ('sources', 'witnesses'):
        for name, pin in data[group].items():
            row = rows.get(name, {})
            if (not exact_pin(pin['oracle'], oracle.get(name)) or row.get('csvid') != pin['csvid']
                    or row.get('handler') != pin['handler'] or row.get('kind') != 'card'
                    or row.get('handler') in (None, 'NULL', '0', 'card_dummy') or oracle[name].get('layout') != 'normal'):
                raise ValueError('implemented exact block-damage source or witness changed: ' + name)
            if group == 'sources':
                mechanism(oracle[name])


class Board(PaymentBoard):
    def __init__(self, name, shape, mode, owner, rows, oracle):
        self.shape, self.mode, self.owner, self.foe = shape, mode, owner, 1-owner
        self.rows, self.oracle = rows, oracle
        self.cards, self.setup, self.steps = {}, [], []
        self.pools = [[0]*6 for _ in (0, 1)]
        self.life = [20, 20]
        self.defending = mode in DEFENSE
        self.active = self.foe if self.defending else owner
        self.add(name, 'source', owner, 'hand' if mode in CAST_MODES else 'battlefield')
        for player in (0, 1):
            for i, label in enumerate(('Island', 'Plains')):
                self.add(label, f'library_{player}_{i}', player, 'library')
        opponent = {'attack_lethal': 'Elvish Mystic', 'defend_lethal': 'Elvish Mystic', 'defend_marked': 'Nessian Asp',
                    'attack_first_strike': 'Thalia, Guardian of Thraben', 'attack_two': 'Elvish Mystic',
                    'hexproof_blocker': 'Slippery Bogle', 'replay': 'Elvish Mystic'}.get(mode, 'Dazzling Ramparts')
        if mode not in ('cast', 'wrong_color', 'unfunded'):
            self.add(opponent, 'peer', self.foe)
        if mode == 'attack_two':
            self.add('Elvish Mystic', 'peer_2', self.foe)
        if mode == 'unrelated':
            self.add('Goblin Raider', 'ally', owner)
        if mode in MARKED:
            self.add('Think Twice', 'window_spell', self.foe, 'hand')

    def stats(self, ref):
        item = self.cards[ref]
        record = self.oracle[item['name']]
        return int(record['power']) + item['boost'], int(record['toughness']) + item['boost']

    def mark(self, ref, damage):
        entry = self.cards[ref]
        entry['damage'] += damage
        if entry['damage'] >= self.stats(ref)[1]:
            entry['zone'] = 'graveyard'

    def receipts(self):
        self.emit('assert_choices_consumed')
        self.emit('assert_stack', value=0)
        self.observe_mana()
        zones = Counter((item['player'], item['zone'], self.rows[item['name']]['csvid']) for item in self.cards.values())
        for player, life in enumerate(self.life):
            self.emit('assert_life', player=player, value=life)
            for zone in ('battlefield', 'hand', 'graveyard', 'library', 'exile'):
                self.emit('assert_zone', player=player, zone=zone,
                          value=sum(n for (p, z, _), n in zones.items() if player == p and zone == z))
            library = [item['name'] for item in self.cards.values() if item['player'] == player and item['zone'] == 'library']
            for index, name in enumerate(library):
                self.emit('assert_zone_card', player=player, zone='library', index=index, value=self.rows[name]['csvid'])
        for (player, zone, index), value in sorted(zones.items()):
            self.emit('assert_zone_multiplicity', player=player, zone=zone, index=index, value=value)
        for ref, item in self.cards.items():
            if item['zone'] != 'battlefield' or 'Creature' not in types(self.oracle[item['name']]):
                continue
            power, toughness = self.stats(ref)
            for prop, value in [('power', power), ('toughness', toughness), ('tapped', int(item['tapped'])), ('damage', item['damage']),
                                ('plus1_counters', item['boost'])]:
                self.emit('assert_card', ref=ref, property=prop, value=value)

    def resolve_window_spell(self):
        # The native spell-resolution path processes pending declared-blocker
        # damage. The harmless instant supplies a real post-block priority
        # action and draws a separately tracked card before combat damage.
        self.cast('window_spell')
        self.emit('resolve')
        self.emit('refresh')
        self.cards['window_spell']['zone'] = 'graveyard'
        self.draw(self.foe)
        self.receipts()

    def combat(self):
        mode = self.mode
        attacker = 'peer' if self.defending else 'ally' if mode == 'unrelated' else 'source'
        blockers = [] if mode in ('unblocked', 'renown_twice', 'fresh_attack') else (
            ['source'] if self.defending else ['peer', 'peer_2'] if mode == 'attack_two' else ['peer'])
        legal = mode != 'fresh_attack'
        self.emit('begin_attackers')
        self.emit('attack', ref=attacker, value=int(legal))
        if legal:
            self.cards[attacker]['tapped'] = True
        self.emit('finish_attackers')
        self.emit('begin_blockers')
        for blocker in blockers:
            self.emit('block', ref=blocker, other=attacker, value=1)
        self.emit('advance_combat')
        opponents = [attacker] if self.defending and self.shape['defense'] else blockers if attacker == 'source' else []
        for peer in opponents:
            self.mark(peer, self.shape['damage'])
            self.life[self.cards[peer]['player']] -= self.shape['controller_damage']
        if attacker == 'source' and legal:
            self.emit('assert_card', ref=attacker, property='blocked', value=int(bool(blockers)))
        if mode in MARKED:
            self.resolve_window_spell()
        self.emit('combat_damage')
        self.emit('resolve')
        self.emit('end_combat')
        if legal and self.cards[attacker]['zone'] == 'battlefield':
            power, _ = self.stats(attacker)
            alive = [ref for ref in blockers if self.cards[ref]['zone'] == 'battlefield']
            if len(alive) > 1:
                raise ValueError('a second surviving blocker would need an assigned damage vector')
            if alive:
                other = alive[0]
                peer_power, _ = self.stats(other)
                self.mark(other, power)
                self.mark(attacker, peer_power)
            elif not blockers:
                self.life[1-self.active] -= power
                if attacker == 'source' and self.shape['renown'] and not self.cards['source']['boost']:
                    self.cards['source']['boost'] = 1
        self.receipts()


def _case(name, csvid, declaration, shape, variant, rows, oracle):
    from recipes import COLORS, step

    mode, actor = variant.rsplit('_', 1)
    owner = int(actor)
    board = Board(name, shape, mode, owner, rows, oracle)
    emit = board.emit
    emit('turn', player=board.active, value=0)
    emit('reset_death_history')
    for player in (0, 1):
        board.fund(player, [0]*6)
    board.receipts()
    if mode in CAST_MODES:
        price = list(shape['printed'])
        denied = mode in ('wrong_color', 'unfunded')
        if mode == 'wrong_color':
            price[COLORS['U']], price[COLORS['R']] = price[COLORS['R']], 0
        if mode == 'unfunded':
            price[next(i for i, value in enumerate(price) if value)] -= 1
        board.cast('source', funding=price, allowed=not denied)
        if not denied:
            emit('resolve')
            emit('refresh')
            board.cards['source']['zone'] = 'battlefield'
            emit('assert_card', ref='source', property='sick', value=1)
        board.receipts()
    replay = len(board.steps) if mode == 'replay' else None
    if mode not in ('cast', 'wrong_color', 'unfunded'):
        board.combat()
    if mode == 'renown_twice':
        emit('advance_turn')
        emit('advance_turn')
        emit('untap_step', player=owner)
        emit('turn', player=owner, value=0)
        board.cards['source']['tapped'] = False
        board.combat()
    if replay is not None:
        suffix = board.steps[replay:]
        board.steps = (board.steps[:replay] + [step('capture')] + suffix
                       + [step('record_state'), step('restore'), step('ai_roundtrip')] + suffix + [step('assert_recorded_state')])
    metadata = dict(family=FAMILY, version=2, variant=variant, params=declaration['params'],
                    oracle_text=declaration['oracle_text'], covered_clauses=shape['clauses'], untested_components=[
                        'Independent raw damage observation during the declaration callback before pending damage processing.',
                        'Response ordering between multiple simultaneous triggers, prevention/redirection and multiplayer.'])
    return dict(name=f'BlockDamageTriggerRecipe_{csvid}_{variant}', card=name, fixture='empty_main', seed=123,
                setup=board.setup, steps=board.steps, _recipe=metadata)


def expand(name, csvid, declaration, shape, selected, rows, oracle):
    if csvid != shape['csvid'] or not selected or len(set(selected)) != len(selected) or set(selected) - set(variants(shape)):
        raise ValueError('distinct implemented block damage alternatives required')
    validate_witnesses(oracle, rows)
    return [_case(name, csvid, declaration, shape, variant, rows, oracle) for variant in selected]
