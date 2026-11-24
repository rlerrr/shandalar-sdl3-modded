"""Test the actual combat rules printed on both faces of eight Werewolves."""
import json
from pathlib import Path

import werewolf_spell_history_recipes as history
from library_search_recipes import wave6_exact


FAMILY = 'werewolf_combat_riders'
PROFILE = Path(__file__).parent / 'data/werewolf_combat_riders_wave8_profiles.json'
MENACE = "Menace (This creature can't be blocked except by two or more creatures.)"
INTIMIDATE = "Intimidate (This creature can't be blocked except by artifact creatures and/or creatures that share a color with it.)"
TRIBAL = "Werewolves you control have menace. (A creature with menace can't be blocked except by two or more creatures.)"
GATE = "This creature can't attack unless you control a creature with power 4 or greater."
TEAM = 'Attacking creatures you control get +1/+0 and have trample.'
MUST_ATTACK = 'This creature attacks each combat if able.'
MUST_BLOCK = 'This creature must be blocked if able.'
GRAMMAR = {
    ((), (INTIMIDATE,)): ('intimidate', ('front_block', 'other_color', 'same_color', 'artifact')),
    (('Defender',), (MUST_ATTACK,)): ('required_attack', ('defender', 'required', 'tapped_required')),
    ((), (MUST_BLOCK,)): ('required_block', ('front_omission', 'auto_block', 'tapped_blocker', 'no_blocker')),
    (('First strike',), ('Double strike', TRIBAL)): (
        'tribal', ('front_first', 'back_double', 'tribal_single', 'tribal_pair', 'other_creature', 'opponent_wolf')),
    ((), (TEAM,)): ('team', ('front_team', 'back_team', 'enemy_team')),
    ((), (MENACE,)): ('menace', ('front_single', 'back_single', 'back_pair')),
    ((GATE,), ()): ('gate', ('no_ally', 'small_ally', 'enemy_four', 'own_four', 'back_ready')),
    (('Haste',), (MENACE,)): ('haste_menace', ('fresh_attack', 'front_single', 'back_single', 'back_pair')),
}
KEYWORDS = {INTIMIDATE: 'Intimidate', MENACE: 'Menace', 'Defender': 'Defender',
            'Haste': 'Haste', 'First strike': 'First strike', 'Double strike': 'Double strike'}
HANDLERS = {
    'intimidate': (history.HUMAN, 'manalinkeh_card_gastaf_howler'),
    'required_attack': ('manalinkeh_card_hanweir_watchkeep', 'manalinkeh_card_bane_of_hanweir'),
    'required_block': (history.HUMAN, 'manalinkeh_card_hinterland_scourge'),
    'tribal': (history.HUMAN, 'manalinkeh_card_terror_of_kruin_pass'),
    'team': (history.HUMAN, 'manalinkeh_card_neck_breaker'),
    'menace': (history.HUMAN, 'manalinkeh_card_gatstaf_ravagers'),
    'gate': ('manalinkeh_card_lambholt_pacifist', history.WOLF),
    'haste_menace': (history.HUMAN, 'manalinkeh_card_gatstaf_ravagers'),
}
COMMON = ('front_split', 'back_split', 'back_two')
FRONT_COMBAT = {'front_block', 'defender', 'front_omission', 'front_first', 'front_team',
                'front_single', 'no_ally', 'small_ally', 'enemy_four', 'own_four'}
TWO_BLOCKERS = {'back_pair', 'tribal_pair'}
SMALL_BLOCK = {
    'front_omission', 'auto_block', 'tapped_blocker', 'front_first', 'front_single', 'back_single',
    'back_pair', 'tribal_single', 'tribal_pair', 'other_creature', 'opponent_wolf',
}
DUMMIES = {history.SMALL, 'Grizzly Bears', 'Hill Giant', 'Durkwood Boars'}
WITNESSES = {*history.DECK, *DUMMIES, 'Fog', 'Scathe Zombies', 'Wall of Tanglecord', 'Lesser Werewolf', 'Twiddle'}


def profiles():
    return json.loads(PROFILE.read_bytes())


def classify(front, back=None):
    from recipes import COLORS, cost_from_oracle

    if not front or front.get('layout') != 'transform' or len(front.get('card_faces', ())) != 2:
        raise ValueError('expected a physical two-face transform record')
    f, b = front['card_faces']
    if back is None:
        back = profiles()['sources'][f['name']]['back_oracle']
    if (front['engine_face'] != f['name'] or back['engine_face'] != b['name']
            or front['name'] != f['name'] + ' // ' + b['name'] or back['name'] != front['name']
            or front['oracle_id'] != back['oracle_id'] or back['layout'] != 'transform'
            or not wave6_exact(front['card_faces'], back['card_faces'])
            or not f['mana_cost'] or b['mana_cost'] or b.get('color_indicator') != b['colors']):
        raise ValueError('expected the matching front and back records')
    fields = {'name', 'oracle_text', 'mana_cost', 'type_line', 'power', 'toughness', 'colors'}
    faces = []
    for raw, record, trigger in zip((f, b), (front, back), history.TRIGGERS):
        clauses = raw['oracle_text'].splitlines()
        selected = record['selected_face']
        if (set(selected) != fields or not wave6_exact(selected, {key: raw.get(key) for key in fields})
                or 'Werewolf' not in raw['type_line'].split()
                or not raw['type_line'].startswith('Creature — ')
                or clauses[-1:] != [trigger]
                or any(not raw[key].isascii() or not raw[key].isdecimal() for key in ('power', 'toughness'))):
            raise ValueError('wrong selected face, creature characteristics, or upkeep trigger')
        faces.append(dict(name=raw['name'], power=int(raw['power']), toughness=int(raw['toughness']),
                          color=sum(1 << COLORS[color] for color in raw['colors']), clauses=clauses))
    signatures = tuple(tuple(face['clauses'][:-1]) for face in faces)
    if signatures not in GRAMMAR:
        raise ValueError('the full paired Oracle clauses do not match a supported combat rider')
    rider, _ = GRAMMAR[signatures]
    keywords = {'Transform', *(KEYWORDS[line] for clause in signatures for line in clause if line in KEYWORDS)}
    if set(front['keywords']) != keywords or set(back['keywords']) != keywords:
        raise ValueError('paired keyword index changed')
    return dict(price=cost_from_oracle(f), front=faces[0], back=faces[1], rider=rider)


def variants(shape):
    _, combat = next(pair for pair in GRAMMAR.values() if pair[0] == shape['rider'])
    return [f'{mode}_{seat}' for mode in COMMON + combat for seat in (0, 1)]


def spec(declaration, record):
    data = profiles()['sources'].get(record.get('engine_face'))
    if not data or not wave6_exact(record, data['oracle']):
        raise ValueError('exact physical front Oracle record required')
    shape = classify(record, data['back_oracle'])
    selected = declaration.get('variants')
    if (declaration.get('family') != FAMILY or type(declaration.get('version')) is not int
            or declaration['version'] != 2 or declaration.get('oracle_text') != record['selected_face']['oracle_text']
            or not wave6_exact(declaration.get('params'), shape) or not isinstance(selected, list)
            or not selected or len(set(selected)) != len(selected) or set(selected) - set(variants(shape))):
        raise ValueError('expected the exact front declaration and unique reviewed variants')
    return dict(shape, family=FAMILY)


def validate_witnesses(oracle, rows):
    data = profiles()
    if set(data) != {'sources', 'witnesses'} or set(data['witnesses']) != WITNESSES:
        raise ValueError('missing paired sources or independent combat witnesses')
    for name, pin in data['witnesses'].items():
        row = rows[name]
        if (pin['registry'] != row or not wave6_exact(pin['oracle'], oracle[name])
                or oracle[name]['layout'] != 'normal' or row['kind'] != 'card'
                or row['handler'] in (None, 'NULL', '0')
                or row['handler'] == 'card_dummy' and name not in DUMMIES):
            raise ValueError('unreviewed combat witness ' + name)
    for name, pair in data['sources'].items():
        shape = classify(pair['oracle'], pair['back_oracle'])
        back = shape['back']['name']
        front_row, back_row = rows[name], rows[back]
        if (name != shape['front']['name'] or not wave6_exact(pair['oracle'], oracle[name])
                or not wave6_exact(pair['back_oracle'], oracle[back])
                or pair['registry'] != front_row or pair['back_registry'] != back_row
                or front_row['kind'] != 'card' or back_row['kind'] != 'card'
                or front_row['csvid'] + 1 != back_row['csvid']
                or (front_row['handler'], back_row['handler']) != HANDLERS[shape['rider']]):
            raise ValueError('physical pair or native combat handler changed ' + name)


class Board(history.Board):
    def __init__(self, name, shape, mode, owner, rows, oracle):
        self.attacking = set()
        super().__init__(name, shape, mode, owner, rows, oracle)
        blocker_side = owner if mode == 'opponent_wolf' else self.foe
        if mode in SMALL_BLOCK:
            self.add(history.SMALL, 'blocker', blocker_side)
        elif mode in ('front_block', 'other_color', 'same_color', 'artifact'):
            blocker = dict(front_block='Scathe Zombies', other_color='Scathe Zombies',
                           same_color='Grizzly Bears', artifact='Wall of Tanglecord')[mode]
            self.add(blocker, 'blocker', self.foe)
        if mode in TWO_BLOCKERS:
            self.add(history.SMALL, 'second', self.foe)
        if mode in ('tribal_single', 'tribal_pair', 'opponent_wolf', 'other_creature'):
            self.add('Grizzly Bears' if mode == 'other_creature' else 'Lesser Werewolf', 'ally',
                     self.foe if mode == 'opponent_wolf' else owner)
        if mode in ('front_team', 'back_team', 'enemy_team'):
            self.add('Grizzly Bears', 'ally', self.foe if mode == 'enemy_team' else owner)
            self.add('Grizzly Bears', 'idle', owner)
            if mode != 'enemy_team':
                for ref in ('blocker', 'second'):
                    self.add(history.SMALL, ref, self.foe)
        if mode in ('small_ally', 'enemy_four', 'own_four'):
            self.add('Hill Giant' if mode == 'small_ally' else 'Durkwood Boars', 'ally',
                     self.foe if mode == 'enemy_four' else owner)
        if mode in ('tapped_required', 'tapped_blocker'):
            self.add('Twiddle', 'tap', self.foe if mode == 'tapped_required' else owner, 'hand')

    @property
    def sick(self):
        # The typed observer exposes STATE_SUMMONSICK_BOTH after the engine
        # clears it for Haste. The inherited turn ledger still tracks age.
        return self._sick and (self.is_back or self.shape['rider'] != 'haste_menace')

    @sick.setter
    def sick(self, value):
        self._sick = value

    def fog_stock(self):
        rosters = {
            'front_split': (self.owner, self.foe),
            'back_split': (self.owner, self.foe),
            'back_two': (self.foe, self.foe),
        }
        casts = rosters.get(self.mode, (self.foe,) if self.mode in FRONT_COMBAT else ())
        return [casts.count(who) for who in (0, 1)]

    def buffed(self, ref):
        return (self.shape['rider'] == 'team' and self.is_back and ref in self.attacking
                and self.cards[ref]['player'] == self.owner and self.cards['source']['zone'] == 'battlefield')

    def stats(self, ref):
        power, toughness = super().stats(ref)
        return power + self.buffed(ref), toughness

    def tap_for_combat(self, ref):
        self.spell('tap', target=ref)
        self.emit('choose_dialog', player=self.cards['tap']['player'], value=0)
        self.emit('resolve')
        self.cards['tap']['zone'] = 'graveyard'
        self.cards[ref]['tapped'] = True
        self.receipts()

    def declare(self, attacking=(), rejected=(), *, required=False, ready_requirement=False):
        self.emit('begin_required_attackers' if required else 'begin_attackers')
        if required:
            self.emit('can_finish_attackers', value=int(not ready_requirement))
        for ref in rejected:
            self.emit('attack', ref=ref, value=0)
        for ref in attacking:
            self.emit('attack', ref=ref, value=1)
            self.cards[ref]['tapped'] = True
        self.attacking.update(attacking)
        if required:
            self.emit('can_finish_attackers', value=1)
        self.emit('finish_attackers')
        for ref in (*attacking, *rejected):
            self.emit('assert_card', ref=ref, property='attacking', value=int(ref in self.attacking))

    def observe_blocks(self, blocks, *, automatic=False):
        if automatic:
            [(blocker, _, allowed)] = blocks
            assert allowed
            self.emit('choose_target', ref=blocker, player=1-self.current)
        self.emit('begin_blockers')
        if not automatic:
            for blocker, attacker, allowed in blocks:
                self.emit('block', ref=blocker, other=attacker, value=int(allowed))
        actual = {blocker: attacker for blocker, attacker, allowed in blocks if allowed}
        self.emit('advance_combat')
        for blocker, _, allowed in blocks:
            self.emit('assert_card', ref=blocker, property='blocking_count', value=int(allowed))
        for attacker in sorted(self.attacking):
            self.emit('assert_card', ref=attacker, property='blocked', value=int(attacker in actual.values()))
        self.receipts()
        return actual

    def combat(self, blocks=(), *, automatic=False, first=False, double=False, declaration_only=False):
        actual = self.observe_blocks(blocks, automatic=automatic)
        if declaration_only:
            return
        self.emit('combat_damage')
        assert len(set(actual.values())) == len(actual)
        blocked = {attacker: blocker for blocker, attacker in actual.items()}
        packets = []
        spill = 0
        for attacker in sorted(self.attacking):
            power, _ = self.stats(attacker)
            if attacker not in blocked:
                spill += power * (2 if double else 1)
                continue
            blocker = blocked[attacker]
            block_power, block_toughness = self.stats(blocker)
            excess = max(0, power - block_toughness) if self.buffed(attacker) else 0
            spill += excess
            damage = power - excess
            assert not double
            packets.extend(((blocker, damage), (attacker, 0 if first and damage >= block_toughness else block_power)))
        self.life[1-self.current] -= spill
        deaths = []
        for ref, damage in packets:
            self.cards[ref]['damage'] += damage
            if self.cards[ref]['damage'] >= self.stats(ref)[1]:
                deaths.append(ref)
        for ref in deaths:
            self.cards[ref]['zone'] = 'graveyard'
        self.emit('resolve')
        self.emit('end_combat')
        self.attacking.clear()
        self.receipts()

    def combat_rider(self):
        mode = self.mode
        if mode in ('defender', 'tapped_required'):
            if mode == 'tapped_required':
                self.tap_for_combat('source')
            self.declare(rejected=('source',), required=True)
            self.combat()
        elif mode == 'required':
            self.declare(('source',), required=True, ready_requirement=True)
            self.combat()
        elif mode in ('opponent_wolf', 'enemy_team'):
            self.visit_upkeep()
            self.declare(('ally',))
            self.combat((('blocker', 'ally', True),) if mode == 'opponent_wolf' else ())
        elif mode in ('no_ally', 'small_ally', 'enemy_four'):
            self.declare(rejected=('source',))
            self.combat()
        elif mode in ('front_team', 'back_team'):
            self.declare(('source', 'ally'))
            self.combat((('blocker', 'source', True), ('second', 'ally', True)))
        elif mode in ('tribal_single', 'tribal_pair', 'other_creature'):
            self.declare(('ally',))
            blocks = [('blocker', 'ally', mode != 'tribal_single')]
            if mode == 'tribal_pair':
                blocks.append(('second', 'ally', True))
            self.combat(blocks, declaration_only=mode == 'tribal_pair')
        else:
            self.declare(('source',))
            if mode in ('other_color', 'same_color', 'artifact', 'front_block', 'front_first', 'front_single',
                        'back_single', 'back_pair', 'auto_block', 'tapped_blocker'):
                blocks = [('blocker', 'source', mode not in ('other_color', 'back_single', 'tapped_blocker'))]
                if mode == 'back_pair':
                    blocks.append(('second', 'source', True))
            else:
                blocks = []
            self.combat(blocks, automatic=mode == 'auto_block', first=mode == 'front_first',
                        double=mode == 'back_double', declaration_only=mode == 'back_pair')

    def run(self):
        self.emit('turn', player=self.owner, value=0)
        for who in (0, 1):
            self.fund(who, [0] * 6)
        self.cast_source()
        if self.mode == 'fresh_attack':
            self.combat_rider()
            return
        self.visit_upkeep()
        if self.mode == 'front_split':
            self.fog(self.owner)
            self.fog(self.foe)
            self.visit_upkeep()
            return
        if self.mode in FRONT_COMBAT:
            self.fog(self.foe)
        self.visit_upkeep()
        if self.mode in ('back_split', 'back_two'):
            for player in (self.owner if self.mode == 'back_split' else self.foe, self.foe):
                self.fog(player)
            self.visit_upkeep()
            return
        if self.mode == 'tapped_blocker':
            self.tap_for_combat('blocker')
        self.combat_rider()


def build(name, csvid, declaration, shape, variant, rows, oracle):
    mode, seat = variant.rsplit('_', 1)
    world = Board(name, shape, mode, int(seat), rows, oracle)
    world.run()
    return dict(name=f'WerewolfCombatRidersRecipe_{csvid}_{variant}', card=name, fixture='empty_main', seed=123,
                setup=world.setup, steps=world.steps,
                _recipe=dict(family=FAMILY, version=2, variant=variant, params=declaration['params'],
                             oracle_text=declaration['oracle_text'],
                             untested_components=['Multiplayer, competing combat requirements, and responses inside the synchronous upkeep callback.']))


def expand(name, csvid, declaration, shape, selected, rows, oracle):
    if rows[name]['csvid'] != csvid or not selected or len(set(selected)) != len(selected) or set(selected) - set(variants(shape)):
        raise ValueError('only distinct case variants for the selected physical front are allowed')
    validate_witnesses(oracle, rows)
    return [build(name, csvid, declaration, shape, variant, rows, oracle) for variant in selected]
