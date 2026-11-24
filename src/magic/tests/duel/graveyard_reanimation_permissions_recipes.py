"""Graveyard ownership, hybrid payment, animation, and named return."""

import json
from pathlib import Path

from cast_untap_trigger_recipes import exact
from defensive_creature_recipes import reviewed_payment
from play_own_exile_effects_recipes import ExileBoard


FAMILY = 'graveyard_reanimation_permissions'
PROFILE = Path(__file__).parent / 'data/graveyard_reanimation_permissions_wave8_profiles.json'
UPKEEP = 'At the beginning of your upkeep, '
PATTERNS = {
    ('({W/B} can be paid with either {W} or {B}.)\n' + UPKEEP
     + 'put target creature card from a graveyard onto the battlefield under your control.'):
        ('any_creature', 'Enchantment', '{4}{W/B}{W/B}{W/B}', (), None),
    ("Fear (This creature can't be blocked except by artifact creatures and/or black creatures.)\n"
     + UPKEEP + 'you may return target Goblin card from your graveyard to your hand.'):
        ('goblin', 'Legendary Creature — Goblin Shaman', '{2}{B}{R}', ('Fear',), (3, 3)),
    (UPKEEP + 'you may return target enchantment card from your graveyard to the battlefield.\n'
     'As long as you control five or more enchantments, each other non-Aura enchantment you control is a creature '
     'in addition to its other types and has base power and base toughness each equal to its mana value.'):
        ('enchantment', 'Enchantment', '{4}{W}', (), None),
    ('Flying\nWhen this creature dies, create a 0/1 red Elemental creature token with "At the beginning of your upkeep, '
     'sacrifice this token and return target card named Rekindling Phoenix from your graveyard to the battlefield. '
     'It gains haste until end of turn."'):
        ('phoenix', 'Creature — Phoenix', '{2}{R}{R}', ('Flying',), (4, 3)),
}
DECK = ('Island', 'Mountain')
ENCHANTMENTS = ('Mobilization', 'Kismet', 'Leyline of Anticipation', 'Levitation')
WITNESSES = set(DECK + ENCHANTMENTS) | {
    'Control Magic', 'Counterspell', 'Elemental', 'Giant Spider', 'Grizzly Bears', 'Hill Giant', 'Murder', 'Naturalize',
    'Ornithopter', 'Pacifism', 'Raging Goblin', 'Reach Through Mists', 'Silent Gravestone', 'Tarfire',
    "Tormod's Crypt", 'Unsummon', 'Walking Corpse',
}
HANDLED = set(ENCHANTMENTS) | {
    'Control Magic', 'Counterspell', 'Elemental', 'Murder', 'Naturalize', 'Pacifism', 'Silent Gravestone',
    "Tormod's Crypt", 'Unsummon',
}
COMMON = ('effect', 'opponent_upkeep', 'replay', 'repeat', 'underpaid', 'countered')
EXTRA = {
    'any_creature': ('own_grave', 'other_and_own', 'owner_death', 'empty', 'wrong_type',
                     'white', 'mixed_one_black', 'mixed_two_black', 'wrong_color'),
    'goblin': ('kindred', 'decline', 'opp_only', 'wrong_type', 'wrong_zone', 'shroud',
               'fear_ground', 'fear_black', 'fear_artifact'),
    'enchantment': ('decline', 'opp_only', 'wrong_type', 'wrong_zone', 'four', 'five', 'six',
                    'threshold_rise', 'threshold_fall'),
    'phoenix': ('alive', 'bounce', 'token_removed', 'missing_name', 'wrong_owner', 'shroud',
                'flying_ground', 'flying_reach', 'haste_expires'),
}
ANIMATION = ('four', 'five', 'six', 'threshold_rise', 'threshold_fall')
FEAR = {'fear_ground': 'Grizzly Bears', 'fear_black': 'Walking Corpse', 'fear_artifact': 'Ornithopter'}
FLYING = {'flying_ground': 'Grizzly Bears', 'flying_reach': 'Giant Spider'}
PAYMENT = {'white': 'WWW', 'mixed_one_black': 'BWW', 'mixed_two_black': 'BBW'}


def profile():
    return json.loads(PROFILE.read_text(encoding='utf8'))


def classify(record):
    text = record['oracle_text']
    if record.get('layout') != 'normal' or record.get('card_faces') or record.get('selected_face') or text not in PATTERNS:
        raise ValueError('supported normal whole-Oracle reanimation permission required')
    model, kind, cost, keywords, stats = PATTERNS[text]
    printed_stats = tuple(str(n) for n in stats) if stats else (None, None)
    if (record['type_line'], record['mana_cost'], tuple(record['keywords']),
            (record.get('power'), record.get('toughness'))) != (kind, cost, keywords, printed_stats):
        raise ValueError('exact Oracle cost, type, keywords and stats required')
    payment = {'payment_colors': ['B'] * 3} if model == 'any_creature' else {}
    mana, hybrids = reviewed_payment(record, payment)
    return dict(model=model, cost=mana, hybrid_symbols=[list(pair) for pair in hybrids])


def variants(params):
    return [f'{mode}_{actor}' for actor in (0, 1) for mode in COMMON + EXTRA[params['model']]]


def spec(declaration, record):
    params = classify(record)
    chosen = declaration.get('variants')
    if (declaration.get('family') != FAMILY or type(declaration.get('version')) is not int or declaration['version'] != 2
            or declaration.get('oracle_text') != record['oracle_text'] or not exact(declaration.get('params'), params)
            or not exact(profile()['sources'].get(record['name']), record)):
        raise ValueError('exact whole Oracle and reviewed reanimation recipe required')
    if (not isinstance(chosen, list) or not chosen or any(not isinstance(v, str) for v in chosen)
            or len(set(chosen)) != len(chosen) or set(chosen) - set(variants(params))):
        raise ValueError('distinct reviewed reanimation variants required')
    return dict(params, family=FAMILY)


def validate_witnesses(oracle, rows):
    pins = profile()
    records = pins['sources'] | pins['witnesses']
    if WITNESSES != set(pins['witnesses']) or set(records) != set(pins['registry']):
        raise ValueError('complete reanimation witness inventory required')
    for name, record in records.items():
        row = rows[name]
        if not exact(record, oracle[name]) or not exact(row, pins['registry'][name]) or row['kind'] != 'card':
            raise ValueError(f'Oracle or registry drift for {name}')
        if name in pins['sources'].keys() | HANDLED and row['handler'] in ('card_dummy', '0', 'NULL', None):
            raise ValueError(f'expected live implementation for {name}')
    token = rows['Elemental']
    if token['csvid'] != 18726 or token['handler'] != 'manalinkeh_card_elemental_token':
        raise ValueError('expected the canonical generated Elemental identity')


class ReanimationBoard(ExileBoard):
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
        self.add(name, 'source', self.owner, 'battlefield' if self.mode == 'wrong_owner' else 'hand')
        if self.mode == 'wrong_owner':
            self.setup[-1]['state'].append('opponent_owned')
        for player in (0, 1):
            for i, card in enumerate(DECK):
                self.add(card, f'deck_{player}_{i}', player, 'library')
        self.setup_witnesses()
        self.library_ids = set(DECK) | {self.name, 'Elemental'} | set().union(*self.zones['graveyard'], *self.zones['exile'])

    def setup_witnesses(self):
        mode = self.mode
        if self.model == 'any_creature' and mode != 'empty':
            self.add('Reach Through Mists', 'noncreature', self.owner, 'graveyard')
            if mode != 'wrong_type':
                if mode in ('own_grave', 'other_and_own'):
                    self.add('Grizzly Bears', 'own_candidate', self.owner, 'graveyard')
                if mode != 'own_grave':
                    self.add('Hill Giant', 'other_candidate', self.enemy, 'graveyard')
                if mode == 'repeat':
                    self.add('Grizzly Bears', 'second_candidate', self.enemy, 'graveyard')
                    self.add('Unsummon', 'reply', self.enemy, 'hand')
            if mode == 'owner_death':
                self.add('Murder', 'reply', self.enemy, 'hand')
        elif self.model in ('goblin', 'enchantment'):
            if self.model == 'goblin':
                targets = ('Raging Goblin', 'Tarfire')
            else:
                targets = ('Mobilization', 'Levitation')
            if mode not in ANIMATION and mode not in FEAR:
                self.add('Grizzly Bears', 'nonmatch', self.owner, 'graveyard')
                if mode != 'wrong_type':
                    who = self.enemy if mode == 'opp_only' else self.owner
                    zone = 'exile' if mode == 'wrong_zone' else 'graveyard'
                    for i, name in enumerate(targets):
                        self.add(name, f'candidate_{i}', who, zone)
            if mode == 'shroud':
                self.add('Silent Gravestone', 'shroud', self.enemy)
            if mode in ANIMATION:
                self.animation_setup()
        elif self.model == 'phoenix':
            self.add('Hill Giant', 'not_named', self.owner, 'graveyard')
            if mode == 'missing_name':
                self.add("Tormod's Crypt", 'crypt', self.enemy)
            if mode == 'token_removed':
                self.add('Murder', 'reply', self.enemy, 'hand')
            if mode == 'bounce':
                self.add('Unsummon', 'reply', self.enemy, 'hand')
            if mode == 'haste_expires':
                self.add('Control Magic', 'theft', self.enemy, 'hand')
            if mode == 'shroud':
                self.add('Silent Gravestone', 'shroud', self.enemy)
        if mode in FEAR or mode in FLYING:
            self.add((FEAR | FLYING)[mode], 'blocker', self.enemy)
        if mode == 'countered':
            self.add('Counterspell', 'counter', self.enemy, 'hand')

    def animation_setup(self):
        # Starfield and a real attached Aura count toward the threshold.
        for i, name in enumerate(ENCHANTMENTS[:3 if self.mode in ('four', 'threshold_rise') else 4]):
            if i == 2 and self.mode == 'threshold_rise':
                self.add(name, 'threshold_return', self.owner, 'graveyard')
            elif i != 2 or self.mode == 'six':
                self.add(name, f'enchantment_{i}', self.owner)
        self.add('Grizzly Bears', 'aura_host', self.owner)
        self.add('Pacifism', 'aura', self.owner, 'hand')
        self.add('Mobilization', 'enemy_enchantment', self.enemy)
        if self.mode == 'threshold_fall':
            self.add('Naturalize', 'reply', self.enemy, 'hand')

    def cast_source(self):
        if self.mode == 'wrong_owner':
            return True
        fee = self.params['cost']
        if self.mode in PAYMENT:
            fee, _ = reviewed_payment(self.oracle[self.name], {'payment_colors': list(PAYMENT[self.mode])})
        budget = list(fee)
        if self.mode == 'underpaid':
            budget[0] -= 1
        if self.mode == 'wrong_color':
            budget[2], budget[1] = budget[1], 0
        denied = self.mode in ('underpaid', 'wrong_color')
        self.start_cast('source', fee=fee, budget=budget, denied=denied)
        if denied:
            self.receipt()
            return False
        if self.mode == 'countered':
            self.emit('announce_spell', ref='source')
            self.start_cast('counter', response=True)
            self.emit('resolve_response')
            self.move('source', 'graveyard')
            self.move('counter', 'graveyard')
            self.receipt()
            return False
        self.finish_spell('source', 'battlefield')
        return True

    def finish_spell(self, ref, zone):
        self.emit('resolve')
        self.move(ref, zone)
        self.emit('refresh')
        self.receipt()

    def remap(self, ref, name, *, stats=None, player=None):
        who = self.owner if player is None else player
        self.emit('bind_permanent_copy', ref=ref, player=who, value=self.rows[name]['csvid'], index=0)
        if stats is None and self.oracle[name].get('power') is not None:
            stats = tuple(int(self.oracle[name][key]) for key in ('power', 'toughness'))
        self.entities[ref] = dict(name=name, player=who, zone='battlefield', stats=stats, tapped=False, damage=0)

    def return_card(self, name, who, destination, bind=None):
        self.zones['graveyard'][who].remove(name)
        self.zones[destination][self.owner].append(name)
        if bind:
            self.remap(bind, name)

    def eligible(self, name):
        type_line, _, subtypes = self.oracle[name]['type_line'].partition(' — ')
        if self.model == 'goblin':
            return 'Goblin' in subtypes.split()
        card_type = 'Creature' if self.model == 'any_creature' else 'Enchantment'
        return card_type in type_line.split()

    def tutor_plan(self):
        eligible = [[name for name in pile if self.eligible(name)] for pile in self.zones['graveyard']]
        who = self.owner
        if self.model == 'any_creature' and eligible[self.enemy]:
            if eligible[self.owner]:
                self.emit('choose_player', player=self.owner, value=self.enemy)
            who = self.enemy
        if not eligible[who] or self.mode == 'shroud':
            return None
        preferred = 'Tarfire' if self.mode == 'kindred' else eligible[who][0]
        if self.owner == 0 and self.model in ('goblin', 'enchantment'):
            self.emit('choose_trigger_source', player=self.owner, ref='source')
        self.emit('choose_optional_zone' if self.mode == 'decline' else 'choose_zone_card',
                  player=self.owner, value=self.rows[preferred]['csvid'])
        return who, preferred, list(self.zones['graveyard'][who])

    def upkeep(self, *, capture=False):
        self.emit('advance_turn')
        self.active = 1 - self.active
        self.emit('untap_step', player=self.active)
        for entity in self.entities.values():
            entity['damage'] = 0
            if entity['player'] == self.active and entity['zone'] == 'battlefield':
                entity['tapped'] = False
        if capture:
            self.replay = len(self.steps)
        expected = self.tutor_plan() if self.active == self.owner and self.model != 'phoenix' else None
        has_token = 'token' in self.entities
        self.emit('upkeep_step', player=self.active)
        if expected:
            who, selection, offered = expected
            self.emit('assert_offered_count', value=len(offered))
            for name in sorted(set(offered)):
                self.emit('assert_offered_legal_card', index=self.rows[name]['csvid'], value=int(self.eligible(name)))
            if self.mode != 'decline':
                source = 'raised' if self.mode == 'owner_death' else 'animated_return' if self.mode == 'threshold_rise' else None
                self.return_card(selection, who, 'hand' if self.model == 'goblin' else 'battlefield', bind=source)
        if (self.model == 'phoenix' and self.active == self.owner and has_token and self.mode != 'shroud'
                and self.name in self.zones['graveyard'][self.owner]):
            self.zones['battlefield'][self.owner].remove('Elemental')
            del self.entities['token']
            self.return_card(self.name, self.owner, 'battlefield', bind='source')
        self.emit('assert_unused_choices', value=0)
        self.emit('assert_forced_zone_choices', value=0)
        self.emit('refresh')
        self.receipt()
        self.emit('turn', player=self.active, value=0)

    def destroy_to_owner(self, ref, who):
        self.emit('destroy', ref=ref)
        unit = self.entities.pop(ref)
        self.zones['battlefield'][unit['player']].remove(unit['name'])
        self.zones['graveyard'][who].append(unit['name'])

    def phoenix_death(self):
        printed_owner = self.enemy if self.mode == 'wrong_owner' else self.owner
        self.destroy_to_owner('source', printed_owner)
        self.zones['battlefield'][self.owner].append('Elemental')
        for prop, value in [('token', 1), ('color', 16), ('subtype_Elemental', 1), ('power', 0), ('toughness', 1)]:
            self.emit('assert_matching_permanents', player=self.owner, index=self.rows['Elemental']['csvid'], property=prop, value=value)
        self.remap('token', 'Elemental', stats=(0, 1))
        self.receipt()

    def exile_phoenix(self):
        self.emit('choose_player', player=self.enemy, value=self.owner)
        self.emit('activate', ref='crypt', value=1)
        self.move('crypt', 'graveyard')
        self.emit('resolve')
        self.zones['exile'][self.owner].extend(self.zones['graveyard'][self.owner])
        self.zones['graveyard'][self.owner].clear()
        self.receipt()

    def combat(self, *, blocked=False, probe_attack=False):
        self.emit('begin_attackers')
        self.emit('attack', ref='source', value=int(not probe_attack))
        if not probe_attack:
            self.entities['source']['tapped'] = True
        self.emit('finish_attackers')
        self.emit('begin_blockers')
        if 'blocker' in self.entities:
            self.emit('block', ref='blocker', other='source', value=int(blocked))
        self.emit('advance_combat')
        self.emit('combat_damage')
        self.emit('resolve')
        self.emit('end_combat')
        if blocked:
            attacker, defender = self.entities['source'], self.entities['blocker']
            attacker['damage'] = defender['stats'][0]
            if attacker['stats'][0] >= defender['stats'][1]:
                self.move('blocker', 'graveyard')
            else:
                defender['damage'] = attacker['stats'][0]
        elif not probe_attack:
            self.life[1 - self.active] -= self.entities['source']['stats'][0]
        self.receipt()

    def animation_receipt(self, enabled):
        for ref in ('source', 'aura', 'enemy_enchantment', 'enchantment_0', 'enchantment_1',
                    'enchantment_2', 'enchantment_3', 'animated_return'):
            entity = self.entities.get(ref)
            if not entity or entity['zone'] != 'battlefield':
                continue
            animated = enabled and ref not in ('source', 'aura', 'enemy_enchantment')
            self.emit('can_target_creature', ref='aura_host', other=ref, value=int(animated))
            if animated:
                mana_value = int(self.oracle[entity['name']]['cmc'])
                self.observe(ref, 'power', mana_value)
                self.observe(ref, 'toughness', mana_value)

    def animation(self):
        mode = self.mode
        self.animation_receipt(mode in ('five', 'six', 'threshold_fall'))
        if mode == 'threshold_rise':
            self.upkeep()
            self.upkeep()
            self.animation_receipt(True)
        if mode == 'threshold_fall':
            self.start_cast('reply', target='enchantment_1')
            self.emit('resolve')
            self.move('reply', 'graveyard')
            self.move('enchantment_1', 'graveyard')
            self.emit('refresh')
            self.animation_receipt(False)
            self.receipt()

    def verify_haste_expiry(self):
        self.upkeep()
        self.start_cast('theft', target='source')
        self.emit('resolve')
        self.move('theft', 'battlefield')
        self.zones['battlefield'][self.owner].remove(self.name)
        self.zones['battlefield'][self.enemy].append(self.name)
        self.remap('source', self.name, player=self.enemy)
        self.receipt()
        self.combat(probe_attack=True)

    def play(self):
        self.emit('turn', player=self.owner, value=0)
        self.emit('present_ai_dialogs', value=1)
        self.emit('observe_search_filters', value=1)
        self.fund(self.owner, [0] * 6)
        if 'aura' in self.entities:
            self.start_cast('aura', target='aura_host')
            self.finish_spell('aura', 'battlefield')
        if not self.cast_source():
            return
        if self.mode in ANIMATION:
            self.animation()
            return
        if self.mode in FEAR or self.mode in FLYING:
            self.upkeep()
            self.upkeep()
            self.combat(blocked=self.mode in ('fear_black', 'fear_artifact', 'flying_reach'))
            return
        if self.model == 'phoenix':
            if self.mode == 'bounce':
                self.start_cast('reply', target='source')
                self.emit('resolve')
                self.move('reply', 'graveyard')
                self.move('source', 'hand')
                self.receipt()
            elif self.mode != 'alive':
                self.phoenix_death()
                if self.mode == 'missing_name':
                    self.exile_phoenix()
                if self.mode == 'token_removed':
                    self.start_cast('reply', target='token')
                    self.emit('resolve')
                    self.move('reply', 'graveyard')
                    self.zones['battlefield'][self.owner].remove('Elemental')
                    del self.entities['token']
                    self.receipt()
        self.upkeep()
        if self.mode == 'opponent_upkeep':
            return
        self.upkeep(capture=self.mode == 'replay')
        if self.mode == 'owner_death':
            self.start_cast('reply', target='raised')
            self.emit('resolve')
            self.move('reply', 'graveyard')
            self.zones['battlefield'][self.owner].remove(self.entities.pop('raised')['name'])
            self.zones['graveyard'][self.enemy].append('Hill Giant')
            self.receipt()
        if self.model == 'phoenix' and self.mode == 'haste_expires':
            self.verify_haste_expiry()
        if self.model == 'phoenix' and self.mode in ('effect', 'replay', 'repeat'):
            self.combat()
            if self.mode == 'repeat':
                self.phoenix_death()
        if self.mode == 'repeat':
            self.upkeep()
            self.upkeep()
            if self.model == 'phoenix':
                self.combat()
            elif self.model == 'any_creature':
                # Both successive returns are observed before verifying the
                # first creature still belongs to its original grave owner.
                self.remap('raised', 'Hill Giant')
                self.start_cast('reply', target='raised')
                self.emit('resolve')
                self.move('reply', 'graveyard')
                self.zones['battlefield'][self.owner].remove(self.entities.pop('raised')['name'])
                self.zones['hand'][self.enemy].append('Hill Giant')
                self.receipt()


def expand(name, csvid, declaration, params, selected, rows, oracle):
    from recipes import step

    validate_witnesses(oracle, rows)
    if profile()['registry'][name]['csvid'] != csvid:
        raise ValueError('unreserved grave reanimation source')
    cases = []
    for variant in selected:
        board = ReanimationBoard(name, csvid, declaration, params, variant, rows, oracle)
        board.play()
        actions = board.steps
        if board.replay is not None:
            cut = board.replay
            tail = actions[cut:]
            actions = actions[:cut] + [step('capture')] + tail + [step('record_state'), step('restore')] + tail + [step('assert_recorded_state')]
        gaps = ['The upkeep wrapper resolves target selection and the trigger in one call. Responses after target selection need a distinct stack boundary.']
        if params['model'] == 'phoenix':
            gaps.append('The existing named return fixture has one legal named card. Choice among two same-name cards is not independently observed.')
        cases.append(dict(name=f'GraveyardReanimationPermissionsRecipe_{csvid}_{variant}', card=name, fixture='empty_main',
                          seed=123, setup=board.setup, steps=actions, _recipe=dict(family=FAMILY, version=2, variant=variant,
                            params=declaration['params'], oracle_text=declaration['oracle_text'], untested_components=gaps)))
    return cases
