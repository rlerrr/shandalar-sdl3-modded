"""Whole-card upkeep and death recursion through actual turn and spell events."""

import json
from pathlib import Path

from cast_untap_trigger_recipes import exact
from play_own_exile_effects_recipes import ExileBoard


FAMILY = 'graveyard_recursion_triggers'
PROFILE = Path(__file__).parent / 'data/graveyard_recursion_triggers_wave8_profiles.json'
FLYING = 'Flying\n'
COST_LINE = "You may pay {W}{U}{B}{R}{G} rather than pay this spell's mana cost.\n"
UPKEEP = 'At the beginning of your upkeep, '
REANIMATE = 'you may return target creature card from your graveyard to the battlefield.'
ARTIFACT = 'you may return target artifact card from your graveyard to the battlefield.'
PATTERNS = {
    FLYING + UPKEEP + 'return target Spirit card from your graveyard to your hand.':
        ('spirit', 'hand', '{4}{W}', 'Creature — Angel', ('Flying',), (4, 4)),
    COST_LINE + 'Trample\n' + UPKEEP + ARTIFACT:
        ('artifact', 'battlefield', '{7}{W}{W}', 'Creature — Bringer', ('Trample',), (5, 5)),
    'This land enters tapped.\n' + UPKEEP + 'if you control seven or more Plains, ' + REANIMATE + '\n{T}: Add {W}.':
        ('plains', 'battlefield', '', 'Land', (), None),
    'When this creature dies, return target creature card with mana value 2 or less from your graveyard to the battlefield.':
        ('death', 'battlefield', '{3}{B}', 'Creature — Vampire', (), (3, 2)),
    UPKEEP + 'if you have four or more creature cards in your graveyard, you may return target creature card from your graveyard to your hand.':
        ('cemetery', 'hand', '{1}{B}', 'Enchantment', (), None),
    FLYING + UPKEEP + REANIMATE:
        ('creature', 'battlefield', '{6}{W}{W}{W}', 'Legendary Creature — Angel', ('Flying',), (4, 6)),
}
DECK = ('Island', 'Mountain')
CREATURES = ('Grizzly Bears', 'Hill Giant', 'Walking Corpse', 'Giant Spider', 'Suntail Hawk')
SPIRITS = ('Kami of Ancient Law', 'Hana Kami')
ARTIFACTS = ('Darksteel Relic', 'Ornithopter')
WITNESSES = set(DECK + CREATURES + SPIRITS + ARTIFACTS) | {
    'Forest', 'Plains', 'Tundra', 'Reach Through Mists', 'Fervor', 'Silent Gravestone', 'Skirk Prospector', 'Scathe Zombies',
    'Murder', 'Swords to Plowshares', 'Unsummon', 'Stone Rain', 'Counterspell',
}
HANDLED = {'Murder', 'Swords to Plowshares', 'Unsummon', 'Stone Rain', 'Counterspell', 'Silent Gravestone'}
FLIGHT = ('flying_ground', 'flying_reach')
COMBAT = (*FLIGHT, 'trample', 'combat_death')
COMMON = ('effect', 'alternate', 'empty', 'wrong_type', 'opponent_grave', 'shroud', 'cross_owned', 'replay')
UPKEEP_MODES = ('opponent_upkeep', 'repeat')
CAST_MODES = ('countered', 'short_mana', 'wrong_mana')
EXTRA = {
    'spirit': FLIGHT,
    'artifact': ('decline', 'rainbow', 'choose_rainbow', 'choose_normal', 'trample'),
    'plains': ('decline', 'six', 'wrong_land', 'opponent_plains', 'subtyped_land', 'minus_plains', 'white_mana'),
    'death': ('exile_source', 'bounce_source', 'other_dies', 'zero_mv', 'combat_death'),
    'cemetery': ('decline', 'threshold_falls', 'below_threshold'),
    'creature': ('decline', *FLIGHT),
}


def profile():
    return json.loads(PROFILE.read_text(encoding='utf8'))


def classify(record):
    from recipes import cost_from_oracle

    text = record['oracle_text']
    if record.get('layout') != 'normal' or record.get('card_faces') or record.get('selected_face') or text not in PATTERNS:
        raise ValueError('supported normal whole-Oracle recursion trigger required')
    model, destination, cost, kind, keywords, stats = PATTERNS[text]
    printed_stats = (record.get('power'), record.get('toughness'))
    if (record['mana_cost'], record['type_line'], tuple(record['keywords']), printed_stats) != (
            cost, kind, keywords, tuple(str(n) for n in stats) if stats else (None, None)):
        raise ValueError('exact cost, Oracle type, printed abilities and stats required')
    return dict(model=model, destination=destination, cost=cost_from_oracle(record) if cost else None,
                power_toughness=list(stats) if stats else None)


def variants(params):
    model = params['model']
    modes = COMMON + (UPKEEP_MODES if model != 'death' else ()) + (CAST_MODES if params['cost'] is not None else ()) + EXTRA[model]
    return [f'{mode}_{owner}' for owner in (0, 1) for mode in modes]


def spec(declaration, record):
    params = classify(record)
    chosen = declaration.get('variants')
    if (declaration.get('family') != FAMILY or type(declaration.get('version')) is not int or declaration['version'] != 2
            or declaration.get('oracle_text') != record['oracle_text'] or not exact(declaration.get('params'), params)
            or not exact(profile()['sources'].get(record['name']), record)):
        raise ValueError('complete pinned Oracle and typed recursion fixture required')
    if (not isinstance(chosen, list) or not chosen or any(not isinstance(v, str) for v in chosen)
            or len(set(chosen)) != len(chosen) or set(chosen) - set(variants(params))):
        raise ValueError('distinct reviewed recursion variants required')
    return dict(params, family=FAMILY)


def validate_witnesses(oracle, rows):
    pins = profile()
    records = pins['sources'] | pins['witnesses']
    if set(pins['witnesses']) != WITNESSES or set(records) != set(pins['registry']):
        raise ValueError('complete recursion source and witness inventory required')
    for name, expected in records.items():
        row = rows[name]
        if not exact(oracle[name], expected) or not exact(row, pins['registry'][name]) or row['kind'] != 'card':
            raise ValueError(f'Oracle or registry drift for {name}')
        if name in pins['sources'].keys() | HANDLED and row['handler'] in ('card_dummy', '0', 'NULL', None):
            raise ValueError(f'expected a live production handler for {name}')


class Recursion(ExileBoard):
    def __init__(self, name, csvid, declaration, params, variant, rows, oracle):
        self.mode, seat = variant.rsplit('_', 1)
        self.owner, self.enemy = int(seat), 1 - int(seat)
        self.active = self.owner
        self.name, self.csvid, self.variant = name, csvid, variant
        self.declaration, self.params, self.rows, self.oracle = declaration, params, rows, oracle
        self.model = params['model']
        self.original_owner = self.enemy if self.mode == 'cross_owned' else self.owner
        self.loyalty = self.replay = None
        self.mana, self.life = [[0] * 6, [0] * 6], [20, 20]
        self.setup, self.steps, self.entities = [], [], {}
        self.zones = {zone: [[], []] for zone in ('hand', 'battlefield', 'graveyard', 'library', 'exile')}
        self.add(name, 'source', self.owner, 'battlefield' if self.mode == 'cross_owned' else 'hand')
        if self.original_owner != self.owner:
            self.setup[-1]['state'].append('opponent_owned')
        self.setup_grave()
        for player in (0, 1):
            for index, card in enumerate(DECK):
                self.add(card, f'lib_{player}_{index}', player, 'library')
        self.library_ids = set(DECK) | set(self.zones['graveyard'][0] + self.zones['graveyard'][1])
        if self.model == 'plains':
            self.setup_lands()
        if self.mode == 'shroud':
            self.add('Silent Gravestone', 'shroud', self.enemy)
        if self.mode == 'countered':
            self.add('Counterspell', 'counter', self.enemy, 'hand')
        if self.mode in COMBAT:
            blocker = 'Giant Spider' if self.mode == 'flying_reach' else 'Grizzly Bears'
            self.add(blocker, 'blocker', self.enemy)
        if self.model == 'death' and self.mode != 'combat_death':
            removal = {'exile_source': 'Swords to Plowshares', 'bounce_source': 'Unsummon'}.get(self.mode, 'Murder')
            self.add(removal, 'removal', self.enemy, 'hand')
            if self.mode == 'other_dies':
                self.add('Hill Giant', 'other', self.owner)

    def setup_grave(self):
        mode, model = self.mode, self.model
        if mode in ('empty', 'white_mana', *FLIGHT, 'trample'):
            return
        names = {'spirit': SPIRITS, 'artifact': ARTIFACTS, 'death': ('Grizzly Bears', 'Skirk Prospector'),
                 'cemetery': CREATURES[:4]}.get(model, CREATURES[:2])
        wrong = {'spirit': ('Grizzly Bears', 'Reach Through Mists'), 'artifact': ('Suntail Hawk', 'Forest'),
                 'death': ('Scathe Zombies', 'Fervor', 'Forest')}.get(model, ('Forest', 'Fervor'))
        if mode == 'wrong_type':
            names = wrong
        elif mode == 'zero_mv':
            names = ('Ornithopter', *names, *wrong)
        elif mode == 'below_threshold':
            names = (*names[:3], *wrong)
        else:
            if model == 'cemetery' and mode == 'repeat':
                names = CREATURES
            names = (*names, *wrong)
        who = self.enemy if mode == 'opponent_grave' else self.owner
        for i, card in enumerate(dict.fromkeys(names)):
            self.add(card, f'grave_{who}_{i}', who, 'graveyard')
        if mode != 'opponent_grave':
            # A legal card in the other grave must never be returned.
            self.add(SPIRITS[0] if model == 'spirit' else ARTIFACTS[0] if model == 'artifact' else CREATURES[0],
                     'enemy_grave', self.enemy, 'graveyard')

    def setup_lands(self):
        six = self.mode in ('six', 'wrong_land', 'opponent_plains', 'subtyped_land', 'white_mana')
        for i in range(6 if six else 7):
            self.add('Plains', f'plains_{i}', self.owner)
        if self.mode == 'subtyped_land':
            self.add('Tundra', 'typed_land', self.owner)
        if self.mode == 'wrong_land':
            self.add('Forest', 'typed_land', self.owner)
        if self.mode == 'opponent_plains':
            for i in range(7):
                self.add('Plains', f'foe_plains_{i}', self.enemy)
        if self.mode == 'minus_plains':
            self.add('Stone Rain', 'stone', self.owner, 'hand')

    def eligible(self, name):
        record = self.oracle[name]
        card_type, _, subtype = record['type_line'].partition(' — ')
        if self.model == 'spirit':
            return 'Spirit' in subtype.split()
        if self.model == 'artifact':
            return 'Artifact' in card_type.split()
        return 'Creature' in card_type.split() and (self.model != 'death' or record['cmc'] <= 2)

    def threshold(self):
        if self.model == 'plains':
            return sum(entity['player'] == self.owner and entity['zone'] == 'battlefield'
                       and 'Plains' in self.oracle[entity['name']]['type_line'].partition(' — ')[2].split()
                       for entity in self.entities.values()) >= 7
        if self.model == 'cemetery':
            return sum(self.eligible(name) for name in self.zones['graveyard'][self.owner]) >= 4
        return True

    def choose_return(self, *, upkeep=True):
        grave = self.zones['graveyard'][self.owner]
        legal = [name for name in grave if self.eligible(name)]
        if not legal or self.mode == 'shroud' or not self.threshold():
            return None, None
        choice = legal[-1] if self.mode == 'alternate' else legal[0]
        if upkeep and self.owner == 0 and self.model in ('creature', 'artifact', 'plains'):
            if self.mode == 'decline':
                return None, None
            self.emit('choose_trigger_source', ref='source', player=self.owner)
        if self.mode == 'decline':
            self.emit('choose_optional_zone', player=self.owner, value=self.rows[choice]['csvid'])
            return None, list(grave)
        self.emit('choose_zone_card', player=self.owner, value=self.rows[choice]['csvid'])
        return choice, list(grave)

    def return_card(self, choice, offered):
        if choice:
            self.zones['graveyard'][self.owner].remove(choice)
            self.zones[self.params['destination']][self.owner].append(choice)
        if offered is not None:
            self.emit('assert_offered_count', value=len(offered))
            for name in sorted(set(offered)):
                self.emit('assert_offered_legal_card', index=self.rows[name]['csvid'], value=int(self.eligible(name)))
        self.emit('assert_unused_choices', value=0)
        self.emit('assert_forced_zone_choices', value=0)
        self.receipt()

    def next_turn(self):
        self.emit('advance_turn')
        self.active = 1 - self.active
        self.emit('untap_step', player=self.active)
        for entity in self.entities.values():
            entity['damage'] = 0
            if entity['player'] == self.active and entity['zone'] == 'battlefield':
                entity['tapped'] = False

    def upkeep(self, *, capture=False):
        self.next_turn()
        if capture:
            self.replay = len(self.steps)
        can_trigger = (self.model != 'death' and self.entities['source']['zone'] == 'battlefield'
                       and self.active == self.owner)
        choice, offered = self.choose_return() if can_trigger else (None, None)
        self.emit('upkeep_step', player=self.active)
        self.return_card(choice, offered)
        self.emit('turn', player=self.active, value=0)

    def deploy(self):
        if self.mode == 'cross_owned':
            return True
        if self.model == 'plains':
            self.emit('play_land', ref='source', value=1)
            self.move('source', 'battlefield')
            self.entities['source']['tapped'] = True
            self.observe('source', 'tapped', 1)
            self.receipt()
            return True
        mode = self.mode
        fee = self.params['cost']
        budget = list(fee)
        alternate = mode in ('rainbow', 'choose_rainbow')
        if alternate:
            fee = [0, 1, 1, 1, 1, 1]
            budget = list(fee)
        dialog = None
        if mode in ('choose_rainbow', 'choose_normal'):
            budget = [7, 1, 1, 1, 1, 2]
            dialog = int(not alternate)
        denied = mode in ('short_mana', 'wrong_mana')
        if mode == 'short_mana':
            budget[0] -= 1
        if mode == 'wrong_mana':
            budget = [sum(fee), 0, 0, 0, 0, 0]
        self.start_cast('source', fee=fee, budget=budget, dialog=dialog, denied=denied)
        self.emit('assert_unused_choices', value=0)
        if denied:
            self.receipt()
            return False
        if mode == 'countered':
            self.emit('announce_spell', ref='source')
            self.start_cast('counter', response=True)
            self.emit('resolve_response')
            self.move('counter', 'graveyard')
            self.move('source', 'graveyard')
            self.receipt()
            return False
        self.emit('resolve')
        self.move('source', 'battlefield')
        self.receipt()
        return mode not in ('choose_rainbow', 'choose_normal')

    def death(self):
        victim = 'other' if self.mode == 'other_dies' else 'source'
        self.start_cast('removal', target=victim)
        if self.mode == 'replay':
            self.replay = len(self.steps)
        dies = self.mode not in ('exile_source', 'bounce_source', 'other_dies')
        choice, offered = self.choose_return(upkeep=False) if dies else (None, None)
        self.emit('resolve')
        self.move('removal', 'graveyard')
        zone = {'exile_source': 'exile', 'bounce_source': 'hand'}.get(self.mode, 'graveyard')
        self.move(victim, zone)
        if victim == 'source' and self.original_owner != self.owner:
            self.zones[zone][self.owner].remove(self.name)
            self.zones[zone][self.original_owner].append(self.name)
        if self.mode == 'exile_source':
            self.life[self.original_owner] += self.params['power_toughness'][0]
        if offered is not None and self.original_owner == self.owner:
            offered.append(self.name)
        self.return_card(choice, offered)

    def combat(self):
        self.emit('begin_attackers')
        self.emit('attack', ref='source', value=1)
        self.entities['source']['tapped'] = True
        self.emit('finish_attackers')
        self.emit('begin_blockers')
        blocked = self.mode != 'flying_ground'
        self.emit('block', ref='blocker', other='source', value=int(blocked))
        self.emit('advance_combat')
        choice, offered = self.choose_return(upkeep=False) if self.mode == 'combat_death' else (None, None)
        self.emit('combat_damage')
        self.emit('resolve')
        self.emit('end_combat')
        attacker = self.entities['source']
        defender = self.entities['blocker']
        power = attacker['stats'][0]
        if blocked:
            attacker['damage'] = defender['stats'][0]
            defender['damage'] = power
            if attacker['damage'] >= attacker['stats'][1]:
                self.move('source', 'graveyard')
                if offered is not None:
                    offered.append(self.name)
            if defender['damage'] >= defender['stats'][1]:
                self.move('blocker', 'graveyard')
            if self.mode == 'trample':
                self.life[self.enemy] -= power - defender['stats'][1]
        else:
            self.life[self.enemy] -= power
        self.return_card(choice, offered)

    def land_mana(self):
        self.emit('can_activate', ref='source', value=1)
        self.emit('activate', ref='source', value=1)
        self.emit('resolve')
        self.entities['source']['tapped'] = True
        self.mana[self.owner][5] += 1
        self.emit('can_activate', ref='source', value=0)
        self.receipt()

    def play(self):
        self.emit('turn', player=self.active, value=0)
        self.emit('present_ai_dialogs', value=1)
        self.emit('observe_search_filters', value=1)
        self.fund(self.owner, [0] * 6)
        self.receipt()
        if not self.deploy():
            return
        if self.model == 'death' and self.mode != 'combat_death':
            self.death()
            return
        if self.mode == 'white_mana':
            self.emit('can_activate', ref='source', value=0)
            self.emit('activate', ref='source', value=0)
        if self.mode == 'minus_plains':
            self.start_cast('stone', target='plains_6')
            self.emit('resolve')
            self.move('plains_6', 'graveyard')
            self.move('stone', 'graveyard')
            self.receipt()
        self.upkeep()
        if self.mode == 'opponent_upkeep':
            return
        self.upkeep(capture=self.mode == 'replay')
        if self.mode in ('repeat', 'threshold_falls'):
            self.upkeep()
            self.upkeep()
        if self.mode in COMBAT:
            self.combat()
        if self.mode == 'white_mana':
            self.land_mana()


def expand(name, csvid, declaration, params, selected, rows, oracle):
    from recipes import step

    validate_witnesses(oracle, rows)
    if profile()['registry'][name]['csvid'] != csvid:
        raise ValueError('unreserved graveyard recursion source')
    cases = []
    for variant in selected:
        board = Recursion(name, csvid, declaration, params, variant, rows, oracle)
        board.play()
        actions = board.steps
        if board.replay is not None:
            cut = board.replay
            tail = actions[cut:]
            actions = actions[:cut] + [step('capture')] + tail + [step('record_state'), step('restore')] + tail + [step('assert_recorded_state')]
        cases.append(dict(name=f'GraveyardRecursionTriggersRecipe_{csvid}_{variant}', card=name, fixture='empty_main',
                          seed=123, setup=board.setup, steps=actions, _recipe=dict(family=FAMILY, version=2,
                            variant=variant, params=declaration['params'], oracle_text=declaration['oracle_text'],
                            untested_components=['Recursion selectors execute within the actual upkeep/death handler. Responses between target selection and resolution have no standalone fixture boundary.'])))
    return cases
