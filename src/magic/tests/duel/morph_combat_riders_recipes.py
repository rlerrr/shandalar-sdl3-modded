"""Real Morph combat triggers and the separate attacking or damage receipts."""
import copy
import json
from pathlib import Path

from morph_creature_recipes import FACE_DOWN, MORPH
from morph_with_keywords_recipes import _World


FAMILY = 'morph_combat_riders'
PROFILE = Path(__file__).parent / 'data/morph_combat_riders_wave8_profiles.json'
HIT = 'Whenever this creature deals combat damage to a player, '
ONE = HIT + 'that player discards a card.'
TWO = HIT + 'that player discards two cards.'
SAC = HIT + 'you may sacrifice it. If you do, that player discards three cards.'
EXILE = HIT + 'that player exiles the top ten cards of their library.'
HALF = ('Whenever this creature attacks, you lose half your life, rounded up.',
        HIT + 'that player loses half their life, rounded up.')
GRAMMAR = {
    HALF: ('half', 0, ()),
    (SAC,): ('sacrifice', 3, ()),
    (ONE,): ('discard', 1, ()),
    (EXILE,): ('exile', 10, ()),
    ('Flying', TWO): ('discard', 2, ('Flying',)),
}
HANDLERS = {
    'half': {'manalinkeh_card_ebonblade_reaper'},
    'sacrifice': {'manalinkeh_card_haunted_cadaver'},
    'discard': {'manalinkeh_card_headhunter', 'manalinkeh_card_riptide_pilferer', 'manalinkeh_card_silent_specter'},
    'exile': {'manalinkeh_card_raven_guild_master'},
}
HAND = ('Forest', 'Lightning Bolt', 'Dark Ritual', 'Swamp')
DECK = ('Forest', 'Mountain', 'Island', 'Forest', 'Plains', 'Swamp', 'Lightning Bolt',
        'Counterspell', 'Unsummon', 'Dark Ritual', 'Plains', 'Island')
WITNESSES = {*HAND, *DECK, 'Fog', 'Hermetic Study', 'Jace Beleren', 'Hill Giant', 'Wind Drake'}
NATIVE_CHARACTERISTICS = {'Hill Giant', 'Wind Drake'}
OPENING = ('printed_cast', 'hidden_cast', 'hidden_choice', 'cast_short', 'reveal_short', 'wrong_color', 'offturn_flip')
HIDDEN = {'hidden_cast', 'hidden_choice', 'hidden_hit', 'hidden_blocked', 'late_flip_hit'}
COMBAT = ('hidden_hit', 'hidden_blocked', 'printed_hit', 'revealed_hit', 'blocked', 'fogged', 'walker_hit',
          'other_attacker', 'late_flip_hit', 'fresh_flip', 'replay_hit')


def profiles():
    return json.loads(PROFILE.read_bytes())


def classify(record):
    from recipes import COLORS, cost_from_oracle

    if (not record or record.get('layout') != 'normal' or record.get('selected_face') or record.get('card_faces')
            or not record['type_line'].startswith('Creature — ')):
        raise ValueError('whole fixed-cost normal Morph creature required')
    clauses = record['oracle_text'].splitlines()
    rule = GRAMMAR.get(tuple(clauses[:-1]))
    morph = MORPH.fullmatch(clauses[-1])
    attributes = (record.get('power'), record.get('toughness'))
    if not rule or not morph or not all(type(stat) is str and stat.isascii() and stat.isdecimal() for stat in attributes):
        raise ValueError('fixed combat rider plus full Morph text required')
    mode, amount, keys = rule
    if sorted(record['keywords']) != sorted(['Morph', *keys]):
        raise ValueError('unmodeled printed keyword')
    fee = cost_from_oracle(dict(record, mana_cost=morph[1]))
    mana = cost_from_oracle(record)
    if not any(fee[1:]) or sum(value != 0 for value in mana[1:]) != 1:
        raise ValueError('reviewed one-color fixed casting and face-up costs required')
    return dict(mode=mode, amount=amount, mana=mana, morph=fee, power=int(attributes[0]), toughness=int(attributes[1]),
                keywords=list(keys), color=sum(1 << COLORS[color] for color in record['colors']))


def variants(shape):
    modes = OPENING + COMBAT + ('noncombat_damage',)
    extra = ()
    if shape['mode'] == 'half':
        modes += ('odd_life',)
    elif shape['mode'] == 'exile':
        modes += ('empty_library', 'short_library', 'exact_library')
    else:
        modes += ('empty_hand',)
        if shape['amount'] > 1:
            modes += ('short_hand',)
        if shape['mode'] == 'sacrifice':
            extra = ('decline_0',)
        else:
            extra = ('human_high_1',)
    if 'Flying' in shape['keywords']:
        modes += ('ground_evasion',)
    return [f'{mode}_{owner}' for mode in modes for owner in (0, 1)] + list(extra)


def spec(declaration, record):
    from library_search_recipes import wave6_exact

    shape = classify(record)
    source = profiles()['sources'].get(record['name'])
    chosen = declaration.get('variants')
    if (not source or not wave6_exact(record, source['oracle']) or declaration.get('family') != FAMILY
            or type(declaration.get('version')) is not int or declaration['version'] != 2
            or declaration.get('oracle_text') != record['oracle_text'] or not wave6_exact(declaration.get('params'), shape)
            or not isinstance(chosen, list) or not chosen or len(set(chosen)) != len(chosen)
            or set(chosen) - set(variants(shape))):
        raise ValueError('full pinned Morph source and distinct combat variants required')
    return dict(shape, effect=FAMILY, family=FAMILY)


def validate_witnesses(oracle, rows):
    data = profiles()
    if set(data) != {'sources', 'witnesses', 'face_down'} or data['face_down']['csvid'] != FACE_DOWN:
        raise ValueError('exact Morph face and pinned sources required')
    if rows.get(data['face_down']['name']) != data['face_down'] or set(data['witnesses']) != WITNESSES:
        raise ValueError('complete independent choice, recipient and combat witnesses required')
    for group in ('sources', 'witnesses'):
        for name, pin in data[group].items():
            row = rows[name]
            if (pin['oracle'] != oracle[name] or pin['registry'] != row or row['kind'] != 'card'
                    or row['handler'] in (None, 'NULL', '0')
                    or (row['handler'] == 'card_dummy' and name not in NATIVE_CHARACTERISTICS)):
                raise ValueError('pinned combat-rider source or witness changed: ' + name)
            if group == 'sources' and row['handler'] not in HANDLERS[classify(oracle[name])['mode']]:
                raise ValueError('whole combat-rider handler changed: ' + name)


class RiderWorld(_World):
    def __init__(self, name, params, scenario, owner, rows, oracle):
        super().__init__(name, params, owner, rows, oracle)
        self.scenario = scenario
        self.discard_receipts = 0
        self.sacrificed = False
        self.highest = scenario == 'human_high'
        self.loyalty = None
        self.add(name, 'source', source=True)
        self.witnesses()

    def witnesses(self):
        scenario, foe = self.scenario, self.foe
        if scenario in ('blocked', 'hidden_blocked', 'ground_evasion'):
            flying_blocker = scenario == 'blocked' and 'Flying' in self.p['keywords']
            self.add('Wind Drake' if flying_blocker else 'Hill Giant', 'blocker', foe, 'battlefield')
        elif scenario == 'walker_hit':
            self.add('Jace Beleren', 'walker', foe, 'battlefield')
            self.loyalty = int(self.o['Jace Beleren']['loyalty'])
        elif scenario == 'other_attacker':
            self.add('Hill Giant', 'ally', self.owner, 'battlefield')
        elif scenario == 'fogged':
            self.add('Fog', 'fog', foe)
        elif scenario == 'odd_life':
            self.add('Lightning Bolt', 'bolt', foe)
        elif scenario == 'noncombat_damage':
            self.add('Hermetic Study', 'grant', self.owner)
        if self.p['mode'] in ('discard', 'sacrifice') and scenario not in OPENING:
            size = {'empty_hand': 0, 'short_hand': 1}.get(scenario, self.p['amount']+1)
            # The human chooses distinct options. Duplicate AI options
            # remove randomness in identity while keeping native selection.
            cards = HAND[:size] if foe == 0 else ('Forest',)*size
            for index, card in enumerate(cards):
                self.add(card, f'hand_{index}', foe)
        for player in (0, 1):
            if self.p['mode'] == 'exile' and player == foe and scenario not in OPENING:
                size = {'empty_library': 0, 'short_library': 7, 'exact_library': 10}.get(scenario, len(DECK))
                library = DECK[:size]
            else:
                library = ('Forest', 'Mountain')
            for index, card in enumerate(library):
                self.add(card, f'library_{player}_{index}', player, 'library')

    def source_cast(self, *, hidden, choice=False, short=False):
        price = self.p['mana']
        if sum(price) >= 3 or (hidden and not choice):
            return super().source_cast(hidden=hidden, choice=choice, short=short)
        if not hidden:
            # With only the two printed mana, production does not offer the
            # unaffordable generic-three route or a redundant mode choice.
            self.pending('source', price=price, spend=price)
        else:
            budget = list(price)
            budget[0] = 3
            self.pending('source', price=budget, spend=[3, 0, 0, 0, 0, 0], dialog=0)
            self.hidden = True
            self.entities['source'].update(power=2, toughness=2, keys=set())
        self.identity()

    def receipts(self):
        start = len(self.steps)
        super().receipts()
        if self.sacrificed:
            # The native damage callback keeps a temporary trigger carrier
            # that shares the sacrificed card's CSVID. The original bound
            # ref and graveyard identity are independent of that shadow.
            source_id = self.rows[self.name]['csvid']
            self.steps[start:] = [
                action for action in self.steps[start:]
                if not (action.get('player') == self.owner and action.get('zone') == 'battlefield' and (
                    action['op'] == 'assert_zone' or
                    action['op'] == 'assert_zone_multiplicity' and action['index'] == source_id))
            ]
            self.observe('source', 'in_play', 0)
        if self.loyalty:
            self.emit('assert_counter', ref='walker', counter_kind='loyalty', value=self.loyalty)
        if self.p['mode'] in ('discard', 'sacrifice'):
            self.emit('assert_discard_choices', value=self.discard_receipts)

    def discount_life(self, player):
        self.life[player] //= 2

    def discard_cards(self):
        stack = self.zones['hand'][self.foe]
        amount = min(self.p['amount'], len(stack))
        selected = sorted(stack, key=lambda name: self.rows[name]['csvid'], reverse=self.highest)[:amount]
        for name in selected:
            stack.remove(name)
            self.zones['graveyard'][self.foe].append(name)
        if self.foe == 0:
            self.discard_receipts += len(selected)

    def hit_rider(self):
        mode = self.p['mode']
        if mode == 'half':
            self.discount_life(self.foe)
        elif mode == 'exile':
            library = self.zones['library'][self.foe]
            amount = min(len(library), self.p['amount'])
            self.zones['exile'][self.foe].extend(library[:amount])
            del library[:amount]
        elif mode == 'sacrifice':
            accept = self.scenario != 'decline' if self.owner == 0 else len(self.zones['hand'][self.foe]) > 1
            if self.owner == 0:
                self.emit('choose_dialog', player=0, value=int(not accept))
            if accept:
                self.sacrificed = True
                self.move('source', 'graveyard')
                self.discard_cards()
        else:
            self.discard_cards()

    def combat(self):
        scenario = self.scenario
        attacker = 'ally' if scenario == 'other_attacker' else 'source'
        attack = self.entities[attacker]
        allowed = scenario != 'fresh_flip'
        walker = scenario == 'walker_hit'
        self.emit('begin_attackers')
        if walker:
            self.emit('choose_dialog', player=self.owner, value=1)
        self.emit('attack_with_tax' if walker else 'attack', ref=attacker, value=int(allowed))
        if allowed:
            attack.update(attacking=True, tapped=True)
        self.emit('finish_attackers')
        if allowed and attacker == 'source' and not self.hidden and self.p['mode'] == 'half':
            self.discount_life(self.owner)
        self.receipts()
        self.emit('begin_blockers')
        attempted_block = scenario in ('blocked', 'hidden_blocked', 'ground_evasion')
        blocked = attempted_block and scenario != 'ground_evasion'
        if attempted_block:
            self.emit('block', ref='blocker', other=attacker, value=int(blocked))
        self.emit('advance_combat')
        if allowed:
            self.observe(attacker, 'blocked', blocked)
        if scenario == 'late_flip_hit':
            self.reveal()
            self.receipts()
        if allowed and not self.fog:
            if blocked:
                defender = self.entities['blocker']
                attack['damage'] += defender['power']
                defender['damage'] += attack['power']
                for ref, item in ((attacker, attack), ('blocker', defender)):
                    if item['damage'] >= item['toughness']:
                        self.move(ref, 'graveyard')
            elif walker:
                self.loyalty = max(0, self.loyalty-attack['power'])
                if not self.loyalty:
                    self.move('walker', 'graveyard')
            else:
                self.life[self.foe] -= attack['power']
                if attacker == 'source' and not self.hidden and attack['power'] > 0:
                    self.hit_rider()
        self.emit('combat_damage')
        self.emit('resolve')
        self.emit('end_combat')
        attack['attacking'] = False
        self.receipts()

    def lightning_bolt(self):
        from recipes import cost_from_oracle

        cost = cost_from_oracle(self.o['Lightning Bolt'])
        self.pool(self.foe, cost)
        self.emit('refresh')
        self.emit('choose_player', player=self.foe, value=self.owner)
        self.emit('cast', ref='bolt', value=1)
        self.mana[self.foe] = [0]*6
        self.life[self.owner] -= 3
        self.finish_spell('bolt', 'graveyard')

    def noncombat_hit(self):
        self.pending('grant', target='source')
        self.finish_spell('grant', 'battlefield')
        self.emit('require_can_activate', ref='grant', value=1)
        self.emit('choose_player', player=self.owner, value=self.foe)
        self.emit('activate', ref='grant', value=1)
        self.entities['source']['tapped'] = True
        self.emit('assert_stack', value=1)
        self.emit('resolve')
        self.life[self.foe] -= 1
        self.receipts()


def build(name, csvid, declaration, params, variant, rows, oracle):
    from recipes import step

    scenario, player = variant.rsplit('_', 1)
    owner = int(player)
    world = RiderWorld(name, params, scenario, owner, rows, oracle)
    world.emit('turn', player=owner, value=0)
    for seat in (0, 1):
        world.pool(seat, [0]*6)
    if params['mode'] in ('discard', 'sacrifice'):
        world.emit('discard_policy', value=2 if world.highest else 1)
    if scenario == 'cast_short':
        world.source_cast(hidden=True, short=True)
        world.receipts()
    else:
        world.source_cast(hidden=scenario not in ('printed_cast', 'printed_hit'), choice=scenario == 'hidden_choice')
        world.finish_spell('source', 'battlefield')
        if scenario == 'offturn_flip':
            world.next_turn()
        elif scenario not in OPENING and scenario != 'fresh_flip':
            world.age()
        if scenario not in HIDDEN | {'printed_cast', 'printed_hit'}:
            world.reveal(denials=scenario if scenario in ('reveal_short', 'wrong_color') else None)
            world.receipts()
        if scenario == 'noncombat_damage':
            world.noncombat_hit()
        elif scenario not in OPENING:
            if scenario == 'fogged':
                world.pending('fog')
                world.fog = True
                world.finish_spell('fog', 'graveyard')
            elif scenario == 'odd_life':
                world.lightning_bolt()
            cut = len(world.steps)
            world.combat()
            if scenario == 'replay_hit':
                original = copy.deepcopy(world.steps[cut:])
                world.steps = (world.steps[:cut] + [step('capture')] + original + [
                    step('record_state'), step('ai_roundtrip'), step('restore')] + copy.deepcopy(original)
                    + [step('assert_recorded_state')])
    gaps = ['Individual priority inside the native damage-trigger callback, multiplayer and battle recipients.']
    if params['mode'] == 'sacrifice':
        gaps.append('AI uses the card handler’s optional heuristic; replacement ordering between the conditional sacrifice and discards has no intermediate receipt.')
    if params['mode'] in ('sacrifice', 'discard'):
        gaps.append('AI hand choices use indistinguishable copies, while human choices prove exact selected identities.')
    return dict(name=f'MorphCombatRidersRecipe_{csvid}_{variant}', card=name, fixture='empty_main', seed=123,
                setup=world.setup, steps=world.steps, _recipe=dict(family=FAMILY, version=2, variant=variant,
                    params=declaration['params'], oracle_text=declaration['oracle_text'], untested_components=gaps))


def expand(name, csvid, declaration, params, selected, rows, oracle):
    if rows[name]['csvid'] != csvid or not selected or len(set(selected)) != len(selected) or set(selected)-set(variants(params)):
        raise ValueError('distinct pinned Morph combat cases required')
    validate_witnesses(oracle, rows)
    return [build(name, csvid, declaration, params, variant, rows, oracle) for variant in selected]
