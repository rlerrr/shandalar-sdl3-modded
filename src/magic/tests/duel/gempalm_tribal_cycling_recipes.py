"""Whole-Oracle Gempalm cycling, tribal effects, and effect-before-draw receipts."""

import json
import re
from collections import Counter
from pathlib import Path

from cast_untap_trigger_recipes import exact, pin

FAMILY = 'gempalm_tribal_cycling'
DATA = Path(__file__).parent / 'data/gempalm_tribal_cycling_wave8_profiles.json'
CYCLE = re.compile(r'Cycling ((?:\{[1-9BUGRW]\})+) \(\1, Discard this card: Draw a card\.\)')
EFFECT = re.compile(r'When you cycle this card, (.+)\.')
TRIBES = dict(Soldier='Elite Vanguard', Elf='Elvish Warrior', Wizard='Crafty Pathmage',
              Goblin='Goblin Piker', Zombie='Scathe Zombies')
LIBRARY = ('Grizzly Bears', 'Mountain', 'Island', 'Forest')
WITNESSES = set(TRIBES.values()) | set(LIBRARY) | {
    'Plains', 'Expunge', 'Tireless Tribe', 'Mothdust Changeling', 'Boggart Shenanigans',
    'Hill Giant', 'Giant Spider', 'Horizon Chimera', 'Ornithopter',
}


def pins():
    return json.loads(DATA.read_text(encoding='utf8'))


def classify(record):
    from recipes import cost_from_oracle

    types = record.get('type_line', '').partition(' — ')[0].split()
    if (record.get('layout') != 'normal' or record.get('card_faces') or record.get('selected_face')
            or 'Creature' not in types or 'Token' in types or record.get('keywords') != ['Cycling']):
        raise ValueError('whole normal creature with ordinary cycling required')
    lines = record.get('oracle_text', '').splitlines()
    if len(lines) != 2 or not (cycling := CYCLE.fullmatch(lines[0])) or not (trigger := EFFECT.fullmatch(lines[1])):
        raise ValueError('whole printed cycling and reward required')
    reward = trigger[1]
    PUMP = r'(Soldier|Elf) creatures get \+(\d+)/\+(\d+)( and gain first strike)? until end of turn'
    if match := re.fullmatch(PUMP, reward):
        effect, tribe, delta = 'mass', match[1], [int(match[2]), int(match[3])]
        keyword = 'first strike' if match[4] else None
    elif reward == 'Wizard creatures gain flying until end of turn':
        effect, tribe, delta, keyword = 'mass', 'Wizard', [0, 0], 'flying'
    elif reward == 'you may have it deal X damage to target creature, where X is the number of Goblins on the battlefield':
        effect, tribe, delta, keyword = 'damage', 'Goblin', [0, 0], None
    elif reward == 'you may have target player lose life equal to the number of Zombies on the battlefield':
        effect, tribe, delta, keyword = 'life_loss', 'Zombie', [0, 0], None
    else:
        raise ValueError('unmodeled complete tribal cycling clause')
    if any(not re.fullmatch(r'\d+', record.get(key, '')) for key in ('power', 'toughness')) or int(record['toughness']) <= 0:
        raise ValueError('positive fixed toughness required')
    return dict(mana=cost_from_oracle(record), cycling_mana=cost_from_oracle({'mana_cost': cycling[1]}),
                power=int(record['power']), toughness=int(record['toughness']), effect=effect, tribe=tribe,
                delta=delta, keyword=keyword)


def variants(params):
    modes = ['effect', 'off_turn', 'repeated', 'pending_replay', 'cancel_cycle', 'insufficient_mana',
             'wrong_color', 'source_cast', 'source_cast_denied', 'plain_discard', 'unrelated_cycle',
             'zero_tribe', 'changeling', 'opponent_only', 'own_only']
    if params['effect'] == 'mass':
        modes += ['cleanup']
    else:
        modes += ['decline_effect', 'own_target', 'count_changes_pending']
    if params['effect'] == 'damage':
        modes += ['kindred_goblin', 'effect_before_draw', 'draw_watcher_survives']
    if params['keyword'] == 'first strike':
        modes += ['first_strike', 'enemy_first_strike', 'base_combat', 'expired_first_strike']
    if params['keyword'] == 'flying':
        modes += ['flying', 'reach_blocks', 'expired_flying', 'enemy_flying_blocks', 'expired_enemy_flying']
    return [f'{mode}_{who}' for who in (0, 1) for mode in modes]


def spec(declaration, record):
    if (declaration.get('family') != FAMILY or type(declaration.get('version')) is not int
            or declaration['version'] != 2 or declaration.get('oracle_text') != record.get('oracle_text')
            or not exact(pins()['sources'].get(record['name']), pin(record))):
        raise ValueError('whole pinned source and exact recipe version required')
    params = classify(record)
    if not exact(declaration.get('params'), params):
        raise ValueError('parameters must derive from complete Oracle clauses')
    listed = declaration.get('variants')
    if (not isinstance(listed, list) or not listed or any(not isinstance(v, str) for v in listed)
            or len(set(listed)) != len(listed) or set(listed) - set(variants(params))):
        raise ValueError('distinct tribal cycling variants required')
    return dict(params, family=FAMILY)


def validate_witnesses(oracle, rows):
    data = pins()
    if set(data['witnesses']) != WITNESSES:
        raise ValueError('exact fixture witness set required')
    records = dict(data['witnesses'], **data['sources'])
    if set(records) != set(data['handlers']):
        raise ValueError('exact fixture handler set required')
    for name, expected in records.items():
        row = rows[name]
        if (not exact(pin(oracle[name]), expected) or row['handler'] != data['handlers'][name]
                or row['handler'] in ('0', 'NULL')):
            raise ValueError(f'Oracle or handler changed for {name}')
        if name in data['sources'] and (row['kind'] != 'card' or row['handler'] == 'card_dummy'):
            raise ValueError(f'implemented primary required for {name}')


class GempalmCase:
    def __init__(self, name, csvid, declaration, params, variant, rows, oracle):
        from recipes import step

        self.name, self.csvid, self.declaration, self.params = name, csvid, declaration, params
        self.variant, self.rows, self.oracle = variant, rows, oracle
        self.mode, who = variant.rsplit('_', 1)
        self.owner = int(who)
        self.other = 1 - self.owner
        self.setup, self.steps, self.permanents = [], [], {}
        self.zones = {zone: [[], []] for zone in ('hand', 'battlefield', 'graveyard', 'library', 'exile')}
        self.mana = [[0] * 6, [0] * 6]
        self.life = [20, 20]
        self.replay_cut = 0
        for player in (0, 1):
            for index, card in enumerate(LIBRARY):
                self.seed(card, f'lib_{player}_{index}', player, 'library')
            self.seed('Plains', f'grave_{player}', player, 'graveyard')
            self.seed('Forest', f'exile_{player}', player, 'exile')
        self.seed('Grizzly Bears', 'neutral', self.owner)
        member = TRIBES[params['tribe']]
        if self.mode not in ('zero_tribe', 'opponent_only'):
            self.seed(member, 'member', self.owner)
        if self.mode not in ('zero_tribe', 'own_only'):
            self.seed(member, 'enemy_member', self.other)
        if self.mode == 'changeling':
            self.seed('Mothdust Changeling', 'changeling', self.other)
        if self.mode == 'kindred_goblin':
            self.seed('Boggart Shenanigans', 'kindred', self.other)
        self.seed(name, 'source', self.owner, 'graveyard' if self.mode in ('unrelated_cycle', 'base_combat') else 'hand')
        if self.mode == 'unrelated_cycle':
            self.seed('Expunge', 'unrelated', self.owner, 'hand')
        if self.mode == 'plain_discard':
            self.seed('Tireless Tribe', 'outlet', self.owner)
        if self.mode == 'repeated':
            self.seed(name, 'second', self.owner, 'hand')
        if params['effect'] == 'damage':
            target_player = self.owner if self.mode == 'own_target' else self.other
            self.seed('Hill Giant', 'target', target_player)
            if self.mode in ('effect_before_draw', 'draw_watcher_survives'):
                self.seed('Horizon Chimera', 'draw_watcher', self.owner)
        elif params['keyword'] and self.mode in ('first_strike', 'base_combat', 'expired_first_strike', 'flying', 'reach_blocks', 'expired_flying'):
            self.seed('Giant Spider' if self.mode == 'reach_blocks' else 'Grizzly Bears', 'blocker', self.other)
        if self.mode in ('enemy_flying_blocks', 'expired_enemy_flying'):
            self.seed('Ornithopter', 'flyer', self.owner)
        self.mana[self.owner] = self.cost_to_seed()
        self.names = sorted({entry['name'] for entry in self.setup})
        self.before = [step('turn', player=self.other if self.mode == 'off_turn' else self.owner, value=0)]
        self.before += [step('set_mana', player=player, color=color, value=amount)
                        for player in (0, 1) for color, amount in enumerate(self.mana[player])]
        self.before.append(step('refresh'))

    def cost_to_seed(self):
        if self.mode in ('plain_discard', 'base_combat'):
            return [0] * 6
        if self.mode == 'unrelated_cycle':
            return [2, 0, 0, 0, 0, 0]
        mana = list(self.params['mana' if self.mode.startswith('source_cast') else 'cycling_mana'])
        if self.mode in ('insufficient_mana', 'source_cast_denied'):
            mana[next(color for color, amount in enumerate(mana) if amount)] -= 1
        elif self.mode == 'wrong_color':
            mana = [sum(mana), 0, 0, 0, 0, 0]
        elif self.mode == 'repeated':
            mana = [2 * amount for amount in mana]
        return mana

    def seed(self, name, ref, player, zone='battlefield'):
        from recipes import card

        entry = card(name, ref, player, zone)
        if zone == 'battlefield':
            entry['state'] = ['untapped']
            self.track(name, ref, player)
        self.setup.append(entry)
        self.zones[zone][player].append(name)

    def track(self, name, ref, player):
        record = self.oracle[name]
        creature = 'Creature' in record['type_line'].partition(' — ')[0].split()
        self.permanents[ref] = dict(name=name, player=player, creature=creature, alive=True, damage=0,
                                    power=int(record['power']) if creature else None,
                                    toughness=int(record['toughness']) if creature else None,
                                    delta_power=0, delta_toughness=0)

    def emit(self, op, **kwargs):
        from recipes import step

        self.steps.append(step(op, **kwargs))

    def property(self, ref, key, value):
        self.emit('assert_card', ref=ref, property=key, value=value)

    def move(self, name, player, start, end):
        self.zones[start][player].remove(name)
        self.zones[end][player].append(name)

    def kill(self, ref, destroy=False):
        if destroy:
            self.emit('destroy', ref=ref)
            self.emit('refresh')
        permanent = self.permanents[ref]
        self.move(permanent['name'], permanent['player'], 'battlefield', 'graveyard')
        permanent['alive'] = False

    def spend(self, cost):
        self.mana[self.owner] = [amount - spent for amount, spent in zip(self.mana[self.owner], cost)]

    def member(self, permanent):
        oracle = self.oracle[permanent['name']]
        return permanent['alive'] and (self.params['tribe'] in oracle['type_line'].partition(' — ')[2].split()
                                       or 'Changeling' in oracle.get('keywords', []))

    def receipt(self, completed=True):
        if completed:
            self.emit('assert_stack', value=0)
        self.emit('assert_choices_consumed')
        for player in (0, 1):
            self.emit('assert_life', player=player, value=self.life[player])
            for color, amount in enumerate(self.mana[player]):
                self.emit('assert_mana', player=player, color=color, value=amount)
            for zone, groups in self.zones.items():
                contents = groups[player]
                self.emit('assert_zone', player=player, zone=zone, value=len(contents))
                counts = Counter(contents)
                for name in self.names:
                    self.emit('assert_zone_multiplicity', player=player, zone=zone,
                              index=self.rows[name]['csvid'], value=counts[name])
                if zone == 'library':
                    for index, name in enumerate(contents):
                        self.emit('assert_zone_card', player=player, zone=zone, index=index, value=self.rows[name]['csvid'])
        for ref, p in self.permanents.items():
            if p['alive'] and p['creature']:
                self.property(ref, 'power', p['power'] + p['delta_power'])
                self.property(ref, 'toughness', p['toughness'] + p['delta_toughness'])
                self.property(ref, 'damage', p['damage'])

    def effect(self):
        members = [p for p in self.permanents.values() if self.member(p)]
        if self.params['effect'] == 'mass':
            for p in members:
                if p['creature']:
                    p['delta_power'] += self.params['delta'][0]
                    p['delta_toughness'] += self.params['delta'][1]
        elif members:
            if self.mode == 'decline_effect':
                self.emit('choose_cancel_target', player=self.owner)
            elif self.params['effect'] == 'damage':
                target = 'draw_watcher' if self.mode == 'effect_before_draw' else 'target'
                self.emit('choose_target', player=self.owner, ref=target)
                p = self.permanents[target]
                p['damage'] += len(members)
                if p['damage'] >= p['toughness']:
                    self.kill(target)
            else:
                target = self.owner if self.mode == 'own_target' else self.other
                self.emit('choose_player', player=self.owner, value=target)
                self.life[target] -= len(members)

    def cycle(self, ref, name, cost, use_effect=True):
        self.emit('require_can_activate_hand', ref=ref, value=1)
        self.emit('choose_dialog', player=self.owner, value=int(self.mode != 'cancel_cycle'))
        self.emit('activate_hand', ref=ref, value=int(self.mode != 'cancel_cycle'))
        self.emit('assert_unused_choices', value=0)
        if self.mode == 'cancel_cycle':
            return
        self.spend(cost)
        self.move(name, self.owner, 'hand', 'graveyard')
        self.receipt(completed=False)
        if self.mode == 'pending_replay':
            self.replay_cut = len(self.steps)
        if self.mode == 'count_changes_pending':
            self.kill('enemy_member', destroy=True)
        if use_effect:
            self.effect()
        self.emit('resolve')
        self.emit('assert_unused_choices', value=0)
        self.emit('refresh')
        self.move(self.zones['library'][self.owner][0], self.owner, 'library', 'hand')
        if self.mode == 'draw_watcher_survives':
            self.life[self.owner] += 1
        self.receipt()

    def plain_discard(self):
        self.emit('require_can_activate', ref='outlet', value=1)
        if self.owner == 0:
            self.emit('choose_zone_card', player=self.owner, value=self.csvid)
        self.emit('activate', ref='outlet', value=1)
        self.emit('assert_unused_choices', value=0)
        self.move(self.name, self.owner, 'hand', 'graveyard')
        self.receipt(completed=False)
        self.emit('resolve')
        self.emit('refresh')
        self.permanents['outlet']['delta_toughness'] = 4

    def cast_source(self):
        valid = self.mode == 'source_cast'
        self.emit('cast', ref='source', value=int(valid))
        if valid:
            self.emit('require_card', ref='source', property='in_hand', value=0)
            self.emit('assert_stack', value=1)
            self.emit('resolve')
            self.emit('refresh')
            self.spend(self.params['mana'])
            self.move(self.name, self.owner, 'hand', 'battlefield')
            self.track(self.name, 'source', self.owner)

    def expire(self):
        if self.mode.startswith('expired_'):
            for player in (self.other, self.owner):
                self.emit('advance_turn')
                self.emit('untap_step', player=player)
            self.emit('turn', player=self.owner, value=0)
        else:
            self.emit('cleanup')
        self.emit('refresh')
        for permanent in self.permanents.values():
            permanent.update(delta_power=0, delta_toughness=0, damage=0)

    def combat(self):
        enemy_reward = self.mode in ('enemy_first_strike', 'enemy_flying_blocks', 'expired_enemy_flying')
        attacker = ('neutral' if self.mode == 'enemy_first_strike' else 'flyer') if enemy_reward else 'member'
        blocker = 'enemy_member' if enemy_reward else 'blocker'
        self.emit('begin_attackers')
        self.emit('attack', ref=attacker, value=1)
        self.emit('finish_attackers')
        self.emit('begin_blockers')
        blocked = self.mode not in ('flying', 'expired_enemy_flying')
        self.emit('block', ref=blocker, other=attacker, value=int(blocked))
        self.emit('advance_combat')
        self.property(attacker, 'blocked', int(blocked))
        self.emit('combat_damage')
        self.emit('end_combat')
        if self.mode == 'flying':
            self.life[self.other] -= 1
        elif self.mode == 'first_strike':
            self.kill('blocker')
        elif self.mode == 'enemy_first_strike':
            self.kill('neutral')
        elif self.mode == 'enemy_flying_blocks':
            self.permanents['flyer']['damage'] = 1
        elif self.mode == 'expired_enemy_flying':
            return
        elif self.mode in ('reach_blocks', 'expired_flying'):
            self.kill('member')
            self.permanents['blocker']['damage'] = 1
        else:
            self.kill('blocker')
            self.kill('member')

    def case(self):
        from recipes import step

        if self.mode in ('wrong_color', 'insufficient_mana'):
            self.emit('can_activate_hand', ref='source', value=0)
            self.emit('activate_hand', ref='source', value=0)
        elif self.mode.startswith('source_cast'):
            self.cast_source()
        elif self.mode == 'plain_discard':
            self.plain_discard()
        elif self.mode == 'unrelated_cycle':
            self.cycle('unrelated', 'Expunge', [2, 0, 0, 0, 0, 0], use_effect=False)
        elif self.mode != 'base_combat':
            self.cycle('source', self.name, self.params['cycling_mana'])
            if self.mode == 'repeated':
                self.cycle('second', self.name, self.params['cycling_mana'])
        if self.mode == 'cleanup' or self.mode.startswith('expired_'):
            self.expire()
        if 'blocker' in self.permanents or self.mode in ('enemy_first_strike', 'enemy_flying_blocks', 'expired_enemy_flying'):
            self.combat()
        self.receipt()
        replay = self.steps[self.replay_cut:]
        sequence = self.before + self.steps[:self.replay_cut] + [step('capture')] + replay
        sequence += [step('record_state'), step('restore')] + replay + [step('assert_recorded_state')]
        return dict(name=f'GempalmTribalCyclingRecipe_{self.csvid}_{self.variant}', card=self.name, fixture='empty_main',
                    seed=123, setup=self.setup, steps=sequence,
                    _recipe=dict(family=FAMILY, version=2, variant=self.variant, params=self.declaration['params'],
                                 oracle_text=self.declaration['oracle_text'], untested_components=[
                                     'Independent priority and target locking between the printed trigger and cycling draw are not exposed by the combined hand callback.',
                                     'Zero-count cases check the unchanged outcome and draw without asserting offer availability.',
                                 ]))


def expand(name, csvid, declaration, params, selected, rows, oracle):
    validate_witnesses(oracle, rows)
    if not selected or len(set(selected)) != len(selected) or set(selected) - set(variants(params)):
        raise ValueError('unsupported tribal cycling variants')
    return [GempalmCase(name, csvid, declaration, params, variant, rows, oracle).case() for variant in selected]
