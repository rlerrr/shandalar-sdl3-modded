"""Paid targeted regeneration and complete affected-card riders."""

from functools import lru_cache
import json
from pathlib import Path

from death_counter_trigger_recipes import exact
from mill_death_and_entry_recipes import Duel as MillBoard, stats
from recipes import COLORS, cost_from_oracle


FAMILY = 'regeneration_targeted_abilities'
PROFILE = Path(__file__).with_name('data') / 'regeneration_targeted_abilities_wave8_profiles.json'
PREFIX = 'RegenerationTargetedAbilitiesRecipe_'
ASCETICISM, EZURI, ISAO, TOOLBOX = 11201, 11251, 7495, 4012
NIALL, RAGNAR, SUTURE, GNOMES = 373, 758, 9885, 2191
FOLLOW_ONS = {NIALL, RAGNAR, SUTURE, GNOMES}
TAP_COST = {NIALL, RAGNAR, GNOMES}
PRINTED = {
    ASCETICISM: ('scars_of_mirrodin.c', '{3}{G}{G}', 'Enchantment', None, ('Heal', 'Regenerate'), '{1}{G}',
                 'Creatures you control have hexproof.\n{1}{G}: Regenerate target creature. '
                 '(The next time it would be destroyed this turn, instead tap it, remove it from combat, and heal all damage on it.)'),
    EZURI: ('scars_of_mirrodin.c', '{1}{G}{G}', 'Legendary Creature — Elf Warrior', (2, 2), ('Regenerate',), '{G}',
            '{G}: Regenerate another target Elf.\n{2}{G}{G}{G}: Elf creatures you control get +3/+3 and gain trample until end of turn.'),
    ISAO: ('betrayers_of_kamigawa.c', '{2}{G}', 'Legendary Creature — Human Samurai', (2, 1), ('Bushido', 'Regenerate'), '{2}',
           "This spell can't be countered.\nBushido 2 (Whenever this creature blocks or becomes blocked, "
           'it gets +2/+2 until end of turn.)\n{2}: Regenerate target Samurai.'),
    TOOLBOX: ('urza_legacy.c', '{2}', 'Artifact Creature — Insect', (1, 1), ('Regenerate',), '{2}',
              '{2}: Regenerate target artifact creature.'),
    NIALL: ('the_dark.c', '{G}{G}{G}', 'Creature — Ouphe', (2, 2), ('Regenerate',), '{G}{G}{G}{G}',
            '{G}{G}{G}{G}, {T}: Regenerate target creature.'),
    RAGNAR: ('legends.c', '{G}{W}{U}', 'Legendary Creature — Human Cleric', (2, 2), ('Regenerate',), '{G}{W}{U}',
             '{G}{W}{U}, {T}: Regenerate target creature.'),
    SUTURE: ('eventide.c', '{1}{W}', 'Creature — Spirit', (1, 1), ('Flying', 'Regenerate'), '{W/B}{W/B}{W/B}',
             'Flying\n{W/B}{W/B}{W/B}: Regenerate target creature.'),
    GNOMES: ('homelands.c', '{4}', 'Artifact Creature — Gnome', (2, 2), ('Regenerate',), '{3}',
             '{3}, {T}: Regenerate target artifact creature.'),
}
MODES = {
    ASCETICISM: ('own', 'foreign', 'heal', 'combat', 'wrong_type', 'short', 'forbidden', 'proactive', 'hexproof'),
    EZURI: ('own', 'foreign', 'self', 'wrong_type', 'short', 'proactive', 'overrun'),
    ISAO: ('self', 'foreign', 'wrong_type', 'short', 'proactive', 'uncounterable', 'bushido'),
    TOOLBOX: ('self', 'own', 'foreign', 'wrong_type', 'not_creature', 'short', 'forbidden', 'proactive'),
    NIALL: ('own', 'foreign', 'sick', 'tapped', 'short', 'proactive'),
    RAGNAR: ('own', 'foreign', 'sick', 'tapped', 'short', 'proactive'),
    SUTURE: ('white', 'black', 'mixed', 'hybrid_menu', 'foreign', 'short', 'proactive', 'flying'),
    GNOMES: ('own', 'foreign', 'self', 'sick', 'tapped', 'short', 'wrong_type', 'not_creature', 'proactive'),
}
HOST, WALL, ELF = 'Prodigal Sorcerer', 'Dazzling Ramparts', 'Wellwisher'
SAMURAI, ARTIFACT_CREATURE, ARTIFACT = 'Mothrider Samurai', 'Bottle Gnomes', 'Millstone'
FLYING_WALL = 'Drift of Phantasms'
KILL, BREAK, FORBID, BOLT, COUNTER = 'Murder', 'Naturalize', 'Terminate', 'Lightning Bolt', 'Counterspell'
STOCK = ('Island', 'Forest', 'Plains', 'Mountain')
OTHER_STOCK = ('Wastes', 'Swamp', 'Mountain', 'Forest')
ZERO = [0] * 6
WITNESSES = set(STOCK + OTHER_STOCK) | {
    HOST, WALL, ELF, SAMURAI, ARTIFACT_CREATURE, ARTIFACT, FLYING_WALL, KILL, BREAK, FORBID, BOLT, COUNTER,
}


class Duel(MillBoard):
    def __init__(self, csvid, record, actor, mode, rows, oracle):
        self.csvid, self.record, self.actor, self.foe, self.mode = csvid, record, actor, 1 - actor, mode
        self.rows, self.oracle = rows, oracle
        self.setup, self.steps, self.cards, self.life = [], [], {}, [20, 20]
        self.active = actor
        self.library = {actor: list(STOCK), self.foe: list(OTHER_STOCK)}
        self.add('source', actor, record['name'], 'hand')
        for seat, cards in self.library.items():
            for index, name in enumerate(cards):
                self.add(f'library_{seat}_{index}', seat, name, 'library')
        self.victim = None
        self.setup_witnesses()

    def setup_witnesses(self):
        if self.mode == 'hexproof':
            self.add('host', self.actor, HOST, 'battlefield')
            self.add('own_probe', self.actor, WALL, 'battlefield')
            self.add('foe_probe', self.foe, HOST, 'battlefield')
            self.add('removal', self.foe, BREAK, 'hand')
        elif self.mode == 'overrun':
            self.add('ally', self.actor, ELF, 'battlefield')
            self.add('non_elf', self.actor, HOST, 'battlefield')
            self.add('foe_elf', self.foe, ELF, 'battlefield')
        elif self.mode == 'uncounterable':
            self.add('counter', self.foe, COUNTER, 'hand')
        elif self.mode == 'bushido':
            self.add('wall', self.foe, WALL, 'battlefield')
        else:
            self.victim = 'source' if self.mode == 'self' else 'victim'
            victim_name = {ASCETICISM: WALL, EZURI: ELF, ISAO: SAMURAI, TOOLBOX: ARTIFACT_CREATURE}[self.csvid]
            spell = KILL
            if self.mode == 'wrong_type':
                if self.csvid == ASCETICISM:
                    victim_name, spell = ARTIFACT, BREAK
                else:
                    victim_name = HOST
            if self.mode == 'not_creature':
                victim_name, spell = ARTIFACT, BREAK
            if self.mode == 'forbidden':
                spell = FORBID
            if self.mode == 'combat':
                victim_name = ARTIFACT_CREATURE
            if self.victim != 'source':
                owner = self.foe if self.mode == 'foreign' else self.actor
                self.add(self.victim, owner, victim_name, 'battlefield')
            self.add('removal', self.actor, spell, 'hand')
            if self.mode == 'heal':
                self.add('bolt', self.actor, BOLT, 'hand')

    @property
    def fee(self):
        return cost_from_oracle({'mana_cost': PRINTED[self.csvid][5]})

    def assert_choices(self, unused=0):
        self.emit('assert_unused_choices', value=unused)
        self.emit('assert_choices_consumed')

    def active_source(self):
        self.paid('source')
        if self.mode == 'uncounterable':
            self.emit('announce_spell', 'source')
            self.mana('set_mana', self.foe, cost_from_oracle(self.oracle[COUNTER]))
            self.emit('cast_response', 'counter', value=1)
            self.emit('assert_stack', value=2)
            self.observe('counter', 'in_hand', 0)
            self.mana('assert_mana', self.foe, ZERO)
            self.emit('resolve_response')
            self.assert_choices()
            self.cards['counter']['zone'] = 'graveyard'
        else:
            self.settle()
        self.cards['source']['zone'] = 'battlefield'
        self.observe('source', 'in_play', 1)
        if self.csvid != ASCETICISM:
            for prop, value in zip(('power', 'toughness'), stats(self.record)):
                self.observe('source', prop, value)
        self.receipts()

    def surviving(self):
        ref = self.victim
        self.observe(ref, 'in_play', 1)
        self.observe(ref, 'tapped', 1)
        self.observe(ref, 'damage', 0)
        self.observe(ref, 'attacking', 0)

    def destruction(self):
        if self.mode == 'combat':
            self.emit('begin_attackers')
            self.emit('attack', self.victim, value=1)
            self.emit('finish_attackers')
            self.observe(self.victim, 'attacking', 1)
        if self.mode == 'heal':
            self.paid('bolt', target=self.victim)
            self.settle()
            self.cards['bolt']['zone'] = 'graveyard'
            self.observe(self.victim, 'damage', 3)
        if self.mode == 'proactive':
            self.mana('set_mana', self.actor, self.fee)
            self.emit('refresh')
            self.emit('can_activate', 'source', value=1)
            self.emit('choose_target', self.victim, player=self.actor)
            self.emit('activate', 'source', value=1)
            self.emit('assert_stack', value=1)
            self.mana('assert_mana', self.actor, ZERO)
            self.settle()
            self.observe(self.victim, 'tapped', 0)
        self.paid('removal', target=self.victim)
        forbidden = self.mode in ('forbidden', 'wrong_type', 'not_creature') or (self.mode == 'self' and self.csvid == EZURI)
        proactive = self.mode == 'proactive'
        live = proactive or not (forbidden or self.mode == 'short')
        funding = ZERO if proactive else self.fee.copy()
        if self.mode == 'short':
            # Leave the same total with the wrong color where the ability has a colored requirement.
            if any(funding[1:]):
                funding = [sum(funding), 0, 0, 0, 0, 0]
            else:
                funding[0] -= 1
        self.mana('set_mana', self.actor, funding)
        self.emit('refresh')
        if not proactive:
            self.emit('choose_regeneration', 'source')
            if live:
                self.emit('choose_target', self.victim, player=self.actor)
        self.emit('resolve')
        self.assert_choices(int(not live))
        self.emit('refresh')
        self.mana('assert_mana', self.actor, ZERO if live else funding)
        self.cards['removal']['zone'] = 'graveyard'
        if live:
            self.surviving()
        else:
            self.cards[self.victim]['zone'] = 'graveyard'
            self.observe(self.victim, 'in_play', 0)
        self.receipts()
        if self.mode == 'combat':
            self.emit('begin_blockers')
            self.emit('advance_combat')
            self.finish_combat()
            self.receipts()

    def hexproof(self):
        self.emit('can_target_creature', 'own_probe', 'host', value=1)
        self.emit('can_target_creature', 'foe_probe', 'host', value=0)
        self.emit('can_target_creature', 'own_probe', 'foe_probe', value=1)
        self.go(self.foe)
        self.paid('removal', target='source')
        self.settle()
        self.cards['source']['zone'] = self.cards['removal']['zone'] = 'graveyard'
        self.emit('can_target_creature', 'foe_probe', 'host', value=1)
        self.receipts()

    def stats(self, ref, power, toughness):
        self.observe(ref, 'power', power)
        self.observe(ref, 'toughness', toughness)

    def overrun(self):
        self.mana('set_mana', self.actor, cost_from_oracle({'mana_cost': '{2}{G}{G}{G}'}))
        self.emit('refresh')
        self.emit('can_activate', 'source', value=1)
        self.emit('activate', 'source', value=1)
        self.emit('assert_stack', value=1)
        self.mana('assert_mana', self.actor, ZERO)
        self.settle()
        self.stats('source', 5, 5)
        self.stats('ally', 4, 4)
        self.stats('non_elf', 1, 1)
        self.stats('foe_elf', 1, 1)
        self.declare_combat('ally', 'foe_elf')
        self.finish_combat()
        self.cards['foe_elf']['zone'] = 'graveyard'
        self.life[self.foe] -= 3
        self.observe('ally', 'in_play', 1)
        self.receipts()
        self.go(self.foe)
        self.stats('source', 2, 2)
        self.stats('ally', 1, 1)

    def declare_combat(self, attacker, blocker):
        self.emit('begin_attackers')
        self.emit('attack', attacker, value=1)
        self.emit('finish_attackers')
        self.emit('begin_blockers')
        self.emit('block', blocker, attacker, value=1)
        self.emit('advance_combat')
        self.observe(attacker, 'blocked', 1)

    def finish_combat(self):
        self.emit('combat_damage')
        self.settle()
        self.emit('end_combat')

    def bushido(self):
        self.go(self.foe)
        self.go(self.actor)
        self.declare_combat('source', 'wall')
        self.stats('source', 4, 3)
        self.finish_combat()
        self.observe('wall', 'damage', 4)
        self.stats('source', 4, 3)
        self.receipts()
        self.go(self.foe)
        self.stats('source', 2, 1)

    def build(self):
        self.emit('turn', player=self.actor)
        self.emit('present_ai_dialogs', value=1)
        for seat in (0, 1):
            self.mana('set_mana', seat, ZERO)
        self.active_source()
        if self.victim:
            self.destruction()
        elif self.mode != 'uncounterable':
            {'hexproof': self.hexproof, 'overrun': self.overrun, 'bushido': self.bushido}[self.mode]()


class FollowOn(Duel):
    def setup_witnesses(self):
        if self.mode == 'flying':
            self.add('ground', self.foe, WALL, 'battlefield')
            self.add('air', self.foe, FLYING_WALL, 'battlefield')
            return
        self.victim = 'source' if self.mode == 'self' else 'victim'
        name = ARTIFACT_CREATURE if self.csvid == GNOMES else HOST
        if self.mode == 'wrong_type':
            name = HOST
        elif self.mode == 'not_creature':
            name = ARTIFACT
        if self.victim != 'source':
            self.add(self.victim, self.foe if self.mode == 'foreign' else self.actor, name, 'battlefield')
        self.add('removal', self.actor, BREAK if self.mode == 'not_creature' else KILL, 'hand')

    @property
    def fee(self):
        if self.csvid != SUTURE:
            return super().fee
        pool = ZERO.copy()
        pool[COLORS['B']] = {'black': 3, 'white': 0}.get(self.mode, 1)
        pool[COLORS['W']] = 3 - pool[COLORS['B']]
        return pool

    def age(self):
        self.go(self.foe)
        self.go(self.actor)

    def tap_by_attacking(self):
        self.emit('begin_attackers')
        self.emit('attack', 'source', value=1)
        self.emit('finish_attackers')
        self.emit('begin_blockers')
        self.emit('advance_combat')
        self.finish_combat()
        self.life[self.foe] -= stats(self.record)[0]
        self.observe('source', 'tapped', 1)
        self.receipts()

    def proactive_shield(self):
        self.mana('set_mana', self.actor, self.fee)
        self.emit('refresh')
        self.emit('can_activate', 'source', value=1)
        self.emit('choose_target', self.victim, player=self.actor)
        self.emit('activate', 'source', value=1)
        self.emit('assert_stack', value=1)
        self.mana('assert_mana', self.actor, ZERO)
        self.settle()
        self.observe('source', 'tapped', int(self.csvid in TAP_COST))
        self.observe(self.victim, 'tapped', 0)

    def destruction(self):
        if self.csvid in TAP_COST:
            if self.mode != 'sick':
                self.age()
            self.observe('source', 'sick', int(self.mode == 'sick'))
            if self.mode == 'tapped':
                self.tap_by_attacking()
            else:
                self.observe('source', 'tapped', 0)
        proactive = self.mode == 'proactive'
        if proactive:
            self.proactive_shield()
        self.paid('removal', target=self.victim)
        denied = self.mode in ('sick', 'tapped', 'wrong_type', 'not_creature', 'short')
        pool = ZERO.copy() if proactive else self.fee.copy()
        if self.mode == 'short':
            if any(pool[1:]):
                color = next(color for color, value in enumerate(pool) if color and value)
                pool[color] -= 1
                pool[0] += 1
            else:
                pool[0] -= 1
        elif self.mode == 'hybrid_menu':
            assert self.actor == 0
            pool[COLORS['B']] = pool[COLORS['W']] = 3
        self.mana('set_mana', self.actor, pool)
        self.emit('refresh')
        if not proactive:
            self.emit('choose_regeneration', 'source')
            if not denied:
                if self.mode == 'hybrid_menu':
                    self.emit('choose_payment', player=self.actor,
                              value=sum(value << (4 * color) for color, value in enumerate(self.fee)))
                self.emit('choose_target', self.victim, player=self.actor)
        self.emit('resolve')
        self.assert_choices(int(denied))
        self.emit('refresh')
        left = pool if proactive or denied else [value - cost for value, cost in zip(pool, self.fee)]
        self.mana('assert_mana', self.actor, left)
        self.cards['removal']['zone'] = 'graveyard'
        if denied:
            self.cards[self.victim]['zone'] = 'graveyard'
            self.observe(self.victim, 'in_play', 0)
        else:
            self.surviving()
        self.observe('source', 'tapped', int(self.csvid in TAP_COST and (not denied or self.mode == 'tapped')))
        self.receipts()

    def build(self):
        self.emit('turn', player=self.actor)
        self.emit('present_ai_dialogs', value=1)
        for seat in (0, 1):
            self.mana('set_mana', seat, ZERO)
        self.active_source()
        if self.mode == 'flying':
            self.flight()
        else:
            self.destruction()


@lru_cache(maxsize=1)
def profiles():
    return json.loads(PROFILE.read_bytes())


def classify(record, csvid):
    if (csvid not in PRINTED or not record or record.get('layout') != 'normal'
            or record.get('selected_face') or record.get('card_faces')):
        raise ValueError('whole normal targeted regeneration record required')
    _, cost, type_line, printed_stats, keywords, activation, rule = PRINTED[csvid]
    if (record.get('type_line') != type_line or record.get('mana_cost') != cost
            or record.get('keywords') != list(keywords) or record.get('oracle_text') != rule
            or (printed_stats is not None and stats(record) != printed_stats)):
        raise ValueError('complete printed regeneration costs, types and riders required')
    return dict(csvid=csvid, mana=cost_from_oracle(record), activation=activation, tap=csvid in TAP_COST,
                stats=list(printed_stats) if printed_stats is not None else None)


def variants(shape):
    return [f'{mode}_{seat}' for mode in MODES[shape['csvid']] for seat in (0, 1)
            if not (mode == 'hybrid_menu' and seat == 1)]


def require_variants(shape, selected):
    if (type(selected) is not list or not selected or any(type(mode) is not str for mode in selected)
            or len(selected) != len(set(selected)) or set(selected) - set(variants(shape))):
        raise ValueError('distinct reviewed regeneration variants required')


def spec(declaration, record):
    primary = profiles()['sources'].get(record.get('name', '')) if record else None
    if not primary or not exact(primary['oracle'], record):
        raise ValueError('exact pinned targeted regeneration record required')
    shape = classify(record, primary['registry']['csvid'])
    if (declaration.get('family') != FAMILY or type(declaration.get('version')) is not int
            or declaration['version'] != 2 or declaration.get('oracle_text') != record['oracle_text']
            or not exact(declaration.get('params'), shape)):
        raise ValueError('complete version 2 targeted regeneration declaration required')
    require_variants(shape, declaration.get('variants'))
    return dict(shape, family=FAMILY)


def validate_witnesses(oracle, rows):
    pins = profiles()
    if (set(pins) != {'sources', 'witnesses'} or set(pins['witnesses']) != WITNESSES
            or {pin['registry']['csvid'] for pin in pins['sources'].values()} != set(PRINTED)):
        raise ValueError('complete targeted regeneration source and witness pins required')
    for name, pin in (pins['sources'] | pins['witnesses']).items():
        row, record = rows.get(name, {}), oracle.get(name, {})
        if (not exact(record, pin['oracle']) or not exact(row, pin['registry'])
                or row.get('kind') != 'card' or row.get('handler') in (None, '0', 'NULL', 'card_dummy')
                or record.get('layout') != 'normal' or record.get('card_faces')):
            raise ValueError('pinned regeneration card changed: ' + name)
        if name in pins['sources']:
            classify(record, row['csvid'])


def expand(name, csvid, declaration, shape, selected, rows, oracle):
    require_variants(shape, selected)
    validate_witnesses(oracle, rows)
    if profiles()['sources'][name]['registry']['csvid'] != csvid or csvid != shape['csvid']:
        raise ValueError('exact registered regeneration primary required')
    cases = []
    for variant in selected:
        mode, seat = variant.rsplit('_', 1)
        implementation = FollowOn if csvid in FOLLOW_ONS else Duel
        board = implementation(csvid, oracle[name], int(seat), mode, rows, oracle)
        board.build()
        cases.append(dict(name=f'{PREFIX}{csvid}_{variant}', card=name, fixture='empty_main', seed=2897,
                          setup=board.setup, steps=board.steps, _recipe=dict(
                              family=FAMILY, version=2, variant=variant, params=declaration['params'],
                              oracle_text=declaration['oracle_text'])))
    return cases

