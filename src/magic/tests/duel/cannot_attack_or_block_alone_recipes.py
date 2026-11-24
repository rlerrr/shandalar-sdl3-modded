"""Combat declarations with a genuine, independently legal companion."""

import copy
import hashlib
import json
from pathlib import Path
import re

from bestow_stat_aura_recipes import GREEDY_COST, REMINDER, Scene
from block_stat_trigger_recipes import BlockStatBoard
from combat_value_trigger_recipes import _mirror
from grant_recipes import LIBRARY

FAMILY = 'cannot_attack_or_block_alone'
PROFILE_PATH = Path(__file__).with_name('data') / 'cannot_attack_or_block_alone_wave8_profiles.json'
CLAUSE = "This creature can't attack or block alone."
ATTACK_GROUP = ('attack_with_ally', 'attack_remove_ally_after', 'attack_tapped_source', 'attack_paid_fresh',
                'attack_paid_next_turn', 'attack_replay', 'attack_ground_block', 'attack_reach_block')
BLOCK_GROUP = ('block_with_ally_different', 'block_with_ally_same', 'block_remove_ally_after', 'block_tapped_source',
               'block_paid_fresh', 'block_replay', 'block_air_with_ally')
PAID = ('attack_paid_fresh', 'attack_paid_next_turn', 'block_paid_fresh', 'unfunded', 'wrong_color')
REMOVE = ('attack_remove_ally_before', 'attack_remove_ally_after', 'block_remove_ally_before', 'block_remove_ally_after')
COMMON = ('attack_solo', 'attack_idle_ally', 'attack_tapped_ally', 'attack_sick_ally', 'attack_defender_ally',
          'attack_foreign_ally', 'attack_remove_ally_before', 'block_solo', 'block_idle_ally', 'block_tapped_ally',
          'block_cannot_block_ally', 'block_wrong_air_ally', 'block_remove_ally_before', 'unfunded', 'wrong_color')
FLYING_OWN = ('attack_ground_block', 'attack_reach_block')


def profiles():
    return json.loads(PROFILE_PATH.read_text(encoding='utf8'))


def parameters(record):
    if record.get('keywords') == ['Bestow']:
        return brawler_parameters(record)
    flying = record.get('keywords') == ['Flying']
    expected = ['Flying', CLAUSE] if flying else [CLAUSE]
    if (record.get('layout') != 'normal' or record.get('card_faces') or record.get('selected_face')
            or not record.get('type_line', '').startswith('Creature — ')
            or record.get('keywords') not in ([], ['Flying']) or record.get('oracle_text', '').splitlines() != expected
            or not re.fullmatch(r'(?:\{[12]\})?\{[RW]\}', record.get('mana_cost', ''))
            or any(not re.fullmatch(r'[1-9]', record.get(key, '')) for key in ('power', 'toughness'))):
        raise ValueError('complete reviewed standalone attack/block-alone clause required')
    return dict(flying=flying, power=int(record['power']), toughness=int(record['toughness']), printed_mana=record['mana_cost'])


def variants(params):
    if 'bestow_cost' in params:
        return [f'{mode}_{player}' for player in (0, 1) for mode in BRAWLER_MODES]
    modes = COMMON + tuple(mode for mode in ATTACK_GROUP if params['flying'] or mode not in FLYING_OWN) + BLOCK_GROUP
    return [f'{mode}_{player}' for player in (0, 1) for mode in modes]


def validate_variants(params, selected):
    if (not isinstance(selected, list) or not selected or any(type(v) is not str for v in selected)
            or len(set(selected)) != len(selected) or not set(selected) <= set(variants(params))):
        raise ValueError('distinct supported attack/block-alone variants required')


def spec(declaration, record):
    params = parameters(record)
    pin = profiles()['sources'].get(record['name'])
    if (declaration.get('family') != FAMILY or type(declaration.get('version')) is not int or declaration['version'] != 2
            or not pin or pin['oracle'] != record or pin['params'] != params or declaration.get('oracle_text') != record['oracle_text']
            or json.dumps(declaration.get('params'), sort_keys=True) != json.dumps(params, sort_keys=True)):
        raise ValueError('complete pinned Oracle and typed attack/block-alone parameters required')
    validate_variants(params, declaration.get('variants'))
    return dict(params, family=FAMILY)


def validate_witnesses(oracle, rows):
    data = profiles()
    root = Path(__file__).resolve().parents[4]
    unimplemented = json.loads((PROFILE_PATH.parent / 'unimplemented.json').read_text(encoding='utf8'))['cases']
    unimplemented_ids = {item['csvid'] for item in unimplemented.values()}
    static_only = set(data['native_characteristics_only_witnesses'])
    for name, pin in (data['witnesses'] | data['sources']).items():
        row = rows.get(name, {})
        if oracle.get(name) != pin['oracle'] or row != pin['registry'] or row.get('kind') != 'card':
            raise ValueError('attack/block-alone source or witness changed: ' + name)
        if name not in static_only and (row['csvid'] in unimplemented_ids or 'dummy' in row['handler']):
            raise ValueError('attack/block-alone action witness must be implemented: ' + name)
    for name, pin in data['sources'].items():
        source = pin['compiled_source']
        if hashlib.sha256((root / source['path']).read_bytes()).hexdigest() != source['sha256']:
            raise ValueError('attack/block-alone compiled source changed: ' + name)


class CompanionCombat(BlockStatBoard):
    def __init__(self, name, csvid, declaration, params, variant, rows, oracle):
        from recipes import card, cost_from_oracle, observe, step

        self.card, self.step, self.observe, self.cost = card, step, observe, cost_from_oracle
        self.name, self.csvid, self.d, self.p, self.rows, self.oracle = name, csvid, declaration, params, rows, oracle
        mode, owner = variant.rsplit('_', 1)
        self.mode, self.owner = mode, int(owner)
        self.steps, self.setup, self.entities, self.base = [], [], {}, {}
        self.tokens, self.bound = [], set()
        self.boundary = self.discard_nondeterministic = False
        self.life = [20, 20]
        self.pools = [[0] * 6, [0] * 6]
        self.mana = [0, 0]
        self.cut = None
        self.zones = []
        self.attacks = []
        self.blocks = {}
        self.defending = mode.startswith('block_')
        for player in (0, 1):
            zones = dict(hand=['Forest', 'Plains'], graveyard=['Island'], library=list(LIBRARY), exile=['Mountain'])
            self.zones.append(zones)
            for zone, names in zones.items():
                self.setup.extend(card(stock, f'stock_{zone}_{player}_{index}', player, zone) for index, stock in enumerate(names))

        self.add(name, 'source', 0, 'hand' if mode in PAID else 'battlefield', ['tapped'] if mode.endswith('_tapped_source') else None)
        self.source_fresh = False
        ally = None
        if mode not in ('attack_solo', 'block_solo', 'unfunded', 'wrong_color'):
            ally = {'attack_defender_ally': 'Wall of Roots', 'block_cannot_block_ally': 'Carrion Feeder'}.get(mode, 'Merfolk Looter')
        if ally:
            state = ['tapped'] if mode.endswith('_tapped_ally') else ['sick'] if mode == 'attack_sick_ally' else None
            self.add(ally, 'ally', int(mode == 'attack_foreign_ally'), state=state)
        if self.defending:
            enemy = {'block_with_ally_same': 'Disowned Ancestor', 'block_air_with_ally': 'Welkin Tern'}.get(mode, 'Raging Goblin')
            self.add(enemy, 'enemy', 1)
            if mode in ('block_with_ally_different', 'block_remove_ally_after', 'block_tapped_source', 'block_paid_fresh', 'block_replay', 'block_air_with_ally', 'block_wrong_air_ally'):
                self.add('Welkin Tern' if mode == 'block_wrong_air_ally' else 'Samite Healer', 'enemy_ally', 1)
        elif mode in FLYING_OWN:
            self.add('Disowned Ancestor' if mode == 'attack_ground_block' else 'Thornweald Archer', 'enemy', 1)
        if mode in REMOVE:
            self.add('Unsummon', 'bounce', 1, 'hand')
            self.pools[1] = self.cost(oracle['Unsummon'])
        if mode in PAID:
            fee = self.cost(oracle[name])
            if mode != 'unfunded':
                self.pools[0] = [sum(fee), 0, 0, 0, 0, 0] if mode == 'wrong_color' else fee
        self.emit('turn', player=int(self.defending and mode != 'block_paid_fresh'), value=0)
        self.emit('present_ai_dialogs', value=1)
        for player in (0, 1):
            self.set_pool(player, self.pools[player])
        self.emit('refresh')

    def property(self, ref, key, value):
        self.emit('assert_card', ref=ref, property=key, value=value)

    def attack(self, ref, legal):
        self.emit('attack', ref=ref, value=int(legal))
        if legal:
            self.attacks.append(ref)
            self.entities[ref]['tapped'] = True
        self.property(ref, 'attacking', int(legal))

    def block(self, ref, other, legal):
        self.emit('block', ref=ref, other=other, value=int(legal))
        if legal:
            self.blocks[other].append(ref)

    def bounce_ally(self):
        self.emit('choose_target', player=1, ref='ally')
        self.emit('cast', ref='bounce', value=1)
        self.spend(self.oracle['Unsummon']['mana_cost'], player=1)
        self.emit('resolve')
        self.emit('refresh')
        self.move('bounce', 'graveyard')
        self.property('ally', 'in_play', 0)
        self.move('ally', 'hand')
        self.emit('bind_hand', ref='ally', player=0, value=self.rows[self.entities['ally']['name']]['csvid'])
        self.property('ally', 'in_hand', 1)

    def turn_advance(self, player):
        if any(map(any, self.pools)):
            raise ValueError('bounded real untap requires empty pools')
        self.emit('advance_turn')
        self.emit('untap_step', player=player)
        self.emit('upkeep_step', player=player)
        self.emit('turn', player=player, value=0)

    def prepare_source(self):
        if self.mode not in PAID:
            return True
        if self.mode in ('unfunded', 'wrong_color'):
            self.emit('cast', ref='source', value=0)
            self.property('source', 'in_hand', 1)
            return False
        self.paid('source')
        self.source_fresh = True
        self.property('source', 'sick', 1)
        if self.mode in ('block_paid_fresh', 'attack_paid_next_turn'):
            self.turn_advance(1)
            if self.mode == 'attack_paid_next_turn':
                self.turn_advance(0)
                self.source_fresh = False
            self.property('source', 'sick', int(self.source_fresh))
        return True

    def attacker_declaration(self):
        mode = self.mode
        if mode == 'attack_remove_ally_before':
            self.bounce_ally()
        self.emit('begin_attackers')
        if self.defending:
            self.attack('enemy', True)
            if 'enemy_ally' in self.entities:
                self.attack('enemy_ally', True)
        else:
            if mode in ATTACK_GROUP:
                self.attack('ally', True)
            elif mode in ('attack_tapped_ally', 'attack_sick_ally', 'attack_defender_ally'):
                self.attack('ally', False)
            if mode == 'attack_replay':
                self.cut = len(self.steps)
            eligible = mode in ATTACK_GROUP and mode not in ('attack_tapped_source', 'attack_paid_fresh')
            self.attack('source', eligible)
        self.emit('finish_attackers')
        if mode == 'attack_remove_ally_after':
            self.bounce_ally()
            self.property('source', 'attacking', 1)
        self.blocks = {ref: [] for ref in self.attacks if self.entities[ref]['zone'] == 'battlefield'}

    def blocker_declaration(self):
        mode = self.mode
        if mode == 'block_remove_ally_before':
            self.bounce_ally()
        self.emit('begin_blockers')
        if self.defending:
            if mode in BLOCK_GROUP:
                self.block('ally', 'enemy' if mode == 'block_with_ally_same' else 'enemy_ally', True)
            elif mode in ('block_tapped_ally', 'block_cannot_block_ally', 'block_wrong_air_ally'):
                self.block('ally', 'enemy_ally' if mode == 'block_wrong_air_ally' else 'enemy', False)
            if mode == 'block_replay':
                self.cut = len(self.steps)
            eligible = mode in BLOCK_GROUP and mode != 'block_tapped_source' and (mode != 'block_air_with_ally' or self.p['flying'])
            self.block('source', 'enemy', eligible)
        elif mode in FLYING_OWN:
            self.block('enemy', 'source', mode == 'attack_reach_block')
        self.emit('advance_combat')
        for ref, blockers in self.blocks.items():
            self.property(ref, 'blocked', int(bool(blockers)))
            for blocker in blockers:
                self.property(blocker, 'blocking_count', 1)
        if self.defending and not any('source' in blockers for blockers in self.blocks.values()):
            self.property('source', 'blocking_count', 0)
        if mode == 'block_remove_ally_after':
            self.bounce_ally()
            self.property('source', 'blocking_count', 1)

    def combat_damage_receipts(self):
        self.emit('combat_damage')
        self.emit('resolve')
        self.emit('end_combat')
        for attacker, declared in self.blocks.items():
            attacking = self.entities[attacker]
            blocking = [self.entities[ref] for ref in declared if self.entities[ref]['zone'] == 'battlefield']
            if not declared:
                self.life[1 - attacking['player']] -= attacking['power']
            else:
                if len(blocking) > 1 and attacking['power']:
                    raise ValueError('multi-block witness requires zero attacker power')
                attacking['damage'] += sum(entity['power'] for entity in blocking)
                for entity in blocking:
                    entity['damage'] += attacking['power']
        for ref, entity in self.entities.items():
            if entity['zone'] == 'battlefield' and entity['creature'] and entity['damage'] >= entity['toughness']:
                self.move(ref, 'graveyard')

    def run(self):
        if self.prepare_source():
            self.attacker_declaration()
            self.blocker_declaration()
            self.combat_damage_receipts()
        self.receipts()
        if self.cut is not None:
            replay = self.steps[self.cut:]
            self.steps[self.cut:self.cut] = [self.step('capture')]
            self.steps += [self.step('record_state'), self.step('ai_roundtrip'), self.step('restore')]
            self.steps += copy.deepcopy(replay) + [self.step('assert_recorded_state')]
        variant = f'{self.mode}_{self.owner}'
        case = dict(name=f'CannotAttackOrBlockAloneRecipe_{self.csvid}_{variant}', card=self.name,
                    fixture='empty_main', seed=123, setup=self.setup, steps=self.steps,
                    _recipe=dict(family=FAMILY, version=2, variant=variant, params=copy.deepcopy(self.d['params']),
                                 oracle_text=self.d['oracle_text'], untested_components=[
                                     'Independent companions use the real sequential declaration UI. An all-restricted group needs a legal combined-selection action beyond this bridge.',
                                 ]))
        if self.owner:
            _mirror(case)
        return case


def expand(name, csvid, declaration, params, selected, rows, oracle):
    validate_witnesses(oracle, rows)
    validate_variants(params, selected)
    if 'bestow_cost' in params:
        return expand_brawler(name, csvid, declaration, params, selected, rows, oracle)
    return [CompanionCombat(name, csvid, declaration, params, variant, rows, oracle).run() for variant in selected]


BRAWLER_MODES = ('regular_solo', 'regular_group', 'regular_fresh_block', 'menu_regular', 'unfunded',
                 'bestow_own_solo', 'bestow_own_group', 'bestow_own_block', 'bestow_foreign_solo', 'bestow_foreign_group',
                 'detached_solo', 'detached_group', 'pending_lost_solo', 'pending_lost_group', 'aura_destroyed_releases_host',
                 'pending_bestow_replay', 'cancel_bestow', 'cancel_bestow_target')
BRAWLER_REGULAR = ('regular_solo', 'regular_group', 'regular_fresh_block', 'menu_regular', 'unfunded')
BRAWLER_GROUP = ('regular_group', 'bestow_own_group', 'bestow_foreign_group', 'detached_group',
                 'pending_lost_group', 'pending_bestow_replay')


def brawler_parameters(record):
    lines = record.get('oracle_text', '').splitlines()
    if (record.get('layout') != 'normal' or record.get('card_faces') or record.get('selected_face')
            or not record.get('type_line', '').startswith('Enchantment Creature — ')
            or record.get('keywords') != ['Bestow'] or len(lines) != 3 or not lines[0].endswith(REMINDER)
            or not re.fullmatch(GREEDY_COST, record.get('mana_cost', '')) or lines[1] != "This creature can't attack alone."
            or any(not re.fullmatch('[1-5]', record.get(p, '')) for p in ('power', 'toughness'))):
        raise ValueError('complete normal Bestow and intrinsic attack-alone clause required')
    price = re.fullmatch(r'Bestow (' + GREEDY_COST + r')', lines[0].removesuffix(REMINDER))
    bonus = re.fullmatch(r"Enchanted creature gets \+([1-5])/\+([1-5]) and can't attack alone\.", lines[2])
    if not price or not bonus:
        raise ValueError('exact Bestow and host stat/attack clause required')
    return dict(normal_cost=record['mana_cost'], bestow_cost=price[1], bonus=list(map(int, bonus.groups())),
                creature=[int(record['power']), int(record['toughness'])])


class BrawlerScene(Scene):
    def __init__(self, name, owner, mode, model, registry, oracle, aura_id):
        from recipes import cost_from_oracle

        self.name, self.owner, self.enemy, self.mode = name, owner, 1 - owner, mode
        self.registry, self.oracle = registry, oracle
        self.model = dict(model, normal=cost_from_oracle(oracle[name]),
                          bestowed=cost_from_oracle({'mana_cost': model['bestow_cost']}))
        self.id, self.aura_id = registry[name]['csvid'], aura_id
        self.setup, self.steps, self.objects = [], [], {}
        self.mana, self.life = [[0] * 6, [0] * 6], [20, 20]
        self.form = self.snapshot = None

    def setup_cards(self):
        self.add('source', self.name, self.owner)
        self.add('probe', 'Murder', self.enemy)
        m = self.mode
        host_owner = self.enemy if m.startswith('bestow_foreign') else self.owner
        if m not in BRAWLER_REGULAR or m == 'menu_regular':
            self.add('host', 'Merfolk Looter', host_owner, 'battlefield')
        if m in BRAWLER_GROUP:
            self.add('ally', 'Samite Healer', host_owner, 'battlefield')
        if m in ('regular_fresh_block', 'bestow_own_block'):
            self.add('attacker', 'Raging Goblin', self.enemy, 'battlefield')
        if m.startswith(('detached_', 'pending_lost_')):
            self.add('helper', 'Unsummon', self.enemy)
        elif m == 'aura_destroyed_releases_host':
            self.add('helper', 'Naturalize', self.enemy)

    def to_opposing_turn(self):
        self.emit('advance_turn')
        self.emit('untap_step', player=self.enemy)
        self.emit('upkeep_step', player=self.enemy)
        self.emit('turn', player=self.enemy, value=0)
        self.source_state()

    def attack_group(self, ref, damage, allowed, companion=False):
        self.emit('begin_attackers')
        if companion:
            self.emit('attack', ref='ally', value=1)
            self.prop('ally', 'attacking', 1)
        self.emit('attack', ref=ref, value=int(allowed))
        self.prop(ref, 'attacking', allowed)
        self.emit('finish_attackers')
        self.emit('begin_blockers')
        self.emit('advance_combat')
        self.emit('combat_damage')
        self.resolve()
        self.emit('end_combat')
        self.objects[ref]['tapped'] = allowed
        defender = 1 - self.objects[ref]['player']
        self.life[defender] -= int(allowed) * damage + int(companion) * int(self.oracle['Samite Healer']['power'])
        self.prop(ref, 'tapped', allowed)
        self.source_state()

    def block_alone(self, ref):
        self.emit('begin_attackers')
        self.emit('attack', ref='attacker', value=1)
        self.emit('finish_attackers')
        self.emit('begin_blockers')
        self.emit('block', ref=ref, other='attacker', value=1)
        self.emit('advance_combat')
        self.prop(ref, 'blocking_count', 1)
        self.prop('attacker', 'blocked', 1)
        self.emit('combat_damage')
        self.resolve()
        self.prop(ref, 'damage', 1)
        self.emit('end_combat')
        self.objects['attacker']['zone'] = 'graveyard'
        self.source_state()

    def play(self):
        self.setup_cards()
        mode = self.mode
        self.emit('turn', player=self.owner, value=0)
        self.emit('present_ai_dialogs', value=1)
        for who in (0, 1):
            for color in range(6):
                self.emit('set_mana', player=who, color=color, value=0)
        self.emit('refresh')
        if mode == 'unfunded':
            self.cast_source(budget=[0] * 6, success=False)
            return
        if mode in ('cancel_bestow', 'cancel_bestow_target'):
            self.cast_source(budget=self.model['bestowed'], choice=2 if mode == 'cancel_bestow' else 0,
                             cancel_target=mode == 'cancel_bestow_target', success=False)
            return
        if mode in BRAWLER_REGULAR:
            if mode == 'menu_regular':
                self.cast_source(budget=self.model['bestowed'], choice=1)
            else:
                self.cast_source()
            self.resolve()
            self.entered('creature')
            if mode == 'menu_regular':
                return
            if mode == 'regular_fresh_block':
                self.to_opposing_turn()
                self.prop('source', 'sick', 1)
                self.block_alone('source')
            else:
                self.age_turns()
                self.attack_group('source', self.model['creature'][0], mode in BRAWLER_GROUP, mode in BRAWLER_GROUP)
            return

        self.cast_source(bestow=True, choice=0)
        if mode.startswith('pending_lost_'):
            self.cast_helper('helper', 'host', response=True)
            self.objects['host']['zone'] = 'hand'
            self.entered('creature')
        else:
            if mode == 'pending_bestow_replay':
                self.snapshot = len(self.steps)
            self.resolve()
            self.entered('aura')
        if mode == 'bestow_own_block' or mode.startswith('bestow_foreign'):
            self.to_opposing_turn()
        else:
            self.age_turns()
        if mode == 'bestow_own_block':
            self.block_alone('host')
            return
        if mode.startswith('detached_'):
            self.cast_helper('helper', 'host')
            self.objects['host']['zone'] = 'hand'
            self.entered('creature')
        if mode == 'aura_destroyed_releases_host':
            self.cast_helper('helper', 'source')
            self.objects['source']['zone'] = 'graveyard'
            self.form = None
            self.host_stats()
        ref = 'source' if self.form == 'creature' else 'host'
        damage = self.model['creature'][0] if ref == 'source' else int(self.oracle['Merfolk Looter']['power'])
        if self.form == 'aura':
            damage += self.model['bonus'][0]
        companion = mode in BRAWLER_GROUP
        self.attack_group(ref, damage, companion or mode == 'aura_destroyed_releases_host', companion)

    def source_state(self):
        if self.form is not None:
            super().source_state()


def expand_brawler(name, csvid, declaration, params, selected, rows, oracle):
    from recipes import step

    source = profiles()['sources'][name]
    candidate = source['engine_aura_face']
    if source['registry']['csvid'] != csvid or rows.get(candidate['name']) != candidate or candidate['csvid'] != csvid + 1:
        raise ValueError('independently pinned adjacent Bestow identity required')
    cases = []
    for variant in selected:
        mode, owner = variant.rsplit('_', 1)
        scene = BrawlerScene(name, int(owner), mode, params, rows, oracle, candidate['csvid'])
        scene.play()
        scene.receipts()
        actions = scene.steps
        if scene.snapshot is not None:
            cut = scene.snapshot
            tail = actions[cut:]
            actions = (actions[:cut] + [step('capture')] + tail + [step('record_state'), step('ai_roundtrip'), step('restore')]
                       + copy.deepcopy(tail) + [step('assert_recorded_state')])
        cases.append(dict(name=f'CannotAttackOrBlockAloneRecipe_{csvid}_{variant}', card=name, fixture='empty_main', seed=123,
                          setup=scene.setup, steps=actions, _recipe=dict(family=FAMILY, version=2, variant=variant,
                              params=declaration['params'], oracle_text=declaration['oracle_text'], untested_components=[
                                  'All-restricted groups require a combined-selection action beyond the current sequential declaration interface.',
                                  'Control changes while attached and payment modifiers are outside this pilot.',
                              ])))
    return cases
