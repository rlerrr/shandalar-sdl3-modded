"""Real alternate casts, linked ownership, combat eligibility and reveal windows."""

from collections import Counter
import copy
import json
from pathlib import Path

from cast_untap_trigger_recipes import exact
from global_attack_tax_recipes import Board


FAMILY = 'alternate_zone_cast_additional_riders'
PROFILE = Path(__file__).parent / 'data' / f'{FAMILY}_wave8_profiles.json'
DRAW = 'Visions of Beyond'
DECK = ('Island', 'Mountain', 'Plains')
WITNESSES = {DRAW, *DECK, 'Forest', 'Mind Stone', 'Grizzly Bears', 'Giant Spider',
             'Hill Giant', 'Elite Vanguard', 'Memnite', 'Silent Gravestone'}
STATIC_WITNESSES = {'Grizzly Bears', 'Giant Spider', 'Hill Giant', 'Elite Vanguard', 'Memnite', *DECK, 'Forest'}
MODELS = {
    ('Sorcery', '{1}{U}{B}', (), (
        "Exile all opponents' graveyards. You may cast spells from among those cards this turn, and mana of any type can "
        "be spent to cast them. At the beginning of the next end step, if any of those cards remain exiled, return them to "
        "their owners' graveyards.\nExile Mnemonic Betrayal.")): 'betrayal',
    ('Legendary Creature — Human Soldier', '{1}{W}{U}', (), (
        "Daxos can't be blocked by creatures with power 3 or greater.\nWhenever Daxos deals combat damage to a player, "
        "exile the top card of that player's library. You gain life equal to that card's mana value. Until end of turn, "
        "you may cast that card and you may spend mana as though it were mana of any color to cast that spell.")): 'daxos',
    ('Enchantment', '{5}{R}', (), (
        "Whenever you cast a spell from your hand, reveal the top X cards of your library, where X is that spell's mana "
        "value. You may cast a spell with mana value X or less from among cards revealed this way without paying its mana "
        "cost. Put the rest on the bottom of your library in a random order.")): 'invocation',
    ('Creature — Dragon', '{5}{R}{R}', ('Flying',), (
        "Flying\nWhenever this creature deals combat damage to a player, exile that many cards from the top of your "
        "library. Until end of turn, you may cast spells from among those exiled cards.")): 'pillager',
}
MODES = {
    'betrayal': ('alternate_pay', 'expiry', 'short', 'wrong_seat', 'empty', 'shroud', 'replay'),
    'daxos': ('alternate_pay', 'short', 'expiry', 'higher', 'land', 'empty', 'big_block', 'legal_block', 'replay'),
    'invocation': ('equal', 'decline', 'higher', 'zero', 'empty', 'short_library', 'enemy_cast', 'replay'),
    'pillager': ('exact_pay', 'two_spells', 'wrong_color', 'expiry', 'land', 'empty', 'short_library',
                 'flying_ground', 'reach_block', 'replay'),
}


def profile():
    return json.loads(PROFILE.read_text(encoding='utf8'))


def parameters(record):
    from recipes import cost_from_oracle

    if record.get('layout') != 'normal' or record.get('card_faces') or record.get('selected_face'):
        raise ValueError('normal whole Oracle required')
    token = record['type_line'], record['mana_cost'], tuple(record['keywords']), record['oracle_text']
    model = MODELS.get(token)
    stats = {'daxos': ('2', '2'), 'pillager': ('4', '4')}.get(model, (None, None))
    if model is None or stats != (record.get('power'), record.get('toughness')):
        raise ValueError('unreviewed printed alternate cast composition')
    return dict(model=model, cost=cost_from_oracle(record))


def variants(params):
    return [f'{mode}_{actor}' for actor in (0, 1) for mode in ('short_source', *MODES[params['model']])]


def spec(declaration, record):
    params = parameters(record)
    pin = profile()['sources'].get(record['name'], {})
    selected = declaration.get('variants')
    if (declaration.get('family') != FAMILY or type(declaration.get('version')) is not int
            or declaration['version'] != 2 or not exact(record, pin.get('oracle'))
            or declaration.get('oracle_text') != record['oracle_text']
            or not exact(params, declaration.get('params'))):
        raise ValueError('complete pinned primary and exact modes required')
    if (not isinstance(selected, list) or not selected or any(type(v) is not str for v in selected)
            or len(set(selected)) != len(selected) or set(selected) - set(variants(params))):
        raise ValueError('explicit distinct reviewed cases required')
    return dict(params, family=FAMILY)


def validate_witnesses(oracle, rows):
    pins = profile()
    if (set(pins) != {'sources', 'witnesses'} or set(pins['witnesses']) != WITNESSES
            or len(pins['sources']) != len(MODELS)):
        raise ValueError('complete alternate cast pins required')
    for name, pin in (pins['witnesses'] | pins['sources']).items():
        row = rows[name]
        if (not exact(pin['oracle'], oracle[name]) or not exact(pin['registry'], row)
                or row['kind'] != 'card' or row['handler'] in ('NULL', '0', None)
                or (row['handler'] == 'card_dummy' and name not in STATIC_WITNESSES)):
            raise ValueError('alternate cast identity changed: ' + name)
    if (oracle[DRAW]['oracle_text'] != 'Draw a card. If a graveyard has twenty or more cards in it, draw three cards instead.'
            or oracle['Silent Gravestone']['oracle_text'].splitlines()[0] !=
            "Cards in graveyards can't be the targets of spells or abilities."):
        raise ValueError('measured draw and nontargeted-graveyard witnesses required')


class AlternateCastBoard(Board):
    def __init__(self, name, params, variant, rows, oracle):
        self.mode, actor = variant.rsplit('_', 1)
        self.owner, self.rows, self.oracle = int(actor), rows, oracle
        self.enemy = 1 - self.owner
        self.active = self.owner
        self.model = params['model']
        self.setup, self.steps, self.entities = [], [], {}
        self.zones = {zone: [[], []] for zone in ('hand', 'battlefield', 'graveyard', 'library', 'exile')}
        self.mana, self.life = [[0] * 6, [0] * 6], [20, 20]
        self.replay = None
        self.unordered_bottom = 0
        self.watch = {name}
        self.add(name, 'source', self.owner, 'hand')

        libraries = [list(DECK), list(DECK)]
        mode = self.mode
        if self.model == 'betrayal' and mode != 'empty':
            for i, card in enumerate(('Forest', DRAW, 'Mind Stone')):
                self.add(card, f'foe_grave_{i}', self.enemy, 'graveyard')
            self.add('Hill Giant', 'own_grave', self.owner, 'graveyard')
            self.add('Plains', 'unrelated_exile', self.enemy, 'exile')
            if mode == 'shroud':
                self.add('Silent Gravestone', 'shroud', self.enemy)
        elif self.model == 'daxos':
            self.witness = {'higher': 'Hill Giant', 'land': 'Forest'}.get(mode, DRAW)
            libraries[self.enemy] = [] if mode == 'empty' else [self.witness, *DECK]
            if mode in ('big_block', 'legal_block'):
                self.add('Hill Giant' if mode == 'big_block' else 'Elite Vanguard', 'blocker', self.enemy)
        elif self.model == 'pillager':
            libraries[self.owner] = ([DRAW, 'Forest'] if mode == 'short_library' else
                                     [] if mode == 'empty' else [DRAW, 'Forest', 'Mind Stone', 'Plains', 'Island', 'Mountain'])
            if mode in ('flying_ground', 'reach_block'):
                self.add('Grizzly Bears' if mode == 'flying_ground' else 'Giant Spider', 'blocker', self.enemy)
        elif self.model == 'invocation':
            parent = 'Memnite' if mode == 'zero' else 'Mind Stone'
            self.parent = parent
            self.witness = 'Grizzly Bears'
            self.add(parent, 'parent', self.enemy if mode == 'enemy_cast' else self.owner, 'hand')
            libraries[self.owner] = {
                'empty': [],
                'short_library': [self.witness],
                'higher': ['Hill Giant', 'Forest', *DECK],
            }.get(mode, ['Forest', self.witness, *DECK])

        for who, library in enumerate(libraries):
            for i, card in enumerate(library):
                self.add(card, f'library_{who}_{i}', who, 'library')

    def add(self, name, ref, player, zone='battlefield', *, state='untapped'):
        super().add(name, ref, player, zone, state=state)
        self.watch.add(name)

    def fee(self, card):
        from recipes import cost_from_oracle

        record = self.oracle[card]
        if record['mana_cost'] == '{0}':
            return [0]*6
        return cost_from_oracle(record)

    def receipt(self, *, stack=0, skip_hand=False):
        self.emit('assert_stack', value=stack)
        self.emit('assert_choices_consumed')
        self.pools()
        for who in (0, 1):
            self.emit('assert_life', player=who, value=self.life[who])
            for zone, seats in self.zones.items():
                if skip_hand and zone == 'hand':
                    continue
                cards = seats[who]
                counts = Counter(cards)
                self.emit('assert_zone', player=who, zone=zone, value=len(cards))
                if zone == 'library':
                    limit = len(cards) - (self.unordered_bottom if who == self.owner else 0)
                    for i, card in enumerate(cards[:limit]):
                        self.emit('assert_zone_card', player=who, zone=zone, index=i, value=self.rows[card]['csvid'])
                for card in sorted(self.watch):
                    self.emit('assert_zone_multiplicity', player=who, zone=zone, index=self.rows[card]['csvid'],
                              value=counts[card])

    def cast_payment(self, ref, *, allow=True):
        card = self.entities[ref]
        cost = list(self.fee(card['name']))
        if not allow:
            cost[0] -= 1
        who = card['player']
        self.fund(who, cost)
        self.emit('cast', ref=ref, value=int(allow))
        self.emit('assert_stack', value=int(allow))
        if allow:
            self.mana[who] = [0]*6
            self.observe(ref, 'in_hand', 0)
        self.pools()

    def finish_spell(self, ref, zone):
        self.emit('resolve')
        self.move(ref, zone)
        self.emit('refresh')

    def transfer(self, owner, card, old, new):
        self.zones[old][owner].remove(card)
        self.zones[new][owner].append(card)

    def draw(self, actor):
        self.zones['hand'][actor].append(self.zones['library'][actor].pop(0))

    def next_turn(self):
        self.emit('advance_turn')
        self.active = 1 - self.active
        self.emit('untap_step', player=self.active)
        self.emit('turn', player=self.active, value=0)

    def budget(self, witness, *, alternate):
        fee = self.fee(witness)
        if alternate:
            return [0, 0, 0, 0, sum(fee), 0]
        return fee

    def permission(self, witness, source_owner, allowed, *, actor=None, use=True, alternate=False):
        actor = self.owner if actor is None else actor
        self.watch.add(witness)
        kwargs = dict(player=actor, owner=source_owner, index=self.rows[witness]['csvid'], value=int(allowed))
        self.emit('can_play_exiled', **kwargs)
        if not use:
            return
        if allowed and alternate and actor == 0:
            pool = self.mana[actor]
            for color, amount in enumerate(pool):
                for _ in range(amount):
                    self.emit('choose_mana', player=actor, color=color)
        self.emit('play_exiled', **kwargs)
        if allowed:
            self.zones['exile'][source_owner].remove(witness)
            if witness == DRAW:
                self.draw(actor)
                self.zones['graveyard'][source_owner].append(witness)
            elif witness == 'Mind Stone':
                self.zones['battlefield'][actor].append(witness)
            else:
                raise ValueError('unmodeled successful exile spell')
            self.mana[actor] = [0] * 6
            self.emit('resolve')
            self.emit('refresh')

    def run_betrayal(self):
        if self.mode == 'replay':
            self.replay = len(self.steps)
        self.cast_payment('source')
        self.finish_spell('source', 'exile')
        stranded = list(self.zones['graveyard'][self.enemy])
        for card in stranded:
            self.transfer(self.enemy, card, 'graveyard', 'exile')
        self.receipt()
        self.permission('Forest', self.enemy, False)
        self.permission('Hill Giant', self.owner, False)
        if self.mode in ('empty', 'shroud'):
            return
        if self.mode == 'wrong_seat':
            self.fund(self.enemy, self.fee(DRAW))
            self.permission(DRAW, self.enemy, False, actor=self.enemy)
            self.fund(self.enemy, [0]*6)
        if self.mode in ('alternate_pay', 'replay'):
            self.fund(self.owner, self.budget(DRAW, alternate=True))
            self.permission(DRAW, self.enemy, True, alternate=True)
            stranded.remove(DRAW)
            self.receipt()
        elif self.mode == 'short':
            self.permission(DRAW, self.enemy, False)
        else:
            self.fund(self.owner, self.budget(DRAW, alternate=True))
            self.permission(DRAW, self.enemy, True, use=False)
            self.fund(self.owner, [0]*6)
        self.next_turn()
        for card in stranded:
            self.transfer(self.enemy, card, 'exile', 'graveyard')
        self.fund(self.owner, self.budget(DRAW, alternate=True))
        self.permission(DRAW, self.enemy, False)
        self.receipt()

    def combat(self):
        self.emit('begin_attackers')
        self.emit('attack', ref='source', value=1)
        self.observe('source', 'tapped', 1)
        self.emit('finish_attackers')
        self.emit('begin_blockers')
        blocked = self.mode in ('legal_block', 'reach_block')
        if 'blocker' in self.entities:
            self.emit('block', ref='blocker', other='source', value=int(blocked))
        self.emit('advance_combat')
        self.observe('source', 'blocked', blocked)
        if self.model == 'pillager' and not blocked and self.zones['library'][self.owner]:
            # The resolver displays the nonempty exile prefix to its controller.
            # Done closes the notice; it neither selects a card nor declines exile.
            self.emit('choose_zone', player=self.owner, value=-1)
        self.emit('combat_damage')
        self.emit('resolve')
        self.emit('end_combat')
        if blocked:
            self.move('blocker', 'graveyard')
            if self.model == 'daxos':
                self.move('source', 'graveyard')
            else:
                self.observe('source', 'damage', 2)
        else:
            self.life[self.enemy] -= self.entities['source']['stats'][0]
        return not blocked

    def run_combat_permission(self):
        self.cast_payment('source')
        self.finish_spell('source', 'battlefield')
        self.next_turn()
        self.next_turn()
        if self.mode == 'replay':
            self.replay = len(self.steps)
        hit = self.combat()
        daxos = self.model == 'daxos'
        exile_owner = self.enemy if daxos else self.owner
        amount = 1 if daxos else self.entities['source']['stats'][0]
        revealed = self.zones['library'][exile_owner][:amount] if hit else []
        for card in revealed:
            self.transfer(exile_owner, card, 'library', 'exile')
        if daxos and revealed:
            self.life[self.owner] += int(self.oracle[revealed[0]]['cmc'])
        self.receipt()
        if not revealed:
            self.permission(DRAW, exile_owner, False)
            return
        if self.mode == 'land':
            self.permission('Forest', exile_owner, False)
        elif self.mode in ('short', 'wrong_color'):
            if self.mode == 'wrong_color':
                self.fund(self.owner, [0, 0, 0, 0, 1, 0])
            self.permission(DRAW, exile_owner, False)
            self.receipt()
        elif self.mode == 'expiry':
            self.fund(self.owner, self.budget(DRAW, alternate=daxos))
            self.permission(DRAW, exile_owner, True, use=False)
            self.fund(self.owner, [0]*6)
            self.next_turn()
            self.fund(self.owner, self.budget(DRAW, alternate=daxos))
            self.permission(DRAW, exile_owner, False)
            self.receipt()
        elif self.mode in ('alternate_pay', 'exact_pay', 'two_spells', 'replay'):
            self.fund(self.owner, self.budget(DRAW, alternate=daxos))
            self.permission(DRAW, exile_owner, True, alternate=daxos)
            self.receipt()
            if self.mode == 'two_spells':
                self.fund(self.owner, self.fee('Mind Stone'))
                self.permission('Mind Stone', exile_owner, True)
                self.receipt()
        else:
            if 'Forest' in revealed:
                self.permission('Forest', exile_owner, False)

    def run_invocation(self):
        self.cast_payment('source')
        self.finish_spell('source', 'battlefield')
        if self.mode == 'enemy_cast':
            self.next_turn()
        if self.mode == 'replay':
            self.replay = len(self.steps)
        self.cast_payment('parent')
        library = self.zones['library'][self.owner]
        count = int(self.oracle[self.parent]['cmc']) if self.active == self.owner else 0
        revealed, rest = library[:count], library[count:]
        eligible = self.witness in revealed
        chosen = self.witness if eligible and self.mode != 'decline' else None
        if revealed:
            # The production display has its own Done acknowledgement. An
            # optional selector follows only if the filtered slice is nonempty.
            self.emit('choose_zone', player=self.owner, value=-1)
        if eligible:
            self.emit('observe_search_filters', value=1)
            if chosen:
                self.emit('choose_zone_card', player=self.owner, value=self.rows[chosen]['csvid'])
            else:
                self.emit('choose_zone', player=self.owner, value=-1)
        self.emit('announce_spell', ref='parent')
        if eligible:
            self.emit('assert_offered_count', value=len(revealed))
            self.emit('assert_offered_selection_required', value=0)
            self.emit('assert_offered_legal_card', index=self.rows[self.witness]['csvid'], value=1)
        if chosen:
            revealed.remove(chosen)
            self.zones['battlefield'][self.owner].append(chosen)
        self.unordered_bottom = len(revealed) if len(revealed) > 1 else 0
        self.zones['library'][self.owner] = rest + revealed
        self.receipt(stack=1, skip_hand=True)
        self.finish_spell('parent', 'battlefield')
        self.receipt()

    def run(self):
        self.emit('turn', player=self.owner, value=0)
        self.emit('present_ai_dialogs', value=1)
        self.fund(self.owner, [0]*6)
        if self.mode == 'short_source':
            self.cast_payment('source', allow=False)
            self.receipt()
        elif self.model == 'betrayal':
            self.run_betrayal()
        elif self.model == 'invocation':
            self.run_invocation()
        else:
            self.run_combat_permission()


def expand(name, csvid, declaration, params, selected, rows, oracle):
    from recipes import step

    validate_witnesses(oracle, rows)
    if rows[name]['csvid'] != csvid or not selected or len(selected) != len(set(selected)) or set(selected) - set(variants(params)):
        raise ValueError('explicit reviewed registered source variants required')
    out = []
    for variant in selected:
        board = AlternateCastBoard(name, params, variant, rows, oracle)
        board.run()
        steps = board.steps
        if board.replay is not None:
            prefix, suffix = steps[:board.replay], steps[board.replay:]
            steps = prefix + [step('capture')] + suffix + [
                step('record_state'), step('restore'), *copy.deepcopy(suffix), step('assert_recorded_state')]
        out.append(dict(name=f'AlternateZoneCastAdditionalRiderRecipe_{csvid}_{variant}', card=name,
                        fixture='empty_main', seed=123, setup=board.setup, steps=steps,
                        _recipe=dict(family=FAMILY, version=2, variant=variant, params=declaration['params'],
                                     oracle_text=declaration['oracle_text'], untested_components=[
                                         'Multiplayer ownership, replacement effects, and overlapping permissions for identical copies.'])))
    return out
