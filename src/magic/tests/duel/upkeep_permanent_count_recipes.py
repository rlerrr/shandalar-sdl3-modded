"""Whole-Oracle upkeep rewards counted from real battlefield permanents."""
import copy
import json
from pathlib import Path
import re

from upkeep_value_trigger_recipes import _UpkeepWorld

FAMILY = 'upkeep_permanent_count'


def _profiles():
    return json.loads((Path(__file__).parent / 'data/upkeep_permanent_count_wave8_profiles.json').read_text(encoding='utf8'))


def classify(r):
    from recipes import cost_from_oracle
    if (not r or r.get('layout') != 'normal' or 'card_faces' in r or 'selected_face' in r
            or r.get('type_line', '').startswith('Token ') or not any(t in r.get('type_line', '') for t in ('Creature', 'Enchantment', 'Artifact'))):
        raise ValueError('whole normal standalone count-upkeep permanent required')
    text = r.get('oracle_text', '')
    m = re.fullmatch(r'At the beginning of (your upkeep|each player\'s upkeep), (.+)', text)
    if not m:
        raise ValueError('one complete direct permanent-count upkeep clause required')
    gate = 'own' if m[1] == 'your upkeep' else 'each'
    body = m[2]
    source = '(?:this enchantment|' + re.escape(r['name']) + ')'
    selector = effect = None
    multiplier = 1
    if body == 'you gain 2 life for each Shrine you control.':
        selector, effect, multiplier = 'shrine', 'gain', 2
    elif re.fullmatch(source + r' deals damage to any target equal to the number of Shrines you control\.', body):
        selector, effect = 'shrine', 'damage_target'
    elif body == 'create a 1/1 colorless Spirit creature token for each Shrine you control.':
        selector, effect = 'shrine', 'spirit'
    elif body == 'target opponent discards a card for each Shrine you control.':
        selector, effect = 'shrine', 'discard_opponent'
    elif body == 'draw a card for each Shrine you control.':
        selector, effect = 'shrine', 'draw'
    elif body == 'you draw X cards and lose X life, where X is the number of Zombies you control.':
        selector, effect = 'zombie', 'draw_loss'
    elif body == 'count the number of permanents you control. Your life total becomes that number.':
        selector, effect = 'permanent', 'set_life'
    else:
        hit = re.fullmatch(source + r' deals damage to that player equal to the number of (artifacts|Swamps|nonbasic lands) they control\.', body)
        if hit:
            selector, effect = {'artifacts': 'artifact', 'Swamps': 'swamp', 'nonbasic lands': 'nonbasic'}[hit[1]], 'damage_active'
    if selector is None or gate != ('each' if effect == 'damage_active' else 'own') or r.get('keywords'):
        raise ValueError('unsupported count, extra callback, passive, or upkeep scope')
    if 'Creature' in r['type_line']:
        int(r['power']); int(r['toughness'])
    cost_from_oracle(r)
    return dict(gate=gate, selector=selector, effect=effect, multiplier=multiplier, keywords=[])


def spec(d, r):
    from recipes import cost_from_oracle
    if (d.get('family') != FAMILY or type(d.get('version')) is not int or d['version'] != 2
            or not r or d.get('oracle_text') != r.get('oracle_text') or 'variants' not in d):
        raise ValueError('pinned typed version2 permanent-count declaration required')
    p, expected = d.get('params'), classify(r)
    if (type(p) is not dict or set(p) != set(expected) or p != expected
            or any(type(p[k]) is not str for k in ('gate', 'selector', 'effect'))
            or type(p['multiplier']) is not int or type(p['keywords']) is not list or p['keywords']):
        raise ValueError('exact JSON types and complete Oracle-derived count parameters required')
    pin = _profiles()['sources'].get(r['name'])
    if not pin or pin['oracle'] != r:
        raise ValueError('source not in independently pinned released cohort')
    ui = pin['ui']
    if (set(ui) != {'status', 'trigger_accept_players', 'target_choice_players'}
            or ui['status'] not in ('provisional_until_capture', 'calibrated')
            or ui['trigger_accept_players'] != []
            or ui['target_choice_players'] != ([0, 1] if p['effect'] == 'damage_target' else [])
            or any(type(x) is not int for x in ui['target_choice_players'])):
        raise ValueError('typed reviewed mandatory-upkeep presentation contract required')
    return dict(copy.deepcopy(p), family=FAMILY, entry_energy=0, mana=cost_from_oracle(r),
                legendary='Legendary' in r['type_line'], creature='Creature' in r['type_line'], ui=copy.deepcopy(ui))


def variants(p):
    values = ['cast', 'insufficient_mana', 'wrong_color', 'effect', 'opponent_upkeep', 'count_two', 'count_three',
              'opposing_count', 'nonmatching', 'count_removed', 'count_added_after', 'source_absent', 'source_in_hand',
              'source_enters_after', 'source_destroyed_after', 'repeat', 'armor', 'ready_snapshot', 'untap_snapshot',
              'resolved_snapshot', 'bare_zones']
    if p['selector'] in ('swamp', 'nonbasic'):
        # Lands are ordinary battlefield fixtures, not castable permanents.
        # The calibrated count/phase/damage model stays unchanged.
        values.remove('count_added_after')
        values += ['zero_count', 'basic_snow', 'dual_swamps', 'mixed_lands', 'snow_nonbasic', 'armor_three', 'controller_split']
    if p['effect'] == 'damage_target':
        values += ['target_creature', 'target_own_creature', 'target_own_player', 'target_planeswalker', 'mandatory_cancel']
    if p['effect'] == 'discard_opponent':
        values += ['empty_hand', 'short_hand', 'exact_hand']
    if p['effect'] == 'draw':
        values += ['exact_library']
    if p['effect'] == 'spirit':
        values += ['token_ready_attack', 'token_ground_block', 'token_defend_flying', 'token_bounce', 'token_exile', 'token_destroy']
    if p['effect'] == 'set_life':
        values += ['count_high', 'tokens_count']
    if p['creature']:
        values += ['unblocked', 'blocked']
    return [f'{v}_{owner}' for owner in (0, 1) for v in values] + ['cast_cancel_0']


def validate_witnesses(o, rows):
    data = _profiles()
    for group in ('witnesses', 'sources'):
        for nm, pin in data[group].items():
            if o.get(nm) != pin['oracle'] or nm not in rows or rows[nm]['handler'] != pin['handler'] or rows[nm].get('kind') != 'card':
                raise ValueError('complete source/witness Oracle or live binding changed: ' + nm)
            if rows[nm]['handler'] in ('NULL', 'card_dummy') and not pin.get('reviewed_native_witness'):
                raise ValueError('unreviewed native-only or unimplemented witness: ' + nm)
            if o[nm].get('layout') not in ('normal', 'token') or o[nm].get('card_faces') or o[nm].get('selected_face'):
                raise ValueError('whole standalone witness required: ' + nm)


class _CountWorld(_UpkeepWorld):
    def __init__(self, *args):
        super().__init__(*args)
        self.configs = {}
        self.target = None
        self.cancel_target = False

    def add(self, nm, ref, pl, zone='battlefield', source=False):
        super().add(nm, ref, pl, zone, source=source)
        if source:
            self.configs[ref] = classify(self.o[nm])

    def count(self, selector, player):
        names = self.zones['battlefield'][player]
        def matches(nm):
            r = self.o[nm]; types = r['type_line'].split(' — ')
            subtype = types[1].split() if len(types) == 2 else []
            if selector == 'permanent': return True
            if selector == 'shrine': return 'Enchantment' in types[0] and 'Shrine' in subtype
            if selector == 'artifact': return 'Artifact' in types[0]
            if selector == 'zombie': return 'Zombie' in subtype or 'Changeling' in r.get('keywords', []) and 'Creature' in types[0]
            if selector == 'swamp': return 'Land' in types[0] and 'Swamp' in subtype
            if selector == 'nonbasic': return 'Land' in types[0] and 'Basic' not in types[0].split()
            raise ValueError('unknown count selector')
        return sum(matches(nm) for nm in names)

    def cast_named(self, ref, denied=None):
        e = self.permanents[ref]
        if self.o[e['name']]['mana_cost'] != '{0}': return super().cast_named(ref, denied)
        if denied is not None: raise ValueError('zero-cost native witness has no invented affordability failure')
        # The immutable general helper deliberately supports positive costs
        # only. A printed {0} witness still uses the ordinary real cast path.
        self.pool(e['player'], [0]*6)
        self.emit('cast', ref=ref, value=1); self.emit('assert_stack', value=1)
        self.emit('resolve'); self.emit('assert_choices_consumed')
        self.zones['hand'][e['player']].remove(e['name']); self.zones['battlefield'][e['player']].append(e['name'])
        e['zone'] = 'battlefield'
        if ref in self.entities: self.entities[ref]['zone'] = 'battlefield'; self.entities[ref]['sick'] = True
        self.receipts()

    def receipts(self):
        for ref in self.tokens:
            e = self.entities[ref]
            if ref not in self.bound and e['zone'] == 'battlefield':
                live = [r for r in self.tokens if self.entities[r]['zone'] == 'battlefield' and self.entities[r]['name'] == e['name'] and self.entities[r]['player'] == e['player']]
                self.emit('bind_permanent_copy', player=e['player'], value=self.rows[e['name']]['csvid'], index=live.index(ref), ref=ref)
                self.bound.add(ref)
        self.emit('assert_stack', value=0); self.emit('assert_choices_consumed')
        for ref, e in self.entities.items():
            if e['zone'] != 'battlefield': continue
            for prop in ('in_play', 'power', 'toughness', 'sick', 'tapped', 'attacking', 'damage', 'plus1_counters'):
                self.emit('assert_card', ref=ref, property=prop, value=1 if prop == 'in_play' else 0 if prop == 'plus1_counters' else int(e[prop]))
        for ref, e in self.permanents.items():
            if ref not in self.entities and e['zone'] == 'battlefield':
                self.emit('assert_card', ref=ref, property='in_play', value=1)
                self.emit('assert_card', ref=ref, property='tapped', value=0)
        for ref, e in self.walkers.items():
            if e['zone'] == 'battlefield': self.emit('assert_counter', ref=ref, counter_kind='loyalty', value=e['loyalty'])
        for pl in (0, 1):
            for nm, subtype, artifact in (('Spirit', 'Spirit', 0), ('Servo', 'Servo', 1)):
                if any(self.entities[t]['zone'] == 'battlefield' and self.entities[t]['player'] == pl and self.entities[t]['name'] == nm for t in self.tokens):
                    for prop, value in (('token', 1), ('artifact', artifact), ('color', 0), ('subtype_' + subtype, 1)):
                        self.emit('assert_matching_permanents', player=pl, index=self.rows[nm]['csvid'], property=prop, value=value)
        universe = {c['name'] for c in self.setup} | {self.entities[t]['name'] for t in self.tokens}
        for pl in (0, 1):
            self.emit('assert_life', player=pl, value=self.life[pl]); self.emit('assert_energy', player=pl, value=0)
            for color, n in enumerate(self.mana[pl]): self.emit('assert_mana', player=pl, color=color, value=n)
            for zone, players in self.zones.items():
                self.emit('assert_zone', player=pl, zone=zone, value=len(players[pl]))
                if zone == 'library':
                    for i, nm in enumerate(players[pl]): self.emit('assert_zone_card', player=pl, zone=zone, index=i, value=self.rows[nm]['csvid'])
                else:
                    for nm in sorted(universe): self.emit('assert_zone_multiplicity', player=pl, zone=zone, index=self.rows[nm]['csvid'], value=players[pl].count(nm))

    def create_tokens(self, nm, player, amount):
        for _ in range(amount):
            ref = f'token_{len(self.tokens)}'; self.tokens.append(ref)
            self.entity(nm, ref, player, 'battlefield', token=True)
            # The source's complete Oracle defines this token. The bulk Spirit
            # name pins a different blue/flying variant; it is identity only.
            self.entities[ref]['keys'] = set()
            self.zones['battlefield'][player].append(nm)

    def draw(self, player, n):
        if len(self.zones['library'][player]) < n: raise ValueError('terminal decking is outside this nonterminal model')
        self.zones['hand'][player].extend(self.zones['library'][player][:n]); del self.zones['library'][player][:n]

    def visit(self):
        if any(any(v) for v in self.mana): raise ValueError('real upkeep requires spent pools')
        self.emit('advance_turn'); self.expire(); self.current = 1 - self.current
        self.emit('untap_step', player=self.current)
        for e in self.entities.values():
            if e['zone'] == 'battlefield' and e['player'] == self.current: e['sick'] = False; e['tapped'] = False
        self.receipts(); cut = len(self.steps)
        live = [ref for ref in self.sources if self.permanents[ref]['zone'] == 'battlefield'
                and (self.configs[ref]['gate'] == 'each' or self.permanents[ref]['player'] == self.current)]
        for ref in live:
            if self.configs[ref]['effect'] == 'damage_target':
                player = self.permanents[ref]['player']
                target = self.target if ref == 'source' and self.target is not None else ('player', 1 - player)
                if self.cancel_target and ref == 'source': self.emit('choose_optional_target', player=player, ref=target[1])
                elif target[0] == 'player': self.emit('choose_player', player=player, value=target[1])
                else: self.emit('choose_target', player=player, ref=target[1])
        self.emit('upkeep_step', player=self.current)
        if self.cancel_target and 'source' in live: self.emit('assert_forced_target_choices', value=1)
        for ref in live:
            p = self.configs[ref]; player = self.permanents[ref]['player']
            recipient = self.current if p['gate'] == 'each' else player
            n = self.count(p['selector'], recipient) * p['multiplier']; kind = p['effect']
            if kind == 'gain': self.life[player] += n
            elif kind == 'set_life': self.life[player] = n
            elif kind in ('draw', 'draw_loss'):
                self.draw(player, n)
                if kind == 'draw_loss': self.life[player] -= n
            elif kind == 'discard_opponent':
                hand = self.zones['hand'][1-player]
                for _ in range(min(n, len(hand))):
                    # Ordinary discard controls use identical Forests, so both
                    # human selection and legal AI policies have exact zones.
                    if len(set(hand)) != 1: raise ValueError('mixed discard hand requires separately calibrated selection receipts')
                    self.zones['graveyard'][1-player].append(hand.pop(0))
            elif kind == 'spirit': self.create_tokens('Spirit', player, n)
            elif kind == 'damage_active': self.life[recipient] -= max(0, n-int(recipient in self.armored))
            else:
                target = self.target if ref == 'source' and self.target is not None else ('player', 1-player)
                if target[0] == 'player': self.life[target[1]] -= max(0, n-int(target[1] in self.armored))
                elif target[1] in self.walkers: self.hit_walker(target[1], n)
                else:
                    e = self.entities[target[1]]; e['damage'] += n
                    if e['damage'] >= e['toughness']: self.leave(target[1], 'graveyard')
        self.receipts(); self.emit('turn', player=self.current, value=0); self.receipts()
        return cut


def expand(name, csvid, d, p, selected, rows, o):
    from recipes import step
    if not selected or len(set(selected)) != len(selected) or any(v not in variants(p) for v in selected):
        raise ValueError('explicit unique released permanent-count variants required')
    validate_witnesses(o, rows); out = []
    if p['selector'] in ('swamp', 'nonbasic'):
        return _expand_land_counts(name, csvid, d, p, selected, rows, o)
    for variant in selected:
        mode, owner = variant.rsplit('_', 1); owner = int(owner); enemy = 1-owner
        w = _CountWorld(name, p, owner, rows, o); w.add(name, 'source', owner, 'hand', source=True)
        supports = []
        count_mode = mode in ('count_two', 'count_three', 'count_removed', 'count_added_after', 'short_hand', 'exact_hand', 'empty_hand')
        quantity = 2 if mode in ('count_three', 'short_hand', 'exact_hand', 'empty_hand') else 1
        if p['selector'] == 'artifact' and mode in ('count_two', 'count_three'): quantity += 1
        if p['selector'] == 'shrine' and count_mode:
            choices = [x for x in ('Honden of Cleansing Fire', 'Honden of Seeing Winds', 'Honden of Infinite Rage') if x != name]
            for i, nm in enumerate(choices[:quantity]):
                ref = f'count_{i}'; w.add(nm, ref, owner, 'hand', source=True); supports.append(ref)
        elif count_mode:
            for i in range(quantity):
                nm = 'Woodland Changeling' if p['selector'] == 'zombie' else 'Memnite'
                ref = f'count_{i}'; w.add(nm, ref, owner, 'hand'); supports.append(ref)
        if mode == 'count_high':
            for i in range(21): w.add('Forest', f'high_{i}', owner)
        if mode == 'opposing_count':
            nm = 'Honden of Cleansing Fire' if p['selector'] == 'shrine' else 'Woodland Changeling' if p['selector'] == 'zombie' else 'Memnite'
            w.add(nm, 'opposing', enemy, source=p['selector'] == 'shrine')
        if mode == 'nonmatching':
            w.add('Woodland Changeling' if p['selector'] == 'shrine' else 'Grizzly Bears', 'nonmatching', owner)
            w.add('Forest', 'nonmatching_land', owner)
        if mode in ('armor',): w.add("Urza's Armor", 'armor', owner, 'hand')
        if mode == 'armor' and p['effect'] == 'damage_target': w.target = ('player', owner)
        if mode in ('target_creature', 'mandatory_cancel'): w.add('Craw Wurm', 'recipient', enemy); w.target = ('card', 'recipient')
        if mode == 'target_own_creature': w.add('Craw Wurm', 'recipient', owner); w.target = ('card', 'recipient')
        if mode == 'target_own_player': w.target = ('player', owner)
        if mode == 'mandatory_cancel': w.cancel_target = True
        if mode == 'target_planeswalker':
            w.add('Tibalt, the Fiend-Blooded', 'walker', enemy, 'hand')
            w.walkers['walker'] = dict(name='Tibalt, the Fiend-Blooded', player=enemy, zone='hand', loyalty=0)
            w.target = ('card', 'walker')
        if mode in ('token_ground_block', 'blocked'): w.add('Grizzly Bears', 'blocker', enemy)
        if mode == 'token_defend_flying': w.add('Air Elemental', 'flying', enemy)
        if mode in ('token_bounce', 'token_exile'): w.add('Unsummon' if mode == 'token_bounce' else 'Swords to Plowshares', 'token_spell', owner, 'hand')
        if mode == 'tokens_count': w.add('Servo Exhibition', 'servo_spell', owner, 'hand')
        if mode == 'cast_cancel':
            for color, count in enumerate(p['mana']):
                for i in range(count): w.add(('Forest', 'Swamp', 'Island', 'Forest', 'Mountain', 'Plains')[color], f'payment_land_{color}_{i}', owner)
        for pl in (0, 1):
            hand_count = 0 if mode == 'empty_hand' and pl == enemy else 1 if mode == 'short_hand' and pl == enemy else 3 if mode == 'exact_hand' and pl == enemy else 4
            for i in range(hand_count): w.add('Forest', f'hand_{pl}_{i}', pl, 'hand')
            if mode != 'bare_zones':
                w.add('Lightning Bolt', f'grave_{pl}', pl, 'graveyard'); w.add('Island', f'exile_{pl}', pl, 'exile')
            library = ['Forest', 'Island', 'Plains', 'Swamp', 'Mountain', 'Grizzly Bears'] * 4
            if mode == 'exact_library' and pl == owner: library = library[:1]
            for i, nm in enumerate(library): w.add(nm, f'library_{pl}_{i}', pl, 'library')
        start = enemy if mode == 'target_planeswalker' else owner
        w.current = start; w.emit('turn', player=start, value=0); w.emit('discard_policy', value=1)
        w.pool(0, [0]*6); w.pool(1, [0]*6); w.emit('refresh'); w.receipts()
        if mode == 'target_planeswalker': w.cast_walker('walker'); w.visit()
        if mode == 'armor': w.cast_named('armor'); w.armored.add(owner)
        if mode == 'cast_cancel':
            for c in w.setup:
                if c['ref'].startswith('payment_land_'): w.emit('require_can_activate', ref=c['ref'], value=1)
            w.emit('choose_cancel_mana', player=0); w.emit('cast', ref='source', value=0); w.emit('assert_unused_choices', value=0); w.receipts()
        elif mode not in ('source_enters_after', 'source_in_hand'):
            w.cast_named('source', mode if mode in ('insufficient_mana', 'wrong_color') else None)
        if mode not in ('cast', 'insufficient_mana', 'wrong_color', 'cast_cancel'):
            if mode != 'count_added_after':
                for ref in supports: w.cast_named(ref)
            if mode == 'tokens_count':
                w.cast_card('Servo Exhibition', 'servo_spell', owner); w.create_tokens('Servo', owner, 2); w.receipts()
            if mode in ('source_absent',): w.destroy('source')
            if mode == 'count_removed': w.destroy(supports[0])
            ready_cut = len(w.steps); untap_cut = w.visit()
            if mode != 'opponent_upkeep': untap_cut = w.visit()
            if mode == 'source_enters_after': w.cast_named('source'); w.visit(); w.visit()
            if mode == 'source_destroyed_after': w.destroy('source'); w.visit(); w.visit()
            if mode == 'count_added_after':
                for ref in supports: w.cast_named(ref)
                w.visit(); w.visit()
            if mode == 'repeat': w.visit(); w.visit()
            if mode in ('unblocked', 'blocked'):
                w.combat(w.declare(['source']), [('blocker', 'source')] if mode == 'blocked' else [])
            if mode in ('token_ready_attack', 'token_ground_block'):
                w.visit(); w.visit()
                w.combat(w.declare(['token_0']), [('blocker', 'token_0')] if mode == 'token_ground_block' else [])
            if mode == 'token_defend_flying':
                w.visit(); w.combat(w.declare(['flying']), [('token_0', 'flying')])
            if mode in ('token_bounce', 'token_exile'):
                w.spell('Unsummon' if mode == 'token_bounce' else 'Swords to Plowshares', 'token_spell', owner, 'token_0')
                w.leave('token_0', 'hand' if mode == 'token_bounce' else 'exile')
                if mode == 'token_exile': w.life[owner] += 1
                w.receipts()
            if mode == 'token_destroy': w.emit('destroy', ref='token_0'); w.leave('token_0', 'graveyard'); w.receipts()
        w.receipts(); seq = w.steps
        if mode in ('ready_snapshot', 'untap_snapshot'):
            cut = ready_cut if mode == 'ready_snapshot' else untap_cut; tail = seq[cut:]
            seq = seq[:cut] + [step('capture')] + copy.deepcopy(tail) + [step('record_state'), step('ai_roundtrip'), step('restore')] + copy.deepcopy(tail) + [step('assert_recorded_state')]
        if mode == 'resolved_snapshot':
            cut = len(w.steps); w.receipts(); checks = w.steps[cut:]
            seq = w.steps[:cut] + [step('capture'), step('record_state'), step('ai_roundtrip'), step('restore')] + checks + [step('assert_recorded_state')]
        out.append(dict(name=f'UpkeepPermanentCountRecipe_{csvid}_{variant}', card=name, fixture='empty_main', seed=123,
                        setup=w.setup, steps=seq, _recipe=dict(family=FAMILY, version=2, variant=variant, params=d['params'], oracle_text=d['oracle_text'],
                        ui_annotations='provisional_until_capture', untested_components=[
                            'target announcement or source removal between pending trigger creation and inline callback: only actual production windows exist',
                            'arbitrary target-filter introspection, multiplayer, terminal decking/life-zero loss, extra upkeep sources',
                            'AI discretionary targeting/discard preference; actual declared selections only',
                            'Shrine is enchantment subtype; no creature-Changeling proxy or unsupported type-changing composition',
                            'noncommuting mixed source portfolios and legendary duplicate selection require separate controls',
                            'held follow-on Karma134 and PrimalOrder1645 not enrolled until pilot calibration'])))
    return out


def _expand_land_counts(name, csvid, d, p, selected, rows, o):
    """Same resolution-time count grammar with independently pinned land sets."""
    from recipes import step
    out = []
    for variant in selected:
        mode, owner = variant.rsplit('_', 1); owner = int(owner); enemy = 1-owner
        w = _CountWorld(name, p, owner, rows, o); w.add(name, 'source', owner, 'hand', source=True)
        matching = 'Swamp' if p['selector'] == 'swamp' else 'Badlands'
        own = [matching] * (3 if mode in ('count_three', 'armor_three') else 2 if mode in ('count_two', 'controller_split') else 1)
        opposing = [matching] * (3 if mode == 'controller_split' else 2 if mode == 'opposing_count' else 0)
        if mode in ('zero_count', 'nonmatching', 'opposing_count'): own = []
        if mode == 'basic_snow': own = ['Swamp', 'Snow-Covered Swamp', 'Forest', 'Snow-Covered Forest', 'Wastes']
        if mode == 'dual_swamps': own = ['Badlands', 'Bayou']
        if mode == 'mixed_lands': own = ['Swamp', 'Snow-Covered Swamp', 'Badlands', 'Bayou', 'Wastes', 'Snow-Covered Forest', 'Mouth of Ronom']
        if mode == 'snow_nonbasic': own = ['Snow-Covered Swamp', 'Mouth of Ronom']
        for player, lands in ((owner, own), (enemy, opposing)):
            for i, nm in enumerate(lands): w.add(nm, f'land_{player}_{i}', player)
        if mode == 'nonmatching': w.add('Memnite', 'nonland', owner); w.add('Forest', 'basic_control', owner)
        if mode in ('armor', 'armor_three'): w.add("Urza's Armor", 'armor', owner, 'hand')
        if mode == 'cast_cancel':
            for color, count in enumerate(p['mana']):
                for i in range(count): w.add(('Forest', 'Swamp', 'Island', 'Forest', 'Mountain', 'Plains')[color], f'payment_land_{color}_{i}', owner)
        for player in (0, 1):
            # Positive land identities in every other zone are real exclusions.
            if mode != 'bare_zones':
                for i, nm in enumerate(('Swamp', 'Badlands', 'Snow-Covered Swamp')): w.add(nm, f'hand_{player}_{i}', player, 'hand')
                for zone in ('graveyard', 'exile'):
                    for i, nm in enumerate(('Swamp', 'Badlands')): w.add(nm, f'{zone}_{player}_{i}', player, zone)
            for i, nm in enumerate(('Swamp', 'Badlands', 'Forest', 'Snow-Covered Swamp', 'Wastes', 'Bayou')): w.add(nm, f'library_{player}_{i}', player, 'library')
        w.emit('turn', player=owner, value=0); w.pool(0, [0]*6); w.pool(1, [0]*6); w.emit('refresh'); w.receipts()
        if mode in ('armor', 'armor_three'): w.cast_named('armor'); w.armored.add(owner)
        if mode == 'cast_cancel':
            for c in w.setup:
                if c['ref'].startswith('payment_land_'): w.emit('require_can_activate', ref=c['ref'], value=1)
            w.emit('choose_cancel_mana', player=0); w.emit('cast', ref='source', value=0); w.emit('assert_unused_choices', value=0); w.receipts()
        elif mode not in ('source_enters_after', 'source_in_hand'):
            w.cast_named('source', mode if mode in ('insufficient_mana', 'wrong_color') else None)
        if mode not in ('cast', 'insufficient_mana', 'wrong_color', 'cast_cancel'):
            if mode == 'source_absent': w.destroy('source')
            if mode == 'count_removed': w.destroy(f'land_{owner}_0')
            ready_cut = len(w.steps); untap_cut = w.visit()
            if mode != 'opponent_upkeep': untap_cut = w.visit()
            if mode == 'source_enters_after': w.cast_named('source'); w.visit(); w.visit()
            if mode == 'source_destroyed_after': w.destroy('source'); w.visit(); w.visit()
            if mode == 'repeat': w.visit(); w.visit()
        w.receipts(); seq = w.steps
        if mode in ('ready_snapshot', 'untap_snapshot'):
            cut = ready_cut if mode == 'ready_snapshot' else untap_cut; tail = seq[cut:]
            seq = seq[:cut] + [step('capture')] + copy.deepcopy(tail) + [step('record_state'), step('ai_roundtrip'), step('restore')] + copy.deepcopy(tail) + [step('assert_recorded_state')]
        if mode == 'resolved_snapshot':
            cut = len(w.steps); w.receipts(); checks = w.steps[cut:]
            seq = w.steps[:cut] + [step('capture'), step('record_state'), step('ai_roundtrip'), step('restore')] + checks + [step('assert_recorded_state')]
        out.append(dict(name=f'UpkeepPermanentCountRecipe_{csvid}_{variant}', card=name, fixture='empty_main', seed=123, setup=w.setup, steps=seq,
                        _recipe=dict(family=FAMILY, version=2, variant=variant, params=d['params'], oracle_text=d['oracle_text'],
                                     ui_annotations='calibrated_mandatory_each_upkeep_no_target_menus', untested_components=[
                                         'real land placement before fixture start; no fabricated land casting or unsupported between-trigger type changes',
                                         'snow-source activation/payment is outside this land-count witness use; no witness ability is activated',
                                         'life-zero loss, multiplayer, additional upkeep sources and arbitrary phase reentry'])))
    return out
