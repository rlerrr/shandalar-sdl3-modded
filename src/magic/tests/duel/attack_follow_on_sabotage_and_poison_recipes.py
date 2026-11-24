"""Combat sabotage and poison at actual declaration and damage boundaries."""

from functools import lru_cache
import json
from pathlib import Path

from death_counter_trigger_recipes import exact
from mill_death_and_entry_recipes import Duel as PaidBoard, stats


FAMILY = 'attack_follow_on_sabotage_and_poison'
PROFILE = Path(__file__).with_name('data') / 'attack_follow_on_sabotage_and_poison_wave8_profiles.json'
PREFIX = 'AttackFollowOnSabotageAndPoisonRecipe_'
MOSQUITO, FLORAL, VANDAL, NECRITE = 2365, 639, 2986, 1809
UNBLOCKED = "Whenever this creature attacks and isn't blocked, "
PRINTED = {
    MOSQUITO: ('alliances.c', '{1}{B}', 'Creature — Insect', (0, 1), ('Flying',),
               'Flying\n' + UNBLOCKED + 'defending player gets a poison counter. '
               '(A player with ten or more poison counters loses the game.)'),
    FLORAL: ('legends.c', '{3}{G}', 'Creature — Elemental', (2, 2), (),
             UNBLOCKED + 'you may destroy target artifact defending player controls. '
             'If you do, this creature assigns no combat damage this turn.'),
    VANDAL: ('weatherlight.c', '{R}', 'Creature — Goblin Rogue', (1, 1), (),
             UNBLOCKED + 'you may pay {R}. If you do, destroy target artifact defending player controls '
             'and this creature assigns no combat damage this turn.'),
    NECRITE: ('fallen_empires.c', '{1}{B}{B}', 'Creature — Thrull', (2, 2), (),
              UNBLOCKED + 'you may sacrifice it. If you do, destroy target creature defending player controls. '
              "It can't be regenerated."),
}
MODES = {
    MOSQUITO: ('unblocked', 'ground_block_fails', 'air_block', 'blocked_then_bounced', 'other_attack', 'repeat'),
    FLORAL: ('destroy', 'decline', 'own_artifact', 'blocked', 'expiry'),
    VANDAL: ('paid', 'decline', 'wrong_color', 'own_artifact', 'blocked', 'indestructible', 'once'),
    NECRITE: ('sacrifice', 'decline', 'blocked', 'cannot_regenerate', 'indestructible'),
}
GROUND, AIR = 'Dazzling Ramparts', 'Drift of Phantasms'
ARTIFACT, INGOT, SENTINEL = 'Millstone', 'Darksteel Ingot', 'Darksteel Sentinel'
CREATURE, SKELETON, BOUNCE = 'Prodigal Sorcerer', 'Drudge Skeletons', 'Unsummon'
ZERO = (0,) * 6
RED = (0, 0, 0, 0, 1, 0)
BLACK = (0, 1, 0, 0, 0, 0)


def label(csvid, mode, actor):
    return f'{PREFIX}{csvid}_{mode}_{actor}'


class Duel(PaidBoard):
    def __init__(self, csvid, record, actor, mode, rows, oracle):
        self.csvid, self.record, self.actor, self.foe, self.mode = csvid, record, actor, 1 - actor, mode
        self.rows, self.oracle = rows, oracle
        self.setup, self.steps, self.cards = [], [], {}
        self.life, self.poison = [20, 20], [0, 0]
        self.active = actor
        self.library = {0: [], 1: []}
        self.add('source', actor, record['name'], 'hand')
        self.power, self.toughness = stats(record)
        if csvid == MOSQUITO:
            if mode in ('ground_block_fails', 'air_block', 'blocked_then_bounced'):
                self.add('wall', self.foe, GROUND if mode == 'ground_block_fails' else AIR, 'battlefield')
            if mode == 'blocked_then_bounced':
                self.add('bounce', self.actor, BOUNCE, 'hand')
            if mode == 'other_attack':
                self.add('partner', self.actor, CREATURE, 'battlefield')
        else:
            if mode == 'blocked':
                self.add('wall', self.foe, GROUND, 'battlefield')
            if csvid in (FLORAL, VANDAL):
                self.add('own_artifact', self.actor, ARTIFACT, 'battlefield')
                if mode != 'own_artifact':
                    self.add('victim', self.foe, INGOT if mode == 'indestructible' else ARTIFACT, 'battlefield')
                if mode == 'once':
                    self.add('untargeted', self.foe, ARTIFACT, 'battlefield')
            else:
                self.add('own_creature', self.actor, CREATURE, 'battlefield')
                victim = SKELETON if mode == 'cannot_regenerate' else SENTINEL if mode == 'indestructible' else CREATURE
                self.add('victim', self.foe, victim, 'battlefield')

    def cast_to(self, ref, zone, target=None):
        self.paid(ref, target=target)
        self.settle()
        self.cards[ref]['zone'] = zone

    def age(self):
        self.go(self.foe)
        self.go(self.actor)

    def observe_poison(self):
        for player in (0, 1):
            self.emit('assert_poison', player=player, value=self.poison[player])

    def attack(self, *, blocked=False, attempt_block=False, source_attacks=True):
        attacker = 'source' if source_attacks else 'partner'
        self.emit('begin_attackers')
        self.emit('attack', attacker, value=1)
        self.observe('source', 'attacking', int(source_attacks))
        self.emit('finish_attackers')
        self.settle()
        self.observe_poison()
        self.emit('begin_blockers')
        if attempt_block or blocked:
            self.emit('block', 'wall', 'source', value=int(blocked))

    def declare(self, unused=0):
        self.emit('advance_combat')
        self.emit('assert_unused_choices', value=unused)
        self.emit('assert_choices_consumed')
        self.emit('refresh')

    def damage(self, attack_damage):
        self.emit('combat_damage')
        self.settle()
        self.life[self.foe] -= attack_damage
        self.observe_poison()
        self.receipts()
        self.emit('end_combat')
        self.settle()

    def mosquito(self):
        source_attacks = self.mode != 'other_attack'
        blocked = self.mode in ('air_block', 'blocked_then_bounced')
        self.attack(blocked=blocked, attempt_block=self.mode == 'ground_block_fails', source_attacks=source_attacks)
        self.declare()
        self.observe('source', 'blocked', int(blocked))
        if source_attacks and not blocked:
            self.poison[self.foe] += 1
        self.observe_poison()
        for player in (0, 1):
            self.emit('assert_life', player=player, value=self.life[player])
        if self.mode == 'blocked_then_bounced':
            self.cast_to('bounce', 'graveyard', 'wall')
            self.cards['wall']['zone'] = 'hand'
            self.observe('source', 'blocked', 1)
        self.damage(int(not source_attacks))
        if self.mode == 'repeat':
            self.age()
            self.attack()
            self.declare()
            self.poison[self.foe] += 1
            self.observe_poison()
            self.damage(0)

    def floral(self):
        blocked = self.mode == 'blocked'
        accepted = self.mode in ('destroy', 'expiry')
        self.attack(blocked=blocked)
        if accepted:
            self.emit('choose_target', 'victim', player=self.actor)
        elif self.mode == 'decline':
            self.emit('choose_cancel_target', player=self.actor)
        self.declare()
        if accepted:
            self.cards['victim']['zone'] = 'graveyard'
        self.receipts()
        self.damage(0 if blocked or accepted else self.power)
        if self.mode == 'expiry':
            self.age()
            self.attack()
            self.declare()
            self.damage(self.power)

    def vandal(self):
        accepted = self.mode in ('paid', 'indestructible', 'once')
        available = (1, 0, 0, 0, 0, 0) if self.mode == 'wrong_color' else (0, 0, 0, 0, 2, 0) if self.mode == 'once' else RED
        self.mana('set_mana', self.actor, available)
        self.emit('refresh')
        self.emit('can_activate', 'source', value=0)
        blocked = self.mode == 'blocked'
        self.attack(blocked=blocked)
        self.emit('can_activate', 'source', value=0)
        self.declare()
        can_activate = accepted or self.mode == 'decline'
        self.emit('can_activate', 'source', value=int(can_activate))
        self.receipts()
        remaining = available
        if accepted:
            self.emit('choose_target', 'victim', player=self.actor)
            self.emit('activate', 'source', value=1)
            self.emit('assert_stack', value=1)
            remaining = RED if self.mode == 'once' else ZERO
            self.mana('assert_mana', self.actor, remaining)
            self.settle()
            if self.mode != 'indestructible':
                self.cards['victim']['zone'] = 'graveyard'
            self.emit('can_activate', 'source', value=0)
        self.mana('assert_mana', self.actor, remaining)
        self.receipts()
        self.damage(0 if blocked or accepted else self.power)

    def necrite(self):
        blocked = self.mode == 'blocked'
        accepted = self.mode in ('sacrifice', 'cannot_regenerate', 'indestructible')
        can_regenerate = self.mode == 'cannot_regenerate'
        if can_regenerate:
            self.mana('set_mana', self.foe, BLACK)
            self.emit('refresh')
        self.attack(blocked=blocked)
        if not blocked:
            self.emit('choose_dialog', player=self.actor, value=int(not accepted))
        if accepted:
            self.emit('choose_target', 'victim', player=self.actor)
        if can_regenerate:
            self.emit('choose_regeneration', 'victim')
        self.declare(unused=int(can_regenerate))
        if accepted:
            self.cards['source']['zone'] = 'graveyard'
            if self.mode != 'indestructible':
                self.cards['victim']['zone'] = 'graveyard'
        self.observe('source', 'in_play', int(not accepted))
        self.observe('victim', 'in_play', int(self.cards['victim']['zone'] == 'battlefield'))
        self.mana('assert_mana', self.foe, BLACK if can_regenerate else ZERO)
        self.receipts()
        self.damage(0 if blocked or accepted else self.power)

    def build(self):
        for player in (0, 1):
            self.mana('set_mana', player, ZERO)
        self.emit('turn', player=self.actor)
        self.emit('present_ai_dialogs', value=1)
        self.cast_to('source', 'battlefield')
        self.observe('source', 'in_play', 1)
        self.observe('source', 'power', self.power)
        self.observe('source', 'toughness', self.toughness)
        self.age()
        self.observe('source', 'sick', 0)
        self.observe_poison()
        action = {MOSQUITO: self.mosquito, FLORAL: self.floral, VANDAL: self.vandal, NECRITE: self.necrite}
        action[self.csvid]()
        self.receipts()


PRINTED.update({2984: ('weatherlight.c',
        '{3}{R}',
        'Creature — Goblin',
        (2, 2),
        (),
        "Whenever this creature attacks and isn't blocked, you may sacrifice it. If you do, destroy target "
        'creature and target land.'),
 5956: ('judgement.c',
        '{3}{B}{B}',
        'Creature — Horror',
        (0, 4),
        ('Fear',),
        "Fear (This creature can't be blocked except by artifact creatures and/or black creatures.)\n"
        "Whenever this creature attacks and isn't blocked, defending player loses 1 life for each card in "
        'their graveyard.'),
 371: ('the_dark.c',
       '{3}{B}',
       'Creature — Zombie',
       (2, 2),
       (),
       "Whenever this creature attacks and isn't blocked, it gets +2/+0 until end of combat."),
 2788: ('visions.c',
        '{1}{B}{B}',
        'Creature — Human Assassin',
        (1, 1),
        ('Fear',),
        "Fear (This creature can't be blocked except by artifact creatures and/or black creatures.)\n"
        "Whenever this creature attacks and isn't blocked, defending player gets a poison counter. (A "
        'player with ten or more poison counters loses the game.)'),
 3442: ('stronghold.c',
        '{3}{U}',
        'Creature — Thalakos Wizard',
        (1, 1),
        ('Shadow',),
        'Shadow (This creature can block or be blocked by only creatures with shadow.)\n'
        "Whenever this creature attacks and isn't blocked, you may sacrifice it. If you do, gain control of "
        'target creature. (This effect lasts indefinitely.)'),
 2441: ('alliances.c',
        '{3}{B}',
        'Creature — Snake',
        (3, 3),
        (),
        "Whenever this creature attacks and isn't blocked, defending player gets a poison counter. (A "
        'player with ten or more poison counters loses the game.)')})

GRENADIERS, GUILTFEEDER, MURK, ASSASSIN, DECEIVER, COBRA = 2984, 5956, 371, 2788, 3442, 2441
FOLLOWERS = {
    GRENADIERS: ('foe_targets', 'own_targets', 'split_targets', 'missing_land', 'decline', 'blocked'),
    GUILTFEEDER: ('empty_defender', 'three', 'ground_denied', 'black_blocker', 'artifact_blocker', 'other_attack'),
    MURK: ('unblocked', 'blocked', 'blocked_then_bounced', 'other_attack'),
    ASSASSIN: ('unblocked', 'ground_denied', 'black_blocker', 'artifact_blocker', 'other_attack'),
    DECEIVER: ('gain', 'owner_return', 'indefinite', 'own_only', 'nonshadow', 'shadow_block',
               'shadow_defend', 'normal_defend_denied'),
    COBRA: ('unblocked', 'blocked', 'blocked_then_bounced', 'other_attack'),
}
MYR, SHADOW, ISLAND, MOUNTAIN = 'Darksteel Myr', 'Soltari Lancer', 'Island', 'Mountain'
WITNESSES = {GROUND, AIR, ARTIFACT, INGOT, SENTINEL, CREATURE, SKELETON, BOUNCE, MYR, SHADOW, ISLAND, MOUNTAIN}
GAPS = {
    VANDAL: ['the native post-blocking activation presents the printed optional payment without a distinct trigger stack object'],
    GRENADIERS: ['the native post-blocking activation presents the printed optional sacrifice without a distinct trigger stack object'],
    DECEIVER: ['the native post-blocking activation presents the printed optional sacrifice without a distinct trigger stack object'],
}


class FollowDuel(Duel):
    def __init__(self, csvid, record, actor, mode, rows, oracle):
        self.csvid, self.record, self.actor, self.foe, self.mode = csvid, record, actor, 1 - actor, mode
        self.rows, self.oracle = rows, oracle
        self.setup, self.steps, self.cards = [], [], {}
        self.life, self.poison = [20, 20], [0, 0]
        self.active = actor
        self.library = {0: [], 1: []}
        self.add('source', actor, record['name'], 'hand')
        self.power, self.toughness = stats(record)
        if csvid == GRENADIERS:
            self.add('own_creature', actor, CREATURE, 'battlefield')
            self.add('foe_creature', self.foe, CREATURE, 'battlefield')
            if mode != 'missing_land':
                self.add('own_land', actor, ISLAND, 'battlefield')
                self.add('foe_land', self.foe, MOUNTAIN, 'battlefield')
            if mode == 'blocked':
                self.add('wall', self.foe, GROUND, 'battlefield')
        elif csvid == DECEIVER:
            if mode == 'own_only':
                self.add('own_creature', actor, CREATURE, 'battlefield')
            elif mode in ('shadow_defend', 'normal_defend_denied'):
                self.add('foe_attacker', self.foe, SHADOW if mode == 'shadow_defend' else CREATURE, 'battlefield')
            else:
                self.add('victim', self.foe, CREATURE, 'battlefield')
            if mode == 'shadow_block':
                self.add('wall', self.foe, SHADOW, 'battlefield')
            if mode == 'owner_return':
                self.add('bounce', actor, BOUNCE, 'hand')
        else:
            self.setup_attack_witnesses()

    def setup_attack_witnesses(self):
        block_cards = {'ground_denied': CREATURE, 'black_blocker': SKELETON,
                       'artifact_blocker': MYR, 'blocked': GROUND, 'blocked_then_bounced': GROUND}
        if self.mode in block_cards:
            self.add('wall', self.foe, block_cards[self.mode], 'battlefield')
        if self.mode == 'blocked_then_bounced':
            self.add('bounce', self.actor, BOUNCE, 'hand')
        if self.mode == 'other_attack':
            self.add('partner', self.actor, CREATURE, 'battlefield')
        if self.csvid == GUILTFEEDER:
            self.add('own_only', self.actor, CREATURE, 'graveyard')
            if self.mode != 'empty_defender':
                for index, name in enumerate((ARTIFACT, MOUNTAIN, CREATURE)):
                    self.add(f'foe_grave_{index}', self.foe, name, 'graveyard')

    def build(self):
        for player in (0, 1):
            self.mana('set_mana', player, ZERO)
        self.emit('turn', player=self.actor)
        self.emit('present_ai_dialogs', value=1)
        self.cast_to('source', 'battlefield')
        self.observe('source', 'power', self.power)
        self.observe('source', 'toughness', self.toughness)
        self.age()
        self.observe('source', 'sick', 0)
        self.observe_poison()
        if self.csvid == GRENADIERS:
            self.grenadiers()
        elif self.csvid == DECEIVER:
            self.deceiver()
        else:
            self.unblocked_effect()
        self.receipts()

    def unblocked_effect(self):
        source_attacks = self.mode != 'other_attack'
        blocked = self.mode in ('blocked', 'blocked_then_bounced', 'black_blocker', 'artifact_blocker')
        attack_effect = source_attacks and not blocked
        self.attack(blocked=blocked, attempt_block=self.mode == 'ground_denied', source_attacks=source_attacks)
        for player in (0, 1):
            self.emit('assert_life', player=player, value=self.life[player])
        self.declare()
        self.observe('source', 'blocked', int(blocked))
        combat_power = self.power
        if self.csvid == MURK:
            combat_power += 2 * attack_effect
            self.observe('source', 'power', combat_power)
            self.observe('source', 'toughness', self.toughness)
        elif self.csvid == GUILTFEEDER and attack_effect:
            self.life[self.foe] -= sum(item['player'] == self.foe and item['zone'] == 'graveyard'
                                       for item in self.cards.values())
        elif self.csvid in (ASSASSIN, COBRA) and attack_effect:
            self.poison[self.foe] += 1
        self.observe_poison()
        for player in (0, 1):
            self.emit('assert_life', player=player, value=self.life[player])
        if self.mode == 'blocked_then_bounced':
            self.cast_to('bounce', 'graveyard', 'wall')
            self.cards['wall']['zone'] = 'hand'
            self.observe('source', 'blocked', 1)
        if self.csvid == ASSASSIN and self.mode == 'black_blocker':
            self.cards['source']['zone'] = self.cards['wall']['zone'] = 'graveyard'
        damage = combat_power if attack_effect else int(not source_attacks)
        self.damage(damage)
        if self.csvid == MURK:
            self.observe('source', 'power', self.power)
            self.observe('source', 'toughness', self.toughness)

    def sacrifice_activation(self, targets):
        for target in targets:
            self.emit('choose_target', target, player=self.actor)
        self.emit('activate', 'source', value=1)
        self.emit('assert_stack', value=1)
        self.emit('assert_choices_consumed')
        self.cards['source']['zone'] = 'graveyard'
        self.observe('source', 'in_play', 0)
        for target in targets:
            self.observe(target, 'in_play', 1)
        self.settle()

    def grenadiers(self):
        blocked = self.mode == 'blocked'
        self.emit('can_activate', 'source', value=0)
        self.attack(blocked=blocked)
        self.emit('can_activate', 'source', value=0)
        self.declare()
        can_activate = self.mode not in ('blocked', 'missing_land')
        self.emit('can_activate', 'source', value=int(can_activate))
        targets = {
            'foe_targets': ('foe_creature', 'foe_land'),
            'own_targets': ('own_creature', 'own_land'),
            'split_targets': ('own_creature', 'foe_land'),
        }.get(self.mode)
        if targets:
            self.sacrifice_activation(targets)
            for ref in targets:
                self.cards[ref]['zone'] = 'graveyard'
        self.mana('assert_mana', self.actor, ZERO)
        self.receipts()
        self.damage(0 if blocked or targets else self.power)

    def deceiver(self):
        if self.mode in ('shadow_defend', 'normal_defend_denied'):
            self.deceiver_defends()
            return
        self.emit('can_activate', 'source', value=0)
        blocked = self.mode == 'shadow_block'
        self.attack(blocked=blocked)
        if self.mode == 'nonshadow':
            self.emit('block', 'victim', 'source', value=0)
        self.emit('can_activate', 'source', value=0)
        self.declare()
        self.emit('can_activate', 'source', value=int(not blocked))
        gain = self.mode in ('gain', 'owner_return', 'indefinite')
        if gain:
            self.sacrifice_activation(('victim',))
            self.cards['victim']['player'] = self.actor
            self.emit('bind_permanent_copy', 'controlled', player=self.actor, index=0, value=self.rows[CREATURE]['csvid'])
            self.observe('controlled', 'in_play', 1)
            self.receipts()
        if blocked:
            self.cards['source']['zone'] = 'graveyard'
        self.mana('assert_mana', self.actor, ZERO)
        self.damage(0 if blocked or gain else self.power)
        if self.mode == 'indefinite':
            self.age()
            self.observe('controlled', 'in_play', 1)
        elif self.mode == 'owner_return':
            self.cast_to('bounce', 'graveyard', 'controlled')
            self.cards['victim']['player'] = self.foe
            self.cards['victim']['zone'] = 'hand'

    def deceiver_defends(self):
        legal = self.mode == 'shadow_defend'
        self.go(self.foe)
        self.emit('begin_attackers')
        self.emit('attack', 'foe_attacker', value=1)
        self.emit('finish_attackers')
        self.settle()
        self.emit('begin_blockers')
        self.emit('block', 'source', 'foe_attacker', value=int(legal))
        self.declare()
        self.emit('can_activate', 'source', value=0)
        self.emit('combat_damage')
        self.settle()
        if legal:
            self.cards['source']['zone'] = 'graveyard'
            self.observe('foe_attacker', 'damage', 0)
        else:
            self.life[self.actor] -= 1
        self.emit('end_combat')
        self.settle()


@lru_cache(maxsize=1)
def profiles():
    return json.loads(PROFILE.read_bytes())


def classify(record, csvid):
    expected = PRINTED.get(csvid)
    observed = (record.get('mana_cost'), record.get('type_line'), stats(record),
                tuple(record.get('keywords', ())), record.get('oracle_text'))
    if expected is None or expected[1:] != observed or record.get('layout') != 'normal' or record.get('card_faces'):
        raise ValueError('complete printed sabotage or poison record required')
    return dict(csvid=csvid, printed_cost=expected[1], keywords=list(expected[4]))


def variants(shape):
    csvid = shape['csvid']
    return [f'{mode}_{actor}' for actor in (0, 1) for mode in (MODES if csvid in MODES else FOLLOWERS)[csvid]]


def require_variants(shape, selected):
    if (type(selected) is not list or not selected or any(type(item) is not str for item in selected)
            or len(selected) != len(set(selected)) or set(selected) - set(variants(shape))):
        raise ValueError('distinct reviewed sabotage and poison variants required')


def spec(declaration, record):
    pin = profiles()['sources'].get(record.get('name', '')) if record else None
    if not pin or not exact(pin['oracle'], record):
        raise ValueError('exact pinned sabotage and poison card required')
    shape = classify(record, pin['registry']['csvid'])
    if (declaration.get('family') != FAMILY or type(declaration.get('version')) is not int
            or declaration['version'] != 2 or declaration.get('oracle_text') != record['oracle_text']
            or not exact(declaration.get('params'), shape)):
        raise ValueError('complete version 2 sabotage and poison recipe required')
    require_variants(shape, declaration.get('variants'))
    return dict(shape, family=FAMILY)


def validate_witnesses(oracle, rows):
    pins = profiles()
    if (set(pins) != {'sources', 'witnesses'} or set(pins['witnesses']) != WITNESSES
            or {pin['registry']['csvid'] for pin in pins['sources'].values()} != set(PRINTED)):
        raise ValueError('complete sabotage and poison source and witness pins required')
    for name, pin in (pins['sources'] | pins['witnesses']).items():
        row = rows.get(name, {})
        if (not exact(oracle.get(name), pin['oracle']) or not exact(row, pin['registry'])
                or row.get('kind') != 'card' or row.get('handler') in (None, '0', 'NULL', 'card_dummy')
                or oracle[name].get('layout') != 'normal' or oracle[name].get('card_faces')):
            raise ValueError('pinned sabotage or poison card changed: ' + name)
        if name in pins['sources']:
            classify(oracle[name], row['csvid'])


def expand(name, csvid, declaration, shape, selected, rows, oracle):
    require_variants(shape, selected)
    validate_witnesses(oracle, rows)
    if profiles()['sources'][name]['registry']['csvid'] != csvid or csvid != shape['csvid']:
        raise ValueError('exact registered sabotage or poison card required')
    cases = []
    for variant in selected:
        mode, actor = variant.rsplit('_', 1)
        builder = Duel if csvid in MODES else FollowDuel
        board = builder(csvid, oracle[name], int(actor), mode, rows, oracle)
        board.build()
        recipe = dict(family=FAMILY, version=2, variant=variant, params=declaration['params'],
                      oracle_text=declaration['oracle_text'])
        if csvid in GAPS:
            recipe['untested_components'] = GAPS[csvid]
        cases.append(dict(name=label(csvid, mode, actor), card=name, fixture='empty_main', seed=3062,
                          setup=board.setup, steps=board.steps, _recipe=recipe))
    return cases
