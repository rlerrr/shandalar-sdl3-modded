"""Whole-Oracle blocker-count stat triggers through actual combat declarations."""
import copy
import hashlib
import json
import re
from pathlib import Path

from return_saboteur_trigger_recipes import ReturnBoard
from combat_value_trigger_recipes import _mirror
from grant_recipes import LIBRARY

FAMILY = 'block_stat_trigger'
PROFILE_PATH = Path(__file__).parent/'data/block_stat_trigger_wave8_profiles.json'


def profiles():
    return json.loads(PROFILE_PATH.read_text(encoding='utf8'))


def parameters(record):
    if (record.get('layout') != 'normal' or record.get('card_faces') or record.get('selected_face')
            or 'Creature' not in record.get('type_line', '')
            or any(not re.fullmatch(r'[0-9]', record.get(k, '')) for k in ('power', 'toughness'))):
        raise ValueError('fixed normal blocker-count source required')
    for found in re.findall(r'\([^()]*\)', record['oracle_text']):
        if not re.fullmatch(r'\(Whenever this creature becomes blocked, it gets \+([1-4])/\+\1 until end of turn for each creature blocking it beyond the first\.\)', found):
            raise ValueError('unreviewed blocker-count reminder')
    text = re.sub(r'\([^()]*\)', '', record['oracle_text'])
    keys = []; trigger = None
    for part in re.split(r'[\n;]', text):
        line = part.strip()
        if not line: continue
        if line.lower() in ('flying', 'trample', 'haste'):
            keys.append(line.capitalize()); continue
        m = re.fullmatch(r'Rampage ([1-4])', line, re.I)
        if m:
            if trigger is not None: raise ValueError('one complete count trigger required')
            keys.append('Rampage'); trigger = dict(basis='beyond_first', power=int(m[1]), toughness=int(m[1]), scope='self'); continue
        m = re.fullmatch(r'Whenever (this creature|a Sliver) becomes blocked, (it|that Sliver) gets ([+-][1-4])/([+-][1-4]) until end of turn for each creature blocking it( beyond the first)?\.', line)
        if not m or trigger is not None or (m[1] == 'a Sliver') != (m[2] == 'that Sliver'):
            raise ValueError('complete supported count clause required')
        trigger = dict(basis='beyond_first' if m[5] else 'per_blocker', power=int(m[3]), toughness=int(m[4]), scope='all_slivers' if m[1] == 'a Sliver' else 'self')
    if trigger is None or len(keys) != len(set(keys)) or set(keys) != set(record.get('keywords', [])):
        raise ValueError('complete text and keyword metadata required')
    return dict(**trigger, keywords=sorted(keys))


def spec(declaration, record):
    from recipes import cost_from_oracle
    p = parameters(record); pin = profiles()['sources'].get(record['name'])
    if (declaration.get('family') != FAMILY or type(declaration.get('version')) is not int or declaration['version'] != 2
            or not pin or pin['oracle'] != record or pin['params'] != p
            or declaration.get('oracle_text') != record['oracle_text']
            or json.dumps(declaration.get('params'), sort_keys=True) != json.dumps(p, sort_keys=True)):
        raise ValueError('whole pinned source and exact blocker-count parameters required')
    p.update(family=FAMILY, mana=cost_from_oracle(record), base_power=int(record['power']), base_toughness=int(record['toughness']))
    selected = declaration.get('variants')
    if not isinstance(selected, list) or not selected or len(selected) != len(set(selected)) or any(v not in variants(p) for v in selected):
        raise ValueError('distinct supported blocker-count variants required')
    return p


def variants(p):
    modes = ['zero_blockers', 'one_blocker', 'two_blockers', 'three_blockers', 'other_attack', 'defend',
             'unrelated_blockers', 'cast', 'payment', 'insufficient_mana', 'fresh_attack',
             'tapped_attack', 'pump', 'cleanup', 'two_combats', 'ready_snapshot', 'declared_snapshot',
             'damage_snapshot', 'noncombat_player', 'ground_block']
    if 'Trample' in p['keywords']: modes += ['trample']
    if any(p['mana'][1:]): modes += ['wrong_color']
    if p['power'] < 0 and p['power'] == p['toughness']: modes += ['zero_toughness']
    if p['power'] < p['toughness']: modes += ['negative_power']
    if p['scope'] == 'all_slivers': modes += ['own_sliver', 'enemy_sliver', 'changeling', 'non_sliver', 'two_sources']
    return [mode+'_'+str(owner) for owner in (0, 1) for mode in modes]


def validate_witnesses(oracle, rows):
    data = profiles(); root = Path(__file__).resolve().parents[4]
    for name, pin in data['witnesses'].items():
        row = rows.get(name, {})
        if (oracle.get(name) != pin or row.get('kind') != 'card'
                or row.get('handler') != data['witness_handlers'][name] or row.get('handler') in (None, 'NULL', '0')):
            raise ValueError('block-stat witness changed: '+name)
        if 'dummy' in row['handler'] and name not in data['native_characteristics_only_witnesses']:
            raise ValueError('unimplemented block-stat effect witness: '+name)
    for name, pin in data['sources'].items():
        row = rows.get(name, {})
        if (oracle.get(name) != pin['oracle'] or row.get('handler') != pin['handler'] or 'dummy' in pin['handler']
                or row.get('kind') != 'card' or row.get('csvid') != pin['csvid']):
            raise ValueError('block-stat source changed: '+name)
        source = pin['compiled_source']
        if hashlib.sha256((root/source['path']).read_bytes()).hexdigest() != source['sha256']:
            raise ValueError('compiled block-stat source changed: '+name)


class BlockStatBoard(ReturnBoard):
    def __init__(self, name, csvid, d, p, variant, rows, oracle):
        from recipes import card, step, observe, cost_from_oracle
        self.card = card; self.step = step; self.observe = observe; self.cost = cost_from_oracle
        self.name = name; self.csvid = csvid; self.d = d; self.p = p; self.rows = rows; self.oracle = oracle
        self.mode, owner = variant.rsplit('_', 1); self.owner = int(owner); self.requested = variant
        self.setup = []; self.steps = []; self.entities = {}; self.tokens = []; self.bound = set()
        self.e = dict(draw=0, discard=0, mill=False, self_counters=0); self.keys = set(p['keywords'])
        self.boundary = False; self.optional = False; self.discard_nondeterministic = False
        self.zones = [{z: [] for z in ('hand', 'graveyard', 'library', 'exile')} for _ in (0, 1)]
        self.life = [20, 20]; self.mana = [60, 60]; self.pools = [[10]*6, [10]*6]
        self.base = {}; self.aura_target = None
        mode = self.mode
        self.actor = 1 if mode in ('defend', 'enemy_sliver') else 0
        self.casting = mode in ('cast', 'payment', 'insufficient_mana', 'wrong_color', 'fresh_attack')
        for player in (0, 1):
            self.zones[player].update(hand=['Forest', 'Plains'], graveyard=['Island'], exile=['Mountain'], library=list(LIBRARY))
            for zone, names in self.zones[player].items():
                self.setup += [card(n, f'stock_{zone}_{player}_{i}', player, zone) for i, n in enumerate(names)]
        self.add(name, 'source', 0, 'hand' if self.casting else 'battlefield', ['tapped'] if mode == 'tapped_attack' else None)
        self.attacker = 'source'
        if mode in ('other_attack', 'non_sliver', 'unrelated_blockers'):
            self.add('Ornithopter' if mode == 'unrelated_blockers' else 'Grizzly Bears', 'idle')
            self.attacker = 'idle' if mode != 'unrelated_blockers' else 'source'
        if mode in ('own_sliver', 'enemy_sliver', 'changeling'):
            self.add('Changeling Sentinel' if mode == 'changeling' else 'Homing Sliver', 'ally', self.actor)
            self.attacker = 'ally'
        if mode == 'defend':
            self.add('Grizzly Bears', 'enemy', 1); self.attacker = 'enemy'
        if mode == 'two_sources': self.add(name, 'copy')
        self.count = 0 if mode in ('zero_blockers', 'fresh_attack', 'tapped_attack') else 1 if mode in ('one_blocker', 'other_attack', 'defend', 'ground_block') else 3 if mode == 'three_blockers' else 6 if mode == 'zero_toughness' else 5 if mode == 'negative_power' else 2
        self.blockers = []
        if mode != 'defend':
            for i in range(self.count):
                ref = 'blocker_'+str(i); self.add('Grizzly Bears' if mode == 'ground_block' else 'Ornithopter', ref, 1-self.actor); self.blockers.append(ref)
        if mode == 'unrelated_blockers':
            self.add('Ornithopter', 'unrelated_0', 1); self.add('Ornithopter', 'unrelated_1', 1)
        # Real prevention keeps the independent count/expiry fixtures alive.
        # A separate printed-Trample cell has no prevention and proves damage.
        self.prevented = not self.casting and mode not in ('noncombat_player', 'trample', 'negative_power')
        if self.prevented: self.add('Gaseous Form', 'prevention', self.actor, 'hand')
        if mode == 'pump': self.add('Giant Growth', 'helper', 0, 'hand')
        if mode == 'noncombat_player': self.add('Hermetic Study', 'grant', 0, 'hand')
        if mode == 'payment': self.pools[0] = list(p['mana'])
        if mode == 'insufficient_mana': self.pools[0] = [0]*6
        if mode == 'wrong_color': self.pools[0] = [60, 0, 0, 0, 0, 0]
        # Teeka is colorless: colored mana is a lawful generic payment.
        if mode == 'wrong_color' and not any(p['mana'][1:]): self.pools[0] = [0]*6
        self.emit('turn', player=self.actor, value=0)
        self.emit('present_ai_dialogs', value=1)
        for player in (0, 1): self.set_pool(player, self.pools[player])
        self.ready = len(self.steps)

    def add(self, *args, **kwargs):
        super().add(*args, **kwargs)
        ref = args[1]; e = self.entities[ref]; self.base[ref] = (e['power'], e['toughness'])

    def paid(self, ref, target=None):
        e = self.entities[ref]; player = e['player']; name = e['name']
        if target: self.emit('choose_target', player=player, ref=target)
        self.emit('cast', ref=ref, value=1); self.spend(self.oracle[name]['mana_cost'], player)
        self.emit('assert_card', ref=ref, property='in_hand', value=0); self.emit('assert_stack', value=1)
        self.emit('assert_mana_total', player=player, value=self.mana[player]); self.emit('resolve')
        permanent = any(t in self.oracle[name]['type_line'] for t in ('Creature', 'Enchantment', 'Artifact'))
        self.move(ref, 'battlefield' if permanent else 'graveyard')

    def eligible(self, ref):
        record = self.oracle[self.entities[ref]['name']]
        return ref == 'source' if self.p['scope'] == 'self' else ('Sliver' in record['type_line'] or 'Changeling' in record.get('keywords', []))

    def reset_stats(self):
        for ref, e in self.entities.items(): e['power'], e['toughness'] = self.base[ref]

    def count_effect(self, ref, count):
        if not self.eligible(ref): return
        amount = max(count-1, 0) if self.p['basis'] == 'beyond_first' else count
        amount *= 2 if self.mode == 'two_sources' else 1
        e = self.entities[ref]; e['power'] += amount*self.p['power']; e['toughness'] += amount*self.p['toughness']
        if e['toughness']+e['boost'] <= 0:
            self.move(ref, 'graveyard')
            if self.aura_target == ref: self.move('prevention', 'graveyard')

    def receipts(self):
        self.emit('assert_unused_choices', value=0)
        super().receipts()

    def assignment_inputs(self, attacker, assigned):
        # The human game loop requests one blocker selection per damage point
        # before prevention. The AI path performs its own real assignment and
        # must not receive a fabricated decision plan. The mirror maps this
        # actor to the actual human0 only when actor == requested owner.
        if self.actor != self.owner or assigned <= 1 or self.entities[attacker]['zone'] != 'battlefield': return
        remaining = self.power(attacker)
        if 'Trample' in self.oracle[self.entities[attacker]['name']].get('keywords', []):
            # Give every earlier0/2 blocker lethal2, then select the last for
            # all remaining points. The legacy trample UI transfers its excess
            # to the defending player. All selections are actual legal offered
            # blockers; Gaseous Form still prevents the resulting real damage.
            for ref in self.blockers[:-1]:
                points = min(2, remaining)
                for _ in range(points): self.emit('choose_target', player=self.actor, ref=ref)
                remaining -= points
            target = self.blockers[-1]
        else:
            # Any allocation among the blockers is legal without Trample; the
            # paid prevention fixtures assert no particular legal distribution.
            target = self.blockers[0]
        for _ in range(remaining): self.emit('choose_target', player=self.actor, ref=target)

    def combat(self):
        mode = self.mode; attacker = self.attacker
        self.emit('begin_attackers')
        legal = mode != 'tapped_attack' and (mode != 'fresh_attack' or 'Haste' in self.keys)
        self.emit('attack', ref=attacker, value=int(legal))
        if legal: self.entities[attacker]['tapped'] = 'Vigilance' not in self.oracle[self.entities[attacker]['name']].get('keywords', [])
        if mode == 'unrelated_blockers':
            self.emit('attack', ref='idle', value=1); self.entities['idle']['tapped'] = True
        self.emit('finish_attackers'); self.emit('begin_blockers')
        assigned = 0
        if mode == 'defend': self.emit('block', ref='source', other='enemy', value=1)
        elif legal:
            for ref in self.blockers:
                legal_block = not (mode == 'ground_block' and 'Flying' in self.oracle[self.entities[attacker]['name']].get('keywords', []))
                self.emit('block', ref=ref, other=attacker, value=int(legal_block)); assigned += int(legal_block)
            if mode == 'unrelated_blockers':
                for ref in ('unrelated_0', 'unrelated_1'): self.emit('block', ref=ref, other='idle', value=1)
        self.emit('advance_combat')
        if mode != 'defend' and legal: self.count_effect(attacker, assigned)
        if legal and self.entities[attacker]['zone'] == 'battlefield':
            self.emit('assert_card', ref=attacker, property='blocked', value=int(mode == 'defend' or assigned > 0))
            if mode == 'defend': self.emit('assert_card', ref='source', property='blocking_count', value=1)
            else:
                for ref in self.blockers:
                    self.emit('assert_card', ref=ref, property='blocking_count', value=int(not (mode == 'ground_block' and 'Flying' in self.keys)))
        self.receipts(); declared = len(self.steps)
        if mode == 'trample':
            damage = max(0, self.entities[attacker]['power']); self.life[1] -= max(0, damage-assigned*2)
            # Ornithopters are printed0/2. With Trample and ample power every
            # blocker gets lethal2 and the rest must reach the defending player.
            for ref in self.blockers: self.move(ref, 'graveyard')
        elif self.casting and legal: self.life[1] -= self.power(attacker)
        self.assignment_inputs(attacker, assigned)
        self.emit('combat_damage'); self.emit('resolve'); self.receipts(); damaged = len(self.steps)
        self.emit('end_combat'); self.receipts()
        return declared, damaged

    def next_own_turn(self):
        for player in (0, 1): self.set_pool(player, [0]*6)
        self.emit('advance_turn'); self.emit('untap_step', player=1)
        self.emit('advance_turn'); self.emit('untap_step', player=0); self.emit('turn', player=0, value=0)
        self.reset_stats()
        for e in self.entities.values(): e.update(tapped=False, boost=0, damage=0)
        for player in (0, 1): self.set_pool(player, [10]*6)

    def run(self):
        mode = self.mode; declared = damaged = self.ready
        if self.casting:
            if mode in ('insufficient_mana', 'wrong_color'):
                self.emit('cast', ref='source', value=0); self.emit('assert_card', ref='source', property='in_hand', value=1)
            else: self.paid('source')
        if mode == 'noncombat_player':
            self.paid('grant', 'source'); self.emit('can_activate', ref='grant', value=1)
            self.emit('choose_player', player=0, value=1); self.emit('activate', ref='grant', value=1)
            self.emit('assert_card', ref='grant', property='tapped', value=0)
            self.entities['source']['tapped'] = True; self.life[1] -= 1; self.emit('resolve')
        elif not self.casting or mode == 'fresh_attack':
            if self.prevented:
                self.aura_target = 'source' if mode == 'defend' else self.attacker
                self.paid('prevention', self.aura_target)
            if mode == 'pump': self.paid('helper', 'source'); self.entities['source']['boost'] += 3
            declared, damaged = self.combat()
            if mode == 'two_combats': self.next_own_turn(); self.combat()
            if mode == 'cleanup':
                for player in (0, 1): self.set_pool(player, [0]*6)
                self.emit('cleanup'); self.emit('refresh'); self.reset_stats()
                for e in self.entities.values(): e.update(boost=0, damage=0)
        self.receipts(); steps = self.steps
        if mode in ('ready_snapshot', 'declared_snapshot', 'damage_snapshot'):
            cut = self.ready if mode == 'ready_snapshot' else declared if mode == 'declared_snapshot' else damaged
            replay = steps[cut:]
            steps = copy.deepcopy(steps[:cut])+[self.step('capture')]+copy.deepcopy(replay)+[self.step('record_state'), self.step('ai_roundtrip'), self.step('restore')]+copy.deepcopy(replay)+[self.step('assert_recorded_state')]
        case = dict(name=f'BlockStatTriggerRecipe_{self.csvid}_{self.requested}', card=self.name, fixture='empty_main', seed=123,
                    setup=self.setup, steps=steps, _recipe=dict(family=FAMILY, version=2, variant=self.requested,
                    params=copy.deepcopy(self.d['params']), oracle_text=self.d['oracle_text'], untested_components=[
                    'No demonstrated response window between becoming-blocked trigger announcement and resolution; no fabricated trigger priority.',
                    'Mandatory blocks, changed type/control between announcement and resolution, additional combat grants and multiplayer remain later compositions.']))
        if self.owner: _mirror(case)
        return case


def expand(name, csvid, declaration, p, selected, rows, oracle):
    validate_witnesses(oracle, rows)
    if not isinstance(selected, list) or not selected or len(selected) != len(set(selected)) or any(v not in variants(p) for v in selected):
        raise ValueError('distinct supported blocker-count variants required')
    return [BlockStatBoard(name, csvid, declaration, p, variant, rows, oracle).run() for variant in selected]


# Fixed/predicate delta is opt-in; removing this appended section restores the
# accepted count grammar and all518 existing case bodies byte for byte.
_count_parameters = parameters
_count_variants = variants
_count_expand = expand


def _fixed_parameters(record):
    if (record.get('layout') != 'normal' or record.get('card_faces') or record.get('selected_face')
            or 'Creature' not in record.get('type_line', '')
            or any(not re.fullmatch(r'[0-9]', record.get(k, '')) for k in ('power', 'toughness'))): return None
    text = record['oracle_text']; keys = []
    if text.startswith('Reach (This creature can block creatures with flying.)\n'):
        keys.append('Reach'); text = text.split('\n', 1)[1]
    aliases = {'this creature', record['name'], record['name'].split(',')[0]}
    if 'Legendary' in record['type_line']: aliases.add(record['name'].split()[0])
    source = '(?:'+'|'.join(re.escape(x) for x in sorted(aliases))+')'
    match = re.fullmatch(r'Whenever '+source+r' (?P<verb>blocks or becomes blocked by one or more (?:Orcs|black creatures)|blocks one or more black creatures|blocks a creature with flying|blocks or becomes blocked|becomes blocked|blocks), (?:it|'+source+r') gets (?P<power>[+-][0-4])/(?P<toughness>[+-][0-4])(?: and gains (?P<grant>trample|first strike))? until end of turn\.', text)
    if not match or set(keys) != set(record.get('keywords', [])): return None
    verb = match['verb']
    role = 'both' if verb.startswith('blocks or') else 'attacking' if verb == 'becomes blocked' else 'defending'
    predicate = 'orc' if verb.endswith('Orcs') else 'black' if verb.endswith('black creatures') else 'flying' if verb.endswith('with flying') else 'any'
    return dict(mechanism='fixed_predicate', role=role, multiplicity='per_opponent' if predicate == 'flying' else 'once',
                predicate=predicate, power=int(match['power']), toughness=int(match['toughness']), keywords=keys,
                granted_keywords=[] if match['grant'] is None else [{'trample': 'Trample', 'first strike': 'First strike'}[match['grant']]])


def parameters(record):
    fixed = _fixed_parameters(record)
    return fixed if fixed is not None else _count_parameters(record)


def _fixed_variants(p):
    modes = ['attack_one', 'defend_one', 'attack_two', 'defend_two', 'defend_three', 'unblocked', 'idle_source',
             'empty_combat', 'cast', 'payment', 'insufficient_mana', 'wrong_color', 'fresh_attack', 'cleanup',
             'repeat_combat', 'ready_snapshot', 'declared_snapshot', 'damage_snapshot', 'multi_snapshot', 'pump',
             'noncombat_player', 'no_permission', 'tapped_block', 'permission_expiry', 'effect_damage']
    if 'Reach' in p['keywords'] and p['predicate'] != 'flying': modes += ['reach_flyer']
    if p['predicate'] != 'any':
        modes += ['attack_nonqualifying', 'defend_nonqualifying', 'attack_mixed', 'defend_mixed',
                  'attack_mixed_reverse', 'defend_mixed_reverse', 'attack_unrelated_qualifier', 'defend_unrelated_qualifier']
    if p['predicate'] == 'orc': modes += ['attack_changeling', 'defend_changeling']
    if p['predicate'] == 'black': modes += ['attack_multicolor', 'defend_multicolor', 'attack_colorless', 'defend_colorless']
    if p['granted_keywords']: modes += ['role_damage_contrary']
    if 'First strike' in p['granted_keywords']: modes += ['first_strike_expiry']
    return [mode+'_'+str(owner) for owner in (0, 1) for mode in modes]


def variants(p):
    return _fixed_variants(p) if p.get('mechanism') == 'fixed_predicate' else _count_variants(p)


class FixedPredicateBoard(BlockStatBoard):
    """Separate expected ledger; multi-opponent cells end after real declaration."""
    def __init__(self, name, csvid, d, p, variant, rows, oracle):
        from recipes import card, step, observe, cost_from_oracle
        self.card = card; self.step = step; self.observe = observe; self.cost = cost_from_oracle
        self.name = name; self.csvid = csvid; self.d = d; self.p = p; self.rows = rows; self.oracle = oracle
        self.mode, owner = variant.rsplit('_', 1); self.owner = int(owner); self.requested = variant
        self.setup = []; self.steps = []; self.entities = {}; self.tokens = []; self.bound = set(); self.base = {}
        self.e = dict(draw=0, discard=0, mill=False, self_counters=0); self.keys = set(p['keywords'])
        self.boundary = False; self.optional = False; self.discard_nondeterministic = False
        self.zones = [{z: [] for z in ('hand', 'graveyard', 'library', 'exile')} for _ in (0, 1)]
        self.life = [20, 20]; self.mana = [60, 60]; self.pools = [[10]*6, [10]*6]
        self.aura_target = None; self.active_keys = set(p['keywords']); self.effect_applied = False
        mode = self.mode; positive_actor = 1 if p['role'] == 'defending' else 0
        self.actor = 1 if mode.startswith('defend_') or mode in ('no_permission', 'tapped_block', 'permission_expiry') else 0 if mode.startswith('attack_') or mode in ('cast', 'payment', 'insufficient_mana', 'wrong_color', 'fresh_attack', 'noncombat_player', 'unblocked', 'idle_source') else positive_actor
        if mode == 'role_damage_contrary': self.actor = 1-positive_actor
        self.casting = mode in ('cast', 'payment', 'insufficient_mana', 'wrong_color', 'fresh_attack')
        self.prevented = not self.casting and mode not in ('effect_damage', 'role_damage_contrary', 'first_strike_expiry', 'noncombat_player')
        self.idle = mode == 'idle_source'
        self.n = 0 if mode in ('unblocked', 'empty_combat', 'fresh_attack') else 3 if mode == 'defend_three' else 2 if mode in ('attack_two', 'defend_two', 'multi_snapshot', 'no_permission', 'permission_expiry') or 'mixed' in mode or 'unrelated_qualifier' in mode else 1
        for player in (0, 1):
            self.zones[player].update(hand=['Forest', 'Plains'], graveyard=['Island'], exile=['Mountain'], library=list(LIBRARY))
            for zone, names in self.zones[player].items():
                self.setup += [card(n, f'stock_{zone}_{player}_{i}', player, zone) for i, n in enumerate(names)]
        self.add(name, 'source', 0, 'hand' if self.casting else 'battlefield', ['tapped'] if mode == 'tapped_block' else None)
        self.opponents = []
        for index in range(self.n):
            witness = self.positive_witness()
            if index > 0 and p['predicate'] == 'orc': witness = 'Orcish Lumberjack'
            if index > 0 and p['predicate'] == 'black': witness = 'Dreg Mangler'
            if 'nonqualifying' in mode or 'colorless' in mode: witness = 'Darksteel Myr' if 'colorless' in mode else 'Grizzly Bears'
            if 'mixed' in mode: witness = self.positive_witness() if (index == 0) != mode.endswith('reverse') else 'Grizzly Bears'
            if 'unrelated_qualifier' in mode: witness = 'Grizzly Bears' if index == 0 else self.positive_witness()
            if 'changeling' in mode: witness = 'Changeling Sentinel'
            if 'multicolor' in mode: witness = 'Dreg Mangler'
            if mode == 'reach_flyer': witness = 'Ornithopter'
            if mode in ('effect_damage', 'role_damage_contrary', 'first_strike_expiry'):
                witness = 'Grizzly Bears' if self.actor == 1 and p['predicate'] == 'any' else self.positive_witness()
            if mode == 'role_damage_contrary' and 'First strike' in p['granted_keywords']: witness = 'Grizzly Bears'
            ref = 'opponent_'+str(index); self.add(witness, ref, 1); self.opponents.append(ref)
        if mode == 'first_strike_expiry': self.add('Grizzly Bears', 'second_victim', 1)
        if self.idle:
            self.add('Ornithopter', 'idle'); self.add('Ornithopter', 'idle_blocker', 1)
        if self.prevented: self.add('Gaseous Form', 'prevention', self.actor, 'hand')
        self.needs_permission = self.actor == 1 and (self.n > 1 and mode not in ('no_permission',) and 'unrelated_qualifier' not in mode or mode == 'permission_expiry')
        if self.needs_permission: self.add('Valor Made Real', 'permission', 0, 'hand')
        if mode == 'pump': self.add('Giant Growth', 'helper', 0, 'hand')
        if mode == 'noncombat_player': self.add('Hermetic Study', 'grant', 0, 'hand')
        if mode == 'payment': self.pools[0] = list(p['mana'])
        if mode == 'insufficient_mana': self.pools[0] = [0]*6
        if mode == 'wrong_color': self.pools[0] = [60, 0, 0, 0, 0, 0]
        self.emit('turn', player=self.actor, value=0); self.emit('present_ai_dialogs', value=1)
        for player in (0, 1): self.set_pool(player, self.pools[player])
        self.ready = len(self.steps)

    def positive_witness(self):
        return {'any': 'Grizzly Bears' if self.actor == 1 else 'Ornithopter', 'flying': 'Ornithopter',
                'orc': 'Orcish Artillery', 'black': 'Giant Scorpion'}[self.p['predicate']]

    def qualifies(self, ref):
        record = self.oracle[self.entities[ref]['name']]; predicate = self.p['predicate']
        if predicate == 'any': return True
        if predicate == 'flying': return 'Flying' in record.get('keywords', [])
        if predicate == 'black': return 'B' in record.get('colors', [])
        return 'Orc' in record['type_line'].split(' — ')[-1].split() or 'Changeling' in record.get('keywords', [])

    def apply_fixed(self, refs):
        self.effect_applied = False
        if self.idle or self.p['role'] not in ('both', 'defending' if self.actor == 1 else 'attacking'): return
        count = sum(self.qualifies(ref) for ref in refs)
        if self.p['multiplicity'] == 'once': count = int(count > 0)
        if not count: return
        self.effect_applied = True; self.active_keys.update(self.p['granted_keywords'])
        source = self.entities['source']; source['power'] += count*self.p['power']; source['toughness'] += count*self.p['toughness']

    def damage_to(self, source_ref, target_ref, amount):
        if amount <= 0: return
        target = self.entities[target_ref]; target['damage'] += amount
        keys = self.active_keys if source_ref == 'source' else set(self.oracle[self.entities[source_ref]['name']].get('keywords', []))
        indestructible = 'Indestructible' in self.oracle[target['name']].get('keywords', [])
        if not indestructible and ('Deathtouch' in keys or target['damage'] >= target['toughness']+target['boost']):
            self.move(target_ref, 'graveyard')

    def single_damage(self, attackers, participating):
        if self.idle: return  # Both real idle combatants are printed0/2.
        source = self.entities['source']
        if self.actor == 0:
            if not attackers: return
            if not participating:
                if not self.prevented: self.life[1] -= self.power('source')
                return
            if self.prevented: return
            target = participating[0]; attack_power = self.power('source'); back = self.power(target)
            lethal = self.entities[target]['toughness']-self.entities[target]['damage']
            assigned = min(attack_power, lethal) if 'Trample' in self.active_keys else attack_power
            self.damage_to('source', target, assigned)
            if 'First strike' not in self.active_keys or self.entities[target]['zone'] == 'battlefield': self.damage_to(target, 'source', back)
            if 'Trample' in self.active_keys: self.life[1] -= max(0, attack_power-assigned)
        else:
            for ref in attackers:
                if ref not in participating: self.life[0] -= self.power(ref)
            if not participating or self.prevented: return
            target = participating[0]; source_power = self.power('source'); other_power = self.power(target)
            if 'First strike' in self.active_keys:
                self.damage_to('source', target, source_power)
                if self.entities[target]['zone'] == 'battlefield': self.damage_to(target, 'source', other_power)
            else:
                self.damage_to(target, 'source', other_power); self.damage_to('source', target, source_power)

    def combat_fixed(self, expired_permission=False):
        mode = self.mode; self.emit('begin_attackers'); participating = []; attackers = []
        if self.actor == 0:
            ref = 'idle' if self.idle else 'source'; legal = mode != 'fresh_attack' and mode != 'empty_combat'
            if mode != 'empty_combat': self.emit('attack', ref=ref, value=int(legal))
            if legal: attackers = [ref]
        elif mode != 'empty_combat':
            attackers = self.opponents[:1] if mode == 'permission_expiry' and not expired_permission else list(self.opponents)
            for ref in attackers: self.emit('attack', ref=ref, value=1)
        for ref in attackers:
            self.entities[ref]['tapped'] = 'Vigilance' not in self.oracle[self.entities[ref]['name']].get('keywords', [])
        self.emit('finish_attackers')
        if mode == 'pump' and self.entities['helper']['zone'] == 'hand':
            self.paid('helper', 'source'); self.entities['source']['boost'] += 3
        if self.needs_permission and not expired_permission: self.paid('permission', 'source')
        before_declare = len(self.steps); self.emit('begin_blockers')
        if self.actor == 0 and attackers:
            targets = ['idle_blocker'] if self.idle else self.opponents[:1] if 'unrelated_qualifier' in mode else list(self.opponents)
            for ref in targets: self.emit('block', ref=ref, other=attackers[0], value=1)
            if not self.idle: participating = targets
        elif self.actor == 1:
            for index, ref in enumerate(attackers):
                if 'unrelated_qualifier' in mode and index > 0: continue
                legal = mode != 'tapped_block' and not (index > 0 and (mode == 'no_permission' or expired_permission))
                self.emit('block', ref='source', other=ref, value=int(legal))
                if legal: participating.append(ref)
            self.emit('assert_card', ref='source', property='blocking_count', value=len(participating))
        self.emit('advance_combat'); self.apply_fixed(participating)
        if self.actor == 1: self.emit('assert_card', ref='source', property='blocking_count', value=len(participating))
        elif attackers: self.emit('assert_card', ref=attackers[0], property='blocked', value=int(bool(participating) or self.idle))
        self.receipts(); declared = len(self.steps)
        if len(participating) > 1 or mode == 'no_permission' or expired_permission:
            # The printed trigger has fully resolved at the real declaration
            # boundary. Do not invent multi-opponent allocation input or let an
            # incorrect pump amount produce a downstream scripted-choice error.
            return before_declare, declared, declared, True
        self.single_damage(attackers, participating)
        self.emit('combat_damage'); self.emit('resolve'); self.receipts(); damaged = len(self.steps)
        self.emit('end_combat'); self.receipts()
        return before_declare, declared, damaged, False

    def return_actor_turn(self):
        for player in (0, 1): self.set_pool(player, [0]*6)
        self.emit('advance_turn'); self.emit('untap_step', player=1-self.actor)
        self.emit('advance_turn'); self.emit('untap_step', player=self.actor); self.emit('turn', player=self.actor, value=0)
        self.reset_stats(); self.active_keys = set(self.p['keywords'])
        for entity in self.entities.values(): entity.update(tapped=False, boost=0, damage=0)
        for player in (0, 1): self.set_pool(player, [10]*6)

    def expiry_strike(self):
        if (self.p['base_power'], self.p['base_toughness']) != (2, 2):
            raise ValueError('unreviewed first-strike expiry combat composition')
        # A second ready Bears is already physically present; the first victim
        # is dead. Advance from the opponent's real turn to the source's own.
        for player in (0, 1): self.set_pool(player, [0]*6)
        self.emit('advance_turn'); self.emit('untap_step', player=0); self.emit('turn', player=0, value=0)
        self.reset_stats(); self.active_keys = set(self.p['keywords'])
        for entity in self.entities.values(): entity.update(damage=0, boost=0)
        self.entities['source']['tapped'] = False
        self.emit('begin_attackers'); self.emit('attack', ref='source', value=1); self.entities['source']['tapped'] = True
        self.emit('finish_attackers'); self.emit('begin_blockers'); self.emit('block', ref='second_victim', other='source', value=1)
        self.emit('advance_combat'); self.receipts()
        self.damage_to('source', 'second_victim', 2); self.damage_to('second_victim', 'source', 2)
        self.emit('combat_damage'); self.emit('resolve'); self.emit('end_combat')

    def run(self):
        mode = self.mode; before_declare = declared = damaged = self.ready; cut_short = False
        if self.casting:
            if mode in ('insufficient_mana', 'wrong_color'):
                self.emit('cast', ref='source', value=0); self.emit('assert_card', ref='source', property='in_hand', value=1)
            else: self.paid('source')
        if mode == 'noncombat_player':
            self.paid('grant', 'source'); self.emit('can_activate', ref='grant', value=1)
            self.emit('choose_player', player=0, value=1); self.emit('activate', ref='grant', value=1)
            self.entities['source']['tapped'] = True; self.life[1] -= 1
            self.emit('assert_card', ref='grant', property='tapped', value=0); self.emit('resolve')
        elif not self.casting or mode == 'fresh_attack':
            if self.prevented: self.aura_target = 'source'; self.paid('prevention', 'source')
            before_declare, declared, damaged, cut_short = self.combat_fixed()
            if mode in ('repeat_combat', 'permission_expiry'):
                if cut_short: raise ValueError('turn lifecycle requires completed real combat')
                self.return_actor_turn(); self.combat_fixed(expired_permission=mode == 'permission_expiry')
            if mode == 'first_strike_expiry': self.expiry_strike()
            if mode == 'cleanup':
                for player in (0, 1): self.set_pool(player, [0]*6)
                self.emit('cleanup'); self.emit('refresh'); self.reset_stats(); self.active_keys = set(self.p['keywords'])
                for entity in self.entities.values(): entity.update(boost=0, damage=0)
        self.receipts(); steps = self.steps
        if mode in ('ready_snapshot', 'declared_snapshot', 'damage_snapshot', 'multi_snapshot'):
            cut = self.ready if mode == 'ready_snapshot' else before_declare if mode == 'multi_snapshot' else declared if mode == 'declared_snapshot' else damaged
            replay = steps[cut:]
            steps = copy.deepcopy(steps[:cut])+[self.step('capture')]+copy.deepcopy(replay)+[self.step('record_state'), self.step('ai_roundtrip'), self.step('restore')]+copy.deepcopy(replay)+[self.step('assert_recorded_state')]
        case = dict(name=f'BlockStatTriggerRecipe_{self.csvid}_{self.requested}', card=self.name, fixture='empty_main', seed=123,
                    setup=self.setup, steps=steps, _recipe=dict(family=FAMILY, version=2, variant=self.requested,
                    params=copy.deepcopy(self.d['params']), oracle_text=self.d['oracle_text'], untested_components=[
                    'Multiple-opponent count/predicate cells end after actual complete declaration; their subsequent allocation/damage is not claimed. Single-block damage, keywords and cleanup have separate real controls.',
                    'No fabricated trigger priority or changes between trigger announcement and resolution. Dynamic type/color/ability/control changes in that window remain unobserved.',
                    'Temporary Trample flag expiry has no independent direct keyword observer; its real same-turn combat effect and stat expiry are covered. First strike expiry is separately proved by a real next-turn contrary combat.']))
        if self.owner: _mirror(case)
        return case


def expand(name, csvid, declaration, p, selected, rows, oracle):
    if p.get('mechanism') != 'fixed_predicate': return _count_expand(name, csvid, declaration, p, selected, rows, oracle)
    validate_witnesses(oracle, rows)
    if not isinstance(selected, list) or not selected or len(selected) != len(set(selected)) or any(v not in variants(p) for v in selected):
        raise ValueError('distinct supported fixed/predicate variants required')
    return [FixedPredicateBoard(name, csvid, declaration, p, variant, rows, oracle).run() for variant in selected]
