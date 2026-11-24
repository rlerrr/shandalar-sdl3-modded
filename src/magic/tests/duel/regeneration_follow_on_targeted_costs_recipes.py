"""Targeted regeneration with actual paid costs and compound riders."""

from functools import lru_cache
import json
from pathlib import Path

from death_counter_trigger_recipes import exact
from mill_death_and_entry_recipes import Duel as MillBoard, stats
from recipes import cost_from_oracle


FAMILY = 'regeneration_follow_on_targeted_costs'
PROFILE = Path(__file__).with_name('data') / 'regeneration_follow_on_targeted_costs_wave8_profiles.json'
PREFIX = 'RegenerationFollowOnTargetedCostsRecipe_'
CYLIX, CHIRURGEON, HERBALIST, LIFEBREATHER = 1762, 1781, 4584, 7139
VILLAGE, BARON, LICID, CUBE, MATRIX = 2652, 1632, 3217, 1760, 714
FOLLOW_ONS = {VILLAGE, BARON, LICID, CUBE, MATRIX}
PRINTED = {
    CYLIX: ('fallen_empires.c', '{3}', 'Artifact', None, ('Regenerate',), '{2}',
            '{2}, {T}, Discard a card at random: Regenerate target creature.'),
    CHIRURGEON: ('fallen_empires.c', '{R}', 'Creature — Goblin Shaman', (0, 2), ('Regenerate',), None,
                 'Sacrifice a Goblin: Regenerate target creature.'),
    HERBALIST: ('mercadian_masques.c', '{2}{G}', 'Creature — Human Spellshaper', (2, 2), ('Regenerate',), '{G}',
                '{G}, {T}, Discard a card: Regenerate target creature.'),
    LIFEBREATHER: ('mirage.c', '{4}{G}', 'Creature — Troll Shaman', (3, 2), ('Regenerate',), '{G}',
                   '{G}, {T}, Sacrifice a Forest: Regenerate target creature.'),
    VILLAGE: ('mirage.c', '{G}', 'Creature — Human Druid', (1, 1), ('Regenerate',), '{G}',
              '{G}, {T}, Sacrifice a Forest: Regenerate target creature.'),
    BARON: ('homelands.c', '{5}{B}{B}{B}', 'Legendary Creature — Vampire Noble', (5, 5), ('Flying', 'Regenerate'), None,
            'Flying\nWhenever a creature dealt damage by Baron Sengir this turn dies, put a +2/+2 counter on Baron Sengir.\n'
            '{T}: Regenerate another target Vampire.'),
    LICID: ('tempest.c', '{1}{G}', 'Creature — Licid', (1, 1), ('Regenerate',), '{G}',
            '{G}, {T}: This creature loses this ability and becomes an Aura enchantment with enchant creature. Attach it to '
            'target creature. You may pay {G} to end this effect.\n{G}: Regenerate enchanted creature.'),
    CUBE: ('fallen_empires.c', '{1}', 'Artifact', None, ('Regenerate',), '{2}',
           "{2}, {T}: This turn, when target creature you control attacks and isn't blocked, it assigns no combat damage "
           'this turn and you put a cube counter on this artifact.\n{2}, Remove a cube counter from this artifact: Regenerate target creature.'),
    MATRIX: ('legends.c', '{4}', 'Artifact', None, ('Regenerate',), '{4}',
             '{4}, {T}: Put a matrix counter on target creature and that creature gains "Remove a matrix counter from this '
             'creature: Regenerate this creature." Activate only during your upkeep.'),
}
MODES = {
    CYLIX: ('own', 'foreign', 'mixed_random', 'empty', 'short', 'wrong_type', 'proactive'),
    CHIRURGEON: ('self_cost', 'other_goblin', 'foreign', 'sacrifice_lock', 'wrong_type', 'proactive'),
    HERBALIST: ('own', 'foreign', 'choose_discard', 'empty', 'sick', 'tapped', 'short', 'proactive'),
    LIFEBREATHER: ('basic', 'nonbasic', 'from_forest', 'foreign', 'foe_forest', 'sick', 'tapped', 'proactive'),
    VILLAGE: ('basic', 'nonbasic', 'from_forest', 'foreign', 'foe_forest', 'sick', 'tapped', 'proactive'),
    BARON: ('vampire', 'foreign', 'self', 'wrong_type', 'proactive', 'grow', 'after_turn'),
    LICID: ('own', 'foreign', 'revert', 'sick', 'short', 'unattached'),
    CUBE: ('consumed', 'blocked', 'unrelated', 'proactive', 'target_scope'),
    MATRIX: ('own_upkeep', 'opponent_upkeep', 'main'),
}
WALL, ARTIFACT, GOBLIN = 'Dazzling Ramparts', 'Millstone', 'Rummaging Goblin'
ANGEL, FOREST, DUAL = 'Angel of Jubilation', 'Forest', 'Taiga'
PITCHES = ('Plains', 'Swamp')
KILL, BREAK = 'Murder', 'Naturalize'
HOST, VAMPIRE, SPIDER, CHISEI = 'Prodigal Sorcerer', 'Child of Night', 'Silklash Spider', 'Chisei, Heart of Oceans'
STOCK = ('Island', 'Forest', 'Plains', 'Mountain')
OTHER_STOCK = ('Wastes', 'Swamp', 'Mountain', 'Forest')
ZERO = [0] * 6
DISCARD = {CYLIX, HERBALIST}
AGE_FOR_TAP = {HERBALIST, LIFEBREATHER, VILLAGE}
DENIED = {'empty', 'short', 'wrong_type', 'sacrifice_lock', 'foe_forest', 'sick', 'tapped'}
WITNESSES = set(STOCK + OTHER_STOCK) | {WALL, ARTIFACT, GOBLIN, ANGEL, FOREST, DUAL, KILL, BREAK,
                                       HOST, VAMPIRE, SPIDER, CHISEI, *PITCHES}


def scenarios(csvid):
    return [(mode, actor) for mode in MODES[csvid] for actor in (0, 1)
            if not (mode == 'choose_discard' and actor == 1)]


class Duel(MillBoard):
    def __init__(self, csvid, record, actor, mode, rows, oracle):
        self.csvid, self.record, self.actor, self.foe, self.mode = csvid, record, actor, 1 - actor, mode
        self.rows, self.oracle = rows, oracle
        self.setup, self.steps, self.cards, self.life = [], [], {}, [20, 20]
        self.active = actor
        self.library = {actor: list(STOCK), self.foe: list(OTHER_STOCK)}
        self.random_consumed = False
        self.cost_ref = None
        self.add('source', actor, record['name'], 'hand')
        self.add('victim', self.foe if mode == 'foreign' else actor,
                 ARTIFACT if mode == 'wrong_type' else WALL, 'battlefield')
        self.add('removal', self.foe, BREAK if mode == 'wrong_type' else KILL, 'hand')
        self.setup_cost()
        for seat, cards in self.library.items():
            for index, name in enumerate(cards):
                self.add(f'library_{seat}_{index}', seat, name, 'library')

    def setup_cost(self):
        if self.csvid in DISCARD:
            if self.mode != 'empty':
                self.add('fodder', self.actor, PITCHES[0], 'hand')
                self.cost_ref = 'fodder'
            if self.mode in ('choose_discard', 'mixed_random'):
                self.add('alternate', self.actor, PITCHES[1], 'hand')
                if self.mode == 'choose_discard':
                    self.cost_ref = 'alternate'
        elif self.csvid == CHIRURGEON:
            self.cost_ref = 'source'
            if self.mode in ('other_goblin', 'foreign'):
                # The AI may legally choose either copy, so exact zone counts
                # remain valid without pinning its sacrifice preference.
                self.add('fodder', self.actor, GOBLIN if self.actor == 0 else self.record['name'], 'battlefield')
                self.cost_ref = 'fodder'
            elif self.mode == 'sacrifice_lock':
                self.add('lock', self.foe, ANGEL, 'battlefield')
            self.add('non_goblin', self.actor, WALL, 'battlefield')
        else:
            if self.mode == 'foe_forest':
                self.add('foe_forest', self.foe, FOREST, 'battlefield')
                self.add('wrong_land', self.actor, 'Mountain', 'battlefield')
            else:
                name = DUAL if self.mode == 'nonbasic' else FOREST
                self.add('fodder', self.actor, name, 'battlefield')
                self.add('wrong_land', self.actor, 'Mountain', 'battlefield')
                self.cost_ref = 'fodder'

    @property
    def fee(self):
        printed = PRINTED[self.csvid][5]
        return cost_from_oracle({'mana_cost': printed}) if printed else ZERO.copy()

    def assert_choices(self, unused=0):
        self.emit('assert_unused_choices', value=unused)
        self.emit('assert_choices_consumed')

    def tap_by_attacking(self):
        self.emit('begin_attackers')
        self.emit('attack', 'source', value=1)
        self.emit('finish_attackers')
        self.emit('begin_blockers')
        self.emit('advance_combat')
        self.emit('combat_damage')
        self.settle()
        self.emit('end_combat')
        self.life[self.foe] -= stats(self.record)[0]
        self.observe('source', 'tapped', 1)
        self.receipts()

    def enter(self):
        self.paid('source')
        self.settle()
        self.cards['source']['zone'] = 'battlefield'
        self.observe('source', 'in_play', 1)
        if self.csvid != CYLIX:
            for prop, value in zip(('power', 'toughness'), stats(self.record)):
                self.observe('source', prop, value)
        self.receipts()
        if self.csvid in AGE_FOR_TAP:
            if self.mode != 'sick':
                self.go(self.foe)
                self.go(self.actor)
            self.observe('source', 'sick', int(self.mode == 'sick'))
        if self.mode == 'tapped':
            self.tap_by_attacking()
        else:
            self.observe('source', 'tapped', 0)

    def payment_choice(self):
        self.emit('choose_target', 'victim', player=self.actor)
        if self.csvid == HERBALIST and self.actor == 0:
            self.emit('choose_zone_card', player=self.actor, value=self.rows[self.cards[self.cost_ref]['name']]['csvid'])
        if self.csvid not in DISCARD and self.actor == 0:
            self.emit('choose_target', self.cost_ref, player=self.actor)

    def pay_card(self):
        self.cards[self.cost_ref]['zone'] = 'graveyard'
        self.random_consumed = self.mode == 'mixed_random'

    def receipts(self):
        start = len(self.steps)
        super().receipts()
        if self.random_consumed:
            pitched = {self.rows[name]['csvid'] for name in PITCHES}
            self.steps[start:] = [step for step in self.steps[start:]
                                  if not (step['op'] == 'assert_zone_multiplicity' and step['player'] == self.actor
                                          and step['zone'] in ('hand', 'graveyard') and step['index'] in pitched)]
            for csvid in sorted(pitched):
                self.emit('assert_hand_graveyard_total', player=self.actor, index=csvid, value=1)

    def proactive_shield(self):
        self.mana('set_mana', self.actor, self.fee)
        self.emit('refresh')
        self.emit('can_activate', 'source', value=1)
        self.payment_choice()
        self.emit('activate', 'source', value=1)
        self.emit('assert_stack', value=1)
        self.mana('assert_mana', self.actor, ZERO)
        self.settle()
        self.pay_card()
        self.observe('victim', 'tapped', 0)
        self.receipts()

    def destruction(self):
        proactive = self.mode == 'proactive'
        denied = self.mode in DENIED
        self.go(self.foe)
        if self.csvid in AGE_FOR_TAP:
            self.observe('source', 'sick', int(self.mode == 'sick'))
        if proactive:
            self.proactive_shield()
        self.paid('removal', target='victim')
        pool = ZERO.copy() if proactive else self.fee
        if self.mode == 'short':
            pool = [1, 0, 0, 0, 0, 0]
        if self.mode == 'from_forest':
            self.emit('activate', self.cost_ref, value=1)
            self.emit('assert_stack', value=1)
            self.observe(self.cost_ref, 'tapped', 1)
            self.mana('assert_mana', self.actor, pool)
        else:
            self.mana('set_mana', self.actor, pool)
        self.emit('refresh')
        if not proactive:
            self.emit('choose_regeneration', 'source')
            if not denied:
                self.payment_choice()
        self.emit('resolve')
        self.assert_choices(int(denied))
        self.emit('refresh')
        self.cards['removal']['zone'] = 'graveyard'
        if denied:
            self.cards['victim']['zone'] = 'graveyard'
            self.observe('victim', 'in_play', 0)
        else:
            if not proactive:
                self.pay_card()
            self.observe('victim', 'in_play', 1)
            self.observe('victim', 'tapped', 1)
            self.observe('victim', 'damage', 0)
            self.observe('victim', 'attacking', 0)
        self.mana('assert_mana', self.actor, pool if denied else ZERO)
        if self.csvid == CYLIX:
            self.emit('assert_discard_choices', value=0)
        if self.csvid != CHIRURGEON:
            self.observe('source', 'tapped', int(not denied or self.mode == 'tapped'))
        self.receipts()

    def build(self):
        self.emit('turn', player=self.actor)
        for seat in (0, 1):
            self.mana('set_mana', seat, ZERO)
        if self.csvid == CYLIX:
            # Counts an incorrect chosen discard without pinning the random result.
            self.emit('discard_policy', value=1)
        self.enter()
        self.destruction()


class Compound(Duel):
    def __init__(self, csvid, record, actor, mode, rows, oracle):
        self.csvid, self.record, self.actor, self.foe, self.mode = csvid, record, actor, 1 - actor, mode
        self.rows, self.oracle = rows, oracle
        self.setup, self.steps, self.cards, self.life = [], [], {}, [20, 20]
        self.active = actor
        self.random_consumed = False
        self.library = {actor: list(STOCK), self.foe: list(OTHER_STOCK)}
        self.add('source', actor, record['name'], 'hand')
        self.add_witnesses()
        for seat, cards in self.library.items():
            for index, name in enumerate(cards):
                self.add(f'library_{seat}_{index}', seat, name, 'library')

    def add_witnesses(self):
        if self.csvid == BARON:
            if self.mode in ('grow', 'after_turn'):
                self.add('victim', self.foe, SPIDER, 'battlefield')
                self.add('ground', self.foe, WALL, 'battlefield')
                self.add('removal', self.actor, KILL, 'hand')
            elif self.mode == 'self':
                self.add('removal', self.foe, KILL, 'hand')
            else:
                self.add('victim', self.foe if self.mode == 'foreign' else self.actor,
                         WALL if self.mode == 'wrong_type' else VAMPIRE, 'battlefield')
                self.add('removal', self.foe, KILL, 'hand')
        elif self.csvid == LICID:
            self.add('victim', self.foe if self.mode == 'foreign' else self.actor, WALL, 'battlefield')
            if self.mode in ('own', 'foreign', 'unattached'):
                self.add('removal', self.foe, KILL, 'hand')
        elif self.csvid == CUBE:
            self.add('victim', self.actor, HOST, 'battlefield')
            self.add('defender', self.foe, WALL, 'battlefield')
            if self.mode == 'unrelated':
                self.add('other', self.actor, HOST, 'battlefield')
            if self.mode not in ('proactive', 'target_scope'):
                self.add('removal', self.foe, KILL, 'hand')
            if self.mode == 'consumed':
                self.add('second_removal', self.foe, KILL, 'hand')
        else:
            self.add('victim', self.actor, WALL, 'battlefield')
            if self.mode == 'own_upkeep':
                self.add('observer', self.actor, CHISEI, 'hand')
                for index in range(4):
                    self.add(f'mana_{index}', self.actor, FOREST, 'battlefield')

    def paid_spell(self, ref, target=None, destination='graveyard'):
        self.paid(ref, target=target)
        self.settle()
        self.cards[ref]['zone'] = destination

    def age(self):
        self.go(self.foe)
        self.go(self.actor)

    def assert_stats(self, ref, power, toughness):
        self.observe(ref, 'power', power)
        self.observe(ref, 'toughness', toughness)

    def paid_ability(self, *, target=None):
        self.mana('set_mana', self.actor, self.fee)
        self.emit('refresh')
        self.emit('can_activate', 'source', value=1)
        if target:
            self.emit('choose_target', target, player=self.actor)
        self.emit('activate', 'source', value=1)
        self.emit('assert_stack', value=1)
        self.mana('assert_mana', self.actor, ZERO)
        self.settle()

    def removal(self, target, *, saved, source='removal', aimed=True, try_regeneration=True):
        self.paid(source, target=target)
        self.mana('set_mana', self.actor, self.fee)
        self.emit('refresh')
        if try_regeneration:
            self.emit('choose_regeneration', 'source')
            if saved and aimed:
                self.emit('choose_target', target, player=self.actor)
        self.emit('resolve')
        self.assert_choices(int(try_regeneration and not saved))
        self.emit('refresh')
        self.cards[source]['zone'] = 'graveyard'
        self.observe(target, 'in_play', int(saved))
        if saved:
            self.observe(target, 'damage', 0)
            self.observe(target, 'tapped', 1)
        else:
            self.cards[target]['zone'] = 'graveyard'
        self.mana('assert_mana', self.actor, ZERO if saved else self.fee)
        self.receipts()

    def start_combat(self, attacker, *, rejected=None, blocker=None):
        self.emit('begin_attackers')
        self.emit('attack', attacker, value=1)
        self.emit('finish_attackers')
        self.emit('begin_blockers')
        if rejected:
            self.emit('block', rejected, attacker, value=0)
        if blocker:
            self.emit('block', blocker, attacker, value=1)
        self.emit('advance_combat')
        self.observe(attacker, 'blocked', int(blocker is not None))
        self.emit('combat_damage')
        self.settle()
        self.emit('end_combat')

    def baron(self):
        self.age()
        if self.mode in ('grow', 'after_turn'):
            self.start_combat('source', rejected='ground', blocker='victim')
            self.observe('victim', 'damage', 5)
            self.assert_stats('source', 5, 5)
            if self.mode == 'after_turn':
                self.age()
                self.observe('victim', 'damage', 0)
            self.paid_spell('removal', target='victim')
            self.cards['victim']['zone'] = 'graveyard'
            expected = 7 if self.mode == 'grow' else 5
            self.assert_stats('source', expected, expected)
            self.receipts()
            if self.mode == 'grow':
                self.go(self.foe)
                self.assert_stats('source', 7, 7)
        elif self.mode == 'proactive':
            self.emit('can_activate', 'source', value=1)
            self.observe('source', 'tapped', 0)
        else:
            self.go(self.foe)
            target = 'source' if self.mode == 'self' else 'victim'
            allowed = self.mode in ('vampire', 'foreign')
            self.removal(target, saved=allowed)
            if target != 'source':
                self.observe('source', 'tapped', int(allowed))

    def licid(self):
        if self.mode != 'sick':
            self.age()
        self.emit('can_target_creature', 'victim', 'source', value=1)
        if self.mode in ('sick', 'short'):
            self.mana('set_mana', self.actor, self.fee if self.mode == 'sick' else [1, 0, 0, 0, 0, 0])
            self.emit('refresh')
            self.emit('can_activate', 'source', value=0)
            self.observe('source', 'tapped', 0)
            return
        if self.mode == 'unattached':
            self.go(self.foe)
            self.removal('victim', saved=False, try_regeneration=False)
            self.emit('can_target_creature', 'source', 'source', value=1)
            return
        self.paid_ability(target='victim')
        self.observe('source', 'tapped', 1)
        self.emit('can_target_creature', 'victim', 'source', value=0)
        if self.mode == 'revert':
            self.paid_ability()
            self.emit('can_target_creature', 'victim', 'source', value=1)
            self.assert_stats('source', 1, 1)
            self.observe('source', 'tapped', 1)
            self.receipts()
        else:
            self.go(self.foe)
            self.removal('victim', saved=True, aimed=False)
            self.observe('source', 'in_play', 1)
            self.emit('can_target_creature', 'victim', 'source', value=0)

    def cube(self):
        if self.mode == 'target_scope':
            self.mana('set_mana', self.actor, self.fee)
            self.emit('refresh')
            self.emit('probe_activation_target', 'source', 'victim', index=-1, value=1)
            self.emit('probe_activation_target', 'source', 'defender', index=-1, value=0)
            self.mana('assert_mana', self.actor, self.fee)
            return
        self.paid_ability(target='victim')
        self.observe('source', 'tapped', 1)
        blocked = self.mode == 'blocked'
        self.start_combat('other' if self.mode == 'unrelated' else 'victim', blocker='defender' if blocked else None)
        if blocked:
            self.observe('defender', 'damage', 1)
        if self.mode == 'unrelated':
            self.life[self.foe] -= 1
        self.receipts()
        if self.mode == 'proactive':
            self.mana('set_mana', self.actor, self.fee)
            self.emit('refresh')
            self.emit('can_activate', 'source', value=1)
        else:
            self.go(self.foe)
            self.removal('victim', saved=self.mode == 'consumed')
            if self.mode == 'consumed':
                # Use an independent live target for the second seat's
                # exhaustion check, even if its first response is unavailable.
                second_target = 'defender' if self.actor == 1 else 'victim'
                self.removal(second_target, saved=False, source='second_removal')

    def next_upkeep(self, seat):
        assert seat == 1 - self.active
        self.emit('advance_turn')
        self.emit('untap_step', player=seat)
        self.active = seat

    def matrix(self):
        if self.mode != 'own_upkeep':
            if self.mode == 'opponent_upkeep':
                self.next_upkeep(self.foe)
                self.emit('upkeep_step', player=self.foe)
            self.mana('set_mana', self.actor, self.fee)
            self.emit('refresh')
            self.emit('can_activate', 'source', value=0)
            self.observe('source', 'tapped', 0)
            self.mana('assert_mana', self.actor, self.fee)
            return
        self.go(self.foe)
        self.next_upkeep(self.actor)
        self.emit('upkeep_step', player=self.actor)
        self.settle()
        # The full upkeep driver suppresses manual stops. It still leaves the
        # engine in PHASE_UPKEEP, the same boundary used by draw_step.
        for index in range(4):
            land = f'mana_{index}'
            self.emit('activate', land, value=1)
            self.emit('assert_stack', value=0)
            self.observe(land, 'tapped', 1)
            self.mana('assert_mana', self.actor, [0, 0, 0, index + 1, 0, 0])
        self.emit('can_activate', 'source', value=1)
        if self.actor == 0:
            for _ in range(4):
                self.emit('choose_mana', player=self.actor, color=3)
        self.emit('choose_target', 'victim', player=self.actor)
        self.emit('activate', 'source', value=1)
        self.emit('assert_stack', value=1)
        self.observe('source', 'tapped', 1)
        self.mana('assert_mana', self.actor, ZERO)
        self.settle()
        self.emit('turn', player=self.actor)
        self.paid_spell('observer', destination='battlefield')
        self.go(self.foe)
        self.next_upkeep(self.actor)
        # Chisei can stay only by selecting and consuming the matrix counter.
        self.emit('choose_target', 'victim', player=self.actor)
        self.emit('choose_dialog', player=self.actor, value=0)
        self.emit('upkeep_step', player=self.actor)
        self.settle()
        self.observe('observer', 'in_play', 1)
        self.go(self.foe)
        self.next_upkeep(self.actor)
        self.emit('upkeep_step', player=self.actor)
        self.settle()
        self.cards['observer']['zone'] = 'graveyard'
        self.observe('observer', 'in_play', 0)
        self.observe('victim', 'in_play', 1)

    def build(self):
        self.emit('turn', player=self.actor)
        self.emit('present_ai_dialogs', value=1)
        for seat in (0, 1):
            self.mana('set_mana', seat, ZERO)
        self.paid_spell('source', destination='battlefield')
        self.observe('source', 'in_play', 1)
        if self.csvid in (BARON, LICID):
            self.assert_stats('source', *stats(self.record))
        {BARON: self.baron, LICID: self.licid, CUBE: self.cube, MATRIX: self.matrix}[self.csvid]()
        self.receipts()


@lru_cache(maxsize=1)
def profiles():
    return json.loads(PROFILE.read_bytes())


def classify(record, csvid):
    if (csvid not in PRINTED or not record or record.get('layout') != 'normal'
            or record.get('selected_face') or record.get('card_faces')):
        raise ValueError('whole normal regeneration cost record required')
    _, cost, type_line, printed_stats, keywords, activation, rule = PRINTED[csvid]
    if (record.get('type_line') != type_line or record.get('mana_cost') != cost
            or record.get('keywords') != list(keywords) or record.get('oracle_text') != rule
            or (printed_stats is not None and stats(record) != printed_stats)):
        raise ValueError('complete printed regeneration costs, types and riders required')
    return dict(csvid=csvid, mana=cost_from_oracle(record), activation=activation,
                stats=list(printed_stats) if printed_stats is not None else None)


def variants(shape):
    return [f'{mode}_{seat}' for mode, seat in scenarios(shape['csvid'])]


def require_variants(shape, selected):
    if (type(selected) is not list or not selected or any(type(mode) is not str for mode in selected)
            or len(selected) != len(set(selected)) or set(selected) - set(variants(shape))):
        raise ValueError('distinct reviewed regeneration cost variants required')


def spec(declaration, record):
    primary = profiles()['sources'].get(record.get('name', '')) if record else None
    if not primary or not exact(primary['oracle'], record):
        raise ValueError('exact pinned targeted regeneration cost record required')
    shape = classify(record, primary['registry']['csvid'])
    if (declaration.get('family') != FAMILY or type(declaration.get('version')) is not int
            or declaration['version'] != 2 or declaration.get('oracle_text') != record['oracle_text']
            or not exact(declaration.get('params'), shape)):
        raise ValueError('complete version 2 targeted regeneration cost declaration required')
    require_variants(shape, declaration.get('variants'))
    return dict(shape, family=FAMILY)


def validate_witnesses(oracle, rows):
    pins = profiles()
    if (set(pins) != {'sources', 'witnesses'} or set(pins['witnesses']) != WITNESSES
            or {pin['registry']['csvid'] for pin in pins['sources'].values()} != set(PRINTED)):
        raise ValueError('complete targeted regeneration cost source and witness pins required')
    for name, pin in (pins['sources'] | pins['witnesses']).items():
        row, record = rows.get(name, {}), oracle.get(name, {})
        if (not exact(record, pin['oracle']) or not exact(row, pin['registry'])
                or row.get('kind') != 'card' or row.get('handler') in (None, '0', 'NULL', 'card_dummy')
                or record.get('layout') != 'normal' or record.get('card_faces')):
            raise ValueError('pinned regeneration cost card changed: ' + name)
        if name in pins['sources']:
            classify(record, row['csvid'])


def expand(name, csvid, declaration, shape, selected, rows, oracle):
    require_variants(shape, selected)
    validate_witnesses(oracle, rows)
    if profiles()['sources'][name]['registry']['csvid'] != csvid or csvid != shape['csvid']:
        raise ValueError('exact registered targeted regeneration cost primary required')
    cases = []
    for variant in selected:
        mode, seat = variant.rsplit('_', 1)
        implementation = Compound if csvid in (BARON, LICID, CUBE, MATRIX) else Duel
        board = implementation(csvid, oracle[name], int(seat), mode, rows, oracle)
        board.build()
        cases.append(dict(name=f'{PREFIX}{csvid}_{variant}', card=name, fixture='empty_main', seed=2964,
                          setup=board.setup, steps=board.steps, _recipe=dict(
                              family=FAMILY, version=2, variant=variant, params=declaration['params'],
                              oracle_text=declaration['oracle_text'])))
    return cases

