"""Entry-triggered reanimation with printed selectors, riders and gates."""

import json
from pathlib import Path

from cast_untap_trigger_recipes import exact
from play_own_exile_effects_recipes import ExileBoard


FAMILY = 'entry_reanimate_rewards'
PROFILE = Path(__file__).parent / 'data/entry_reanimate_rewards_wave8_profiles.json'
ENTRY = 'When this creature enters, '
MODELS = {
    ENTRY + "return target creature card from your graveyard to the battlefield. You lose life equal to that card's mana value.":
        ('delver', 'Creature — Phyrexian Zombie', '{3}{B}{B}', (), (3, 2)),
    ('Raid — ' + ENTRY + 'if you attacked this turn, '
     'return target creature card with mana value 2 or less from your graveyard to the battlefield.'):
        ('raid', 'Creature — Human Warrior', '{3}{W}', ('Raid',), (3, 2)),
    ('Lifelink\n' + ENTRY + 'you may return target Vampire or Wizard creature card from your graveyard to the battlefield.'):
        ('bloodline', 'Creature — Vampire Wizard', '{4}{B}', ('Lifelink',), (3, 2)),
    ENTRY + 'each other player may return a creature card from their graveyard to the battlefield.':
        ('horror', 'Creature — Horror', '{3}{B}', (), (4, 4)),
    ("Intimidate (This creature can't be blocked except by artifact creatures and/or creatures that share a color with it.)\n"
     + ENTRY + "for each opponent, you may put up to one target creature card from that player's graveyard onto the battlefield under your control."):
        ('primordial', 'Creature — Avatar', '{5}{B}{B}', ('Intimidate',), (5, 4)),
    ('Revolt — ' + ENTRY + 'if a permanent left the battlefield under your control this turn, '
     'return target permanent card with mana value 2 or less from your graveyard to the battlefield.'):
        ('revolt', 'Creature — Human Warrior', '{1}{G}{W}', ('Revolt',), (3, 2)),
}
COMMON = ('effect', 'alternate', 'empty', 'wrong_side', 'wrong_type', 'shroud', 'underpaid', 'countered', 'replay')
OPTIONS = {
    'delver': ('zero',),
    'raid': ('zero', 'no_attack', 'opponent_attack', 'prior_turn', 'damage_only', 'fogged', 'mana_boundary'),
    'bloodline': ('changeling', 'decline', 'lifelink', 'life_trade', 'subtype_only'),
    'horror': ('decline',),
    'primordial': ('decline', 'intimidate_ground', 'intimidate_black', 'intimidate_artifact'),
    'revolt': ('artifact', 'land', 'no_departure', 'opponent_departs', 'prior_turn', 'stolen_departure', 'mana_boundary', 'blink'),
}
OPTIONS_AT_ENTRY = {'bloodline', 'primordial'}
FOREIGN_GRAVE = {'horror', 'primordial'}
FIGHT = {'lifelink': None, 'life_trade': 'Grizzly Bears', 'intimidate_ground': 'Grizzly Bears',
         'intimidate_black': 'Walking Corpse', 'intimidate_artifact': 'Ornithopter'}
DECK = ('Island', 'Mountain')
CREATURES = ('Grizzly Bears', 'Hill Giant')
ELIGIBLE = {
    'delver': CREATURES, 'raid': ('Grizzly Bears', 'Memnite'), 'bloodline': ('Sengir Vampire', 'Prodigal Sorcerer'),
    'horror': CREATURES, 'primordial': CREATURES, 'revolt': ('Grizzly Bears', 'Darksteel Relic', 'Forest'),
}
PERMANENTS = {'Artifact', 'Battle', 'Creature', 'Enchantment', 'Land', 'Planeswalker'}
WITNESSES = set(DECK + CREATURES) | {
    'Boomerang', 'Cloudshift', 'Counterspell', 'Darksteel Relic', 'Fog', 'Forest', 'Memnite', 'Nameless Inversion',
    'Ornithopter', 'Prodigal Sorcerer', 'Reach Through Mists', 'Sengir Vampire', 'Shock', 'Silent Gravestone',
    'Spellbook', 'Woodland Changeling', 'Unsummon', 'Walking Corpse',
}
HANDLED = {'Boomerang', 'Cloudshift', 'Counterspell', 'Fog', 'Prodigal Sorcerer', 'Sengir Vampire',
           'Shock', 'Silent Gravestone', 'Spellbook', 'Unsummon'}


def profile():
    return json.loads(PROFILE.read_text(encoding='utf8'))


def classify(record):
    from recipes import cost_from_oracle

    text = record['oracle_text']
    if record.get('layout') != 'normal' or record.get('card_faces') or record.get('selected_face') or text not in MODELS:
        raise ValueError('supported normal whole-Oracle entry reanimation required')
    model, card_type, payment, keywords, stats = MODELS[text]
    if (record['type_line'], record['mana_cost'], tuple(record['keywords']), (record.get('power'), record.get('toughness'))) != (
            card_type, payment, keywords, tuple(str(n) for n in stats)):
        raise ValueError('exact printed type, payment, keywords and stats required')
    return dict(model=model, cost=cost_from_oracle(record), from_opponent=model in FOREIGN_GRAVE,
                choice_for_opponent=model == 'horror', optional=model in OPTIONS_AT_ENTRY | {'horror'},
                maximum_mana_value=2 if model in ('raid', 'revolt') else None)


def variants(params):
    return [f'{mode}_{owner}' for owner in (0, 1) for mode in COMMON + OPTIONS[params['model']]]


def spec(declaration, record):
    params = classify(record)
    chosen = declaration.get('variants')
    if (declaration.get('family') != FAMILY or type(declaration.get('version')) is not int or declaration['version'] != 2
            or declaration.get('oracle_text') != record['oracle_text'] or not exact(declaration.get('params'), params)
            or not exact(profile()['sources'].get(record['name']), record)):
        raise ValueError('exact whole-Oracle and typed entry reanimation recipe required')
    if (not isinstance(chosen, list) or not chosen or any(not isinstance(v, str) for v in chosen)
            or len(set(chosen)) != len(chosen) or set(chosen) - set(variants(params))):
        raise ValueError('unique reviewed entry reanimation variants required')
    return dict(params, family=FAMILY)


def validate_witnesses(oracle, rows):
    pins = profile()
    records = pins['sources'] | pins['witnesses']
    if set(pins['witnesses']) != WITNESSES or set(pins['registry']) != set(records):
        raise ValueError('complete entry reanimation witness inventory required')
    for name, record in records.items():
        row = rows[name]
        if not exact(oracle[name], record) or not exact(row, pins['registry'][name]) or row['kind'] != 'card':
            raise ValueError(f'Oracle or registry drift for {name}')
        if name in pins['sources'].keys() | HANDLED and row['handler'] in ('card_dummy', '0', 'NULL', None):
            raise ValueError(f'expected live implementation for {name}')


class EntryBoard(ExileBoard):
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
        self.loyalty = self.replay = None
        self.attacked = set()
        self.departed = set()
        self.grave_owner = self.enemy if params['from_opponent'] else self.owner
        self.receiver = self.enemy if params['choice_for_opponent'] else self.owner
        self.add(name, 'source', self.owner, 'hand')
        for who in (0, 1):
            for i, item in enumerate(DECK):
                self.add(item, f'library_{who}_{i}', who, 'library')
        self.setup_witnesses()
        self.library_ids = set(DECK) | set().union(*self.zones['graveyard'])

    def setup_witnesses(self):
        mode = self.mode
        if mode != 'empty' and mode not in FIGHT:
            candidates = list(ELIGIBLE[self.model])
            if mode == 'zero' and self.model == 'delver':
                candidates.insert(0, 'Memnite')
            if self.model == 'bloodline':
                candidates += ['Nameless Inversion', 'Grizzly Bears']
                if mode == 'changeling':
                    candidates.insert(0, 'Woodland Changeling')
                if mode == 'subtype_only':
                    candidates = ['Nameless Inversion', 'Grizzly Bears']
            if mode == 'mana_boundary':
                candidates = ['Hill Giant']
            if mode == 'wrong_type':
                candidates = []
            if self.model in ('raid', 'revolt') and mode not in ('wrong_type', 'mana_boundary'):
                candidates.append('Hill Giant')
            candidates += ['Reach Through Mists']
            who = 1 - self.grave_owner if mode == 'wrong_side' else self.grave_owner
            for i, item in enumerate(candidates):
                self.add(item, f'grave_{i}', who, 'graveyard')
        if mode == 'shroud':
            self.add('Silent Gravestone', 'shroud', self.enemy)
        if mode == 'countered':
            self.add('Counterspell', 'counter', self.enemy, 'hand')
        if self.model == 'primordial' and mode in ('effect', 'alternate', 'replay'):
            self.add('Unsummon', 'reply', self.enemy, 'hand')
        if mode in FIGHT and FIGHT[mode]:
            self.add(FIGHT[mode], 'blocker', self.enemy)
        if self.model == 'raid' and mode not in ('underpaid', 'countered', 'no_attack', 'damage_only'):
            who = self.enemy if mode == 'opponent_attack' else self.owner
            self.add('Grizzly Bears', 'attacker', who)
        if mode == 'damage_only':
            self.add('Shock', 'shock', self.owner, 'hand')
        if mode == 'fogged':
            self.add('Fog', 'fog', self.enemy, 'hand')
        if mode in ('blink', 'opponent_attack'):
            self.add('Cloudshift', 'blink', self.owner, 'hand')
        if self.model == 'revolt' and mode not in ('underpaid', 'countered', 'no_departure', 'blink'):
            who = self.enemy if mode == 'opponent_departs' else self.owner
            stolen = mode == 'stolen_departure'
            self.add('Grizzly Bears' if stolen else 'Spellbook', 'departure', who)
            if stolen:
                self.setup[-1]['state'].append('opponent_owned')
            self.add('Unsummon' if stolen else 'Boomerang', 'bounce', self.owner, 'hand')

    def eligible(self, name):
        record = self.oracle[name]
        types, _, subtypes = record['type_line'].partition(' — ')
        types = set(types.split())
        bound = self.params['maximum_mana_value']
        if bound is not None and record['cmc'] > bound:
            return False
        if self.model == 'revolt':
            return bool(types & PERMANENTS)
        if 'Creature' not in types:
            return False
        return (self.model != 'bloodline' or bool({'Vampire', 'Wizard'} & set(subtypes.split()))
                or 'Changeling' in record['keywords'])

    def queue_choice(self):
        if self.model == 'raid' and self.owner not in self.attacked:
            return None
        if self.model == 'revolt' and self.owner not in self.departed:
            return None
        offered = self.zones['graveyard'][self.grave_owner]
        legal = [name for name in offered if self.eligible(name)]
        # The optional outer ETB is a legacy UI boundary. When the human
        # chooses a card or inspects the menu, accept that real trigger first.
        if not legal or (self.mode == 'shroud' and self.model != 'horror'):
            return None
        if self.model in OPTIONS_AT_ENTRY and self.owner == 0:
            self.emit('choose_trigger', ref='source', player=self.owner)
        if self.mode == 'zero' and self.model == 'raid':
            chosen = 'Memnite'
        elif self.mode == 'alternate':
            chosen = legal[1]
        elif self.mode in ('land', 'artifact'):
            chosen = 'Forest' if self.mode == 'land' else 'Darksteel Relic'
        else:
            chosen = legal[0]
        chooser = self.receiver if self.model == 'horror' else self.owner
        self.emit('choose_optional_zone' if self.mode == 'decline' else 'choose_zone_card',
                  player=chooser, value=self.rows[chosen]['csvid'])
        return chosen, list(offered)

    def settle_entry(self, choice):
        if choice:
            chosen, offered = choice
            self.emit('assert_offered_count', value=len(offered))
            for item in sorted(set(offered)):
                self.emit('assert_offered_legal_card', index=self.rows[item]['csvid'], value=int(self.eligible(item)))
            if self.mode != 'decline':
                self.zones['graveyard'][self.grave_owner].remove(chosen)
                self.zones['battlefield'][self.receiver].append(chosen)
                if self.model == 'delver':
                    self.life[self.owner] -= int(self.oracle[chosen]['cmc'])
        self.emit('assert_unused_choices', value=0)
        self.emit('assert_forced_zone_choices', value=0)
        self.emit('refresh')
        self.receipt()

    def turn_over(self):
        self.advance()
        self.attacked.clear()
        self.departed.clear()

    def raid_combat(self):
        self.emit('begin_attackers')
        self.emit('attack', ref='attacker', value=1)
        self.entities['attacker']['tapped'] = True
        self.attacked.add(self.active)
        self.emit('finish_attackers')
        self.emit('begin_blockers')
        self.emit('advance_combat')
        self.emit('combat_damage')
        self.emit('resolve')
        self.emit('end_combat')
        if self.mode != 'fogged':
            self.life[1 - self.active] -= 2
        self.receipt()

    def nonentry_spell(self, spell, *, target=None, player_target=None):
        self.start_cast(spell, target=target, player_target=player_target)
        self.emit('resolve')
        self.move(spell, 'graveyard')

    def gate_setup(self):
        if self.model == 'raid':
            if self.mode == 'fogged':
                self.nonentry_spell('fog')
            if 'attacker' in self.entities and self.mode != 'opponent_attack':
                self.raid_combat()
            if self.mode == 'damage_only':
                self.nonentry_spell('shock', player_target=self.enemy)
                self.life[self.enemy] -= 2
                self.receipt()
        if 'departure' in self.entities:
            self.nonentry_spell('bounce', target='departure')
            old = self.entities.pop('departure')
            who = old['player']
            self.departed.add(who)
            self.zones['battlefield'][who].remove(old['name'])
            rightful = self.enemy if self.mode == 'stolen_departure' else who
            self.zones['hand'][rightful].append(old['name'])
            self.library_ids.add(old['name'])
            self.receipt()
        if self.mode == 'prior_turn':
            self.turn_over()
            self.turn_over()

    def cast_entry(self):
        fee = list(self.params['cost'])
        if self.mode == 'underpaid':
            fee[0] -= 1
        self.start_cast('source', budget=fee, denied=self.mode == 'underpaid')
        if self.mode == 'underpaid':
            self.receipt()
            return None
        if self.mode == 'countered':
            self.emit('announce_spell', ref='source')
            self.start_cast('counter', response=True)
            self.emit('resolve_response')
            self.move('source', 'graveyard')
            self.move('counter', 'graveyard')
            self.receipt()
            return None
        # At this actual stack boundary, returning a creature or losing life
        # before entry would fail even if resolution later repaired the zones.
        for who, pile in enumerate(self.zones['graveyard']):
            self.emit('assert_zone', player=who, zone='graveyard', value=len(pile))
            self.emit('assert_life', player=who, value=self.life[who])
        choice = self.queue_choice()
        self.emit('resolve')
        self.move('source', 'battlefield')
        self.settle_entry(choice)
        return choice

    def owner_hand(self, chosen):
        self.emit('bind_permanent_copy', ref='raised', player=self.owner, value=self.rows[chosen]['csvid'], index=0)
        self.nonentry_spell('reply', target='raised')
        self.zones['battlefield'][self.owner].remove(chosen)
        self.zones['hand'][self.enemy].append(chosen)
        self.receipt()

    def blink_entry(self):
        # The same real blink first causes a departure and then enters. The
        # old reference remains sufficient for the activation target.
        self.start_cast('blink', target='source')
        self.departed.add(self.owner)
        choice = self.queue_choice()
        self.emit('resolve')
        self.move('blink', 'graveyard')
        self.emit('bind_permanent_copy', ref='source', player=self.owner, value=self.csvid, index=0)
        self.settle_entry(choice)

    def printed_combat(self):
        self.turn_over()
        self.turn_over()
        self.emit('begin_attackers')
        self.emit('attack', ref='source', value=1)
        attacker = self.entities['source']
        attacker['tapped'] = True
        self.emit('finish_attackers')
        self.emit('begin_blockers')
        blocks = self.mode not in ('lifelink', 'intimidate_ground')
        if 'blocker' in self.entities:
            self.emit('block', ref='blocker', other='source', value=int(blocks))
        self.emit('advance_combat')
        self.emit('combat_damage')
        self.emit('resolve')
        self.emit('end_combat')
        if self.model == 'bloodline':
            self.life[self.owner] += attacker['stats'][0]
        if blocks:
            blocker = self.entities['blocker']
            if blocker['stats'][0] >= attacker['stats'][1]:
                self.move('source', 'graveyard')
            else:
                attacker['damage'] = blocker['stats'][0]
            self.move('blocker', 'graveyard')
        else:
            self.life[self.enemy] -= attacker['stats'][0]
        self.receipt()

    def play(self):
        self.emit('turn', player=self.owner, value=0)
        self.emit('present_ai_dialogs', value=1)
        self.emit('observe_search_filters', value=1)
        self.fund(self.owner, [0] * 6)
        self.receipt()
        self.gate_setup()
        if self.mode == 'replay':
            self.replay = len(self.steps)
        choice = self.cast_entry()
        if self.model == 'primordial' and choice and self.mode != 'decline':
            self.owner_hand(choice[0])
        if self.mode == 'opponent_attack':
            self.turn_over()
            self.raid_combat()
        if self.mode in ('blink', 'opponent_attack'):
            self.blink_entry()
        if self.mode in FIGHT:
            self.printed_combat()


def expand(name, csvid, declaration, params, selected, rows, oracle):
    from recipes import step

    validate_witnesses(oracle, rows)
    if profile()['registry'][name]['csvid'] != csvid:
        raise ValueError('unreserved entry reanimation source')
    cases = []
    for variant in selected:
        board = EntryBoard(name, csvid, declaration, params, variant, rows, oracle)
        board.play()
        actions = board.steps
        if board.replay is not None:
            cut = board.replay
            tail = actions[cut:]
            actions = actions[:cut] + [step('capture')] + tail + [step('record_state'), step('restore')] + tail + [step('assert_recorded_state')]
        gaps = ['The entry resolver closes the trigger within one step. Priority between grave selection and the return is unobserved.']
        if params['from_opponent']:
            gaps.append('The duel has one opponent. Simultaneous choices in multiplayer are unobserved.')
        cases.append(dict(name=f'EntryReanimateRewardsRecipe_{csvid}_{variant}', card=name, fixture='empty_main',
                          seed=123, setup=board.setup, steps=actions, _recipe=dict(family=FAMILY, version=2, variant=variant,
                            params=declaration['params'], oracle_text=declaration['oracle_text'], untested_components=gaps)))
    return cases
