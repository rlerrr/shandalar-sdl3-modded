"""Whole-card discard triggers on actual declared blockers and combat."""

import copy
import json
from pathlib import Path

from attack_value_trigger_recipes import _AttackResourceWorld

FAMILY = 'blocked_discard'
PROFILE = Path(__file__).with_name('data') / 'blocked_discard_wave8_profiles.json'
BLOCKED = 'Whenever this creature becomes blocked, defending player discards a card.'
UNBLOCKED = "Whenever this creature attacks and isn't blocked, defending player discards a card."
ROBBER = ('Flying\nWhenever this creature becomes blocked, defending player discards all the cards in their hand, '
          'then draws that many cards.')
THRULL = ("Whenever this creature attacks and isn't blocked, you may sacrifice it. "
          'If you do, defending player discards three cards.')
MINDRIPPER = 'Shadow (This creature can block or be blocked by only creatures with shadow.)\n' + THRULL
RULES = {
    BLOCKED: (True, 'one', (), 'manalinkeh_card_alley_grifters'),
    UNBLOCKED: (False, 'one', (), 'manalinkeh_card_abyssal_nightstalker'),
    ROBBER: (True, 'redraw', ('Flying',), 'manalinkeh_card_robber_fly'),
    THRULL: (False, 'sacrifice', (), 'manalinkeh_card_mindstab_thrull'),
    MINDRIPPER: (False, 'sacrifice', ('Shadow',), 'manalinkeh_card_dauthi_mindripper'),
}
DISCARD_CARDS = ('Forest', 'Lightning Bolt', 'Dark Ritual', 'Swamp')
DRAW_CARDS = ('Island', 'Mountain', 'Plains', 'Counterspell', 'Unsummon')
WALL = 'Wall of Stone'
SECOND_WALL = 'Fortified Rampart'
REACH = 'Giant Spider'
FLYER = 'Storm Crow'
OPPOSING = 'Grizzly Bears'
SHADOW = 'Dauthi Marauder'
SECOND_SHADOW = 'Soltari Foot Soldier'
WITNESSES = set(DISCARD_CARDS + DRAW_CARDS + (WALL, SECOND_WALL, REACH, FLYER, OPPOSING, SHADOW, SECOND_SHADOW))
COMMON = ('blocked', 'unblocked', 'multi_blockers', 'empty_hand', 'short_hand', 'late_blocker_bounced',
          'source_blocks', 'pending_block_snapshot', 'countered', 'wrong_color', 'insufficient_mana')
GAPS = (
    'The driver completes the real blocker event in one operation; it cannot insert a response to an individual pending discard trigger.',
    'The two-blocker test stops after trigger resolution. Single-blocker tests separately drive combat damage.',
)
REDRAW_GAP = ('Robber Fly with enemy-forced discard prevented is held: legacy new_discard_all retries '
               'while the hand count remains positive.')


def profiles():
    return json.loads(PROFILE.read_text(encoding='utf8'))


def classify(record):
    from recipes import cost_from_oracle

    rule = RULES.get(record.get('oracle_text')) if record else None
    if (rule is None or record.get('layout') != 'normal' or record.get('selected_face') or record.get('card_faces')
            or not record.get('type_line', '').startswith('Creature — ') or record.get('keywords') != list(rule[2])):
        raise ValueError('whole standalone declared-blockers discard clause required')
    fixed = [record.get(prop) for prop in ('power', 'toughness')]
    if any(not isinstance(value, str) or not value.isascii() or not value.isdecimal() for value in fixed):
        raise ValueError('fixed power and toughness required')
    power, toughness = map(int, fixed)
    blocked, effect, keywords, _ = rule
    return dict(mana=cost_from_oracle(record), power=power, toughness=toughness, blocked=blocked,
                effect=effect, keywords=list(keywords))


def variants(params):
    modes = COMMON
    if params['effect'] == 'redraw':
        modes += ('flying_ground', 'flying_blocker')
    if params['effect'] == 'sacrifice':
        modes += ('decline',)
    if 'Shadow' in params['keywords']:
        modes += ('shadow_ground_denied', 'shadow_defends')
    result = [f'{mode}_{owner}' for owner in (0, 1) for mode in modes]
    if params['effect'] != 'redraw':
        result.extend(('human_discard_high_1',))
    return result


def check_variants(params, selected):
    if (type(selected) is not list or not selected or any(type(item) is not str for item in selected)
            or len(set(selected)) != len(selected) or not set(selected) <= set(variants(params))):
        raise ValueError('distinct reviewed combat discard variants required')


def spec(declaration, record):
    expected = classify(record)
    pin = profiles()['sources'].get(record['name'])
    if (declaration.get('family') != FAMILY or type(declaration.get('version')) is not int or declaration['version'] != 2
            or not pin or pin['oracle'] != record or declaration.get('oracle_text') != record['oracle_text']
            or json.dumps(expected, sort_keys=True) != json.dumps(declaration.get('params'), sort_keys=True)):
        raise ValueError('whole combat-discard Oracle and exact typed parameters required')
    check_variants(expected, declaration.get('variants'))
    return dict(expected, family=FAMILY)


def validate_witnesses(oracle, registry):
    data = profiles()
    skipped = {row['csvid'] for row in json.loads((PROFILE.parent / 'unimplemented.json').read_text(
        encoding='utf8'))['cases'].values()}
    if data.get('version') != 1 or set(data['witnesses']) != WITNESSES or set(data['sources']) & WITNESSES:
        raise ValueError('reviewed combat-discard witness set changed')
    for group in ('sources', 'witnesses'):
        for name, pin in data[group].items():
            row = registry.get(name, {})
            if (pin['oracle'] != oracle.get(name) or pin['registry'] != row or row.get('kind') != 'card'
                    or row['csvid'] in skipped or row['handler'] == 'NULL'
                    or (row['handler'] == 'card_dummy' and not pin.get('reviewed_native_characteristics'))):
                raise ValueError(f'changed combat-discard Oracle or binding: {name}')
            if group == 'sources':
                classify(oracle[name])
                if row['handler'] != RULES[oracle[name]['oracle_text']][3]:
                    raise ValueError(f'unreviewed combat-discard handler: {name}')


class DiscardWorld(_AttackResourceWorld):
    def __init__(self, name, params, owner, registry, oracle):
        super().__init__(name, dict(params, entry_energy=0), owner, registry, oracle)
        self.defender = 1 - owner

    def observe(self, ref, prop, value):
        self.emit('assert_card', ref=ref, property=prop, value=int(value))

    def receipts(self):
        self.emit('refresh')
        super().receipts()

    def configure(self, mode):
        self.add(self.name, 'source', self.owner, 'hand', source=True)
        amount = {'empty_hand': 0, 'short_hand': 2 if self.p['effect'] == 'sacrifice' else 1}.get(mode)
        if amount is None:
            amount = {'one': 2, 'redraw': 3, 'sacrifice': 4}[self.p['effect']]
        # Against the default nonhuman discard chooser, equivalent cards make
        # the result exact regardless of which legal card the engine picks.
        hand = DISCARD_CARDS[:amount] if self.defender == 0 else (DISCARD_CARDS[0],) * amount
        for index, name in enumerate(hand):
            self.add(name, f'original_hand_{index}', self.defender, 'hand')
        self.add('Swamp', 'attacker_keeps', self.owner, 'hand')
        self.add('Dark Ritual', 'attacker_top', self.owner, 'library')
        for index, name in enumerate(DRAW_CARDS):
            self.add(name, f'draw_{index}', self.defender, 'library')
        self.add('Mountain', 'old_grave', self.defender, 'graveyard')
        self.add('Forest', 'old_exile', self.defender, 'exile')
        if mode == 'countered':
            self.add('Counterspell', 'counter', self.defender, 'hand')
        if mode == 'source_blocks':
            self.add(OPPOSING, 'opposing', self.defender)
        if mode == 'shadow_defends':
            self.add(SHADOW, 'opposing', self.defender)
        if mode == 'late_blocker_bounced':
            self.add('Unsummon', 'bounce', self.owner, 'hand')
        if mode in ('wrong_color', 'insufficient_mana', 'countered', 'source_blocks', 'shadow_defends'):
            return []
        wants_block = (mode in ('blocked', 'multi_blockers', 'late_blocker_bounced', 'flying_ground',
                                'flying_blocker', 'shadow_ground_denied')
                       or mode not in ('unblocked', 'decline') and self.p['blocked'])
        if not wants_block:
            return []
        first = (WALL if mode == 'flying_ground' or self.p['effect'] != 'redraw'
                 else FLYER if mode == 'flying_blocker' else REACH)
        if self.has('source', 'Shadow') and mode != 'shadow_ground_denied':
            first = SHADOW
        self.add(first, 'blocker', self.defender)
        if mode == 'multi_blockers':
            second = {REACH: FLYER, SHADOW: SECOND_SHADOW, WALL: SECOND_WALL}[first]
            self.add(second, 'second_blocker', self.defender)
            return ['blocker', 'second_blocker']
        return ['blocker']

    def counter_source(self):
        from recipes import cost_from_oracle

        self.pool(self.owner, self.p['mana'])
        self.emit('cast', ref='source', value=1)
        self.emit('assert_stack', value=1)
        self.emit('announce_spell', ref='source')
        self.pool(self.defender, cost_from_oracle(self.o['Counterspell']))
        self.emit('cast_response', ref='counter', value=1)
        self.emit('assert_stack', value=2)
        self.emit('resolve_response')
        self.mana = [[0] * 6, [0] * 6]
        self.leave('source', 'graveyard')
        self.zones['hand'][self.defender].remove('Counterspell')
        self.zones['graveyard'][self.defender].append('Counterspell')
        self.receipts()

    def declare(self, attacker, blockers, legal):
        self.emit('begin_attackers')
        self.emit('attack', ref=attacker, value=1)
        self.entities[attacker].update(attacking=True, tapped=True)
        self.emit('finish_attackers')
        self.receipts()
        self.emit('begin_blockers')
        for ref in blockers:
            self.emit('block', ref=ref, other=attacker, value=int(legal))
            self.observe(ref, 'blocking_count', legal)
        # The declared-blockers event has not happened yet. These assertions
        # distinguish the attack, assignment, and real trigger boundaries.
        self.receipts()

    def complete_declaration(self, attacker, blocked, discard, decline, highest):
        self.emit('advance_combat')
        self.emit('resolve')
        self.emit('assert_unused_choices', value=0)
        if discard and not decline:
            hand = self.zones['hand'][self.defender]
            effect = self.p['effect']
            if effect == 'sacrifice':
                self.leave('source', 'graveyard')
            count = len(hand) if effect == 'redraw' else min(3 if effect == 'sacrifice' else 1, len(hand))
            chosen = sorted(hand, key=lambda name: self.rows[name]['csvid'], reverse=highest)[:count]
            for card in chosen:
                hand.remove(card)
                self.zones['graveyard'][self.defender].append(card)
            if effect == 'redraw':
                library = self.zones['library'][self.defender]
                hand.extend(library[:count])
                del library[:count]
        if self.entities[attacker]['zone'] == 'battlefield':
            self.observe(attacker, 'blocked', blocked)
        self.receipts()

    def combat_damage(self, attacker, blocker, blocked):
        self.emit('combat_damage')
        self.emit('resolve')
        self.emit('end_combat')
        attack = self.entities[attacker]
        if attack['zone'] == 'battlefield':
            if not blocked:
                self.life[1 - self.current] -= attack['power']
            elif blocker and self.entities[blocker]['zone'] == 'battlefield':
                defense = self.entities[blocker]
                attack['damage'] += defense['power']
                defense['damage'] += attack['power']
                for ref, creature in ((attacker, attack), (blocker, defense)):
                    if creature['damage'] >= creature['toughness']:
                        self.leave(ref, 'graveyard')
        attack['attacking'] = False
        self.receipts()


def scenario(world, mode):
    blockers = world.configure(mode)
    world.emit('turn', player=world.owner, value=0)
    world.emit('present_ai_dialogs', value=1)
    highest = mode == 'human_discard_high'
    world.emit('discard_policy', value=2 if highest else 1)
    for player in (0, 1):
        world.pool(player, [0] * 6)
    world.receipts()
    if mode == 'countered':
        world.counter_source()
        return None
    if mode in ('wrong_color', 'insufficient_mana'):
        world.cast_source('source', denied=mode)
        return None
    world.cast_source('source')
    world.age()
    shadow = world.has('source', 'Shadow')
    defending = mode in ('source_blocks', 'shadow_defends')
    if defending:
        world.next_turn()
        blockers = ['source']
    attacker = 'opposing' if defending else 'source'
    legal = mode not in ('flying_ground', 'shadow_ground_denied') and not (shadow and mode == 'source_blocks')
    world.declare(attacker, blockers, legal)
    blocked = bool(blockers) and legal
    discard = not defending and blocked == world.p['blocked']
    decline = mode == 'decline'
    if discard and world.p['effect'] == 'sacrifice':
        world.emit('choose_dialog', player=world.owner, value=int(decline))
    cut = len(world.steps) if mode == 'pending_block_snapshot' else None
    world.complete_declaration(attacker, blocked, discard, decline, highest)
    if shadow and decline:
        # An optional triggered ability does not grant a separate activation
        # after its choice resolves. Decline keeps a live ref for this receipt.
        world.emit('can_activate', ref='source', value=0)
    if mode == 'multi_blockers':
        return None
    if mode == 'late_blocker_bounced':
        world.spell('Unsummon', 'bounce', world.owner, 'blocker')
        world.leave('blocker', 'hand')
        world.observe(attacker, 'blocked', True)
        world.receipts()
    world.combat_damage(attacker, blockers[0] if blockers else None, blocked)
    return cut


def expand(name, csvid, declaration, params, selected, registry, oracle):
    from recipes import step

    check_variants(params, selected)
    validate_witnesses(oracle, registry)
    if profiles()['sources'][name]['registry']['csvid'] != csvid:
        raise ValueError('combat-discard source CSVID changed')
    cases = []
    for variant in selected:
        mode, owner = variant.rsplit('_', 1)
        world = DiscardWorld(name, params, int(owner), registry, oracle)
        cut = scenario(world, mode)
        actions = world.steps
        if cut is not None:
            tail = actions[cut:]
            actions = (actions[:cut] + [step('capture')] + tail + [step('record_state'), step('ai_roundtrip'), step('restore')]
                       + copy.deepcopy(tail) + [step('assert_recorded_state')])
        gaps = GAPS + ((REDRAW_GAP,) if params['effect'] == 'redraw' else ())
        cases.append(dict(name=f'BlockedDiscardRecipe_{csvid}_{variant}', card=name, fixture='empty_main', seed=241,
                          setup=world.setup, steps=actions, _recipe=dict(family=FAMILY, version=2, variant=variant,
                              params=declaration['params'], oracle_text=declaration['oracle_text'],
                              untested_components=list(gaps))))
    return cases
