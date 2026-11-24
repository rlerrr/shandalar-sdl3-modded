"""Check Level Up timing and printed combat riders in both earned level bands."""
import json
from collections import Counter
from pathlib import Path

from level_up_case_support import LevelUpBoard, classify_leveler
from library_search_recipes import wave6_exact


FAMILY = 'level_keyword_combat'
PROFILE = Path(__file__).parent / 'data/level_keyword_combat_wave8_profiles.json'
SHROUD = "Shroud (This creature can't be the target of spells or abilities.)"
UNBLOCKABLE = "This creature can't be blocked."
ISLANDWALK = "Islandwalk (This creature can't be blocked as long as defending player controls an Island.)"
GOBLIN = "Mons's Goblin Raiders"
REACH = 'Wall of Vines'
COMMON = ('thresholds', 'short_payment', 'offturn', 'pending_spell')
# Each combat case names the earned band, the actual blocker, and any land setup.
RULES = {
    ((), ('Trample',)): ('trample', {'base_block': (0, 'Grizzly Bears'), 'first_block': (1, 'Grizzly Bears'),
                                      'second_block': (2, 'Grizzly Bears')}, {'Trample'}),
    ((), ('First strike',)): ('first_strike', {'base_block': (0, GOBLIN), 'first_block': (1, GOBLIN),
                                              'second_block': (2, GOBLIN)}, {'First strike'}),
    ((UNBLOCKABLE,), (SHROUD, UNBLOCKABLE)): (
        'shroud', {'base_block': (0, GOBLIN), 'first_block': (1, GOBLIN), 'second_block': (2, GOBLIN)}, {'Shroud'}),
    ((), (ISLANDWALK,)): ('islandwalk', {'first_defense_island': (1, GOBLIN), 'second_defense_island': (2, GOBLIN),
                                        'second_own_island': (2, GOBLIN), 'second_no_island': (2, GOBLIN)},
                         {'Islandwalk', 'Landwalk'}),
    (('Vigilance',), ('Vigilance',)): ('vigilance', {'base_attack': (0, None), 'first_attack': (1, None),
                                                    'second_attack': (2, None)}, {'Vigilance'}),
    (('Flying',), ('Flying, vigilance',)): (
        'flight_vigilance', {'base_block': (0, GOBLIN), 'first_ground': (1, GOBLIN),
                             'second_ground': (2, GOBLIN), 'second_reach': (2, REACH)}, {'Flying', 'Vigilance'}),
    (('Flying',), ('Flying',)): ('flight', {'base_block': (0, GOBLIN), 'first_ground': (1, GOBLIN),
                                            'second_ground': (2, GOBLIN), 'second_reach': (2, REACH)}, {'Flying'}),
    (('First strike',), ('Double strike',)): (
        'strikes', {'base_block': (0, GOBLIN), 'first_block': (1, GOBLIN),
                    'second_attack': (2, None), 'second_block': (2, GOBLIN)}, {'First strike', 'Double strike'}),
}
BY_RIDER = {key: (combat, keywords) for key, combat, keywords in RULES.values()}
DUMMIES = {GOBLIN, 'Grizzly Bears', REACH}
WITNESSES = {GOBLIN, 'Grizzly Bears', REACH, 'Island', 'Tropical Island', 'Forest', 'Plains', 'Unsummon', 'Fog'}


def profiles():
    return json.loads(PROFILE.read_bytes())


def classify(record):
    model = classify_leveler(record)
    signature = (tuple(model['first_riders']), tuple(model['second_riders']))
    if signature not in RULES:
        raise ValueError('unreviewed printed Level Up combat riders')
    rider, _, keywords = RULES[signature]
    if set(record['keywords']) != {'Level Up', *keywords}:
        raise ValueError('full printed keyword set changed')
    return dict(model, rider=rider)


def variants(shape):
    combat, _ = BY_RIDER[shape['rider']]
    extra = ('target_gate',) if shape['rider'] == 'shroud' else ()
    return [f'{mode}_{seat}' for mode in (*COMMON, *combat, *extra) for seat in (0, 1)]


def spec(declaration, record):
    source = profiles()['sources'].get(record['name'])
    if not source or not wave6_exact(record, source['oracle']):
        raise ValueError('whole pinned Oracle Level Up creature required')
    shape = classify(record)
    chosen = declaration.get('variants')
    if (declaration.get('family') != FAMILY or type(declaration.get('version')) is not int
            or declaration['version'] != 2 or declaration.get('oracle_text') != record['oracle_text']
            or not wave6_exact(declaration.get('params'), shape) or not isinstance(chosen, list)
            or not chosen or len(set(chosen)) != len(chosen) or set(chosen) - set(variants(shape))):
        raise ValueError('expected exact whole Oracle and unique reviewed Level Up variants')
    return dict(shape, family=FAMILY)


def validate_witnesses(oracle, rows):
    data = profiles()
    if set(data) != {'sources', 'witnesses'} or set(data['witnesses']) != WITNESSES:
        raise ValueError('missing exact sources or combat witnesses')
    for kind, group in data.items():
        for name, pin in group.items():
            row, record = rows[name], oracle[name]
            if (pin['registry'] != row or not wave6_exact(pin['oracle'], record) or row['kind'] != 'card'
                    or row['handler'] in (None, 'NULL', '0') or row['handler'] == 'card_dummy' and name not in DUMMIES):
                raise ValueError('native or Oracle pin changed for ' + name)
            if kind == 'sources':
                classify(record)
            elif record['layout'] != 'normal':
                raise ValueError('independent ordinary witness required: ' + name)


class Board(LevelUpBoard):
    def build(self):
        self.life = [20, 20]
        for seat in (0, 1):
            for index, name in enumerate(('Forest', 'Plains')):
                self.add(name, f'library_{seat}_{index}', seat, 'library')
        combat, _ = BY_RIDER[self.shape['rider']]
        self.band, blocker = combat.get(self.mode, (0, None))
        if blocker is not None:
            self.add(blocker, 'blocker', self.foe)
        if self.mode in ('first_defense_island', 'second_defense_island', 'second_own_island'):
            name = 'Tropical Island' if self.mode == 'second_defense_island' and self.owner == 1 else 'Island'
            seat = self.owner if self.mode == 'second_own_island' else self.foe
            self.add(name, 'land', seat)
        if self.mode == 'target_gate':
            for seat in (0, 1):
                self.add('Unsummon', f'bounce_{seat}', seat, 'hand')
        if self.mode == 'pending_spell':
            self.add('Fog', 'fog', self.owner, 'hand')

    def receipts(self):
        self.emit('refresh')
        self.emit('assert_choices_consumed')
        self.emit('assert_stack', value=0)
        self.observe_mana()
        for who in (0, 1):
            self.emit('assert_life', player=who, value=self.life[who])
            for zone in ('hand', 'battlefield', 'graveyard', 'library', 'exile'):
                cards = Counter(self.rows[item['name']]['csvid'] for item in self.cards.values()
                                if item['player'] == who and item['zone'] == zone)
                self.emit('assert_zone', player=who, zone=zone, value=sum(cards.values()))
                if zone != 'library':
                    for csvid, count in sorted(cards.items()):
                        self.emit('assert_zone_multiplicity', player=who, zone=zone, index=csvid, value=count)
        for ref, card in self.cards.items():
            if card['zone'] != 'battlefield':
                continue
            self.emit('assert_card', ref=ref, property='tapped', value=int(card['tapped']))
            if 'Creature' in self.oracle[card['name']]['type_line']:
                self.emit('assert_card', ref=ref, property='damage', value=card['damage'])
        if self.cards['source']['zone'] == 'battlefield':
            self.observe_level()

    def reject_level(self, pool, *, attempt=False):
        self.fund(self.owner, pool)
        self.emit('require_can_activate', ref='source', value=0)
        if attempt:
            self.emit('activate', ref='source', value=0)
        self.observe_mana()
        self.observe_level()

    def check_timing_and_payment(self):
        if self.mode == 'short_payment':
            insufficient = list(self.shape['activation'])
            insufficient[next(index for index, mana in enumerate(insufficient) if mana)] -= 1
            self.reject_level(insufficient, attempt=True)
        elif self.mode == 'offturn':
            self.advance_turn()
            self.emit('turn', player=self.foe, value=0)
            self.reject_level(self.shape['activation'])
        else:
            self.cast('fog')
            self.reject_level(self.shape['activation'])
            self.emit('assert_stack', value=1)
            self.emit('resolve')
            self.cards['fog']['zone'] = 'graveyard'
            self.emit('require_can_activate', ref='source', value=1)
        self.receipts()

    def check_shroud(self):
        for band, expected in [('first', 1), ('second', 0)]:
            self.level_to(self.shape[band][0])
            for seat in (0, 1):
                self.fund(seat, [0, 0, 1, 0, 0, 0])
                self.emit('can_cast', ref=f'bounce_{seat}', value=expected)
                self.observe_mana()
                self.fund(seat, [0]*6)
        self.receipts()

    def attack_ready(self):
        for seat in (self.foe, self.owner):
            self.advance_turn()
            self.emit('untap_step', player=seat)
        self.emit('turn', player=self.owner, value=0)
        self.emit('assert_card', ref='source', property='sick', value=0)

    def combat(self):
        if self.band:
            self.level_to(self.shape['first' if self.band == 1 else 'second'][0])
        self.attack_ready()
        source = self.cards['source']
        power, toughness = self.source_stats()
        riders = [] if not self.band else self.shape['first_riders' if self.band == 1 else 'second_riders']
        keywords = {keyword for line in riders for keyword in line.split(', ')}
        vigilance = bool(keywords & {'Vigilance', 'vigilance'})
        self.emit('begin_attackers')
        self.emit('attack', ref='source', value=1)
        source['tapped'] = not vigilance
        self.emit('assert_card', ref='source', property='tapped', value=int(not vigilance))
        self.emit('finish_attackers')
        self.emit('begin_blockers')
        blocker = self.cards.get('blocker')
        record = self.oracle[blocker['name']] if blocker else None
        blocked = blocker is not None and (
            UNBLOCKABLE not in riders and (ISLANDWALK not in riders or self.mode != 'second_defense_island')
            and ('Flying' not in keywords or 'Reach' in record['keywords']))
        if blocker:
            self.emit('block', ref='blocker', other='source', value=int(blocked))
        self.emit('advance_combat')
        self.emit('assert_card', ref='source', property='blocked', value=int(blocked))
        if blocker:
            self.emit('assert_card', ref='blocker', property='blocking_count', value=int(blocked))
        self.emit('combat_damage')
        double = 'Double strike' in keywords
        first = double or 'First strike' in keywords
        if blocked:
            block_power, block_toughness = int(record['power']), int(record['toughness'])
            excess = max(0, power - block_toughness) if 'Trample' in keywords else 0
            dealt = power - excess
            self.life[self.foe] -= excess
            blocker['damage'] = dealt if not double or dealt >= block_toughness else dealt * 2
            source['damage'] = 0 if first and dealt >= block_toughness else block_power
            if blocker['damage'] >= block_toughness:
                blocker['zone'] = 'graveyard'
            if source['damage'] >= toughness:
                source['zone'] = 'graveyard'
        else:
            self.life[self.foe] -= power * (2 if double else 1)
        self.emit('resolve')
        self.emit('end_combat')
        self.receipts()

    def run(self):
        self.play_source()
        if self.mode == 'thresholds':
            self.level_to(self.shape['second'][0] + 1)
            self.receipts()
        elif self.mode in COMMON:
            self.check_timing_and_payment()
        elif self.mode == 'target_gate':
            self.check_shroud()
        else:
            self.combat()


def build(name, csvid, declaration, shape, variant, rows, oracle):
    mode, seat = variant.rsplit('_', 1)
    board = Board(name, shape, mode, int(seat), rows, oracle)
    board.run()
    return dict(name=f'LevelKeywordCombatRecipe_{csvid}_{variant}', card=name, fixture='empty_main',
                seed=123, setup=board.setup, steps=board.steps,
                _recipe=dict(family=FAMILY, version=2, variant=variant, params=declaration['params'],
                             oracle_text=declaration['oracle_text'],
                             untested_components=['Responses to Level Up, outside modifiers, and multiplayer.']))


def expand(name, csvid, declaration, shape, selected, rows, oracle):
    if rows[name]['csvid'] != csvid or not selected or len(set(selected)) != len(selected) or set(selected) - set(variants(shape)):
        raise ValueError('only distinct reviewed cases for the exact Level Up creature')
    validate_witnesses(oracle, rows)
    return [build(name, csvid, declaration, shape, variant, rows, oracle) for variant in selected]
