"""Whole-Oracle conditional attack/block permissions through actual declarations."""
import copy
import hashlib
import json
import re
from pathlib import Path

from block_stat_trigger_recipes import BlockStatBoard
from combat_value_trigger_recipes import _mirror
from grant_recipes import LIBRARY

FAMILY = 'conditional_blocking_legality'
PROFILE_PATH = Path(__file__).parent / 'data/conditional_blocking_legality_wave8_profiles.json'


def profiles():
    return json.loads(PROFILE_PATH.read_text(encoding='utf8'))


def parameters(record):
    if (record.get('layout') != 'normal' or record.get('card_faces') or record.get('selected_face')
            or 'Creature' not in record.get('type_line', '')
            or any(not re.fullmatch(r'[1-9]', record.get(k, '')) for k in ('power', 'toughness'))
            or record.get('keywords') != []):
        raise ValueError('complete fixed normal permission creature required')
    text = record['oracle_text']
    match = re.fullmatch(r"This creature can't be blocked except by artifact creatures and/or (white|red) creatures\.", text)
    if match: return dict(rule='allow', predicates=['artifact', 'color:' + {'white': 'W', 'red': 'R'}[match[1]]])
    if text == "This creature can't be blocked except by Walls and/or creatures with flying.":
        return dict(rule='allow', predicates=['subtype:Wall', 'keyword:Flying'])
    match = re.fullmatch(r"This creature can't be blocked by (Walls|creatures with flying|artifact creatures)\.", text)
    if match:
        return dict(rule='deny', predicates=[{'Walls': 'subtype:Wall', 'creatures with flying': 'keyword:Flying', 'artifact creatures': 'artifact'}[match[1]]])
    match = re.fullmatch(r"This creature can't be blocked by creatures with power ([1-3]) or less\.", text)
    if match: return dict(rule='power_fixed', limit=int(match[1]))
    if text == "Creatures with power less than this creature's power can't block it.":
        return dict(rule='power_source')
    if text == "This creature can't be blocked as long as defending player controls an artifact.":
        return dict(rule='defender_artifact')
    if text == "This creature can't block unless you control another Minotaur.":
        return dict(rule='support', subtype='Minotaur', another=True)
    if text == "This creature can't attack if defending player controls an untapped land.\nThis creature can't block if you control an untapped land.":
        return dict(rule='untapped_land')
    raise ValueError('unreviewed complete permission clause')


def spec(declaration, record):
    from recipes import cost_from_oracle
    pin = profiles()['sources'].get(record['name']); p = parameters(record)
    if (declaration.get('family') != FAMILY or type(declaration.get('version')) is not int or declaration['version'] != 2
            or not pin or pin['oracle'] != record or pin['params'] != p
            or declaration.get('oracle_text') != record['oracle_text']
            or json.dumps(declaration.get('params'), sort_keys=True) != json.dumps(p, sort_keys=True)):
        raise ValueError('full pinned Oracle and exact typed permission parameters required')
    p.update(family=FAMILY, mana=cost_from_oracle(record), base_power=int(record['power']), base_toughness=int(record['toughness']))
    chosen = declaration.get('variants')
    if not isinstance(chosen, list) or not chosen or len(set(chosen)) != len(chosen) or any(v not in variants(p) for v in chosen):
        raise ValueError('explicit distinct permission variants required')
    return p


def variants(p):
    modes = ['allowed_block', 'rejected_block', 'unblocked', 'defend', 'other_attacker', 'empty_combat',
             'cast', 'payment', 'insufficient_mana', 'wrong_color', 'fresh_attack', 'tapped_attack',
             'tapped_block', 'pump', 'cleanup', 'two_combats', 'source_bounce',
             'ready_snapshot', 'declared_snapshot', 'damage_snapshot']
    if p['rule'] == 'allow': modes += ['exception_second', 'exception_both', 'neither']
    if p['rule'] == 'deny': modes += ['flying_nonwall', 'wall_flying']
    if p['rule'] in ('power_fixed', 'power_source'): modes += ['power_zero', 'power_one', 'blocker_pump', 'source_pump_boundary']
    if p['rule'] == 'defender_artifact': modes += ['artifact_own', 'artifact_both', 'artifact_remove', 'artifact_own_cast']
    if p['rule'] == 'support': modes += ['support_other_controller', 'support_removed', 'support_second_copy', 'support_tapped']
    if p['rule'] == 'untapped_land': modes += ['defend_untapped', 'defend_no_land', 'attack_no_land',
        'attack_own_untapped', 'defend_enemy_untapped', 'land_tap', 'defend_land_tap', 'two_lands']
    return [mode + '_' + str(owner) for owner in (0, 1) for mode in modes]


def validate_witnesses(oracle, rows):
    data = profiles(); root = Path(__file__).resolve().parents[4]
    unimplemented = {r['csvid'] for r in json.loads((PROFILE_PATH.parent / 'unimplemented.json').read_text(encoding='utf8'))['cases'].values()}
    for name, pin in data['witnesses'].items():
        row = rows.get(name, {})
        if (oracle.get(name) != pin or row.get('kind') != 'card' or row.get('handler') != data['witness_handlers'][name]
                or row.get('handler') in (None, 'NULL', '0')
                or 'dummy' in row['handler'] and name not in data['native_characteristics_only_witnesses']):
            raise ValueError('permission witness changed: ' + name)
    for name, pin in data['sources'].items():
        row = rows.get(name, {}); source = pin['compiled_source']
        if (oracle.get(name) != pin['oracle'] or row.get('kind') != 'card' or row.get('handler') != pin['handler']
                or row.get('csvid') != pin['csvid'] or row.get('csvid') in unimplemented or 'dummy' in pin['handler']
                or hashlib.sha256((root / source['path']).read_bytes()).hexdigest() != source['sha256']):
            raise ValueError('implemented permission source changed: ' + name)


class PermissionBoard(BlockStatBoard):
    """Independent Oracle ledger; old helpers provide only emission/payment/zones."""
    def __init__(self, name, csvid, d, p, variant, rows, oracle):
        from recipes import card, step, observe, cost_from_oracle
        self.card = card; self.step = step; self.observe = observe; self.cost = cost_from_oracle
        self.name = name; self.csvid = csvid; self.d = d; self.p = p; self.rows = rows; self.oracle = oracle
        self.mode, owner = variant.rsplit('_', 1); self.owner = int(owner); self.requested = variant
        self.steps = []; self.setup = []; self.entities = {}; self.tokens = []; self.bound = set(); self.base = {}
        self.e = dict(draw=0, discard=0, mill=False, self_counters=0); self.keys = set()
        self.boundary = False; self.optional = False; self.discard_nondeterministic = False
        self.zones = [{z: [] for z in ('hand', 'graveyard', 'library', 'exile')} for _ in (0, 1)]
        self.life = [20, 20]; self.mana = [60, 60]; self.pools = [[10]*6, [10]*6]
        self.aura_target = None
        mode = self.mode; rule = p['rule']
        self.casting = mode in ('cast', 'payment', 'insufficient_mana', 'wrong_color', 'fresh_attack')
        self.actor = 1 if mode.startswith('defend') or (rule == 'support' and mode in (
            'allowed_block', 'rejected_block', 'tapped_block', 'ready_snapshot', 'declared_snapshot', 'damage_snapshot',
            'support_other_controller', 'support_removed', 'support_second_copy', 'support_tapped')) else 0
        for player in (0, 1):
            self.zones[player].update(hand=['Forest', 'Plains'], graveyard=['Island'], exile=['Mountain'], library=list(LIBRARY))
            for zone, names in self.zones[player].items():
                self.setup += [card(n, f'stock_{zone}_{player}_{i}', player, zone) for i, n in enumerate(names)]
        self.add(name, 'source', 0, 'hand' if self.casting else 'battlefield', ['tapped'] if mode == 'tapped_attack' else None)
        opponent = self.opponent_name()
        self.add(opponent, 'opponent', 1, state=['tapped'] if mode == 'tapped_block' and self.actor == 0 else None)
        if mode == 'tapped_block' and self.actor == 1:
            next(c for c in self.setup if c['ref'] == 'source')['state'] = ['tapped']; self.entities['source']['tapped'] = True
        if mode == 'other_attacker': self.add('Grizzly Bears', 'idle')
        if rule == 'defender_artifact':
            if mode in ('rejected_block', 'artifact_both', 'artifact_remove'):
                self.add('Glasses of Urza', 'gate_artifact', 1)
            if mode in ('artifact_own', 'artifact_both', 'artifact_own_cast'):
                self.add('Glasses of Urza', 'own_artifact', 0, 'hand' if mode == 'artifact_own_cast' else 'battlefield')
        if rule == 'support' and self.actor == 1:
            if mode != 'rejected_block':
                self.add(name if mode == 'support_second_copy' else 'Kragma Butcher', 'support',
                         1 if mode == 'support_other_controller' else 0,
                         state=['tapped'] if mode == 'support_tapped' else None)
        if rule == 'untapped_land': self.add_gate_lands()
        helper = 'Giant Growth' if mode in ('pump', 'cleanup', 'blocker_pump', 'source_pump_boundary', 'land_tap', 'defend_land_tap') else 'Unsummon' if mode in ('source_bounce', 'support_removed') else 'Shatter' if mode == 'artifact_remove' else None
        if helper:
            player = 1 if mode in ('blocker_pump', 'land_tap') else 0
            self.add(helper, 'helper', player, 'hand')
        if mode == 'payment': self.pools[0] = list(p['mana'])
        if mode == 'insufficient_mana': self.pools[0] = [0]*6
        if mode == 'wrong_color': self.pools[0] = [60, 0, 0, 0, 0, 0]
        self.emit('turn', player=self.actor, value=0); self.emit('present_ai_dialogs', value=1)
        for player in (0, 1): self.set_pool(player, self.pools[player])
        self.emit('refresh')
        self.ready = len(self.steps)

    def opponent_name(self):
        p = self.p; mode = self.mode
        if self.actor == 1 or mode in ('other_attacker', 'unblocked', 'empty_combat', 'pump', 'cleanup', 'two_combats', 'source_bounce') or self.casting:
            return 'Grizzly Bears'
        if p['rule'] == 'allow':
            first, second = p['predicates']
            if mode == 'exception_second': return 'Savannah Lions' if second.startswith('color:') else 'Ornithopter'
            if mode == 'exception_both': return 'Court Homunculus' if second.startswith('color:') else 'Wall of Denial'
            if mode in ('rejected_block', 'neither'): return 'Grizzly Bears' if mode == 'neither' else 'Giant Spider' if second == 'keyword:Flying' else 'Grizzly Bears'
            return 'Ornithopter' if first == 'artifact' else 'Wall of Roots'
        if p['rule'] == 'deny':
            return 'Wall of Roots' if mode == 'rejected_block' else 'Wall of Denial' if mode == 'wall_flying' else 'Ornithopter' if mode == 'flying_nonwall' else 'Grizzly Bears'
        if p['rule'] in ('power_fixed', 'power_source'):
            if mode == 'power_zero': return 'Ornithopter'
            if mode in ('power_one', 'blocker_pump'): return 'Orcish Artillery'
            if mode == 'rejected_block': return 'Grizzly Bears' if p['rule'] == 'power_fixed' else 'Orcish Artillery'
            return 'Centaur Courser' if p['rule'] == 'power_fixed' else 'Grizzly Bears'
        return 'Grizzly Bears'

    def add_gate_lands(self):
        mode = self.mode
        if mode in ('cast', 'payment', 'insufficient_mana', 'wrong_color', 'fresh_attack', 'unblocked', 'empty_combat', 'other_attacker',
                    'pump', 'cleanup', 'two_combats', 'source_bounce', 'attack_no_land', 'defend_no_land'): return
        player = 0 if self.actor == 1 else 1
        if mode in ('attack_own_untapped', 'defend_enemy_untapped'): player = 1-player
        untapped = mode in ('rejected_block', 'defend_untapped', 'attack_own_untapped', 'defend_enemy_untapped', 'land_tap', 'defend_land_tap', 'two_lands')
        self.add('Forest', 'gate_land', player, state=None if untapped else ['tapped'])
        if mode == 'two_lands': self.add('Forest', 'other_land', player, state=['tapped'])

    def has(self, ref, predicate):
        record = self.oracle[self.entities[ref]['name']]
        if predicate == 'artifact': return 'Artifact' in record['type_line']
        kind, value = predicate.split(':')
        if kind == 'color': return value in record['colors']
        if kind == 'keyword': return value in record['keywords']
        return value in re.split(r'\W+', record['type_line']) or 'Changeling' in record['keywords']

    def attack_legal(self, ref):
        e = self.entities[ref]
        if e['tapped'] or self.mode == 'fresh_attack' and ref == 'source': return False
        if ref != 'source' or self.p['rule'] != 'untapped_land': return True
        return not any(x['zone'] == 'battlefield' and x['player'] == 1 and 'Land' in self.oracle[x['name']]['type_line'] and not x['tapped'] for x in self.entities.values())

    def block_legal(self, blocker, attacker):
        b = self.entities[blocker]; a = self.entities[attacker]
        if b['tapped']: return False
        if attacker != 'source' and blocker != 'source': return True
        rule = self.p['rule']
        if attacker == 'source':
            if rule in ('allow', 'deny'):
                matched = any(self.has(blocker, pred) for pred in self.p['predicates'])
                return matched if rule == 'allow' else not matched
            if rule == 'power_fixed': return self.power(blocker) > self.p['limit']
            if rule == 'power_source': return self.power(blocker) >= self.power('source')
            if rule == 'defender_artifact':
                return not any(e['player'] == 1 and e['zone'] == 'battlefield' and 'Artifact' in self.oracle[e['name']]['type_line'] for e in self.entities.values())
        if blocker == 'source':
            if rule == 'support':
                return any(ref != 'source' and e['player'] == 0 and e['zone'] == 'battlefield' and self.has(ref, 'subtype:' + self.p['subtype']) for ref, e in self.entities.items())
            if rule == 'untapped_land':
                return not any(e['player'] == 0 and e['zone'] == 'battlefield' and 'Land' in self.oracle[e['name']]['type_line'] and not e['tapped'] for e in self.entities.values())
        return True

    def damage_to(self, target, amount):
        e = self.entities[target]; e['damage'] += amount
        if e['damage'] >= e['toughness'] + e['boost']: self.move(target, 'graveyard')

    def combat(self):
        mode = self.mode; attacker = 'opponent' if self.actor == 1 else 'idle' if mode == 'other_attacker' else 'source'
        blocker = 'source' if self.actor == 1 else 'opponent'
        self.emit('begin_attackers'); attacking = False
        if mode != 'empty_combat':
            attacking = self.attack_legal(attacker)
            self.emit('attack', ref=attacker, value=int(attacking))
            if attacking: self.entities[attacker]['tapped'] = True
        self.emit('finish_attackers'); self.emit('begin_blockers'); blocked = False
        if attacking and mode not in ('unblocked', 'pump', 'cleanup', 'two_combats'):
            blocked = self.block_legal(blocker, attacker)
            self.emit('block', ref=blocker, other=attacker, value=int(blocked))
            self.emit('assert_card', ref=blocker, property='blocking_count', value=int(blocked))
        self.emit('advance_combat')
        if attacking: self.emit('assert_card', ref=attacker, property='blocked', value=int(blocked))
        self.receipts(); declared = len(self.steps)
        if attacking:
            if blocked:
                attack_power, block_power = self.power(attacker), self.power(blocker)
                self.damage_to(blocker, attack_power); self.damage_to(attacker, block_power)
            else: self.life[1-self.actor] -= self.power(attacker)
        self.emit('combat_damage'); self.emit('resolve'); self.receipts(); damaged = len(self.steps)
        self.emit('end_combat'); self.receipts()
        return declared, damaged

    def return_turn(self):
        for who in (0, 1): self.set_pool(who, [0]*6)
        self.emit('advance_turn'); self.emit('untap_step', player=1-self.actor)
        self.emit('advance_turn'); self.emit('untap_step', player=self.actor); self.emit('turn', player=self.actor, value=0)
        for ref, e in self.entities.items():
            if e['zone'] == 'battlefield': e.update(tapped=False, boost=0, damage=0)
        for who in (0, 1): self.set_pool(who, [10]*6)

    def run(self):
        mode = self.mode; declared = damaged = self.ready
        if self.casting:
            if mode in ('insufficient_mana', 'wrong_color'):
                self.emit('cast', ref='source', value=0); self.emit('assert_card', ref='source', property='in_hand', value=1)
            else:
                self.paid('source'); self.emit('assert_card', ref='source', property='sick', value=1)
        if mode in ('pump', 'cleanup', 'source_pump_boundary', 'blocker_pump'):
            target = 'opponent' if mode == 'blocker_pump' else 'source'
            self.paid('helper', target); self.entities[target]['boost'] += 3
        if mode in ('land_tap', 'defend_land_tap'):
            land = self.entities['gate_land']; who = land['player']
            self.emit('can_activate', ref='gate_land', value=1); self.emit('activate', ref='gate_land', value=1)
            land['tapped'] = True; self.pools[who][3] += 1; self.mana[who] += 1
            self.emit('assert_stack', value=0); self.emit('assert_card', ref='gate_land', property='tapped', value=1)
            self.emit('assert_mana', player=who, color=3, value=11); self.emit('assert_mana_total', player=who, value=61)
            target = 'source' if who == 0 else 'opponent'
            self.paid('helper', target); self.entities[target]['boost'] += 3
        if mode == 'artifact_own_cast': self.paid('own_artifact')
        if mode == 'artifact_remove': self.paid('helper', 'gate_artifact'); self.move('gate_artifact', 'graveyard')
        if mode == 'support_removed': self.paid('helper', 'support'); self.move('support', 'hand')
        if mode == 'source_bounce':
            self.paid('helper', 'source'); self.move('source', 'hand')
        elif not self.casting or mode == 'fresh_attack':
            declared, damaged = self.combat()
            if mode == 'two_combats': self.return_turn(); self.combat()
            if mode == 'cleanup':
                for who in (0, 1): self.set_pool(who, [0]*6)
                self.emit('cleanup'); self.emit('refresh')
                for e in self.entities.values(): e.update(boost=0, damage=0)
        self.receipts(); steps = self.steps
        if mode in ('ready_snapshot', 'declared_snapshot', 'damage_snapshot'):
            cut = self.ready if mode == 'ready_snapshot' else declared if mode == 'declared_snapshot' else damaged
            replay = copy.deepcopy(steps[cut:])
            steps = copy.deepcopy(steps[:cut]) + [self.step('capture')] + replay + [self.step('record_state'), self.step('ai_roundtrip'), self.step('restore')] + copy.deepcopy(replay) + [self.step('assert_recorded_state')]
        case = dict(name=f'ConditionalBlockingLegalityRecipe_{self.csvid}_{self.requested}', card=self.name, fixture='empty_main', seed=123,
                    setup=self.setup, steps=steps, _recipe=dict(family=FAMILY, version=2, variant=self.requested,
                    params=copy.deepcopy(self.d['params']), oracle_text=self.d['oracle_text'], untested_components=[
                    'No invented response window inside a legality query; environmental changes precede actual declaration.',
                    'Mandatory attack/block completeness, multiplayer defenders, type/color replacement effects and control changes remain later compositions.']))
        if self.owner: _mirror(case)
        return case


def expand(name, csvid, declaration, p, selected, rows, oracle):
    validate_witnesses(oracle, rows)
    if not isinstance(selected, list) or not selected or len(set(selected)) != len(selected) or any(v not in variants(p) for v in selected):
        raise ValueError('distinct supported conditional permission variants required')
    return [PermissionBoard(name, csvid, declaration, p, v, rows, oracle).run() for v in selected]


# Exact follower composition is appended. The accepted pilot implementation,
# source records, variants and generated bodies remain unchanged.
_pilot_parameters = parameters
_pilot_spec = spec
_pilot_variants = variants
_pilot_expand = expand
PILOT_IDS = frozenset((8476, 622, 4566, 16761, 12507, 6742, 13711, 4804))


def parameters(record):
    try:
        return _pilot_parameters(record)
    except ValueError:
        # Reuse the original fixed-creature/full-keyword gate without accepting
        # any unknown printed clause.
        _pilot_parameters(dict(record, oracle_text="This creature can't be blocked by Walls."))
    text = record['oracle_text']
    if text == "This creature can't be blocked except by black creatures.":
        return dict(rule='allow', predicates=['color:B'])
    if text == "This creature can't be blocked except by Rogues.":
        p = dict(rule='allow', predicates=['subtype:Rogue'])
        pairs = re.findall(r'\{([WUBRG])/([WUBRG])\}', record['mana_cost'])
        if pairs: p.update(hybrid_pairs=[list(pair) for pair in pairs], payment_colors=[a for a, _ in pairs])
        return p
    if text == "This creature can't be blocked except by creatures with flying or reach.":
        return dict(rule='allow', predicates=['keyword:Flying', 'keyword:Reach'])
    if text == "This creature can't block unless you control a Vampire.":
        return dict(rule='support', subtype='Vampire', another=False)
    if text == "This creature can't block unless you control another Wolf or Werewolf.":
        return dict(rule='support', subtypes=['Wolf', 'Werewolf'], another=True)
    raise ValueError('unreviewed whole follower permission clause')


def spec(declaration, record):
    p = parameters(record)
    if not p.get('hybrid_pairs'): return _pilot_spec(declaration, record)
    from defensive_creature_recipes import reviewed_payment
    pin = profiles()['sources'].get(record['name'])
    if (declaration.get('family') != FAMILY or type(declaration.get('version')) is not int or declaration['version'] != 2
            or not pin or pin['oracle'] != record or pin['params'] != p
            or declaration.get('oracle_text') != record['oracle_text']
            or json.dumps(declaration.get('params'), sort_keys=True) != json.dumps(p, sort_keys=True)):
        raise ValueError('full pinned hybrid source and exact printed choices required')
    mana, pairs = reviewed_payment(record, p)
    if [list(pair) for pair in pairs] != p['hybrid_pairs']: raise ValueError('hybrid pairs differ from printed symbols')
    p.update(family=FAMILY, mana=mana, base_power=int(record['power']), base_toughness=int(record['toughness']))
    chosen = declaration.get('variants')
    if not isinstance(chosen, list) or not chosen or len(set(chosen)) != len(chosen) or any(v not in variants(p) for v in chosen):
        raise ValueError('explicit distinct hybrid permission variants required')
    return p


def variants(p):
    result = _pilot_variants(p)
    if p['rule'] == 'allow' and len(p['predicates']) == 1:
        result = [v for v in result if not v.startswith(('exception_second_', 'exception_both_'))]
    extra = []
    if p.get('hybrid_pairs'): extra += ['hybrid_other', 'hybrid_mixed']
    if p.get('subtypes'): extra += ['support_alternative']
    if p['rule'] == 'deny' and p['predicates'] == ['artifact']: extra += ['defend_artifact']
    return result + [mode + '_' + str(owner) for owner in (0, 1) for mode in extra]


class FollowerPermissionBoard(PermissionBoard):
    def __init__(self, name, csvid, d, p, variant, rows, oracle):
        requested = variant; mode, owner = variant.rsplit('_', 1)
        self.hybrid_mana = None
        if p.get('hybrid_pairs'):
            from defensive_creature_recipes import reviewed_payment
            choices = [pair[1] if mode == 'hybrid_other' or mode == 'hybrid_mixed' and i % 2 else pair[0] for i, pair in enumerate(p['hybrid_pairs'])]
            self.hybrid_mana, _ = reviewed_payment(oracle[name], dict(payment_colors=choices))
            p = dict(p, mana=self.hybrid_mana)
            if mode in ('hybrid_other', 'hybrid_mixed'): variant = 'payment_' + owner
        super().__init__(name, csvid, d, p, variant, rows, oracle)
        self.requested = requested; self.mode = mode
        if self.hybrid_mana:
            ordinary_cost = self.cost; raw_cost = oracle[name]['mana_cost']
            self.cost = lambda record: list(self.hybrid_mana) if record.get('mana_cost') == raw_cost else ordinary_cost(record)
            if self.owner == 1 and mode in ('cast', 'fresh_attack'):
                # The AI may choose any legal affordable vector. Offer just
                # the pinned route, retaining exact pools without normalizing
                # a legal AI choice from two funded alternatives.
                selected = set(p['payment_colors'])
                for pair in p['hybrid_pairs']:
                    for color in pair:
                        if color not in selected:
                            c = {'B': 1, 'U': 2, 'G': 3, 'R': 4, 'W': 5}[color]
                            self.pools[0][c] = 0
                            for step in self.steps:
                                if step['op'] == 'set_mana' and step.get('player') == 0 and step['color'] == c: step['value'] = 0
                self.mana[0] = sum(self.pools[0])
        if p['rule'] == 'support' and (p.get('subtype') != 'Minotaur') and 'support' in self.entities:
            ref = 'support'; e = self.entities[ref]
            supporter = 'Vampire Nighthawk' if p.get('subtype') == 'Vampire' else 'Greater Werewolf' if mode == 'support_alternative' else 'Young Wolf'
            if mode == 'support_second_copy': supporter = name
            e.update(name=supporter, power=int(oracle[supporter]['power']), toughness=int(oracle[supporter]['toughness']))
            self.base[ref] = (e['power'], e['toughness'])
            next(c for c in self.setup if c['ref'] == ref)['name'] = supporter
        if p.get('subtypes') and mode == 'support_alternative':
            # This mode is not a legacy constructor mode, so it deliberately
            # uses the same real defending fixture as allowed_block.
            self.actor = 1; self.steps[0]['player'] = 1
            if 'support' not in self.entities: self.add('Greater Werewolf', 'support')
        if p['rule'] == 'allow' and p['predicates'] == ['keyword:Flying', 'keyword:Reach'] and mode == 'exception_both':
            self.add('Spidery Grasp', 'reach_grant', 1, 'hand')
            e = self.entities['opponent']; e['tapped'] = True
            next(c for c in self.setup if c['ref'] == 'opponent')['state'] = ['tapped']
        self.ready = len(self.steps)

    def opponent_name(self):
        p = self.p; mode = self.mode
        if mode == 'defend_artifact': return 'Iron Myr'
        ordinary = (self.actor == 1 or mode in ('other_attacker', 'unblocked', 'empty_combat', 'pump', 'cleanup', 'two_combats', 'source_bounce') or self.casting)
        if ordinary: return super().opponent_name()
        if p['rule'] == 'allow':
            if p['predicates'] == ['color:B']:
                return 'Dreg Mangler' if mode not in ('rejected_block', 'neither') else 'Ornithopter' if mode == 'rejected_block' else 'Grizzly Bears'
            if p['predicates'] == ['subtype:Rogue']:
                return 'Invisible Stalker' if mode not in ('rejected_block', 'neither') else 'Ornithopter' if mode == 'rejected_block' else 'Grizzly Bears'
            if p['predicates'] == ['artifact', 'color:R']:
                if mode == 'exception_second': return 'Orcish Artillery'
                if mode == 'exception_both': return 'Etherium-Horn Sorcerer'
            if p['predicates'] == ['keyword:Flying', 'keyword:Reach']:
                return 'Giant Spider' if mode == 'exception_second' else 'Grizzly Bears' if mode in ('rejected_block', 'neither') else 'Ornithopter'
        if p['rule'] == 'deny' and p['predicates'] != ['subtype:Wall']:
            if p['predicates'] == ['keyword:Flying']:
                return 'Ornithopter' if mode in ('rejected_block', 'flying_nonwall') else 'Wall of Denial' if mode == 'wall_flying' else 'Grizzly Bears'
            return 'Iron Myr' if mode == 'rejected_block' else 'Wall of Denial' if mode == 'wall_flying' else 'Ornithopter' if mode == 'flying_nonwall' else 'Grizzly Bears'
        return super().opponent_name()

    def block_legal(self, blocker, attacker):
        if blocker == 'source' and self.p['rule'] == 'support' and self.p.get('subtype') != 'Minotaur':
            if self.entities[blocker]['tapped']: return False
            subtypes = self.p.get('subtypes', [self.p.get('subtype')])
            return any((not self.p['another'] or ref != 'source') and e['player'] == 0 and e['zone'] == 'battlefield'
                       and any(self.has(ref, 'subtype:' + subtype) for subtype in subtypes) for ref, e in self.entities.items())
        return super().block_legal(blocker, attacker)

    def paid(self, ref, target=None):
        if ref == 'source' and self.hybrid_mana and self.owner == 0:
            from defensive_creature_recipes import reviewed_payment
            from itertools import product
            vectors = {tuple(reviewed_payment(self.oracle[self.name], dict(payment_colors=list(colors)))[0]) for colors in product(*self.p['hybrid_pairs'])}
            affordable = lambda vec: all(a >= b for a, b in zip(self.pools[0], vec))
            if sum(map(affordable, vectors)) > 1:
                self.emit('choose_payment', player=0, value=sum(n << (4*c) for c, n in enumerate(self.hybrid_mana)))
        super().paid(ref, target)

    def run(self):
        if 'reach_grant' in self.entities:
            self.paid('reach_grant', 'opponent')
            e = self.entities['opponent']; e.update(tapped=False, power=e['power']+2, toughness=e['toughness']+4)
            self.emit('assert_card', ref='opponent', property='tapped', value=0)
            self.emit('assert_card', ref='opponent', property='power', value=2)
            self.emit('assert_card', ref='opponent', property='toughness', value=6)
        return super().run()


def expand(name, csvid, declaration, p, selected, rows, oracle):
    if csvid in PILOT_IDS: return _pilot_expand(name, csvid, declaration, p, selected, rows, oracle)
    validate_witnesses(oracle, rows)
    if not isinstance(selected, list) or not selected or len(set(selected)) != len(selected) or any(v not in variants(p) for v in selected):
        raise ValueError('distinct supported follower permission variants required')
    return [FollowerPermissionBoard(name, csvid, declaration, p, v, rows, oracle).run() for v in selected]
