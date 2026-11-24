"""Flanking trigger receipts from real declared attacks and blocks."""

import copy
import hashlib
import json
from pathlib import Path

from block_stat_trigger_recipes import BlockStatBoard
from combat_value_trigger_recipes import _mirror
from grant_recipes import LIBRARY

FAMILY = 'combat_flanking_composition'
PROFILE_PATH = Path(__file__).with_name('data') / 'combat_flanking_composition_wave8_profiles.json'
FLANKING = 'Flanking (Whenever a creature without flanking blocks this creature, the blocking creature gets -1/-1 until end of turn.)'
CLAUSES = {
    FLANKING + "\nThis creature can't block.": 'cannot_block',
    "Shroud (This creature can't be the target of spells or abilities.)\n" + FLANKING: 'shroud',
    'All Sliver creatures have flanking. (Whenever a creature without flanking blocks a Sliver, the blocking creature gets -1/-1 until end of turn.)': 'slivers',
    FLANKING + '\nOther creatures you control with flanking have flanking. (Each instance of flanking triggers separately.)': 'cavalry',
    '{1}, {T}: Target creature gains flanking until end of turn. (Whenever a creature without flanking blocks this creature, the blocking creature gets -1/-1 until end of turn.)': 'banner',
    FLANKING + '\nWhenever Sidar Jabari attacks, tap target creature defending player controls.': 'attack_tap',
}


def profiles():
    return json.loads(PROFILE_PATH.read_text(encoding='utf8'))


def parameters(record):
    role = CLAUSES.get(record.get('oracle_text'))
    keywords = ['Shroud', 'Flanking'] if role == 'shroud' else [] if role in ('slivers', 'banner') else ['Flanking']
    if (not role or record.get('layout') != 'normal' or record.get('card_faces') or record.get('selected_face')
            or set(record.get('keywords', [])) != set(keywords)):
        raise ValueError('one complete reviewed flanking composition required')
    if role == 'banner':
        if record['type_line'] != 'Artifact':
            raise ValueError('the reviewed grant source is an artifact')
    elif ('Creature' not in record['type_line'] or any(not record.get(key, '').isascii() or not record[key].isdecimal()
                                                    for key in ('power', 'toughness'))):
        raise ValueError('a fixed-stat flanking creature is required')
    if role == 'attack_tap' and 'Legendary' not in record['type_line']:
        raise ValueError('the reviewed attack-tap source is legendary')
    return dict(role=role, printed_mana=record['mana_cost'], intrinsic_flanking='Flanking' in keywords,
                grant_cost='{1}' if role == 'banner' else '', grant_taps_source=role == 'banner')


def variants(params):
    role = params['role']
    modes = ['cast_payment', 'cast_unaffordable', 'countered_cast']
    if role != 'banner':
        modes += ['normal_block', 'lethal_declaration', 'flanking_blocker', 'two_blockers', 'defending',
                  'unblocked', 'empty_attack', 'declared_snapshot', 'cleanup']
    modes += {
        'cannot_block': [],
        'shroud': ['shroud_targets'],
        'slivers': ['friendly_sliver', 'enemy_sliver', 'non_sliver', 'sliver_blocker', 'two_sidewinders', 'grant_removed'],
        'cavalry': ['friendly_native', 'enemy_native', 'ordinary_ally', 'second_master', 'two_master_self', 'grant_removed'],
        'banner': ['grant_own', 'grant_opponent', 'grant_defender', 'grant_native', 'grant_repeat', 'grant_pending_snapshot',
                   'grant_expiry', 'tapped', 'insufficient', 'target_probe', 'source_removed'],
        'attack_tap': ['tap_only_blocker', 'already_tapped', 'no_defender', 'legend_rule'],
    }[role]
    return [f'{mode}_{owner}' for owner in (0, 1) for mode in modes]


def spec(declaration, record):
    params = parameters(record)
    pin = profiles()['sources'].get(record['name'])
    if (declaration.get('family') != FAMILY or type(declaration.get('version')) is not int or declaration['version'] != 2
            or not pin or pin['oracle'] != record or pin['params'] != params or declaration.get('oracle_text') != record['oracle_text']
            or json.dumps(declaration.get('params'), sort_keys=True) != json.dumps(params, sort_keys=True)):
        raise ValueError('complete pinned Oracle and typed flanking parameters required')
    validate_variants(params, declaration.get('variants'))
    return dict(params, family=FAMILY)


def validate_variants(params, selected):
    if (not isinstance(selected, list) or not selected or any(type(v) is not str for v in selected)
            or len(selected) != len(set(selected)) or not set(selected) <= set(variants(params))):
        raise ValueError('distinct reviewed flanking variants required')


def validate_witnesses(oracle, rows):
    data = profiles()
    root = Path(__file__).resolve().parents[4]
    catalog = json.loads((PROFILE_PATH.parent / 'unimplemented.json').read_text(encoding='utf8'))
    unimplemented = {row['csvid'] for row in catalog['cases'].values()}
    static_only = set(data['native_characteristics_only_witnesses'])
    for name, pin in data['witnesses'].items():
        row = rows.get(name, {})
        if (oracle.get(name) != pin['oracle'] or row != pin['registry'] or row.get('kind') != 'card'
                or name not in static_only and (row['csvid'] in unimplemented or 'dummy' in row['handler'])):
            raise ValueError('flanking witness changed: ' + name)
    for name, pin in data['sources'].items():
        row = rows.get(name, {})
        source = pin['compiled_source']
        if (oracle.get(name) != pin['oracle'] or row != pin['registry'] or row.get('csvid') in unimplemented
                or row.get('kind') != 'card' or 'dummy' in row['handler']
                or hashlib.sha256((root / source['path']).read_bytes()).hexdigest() != source['sha256']):
            raise ValueError('implemented flanking source changed: ' + name)


class FlankingBoard(BlockStatBoard):
    def __init__(self, name, csvid, declaration, params, variant, rows, oracle):
        from recipes import card, cost_from_oracle, observe, step

        self.card, self.step, self.observe, self.cost = card, step, observe, cost_from_oracle
        self.name, self.csvid, self.d, self.p, self.rows, self.oracle = name, csvid, declaration, params, rows, oracle
        self.mode, owner = variant.rsplit('_', 1)
        self.owner = int(owner)
        self.requested = variant
        self.steps, self.setup, self.entities, self.base = [], [], {}, {}
        self.tokens, self.bound = [], set()
        self.aura_target = None
        self.boundary = self.discard_nondeterministic = False
        self.life = [20, 20]
        self.pools = [[10] * 6, [10] * 6]
        self.mana = [60, 60]
        self.grants = {}
        self.cuts = {}
        role = params['role']
        mode = self.mode
        self.actor = int(mode in ('defending', 'enemy_sliver', 'enemy_native', 'grant_opponent'))
        self.attacker, self.blockers = 'source', ['blocker']
        self.tap_target = None
        self.zones = []
        for player in (0, 1):
            zones = dict(hand=['Forest', 'Plains'], graveyard=['Island'], library=list(LIBRARY), exile=['Mountain'])
            self.zones.append(zones)
            for zone, names in zones.items():
                for index, stock in enumerate(names):
                    self.setup.append(card(stock, f'stock_{zone}_{player}_{index}', player, zone))

        casts_source = mode in ('cast_payment', 'cast_unaffordable', 'countered_cast', 'legend_rule')
        self.add(name, 'source', 0, 'hand' if casts_source else 'battlefield', ['tapped'] if mode == 'tapped' else None)
        if mode == 'legend_rule':
            self.add(name, 'other_legend')
        elif mode == 'countered_cast':
            self.add('Counterspell', 'counter', 1, 'hand')
        elif not casts_source:
            self.setup_combat(role)

        self.emit('turn', player=self.actor, value=0)
        self.emit('present_ai_dialogs', value=1)
        if mode == 'cast_payment':
            self.pools[0] = self.cost(oracle[name])
        if mode in ('cast_unaffordable', 'insufficient'):
            self.pools[0] = [0] * 6
        for player in (0, 1):
            self.set_pool(player, self.pools[player])
        self.emit('refresh')

    def setup_combat(self, role):
        mode = self.mode
        if role == 'banner':
            name = 'Fallen Askari' if mode in ('grant_native', 'grant_defender') else 'Grizzly Bears'
            self.add(name, 'attacker', self.actor)
            self.attacker = 'attacker'
        elif mode == 'defending':
            self.add('Grizzly Bears', 'attacker', 1)
            self.attacker, self.blockers = 'attacker', ['source']
            return
        elif mode in ('friendly_sliver', 'enemy_sliver', 'two_sidewinders', 'non_sliver'):
            self.add('Grizzly Bears' if mode == 'non_sliver' else 'Metallic Sliver', 'attacker', self.actor)
            self.attacker = 'attacker'
            if mode == 'two_sidewinders':
                self.add(self.name, 'second_source')
        elif mode in ('friendly_native', 'enemy_native', 'ordinary_ally', 'second_master'):
            self.add('Grizzly Bears' if mode == 'ordinary_ally' else 'Fallen Askari', 'attacker', self.actor)
            self.attacker = 'attacker'
            if mode == 'second_master':
                self.add(self.name, 'second_source')
        elif mode == 'two_master_self':
            self.add(self.name, 'second_source')
        elif mode == 'grant_removed':
            self.add('Metallic Sliver' if role == 'slivers' else 'Fallen Askari', 'attacker')
            self.add('Unsummon', 'remove_source', 0, 'hand')
            self.attacker = 'attacker'

        if mode == 'no_defender':
            self.blockers = []
        else:
            blocker = {
                'lethal_declaration': 'Elvish Mystic',
                'flanking_blocker': "Jolrael's Centaur",
                'sliver_blocker': 'Metallic Sliver',
                'second_master': 'Centaur Courser',
                'two_sidewinders': 'Centaur Courser',
                'grant_repeat': 'Centaur Courser',
                'grant_expiry': 'Giant Spider',
            }.get(mode, 'Grizzly Bears')
            self.add(blocker, 'blocker', 1 - self.actor, state=['tapped'] if mode == 'already_tapped' else None)
            if mode == 'two_blockers':
                self.add('Centaur Courser', 'blocker_two', 1 - self.actor)
                self.blockers.append('blocker_two')
        if role == 'attack_tap' and mode != 'no_defender':
            self.tap_target = 'blocker'
            if mode not in ('tap_only_blocker', 'already_tapped'):
                self.add('Elvish Mystic', 'tap_target', 1)
                self.tap_target = 'tap_target'
        if mode == 'shroud_targets':
            self.add('Unsummon', 'friendly_spell', 0, 'hand')
            self.add('Unsummon', 'enemy_spell', 1, 'hand')
        if mode == 'target_probe':
            self.add("Jolrael's Centaur", 'shroud_target', 1)
            self.add('Forest', 'land_target', 0)
        if mode == 'grant_repeat':
            self.add('Twiddle', 'untap_banner', 0, 'hand')
        if mode == 'source_removed':
            self.add('Shatter', 'remove_banner', 0, 'hand')

    def creature_subtype(self, ref, subtype):
        type_line = self.oracle[self.entities[ref]['name']]['type_line']
        return subtype in type_line.partition(' — ')[2].split()

    def flanking_instances(self, ref):
        entity = self.entities[ref]
        keywords = self.oracle[entity['name']]['keywords']
        battlefield = [item for item in self.entities.values() if item['zone'] == 'battlefield']
        printed_or_granted = int('Flanking' in keywords) + self.grants.get(ref, 0)
        if self.creature_subtype(ref, 'Sliver'):
            printed_or_granted += sum(item['name'] == 'Sidewinder Sliver' for item in battlefield)
        if not printed_or_granted:
            return 0
        # The card itself is excluded from its own additional instance.
        return printed_or_granted + sum(item is not entity and item['name'] == 'Cavalry Master'
                                        and item['player'] == entity['player'] for item in battlefield)

    def cast_countered(self):
        self.emit('cast', ref='source', value=1)
        self.spend(self.p['printed_mana'])
        self.emit('assert_card', ref='source', property='in_hand', value=0)
        self.emit('assert_stack', value=1)
        self.emit('assert_mana_total', player=0, value=self.mana[0])
        self.emit('cast_response', ref='counter', value=1)
        self.spend(self.oracle['Counterspell']['mana_cost'], 1)
        self.emit('assert_stack', value=2)
        self.emit('resolve_response')
        self.move('source', 'graveyard')
        self.move('counter', 'graveyard')

    def grant(self, target):
        source = self.entities['source']
        available = not source['tapped'] and self.pools[0][0] >= 1
        self.emit('can_activate', ref='source', value=int(available))
        if not available:
            self.emit('activate', ref='source', value=0)
            return
        self.emit('choose_target', player=0, ref=target)
        self.emit('activate', ref='source', value=1)
        self.spend(self.p['grant_cost'])
        source['tapped'] = True
        self.emit('assert_card', ref='source', property='tapped', value=1)
        self.emit('assert_stack', value=1)
        self.emit('assert_mana_total', player=0, value=self.mana[0])
        self.cuts['grant_pending_snapshot'] = len(self.steps)
        self.emit('resolve')
        self.grants[target] = self.grants.get(target, 0) + 1
        self.receipts()

    def untap_banner(self):
        self.emit('choose_target', player=0, ref='source')
        self.emit('cast', ref='untap_banner', value=1)
        self.spend(self.oracle['Twiddle']['mana_cost'])
        self.emit('choose_dialog', player=0, value=1)
        self.emit('assert_stack', value=1)
        self.emit('resolve')
        self.move('untap_banner', 'graveyard')
        self.entities['source']['tapped'] = False
        self.receipts()

    def combat(self):
        mode = self.mode
        attacker = self.attacker
        self.emit('begin_attackers')
        attacking = mode != 'empty_attack'
        if attacking:
            self.emit('attack', ref=attacker, value=1)
            self.entities[attacker]['tapped'] = True
        if attacking and self.tap_target:
            self.emit('choose_target', player=0, ref=self.tap_target)
        self.emit('finish_attackers')
        if attacking and self.tap_target:
            self.entities[self.tap_target]['tapped'] = True
            self.emit('assert_card', ref=self.tap_target, property='tapped', value=1)
            self.emit('assert_unused_choices', value=0)
        self.emit('begin_blockers')
        assigned = []
        if attacking and mode != 'unblocked':
            for blocker in self.blockers:
                permitted = not self.entities[blocker]['tapped'] and not (blocker == 'source' and self.p['role'] == 'cannot_block')
                self.emit('block', ref=blocker, other=attacker, value=int(permitted))
                self.emit('assert_card', ref=blocker, property='blocking_count', value=int(permitted))
                if permitted:
                    assigned.append(blocker)
        self.emit('advance_combat')
        # Each valid declaration gives each eligible blocker one penalty per
        # flanking instance. Losing all blockers does not make an attacker unblocked.
        amount = self.flanking_instances(attacker) if attacking else 0
        eligible = [blocker for blocker in assigned if not self.flanking_instances(blocker)]
        for blocker in eligible:
            self.entities[blocker]['boost'] -= amount
        for blocker in eligible:
            entity = self.entities[blocker]
            if entity['toughness'] + entity['boost'] <= 0:
                self.move(blocker, 'graveyard')
        if attacking:
            self.emit('assert_card', ref=attacker, property='blocked', value=int(bool(assigned)))
        self.receipts()
        self.cuts['declared_snapshot'] = len(self.steps)
        if len(assigned) > 1:
            return
        if attacking and not assigned:
            self.life[1 - self.actor] -= self.power(attacker)
        if assigned and self.entities[assigned[0]]['zone'] == 'battlefield':
            blocker = assigned[0]
            damage_out, damage_in = self.power(attacker), self.power(blocker)
            self.apply_damage(blocker, damage_out)
            self.apply_damage(attacker, damage_in)
        self.emit('combat_damage')
        self.emit('resolve')
        self.receipts()
        self.emit('end_combat')

    def apply_damage(self, ref, damage):
        entity = self.entities[ref]
        entity['damage'] += damage
        if entity['damage'] >= entity['toughness'] + entity['boost']:
            self.move(ref, 'graveyard')

    def cleanup(self):
        for player in (0, 1):
            self.set_pool(player, [0] * 6)
        self.emit('cleanup')
        self.emit('refresh')
        self.grants.clear()
        for item in self.entities.values():
            item.update(boost=0, damage=0)

    def run(self):
        mode = self.mode
        if mode == 'cast_payment':
            self.paid('source')
            if self.p['role'] != 'banner':
                self.emit('assert_card', ref='source', property='sick', value=1)
        elif mode == 'legend_rule':
            self.emit('cast', ref='source', value=1)
            self.spend(self.p['printed_mana'])
            self.emit('assert_stack', value=1)
            # The engine presents the duplicate to send away, the inverse of
            # choosing the single copy to keep for a two-copy legend check.
            self.emit('choose_target', player=0, ref='other_legend')
            self.emit('resolve')
            self.move('source', 'battlefield')
            self.move('other_legend', 'graveyard')
            self.emit('assert_card', ref='source', property='sick', value=1)
        elif mode == 'cast_unaffordable':
            self.emit('cast', ref='source', value=0)
            self.emit('assert_card', ref='source', property='in_hand', value=1)
        elif mode == 'countered_cast':
            self.cast_countered()
        elif mode == 'target_probe':
            for target, expected in ((self.attacker, 1), ('blocker', 1), ('shroud_target', 0), ('land_target', 0)):
                self.emit('probe_activation_target', ref='source', other=target, index=-1, value=expected)
        else:
            if mode == 'shroud_targets':
                for spell in ('friendly_spell', 'enemy_spell'):
                    self.emit('can_target_creature', ref=spell, other='source', value=0)
                    self.emit('can_target_creature', ref=spell, other='blocker', value=1)
            if mode == 'grant_removed':
                self.paid('remove_source', 'source')
                self.move('source', 'hand')
            if self.p['role'] == 'banner':
                self.grant('blocker' if mode == 'grant_defender' else self.attacker)
                if mode == 'grant_repeat':
                    self.untap_banner()
                    self.grant(self.attacker)
                if mode == 'source_removed':
                    self.paid('remove_banner', 'source')
                    self.move('source', 'graveyard')
            self.combat()
            if mode == 'cleanup':
                self.cleanup()
            if mode == 'grant_expiry':
                self.next_own_turn()
                self.grants.clear()
                self.combat()
        self.receipts()
        steps = self.steps
        if mode in ('declared_snapshot', 'grant_pending_snapshot'):
            cut = self.cuts[mode]
            replay = steps[cut:]
            steps = steps[:cut] + [self.step('capture')] + replay
            steps += [self.step('record_state'), self.step('ai_roundtrip'), self.step('restore')]
            steps += copy.deepcopy(replay) + [self.step('assert_recorded_state')]
        case = dict(name=f'CombatFlankingCompositionRecipe_{self.csvid}_{self.requested}', card=self.name,
                    fixture='empty_main', seed=123, setup=self.setup, steps=steps,
                    _recipe=dict(family=FAMILY, version=2, variant=self.requested, params=copy.deepcopy(self.d['params']),
                                 oracle_text=self.d['oracle_text'], untested_components=[
                                     'The two-blocker probes stop after real declaration completion. Single-block and unblocked probes cover damage and cleanup.',
                                     'Flanking is inferred from the actual blocker effect, independent of raw special-keyword flags.',
                                 ]))
        if self.owner:
            _mirror(case)
        return case


def expand(name, csvid, declaration, params, selected, rows, oracle):
    validate_witnesses(oracle, rows)
    validate_variants(params, selected)
    return [FlankingBoard(name, csvid, declaration, params, variant, rows, oracle).run() for variant in selected]
