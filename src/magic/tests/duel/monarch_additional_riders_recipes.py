"""Remaining duel-observable monarch rewards, combat, and land abilities."""

import json
from pathlib import Path

from cast_untap_trigger_recipes import exact
from monarch_entry_riders_recipes import DECK, ENTRY, MonarchBoard


FAMILY = 'monarch_additional_riders'
PROFILE = Path(__file__).parent / 'data/monarch_additional_riders_wave8_profiles.json'
MODELS = {
    (ENTRY + "\nWhenever an opponent becomes the monarch, if you were the monarch as the turn began, "
     "that player loses 2 life and you gain 2 life."):
        ('knights', 'Creature — Human Knight', '{3}{W}{B}', (), (4, 4)),
    ("Trample\n" + ENTRY + "\nWhenever you tap a land for mana while you're the monarch, add an additional one mana of any color."):
        ('regal', 'Creature — Dinosaur', '{4}{G}{G}', ('Trample',), (5, 5)),
    ("At the beginning of your end step, if you're the monarch, each opponent loses 1 life and you gain 1 life."):
        ('siphon', 'Creature — Human Advisor', '{2}{B}', (), (1, 4)),
    ("When this enchantment enters, you become the monarch.\n"
     "Whenever a creature attacks you or a planeswalker you control, that creature's controller loses 1 life."):
        ('decree', 'Enchantment', '{3}{B}', (), None),
    ("Deathtouch, haste\nWhen Queen Marchesa enters, you become the monarch.\n"
     "At the beginning of your upkeep, if an opponent is the monarch, create a 1/1 black Assassin creature token with deathtouch and haste."):
        ('queen', 'Legendary Creature — Human Assassin', '{1}{R}{W}{B}', ('Haste', 'Deathtouch'), (3, 3)),
    ("At the beginning of your end step, if you're the monarch, put a +1/+1 counter on this creature."):
        ('warden', 'Creature — Human Soldier', '{1}{W}', (), (2, 2)),
    '{T}: Add {C}.\n{4}, {T}, Sacrifice this land: You become the monarch.':
        ('throne', 'Land', '', (), None),
}
COMMON = ('entry', 'underpaid', 'countered', 'replay')
OPTION = {
    'knights': ('etb_loss', 'combat_loss', 'midturn', 'earlier_opponent', 'gone'),
    'regal': ('white', 'blue', 'black', 'red', 'green', 'lost', 'opponent_land', 'nonland', 'nonmana', 'trample'),
    'siphon': ('crowned', 'not_your_turn', 'lost', 'regain'),
    'decree': ('attack_twice', 'planeswalker', 'fogged', 'own_attack', 'no_crown'),
    'queen': ('haste', 'death_touch', 'own_upkeep', 'enemy_upkeep', 'assassin_haste', 'assassin_touch'),
    'warden': ('crowned', 'not_your_turn', 'lost', 'regain'),
    'throne': ('mana', 'paid', 'short', 'tapped', 'retake', 'replay'),
}
BONUS_COLOR = {'black': 1, 'blue': 2, 'green': 3, 'red': 4, 'white': 5}
END_REWARDS = {'siphon', 'warden'}
ENEMY_TOKEN = {'enemy_upkeep', 'assassin_haste', 'assassin_touch'}
WITNESSES = set(DECK) | {
    'Assassin', 'Bonesplitter', 'Counterspell', 'Fog', 'Giant Spider', 'Grizzly Bears', 'Hill Giant',
    'Icy Manipulator', 'Jace Beleren', 'Murder', 'Palace Sentinels', 'Sol Ring', 'Vedalken Orrery', 'Watchwolf',
}
HANDLED = {'Assassin', 'Bonesplitter', 'Counterspell', 'Fog', 'Icy Manipulator', 'Jace Beleren', 'Murder',
           'Palace Sentinels', 'Sol Ring', 'Vedalken Orrery'}


def profile():
    return json.loads(PROFILE.read_text(encoding='utf8'))


def classify(record):
    from recipes import cost_from_oracle

    text = record['oracle_text']
    if record.get('layout') != 'normal' or record.get('card_faces') or record.get('selected_face') or text not in MODELS:
        raise ValueError('supported normal whole-Oracle monarch rider required')
    model, card_type, cost, keywords, stats = MODELS[text]
    fields = record['type_line'], record['mana_cost'], tuple(record['keywords']), (record.get('power'), record.get('toughness'))
    if fields != (card_type, cost, keywords, tuple(str(n) for n in stats) if stats else (None, None)):
        raise ValueError('exact monarch source cost, type, passives and stats required')
    return dict(model=model, cost=cost_from_oracle(record) if cost else None)


def variants(params):
    mode = params['model']
    names = OPTION[mode] if mode == 'throne' else COMMON + OPTION[mode]
    return [f'{name}_{owner}' for owner in (0, 1) for name in names]


def spec(declaration, record):
    params = classify(record)
    chosen = declaration.get('variants')
    if (declaration.get('family') != FAMILY or type(declaration.get('version')) is not int or declaration['version'] != 2
            or declaration.get('oracle_text') != record['oracle_text'] or not exact(declaration.get('params'), params)
            or not exact(profile()['sources'].get(record['name']), record)):
        raise ValueError('exact whole Oracle and typed monarch rider required')
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
            raise ValueError(f'Oracle or implementation drift for {name}')
        if name in pins['sources'].keys() | HANDLED and row['handler'] in ('card_dummy', '0', 'NULL', None):
            raise ValueError(f'expected live implementation for {name}')
    if rows['Assassin']['csvid'] != 18691 or oracle['Assassin']['layout'] != 'token':
        raise ValueError('expected canonical Assassin identity')


class AdditionalBoard(MonarchBoard):
    def __init__(self, *args):
        super().__init__(*args)
        self.library_ids = set(DECK) | {'Assassin', self.name}
        self.turn_start_crown = None
        self.warden_counters = 0

    def setup_witnesses(self):
        mode = self.mode
        if mode == 'countered':
            self.add('Counterspell', 'counter', self.enemy, 'hand')
        if self.model == 'knights' and mode in ('etb_loss', 'midturn', 'earlier_opponent', 'gone'):
            self.add('Palace Sentinels', 'enemy_claim', self.enemy, 'hand')
            if mode in ('midturn', 'earlier_opponent'):
                self.add('Vedalken Orrery', 'orrery', self.enemy)
            if mode == 'earlier_opponent':
                self.add('Palace Sentinels', 'first_claim', self.enemy, 'hand')
            if mode == 'gone':
                self.add('Murder', 'remove', self.enemy, 'hand')
        if self.model in END_REWARDS and mode in OPTION[self.model]:
            self.add('Palace Sentinels', 'claim', self.owner, 'hand')
        if mode in ('combat_loss', 'lost', 'regain', 'opponent_land', *ENEMY_TOKEN, 'attack_twice', 'planeswalker', 'fogged', 'no_crown'):
            self.add('Grizzly Bears', 'enemy_attacker', self.enemy)
        if self.model == 'decree':
            if mode == 'attack_twice':
                self.add('Hill Giant', 'second_attacker', self.enemy)
            if mode == 'planeswalker':
                self.add('Jace Beleren', 'walker', self.owner, 'hand')
            if mode == 'fogged':
                self.add('Fog', 'fog', self.owner, 'hand')
            if mode == 'own_attack':
                self.add('Grizzly Bears', 'ally', self.owner)
        if self.model == 'queen' and mode in ('death_touch', 'assassin_touch'):
            self.add('Giant Spider', 'blocker', self.enemy)
        if self.model == 'regal':
            if mode in BONUS_COLOR or mode in ('lost', 'nonmana'):
                self.add('Forest', 'producer', self.owner)
            if mode == 'opponent_land':
                self.add('Forest', 'producer', self.enemy)
            if mode == 'nonland':
                self.add('Sol Ring', 'producer', self.owner)
            if mode == 'nonmana':
                self.add('Icy Manipulator', 'icy', self.owner)
            if mode == 'white':
                self.add('Watchwolf', 'spend', self.owner, 'hand')
            if mode == 'trample':
                self.add('Grizzly Bears', 'blocker', self.enemy)
        if self.model == 'throne':
            if mode in ('mana', 'tapped', 'short'):
                self.add('Vedalken Orrery' if mode == 'short' else 'Bonesplitter', 'spend', self.owner, 'hand')
            if mode == 'retake':
                self.add('Palace Sentinels', 'enemy_claim', self.enemy, 'hand')

    def endpoint(self, *, unused=0):
        if self.model == 'warden' and self.entities['source']['zone'] == 'battlefield':
            self.observe('source', 'plus1_counters', self.warden_counters)
        super().endpoint(unused=unused)

    def draw_end(self):
        super().draw_end()
        if self.active != self.owner or self.crown != self.owner or self.entities['source']['zone'] != 'battlefield':
            return
        if self.model == 'siphon':
            self.life[self.owner] += 1
            self.life[self.enemy] -= 1
        if self.model == 'warden':
            self.warden_counters += 1
            stats = self.entities['source']['stats']
            self.entities['source']['stats'] = tuple(value+1 for value in stats)

    def change_crown(self, who):
        if (self.model == 'knights' and self.turn_start_crown == self.owner and self.crown != who
                and who == self.enemy and self.entities['source']['zone'] == 'battlefield'):
            self.life[self.owner] += 2
            self.life[self.enemy] -= 2
        self.crown = who

    def visit(self, *, upkeep=False):
        self.emit('advance_turn')
        self.draw_end()
        self.active = 1 - self.active
        self.turn_start_crown = self.crown
        self.emit('untap_step', player=self.active)
        for entity in self.entities.values():
            if entity['zone'] == 'battlefield':
                entity['damage'] = 0
                if entity['player'] == self.active:
                    entity['tapped'] = False
        if upkeep:
            self.emit('upkeep_step', player=self.active)
            if self.model == 'queen' and self.active == self.owner and self.crown == self.enemy:
                self.zones['battlefield'][self.owner].append('Assassin')
                self.emit('assert_zone_multiplicity', player=self.owner, zone='battlefield', index=18691, value=1)
                for prop, n in [('token', 1), ('color', 2), ('power', 1), ('toughness', 1)]:
                    self.emit('assert_matching_permanents', player=self.owner, index=18691, property=prop, value=n)
        self.endpoint()
        self.emit('turn', player=self.active, value=0)

    def monarch_spell(self, ref, who):
        self.spell(ref, permanent=True)
        self.change_crown(who)
        self.endpoint()

    def cast_reward(self):
        if self.model not in END_REWARDS:
            return self.cast_source()
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
        self.emit('resolve')
        self.move('source', 'battlefield')
        self.endpoint()
        return True

    def combat(self, attackers, *, block=None, touch=False, trample=False, fog=False, walker=False):
        defender = 1-self.active
        self.emit('begin_attackers')
        for ref in attackers:
            if walker:
                self.emit('choose_dialog', player=self.active, value=1)
            self.emit('attack_with_tax' if walker else 'attack', ref=ref, value=1)
            self.entities[ref]['tapped'] = True
        self.emit('finish_attackers')
        if self.model == 'decree' and defender == self.owner:
            self.life[self.active] -= len(attackers)
        self.emit('assert_life', player=self.active, value=self.life[self.active])
        self.emit('assert_life', player=defender, value=self.life[defender])
        self.emit('begin_blockers')
        if block:
            self.emit('block', ref=block, other=attackers[0], value=1)
        self.emit('advance_combat')
        self.emit('combat_damage')
        self.emit('resolve')
        self.emit('end_combat')
        total = 0
        if not fog:
            total = sum(self.entities[ref]['stats'][0] for ref in attackers)
            if block:
                ref = attackers[0]
                unit, blocker = self.entities[ref], self.entities[block]
                power, toughness = blocker['stats']
                if total >= toughness or touch:
                    self.move(block, 'graveyard')
                else:
                    blocker['damage'] = total
                if power >= unit['stats'][1]:
                    if ref == 'assassin':
                        self.zones['battlefield'][self.owner].remove('Assassin')
                        del self.entities[ref]
                    else:
                        self.move(ref, 'graveyard')
                else:
                    unit['damage'] = power
                total = max(0,total-toughness) if trample else 0
        if walker:
            self.loyalty -= total
        elif total:
            self.life[defender] -= total
            if self.crown == defender:
                self.change_crown(self.active)
        self.endpoint()

    def knights(self):
        if self.mode in ('etb_loss', 'combat_loss', 'gone'):
            self.visit()
        if self.mode == 'gone':
            self.spell('remove', target='source')
            self.move('source', 'graveyard')
            self.endpoint()
        if 'enemy_claim' in self.entities:
            self.monarch_spell('enemy_claim', self.enemy)
        elif self.mode == 'combat_loss':
            self.combat(['enemy_attacker'])

    def regal(self):
        mode = self.mode
        if mode == 'trample':
            self.visit()
            self.visit()
            self.combat(['source'], block='blocker', trample=True)
            self.end()
            return
        if mode in ('lost', 'opponent_land'):
            self.visit()
            self.combat(['enemy_attacker'])
            if mode == 'lost':
                self.visit()
        if 'producer' not in self.entities:
            self.end()
            return
        if mode == 'nonmana':
            self.fund(self.owner, [1,0,0,0,0,0])
            self.emit('choose_target', player=self.owner, ref='producer')
            self.emit('activate', ref='icy', value=1)
            self.mana[self.owner][0] = 0
            self.entities['icy']['tapped'] = True
            # Explicit receipt retires the optional legacy fallback; if
            # incorrectly requested, pool and unused-plan counts both differ.
            self.emit('choose_color', player=self.owner, value=5)
            self.emit('resolve')
            self.entities['producer']['tapped'] = True
            self.endpoint(unused=1)
            return
        expected_color = BONUS_COLOR.get(mode)
        if expected_color is not None or mode == 'opponent_land':
            self.emit('choose_color', player=self.owner, value=expected_color or 5)
        self.emit('activate', ref='producer', value=1)
        self.entities['producer']['tapped'] = True
        if mode == 'nonland':
            self.mana[self.owner][0] = 2
        else:
            self.mana[self.entities['producer']['player']][3] += 1
        if expected_color is not None:
            self.mana[self.owner][expected_color] += 1
        self.endpoint(unused=int(mode == 'opponent_land'))
        if mode == 'white':
            self.spend_mana('spend')
            self.end()

    def spend_mana(self, ref):
        from recipes import cost_from_oracle

        fee = cost_from_oracle(self.oracle[self.entities[ref]['name']])
        if self.mana[self.owner] != fee:
            raise ValueError('the next spell must spend exactly the demonstrated pool')
        self.emit('cast', ref=ref, value=1)
        self.observe(ref, 'in_hand', 0)
        self.emit('assert_stack', value=1)
        self.mana[self.owner] = [0]*6
        self.pools()
        self.emit('resolve')
        self.move(ref, 'battlefield')
        self.endpoint()

    def decree(self):
        mode = self.mode
        if mode == 'own_attack':
            self.combat(['ally'])
        elif mode in ('attack_twice', 'planeswalker', 'fogged', 'no_crown'):
            if mode == 'planeswalker':
                self.spell('walker', permanent=True)
                self.loyalty = 3
                self.endpoint()
            self.visit()
            if mode == 'fogged':
                self.spell('fog')
            attackers = ['enemy_attacker', 'second_attacker'] if mode == 'attack_twice' else ['enemy_attacker']
            self.combat(attackers, walker=mode == 'planeswalker', fog=mode == 'fogged')
            if mode == 'no_crown':
                self.visit()
                self.visit()
                self.combat(['enemy_attacker'])

    def queen(self):
        mode = self.mode
        if mode in ('haste', 'death_touch'):
            self.combat(['source'], block='blocker' if mode == 'death_touch' else None, touch=True)
        if mode == 'own_upkeep' or mode in ENEMY_TOKEN:
            self.visit(upkeep=True)
            if mode in ENEMY_TOKEN:
                self.combat(['enemy_attacker'])
            self.visit(upkeep=True)
        if mode in ('assassin_haste', 'assassin_touch'):
            self.emit('bind_permanent_copy', ref='assassin', player=self.owner, value=18691, index=0)
            self.entities['assassin'] = dict(name='Assassin', player=self.owner, zone='battlefield',
                                             stats=(1,1), tapped=False, damage=0)
            self.combat(['assassin'], block='blocker' if mode == 'assassin_touch' else None, touch=True)

    def throne(self):
        mode = self.mode
        if mode == 'retake':
            self.visit()
            self.monarch_spell('enemy_claim', self.enemy)
            self.visit()
        self.emit('play_land', ref='source', value=1)
        self.move('source', 'battlefield')
        self.endpoint()
        if mode in ('mana', 'short', 'tapped'):
            if mode == 'short':
                self.fund(self.owner, [3,0,0,0,0,0])
            self.emit('activate', ref='source', value=1)
            self.entities['source']['tapped'] = True
            self.mana[self.owner][0] += 1
            self.endpoint()
            self.spend_mana('spend')
            if mode == 'tapped':
                self.fund(self.owner, [4,0,0,0,0,0])
                self.emit('activate', ref='source', value=0)
                self.endpoint()
                return
        else:
            self.fund(self.owner, [4,0,0,0,0,0])
            self.emit('choose_dialog', player=self.owner, value=1)
            self.emit('activate', ref='source', value=1)
            self.mana[self.owner][0] = 0
            self.move('source', 'graveyard')
            self.emit('assert_stack', value=1)
            self.pools()
            self.emit('resolve')
            self.change_crown(self.owner)
            self.endpoint()
        self.end()

    def play(self):
        self.emit('turn', player=self.owner, value=0)
        self.emit('present_ai_dialogs', value=1)
        self.fund(self.owner, [0]*6)
        self.endpoint()
        if self.mode == 'replay':
            self.replay = len(self.steps)
        if self.model == 'throne':
            self.throne()
            return
        if self.mode == 'earlier_opponent':
            self.visit()
            self.monarch_spell('first_claim', self.enemy)
            self.visit()
        if 'claim' in self.entities:
            self.monarch_spell('claim', self.owner)
        if not self.cast_reward():
            self.end()
            return
        if self.model == 'knights':
            self.knights()
        if self.model == 'regal':
            self.regal()
            return
        if self.model == 'decree':
            self.decree()
        if self.model == 'queen':
            self.queen()
        if self.model in END_REWARDS and self.mode in ('not_your_turn', 'lost', 'regain'):
            self.visit()
            if self.mode in ('lost', 'regain'):
                self.combat(['enemy_attacker'])
                self.visit()
            if self.mode == 'regain':
                self.combat(['source'])
        self.end()


def expand(name, csvid, declaration, params, selected, rows, oracle):
    from recipes import step

    validate_witnesses(oracle, rows)
    if profile()['registry'][name]['csvid'] != csvid:
        raise ValueError('unreserved additional monarch source')
    cases = []
    for variant in selected:
        board = AdditionalBoard(name, csvid, declaration, params, variant, rows, oracle)
        board.play()
        actions = board.steps
        if board.replay is not None:
            cut = board.replay
            tail = actions[cut:]
            actions = actions[:cut] + [step('capture')] + tail + [step('record_state'), step('restore')] + tail + [step('assert_recorded_state')]
        gaps = ['The duel has two players and resolves entry and combat triggers atomically.']
        cases.append(dict(name=f'MonarchAdditionalRidersRecipe_{csvid}_{variant}', card=name, fixture='empty_main', seed=123,
                          setup=board.setup, steps=actions, _recipe=dict(family=FAMILY, version=2, variant=variant,
                          params=declaration['params'], oracle_text=declaration['oracle_text'], untested_components=gaps)))
    return cases
