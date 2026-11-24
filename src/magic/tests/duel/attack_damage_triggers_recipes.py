"""Whole-Oracle attack damage, paid equip, and declared attack destinations."""
import json
from collections import Counter
from pathlib import Path

from counted_battlefield_damage_recipes import Game as PaymentBoard, types
from library_search_recipes import wave6_exact as exact_pin


FAMILY = 'attack_damage_triggers'
DATA = Path(__file__).parent / 'data/attack_damage_triggers_wave8_profiles.json'
GRAMMAR = {
    'caltrops': ('Artifact', ('Whenever a creature attacks, this artifact deals 1 damage to it.',), 0),
    'bow': ('Artifact — Equipment', (
        'Whenever equipped creature attacks, this Equipment deals 1 damage to target creature defending player controls.',
        'Equip {1} ({1}: Attach to target creature you control. Equip only as a sorcery.)'), 1),
    'slayer': ('Artifact — Equipment', (
        "Whenever equipped creature attacks, it deals damage equal to its power to the player or planeswalker it's attacking.",
        'Equip {3}'), 3),
    'raid': ('Enchantment', (
        'Whenever a creature you control with power 2 or less attacks, this enchantment deals 1 damage to the player or planeswalker that creature is attacking.',), 0),
    'circle': ('Enchantment', (
        'Whenever a creature without flying attacks you or a planeswalker you control, this enchantment deals 1 damage to that creature.',), 0),
    'canopy': ('Enchantment', ('Whenever a creature with flying attacks you, this enchantment deals 4 damage to it.',), 0),
    'minefield': ('Enchantment', ('Whenever a creature attacks or blocks, this enchantment deals 2 damage to it.',), 0),
    'cavalcade': ('Enchantment', (
        'Whenever a creature you control with power 1 or less attacks, this enchantment deals 1 damage to the player or planeswalker that creature is attacking.',), 0),
    'regisaur': ('Creature — Dinosaur', ('Whenever this creature attacks, it deals 1 damage to any target.',), 0),
    'salamander': ('Creature — Salamander', (
        'Whenever this creature attacks, you may have it deal 1 damage to each creature without flying defending player controls.',), 0),
    'gladiator': ('Creature — Cyclops Warrior', (
        'Whenever this creature attacks, you may have it deal damage equal to its power to target creature defending player controls. If you do, that creature deals damage equal to its power to this creature.',), 0),
    'siege': ('Creature — Dragon', (
        'Flying', 'When this creature enters, destroy all Walls your opponents control.',
        'Whenever this creature attacks, if defending player controls no Walls, it deals 2 damage to each creature without flying that player controls.'), 0),
    'forger': ('Creature — Elemental Shaman', (
        'When this creature enters, put a +1/+1 counter on each other Shaman creature you control.',
        'Whenever a creature you control with a +1/+1 counter on it attacks, you may have that creature deal 1 damage to target player or planeswalker.'), 0),
    'responder': ('Artifact Creature — Golem', (
        "This creature doesn't untap during your untap step.", '{7}: Untap this creature.',
        'Whenever this creature attacks, it deals 7 damage to target creature defending player controls.'), 0),
    'titan': ('Creature — Giant', (
        '{R}: This creature gets +1/+0 until end of turn.',
        'Whenever this creature enters or attacks, it deals 3 damage divided as you choose among one, two, or three targets.'), 0),
    'preyseizer': ('Creature — Dragon', (
        'Flying', 'Devour 2 (As this creature enters, you may sacrifice any number of creatures. It enters with twice that many +1/+1 counters on it.)',
        'Whenever this creature attacks, it deals damage to any target equal to the number of +1/+1 counters on this creature.'), 0),
    'thorncaster': ('Creature — Sliver', (
        'Sliver creatures you control have "Whenever this creature attacks, it deals 1 damage to any target."',), 0),
}
WITNESSES = {'Birds of Paradise', 'Child of Night', 'Dazzling Ramparts', 'Elvish Mystic', 'Galerider Sliver',
             'Giant Growth', 'Goblin Raider', 'Island', 'Jace Beleren', 'Nessian Asp', 'Plains', 'Slippery Bogle',
             'Welkin Tern', 'Sphinx of Jwar Isle', 'Think Twice', 'Wall of Frost', 'Wall of Tanglecord',
             'Anaba Shaman', 'Skinshifter', 'Hunger of the Howlpack', 'Leatherback Baloth'}
CASTS = {'cast', 'short_cast', 'wrong_color', 'fresh_attack'}
EQUIP = ('unequipped', 'short_equip', 'offturn_equip', 'other_attacker', 'lifelink')
MODES = {
    'caltrops': ('lethal', 'multiple', 'own_bystander'),
    'bow': EQUIP + ('lethal', 'no_target', 'walker_attack'),
    'slayer': EQUIP + ('walker_attack', 'higher_power', 'zero_power', 'blocked'),
    'raid': ('walker_attack', 'zero_power', 'above_threshold', 'higher_power', 'multiple', 'blocked'),
    'circle': ('walker_attack', 'filter', 'only_excluded', 'own_attack'),
    'canopy': ('walker_attack', 'filter', 'only_excluded', 'own_attack'),
    'minefield': ('lethal', 'multiple', 'defending_marked', 'both_marked', 'both_lethal'),
    'cavalcade': ('walker_attack', 'zero_power', 'above_threshold', 'higher_power', 'multiple', 'blocked'),
    'regisaur': ('other_attacker', 'own_player', 'own_creature', 'walker_target', 'walker_attack', 'opponent_creature'),
    'salamander': ('walker_attack', 'other_attacker', 'no_ground'),
    'thorncaster': ('peer_sliver', 'non_sliver', 'multiple', 'own_player', 'own_creature', 'walker_target', 'walker_attack'),
}
LIFECYCLES = {
    'gladiator': ('hit', 'return_lethal', 'zero_return', 'own_only', 'opponent_attack', 'replay'),
    'siege': ('hit', 'wall_present', 'own_wall_only', 'ground_block', 'opponent_attack', 'cast_attack', 'replay'),
    'forger': ('cast_attack', 'counted_lifelink', 'counted_source', 'walker_target', 'uncounted', 'opponent_counted', 'replay'),
    'responder': ('hit', 'own_only', 'locked_next_turn', 'paid_reentry', 'short_untap', 'opponent_attack', 'replay'),
    'titan': ('hit', 'split_two', 'walker_target', 'entry_split', 'pump_attack', 'wrong_pump', 'opponent_attack', 'replay'),
    'preyseizer': ('hit', 'devour_one', 'devour_two', 'walker_target', 'ground_block', 'opponent_attack', 'replay'),
}
OPTIONAL_LIFECYCLES = {'gladiator', 'forger'}
LIFECYCLE_CAST = {'cast_attack', 'entry_split', 'devour_one', 'devour_two', 'devour_pass'}
WALKER = {'walker_attack', 'walker_target'}
POWER_CAP = {'raid': 2, 'cavalcade': 1}
DEFENSIVE = {'circle', 'canopy'}
MINEFIELD_BLOCK = {'defending_marked', 'both_marked', 'both_lethal'}


def pins():
    return json.loads(DATA.read_text(encoding='utf8'))


def mechanism(record):
    from recipes import cost_from_oracle

    if (not record or record.get('layout') != 'normal' or record.get('selected_face') or record.get('card_faces')):
        raise ValueError('normal whole Oracle record required')
    clauses = record['oracle_text'].splitlines()
    for kind, (type_line, expected, equip) in GRAMMAR.items():
        if record['type_line'] == type_line and tuple(clauses) == expected:
            break
    else:
        raise ValueError('complete reviewed attack trigger and printed riders required')
    keywords = {'siege': ['Flying'], 'preyseizer': ['Devour', 'Flying']}
    if set(record['keywords']) != set(keywords.get(kind, ['Equip'] if equip else [])):
        raise ValueError('printed keyword ledger changed')
    printed = cost_from_oracle(record)
    if printed[0] <= 0 or any(printed[index] for index in (1, 2)):
        raise ValueError('reviewed generic or red/green cost required')
    return dict(kind=kind, printed=printed, equip=equip, creature='Creature' in types(record), clauses=clauses)


def variants(shape):
    kind = shape['kind']
    if kind in LIFECYCLES:
        modes = ('cast', 'short_cast', 'fresh_attack') + LIFECYCLES[kind]
        if sum(shape['printed'][1:]):
            modes += ('wrong_color',)
        extra = ['decline_0'] if kind in OPTIONAL_LIFECYCLES else ['devour_pass_0'] if kind == 'preyseizer' else []
        return [f'{mode}_{owner}' for mode in modes for owner in ((0,) if mode in ('devour_one', 'devour_two') else (0, 1))] + extra
    modes = ('cast', 'short_cast', 'hit', 'idle', 'opponent_attack', 'replay') + MODES[kind]
    if sum(shape['printed'][1:]):
        modes += ('wrong_color',)
    if shape['creature']:
        modes += ('fresh_attack',)
    return [f'{mode}_{owner}' for mode in modes for owner in (0, 1)] + (['decline_0'] if kind == 'salamander' else [])


def spec(declaration, record):
    shape = mechanism(record)
    source = pins()['sources'].get(record['name'])
    params = declaration.get('params')
    if (not source or not exact_pin(source['oracle'], record) or declaration.get('family') != FAMILY
            or type(declaration.get('version')) is not int or declaration['version'] != 2
            or declaration.get('oracle_text') != record['oracle_text'] or not isinstance(params, dict)
            or set(params) != {'schema', 'mechanism'} or type(params['schema']) is not int or params['schema'] != 1
            or not exact_pin(params['mechanism'], shape)):
        raise ValueError('whole pinned attack source, exact printed payment and clauses required')
    return dict(shape, csvid=source['csvid'], family=FAMILY)


def validate_witnesses(oracle, rows):
    data = pins()
    if set(data) != {'sources', 'witnesses'} or set(data['witnesses']) != WITNESSES:
        raise ValueError('complete attack, attachment, planeswalker and damage witnesses required')
    for group in ('sources', 'witnesses'):
        for name, pin in data[group].items():
            row = rows.get(name, {})
            if (not exact_pin(pin['oracle'], oracle.get(name)) or row.get('csvid') != pin['csvid']
                    or row.get('handler') != pin['handler'] or row.get('kind') != 'card'
                    or row.get('handler') in (None, 'NULL', '0', 'card_dummy') or oracle[name].get('layout') != 'normal'):
                raise ValueError('implemented exact attack source or witness changed: ' + name)
            if group == 'sources':
                mechanism(oracle[name])


class Board(PaymentBoard):
    def __init__(self, name, shape, mode, owner, rows, oracle):
        self.shape, self.mode, self.owner, self.foe = shape, mode, owner, 1-owner
        self.kind, self.equip = shape['kind'], shape['equip']
        self.rows, self.oracle = rows, oracle
        self.cards, self.setup, self.steps = {}, [], []
        self.pools = [[0]*6 for _ in (0, 1)]
        self.life = [20, 20]
        defending = self.kind in DEFENSIVE and mode not in CASTS | {'own_attack', 'idle'}
        self.active = self.foe if defending or mode in ('opponent_attack', 'offturn_equip', 'defending_marked') else owner
        self.attached = False
        self.add(name, 'source', owner, 'hand' if mode in CASTS else 'battlefield')
        for player in (0, 1):
            for index, label in enumerate(('Island', 'Plains')):
                self.add(label, f'library_{player}_{index}', player, 'library')
        if mode not in CASTS:
            if self.kind in DEFENSIVE:
                self.defensive_fixture()
            elif self.kind == 'minefield':
                self.minefield_fixture()
            else:
                self.combat_fixture()

    def defensive_fixture(self):
        ground = self.kind == 'circle'
        if self.mode == 'only_excluded':
            ground = not ground
        if self.mode == 'opponent_attack' and self.kind == 'canopy':
            host = 'Welkin Tern'
        else:
            host = 'Goblin Raider' if ground else 'Sphinx of Jwar Isle'
        self.add(host, 'host', self.active)
        if self.mode == 'filter':
            self.add('Welkin Tern' if ground else 'Goblin Raider', 'peer', self.active)
        if self.mode == 'walker_attack':
            self.add('Jace Beleren', 'walker', 1-self.active)
            self.loyalty = int(self.oracle['Jace Beleren']['loyalty'])

    def minefield_fixture(self):
        self.add('Elvish Mystic' if self.mode == 'lethal' else 'Nessian Asp', 'host', self.active)
        if self.mode == 'multiple':
            self.add('Elvish Mystic', 'peer', self.active)
        if self.mode in MINEFIELD_BLOCK:
            self.add('Elvish Mystic' if self.mode == 'both_lethal' else 'Dazzling Ramparts', 'victim', 1-self.active)
            self.add('Think Twice', 'window_spell', 1-self.active, 'hand')

    def combat_fixture(self):
        mode, kind, owner, foe = self.mode, self.kind, self.owner, self.foe
        if self.equip or kind in ('caltrops', *POWER_CAP) or mode == 'other_attacker':
            host = {'lethal': 'Elvish Mystic', 'lifelink': 'Child of Night', 'zero_power': 'Birds of Paradise',
                    'above_threshold': 'Nessian Asp'}.get(mode, 'Goblin Raider')
            if kind == 'cavalcade' and mode not in ('zero_power', 'above_threshold'):
                host = 'Elvish Mystic'
            if kind == 'cavalcade' and mode == 'above_threshold':
                host = 'Goblin Raider'
            if kind == 'bow' and mode == 'lethal':
                host = 'Goblin Raider'
            self.add(host, 'host', owner)
        if mode == 'opponent_attack':
            hostile = 'Galerider Sliver' if kind == 'thorncaster' else 'Goblin Raider'
            self.add(hostile, 'enemy', foe)
        if mode == 'other_attacker':
            self.add('Goblin Raider', 'ally', owner)
        if mode in ('multiple', 'peer_sliver'):
            peer = {'caltrops': 'Elvish Mystic', 'raid': 'Birds of Paradise', 'cavalcade': 'Birds of Paradise', 'thorncaster': 'Galerider Sliver'}[kind]
            self.add(peer, 'peer', owner)
        if mode in ('own_bystander', 'own_creature', 'non_sliver', 'no_target') or kind == 'salamander':
            self.add('Goblin Raider', 'bystander', owner)
        if mode == 'higher_power':
            self.add('Giant Growth', 'boost_spell', owner, 'hand')
        if mode in WALKER:
            self.add('Jace Beleren', 'walker', foe)
            self.loyalty = int(self.oracle['Jace Beleren']['loyalty'])
        if (kind == 'bow' and mode != 'no_target') or mode in ('blocked', 'opponent_creature'):
            victim = 'Elvish Mystic' if mode == 'lethal' else 'Dazzling Ramparts'
            self.add(victim, 'victim', foe)
        if kind == 'salamander':
            self.add('Welkin Tern', 'flyer', foe)
            if mode != 'no_ground':
                self.add('Dazzling Ramparts', 'ground', foe)
                self.add('Slippery Bogle', 'hexproof', foe)

    def stats(self, ref):
        item = self.cards[ref]
        record = self.oracle[item['name']]
        return int(record['power']) + item['boost'], int(record['toughness']) + item['boost']

    def damage(self, amount, *, ref=None, player=None, source=None):
        if not amount:
            return
        if player is not None:
            self.life[player] -= amount
        elif ref == 'walker':
            self.loyalty = max(0, self.loyalty - amount)
            if not self.loyalty:
                self.cards[ref]['zone'] = 'graveyard'
        else:
            self.cards[ref]['damage'] += amount
            if self.cards[ref]['damage'] >= self.stats(ref)[1]:
                self.cards[ref]['zone'] = 'graveyard'
        if source and self.cards[source]['name'] == 'Child of Night':
            self.life[self.cards[source]['player']] += amount

    def receipts(self):
        self.emit('assert_choices_consumed')
        self.emit('assert_stack', value=0)
        self.observe_mana()
        zones = Counter((item['player'], item['zone'], self.rows[item['name']]['csvid']) for item in self.cards.values())
        for player, life in enumerate(self.life):
            self.emit('assert_life', player=player, value=life)
            for zone in ('battlefield', 'hand', 'graveyard', 'library', 'exile'):
                self.emit('assert_zone', player=player, zone=zone,
                          value=sum(n for (p, z, _), n in zones.items() if player == p and zone == z))
        for (player, zone, index), value in sorted(zones.items()):
            self.emit('assert_zone_multiplicity', player=player, zone=zone, index=index, value=value)
        for ref, item in self.cards.items():
            if item['zone'] != 'battlefield':
                continue
            if ref == 'walker':
                self.emit('assert_counter', ref=ref, counter_kind='loyalty', value=self.loyalty)
            if 'Creature' in types(self.oracle[item['name']]):
                power, toughness = self.stats(ref)
                for prop, value in [('power', power), ('toughness', toughness), ('tapped', int(item['tapped'])),
                                    ('damage', item['damage']), ('plus1_counters', item.get('counters', 0))]:
                    self.emit('assert_card', ref=ref, property=prop, value=value)

    def cast_source(self):
        price = list(self.shape['printed'])
        denied = self.mode in ('short_cast', 'wrong_color')
        if self.mode == 'short_cast':
            price[0] -= 1
        elif self.mode == 'wrong_color':
            price[2], price[3], price[4], price[5] = sum(price[3:]), 0, 0, 0
        self.cast('source', funding=price, allowed=not denied)
        if not denied:
            self.emit('resolve')
            self.emit('refresh')
            self.cards['source']['zone'] = 'battlefield'
            if self.shape['creature']:
                self.emit('assert_card', ref='source', property='sick', value=1)
        self.receipts()

    def equip_source(self):
        short = self.mode == 'short_equip'
        eligible = not short and self.mode != 'offturn_equip'
        self.fund(self.owner, [self.equip - int(short), 0, 0, 0, 0, 0])
        self.emit('can_activate', ref='source', value=int(eligible))
        if eligible:
            self.emit('choose_target', ref='host', player=self.owner)
        self.emit('activate', ref='source', value=int(eligible))
        self.emit('assert_choices_consumed')
        self.emit('assert_stack', value=int(eligible))
        if eligible:
            self.pools[self.owner][0] -= self.equip
            self.emit('resolve')
            self.emit('refresh')
            self.attached = True
        self.receipts()

    def attackers(self):
        if self.mode in ('idle', 'offturn_equip'):
            return []
        if self.kind in DEFENSIVE | {'minefield'}:
            return ['host', 'peer'] if self.mode in ('filter', 'multiple') else ['host']
        if self.mode == 'opponent_attack':
            return ['enemy']
        if self.mode == 'other_attacker':
            return ['ally']
        if self.mode == 'non_sliver':
            return ['bystander']
        if self.mode == 'peer_sliver':
            return ['peer']
        source = 'source' if self.shape['creature'] else 'host'
        return [source, 'peer'] if self.mode == 'multiple' else [source]

    def trigger(self, attackers):
        kind, mode = self.kind, self.mode
        if kind in ('caltrops', 'minefield'):
            for ref in attackers:
                self.damage(1 if kind == 'caltrops' else 2, ref=ref)
        if kind in DEFENSIVE and self.active != self.owner:
            for ref in attackers:
                flying = 'Flying' in self.oracle[self.cards[ref]['name']]['keywords']
                if kind == 'circle' and not flying:
                    self.damage(1, ref=ref)
                if kind == 'canopy' and flying and mode != 'walker_attack':
                    self.damage(4, ref=ref)
        if kind == 'bow' and self.attached and 'host' in attackers and 'victim' in self.cards:
            self.emit('choose_target', ref='victim', player=self.owner)
            self.damage(1, ref='victim', source='source')
        if kind == 'slayer' and self.attached and 'host' in attackers:
            self.damage_defender(self.stats('host')[0], 'host')
        if kind in POWER_CAP and self.active == self.owner:
            for ref in attackers:
                if self.stats(ref)[0] <= POWER_CAP[kind]:
                    self.damage_defender(1, 'source')
        if kind == 'salamander' and 'source' in attackers and mode != 'decline':
            if self.owner == 0:
                self.emit('choose_trigger_source', ref='source', player=0)
            for ref in ('ground', 'hexproof'):
                if ref in self.cards:
                    self.damage(1, ref=ref, source='source')
        if kind in ('thorncaster', 'regisaur') and self.active == self.owner:
            qualifying = ([ref for ref in attackers if 'Sliver' in self.oracle[self.cards[ref]['name']]['type_line'].split()]
                          if kind == 'thorncaster' else [ref for ref in attackers if ref == 'source'])
            target = {'own_creature': 'bystander', 'opponent_creature': 'victim', 'walker_target': 'walker'}.get(mode)
            player = self.owner if mode == 'own_player' else self.foe
            for ref in qualifying:
                if target:
                    self.emit('choose_target', ref=target, player=self.owner)
                    self.damage(1, ref=target, source=ref)
                else:
                    self.emit('choose_player', player=self.owner, value=player)
                    self.damage(1, player=player, source=ref)

    def damage_defender(self, amount, source):
        if self.mode == 'walker_attack':
            if self.cards['walker']['zone'] == 'battlefield':
                self.damage(amount, ref='walker', source=source)
        else:
            self.damage(amount, player=1-self.active, source=source)

    def combat(self):
        mode = self.mode
        declared = self.attackers()
        attackers = declared if mode != 'fresh_attack' and not getattr(self, 'locked_attack', False) else []
        self.emit('begin_attackers')
        for ref in declared:
            if mode in WALKER:
                self.emit('choose_dialog', player=self.active, value=int(mode == 'walker_attack'))
                self.emit('attack_with_tax', ref=ref, value=1)
            else:
                self.emit('attack', ref=ref, value=int(ref in attackers))
            if ref in attackers:
                self.cards[ref]['tapped'] = True
        self.trigger(attackers)
        self.emit('finish_attackers')
        self.receipts()
        self.emit('begin_blockers')
        if mode == 'ground_block':
            self.emit('block', ref='ground', other='source', value=0)
        blocked = mode == 'blocked' or mode in MINEFIELD_BLOCK
        if blocked:
            self.emit('block', ref='victim', other='host', value=1)
        self.emit('advance_combat')
        if blocked:
            self.emit('assert_card', ref='host', property='blocked', value=1)
        if mode in MINEFIELD_BLOCK:
            # This paid instant enters a genuine post-block window where the
            # native spell resolver processes pending declared-blocker damage.
            self.cast('window_spell')
            self.emit('resolve')
            self.emit('refresh')
            self.cards['window_spell']['zone'] = 'graveyard'
            self.draw(1-self.active)
            self.damage(2, ref='victim')
            self.receipts()
        self.emit('combat_damage')
        self.emit('resolve')
        self.emit('end_combat')
        for ref in attackers:
            if self.cards[ref]['zone'] == 'battlefield':
                if blocked:
                    if self.cards['victim']['zone'] == 'battlefield':
                        self.damage(self.stats(ref)[0], ref='victim', source=ref)
                else:
                    self.damage_defender(self.stats(ref)[0], ref)
        self.receipts()


def _case(name, csvid, declaration, shape, variant, rows, oracle):
    from recipes import step

    mode, owner = variant.rsplit('_', 1)
    if shape['kind'] in LIFECYCLES:
        return _lifecycle_case(name, csvid, declaration, shape, variant, int(owner), rows, oracle)
    board = Board(name, shape, mode, int(owner), rows, oracle)
    board.emit('turn', player=board.active, value=0)
    board.emit('reset_death_history')
    for player in (0, 1):
        board.fund(player, [0]*6)
    board.receipts()
    if mode in CASTS:
        board.cast_source()
    if shape['equip'] and mode not in CASTS | {'unequipped', 'opponent_attack'}:
        board.equip_source()
    if mode == 'higher_power':
        board.cast('boost_spell', target='host')
        board.emit('resolve')
        board.emit('refresh')
        board.cards['boost_spell']['zone'] = 'graveyard'
        board.cards['host']['boost'] = 3
        board.receipts()
    replay = len(board.steps) if mode == 'replay' else None
    if mode not in CASTS - {'fresh_attack'}:
        board.combat()
    if replay is not None:
        suffix = board.steps[replay:]
        board.steps = (board.steps[:replay] + [step('capture')] + suffix
                       + [step('record_state'), step('restore'), step('ai_roundtrip')] + suffix + [step('assert_recorded_state')])
    gaps = ['Independent priority, trigger ordering and damage replacement inside the engine declaration callback.',
            'Combat against multiplayer defenders or battles, which the pinned duel inventory cannot supply.']
    if shape['kind'] == 'salamander':
        gaps.append('AI-controlled decline while the production handler automatically resolves for that controller.')
    return dict(name=f'AttackDamageTriggersRecipe_{csvid}_{variant}', card=name, fixture='empty_main', seed=123,
                setup=board.setup, steps=board.steps, _recipe=dict(family=FAMILY, version=2, variant=variant,
                    params=declaration['params'], oracle_text=declaration['oracle_text'],
                    covered_clauses=shape['clauses'], untested_components=gaps))


def expand(name, csvid, declaration, shape, selected, rows, oracle):
    if csvid != shape['csvid'] or not selected or len(set(selected)) != len(selected) or set(selected) - set(variants(shape)):
        raise ValueError('distinct implemented attack damage alternatives required')
    validate_witnesses(oracle, rows)
    return [_case(name, csvid, declaration, shape, variant, rows, oracle) for variant in selected]

class LifecycleBoard(Board):
    def __init__(self, name, shape, mode, owner, rows, oracle):
        self.shape, self.mode, self.owner, self.foe = shape, mode, owner, 1-owner
        self.kind, self.equip = shape['kind'], 0
        self.rows, self.oracle = rows, oracle
        self.cards, self.setup, self.steps = {}, [], []
        self.pools = [[0]*6 for _ in (0, 1)]
        self.life = [20, 20]
        self.prepare_titan_entry = self.kind == 'titan' and mode in LIFECYCLES['titan'] and mode != 'entry_split'
        opponent_starts = mode in ('opponent_attack', 'opponent_counted') and not self.prepare_titan_entry
        self.active = self.foe if opponent_starts else owner
        self.locked_attack = False
        self.attacker = 'source'
        starts_in_hand = mode in CASTS | LIFECYCLE_CAST or self.prepare_titan_entry
        self.add(name, 'source', owner, 'hand' if starts_in_hand else 'battlefield')
        for player in (0, 1):
            for index, label in enumerate(('Island', 'Plains')):
                self.add(label, f'library_{player}_{index}', player, 'library')
        if mode == 'walker_target':
            self.add('Jace Beleren', 'walker', self.foe)
            self.loyalty = int(oracle['Jace Beleren']['loyalty'])
        if mode == 'opponent_attack':
            self.attacker = 'enemy'
            self.add('Goblin Raider', 'enemy', self.foe)
        self.build_witnesses()

    def build_witnesses(self):
        mode, owner, foe = self.mode, self.owner, self.foe
        if mode in ('short_cast', 'wrong_color'):
            return
        if self.kind == 'gladiator':
            if mode in ('own_only', 'hit', 'return_lethal', 'zero_return', 'decline', 'replay'):
                target = 'Nessian Asp' if mode == 'return_lethal' else 'Dazzling Ramparts' if mode == 'zero_return' else 'Elvish Mystic'
                self.add(target, 'victim', owner if mode == 'own_only' else foe)
        elif self.kind == 'siege':
            self.add('Elvish Mystic', 'fragile', foe)
            self.add('Leatherback Baloth', 'ground', foe)
            self.add('Welkin Tern', 'flyer', foe)
            self.add('Goblin Raider', 'friendly', owner)
            if mode in ('cast', 'fresh_attack', 'cast_attack', 'own_wall_only'):
                self.add('Wall of Frost', 'own_wall', owner)
            if mode in ('cast', 'fresh_attack', 'cast_attack', 'wall_present'):
                self.add('Wall of Frost', 'wall', foe)
                self.add('Wall of Tanglecord', 'other_wall', foe)
        elif self.kind == 'forger':
            self.forger_witnesses()
        elif self.kind == 'responder':
            if mode not in CASTS:
                who = owner if mode == 'own_only' else foe
                self.add('Nessian Asp', 'victim', who)
                if mode == 'paid_reentry':
                    self.add('Dazzling Ramparts', 'second', foe)
        elif self.kind == 'titan':
            if mode in ('walker_target', 'entry_split', 'split_two'):
                self.add('Dazzling Ramparts', 'victim', foe)
            if mode == 'entry_split':
                self.add('Jace Beleren', 'walker', foe)
                self.loyalty = int(self.oracle['Jace Beleren']['loyalty'])
        else:
            if mode == 'ground_block':
                self.add('Dazzling Ramparts', 'ground', foe)
            if mode in ('devour_one', 'devour_two', 'devour_pass'):
                self.add('Goblin Raider', 'first_meal', owner)
                if mode != 'devour_pass':
                    self.add('Elvish Mystic', 'second_meal', owner)
                self.add('Goblin Raider', 'foreign_creature', foe)
            if mode in ('walker_target', 'replay'):
                self.add('Hunger of the Howlpack', 'counter_spell', owner, 'hand')

    def forger_witnesses(self):
        mode, owner, foe = self.mode, self.owner, self.foe
        if mode in ('cast', 'fresh_attack', 'cast_attack'):
            self.add('Anaba Shaman', 'shaman', owner)
            self.add('Skinshifter', 'second_shaman', owner)
            self.add('Anaba Shaman', 'foreign_shaman', foe)
            self.add('Goblin Raider', 'ordinary', owner)
            if mode == 'cast_attack':
                self.attacker = 'shaman'
            return
        if mode == 'counted_source':
            self.add('Hunger of the Howlpack', 'counter_spell', owner, 'hand')
            return
        self.attacker = 'ally'
        actor = foe if mode == 'opponent_counted' else owner
        self.add('Child of Night' if mode == 'counted_lifelink' else 'Goblin Raider', 'ally', actor)
        if mode != 'uncounted':
            self.add('Hunger of the Howlpack', 'counter_spell', actor, 'hand')

    def stats(self, ref):
        power, toughness = super().stats(ref)
        item = self.cards[ref]
        counters = item.get('counters', 0)
        return power + counters + item.get('temporary_power', 0), toughness + counters

    def target(self, ref, amount, source='source'):
        if ref in ('owner', 'opponent'):
            player = self.owner if ref == 'owner' else self.foe
            self.emit('choose_player', player=self.owner, value=player)
            self.damage(amount, player=player, source=source)
        else:
            self.emit('choose_target', player=self.owner, ref=ref)
            self.damage(amount, ref=ref, source=source)

    def titan_damage(self, *, prepare_entry=False):
        if prepare_entry:
            assignments = ('owner',) * 3
        elif self.mode in ('walker_target', 'entry_split'):
            assignments = ('opponent', 'victim', 'walker')
        elif self.mode == 'split_two':
            assignments = ('opponent', 'opponent', 'victim')
        else:
            assignments = ('opponent',) * 3
        for ref in assignments:
            self.target(ref, 1)

    def meals(self):
        mode = self.mode
        if mode not in ('devour_one', 'devour_two', 'devour_pass'):
            return
        chosen = () if mode == 'devour_pass' else ('first_meal', 'second_meal') if mode == 'devour_two' else ('first_meal',)
        for ref in chosen:
            self.emit('choose_target', player=self.owner, ref=ref)
            self.cards[ref]['zone'] = 'graveyard'
        if len(chosen) < 2:
            self.emit('choose_cancel_target', player=self.owner)
        self.cards['source']['counters'] = len(chosen) * 2

    def cast_source(self):
        if self.mode in ('short_cast', 'wrong_color'):
            return super().cast_source()
        self.cast('source')
        if self.kind == 'titan':
            self.titan_damage(prepare_entry=self.prepare_titan_entry)
        elif self.kind == 'preyseizer':
            self.meals()
        self.emit('resolve')
        self.emit('refresh')
        self.cards['source']['zone'] = 'battlefield'
        self.emit('assert_card', ref='source', property='sick', value=1)
        if self.kind == 'siege':
            for ref in ('wall', 'other_wall'):
                self.cards[ref]['zone'] = 'graveyard'
        elif self.kind == 'forger':
            for ref in ('shaman', 'second_shaman'):
                self.cards[ref]['counters'] = 1
        self.receipts()

    def prepare_combat(self):
        if self.prepare_titan_entry:
            if self.mode == 'opponent_attack':
                self.emit('advance_turn')
                self.active = self.foe
                self.emit('untap_step', player=self.active)
                self.emit('turn', player=self.active, value=0)
                self.receipts()
            else:
                self.next_own_turn()
        if 'counter_spell' in self.cards:
            ref = self.attacker
            self.cast('counter_spell', target=ref)
            self.emit('resolve')
            self.emit('refresh')
            self.cards['counter_spell']['zone'] = 'graveyard'
            self.cards[ref]['counters'] = 1
            self.receipts()
        if self.mode in ('pump_attack', 'wrong_pump'):
            self.paid_source_ability('pump')
        if self.kind == 'preyseizer' and self.mode in ('devour_one', 'devour_two') or (
                self.kind == 'siege' and self.mode == 'cast_attack'):
            self.next_own_turn()

    def next_own_turn(self):
        self.emit('advance_turn')
        self.emit('advance_turn')
        self.emit('untap_step', player=self.owner)
        self.emit('turn', player=self.owner, value=0)
        for item in self.cards.values():
            item['damage'] = 0
            item['temporary_power'] = 0
        self.cards['source']['tapped'] = self.kind == 'responder'
        self.receipts()

    def paid_source_ability(self, ability):
        pump = ability == 'pump'
        allowed = self.mode not in ('wrong_pump', 'short_untap')
        price = [0, 0, 0, 0, 1, 0] if pump else [7, 0, 0, 0, 0, 0]
        payment = price if allowed else [0, 0, 1, 0, 0, 0] if pump else [6, 0, 0, 0, 0, 0]
        self.fund(self.owner, payment)
        self.emit('can_activate', ref='source', value=int(allowed))
        self.emit('activate', ref='source', value=int(allowed))
        self.emit('assert_stack', value=int(allowed))
        if allowed:
            self.pools[self.owner] = [0]*6
            self.emit('resolve')
            self.emit('refresh')
            if pump:
                self.cards['source']['temporary_power'] = 1
            else:
                self.cards['source']['tapped'] = False
        self.receipts()

    def attackers(self):
        return [self.attacker]

    def allow_optional(self):
        if self.mode == 'decline':
            return False
        if self.owner == 0:
            self.emit('choose_trigger_source', ref='source', player=0)
        return True

    def trigger(self, attackers):
        kind, mode = self.kind, self.mode
        if kind == 'forger':
            if self.active != self.owner:
                return
            for ref in attackers:
                if self.cards[ref].get('counters', 0) and self.allow_optional():
                    self.target('walker' if mode == 'walker_target' else 'opponent', 1, source=ref)
            return
        if 'source' not in attackers:
            return
        if kind == 'gladiator':
            if not self.allow_optional() or mode == 'own_only':
                return
            amount = self.stats('victim')[0]
            self.target('victim', self.stats('source')[0])
            self.damage(amount, ref='source', source='victim')
        elif kind == 'siege':
            wall_present = any(self.cards[ref]['zone'] == 'battlefield' for ref in ('wall', 'other_wall') if ref in self.cards)
            if not wall_present:
                for ref in ('ground', 'fragile'):
                    if self.cards[ref]['zone'] == 'battlefield':
                        self.damage(2, ref=ref, source='source')
        elif kind == 'responder':
            eligible = [ref for ref in ('victim', 'second')
                          if ref in self.cards and self.cards[ref]['zone'] == 'battlefield'
                          and self.cards[ref]['player'] == self.foe]
            if eligible:
                self.target(eligible[0], 7)
        elif kind == 'titan':
            self.titan_damage()
        else:
            target = 'walker' if mode == 'walker_target' else 'opponent'
            self.target(target, self.cards['source'].get('counters', 0))


def _lifecycle_case(name, csvid, declaration, shape, variant, owner, rows, oracle):
    from recipes import step

    mode = variant.rsplit('_', 1)[0]
    board = LifecycleBoard(name, shape, mode, owner, rows, oracle)
    board.emit('turn', player=board.active, value=0)
    board.emit('reset_death_history')
    for player in (0, 1):
        board.fund(player, [0]*6)
    board.receipts()
    if mode in CASTS | LIFECYCLE_CAST or board.prepare_titan_entry:
        board.cast_source()
    if mode not in {'cast', 'short_cast', 'wrong_color', 'entry_split', 'devour_pass'}:
        board.prepare_combat()
        replay = len(board.steps) if mode == 'replay' else None
        board.combat()
        if mode in ('locked_next_turn', 'paid_reentry', 'short_untap'):
            board.next_own_turn()
            if mode != 'locked_next_turn':
                board.paid_source_ability('untap')
            board.locked_attack = mode != 'paid_reentry'
            board.combat()
        if mode == 'pump_attack':
            board.next_own_turn()
        if replay is not None:
            suffix = board.steps[replay:]
            board.steps = (board.steps[:replay] + [step('capture')] + suffix
                           + [step('record_state'), step('restore'), step('ai_roundtrip')] + suffix + [step('assert_recorded_state')])
    gaps = ['Independent priority or damage replacement inside the native callback; multiplayer and battle recipients.']
    if shape['kind'] in OPTIONAL_LIFECYCLES:
        gaps.append('AI decline: this handler automatically resolves for that controller.')
    if shape['kind'] == 'preyseizer':
        gaps.append('Devour with eligible AI-controlled fodder: the automatic sacrifice path bypasses the target write that the Devour caller consumes. Human selection, subset selection, full selection and decline use the real selector.')
    return dict(name=f'AttackDamageTriggersRecipe_{csvid}_{variant}', card=name, fixture='empty_main', seed=123,
                setup=board.setup, steps=board.steps, _recipe=dict(family=FAMILY, version=2, variant=variant,
                    params=declaration['params'], oracle_text=declaration['oracle_text'],
                    covered_clauses=shape['clauses'], untested_components=gaps))
