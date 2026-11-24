"""Landfall Expeditions using real land entry, quest thresholds, and sacrifice."""
from collections import Counter
import copy
import json
from pathlib import Path
import re

from counter_condition_recurrence_recipes import exact


FAMILY = 'landfall_quest_expedition'
TRIGGER = 'Landfall — Whenever a land you control enters, you may put a quest counter on this enchantment.'
COST = 'Remove three quest counters from this enchantment and sacrifice it: '
EFFECTS = {
    'Draw two cards.': dict(kind='draw', quantity=2),
    'Search your library for up to two basic land cards, put them onto the battlefield tapped, then shuffle.':
        dict(kind='basic_search', limit=2, tapped=True),
    'Return up to two target creature cards from your graveyard to your hand.':
        dict(kind='grave_return', limit=2),
    'You gain 8 life.': dict(kind='life', amount=8),
    'Create a 7/1 red Elemental creature token with trample and haste. Exile it at the beginning of the next end step.':
        dict(kind='delayed_token', power=7, toughness=1, color='R', subtype='Elemental',
             keywords=['Trample', 'Haste'], expires='next_end_step'),
}
COMMON = ('cast', 'unfunded', 'threshold', 'activate', 'extra_counter', 'pending_replay',
          'opponent_land', 'removed_source', 'cleanup', 'offturn')
EXTRAS = {'draw': (), 'basic_search': ('one_match', 'no_match'), 'grave_return': ('one_match', 'no_match'),
          'life': ('low_life',), 'delayed_token': ('haste_trample', 'expiry_replay')}


def pins():
    path = Path(__file__).parent/'data/landfall_quest_expedition_wave8_profiles.json'
    return json.loads(path.read_text(encoding='utf8'))


def oracle_model(record):
    if (record.get('layout') != 'normal' or record.get('card_faces') or record.get('selected_face')
            or record.get('type_line') != 'Enchantment' or record.get('keywords') != ['Landfall']
            or not re.fullmatch(r'(?:\{[1-9]\})?\{[WUBRG]\}', record.get('mana_cost', ''))):
        raise ValueError('reviewed normal landfall enchantment required')
    lines = record['oracle_text'].splitlines()
    if len(lines) != 2 or lines[0] != TRIGGER or not lines[1].startswith(COST):
        raise ValueError('optional own-land quest and full threshold/sacrifice required')
    effect = EFFECTS.get(lines[1].removeprefix(COST))
    if effect is None:
        raise ValueError('unreviewed Expedition effect')
    return dict(quest_cost=3, effect=copy.deepcopy(effect))


def variants(model):
    effect = model['effect']['kind']
    cells = [f'{mode}_{owner}' for owner in (0, 1) for mode in (*COMMON, *EXTRAS[effect])]
    cells += ['decline_0', 'decline_then_pay_0']
    if effect in ('basic_search', 'grave_return'):
        cells += ['choose_one_0', 'choose_none_0']
    return cells


def spec(declaration, record):
    from recipes import cost_from_oracle
    if (declaration.get('family') != FAMILY or type(declaration.get('version')) is not int
            or declaration['version'] != 2 or declaration.get('oracle_text') != record.get('oracle_text')):
        raise ValueError('whole Oracle version-two Expedition required')
    model = oracle_model(record)
    if not exact(declaration.get('params'), dict(schema=8, **model)):
        raise ValueError('typed cost and result must match Oracle')
    selected = declaration.get('variants')
    if (not isinstance(selected, list) or not selected or any(not isinstance(v, str) for v in selected)
            or len(set(selected)) != len(selected) or set(selected)-set(variants(model))):
        raise ValueError('distinct supported Expedition variants required')
    return dict(model, family=FAMILY, cast_fee=cost_from_oracle(record))


def validate_witnesses(oracle, registry):
    data = pins()
    for name, pin in (data['sources'] | data['witnesses']).items():
        row = registry.get(name, {})
        if (not exact(oracle.get(name), pin['oracle']) or not exact(row, pin['registry'])
                or row.get('kind') != 'card' or row.get('handler') in (None, 'card_dummy', '0', 'NULL')):
            raise ValueError('complete implemented card pin changed: '+name)
    token = data['token_binding']
    if not exact(registry.get('Elemental'), token):
        raise ValueError('Elemental binding identity changed')
    return data


class ExpeditionCase:
    def __init__(self, name, csvid, declaration, model, variant, rows, oracle):
        from recipes import card, step, cost_from_oracle
        self.card, self.step, self.cost = card, step, cost_from_oracle
        self.name, self.csvid, self.declaration, self.model = name, csvid, declaration, model
        self.variant, self.rows, self.oracle = variant, rows, oracle
        self.mode, owner = variant.rsplit('_', 1)
        self.owner = int(owner)
        self.enemy = 1-self.owner
        self.turn = self.enemy if self.mode in ('opponent_land', 'offturn') else self.owner
        self.actor = self.enemy if self.mode=='opponent_land' else self.owner
        self.effect = model['effect']
        self.kind = self.effect['kind']
        self.counters = 0
        self.life, self.pool = [20, 20], [[0]*6, [0]*6]
        self.shuffle_count = 0
        self.replay_cut = None
        self.zones = [{z:[] for z in ('hand','battlefield','graveyard','exile','library')} for _ in (0, 1)]
        self.setup, self.steps, self.entities = [], [], {}
        self.entrants = self.entry_count()
        self.prepare_setup()

    def entry_count(self):
        if self.mode in ('cast', 'unfunded'):
            return 0
        if self.mode in ('opponent_land', 'removed_source', 'decline', 'cleanup'):
            return 1
        return self.model['quest_cost']+int(self.mode in ('extra_counter', 'decline_then_pay'))

    def emit(self, op, **kw):
        self.steps.append(self.step(op, **kw))

    def add(self, name, ref, actor, zone='hand'):
        item = self.card(name, ref, actor, zone)
        if zone=='battlefield':
            item['state'] = ['untapped']
        self.setup.append(item)
        self.entities[ref] = dict(name=name, player=actor, zone=zone)
        self.zones[actor][zone].append(name)

    def prepare_setup(self):
        source_zone = 'hand' if self.mode in ('cast','unfunded') else 'battlefield'
        self.add(self.name, 'source', self.owner, source_zone)
        for player in (0, 1):
            self.add('Darksteel Relic', f'hand_reserve_{player}', player)
            self.add('Swamp', f'grave_reserve_{player}', player, 'graveyard')
            self.add('Mountain', f'exile_reserve_{player}', player, 'exile')
        self.add('Knight of Glory', 'foreign_creature', self.enemy, 'graveyard')
        for i, name in enumerate(('Forest', 'Plains', 'Island', 'Mountain')[:self.entrants]):
            self.add('Walking Atlas', f'atlas_{i}', self.actor, 'battlefield')
            self.add(name, f'land_{i}', self.actor)
        library = ['Darksteel Relic', 'Elvish Mystic', 'Walking Atlas']
        if self.kind=='basic_search':
            library = ['Darksteel Relic', 'Dryad Arbor']
            if self.mode!='no_match':
                library.append('Swamp')
            if self.mode not in ('no_match', 'one_match'):
                library.append('Mountain')
        for i, name in enumerate(library):
            self.add(name, f'library_{i}', self.owner, 'library')
        self.add('Island', 'opposing_library', self.enemy, 'library')
        if self.kind=='grave_return' and self.mode!='no_match':
            self.add('Elvish Mystic', 'own_grave_1', self.owner, 'graveyard')
            if self.mode!='one_match':
                self.add('Walking Atlas', 'own_grave_2', self.owner, 'graveyard')
        if self.mode=='removed_source':
            self.add('Disenchant', 'disenchant', self.enemy)
        if self.mode=='haste_trample':
            self.add('Drift of Phantasms', 'blocker', self.enemy, 'battlefield')

    def move(self, ref, zone):
        entity = self.entities[ref]
        current = entity['zone']
        zones = self.zones[entity['player']]
        if current!='stack':
            zones[current].remove(entity['name'])
        if zone!='stack':
            zones[zone].append(entity['name'])
        entity['zone'] = zone

    def fund(self, player, vector):
        self.pool[player] = list(vector)
        for color, amount in enumerate(vector):
            self.emit('set_mana', player=player, color=color, value=amount)
        self.emit('refresh')

    def cast(self, ref, fee, allowed=True, target=None):
        actor = self.entities[ref]['player']
        self.fund(actor, fee if allowed else [0]*6)
        if target is not None:
            self.emit('choose_target', player=actor, ref=target)
        self.emit('cast', ref=ref, value=int(allowed))
        self.emit('assert_stack', value=int(allowed))
        if allowed:
            self.move(ref,'stack')
            self.pool[actor] = [0]*6
        self.mana_checks()

    def mana_checks(self):
        for player in (0, 1):
            for color, amount in enumerate(self.pool[player]):
                self.emit('assert_mana', player=player, color=color, value=amount)

    def resolve(self):
        self.emit('resolve')
        self.emit('refresh')
        self.emit('assert_choices_consumed')

    def counter_check(self):
        self.emit('assert_counter', ref='source', counter_kind='quest', value=self.counters)

    def permission(self):
        self.emit('require_can_activate', ref='source', value=int(self.counters>=self.model['quest_cost']))

    def receipts(self):
        self.emit('assert_stack', value=0)
        self.emit('assert_choices_consumed')
        self.mana_checks()
        self.emit('assert_shuffle_count', player=self.owner, value=self.shuffle_count)
        universe = {item['name'] for item in self.setup}
        if self.kind=='delayed_token':
            universe.add('Elemental')
        for player, zones in enumerate(self.zones):
            self.emit('assert_life', player=player, value=self.life[player])
            for zone, cards in zones.items():
                self.emit('assert_zone', player=player, zone=zone, value=len(cards))
                counts = Counter(cards)
                for name in sorted(universe):
                    self.emit('assert_zone_multiplicity', player=player, zone=zone,
                              index=self.rows[name]['csvid'], value=counts[name])
                if zone=='library' and (player!=self.owner or not self.shuffle_count):
                    for i, name in enumerate(cards):
                        self.emit('assert_zone_card', player=player, zone='library', index=i, value=self.rows[name]['csvid'])
        if self.entities['source']['zone']=='battlefield':
            self.counter_check()

    def put_land(self, i):
        atlas, land = f'atlas_{i}', f'land_{i}'
        active = self.actor==self.owner and self.entities['source']['zone']=='battlefield'
        accept = active and not (i==0 and self.mode in ('decline', 'decline_then_pay'))
        self.emit('require_can_activate', ref=atlas, value=1)
        self.emit('activate', ref=atlas, value=1)
        self.emit('assert_stack', value=1)
        self.emit('assert_card', ref=atlas, property='tapped', value=1)
        self.emit('assert_card', ref=land, property='in_hand', value=1)
        if active:
            self.counter_check()
        self.emit('choose_zone_card', player=self.actor, value=self.rows[self.entities[land]['name']]['csvid'])
        if accept and self.owner==0:
            self.emit('choose_trigger', player=0, ref='source')
        self.resolve()
        self.move(land, 'battlefield')
        self.emit('assert_card', ref=land, property='in_play', value=1)
        self.emit('assert_card', ref=land, property='tapped', value=0)
        if accept:
            self.counters += 1
        if self.entities['source']['zone']=='battlefield':
            self.counter_check()
            self.permission()
        if self.mode=='threshold' and self.counters<self.model['quest_cost']:
            self.emit('activate', ref='source', value=0)
            self.emit('assert_stack', value=0)

    def queue_optional_selection(self, zone, chosen, eligible, limit):
        for name in chosen:
            self.emit('choose_zone_card', player=self.owner, value=self.rows[name]['csvid'])
        if len(chosen)<min(limit, len(eligible)):
            # A legal unchosen card supplies the harness fallback if the game
            # wrongly forces selection. The receipt checks that it did not.
            fallback = next(name for name in eligible if name not in chosen)
            self.emit('choose_optional_zone', player=self.owner, value=self.rows[fallback]['csvid'])
        elif len(chosen)<limit and self.zones[self.owner][zone]:
            # A depleted eligible set may either prompt a decline or auto-close.
            fallback = next((n for n in self.zones[self.owner][zone] if n not in chosen), None)
            if fallback:
                self.emit('choose_optional_zone', player=self.owner, value=self.rows[fallback]['csvid'])

    def selection(self, zone):
        if zone=='graveyard':
            eligible = [n for n in self.zones[self.owner][zone]
                        if 'Creature' in self.oracle[n]['type_line'].partition(' — ')[0]]
        else:
            eligible = [n for n in self.zones[self.owner][zone] if self.oracle[n]['type_line'].startswith('Basic Land — ')]
        limit = self.effect['limit']
        count = {'choose_one':1, 'choose_none':0}.get(self.mode, limit)
        return eligible[:count], eligible

    def sacrifice(self):
        self.counter_check()
        self.permission()
        targets = []
        if self.kind=='grave_return':
            targets, eligible = self.selection('graveyard')
            self.queue_optional_selection('graveyard', targets, eligible, self.effect['limit'])
        self.emit('activate', ref='source', value=1)
        self.move('source', 'graveyard')
        self.counters -= self.model['quest_cost']
        self.emit('assert_stack', value=1)
        self.emit('assert_card', ref='source', property='in_play', value=0)
        self.emit('assert_zone_multiplicity', player=self.owner, zone='graveyard', index=self.csvid, value=1)
        self.emit('assert_zone', player=self.owner, zone='library', value=len(self.zones[self.owner]['library']))
        self.emit('assert_life', player=self.owner, value=self.life[self.owner])
        self.mana_checks()
        if self.mode=='pending_replay':
            self.replay_cut = len(self.steps)
        if self.kind=='basic_search':
            targets, eligible = self.selection('library')
            self.queue_optional_selection('library', targets, eligible, self.effect['limit'])
        self.resolve()
        self.emit('assert_forced_zone_choices', value=0)
        self.apply_effect(targets)
        self.receipts()

    def apply_effect(self, targets):
        if self.kind=='draw':
            for name in self.zones[self.owner]['library'][:self.effect['quantity']]:
                ref = next(r for r,e in self.entities.items() if e['zone']=='library' and e['player']==self.owner and e['name']==name)
                self.move(ref, 'hand')
        elif self.kind=='life':
            self.life[self.owner] += self.effect['amount']
        elif self.kind in ('basic_search','grave_return'):
            origin, destination = ('library','battlefield') if self.kind=='basic_search' else ('graveyard','hand')
            for i,name in enumerate(targets):
                ref = next(r for r,e in self.entities.items() if e['zone']==origin and e['player']==self.owner and e['name']==name)
                self.move(ref, destination)
                if destination=='battlefield':
                    land_ref = f'fetched_{i}'
                    # A same-name land from Walking Atlas may already be present.
                    copy_index = self.zones[self.owner]['battlefield'].count(name)-1
                    self.emit('bind_permanent_copy', ref=land_ref, player=self.owner, value=self.rows[name]['csvid'], index=copy_index)
                    self.emit('assert_card', ref=land_ref, property='tapped', value=1)
            if self.kind=='basic_search':
                self.shuffle_count += 1
        elif self.kind=='delayed_token':
            from recipes import COLORS
            name = self.effect['subtype']
            self.zones[self.owner]['battlefield'].append(name)
            token_id = self.rows[name]['csvid']
            self.emit('bind_permanent', ref='token', player=self.owner, value=token_id)
            for prop,value in [('token',1), ('color',1 << COLORS[self.effect['color']]), ('subtype_'+name,1)]:
                self.emit('assert_matching_permanents', player=self.owner, index=token_id, property=prop, value=value)
            for prop,value in [('power',self.effect['power']), ('toughness',self.effect['toughness']), ('tapped',0)]:
                self.emit('assert_card', ref='token', property=prop, value=value)

    def expire_token(self):
        if self.mode=='haste_trample':
            self.emit('begin_attackers')
            self.emit('attack', ref='token', value=1)
            self.emit('finish_attackers')
            self.emit('begin_blockers')
            self.emit('block', ref='blocker', other='token', value=1)
            self.emit('advance_combat')
            self.emit('combat_damage')
            self.emit('end_combat')
            self.move('blocker', 'graveyard')
            self.life[self.enemy] -= self.effect['power']-int(self.oracle['Drift of Phantasms']['toughness'])
            self.receipts()
        if self.mode=='expiry_replay':
            self.replay_cut = len(self.steps)
        self.emit('cleanup')
        self.emit('refresh')
        self.zones[self.owner]['battlefield'].remove('Elemental')
        self.emit('assert_card', ref='token', property='in_play', value=0)
        self.receipts()

    def build(self):
        self.emit('reset_death_history')
        self.emit('turn', player=self.turn, value=0)
        for player in (0, 1):
            self.fund(player, [0]*6)
        if self.mode=='low_life':
            self.life[self.owner] = 3
            self.emit('set_life', player=self.owner, value=3)
        if self.mode in ('cast','unfunded'):
            allowed = self.mode=='cast'
            self.cast('source', self.model['cast_fee'], allowed)
            if allowed:
                self.resolve()
                self.move('source','battlefield')
        if self.entities['source']['zone']=='battlefield':
            self.counter_check()
            self.permission()
        if self.mode=='removed_source':
            self.cast('disenchant', self.cost(self.oracle['Disenchant']), target='source')
            self.resolve()
            self.move('disenchant','graveyard')
            self.move('source','graveyard')
        for index in range(self.entrants):
            self.put_land(index)
        if self.mode in ('threshold','opponent_land','decline'):
            if self.counters<self.model['quest_cost']:
                self.emit('activate', ref='source', value=0)
            self.receipts()
        elif self.mode=='cleanup':
            self.emit('cleanup')
            self.emit('refresh')
            self.receipts()
        elif self.counters>=self.model['quest_cost']:
            self.sacrifice()
            if self.kind=='delayed_token':
                self.expire_token()
        else:
            self.receipts()
        if self.replay_cut is not None:
            cut = self.replay_cut
            tail = self.steps[cut:]
            self.steps = (self.steps[:cut]+[self.step('capture')]+tail+
                          [self.step('record_state'), self.step('ai_roundtrip'), self.step('restore')]+
                          copy.deepcopy(tail)+[self.step('assert_recorded_state')])
        return dict(name=f'LandfallQuestExpeditionRecipe_{self.csvid}_{self.variant}', card=self.name,
                    fixture='empty_main', seed=123, setup=self.setup, steps=self.steps,
                    _recipe=dict(family=FAMILY, version=2, variant=self.variant, params=self.declaration['params'],
                                 oracle_text=self.declaration['oracle_text'],
                                 untested_components=['Separate counter removal receipt unavailable after the same cost sacrifices its source',
                                                      'Manual land play and simultaneous land entry',
                                                      'Voluntary decline controls use the human controller']))


def expand(name, csvid, declaration, model, selected, registry, oracle):
    data = validate_witnesses(oracle, registry)
    if data['sources'].get(name, {}).get('registry', {}).get('csvid')!=csvid:
        raise ValueError('unreserved Expedition source')
    return [ExpeditionCase(name, csvid, declaration, model, variant, registry, oracle).build() for variant in selected]
