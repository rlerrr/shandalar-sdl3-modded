"""Whole Oracle recurrence paired with entry effects or upkeep counter removal."""
from collections import Counter
import copy
import json
from pathlib import Path
import re

from counter_condition_recurrence_recipes import exact
from defensive_creature_recipes import reviewed_payment
from hybrid_creature_recipes import script_payments


FAMILY = 'recurrence_entry_upkeep'
ZONES = ('hand', 'graveyard', 'exile', 'library', 'battlefield')
LIBRARY = ('Forest', 'Island', 'Swamp', 'Mountain', 'Plains', 'Mountain', 'Forest', 'Plains',
           'Swamp', 'Island', 'Forest', 'Swamp')
NUMBERS = {str(n): n for n in range(1, 10)} | {'one': 1, 'two': 2, 'three': 3, 'four': 4, 'five': 5}


def pins():
    path = Path(__file__).parent/'data/recurrence_entry_upkeep_wave8_profiles.json'
    return json.loads(path.read_text(encoding='utf8'))


def oracle_model(record):
    if (record.get('layout') != 'normal' or record.get('card_faces') or record.get('selected_face')
            or not record.get('type_line', '').startswith('Creature — ')
            or any(not re.fullmatch(r'[1-9][0-9]*', record.get(key, '')) for key in ('power', 'toughness'))):
        raise ValueError('normal numeric recurrence creature required')
    keywords = record.get('keywords', [])
    recurrence_keywords = set(keywords) & {'Persist', 'Undying'}
    if len(recurrence_keywords) != 1 or len(keywords) != len(set(keywords)):
        raise ValueError('one reviewed recurrence keyword required')
    keyword = next(iter(recurrence_keywords))
    sign = '-1/-1' if keyword == 'Persist' else '+1/+1'
    recurrence = (f'{keyword} (When this creature dies, if it had no {sign} counters on it, '
                  f"return it to the battlefield under its owner's control with a {sign} counter on it.)")
    lines = record['oracle_text'].splitlines()
    if lines.count(recurrence) != 1:
        raise ValueError('full recurrence trigger and owner destination required')
    clauses = [line for line in lines if line != recurrence]
    tapped = 'This creature enters tapped.' in clauses
    if tapped:
        clauses.remove('This creature enters tapped.')
    if len(clauses) != 1:
        raise ValueError('one distinct reviewed recurrence companion clause required')
    clause = clauses[0]
    loss = re.fullmatch(r'When this creature enters, target opponent loses ([1-9]) life\.', clause)
    mill = re.fullmatch(r'When this creature enters, target player mills (\w+) cards\.', clause)
    if loss:
        effect = {'kind': 'opponent_loss', 'amount': int(loss[1])}
    elif mill and mill[1] in NUMBERS:
        effect = {'kind': 'target_mill', 'amount': NUMBERS[mill[1]]}
    elif clause == 'When this creature enters, it deals damage equal to its power to any target.':
        effect = {'kind': 'power_damage'}
    elif clause == 'At the beginning of your upkeep, remove a -1/-1 counter from each creature you control.':
        effect = {'kind': 'own_upkeep_minus1'}
    else:
        raise ValueError('unreviewed printed companion clause')
    if tapped and effect['kind'] != 'opponent_loss':
        raise ValueError('unreviewed tapped-entry composition')
    expected_keywords = recurrence_keywords | ({'Mill'} if effect['kind'] == 'target_mill' else set())
    if set(keywords) != expected_keywords:
        raise ValueError('printed companion and keyword metadata disagree')
    return dict(mechanism=keyword.lower(), enters_tapped=tapped, effect=effect)


def variants(model):
    common = ['entry', 'cycle', 'destroy', 'countered', 'bounce', 'excluded_counter',
              'death_replay', 'cast_replay', 'zero_mana', 'real_reset']
    effect = model['effect']['kind']
    extras = {
        'opponent_loss': ['opponent_shroud', 'own_untap'],
        'target_mill': ['short_library', 'empty_library', 'exact_library'],
        'power_damage': ['creature_target', 'planeswalker_target'],
        'own_upkeep_minus1': ['upkeep_scope', 'upkeep_replay', 'upkeep_empty'],
    }[effect]
    if model['hybrid_pairs']:
        extras = extras + ['hybrid_alternate', 'hybrid_mixed']
    cells = [f'{mode}_{owner}' for owner in (0, 1) for mode in common+extras]
    if effect in ('target_mill', 'power_damage'):
        # Human self-targeting is deterministic. AI preference need not expose it.
        cells.append('self_target_0')
    return cells


def spec(declaration, record):
    if (declaration.get('family') != FAMILY or type(declaration.get('version')) is not int
            or declaration['version'] != 2 or declaration.get('oracle_text') != record.get('oracle_text')):
        raise ValueError('whole Oracle recurrence entry/upkeep declaration required')
    model = oracle_model(record)
    params = declaration.get('params')
    expected = dict(schema=8, **model)
    if re.findall(r'\{([WUBRG])/([WUBRG])\}', record['mana_cost']):
        expected['payment_colors'] = params.get('payment_colors') if isinstance(params, dict) else None
    if not exact(params, expected):
        raise ValueError('typed params must match every reviewed Oracle clause')
    mana, pairs = reviewed_payment(record, params)
    result = dict(model, family=FAMILY, mana=mana, hybrid_pairs=pairs,
                  power=int(record['power']), toughness=int(record['toughness']))
    selected = declaration.get('variants')
    if (not isinstance(selected, list) or not selected or any(not isinstance(value, str) for value in selected)
            or len(selected) != len(set(selected)) or set(selected)-set(variants(result))):
        raise ValueError('explicit supported distinct variants required')
    return result


def validate_witnesses(oracle, registry):
    data = pins()
    for name, pin in (data['sources'] | data['witnesses']).items():
        row = registry.get(name, {})
        if (not exact(oracle.get(name), pin['oracle']) or not exact(row, pin['registry'])
                or row.get('kind') != 'card' or row.get('handler') in (None, '', '0', 'NULL', 'card_dummy')):
            raise ValueError('complete implemented card pin changed: '+name)
    return data


class Scenario:
    def __init__(self, name, csvid, declaration, model, variant, registry, oracle):
        from recipes import card, step, cost_from_oracle
        self.card, self.step, self.cost = card, step, cost_from_oracle
        self.name, self.csvid, self.model = name, csvid, model
        self.registry, self.oracle = registry, oracle
        self.variant = variant
        self.mode, actor = variant.rsplit('_', 1)
        self.owner = int(actor)
        self.enemy = 1-self.owner
        self.current = self.owner
        self.recipient = self.owner if self.mode == 'self_target' else self.enemy
        self.effect = model['effect']['kind']
        self.params = copy.deepcopy(declaration['params'])
        self.declaration = declaration
        self.pairs = model['hybrid_pairs']
        if self.mode == 'hybrid_alternate':
            self.params['payment_colors'] = [b if a == c else a for (a, b), c in zip(self.pairs, self.params['payment_colors'])]
        elif self.mode == 'hybrid_mixed':
            self.params['payment_colors'] = [pair[i % 2] for i, pair in enumerate(self.pairs)]
        self.source_fee, _ = reviewed_payment(oracle[name], self.params)
        self.setup, self.steps, self.entities = [], [], {}
        self.zones = [{zone: [] for zone in ZONES} for _ in range(2)]
        self.life = [20, 20]
        self.mana = [[0]*6, [0]*6]
        self.source_ref = 'source'
        self.returns = 0
        self.replay_cut = None
        self._build_setup()

    def emit(self, op, **kw):
        self.steps.append(self.step(op, **kw))

    def add(self, name, ref, player, zone='hand'):
        item = self.card(name, ref, player, zone)
        if zone == 'battlefield':
            item['state'] = ['untapped']
        self.setup.append(item)
        self.entities[ref] = dict(name=name, player=player, zone=zone, plus=0, minus=0,
                                  damage=0, sick=0, tapped=0)
        self.zones[player][zone].append(name)

    def _build_setup(self):
        self.add(self.name, 'source', self.owner)
        for player in range(2):
            self.add('Plains', f'reserve_hand_{player}', player)
            self.add('Swamp', f'reserve_grave_{player}', player, 'graveyard')
            self.add('Island', f'reserve_exile_{player}', player, 'exile')
            length = {'short_library': 3, 'exact_library': 5, 'empty_library': 0}.get(self.mode, len(LIBRARY))
            cards = LIBRARY[:length] if player == self.recipient else LIBRARY
            for i, name in enumerate(cards):
                self.add(name, f'library_{player}_{i}', player, 'library')
        if self.mode == 'countered':
            self.add('Counterspell', 'counter', self.enemy)
        if self.mode == 'bounce':
            self.add('Unsummon', 'bounce_spell', self.enemy)
        if self.mode in ('cycle', 'real_reset', 'upkeep_scope', 'upkeep_replay'):
            for i in range(1, 4 if self.mode.startswith('upkeep_') else 3):
                self.add('Murder', f'death_{i}', self.enemy)
        if self.mode in ('death_replay', 'excluded_counter', 'short_library', 'exact_library', 'empty_library',
                         'opponent_shroud', 'own_untap', 'self_target', 'creature_target', 'planeswalker_target'):
            self.add('Murder', 'death_1', self.enemy)
        if self.mode in ('excluded_counter', 'real_reset'):
            control_plus = (self.model['mechanism'] == 'undying') == (self.mode == 'excluded_counter')
            self.add('Burst of Strength' if control_plus else 'Fume Spitter', 'counter_control', self.owner)
        if self.mode == 'opponent_shroud':
            self.add('Ivory Mask', 'mask', self.enemy, 'battlefield')
        if self.mode == 'creature_target':
            self.add('Serra Angel', 'damage_target', self.enemy, 'battlefield')
        if self.mode == 'planeswalker_target':
            self.add('Jace Beleren', 'damage_target', self.enemy)
        if self.mode == 'upkeep_scope':
            self.add('Serra Angel', 'ally_counter', self.owner, 'battlefield')
            self.add('Serra Angel', 'enemy_counter', self.enemy, 'battlefield')
            self.add('Elvish Mystic', 'ally_plain', self.owner, 'battlefield')
            for i in range(3):
                self.add('Fume Spitter', f'spitter_{i}', self.owner)

    def fund(self, player, mana):
        self.mana[player] = list(mana)
        for color, amount in enumerate(mana):
            self.emit('set_mana', player=player, color=color, value=amount)
        self.emit('refresh')

    def pay(self, player, mana):
        self.mana[player] = [available-fee for available, fee in zip(self.mana[player], mana)]
        if min(self.mana[player]) < 0:
            raise ValueError('underfunded fixture payment')

    def move(self, ref, zone):
        entity = self.entities[ref]
        player, name = entity['player'], entity['name']
        if entity['zone'] != 'stack':
            self.zones[player][entity['zone']].remove(name)
        if zone != 'stack':
            self.zones[player][zone].append(name)
        entity['zone'] = zone

    def cast_pending(self, ref, fee, target=None, response=False):
        entity = self.entities[ref]
        player = entity['player']
        self.fund(player, fee)
        if target is not None:
            self.emit('choose_target', player=player, ref=target)
        self.emit('cast_response' if response else 'cast', ref=ref, value=1)
        self.pay(player, fee)
        self.move(ref, 'stack')
        self.emit('assert_stack', value=2 if response else 1)
        self.emit('assert_card', ref=ref, property='in_hand', value=0)
        self.assert_mana()

    def resolve_spell(self, ref, destination):
        self.emit('resolve')
        self.emit('refresh')
        self.emit('assert_choices_consumed')
        self.move(ref, destination)

    def assert_mana(self):
        for player, pool in enumerate(self.mana):
            for color, amount in enumerate(pool):
                self.emit('assert_mana', player=player, color=color, value=amount)

    def receipts(self):
        self.emit('assert_stack', value=0)
        self.emit('assert_choices_consumed')
        self.assert_mana()
        names = sorted({item['name'] for item in self.setup})
        for player in range(2):
            self.emit('assert_life', player=player, value=self.life[player])
            for zone in ZONES:
                cards = self.zones[player][zone]
                counts = Counter(cards)
                self.emit('assert_zone', player=player, zone=zone, value=len(cards))
                for name in names:
                    self.emit('assert_zone_multiplicity', player=player, zone=zone,
                              index=self.registry[name]['csvid'], value=counts[name])
                if zone in ('library', 'graveyard'):
                    for i, card in enumerate(cards):
                        self.emit('assert_zone_card', player=player, zone=zone, index=i,
                                  value=self.registry[card]['csvid'])
        for ref, entity in self.entities.items():
            if entity['zone'] != 'battlefield':
                continue
            self.emit('assert_card', ref=ref, property='in_play', value=1)
            if 'loyalty' in entity:
                self.emit('assert_counter', ref=ref, counter_kind='loyalty', value=entity['loyalty'])
            if not self.oracle[entity['name']]['type_line'].startswith('Creature — '):
                continue
            for kind in ('plus', 'minus'):
                self.emit('assert_counter', ref=ref, counter_kind=kind+'1', value=entity[kind])
            for prop in ('power', 'toughness'):
                base = int(self.oracle[entity['name']][prop])
                self.emit('assert_card', ref=ref, property=prop, value=base+entity['plus']-entity['minus'])
            for prop in ('damage', 'tapped', 'sick'):
                self.emit('assert_card', ref=ref, property=prop, value=entity[prop])

    def entry_choice(self):
        if self.effect == 'target_mill':
            self.emit('choose_player', player=self.owner, value=self.recipient)
        elif self.effect == 'power_damage':
            if self.mode in ('creature_target', 'planeswalker_target'):
                self.emit('choose_target', player=self.owner, ref='damage_target')
            else:
                self.emit('choose_player', player=self.owner, value=self.recipient)

    def entry_effect(self):
        effect = self.model['effect']
        if self.effect == 'opponent_loss' and self.mode != 'opponent_shroud':
            self.life[self.enemy] -= effect['amount']
        elif self.effect == 'target_mill':
            library = self.zones[self.recipient]['library']
            n = min(effect['amount'], len(library))
            self.zones[self.recipient]['graveyard'].extend(library[:n])
            del library[:n]
        elif self.effect == 'power_damage':
            source = self.entities[self.source_ref]
            power = self.model['power']+source['plus']-source['minus']
            if self.mode == 'creature_target':
                target = self.entities['damage_target']
                target['damage'] += power
                if target['damage'] >= int(self.oracle[target['name']]['toughness']):
                    self.move('damage_target', 'graveyard')
            elif self.mode == 'planeswalker_target':
                self.entities['damage_target']['loyalty'] -= power
                if self.entities['damage_target']['loyalty'] == 0:
                    self.move('damage_target', 'graveyard')
            else:
                self.life[self.recipient] -= power

    def source_cast(self):
        self.cast_pending('source', self.source_fee)
        self.emit('assert_card', ref='source', property='in_play', value=0)
        for player in range(2):
            self.emit('assert_life', player=player, value=self.life[player])
        if self.effect == 'target_mill':
            self.emit('assert_zone', player=self.recipient, zone='library',
                      value=len(self.zones[self.recipient]['library']))
        if self.mode == 'cast_replay':
            self.replay_cut = len(self.steps)
        self.entry_choice()
        self.resolve_spell('source', 'battlefield')
        self.entities['source'].update(sick=1, tapped=int(self.model['enters_tapped']))
        self.entry_effect()
        self.receipts()

    def death(self, spell='death_1'):
        ref = self.source_ref
        old = self.entities[ref]
        condition = 'minus' if self.model['mechanism'] == 'persist' else 'plus'
        returns = old[condition] == 0
        if spell:
            self.cast_pending(spell, self.cost(self.oracle['Murder']), target=ref)
        if self.mode == 'death_replay':
            self.replay_cut = len(self.steps)
        if returns:
            self.entry_choice()
        if spell:
            self.resolve_spell(spell, 'graveyard')
        else:
            self.emit('destroy', ref=ref)
            self.emit('resolve')
            self.emit('refresh')
            self.emit('assert_choices_consumed')
        self.move(ref, 'graveyard')
        if returns:
            self.returns += 1
            self.source_ref = f'returned_{self.returns}'
            self.entities[self.source_ref] = dict(old, zone='graveyard', damage=0, sick=1,
                                                  tapped=int(self.model['enters_tapped']), plus=0, minus=0)
            self.entities[self.source_ref][condition] = 1
            self.move(self.source_ref, 'battlefield')
            self.emit('bind_permanent', ref=self.source_ref, player=self.owner, value=self.csvid)
            self.entry_effect()
        else:
            self.emit('assert_card', ref=ref, property='in_play', value=0)
        self.receipts()

    def counter_from(self, ref, target):
        name = self.entities[ref]['name']
        self.cast_pending(ref, self.cost(self.oracle[name]), target=target if name == 'Burst of Strength' else None)
        if name == 'Burst of Strength':
            self.resolve_spell(ref, 'graveyard')
            entity = self.entities[target]
            entity['plus'] += 1
            entity['tapped'] = 0
        else:
            self.resolve_spell(ref, 'battlefield')
            self.emit('require_can_activate', ref=ref, value=1)
            self.emit('choose_target', player=self.owner, ref=target)
            self.emit('activate', ref=ref, value=1)
            self.emit('assert_stack', value=1)
            self.resolve_spell(ref, 'graveyard')
            entity = self.entities[target]
            entity['minus'] += 1
        if entity['plus'] and entity['minus']:
            entity['plus'] -= 1
            entity['minus'] -= 1
        self.receipts()

    def next_upkeep(self):
        self.emit('advance_turn')
        self.current = 1-self.current
        self.emit('untap_step', player=self.current)
        for entity in self.entities.values():
            if entity['zone'] == 'battlefield':
                entity['damage'] = 0
                if entity['player'] == self.current:
                    entity.update(tapped=0, sick=0)
        if self.mode == 'upkeep_replay' and self.current == self.owner and self.replay_cut is None:
            self.replay_cut = len(self.steps)
        self.emit('upkeep_step', player=self.current)
        if self.effect == 'own_upkeep_minus1' and self.current == self.owner:
            for entity in self.entities.values():
                if (entity['zone'] == 'battlefield' and entity['player'] == self.owner and entity['minus']
                        and self.oracle[entity['name']]['type_line'].startswith('Creature — ')):
                    entity['minus'] -= 1
        self.emit('turn', player=self.current, value=0)
        self.receipts()

    def build(self):
        self.emit('reset_death_history')
        self.emit('turn', player=self.owner, value=0)
        for player in range(2):
            self.fund(player, [0]*6)
        if self.mode == 'planeswalker_target':
            self.emit('turn', player=self.enemy, value=0)
            self.cast_pending('damage_target', self.cost(self.oracle['Jace Beleren']))
            self.resolve_spell('damage_target', 'battlefield')
            self.entities['damage_target']['loyalty'] = int(self.oracle['Jace Beleren']['loyalty'])
            self.emit('advance_turn')
            self.emit('untap_step', player=self.owner)
            self.emit('turn', player=self.owner, value=0)
        if self.mode == 'zero_mana':
            self.emit('cast', ref='source', value=0)
            self.receipts()
        elif self.mode == 'countered':
            self.cast_pending('source', self.source_fee)
            self.emit('announce_spell', ref='source')
            self.cast_pending('counter', self.cost(self.oracle['Counterspell']), response=True)
            self.emit('resolve_response')
            self.move('counter', 'graveyard')
            self.move('source', 'graveyard')
            self.receipts()
        else:
            self.source_cast()
            self.play_followup()
        if self.replay_cut is not None:
            index = self.replay_cut
            replay = self.steps[index:]
            self.steps = (self.steps[:index]+[self.step('capture')]+replay+
                          [self.step('record_state'), self.step('ai_roundtrip'), self.step('restore')]+
                          copy.deepcopy(replay)+[self.step('assert_recorded_state')])
        case = dict(name=f'RecurrenceEntryUpkeepRecipe_{self.csvid}_{self.variant}', card=self.name,
                    fixture='empty_main', seed=123, setup=self.setup, steps=self.steps,
                    _recipe=dict(family=FAMILY, version=2, variant=self.variant, params=self.params,
                                 oracle_text=self.declaration['oracle_text'],
                                 untested_components=['Separate response windows within synchronous death/entry trigger processing',
                                                      'Multiplayer and additional control-transfer compositions']))
        if self.pairs:
            script_payments(case, dict(self.declaration, params=self.params), self.oracle[self.name])
        return case

    def play_followup(self):
        if self.mode == 'bounce':
            self.cast_pending('bounce_spell', self.cost(self.oracle['Unsummon']), target='source')
            self.resolve_spell('bounce_spell', 'graveyard')
            self.move('source', 'hand')
            self.emit('bind_hand', ref='bounced', player=self.owner, value=self.csvid)
            self.receipts()
            return
        if self.mode == 'upkeep_empty':
            self.next_upkeep()
            self.next_upkeep()
            return
        if self.mode == 'destroy':
            self.death(spell=None)
            return
        if self.mode == 'excluded_counter':
            self.counter_from('counter_control', 'source')
        if 'death_1' not in self.entities:
            return
        self.death()
        if self.mode in ('own_untap', 'upkeep_scope', 'upkeep_replay'):
            if self.mode == 'upkeep_scope':
                for i, target in enumerate(('ally_counter', 'ally_counter', 'enemy_counter')):
                    self.counter_from(f'spitter_{i}', target)
            self.next_upkeep()
            self.next_upkeep()
        if self.mode == 'real_reset':
            self.counter_from('counter_control', self.source_ref)
        if 'death_2' in self.entities:
            self.death('death_2')
        if 'death_3' in self.entities:
            self.death('death_3')


def expand(name, csvid, declaration, model, selected, registry, oracle):
    data = validate_witnesses(oracle, registry)
    if data['sources'].get(name, {}).get('registry', {}).get('csvid') != csvid:
        raise ValueError('unreserved recurrence entry/upkeep source')
    return [Scenario(name, csvid, declaration, model, variant, registry, oracle).build() for variant in selected]
