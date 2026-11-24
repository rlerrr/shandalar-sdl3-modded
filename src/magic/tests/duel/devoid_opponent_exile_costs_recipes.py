"""Devoid spells and abilities that return actual opponent-owned exile cards."""

from collections import Counter
import copy
import json
from pathlib import Path

from cast_untap_trigger_recipes import exact
from devoid_case_support import DevoidBoard, devoid_rules


FAMILY = 'devoid_opponent_exile_costs'
PROFILE = Path(__file__).parent / 'data' / f'{FAMILY}_wave8_profiles.json'
PROCESS = "put a card an opponent owns from exile into that player's graveyard"
ENTRY = f'When this creature enters, you may {PROCESS}. If you do, '
ACTIVATION = f', {PROCESS.capitalize()}: '
SCION = 'Eldrazi Scion'
MODELS = {
    ('{2}{U}' + ACTIVATION + 'Tap target creature.',): 'tap',
    (ENTRY + 'each opponent discards a card.',): 'discard',
    (ENTRY + "return target creature to its owner's hand.",): 'bounce',
    ('{2}' + ACTIVATION + 'Draw a card, then discard a card.',): 'loot',
    ('As an additional cost to cast this spell, ' + PROCESS + '.',
     'Processor Assault deals 5 damage to target creature.'): 'assault',
    (ENTRY + 'return target instant or sorcery card from your graveyard to your hand.',): 'return',
    (ENTRY + 'target creature gets -3/-3 until end of turn.',): 'shrink',
    ('Flash', 'Flying', "When this creature enters, you may put two cards your opponents own from exile into their owners' graveyards. If you do, counter target spell."): 'counter',
    ('{1}{G}' + ACTIVATION + 'Create a 1/1 colorless Eldrazi Scion creature token. It has "Sacrifice this token: Add {C}."',): 'scion',
}
STATS = {'tap': ('3', '3'), 'discard': ('3', '3'), 'bounce': ('3', '2'),
         'loot': ('3', '5'), 'assault': (None, None), 'return': ('2', '5'),
         'shrink': ('3', '2'), 'counter': ('2', '3'), 'scion': ('2', '3')}
BASE = ('paid', 'own_exile', 'empty', 'mixed', 'replay')
EXTRAS = {
    'tap': ('short', 'own_target'),
    'discard': ('decline', 'empty_hand'),
    'bounce': ('decline', 'self'),
    'loot': ('short', 'discard_old'),
    'assault': ('short', 'lethal', 'red_protection', 'countered'),
    'return': ('decline', 'wrong_grave', 'sorcery'),
    'shrink': ('decline', 'lethal', 'expiry'),
    'counter': ('one_card', 'decline', 'no_stack_flying'),
    'scion': ('short', 'sacrifice'),
}
RESPONSE = 'Think Twice'
WITNESSES = {'Forest', 'Island', 'Mountain', 'Plains', 'Swamp', 'Grizzly Bears', 'Silver Knight',
             'Scaled Wurm', 'Giant Spider', 'Defiant Strike', 'Divination', 'Counterspell', RESPONSE, SCION}
STATIC = {'Grizzly Bears', 'Scaled Wurm', 'Giant Spider'}
WATCHED_ZONES = ('battlefield', 'hand', 'graveyard', 'library', 'exile')
ACTIVATED = {'tap', 'loot', 'scion'}


def profiles():
    return json.loads(PROFILE.read_text(encoding='utf8'))


def parameters(record):
    from recipes import cost_from_oracle

    lines = tuple(devoid_rules(record))
    model = MODELS.get(lines)
    keywords = {'Devoid', 'Flying', 'Flash'} if model == 'counter' else {'Devoid'}
    expected_type = 'Sorcery' if model == 'assault' else 'Creature — Eldrazi Processor'
    if (model is None or set(record['keywords']) != keywords or record['type_line'] != expected_type
            or STATS[model] != (record.get('power'), record.get('toughness'))):
        raise ValueError('Complete Devoid processor Oracle and characteristics required')
    fee = list(cost_from_oracle({'mana_cost': lines[0].split(', ', 1)[0]})) if model in ACTIVATED else None
    return dict(model=model, cost=cost_from_oracle(record), fee=fee,
                exile_cost=2 if model == 'counter' else 1)


def variants(shape):
    model = shape['model']
    return [f'{mode}_{owner}' for owner in (0, 1) for mode in BASE + EXTRAS[model]
            if (model, mode, owner) != ('loot', 'discard_old', 1)]


def spec(declaration, record):
    params = parameters(record)
    pin = profiles()['sources'].get(record['name'], {})
    selected = declaration.get('variants')
    if (declaration.get('family') != FAMILY or type(declaration.get('version')) is not int
            or declaration['version'] != 2 or not exact(record, pin.get('oracle'))
            or declaration.get('oracle_text') != record['oracle_text']
            or not exact(params, declaration.get('params'))):
        raise ValueError('Exact whole pinned Oracle source required')
    if (not isinstance(selected, list) or not selected or any(type(v) is not str for v in selected)
            or len(set(selected)) != len(selected) or set(selected) - set(variants(params))):
        raise ValueError('Explicit distinct reviewed processor variants required')
    return dict(params, family=FAMILY)


def validate_witnesses(oracle, rows):
    pins = profiles()
    if set(pins) != {'sources', 'witnesses'} or set(pins['witnesses']) != WITNESSES:
        raise ValueError('Complete stable Devoid processor witness pins required')
    for name, pin in (pins['sources'] | pins['witnesses']).items():
        row = rows[name]
        if (not exact(pin['oracle'], oracle[name]) or not exact(pin['registry'], row)
                or row['kind'] != 'card' or row['handler'] in ('0', 'NULL', None)
                or (row['handler'] == 'card_dummy' and name not in STATIC)):
            raise ValueError('Devoid source or witness changed: ' + name)
    if (rows[SCION]['csvid'] != 18724 or oracle[SCION]['layout'] != 'token'
            or oracle[SCION]['oracle_text'] != 'Sacrifice this creature: Add {C}.'
            or oracle[SCION].get('colors') != []
            or (oracle[SCION].get('power'), oracle[SCION].get('toughness')) != ('1', '1')):
        raise ValueError('Canonical Scion identity required')


class ProcessorBoard(DevoidBoard):
    def build(self):
        self.model = self.shape['model']
        self.replay = None
        self.libraries = [[], []]
        self.exile_refs = []
        self.life = [20, 20]
        self.debuff = None
        self.token_live = False
        self.token_created = False
        self.victim = 'victim'
        self.add('Plains', 'old_grave', self.foe, 'graveyard')
        if self.mode != 'empty':
            self.add('Swamp', 'own_exile', self.owner, 'exile')
        available = 0 if self.mode in ('empty', 'own_exile') else self.shape['exile_cost']
        if self.mode == 'mixed':
            available += 1
        if self.model == 'counter' and self.mode == 'one_card':
            available = 1
        for i, name in enumerate(('Forest', 'Island', 'Mountain')[:available]):
            ref = f'foe_exile_{i}'
            self.add(name, ref, self.foe, 'exile')
            self.exile_refs.append(ref)
        libraries = [('Plains', 'Island', 'Mountain', 'Forest'), ('Plains', 'Island', 'Mountain', 'Forest')]
        libraries[self.owner] = ('Forest', 'Island', 'Mountain', 'Plains')
        for who, library in enumerate(libraries):
            for i, name in enumerate(library):
                ref = f'library_{who}_{i}'
                self.add(name, ref, who, 'library')
                self.libraries[who].append(ref)
        if self.model in ('tap', 'bounce', 'shrink', 'assault'):
            if self.mode == 'self':
                self.victim = 'source'
            else:
                name = 'Grizzly Bears'
                if self.model in ('assault', 'shrink') and self.mode != 'lethal':
                    name = 'Scaled Wurm'
                if self.mode == 'red_protection':
                    name = 'Silver Knight'
                self.add(name, 'victim', self.owner if self.mode == 'own_target' else self.foe)
        if self.model == 'discard' and self.mode != 'empty_hand':
            self.add('Mountain', 'discarded', self.foe, 'hand')
        if self.model == 'loot' and self.mode == 'discard_old':
            self.add('Island', 'discarded', self.owner, 'hand')
        if self.model == 'return':
            self.add('Grizzly Bears', 'bad_grave', self.owner, 'graveyard')
            if self.mode != 'wrong_grave':
                self.add('Divination' if self.mode == 'sorcery' else 'Defiant Strike',
                         'recover', self.owner, 'graveyard')
        if self.model == 'assault' and self.mode == 'countered':
            self.add('Counterspell', 'counter', self.foe, 'hand')
        if self.model == 'counter' and self.mode != 'no_stack_flying':
            self.add(RESPONSE, 'incoming', self.foe, 'hand')
        if self.model == 'counter' and self.mode == 'no_stack_flying':
            self.add('Grizzly Bears', 'ground_blocker', self.foe)

    @property
    def intended(self):
        if self.mode in ('empty', 'own_exile', 'short', 'wrong_grave', 'one_card', 'decline'):
            return []
        needed = self.shape['exile_cost']
        if self.mode == 'mixed':
            return list(reversed(self.exile_refs[-needed:]))
        return self.exile_refs[:needed]

    def inspect(self, ref, prop, value):
        self.emit('assert_card', ref=ref, property=prop, value=value)

    def transfer(self, ref, zone):
        self.cards[ref]['zone'] = zone

    def choose_named(self, ref, who=None):
        self.emit('choose_zone_card', player=self.owner if who is None else who,
                  value=self.rows[self.cards[ref]['name']]['csvid'])

    def exile_input(self, *, declined=False):
        if declined:
            self.emit('choose_zone', player=self.owner, value=-1)
        else:
            for ref in self.intended:
                self.choose_named(ref)

    def move_cost(self):
        for ref in self.intended:
            self.transfer(ref, 'graveyard')

    def draw(self, who):
        ref = self.libraries[who].pop(0)
        self.transfer(ref, 'hand')
        return ref

    def receipts(self, *, stack=0):
        self.emit('refresh')
        self.emit('assert_choices_consumed')
        self.emit('assert_stack', value=stack)
        self.observe_mana()
        watch = {item['name'] for item in self.cards.values()}
        quantities = Counter((item['player'], item['zone'], item['name']) for item in self.cards.values())
        if self.token_created:
            watch.add(SCION)
            quantities[self.owner, 'battlefield', SCION] = int(self.token_live)
        for who in (0, 1):
            self.emit('assert_life', player=who, value=self.life[who])
            for zone in WATCHED_ZONES:
                if stack and zone == 'hand':
                    continue
                self.emit('assert_zone', player=who, zone=zone,
                          value=sum(n for (seat, area, _), n in quantities.items() if (seat, area) == (who, zone)))
                for name in sorted(watch):
                    self.emit('assert_zone_multiplicity', player=who, zone=zone,
                              index=self.rows[name]['csvid'], value=quantities[who, zone, name])
            for index, ref in enumerate(self.libraries[who]):
                self.emit('assert_zone_card', player=who, zone='library', index=index,
                          value=self.rows[self.cards[ref]['name']]['csvid'])
        source = self.cards['source']
        if source['zone'] == 'battlefield':
            self.emit('assert_matching_permanents', player=self.owner, index=self.rows[source['name']]['csvid'],
                      property='color', value=0)
        target = self.cards.get(self.victim)
        if target and target['zone'] == 'battlefield':
            self.inspect(self.victim, 'tapped', int(target['tapped']))
            self.inspect(self.victim, 'damage', target['damage'])
            if self.model == 'shrink':
                power, toughness = (int(self.oracle[target['name']][key]) for key in ('power', 'toughness'))
                modifier = 3 if self.debuff else 0
                self.inspect(self.victim, 'power', power - modifier)
                self.inspect(self.victim, 'toughness', toughness - modifier)

    def source_turn(self):
        self.emit('turn', player=self.owner)
        self.fund(self.foe, [0]*6)

    def entry_inputs(self):
        if self.owner == 0:
            self.emit('choose_trigger', player=self.owner, ref='source')
        if self.model in ('bounce', 'shrink'):
            self.emit('choose_target', player=self.owner, ref=self.victim)
        self.exile_input(declined=self.mode == 'decline')
        if not self.intended:
            return
        if self.model == 'return':
            self.choose_named('recover')
        elif self.model == 'discard' and self.foe == 0 and 'discarded' in self.cards:
            self.choose_named('discarded', self.foe)

    def apply_entry(self):
        if not self.intended:
            return
        self.move_cost()
        if self.model == 'return':
            self.transfer('recover', 'hand')
        elif self.model == 'discard' and 'discarded' in self.cards:
            self.transfer('discarded', 'graveyard')
        elif self.model == 'bounce':
            self.transfer(self.victim, 'hand')
        elif self.model == 'shrink':
            if int(self.oracle[self.cards[self.victim]['name']]['toughness']) <= 3:
                self.transfer(self.victim, 'graveyard')
            else:
                self.debuff = self.victim

    def run_entry(self):
        self.source_turn()
        self.cast('source')
        if self.mode == 'replay':
            self.replay = len(self.steps)
        self.entry_inputs()
        self.emit('resolve')
        self.transfer('source', 'battlefield')
        self.apply_entry()
        self.receipts()
        if self.mode == 'expiry':
            self.emit('advance_turn')
            self.emit('untap_step', player=self.foe)
            self.debuff = None
            self.receipts()

    def run_activation(self):
        self.play_source()
        pool = list(self.shape['fee'])
        if self.mode == 'short':
            pool[0] -= 1
        self.fund(self.owner,pool)
        if self.mode == 'replay':
            self.replay = len(self.steps)
        allowed = bool(self.intended)
        self.emit('can_activate', ref='source', value=int(allowed))
        if allowed:
            if self.model == 'tap':
                self.emit('choose_target', player=self.owner, ref=self.victim)
            if self.mode == 'mixed':
                self.emit('observe_search_filters', value=1)
            self.exile_input()
        self.emit('activate', ref='source', value=int(allowed))
        if not allowed:
            self.receipts()
            return
        self.pools[self.owner] = [0]*6
        self.move_cost()
        if self.mode == 'mixed':
            self.emit('assert_offered_count', value=len(self.exile_refs))
            for ref in self.exile_refs:
                card = self.cards[ref]['name']
                self.emit('assert_offered_legal_card', index=self.rows[card]['csvid'], value=1)
        self.receipts(stack=1)
        if self.model == 'loot':
            drawn = self.draw(self.owner)
            victim = 'discarded' if self.mode == 'discard_old' else drawn
            if self.owner == 0:
                self.choose_named(victim)
            self.transfer(victim, 'graveyard')
        elif self.model == 'tap':
            self.cards[self.victim]['tapped'] = True
        else:
            self.token_created = self.token_live = True
        self.emit('resolve')
        self.receipts()
        if self.token_live:
            self.scion_receipts()

    def scion_receipts(self):
        csvid = self.rows[SCION]['csvid']
        self.emit('bind_permanent', ref='scion', player=self.owner, value=csvid)
        for prop, value in (('token', 1), ('subtype_Scion', 1), ('color', 0)):
            self.emit('assert_matching_permanents', player=self.owner, index=csvid, property=prop, value=value)
        self.inspect('scion', 'power', 1)
        self.inspect('scion', 'toughness', 1)
        if self.mode == 'sacrifice':
            self.emit('can_activate', ref='scion', value=1)
            self.emit('activate', ref='scion', value=1)
            self.pools[self.owner][0] = 1
            self.token_live = False
            self.receipts()

    def run_assault(self):
        self.source_turn()
        fee = list(self.shape['cost'])
        pool = list(fee)
        if self.mode == 'short':
            pool[0] -= 1
        self.fund(self.owner, pool)
        if self.mode == 'replay':
            self.replay = len(self.steps)
        allowed = bool(self.intended)
        if allowed:
            self.emit('choose_target', player=self.owner, ref=self.victim)
            self.exile_input()
        self.emit('cast', ref='source', value=int(allowed))
        if not allowed:
            self.receipts()
            return
        self.transfer('source','stack')
        self.pools[self.owner] = [0]*6
        self.move_cost()
        self.inspect('source', 'in_hand', 0)
        self.receipts(stack=1)
        self.inspect('source', 'color', 0)
        if self.mode == 'countered':
            self.emit('announce_spell', ref='source')
            self.cast('counter', response=True)
            self.emit('resolve_response')
            self.transfer('counter', 'graveyard')
        else:
            self.emit('resolve')
            target = self.cards[self.victim]
            target['damage'] = 5
            if int(self.oracle[target['name']]['toughness']) <= 5:
                self.transfer(self.victim, 'graveyard')
        self.transfer('source', 'graveyard')
        self.receipts()

    def run_nullifier(self):
        self.emit('turn', player=self.foe)
        if self.mode == 'no_stack_flying':
            self.fund(self.foe, [0]*6)
            self.cast('source')
            self.emit('resolve')
            self.transfer('source', 'battlefield')
            self.receipts()
            self.emit('advance_turn')
            self.emit('untap_step', player=self.owner)
            self.emit('turn', player=self.owner)
            self.emit('begin_attackers')
            self.emit('attack', ref='source', value=1)
            self.emit('finish_attackers')
            self.emit('begin_blockers')
            self.emit('block', ref='ground_blocker', other='source', value=0)
            self.emit('advance_combat')
            self.emit('combat_damage')
            self.emit('resolve')
            self.emit('end_combat')
            self.life[self.foe] -= 2
            self.receipts()
            return
        self.fund(self.owner, [0]*6)
        self.cast('incoming')
        self.emit('announce_spell', ref='incoming')
        if self.mode == 'replay':
            self.replay = len(self.steps)
        self.cast('source', response=True)
        self.exile_input(declined=self.mode == 'decline')
        self.emit('resolve_response')
        self.transfer('source', 'battlefield')
        self.transfer('incoming','graveyard')
        if self.intended:
            self.move_cost()
        else:
            self.draw(self.foe)
        self.receipts()

    def run(self):
        self.emit('present_ai_dialogs', value=1)
        if self.model in ACTIVATED:
            self.run_activation()
        elif self.model == 'assault':
            self.run_assault()
        elif self.model == 'counter':
            self.run_nullifier()
        else:
            self.run_entry()


def expand(name, csvid, declaration, params, selected, rows, oracle):
    from recipes import step

    validate_witnesses(oracle, rows)
    if (rows[name]['csvid'] != csvid or not selected or len(set(selected)) != len(selected)
            or set(selected) - set(variants(params))):
        raise ValueError('Only explicitly reviewed registered full processor variants may expand')
    cases = []
    for variant in selected:
        mode, owner = variant.rsplit('_',1)
        game = ProcessorBoard(name, params, mode, int(owner), rows, oracle)
        game.run()
        steps = game.steps
        if game.replay is not None:
            prefix, suffix = steps[:game.replay], steps[game.replay:]
            steps = prefix + [step('capture')] + suffix + [
                step('record_state'), step('restore'), *copy.deepcopy(suffix), step('assert_recorded_state')]
        cases.append(dict(name=f'DevoidOpponentExileCostRecipe_{csvid}_{variant}', card=name, fixture='empty_main',
                          seed=123, setup=game.setup, steps=steps,
                          _recipe=dict(family=FAMILY, version=2, variant=variant, params=declaration['params'],
                                       oracle_text=declaration['oracle_text'], untested_components=[
                                           'Multiplayer owners, face-down exile, and replacement effects on the graveyard destination.'])))
    return cases
