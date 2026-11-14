"""Paid evasion activations proved by real target validation and combat."""
import copy
import hashlib
import json
import re
from pathlib import Path

from conditional_blocking_legality_recipes import PermissionBoard
from combat_value_trigger_recipes import _mirror
from grant_recipes import LIBRARY

FAMILY = 'paid_unblockability_activation'
PROFILE_PATH = Path(__file__).parent / 'data/paid_unblockability_activation_wave8_profiles.json'


def profiles(): return json.loads(PROFILE_PATH.read_text(encoding='utf8'))


def parameters(record):
    if (record.get('layout') != 'normal' or record.get('card_faces') or record.get('selected_face')
            or 'Creature' not in record.get('type_line', '') or record.get('keywords') != []
            or any(not re.fullmatch(r'[1-9]', record.get(k, '')) for k in ('power', 'toughness'))):
        raise ValueError('whole fixed normal activation creature required')
    text = record['oracle_text']; timing = 'any'
    restriction = ' Activate only during your turn, before attackers are declared.'
    if text.endswith(restriction): text = text[:-len(restriction)]; timing = 'own_before_attackers'
    match = re.fullmatch(r'([^:\n]+): (.+)', text)
    if not match: raise ValueError('one complete printed evasion ability required')
    cost, effect = match.groups(); tap = cost == '{T}' or cost.endswith(', {T}')
    mana = '' if cost == '{T}' else cost.removesuffix(', {T}')
    if (not mana and not tap) or mana and not re.fullmatch(r'(?:\{(?:[1-9]|[BUGRW])\})+', mana):
        raise ValueError('unreviewed activation cost')
    bonus = [0, 0]
    snake = ' If that creature is a Snake, it gets +2/+2 until end of turn.'
    if effect.endswith(snake): effect = effect[:-len(snake)]; bonus = [2, 2]
    if effect == "This creature can't be blocked this turn.": scope = 'self'; target_max = blocker_max = None
    else:
        m = re.fullmatch(r"Target creature( you control| with power 2 or less)? can't be blocked( by creatures with power 2 or less)? this turn\.", effect)
        if not m: raise ValueError('unreviewed full target/evasion clause')
        scope = 'own' if m[1] == ' you control' else 'any'
        target_max = 2 if m[1] == ' with power 2 or less' else None
        blocker_max = 2 if m[2] else None
    if bonus != [0, 0] and scope == 'self': raise ValueError('unreviewed self Snake clause')
    return dict(activation_cost=mana, tap=tap, scope=scope, timing=timing,
                target_power_max=target_max, blocker_power_max=blocker_max, snake_bonus=bonus)


def spec(declaration, record):
    from recipes import cost_from_oracle
    p = parameters(record); pin = profiles()['sources'].get(record['name'])
    if (declaration.get('family') != FAMILY or type(declaration.get('version')) is not int or declaration['version'] != 2
            or not pin or pin['oracle'] != record or pin['params'] != p
            or declaration.get('oracle_text') != record['oracle_text']
            or json.dumps(declaration.get('params'), sort_keys=True) != json.dumps(p, sort_keys=True)):
        raise ValueError('complete pinned Oracle and exact typed activation parameters required')
    p.update(family=FAMILY, mana=cost_from_oracle(record),
             activation_mana=cost_from_oracle(dict(mana_cost=p['activation_cost'])) if p['activation_cost'] else [0]*6)
    chosen = declaration.get('variants')
    if not isinstance(chosen, list) or not chosen or len(chosen) != len(set(chosen)) or any(v not in variants(p) for v in chosen):
        raise ValueError('distinct explicit evasion activation variants required')
    return p


def variants(p):
    modes = ['activated', 'unactivated', 'cast', 'cast_payment', 'cast_insufficient', 'cast_wrong_color',
             'fresh_attack', 'fresh_source', 'activation_payment', 'tapped_source', 'repeat_activation',
             'after_attackers', 'other_attacker', 'empty_combat', 'expiry_combat', 'ready_snapshot',
             'pending_snapshot', 'resolved_snapshot', 'declared_snapshot', 'shrouded_source']
    if p['activation_cost']: modes += ['activation_insufficient', 'activation_wrong_color']
    if p['tap']: modes += ['repeat_denied']
    if p['scope'] != 'self': modes += ['self_target', 'probe_self', 'probe_foreign',
        'probe_noncreature', 'probe_shroud', 'source_bounce', 'target_bounce']
    if p['scope'] == 'any': modes += ['foreign_target']
    if p['target_power_max'] is not None: modes += ['small_target', 'zero_target', 'probe_large', 'pump_before_probe', 'pumped_after_grant']
    if p['snake_bonus'] != [0, 0]: modes += ['snake_target', 'foreign_snake', 'snake_expiry', 'snake_repeat']
    if p['blocker_power_max'] is not None: modes += ['blocker_one', 'blocker_two', 'blocker_three', 'blocker_pump']
    if p['timing'] != 'any': modes += ['opposing_turn', 'late_main2']
    return [mode + '_' + str(owner) for owner in (0, 1) for mode in modes]


def validate_witnesses(oracle, rows):
    data = profiles(); root = Path(__file__).resolve().parents[4]
    missing = {r['csvid'] for r in json.loads((PROFILE_PATH.parent / 'unimplemented.json').read_text(encoding='utf8'))['cases'].values()}
    for name, record in data['witnesses'].items():
        row = rows.get(name, {})
        if (oracle.get(name) != record or row.get('kind') != 'card' or row.get('handler') != data['witness_handlers'][name]
                or row.get('handler') in (None, 'NULL', '0')
                or 'dummy' in row['handler'] and name not in data['native_characteristics_only_witnesses']):
            raise ValueError('paid evasion witness changed: ' + name)
    for name, pin in data['sources'].items():
        row = rows.get(name, {}); source = pin['compiled_source']
        if (oracle.get(name) != pin['oracle'] or row.get('csvid') != pin['csvid'] or row.get('kind') != 'card'
                or row.get('handler') != pin['handler'] or row.get('csvid') in missing or 'dummy' in pin['handler']
                or hashlib.sha256((root / source['path']).read_bytes()).hexdigest() != source['sha256']):
            raise ValueError('implemented paid evasion source changed: ' + name)


class UnblockBoard(PermissionBoard):
    def __init__(self, name, csvid, d, p, variant, rows, oracle):
        from recipes import card, step, observe, cost_from_oracle
        self.card = card; self.step = step; self.observe = observe; self.cost = cost_from_oracle
        self.name = name; self.csvid = csvid; self.d = d; self.p = p; self.rows = rows; self.oracle = oracle
        self.mode, owner = variant.rsplit('_', 1); self.owner = int(owner); self.requested = variant
        self.steps = []; self.setup = []; self.entities = {}; self.tokens = []; self.bound = set(); self.base = {}
        self.e = dict(draw=0, discard=0, mill=False, self_counters=0); self.keys = set()
        self.boundary = False; self.optional = False; self.discard_nondeterministic = False; self.aura_target = None
        self.zones = [{z: [] for z in ('hand', 'graveyard', 'library', 'exile')} for _ in (0, 1)]
        self.life = [20, 20]; self.mana = [60, 60]; self.pools = [[10]*6, [10]*6]
        self.applied = False; self.pending = self.resolved = self.declared = 0; mode = self.mode
        self.casting = mode in ('cast', 'cast_payment', 'cast_insufficient', 'cast_wrong_color', 'fresh_source', 'fresh_attack')
        foreign = mode in ('foreign_target', 'foreign_snake')
        self.actor = 1 if foreign and p['timing'] == 'any' or mode == 'opposing_turn' else 0
        for who in (0, 1):
            self.zones[who].update(hand=['Forest', 'Plains'], graveyard=['Island'], exile=['Mountain'], library=list(LIBRARY))
            for zone, names in self.zones[who].items():
                self.setup += [card(n, f'stock_{zone}_{who}_{i}', who, zone) for i, n in enumerate(names)]
        self.add(name, 'source', 0, 'hand' if self.casting else 'battlefield', ['tapped'] if mode == 'tapped_source' else None)
        target_name = 'Sakura-Tribe Elder' if mode in ('snake_target', 'foreign_snake', 'snake_expiry', 'snake_repeat', 'small_target') else 'Ornithopter' if mode == 'zero_target' else 'Centaur Courser' if mode == 'probe_large' else 'Grizzly Bears'
        if p['scope'] != 'self': self.add(target_name, 'ally', 1 if foreign else 0)
        self.target = 'source' if p['scope'] == 'self' or mode == 'self_target' else 'ally'
        blocker = {'blocker_one': 'Orcish Artillery', 'blocker_two': 'Grizzly Bears', 'blocker_three': 'Centaur Courser', 'blocker_pump': 'Grizzly Bears'}.get(mode, 'Wall of Denial')
        self.add(blocker, 'blocker', 1-self.actor)
        if mode == 'other_attacker': self.add('Grizzly Bears', 'idle')
        if mode == 'probe_foreign': self.add('Grizzly Bears', 'foreign', 1)
        if mode == 'probe_noncreature': self.add('Forest', 'noncreature', 1)
        if mode in ('shrouded_source', 'probe_shroud'): self.add('Diplomatic Immunity', 'cloak', 0, 'hand')
        if mode in ('source_bounce', 'target_bounce'): self.add('Unsummon', 'bounce', 0, 'hand')
        if mode in ('pumped_after_grant', 'pump_before_probe', 'blocker_pump'):
            self.add('Giant Growth', 'growth', 1 if mode == 'blocker_pump' else 0, 'hand')
        if p['tap'] and mode in ('repeat_activation', 'self_target'): self.add('Twiddle', 'untap', 0, 'hand')
        for e in self.entities.values(): e['sick'] = False
        if mode == 'cast_payment': self.pools[0] = list(p['mana'])
        if mode == 'cast_insufficient': self.pools[0] = [0]*6
        if mode == 'cast_wrong_color': self.pools[0] = [60, 0, 0, 0, 0, 0]
        if mode == 'activation_payment': self.pools[0] = list(p['activation_mana'])
        if mode == 'activation_insufficient': self.pools[0] = [0]*6
        if mode == 'activation_wrong_color': self.pools[0] = [60, 0, 0, 0, 0, 0]
        self.emit('turn', player=self.actor, value=0); self.emit('present_ai_dialogs', value=1)
        for who in (0, 1): self.set_pool(who, self.pools[who])
        self.emit('refresh'); self.ready = len(self.steps)

    def receipts(self):
        super().receipts()
        if self.entities['source']['zone'] == 'battlefield':
            self.emit('assert_card', ref='source', property='sick', value=int(self.entities['source']['sick']))

    def activation_legal(self, late=False):
        source = self.entities['source']
        if self.p['tap'] and (source['tapped'] or source['sick']): return False
        if self.p['timing'] == 'own_before_attackers' and (self.actor != 0 or late): return False
        return all(a >= b for a, b in zip(self.pools[0], self.p['activation_mana']))

    def activate(self, late=False):
        legal = self.activation_legal(late)
        self.emit('can_activate', ref='source', value=int(legal))
        if not legal:
            # No unsupported selector intent is queued beside an unavailable
            # source. Phase/turn negatives observe the real permission query.
            if self.mode not in ('after_attackers', 'opposing_turn', 'late_main2'): self.emit('activate', ref='source', value=0)
            self.receipts(); return False
        if self.p['scope'] != 'self': self.emit('choose_target', player=0, ref=self.target)
        self.emit('activate', ref='source', value=1)
        if self.p['activation_cost']: self.spend(self.p['activation_cost'])
        if self.p['tap']: self.entities['source']['tapped'] = True
        self.emit('assert_stack', value=1)
        self.emit('assert_card', ref='source', property='tapped', value=int(self.entities['source']['tapped']))
        for who in (0, 1):
            self.emit('assert_mana_total', player=who, value=self.mana[who])
            for color, value in enumerate(self.pools[who]): self.emit('assert_mana', player=who, color=color, value=value)
        self.emit('assert_card', ref=self.target, property='power', value=self.power(self.target))
        self.pending = len(self.steps)
        self.emit('resolve'); self.applied = True
        if self.p['snake_bonus'] != [0, 0] and 'Snake' in re.split(r'\W+', self.oracle[self.entities[self.target]['name']]['type_line']):
            self.entities[self.target]['boost'] += self.p['snake_bonus'][0]
        self.receipts(); self.resolved = len(self.steps)
        return True

    def untap_source(self):
        self.emit('choose_target', player=0, ref='source'); self.emit('cast', ref='untap', value=1)
        self.spend(self.oracle['Twiddle']['mana_cost']); self.emit('choose_dialog', player=0, value=1)
        self.emit('assert_stack', value=1); self.emit('assert_card', ref='untap', property='in_hand', value=0)
        self.emit('resolve'); self.move('untap', 'graveyard'); self.entities['source']['tapped'] = False
        self.emit('assert_card', ref='source', property='tapped', value=0); self.receipts()

    def target_probe(self):
        mode = self.mode; target = self.target; expected = 1
        if mode == 'probe_self': target = 'source'
        if mode == 'probe_foreign': target = 'foreign'; expected = int(self.p['scope'] == 'any')
        if mode == 'probe_noncreature': target = 'noncreature'; expected = 0
        if mode in ('probe_shroud', 'probe_large', 'pump_before_probe'): expected = 0
        self.emit('probe_activation_target', ref='source', other=target, index=-1, value=expected)
        self.receipts()

    def combat(self, expired=False):
        mode = self.mode
        attacker = 'idle' if mode == 'other_attacker' else 'source' if mode == 'fresh_attack' else self.target
        self.emit('begin_attackers'); attacking = False
        if mode != 'empty_combat':
            entity = self.entities[attacker]
            attacking = entity['zone'] == 'battlefield' and not entity['tapped'] and not entity['sick']
            self.emit('attack', ref=attacker, value=int(attacking))
            if attacking: entity['tapped'] = True
        self.emit('finish_attackers')
        if mode == 'after_attackers' and not expired: self.activate(late=True)
        self.emit('begin_blockers'); blocked = False
        if attacking:
            protected = self.applied and attacker == self.target
            blocked = not protected or self.p['blocker_power_max'] is not None and self.power('blocker') > self.p['blocker_power_max']
            self.emit('block', ref='blocker', other=attacker, value=int(blocked))
            self.emit('assert_card', ref='blocker', property='blocking_count', value=int(blocked))
        self.emit('advance_combat')
        if attacking: self.emit('assert_card', ref=attacker, property='blocked', value=int(blocked))
        self.receipts(); self.declared = len(self.steps)
        if attacking:
            if blocked:
                damage, back = self.power(attacker), self.power('blocker')
                self.damage_to('blocker', damage); self.damage_to(attacker, back)
            else: self.life[1-self.actor] -= self.power(attacker)
        self.emit('combat_damage'); self.emit('resolve'); self.receipts()
        self.emit('end_combat'); self.receipts()
        if mode == 'late_main2': self.emit('can_activate', ref='source', value=0)

    def run(self):
        mode = self.mode
        if self.casting:
            if mode in ('cast_insufficient', 'cast_wrong_color'):
                self.emit('cast', ref='source', value=0); self.emit('assert_card', ref='source', property='in_hand', value=1)
            else:
                self.paid('source'); self.entities['source']['sick'] = True
                self.emit('assert_card', ref='source', property='sick', value=1)
            if mode not in ('fresh_source', 'fresh_attack'): return self.finish()
        if mode in ('shrouded_source', 'probe_shroud'):
            self.paid('cloak', 'source' if mode == 'shrouded_source' else self.target)
        if mode in ('blocker_pump', 'pump_before_probe'):
            target = 'blocker' if mode == 'blocker_pump' else self.target
            self.paid('growth', target); self.entities[target]['boost'] += 3
        if mode.startswith('probe_') or mode == 'pump_before_probe':
            self.target_probe(); return self.finish()
        if mode == 'opposing_turn':
            self.emit('can_activate', ref='source', value=0); return self.finish()
        if mode not in ('unactivated', 'fresh_attack', 'after_attackers', 'late_main2'):
            success = self.activate()
            if success and mode in ('repeat_activation', 'snake_repeat'):
                if self.p['tap']: self.untap_source()
                self.activate()
            if success and mode == 'repeat_denied':
                self.emit('can_activate', ref='source', value=0); self.emit('activate', ref='source', value=0)
            if success and mode == 'self_target' and self.p['tap']: self.untap_source()
        if mode == 'pumped_after_grant': self.paid('growth', self.target); self.entities[self.target]['boost'] += 3
        if mode == 'source_bounce': self.paid('bounce', 'source'); self.move('source', 'hand')
        if mode == 'target_bounce':
            self.paid('bounce', self.target); self.move(self.target, 'hand'); self.applied = False
            return self.finish()
        if mode == 'foreign_target' and self.p['timing'] != 'any':
            # Zhuge may legally target an opposing creature on the caster's
            # own turn. That opponent cannot attack on this turn, and the
            # grant expires before their next turn. Do not flip turn/history.
            return self.finish()
        self.combat()
        if mode in ('expiry_combat', 'snake_expiry'):
            self.return_turn(); self.applied = False
            for e in self.entities.values(): e['sick'] = False
            self.combat(expired=True)
        return self.finish()

    def finish(self):
        self.receipts(); steps = self.steps; mode = self.mode
        if mode in ('ready_snapshot', 'pending_snapshot', 'resolved_snapshot', 'declared_snapshot'):
            cut = {'ready_snapshot': self.ready, 'pending_snapshot': self.pending, 'resolved_snapshot': self.resolved, 'declared_snapshot': self.declared}[mode]
            if not cut: raise ValueError('actual activation/declaration snapshot boundary absent')
            replay = copy.deepcopy(steps[cut:])
            steps = copy.deepcopy(steps[:cut]) + [self.step('capture')] + replay + [self.step('record_state'), self.step('ai_roundtrip'), self.step('restore')] + copy.deepcopy(replay) + [self.step('assert_recorded_state')]
        case = dict(name=f'PaidUnblockabilityActivationRecipe_{self.csvid}_{self.requested}', card=self.name, fixture='empty_main', seed=123,
            setup=self.setup, steps=steps, _recipe=dict(family=FAMILY, version=2, variant=self.requested,
            params=copy.deepcopy(self.d['params']), oracle_text=self.d['oracle_text'], untested_components=[
                'cast_response rejects pending activations; no invented source/target-change response window or fake priority.',
                'Zhuge foreign-target permission/cost is real; foreign attacking-evasion requires a separate legal control-change composition because the grant expires before the opponent turn.',
                'No mandatory combat participation, multiplayer or generalized type/color change claim.']))
        if self.owner: _mirror(case)
        return case


def expand(name, csvid, declaration, p, selected, rows, oracle):
    validate_witnesses(oracle, rows)
    if not isinstance(selected, list) or not selected or len(set(selected)) != len(selected) or any(v not in variants(p) for v in selected):
        raise ValueError('distinct supported paid evasion variants required')
    return [UnblockBoard(name, csvid, declaration, p, v, rows, oracle).run() for v in selected]
