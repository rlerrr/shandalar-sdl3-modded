"""Whole-card permissions to play face-up cards from an opponent's exile."""

from collections import Counter
import json
from pathlib import Path

from cast_untap_trigger_recipes import exact
from global_attack_tax_recipes import Board


FAMILY = 'opponent_exile_permissions'
PROFILE = Path(__file__).parent / 'data/opponent_exile_permissions_wave8_profiles.json'
DRAW = 'Visions of Beyond'
DECK = ('Forest', 'Island', 'Mountain', 'Plains')
WITNESSES = set(DECK) | {DRAW, 'Mind Stone', 'Darksteel Citadel', 'Grizzly Bears',
                         'Giant Spider', 'Crater Elemental', 'Unsummon', 'Counterspell'}
ACTIVE_WITNESSES = {DRAW, 'Mind Stone', 'Darksteel Citadel', 'Crater Elemental', 'Unsummon', 'Counterspell'}
MODES = {
    'upkeep': ('alternate_color', 'short', 'wrong_seat', 'wrong_owner', 'expiry', 'land',
               'empty', 'opposing_upkeep', 'cast_source', 'replay'),
    'combat_modes': ('alternate_color', 'short', 'wrong_seat', 'expiry', 'land', 'empty', 'blocked',
                     'ally', 'goad', 'goad_expiry', 'replay'),
    'land_prefix': ('free', 'land_prefix', 'wrong_seat', 'wrong_owner', 'expiry', 'all_lands',
                    'empty', 'countered', 'replay'),
    'artifact_search': ('exact_cost', 'short', 'wrong_seat', 'expiry', 'artifact_land', 'land_spent',
                        'blocked', 'islandwalk', 'no_artifact', 'empty', 'replay'),
    'hand_or_grave': ('alternate_color', 'short', 'wrong_seat', 'wrong_owner', 'grave', 'decline_hand',
                      'only_land', 'empty', 'persistent', 'offturn_instant', 'offturn_artifact', 'replay'),
    'entry_link': ('alternate_color', 'short', 'wrong_seat', 'wrong_owner', 'artifact', 'own_target',
                   'no_target', 'return', 'spent_return', 'persistent', 'offturn_creature', 'replay'),
}


def profile():
    return json.loads(PROFILE.read_text(encoding='utf8'))


def parameters(record):
    from recipes import cost_from_oracle

    text = record['oracle_text']
    kind, symbols, keywords = record['type_line'], record['mana_cost'], record['keywords']
    mana_permission = 'and you may spend mana as though it were mana of any color to cast '
    until_turn = 'Until end of turn, you may '
    if (record.get('layout') != 'normal' or record.get('selected_face') or record.get('card_faces')):
        raise ValueError('normal primary required for opposing exile permissions')
    if (kind, symbols, keywords, text) == (
         'Enchantment', '{4}{R}', [], "At the beginning of your upkeep, exile the top card of each opponent's library. "
         + until_turn + 'cast spells from among those exiled cards, ' + mana_permission + 'those spells.'):
        model = 'upkeep'
    elif (kind, symbols, keywords, text) == (
         'Legendary Creature — Goblin Rogue', '{R}{R}', ['Goad'],
         'Whenever a creature you control deals combat damage to a player, choose one —\n'
         '• Goad target creature that player controls.\n'
         "• Exile the top card of that player's library. " + until_turn + 'cast that card ' + mana_permission + 'that spell.'):
        model = 'combat_modes'
    elif (kind, symbols, keywords, text) == (
         'Sorcery', '{3}{U}', [], 'Target opponent exiles cards from the top of their library until they exile a nonland card. '
         + until_turn + 'cast that card without paying its mana cost.'):
        model = 'land_prefix'
    elif (kind, symbols, keywords, text) == (
         'Legendary Creature — Merfolk Rogue', '{1}{U}{U}', ['Landwalk', 'Islandwalk'],
         'Islandwalk (This creature can\'t be blocked as long as defending player controls an Island.)\n'
         "Whenever Thada Adel deals combat damage to a player, search that player's library for an artifact card and exile it. "
         'Then that player shuffles. ' + until_turn + 'play that card.'):
        model = 'artifact_search'
    elif (kind, symbols, keywords, text) == (
         'Sorcery', '{3}{U}{B}', [], "Target opponent reveals their hand. You choose a nonland card from that player's "
         'graveyard or hand and exile it. You may cast that card for as long as it remains exiled, '
         + mana_permission + 'that spell.'):
        model = 'hand_or_grave'
    elif (kind, symbols, keywords, text) == (
         'Creature — Human Pirate', '{2}{U}{B}', [],
         'When this creature enters, exile another target creature or artifact until this creature leaves the battlefield. '
         'You may cast that card for as long as it remains exiled, and mana of any type can be spent to cast that spell.'):
        model = 'entry_link'
    else:
        raise ValueError('whole Oracle opposing exile permission with calibrated composition required')
    stats = {'combat_modes': ('2', '2'), 'artifact_search': ('2', '2'), 'entry_link': ('2', '3')}
    if model in stats and (record.get('power'), record.get('toughness')) != stats[model]:
        raise ValueError('printed source stats changed')
    return dict(model=model, cost=cost_from_oracle(record),
                payment=('free' if model == 'land_prefix' else 'printed' if model == 'artifact_search' else 'any_mana'),
                expiry='turn' if model not in {'hand_or_grave', 'entry_link'} else 'while_exiled')


def variants(params):
    return [f'{mode}_{seat}' for seat in (0, 1) for mode in MODES[params['model']]]


def spec(declaration, record):
    params = parameters(record)
    pin = profile()['sources'].get(record['name'], {})
    selected = declaration.get('variants')
    if (declaration.get('family') != FAMILY or type(declaration.get('version')) is not int or declaration['version'] != 2
            or not exact(pin.get('oracle'), record) or declaration.get('oracle_text') != record['oracle_text']
            or not exact(params, declaration.get('params'))):
        raise ValueError('independently pinned whole Oracle and exact payment scope required')
    if (not isinstance(selected, list) or not selected or any(type(item) is not str for item in selected)
            or len(set(selected)) != len(selected) or set(selected) - set(variants(params))):
        raise ValueError('distinct reviewed opposing exile variants required')
    return dict(params, family=FAMILY)


def validate_witnesses(oracle, rows):
    pins = profile()
    if set(pins['witnesses']) != WITNESSES or len(pins['sources']) != len(MODES):
        raise ValueError('complete opposing exile witness inventory required')
    for name, pin in (pins['sources'] | pins['witnesses']).items():
        row = rows[name]
        if not exact(pin['oracle'], oracle[name]) or not exact(pin['registry'], row) or row['kind'] != 'card':
            raise ValueError(f'Oracle or registry drift for {name}')
        if name in pins['sources'].keys() | ACTIVE_WITNESSES and row['handler'] in ('card_dummy', '0', 'NULL', None):
            raise ValueError(f'live action requires implemented handler for {name}')
    if (oracle[DRAW]['oracle_text'] != 'Draw a card. If a graveyard has twenty or more cards in it, draw three cards instead.'
            or oracle[DRAW]['mana_cost'] != '{U}' or oracle['Darksteel Citadel']['type_line'] != 'Artifact Land'):
        raise ValueError('targetless draw and artifact land witnesses changed')


class PermissionBoard(Board):
    def __init__(self, name, declaration, params, variant, rows, oracle):
        mode, seat = variant.rsplit('_', 1)
        self.mode, self.owner, self.rows, self.oracle = mode, int(seat), rows, oracle
        self.enemy = 1 - self.owner
        self.name, self.params, self.model = name, params, params['model']
        self.active = self.owner
        self.setup, self.steps, self.entities = [], [], {}
        self.zones = {zone: [[], []] for zone in ('hand', 'battlefield', 'graveyard', 'library', 'exile')}
        self.mana, self.life = [[0] * 6, [0] * 6], [20, 20]
        self.shuffle = [0, 0]
        self.loyalty = None
        self.replay = None
        self.witness = DRAW
        self.spell_owner = self.enemy
        if self.model == 'artifact_search':
            self.witness = 'Darksteel Citadel' if mode in ('artifact_land', 'land_spent') else 'Mind Stone'
        if self.model == 'entry_link':
            self.witness = 'Mind Stone' if mode == 'artifact' else 'Crater Elemental'
            if mode == 'own_target':
                self.spell_owner = self.owner
        if mode == 'offturn_artifact':
            self.witness = 'Mind Stone'
        if mode == 'land':
            self.witness = 'Plains'
        battlefield = self.model in ('upkeep', 'combat_modes', 'artifact_search') and mode != 'cast_source'
        self.add(name, 'source', self.owner, 'battlefield' if battlefield else 'hand')
        if mode in ('blocked', 'islandwalk', 'goad', 'goad_expiry'):
            self.add('Giant Spider', 'blocker', self.enemy)
        if mode == 'ally':
            self.add('Grizzly Bears', 'ally', self.owner)
        if mode == 'islandwalk':
            self.add('Island', 'defending_island', self.enemy)
        if mode == 'land_spent':
            self.add('Forest', 'land_drop', self.owner, 'hand')
        if mode == 'countered':
            self.add('Counterspell', 'counter', self.enemy, 'hand')
        if mode in ('return', 'spent_return'):
            self.add('Unsummon', 'bounce', self.owner, 'hand')
        start = ['Forest', 'Island'] if mode in ('land_prefix', 'all_lands') else []
        enemy_library = ['Mountain', 'Forest']
        if self.model in ('upkeep', 'combat_modes', 'land_prefix', 'artifact_search'):
            if mode == 'empty':
                enemy_library = []
            elif mode == 'all_lands':
                enemy_library = start + enemy_library
            elif mode == 'no_artifact':
                enemy_library = [DRAW, *enemy_library]
            elif self.model == 'artifact_search':
                enemy_library = ['Forest', self.witness, DRAW]
            else:
                enemy_library = start + [self.witness, *enemy_library]
        for player, library in ((self.owner, DECK), (self.enemy, enemy_library)):
            for index, card in enumerate(library):
                self.add(card, f'library_{player}_{index}', player, 'library')
        if self.model == 'hand_or_grave' and mode != 'empty':
            self.add('Forest', 'foe_hand_land', self.enemy, 'hand')
            if mode not in ('only_land', 'grave'):
                self.add(self.witness if mode != 'decline_hand' else 'Mind Stone', 'foe_hand_spell', self.enemy, 'hand')
            if mode in ('grave', 'decline_hand', 'only_land'):
                self.add('Mountain', 'foe_grave_land', self.enemy, 'graveyard')
                if mode != 'only_land':
                    self.add(self.witness, 'foe_grave_spell', self.enemy, 'graveyard')
        if self.model == 'entry_link':
            self.add('Mountain' if mode == 'no_target' else self.witness, 'target', self.spell_owner)
        if mode == 'wrong_owner':
            self.add(self.witness, 'unrelated_exile', 1 - self.spell_owner, 'exile')
        self.card_names = {item['name'] for item in self.entities.values()} | {n for pile in self.zones['library'] for n in pile}
        self.card_names.add(self.witness)

    def receipt(self):
        self.emit('assert_stack', value=0)
        self.emit('assert_choices_consumed')
        self.pools()
        for who in (0, 1):
            self.emit('assert_life', player=who, value=self.life[who])
            self.emit('assert_shuffle_count', player=who, value=self.shuffle[who])
            for zone, players in self.zones.items():
                cards = players[who]
                self.emit('assert_zone', player=who, zone=zone, value=len(cards))
                if zone == 'library' and self.shuffle[who] == 0:
                    for pos, card in enumerate(cards):
                        self.emit('assert_zone_card', player=who, zone=zone, index=pos, value=self.rows[card]['csvid'])
                else:
                    counts = Counter(cards)
                    for card in sorted(self.card_names):
                        self.emit('assert_zone_multiplicity', player=who, zone=zone, index=self.rows[card]['csvid'],
                                  value=counts[card])

    def fee(self, name):
        from recipes import cost_from_oracle

        return cost_from_oracle(self.oracle[name])

    def start(self, ref, *, response=False, target=None):
        entity = self.entities[ref]
        player, name = entity['player'], entity['name']
        self.fund(player, self.fee(name))
        if target is not None:
            self.emit('choose_target', player=player, ref=target)
        self.emit('cast_response' if response else 'cast', ref=ref, value=1)
        self.observe(ref, 'in_hand', 0)
        self.emit('assert_stack', value=2 if response else 1)
        self.mana[player] = [0] * 6
        self.pools()

    def complete(self, ref, zone):
        self.emit('resolve')
        self.move(ref, zone)
        self.emit('refresh')

    def turn(self, *, upkeep=False):
        assert not any(amount for pool in self.mana for amount in pool)
        self.emit('advance_turn')
        self.active = 1 - self.active
        self.emit('untap_step', player=self.active)
        if upkeep:
            self.emit('upkeep_step', player=self.active)
        self.emit('turn', player=self.active, value=0)

    def move_card(self, player, name, source, destination):
        self.zones[source][player].remove(name)
        self.zones[destination][player].append(name)

    def zone_choice(self, cards, chosen, is_legal):
        self.emit('observe_search_filters', value=1)
        if chosen is None:
            self.emit('choose_zone', player=self.owner, value=-1)
        else:
            assert chosen in cards and is_legal(chosen)
            self.emit('choose_zone_card', player=self.owner, value=self.rows[chosen]['csvid'])
        return list(cards), is_legal

    def search_receipt(self, offered):
        if offered is None:
            return
        cards, is_legal = offered
        self.emit('assert_offered_count', value=len(cards))
        for name in sorted(set(cards)):
            self.emit('assert_offered_legal_card', index=self.rows[name]['csvid'], value=int(is_legal(name)))

    def permission(self, allowed, *, actor=None, owner=None, use=True):
        actor = self.owner if actor is None else actor
        owner = self.spell_owner if owner is None else owner
        kwargs = dict(player=actor, owner=owner, index=self.rows[self.witness]['csvid'], value=int(allowed))
        self.emit('can_play_exiled', **kwargs)
        if use:
            if allowed and actor == 0 and self.params['payment'] == 'any_mana':
                # The human legacy activation charges the generic equivalent one pool click at a time.
                for color, amount in enumerate(self.mana[actor]):
                    for _ in range(amount):
                        self.emit('choose_mana', player=actor, color=color)
            self.emit('play_exiled', **kwargs)
        if not allowed or not use:
            return
        self.zones['exile'][owner].remove(self.witness)
        if self.witness == DRAW:
            self.zones['graveyard'][owner].append(self.witness)
            self.zones['hand'][actor].append(self.zones['library'][actor].pop(0))
        else:
            self.zones['battlefield'][actor].append(self.witness)
        self.mana[actor] = [0] * 6
        self.emit('resolve')
        self.emit('refresh')

    def spell_budget(self):
        if self.params['payment'] == 'free' or 'Land' in self.oracle[self.witness]['type_line']:
            return [0] * 6
        fee = self.fee(self.witness)
        if self.params['payment'] != 'any_mana':
            return fee
        budget = [0] * 6
        budget[3 if fee[4] else 4] = sum(fee)
        return budget

    def resolve_grant(self):
        if self.model == 'upkeep':
            if self.mode == 'cast_source':
                self.start('source')
                self.complete('source', 'battlefield')
            if self.mode == 'opposing_upkeep':
                self.turn(upkeep=True)
                return False
            self.turn()
            self.turn(upkeep=True)
            if self.mode == 'empty':
                return False
            self.move_card(self.enemy, self.witness, 'library', 'exile')
            return self.mode != 'land'
        if self.model in ('combat_modes', 'artifact_search'):
            return self.combat_grant()
        if self.model == 'entry_link':
            self.start('source')
            if self.mode != 'no_target':
                self.emit('choose_target', player=self.owner, ref='target')
            self.complete('source', 'battlefield')
            if self.mode == 'no_target':
                return False
            self.move('target', 'exile')
            return True
        # These spells select the sole opponent in generic_spell.
        self.start('source')
        if self.mode == 'countered':
            self.emit('announce_spell', ref='source')
            # Counterspell targets the actual announced parent in its cast callback.
            self.start('counter', response=True)
            self.emit('resolve_response')
            self.move('counter', 'graveyard')
            self.move('source', 'graveyard')
            return False
        if self.model == 'land_prefix':
            removed = []
            while self.zones['library'][self.enemy]:
                card = self.zones['library'][self.enemy][0]
                removed.append(card)
                self.move_card(self.enemy, card, 'library', 'exile')
                if 'Land' not in self.oracle[card]['type_line']:
                    for player in (self.owner, self.enemy):
                        self.emit('choose_zone', player=player, value=-1)
                    break
            self.complete('source', 'graveyard')
            return self.witness in removed
        return self.intrusion_grant()

    def intrusion_grant(self):
        hand = self.zones['hand'][self.enemy]
        grave = self.zones['graveyard'][self.enemy]
        nonland = lambda name: 'Land' not in self.oracle[name]['type_line']
        source, offered = None, None
        if hand:
            chosen = self.witness if self.witness in hand and self.mode != 'decline_hand' else None
            offered = self.zone_choice(hand, chosen, nonland)
            if chosen:
                source = 'hand'
        if source is None and grave:
            chosen = self.witness if self.witness in grave else None
            offered = self.zone_choice(grave, chosen, nonland)
            if chosen:
                source = 'graveyard'
        self.complete('source', 'graveyard')
        self.search_receipt(offered)
        if source:
            self.move_card(self.enemy, self.witness, source, 'exile')
        return source is not None

    def combat_grant(self):
        attacker = 'ally' if self.mode == 'ally' else 'source'
        self.emit('begin_attackers')
        self.emit('attack', ref=attacker, value=1)
        self.observe(attacker, 'tapped', 1)
        self.emit('finish_attackers')
        self.emit('begin_blockers')
        if self.mode in ('blocked', 'islandwalk'):
            self.emit('block', ref='blocker', other=attacker, value=int(self.mode == 'blocked'))
        self.emit('advance_combat')
        self.observe(attacker, 'blocked', self.mode == 'blocked')
        offered = None
        if self.mode.startswith('goad'):
            self.emit('choose_dialog', player=self.owner, value=0)
            self.emit('choose_target', player=self.owner, ref='blocker')
        elif self.model == 'artifact_search' and self.mode not in ('blocked', 'empty'):
            chosen = None if self.mode == 'no_artifact' else self.witness
            offered = self.zone_choice(self.zones['library'][self.enemy], chosen,
                                       lambda name: 'Artifact' in self.oracle[name]['type_line'])
        self.emit('combat_damage')
        if self.mode.startswith('goad'):
            self.emit('assert_unused_choices', value=0)
        self.emit('end_combat')
        if self.mode == 'blocked':
            self.move(attacker, 'graveyard')
            return False
        self.life[self.enemy] -= 2
        self.search_receipt(offered)
        if self.mode.startswith('goad'):
            return False
        if self.model == 'artifact_search':
            self.shuffle[self.enemy] += 1
        if self.mode in ('empty', 'no_artifact'):
            return False
        self.move_card(self.enemy, self.witness, 'library', 'exile')
        return self.mode != 'land'

    def depart(self, *, return_target):
        self.start('bounce', target='source')
        self.complete('bounce', 'graveyard')
        self.move('source', 'hand')
        if return_target:
            self.move_card(self.spell_owner, self.witness, 'exile', 'battlefield')
        self.permission(False)
        self.receipt()

    def goad_combat(self, *, required):
        self.emit('begin_required_attackers')
        self.emit('can_finish_attackers', value=int(not required))
        self.emit('attack', ref='blocker', value=1)
        self.emit('can_finish_attackers', value=1)
        self.emit('finish_attackers')
        self.emit('begin_blockers')
        self.emit('advance_combat')
        self.emit('combat_damage')
        self.emit('end_combat')
        self.life[self.owner] -= 2
        self.receipt()

    def actions(self):
        from recipes import step

        self.emit('turn', player=self.owner, value=0)
        self.fund(self.owner, [0] * 6)
        granted = self.resolve_grant()
        self.receipt()
        mode = self.mode
        if mode.startswith('goad'):
            self.turn()
            self.goad_combat(required=True)
            if mode == 'goad_expiry':
                self.turn()
                self.turn()
                self.goad_combat(required=False)
        if not granted:
            self.permission(False)
            self.receipt()
            return
        if mode == 'return':
            self.fund(self.owner, self.spell_budget())
            self.permission(True, use=False)
            self.depart(return_target=True)
            return
        if mode == 'land_spent':
            self.emit('play_land', ref='land_drop', value=1)
            self.move('land_drop', 'battlefield')
        self.fund(self.owner, self.spell_budget())
        if mode in ('expiry', 'offturn_creature', 'offturn_instant', 'offturn_artifact', 'persistent'):
            self.permission(True, use=False)
            self.fund(self.owner, [0] * 6)
            self.turn()
            if mode in ('expiry', 'persistent'):
                self.turn()
            self.fund(self.owner, self.spell_budget())
        if mode == 'short':
            self.fund(self.owner, [0] * 6)
        if mode == 'wrong_seat':
            self.fund(self.enemy, self.spell_budget())
            self.permission(False, actor=self.enemy)
            self.receipt()
        if mode == 'wrong_owner':
            self.permission(False, owner=1 - self.spell_owner)
            self.receipt()
        if mode == 'replay':
            self.replay = len(self.steps)
        allowed = mode not in ('expiry', 'short', 'offturn_creature', 'offturn_artifact', 'land_spent')
        self.permission(allowed)
        self.receipt()
        if allowed:
            self.permission(False)
            self.receipt()
        if mode == 'spent_return':
            self.depart(return_target=False)
        if self.replay is not None:
            tail = self.steps[self.replay:]
            self.steps[self.replay:] = [step('capture'), *tail, step('record_state'), step('ai_roundtrip'),
                                        step('restore'), *tail, step('assert_recorded_state')]


def expand(name, csvid, declaration, params, selected, rows, oracle):
    validate_witnesses(oracle, rows)
    if rows[name]['csvid'] != csvid or not selected or set(selected) - set(variants(params)):
        raise ValueError('opposing permission source and variant must remain pinned')
    composition = {
        'upkeep': ['upkeep, opponent library top, spell-only', 'colored payment as though any color'],
        'combat_modes': ['Grenzo or ally combat damage, exile or goad mode', 'required attack and expiry'],
        'land_prefix': ['opposing target, first nonland prefix or all lands', 'free cast in window'],
        'artifact_search': ['Islandwalk, artifact selection and owner shuffle', 'printed spell cost or land play'],
        'hand_or_grave': ['actual hand reveal/filter and hand or graveyard choice', 'permission while exiled and timing'],
        'entry_link': ['another artifact or creature, own and foreign targets', 'return on departure or spent exile',
                       'any-type payment and physical card ownership for exit from exile'],
    }[params['model']]
    result = []
    for variant in selected:
        board = PermissionBoard(name, declaration, params, variant, rows, oracle)
        board.actions()
        metadata = dict(family=FAMILY, version=2, variant=variant, params=declaration['params'],
                        oracle_text=declaration['oracle_text'], covered_clauses=composition,
                        untested_components=['face-down permission identity', 'multiplayer', 'shared-name duplicate permits',
                                             'replacement effects', 'additional costs of the exiled spell'])
        result.append(dict(name=f'OpponentExilePermissionRecipe_{csvid}_{variant}', card=name,
                           fixture='empty_main', seed=123, setup=board.setup, steps=board.steps, _recipe=metadata))
    return result
