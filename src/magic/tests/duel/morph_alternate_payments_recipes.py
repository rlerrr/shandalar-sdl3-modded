"""Actual nonmana Morph payments and the complete printed companions."""

import copy
import json
from functools import lru_cache
from pathlib import Path

from attack_value_trigger_recipes import _AttackResourceWorld
from morph_creature_recipes import FACE_DOWN, COLORS

FAMILY = 'morph_alternate_payments'
PROFILE = Path(__file__).with_name('data') / 'morph_alternate_payments_wave8_profiles.json'
REMINDER = ' (You may cast this card face down as a 2/2 creature for {3}. Turn it face up any time for its morph cost.)'
DRAW = 'When this creature is turned face up, draw two cards.'
HELLBENT = 'Hellbent — This creature gets +2/+2 as long as you have no cards in hand.'
LOSE = 'When this creature is turned face up, target player loses 2 life.'
TRAMPLE = 'When this creature is turned face up, target creature gains trample until end of turn.'
GAIN = 'When this creature is turned face up, you gain 2 life.'
SELF_DAMAGE = 'Whenever this creature blocks, it deals 1 damage to you.'
NO_BLOCK = "When this creature is turned face up, target creature can't block this turn."
PLANS = {
    "Return two Islands you control to their owner's hand.": ('islands', (DRAW,), (), 'manalinkeh_card_fathom_seer'),
    'Discard a card.': ('discard', (HELLBENT,), ('Hellbent',), 'manalinkeh_card_gathan_raiders'),
    'Discard a Zombie card.': ('zombie', (), (), 'manalinkeh_card_putrid_raptor'),
    'Reveal a black card in your hand.': ('black', ('Deathtouch', LOSE), ('Deathtouch',), 'manalinkeh_card_ruthless_ripper'),
    'Reveal a green card in your hand.': ('green', (TRAMPLE,), (), 'manalinkeh_card_temur_charger'),
    'Reveal a white card in your hand.': ('white', ('Flying', GAIN), ('Flying',), 'manalinkeh_card_watcher_of_the_roost'),
    'Reveal a red card in your hand.': ('red', (SELF_DAMAGE, NO_BLOCK), (), 'manalinkeh_card_horde_ambusher'),
    'Pay 5 life.': ('life', (), (), 'manalinkeh_card_zombie_cutthroat'),
}
REVEAL = {'black': ('Dark Ritual', 'B'), 'green': ('Giant Growth', 'G'), 'white': ('Healing Salve', 'W'),
          'red': ('Shock', 'R')}
ZOMBIE = 'Scathe Zombies'
LARGE, SMALL, WALL, REACH = 'Hill Giant', "Mons's Goblin Raiders", 'Fortified Rampart', 'Giant Spider'
WITNESSES = {'Island', 'Tropical Island', 'Forest', 'Mountain', 'Plains', 'Shock', 'Unsummon', 'Grizzly Bears', 'Safe Passage',
             *[item[0] for item in REVEAL.values()], ZOMBIE, LARGE, SMALL, WALL, REACH}
DUMMIES = {ZOMBIE, LARGE, SMALL, WALL, REACH, 'Grizzly Bears'}
COMMON = ('hidden', 'printed', 'paid', 'denied', 'opponent_turn', 'pending_spell', 'replay', 'cast_short')
SPECIAL = {
    'islands': (),
    'discard': ('hellbent_nonempty', 'hellbent_changes'),
    'zombie': (),
    'black': ('self_loss', 'deathtouch_combat'),
    'green': ('trample_hit', 'trample_expiry', 'self_trample'),
    'white': ('flying_evasion', 'reachable'),
    'red': ('deny_block', 'block_expiry', 'untargeted_block', 'blocks_self_damage', 'blocks_prevented'),
    'life': ('exact_life',),
}
COMBAT = {'deathtouch_combat', 'trample_hit', 'trample_expiry', 'self_trample', 'flying_evasion', 'reachable'}
HORDE_BLOCK = ('deny_block', 'block_expiry', 'untargeted_block')
HORDE_DEFEND = ('blocks_self_damage', 'blocks_prevented')
GAPS = ('Multiplayer and a separately pending turn-up trigger; the native callback resolves the companion inline.',
        'Card disclosure uses the actual public notice; no separate persistent reveal-history observer.')


@lru_cache(maxsize=1)
def profiles():
    return json.loads(PROFILE.read_text(encoding='utf8'))


def morph_clause(record):
    return [line for line in record['oracle_text'].splitlines() if line.startswith('Morph—')]


def classify(record):
    from recipes import cost_from_oracle

    if (not record or record.get('layout') != 'normal' or record.get('selected_face') or record.get('card_faces')
            or not record.get('type_line', '').startswith('Creature — ')):
        raise ValueError('complete normal nonmana Morph creature required')
    printed = morph_clause(record)
    if len(printed) != 1 or not printed[0].endswith(REMINDER):
        raise ValueError('one complete nonmana Morph cost required')
    cost = printed[0].removeprefix('Morph—').removesuffix(REMINDER)
    plan = PLANS.get(cost)
    if plan is None:
        raise ValueError('unreviewed nonmana Morph payment')
    mode, companions, keywords, _ = plan
    lines = tuple(line for line in record['oracle_text'].splitlines() if line != printed[0])
    if lines != companions or set(record['keywords']) != {'Morph', *keywords}:
        raise ValueError('every printed nonmana Morph companion is required')
    p, t = (record.get(key) for key in ('power', 'toughness'))
    if any(not isinstance(stat, str) or not stat.isascii() or not stat.isdecimal() for stat in (p, t)):
        raise ValueError('fixed printed creature statistics required')
    return dict(payment=cost, mode=mode, keywords=list(keywords), power=int(p), toughness=int(t),
                mana=cost_from_oracle(record), color=sum(1 << COLORS.index(symbol) for symbol in record['colors']))


def variants(params):
    mode = params['mode']
    supported = [f'{scenario}_{owner}' for owner in (0, 1) for scenario in COMMON + SPECIAL[mode]]
    if mode in ('islands', 'zombie', *REVEAL):
        supported.append('cancel_0')
    return supported


def check_variants(params, selected):
    if (type(selected) is not list or not selected or any(type(v) is not str for v in selected)
            or len(set(selected)) != len(selected) or not set(selected) <= set(variants(params))):
        raise ValueError('explicit distinct reviewed nonmana Morph variants required')


def spec(declaration, record):
    params = classify(record)
    if (declaration.get('family') != FAMILY or type(declaration.get('version')) is not int or declaration['version'] != 2
            or profiles()['sources'].get(record['name'], {}).get('oracle') != record
            or declaration.get('oracle_text') != record['oracle_text']
            or json.dumps(declaration.get('params'), sort_keys=True) != json.dumps(params, sort_keys=True)):
        raise ValueError('whole pinned nonmana Morph record and exact typed parameters required')
    check_variants(params, declaration.get('variants'))
    return dict(params, family=FAMILY)


def validate_witnesses(oracle, rows):
    data = profiles()
    skipped = {row['csvid'] for row in json.loads((PROFILE.parent / 'unimplemented.json').read_text(
        encoding='utf8'))['cases'].values()}
    if (data.get('version') != 1 or set(data['witnesses']) != WITNESSES
            or data['hidden'] != rows.get('Face-Down Creature') or data['hidden']['csvid'] != FACE_DOWN):
        raise ValueError('reviewed nonmana Morph witnesses and hidden observer required')
    for group in ('sources', 'witnesses'):
        for name, pin in data[group].items():
            row, record = rows.get(name, {}), oracle.get(name, {})
            if (pin != dict(oracle=record, registry=row) or row.get('kind') != 'card' or record.get('layout') != 'normal'
                    or row['csvid'] in skipped or row['handler'] in (None, 'NULL', '0')
                    or (row['handler'] == 'card_dummy' and name not in DUMMIES)):
                raise ValueError(f'unreviewed nonmana Morph binding: {name}')
            if group == 'sources' and row['handler'] != PLANS[classify(record)['payment']][3]:
                raise ValueError(f'unreviewed nonmana Morph callback: {name}')
    for name in ('Island', 'Tropical Island'):
        if 'Island' not in oracle[name]['type_line'].split(' — ', 1)[1].split():
            raise ValueError('Island subtype witness required')
    if 'Zombie' not in oracle[ZOMBIE]['type_line'].split(' — ', 1)[1].split():
        raise ValueError('Zombie discard witness required')
    if any(symbol not in oracle[name]['colors'] for name, symbol in REVEAL.values()):
        raise ValueError('legal color reveal witnesses required')


class MorphWorld(_AttackResourceWorld):
    def __init__(self, name, params, test, owner, rows, oracle):
        super().__init__(name, dict(params, effect=FAMILY), owner, rows, oracle)
        self.test = test
        self.foe = 1 - owner
        self.mode = params['mode']
        self.hidden = False

    def observe(self, ref, prop, value):
        self.emit('assert_card', ref=ref, property=prop, value=int(value))

    def receipts(self):
        self.emit('refresh')
        source = self.entities['source']
        present = source['zone'] == 'battlefield'
        if present and not self.hidden and self.mode == 'discard':
            bonus = 2 * (not self.zones['hand'][self.owner])
            source.update(power=self.p['power'] + bonus, toughness=self.p['toughness'] + bonus)
        start = len(self.steps)
        super().receipts()
        if not present:
            return
        printed = self.rows[self.name]['csvid']
        self.observe('source', 'csvid', FACE_DOWN if self.hidden else printed)
        if self.hidden:
            for action in self.steps[start:]:
                if (action['op'] == 'assert_zone_multiplicity' and action.get('zone') == 'battlefield'
                        and action['player'] == self.owner and action['index'] == printed):
                    action['value'] = 0
            self.emit('assert_zone_multiplicity', player=self.owner, zone='battlefield', index=FACE_DOWN, value=1)
        for prop, value in (('color', 0 if self.hidden else self.p['color']), ('token', 0), ('artifact', 0)):
            self.emit('assert_matching_permanents', player=self.owner, index=FACE_DOWN if self.hidden else printed,
                      property=prop, value=value)

    def configure(self):
        test, mode, owner, foe = self.test, self.mode, self.owner, self.foe
        self.add(self.name, 'source', owner, 'hand', source=True)
        for player in (0, 1):
            for index, name in enumerate(('Mountain', 'Plains', 'Forest')):
                self.add(name, f'library_{player}_{index}', player, 'library')
        if mode == 'islands':
            if test != 'cast_short':
                self.add('Island', 'island', owner)
                if test != 'denied':
                    self.add('Tropical Island', 'dual_island', owner)
                self.add('Forest', 'not_island', owner)
            self.add('Island', 'foe_island', foe)
        elif mode == 'discard' and test != 'denied':
            self.add('Forest', 'payment', owner, 'hand')
            if test == 'hellbent_nonempty':
                self.add('Mountain' if owner == 0 else 'Forest', 'keep', owner, 'hand')
        elif mode == 'zombie' or mode in REVEAL:
            if test != 'denied':
                self.add(ZOMBIE if mode == 'zombie' else REVEAL[mode][0], 'payment', owner, 'hand')
            self.add('Mountain', 'decoy', owner, 'hand')
        if test == 'pending_spell':
            self.add('Shock', 'shock', foe, 'hand')
        if test == 'hellbent_changes':
            self.add('Grizzly Bears', 'ally', owner)
            self.add('Unsummon', 'bounce', foe, 'hand')
        if mode == 'green' and test != 'self_trample':
            self.add(LARGE, 'ally', owner)
        if mode == 'red':
            self.add(LARGE, 'ally', owner)
            if test in HORDE_BLOCK:
                self.add(WALL, 'blocker', foe)
            if test in HORDE_DEFEND:
                self.add(LARGE, 'opposing', foe)
            if test == 'blocks_prevented':
                self.add('Safe Passage', 'shield', owner, 'hand')
        if test in COMBAT:
            blocker = {'deathtouch_combat': LARGE, 'flying_evasion': WALL, 'reachable': REACH}.get(test, SMALL)
            self.add(blocker, 'blocker', foe)

    def cast_source_card(self):
        printed = self.test == 'printed'
        denied = self.test == 'cast_short'
        cost = self.p['mana'] if printed else [2 if denied else 3, 0, 0, 0, 0, 0]
        self.pool(self.owner, cost)
        self.emit('refresh')
        if not denied and (self.mode == 'islands' or printed and sum(cost) >= 3):
            # An untapped controlled Island makes Fathom's normal price
            # affordable even when the initial pool is entirely generic.
            self.emit('choose_dialog', player=self.owner, value=int(printed))
        self.emit('cast', ref='source', value=int(not denied))
        self.emit('assert_stack', value=int(not denied))
        self.emit('assert_choices_consumed')
        if not denied:
            self.mana[self.owner] = [0] * 6
            self.hidden = not printed
            self.observe('source', 'csvid', FACE_DOWN if self.hidden else self.rows[self.name]['csvid'])
            self.emit('resolve')
            self.arrive('source')
            if self.hidden:
                self.entities['source'].update(power=2, toughness=2, keys=set())
        self.receipts()

    def option(self, canceled):
        mode = self.mode
        if mode == 'islands':
            self.emit('choose_target', ref='island', player=self.owner)
            if canceled:
                self.emit('choose_cancel_target', player=self.owner)
            else:
                self.emit('choose_target', ref='dual_island', player=self.owner)
            return
        if mode not in REVEAL and mode != 'zombie':
            return
        payment = ZOMBIE if mode == 'zombie' else REVEAL[mode][0]
        self.emit('observe_search_filters', value=1)
        self.emit('choose_optional_zone' if canceled else 'choose_zone_card', player=self.owner,
                  value=self.rows[payment]['csvid'])
        if not canceled and mode in REVEAL and self.owner == 0:
            self.emit('choose_dialog', player=0, value=0)  # Native public reveal acknowledgement.

    def offered(self):
        mode = self.mode
        if mode not in REVEAL and mode != 'zombie':
            return
        valid = ZOMBIE if mode == 'zombie' else REVEAL[mode][0]
        hand = self.zones['hand'][self.owner]
        self.emit('assert_offered_count', value=len(hand))
        self.emit('assert_offered_selection_required', value=0)
        self.emit('assert_offered_legal_card', index=self.rows[valid]['csvid'], value=1)
        self.emit('assert_offered_legal_card', index=self.rows['Mountain']['csvid'], value=0)
        if self.test == 'blocks_prevented':
            self.emit('assert_offered_legal_card', index=self.rows['Safe Passage']['csvid'], value=0)
        self.emit('observe_search_filters', value=0)

    def transform(self, pending=False):
        denied, canceled = self.test == 'denied', self.test == 'cancel'
        self.emit('can_activate', ref='source', value=int(not denied))
        target = None
        if not denied:
            self.option(canceled)
        if not denied and not canceled and self.mode == 'black':
            target = self.owner if self.test == 'self_loss' else self.foe
            self.emit('choose_player', player=self.owner, value=target)
        if not denied and not canceled and self.mode == 'green':
            target = 'source' if self.test == 'self_trample' else 'ally'
            self.emit('choose_target', player=self.owner, ref=target)
        if not denied and not canceled and self.mode == 'red':
            target = 'blocker' if self.test in ('deny_block', 'block_expiry') else 'ally'
            self.emit('choose_target', player=self.owner, ref=target)
        self.emit('activate', ref='source', value=int(not denied and not canceled))
        self.emit('assert_unused_choices', value=0)
        if not denied:
            self.offered()
        if denied or canceled:
            self.emit('resolve')
            self.receipts()
            return
        self.hidden = False
        self.entities['source'].update(power=self.p['power'], toughness=self.p['toughness'], keys=set(self.p['keywords']))
        self.finalize_payment(target)
        # The current Morph callback flips before the activation proxy drains.
        # Observe the new identity immediately, even with a real parent spell.
        self.emit('refresh')
        self.observe('source', 'csvid', self.rows[self.name]['csvid'])
        self.emit('resolve_response' if pending else 'resolve')
        if pending:
            self.zones['hand'][self.foe].remove('Shock')
            self.zones['graveyard'][self.foe].append('Shock')
            self.mana[self.foe] = [0] * 6
            self.life[self.owner] -= 2
        self.receipts()

    def finalize_payment(self, target):
        mode, owner = self.mode, self.owner
        if mode == 'islands':
            for name in ('Island', 'Tropical Island'):
                self.zones['battlefield'][owner].remove(name)
                self.zones['hand'][owner].append(name)
            library = self.zones['library'][owner]
            self.zones['hand'][owner].extend(library[:2])
            del library[:2]
        if mode in ('discard', 'zombie'):
            paid = 'Forest' if mode == 'discard' else ZOMBIE
            self.zones['hand'][owner].remove(paid)
            self.zones['graveyard'][owner].append(paid)
        if mode == 'life':
            self.life[owner] -= 5
        if mode == 'black':
            self.life[target] -= 2
        if mode == 'white':
            self.life[owner] += 2
        if mode == 'green':
            self.entities[target]['grants'].add('Trample')

    def shock(self):
        self.pool(self.foe, [0, 0, 0, 0, 1, 0])
        self.emit('choose_player', player=self.foe, value=self.owner)
        self.emit('cast', ref='shock', value=1)
        self.emit('assert_stack', value=1)
        self.emit('assert_choices_consumed')

    def fight(self):
        attacker = 'ally' if self.test in ('trample_hit', 'trample_expiry') else 'source'
        blocked = self.test not in ('flying_evasion', 'deny_block')
        attack, defense = self.entities[attacker], self.entities['blocker']
        self.emit('begin_attackers')
        self.emit('attack', ref=attacker, value=1)
        self.emit('finish_attackers')
        attack.update(attacking=True, tapped=True)
        self.emit('begin_blockers')
        self.emit('block', ref='blocker', other=attacker, value=int(blocked))
        self.observe('blocker', 'blocking_count', blocked)
        self.emit('advance_combat')
        self.emit('combat_damage')
        self.emit('resolve')
        self.emit('end_combat')
        attack['attacking'] = False
        if blocked:
            strength = attack['power']
            if self.has(attacker, 'Trample'):
                assigned = min(strength, defense['toughness'])
                self.life[self.foe] -= strength - assigned
                strength = assigned
            attack['damage'] += defense['power']
            defense['damage'] += strength
            if attack['damage'] >= attack['toughness']:
                self.leave(attacker, 'graveyard')
            if defense['damage'] >= defense['toughness'] or (strength > 0 and self.has(attacker, 'Deathtouch')):
                self.leave('blocker', 'graveyard')
        else:
            self.life[self.foe] -= attack['power']
        self.receipts()

    def horde_defense(self):
        # A real spell establishes prevention before the blocker and combat
        # damage. A blocked attacker cannot account for damage to the player.
        prevented = self.test == 'blocks_prevented'
        if prevented:
            from recipes import cost_from_oracle

            self.pool(self.owner, cost_from_oracle(self.o['Safe Passage']))
            self.emit('cast', ref='shield', value=1)
            self.emit('assert_stack', value=1)
            self.emit('resolve')
            self.mana[self.owner] = [0] * 6
            self.zones['hand'][self.owner].remove('Safe Passage')
            self.zones['graveyard'][self.owner].append('Safe Passage')
        self.emit('begin_attackers')
        self.emit('attack', ref='opposing', value=1)
        opposing = self.entities['opposing']
        opposing.update(attacking=True, tapped=True)
        self.emit('finish_attackers')
        self.emit('begin_blockers')
        self.emit('block', ref='source', other='opposing', value=1)
        self.observe('source', 'blocking_count', 1)
        self.emit('advance_combat')
        self.emit('resolve')
        self.emit('combat_damage')
        self.emit('resolve')
        self.emit('end_combat')
        opposing.update(attacking=False, damage=2)
        if not prevented:
            self.life[self.owner] -= 1
            self.leave('source', 'graveyard')
        self.receipts()


def scenario(world):
    test, mode = world.test, world.mode
    world.configure()
    world.emit('turn', player=world.owner, value=0)
    world.emit('present_ai_dialogs', value=1)
    world.emit('discard_policy', value=1)
    for player in (0, 1):
        world.pool(player, [0] * 6)
    if mode == 'life' and test in ('exact_life', 'denied'):
        starting = 5 if test == 'exact_life' else 4
        world.emit('set_life', player=world.owner, value=starting)
        world.life[world.owner] = starting
    world.receipts()
    world.cast_source_card()
    if test in ('printed', 'cast_short', 'hidden'):
        return None
    pending = test == 'pending_spell'
    if pending or test == 'opponent_turn' or test in HORDE_DEFEND:
        world.next_turn()
    if test in COMBAT or test in HORDE_BLOCK:
        world.age()
    if pending:
        world.shock()
    cut = len(world.steps) if test == 'replay' else None
    world.transform(pending)
    if test == 'hellbent_changes':
        world.spell('Unsummon', 'bounce', world.foe, 'ally')
        world.leave('ally', 'hand')
        world.receipts()
    if test in COMBAT or test in HORDE_BLOCK:
        if test in ('trample_expiry', 'block_expiry'):
            world.age()
        world.fight()
    if test in HORDE_DEFEND:
        world.horde_defense()
    return cut


def expand(name, csvid, declaration, params, selected, rows, oracle):
    from recipes import step

    check_variants(params, selected)
    validate_witnesses(oracle, rows)
    if profiles()['sources'][name]['registry']['csvid'] != csvid:
        raise ValueError('exact nonmana Morph source CSVID required')
    cases = []
    for variant in selected:
        test, owner = variant.rsplit('_', 1)
        world = MorphWorld(name, params, test, int(owner), rows, oracle)
        cut = scenario(world)
        steps = world.steps
        if cut is not None:
            tail = steps[cut:]
            steps = (steps[:cut] + [step('capture')] + tail + [step('record_state'), step('ai_roundtrip'), step('restore')]
                     + copy.deepcopy(tail) + [step('assert_recorded_state')])
        cases.append(dict(name=f'MorphAlternatePaymentRecipe_{csvid}_{variant}', card=name, fixture='empty_main', seed=238,
                          setup=world.setup, steps=steps, _recipe=dict(family=FAMILY, version=2, variant=variant,
                              params=declaration['params'], oracle_text=declaration['oracle_text'],
                              untested_components=list(GAPS))))
    return cases
