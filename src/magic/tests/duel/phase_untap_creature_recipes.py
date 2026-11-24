"""Phasing through genuine untap, upkeep, spell, attachment, and combat actions."""

import copy
import json
from collections import Counter
from pathlib import Path

FAMILY = 'phase_untap_creature'
PROFILE = Path(__file__).with_name('data') / 'phase_untap_creature_wave8_profiles.json'
PHASING = ("Phasing (This phases in or out before you untap during each of your untap steps. "
           "While it's phased out, it's treated as though it doesn't exist.)")
ISLANDWALK = "Islandwalk (This creature can't be blocked as long as defending player controls an Island.)"
IMP = ('Whenever this creature phases out, discard a card.\nWhenever this creature phases in, draw a card.')
HANDLERS = {'manalinkeh_card_breezekeeper', 'manalinkeh_card_sandbar_crocodile'}
EFFECT_RULES = {
    "Target artifact, creature, or land phases out. (While it's phased out, it's treated as though it doesn't exist. It phases in before its controller untaps during their next untap step.)": 'ripple',
    "{U}{U}, {T}: Target creature phases out. (While it's phased out, it's treated as though it doesn't exist. It phases in before its controller untaps during their next untap step.)": 'illusionist',
    "Whenever this creature blocks or becomes blocked by a creature, this creature and that creature phase out. (While they're phased out, they're treated as though they don't exist. Each one phases in before its controller untaps during their next untap step.)": 'dream',
    "Whenever a creature you control attacks, it phases out at end of combat. (While it's phased out, it's treated as though it doesn't exist. It phases in before you untap during your next untap step.)": 'veil',
}
EFFECT_HANDLERS = {'ripple': 'manalinkeh_card_reality_ripple', 'illusionist': 'manalinkeh_card_vodalian_illusionist',
                   'dream': 'manalinkeh_card_dream_fighter', 'veil': 'manalinkeh_card_teferis_veil'}
EFFECT_MODES = {
    'ripple': ('opponent_creature', 'own_creature', 'opponent_artifact', 'opponent_land', 'carry_aura',
               'offturn', 'insufficient_mana', 'only_enchantment', 'lost_target', 'pending_replay'),
    'illusionist': ('opponent_creature', 'own_creature', 'self_target', 'offturn', 'fresh', 'short_activation',
                    'wrong_activation_color', 'noncreature_filter', 'pending_replay'),
    'dream': ('blocks', 'blocked', 'two_blockers', 'unblocked', 'unrelated_fight', 'combat_replay'),
    'veil': ('own_attack', 'two_attackers', 'opponent_attack', 'own_absent', 'attacker_dies',
             'veil_removed_pending', 'combat_replay'),
}
FILLER = "Mons's Goblin Raiders"
WITNESSES = {'Burst of Strength', 'Icy Manipulator', 'Holy Strength', 'Bonesplitter', 'Soul Warden', 'Dingus Staff',
             'Naturalize', 'Stasis', 'Counterspell', 'Murder', 'Grizzly Bears', FILLER, 'Giant Spider',
             'Plains', 'Forest', 'Mountain', 'Island'}
MODES = ('cycle', 'untap_order', 'retained_attachments', 'foreign_aura', 'target_visibility', 'no_zone_change_triggers',
         'skip_out', 'skip_in', 'replay_phase_in', 'attack_after_return', 'block_while_out',
         'wrong_color', 'insufficient_mana', 'countered')
BATTLE_MODES = ('attack_after_return', 'flying_ground', 'flying_reach', 'islandwalk_enabled', 'islandwalk_own_island')
GAPS = ('independently responding to a pending phase trigger', 'ownership-changing effects on the phasing source',
        'game outcome for an empty-library draw')


def profiles():
    return json.loads(PROFILE.read_text(encoding='utf8'))


def classify(record):
    from recipes import cost_from_oracle

    if not record or record.get('layout') != 'normal' or record.get('selected_face') or record.get('card_faces'):
        raise ValueError('whole standalone phasing source required')
    text = record.get('oracle_text', '')
    effect = EFFECT_RULES.get(text)
    if effect:
        expected = {'ripple': 'Instant', 'veil': 'Enchantment'}.get(effect)
        type_line = record.get('type_line', '')
        if record.get('keywords') != [] or (type_line != expected if expected else not type_line.startswith('Creature — ')):
            raise ValueError('phasing source type or keywords changed')
        return dict(mana=cost_from_oracle(record), mechanism=effect)
    if not record.get('type_line', '').startswith('Creature — '):
        raise ValueError('whole fixed-stat phasing creature required')
    legal = {PHASING: (False, False, False), 'Flying\n' + PHASING: (True, False, False),
             ISLANDWALK + '\n' + PHASING: (False, True, False), 'Flying\n' + PHASING + '\n' + IMP: (True, False, True)}
    if text not in legal:
        raise ValueError('unsupported phasing companion ability')
    flying, islandwalk, imp = legal[text]
    expected = {'Phasing'} | ({'Flying'} if flying else set()) | ({'Landwalk', 'Islandwalk'} if islandwalk else set())
    if set(record.get('keywords', [])) != expected:
        raise ValueError('phasing companion keyword record changed')
    values = [record.get(key) for key in ('power', 'toughness')]
    if any(not isinstance(value, str) or not value.isascii() or not value.isdecimal() for value in values):
        raise ValueError('printed creature statistics must be fixed')
    power, toughness = map(int, values)
    return dict(mana=cost_from_oracle(record), power=power, toughness=toughness, flying=flying, islandwalk=islandwalk, imp=imp)


def variants(params):
    if 'mechanism' in params:
        return [f'{mode}_{player}' for player in (0, 1) for mode in EFFECT_MODES[params['mechanism']]]
    modes = MODES
    if params['flying']:
        modes += ('flying_ground', 'flying_reach')
    if params['islandwalk']:
        modes += ('islandwalk_enabled', 'islandwalk_own_island')
    if params['imp']:
        modes += ('imp_timing', 'imp_empty_hand')
    result = [f'{mode}_{player}' for player in (0, 1) for mode in modes]
    return result + (['imp_choice_0'] if params['imp'] else [])


def check_variants(params, selected):
    if (type(selected) is not list or not selected or any(type(item) is not str for item in selected)
            or len(set(selected)) != len(selected) or not set(selected) <= set(variants(params))):
        raise ValueError('distinct reviewed phasing variants required')


def spec(declaration, record):
    expected = classify(record)
    pin = profiles()['sources'].get(record['name'])
    if (declaration.get('family') != FAMILY or type(declaration.get('version')) is not int or declaration['version'] != 2
            or not pin or pin['oracle'] != record or declaration.get('oracle_text') != record['oracle_text']
            or json.dumps(expected, sort_keys=True) != json.dumps(declaration.get('params'), sort_keys=True)):
        raise ValueError('whole phasing Oracle and exact typed parameters required')
    check_variants(expected, declaration.get('variants'))
    return dict(expected, family=FAMILY, effect=FAMILY)


def validate_witnesses(oracle, registry):
    data = profiles()
    skipped = {row['csvid'] for row in json.loads((PROFILE.parent / 'unimplemented.json').read_text(
        encoding='utf8'))['cases'].values()}
    if data.get('version') != 1 or set(data['witnesses']) != WITNESSES or set(data['sources']) & WITNESSES:
        raise ValueError('reviewed phasing profile/witness set changed')
    for group in ('sources', 'witnesses'):
        for name, pin in data[group].items():
            row = registry.get(name, {})
            if (pin['oracle'] != oracle.get(name) or pin['registry'] != row or row.get('kind') != 'card'
                    or row['csvid'] in skipped or row['handler'] == 'NULL'
                    or (row['handler'] == 'card_dummy' and not pin.get('reviewed_native_characteristics'))):
                raise ValueError(f'changed phasing Oracle or binding: {name}')
            if group == 'sources':
                model = classify(oracle[name])
                handlers = {EFFECT_HANDLERS[model['mechanism']]} if 'mechanism' in model else HANDLERS
                if row['handler'] not in handlers:
                    raise ValueError(f'unreviewed phasing handler: {name}')


class PhaseWorld:
    def __init__(self, name, params, owner, registry, oracle, check_imp_timing):
        self.name, self.params, self.rows, self.oracle = name, params, registry, oracle
        self.owner = self.current = owner
        self.foe = 1 - owner
        self.setup, self.steps = [], []
        self.objects = {}
        self.library = [[], []]
        self.life = [20, 20]
        self.attachments = []
        self.counters = 0
        self.check_imp_timing = check_imp_timing

    def emit(self, op, **fields):
        from recipes import step

        self.steps.append(step(op, **fields))

    def add(self, name, ref, player=None, zone='hand'):
        from recipes import card

        player = self.owner if player is None else player
        self.setup.append(card(name, ref, player, zone))
        self.objects[ref] = dict(name=name, player=player, zone=zone, tapped=False, phased=False, damage=0)
        if zone == 'library':
            self.library[player].append(ref)

    def observe(self, ref, field, value):
        self.emit('assert_card', ref=ref, property=field, value=int(value))

    def pool(self, player, cost):
        for color, amount in enumerate(cost):
            self.emit('set_mana', player=player, color=color, value=amount)

    def mana(self, player, expected):
        for color, amount in enumerate(expected):
            self.emit('assert_mana', player=player, color=color, value=amount)

    def cast(self, ref, *, target=None, denial=None, response=False):
        from recipes import cost_from_oracle

        item = self.objects[ref]
        player = item['player']
        cost = cost_from_oracle(self.oracle[item['name']])
        if denial:
            cost = [sum(cost) if denial == 'wrong_color' else 0] + [0] * 5
        self.pool(player, cost)
        self.emit('refresh')
        if target is not None:
            self.emit('choose_target', player=player, ref=target)
        self.emit('cast_response' if response else 'cast', ref=ref, value=int(denial is None))
        self.emit('assert_choices_consumed')
        self.emit('assert_stack', value=0 if denial else 2 if response else 1)
        self.mana(player, cost if denial else [0] * 6)
        self.observe(ref, 'in_hand', denial is not None)
        self.observe(ref, 'in_play', 0)

    def resolve(self, ref, zone='battlefield', target=None):
        self.cast(ref, target=target)
        self.emit('resolve')
        self.objects[ref]['zone'] = zone
        self.emit('assert_choices_consumed')

    def pay_tap(self, ref):
        self.pool(self.owner, [1, 0, 0, 0, 0, 0])
        self.emit('refresh')
        self.emit('require_can_activate', ref=ref, value=1)
        self.emit('choose_target', ref='source', player=self.owner)
        self.emit('activate', ref=ref, value=1)
        self.emit('assert_stack', value=1)
        self.emit('assert_choices_consumed')
        self.mana(self.owner, [0] * 6)
        self.emit('resolve')
        self.emit('assert_choices_consumed')
        if ref == 'icy':
            self.objects[ref]['tapped'] = True
            self.objects['source']['tapped'] = True
        else:
            self.attachments.append(ref)

    def stats(self, ref):
        obj = self.objects[ref]
        record = self.oracle[obj['name']]
        power, toughness = int(record['power']), int(record['toughness'])
        if ref == 'source':
            power += self.counters + int('aura' in self.attachments) + 2 * int('equipment' in self.attachments)
            toughness += self.counters + 2 * int('aura' in self.attachments)
        return power, toughness

    def source_presence(self):
        obj = self.objects['source']
        if obj['zone'] == 'battlefield':
            # Inspect this named physical instance even while game rules ignore it.
            self.observe('source', 'csvid', self.rows[self.name]['csvid'])
            self.observe('source', 'in_play', not obj['phased'])
            self.observe('source', 'in_hand', False)
            self.observe('source', 'tapped', obj['tapped'])

    def receipts(self, *, check_handflow=True):
        self.emit('refresh')
        self.emit('assert_stack', value=0)
        self.emit('assert_choices_consumed')
        self.source_presence()
        for ref, obj in self.objects.items():
            if obj['zone'] != 'battlefield':
                continue
            if ref != 'source':
                self.observe(ref, 'csvid', self.rows[obj['name']]['csvid'])
                self.observe(ref, 'in_play', not obj['phased'])
                self.observe(ref, 'tapped', obj['tapped'])
            if 'Creature' in self.oracle[obj['name']]['type_line'].split(' — ')[0].split() and not obj['phased']:
                power, toughness = self.stats(ref)
                for key, value in (('power', power), ('toughness', toughness), ('damage', obj['damage'])):
                    self.observe(ref, key, value)
                if ref == 'source':
                    self.observe(ref, 'plus1_counters', self.counters)
        names = sorted({obj['name'] for obj in self.objects.values()})
        imp_cards = {obj['name'] for ref, obj in self.objects.items()
                     if ref in ('discard', 'keeper') or ref.startswith('draw_')}
        ledger = Counter((obj['player'], obj['zone'], obj['name']) for obj in self.objects.values())
        for player in (0, 1):
            self.emit('assert_life', player=player, value=self.life[player])
            # Its battlefield observer enumerates raw slots, including phased permanents.
            for zone in ('battlefield', 'hand', 'graveyard', 'exile'):
                for name in names:
                    if not check_handflow and player == self.owner and name in imp_cards and zone in ('hand', 'graveyard'):
                        continue
                    self.emit('assert_zone_multiplicity', player=player, zone=zone, index=self.rows[name]['csvid'],
                              value=ledger[player, zone, name])
            if not check_handflow and player == self.owner:
                continue
            self.emit('assert_zone', player=player, zone='library', value=len(self.library[player]))
            for index, ref in enumerate(self.library[player]):
                self.emit('assert_zone_card', player=player, zone='library', index=index,
                          value=self.rows[self.objects[ref]['name']]['csvid'])

    def advance(self):
        self.emit('advance_turn')
        self.current = 1 - self.current
        for obj in self.objects.values():
            obj['damage'] = 0
        self.source_presence()

    def imp_upkeep(self, phased_out):
        if phased_out:
            hand = [obj for obj in self.objects.values() if obj['player'] == self.owner and obj['zone'] == 'hand']
            if self.owner == 1 and len(hand) > 1:
                raise ValueError('use a sole legal AI discard witness')
            if hand:
                min(hand, key=lambda obj:self.rows[obj['name']]['csvid'])['zone'] = 'graveyard'
        else:
            top = self.library[self.owner].pop(0)
            self.objects[top]['zone'] = 'hand'

    def finish_turn_start(self, skip=False):
        self.emit('untap_step', player=self.current)
        source = self.objects['source']
        changes = not skip and self.current == self.owner and source['zone'] == 'battlefield'
        if changes:
            source['phased'] = not source['phased']
            for ref in self.attachments:
                self.objects[ref]['phased'] = source['phased']
        if not skip:
            for obj in self.objects.values():
                if obj['zone'] == 'battlefield' and obj['player'] == self.current and not obj['phased']:
                    obj['tapped'] = False
        self.receipts(check_handflow=not (changes and self.params['imp']) or self.check_imp_timing)
        self.emit('upkeep_step', player=self.current)
        if changes and self.params['imp']:
            # These two trigger effects resolve with upkeep priority. The real
            # discard selector can be offered by either implementation timing.
            self.imp_upkeep(source['phased'])
        self.receipts()
        self.emit('turn', player=self.current, value=0)

    def next_turn(self, skip=False):
        self.advance()
        self.finish_turn_start(skip)

    def round(self):
        self.next_turn()
        self.next_turn()

    def targetable(self, expected):
        self.emit('can_target_creature', ref='murder', other='source', value=int(expected))

    def countered(self):
        self.cast('source')
        self.emit('announce_spell', ref='source')
        self.cast('counter', response=True)
        self.emit('resolve_response')
        for name in ('source', 'counter'):
            self.objects[name]['zone'] = 'graveyard'
        self.receipts()

    def combat(self, mode):
        defending = mode == 'block_while_out'
        attacker, blocker = ('opposing', 'source') if defending else ('source', 'opposing')
        legal_block = mode in ('flying_reach', 'islandwalk_own_island')
        a = self.objects[attacker]
        self.emit('begin_attackers')
        self.emit('attack', ref=attacker, value=1)
        a['tapped'] = True
        self.emit('finish_attackers')
        self.emit('begin_blockers')
        if blocker in self.objects:
            self.emit('block', ref=blocker, other=attacker, value=int(legal_block))
            if not defending:
                self.observe(blocker, 'blocking_count', legal_block)
        self.emit('advance_combat')
        self.emit('combat_damage')
        self.emit('resolve')
        self.emit('end_combat')
        ap, at = self.stats(attacker)
        if legal_block:
            b = self.objects[blocker]
            bp, bt = self.stats(blocker)
            a['damage'], b['damage'] = bp, ap
            if bp >= at:
                a['zone'] = 'graveyard'
            if ap >= bt:
                b['zone'] = 'graveyard'
        else:
            self.life[1-self.current] -= ap
        self.receipts()


def setup(world, mode):
    owner, foe = world.owner, world.foe
    world.add(world.name, 'source')
    if mode in ('untap_order', 'retained_attachments', 'skip_out', 'skip_in'):
        world.add('Icy Manipulator', 'icy', zone='battlefield')
    if mode in ('retained_attachments', 'foreign_aura'):
        world.add('Holy Strength', 'aura', foe if mode == 'foreign_aura' else owner)
    if mode == 'retained_attachments':
        world.add('Bonesplitter', 'equipment')
        world.add('Burst of Strength', 'burst')
    if mode in ('skip_out', 'skip_in'):
        world.add('Stasis', 'stasis', foe)
        world.add('Naturalize', 'naturalize', foe)
    if mode in ('no_zone_change_triggers', 'target_visibility'):
        world.add('Murder', 'murder', foe)
    if mode == 'no_zone_change_triggers':
        world.add('Grizzly Bears', 'sentinel', zone='battlefield')
        world.add('Soul Warden', 'warden')
        world.add('Dingus Staff', 'staff')
    if mode == 'countered':
        world.add('Counterspell', 'counter', foe)
    if mode in (*BATTLE_MODES[1:], 'block_while_out'):
        world.add('Giant Spider' if mode == 'flying_reach' else FILLER, 'opposing', foe, 'battlefield')
    if mode.startswith('islandwalk_'):
        world.add('Island', 'island', owner if mode == 'islandwalk_own_island' else foe, 'battlefield')
        world.add('Plains', 'plains', foe if mode == 'islandwalk_own_island' else owner, 'battlefield')
    if world.params['imp']:
        if mode != 'imp_empty_hand':
            world.add(FILLER, 'discard')
        if mode == 'imp_choice':
            world.add('Plains', 'keeper')
        for index, name in enumerate(('Forest', 'Mountain', 'Island')):
            world.add(name, f'draw_{index}', zone='library')


def scenario(world, mode):
    setup(world, mode)
    world.emit('turn', player=world.owner, value=0)
    world.emit('present_ai_dialogs', value=1)
    if world.params['imp']:
        world.emit('discard_policy', value=1)
    for player in (0, 1):
        world.pool(player, [0] * 6)
    world.receipts()
    if mode in ('wrong_color', 'insufficient_mana'):
        world.cast('source', denial=mode)
        world.receipts()
        return None
    if mode == 'countered':
        world.countered()
        world.round()
        return None
    if mode == 'no_zone_change_triggers':
        world.resolve('warden')
        world.resolve('staff')
    world.resolve('source')
    if mode == 'no_zone_change_triggers':
        world.life[world.owner] += 1
    if mode == 'retained_attachments':
        world.resolve('aura', target='source')
        world.attachments.append('aura')
        world.resolve('equipment')
        world.pay_tap('equipment')
        world.resolve('burst', 'graveyard', 'source')
        world.counters = 1
    if 'icy' in world.objects:
        world.pay_tap('icy')
    world.receipts()
    if mode == 'target_visibility':
        world.targetable(True)
    if mode == 'foreign_aura':
        world.next_turn()
        world.resolve('aura', target='source')
        world.attachments.append('aura')
        world.receipts()
        world.next_turn()
    elif mode != 'skip_out':
        world.round()
    if mode in ('skip_out', 'skip_in'):
        world.next_turn()
        world.resolve('stasis')
        world.next_turn(skip=True)
        world.resolve('naturalize', 'graveyard', 'stasis')
        world.objects['stasis']['zone'] = 'graveyard'
        world.receipts()
        world.round()
        return None
    if mode == 'target_visibility':
        world.targetable(False)
    if mode == 'block_while_out':
        world.next_turn()
        world.combat(mode)
        return None
    world.next_turn()
    world.advance()
    cut = len(world.steps) if mode == 'replay_phase_in' else None
    world.finish_turn_start()
    if mode == 'target_visibility':
        world.targetable(True)
    if mode == 'imp_timing':
        world.round()
        world.round()
    if mode in BATTLE_MODES:
        world.combat(mode)
    if mode == 'no_zone_change_triggers':
        world.resolve('murder', 'graveyard', 'sentinel')
        world.objects['sentinel']['zone'] = 'graveyard'
        world.life[world.owner] -= 2
        world.receipts()
    return cut


def expand(name, csvid, declaration, params, selected, registry, oracle):
    from recipes import step

    check_variants(params, selected)
    validate_witnesses(oracle, registry)
    if profiles()['sources'][name]['registry']['csvid'] != csvid:
        raise ValueError('phasing source CSVID changed')
    if 'mechanism' in params:
        return expand_effect(name, csvid, declaration, params, selected, registry, oracle)
    cases = []
    for variant in selected:
        mode, owner = variant.rsplit('_', 1)
        world = PhaseWorld(name, params, int(owner), registry, oracle, check_imp_timing=mode == 'imp_timing')
        cut = scenario(world, mode)
        actions = world.steps
        if cut is not None:
            tail = actions[cut:]
            actions = (actions[:cut] + [step('capture')] + tail + [step('record_state'), step('ai_roundtrip'), step('restore')]
                       + copy.deepcopy(tail) + [step('assert_recorded_state')])
        cases.append(dict(name=f'PhaseUntapCreatureRecipe_{csvid}_{variant}', card=name, fixture='empty_main', seed=283,
                          setup=world.setup, steps=actions, _recipe=dict(family=FAMILY, version=2, variant=variant,
                              params=declaration['params'], oracle_text=declaration['oracle_text'], untested_components=list(GAPS))))
    return cases

class EffectWorld(PhaseWorld):
    """External phase effects wait for the recipient's next genuine untap."""

    def __init__(self, name, params, owner, registry, oracle):
        super().__init__(name, params, owner, registry, oracle, check_imp_timing=False)
        self.carried = {}

    def apply_phase(self, refs):
        for ref in refs:
            target = self.objects[ref]
            target.update(phased=True, phase_controller=target['player'])
            for attachment, host in self.carried.items():
                if host == ref:
                    self.objects[attachment].update(phased=True, phase_controller=target['player'])

    def stats(self, ref):
        power, toughness = super().stats(ref)
        if self.carried.get('aura') == ref:
            power, toughness = power + 1, toughness + 2
        return power, toughness

    def finish_turn_start(self, skip=False):
        if skip:
            raise ValueError('the follow-on does not use forced step omission')
        self.emit('untap_step', player=self.current)
        for obj in self.objects.values():
            if obj['zone'] != 'battlefield':
                continue
            if obj['phased'] and obj['phase_controller'] == self.current:
                obj['phased'] = False
            if not obj['phased'] and obj['player'] == self.current:
                obj['tapped'] = False
        self.receipts()
        self.emit('upkeep_step', player=self.current)
        self.emit('turn', player=self.current, value=0)

    def activate_paid(self, source, target, cost):
        self.pool(self.owner, cost)
        self.emit('refresh')
        self.emit('require_can_activate', ref=source, value=1)
        self.emit('choose_target', ref=target, player=self.owner)
        self.emit('activate', ref=source, value=1)
        self.emit('assert_choices_consumed')
        self.emit('assert_stack', value=1)
        self.mana(self.owner, [0] * 6)
        self.objects[source]['tapped'] = True
        self.observe(source, 'tapped', True)
        self.observe(target, 'in_play', True)

    def failed_activation(self, cost):
        self.pool(self.owner, cost)
        self.emit('refresh')
        self.emit('can_activate', ref='source', value=0)
        self.emit('activate', ref='source', value=0)
        self.emit('assert_choices_consumed')
        self.mana(self.owner, cost)
        self.receipts()

    def declare(self, attackers, blocks):
        self.emit('begin_attackers')
        for attacker in attackers:
            self.emit('attack', ref=attacker, value=1)
            self.objects[attacker]['tapped'] = True
        self.emit('finish_attackers')
        self.emit('begin_blockers')
        for blocker, attacker in blocks:
            self.emit('block', ref=blocker, other=attacker, value=1)
            self.observe(blocker, 'blocking_count', 1)

    def actual_damage(self, attackers, blocks):
        self.emit('combat_damage')
        self.emit('resolve')
        blockers = {attacker:blocker for blocker,attacker in blocks}
        for attacker in attackers:
            item = self.objects[attacker]
            if item['phased']:
                continue
            ap, at = self.stats(attacker)
            blocker = blockers.get(attacker)
            if blocker is None:
                self.life[1-self.current] -= ap
                continue
            other = self.objects[blocker]
            bp, bt = self.stats(blocker)
            item['damage'], other['damage'] = bp, ap
            if bp >= at:
                item['zone'] = 'graveyard'
            if ap >= bt:
                other['zone'] = 'graveyard'
        self.receipts()


def setup_ripple(world, mode):
    if mode == 'only_enchantment':
        world.add('Stasis', 'sentinel', world.foe, 'battlefield')
        return
    name = {'opponent_artifact':'Icy Manipulator', 'opponent_land':'Island'}.get(mode, 'Grizzly Bears')
    owner = world.owner if mode in ('own_creature','carry_aura') else world.foe
    world.add(name, 'target', owner, 'battlefield')
    if mode == 'carry_aura':
        world.add('Holy Strength', 'aura')
        world.add('Icy Manipulator', 'icy', zone='battlefield')
    if mode == 'lost_target':
        world.add('Murder', 'answer', world.foe)


def run_ripple(world, mode):
    if mode == 'only_enchantment':
        cost = world.params['mana']
        world.pool(world.owner, cost)
        world.emit('refresh')
        world.emit('can_cast', ref='source', value=0)
        world.emit('cast', ref='source', value=0)
        world.mana(world.owner, cost)
        world.receipts()
        return None
    if mode == 'insufficient_mana':
        world.cast('source', denial=mode)
        world.receipts()
        return None
    if mode == 'offturn':
        world.next_turn()
    if mode == 'carry_aura':
        world.resolve('aura', target='target')
        world.carried['aura'] = 'target'
        world.activate_paid('icy', 'target', [1, 0, 0, 0, 0, 0])
        world.emit('resolve')
        world.objects['target']['tapped'] = True
        world.receipts()
    world.cast('source', target='target')
    world.observe('target', 'in_play', True)
    if mode == 'lost_target':
        world.emit('announce_spell', ref='source')
        world.cast('answer', target='target', response=True)
        world.emit('resolve_response')
        for ref in ('source', 'target', 'answer'):
            world.objects[ref]['zone'] = 'graveyard'
        world.receipts()
        return None
    cut = len(world.steps) if mode == 'pending_replay' else None
    world.emit('resolve')
    world.objects['source']['zone'] = 'graveyard'
    world.apply_phase(('target',))
    world.receipts()
    world.round()
    return cut


def setup_illusionist(world, mode):
    if mode in ('self_target','fresh'):
        return
    player = world.owner if mode == 'own_creature' else world.foe
    world.add('Grizzly Bears','target',player,'battlefield')
    if mode == 'noncreature_filter':
        world.add('Icy Manipulator','artifact',world.foe,'battlefield')


def run_illusionist(world, mode):
    from recipes import COLORS

    world.resolve('source')
    world.receipts()
    cost = [0] * 6
    cost[COLORS['U']] = 2
    if mode == 'fresh':
        world.failed_activation(cost)
        return None
    world.round()
    if mode == 'offturn':
        world.next_turn()
    if mode in ('short_activation','wrong_activation_color'):
        price = [0] * 6
        price[COLORS['U'] if mode == 'short_activation' else 0] = 1 if mode == 'short_activation' else 2
        world.failed_activation(price)
        return None
    target = 'source' if mode == 'self_target' else 'target'
    if mode == 'noncreature_filter':
        world.pool(world.owner, cost)
        world.emit('refresh')
        world.emit('probe_activation_target', ref='source', other='artifact', index=-1, value=0)
    world.activate_paid('source', target, cost)
    cut = len(world.steps) if mode == 'pending_replay' else None
    world.emit('resolve')
    world.apply_phase((target,))
    world.receipts()
    world.round()
    return cut


def setup_dream(world, mode):
    if mode != 'unblocked':
        world.add(FILLER, 'opposing', world.foe, 'battlefield')
    if mode == 'unrelated_fight':
        world.add('Grizzly Bears','ally',zone='battlefield')
    if mode == 'two_blockers':
        world.add('Grizzly Bears','second',world.foe,'battlefield')


def run_dream(world, mode):
    world.resolve('source')
    world.receipts()
    if mode == 'blocks':
        world.next_turn()
        attacker, blocker = 'opposing', 'source'
    else:
        world.round()
        attacker = 'ally' if mode == 'unrelated_fight' else 'source'
        blocker = 'opposing'
    blocks = [] if mode == 'unblocked' else [(blocker,attacker)]
    if mode == 'two_blockers':
        blocks.append(('second',attacker))
    world.declare((attacker,), blocks)
    cut = len(world.steps) if mode == 'combat_replay' else None
    world.emit('advance_combat')
    if mode not in ('unblocked','unrelated_fight'):
        world.apply_phase((attacker, *(b for b,_ in blocks)))
    world.receipts()
    world.actual_damage((attacker,), blocks)
    world.emit('end_combat')
    world.receipts()
    world.round()
    return cut


def setup_veil(world, mode):
    enemy = mode == 'opponent_attack'
    world.add('Grizzly Bears','first',world.foe if enemy else world.owner,'battlefield')
    if mode == 'two_attackers':
        world.add(FILLER,'second',zone='battlefield')
    if mode == 'attacker_dies':
        world.add('Giant Spider','opposing',world.foe,'battlefield')
    if mode == 'veil_removed_pending':
        world.add('Naturalize','answer',world.foe)
    world.add('Icy Manipulator','sentinel',zone='battlefield')


def run_veil(world, mode):
    world.resolve('source')
    world.receipts()
    if mode == 'opponent_attack':
        world.next_turn()
    attackers = () if mode == 'own_absent' else ('first','second') if mode == 'two_attackers' else ('first',)
    blocks = [('opposing','first')] if mode == 'attacker_dies' else []
    if mode == 'veil_removed_pending':
        world.emit('begin_attackers')
        world.emit('attack', ref='first', value=1)
        world.objects['first']['tapped'] = True
        world.emit('finish_attackers')
        world.resolve('answer','graveyard','source')
        world.objects['source']['zone'] = 'graveyard'
        world.emit('begin_blockers')
    else:
        world.declare(attackers, blocks)
    world.emit('advance_combat')
    world.receipts()
    world.actual_damage(attackers, blocks)
    cut = len(world.steps) if mode == 'combat_replay' else None
    world.emit('end_combat')
    if mode != 'opponent_attack':
        world.apply_phase(ref for ref in attackers if world.objects[ref]['zone'] == 'battlefield')
    world.receipts()
    world.round()
    return cut


EFFECT_RUNNERS = {
    'ripple': (setup_ripple, run_ripple),
    'illusionist': (setup_illusionist, run_illusionist),
    'dream': (setup_dream, run_dream),
    'veil': (setup_veil, run_veil),
}


def expand_effect(name, csvid, declaration, params, selected, registry, oracle):
    from recipes import step

    mechanism = params['mechanism']
    prepare, run = EFFECT_RUNNERS[mechanism]
    cases = []
    for variant in selected:
        mode, owner = variant.rsplit('_', 1)
        world = EffectWorld(name, params, int(owner), registry, oracle)
        world.add(name, 'source')
        prepare(world, mode)
        world.emit('turn', player=world.owner, value=0)
        world.emit('present_ai_dialogs', value=1)
        for player in (0, 1):
            world.pool(player, [0] * 6)
        world.receipts()
        cut = run(world, mode)
        actions = world.steps
        if cut is not None:
            tail = actions[cut:]
            actions = (actions[:cut] + [step('capture')] + tail + [step('record_state'), step('ai_roundtrip'), step('restore')]
                       + copy.deepcopy(tail) + [step('assert_recorded_state')])
        case = dict(name=f'PhaseUntapCreatureRecipe_{csvid}_{variant}', card=name, fixture='empty_main', seed=283,
                    setup=world.setup, steps=actions, _recipe=dict(family=FAMILY, version=2, variant=variant,
                    params=declaration['params'], oracle_text=declaration['oracle_text'],
                    untested_components=['responding to an individually pending combat trigger before effects are applied']))
        cases.append(case)
    return cases
