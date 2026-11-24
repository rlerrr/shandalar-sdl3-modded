"""Actual upkeep life choices, tap/sacrifice, and Black Dawn's printed tutor."""

import copy
import json
import re
from functools import lru_cache
from pathlib import Path

from attack_value_trigger_recipes import _AttackResourceWorld

FAMILY = 'upkeep_life_tap'
PROFILE = Path(__file__).with_name('data') / 'upkeep_life_tap_wave8_profiles.json'
TAP = re.compile(r'At the beginning of your upkeep, tap this creature unless you pay ([1-9]\d*) life\.')
VILE = 'All creatures have "At the beginning of your upkeep, sacrifice this creature unless you pay 1 life."'
BRINGER = ("You may pay {W}{U}{B}{R}{G} rather than pay this spell's mana cost.\nTrample\n"
           'At the beginning of your upkeep, you may pay 2 life. If you do, search your library for a card, then shuffle and put that card on top.')
HANDLERS = {'tap': {'manalinkeh_card_carnophage', 'manalinkeh_card_sangrophage'},
            'vile': {'manalinkeh_card_vile_consumption'}, 'bringer': {'manalinkeh_card_bringer_of_the_black_dawn'}}
BEARS, SHIELD, AURA, BLOCKER = 'Grizzly Bears', 'Platinum Emperion', 'Claustrophobia', "Mons's Goblin Raiders"
LIBRARY = ('Island', 'Mountain', 'Plains')
RAINBOW = [0, 1, 1, 1, 1, 1]
WITNESSES = {BEARS, SHIELD, AURA, BLOCKER, 'Counterspell', *LIBRARY}
COMMON = ('pay', 'decline', 'opponent_upkeep', 'countered', 'wrong_mana', 'short', 'snapshot')
EXTRA = {'tap': ('attack_paid', 'attack_declined', 'already_tapped', 'locked_life', 'exact_life', 'repeat'),
         'vile': ('mixed', 'paid_attack', 'locked_life', 'exact_life', 'no_creature'),
         'bringer': ('rainbow', 'choose_rainbow', 'choose_normal', 'trample', 'search_policy', 'repeat')}
GAPS = ('The upkeep driver resolves the actual window to completion, without an independently pending response boundary.',
        'No fixed order is prescribed for independently triggered identical Vile creatures.')


@lru_cache(maxsize=1)
def profiles():
    return json.loads(PROFILE.read_text(encoding='utf8'))


def classify(record):
    from recipes import cost_from_oracle

    if not record or record.get('layout') != 'normal' or record.get('card_faces') or record.get('selected_face'):
        raise ValueError('complete normal upkeep life-payment card required')
    text, type_line, keywords = (record[key] for key in ('oracle_text', 'type_line', 'keywords'))
    if (match := TAP.fullmatch(text)) and type_line.startswith('Creature — ') and keywords == []:
        mode, life = 'tap', int(match[1])
    elif text == VILE and type_line == 'Enchantment' and keywords == []:
        mode, life = 'vile', 1
    elif text == BRINGER and type_line.startswith('Creature — ') and keywords == ['Trample']:
        mode, life = 'bringer', 2
    else:
        raise ValueError('reviewed complete upkeep life-payment clause required')
    return dict(mode=mode, life=life, mana=cost_from_oracle(record), keywords=keywords)


def variants(params):
    mode = params['mode']
    tests = COMMON + EXTRA[mode]
    if mode == 'tap' and params['life'] > 1:
        tests += ('unaffordable',)
    return [f'{test}_{controller}' for controller in (0, 1) for test in tests]


def check_variants(params, selected):
    if (type(selected) is not list or not selected or any(type(test) is not str for test in selected)
            or len(set(selected)) != len(selected) or set(selected) - set(variants(params))):
        raise ValueError('distinct reviewed upkeep life variants required')


def spec(declaration, record):
    params = classify(record)
    if (declaration.get('family') != FAMILY or type(declaration.get('version')) is not int or declaration['version'] != 2
            or profiles()['sources'].get(record['name'], {}).get('oracle') != record
            or declaration.get('oracle_text') != record['oracle_text']
            or json.dumps(declaration.get('params'), sort_keys=True) != json.dumps(params, sort_keys=True)):
        raise ValueError('pinned full upkeep life Oracle and exact typed declaration required')
    check_variants(params, declaration.get('variants'))
    return dict(params, family=FAMILY)


def validate_witnesses(oracle, rows):
    data = profiles()
    skipped = {row['csvid'] for row in json.loads((PROFILE.parent / 'unimplemented.json').read_text(
        encoding='utf8'))['cases'].values()}
    if data.get('version') != 1 or set(data['witnesses']) != WITNESSES:
        raise ValueError('reviewed exact upkeep-life witnesses required')
    for group in ('sources', 'witnesses'):
        for name, pin in data[group].items():
            row, record = rows.get(name, {}), oracle.get(name, {})
            if (pin != dict(oracle=record, registry=row) or row.get('kind') != 'card' or record.get('layout') != 'normal'
                    or row['csvid'] in skipped or row['handler'] in (None, 'NULL', '0')
                    or row['handler'] == 'card_dummy' and name not in (BEARS, BLOCKER)):
                raise ValueError(f'unreviewed upkeep-life binding: {name}')
            if group == 'sources' and row['handler'] not in HANDLERS[classify(record)['mode']]:
                raise ValueError(f'unreviewed upkeep-life handler: {name}')


class UpkeepWorld(_AttackResourceWorld):
    def __init__(self, name, params, test, owner, rows, oracle):
        super().__init__(name, dict(params, effect=FAMILY), owner, rows, oracle)
        self.test = test
        self.foe = 1 - owner
        self.mode = params['mode']
        self.kept_tapped = False
        self.shuffle_count = 0
        self.mixed_bears = False

    def observe(self, ref, prop, value):
        self.emit('assert_card', ref=ref, property=prop, value=int(value))

    def receipts(self):
        self.emit('refresh')
        start = len(self.steps)
        super().receipts()
        if self.mixed_bears or self.shuffle_count:
            stable_receipts = []
            for receipt in self.steps[start:]:
                copy_dependent = self.mixed_bears and receipt['op'] == 'assert_card' and receipt['ref'] in ('ally', 'copy')
                shuffle_dependent = (self.shuffle_count and receipt['op'] == 'assert_zone_card' and receipt['zone'] == 'library'
                                     and receipt['player'] == self.owner and receipt['index'] != 0)
                if not copy_dependent and not shuffle_dependent:
                    stable_receipts.append(receipt)
            self.steps[start:] = stable_receipts
        if self.mode == 'bringer':
            for player in (0, 1):
                self.emit('assert_shuffle_count', player=player, value=self.shuffle_count if player == self.owner else 0)
            if self.shuffle_count:
                for name in LIBRARY:
                    self.emit('assert_zone_multiplicity', player=self.owner, zone='library', index=self.rows[name]['csvid'], value=1)

    def configure(self):
        mode, test = self.mode, self.test
        self.add(self.name, 'source', self.owner, 'hand', source=True)
        if test == 'countered':
            self.add('Counterspell', 'counter', self.foe, 'hand')
        if test == 'already_tapped':
            self.add(AURA, 'aura', self.foe, 'hand')
        if mode == 'vile' and test != 'no_creature':
            self.add(SHIELD if test == 'locked_life' else BEARS, 'ally', self.foe if test == 'opponent_upkeep' else self.owner)
        if mode == 'tap' and test == 'locked_life':
            self.add(SHIELD, 'shield', self.owner)
        if test == 'mixed':
            self.add(BEARS, 'copy', self.owner)
        if test == 'trample':
            self.add(BLOCKER, 'blocker', self.foe)
        for player in (0, 1):
            for index, name in enumerate(LIBRARY):
                self.add(name, f'library_{player}_{index}', player, 'library')
        life = self.p['life']
        if test in ('exact_life', 'unaffordable'):
            self.life[self.owner] = life if test == 'exact_life' else life - 1
            self.emit('set_life', player=self.owner, value=self.life[self.owner])
        if mode == 'bringer' and test == 'decline' and self.owner == 1:
            # AI makes its actual optional decision once spending 2 would
            # leave at most five life. No scripted human-only priority pass.
            self.life[self.owner] = 6
            self.emit('set_life', player=self.owner, value=6)

    def cast(self):
        mode, test = self.mode, self.test
        base = self.p['mana']
        alternate = test in ('rainbow', 'choose_rainbow')
        cost = RAINBOW if alternate else base
        pool = cost.copy()
        denied = test in ('wrong_mana', 'short')
        if test in ('choose_rainbow', 'choose_normal'):
            pool = [7, 2, 1, 1, 1, 1]
        elif test == 'wrong_mana':
            pool = [0, 0, 0, 0, sum(base), 0]
        elif test == 'short':
            pool = [0] * 6
        self.pool(self.owner, pool)
        self.emit('refresh')
        if test in ('choose_rainbow', 'choose_normal'):
            self.emit('choose_dialog', player=self.owner, value=int(not alternate))
        self.emit('cast', ref='source', value=int(not denied))
        self.emit('assert_stack', value=int(not denied))
        self.emit('assert_unused_choices', value=0)
        self.observe('source', 'in_hand', denied)
        if denied:
            self.receipts()
            return
        self.mana[self.owner] = [have - spend for have, spend in zip(pool, cost)]
        if test == 'countered':
            from recipes import cost_from_oracle

            self.emit('announce_spell', ref='source')
            self.pool(self.foe, cost_from_oracle(self.o['Counterspell']))
            self.emit('cast_response', ref='counter', value=1)
            self.emit('assert_stack', value=2)
            self.emit('resolve_response')
            self.zones['hand'][self.foe].remove('Counterspell')
            self.zones['graveyard'][self.foe].append('Counterspell')
            self.mana[self.foe] = [0] * 6
            self.move_source('graveyard')
        else:
            self.emit('resolve')
            if mode == 'vile':
                self.move_source('battlefield')
            else:
                self.arrive('source')
        self.receipts()

    def move_source(self, destination):
        self.zones['hand'][self.owner].remove(self.name)
        self.zones[destination][self.owner].append(self.name)
        if 'source' in self.entities:
            self.entities['source']['zone'] = destination

    def attach_aura(self):
        self.spell(AURA, 'aura', self.foe, 'source')
        self.zones['graveyard'][self.foe].remove(AURA)
        self.zones['battlefield'][self.foe].append(AURA)
        self.entities['source']['tapped'] = True
        self.kept_tapped = True
        self.receipts()

    def setup_next_upkeep(self):
        if any(any(pool) for pool in self.mana):
            raise ValueError('upkeep transition requires empty pools')
        self.emit('advance_turn')
        self.expire()
        self.current = 1 - self.current
        self.emit('untap_step', player=self.current)
        for ref, subject in self.entities.items():
            if subject['player'] == self.current and subject['zone'] == 'battlefield':
                subject.update(tapped=self.kept_tapped and ref == 'source', sick=False)
        self.receipts()

    def life_choice(self, choice, locked=False):
        cost = self.p['life']
        if locked or self.life[self.current] < cost:
            return False
        self.emit('choose_dialog', player=self.current, value=int(choice == 'decline'))
        if choice == 'pay':
            self.life[self.current] -= cost
            return True
        return False

    def settle_upkeep(self, choice='pay'):
        player = self.current
        if self.name in self.zones['battlefield'][self.owner]:
            if self.mode == 'vile':
                self.vile_upkeep(choice)
            elif player == self.owner:
                if self.mode == 'tap':
                    if not self.life_choice(choice, locked=self.test == 'locked_life'):
                        self.entities['source']['tapped'] = True
                elif choice == 'pay':
                    self.bringer_upkeep()
        self.emit('upkeep_step', player=player)
        self.emit('assert_unused_choices', value=0)
        if self.mode == 'bringer' and self.test == 'search_policy' and player == self.owner:
            self.emit('assert_forced_zone_choices', value=1)
        self.receipts()
        self.emit('turn', player=player, value=0)

    def vile_upkeep(self, choice):
        living = [ref for ref, entity in self.entities.items()
                  if entity['player'] == self.current and entity['zone'] == 'battlefield']
        mixed = self.test == 'mixed' and len(living) == 2
        for index, ref in enumerate(living):
            option = 'decline' if mixed and index else choice
            if not self.life_choice(option, locked=self.test == 'locked_life'):
                self.leave(ref, 'graveyard')
        self.mixed_bears = mixed

    def bringer_upkeep(self):
        if self.owner == 0:
            self.emit('choose_trigger_source', ref='source', player=0)
        selected = 'Mountain' if self.shuffle_count else 'Plains'
        choice = 'choose_optional_zone' if self.test == 'search_policy' else 'choose_zone_card'
        self.emit(choice, player=self.owner, value=self.rows[selected]['csvid'])
        self.life[self.owner] -= self.p['life']
        library = self.zones['library'][self.owner]
        library.remove(selected)
        library.insert(0, selected)
        self.shuffle_count += 1

    def fight(self, ref, attack):
        creature = self.entities[ref]
        blocked = self.test == 'trample'
        self.emit('begin_attackers')
        self.emit('attack', ref=ref, value=int(attack))
        self.emit('finish_attackers')
        self.emit('begin_blockers')
        if blocked:
            self.emit('block', ref='blocker', other=ref, value=1)
            self.observe('blocker', 'blocking_count', True)
        self.emit('advance_combat')
        self.emit('combat_damage')
        self.emit('resolve')
        self.emit('end_combat')
        if attack:
            creature['tapped'] = True
            if blocked:
                blocker = self.entities['blocker']
                self.life[self.foe] -= creature['power'] - blocker['toughness']
                creature['damage'] += blocker['power']
                self.leave('blocker', 'graveyard')
            else:
                self.life[self.foe] -= creature['power']
        self.receipts()


def scenario(world):
    mode, test = world.mode, world.test
    world.emit('turn', player=world.owner, value=0)
    world.configure()
    world.emit('present_ai_dialogs', value=1)
    for player in (0, 1):
        world.pool(player, [0] * 6)
    world.receipts()
    world.cast()
    if test in ('short', 'wrong_mana', 'choose_rainbow', 'choose_normal'):
        return None
    world.setup_next_upkeep()
    world.settle_upkeep()
    if test == 'opponent_upkeep':
        return None
    if test == 'already_tapped':
        world.attach_aura()
    world.setup_next_upkeep()
    cut = len(world.steps) if test == 'snapshot' else None
    world.settle_upkeep('decline' if test in ('decline', 'attack_declined') else 'pay')
    if test == 'repeat':
        world.setup_next_upkeep()
        world.settle_upkeep()
        world.setup_next_upkeep()
        world.settle_upkeep('pay' if mode == 'bringer' else 'decline')
    if test in ('attack_paid', 'attack_declined', 'trample', 'paid_attack'):
        world.fight('ally' if mode == 'vile' else 'source', test != 'attack_declined')
    return cut


def expand(name, csvid, declaration, params, selected, rows, oracle):
    from recipes import step

    check_variants(params, selected)
    validate_witnesses(oracle, rows)
    if profiles()['sources'][name]['registry']['csvid'] != csvid:
        raise ValueError('exact upkeep life source CSVID required')
    cases = []
    for variant in selected:
        test, owner = variant.rsplit('_', 1)
        world = UpkeepWorld(name, params, test, int(owner), rows, oracle)
        cut = scenario(world)
        steps = world.steps
        if cut is not None:
            tail = steps[cut:]
            steps = (steps[:cut] + [step('capture')] + tail + [step('record_state'), step('ai_roundtrip'), step('restore')]
                     + copy.deepcopy(tail) + [step('assert_recorded_state')])
        cases.append(dict(name=f'UpkeepLifeTapRecipe_{csvid}_{variant}', card=name, fixture='empty_main', seed=163,
                          setup=world.setup, steps=steps, _recipe=dict(family=FAMILY, version=2, variant=variant,
                              params=declaration['params'], oracle_text=declaration['oracle_text'],
                              untested_components=list(GAPS))))
    return cases
