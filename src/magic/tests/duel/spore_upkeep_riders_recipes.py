"""Exercise real upkeep and each printed spending ability of a spore permanent."""

import copy
import json
from functools import lru_cache
from pathlib import Path

from attack_value_trigger_recipes import _AttackResourceWorld

FAMILY = 'spore_upkeep_riders'
PROFILE = Path(__file__).with_name('data') / 'spore_upkeep_riders_wave8_profiles.json'
UPKEEP = 'At the beginning of your upkeep, put a spore counter on this creature.'
COST = 'Remove three spore counters from this creature: '
MAKE = COST + 'Create a 1/1 green Saproling creature token.'
FOG = COST + 'Prevent all combat damage that would be dealt this turn.'
SACRIFICE = 'Sacrifice a Saproling: Target creature '
RIDERS = {
    'weaken': SACRIFICE + 'gets -1/-1 until end of turn.',
    'strengthen': SACRIFICE + 'gets +1/+1 until end of turn.',
    'haste': SACRIFICE + 'gains haste until end of turn.',
}
MENUS = {'weaken': 'Sac & give -1/-1', 'strengthen': 'Sac & give +1/+1', 'haste': 'Give Haste'}
HANDLERS = {
    'plain': 'manalinkeh_card_thallid',
    'defender': 'manalinkeh_card_thallid',
    'weaken': 'manalinkeh_card_deathspore_thallid',
    'strengthen': 'manalinkeh_card_thallid_germinator',
    'haste': 'manalinkeh_card_vitaspore_thallid',
    'fog': 'manalinkeh_card_spore_flower',
}
TOKEN = 'Saproling'
BEAR = 'Grizzly Bears'
GIANT = 'Hill Giant'
WURM = 'Scaled Wurm'
GOBLIN = "Mons's Goblin Raiders"
BOLT = 'Lightning Bolt'
VANILLA = {BEAR, GIANT, WURM, GOBLIN}
WITNESSES = {TOKEN, BOLT, *VANILLA}
EXTRAS = {
    'plain': (), 'defender': ('defender',),
    'weaken': ('lethal', 'enemy_expiry', 'ally_effect'),
    'strengthen': ('enemy_expiry', 'ally_effect'),
    'haste': ('fresh_haste', 'haste_expiry'),
    'fog': ('fog_combat', 'fog_expired', 'fog_enemy_turn'),
}
GAPS = ('Spore counts use a real activation threshold and repeated spending. '
        'The duel driver has no direct named spore counter observer.',)


@lru_cache(maxsize=1)
def profiles():
    return json.loads(PROFILE.read_text(encoding='utf8'))


def classify(record):
    from recipes import cost_from_oracle

    if (not record or record.get('layout') != 'normal' or record.get('card_faces')
            or record.get('selected_face') or 'Fungus' not in record.get('type_line', '').split(' — ')[-1].split()
            or not record['type_line'].startswith('Creature — ')):
        raise ValueError('complete normal Fungus creature required')
    lines = tuple(record['oracle_text'].splitlines())
    complete = {(UPKEEP, MAKE): 'plain', ('Defender', UPKEEP, MAKE): 'defender', (UPKEEP, FOG): 'fog'}
    complete.update({(UPKEEP, MAKE, text): key for key, text in RIDERS.items()})
    shape = complete.get(lines)
    keywords = ['Defender'] if shape == 'defender' else []
    if shape is None or record['keywords'] != keywords:
        raise ValueError('whole supported spore Oracle required')
    return dict(shape=shape, keywords=keywords, mana=cost_from_oracle(record),
                power=int(record['power']), toughness=int(record['toughness']))


def variants(params):
    return [f'{mode}_{p}' for p in (0, 1) for mode in ('threshold', 'bank_four', 'snapshot') + EXTRAS[params['shape']]]


def check_variants(params, selected):
    if (type(selected) is not list or not selected or any(type(v) is not str for v in selected)
            or len(set(selected)) != len(selected) or set(selected) - set(variants(params))):
        raise ValueError('distinct reviewed spore rider variants required')


def spec(declaration, record):
    params = classify(record)
    if (declaration.get('family') != FAMILY or type(declaration.get('version')) is not int or declaration['version'] != 2
            or profiles()['sources'].get(record['name'], {}).get('oracle') != record
            or declaration.get('oracle_text') != record['oracle_text']
            or json.dumps(declaration.get('params'), sort_keys=True) != json.dumps(params, sort_keys=True)):
        raise ValueError('typed spore declaration must equal whole pinned Oracle')
    check_variants(params, declaration.get('variants'))
    return dict(params, family=FAMILY)


def validate_witnesses(oracle, rows):
    data = profiles()
    skipped = {r['csvid'] for r in json.loads((PROFILE.parent / 'unimplemented.json').read_text(
        encoding='utf8'))['cases'].values()}
    if data.get('version') != 1 or not data.get('sources') or set(data.get('witnesses', ())) != WITNESSES:
        raise ValueError('exact reviewed spore rider profile required')
    for group in ('sources', 'witnesses'):
        for name, pin in data[group].items():
            row, record = rows.get(name, {}), oracle.get(name, {})
            layout = 'token' if name == TOKEN else 'normal'
            if (pin != dict(oracle=record, registry=row) or row.get('kind') != 'card' or record.get('layout') != layout
                    or row['csvid'] in skipped or (row['handler'] in (None, 'NULL', '0', 'card_dummy') and name not in VANILLA)):
                raise ValueError(f'exact implemented spore fixture required: {name}')
            if group == 'sources' and row['handler'] != HANDLERS[classify(record)['shape']]:
                raise ValueError(f'changed spore production handler: {name}')
    token = oracle[TOKEN]
    if (rows[TOKEN]['csvid'], token['type_line'], token['power'], token['toughness'], token['colors']) != (
            18813, 'Token Creature — Saproling', '1', '1', ['G']):
        raise ValueError('exact printed Saproling token required')


class SporeWorld(_AttackResourceWorld):
    def __init__(self, name, params, mode, owner, rows, oracle):
        super().__init__(name, dict(params, entry_energy=0, effect='spore'), owner, rows, oracle)
        self.foe = 1 - owner
        self.mode = mode
        self.shape = params['shape']
        self.spores = 0
        self.created = 0
        self.fog = False
        self.add(name, 'source', owner, 'hand', source=True)
        if mode in ('defender', 'ally_effect') or (mode == 'bank_four' and self.shape in RIDERS):
            self.add(BEAR, 'ally', owner)
        if mode in ('enemy_expiry', 'ally_effect'):
            self.add(GIANT, 'enemy', self.foe)
        if mode == 'lethal':
            self.add(GOBLIN, 'enemy', self.foe)
        if mode in ('fresh_haste', 'haste_expiry'):
            self.add(BEAR, 'fresh', owner, 'hand')
            self.add(GOBLIN, 'fresh_control', owner, 'hand')
        if mode.startswith('fog_'):
            active = self.foe if mode == 'fog_enemy_turn' else owner
            self.add(GIANT, 'attacker', active)
            self.add(BEAR, 'unblocked', active)
            self.add(WURM, 'blocker', 1 - active)
            if mode == 'fog_combat':
                self.add(BOLT, 'bolt', owner, 'hand')

    def observe(self, ref, prop, value):
        self.emit('assert_card', ref=ref, property=prop, value=value)

    def zone_count(self, name, player, zone, value):
        self.emit('assert_zone_multiplicity', player=player, zone=zone, index=self.rows[name]['csvid'], value=value)

    def zero_pool(self):
        for p in (0, 1):
            for color in range(6):
                self.emit('assert_mana', player=p, color=color, value=0)

    def receipts(self):
        start = len(self.steps)
        super().receipts()
        # A haste effect can temporarily clear raw sickness bits. The actual
        # fresh attack and cleanup denial are the rules observations instead.
        self.steps[start:] = [step for step in self.steps[start:]
                              if (step.get('ref'), step.get('property')) != ('fresh', 'sick')]
        for p in (0, 1):
            for zone in ('battlefield', 'graveyard', 'exile'):
                self.zone_count(TOKEN, p, zone, self.zones[zone][p].count(TOKEN))
        if TOKEN in self.zones['battlefield'][self.owner]:
            for prop, value in (('token', 1), ('artifact', 0), ('power', 1), ('toughness', 1),
                                ('color', 8), ('subtype_Saproling', 1)):
                self.emit('assert_matching_permanents', player=self.owner, index=self.rows[TOKEN]['csvid'],
                          property=prop, value=value)

    def availability(self):
        saproling_cost = self.shape in RIDERS and TOKEN in self.zones['battlefield'][self.owner]
        self.emit('can_activate', ref='source', value=int(self.spores >= 3 or saproling_cost))

    def upkeep_turn(self):
        self.emit('advance_turn')
        self.expire()
        self.fog = False
        self.current = 1 - self.current
        self.emit('untap_step', player=self.current)
        for creature in self.entities.values():
            if creature['zone'] == 'battlefield' and creature['player'] == self.current:
                creature.update(tapped=False, sick=False)
        self.availability()
        self.emit('upkeep_step', player=self.current)
        self.spores += int(self.current == self.owner)
        self.emit('turn', player=self.current, value=0)
        self.emit('assert_stack', value=0)
        self.emit('assert_unused_choices', value=0)
        self.availability()

    def own_upkeeps(self, count):
        for _ in range(count * 2):
            self.upkeep_turn()

    def mode_intent(self, label):
        self.emit('choose_activation_mode', ref='source', player=self.owner, mode_label=label,
                  fallback_color=0, allow_auto_nonmana=False)

    def primary(self):
        if self.spores < 3 or (self.shape in RIDERS and TOKEN in self.zones['battlefield'][self.owner]):
            raise ValueError('threshold requires exact legal spore ability without a competing rider')
        if self.shape in RIDERS:
            self.mode_intent('Generate a Saproling')
        self.emit('activate', ref='source', value=1)
        if self.shape in RIDERS:
            self.emit('require_activation_mode', value=1)
        self.spores -= 3
        self.emit('assert_stack', value=1)
        self.emit('assert_choices_consumed')
        self.zero_pool()
        self.observe('source', 'tapped', 0)
        count = self.zones['battlefield'][self.owner].count(TOKEN)
        self.zone_count(TOKEN, self.owner, 'battlefield', count)
        self.availability()
        self.emit('resolve')
        if self.shape == 'fog':
            self.fog = True
        else:
            ref = f'saproling_{self.created}'
            self.created += 1
            self.entity(TOKEN, ref, self.owner, 'battlefield', token=True)
            self.zones['battlefield'][self.owner].append(TOKEN)
            self.emit('bind_permanent_copy' if count else 'bind_permanent', player=self.owner,
                      value=self.rows[TOKEN]['csvid'], ref=ref, **({'index': count} if count else {}))
        self.availability()
        self.receipts()

    def rider(self, target):
        if self.shape not in RIDERS or self.spores >= 3 or self.zones['battlefield'][self.owner].count(TOKEN) != 1:
            raise ValueError('rider requires exactly one Saproling and no competing spore ability')
        ref = f'saproling_{self.created - 1}'
        recipient = self.entities[target]
        self.mode_intent(MENUS[self.shape])
        self.emit('choose_target', player=self.owner, ref=target)
        if self.owner == 0:
            self.emit('choose_target', player=self.owner, ref=ref)
        self.emit('activate', ref='source', value=1)
        self.emit('require_activation_mode', value=1)
        self.leave(ref, 'graveyard')
        self.emit('assert_stack', value=1)
        self.emit('assert_choices_consumed')
        self.zero_pool()
        self.zone_count(TOKEN, self.owner, 'battlefield', 0)
        self.zone_count(TOKEN, self.owner, 'graveyard', 0)
        self.observe(target, 'power', recipient['power'])
        self.observe(target, 'toughness', recipient['toughness'])
        self.availability()
        self.emit('resolve')
        if self.shape == 'haste':
            recipient['grants'].add('Haste')
        else:
            delta = -1 if self.shape == 'weaken' else 1
            recipient['power'] += delta
            recipient['toughness'] += delta
            if recipient['toughness'] <= 0:
                self.leave(target, 'graveyard')
        self.availability()
        self.receipts()

    def expire(self):
        super().expire()
        for creature in self.entities.values():
            printed = self.o[creature['name']]
            creature.update(power=int(printed['power']), toughness=int(printed['toughness']))

    def cast_fresh(self, ref):
        from recipes import cost_from_oracle

        self.pool(self.owner, cost_from_oracle(self.o[self.entities[ref]['name']]))
        self.emit('cast', ref=ref, value=1)
        self.emit('assert_stack', value=1)
        self.emit('resolve')
        self.mana[self.owner] = [0] * 6
        self.arrive(ref)

    def attack(self, ref, allowed):
        self.emit('attack', ref=ref, value=int(allowed))
        if allowed:
            self.entities[ref].update(tapped=True, attacking=True)

    def duel(self, attackers, rejected=(), blocker=None):
        self.emit('begin_attackers')
        for ref in rejected:
            self.attack(ref, False)
        for ref in attackers:
            self.attack(ref, True)
        self.emit('finish_attackers')
        self.emit('begin_blockers')
        if blocker:
            self.emit('block', ref=blocker, other=attackers[0], value=1)
            self.observe(blocker, 'blocking_count', 1)
        self.emit('advance_combat')
        self.emit('combat_damage')
        self.emit('resolve')
        self.emit('end_combat')
        for index, ref in enumerate(attackers):
            entity = self.entities[ref]
            entity['attacking'] = False
            if self.fog:
                continue
            if blocker and index == 0:
                enemy = self.entities[blocker]
                enemy['damage'] += entity['power']
                entity['damage'] += enemy['power']
                if entity['damage'] >= entity['toughness']:
                    self.leave(ref, 'graveyard')
            else:
                self.life[1 - self.current] -= entity['power']
        self.receipts()

    def bolt(self):
        from recipes import cost_from_oracle

        self.pool(self.owner, cost_from_oracle(self.o[BOLT]))
        self.emit('choose_player', player=self.owner, value=self.foe)
        self.emit('cast', ref='bolt', value=1)
        self.emit('assert_stack', value=1)
        self.emit('resolve')
        self.mana[self.owner] = [0] * 6
        self.zones['hand'][self.owner].remove(BOLT)
        self.zones['graveyard'][self.owner].append(BOLT)
        self.life[self.foe] -= 3
        self.receipts()


def scenario(world):
    mode = world.mode
    world.emit('turn', player=world.owner, value=0)
    world.pool(world.foe, [0] * 6)
    world.cast_source('source')
    world.availability()
    world.emit('activate', ref='source', value=0)
    world.own_upkeeps(4 if mode == 'bank_four' else 3)
    if mode == 'fog_enemy_turn':
        world.upkeep_turn()
    cut = len(world.steps) if mode == 'snapshot' else None
    world.primary()
    if mode == 'bank_four':
        if world.shape in RIDERS:
            world.rider('ally')
        world.own_upkeeps(2)
        world.primary()
    elif mode == 'defender':
        world.duel(('ally',), rejected=('source',))
    elif mode in ('lethal', 'enemy_expiry', 'ally_effect'):
        world.rider('ally' if mode == 'ally_effect' else 'enemy')
        if mode == 'enemy_expiry':
            world.upkeep_turn()
            world.receipts()
    elif mode in ('fresh_haste', 'haste_expiry'):
        world.cast_fresh('fresh')
        world.cast_fresh('fresh_control')
        world.rider('fresh')
        if mode == 'fresh_haste':
            world.duel(('fresh',), rejected=('fresh_control',))
        else:
            # A cleanup step ends the effect without aging either real cast.
            world.emit('cleanup')
            world.expire()
            world.emit('turn', player=world.owner, value=0)
            world.duel((), rejected=('fresh', 'fresh_control'))
    elif mode.startswith('fog_'):
        if mode == 'fog_combat':
            world.bolt()
        if mode == 'fog_expired':
            world.own_upkeeps(1)
        world.duel(('attacker', 'unblocked'), blocker='blocker')
    return cut


def expand(name, csvid, declaration, params, selected, rows, oracle):
    from recipes import step

    check_variants(params, selected)
    validate_witnesses(oracle, rows)
    if profiles()['sources'][name]['registry']['csvid'] != csvid:
        raise ValueError('exact spore source CSVID required')
    cases = []
    for variant in selected:
        mode, owner = variant.rsplit('_', 1)
        world = SporeWorld(name, params, mode, int(owner), rows, oracle)
        cut = scenario(world)
        steps = world.steps
        if cut is not None:
            tail = steps[cut:]
            steps = (steps[:cut] + [step('capture')] + tail
                     + [step('record_state'), step('ai_roundtrip'), step('restore')]
                     + copy.deepcopy(tail) + [step('assert_recorded_state')])
        cases.append(dict(name=f'SporeUpkeepRidersRecipe_{csvid}_{variant}', card=name, fixture='empty_main',
                          seed=197, setup=world.setup, steps=steps,
                          _recipe=dict(family=FAMILY, version=2, variant=variant, params=declaration['params'],
                                       oracle_text=declaration['oracle_text'], untested_components=list(GAPS))))
    return cases
