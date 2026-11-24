"""Monarch entry, actual end-step draws, and separate rules riders."""

import json
from pathlib import Path

from cast_untap_trigger_recipes import exact
from play_own_exile_effects_recipes import ExileBoard


FAMILY = 'monarch_entry_riders'
PROFILE = Path(__file__).parent / 'data/monarch_entry_riders_wave8_profiles.json'
ENTRY = 'When this creature enters, you become the monarch.'
MODELS = {
    ENTRY: ('entry', 'Creature — Human Soldier', '{3}{W}', (), (2, 4)),
    'Deathtouch\n' + ENTRY: ('touch', 'Creature — Human Assassin', '{3}{B}', ('Deathtouch',), (1, 3)),
    ENTRY + "\nThis creature can't attack unless defending player is the monarch.":
        ('hunter', 'Creature — Ogre Mercenary', '{4}{R}', (), (4, 4)),
    ENTRY + '\nWhenever you become the monarch, target player sacrifices a creature of their choice.':
        ('lich', 'Creature — Zombie Cleric', '{3}{B}{B}', (), (4, 2)),
    ENTRY + "\nAt the beginning of your upkeep, if you're the monarch, creatures you control can't be blocked this turn.":
        ('keeper', 'Creature — Human Rogue Mutant', '{3}{U}{U}', (), (4, 4)),
    ENTRY + '\nWhen this creature enters, exile target creature an opponent controls until an opponent becomes the monarch.':
        ('jailer', 'Creature — Human Soldier', '{2}{W}{W}', (), (2, 2)),
    ENTRY + '\nAll damage that would be dealt to you is dealt to this creature instead.':
        ('protector', 'Creature — Giant Soldier', '{5}{W}', (), (2, 5)),
    'Flying\n' + ENTRY + "\nAt the beginning of your upkeep, if you're the monarch, create a 5/5 red Dragon creature token with flying.":
        ('despot', 'Creature — Dragon', '{5}{R}{R}', ('Flying',), (5, 5)),
}
COMMON = ('entry', 'transfer', 'noncombat', 'already', 'underpaid', 'countered', 'replay')
SPECIFIC = {
    'entry': ('multiple_turns', 'source_gone', 'fogged'),
    'touch': ('death_touch',),
    'hunter': ('locked', 'steal_back'),
    'lich': ('self_target', 'empty_target', 'regain_combat'),
    'keeper': ('upkeep_ally', 'upkeep_self', 'no_upkeep', 'lost', 'removed'),
    'jailer': ('alternate', 'no_target', 'source_gone', 'cross_owned'),
    'protector': ('redirect_foe', 'source_gone', 'redirect_lethal'),
    'despot': ('tokens', 'no_upkeep', 'dragon_flying', 'source_flying', 'lost'),
}
DECK = ('Island', 'Mountain', 'Plains', 'Forest', 'Swamp')
TRANSFERS = {'transfer', 'fogged', 'death_touch', 'steal_back', 'cross_owned', 'regain_combat', 'lost', 'redirect_lethal'}
KEEPER_COMBAT = {'upkeep_ally', 'upkeep_self', 'no_upkeep', 'lost', 'removed'}
WITNESSES = set(DECK) | {'Counterspell', 'Darksteel Relic', 'Dragon', 'Fog', 'Grizzly Bears', 'Hill Giant',
                       'Murder', 'Palace Sentinels', 'Raging Goblin', 'Shock'}
HANDLED = {'Counterspell', 'Dragon', 'Fog', 'Murder', 'Palace Sentinels', 'Raging Goblin', 'Shock'}


def profile():
    return json.loads(PROFILE.read_text(encoding='utf8'))


def classify(record):
    from recipes import cost_from_oracle

    text = record['oracle_text']
    if record.get('layout') != 'normal' or record.get('card_faces') or record.get('selected_face') or text not in MODELS:
        raise ValueError('supported normal whole-Oracle monarch entry required')
    model, card_type, fee, keywords, stats = MODELS[text]
    if (record['type_line'], record['mana_cost'], tuple(record['keywords']), (record.get('power'), record.get('toughness'))) != (
            card_type, fee, keywords, tuple(str(n) for n in stats)):
        raise ValueError('exact printed monarch case payment, stats and passives required')
    return dict(model=model, cost=cost_from_oracle(record))


def variants(params):
    return [f'{mode}_{owner}' for owner in (0, 1) for mode in COMMON + SPECIFIC[params['model']]]


def spec(declaration, record):
    params = classify(record)
    chosen = declaration.get('variants')
    if (declaration.get('family') != FAMILY or type(declaration.get('version')) is not int or declaration['version'] != 2
            or declaration.get('oracle_text') != record['oracle_text'] or not exact(declaration.get('params'), params)
            or not exact(profile()['sources'].get(record['name']), record)):
        raise ValueError('exact whole Oracle and typed monarch entry required')
    if (not isinstance(chosen, list) or not chosen or any(not isinstance(v, str) for v in chosen)
            or len(set(chosen)) != len(chosen) or set(chosen) - set(variants(params))):
        raise ValueError('distinct reviewed monarch variants required')
    return dict(params, family=FAMILY)


def validate_witnesses(oracle, rows):
    pins = profile()
    records = pins['sources'] | pins['witnesses']
    if set(pins['witnesses']) != WITNESSES or set(pins['registry']) != set(records):
        raise ValueError('complete monarch witness inventory required')
    for name, record in records.items():
        row = rows[name]
        if row['kind'] != 'card' or not exact(record, oracle[name]) or not exact(row, pins['registry'][name]):
            raise ValueError(f'Oracle or production identity drift for {name}')
        if name in pins['sources'].keys() | HANDLED and row['handler'] in ('card_dummy', '0', 'NULL', None):
            raise ValueError(f'expected live implementation for {name}')
    if rows['Dragon']['csvid'] != 18719 or oracle['Dragon'].get('layout') != 'token':
        raise ValueError('canonical Dragon token identity required')


class MonarchBoard(ExileBoard):
    def __init__(self, name, csvid, declaration, params, variant, rows, oracle):
        self.mode, seat = variant.rsplit('_', 1)
        self.owner, self.enemy = int(seat), 1 - int(seat)
        self.active = self.owner
        self.name, self.csvid, self.variant = name, csvid, variant
        self.declaration, self.params, self.rows, self.oracle = declaration, params, rows, oracle
        self.model = params['model']
        self.setup, self.steps, self.entities = [], [], {}
        self.mana, self.life = [[0] * 6, [0] * 6], [20, 20]
        self.zones = {zone: [[], []] for zone in ('hand', 'battlefield', 'graveyard', 'library', 'exile')}
        self.loyalty = self.replay = self.crown = self.exiled = None
        self.add(name, 'source', self.owner, 'hand')
        for who in (0, 1):
            deck = DECK if who == self.owner else tuple(reversed(DECK))
            for i, card in enumerate(deck):
                self.add(card, f'deck_{who}_{i}', who, 'library')
        self.setup_witnesses()
        self.library_ids = set(DECK) | {'Dragon', self.name}

    def setup_witnesses(self):
        mode = self.mode
        if mode == 'already':
            self.add('Palace Sentinels', 'backup', self.owner, 'hand')
        if mode == 'countered':
            self.add('Counterspell', 'counter', self.enemy, 'hand')
        if mode in ('source_gone', 'removed'):
            self.add('Murder', 'remove', self.enemy, 'hand')
        if mode in ('noncombat', 'redirect_foe', 'redirect_lethal'):
            self.add('Shock', 'shock', self.owner if mode == 'redirect_foe' else self.enemy, 'hand')
        if mode == 'fogged':
            self.add('Fog', 'fog', self.owner, 'hand')
        if mode in TRANSFERS or (mode == 'source_gone' and self.model in ('jailer', 'protector')):
            attacker = 'Hill Giant' if mode in ('death_touch', 'redirect_lethal') else 'Grizzly Bears'
            cast_stealer = self.model == 'lich' and mode in ('transfer', 'regain_combat')
            self.add('Raging Goblin' if cast_stealer else attacker, 'stealer', self.enemy,
                     'hand' if cast_stealer else 'battlefield')
        if self.model == 'keeper' and mode in KEEPER_COMBAT:
            self.add('Grizzly Bears', 'ally', self.owner)
            self.add('Hill Giant', 'blocker', self.enemy)
        if self.model == 'despot' and mode in ('dragon_flying', 'source_flying'):
            self.add('Grizzly Bears', 'blocker', self.enemy)
        if self.model == 'jailer':
            self.add('Darksteel Relic', 'artifact', self.enemy)
            if mode != 'no_target':
                self.add('Hill Giant', 'victim', self.enemy)
                if mode == 'cross_owned':
                    self.setup[-1]['state'].append('opponent_owned')
            if mode == 'alternate':
                self.add('Grizzly Bears', 'alternate_target', self.enemy)
            if mode == 'no_target':
                self.add('Grizzly Bears', 'own_creature', self.owner)
        if self.model == 'lich':
            self.sacrifice_target = self.owner if mode == 'self_target' else self.enemy
            self.add('Darksteel Relic', 'artifact', self.sacrifice_target)
            if mode not in ('empty_target', 'regain_combat'):
                if self.sacrifice_target == 0:
                    self.add('Grizzly Bears', 'victim', self.sacrifice_target)
                    if self.sacrifice_target != self.owner and 'stealer' not in self.entities:
                        self.add('Hill Giant', 'other_creature', self.sacrifice_target)
                elif self.sacrifice_target != self.owner and 'stealer' not in self.entities:
                    self.add('Hill Giant', 'victim', self.sacrifice_target)

    def endpoint(self, *, unused=0):
        self.emit('assert_unused_choices', value=unused)
        self.emit('refresh')
        self.receipt()

    def draw_end(self):
        if self.active == self.crown:
            drawn = self.zones['library'][self.active].pop(0)
            self.zones['hand'][self.active].append(drawn)

    def visit(self, *, upkeep=False):
        self.emit('advance_turn')
        self.draw_end()
        self.active = 1 - self.active
        self.emit('untap_step', player=self.active)
        for entity in self.entities.values():
            if entity['zone'] == 'battlefield':
                entity['damage'] = 0
                if entity['player'] == self.active:
                    entity['tapped'] = False
        if upkeep:
            self.emit('upkeep_step', player=self.active)
            if self.model == 'despot' and self.active == self.owner == self.crown:
                self.zones['battlefield'][self.owner].append('Dragon')
                for prop, value in [('token', 1), ('color', 16), ('subtype_Dragon', 1), ('power', 5), ('toughness', 5)]:
                    self.emit('assert_matching_permanents', player=self.owner, index=self.rows['Dragon']['csvid'], property=prop, value=value)
        self.endpoint()
        self.emit('turn', player=self.active, value=0)

    def end(self):
        self.emit('cleanup')
        self.draw_end()
        for entity in self.entities.values():
            if entity['zone'] == 'battlefield':
                entity['damage'] = 0
        self.endpoint()

    def spell(self, ref, *, target=None, player_target=None, permanent=False):
        self.start_cast(ref, target=target, player_target=player_target)
        self.emit('resolve')
        self.move(ref, 'battlefield' if permanent else 'graveyard')

    def lich_plan(self, *, reclaims=False):
        who = self.sacrifice_target
        if reclaims:
            victim = 'stealer'
        elif who == self.owner == 1:
            victim = 'source'
        else:
            candidates = ('victim', 'stealer')
            victim = next((ref for ref in candidates if ref in self.entities
                           and self.entities[ref]['zone'] == 'battlefield' and self.entities[ref]['player'] == who), None)
        self.emit('choose_player', player=self.owner, value=who)
        if victim and who == 0:
            self.emit('choose_target', player=who, ref=victim)
        return victim, 1 + int(bool(victim) and who == 0)

    def jailer_depart(self, ref):
        target = self.entities.pop(ref)
        rightful = self.owner if self.mode == 'cross_owned' else self.enemy
        self.zones['battlefield'][self.enemy].remove(target['name'])
        self.zones['exile'][rightful].append(target['name'])
        self.library_ids.add(target['name'])
        self.exiled = (target['name'], rightful)

    def change_crown(self, who):
        self.crown = who
        if who == self.enemy and self.exiled:
            name, rightful = self.exiled
            self.zones['exile'][rightful].remove(name)
            self.zones['battlefield'][rightful].append(name)
            self.exiled = None

    def cast_source(self):
        fee = list(self.params['cost'])
        denied = self.mode == 'underpaid'
        if denied:
            fee[0] -= 1
        self.start_cast('source', budget=fee, denied=denied)
        if denied:
            self.endpoint()
            return False
        if self.mode == 'countered':
            self.emit('announce_spell', ref='source')
            self.start_cast('counter', response=True)
            self.emit('resolve_response')
            self.move('source', 'graveyard')
            self.move('counter', 'graveyard')
            self.endpoint()
            return False
        jail = None
        sacrifice, intended = None, 0
        if self.model == 'jailer' and 'victim' in self.entities:
            jail = 'alternate_target' if self.mode == 'alternate' else 'victim'
            self.emit('choose_target', player=self.owner, ref=jail)
        if self.model == 'lich':
            sacrifice, intended = self.lich_plan()
        becoming = self.crown != self.owner
        self.emit('resolve')
        self.move('source', 'battlefield')
        self.change_crown(self.owner)
        if jail:
            self.jailer_depart(jail)
        if sacrifice and becoming:
            self.move(sacrifice, 'graveyard')
        self.endpoint(unused=int(not becoming) * intended)
        return True

    def redirected(self, damage):
        source = self.entities['source']
        source['damage'] += damage
        if source['damage'] >= source['stats'][1]:
            self.move('source', 'graveyard')

    def shock(self):
        victim = self.enemy if self.mode == 'redirect_foe' else self.owner
        self.spell('shock', player_target=victim)
        if self.model == 'protector' and victim == self.owner:
            self.redirected(2)
        else:
            self.life[victim] -= 2
        self.endpoint()

    def fight(self, ref, *, legal=True, blocker=None, blocks=False):
        self.emit('begin_attackers')
        self.emit('attack', ref=ref, value=int(legal))
        if legal:
            self.entities[ref]['tapped'] = True
        self.emit('finish_attackers')
        self.emit('begin_blockers')
        if blocker:
            self.emit('block', ref=blocker, other=ref, value=int(blocks))
        sacrifice = None
        if self.mode == 'regain_combat' and self.active == self.owner:
            sacrifice, _ = self.lich_plan(reclaims=True)
        self.emit('advance_combat')
        self.emit('combat_damage')
        self.emit('resolve')
        self.emit('end_combat')
        attacker = self.entities[ref]
        if legal and self.mode != 'fogged':
            if blocks:
                defender = self.entities[blocker]
                attacker_lethal = defender['stats'][0] >= attacker['stats'][1]
                defender_lethal = attacker['stats'][0] >= defender['stats'][1]
                if self.mode == 'death_touch':
                    attacker_lethal = True
                if attacker_lethal:
                    self.move(ref, 'graveyard')
                else:
                    attacker['damage'] = defender['stats'][0]
                if defender_lethal:
                    self.move(blocker, 'graveyard')
                else:
                    defender['damage'] = attacker['stats'][0]
            elif self.model == 'protector' and self.active == self.enemy and self.entities['source']['zone'] == 'battlefield':
                self.redirected(attacker['stats'][0])
            else:
                defender = 1 - self.active
                self.life[defender] -= attacker['stats'][0]
                if self.crown == defender:
                    self.change_crown(self.active)
        if sacrifice:
            self.move(sacrifice, 'graveyard')
        self.endpoint()

    def opposing_combat(self):
        self.visit()
        if self.entities['stealer']['zone'] == 'hand':
            self.spell('stealer', permanent=True)
            self.endpoint()
        if self.mode == 'fogged':
            self.spell('fog')
        if self.mode == 'redirect_lethal':
            self.shock()
        thorn_blocks = self.mode == 'death_touch'
        self.fight('stealer', blocker='source' if thorn_blocks else None, blocks=thorn_blocks)

    def keeper_finish(self):
        self.visit(upkeep=self.mode != 'no_upkeep')
        if self.mode == 'removed':
            self.spell('remove', target='source')
            self.move('source', 'graveyard')
            self.endpoint()
        self.fight('source' if self.mode == 'upkeep_self' else 'ally', blocker='blocker', blocks=self.mode in ('no_upkeep', 'lost'))

    def play(self):
        self.emit('turn', player=self.owner, value=0)
        self.emit('present_ai_dialogs', value=1)
        self.fund(self.owner, [0] * 6)
        self.endpoint()
        if self.mode == 'replay':
            self.replay = len(self.steps)
        if self.mode == 'already':
            self.spell('backup', permanent=True)
            self.change_crown(self.owner)
            self.endpoint()
        if not self.cast_source():
            self.end()
            return
        if self.mode == 'source_gone':
            self.spell('remove', target='source')
            self.move('source', 'graveyard')
            self.endpoint()
        if self.mode in ('noncombat', 'redirect_foe'):
            self.shock()
        if self.mode in TRANSFERS or (self.mode == 'source_gone' and self.model in ('jailer', 'protector')):
            self.opposing_combat()
        if self.model == 'keeper' and self.mode in KEEPER_COMBAT:
            if self.mode != 'lost':
                self.visit()
            self.keeper_finish()
        if self.mode in ('steal_back', 'regain_combat'):
            self.visit()
            self.fight('source')
        if self.mode in ('fogged', 'death_touch'):
            self.visit()
        if self.mode in ('multiple_turns', 'locked') or (self.model == 'despot' and self.mode in ('tokens', 'no_upkeep', 'dragon_flying', 'source_flying')):
            self.visit(upkeep=True)
            self.visit(upkeep=self.mode != 'no_upkeep')
        if self.mode == 'locked':
            self.fight('source', legal=False)
        if self.mode == 'no_upkeep' and self.model == 'despot':
            self.emit('assert_zone_multiplicity', player=self.owner, zone='battlefield', index=self.rows['Dragon']['csvid'], value=0)
        if self.model == 'despot' and self.mode == 'lost':
            self.visit(upkeep=True)
        if self.mode == 'dragon_flying':
            self.visit(upkeep=True)
            self.visit(upkeep=True)
            self.emit('bind_permanent_copy', ref='dragon', value=self.rows['Dragon']['csvid'], player=self.owner, index=0)
            self.entities['dragon'] = dict(name='Dragon', player=self.owner, zone='battlefield', stats=(5, 5), tapped=False, damage=0)
            self.fight('dragon', blocker='blocker')
        if self.mode == 'source_flying':
            self.fight('source', blocker='blocker')
        self.end()


def expand(name, csvid, declaration, params, selected, rows, oracle):
    from recipes import step

    validate_witnesses(oracle, rows)
    if profile()['registry'][name]['csvid'] != csvid:
        raise ValueError('unreserved monarch entry source')
    cases = []
    for variant in selected:
        board = MonarchBoard(name, csvid, declaration, params, variant, rows, oracle)
        board.play()
        actions = board.steps
        if board.replay is not None:
            cut = board.replay
            tail = actions[cut:]
            actions = actions[:cut] + [step('capture')] + tail + [step('record_state'), step('restore')] + tail + [step('assert_recorded_state')]
        gaps = ['The duel fixture has two players and does not separate priority during the atomic combat-damage or entry resolver.']
        cases.append(dict(name=f'MonarchEntryRidersRecipe_{csvid}_{variant}', card=name, fixture='empty_main', seed=123,
                          setup=board.setup, steps=actions, _recipe=dict(family=FAMILY, version=2, variant=variant,
                          params=declaration['params'], oracle_text=declaration['oracle_text'], untested_components=gaps)))
    return cases
