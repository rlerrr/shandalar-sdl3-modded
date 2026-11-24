"""Whole Oracle self changes from real land entry and normal combat."""
from collections import Counter
import copy
import json
from pathlib import Path
import re

from counter_condition_recurrence_recipes import exact


FAMILY = 'landfall_self_change'
TRIGGER = 'Landfall — Whenever a land you control enters, '
TRAMPLE_REMINDER = " (It can deal excess combat damage to the player or planeswalker it's attacking.)"
COMMON = ('cast', 'unfunded', 'own_land', 'opponent_land', 'consecutive_lands', 'cleanup',
          'empty_search', 'nonland_spell', 'late_source', 'removed_source', 'countered_search',
          'pending_replay', 'fresh_source', 'other_source')


def pins():
    path = Path(__file__).parent/'data/landfall_self_change_wave8_profiles.json'
    return json.loads(path.read_text(encoding='utf8'))


def oracle_model(record):
    if (record.get('layout') != 'normal' or record.get('card_faces') or record.get('selected_face')
            or not record.get('type_line', '').startswith('Creature — ')
            or any(not re.fullmatch(r'[1-9][0-9]*', record.get(k, '')) for k in ('power', 'toughness'))):
        raise ValueError('normal creature with numeric power and toughness required')
    lines = record['oracle_text'].splitlines()
    triggers = [line for line in lines if line.startswith(TRIGGER)]
    if len(triggers) != 1:
        raise ValueError('one complete own-land-entry clause required')
    passive = [line for line in lines if line not in triggers]
    if passive not in ([], ['Trample'], ['Defender'], ["This creature can't block."]):
        raise ValueError('unreviewed passive landfall clause')
    expected_keywords = {'Landfall'} | (set(passive) & {'Trample', 'Defender'})
    keywords = record.get('keywords', [])
    if len(keywords) != len(expected_keywords) or set(keywords) != expected_keywords:
        raise ValueError('landfall keyword metadata and clauses disagree')
    clause = triggers[0].removeprefix(TRIGGER)
    counter_clause = 'put a +1/+1 counter on this creature.'
    forest_clause = ' If that land is a Forest, put two +1/+1 counters on this creature instead.'
    pump = re.fullmatch(
        r'this creature gets \+([1-9])/\+([1-9])( and gains trample)? until end of turn\.(.*)', clause)
    if clause in (counter_clause, counter_clause+forest_clause):
        change = dict(kind='counter', forest_bonus=int(clause.endswith(forest_clause)))
    elif pump and pump[4] == (TRAMPLE_REMINDER if pump[3] else ''):
        change = dict(kind='pump', power=int(pump[1]), toughness=int(pump[2]), trample=bool(pump[3]))
    elif clause in ('this creature gains flying until end of turn.',
                    'this creature gains first strike until end of turn.'):
        change = dict(kind='gain_keyword', keyword=clause.removeprefix('this creature gains ').removesuffix(' until end of turn.'))
    elif clause == 'this creature loses defender until end of turn.' and passive == ['Defender']:
        change = dict(kind='remove_defender')
    else:
        raise ValueError('unreviewed landfall self change')
    if (passive == ['Trample'] and change['kind'] != 'counter'
            or passive == ['Defender'] and change['kind'] != 'remove_defender'
            or passive == ["This creature can't block."] and change['kind'] != 'pump'):
        raise ValueError('unreviewed passive/landfall composition')
    return dict(change=change, passive=passive[0] if passive else '')


def variants(model):
    extra = []
    change = model['change']
    if change.get('forest_bonus'):
        extra += ['nonforest', 'nonbasic_forest']
    if change['kind'] in ('gain_keyword', 'remove_defender') or change.get('trample') or model['passive']=='Trample':
        extra += ['combat_before', 'combat_after', 'combat_expired']
    if change.get('keyword') == 'flying':
        extra += ['flying_blocker']
    if model['passive'] == "This creature can't block.":
        extra += ['cannot_block']
    return [f'{mode}_{actor}' for actor in (0, 1) for mode in (*COMMON, *extra)]


def spec(declaration, record):
    from recipes import cost_from_oracle
    if (declaration.get('family') != FAMILY or type(declaration.get('version')) is not int
            or declaration['version'] != 2 or declaration.get('oracle_text') != record.get('oracle_text')):
        raise ValueError('whole Oracle version-two landfall declaration required')
    model = oracle_model(record)
    if not exact(declaration.get('params'), dict(schema=8, **model)):
        raise ValueError('typed landfall parameters must match printed clauses')
    selected = declaration.get('variants')
    if (not isinstance(selected, list) or not selected or any(not isinstance(v, str) for v in selected)
            or len(set(selected)) != len(selected) or set(selected)-set(variants(model))):
        raise ValueError('distinct supported landfall variants required')
    return dict(model, family=FAMILY, cost=cost_from_oracle(record),
                power=int(record['power']), toughness=int(record['toughness']))


def validate_witnesses(oracle, registry):
    data = pins()
    for name, pin in (data['sources'] | data['witnesses']).items():
        row = registry.get(name, {})
        if (not exact(oracle.get(name), pin['oracle']) or not exact(row, pin['registry'])
                or row.get('kind') != 'card' or row.get('handler') in (None, 'card_dummy', '0', 'NULL')):
            raise ValueError('complete implemented card pin changed: '+name)
    return data


class LandfallCase:
    def __init__(self, name, csvid, declaration, model, variant, rows, oracle):
        from recipes import card, step, cost_from_oracle
        self.card, self.step, self.cost = card, step, cost_from_oracle
        self.name, self.csvid, self.declaration, self.model = name, csvid, declaration, model
        self.rows, self.oracle, self.variant = rows, oracle, variant
        self.mode, player = variant.rsplit('_', 1)
        self.owner = int(player)
        self.enemy = 1-self.owner
        self.actor = self.enemy if self.mode in ('opponent_land', 'cannot_block') else self.owner
        self.initially_cast = self.mode in ('cast', 'unfunded', 'late_source', 'fresh_source')
        self.entities, self.setup, self.steps = {}, [], []
        self.zones = [{z: [] for z in ('hand', 'battlefield', 'graveyard', 'exile', 'library')} for _ in (0, 1)]
        self.pools, self.life = [[0]*6, [0]*6], [20, 20]
        self.counters = self.power_bonus = self.toughness_bonus = 0
        self.landfall_active = False
        self.replay_cut = None
        self.land_plan = self.plan_land_entries()
        self.prepare_setup()

    def emit(self, op, **data):
        self.steps.append(self.step(op, **data))

    def plan_land_entries(self):
        if self.mode in ('cast', 'unfunded', 'nonland_spell', 'combat_before', 'cannot_block'):
            return []
        lands = {'nonforest': ['Plains'], 'consecutive_lands': ['Forest', 'Plains', 'Island'],
                 'nonbasic_forest': ['Dryad Arbor'], 'empty_search': [],
                 'countered_search': ['Forest']}.get(self.mode, ['Forest'])
        spell = "Nature's Lore" if self.mode == 'nonbasic_forest' else 'Rampant Growth'
        return [(spell, land) for land in (lands or [None])]

    def add(self, name, ref, player, zone='hand'):
        setup = self.card(name, ref, player, zone)
        if zone == 'battlefield':
            setup['state'] = ['untapped']
        self.setup.append(setup)
        self.entities[ref] = dict(name=name, player=player, zone=zone, tapped=0, sick=0, damage=0)
        self.zones[player][zone].append(name)

    def prepare_setup(self):
        self.add(self.name, 'source', self.owner, 'hand' if self.initially_cast else 'battlefield')
        if self.mode == 'other_source':
            self.add(self.name, 'opposing_copy', self.enemy, 'battlefield')
        for player in (0, 1):
            self.add('Plains', f'hand_{player}', player)
            self.add('Swamp', f'grave_{player}', player, 'graveyard')
            self.add('Island', f'exile_{player}', player, 'exile')
            self.add('Darksteel Relic', f'library_filler_{player}', player, 'library')
        for i, (spell, land) in enumerate(self.land_plan):
            self.add(spell, f'search_{i}', self.actor)
            if land is not None:
                self.add(land, f'in_library_{i}', self.actor, 'library')
        if self.mode == 'countered_search':
            self.add('Counterspell', 'counter', self.enemy)
        if self.mode == 'removed_source':
            self.add('Murder', 'murder', self.enemy)
        if self.mode == 'nonland_spell':
            self.add('Darksteel Relic', 'nonland', self.owner)
        if self.mode == 'cannot_block':
            self.add('Elvish Mystic', 'opposing_attacker', self.enemy, 'battlefield')
        if self.mode.startswith('combat_') or self.mode == 'flying_blocker':
            change = self.model['change']
            if self.mode=='flying_blocker' or self.model['passive']=='Trample' or change.get('trample'):
                blocker = 'Drift of Phantasms'
            elif change.get('keyword')=='first strike':
                blocker = 'Knight of Glory'
            else:
                blocker = 'Darksteel Myr'
            self.add(blocker, 'blocker', self.enemy, 'battlefield')

    def move(self, ref, zone):
        entity = self.entities[ref]
        zones = self.zones[entity['player']]
        if entity['zone'] != 'stack':
            zones[entity['zone']].remove(entity['name'])
        if zone != 'stack':
            zones[zone].append(entity['name'])
        entity['zone'] = zone

    def fund(self, player, pool):
        self.pools[player] = list(pool)
        for color, amount in enumerate(pool):
            self.emit('set_mana', player=player, color=color, value=amount)
        self.emit('refresh')

    def assert_mana(self):
        for player, pool in enumerate(self.pools):
            for color, amount in enumerate(pool):
                self.emit('assert_mana', player=player, color=color, value=amount)

    def cast(self, ref, *, allowed=True, target=None, response=False):
        entity = self.entities[ref]
        player = entity['player']
        record = self.oracle[entity['name']]
        if ref == 'source':
            fee = self.model['cost']
        elif record['mana_cost'] == '{0}':
            fee = [0]*6
        else:
            fee = self.cost(record)
        self.fund(player, fee if allowed else [0]*6)
        if target is not None:
            self.emit('choose_target', player=player, ref=target)
        self.emit('cast_response' if response else 'cast', ref=ref, value=int(allowed))
        if allowed:
            self.move(ref, 'stack')
            self.pools[player] = [0]*6
            self.emit('assert_card', ref=ref, property='in_hand', value=0)
        self.emit('assert_stack', value=2 if response else int(allowed))
        self.assert_mana()

    def resolve(self, ref, destination):
        self.emit('resolve')
        self.emit('refresh')
        self.emit('assert_choices_consumed')
        self.move(ref, destination)

    def source_stats(self):
        for ref in ('source', 'opposing_copy'):
            ent = self.entities.get(ref)
            if ent is None or ent['zone'] != 'battlefield':
                continue
            counter = self.counters if ref=='source' else 0
            bonus = (self.power_bonus, self.toughness_bonus) if ref=='source' else (0, 0)
            self.emit('assert_card', ref=ref, property='in_play', value=1)
            self.emit('assert_counter', ref=ref, counter_kind='plus1', value=counter)
            for prop, extra in zip(('power','toughness'), bonus):
                self.emit('assert_card', ref=ref, property=prop, value=self.model[prop]+counter+extra)
            for prop in ('tapped','sick','damage'):
                self.emit('assert_card', ref=ref, property=prop, value=ent[prop])

    def receipts(self):
        self.emit('assert_stack', value=0)
        self.emit('assert_choices_consumed')
        self.assert_mana()
        universe = sorted({item['name'] for item in self.setup})
        for player, zones in enumerate(self.zones):
            self.emit('assert_life', player=player, value=self.life[player])
            for zone, names in zones.items():
                self.emit('assert_zone', player=player, zone=zone, value=len(names))
                counts = Counter(names)
                for name in universe:
                    self.emit('assert_zone_multiplicity', player=player, zone=zone,
                              index=self.rows[name]['csvid'], value=counts[name])
        self.source_stats()

    def landfall(self, land):
        if self.actor != self.owner or self.entities['source']['zone'] != 'battlefield':
            return
        self.landfall_active = True
        change = self.model['change']
        if change['kind']=='counter':
            subtypes = self.oracle[land]['type_line'].partition(' — ')[2].split()
            self.counters += 1+change['forest_bonus']*int('Forest' in subtypes)
        elif change['kind']=='pump':
            self.power_bonus += change['power']
            self.toughness_bonus += change['toughness']

    def search(self, i, spell, land):
        ref = f'search_{i}'
        self.cast(ref)
        self.source_stats()
        self.emit('assert_zone', player=self.actor, zone='library', value=len(self.zones[self.actor]['library']))
        if self.mode=='pending_replay':
            self.replay_cut = len(self.steps)
        if self.mode=='countered_search':
            self.emit('announce_spell', ref=ref)
            self.cast('counter', response=True)
            self.emit('resolve_response')
            self.move('counter', 'graveyard')
            self.move(ref, 'graveyard')
            self.receipts()
            return
        if land is None:
            self.emit('choose_zone', player=-1, value=-1)
        else:
            self.emit('choose_zone_card', player=-1, value=self.rows[land]['csvid'])
        self.resolve(ref, 'graveyard')
        if land is not None:
            original_ref, new_ref = f'in_library_{i}', f'land_{i}'
            self.entities[new_ref] = self.entities.pop(original_ref)
            self.move(new_ref, 'battlefield')
            self.emit('bind_permanent', ref=new_ref, player=self.actor, value=self.rows[land]['csvid'])
            ent = self.entities[new_ref]
            ent['tapped'] = int(spell=='Rampant Growth')
            self.emit('assert_card', ref=new_ref, property='tapped', value=ent['tapped'])
            if 'Land Creature' in self.oracle[land]['type_line']:
                ent['sick'] = 1
                self.emit('assert_card', ref=new_ref, property='sick', value=1)
            self.landfall(land)
        self.receipts()

    def source_cast(self):
        self.cast('source', allowed=self.mode!='unfunded')
        if self.mode!='unfunded':
            self.resolve('source', 'battlefield')
            self.entities['source']['sick'] = 1
        self.receipts()

    def reset_temporary(self):
        self.power_bonus = self.toughness_bonus = 0
        self.landfall_active = False

    def expire_for_combat(self):
        self.emit('advance_turn')
        self.emit('advance_turn')
        self.emit('untap_step', player=self.owner)
        self.emit('turn', player=self.owner, value=0)
        for entity in self.entities.values():
            if entity['zone']=='battlefield':
                entity['damage'] = entity['sick'] = 0
                if entity['player']==self.owner:
                    entity['tapped'] = 0
        self.reset_temporary()

    def fight(self):
        if self.mode=='combat_expired':
            self.expire_for_combat()
        if self.mode=='cannot_block':
            self.emit('begin_attackers')
            self.emit('attack', ref='opposing_attacker', value=1)
            self.emit('finish_attackers')
            self.emit('begin_blockers')
            self.emit('block', ref='source', other='opposing_attacker', value=0)
            self.emit('advance_combat')
            self.emit('combat_damage')
            self.emit('end_combat')
            self.life[self.owner] -= int(self.oracle['Elvish Mystic']['power'])
            self.receipts()
            return
        source = self.entities['source']
        change = self.model['change']
        flying = self.landfall_active and change.get('keyword')=='flying'
        first_strike = self.landfall_active and change.get('keyword')=='first strike'
        trample = self.model['passive']=='Trample' or (self.landfall_active and change.get('trample'))
        defender = self.model['passive']=='Defender' and not self.landfall_active
        attacking = not defender and not source['sick']
        self.emit('begin_attackers')
        self.emit('attack', ref='source', value=int(attacking))
        source['tapped'] = int(attacking)
        self.emit('assert_card', ref='source', property='attacking', value=int(attacking))
        self.emit('finish_attackers')
        self.emit('begin_blockers')
        blocker = self.entities.get('blocker')
        blocked = attacking and blocker is not None and (not flying or 'Flying' in self.oracle[blocker['name']]['keywords'])
        if attacking and blocker is not None:
            self.emit('block', ref='blocker', other='source', value=int(blocked))
        self.emit('advance_combat')
        self.emit('combat_damage')
        if attacking:
            power = self.model['power']+self.counters+self.power_bonus
            toughness = self.model['toughness']+self.counters+self.toughness_bonus
            if not blocked:
                self.life[self.enemy] -= power
            else:
                record = self.oracle[blocker['name']]
                blocker_power, blocker_toughness = int(record['power']), int(record['toughness'])
                blocker_dies = power>=blocker_toughness and 'Indestructible' not in record['keywords']
                if blocker_dies:
                    self.move('blocker', 'graveyard')
                if not (first_strike and blocker_dies):
                    source['damage'] += blocker_power
                if source['damage']>=toughness:
                    self.move('source', 'graveyard')
                if trample:
                    self.life[self.enemy] -= max(0, power-blocker_toughness)
        self.emit('end_combat')
        self.receipts()

    def build(self):
        self.emit('reset_death_history')
        self.emit('turn', player=self.actor, value=0)
        for player in (0, 1):
            self.fund(player, [0]*6)
        if self.initially_cast and self.mode!='late_source':
            self.source_cast()
        else:
            self.receipts()
        if self.mode=='removed_source':
            self.cast('murder', target='source')
            self.resolve('murder', 'graveyard')
            self.move('source', 'graveyard')
            self.receipts()
        if self.mode=='nonland_spell':
            self.cast('nonland')
            self.resolve('nonland', 'battlefield')
            self.receipts()
        for i, (spell, land) in enumerate(self.land_plan):
            self.search(i, spell, land)
        if self.mode=='late_source':
            self.source_cast()
        if self.mode=='cleanup':
            self.emit('cleanup')
            self.emit('refresh')
            self.reset_temporary()
            self.receipts()
        if self.mode.startswith('combat_') or self.mode in ('cannot_block','flying_blocker','fresh_source'):
            self.fight()
        if self.replay_cut is not None:
            tail = self.steps[self.replay_cut:]
            self.steps = (self.steps[:self.replay_cut]+[self.step('capture')]+tail+
                          [self.step('record_state'), self.step('ai_roundtrip'), self.step('restore')]+
                          copy.deepcopy(tail)+[self.step('assert_recorded_state')])
        return dict(name=f'LandfallSelfChangeRecipe_{self.csvid}_{self.variant}', card=self.name,
                    fixture='empty_main', seed=123, setup=self.setup, steps=self.steps,
                    _recipe=dict(family=FAMILY, version=2, variant=self.variant,
                                 params=self.declaration['params'], oracle_text=self.declaration['oracle_text'],
                                 untested_components=['Manual land play action and land-play limits',
                                                      'Standalone response windows for nested landfall triggers',
                                                      'Multiplayer and simultaneous land-entry trigger ordering']))


def expand(name, csvid, declaration, model, selected, registry, oracle):
    data = validate_witnesses(oracle, registry)
    if data['sources'].get(name, {}).get('registry', {}).get('csvid')!=csvid:
        raise ValueError('unreserved landfall source')
    return [LandfallCase(name, csvid, declaration, model, variant, registry, oracle).build() for variant in selected]
