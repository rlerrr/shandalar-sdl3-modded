"""Whole-Oracle Island, Phyrexian, affinity and Dethrone/evasion composition."""
import copy
import hashlib
import json
from pathlib import Path
import re

from paid_unblockability_activation_recipes import UnblockBoard, parameters as paid_parameters, variants as paid_variants

FAMILY = 'unblockability_composition'
PROFILE_PATH = Path(__file__).parent / 'data/unblockability_composition_wave8_profiles.json'


def profiles(): return json.loads(PROFILE_PATH.read_text(encoding='utf8'))


def parameters(record):
    text = record.get('oracle_text', ''); island = False; phyrexian = None; affinity = False; dethrone = False; haste = False
    keys = []
    island_line = 'This creature gets +1/+1 as long as you control an Island.\n'
    affinity_line = 'Affinity for artifacts (This spell costs {1} less to cast for each artifact you control.)\n'
    dethrone_line = 'Dethrone (Whenever this creature attacks the player with the most life or tied for most life, put a +1/+1 counter on it.)\n'
    if text.startswith(island_line): text = text[len(island_line):]; island = True
    elif text.startswith(affinity_line): text = text[len(affinity_line):]; affinity = True; keys = ['Affinity']
    elif text.startswith(dethrone_line):
        text = text[len(dethrone_line):]; dethrone = True; haste = True; keys = ['Dethrone']
        text = text.replace(' gains haste until end of turn and can\'t be blocked this turn.', " can't be blocked this turn.")
    elif text == "{U/P}: This creature can't be blocked this turn. ({U/P} can be paid with either {U} or 2 life.)":
        text = "{U}: This creature can't be blocked this turn."; phyrexian = 'U'
    else: raise ValueError('one complete reviewed adjacent clause composition required')
    if record.get('keywords') != keys: raise ValueError('whole composition keyword metadata required')
    paid = paid_parameters(dict(record, oracle_text=text, keywords=[]))
    if paid['tap'] or paid['target_power_max'] is not None or paid['blocker_power_max'] is not None or paid['snake_bonus'] != [0, 0]:
        raise ValueError('unreviewed adjacent paid component')
    if (haste and paid['scope'] != 'own') or (not haste and paid['scope'] != 'self'):
        raise ValueError('complete printed source/own target scope required')
    return dict(paid=paid, island_bonus=island, phyrexian_color=phyrexian,
                affinity_artifacts=affinity, dethrone=dethrone, grants_haste=haste)


def spec(declaration, record):
    from recipes import cost_from_oracle
    params = parameters(record); pin = profiles()['sources'].get(record['name'])
    if (declaration.get('family') != FAMILY or type(declaration.get('version')) is not int or declaration['version'] != 2
            or not pin or pin['oracle'] != record or pin['params'] != params
            or declaration.get('oracle_text') != record['oracle_text']
            or json.dumps(declaration.get('params'), sort_keys=True) != json.dumps(params, sort_keys=True)):
        raise ValueError('whole pinned Oracle and exact typed composition parameters required')
    p = copy.deepcopy(params['paid']); p.update({k: v for k, v in params.items() if k != 'paid'})
    p.update(family=FAMILY, mana=cost_from_oracle(record), activation_mana=cost_from_oracle(dict(mana_cost=p['activation_cost'])))
    selected = declaration.get('variants')
    if not isinstance(selected, list) or not selected or len(set(selected)) != len(selected) or any(v not in variants(p) for v in selected):
        raise ValueError('explicit distinct composition variants required')
    return p


def variants(p):
    paid = p.get('paid', p)
    result = paid_variants(paid)
    if p['phyrexian_color']:
        result = [v for v in result if not v.startswith(('cast_wrong_color_', 'activation_insufficient_', 'activation_wrong_color_'))]
    modes = ['countered_cast']
    if p['island_bonus']: modes += ['island_own', 'island_opponent', 'island_two', 'island_tapped', 'island_dual', 'island_removed', 'island_expiry', 'island_pending_snapshot']
    if p['phyrexian_color']: modes += ['pay_blue', 'pay_life', 'pay_cancel', 'blue_low_life', 'insufficient_life', 'life_repeat', 'life_pending_snapshot']
    if p['affinity_artifacts']: modes += ['affinity_one', 'affinity_three', 'affinity_floor', 'affinity_overflow', 'affinity_opponent',
        'affinity_hand', 'affinity_tapped', 'affinity_creature', 'affinity_removed', 'affinity_blue_short', 'affinity_wrong_color', 'affinity_activation', 'affinity_countered', 'affinity_cast_snapshot']
    if p['dethrone']: modes += ['dethrone_tied', 'dethrone_behind', 'dethrone_ahead', 'dethrone_unactivated', 'dethrone_repeat',
        'dethrone_other', 'haste_fresh_ally', 'haste_expiry', 'haste_fresh_self', 'haste_pending_snapshot', 'dethrone_declared_snapshot']
    result += [mode+'_'+str(owner) for owner in (0, 1) for mode in modes]
    if p['phyrexian_color']: result += ['life_low_0', 'life_boundary_query_0']
    return result


def validate_witnesses(oracle, rows):
    data = profiles(); root = Path(__file__).resolve().parents[4]
    missing = {r['csvid'] for r in json.loads((PROFILE_PATH.parent/'unimplemented.json').read_text(encoding='utf8'))['cases'].values()}
    for name, record in data['witnesses'].items():
        row = rows.get(name, {})
        if (oracle.get(name) != record or row.get('kind') != 'card' or row.get('handler') != data['witness_handlers'][name]
                or row.get('handler') in (None, 'NULL', '0')
                or name not in data['native_characteristics_only_witnesses'] and row.get('csvid') in missing
                or 'dummy' in row['handler'] and name not in data['native_characteristics_only_witnesses']):
            raise ValueError('composition witness changed: '+name)
    for name, pin in data['sources'].items():
        row = rows.get(name, {}); source = pin['compiled_source']
        if (oracle.get(name) != pin['oracle'] or row.get('kind') != 'card' or row.get('csvid') != pin['csvid'] or row.get('handler') != pin['handler']
                or row.get('csvid') in missing or 'dummy' in pin['handler'] or hashlib.sha256((root/source['path']).read_bytes()).hexdigest() != source['sha256']):
            raise ValueError('implemented whole composition source changed: '+name)


class CompositionBoard(UnblockBoard):
    def __init__(self, name, csvid, d, p, variant, rows, oracle):
        self.original_mode, owner = variant.rsplit('_', 1); original = self.original_mode
        self.fresh_refs = set(); self.island_applied = 0
        op = {'countered_cast': 'cast', 'island_expiry': 'expiry_combat', 'life_repeat': 'repeat_activation',
              'life_pending_snapshot': 'pending_snapshot', 'island_pending_snapshot': 'pending_snapshot',
              'haste_pending_snapshot': 'pending_snapshot', 'dethrone_declared_snapshot': 'declared_snapshot',
              'dethrone_unactivated': 'unactivated', 'dethrone_other': 'other_attacker'}.get(original, 'activated' if original.startswith(('island_', 'pay_', 'blue_', 'insufficient_', 'life_', 'dethrone_', 'haste_')) else original)
        if original.startswith('affinity_'):
            op = 'activation_payment' if original == 'affinity_activation' else 'cast_insufficient' if original in ('affinity_blue_short', 'affinity_wrong_color') else 'cast_payment'
        super().__init__(name, csvid, d, p, op+'_'+owner, rows, oracle)
        self.requested = variant
        if p['island_bonus'] and original.startswith('island_'):
            self.add('Underground Sea' if original == 'island_dual' else 'Island', 'island_0', 1 if original == 'island_opponent' else 0,
                     state=['tapped'] if original == 'island_tapped' else None)
            if original == 'island_two': self.add('Island', 'island_1')
            if original == 'island_removed': self.add('Stone Rain', 'land_remove', 0, 'hand')
        if p['affinity_artifacts'] and original.startswith('affinity_'):
            n = 1 if original in ('affinity_one', 'affinity_creature') else 3 if original in ('affinity_three', 'affinity_hand', 'affinity_tapped', 'affinity_removed') else 7 if original == 'affinity_overflow' else 5
            for i in range(n):
                self.add('Ornithopter' if original == 'affinity_creature' else 'Glasses of Urza', 'artifact_'+str(i),
                    1 if original == 'affinity_opponent' else 0, 'hand' if original == 'affinity_hand' else 'battlefield',
                    ['tapped'] if original == 'affinity_tapped' else None)
            if original == 'affinity_removed': self.add('Shatter', 'artifact_remove', 0, 'hand')
        if original in ('countered_cast', 'affinity_countered'): self.add('Counterspell', 'counter', 1, 'hand')
        if p['dethrone'] and original.startswith('dethrone_') and original != 'dethrone_other': self.target = 'source'
        if original in ('haste_fresh_ally', 'haste_expiry', 'haste_pending_snapshot'):
            e = self.entities['ally']; e.update(name='Elvish Mystic', zone='hand', power=1, toughness=1)
            item = next(c for c in self.setup if c['ref'] == 'ally'); item.update(name='Elvish Mystic', zone='hand')
            self.zones[0]['hand'].append('Elvish Mystic'); self.base['ally'] = (1, 1)
            if original == 'haste_expiry': self.add('Twiddle', 'expiry_untap', 0, 'hand')
        if original == 'haste_fresh_self':
            self.target = 'source'; self.entities['source']['zone'] = 'hand'; self.zones[0]['hand'].append(name)
            next(c for c in self.setup if c['ref'] == 'source')['zone'] = 'hand'
        for e in self.entities.values(): e.setdefault('sick', False)
        if original in ('dethrone_behind', 'dethrone_repeat', 'dethrone_ahead', 'blue_low_life', 'insufficient_life', 'life_low', 'life_boundary_query'):
            self.life[0] = {'dethrone_behind': 18, 'dethrone_repeat': 10, 'dethrone_ahead': 21, 'life_low': 3, 'life_boundary_query': 2}.get(original, 1)
            self.emit('set_life', player=0, value=self.life[0])
        if original in ('pay_blue', 'pay_cancel', 'blue_low_life'): self.set_pool(0, [0, 0, 1, 0, 0, 0])
        if original in ('pay_life', 'life_repeat', 'life_pending_snapshot', 'life_low', 'life_boundary_query', 'insufficient_life'): self.set_pool(0, [0]*6)
        if p['affinity_artifacts'] and original.startswith('affinity_') and original != 'affinity_activation':
            wallet = self.cast_vector()
            if original == 'affinity_removed': wallet = [4, 0, 2, 0, 1, 0]
            if original == 'affinity_blue_short': wallet = [0, 0, 1, 0, 0, 0]
            if original == 'affinity_wrong_color': wallet = [10, 0, 0, 0, 0, 0]
            self.set_pool(0, wallet)
        if original == 'affinity_countered': self.set_pool(1, [0, 0, 2, 0, 0, 0])
        self.emit('refresh'); self.sync(); self.ready = len(self.steps)

    def sync(self):
        source = self.entities['source']
        active = int(self.p['island_bonus'] and source['zone'] == 'battlefield' and any(e['zone'] == 'battlefield' and e['player'] == 0
            and 'Island' in re.split(r'\W+', self.oracle[e['name']]['type_line']) for e in self.entities.values()))
        delta = active - self.island_applied; source['power'] += delta; source['toughness'] += delta; self.island_applied = active
        for ref in self.fresh_refs:
            self.entities[ref]['sick'] = not (self.p['grants_haste'] and self.applied and ref == self.target)

    def receipts(self):
        self.sync(); super().receipts()
        for ref in self.fresh_refs:
            if ref != 'source' and self.entities[ref]['zone'] == 'battlefield':
                self.emit('assert_card', ref=ref, property='sick', value=int(self.entities[ref]['sick']))

    def cast_vector(self):
        vector = list(self.p['mana'])
        if self.p['affinity_artifacts']:
            n = sum(e['player'] == 0 and e['zone'] == 'battlefield' and 'Artifact' in self.oracle[e['name']]['type_line'] for e in self.entities.values())
            vector[0] = max(0, vector[0]-n)
        return vector

    def spend_vector(self, vector, player=0):
        if any(a < b for a, b in zip(self.pools[player], vector)): raise ValueError('composition wallet cannot pay real selected vector')
        self.pools[player] = [a-b for a, b in zip(self.pools[player], vector)]; self.mana[player] = sum(self.pools[player])

    def paid(self, ref, target=None):
        if ref == 'source' and self.p['affinity_artifacts']:
            self.emit('cast', ref=ref, value=1); self.spend_vector(self.cast_vector())
            self.emit('assert_card', ref=ref, property='in_hand', value=0); self.emit('assert_stack', value=1)
            self.emit('assert_mana_total', player=0, value=self.mana[0]); self.pending = len(self.steps)
            self.emit('resolve'); self.move(ref, 'battlefield')
        else: super().paid(ref, target)
        if self.entities[ref]['creature']:
            self.fresh_refs.add(ref); self.entities[ref]['sick'] = True

    def activate(self, late=False):
        self.sync()
        if not self.p['phyrexian_color']: return super().activate(late)
        mode = self.original_mode
        life_choice = mode in ('pay_life', 'life_repeat', 'life_pending_snapshot', 'life_low')
        available = self.pools[0][2] >= 1 or self.life[0] >= 2
        self.emit('can_activate', ref='source', value=int(available))
        if not available:
            self.emit('activate', ref='source', value=0); self.receipts(); return False
        if self.life[0] >= 2: self.emit('choose_number', player=0, value=-1 if mode == 'pay_cancel' else int(life_choice))
        if mode == 'pay_cancel':
            self.emit('activate', ref='source', value=0); self.receipts(); return False
        self.emit('activate', ref='source', value=1)
        if life_choice: self.life[0] -= 2
        else: self.spend_vector([0, 0, 1, 0, 0, 0])
        self.emit('assert_stack', value=1); self.emit('assert_life', player=0, value=self.life[0])
        self.emit('assert_mana_total', player=0, value=self.mana[0])
        for color, value in enumerate(self.pools[0]): self.emit('assert_mana', player=0, color=color, value=value)
        self.pending = len(self.steps); self.emit('resolve'); self.applied = True
        self.receipts(); self.resolved = len(self.steps); return True

    def combat(self, expired=False):
        self.sync(); mode = self.mode
        attacker = 'idle' if mode == 'other_attacker' else 'source' if mode == 'fresh_attack' else self.target
        self.emit('begin_attackers'); attacking = False
        if mode != 'empty_combat':
            e = self.entities[attacker]; attacking = e['zone'] == 'battlefield' and not e['tapped'] and not e['sick']
            self.emit('attack', ref=attacker, value=int(attacking))
            if attacking:
                e['tapped'] = True
                if self.p['dethrone'] and attacker == 'source' and self.life[0] <= self.life[1]: e['counters'] += 1
        self.emit('finish_attackers')
        if mode == 'after_attackers' and not expired: self.activate(late=True)
        self.emit('begin_blockers'); blocked = False
        if attacking:
            blocked = not (self.applied and attacker == self.target)
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
        self.emit('combat_damage'); self.emit('resolve'); self.receipts(); self.emit('end_combat'); self.receipts()

    def return_turn(self):
        super().return_turn(); self.fresh_refs.clear(); self.applied = False; self.sync()

    def countered_cast(self):
        self.emit('cast', ref='source', value=1); self.spend_vector(self.cast_vector())
        self.emit('assert_card', ref='source', property='in_hand', value=0); self.emit('assert_stack', value=1)
        self.emit('assert_mana_total', player=0, value=self.mana[0])
        # Explicit spell-parent context supplies Counterspell's sole target.
        self.emit('cast_response', ref='counter', value=1); self.spend(self.oracle['Counterspell']['mana_cost'], 1)
        self.emit('assert_stack', value=2); self.emit('resolve_response')
        self.move('counter', 'graveyard'); self.move('source', 'graveyard'); return self.finish()

    def run(self):
        original = self.original_mode
        if original in ('countered_cast', 'affinity_countered'): return self.countered_cast()
        if original == 'life_boundary_query': self.emit('can_activate', ref='source', value=1); return self.finish()
        if original == 'island_removed': self.paid('land_remove', 'island_0'); self.move('island_0', 'graveyard'); self.sync()
        if original == 'affinity_removed': self.paid('artifact_remove', 'artifact_0'); self.move('artifact_0', 'graveyard')
        if original in ('haste_fresh_ally', 'haste_expiry', 'haste_pending_snapshot'): self.paid('ally'); self.emit('assert_card', ref='ally', property='sick', value=1)
        if original == 'haste_fresh_self': self.paid('source'); self.emit('assert_card', ref='source', property='sick', value=1)
        if original == 'dethrone_repeat':
            self.activate(); self.combat(); self.return_turn(); self.activate(); self.combat(); return self.finish()
        return super().run()

    def finish(self):
        if self.original_mode == 'haste_expiry' and not getattr(self, 'haste_expired', False):
            self.haste_expired = True
            self.emit('choose_target', player=0, ref='ally'); self.emit('cast', ref='expiry_untap', value=1)
            self.spend(self.oracle['Twiddle']['mana_cost']); self.emit('choose_dialog', player=0, value=1)
            self.emit('resolve'); self.move('expiry_untap', 'graveyard'); self.entities['ally']['tapped'] = False
            self.emit('assert_card', ref='ally', property='tapped', value=0)
            for who in (0, 1): self.set_pool(who, [0]*6)
            self.emit('cleanup'); self.emit('refresh'); self.applied = False
            for e in self.entities.values(): e.update(boost=0, damage=0)
            self.sync(); self.emit('assert_card', ref='ally', property='sick', value=1)
            self.emit('can_activate', ref='ally', value=0)
        if self.original_mode == 'affinity_cast_snapshot': self.mode = 'pending_snapshot'
        case = super().finish()
        case['name'] = case['name'].replace('PaidUnblockabilityActivationRecipe_', 'UnblockabilityCompositionRecipe_', 1)
        case['_recipe']['family'] = FAMILY
        case['_recipe']['untested_components'] = [
            'No fabricated activated-parent response; cast_response is used only for a real pending creature spell.',
            'Phyrexian pay-to-zero game loss has no winner observer here; Human life2 checks legal availability only. AI low-life safety suppression is strategy/noncoverage, never an Oracle prohibition or baseline.',
            'Island entry via normal land-play history and Dethrone condition changes inside a trigger response window remain separate compositions.'
        ]
        return case


def expand(name, csvid, declaration, p, selected, rows, oracle):
    validate_witnesses(oracle, rows)
    if not isinstance(selected, list) or not selected or len(set(selected)) != len(selected) or any(v not in variants(p) for v in selected):
        raise ValueError('distinct supported complete composition variants required')
    return [CompositionBoard(name, csvid, declaration, p, v, rows, oracle).run() for v in selected]
