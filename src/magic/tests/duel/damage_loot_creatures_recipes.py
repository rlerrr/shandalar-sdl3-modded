"""Whole-card draw-then-discard effects from actual combat and attributed damage."""

import copy
import json
from functools import lru_cache
from pathlib import Path

from attack_value_trigger_recipes import _AttackResourceWorld
from morph_creature_recipes import FACE_DOWN, MORPH

FAMILY = 'damage_loot_creatures'
PROFILE = Path(__file__).with_name('data') / 'damage_loot_creatures_wave8_profiles.json'
ANY_MANDATORY = 'Whenever this creature deals damage to an opponent, draw a card, then discard a card.'
ANY_OPTIONAL = 'Whenever this creature deals damage to an opponent, you may draw a card. If you do, discard a card.'
COMBAT_OPTIONAL = 'Whenever this creature deals combat damage to a player, you may draw a card. If you do, discard a card.'
SHADOW = 'Shadow (This creature can block or be blocked by only creatures with shadow.)'
SKULK = "Skulk (This creature can't be blocked by creatures with greater power.)"
GREEN = "This creature can't be blocked by green creatures."
MARE_PUMP = '{1}{U}: This creature gets +2/-2 until end of turn.'
WHARF_PAY = ('Whenever you discard a creature card, you may pay {2}. If you do, '
             'create a 3/2 colorless Eldrazi Horror creature token.')
PATTERNS = {
    (SHADOW, ANY_MANDATORY): ('shadow', [], ['Shadow'], 'manalinkeh_card_looter_il_kor'),
    (GREEN, ANY_OPTIONAL, MARE_PUMP): ('mare', [], [], 'manalinkeh_card_surge_mare'),
    ('Flying', COMBAT_OPTIONAL): ('morph', ['Morph'], ['Flying'], 'manalinkeh_card_abomination_of_gudul'),
    (SKULK, COMBAT_OPTIONAL, WHARF_PAY): ('wharf', [], ['Skulk'], 'manalinkeh_card_wharf_infiltrator'),
}
WALL, GREEN_BLOCKER, SHADOW_BLOCKER = 'Fortified Rampart', 'Grizzly Bears', 'Dauthi Marauder'
REACH, FLYER = 'Giant Spider', 'Wind Drake'
TOKEN = 'Eldrazi Horror'
WITNESSES = {WALL, GREEN_BLOCKER, SHADOW_BLOCKER, REACH, FLYER, 'Forest', 'Mountain', 'Island',
             'Plains', 'Hermetic Study', 'Fog', 'Merfolk Looter'}
DUMMIES = {WALL, GREEN_BLOCKER, REACH, FLYER}
COMMON = ('connect', 'blocked', 'evasion', 'prevented', 'noncombat', 'self_damage', 'source_blocks',
          'snapshot', 'cast_short')
SPECIAL = {
    'shadow': (),
    'mare': ('zero_power', 'pump_duration', 'pump_short', 'pump_lethal'),
    'morph': ('hidden_hit', 'hidden_block', 'morph_hit', 'morph_short', 'morph_offturn'),
    'wharf': ('wharf_funded', 'wharf_unfunded', 'external_discard'),
}
GAPS = ('Individual pending-damage-trigger response window and multiplayer recipients.',
        'An empty-library mandatory draw would require observing the game result after state-based actions.')


@lru_cache(maxsize=1)
def profiles():
    return json.loads(PROFILE.read_text(encoding='utf8'))


def classify(record):
    from recipes import cost_from_oracle

    if (not record or record.get('layout') != 'normal' or record.get('selected_face') or record.get('card_faces')
            or not record.get('type_line', '').startswith('Creature — ')):
        raise ValueError('whole normal creature loot clause required')
    lines = record['oracle_text'].splitlines()
    morphs = [m for line in lines if (m := MORPH.fullmatch(line))]
    remaining = tuple(line for line in lines if not MORPH.fullmatch(line))
    pattern = PATTERNS.get(remaining)
    if pattern is None:
        raise ValueError('whole supported draw-discard and companion clauses required')
    mode, cast_keywords, keywords, _ = pattern
    if len(morphs) != int(mode == 'morph') or set(record['keywords']) != set(cast_keywords + keywords):
        raise ValueError('printed evasion and Morph must be fully modeled')
    flips = cost_from_oracle(dict(record, mana_cost=morphs[0][1])) if morphs else []
    stats = [record.get(key) for key in ('power', 'toughness')]
    if any(not isinstance(n, str) or not n.isascii() or not n.isdecimal() for n in stats):
        raise ValueError('fixed creature power and toughness required')
    return dict(mode=mode, keywords=keywords, mana=cost_from_oracle(record), morph=flips,
                power=int(stats[0]), toughness=int(stats[1]), optional=mode != 'shadow',
                combat_only=mode in ('morph', 'wharf'))


def variants(params):
    mode = params['mode']
    result = [f'{test}_{owner}' for owner in (0, 1) for test in COMMON + SPECIAL[mode]]
    result.extend(('discard_drawn_0', 'discard_old_0'))
    if params['optional']:
        result.extend(('decline_0', 'decline_empty_0'))
    if mode == 'wharf':
        result.extend(('wharf_payment_cancel_0', 'wharf_noncreature_0', 'wharf_noncreature_1'))
    return result


def check_variants(params, selected):
    if (type(selected) is not list or not selected or any(type(v) is not str for v in selected)
            or len(set(selected)) != len(selected) or not set(selected) <= set(variants(params))):
        raise ValueError('distinct reviewed damage-loot variants required')


def spec(declaration, record):
    params = classify(record)
    pin = profiles()['sources'].get(record['name'])
    if (declaration.get('family') != FAMILY or type(declaration.get('version')) is not int or declaration['version'] != 2
            or not pin or pin['oracle'] != record or declaration.get('oracle_text') != record['oracle_text']
            or json.dumps(params, sort_keys=True) != json.dumps(declaration.get('params'), sort_keys=True)):
        raise ValueError('exact typed complete Oracle damage-loot declaration required')
    check_variants(params, declaration.get('variants'))
    return dict(params, family=FAMILY)


def validate_witnesses(oracle, rows):
    pins = profiles()
    skipped = {entry['csvid'] for entry in json.loads((PROFILE.parent / 'unimplemented.json').read_text(
        encoding='utf8'))['cases'].values()}
    if pins.get('version') != 1 or set(pins['witnesses']) != WITNESSES or pins['hidden'] != rows.get('Face-Down Creature'):
        raise ValueError('reviewed damage-loot witness set or hidden observer changed')
    if pins['hidden']['csvid'] != FACE_DOWN or pins['token'] != dict(oracle=oracle.get(TOKEN), registry=rows.get(TOKEN)):
        raise ValueError('independent Morph and native Eldrazi Horror observers required')
    token = pins['token']['oracle']
    if (token['layout'], token['type_line'], token['power'], token['toughness'], token['colors'], token['oracle_text']) != (
            'token', 'Token Creature — Eldrazi Horror', '3', '2', [], ''):
        raise ValueError('complete printed Eldrazi Horror characteristics required')
    for group in ('sources', 'witnesses'):
        for name, pin in pins[group].items():
            row, record = rows.get(name), oracle.get(name)
            if (pin != dict(oracle=record, registry=row) or row.get('kind') != 'card' or record.get('layout') != 'normal'
                    or row['csvid'] in skipped or row['handler'] in ('NULL', None, '0')
                    or (row['handler'] == 'card_dummy' and name not in DUMMIES)):
                raise ValueError(f'unreviewed damage-loot binding: {name}')
            if group == 'sources':
                classify(record)
                lines = tuple(line for line in record['oracle_text'].splitlines() if not MORPH.fullmatch(line))
                if row['handler'] != PATTERNS[lines][3]:
                    raise ValueError(f'unreviewed damage-loot callback: {name}')


class LootWorld(_AttackResourceWorld):
    def __init__(self, name, params, scenario, owner, rows, oracle):
        super().__init__(name, dict(params, effect=FAMILY, entry_energy=0), owner, rows, oracle)
        self.foe = 1 - owner
        self.scenario = scenario
        self.hidden = False

    def observe(self, ref, prop, value):
        self.emit('assert_card', ref=ref, property=prop, value=int(value))

    def receipts(self):
        self.emit('refresh')
        start = len(self.steps)
        super().receipts()
        if self.hidden and self.entities['source']['zone'] == 'battlefield':
            printed = self.rows[self.name]['csvid']
            for action in self.steps[start:]:
                if (action['op'] == 'assert_zone_multiplicity' and action.get('zone') == 'battlefield'
                        and action['player'] == self.owner and action['index'] == printed):
                    action['value'] = 0
            self.emit('assert_zone_multiplicity', player=self.owner, zone='battlefield', index=FACE_DOWN, value=1)
            self.observe('source', 'csvid', FACE_DOWN)
        for player in (0, 1):
            count = self.zones['battlefield'][player].count(TOKEN)
            if self.p['mode'] != 'wharf':
                continue
            index = self.rows[TOKEN]['csvid']
            self.emit('assert_zone_multiplicity', player=player, zone='battlefield', index=index, value=count)
            if count:
                for prop, value in (('power', 3), ('toughness', 2), ('color', 0), ('artifact', 0), ('token', 1),
                                    ('subtype_Eldrazi', 1), ('subtype_Horror', 1), ('tapped', 0)):
                    self.emit('assert_matching_permanents', player=player, property=prop, index=index, value=value)

    def setup_board(self):
        scenario, mode = self.scenario, self.p['mode']
        self.add(self.name, 'source', self.owner, 'hand', source=True)
        if scenario in ('discard_old', 'discard_drawn', 'decline_empty'):
            self.add('Forest', 'old_hand', self.owner, 'hand')
        self.add('Forest', 'foe_hand', self.foe, 'hand')
        self.add('Island', 'foe_top', self.foe, 'library')
        if scenario != 'decline_empty':
            creature = scenario in ('wharf_funded', 'wharf_unfunded', 'external_discard', 'wharf_payment_cancel')
            for i, name in enumerate((GREEN_BLOCKER if creature else 'Island', 'Mountain', 'Plains')):
                self.add(name, f'library_{i}', self.owner, 'library')
        if scenario in ('noncombat', 'self_damage'):
            self.add('Hermetic Study', 'grant', self.owner, 'hand')
        if scenario == 'prevented':
            self.add('Fog', 'shield', self.foe, 'hand')
        if scenario == 'external_discard':
            self.add('Merfolk Looter', 'external', self.owner)
        if scenario in ('blocked', 'hidden_block'):
            name = {'shadow': SHADOW_BLOCKER, 'morph': REACH}.get(mode, WALL) if scenario == 'blocked' else WALL
            self.add(name, 'blocker', self.foe)
        if scenario == 'evasion':
            self.add(GREEN_BLOCKER if mode in ('wharf', 'mare') else WALL, 'blocker', self.foe)
        if scenario == 'source_blocks':
            self.add(dict(shadow=SHADOW_BLOCKER, morph=FLYER).get(mode, GREEN_BLOCKER), 'opposing', self.foe)

    def enter(self):
        scenario = self.scenario
        hidden = scenario in ('hidden_hit', 'hidden_block', 'morph_hit', 'morph_short', 'morph_offturn')
        if self.p['mode'] != 'morph' or scenario == 'cast_short':
            self.cast_source('source', denied='insufficient_mana' if scenario == 'cast_short' else None)
            return
        cost = [3, 0, 0, 0, 0, 0] if hidden else self.p['mana']
        self.pool(self.owner, cost)
        self.emit('refresh')
        if not hidden:
            self.emit('choose_dialog', player=self.owner, value=1)
        self.emit('cast', ref='source', value=1)
        self.emit('assert_stack', value=1)
        self.emit('resolve')
        self.arrive('source')
        self.mana[self.owner] = [0] * 6
        self.hidden = hidden
        if hidden:
            self.entities['source'].update(power=2, toughness=2, keys=set())
        self.receipts()

    def reveal(self, allowed):
        cost = list(self.p['morph'])
        if not allowed:
            cost[3] -= 1  # Printed green requirement remains unpayable.
        self.pool(self.owner, cost)
        self.emit('refresh')
        self.emit('can_activate', ref='source', value=int(allowed))
        self.emit('activate', ref='source', value=int(allowed))
        if allowed:
            self.hidden = False
            self.entities['source'].update(power=self.p['power'], toughness=self.p['toughness'], keys={'Flying'})
            self.mana[self.owner] = [0] * 6
            self.emit('resolve')
            self.observe('source', 'csvid', self.rows[self.name]['csvid'])
        self.receipts()

    def pump(self, count, allowed=True):
        for _ in range(count):
            self.pool(self.owner, [1, 0, 1, 0, 0, 0] if allowed else [2, 0, 0, 0, 0, 0])
            self.emit('refresh')
            self.emit('can_activate', ref='source', value=int(allowed))
            self.emit('activate', ref='source', value=int(allowed))
            if allowed:
                self.emit('assert_stack', value=1)
                self.emit('resolve')
                self.mana[self.owner] = [0] * 6
                source = self.entities['source']
                source['power'] += 2
                source['toughness'] -= 2
                if source['toughness'] <= 0:
                    self.leave('source', 'graveyard')
            self.receipts()

    def expire(self):
        super().expire()
        if self.p['mode'] == 'mare':
            self.entities['source'].update(power=self.p['power'], toughness=self.p['toughness'])

    def plan_loot(self, *, qualifies, external=False):
        if not qualifies or self.scenario in ('decline', 'decline_empty'):
            return
        if self.owner == 0 and self.p['optional'] and not external:
            self.emit('choose_trigger_source', ref='source', player=0)
        hand = self.zones['hand'][self.owner]
        hand.append(self.zones['library'][self.owner].pop(0))
        if self.owner == 1 and len(hand) != 1:
            raise ValueError('nonhuman discard choice requires an unambiguous hand')
        high = self.scenario == 'discard_drawn'
        selected = sorted(hand, key=lambda name: self.rows[name]['csvid'], reverse=high)[0]
        hand.remove(selected)
        self.zones['graveyard'][self.owner].append(selected)
        if self.p['mode'] != 'wharf' or 'Creature' not in self.o[selected]['type_line']:
            return
        if self.owner == 0:
            self.emit('choose_trigger_source', ref='source', player=0)
        if self.scenario == 'wharf_payment_cancel' or (self.scenario == 'wharf_unfunded' and self.owner == 0):
            self.emit('choose_cancel_mana', player=0)
        elif self.mana[self.owner][0] >= 2:
            self.mana[self.owner][0] -= 2
            self.zones['battlefield'][self.owner].append(TOKEN)

    def prepare_wharf_payment(self):
        scenario = self.scenario
        if scenario == 'wharf_payment_cancel':
            self.pool(self.owner, [0, 0, 2, 0, 0, 0])
        if scenario in ('wharf_funded', 'wharf_noncreature', 'external_discard'):
            self.pool(self.owner, [2, 0, 0, 0, 0, 0])

    def external(self):
        self.prepare_wharf_payment()
        self.emit('require_can_activate', ref='external', value=1)
        self.emit('activate', ref='external', value=1)
        self.emit('assert_stack', value=1)
        self.entities['external']['tapped'] = True
        self.plan_loot(qualifies=True, external=True)
        self.emit('resolve')
        self.emit('assert_unused_choices', value=0)
        self.receipts()

    def direct_damage(self):
        self.spell('Hermetic Study', 'grant', self.owner, 'source')
        self.zones['graveyard'][self.owner].remove('Hermetic Study')
        self.zones['battlefield'][self.owner].append('Hermetic Study')
        target = self.owner if self.scenario == 'self_damage' else self.foe
        self.emit('require_can_activate', ref='grant', value=1)
        self.emit('choose_player', player=self.owner, value=target)
        self.emit('activate', ref='grant', value=1)
        self.emit('assert_stack', value=1)
        self.entities['source']['tapped'] = True
        self.life[target] -= 1
        self.plan_loot(qualifies=target == self.foe and not self.p['combat_only'])
        self.emit('resolve')
        self.emit('assert_unused_choices', value=0)
        self.receipts()

    def fight(self):
        scenario = self.scenario
        attacker = 'opposing' if scenario == 'source_blocks' else 'source'
        if scenario == 'source_blocks':
            blocker = 'source'
        else:
            blocker = 'blocker' if 'blocker' in self.entities else None
        blocked = blocker is not None and scenario != 'evasion'
        self.emit('begin_attackers')
        self.emit('attack', ref=attacker, value=1)
        attack = self.entities[attacker]
        attack.update(attacking=True, tapped=True)
        self.emit('finish_attackers')
        self.emit('begin_blockers')
        if blocker:
            self.emit('block', ref=blocker, other=attacker, value=int(blocked))
            self.observe(blocker, 'blocking_count', blocked)
        self.emit('advance_combat')
        self.observe(attacker, 'blocked', blocked)
        if scenario == 'prevented':
            self.pool(self.foe, [0, 0, 0, 1, 0, 0])
            self.emit('cast', ref='shield', value=1)
            self.emit('assert_stack', value=1)
            self.emit('resolve')
            self.mana[self.foe] = [0] * 6
            self.zones['hand'][self.foe].remove('Fog')
            self.zones['graveyard'][self.foe].append('Fog')
        self.receipts()
        self.prepare_wharf_payment()
        cut = len(self.steps) if scenario == 'snapshot' else None
        self.plan_loot(qualifies=(attacker == 'source' and not self.hidden and not blocked and attack['power'] > 0
                                  and scenario != 'prevented'))
        self.emit('combat_damage')
        self.emit('resolve')
        self.emit('assert_unused_choices', value=0)
        self.emit('end_combat')
        if blocked:
            defense = self.entities[blocker]
            attack['damage'] += defense['power']
            defense['damage'] += attack['power']
            for ref, entity in ((attacker, attack), (blocker, defense)):
                if entity['damage'] >= entity['toughness']:
                    self.leave(ref, 'graveyard')
        elif scenario != 'prevented':
            self.life[1 - self.current] -= attack['power']
        attack['attacking'] = False
        self.receipts()
        return cut


def scenario(world):
    mode, test = world.p['mode'], world.scenario
    world.setup_board()
    world.emit('turn', player=world.owner, value=0)
    world.emit('present_ai_dialogs', value=1)
    world.emit('discard_policy', value=2 if test == 'discard_drawn' else 1)
    for participant in (0, 1):
        world.pool(participant, [0] * 6)
    world.receipts()
    world.enter()
    if test == 'cast_short':
        return None
    world.age()
    if test == 'morph_offturn':
        world.next_turn()
        world.reveal(True)
        world.next_turn()
    if test in ('morph_hit', 'morph_short'):
        world.reveal(test == 'morph_hit')
        if test == 'morph_short':
            return None
    if test in ('noncombat', 'self_damage'):
        world.direct_damage()
        return None
    if test == 'external_discard':
        world.external()
        return None
    if test in ('pump_short', 'pump_lethal'):
        world.pump(1 if test == 'pump_short' else 3, allowed=test != 'pump_short')
        return None
    if test == 'source_blocks':
        world.next_turn()
    elif mode == 'mare' and test != 'zero_power':
        world.pump(1)
    cut = world.fight()
    if test == 'pump_duration':
        world.next_turn()
    return cut


def expand(name, csvid, declaration, params, selected, registry, oracle):
    from recipes import step

    check_variants(params, selected)
    validate_witnesses(oracle, registry)
    if profiles()['sources'][name]['registry']['csvid'] != csvid:
        raise ValueError('exact damage-loot source CSVID required')
    result = []
    for variant in selected:
        test, owner = variant.rsplit('_', 1)
        world = LootWorld(name, params, test, int(owner), registry, oracle)
        cut = scenario(world)
        steps = world.steps
        if cut is not None:
            tail = steps[cut:]
            steps = (steps[:cut] + [step('capture')] + tail + [step('record_state'), step('ai_roundtrip'), step('restore')]
                     + copy.deepcopy(tail) + [step('assert_recorded_state')])
        gaps = list(GAPS)
        if params['optional']:
            gaps.append('AI optional-decline strategy differs from the scripted human choice; human decline is exercised.')
        result.append(dict(name=f'DamageLootCreatureRecipe_{csvid}_{variant}', card=name, fixture='empty_main', seed=128,
                           setup=world.setup, steps=steps, _recipe=dict(family=FAMILY, version=2, variant=variant,
                               params=declaration['params'], oracle_text=declaration['oracle_text'], untested_components=gaps)))
    return result
