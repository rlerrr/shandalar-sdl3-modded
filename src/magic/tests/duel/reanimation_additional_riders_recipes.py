"""Whole-Oracle reanimation composed with separate life-cycle riders."""

import json
from pathlib import Path

from cast_untap_trigger_recipes import exact
from play_own_exile_effects_recipes import ExileBoard


FAMILY = 'reanimation_additional_riders'
PROFILE = Path(__file__).parent / 'data/reanimation_additional_riders_wave8_profiles.json'
ENTRY = 'When this creature enters, '
CREATURE_RETURN = 'return target creature card from your graveyard to the battlefield.'
MODELS = {
    ('Flying, protection from black\nEcho {3}{W}{W} (At the beginning of your upkeep, if this came under your control '
     'since the beginning of your last upkeep, sacrifice it unless you pay its echo cost.)\n' + ENTRY + CREATURE_RETURN):
        ('echo', 'Creature — Angel Spirit', '{3}{W}{W}', ('Echo', 'Flying', 'Protection'), (2, 2)),
    (ENTRY + 'exile it unless you discard a creature card.\nWhen this creature dies, exile it and ' + CREATURE_RETURN):
        ('discard', 'Creature — Phyrexian Minion', '{2}{B}{B}', (), (2, 2)),
    ('Double strike\n' + ENTRY + 'each player puts a creature card with mana value 3 or less from their graveyard onto the battlefield.\n'
     + ENTRY + 'sacrifice it unless {R} was spent to cast it.'):
        ('red', 'Creature — Zombie', '{3}{B}', ('Double strike',), (2, 2)),
    ('Flying\n' + ENTRY + "put target creature card from an opponent's graveyard onto the battlefield under your control. "
     "It gains haste. At the beginning of your next end step, exile it.\n"
     "Persist (When this creature dies, if it had no -1/-1 counters on it, return it to the battlefield under its owner's control with a -1/-1 counter on it.)"):
        ('persist', 'Creature — Faerie Wizard', '{3}{B}{B}', ('Flying', 'Persist'), (3, 2)),
    ("When this creature enters from a graveyard, target opponent gains control of it.\n"
     "Undying (When this creature dies, if it had no +1/+1 counters on it, return it to the battlefield under its owner's control with a +1/+1 counter on it.)"):
        ('undying', 'Creature — Demon', '{B}{B}', ('Undying',), (4, 3)),
    ('Flying\nWill of the council — ' + ENTRY + 'starting with you, each player votes for grace or condemnation. '
     'If grace gets more votes, each player returns each creature card from their graveyard to the battlefield. '
     'If condemnation gets more votes or the vote is tied, destroy all creatures other than this creature.'):
        ('vote', 'Creature — Angel', '{4}{W}{B}', ('Flying', 'Will of the council'), (4, 4)),
}
COMMON = ('effect', 'underpaid', 'countered', 'replay')
OPTIONS = {
    'echo': ('empty', 'wrong_type', 'alternate', 'opponent_only', 'shroud', 'echo_pay', 'echo_decline',
             'echo_short', 'echo_wrong_color', 'flight_ground', 'protection_black', 'flight_artifact', 'target_protection'),
    'discard': ('empty', 'wrong_type', 'entry_decline', 'death_other', 'death_shroud', 'bounce_no_death'),
    'red': ('empty', 'wrong_type', 'red', 'red_empty', 'own_only', 'enemy_only', 'shroud', 'double_strike', 'boundary'),
    'persist': ('empty', 'wrong_type', 'alternate', 'own_only', 'shroud', 'persist', 'source_bounced',
                'borrowed_bounced', 'flight_ground', 'flight_reach'),
    'undying': ('from_grave', 'hexproof', 'twice', 'blink', 'grave_return_shroud'),
    'vote': ('empty', 'wrong_type', 'condemn', 'tie_owner_grace', 'tie_enemy_grace', 'shroud', 'flight_ground'),
}
ECHO = {'echo_pay', 'echo_decline', 'echo_short', 'echo_wrong_color'}
RED = {'red', 'red_empty', 'double_strike', 'boundary'}
VOTES = {'condemn': (1, 1), 'tie_owner_grace': (0, 1), 'tie_enemy_grace': (1, 0)}
FLIGHT = {'flight_ground': 'Grizzly Bears', 'protection_black': 'Sengir Vampire',
          'flight_artifact': 'Ornithopter', 'flight_reach': 'Canopy Spider'}
DECK = ('Island', 'Mountain')
BASE_GRAVE = ('Grizzly Bears', 'Hill Giant', 'Reach Through Mists')
WITNESSES = set(DECK + BASE_GRAVE) | {
    'Canopy Spider', 'Cloudshift', 'Counterspell', 'Darksteel Myr', 'Forest', 'Leyline of Sanctity', 'Memnite',
    'Murder', 'Ornithopter', 'Plains', 'Sengir Vampire', 'Silent Gravestone', 'Spellbook', 'Trained Armodon',
    'Unsummon', 'Zombify',
}
HANDLED = {'Cloudshift', 'Counterspell', 'Darksteel Myr', 'Leyline of Sanctity', 'Murder',
           'Sengir Vampire', 'Silent Gravestone', 'Spellbook', 'Unsummon', 'Zombify'}


def profile():
    return json.loads(PROFILE.read_text(encoding='utf8'))


def classify(record):
    from recipes import cost_from_oracle

    text = record['oracle_text']
    if record.get('layout') != 'normal' or record.get('card_faces') or record.get('selected_face') or text not in MODELS:
        raise ValueError('supported normal whole-Oracle reanimation rider required')
    rider, type_line, cost, keywords, stats = MODELS[text]
    if (record['type_line'], record['mana_cost'], tuple(record['keywords']), (record['power'], record['toughness'])) != (
            type_line, cost, keywords, tuple(str(n) for n in stats)):
        raise ValueError('exact reanimation source mana, type and keywords required')
    return dict(rider=rider, cost=cost_from_oracle(record), threshold=3 if rider == 'red' else None)


def variants(params):
    return [f'{mode}_{owner}' for owner in (0, 1) for mode in COMMON + OPTIONS[params['rider']]]


def spec(declaration, record):
    params = classify(record)
    chosen = declaration.get('variants')
    if (declaration.get('family') != FAMILY or type(declaration.get('version')) is not int or declaration['version'] != 2
            or declaration.get('oracle_text') != record['oracle_text'] or not exact(declaration.get('params'), params)
            or not exact(profile()['sources'].get(record['name']), record)):
        raise ValueError('exact pinned Oracle and typed reanimation rider required')
    if (not isinstance(chosen, list) or not chosen or any(not isinstance(v, str) for v in chosen)
            or len(set(chosen)) != len(chosen) or set(chosen) - set(variants(params))):
        raise ValueError('distinct reviewed reanimation variants required')
    return dict(params, family=FAMILY)


def validate_witnesses(oracle, rows):
    pins = profile()
    records = pins['sources'] | pins['witnesses']
    if set(pins['witnesses']) != WITNESSES or set(records) != set(pins['registry']):
        raise ValueError('complete reanimation rider witness inventory required')
    for name, record in records.items():
        row = rows[name]
        if row['kind'] != 'card' or not exact(record, oracle[name]) or not exact(row, pins['registry'][name]):
            raise ValueError(f'Oracle or registry drift for {name}')
        if name in pins['sources'].keys() | HANDLED and row['handler'] in ('card_dummy', '0', 'NULL', None):
            raise ValueError(f'expected production implementation for {name}')


class RiderBoard(ExileBoard):
    def __init__(self, name, csvid, declaration, params, variant, rows, oracle):
        self.mode, seat = variant.rsplit('_', 1)
        self.owner, self.enemy = int(seat), 1 - int(seat)
        self.active = self.owner
        self.name, self.csvid, self.variant = name, csvid, variant
        self.declaration, self.params, self.rows, self.oracle = declaration, params, rows, oracle
        self.model = params['rider']
        self.setup, self.steps, self.entities = [], [], {}
        self.mana, self.life = [[0] * 6, [0] * 6], [20, 20]
        self.zones = {zone: [[], []] for zone in ('hand', 'battlefield', 'graveyard', 'library', 'exile')}
        self.loyalty = self.replay = None
        self.borrowed = None
        self.add(name, 'source', self.owner, 'graveyard' if self.mode == 'from_grave' else 'hand')
        for who in (0, 1):
            for i, item in enumerate(DECK):
                self.add(item, f'lib_{who}_{i}', who, 'library')
        self.setup_witnesses()
        self.library_ids = set(DECK) | {self.name} | set().union(*self.zones['graveyard'])

    def seed_graves(self, who, names):
        for i, name in enumerate(names):
            self.add(name, f'grave_{who}_{i}', who, 'graveyard')

    def setup_witnesses(self):
        mode = self.mode
        if self.model in ('echo', 'discard', 'persist') and mode not in ('empty', *FLIGHT):
            who = self.enemy if (self.model == 'persist') != (mode in ('opponent_only', 'own_only')) else self.owner
            self.seed_graves(who, ['Reach Through Mists'] if mode == 'wrong_type' else BASE_GRAVE)
        if self.model in ('red', 'vote') and mode not in ('empty', 'red_empty', *FLIGHT):
            for who in (self.owner, self.enemy):
                if (mode == 'own_only' and who == self.enemy) or (mode == 'enemy_only' and who == self.owner):
                    continue
                good = ['Trained Armodon' if mode == 'boundary' else 'Grizzly Bears']
                self.seed_graves(who, ['Reach Through Mists', 'Hill Giant'] if mode == 'wrong_type' and self.model == 'red'
                                 else ['Reach Through Mists'] if mode == 'wrong_type' else [*good, 'Hill Giant', 'Reach Through Mists'])
        if self.model == 'discard' and mode != 'empty':
            self.add('Reach Through Mists', 'hand_spell', self.owner, 'hand')
            if mode != 'wrong_type':
                self.add('Memnite', 'discard_card', self.owner, 'hand')
        if mode in ('shroud', 'death_shroud', 'grave_return_shroud'):
            self.add('Silent Gravestone', 'shroud', self.enemy)
        if mode in ECHO or (self.model == 'echo' and mode in FLIGHT):
            for i in range(4 if mode == 'echo_short' else 5):
                self.add('Forest' if mode == 'echo_wrong_color' else 'Plains', f'land_{i}', self.owner)
        if mode in ('bounce_no_death', 'source_bounced', 'borrowed_bounced', 'target_protection'):
            self.add('Unsummon', 'bounce', self.enemy, 'hand')
        if mode in FLIGHT:
            self.add(FLIGHT[mode], 'blocker', self.enemy)
        if mode == 'target_protection':
            self.add('Murder', 'black_spell', self.enemy, 'hand')
        if mode == 'countered':
            self.add('Counterspell', 'counter', self.enemy, 'hand')
        if mode == 'from_grave':
            self.add('Zombify', 'return_spell', self.owner, 'hand')
        if mode == 'blink':
            self.add('Cloudshift', 'blink', self.owner, 'hand')
        if mode == 'hexproof':
            self.add('Leyline of Sanctity', 'protected_player', self.enemy)
        if mode in VOTES:
            self.add('Grizzly Bears', 'own_creature', self.owner)
            self.add('Hill Giant', 'enemy_creature', self.enemy)
            self.add('Darksteel Myr', 'indestructible', self.enemy)
            self.add('Spellbook', 'noncreature', self.owner)

    def creature(self, name):
        return 'Creature' in self.oracle[name]['type_line'].partition(' — ')[0].split()

    def eligible(self, name):
        bound = self.params['threshold']
        return self.creature(name) and (bound is None or self.oracle[name]['cmc'] <= bound)

    def plan_grave(self, who, *, prefer=None):
        candidates = [name for name in self.zones['graveyard'][who] if self.eligible(name)]
        if not candidates or (self.mode in ('shroud', 'death_shroud') and self.model not in ('red', 'vote')):
            return None
        choice = prefer if prefer else candidates[-1 if self.mode == 'alternate' else 0]
        if choice not in candidates:
            raise ValueError('printed graveyard eligibility required')
        self.emit('choose_zone_card', player=who if self.model == 'red' else self.owner, value=self.rows[choice]['csvid'])
        return who, choice

    def reanimate(self, plan, receiver=None):
        if plan is None:
            return
        who, chosen = plan
        self.zones['graveyard'][who].remove(chosen)
        self.zones['battlefield'][self.owner if receiver is None else receiver].append(chosen)

    def bind(self, ref, name, who, modifier=0):
        self.emit('bind_permanent_copy', ref=ref, player=who, value=self.rows[name]['csvid'], index=0)
        types = self.oracle[name]
        stats = tuple(int(types[p]) + modifier for p in ('power', 'toughness'))
        self.entities[ref] = dict(name=name, player=who, zone='battlefield', stats=stats, tapped=False, damage=0)

    def endpoint(self):
        self.emit('assert_unused_choices', value=0)
        self.emit('assert_forced_zone_choices', value=0)
        self.emit('refresh')
        self.receipt()

    def cast_source(self):
        fee = list(self.params['cost'])
        budget = fee.copy()
        if self.mode in RED:
            fee[0], fee[4] = fee[0]-1, 1
            budget = fee.copy()
            if self.owner == 0:
                self.emit('choose_dialog', player=self.owner, value=1)
        if self.mode == 'underpaid':
            budget[next(i for i, amount in enumerate(fee) if amount > 0)] -= 1
        self.start_cast('source', fee=fee, budget=budget, denied=self.mode == 'underpaid')
        if self.mode == 'underpaid':
            self.endpoint()
            return False
        if self.mode == 'countered':
            self.emit('announce_spell', ref='source')
            self.start_cast('counter', response=True)
            self.emit('resolve_response')
            self.move('counter', 'graveyard')
            self.move('source', 'graveyard')
            self.endpoint()
            return False
        return True

    def finish_entry(self):
        if self.model == 'discard':
            self.discard_entry()
            return
        if self.model == 'red':
            plans = [self.plan_grave(who) for who in (self.owner, self.enemy)]
            self.emit('resolve')
            self.move('source', 'battlefield')
            for plan, who in zip(plans, (self.owner, self.enemy)):
                self.reanimate(plan, who)
            if self.mode not in RED:
                self.move('source', 'graveyard')
        elif self.model == 'vote':
            self.resolve_vote()
        else:
            plan = None if self.model == 'undying' else self.plan_grave(self.enemy if self.model == 'persist' else self.owner)
            self.emit('resolve')
            self.move('source', 'battlefield')
            self.reanimate(plan)
            if self.model == 'persist' and plan:
                self.borrowed = plan[1]
        self.endpoint()

    def discard_entry(self):
        # The spell is physically pending, but the resolved creature is no
        # longer a card in hand when its ETB opens the real discard selector.
        offered = [card for card in self.zones['hand'][self.owner] if card != self.name]
        discard = 'discard_card' in self.entities and self.mode != 'entry_decline'
        if offered:
            preferred = self.entities['discard_card']['name'] if 'discard_card' in self.entities else offered[0]
            self.emit('choose_zone_card' if discard else 'choose_optional_zone', player=self.owner, value=self.rows[preferred]['csvid'])
        self.emit('resolve')
        self.move('source', 'battlefield' if discard else 'exile')
        if discard:
            self.move('discard_card', 'graveyard')
        if offered:
            self.emit('assert_offered_count', value=len(offered))
            for card in offered:
                self.emit('assert_offered_legal_card', index=self.rows[card]['csvid'], value=int(self.creature(card)))
        self.endpoint()

    def snatcher_death(self):
        if self.entities['source']['zone'] != 'battlefield':
            return
        if self.mode == 'bounce_no_death':
            self.spell('bounce', 'source')
            self.move('source', 'hand')
        else:
            choice = 'Hill Giant' if self.mode == 'death_other' else 'Memnite'
            plan = self.plan_grave(self.owner, prefer=choice)
            self.emit('destroy', ref='source')
            # With no legal target the entire death trigger is absent,
            # including its effect on the dying Snatcher itself.
            self.move('source', 'exile' if plan else 'graveyard')
            self.reanimate(plan)
        self.endpoint()

    def resolve_vote(self):
        votes = VOTES.get(self.mode, (0, 0))
        for player, ballot in zip((self.owner, self.enemy), votes):
            self.emit('choose_dialog', player=player, value=ballot)
        self.emit('resolve')
        self.move('source', 'battlefield')
        if sum(votes) == 0:
            for who, pile in enumerate(self.zones['graveyard']):
                returning = [name for name in pile if self.creature(name)]
                for name in returning:
                    self.reanimate((who, name), who)
        else:
            self.move('own_creature', 'graveyard')
            self.move('enemy_creature', 'graveyard')

    def echo_turn(self, *, due=False):
        self.emit('advance_turn')
        self.active = 1 - self.active
        self.emit('untap_step', player=self.active)
        for entity in self.entities.values():
            if entity['zone'] == 'battlefield' and entity['player'] == self.active:
                entity['tapped'] = False
        if due:
            pay = self.mode == 'echo_pay' or self.mode in FLIGHT
            if pay or self.mode == 'echo_decline':
                self.emit('choose_dialog', player=self.owner, value=int(not pay))
            if pay:
                for ref in (f'land_{i}' for i in range(5)):
                    if self.owner == 0:
                        self.emit('choose_mana_source', ref=ref)
                    self.entities[ref]['tapped'] = True
            else:
                self.move('source', 'graveyard')
        self.emit('upkeep_step', player=self.active)
        self.endpoint()
        self.emit('turn', player=self.active, value=0)

    def spell(self, ref, target=None):
        self.start_cast(ref, target=target)
        self.emit('resolve')
        self.move(ref, 'graveyard')

    def depart_borrowed(self, destination):
        if self.borrowed:
            self.zones['battlefield'][self.owner].remove(self.borrowed)
            self.zones[destination][self.enemy].append(self.borrowed)
            self.borrowed = None
            self.entities.pop('borrowed', None)

    def expire_borrowed(self):
        self.emit('cleanup')
        self.depart_borrowed('exile')
        self.endpoint()

    def clique(self):
        if self.mode == 'persist':
            # End the first loan on the active controller's real end step.
            # A new loan from a death on the opponent's turn survives that
            # opponent's end step and expires during the controller's next.
            self.advance()
            self.depart_borrowed('exile')
            self.endpoint()
            plan = self.plan_grave(self.enemy)
            self.emit('destroy', ref='source')
            self.bind('source', self.name, self.owner, -1)
            self.observe('source', 'minus1_counters', 1)
            self.reanimate(plan)
            if plan:
                self.borrowed = plan[1]
            self.endpoint()
            self.emit('destroy', ref='source')
            self.move('source', 'graveyard')
            self.endpoint()
            self.advance()
            self.endpoint()
        if not self.borrowed:
            return
        if self.mode == 'source_bounced':
            self.spell('bounce', 'source')
            self.move('source', 'hand')
            self.endpoint()
        self.bind('borrowed', self.borrowed, self.owner)
        if self.mode == 'borrowed_bounced':
            self.spell('bounce', 'borrowed')
            self.depart_borrowed('hand')
            self.endpoint()
        else:
            self.fight('borrowed')
        self.expire_borrowed()

    def demon_returns(self, *, counters):
        who = self.owner if self.mode == 'hexproof' else self.enemy
        self.zones['battlefield'][who].append(self.name)
        self.bind('source', self.name, who, counters)
        self.observe('source', 'plus1_counters', counters)
        self.endpoint()

    def demon(self):
        if self.mode == 'blink':
            self.spell('blink', 'source')
            self.bind('source', self.name, self.owner)
            self.observe('source', 'plus1_counters', 0)
            self.endpoint()
            return
        self.emit('destroy', ref='source')
        self.zones['battlefield'][self.owner].remove(self.name)
        self.demon_returns(counters=1)
        if self.mode == 'twice':
            self.emit('destroy', ref='source')
            self.zones['battlefield'][self.enemy].remove(self.name)
            self.entities.pop('source')
            self.zones['graveyard'][self.owner].append(self.name)
            self.endpoint()

    def fight(self, ref, *, blocked=False):
        self.emit('begin_attackers')
        self.emit('attack', ref=ref, value=1)
        self.entities[ref]['tapped'] = True
        self.emit('finish_attackers')
        self.emit('begin_blockers')
        if 'blocker' in self.entities:
            self.emit('block', ref='blocker', other=ref, value=int(blocked))
        self.emit('advance_combat')
        self.emit('combat_damage')
        self.emit('resolve')
        self.emit('end_combat')
        attacker = self.entities[ref]
        if blocked:
            blocker = self.entities['blocker']
            if attacker['stats'][0] >= blocker['stats'][1]:
                self.move('blocker', 'graveyard')
            else:
                blocker['damage'] = attacker['stats'][0]
            if blocker['stats'][0] >= attacker['stats'][1]:
                self.move(ref, 'graveyard')
            else:
                attacker['damage'] = blocker['stats'][0]
        else:
            self.life[self.enemy] -= attacker['stats'][0] * (2 if self.model == 'red' else 1)
        self.endpoint()

    def play(self):
        self.emit('turn', player=self.owner, value=0)
        self.emit('present_ai_dialogs', value=1)
        self.emit('observe_search_filters', value=1)
        self.fund(self.owner, [0] * 6)
        self.endpoint()
        if self.mode == 'replay':
            self.replay = len(self.steps)
        if self.mode == 'from_grave':
            self.emit('choose_zone_card', player=self.owner, value=self.csvid)
            self.spell('return_spell')
            self.zones['graveyard'][self.owner].remove(self.name)
            self.demon_returns(counters=0)
            return
        if not self.cast_source():
            return
        self.finish_entry()
        if self.mode == 'target_protection':
            self.emit('can_target_creature', ref='black_spell', other='source', value=0)
            self.emit('can_target_creature', ref='bounce', other='source', value=1)
        if self.model == 'discard':
            self.snatcher_death()
        if self.model == 'undying':
            self.demon()
        if self.model == 'persist' and self.mode not in FLIGHT:
            self.clique()
        if self.mode in ECHO:
            self.echo_turn()
            self.echo_turn(due=True)
            if self.mode == 'echo_pay':
                self.echo_turn()
                self.echo_turn()
        if self.mode in FLIGHT or self.mode == 'double_strike':
            if self.model == 'echo':
                # Pay the actual Echo cost to isolate a subsequent combat.
                self.echo_turn()
                self.echo_turn(due=True)
            else:
                self.advance()
                self.advance()
            self.fight('source', blocked=self.mode in ('flight_artifact', 'flight_reach'))


def expand(name, csvid, declaration, params, selected, rows, oracle):
    from recipes import step

    validate_witnesses(oracle, rows)
    if profile()['registry'][name]['csvid'] != csvid:
        raise ValueError('unreserved reanimation rider source')
    cases = []
    for variant in selected:
        board = RiderBoard(name, csvid, declaration, params, variant, rows, oracle)
        board.play()
        actions = board.steps
        if board.replay is not None:
            cut = board.replay
            tail = actions[cut:]
            actions = actions[:cut] + [step('capture')] + tail + [step('record_state'), step('restore')] + tail + [step('assert_recorded_state')]
        gaps = ['The resolver completes the trigger in a single action. Interleaved priority and independently rearranged simultaneous triggers are unobserved.']
        if params['rider'] in ('vote', 'red'):
            gaps.append('The duel has two players; multiplayer seat order is unobserved.')
        cases.append(dict(name=f'ReanimationAdditionalRidersRecipe_{csvid}_{variant}', card=name, fixture='empty_main',
                          seed=123, setup=board.setup, steps=actions, _recipe=dict(family=FAMILY, version=2, variant=variant,
                            params=declaration['params'], oracle_text=declaration['oracle_text'], untested_components=gaps)))
    return cases
